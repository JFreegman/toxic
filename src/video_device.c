/*  video_device.c
 *
 *
 *  Copyright (C) 2014 Toxic All Rights Reserved.
 *
 *  This file is part of Toxic.
 *
 *  Toxic is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Toxic is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Toxic.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "video_device.h"
#include "video_call.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>

#include <vpx/vpx_image.h>

#ifdef __linux__
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#endif /* __linux__ */

#include "line_info.h"
#include "settings.h"

#include <errno.h>

#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>

#define inline__ inline __attribute__((always_inline))

extern struct user_settings *user_settings;

struct VideoBuffer {
    void *start;
    size_t length;
};

typedef struct VideoDevice {
    int fd;                                 /* File descriptor of video device selected/opened */
    VideoDataHandleCallback cb;             /* Use this to handle data from input device usually */
    void* cb_data;                          /* Data to be passed to callback */
    int32_t friend_number;                  /* ToxAV friend number */
    
    struct v4l2_format fmt;
    struct VideoBuffer *buffers;
    uint32_t n_buffers; 

    uint32_t ref_count;
    int32_t selection;
    pthread_mutex_t mutex[1];
    uint16_t video_width;
    uint16_t video_height;

    vpx_image_t input;

    Display *x_display;
    Window x_window;
    GC x_gc;

} VideoDevice;

const char *dvideo_device_names[2];              /* Default device */
const char *video_devices_names[2][MAX_DEVICES]; /* Container of available devices */
static int size[2];                        /* Size of above containers */
VideoDevice *video_devices_running[2][MAX_DEVICES] = {{NULL}};     /* Running devices */
uint32_t primary_video_device[2];          /* Primary device */

#ifdef VIDEO
static ToxAV* av = NULL;
#endif /* VIDEO */

/* q_mutex */
#define lock pthread_mutex_lock(&video_mutex);
#define unlock pthread_mutex_unlock(&video_mutex);
pthread_mutex_t video_mutex;

bool video_thread_running = true,
      video_thread_paused = true;                /* Thread control */

void* video_thread_poll(void*);

static int xioctl(int fh, unsigned long request, void *arg)
{
    int r;

    do {
        r = ioctl(fh, request, arg);
    } while (-1 == r && EINTR == errno);

    return r;
}

static void yuv420tobgr(uint16_t width, uint16_t height, const uint8_t *y, 
                 const uint8_t *u, const uint8_t *v, unsigned int ystride, 
                 unsigned int ustride, unsigned int vstride, uint8_t *out)
{
    unsigned long int i, j;
    for (i = 0; i < height; ++i) {
        for (j = 0; j < width; ++j) {
            uint8_t *point = out + 4 * ((i * width) + j);
            int t_y = y[((i * ystride) + j)];
            int t_u = u[(((i / 2) * ustride) + (j / 2))];
            int t_v = v[(((i / 2) * vstride) + (j / 2))];
            t_y = t_y < 16 ? 16 : t_y;

            int r = (298 * (t_y - 16) + 409 * (t_v - 128) + 128) >> 8;
            int g = (298 * (t_y - 16) - 100 * (t_u - 128) - 208 * (t_v - 128) + 128) >> 8;
            int b = (298 * (t_y - 16) + 516 * (t_u - 128) + 128) >> 8;

            point[2] = r>255? 255 : r<0 ? 0 : r;
            point[1] = g>255? 255 : g<0 ? 0 : g;
            point[0] = b>255? 255 : b<0 ? 0 : b;
            point[3] = ~0;
        }
    }
}

static void yuv422to420(uint8_t *plane_y, uint8_t *plane_u, uint8_t *plane_v, 
                 uint8_t *input, uint16_t width, uint16_t height)
{
    uint8_t *end = input + width * height * 2;
    while(input != end) {
        uint8_t *line_end = input + width * 2;
        while(input != line_end) {
            *plane_y++ = *input++;
            *plane_u++ = *input++;
            *plane_y++ = *input++;
            *plane_v++ = *input++;
        }

        line_end = input + width * 2;
        while(input != line_end) {
            *plane_y++ = *input++;
            input++;//u
            *plane_y++ = *input++;
            input++;//v
        }
    }
}

/* Meet devices */
#ifdef VIDEO
VideoDeviceError init_video_devices(ToxAV* av_)
#else
VideoDeviceError init_video_devices()
#endif /* VIDEO */
{
    size[vdt_input] = 0;

    #ifdef __linux__
    for(; size[vdt_input] <= MAX_DEVICES; ++size[vdt_input]) {
        int fd;
        char device_address[] = "/dev/videoXX";
        snprintf(device_address + 10, sizeof(char) * strlen(device_address) - 10, "%i", size[vdt_input]);

        fd = open(device_address, O_RDWR | O_NONBLOCK, 0);
        if (fd == -1) {
            break;
        } else {
            struct v4l2_capability cap;
            char* video_input_name;

            /* Query V4L for capture capabilities */
            if(ioctl(fd, VIDIOC_QUERYCAP, &cap) == -1) {
                video_input_name = cap.card;
            } else {
                video_input_name = device_address;
            }
            video_devices_names[vdt_input][size[vdt_input]] = video_input_name;

            close(fd);
        }
    }
    #endif /* __linux__ */
    /* TODO: Add OSX implementation for listing input video devices */

    size[vdt_output] = 1;
    char* video_output_name = "Video Receiver";
    video_devices_names[vdt_output][0] = video_output_name;

    // Start poll thread
    if (pthread_mutex_init(&video_mutex, NULL) != 0)
        return vde_InternalError;

    pthread_t thread_id;
    if ( pthread_create(&thread_id, NULL, video_thread_poll, NULL) != 0 || pthread_detach(thread_id) != 0)
        return vde_InternalError;

#ifdef VIDEO
    av = av_;
#endif /* VIDEO */

    return (VideoDeviceError) vde_None;
}

VideoDeviceError terminate_video_devices()
{
    /* Cleanup if needed */
    video_thread_running = false;
    usleep(20000);

    if (pthread_mutex_destroy(&video_mutex) != 0)
        return (VideoDeviceError) vde_InternalError;

    return (VideoDeviceError) vde_None;
}

VideoDeviceError register_video_device_callback(int32_t friend_number, uint32_t device_idx, 
                                                VideoDataHandleCallback callback, void* data)
{
    if (size[vdt_input] <= device_idx || !video_devices_running[vdt_input][device_idx] || video_devices_running[vdt_input][device_idx]->fd == NULL) 
        return vde_InvalidSelection;

    lock;
    video_devices_running[vdt_input][device_idx]->cb = callback;
    video_devices_running[vdt_input][device_idx]->cb_data = data;
    video_devices_running[vdt_input][device_idx]->friend_number = friend_number;
    unlock;

    return vde_None;
}

VideoDeviceError set_primary_video_device(VideoDeviceType type, int32_t selection)
{
    if (size[type] <= selection || selection < 0) return vde_InvalidSelection;
   
    primary_video_device[type] = selection;
    
    return vde_None;
}

VideoDeviceError open_primary_video_device(VideoDeviceType type, uint32_t* device_idx)
{
    return open_video_device(type, primary_video_device[type], device_idx);
}

void get_primary_video_device_name(VideoDeviceType type, char *buf, int size)
{
    memcpy(buf, dvideo_device_names[type], size);
}

VideoDeviceError open_video_device(VideoDeviceType type, int32_t selection, uint32_t* device_idx)
{
    if (size[type] <= selection || selection < 0) return vde_InvalidSelection;
    
    lock;
    
    uint32_t i;
    for (i = 0; i < MAX_DEVICES && video_devices_running[type][i] != NULL; ++i);
    
    if (i == MAX_DEVICES) { unlock; return vde_AllDevicesBusy; }
    else *device_idx = i;
    
    for (i = 0; i < MAX_DEVICES; i ++) { /* Check if any device has the same selection */
        if ( video_devices_running[type][i] && video_devices_running[type][i]->selection == selection ) {
            
            video_devices_running[type][*device_idx] = video_devices_running[type][i];            
            video_devices_running[type][i]->ref_count ++;
            
            unlock;
            return vde_None;
        }
    }
    
    VideoDevice* device = video_devices_running[type][*device_idx] = calloc(1, sizeof(VideoDevice));
    device->selection = selection;
    
    if (pthread_mutex_init(device->mutex, NULL) != 0) {
        free(device);
        unlock;
        return vde_InternalError;
    }
    
    if (type == vdt_input) {
        video_thread_paused = true;

#ifdef __linux__
        char device_address[] = "/dev/videoXX";
        snprintf(device_address + 10 , sizeof(device_address) - 10, "%i", selection);

        device->fd = open(device_address, O_RDWR);
        if ( device->fd == -1 ) {
            return vde_FailedStart;
        }

        /* Obtain video device capabilities */
        struct v4l2_capability cap;
        if (-1 == xioctl(device->fd, VIDIOC_QUERYCAP, &cap)) {
            return vde_FailedStart;
        }

        /* Setup video format */
        struct v4l2_format fmt;
        memset(&(fmt), 0, sizeof(fmt));

        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
        if(-1 == xioctl(device->fd, VIDIOC_G_FMT, &fmt)) {
            return vde_FailedStart;
        }

        device->video_width = fmt.fmt.pix.width;
        device->video_height = fmt.fmt.pix.height;



        /* Request buffers */
        struct v4l2_requestbuffers req;
        memset(&(req), 0, sizeof(req));
        req.count = 4;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;
        if (-1 == xioctl(device->fd, VIDIOC_REQBUFS, &req)) {
            return vde_FailedStart;
        }

        if(req.count < 2) {
            return vde_FailedStart;
        }

        device->buffers = calloc(req.count, sizeof(struct VideoBuffer));

        for(i = 0; i < req.count; ++i) {
            struct v4l2_buffer buf;
            memset(&(buf), 0, sizeof(buf));

            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;

            if (-1 == xioctl(device->fd, VIDIOC_QUERYBUF, &buf)) {
                return vde_FailedStart;
            }

            device->buffers[i].length = buf.length;
            device->buffers[i].start = mmap(NULL /* start anywhere */,
                          buf.length,
                          PROT_READ | PROT_WRITE /* required */,
                          MAP_SHARED /* recommended */,
                          device->fd, buf.m.offset);

            if(MAP_FAILED == device->buffers[i].start) {
                return vde_FailedStart;
            }
        }
        device->n_buffers = i;

        enum v4l2_buf_type type;

        for (i = 0; i < device->n_buffers; ++i) {
            struct v4l2_buffer buf;
            memset(&(buf), 0, sizeof(buf));

            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;

            if (-1 == xioctl(device->fd, VIDIOC_QBUF, &buf)) {
                return vde_FailedStart;
            }
        }
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (-1 == xioctl(device->fd, VIDIOC_STREAMON, &type)) {
            return vde_FailedStart;
        }
#endif /* __linux__ */

        /*TODO: Add OSX implementation of opening video devices */
#ifdef __OSX__

#endif /*__OSX__*/

        /* Initialize X11 window associated to device */
        if ((device->x_display = XOpenDisplay(NULL)) == NULL) {
            return vde_FailedStart;
        }

        int screen = DefaultScreen(device->x_display);

        if ((device->x_window = XCreateSimpleWindow(device->x_display, RootWindow(device->x_display, screen), 
                                                    0, 0, device->video_width, device->video_height, 0, 
                                                    BlackPixel(device->x_display, screen), 
                                                    BlackPixel(device->x_display, screen))) == NULL) {
            return vde_FailedStart;
        }

        XStoreName(device->x_display, device->x_window, "Video Preview");
        XSelectInput(device->x_display, device->x_window, ExposureMask|ButtonPressMask|KeyPressMask);

        if ((device->x_gc = DefaultGC(device->x_display, screen)) == NULL) {
            return vde_FailedStart;
        }

        XMapWindow(device->x_display, device->x_window);
        XClearWindow(device->x_display, device->x_window);
        XMapRaised(device->x_display, device->x_window);
        XFlush(device->x_display);

        vpx_img_alloc(&device->input, VPX_IMG_FMT_I420, device->video_width, device->video_height, 1);

        video_thread_paused = false;
    } else {
        /* Initialize X11 window associated to device */
        if ((device->x_display = XOpenDisplay(NULL)) == NULL) {
            return vde_FailedStart;
        }

        int screen = DefaultScreen(device->x_display);

        if ((device->x_window = XCreateSimpleWindow(device->x_display, RootWindow(device->x_display, screen), 
                                                    0, 0, 100, 100, 0, 
                                                    BlackPixel(device->x_display, screen), 
                                                    BlackPixel(device->x_display, screen))) == NULL) {
            return vde_FailedStart;
        }

        XStoreName(device->x_display, device->x_window, "Video Receive");
        XSelectInput(device->x_display, device->x_window, ExposureMask|ButtonPressMask|KeyPressMask);

        if ((device->x_gc = DefaultGC(device->x_display, screen)) == NULL) {
            return vde_FailedStart;
        }


        XMapWindow(device->x_display, device->x_window);
        XClearWindow(device->x_display, device->x_window);
        XMapRaised(device->x_display, device->x_window);
        XFlush(device->x_display);

        vpx_img_alloc(&device->input, VPX_IMG_FMT_I420, device->video_width, device->video_height, 1);
           
    }
    
    unlock;
    return vde_None;
}

__inline VideoDeviceError write_video_out(uint16_t width, uint16_t height,
                                          uint8_t const *y, uint8_t const *u, uint8_t const *v,
                                          int32_t ystride, int32_t ustride, int32_t vstride,
                                          void *user_data)
{
    VideoDevice* device = video_devices_running[vdt_output][0];

    if (!device) return vde_DeviceNotActive;

    pthread_mutex_lock(device->mutex);

    int screen = DefaultScreen(device->x_display);

    /* Recreate missing window */
    if(!device->x_window) {
        device->x_window = XCreateSimpleWindow(device->x_display, RootWindow(device->x_display, screen), 
                                                    0, 0, device->video_width, device->video_height, 0, 
                                                    BlackPixel(device->x_display, screen), 
                                                    BlackPixel(device->x_display, screen));
        XMapWindow(device->x_display, device->x_window);
        XClearWindow(device->x_display, device->x_window);
        XMapRaised(device->x_display, device->x_window);
        XFlush(device->x_display);
    }

    /* Resize X11 window to correct size */
    if(device->video_width != width || device->video_height != height) {
        device->video_width = width;
        device->video_height = height;
        XResizeWindow(device->x_display, device->x_window, width, height);
        vpx_img_free(&device->input);
        vpx_img_alloc(&device->input, VPX_IMG_FMT_I420, width, height, 1);
    }

    /* Display video image */
    ystride = abs(ystride);
    ustride = abs(ustride);
    vstride = abs(vstride);

    uint8_t *img_data = malloc(width * height * 4);
    yuv420tobgr(width, height, y, u, v, ystride, ustride, vstride, img_data);

    XImage image = {
        .width = width,
        .height = height,
        .depth = 24,
        .bits_per_pixel = 32,
        .format = ZPixmap,
        .byte_order = LSBFirst,
        .bitmap_unit = 8,
        .bitmap_bit_order = LSBFirst,
        .bytes_per_line = width * 4,
        .red_mask = 0xFF0000,
        .green_mask = 0xFF00,
        .blue_mask = 0xFF,
        .data = (char*)img_data
    };

    Pixmap pixmap = XCreatePixmap(device->x_display, device->x_window, width, height, 24);
    XPutImage(device->x_display, pixmap, device->x_gc, &image, 0, 0, 0, 0, width, height);
    XCopyArea(device->x_display, pixmap, device->x_window, device->x_gc, 0, 0, width, height, 0, 0);
    XFreePixmap(device->x_display, pixmap);
    XFlush(device->x_display);
    free(img_data);

    pthread_mutex_unlock(device->mutex);
    return vde_None;
}

void* video_thread_poll (void* arg) // TODO: maybe use thread for every input source
{
    /*
     * NOTE: We only need to poll input devices for data.
     */
    (void)arg;
    uint32_t i;
    
    
    while (video_thread_running)
    {
        if (video_thread_paused) usleep(10000); /* Wait for unpause. */
        else
        {
            for (i = 0; i < size[vdt_input]; ++i)
             {
                lock;
                if (video_devices_running[vdt_input][i] != NULL) 
                {
                    VideoDevice* device = video_devices_running[vdt_input][i];
                    struct v4l2_buffer buf;
                    memset(&(buf), 0, sizeof(buf));

                    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                    buf.memory = V4L2_MEMORY_MMAP;

                    if (-1 == ioctl(device->fd, VIDIOC_DQBUF, &buf)) {
                        unlock;
                        continue;
                    }   

                    void *data = (void*)device->buffers[buf.index].start;
                    uint16_t video_width = device->video_width;
                    uint16_t video_height = device->video_height;
                    uint8_t *y = device->input.planes[0];
                    uint8_t *u = device->input.planes[1];
                    uint8_t *v = device->input.planes[2];
                    int screen = DefaultScreen(device->x_display);

                    /* Convert to YUV420 for ToxAV */
                    yuv422to420(y, u, v, data, video_width, video_height);

                    /* Send data to friend through ToxAV */
                    if ( device->cb )
                        device->cb(video_width, video_height, y, u, v, device->cb_data);

                    /* Display image for video preview */
                    uint8_t *img_data = malloc(video_width * video_height * 4);
                    yuv420tobgr(video_width, video_height, y, u, v, 
                                video_width, video_width/2, video_width/2, img_data);
                    XImage image = {
                        .width = video_width,
                        .height = video_height,
                        .depth = 24,
                        .bits_per_pixel = 32,
                        .format = ZPixmap,
                        .byte_order = LSBFirst,
                        .bitmap_unit = 8,
                        .bitmap_bit_order = LSBFirst,
                        .bytes_per_line = video_width * 4,
                        .red_mask = 0xFF0000,
                        .green_mask = 0xFF00,
                        .blue_mask = 0xFF,
                        .data = (char*)img_data
                    };

                    Pixmap pixmap = XCreatePixmap(device->x_display, device->x_window, video_width, video_height, 24);
                    XPutImage(device->x_display, pixmap, device->x_gc, &image, 0, 0, 0, 0, video_width, video_height);
                    XCopyArea(device->x_display, pixmap, device->x_window, device->x_gc, 0, 0, video_width, video_height, 0, 0);
                    XFreePixmap(device->x_display, pixmap);
                    XFlush(device->x_display);
                    free(img_data);


                    if (-1 == xioctl(device->fd, VIDIOC_QBUF, &buf)) {
                        unlock;
                        continue;
                    }
                } 
                unlock;
            }
            usleep(1000 * 1000 / 24);
        }
    }
    
    pthread_exit(NULL);
}

VideoDeviceError close_video_device(VideoDeviceType type, uint32_t device_idx)
{
    if (device_idx >= MAX_DEVICES) return vde_InvalidSelection;
    
    lock;
    VideoDevice *device = video_devices_running[type][device_idx];
    VideoDeviceError rc = vde_None;
    
    if (!device) { 
        unlock;
        return vde_DeviceNotActive;
    }
    
    video_devices_running[type][device_idx] = NULL;
    
    if ( !device->ref_count ) {
        
        if (type == vdt_input) {
            enum v4l2_buf_type buf_type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            if(-1 == xioctl(device->fd, VIDIOC_STREAMOFF, &buf_type)) {}

            int i;
            for(i = 0; i < device->n_buffers; ++i) {
                if (-1 == munmap(device->buffers[i].start, device->buffers[i].length)) {
                }
            }
            close(device->fd);
            //vpx_img_free(&device->input);
            XDestroyWindow(device->x_display, device->x_window);
            XFlush(device->x_display);
            XCloseDisplay(device->x_display);
            pthread_mutex_destroy(device->mutex);
            free(device->buffers);
            free(device);
        } else {   
            vpx_img_free(&device->input);
            XDestroyWindow(device->x_display, device->x_window);
            XFlush(device->x_display);
            XCloseDisplay(device->x_display);
            pthread_mutex_destroy(device->mutex);
            free(device); 
        }

    }
    else device->ref_count--;
    
    unlock;
    return rc;
}

void print_video_devices(ToxWindow* self, VideoDeviceType type)
{
    int i;

    for (i = 0; i < size[type]; ++i)
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "%d: %s", i, video_devices_names[type][i]);

    return;
}
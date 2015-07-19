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

#ifdef VIDEO
#include "video_call.h"
#endif /* VIDEO */

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
    DataHandleCallback cb;                 /* Use this to handle data from input device usually */
    void* cb_data;                         /* Data to be passed to callback */
    int32_t friend_number;                      /* ToxAV friend number */
    
    struct v4l2_format fmt;
    struct VideoBuffer *buffers;
    uint32_t n_buffers; 

    uint32_t ref_count;
    int32_t selection;
    pthread_mutex_t mutex[1];
    uint16_t video_width;
    uint16_t video_height;
} VideoDevice;

const char *ddevice_names[2];              /* Default device */
const char *devices_names[2][MAX_DEVICES]; /* Container of available devices */
static int size[2];                        /* Size of above containers */
VideoDevice *running[2][MAX_DEVICES] = {{NULL}};     /* Running devices */
uint32_t primary_device[2];          /* Primary device */

#ifdef VIDEO
static ToxAV* av = NULL;
#endif /* VIDEO */

/* q_mutex */
#define lock pthread_mutex_lock(&mutex);
#define unlock pthread_mutex_unlock(&mutex);
pthread_mutex_t mutex;

bool thread_running = true,
      thread_paused = true;                /* Thread control */

void* thread_poll(void*);

static int xioctl(int fh, unsigned long request, void *arg)
{
    int r;

    do {
        r = ioctl(fh, request, arg);
    } while (-1 == r && EINTR == errno);

    return r;
}

/* Meet devices */
#ifdef VIDEO
VideoDeviceError init_video_devices(ToxAV* av_)
#else
VideoDeviceError init_video_devices()
#endif /* VIDEO */
{
    size[input] = 0;

    #ifdef __linux__
    for(int i = 0; i <= MAX_DEVICES; ++i) {
        int fd;
        struct v4l2_capability cap;
        char *device_address;

        fd = open(device_address, O_RDWR | O_NONBLOCK, 0);
        if (fd == -1)
            break;
        else {
            devices_names[input][i] = cap.card;
        }

        close(fd);
        size[input] = i;
    }
    #endif /* __linux__ */
    /* TODO: Add OSX implementation for listing input video devices */

    size[output] = 0;
    /* TODO: List output video devices */

    // Start poll thread
    if (pthread_mutex_init(&mutex, NULL) != 0)
        return vde_InternalError;

    pthread_t thread_id;
    if ( pthread_create(&thread_id, NULL, thread_poll, NULL) != 0 || pthread_detach(thread_id) != 0)
        return vde_InternalError;

#ifdef VIDEO
    av = av_;
#endif /* VIDEO */

    return (VideoDeviceError) vde_None;
}

VideoDeviceError terminate_video_devices()
{
    /* Cleanup if needed */
    thread_running = false;
    usleep(20000);

    if (pthread_mutex_destroy(&mutex) != 0)
        return (VideoDeviceError) vde_InternalError;

    return (VideoDeviceError) vde_None;
}

VideoDeviceError set_primary_video_device(VideoDeviceType type, int32_t selection)
{
    if (size[type] <= selection || selection < 0) return vde_InvalidSelection;
    primary_device[type] = selection;
    
    return vde_None;
}

VideoDeviceError open_primary_video_device(VideoDeviceType type, uint32_t* device_idx)
{
    return open_video_device(type, primary_device[type], device_idx);
}

void get_primary_device_name(VideoDeviceType type, char *buf, int size)
{
    memcpy(buf, ddevice_names[type], size);
}

VideoDeviceError open_video_device(VideoDeviceType type, int32_t selection, uint32_t* device_idx)
{
    if (size[type] <= selection || selection < 0) return vde_InvalidSelection;
    
    lock;
    
    uint32_t i;
    for (i = 0; i < MAX_DEVICES && running[type][i] != NULL; ++i);
    
    if (i == MAX_DEVICES) { unlock; return vde_AllDevicesBusy; }
    else *device_idx = i;
    
    for (i = 0; i < MAX_DEVICES; i ++) { /* Check if any device has the same selection */
        if ( running[type][i] && running[type][i]->selection == selection ) {
            
            running[type][*device_idx] = running[type][i];            
            running[type][i]->ref_count ++;
            
            unlock;
            return vde_None;
        }
    }
    
    VideoDevice* device = running[type][*device_idx] = calloc(1, sizeof(VideoDevice));
    device->selection = selection;
    
    if (pthread_mutex_init(device->mutex, NULL) != 0) {
        free(device);
        unlock;
        return vde_InternalError;
    }
    
    if (type == input) {
        char device_address[] = "/dev/videoXX";
        snprintf(device_address + 10 , sizeof(device_address) - 10, "%i", selection);

        device->fd = open(device_address, O_RDWR);
        if ( device->fd == -1 )
            return vde_FailedStart;
    }
    else { 

    }
    
    if (type == input) {
#ifdef __linux__
        /* Obtain video device capabilities */
        struct v4l2_capability cap;
        if (-1 == xioctl(device->fd, VIDIOC_QUERYCAP, &cap)) {
            return vde_UnsupportedMode;
        }

        /* Setup video format */
        struct v4l2_format fmt;
        memset(&(fmt), 0, sizeof(fmt));

        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
        if(-1 == xioctl(device->fd, VIDIOC_G_FMT, &fmt)) {
            return vde_UnsupportedMode;
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
            return vde_UnsupportedMode;
        }

        if(req.count < 2) {
            return vde_UnsupportedMode;
        }

        device->buffers = calloc(req.count, sizeof(*device->buffers));

        for(i = 0; i < req.count; ++i) {
            struct v4l2_buffer buf;
            memset(&(buf), 0, sizeof(buf));

            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = device->n_buffers;
            device->n_buffers = i;

            if (-1 == xioctl(device->fd, VIDIOC_QUERYBUF, &buf)) {
                return vde_UnsupportedMode;
            }

            device->buffers[i].length = buf.length;
            device->buffers[i].start = mmap(NULL /* start anywhere */,
                          buf.length,
                          PROT_READ | PROT_WRITE /* required */,
                          MAP_SHARED /* recommended */,
                          device->fd, buf.m.offset);

            if(MAP_FAILED == device->buffers[i].start) {
                return vde_UnsupportedMode;
            }
        }

        enum v4l2_buf_type type;

        for (i = 0; i < device->n_buffers; ++i) {
            struct v4l2_buffer buf;

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

        thread_paused = false;
    }
    
    unlock;
    return vde_None;
}

void* thread_poll (void* arg) // TODO: maybe use thread for every input source
{
    /*
     * NOTE: We only need to poll input devices for data.
     */
    (void)arg;
    uint32_t i;
    
    
    while (thread_running)
    {
        if (thread_paused) usleep(10000); /* Wait for unpause. */
        else
        {
            for (i = 0; i < size[input]; ++i)
             {
                lock;
                if (running[input][i] != NULL) 
                {
                    VideoDevice* device = running[input][i];
                    struct v4l2_buffer buf;
                    memset(&(buf), 0, sizeof(buf));

                    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                    buf.memory = V4L2_MEMORY_MMAP;

                    if (-1 == ioctl(device->fd, VIDIOC_DQBUF, &buf)) {
                        unlock;
                        continue;
                    }

                    void *data = (void*)device->buffers[buf.index].start;
                    uint8_t *y;
                    uint8_t *u;
                    uint8_t *v;
                    yuv422to420(y, u, v, data, device->video_width, device->video_width);

                    if ( device->cb ) device->cb(device->video_width, device->video_height, y, u, v, device->cb_data);

                    if (-1 == xioctl(device->fd, VIDIOC_QBUF, &buf)) {
                        unlock;
                        continue;
                    }
                } 
                unlock;
            }
            usleep(5000);
        }
    }
    
    pthread_exit(NULL);
}

VideoDeviceError close_video_device(VideoDeviceType type, uint32_t device_idx)
{
    if (device_idx >= MAX_DEVICES) return vde_InvalidSelection;
    
    lock;
    VideoDevice* device = running[type][device_idx];
    VideoDeviceError rc = vde_None;
    
    if (!device) { 
        unlock;
        return vde_DeviceNotActive;
    }
    
    running[type][device_idx] = NULL;
    
    if ( !device->ref_count ) {
        
        if (type == input) {
            int i;
            for(i = 0; i < device->n_buffers; ++i) {
                if (-1 == munmap(device->buffers[i].start, device->buffers[i].length)) {}
            }

            close(device->fd);
        }
        else { 
            
        }
        
        free(device);
    }
    else device->ref_count--;
    
    unlock;
    return rc;
}

void yuv422to420(uint8_t *plane_y, uint8_t *plane_u, uint8_t *plane_v, uint8_t *input, uint16_t width, uint16_t height)
{
    uint8_t *end = input + width * height * 2;
    while(input != end) {
        uint8_t *line_end = input + width * 2;
        while(input != line_end) {
            *plane_y++ = *input++;
            *plane_v++ = *input++;
            *plane_y++ = *input++;
            *plane_u++ = *input++;
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
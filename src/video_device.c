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
#endif

#include "line_info.h"
#include "settings.h"

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <vpx/vpx_image.h>

#ifdef __linux__
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#endif /* __linux __ */

#ifdef __WIN32
#include <windows.h>
#include <dshow.h>

#pragma comment(lib, "strmiids.lib")
#endif /* __WIN32 */

#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>

#define inline__ inline __attribute__((always_inline))

extern struct user_settings *user_settings;

 typedef struct VideoDevice {
    CvCapture* dhndl;
    char* window;
    IplImage* frame;
    int32_t call_idx;

    uint32_t ref_count;
    int32_t selection;
    pthread_mutex_t mutex[1];
    uint16_t frame_width;
    uint16_t frame_height;
 } VideoDevice;

const char *default_video_device_names[2];
const char *video_device_names[2][MAX_DEVICES];
static int video_device_size[2];
VideoDevice *video_device_running[2][MAX_DEVICES] = {{NULL}};
uint32_t primary_video_device[2];

#ifdef VIDEO
static ToxAv* av = NULL;
#endif /* VIDEO */

pthread_mutex_t video_mutex;

bool video_thread_running = true,
      video_thread_paused =  true;

void* video_thread_poll(void*);
/* Meet devices */
#ifdef VIDEO
VideoDeviceError init_video_devices(ToxAv* av_)
#else
VideoDeviceError init_video_devices()
#endif /* AUDIO */
{
    video_device_size[input] = 0;
    uint32_t i;
#ifdef __linux__
    /* Enumerate video capture devices using v4l */
    for(i = 0; i <= MAX_DEVICES; ++i)
    {
        int fd;
        struct v4l2_capability video_cap;
        char device_address[256];
        snprintf(device_address, sizeof device_address, "/dev/video%d",i);

        if((fd = open(device_address, O_RDONLY)) == -1) {
            break;
        }
        else {
            // Query capture device information
            if(ioctl(fd, VIDIOC_QUERYCAP, &video_cap) == -1)
                perror("cam_info: Can't get capabilities");
            else {
                int name_length = sizeof(video_cap.card);
                char* name = (char*)malloc(name_length+1);
                name = video_cap.card;
                video_device_names[input][i] = name;
            }
            close(fd);
            video_device_size[input] = i;
        }
    }
#endif /* __linux__ */
    
#ifdef __WIN32
    /* Enumerate video capture devices using win32 api */

    HRESULT hr;
    CoInitialize(NULL);
    ICreateDevEnum *pSysDevEnum = NULL;
    hr = CoCreateInstance(&CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER, &IID_ICreateDevEnum, (void**)&pSysDevEnum);
    if(FAILED(hr)) {
        printf("CoCreateInstance failed()\n");
    }
    // Obtain a class enumerator for the video compressor category.
    IEnumMoniker *pEnumCat = NULL;
    hr = pSysDevEnum->lpVtbl->CreateClassEnumerator(pSysDevEnum, &CLSID_VideoInputDeviceCategory, &pEnumCat, 0);
    if(hr != S_OK) {
        pSysDevEnum->lpVtbl->Release(pSysDevEnum);
        printf("CreateClassEnumerator failed()\n");
    }

    IMoniker *pMoniker = NULL;

    ULONG cFetched;
    i = 0;
    while( pEnumCat->lpVtbl->Next(pEnumCat, 1, &pMoniker, &cFetched) == S_OK && i <= MAX_DEVICES ) {
        IPropertyBag *pPropBag;
        hr = pMoniker->lpVtbl->BindToStorage(pMoniker, 0, 0, &IID_IPropertyBag, (void **)&pPropBag);
        if(SUCCEEDED(hr)) {
            /* To retrieve the filter's friendly name, do the following: */
            VARIANT varName;
            VariantInit(&varName);
            hr = pPropBag->lpVtbl->Read(pPropBag, L"FriendlyName", &varName, 0);
            if (SUCCEEDED(hr)) {
                if(varName.vt == VT_BSTR) {
                    int name_length = wcslen(varName.bstrVal);
                    char* name = (char*)malloc(name_length + 1);
                    wcstombs(name, varName.bstrVal, name_length + 1);
                    video_device_names[input][i] = name;
                } else {
                    video_device_names[input][i] = "Unknown Device";
                }
                ++i;
            }
            
            VariantClear(&varName);
            pPropBag->lpVtbl->Release(pPropBag);
        }
        pMoniker->lpVtbl->Release(pMoniker);
    }
    video_device_size[input] = i-1;
    pEnumCat->lpVtbl->Release(pEnumCat);
    pSysDevEnum->lpVtbl->Release(pSysDevEnum);
#endif /* __WIN32 */

    /* Start poll thread */
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

void* video_thread_poll(void* arg)
{
    (void)arg;
    uint32_t i;

    while(video_thread_running)
    {
        if (video_thread_paused) usleep(10000); /* Wait for unpause. */
        else
        {
            for (i = 0; i < video_device_size[input]; ++i)
            {
                pthread_mutex_lock(&video_mutex);

                if (video_device_running[input][i] != NULL)
                {
                    /* Capture video frame data of input device */
                    video_device_running[input][i]->frame = cvQueryFrame(video_device_running[input][i]->dhndl);
                }

                pthread_mutex_unlock(&video_mutex);
            }
            usleep(5000);
        }
    }

    pthread_exit(NULL);
}


VideoDeviceError set_primary_video_device(DeviceType type, int32_t selection)
{
    if (video_device_size[type] <= selection || selection < 0) return (VideoDeviceError) vde_InvalidSelection;
    primary_video_device[type] = selection;
    
    return (VideoDeviceError) vde_None;
}

VideoDeviceError open_video_device(DeviceType type, int32_t selection, uint32_t* device_idx)
{
    if (video_device_size[type] <= selection || selection < 0) return vde_InvalidSelection;

    pthread_mutex_lock(&video_mutex);
    uint32_t i;
    for (i = 0; i < MAX_DEVICES; ++i) { /* Check if any device has the same selection */
        if ( video_device_running[type][i] && video_device_running[type][i]->selection == selection ) {

            video_device_running[type][*device_idx] = video_device_running[type][i];
            video_device_running[type][i]->ref_count++;

            pthread_mutex_unlock(&video_mutex);
            return vde_None;
        }
    }

    VideoDevice* device = video_device_running[type][*device_idx] = calloc(1, sizeof(VideoDevice));
    device->selection = selection;

    if (pthread_mutex_init(device->mutex, NULL) != 0) {
        free(device);
        pthread_mutex_unlock(&video_mutex);
        return vde_InternalError;
    }

    if (type  == input) {
        device->window = video_device_names[input][selection];
        cvNamedWindow(device->window, 1);
        device->dhndl = cvCreateCameraCapture(selection);
    }
    else {
        device->dhndl = NULL;
        device->window = video_device_names[output][selection];
        if ( device->dhndl || !device->window ) {
            free(device);
            video_device_running[type][*device_idx] = NULL;
            pthread_mutex_unlock(&video_mutex);
            return vde_FailedStart;
        }

        cvNamedWindow(device->window,1);
    }

    if (type == input) {
        video_thread_paused = false;
    }

    pthread_mutex_unlock(&video_mutex);
    return vde_None;
}

VideoDeviceError close_video_device(DeviceType type, uint32_t device_idx)
{
    if (device_idx >= MAX_DEVICES) return vde_InvalidSelection;

    pthread_mutex_lock(&video_mutex);
    VideoDevice* device = video_device_running[type][device_idx];
    VideoDeviceError rc = de_None;

    if (!device) {
        pthread_mutex_unlock(&video_mutex);
        return vde_DeviceNotActive;
    }

    video_device_running[type][device_idx] = NULL;

    if ( !device->ref_count ) {
        if (type == input) {
            cvReleaseCapture(&device->dhndl);
            cvDestroyWindow(device->window);
        }
        else {
            cvDestroyWindow(device->window);
        }

        free(device);
    }
    else device->ref_count--;

    pthread_mutex_unlock(&video_mutex);
    return rc;
}
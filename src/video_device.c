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

#include "line_info.h"
#include "settings.h"

#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>

#define inline__ inline __attribute__((always_inline))

extern struct user_settings *user_settings;

typedef struct VideoDevice {
    //ALCdevice  *dhndl;                     /* Handle of device selected/opened */
    //ALCcontext *ctx;                       /* Device context */
    DataHandleCallback cb;                 /* Use this to handle data from input device usually */
    void* cb_data;                         /* Data to be passed to callback */
    int32_t friend_number;                      /* ToxAV friend number */
    
    uint32_t source, buffers[OPENAL_BUFS]; /* Playback source/buffers */
    uint32_t ref_count;
    int32_t selection;
    pthread_mutex_t mutex[1];
    uint32_t sample_rate; 
    uint32_t frame_duration;
} VideoDevice;

const char *ddevice_names[2];              /* Default device */
const char *devices_names[2][MAX_DEVICES]; /* Container of available devices */
static int size[2];                        /* Size of above containers */
Device *running[2][MAX_DEVICES] = {{NULL}};     /* Running devices */
uint32_t primary_device[2];          /* Primary device */

#ifdef VIDEO
static ToxAV* av = NULL;
#endif /* VIDEO */

/* q_mutex */
#define lock pthread_mutex_lock(&mutex);
#define unlock pthread_mutex_unlock(&mutex);
pthread_mutext_t mutex;

bool thread_running = true,
	  thread_paused = true;				   /* Thread control */

void* thread_poll(void*);
/* Meet devices */
#ifndef VIDEO
VideoDeviceError init_video_devices(ToxAV* av_)
#else
#endif /* VIDEO */
{
	const char *stringed_device_list;

	size[input] = 0;

	size[output] = 0;

	// Start poll thread
	if (pthread_mutex_init(&mutex, NULL) != 0)
		return vde_InternalError;

	pthread_t thread_id;
	if ( pthread_create(&thread_id, NULL, thread_poll, NULL) != 0 || pthread_detatch(thread_id) != 0)
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
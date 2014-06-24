/*  device.c
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

#include "audio_call.h"
#include "line_info.h"

#ifdef __APPLE__
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
#endif

#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>

#include <tox/toxav.h>

#define openal_bufs 5
#define sample_rate 48000
#define inline__ inline __attribute__((always_inline))
#define frame_size (av_DefaultSettings.audio_sample_rate * av_DefaultSettings.audio_frame_duration / 1000)

typedef struct _Device {
    ALCdevice  *dhndl;                     /* Handle of device selected/opened */
    ALCcontext *ctx;                       /* Device context */
    DataHandleCallback cb;                 /* Use this to handle data from input device usually */
    void* cb_data;                         /* Data to be passed to callback */
    int32_t call_idx;                      /* ToxAv call index */
    
    uint32_t source, buffers[openal_bufs]; /* Playback source/buffers */
    size_t ref_count;
    int32_t selection;
    _Bool enable_VAD;
    _Bool muted;
    float VAD_treshold;                    /* 40 is usually recommended value */
} Device;

const char *ddevice_names[2];              /* Default device */
const char *devices_names[2][MAX_DEVICES]; /* Container of available devices */
static int size[2];                        /* Size of above containers */
Device *running[2][MAX_DEVICES]={NULL};    /* Running devices */
uint32_t primary_device[2] = {0};          /* Primary device */

static ToxAv* av = NULL;

/* q_mutex */
#define lock pthread_mutex_lock(&mutex)
#define unlock pthread_mutex_unlock(&mutex)
pthread_mutex_t mutex;


_Bool thread_running = _True, 
      thread_paused = _True;               /* Thread control */

void* thread_poll(void*);
/* Meet devices */
DeviceError init_devices(ToxAv* av_)
{
    const char *stringed_device_list;
    
    
    
    size[input] = 0;
    if ( (stringed_device_list = alcGetString(NULL, ALC_CAPTURE_DEVICE_SPECIFIER)) ) {
        ddevice_names[input] = alcGetString(NULL, ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER);
        
        for ( ; *stringed_device_list && size[input] < MAX_DEVICES; ++size[input] ) {
            devices_names[input][size[input]] = stringed_device_list;                        
            stringed_device_list += strlen( stringed_device_list ) + 1;
        }
    }
    
    
    
    size[output] = 0;
    if ( (stringed_device_list = alcGetString(NULL, ALC_DEVICE_SPECIFIER)) ) {
        ddevice_names[output] = alcGetString(NULL, ALC_DEFAULT_DEVICE_SPECIFIER);
        
        for ( ; *stringed_device_list && size[output] < MAX_DEVICES; ++size[output] ) {
            devices_names[output][size[output]] = stringed_device_list;            
            stringed_device_list += strlen( stringed_device_list ) + 1;
        }
    }
    
    // Start poll thread
    
    pthread_mutex_init(&mutex, NULL);
    
    pthread_t thread_id;
    if ( pthread_create(&thread_id, NULL, thread_poll, NULL) != 0 || pthread_detach(thread_id) != 0) 
        return de_InternalError;    
    
    av = av_;
    
    return ae_None;
}

DeviceError terminate_devices()
{
    /* Cleanup if needed */
    thread_running = false;
    usleep(20000);
    
    pthread_mutex_destroy(&mutex);
    
    return ae_None;
}

DeviceError device_mute(DeviceType type, uint32_t device_idx)
{
    if (device_idx >= MAX_DEVICES) return de_InvalidSelection;
    lock;
    
    Device* device = running[type][device_idx];
    
    if (!device) { 
        unlock;
        return de_DeviceNotActive;
    }
    
    device->muted = !device->muted;
    
    unlock;
    return de_None;
}

DeviceError device_set_VAD_treshold(uint32_t device_idx, float value)
{
    if (device_idx >= MAX_DEVICES) return de_InvalidSelection;
    lock;
    
    Device* device = running[input][device_idx];
    
    if (!device) { 
        unlock;
        return de_DeviceNotActive;
    }
    
    device->VAD_treshold = value;
    
    unlock;
    return de_None;
}

DeviceError set_primary_device(DeviceType type, int32_t selection)
{
    if (size[type] <= selection || selection < 0) return de_InvalidSelection;
    primary_device[type] = selection;
    
    return de_None;
}

DeviceError open_primary_device(DeviceType type, uint32_t* device_idx)
{
    return open_device(type, primary_device[type], device_idx);
}


// TODO: generate buffers separately
DeviceError open_device(DeviceType type, int32_t selection, uint32_t* device_idx)
{
    if (size[type] <= selection || selection < 0) return de_InvalidSelection;
    
    lock;
    
    uint32_t i;
    for (i = 0; i < MAX_DEVICES && running[type][i] != NULL; i ++);
    
    if (i == size[type]) { unlock; return de_AllDevicesBusy; }
    else *device_idx = i;
    
    Device* device = running[type][*device_idx] = calloc(1, sizeof(Device));;
    device->selection = selection;
    
    for (i = 0; i < *device_idx; i ++) { /* Check if any previous has the same selection */
        if ( running[type][i]->selection == selection ) {
            device->dhndl = running[type][i]->dhndl;
            if (type == output) {
                device->ctx = running[type][i]->ctx;
                memcpy(device->buffers, running[type][i]->buffers, sizeof(running[type][i]->buffers));
                device->source = running[type][i]->source;
            }
            device->ref_count++;
            unlock;
            return de_None;
        }
    }
    
    if (type == input) {
        device->dhndl = alcCaptureOpenDevice(devices_names[type][selection], 
                        av_DefaultSettings.audio_sample_rate, AL_FORMAT_MONO16, frame_size * 4);
        device->VAD_treshold = VAD_THRESHOLD_DEFAULT;
    }
    else { 
        device->dhndl = alcOpenDevice(devices_names[type][selection]);
        if ( !device->dhndl ) { 
            free(device);
            running[type][*device_idx] = NULL;
            unlock;
            return de_FailedStart;
        }
        
        device->ctx = alcCreateContext(device->dhndl, NULL);
        alcMakeContextCurrent(device->ctx);
        
        alGenBuffers(openal_bufs, device->buffers);
        alGenSources((uint32_t)1, &device->source);
        alSourcei(device->source, AL_LOOPING, AL_FALSE);
        
        uint16_t zeros[frame_size];
        memset(zeros, 0, frame_size);
        
        for ( i =0; i < openal_bufs; ++i) {
            alBufferData(device->buffers[i], AL_FORMAT_MONO16, zeros, frame_size, sample_rate);
        }
        
        alSourceQueueBuffers(device->source, openal_bufs, device->buffers);
        alSourcePlay(device->source);
    }
    
    if (alcGetError(device->dhndl) != AL_NO_ERROR) {
        free(device);
        running[type][*device_idx] = NULL;
        unlock;
        return de_FailedStart;
    }
    
    if (type == input) {
        alcCaptureStart(device->dhndl);
        thread_paused = _False;
    }
    
    unlock;
    return de_None;
}

DeviceError close_device(DeviceType type, uint32_t device_idx)
{
    if (device_idx >= MAX_DEVICES) return de_InvalidSelection;
    
    lock;
    Device* device = running[type][device_idx];
    
    if (!device) { 
        unlock;
        return de_DeviceNotActive;
    }
            
    if ( !(device->ref_count--) ) {
        running[type][device_idx] = NULL;
        unlock;
        
        DeviceError rc = de_None;
        
        if (type == input) {
            if ( !alcCaptureCloseDevice(device->dhndl) ) rc = de_AlError;
        }
        else { 
            if (alcGetCurrentContext() != device->ctx) alcMakeContextCurrent(device->ctx);
            
            alDeleteSources(1, &device->source);
            alDeleteBuffers(openal_bufs, device->buffers);
            
            if ( !alcCloseDevice(device->dhndl) ) rc = de_AlError;
            alcMakeContextCurrent(NULL);
            if ( device->ctx ) alcDestroyContext(device->ctx);
        }
                
        free(device);
        return rc;
    }
    
    return de_None;
}

DeviceError register_device_callback( int32_t call_idx, uint32_t device_idx, DataHandleCallback callback, void* data, _Bool enable_VAD)
{
    if (size[input] <= device_idx || !running[input][device_idx] || running[input][device_idx]->dhndl == NULL) 
        return de_InvalidSelection;
    
    lock;
    running[input][device_idx]->cb = callback;
    running[input][device_idx]->cb_data = data;
    running[input][device_idx]->enable_VAD = enable_VAD;
    running[input][device_idx]->call_idx = call_idx;
    unlock;
    
    return de_None;
}

inline__ DeviceError playback_device_ready(uint32_t device_idx)
{
    if (device_idx >= MAX_DEVICES) return de_InvalidSelection;
    
    Device* device = running[output][device_idx];
    
    if (!device) return de_DeviceNotActive;
    
    int32_t ready;
    alGetSourcei(device->source, AL_BUFFERS_PROCESSED, &ready);
    
    return ready <= 0 ? de_Busy : de_None;
}

/* TODO: thread safety? */
inline__ DeviceError write_out(uint32_t device_idx, int16_t* data, uint32_t lenght, uint8_t channels)
{
    if (device_idx >= MAX_DEVICES) return de_InvalidSelection;
    
    Device* device = running[output][device_idx];
    
    if (!device || device->muted) return de_DeviceNotActive;
    
    alcMakeContextCurrent(device->ctx); /* TODO: Check for error */
    
    uint32_t buffer;
    int32_t ready;
    
    
    alSourceUnqueueBuffers(device->source, 1, &buffer);
    alBufferData(buffer, AL_FORMAT_MONO16, data, lenght * 2 * 1 /*channels*/, sample_rate); // TODO: Frequency must be set dynamically
    
    int rc = alGetError();
    if (rc != AL_NO_ERROR) {
        fprintf(stderr, "Error setting buffer %d\n", rc);
        return de_BufferError;
    }
    
    alSourceQueueBuffers(device->source, 1, &buffer);
    
    rc = alGetError();
    if (alGetError() != AL_NO_ERROR) {
        fprintf(stderr, "Error: could not buffer audio: %d\n", rc);
        return de_BufferError;
    }
    
    alGetSourcei(device->source, AL_SOURCE_STATE, &ready);
    
    if (ready != AL_PLAYING) {
        alSourcePlay(device->source);
        return de_None;
    }
    
    return de_Busy;
}

void* thread_poll (void* arg) // TODO: maybe use thread for every input source
{
    /*
     * NOTE: We only need to poll input devices for data.
     */
    (void)arg;
    uint32_t i;
    int32_t sample = 0;
    
    int f_size = frame_size;
    
    while (thread_running)
    {
        if (thread_paused) usleep(10000); /* Wait for unpause. */
        else
        {
            for (i = 0; i < size[input]; i ++) 
            {
                lock;
                if (running[input][i] != NULL) 
                {
                    alcGetIntegerv(running[input][i]->dhndl, ALC_CAPTURE_SAMPLES, sizeof(int32_t), &sample);
                    
                    if (sample < f_size) { 
                        unlock;
                        continue;
                    }
                    Device* device = running[input][i];
                    
                    int16_t frame[4096];
                    alcCaptureSamples(device->dhndl, frame, f_size);
                    
                    if ( device->muted || 
                        (device->enable_VAD && !toxav_has_activity(av, device->call_idx, frame, f_size, device->VAD_treshold)))
                        { unlock; continue; } /* Skip if no voice activity */
                    
                    if ( device->cb ) device->cb(frame, f_size, device->cb_data);
                } 
                unlock;
            }
            usleep(5000);
        }
    }
    
    pthread_exit(NULL);
}

void print_devices(ToxWindow* self, DeviceType type)
{
    int i = 0;
    for ( ; i < size[type]; i ++) {
        uint8_t msg[MAX_STR_SIZE];
        snprintf(msg, sizeof(msg), "%d: %s", i, devices_names[type][i]);
        line_info_add(self, NULL, NULL, NULL, msg, SYS_MSG, 0, 0);
    }
    
    return;
}

DeviceError selection_valid(DeviceType type, int32_t selection)
{
    return (size[type] <= selection || selection < 0) ? de_InvalidSelection : de_None;
}

void* get_device_callback_data(uint32_t device_idx)
{
    if (size[input] <= device_idx || !running[input][device_idx] || running[input][device_idx]->dhndl == NULL) 
        return NULL;
        
    return running[input][device_idx]->cb_data;
}
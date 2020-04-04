/*  audio_device.c
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

#include "audio_device.h"

#ifdef AUDIO
#include "audio_call.h"
#endif

#include "line_info.h"
#include "settings.h"
#include "misc_tools.h"

#include <AL/al.h>
#include <AL/alc.h>
/* compatibility with older versions of OpenAL */
#ifndef ALC_ALL_DEVICES_SPECIFIER
#include <AL/alext.h>
#endif /* ALC_ALL_DEVICES_SPECIFIER */

#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>

#define inline__ inline __attribute__((always_inline))

extern struct user_settings *user_settings;

typedef struct FrameInfo {
    uint32_t samples_per_frame;
    uint32_t sample_rate;
    bool stereo;
} FrameInfo;

/* A virtual input/output device, abstracting the currently selected openal
 * device (which may change during the lifetime of the virtual device).
 * We refer to a virtual device as a "device", and refer to an underlying
 * openal device as an "al_device".
 * Multiple virtual devices may be open at once; the callback of each virtual
 * input device has data captured from the input al_device passed to it, and
 * each virtual output device acts as a source for the output al_device.
 */
typedef struct Device {
    bool active;
    bool muted;

    FrameInfo frame_info;

    // used only by input devices:
    DataHandleCallback cb;
    void *cb_data;
    // TODO: implement VAD, and fix the typo, or remove these.
    bool enable_VAD;
#ifdef AUDIO
    float VAD_treshold; /* 40 is usually recommended value */
#endif

    // used only by output devices:
    uint32_t source, buffers[OPENAL_BUFS];
    bool source_open;
} Device;

typedef struct AudioState {
    ALCdevice *al_device[2];

    Device devices[2][MAX_DEVICES];
    uint32_t num_devices[2];

    FrameInfo capture_frame_info;

    pthread_mutex_t mutex[2];

    // TODO: unused
    const char *default_al_device_name[2];              /* Default devices */

    const char *al_device_names[2][MAX_OPENAL_DEVICES]; /* Available devices */
    uint32_t num_al_devices[2];
    char *current_al_device_name[2];
} AudioState;

AudioState *audio_state;

void lock(DeviceType type)
{
    pthread_mutex_lock(&audio_state->mutex[type]);
}

void unlock(DeviceType type)
{
    pthread_mutex_unlock(&audio_state->mutex[type]);
}


bool thread_running = true,
     thread_paused = true;               /* Thread control */

static void *poll_input(void *);

static uint32_t sound_mode(bool stereo)
{
    return (stereo ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16);
}

static uint32_t sample_size(bool stereo)
{
    return (stereo ? 4 : 2);
}

DeviceError init_devices(void)
{
    audio_state = calloc(1, sizeof(AudioState));

    if (audio_state == NULL) {
        return de_InternalError;
    }

    get_al_device_names();

    for (DeviceType type = input; type <= output; type++) {
        audio_state->al_device[type] = NULL;

        if (pthread_mutex_init(&audio_state->mutex[type], NULL) != 0) {
            return de_InternalError;
        }
    }

    // Start poll thread
    pthread_t thread_id;

    if (pthread_create(&thread_id, NULL, poll_input, NULL) != 0 || pthread_detach(thread_id) != 0) {
        return de_InternalError;
    }

    return de_None;
}

DeviceError terminate_devices(void)
{
    lock(input);
    thread_running = false;
    unlock(input);

    usleep(20000);

    for (DeviceType type = input; type <= output; type++) {
        if (pthread_mutex_destroy(&audio_state->mutex[type]) != 0) {
            return de_InternalError;
        }

        if (audio_state->current_al_device_name[type] != NULL) {
            free(audio_state->current_al_device_name[type]);
        }
    }

    free(audio_state);

    return de_None;
}

void get_al_device_names()
{
    const char *stringed_device_list;

    for (DeviceType type = input; type <= output; type++) {
        audio_state->num_al_devices[type] = 0;

        if (type == input) {
            stringed_device_list = alcGetString(NULL, ALC_CAPTURE_DEVICE_SPECIFIER);
        } else {
            if (alcIsExtensionPresent(NULL, "ALC_ENUMERATE_ALL_EXT") != AL_FALSE) {
                stringed_device_list = alcGetString(NULL, ALC_ALL_DEVICES_SPECIFIER);
            } else {
                stringed_device_list = alcGetString(NULL, ALC_DEVICE_SPECIFIER);
            }
        }

        if (stringed_device_list != NULL) {
            audio_state->default_al_device_name[type] = alcGetString(NULL,
                    type == input ? ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER : ALC_DEFAULT_DEVICE_SPECIFIER);

            for (; *stringed_device_list
                    && audio_state->num_al_devices[type] < MAX_OPENAL_DEVICES; ++audio_state->num_al_devices[type]) {
                audio_state->al_device_names[type][audio_state->num_al_devices[type]] = stringed_device_list;
                stringed_device_list += strlen(stringed_device_list) + 1;
            }
        }
    }
}

DeviceError device_mute(DeviceType type, uint32_t device_idx)
{
    if (device_idx >= MAX_DEVICES) {
        return de_InvalidSelection;
    }

    Device *device = &audio_state->devices[type][device_idx];

    if (!device->active) {
        return de_DeviceNotActive;
    }

    lock(type);

    device->muted = !device->muted;

    unlock(type);
    return de_None;
}

#ifdef AUDIO
DeviceError device_set_VAD_treshold(uint32_t device_idx, float value)
{
    if (device_idx >= MAX_DEVICES) {
        return de_InvalidSelection;
    }

    Device *device = &audio_state->devices[input][device_idx];

    if (!device->active) {
        return de_DeviceNotActive;
    }

    lock(input);

    device->VAD_treshold = value;

    unlock(input);
    return de_None;
}
#endif

DeviceError set_source_position(uint32_t device_idx, float x, float y, float z)
{
    if (device_idx >= MAX_DEVICES) {
        return de_InvalidSelection;
    }

    Device *device = &audio_state->devices[output][device_idx];

    if (!device->active) {
        return de_DeviceNotActive;
    }

    lock(output);

    alSource3f(device->source, AL_POSITION, x, y, z);

    unlock(output);

    if (!audio_state->al_device[output] || alcGetError(audio_state->al_device[output]) != AL_NO_ERROR) {
        return de_AlError;
    }

    return de_None;
}

static DeviceError close_al_device(DeviceType type)
{
    if (audio_state->al_device[type] == NULL) {
        return de_None;
    }

    if (type == input) {
        if (!alcCaptureCloseDevice(audio_state->al_device[type])) {
            return de_AlError;
        }

        thread_paused = true;
    } else {
        ALCcontext *context = alcGetCurrentContext();
        alcMakeContextCurrent(NULL);
        alcDestroyContext(context);

        if (!alcCloseDevice(audio_state->al_device[type])) {
            return de_AlError;
        }
    }

    audio_state->al_device[type] = NULL;

    return de_None;
}

static DeviceError open_al_device(DeviceType type, FrameInfo frame_info)
{
    audio_state->al_device[type] = type == input
                                   ? alcCaptureOpenDevice(audio_state->current_al_device_name[type],
                                           frame_info.sample_rate, sound_mode(frame_info.stereo), frame_info.samples_per_frame * 2)
                                   : alcOpenDevice(audio_state->current_al_device_name[type]);

    if (audio_state->al_device[type] == NULL) {
        return de_FailedStart;
    }

    if (type == input) {
        alcCaptureStart(audio_state->al_device[type]);
        thread_paused = false;

        audio_state->capture_frame_info = frame_info;
    } else {
        alcMakeContextCurrent(alcCreateContext(audio_state->al_device[type], NULL));
    }

    if (alcGetError(audio_state->al_device[type]) != AL_NO_ERROR) {
        close_al_device(type);
        return de_AlError;
    }

    return de_None;
}

static void close_source(Device *device)
{
    if (device->source_open) {
        alDeleteSources(1, &device->source);
        alDeleteBuffers(OPENAL_BUFS, device->buffers);

        device->source_open = false;
    }
}

static DeviceError open_source(Device *device)
{
    alGenBuffers(OPENAL_BUFS, device->buffers);

    if (alcGetError(audio_state->al_device[output]) != AL_NO_ERROR) {
        return de_FailedStart;
    }

    alGenSources((uint32_t)1, &device->source);

    if (alcGetError(audio_state->al_device[output]) != AL_NO_ERROR) {
        alDeleteBuffers(OPENAL_BUFS, device->buffers);
        return de_FailedStart;
    }

    device->source_open = true;

    alSourcei(device->source, AL_LOOPING, AL_FALSE);

    const uint32_t frame_size = device->frame_info.samples_per_frame * sample_size(device->frame_info.stereo);
    uint16_t zeros[frame_size];
    memset(zeros, 0, frame_size);

    for (int i = 0; i < OPENAL_BUFS; ++i) {
        alBufferData(device->buffers[i], sound_mode(device->frame_info.stereo), zeros,
                     frame_size, device->frame_info.sample_rate);
    }

    alSourceQueueBuffers(device->source, OPENAL_BUFS, device->buffers);
    alSourcePlay(device->source);

    if (alcGetError(audio_state->al_device[output]) != AL_NO_ERROR) {
        close_source(device);
        return de_FailedStart;
    }

    return de_None;
}

DeviceError set_al_device(DeviceType type, int32_t selection)
{
    if (audio_state->num_al_devices[type] <= selection || selection < 0) {
        return de_InvalidSelection;
    }

    const char *name = audio_state->al_device_names[type][selection];

    char **cur_name = &audio_state->current_al_device_name[type];

    if (*cur_name != NULL) {
        free(*cur_name);
    }

    *cur_name = malloc(strlen(name) + 1);

    if (*cur_name == NULL) {
        return de_InternalError;
    }

    strcpy(*cur_name, name);

    if (audio_state->num_devices[type] > 0) {
        // close any existing al_device and try to open new one, reopening existing sources
        lock(type);

        if (type == output) {
            for (int i = 0; i < MAX_DEVICES; i++) {
                Device *device = &audio_state->devices[type][i];

                if (device->active) {
                    close_source(device);
                }
            }
        }

        close_al_device(type);

        DeviceError err = open_al_device(type, audio_state->capture_frame_info);

        if (err != de_None) {
            unlock(type);
            return err;
        }

        if (type == output) {
            for (int i = 0; i < MAX_DEVICES; i++) {
                Device *device = &audio_state->devices[type][i];

                if (device->active) {
                    open_source(device);
                }
            }
        }

        unlock(type);
    }

    return de_None;
}

static DeviceError open_device(DeviceType type, uint32_t *device_idx,
                               DataHandleCallback cb, void *cb_data, bool enable_VAD,
                               uint32_t sample_rate, uint32_t frame_duration, uint8_t channels)
{
    if (channels != 1 && channels != 2) {
        return de_UnsupportedMode;
    }

    const uint32_t samples_per_frame = (sample_rate * frame_duration / 1000);
    FrameInfo frame_info = {samples_per_frame, sample_rate, channels == 2};

    uint32_t i;

    for (i = 0; i < MAX_DEVICES && audio_state->devices[type][i].active; ++i);

    if (i == MAX_DEVICES) {
        return de_AllDevicesBusy;
    }

    *device_idx = i;

    lock(type);

    if (audio_state->al_device[type] == NULL) {
        DeviceError err = open_al_device(type, frame_info);

        if (err != de_None) {
            unlock(type);
            return err;
        }
    } else if (type == input) {
        // Use previously set frame info on existing capture device
        frame_info = audio_state->capture_frame_info;
    }

    Device *device = &audio_state->devices[type][i];
    device->active = true;
    ++audio_state->num_devices[type];

    device->muted = false;
    device->frame_info = frame_info;

    if (type == input) {
        device->cb = cb;
        device->cb_data = cb_data;
        device->enable_VAD = enable_VAD;
#ifdef AUDIO
        device->VAD_treshold = user_settings->VAD_treshold;
#endif
    } else {
        if (open_source(device) != de_None) {
            device->active = false;
            --audio_state->num_devices[type];
            unlock(type);
            return de_FailedStart;
        }
    }

    unlock(type);
    return de_None;
}

DeviceError open_input_device(uint32_t *device_idx,
                              DataHandleCallback cb, void *cb_data, bool enable_VAD,
                              uint32_t sample_rate, uint32_t frame_duration, uint8_t channels)
{
    return open_device(input, device_idx,
                       cb, cb_data, enable_VAD,
                       sample_rate, frame_duration, channels);
}

DeviceError open_output_device(uint32_t *device_idx,
                               uint32_t sample_rate, uint32_t frame_duration, uint8_t channels)
{
    return open_device(output, device_idx,
                       0, 0, 0,
                       sample_rate, frame_duration, channels);
}

DeviceError close_device(DeviceType type, uint32_t device_idx)
{
    if (device_idx >= MAX_DEVICES) {
        return de_InvalidSelection;
    }

    lock(type);

    Device *device = &audio_state->devices[type][device_idx];

    if (!device->active) {
        return de_DeviceNotActive;
    }

    if (type == output) {
        close_source(device);
    }

    device->active = false;
    --audio_state->num_devices[type];

    DeviceError err = de_None;

    if (audio_state->num_devices[type] == 0) {
        err = close_al_device(type);
    }

    unlock(type);
    return err;
}

inline__ DeviceError write_out(uint32_t device_idx, const int16_t *data, uint32_t sample_count, uint8_t channels,
                               uint32_t sample_rate)
{
    if (device_idx >= MAX_DEVICES) {
        return de_InvalidSelection;
    }

    lock(output);

    Device *device = &audio_state->devices[output][device_idx];

    if (!device->active || device->muted) {
        unlock(output);
        return de_DeviceNotActive;
    }

    ALuint bufid;
    ALint processed, queued;
    alGetSourcei(device->source, AL_BUFFERS_PROCESSED, &processed);
    alGetSourcei(device->source, AL_BUFFERS_QUEUED, &queued);

    if (audio_state->al_device[output] == NULL || alcGetError(audio_state->al_device[output]) != AL_NO_ERROR) {
        unlock(output);
        return de_AlError;
    }

    if (processed) {
        ALuint bufids[processed];
        alSourceUnqueueBuffers(device->source, processed, bufids);
        alDeleteBuffers(processed - 1, bufids + 1);
        bufid = bufids[0];
    } else if (queued < 16) {
        alGenBuffers(1, &bufid);
    } else {
        unlock(output);
        return de_Busy;
    }


    const bool stereo = channels == 2;
    alBufferData(bufid, sound_mode(stereo), data,
                 sample_count * sample_size(stereo),
                 sample_rate);
    alSourceQueueBuffers(device->source, 1, &bufid);

    ALint state;
    alGetSourcei(device->source, AL_SOURCE_STATE, &state);

    if (state != AL_PLAYING) {
        alSourcePlay(device->source);
    }

    unlock(output);
    return de_None;
}

static void *poll_input(void *arg)
{
    UNUSED_VAR(arg);

    while (1) {
        lock(input);

        if (!thread_running) {
            unlock(input);
            break;
        }

        if (thread_paused) {
            unlock(input);
            usleep(10000);
            continue;
        }

        if (audio_state->al_device[input] != NULL) {
            int32_t available_samples;
            alcGetIntegerv(audio_state->al_device[input], ALC_CAPTURE_SAMPLES, sizeof(int32_t), &available_samples);

            const uint32_t f_size = audio_state->capture_frame_info.samples_per_frame;

            if (available_samples >= f_size) {
                int16_t frame[16000];
                alcCaptureSamples(audio_state->al_device[input], frame, f_size);

                for (int i = 0; i < MAX_DEVICES; i++) {
                    Device *device = &audio_state->devices[input][i];

                    if (device->active && !device->muted && device->cb) {
                        const DataHandleCallback cb = device->cb;
                        void *const cb_data = device->cb_data;
                        unlock(input);
                        cb(frame, f_size, cb_data);
                        lock(input);
                    }
                }
            }
        }

        unlock(input);
        usleep(5000);
    }

    pthread_exit(NULL);
}

void print_al_devices(ToxWindow *self, DeviceType type)
{
    for (int i = 0; i < audio_state->num_al_devices[type]; ++i) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG,
                      audio_state->current_al_device_name[type]
                      && strcmp(audio_state->current_al_device_name[type], audio_state->al_device_names[type][i]) == 0 ? 1 : 0,
                      0, "%d: %s", i, audio_state->al_device_names[type][i]);
    }

    return;
}

DeviceError selection_valid(DeviceType type, int32_t selection)
{
    return (audio_state->num_al_devices[type] <= selection || selection < 0) ? de_InvalidSelection : de_None;
}

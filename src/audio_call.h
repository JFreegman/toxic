/*  audio_call.h
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

#ifndef AUDIO_CALL_H
#define AUDIO_CALL_H

#include <tox/toxav.h>

#include "audio_device.h"

#define MAX_CALLS 10

typedef enum _AudioError {
    ae_None = 0,
    ae_StartingCaptureDevice = 1 << 0,
    ae_StartingOutputDevice = 1 << 1,
    ae_StartingCoreAudio = 1 << 2
} AudioError;

#ifdef VIDEO
typedef enum _VideoError {
    ve_None = 0,
    ve_StartingCaptureDevice = 1 << 0,
    ve_StartingOutputDevice = 1 << 1,
    ve_StartingCoreVideo = 1 << 2
} VideoError;

#endif /* VIDEO */

typedef struct Call {
    pthread_t ttid; /* Transmission thread id */
    bool ttas, has_output; /* Transmission thread active status (0 - stopped, 1- running) */
    uint32_t in_idx, out_idx; /* Audio Index */
#ifdef VIDEO
    uint32_t vin_idx, vout_idx; /* Video Index */
#endif /* VIDEO */
    pthread_mutex_t mutex;
} Call;

struct CallControl {
    AudioError audio_errors;
#ifdef VIDEO
    VideoError video_errors;
#endif /* VIDEO */

    ToxAV *av;
    ToxWindow *prompt;

    Call calls[MAX_CALLS];
    uint32_t call_state;
    bool pending_call;
    bool audio_enabled;
    bool video_enabled;

    uint32_t audio_bit_rate;
    int32_t audio_frame_duration;
    uint32_t audio_sample_rate;
    uint8_t audio_channels;

    uint32_t video_bit_rate;
    int32_t video_frame_duration;

} CallControl;

struct CallControl CallControl;

/* You will have to pass pointer to first member of 'windows' declared in windows.c */
ToxAV *init_audio(ToxWindow *self, Tox *tox);
void terminate_audio();
int start_transmission(ToxWindow *self, Call *call);
int stop_transmission(Call *call, uint32_t friend_number);
void stop_current_call(ToxWindow *self);

#endif /* AUDIO_CALL_H */

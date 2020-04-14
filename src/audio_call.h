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

typedef enum AudioError {
    ae_None = 0,
    ae_StartingCaptureDevice = 1 << 0,
    ae_StartingOutputDevice = 1 << 1,
    ae_StartingCoreAudio = 1 << 2
} AudioError;

#ifdef VIDEO
typedef enum VideoError {
    ve_None = 0,
    ve_StartingCaptureDevice = 1 << 0,
    ve_StartingOutputDevice = 1 << 1,
    ve_StartingCoreVideo = 1 << 2
} VideoError;

#endif /* VIDEO */

/* Status transitions:
 * None -> Pending (call invitation made or received);
 * Pending -> None (invitation rejected or failed);
 * Pending -> Active (call starts);
 * Active -> None (call ends).
 */
typedef enum CallStatus {
    cs_None = 0,
    cs_Pending,
    cs_Active
} CallStatus;

typedef struct Call {
    CallStatus status;
    uint32_t state; /* ToxAV call state, valid when `status == cs_Active` */
    uint32_t in_idx, out_idx; /* Audio device index, or -1 if not open */
    uint32_t audio_bit_rate; /* Bit rate for sending audio */

    uint32_t vin_idx, vout_idx; /* Video device index, or -1 if not open */
    uint32_t video_width, video_height;
    uint32_t video_bit_rate; /* Bit rate for sending video; 0 for no video */
} Call;

struct CallControl {
    AudioError audio_errors;
#ifdef VIDEO
    VideoError video_errors;
#endif /* VIDEO */

    ToxAV *av;
    ToxWindow *prompt;

    Call *calls;
    uint32_t max_calls;

    bool audio_enabled;
    bool video_enabled;

    int32_t audio_frame_duration;
    uint32_t audio_sample_rate;
    uint8_t audio_channels;
    uint32_t default_audio_bit_rate;

    int32_t video_frame_duration;
    uint32_t default_video_width, default_video_height;
    uint32_t default_video_bit_rate;
};

extern struct CallControl CallControl;

/* You will have to pass pointer to first member of 'windows' declared in windows.c */
ToxAV *init_audio(ToxWindow *self, Tox *tox);
void terminate_audio(void);

bool init_call(Call *call);

void place_call(ToxWindow *self);
void stop_current_call(ToxWindow *self);

void init_friend_AV(uint32_t index);
void del_friend_AV(uint32_t index);

#endif /* AUDIO_CALL_H */

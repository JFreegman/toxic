/*  video_call.h
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

#ifndef VIDEO_H
#define VIDEO_H

#include <tox/toxav.h>

#include "audio_call.h"
#include "video_device.h"

typedef enum _VideoError {
    ve_None = 0,
    ve_StartingCaptureDevice = 1 << 0,
    ve_StartingOutputDevice = 1 << 1,
    ve_StartingCoreVideo = 1 << 2
} VideoError;

/* You will have to pass pointer to first member of 'windows' declared in windows.c */
ToxAv *init_video(ToxWindow *self, Tox *tox, ToxAv *av);
void terminate_video();
int start_video_transmission(ToxWindow *self, Call *call);
int stop_video_transmission(Call *call, int call_index);
void stop_current_video_call(ToxWindow *self);

 #endif /* VIDEO_H */
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

#ifndef VIDEO_CALL_H
#define VIDEO_CALL_H

#include <tox/toxav.h>

#include "audio_call.h"

#include "video_device.h"

/* You will have to pass pointer to first member of 'windows' declared in windows.c */
ToxAV *init_video(ToxWindow *self, Tox *tox);
void terminate_video(void);
int start_video_transmission(ToxWindow *self, ToxAV *av, Call *call);
int stop_video_transmission(Call *call, int friend_number);

void callback_recv_video_starting(uint32_t friend_number);
void callback_recv_video_end(uint32_t friend_number);
void callback_video_starting(uint32_t friend_number);
void callback_video_end(uint32_t friend_number);

#endif /* VIDEO_CALL_H */

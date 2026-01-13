/*  video_call.h
 *
 *  Copyright (C) 2014-2026 Toxic All Rights Reserved.
 *
 *  This file is part of Toxic. Toxic is free software licensed
 *  under the GNU General Public License 3.0.
 */

#ifndef VIDEO_CALL_H
#define VIDEO_CALL_H

#include <tox/toxav.h>

#include "audio_call.h"
#include "toxic.h"
#include "video_device.h"

ToxAV *init_video(Toxic *toxic);
void terminate_video(void);
int start_video_transmission(ToxWindow *self, Toxic *toxic, Call *call);
int stop_video_transmission(Call *call, int friend_number);

void callback_recv_video_starting(uint32_t friend_number);
void callback_recv_video_end(uint32_t friend_number);
void callback_video_end(uint32_t friend_number);

#endif /* VIDEO_CALL_H */

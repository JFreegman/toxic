/*  groupchat.h
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

#ifndef GROUPCHAT_H
#define GROUPCHAT_H

#include "toxic.h"
#include "windows.h"

#ifdef AUDIO
#include "audio_call.h"
#endif

#ifdef AUDIO
#ifdef __APPLE__
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
/* compatibility with older versions of OpenAL */
#ifndef ALC_ALL_DEVICES_SPECIFIER
#include <AL/alext.h>
#endif  /* ALC_ALL_DEVICES_SPECIFIER */
#endif  /* __APPLE__ */
#endif  /* AUDIO */

#define SIDEBAR_WIDTH 16
#define SDBAR_OFST 2    /* Offset for the peer number box at the top of the statusbar */
#define MAX_GROUPCHAT_NUM MAX_WINDOWS_NUM - 2
#define GROUP_EVENT_WAIT 3

#ifdef AUDIO
#define MAX_AUDIO_SOURCES 128    /* arbitrary limit */

struct GAudio {
    ALCdevice  *dvhandle;    /* Handle of device selected/opened */
    ALCcontext *dvctx;
    ALuint sources[MAX_AUDIO_SOURCES];    /* one audio source per peer */
    uint8_t muted[MAX_AUDIO_SOURCES];
};
#endif  /* AUDIO */

typedef struct {
    int chatwin;
    bool active;
    uint8_t type;
    int num_peers;
    int side_pos;    /* current position of the sidebar - used for scrolling up and down */
    uint64_t start_time;
    uint8_t  *peer_names;
    uint8_t  *oldpeer_names;
    uint16_t *peer_name_lengths;
    uint16_t *oldpeer_name_lengths;

#ifdef AUDIO
    struct GAudio audio;
#endif
} GroupChat;

void close_groupchat(ToxWindow *self, Tox *m, int groupnum);
int init_groupchat_win(ToxWindow *prompt, Tox *m, int groupnum, uint8_t type);

/* destroys and re-creates groupchat window with or without the peerlist */
void redraw_groupchat_win(ToxWindow *self);

ToxWindow new_group_chat(Tox *m, int groupnum);

#endif /* #define GROUPCHAT_H */

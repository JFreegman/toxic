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

#define SIDEBAR_WIDTH 16
#define SDBAR_OFST 2    /* Offset for the peer number box at the top of the statusbar */
#define MAX_GROUPCHAT_NUM MAX_WINDOWS_NUM - 2
#define GROUP_EVENT_WAIT 2

#ifdef AUDIO
#define MAX_GROUP_PEERS 256    /* arbitrary limit, only used for audio */
#endif

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
    Call call;
#endif
} GroupChat;

void kill_groupchat_window(ToxWindow *self);
int init_groupchat_win(ToxWindow *prompt, Tox *m, int groupnum, uint8_t type);

/* destroys and re-creates groupchat window with or without the peerlist */
void redraw_groupchat_win(ToxWindow *self);

ToxWindow new_group_chat(Tox *m, int groupnum);

#endif /* #define GROUPCHAT_H */

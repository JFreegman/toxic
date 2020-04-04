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

#define SIDEBAR_WIDTH 16
#define MAX_GROUPCHAT_NUM MAX_WINDOWS_NUM - 2
#define GROUP_EVENT_WAIT 3

typedef struct GroupPeer {
    bool       active;

    uint8_t    pubkey[TOX_PUBLIC_KEY_SIZE];
    uint32_t   peernumber;

    char       name[TOX_MAX_NAME_LENGTH];
    size_t     name_length;

    bool       sending_audio;
    bool       mute;
    uint32_t   audio_out_idx;
    time_t     last_audio_time;
} GroupPeer;

typedef struct AudioInputCallbackData {
    Tox *tox;
    uint32_t groupnumber;
} AudioInputCallbackData;

typedef struct NameListEntry NameListEntry;

typedef struct {
    int chatwin;
    bool active;
    uint8_t type;
    int side_pos;    /* current position of the sidebar - used for scrolling up and down */
    time_t start_time;

    GroupPeer *peer_list;
    uint32_t max_idx;

    NameListEntry *name_list;
    uint32_t num_peers;

    bool audio_enabled;
    time_t last_sent_audio;
    uint32_t audio_in_idx;
    bool mute;
    AudioInputCallbackData audio_input_callback_data;
} GroupChat;

/* Frees all Toxic associated data structures for a groupchat (does not call tox_conference_delete() ) */
void free_groupchat(ToxWindow *self, uint32_t groupnum);

int init_groupchat_win(Tox *m, uint32_t groupnum, uint8_t type, const char *title, size_t title_length);

/* destroys and re-creates groupchat window with or without the peerlist */
void redraw_groupchat_win(ToxWindow *self);

bool init_group_audio_input(Tox *tox, uint32_t groupnumber);
bool enable_group_audio(Tox *tox, uint32_t groupnumber);
bool disable_group_audio(Tox *tox, uint32_t groupnumber);
void audio_group_callback(void *tox, uint32_t groupnumber, uint32_t peernumber,
                          const int16_t *pcm, unsigned int samples, uint8_t channels, uint32_t
                          sample_rate, void *userdata);

bool group_mute_self(uint32_t groupnumber);
bool group_mute_peer(uint32_t groupnumber, const char *prefix);

#endif /* GROUPCHAT_H */

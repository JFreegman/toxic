/*  conference.h
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

#ifndef CONFERENCE_H
#define CONFERENCE_H

#include "toxic.h"
#include "windows.h"

#define SIDEBAR_WIDTH 16
#define MAX_CONFERENCE_NUM MAX_WINDOWS_NUM - 2
#define CONFERENCE_EVENT_WAIT 3

typedef struct ConferencePeer {
    bool       active;

    uint8_t    pubkey[TOX_PUBLIC_KEY_SIZE];
    uint32_t   peernum;    /* index in chat->peer_list */

    char       name[TOX_MAX_NAME_LENGTH];
    size_t     name_length;

    bool       sending_audio;
    uint32_t   audio_out_idx;
    time_t     last_audio_time;
} ConferencePeer;

typedef struct AudioInputCallbackData {
    Tox *tox;
    uint32_t conferencenum;
} AudioInputCallbackData;

#define PUBKEY_STRING_SIZE (2 * TOX_PUBLIC_KEY_SIZE + 1)
typedef struct NameListEntry {
    char name[TOX_MAX_NAME_LENGTH];
    char pubkey_str[PUBKEY_STRING_SIZE];
    uint32_t peernum;
} NameListEntry;


typedef struct {
    int chatwin;
    bool active;
    uint8_t type;
    int side_pos;    /* current position of the sidebar - used for scrolling up and down */
    time_t start_time;

    ConferencePeer *peer_list;
    uint32_t max_idx;

    NameListEntry *name_list;
    uint32_t num_peers;

    bool audio_enabled;
    time_t last_sent_audio;
    uint32_t audio_in_idx;
    AudioInputCallbackData audio_input_callback_data;
} ConferenceChat;

/* Frees all Toxic associated data structures for a conference (does not call tox_conference_delete() ) */
void free_conference(ToxWindow *self, uint32_t conferencenum);

int init_conference_win(Tox *m, uint32_t conferencenum, uint8_t type, const char *title, size_t title_length);

/* destroys and re-creates conference window with or without the peerlist */
void redraw_conference_win(ToxWindow *self);

/* Puts `(NameListEntry *)`s in `entries` for each matched peer, up to a maximum
 * of `maxpeers`.
 * Maches each peer whose name or pubkey begins with `prefix`.
 * If `prefix` is exactly the pubkey of a peer, matches only that peer.
 * return number of entries placed in `entries`.
 */
uint32_t get_name_list_entries_by_prefix(uint32_t conferencenum, const char *prefix, NameListEntry **entries,
        uint32_t maxpeers);

bool init_conference_audio_input(Tox *tox, uint32_t conferencenum);
bool enable_conference_audio(Tox *tox, uint32_t conferencenum);
bool disable_conference_audio(Tox *tox, uint32_t conferencenum);
void audio_conference_callback(void *tox, uint32_t conferencenum, uint32_t peernum,
                          const int16_t *pcm, unsigned int samples, uint8_t channels, uint32_t
                          sample_rate, void *userdata);

bool conference_mute_self(uint32_t conferencenum);
bool conference_mute_peer(const Tox *m, uint32_t conferencenum, uint32_t peernum);
bool conference_set_VAD_threshold(uint32_t conferencenum, float threshold);
float conference_get_VAD_threshold(uint32_t conferencenum);

#endif /* CONFERENCE_H */

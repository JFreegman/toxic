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
#define SDBAR_OFST 2    /* Offset for the peer number box at the top of the statusbar */
#define MAX_GROUPCHAT_NUM MAX_WINDOWS_NUM - 2
#define GROUP_EVENT_WAIT 3

struct GroupPeer {
    bool       active;
    char       name[TOX_MAX_NAME_LENGTH];
    size_t     name_length;
    uint32_t   peer_id;
    uint8_t    public_key[TOX_GROUP_PEER_PUBLIC_KEY_SIZE];
    TOX_USER_STATUS status;
    TOX_GROUP_ROLE  role;
    uint64_t   last_active;
};

typedef struct {
    struct GroupPeer *peer_list;
    char       *name_list;    /* List of peer names, needed for tab completion */
    uint32_t   num_peers;     /* Number of peers in the chat/name_list array */
    uint32_t   max_idx;       /* Maximum peer list index - 1 */
    uint32_t   groupnumber;
    int        chatwin;
    bool       active;
    uint64_t   time_connected;    /* The time we successfully connected to the group */
    int        side_pos;     /* current position of the sidebar - used for scrolling up and down */
} GroupChat;

void close_groupchat(ToxWindow *self, Tox *m, uint32_t groupnum);
int init_groupchat_win(Tox *m, uint32_t groupnum, const char *groupname, size_t length);
void set_nick_all_groups(Tox *m, const char *nick, size_t length);
void set_status_all_groups(Tox *m, uint8_t status);
int group_get_nick_peer_id(uint32_t groupnum, const char *nick, uint32_t *peer_id);
int get_peer_index(uint32_t groupnum, uint32_t peer_id);

/* destroys and re-creates groupchat window */
void redraw_groupchat_win(ToxWindow *self);

#endif /* #define GROUPCHAT_H */

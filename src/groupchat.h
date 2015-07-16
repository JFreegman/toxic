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
    char       name[TOX_MAX_NAME_LENGTH];
    size_t     name_length;
    TOX_USER_STATUS status;
    TOX_GROUP_ROLE  role;
};

typedef struct {
    struct GroupPeer *peer_list;
    char       *name_list;    /* List of peer names, needed for tab completion */
    uint32_t   num_peers;
    uint32_t   groupnumber;
    int        chatwin;
    bool       active;
    bool       is_connected;
    int        side_pos;    /* current position of the sidebar - used for scrolling up and down */
} GroupChat;

void close_groupchat(ToxWindow *self, Tox *m, uint32_t groupnum);
int init_groupchat_win(Tox *m, uint32_t groupnum, const char *groupname, size_t length);
void set_nick_all_groups(Tox *m, const char *nick, size_t length);
void set_status_all_groups(Tox *m, uint8_t status);
int group_get_nick_peernumber(uint32_t groupnum, const char *nick);

/* destroys and re-creates groupchat window */
void redraw_groupchat_win(ToxWindow *self);

#endif /* #define GROUPCHAT_H */

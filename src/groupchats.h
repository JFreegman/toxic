/*  groupchats.h
 *
 *
 *  Copyright (C) 2020 Toxic All Rights Reserved.
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

#ifndef GROUPCHATS_H
#define GROUPCHATS_H

#include "toxic.h"
#include "windows.h"

#ifndef SIDEWAR_WIDTH
#define SIDEBAR_WIDTH 16
#endif

#define MAX_GROUPCHAT_NUM (MAX_WINDOWS_NUM - 2)

typedef enum Group_Join_Type {
    Group_Join_Type_Create,
    Group_Join_Type_Join,
    Group_Join_Type_Load,
} Group_Join_Type;

typedef struct GroupPeer {
    bool             active;
    char             name[TOX_MAX_NAME_LENGTH];
    size_t           name_length;
    uint32_t         peer_id;
    uint8_t          public_key[TOX_GROUP_PEER_PUBLIC_KEY_SIZE];
    TOX_USER_STATUS  status;
    TOX_GROUP_ROLE   role;
    uint64_t         last_active;
} GroupPeer;

typedef struct {
    GroupPeer  *peer_list;
    char       **name_list;   /* List of peer names, needed for tab completion */
    uint32_t   num_peers;     /* Number of peers in the chat/name_list array */
    uint32_t   max_idx;       /* Maximum peer list index - 1 */

    char       group_name[TOX_GROUP_MAX_GROUP_NAME_LENGTH + 1];
    size_t     group_name_length;
    uint32_t   groupnumber;
    bool       active;
    uint64_t   time_connected;    /* The time we successfully connected to the group */

    int        chatwin;
    int        side_pos;     /* current position of the sidebar - used for scrolling up and down */
} GroupChat;

void exit_groupchat(ToxWindow *self, Tox *m, uint32_t groupnumber, const char *partmessage, size_t length);
int init_groupchat_win(Tox *m, uint32_t groupnumber, const char *groupname, size_t length, Group_Join_Type join_type);
void set_nick_all_groups(Tox *m, const char *new_nick, size_t length);
void set_status_all_groups(Tox *m, uint8_t status);
int group_get_nick_peer_id(uint32_t groupnumber, const char *nick, uint32_t *peer_id);
int get_peer_index(uint32_t groupnumber, uint32_t peer_id);
void groupchat_onGroupPeerExit(ToxWindow *self, Tox *m, uint32_t groupnumber, uint32_t peer_id,
                               Tox_Group_Exit_Type exit_type,
                               const char *name, size_t name_len, const char *partmessage, size_t len);
void groupchat_onGroupModeration(ToxWindow *self, Tox *m, uint32_t groupnumber, uint32_t src_peer_id,
                                 uint32_t tgt_peer_id, TOX_GROUP_MOD_EVENT type);

void groupchat_rejoin(ToxWindow *self, Tox *m);

/* destroys and re-creates groupchat window */
void redraw_groupchat_win(ToxWindow *self);

/*
 * Return a GroupChat pointer associated with groupnumber.
 * Return NULL if groupnumber is invalid.
 */
GroupChat *get_groupchat(uint32_t groupnumber);

#endif /* #define GROUPCHATS_H */

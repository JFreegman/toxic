/*  groupchats.h
 *
 *
 *  Copyright (C) 2024 Toxic All Rights Reserved.
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

#include <assert.h>

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
    char             prev_name[TOX_MAX_NAME_LENGTH];
    uint32_t         peer_id;
    uint8_t          public_key[TOX_GROUP_PEER_PUBLIC_KEY_SIZE];
    TOX_USER_STATUS  status;
    Tox_Group_Role   role;
    bool             is_ignored;
    uint64_t         last_active;
} GroupPeer;

typedef struct {
    char       chat_id[TOX_GROUP_CHAT_ID_SIZE];
    GroupPeer  *peer_list;
    char       **name_list;   /* List of peer names, needed for tab completion */
    uint32_t   num_peers;     /* Number of peers in the chat/name_list array */
    uint32_t   max_idx;       /* Maximum peer list index - 1 */

    uint8_t    **ignored_list; /* List of keys of peers that we're ignoring */
    uint16_t   num_ignored;

    char       group_name[TOX_GROUP_MAX_GROUP_NAME_LENGTH + 1];
    size_t     group_name_length;
    uint32_t   groupnumber;
    bool       active;
    uint64_t   time_connected;    /* The time we successfully connected to the group */

    int        chatwin;
    int        side_pos;     /* current position of the sidebar - used for scrolling up and down */
} GroupChat;

void exit_groupchat(ToxWindow *self, Toxic *toxic, uint32_t groupnumber, const char *partmessage, size_t length);
int init_groupchat_win(Toxic *toxic, uint32_t groupnumber, const char *groupname, size_t length,
                       Group_Join_Type join_type);
void set_nick_this_group(ToxWindow *self, Toxic *toxic, const char *new_nick, size_t length);
void set_status_all_groups(Toxic *toxic, uint8_t status);
int get_peer_index(uint32_t groupnumber, uint32_t peer_id);
void groupchat_onGroupPeerExit(ToxWindow *self, Toxic *toxic, uint32_t groupnumber, uint32_t peer_id,
                               Tox_Group_Exit_Type exit_type,
                               const char *name, size_t name_len, const char *partmessage, size_t len);
void groupchat_onGroupModeration(ToxWindow *self, Toxic *toxic, uint32_t groupnumber, uint32_t src_peer_id,
                                 uint32_t tgt_peer_id, Tox_Group_Mod_Event type);

void groupchat_rejoin(ToxWindow *self, Toxic *toxic);

/* Puts the peer_id associated with `identifier` in `peer_id`. The string may be
 * either a nick or a public key.
 *
 * On failure, `peer_id` is set to (uint32_t)-1.
 *
 * This function is intended to be a helper for groupchat_commands.c and will print
 * error messages to `self`.
 * Return 0 on success.
 * Return -1 if the identifier does not correspond with a peer in the group.
 * Return -2 if the identifier is a nick and the nick is in use by multiple peers.
 */
int group_get_peer_id_of_identifier(ToxWindow *self, const Client_Config *c_config, const char *identifier,
                                    uint32_t *peer_id);

/* Gets the peer_id associated with `public_key`.
 *
 * Returns 0 on success.
 * Returns -1 on failure or if `public_key` is invalid.
 */
int group_get_public_key_peer_id(uint32_t groupnumber, const char *public_key, uint32_t *peer_id);

/* destroys and re-creates groupchat window */
void redraw_groupchat_win(ToxWindow *self);

/*
 * Return a GroupChat pointer associated with groupnumber.
 * Return NULL if groupnumber is invalid.
 */
GroupChat *get_groupchat(uint32_t groupnumber);

/**
 * Toggles the ignore status of the peer associated with `peer_id`.
 */
void group_toggle_peer_ignore(uint32_t groupnumber, int peer_id, bool ignore);

/*
 * Sets the tab name colour config option for the groupchat associated with `public_key` to `colour`.
 *
 * `public_key` should be a string representing the group's public chat ID.
 *
 * Return true on success.
 */
bool groupchat_config_set_tab_name_colour(const char *public_key, const char *colour);

/*
 * Sets the auto-logging preference for the groupchat associated with `public_key`.
 *
 * `public_key` should be a string representing the group's public chat ID.
 *
 * Return true on success.
 */
bool groupchat_config_set_autolog(const char *public_key, bool autolog_enabled);

#endif /* #define GROUPCHATS_H */

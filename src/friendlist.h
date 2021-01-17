/*  friendlist.h
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

#ifndef FRIENDLIST_H
#define FRIENDLIST_H

#include <time.h>

#include "file_transfers.h"
#include "game_base.h"
#include "toxic.h"
#include "windows.h"

struct LastOnline {
    uint64_t last_on;
    struct tm tm;
    char hour_min_str[TIME_STR_SIZE];    /* holds 12/24-hour time string e.g. "10:43 PM" */
};

struct ConferenceInvite {
    char *key;
    uint16_t length;
    uint8_t type;
    bool pending;
};

struct GameInvite {
    uint8_t *data;
    size_t data_length;
    GameType type;
    uint32_t id;
    bool pending;
};

typedef struct {
    char name[TOXIC_MAX_NAME_LENGTH + 1];
    uint16_t namelength;
    char statusmsg[TOX_MAX_STATUS_MESSAGE_LENGTH + 1];
    size_t statusmsg_len;
    char pub_key[TOX_PUBLIC_KEY_SIZE];
    uint32_t num;
    int chatwin;
    bool active;
    Tox_Connection connection_status;
    bool is_typing;
    bool logging_on;    /* saves preference for friend irrespective of global settings */
    Tox_User_Status status;

    struct LastOnline last_online;
    struct ConferenceInvite conference_invite;
    struct GameInvite game_invite;

    struct FileTransfer file_receiver[MAX_FILES];
    struct FileTransfer file_sender[MAX_FILES];
} ToxicFriend;

typedef struct {
    char name[TOXIC_MAX_NAME_LENGTH + 1];
    uint16_t namelength;
    char pub_key[TOX_PUBLIC_KEY_SIZE];
    uint32_t num;
    bool active;
    uint64_t last_on;
} BlockedFriend;

typedef struct {
    int num_selected;
    size_t num_friends;
    size_t num_online;
    size_t max_idx;    /* 1 + the index of the last friend in list */
    uint32_t *index;
    ToxicFriend *list;
} FriendsList;

ToxWindow *new_friendlist(void);
void friendlist_onInit(ToxWindow *self, Tox *m);
void disable_chatwin(uint32_t f_num);
int get_friendnum(uint8_t *name);
int load_blocklist(char *data);
void kill_friendlist(ToxWindow *self);
void friendlist_onFriendAdded(ToxWindow *self, Tox *m, uint32_t num, bool sort);
Tox_User_Status get_friend_status(uint32_t friendnumber);
Tox_Connection get_friend_connection_status(uint32_t friendnumber);

/* sorts friendlist_index first by connection status then alphabetically */
void sort_friendlist_index(void);

/*
 * Returns true if friend associated with `public_key` is in the block list.
 *
 * `public_key` must be at least TOX_PUBLIC_KEY_SIZE bytes.
 */
bool friend_is_blocked(const char *public_key);

#endif /* end of include guard: FRIENDLIST_H */

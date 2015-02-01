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

#include "toxic.h"
#include "windows.h"
#include "file_senders.h"

struct FileReceiver {
    char filename[MAX_STR_SIZE];
    int filenum;
    FILE *file;
    bool pending;
    bool active;
    uint64_t size;
    uint64_t bytes_recv;
    double bps;
    uint64_t last_progress;   /* unix-time when we last updated progress */
    uint32_t line_id;
};

struct LastOnline {
    uint64_t last_on;
    struct tm tm;
    char hour_min_str[TIME_STR_SIZE];    /* holds 12/24-hour time string e.g. "10:43 PM" */
};

struct GroupChatInvite {
    char *key;
    uint16_t length;
    uint8_t type;
    bool pending;
};

typedef struct {
    char name[TOXIC_MAX_NAME_LENGTH + 1];
    int namelength;
    char statusmsg[TOX_MAX_STATUSMESSAGE_LENGTH + 1];
    uint16_t statusmsg_len;
    char pub_key[TOX_PUBLIC_KEY_SIZE];
    int32_t num;
    int chatwin;
    bool active;
    bool online;
    uint8_t is_typing;
    bool logging_on;    /* saves preference for friend irrespective of global settings */
    uint8_t status;
    struct LastOnline last_online;
    struct FileReceiver file_receiver[MAX_FILES];
    struct GroupChatInvite group_invite;
    uint8_t active_file_receivers;
} ToxicFriend;

typedef struct {
    char name[TOXIC_MAX_NAME_LENGTH + 1];
    int namelength;
    char pub_key[TOX_PUBLIC_KEY_SIZE];
    int32_t num;
    bool active;
    uint64_t last_on;
} BlockedFriend;

typedef struct {
    int num_selected;
    int max_idx;    /* 1 + the index of the last friend in list */
    int num_friends;
    int *index;
    ToxicFriend *list;
} FriendsList;

ToxWindow new_friendlist(void);
void disable_chatwin(int32_t f_num);
int get_friendnum(uint8_t *name);
int load_blocklist(char *data);
void kill_friendlist(void);
void friendlist_onFriendAdded(ToxWindow *self, Tox *m, int32_t num, bool sort);

/* sorts friendlist_index first by connection status then alphabetically */
void sort_friendlist_index(void);

#endif /* end of include guard: FRIENDLIST_H */

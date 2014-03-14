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

#ifndef FRIENDLIST_H_53I41IM
#define FRIENDLIST_H_53I41IM

#include "toxic_windows.h"

typedef struct {
    uint8_t name[TOX_MAX_NAME_LENGTH];
    uint16_t namelength;
    uint8_t statusmsg[TOX_MAX_STATUSMESSAGE_LENGTH];
    uint16_t statusmsg_len;
    uint8_t pending_groupchat[TOX_CLIENT_ID_SIZE];
    uint8_t pub_key[TOX_CLIENT_ID_SIZE];
    int num;
    int chatwin;
    bool active;
    bool online;
    bool is_typing;
    bool logging_on;    /* saves preference for friend irrespective of chat windows */
    uint64_t last_online;
    TOX_USERSTATUS status;
    struct FileReceiver file_receiver;
} ToxicFriend;

typedef struct {
    int num;
    bool active;
} PendingDel;

ToxWindow new_friendlist(void);
void disable_chatwin(int f_num);
int get_friendnum(uint8_t *name);

void friendlist_onFriendAdded(ToxWindow *self, Tox *m, int num, bool sort);

/* sorts friendlist_index first by connection status then alphabetically */
void sort_friendlist_index(void);

#endif /* end of include guard: FRIENDLIST_H_53I41IM */

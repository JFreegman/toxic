/*  prompt.h
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

#ifndef PROMPT_H
#define PROMPT_H

#include "toxic.h"
#include "windows.h"

#define MAX_FRIEND_REQUESTS 20

struct friend_request {
    bool active;
    char msg[TOX_MAX_FRIEND_REQUEST_LENGTH + 1];
    uint8_t key[TOX_PUBLIC_KEY_SIZE];
};

typedef struct FriendRequests {
    int max_idx;
    int num_requests;
    struct friend_request request[MAX_FRIEND_REQUESTS];
} FriendRequests;

extern ToxWindow *prompt;
extern FriendRequests FrndRequests;

ToxWindow *new_prompt(void);

void prep_prompt_win(void);
void prompt_init_statusbar(ToxWindow *self, Toxic *toxic, bool first_time_run);
void prompt_update_nick(ToxWindow *prompt, const char *nick);
void prompt_update_statusmessage(ToxWindow *prompt, Toxic *toxic, const char *statusmsg);
void prompt_update_status(ToxWindow *prompt, Tox_User_Status status);
void prompt_update_connectionstatus(ToxWindow *prompt, bool is_connected);
void kill_prompt_window(ToxWindow *self, const Client_Config *c_config);

/* callback: Updates own connection status in prompt statusbar */
void on_self_connection_status(Tox *tox, Tox_Connection connection_status, void *userdata);

/* Returns our own connection status */
Tox_Connection prompt_selfConnectionStatus(void);

#endif /* end of include guard: PROMPT_H */

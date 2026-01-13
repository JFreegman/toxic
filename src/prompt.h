/*  prompt.h
 *
 *  Copyright (C) 2014-2026 Toxic All Rights Reserved.
 *
 *  This file is part of Toxic. Toxic is free software licensed
 *  under the GNU General Public License 3.0.
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

extern FriendRequests FrndRequests;

ToxWindow *new_prompt(void);

void prep_prompt_win(void);
void prompt_init_statusbar(Toxic *toxic, bool first_time_run);
void prompt_update_nick(ToxWindow *self, const char *nick);
void prompt_update_statusmessage(Toxic *toxic, const char *statusmsg);
void prompt_update_status(ToxWindow *self, Tox_User_Status status);
void kill_prompt_window(ToxWindow *self, Windows *windows, const Client_Config *c_config);

/* callback: Updates own connection status in prompt statusbar */
void on_self_connection_status(Tox *tox, Tox_Connection connection_status, void *userdata);

/* Returns our own connection status */
Tox_Connection prompt_selfConnectionStatus(Toxic *toxic);

#endif /* end of include guard: PROMPT_H */

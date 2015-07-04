/*  global_commands.c
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

#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#include "toxic.h"
#include "windows.h"
#include "misc_tools.h"
#include "friendlist.h"
#include "log.h"
#include "line_info.h"
#include "dns.h"
#include "groupchat.h"
#include "prompt.h"
#include "help.h"
#include "term_mplex.h"
#include "avatars.h"

extern char *DATA_FILE;
extern ToxWindow *prompt;
extern FriendsList Friends;
extern FriendRequests FrndRequests;

/* command functions */
void cmd_accept(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    if (argc < 1) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Request ID required.");
        return;
    }

    int req = atoi(argv[1]);

    if ((req == 0 && strcmp(argv[1], "0")) || req < 0 || req > MAX_FRIEND_REQUESTS) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "No pending friend request with that ID.");
        return;
    }

    if (!FrndRequests.request[req].active) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "No pending friend request with that ID.");
        return;
    }

    TOX_ERR_FRIEND_ADD err;
    uint32_t friendnum = tox_friend_add_norequest(m, FrndRequests.request[req].key, &err);

    if (err != TOX_ERR_FRIEND_ADD_OK) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to add friend (error %d\n)", err);
        return;
    } else {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Friend request accepted.");
        on_friendadded(m, friendnum, true);
    }

    memset(&FrndRequests.request[req], 0, sizeof(struct friend_request));

    int i;

    for (i = FrndRequests.max_idx; i > 0; --i) {
        if (FrndRequests.request[i - 1].active)
            break;
    }

    FrndRequests.max_idx = i;
    --FrndRequests.num_requests;

}

void cmd_add_helper(ToxWindow *self, Tox *m, const char *id_bin, const char *msg)
{
    const char *errmsg;

    TOX_ERR_FRIEND_ADD err;
    uint32_t f_num = tox_friend_add(m, (uint8_t *) id_bin, (uint8_t *) msg, strlen(msg), &err);

    switch (err) {
        case TOX_ERR_FRIEND_ADD_TOO_LONG:
            errmsg = "Message is too long.";
            break;

        case TOX_ERR_FRIEND_ADD_NO_MESSAGE:
            errmsg = "Please add a message to your request.";
            break;

        case TOX_ERR_FRIEND_ADD_OWN_KEY:
            errmsg = "That appears to be your own ID.";
            break;

        case TOX_ERR_FRIEND_ADD_ALREADY_SENT:
            errmsg = "Friend request has already been sent.";
            break;

        case TOX_ERR_FRIEND_ADD_BAD_CHECKSUM:
            errmsg = "Bad checksum in address.";
            break;

        case TOX_ERR_FRIEND_ADD_SET_NEW_NOSPAM:
            errmsg = "Nospam was different.";
            break;

        case TOX_ERR_FRIEND_ADD_MALLOC:
            errmsg = "Core memory allocation failed.";
            break;

        case TOX_ERR_FRIEND_ADD_OK:
            errmsg = "Friend request sent.";
            on_friendadded(m, f_num, true);
            break;

        case TOX_ERR_FRIEND_ADD_NULL:
        /* fallthrough */
        default:
            errmsg = "Faile to add friend: Unknown error.";
            break;
    }

    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, errmsg);
}

void cmd_add(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    if (argc < 1) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Tox ID or address required.");
        return;
    }

    const char *id = argv[1];
    char msg[MAX_STR_SIZE];

    if (argc > 1) {
        if (argv[2][0] != '\"') {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Message must be enclosed in quotes.");
            return;
        }

        /* remove opening and closing quotes */
        char tmp[MAX_STR_SIZE];
        snprintf(tmp, sizeof(tmp), "%s", &argv[2][1]);
        int len = strlen(tmp) - 1;
        tmp[len] = '\0';
        snprintf(msg, sizeof(msg), "%s", tmp);
    } else {
        char selfname[TOX_MAX_NAME_LENGTH];
        tox_self_get_name(m, (uint8_t *) selfname);

        size_t n_len = tox_self_get_name_size(m);
        selfname[n_len] = '\0';
        snprintf(msg, sizeof(msg), "Hello, my name is %s. Care to Tox?", selfname);
    }

    char id_bin[TOX_ADDRESS_SIZE] = {0};
    uint16_t id_len = (uint16_t) strlen(id);

    /* try to add tox ID */
    if (id_len == 2 * TOX_ADDRESS_SIZE) {
        size_t i;
        char xx[3];
        uint32_t x;

        for (i = 0; i < TOX_ADDRESS_SIZE; ++i) {
            xx[0] = id[2 * i];
            xx[1] = id[2 * i + 1];
            xx[2] = '\0';

            if (sscanf(xx, "%02x", &x) != 1) {
                line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Invalid Tox ID.");
                return;
            }

            id_bin[i] = x;
        }

        cmd_add_helper(self, m, id_bin, msg);
    } else {    /* assume id is a username@domain address and do DNS lookup */
        dns3_lookup(self, m, id_bin, id, msg);
    }
}

void cmd_avatar(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    if (argc < 2 || strlen(argv[1]) < 3) {
        avatar_unset(m);
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Avatar is not set.");
        return;
    }

    if (argv[1][0] != '\"') {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Path must be enclosed in quotes.");
        return;
    }

    /* remove opening and closing quotes */
    char path[MAX_STR_SIZE];
    snprintf(path, sizeof(path), "%s", &argv[1][1]);
    int len = strlen(path) - 1;

    if (len <= 0) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Invalid path.");
        return;
    }

    path[len] = '\0';
    char filename[MAX_STR_SIZE];
    get_file_name(filename, sizeof(filename), path);

    if (avatar_set(m, path, len) == -1) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0,
                      "Failed to set avatar. Avatars must be in PNG format and may not exceed %d bytes.",
                      MAX_AVATAR_FILE_SIZE);
        return;
    }

    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Avatar set to '%s'", filename);
}

void cmd_clear(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    line_info_clear(self->chatwin->hst);
    force_refresh(window);
}

void cmd_connect(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    if (argc != 3) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Require: <ip> <port> <key>");
        return;
    }

    const char *ip = argv[1];
    const char *port = argv[2];
    const char *key = argv[3];

    if (atoi(port) == 0) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Invalid port.");
        return;
    }

    char *binary_string = hex_string_to_bin(key);

    TOX_ERR_BOOTSTRAP err;
    tox_bootstrap(m, ip, atoi(port), (uint8_t *) binary_string, &err);
    tox_add_tcp_relay(m, ip, atoi(port), (uint8_t *) binary_string, &err);
    free(binary_string);

    switch (err) {
        case TOX_ERR_BOOTSTRAP_BAD_HOST:
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Bootstrap failed: Invalid IP.");
            break;

        case TOX_ERR_BOOTSTRAP_BAD_PORT:
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Bootstrap failed: Invalid port.");
            break;

        case TOX_ERR_BOOTSTRAP_NULL:
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Bootstrap failed.");
            break;
        default:
            break;
    }
}

void cmd_decline(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    if (argc < 1) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Request ID required.");
        return;
    }

    int req = atoi(argv[1]);

    if ((req == 0 && strcmp(argv[1], "0")) || req < 0 || req > MAX_FRIEND_REQUESTS) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "No pending friend request with that ID.");
        return;
    }

    if (!FrndRequests.request[req].active) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "No pending friend request with that ID.");
        return;
    }

    memset(&FrndRequests.request[req], 0, sizeof(struct friend_request));

    int i;

    for (i = FrndRequests.max_idx; i > 0; --i) {
        if (FrndRequests.request[i - 1].active)
            break;
    }

    FrndRequests.max_idx = i;
    --FrndRequests.num_requests;
}

void cmd_groupchat(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    if (get_num_active_windows() >= MAX_WINDOWS_NUM) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, RED, " * Warning: Too many windows are open.");
        return;
    }

    if (argc < 1) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Please specify group type: text | audio");
        return;
    }

    uint8_t type;

    if (!strcasecmp(argv[1], "audio"))
        type = TOX_GROUPCHAT_TYPE_AV;
    else if (!strcasecmp(argv[1], "text"))
        type = TOX_GROUPCHAT_TYPE_TEXT;
    else {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Valid group types are: text | audio");
        return;
    }

    int groupnum = -1;

    if (type == TOX_GROUPCHAT_TYPE_TEXT)
        groupnum = tox_add_groupchat(m);
/*#ifdef AUDIO
    else
        groupnum = toxav_add_av_groupchat(m, NULL, NULL);
#endif*/

    if (groupnum == -1) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Group chat instance failed to initialize.");
        return;
    }

    if (init_groupchat_win(prompt, m, groupnum, type) == -1) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Group chat window failed to initialize.");
        tox_del_groupchat(m, groupnum);
        return;
    }

    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Group chat [%d] created.", groupnum);
}

void cmd_log(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    const char *msg;
    struct chatlog *log = self->chatwin->log;

    if (argc == 0) {
        if (log->log_on)
            msg = "Logging for this window is ON. Type \"/log off\" to disable.";
        else
            msg = "Logging for this window is OFF. Type \"/log on\" to enable.";

        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, msg);
        return;
    }

    const char *swch = argv[1];

    if (!strcmp(swch, "1") || !strcmp(swch, "on")) {
        char myid[TOX_ADDRESS_SIZE];
        tox_self_get_address(m, (uint8_t *) myid);

        if (self->is_chat) {
            Friends.list[self->num].logging_on = true;
            log_enable(self->name, myid, Friends.list[self->num].pub_key, log, LOG_CHAT);
        } else if (self->is_prompt) {
            log_enable(self->name, myid, NULL, log, LOG_PROMPT);
        } else if (self->is_groupchat) {
            log_enable(self->name, myid, NULL, log, LOG_GROUP);
        }

        msg = "Logging enabled";
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, msg);
        return;
    } else if (!strcmp(swch, "0") || !strcmp(swch, "off")) {
        if (self->is_chat)
            Friends.list[self->num].logging_on = false;

        log_disable(log);

        msg = "Logging disabled";
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, msg);
        return;
    }

    msg = "Invalid option. Use \"/log on\" and \"/log off\" to toggle logging.";
    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, msg);
}

void cmd_myid(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    char id[TOX_ADDRESS_SIZE * 2 + 1] = {0};
    char address[TOX_ADDRESS_SIZE];
    tox_self_get_address(m, (uint8_t *) address);

    size_t i;

    for (i = 0; i < TOX_ADDRESS_SIZE; ++i) {
        char xx[3];
        snprintf(xx, sizeof(xx), "%02X", address[i] & 0xff);
        strcat(id, xx);
    }

    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "%s", id);
}

void cmd_nick(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    if (argc < 1) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Input required.");
        return;
    }

    char nick[MAX_STR_SIZE];
    size_t len = 0;

    if (argv[1][0] == '\"') {    /* remove opening and closing quotes */
        snprintf(nick, sizeof(nick), "%s", &argv[1][1]);
        len = strlen(nick) - 1;
        nick[len] = '\0';
    } else {
        snprintf(nick, sizeof(nick), "%s", argv[1]);
        len = strlen(nick);
    }

    if (!valid_nick(nick)) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Invalid name.");
        return;
    }

    len = MIN(len, TOXIC_MAX_NAME_LENGTH - 1);
    nick[len] = '\0';

    tox_self_set_name(m, (uint8_t *) nick, len, NULL);
    prompt_update_nick(prompt, nick);

    store_data(m, DATA_FILE);
}

void cmd_note(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    if (argc < 1) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Input required.");
        return;
    }

    if (argv[1][0] != '\"') {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Note must be enclosed in quotes.");
        return;
    }

    /* remove opening and closing quotes */
    char msg[MAX_STR_SIZE];
    snprintf(msg, sizeof(msg), "%s", &argv[1][1]);
    int len = strlen(msg) - 1;
    msg[len] = '\0';

    prompt_update_statusmessage(prompt, m, msg);
}

void cmd_prompt_help(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    help_init_menu(self);
}

void cmd_quit(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    exit_toxic_success(m);
}

void cmd_requests(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    if (FrndRequests.num_requests == 0) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "No pending friend requests.");
        return;
    }

    int i, j;
    int count = 0;

    for (i = 0; i < FrndRequests.max_idx; ++i) {
        if (!FrndRequests.request[i].active)
            continue;

        char id[TOX_PUBLIC_KEY_SIZE * 2 + 1] = {0};

        for (j = 0; j < TOX_PUBLIC_KEY_SIZE; ++j) {
            char d[3];
            snprintf(d, sizeof(d), "%02X", FrndRequests.request[i].key[j] & 0xff);
            strcat(id, d);
        }

        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "%d : %s", i, id);
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "%s", FrndRequests.request[i].msg);

        if (++count < FrndRequests.num_requests)
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "");
    }
}

void cmd_status(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    bool have_note = false;
    const char *errmsg;

    lock_status ();

    if (argc >= 2) {
        have_note = true;
    } else if (argc < 1) {
        errmsg = "Require a status. Statuses are: online, busy and away.";
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, errmsg);
        goto finish;
    }

    const char *status_str = argv[1];
    TOX_USER_STATUS status;

    if (!strcasecmp(status_str, "online"))
        status = TOX_USER_STATUS_NONE;
    else if (!strcasecmp(status_str, "away"))
        status = TOX_USER_STATUS_AWAY;
    else if (!strcasecmp(status_str, "busy"))
        status = TOX_USER_STATUS_BUSY;
    else {
        errmsg = "Invalid status. Valid statuses are: online, busy and away.";
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, errmsg);
        goto finish;
    }

    tox_self_set_status(m, status);
    prompt_update_status(prompt, status);

    if (have_note) {
        if (argv[2][0] != '\"') {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Note must be enclosed in quotes.");
            goto finish;
        }

        /* remove opening and closing quotes */
        char msg[MAX_STR_SIZE];
        snprintf(msg, sizeof(msg), "%s", &argv[2][1]);
        int len = strlen(msg) - 1;
        msg[len] = '\0';

        prompt_update_statusmessage(prompt, m, msg);
    }

finish:
    unlock_status ();
}

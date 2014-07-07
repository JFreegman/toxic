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

extern char *DATA_FILE;
extern ToxWindow *prompt;

extern ToxicFriend friends[MAX_FRIENDS_NUM];

extern char pending_frnd_requests[MAX_FRIENDS_NUM][TOX_CLIENT_ID_SIZE];
extern uint8_t num_frnd_requests;

/* command functions */
void cmd_accept(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    char *msg;

    if (argc != 1) {
        msg = "Invalid syntax.";
        line_info_add(self, NULL, NULL, NULL, msg, SYS_MSG, 0, 0);
        return;
    }

    int req = atoi(argv[1]);

    if ((req == 0 && strcmp(argv[1], "0")) || req >= MAX_FRIENDS_NUM) {
        msg = "No pending friend request with that number.";
        line_info_add(self, NULL, NULL, NULL, msg, SYS_MSG, 0, 0);
        return;
    }

    if (!strlen(pending_frnd_requests[req])) {
        msg = "No pending friend request with that number.";
        line_info_add(self, NULL, NULL, NULL, msg, SYS_MSG, 0, 0);
        return;
    }

    int32_t friendnum = tox_add_friend_norequest(m, (uint8_t *) pending_frnd_requests[req]);

    if (friendnum == -1)
        msg = "Failed to add friend.";
    else {
        msg = "Friend request accepted.";
        on_friendadded(m, friendnum, true);
    }

    memset(&pending_frnd_requests[req], 0, TOX_CLIENT_ID_SIZE);

    int i;

    for (i = num_frnd_requests; i > 0; --i) {
        if (!strlen(pending_frnd_requests[i - 1]))
            break;
    }

    num_frnd_requests = i;
    line_info_add(self, NULL, NULL, NULL, msg, SYS_MSG, 0, 0);
}

void cmd_add_helper(ToxWindow *self, Tox *m, char *id_bin, char *msg)
{
    char *errmsg;
    int32_t f_num = tox_add_friend(m, (uint8_t *) id_bin, (uint8_t *) msg, (uint16_t) strlen(msg));

    switch (f_num) {
        case TOX_FAERR_TOOLONG:
            errmsg = "Message is too long.";
            break;

        case TOX_FAERR_NOMESSAGE:
            errmsg = "Please add a message to your request.";
            break;

        case TOX_FAERR_OWNKEY:
            errmsg = "That appears to be your own ID.";
            break;

        case TOX_FAERR_ALREADYSENT:
            errmsg = "Friend request has already been sent.";
            break;

        case TOX_FAERR_UNKNOWN:
            errmsg = "Undefined error when adding friend.";
            break;

        case TOX_FAERR_BADCHECKSUM:
            errmsg = "Bad checksum in address.";
            break;

        case TOX_FAERR_SETNEWNOSPAM:
            errmsg = "Nospam was different.";
            break;

        default:
            errmsg = "Friend request sent.";
            on_friendadded(m, f_num, true);
            break;
    }

    line_info_add(self, NULL, NULL, NULL, errmsg, SYS_MSG, 0, 0);
}

void cmd_add(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    char *errmsg;

    if (argc < 1) {
        errmsg = "Invalid syntax.";
        line_info_add(self, NULL, NULL, NULL, errmsg, SYS_MSG, 0, 0);
        return;
    }

    char *id = argv[1];
    char msg[MAX_STR_SIZE];

    if (argc > 1) {
        char *temp = argv[2];

        if (temp[0] != '\"') {
            errmsg = "Message must be enclosed in quotes.";
            line_info_add(self, NULL, NULL, NULL, errmsg, SYS_MSG, 0, 0);
            return;
        }

        ++temp;
        temp[strlen(temp) - 1] = L'\0';
        snprintf(msg, sizeof(msg), "%s", temp);
    } else {
        char selfname[TOX_MAX_NAME_LENGTH];
        uint16_t n_len = tox_get_self_name(m, (uint8_t *) selfname);
        selfname[n_len] = '\0';
        snprintf(msg, sizeof(msg), "Hello, my name is %s. Care to Tox?", selfname);
    }

    char id_bin[TOX_FRIEND_ADDRESS_SIZE] = {0};
    uint16_t id_len = (uint16_t) strlen(id);

    /* try to add tox ID */
    if (id_len == 2 * TOX_FRIEND_ADDRESS_SIZE) {
        size_t i;
        char xx[3];
        uint32_t x;

        for (i = 0; i < TOX_FRIEND_ADDRESS_SIZE; ++i) {
            xx[0] = id[2 * i];
            xx[1] = id[2 * i + 1];
            xx[2] = '\0';

            if (sscanf(xx, "%02x", &x) != 1) {
                errmsg = "Invalid ID.";
                line_info_add(self, NULL, NULL, NULL, errmsg, SYS_MSG, 0, 0);
                return;
            }

            id_bin[i] = x;
        }

        cmd_add_helper(self, m, id_bin, msg);
    } else {    /* assume id is a username@domain address and do DNS lookup */
        dns3_lookup(self, m, id_bin, id, msg);
    }
}

void cmd_clear(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    line_info_clear(self->chatwin->hst);
    wclear(window);
    endwin();
    refresh();
}

void cmd_connect(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    char *errmsg;

    /* check arguments */
    if (argc != 3) {
        errmsg = "Invalid syntax.";
        line_info_add(self, NULL, NULL, NULL, errmsg, SYS_MSG, 0, 0);
        return;
    }

    const char *ip = argv[1];
    const char *port = argv[2];
    const char *key = argv[3];

    if (atoi(port) == 0) {
        errmsg = "Invalid syntax.";
        line_info_add(self, NULL, NULL, NULL, errmsg, SYS_MSG, 0, 0);
        return;
    }

    char *binary_string = hex_string_to_bin(key);
    tox_bootstrap_from_address(m, ip, TOX_ENABLE_IPV6_DEFAULT, htons(atoi(port)), (uint8_t *) binary_string);
    free(binary_string);
}

void cmd_groupchat(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    char *errmsg;

    if (get_num_active_windows() >= MAX_WINDOWS_NUM) {
        errmsg = " * Warning: Too many windows are open.";
        line_info_add(self, NULL, NULL, NULL, errmsg, SYS_MSG, 0, RED);
        return;
    }

    int groupnum = tox_add_groupchat(m);

    if (groupnum == -1) {
        errmsg = "Group chat instance failed to initialize.";
        line_info_add(self, NULL, NULL, NULL, errmsg, SYS_MSG, 0, 0);
        return;
    }

    if (init_groupchat_win(prompt, m, groupnum) == -1) {
        errmsg = "Group chat window failed to initialize.";
        line_info_add(self, NULL, NULL, NULL, errmsg, SYS_MSG, 0, 0);
        tox_del_groupchat(m, groupnum);
        return;
    }

    char msg[MAX_STR_SIZE];
    snprintf(msg, sizeof(msg), "Group chat created as %d.", groupnum);
    line_info_add(self, NULL, NULL, NULL, msg, SYS_MSG, 0, 0);
}

void cmd_log(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    char *msg;
    struct chatlog *log = self->chatwin->log;

    if (argc == 0) {
        if (log->log_on)
            msg = "Logging for this window is ON. Type \"/log off\" to disable.";
        else
            msg = "Logging for this window is OFF. Type \"/log on\" to enable.";

        line_info_add(self, NULL, NULL, NULL, msg, SYS_MSG, 0, 0);
        return;
    }

    char *swch = argv[1];

    if (!strcmp(swch, "1") || !strcmp(swch, "on")) {

        if (self->is_chat) {
            friends[self->num].logging_on = true;
            log_enable(self->name, friends[self->num].pub_key, log);
        } else if (self->is_prompt) {
            char myid[TOX_FRIEND_ADDRESS_SIZE];
            tox_get_address(m, (uint8_t *) myid);
            log_enable(self->name, myid, log);
        } else if (self->is_groupchat) {
            log_enable(self->name, NULL, log);
        }

        msg = "Logging enabled";
        line_info_add(self, NULL, NULL, NULL, msg, SYS_MSG, 0, 0);
        return;
    } else if (!strcmp(swch, "0") || !strcmp(swch, "off")) {
        if (self->is_chat)
            friends[self->num].logging_on = false;

        log_disable(log);

        msg = "Logging disabled";
        line_info_add(self, NULL, NULL, NULL, msg, SYS_MSG, 0, 0);
        return;
    }

    msg = "Invalid option. Use \"/log on\" and \"/log off\" to toggle logging.";
    line_info_add(self, NULL, NULL, NULL, msg, SYS_MSG, 0, 0);
}

void cmd_myid(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    char id[TOX_FRIEND_ADDRESS_SIZE * 2 + 1] = {0};
    char address[TOX_FRIEND_ADDRESS_SIZE];
    tox_get_address(m, (uint8_t *) address);

    size_t i;

    for (i = 0; i < TOX_FRIEND_ADDRESS_SIZE; ++i) {
        char xx[3];
        snprintf(xx, sizeof(xx), "%02X", address[i] & 0xff);
        strcat(id, xx);
    }

    line_info_add(self, NULL, NULL, NULL, id, SYS_MSG, 0, 0);
}

void cmd_nick(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    char *errmsg;

    /* check arguments */
    if (argc < 1) {
        errmsg = "Invalid name.";
        line_info_add(self, NULL, NULL, NULL, errmsg, SYS_MSG, 0, 0);
        return;
    }

    char *nick = argv[1];
    int len = strlen(nick);

    if (nick[0] == '\"') {
        ++nick;
        len -= 2;
        nick[len] = '\0';
    }

    if (!valid_nick(nick)) {
        errmsg = "Invalid name.";
        line_info_add(self, NULL, NULL, NULL, errmsg, SYS_MSG, 0, 0);
        return;
    }

    len = MIN(len, TOXIC_MAX_NAME_LENGTH - 1);
    nick[len] = '\0';

    tox_set_name(m, (uint8_t *) nick, (uint16_t) len);
    prompt_update_nick(prompt, nick);

    store_data(m, DATA_FILE);
}

void cmd_note(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    char *errmsg;

    if (argc < 1) {
        errmsg = "Wrong number of arguments.";
        line_info_add(self, NULL, NULL, NULL, errmsg, SYS_MSG, 0, 0);
        return;
    }

    char *msg = argv[1];

    if (msg[0] != '\"') {
        errmsg = "Note must be enclosed in quotes.";
        line_info_add(self, NULL, NULL, NULL, errmsg, SYS_MSG, 0, 0);
        return;
    }

    ++msg;
    msg[strlen(msg) - 1] = '\0';
    int len = strlen(msg);
    tox_set_status_message(m, (uint8_t *) msg, (uint16_t) len);
    prompt_update_statusmessage(prompt, msg);
}

void cmd_prompt_help(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    help_init_menu(self);
}

void cmd_quit(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    exit_toxic_success(m);
}

void cmd_status(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    char *msg = NULL;
    char *errmsg;

    if (argc >= 2) {
        msg = argv[2];

        if (msg[0] != '\"') {
            errmsg = "Note must be enclosed in quotes.";
            line_info_add(self, NULL, NULL, NULL, errmsg, SYS_MSG, 0, 0);
            return;
        }
    } else if (argc != 1) {
        errmsg = "Wrong number of arguments.";
        line_info_add(self, NULL, NULL, NULL, errmsg, SYS_MSG, 0, 0);
        return;
    }

    char *status = argv[1];
    str_to_lower(status);

    TOX_USERSTATUS status_kind;

    if (!strcmp(status, "online"))
        status_kind = TOX_USERSTATUS_NONE;
    else if (!strcmp(status, "away"))
        status_kind = TOX_USERSTATUS_AWAY;
    else if (!strcmp(status, "busy"))
        status_kind = TOX_USERSTATUS_BUSY;
    else {
        errmsg = "Invalid status. Valid statuses are: online, busy and away.";
        line_info_add(self, NULL, NULL, NULL, errmsg, SYS_MSG, 0, 0);
        return;
    }

    tox_set_user_status(m, status_kind);
    prompt_update_status(prompt, status_kind);

    if (msg != NULL) {
        ++msg;
        msg[strlen(msg) - 1] = L'\0'; /* remove opening and closing quotes */
        int len = strlen(msg);
        tox_set_status_message(m, (uint8_t *) msg, (uint16_t) len);
        prompt_update_statusmessage(prompt, msg);
    }
}

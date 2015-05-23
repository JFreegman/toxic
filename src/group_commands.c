/*  group_commands.c
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

#include <string.h>

#include "toxic.h"
#include "windows.h"
#include "line_info.h"
#include "misc_tools.h"
#include "log.h"
#include "groupchat.h"

void cmd_chatid(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    char chatid[TOX_GROUP_CHAT_ID_SIZE * 2 + 1] = {0};
    char chat_public_key[TOX_GROUP_CHAT_ID_SIZE];

    if (tox_group_get_chat_id(m, self->num, (uint8_t *) chat_public_key) == -1) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Error retreiving chat id.");
        return;
    }

    size_t i;

    for (i = 0; i < TOX_GROUP_CHAT_ID_SIZE; ++i) {
        char xx[3];
        snprintf(xx, sizeof(xx), "%02X", chat_public_key[i] & 0xff);
        strcat(chatid, xx);
    }

    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "%s", chatid);
}

void cmd_ignore(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    if (argc < 1) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Who do you want to ignore?");
        return;
    }

    const char *nick = argv[1];
    int peernum = group_get_nick_peernumber(self->num, nick);

    if (peernum == -1) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Peer '%s' does not exist", nick);
        return;
    }

    if (tox_group_toggle_ignore(m, self->num, peernum, 1) == -1) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to ignore %s", nick);
        return;
    }

    char timefrmt[TIME_STR_SIZE];
    get_time_str(timefrmt, sizeof(timefrmt));

    line_info_add(self, timefrmt, NULL, NULL, SYS_MSG, 1, BLUE, "-!- Ignoring %s", nick);
}

void cmd_set_passwd(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    const char *passwd = NULL;
    size_t len = 0;

    if (argc > 0) {
        passwd = argv[1];
        len = strlen(passwd);
    }

    if (len > TOX_MAX_GROUP_PASSWD_SIZE) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Password exceeds %d character limit", TOX_MAX_GROUP_PASSWD_SIZE);
        return;
    }

    int ret = tox_group_set_password(m, self->num, (uint8_t *) passwd, len);

    switch (ret) {
        case 0: {
            if (len > 0)
                line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Password has been set to %s", passwd);
            else
                line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Password has been unset");
            return;
        }
        case -2: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "You do not have permission to set the password");
            return;
        }
        default: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Error setting password");
            return;
        }
    }
}

void cmd_set_privacy(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    const char *pstate_str = NULL;
    TOX_GROUP_PRIVACY_STATE privacy_state;

    if (argc < 1) {
        privacy_state = tox_group_get_privacy_state(m, self->num);

        if (privacy_state == TOX_GP_INVALID) {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to retrieve privacy state");
            return;
        }

        pstate_str = privacy_state == TOX_GP_PRIVATE ? "private" : "public";
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Privacy state is set to %s.", pstate_str);
        return;
    }

    pstate_str = argv[1];

    if (strcasecmp(pstate_str, "private") != 0 && strcasecmp(pstate_str, "public") != 0) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Privacy state must be \"private\" or \"public\".");
        return;
    }

    privacy_state = strcasecmp(pstate_str, "private") == 0 ? TOX_GP_PRIVATE : TOX_GP_PUBLIC;

    int ret = tox_group_set_privacy_state(m, self->num, privacy_state);

    switch (ret) {
        case 0: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Privacy state has been set to %s.", pstate_str);
            return;
        }
        case -2: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "You do not have permission to set the privacy state.");
            return;
        }
        default: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Error setting privacy state.");
            return;
        }
    }
}

void cmd_rejoin(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    if (tox_group_reconnect(m, self->num) == -1)
        return;

    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Reconnecting to group...");
}

void cmd_set_topic(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    if (argc < 1) {
        char cur_topic[MAX_STR_SIZE];
        int tlen = tox_group_get_topic(m, self->num, (uint8_t *) cur_topic);

        if (tlen > 0) {
            cur_topic[tlen] = '\0';
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Topic is set to: %s", cur_topic);
        } else {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Topic is not set");
        }

        return;
    }

    const char *topic = argv[1];

    if (tox_group_set_topic(m, self->num, (uint8_t *) topic, strlen(topic)) != 0) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to set topic.");
        return;
    }

    char timefrmt[TIME_STR_SIZE];
    char selfnick[TOX_MAX_NAME_LENGTH];

    get_time_str(timefrmt, sizeof(timefrmt));

    tox_self_get_name(m, (uint8_t *) selfnick);
    size_t sn_len = tox_self_get_name_size(m);
    selfnick[sn_len] = '\0';

    line_info_add(self, timefrmt, NULL, NULL, SYS_MSG, 1, MAGENTA, "-!- You set the topic to: %s", topic);

    char tmp_event[MAX_STR_SIZE];
    snprintf(tmp_event, sizeof(tmp_event), "set topic to %s", topic);
    write_to_log(tmp_event, selfnick, self->chatwin->log, true);
}

void cmd_unignore(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    if (argc < 1) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Who do you want to unignore?");
        return;
    }

    const char *nick = argv[1];
    int peernum = group_get_nick_peernumber(self->num, nick);

    if (peernum == -1) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Peer '%s' does not exist", nick);
        return;
    }

    if (tox_group_toggle_ignore(m, self->num, peernum, 0) == -1) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to unignore %s", nick);
        return;
    }

    char timefrmt[TIME_STR_SIZE];
    get_time_str(timefrmt, sizeof(timefrmt));

    line_info_add(self, timefrmt, NULL, NULL, SYS_MSG, 1, BLUE, "-!- You are no longer ignoring %s", nick);
}

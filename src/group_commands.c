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
    int sn_len = tox_group_get_self_name(m, self->num, (uint8_t *) selfnick);
    selfnick[sn_len] = '\0';

    line_info_add(self, timefrmt, NULL, NULL, SYS_MSG, 1, MAGENTA, "-!- You set the topic to: %s", topic);

    char tmp_event[MAX_STR_SIZE];
    snprintf(tmp_event, sizeof(tmp_event), "set topic to %s", topic);
    write_to_log(tmp_event, selfnick, self->chatwin->log, true);
}

void cmd_chatid(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    char chatid[TOX_GROUP_CHAT_ID_SIZE * 2 + 1] = {0};
    char chat_public_key[TOX_GROUP_CHAT_ID_SIZE];
    tox_group_get_invite_key(m, self->num, (uint8_t *) chat_public_key);

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

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
#include <stdlib.h>

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
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Peer name must be specified.");
        return;
    }

    const char *nick = argv[1];
    int peernum = group_get_nick_peernumber(self->num, nick);

    if (peernum == -1) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0,  "Invalid peer name '%s'.", nick);
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

static void cmd_kickban_helper(ToxWindow *self, Tox *m, const char *nick, bool set_ban)
{
    int peernumber = group_get_nick_peernumber(self->num, nick);

    if (peernumber == -1) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0,  "Invalid peer name '%s'.", nick);
        return;
    }

    const char *type_str = set_ban ? "ban" : "kick";
    int ret = tox_group_remove_peer(m, self->num, (uint32_t) peernumber, set_ban);

    switch (ret) {
        case 0: {
            type_str = set_ban ? "banned" : "kicked";
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 1, RED,  "You have %s %s from the group.", type_str, nick);
            return;
        }
        case -1: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0,  "Failed to %s %s from the group.", type_str, nick);
            return;
        }
        case -2: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0,  "You do not have permission to %s %s.", type_str, nick);
            return;
        }
    }
}

void cmd_kick(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    if (argc < 1) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Peer name must be specified.");
        return;
    }

    cmd_kickban_helper(self, m, argv[1], false);
}

void cmd_ban(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    if (argc < 1) {
        int num_banned = tox_group_get_ban_list_size(m, self->num);

        if (num_banned == -1) {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to get the ban list.");
            return;
        }

        if (num_banned == 0) {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Ban list is empty.");
            return;
        }

        struct Tox_Group_Ban ban_list[num_banned];

        if (tox_group_get_ban_list(m, self->num, ban_list) == -1) {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to get the ban list.");
            return;
        }

        uint16_t i;

        for (i = 0; i < num_banned; ++i) {
            ban_list[i].nick[ban_list[i].nick_len] = '\0';
            struct tm tm_set = *localtime((const time_t *) &ban_list[i].time_set);
            char time_str[64];
            strftime(time_str, sizeof(time_str), "%e %b %Y %H:%M:%S%p", &tm_set);
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "ID %d : %s [Set:%s]", ban_list[i].id,
                                                                  ban_list[i].nick, time_str);
        }

        return;
    }

    cmd_kickban_helper(self, m, argv[1], true);
}

void cmd_unban(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    if (argc < 1) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Ban ID must be specified.");
        return;
    }

    int ban_id = atoi(argv[1]);

    if (ban_id == 0 && strcmp(argv[1], "0")) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Ban ID must be a non-negative interger.");
        return;
    }

    int ret = tox_group_remove_ban_entry(m, self->num, ban_id);

    switch (ret) {
        case 0: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Ban list entry with id %d has been removed.", ban_id);
            return;
        }
        case -2: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "You do not have permission to unban peers.");
            return;
        }
        default: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Ban ID does not exist.");
            return;
        }
    }
}

void cmd_mod(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    if (argc < 1) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Peer name must be specified.");
        return;
    }

    const char *nick = argv[1];
    int peernumber = group_get_nick_peernumber(self->num, nick);

    if (peernumber == -1) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0,  "Invalid peer name '%s'.", nick);
        return;
    }

    int ret = tox_group_set_peer_role(m, self->num, peernumber, TOX_GR_MODERATOR);

    switch (ret) {
        case 0: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 1, BLUE, "You have promoted %s to moderator.", nick);
            return;
        }
        case -2: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0,  "You do not have permission to promote moderators.");
            return;
        }
        default: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0,  "Failed to promote peer to moderator");
            return;
        }
    }
}

void cmd_unmod(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    if (argc < 1) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Peer name must be specified.");
        return;
    }

    const char *nick = argv[1];
    int peernumber = group_get_nick_peernumber(self->num, nick);

    if (peernumber == -1) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0,  "Invalid peer name '%s'.", nick);
        return;
    }

    if (tox_group_get_peer_role(m, self->num, peernumber) != TOX_GR_MODERATOR) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "%s is not a moderator", nick);
        return;
    }

    int ret = tox_group_set_peer_role(m, self->num, peernumber, TOX_GR_USER);

    switch (ret) {
        case 0: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 1, BLUE, "You have revoked moderator powers from %s.", nick);
            return;
        }
        case -1: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0,  "Failed to revoke moderator powers from %s.", nick);
            return;
        }
        case -2: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0,  "Nice try.");
            return;
        }
    }
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

void cmd_set_peerlimit(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    int maxpeers = 0;

    if (argc < 1) {
        maxpeers = tox_group_get_peer_limit(m, self->num);

        if (maxpeers == -1) {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to retrieve peer limit");
            return;
        }

        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Peer limit is set to %d", maxpeers);
        return;
    }

    maxpeers = atoi(argv[1]);

    if (maxpeers <= 0) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Peer limit must be a value greater than 0");
        return;
    }

    int ret = tox_group_set_peer_limit(m, self->num, (uint32_t) maxpeers);

    switch (ret) {
        case 0: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Peer limit has been set to %d", maxpeers);
            return;
        }
        case -2: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "You do not have permission to set the peer limit.");
            return;
        }
        default: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to set the peer limit");
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

void cmd_silence(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    if (argc < 1) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Peer name must be specified.");
        return;
    }

    const char *nick = argv[1];
    int peernumber = group_get_nick_peernumber(self->num, nick);

    if (peernumber == -1) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0,  "Invalid peer name '%s'.", nick);
        return;
    }

    int ret = tox_group_set_peer_role(m, self->num, peernumber, TOX_GR_OBSERVER);

    switch (ret) {
        case 0: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 1, BLUE, "You have silenced %s", nick);
            return;
        }
        case -2: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "You do not have permission to silence %s.", nick);
            return;
        }
        default: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to silence %s.", nick);
            return;
        }
    }
}

void cmd_unsilence(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    if (argc < 1) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Peer name must be specified.");
        return;
    }

    const char *nick = argv[1];
    int peernumber = group_get_nick_peernumber(self->num, nick);

    if (peernumber == -1) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0,  "Invalid peer name '%s'.", nick);
        return;
    }

    if (tox_group_get_peer_role(m, self->num, peernumber) != TOX_GR_OBSERVER) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "%s is not silenced", nick);
        return;
    }

    int ret = tox_group_set_peer_role(m, self->num, peernumber, TOX_GR_USER);

    switch (ret) {
        case 0: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 1, BLUE, "You have unsilenced %s", nick);
            return;
        }
        case -2: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "You do not have permission to unsilence %s.", nick);
            return;
        }
        default: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to unsilence %s.", nick);
            return;
        }
    }
}

void cmd_rejoin(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    if (tox_group_reconnect(m, self->num) == -1) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to rejoin group.");
        return;
    }

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
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0,  "Invalid peer name '%s'.", nick);
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

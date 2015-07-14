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

    TOX_ERR_GROUP_STATE_QUERIES err;

    if (!tox_group_get_chat_id(m, self->num, (uint8_t *) chat_public_key, &err)) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to retrieve the Chat ID (error %d).", err);
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

    TOX_ERR_GROUP_TOGGLE_IGNORE err;
    if (!tox_group_toggle_ignore(m, self->num, peernum, true, &err)) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to ignore %s (error %d).", nick, err);
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

    TOX_ERR_GROUP_MOD_REMOVE_PEER err;
    tox_group_mod_remove_peer(m, self->num, (uint32_t) peernumber, set_ban, &err);

    switch (err) {
        case TOX_ERR_GROUP_MOD_REMOVE_PEER_OK: {
            type_str = set_ban ? "banned" : "kicked";
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 1, RED,  "You have %s %s from the group.", type_str, nick);
            return;
        }
        case TOX_ERR_GROUP_MOD_REMOVE_PEER_PERMISSIONS: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0,  "You do not have permission to %s %s.", type_str, nick);
            return;
        }
        default: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0,  "Failed to %s %s from the group (error %d).", type_str, nick, err);
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
    TOX_ERR_GROUP_BAN_QUERY err;

    if (argc < 1) {
        size_t num_banned = tox_group_ban_get_list_size(m, self->num, &err);

        if (err != TOX_ERR_GROUP_BAN_QUERY_OK) {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to get the ban list size (error %d).", err);
            return;
        }

        if (num_banned == 0) {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Ban list is empty.");
            return;
        }

        uint16_t ban_list[num_banned];

        if (!tox_group_ban_get_list(m, self->num, ban_list, &err)) {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to get the ban list (error %d).", err);
            return;
        }

        uint16_t i;

        for (i = 0; i < num_banned; ++i) {
            uint16_t id = ban_list[i];
            size_t len = tox_group_ban_get_name_size(m, self->num, id, &err);

            if (err != TOX_ERR_GROUP_BAN_QUERY_OK) {
                line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to retrieve name length for ban %d (error %d).", id, err);
                continue;
            }

            char tmp_nick[len];

            if (!tox_group_ban_get_name(m, self->num, id, (uint8_t *) tmp_nick, &err)) {
                line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to retrieve name for ban %d (error %d).", id, err);
                continue;
            }

            char nick[len + 1];
            copy_tox_str(nick, sizeof(nick), tmp_nick, len);

            uint64_t time_set = tox_group_ban_get_time_set(m, self->num, id, &err);

            if (err != TOX_ERR_GROUP_BAN_QUERY_OK) {
                line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to retrieve timestamp for ban %d (error %d).", id, err);
                continue;
            }

            struct tm tm_set = *localtime((const time_t *) &time_set);
            char time_str[64];
            strftime(time_str, sizeof(time_str), "%e %b %Y %H:%M:%S%p", &tm_set);
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "ID %d : %s [Set:%s]", id, nick, time_str);
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

    TOX_ERR_GROUP_MOD_REMOVE_BAN err;
    tox_group_mod_remove_ban(m, self->num, ban_id, &err);

    switch (err) {
        case TOX_ERR_GROUP_MOD_REMOVE_BAN_OK: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Ban list entry with id %d has been removed.", ban_id);
            return;
        }
        case TOX_ERR_GROUP_MOD_REMOVE_BAN_PERMISSIONS: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "You do not have permission to unban peers.");
            return;
        }
        case TOX_ERR_GROUP_MOD_REMOVE_BAN_FAIL_ACTION: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Ban ID does not exist.");
            return;
        }
        default: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to remove ban list entry (error %d).", err);
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

    TOX_ERR_GROUP_MOD_SET_ROLE err;
    tox_group_mod_set_role(m, self->num, peernumber, TOX_GROUP_ROLE_MODERATOR, &err);

    switch (err) {
        case TOX_ERR_GROUP_MOD_SET_ROLE_OK: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 1, BLUE, "You have promoted %s to moderator.", nick);
            return;
        }
        case TOX_ERR_GROUP_MOD_SET_ROLE_PERMISSIONS: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0,  "You do not have permission to promote moderators.");
            return;
        }
        case TOX_ERR_GROUP_MOD_SET_ROLE_ASSIGNMENT: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0,  "This peer is already a moderator.");
            return;
        }
        default: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0,  "Failed to promote peer to moderator (error %d).", err);
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

    if (tox_group_peer_get_role(m, self->num, peernumber, NULL) != TOX_GROUP_ROLE_MODERATOR) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "%s is not a moderator", nick);
        return;
    }

    TOX_ERR_GROUP_MOD_SET_ROLE err;
    tox_group_mod_set_role(m, self->num, peernumber, TOX_GROUP_ROLE_USER, &err);

    switch (err) {
        case TOX_ERR_GROUP_MOD_SET_ROLE_OK: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 1, BLUE, "You have revoked moderator powers from %s.", nick);
            return;
        }
        case TOX_ERR_GROUP_MOD_SET_ROLE_PERMISSIONS: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0,  "Nice try.");
            return;
        }
        default: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0,  "Failed to revoke moderator powers from %s (error %d).", nick, err);
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

    TOX_ERR_GROUP_FOUNDER_SET_PASSWORD err;
    tox_group_founder_set_password(m, self->num, (uint8_t *) passwd, len, &err);

    switch (err) {
        case TOX_ERR_GROUP_FOUNDER_SET_PASSWORD_OK: {
            if (len > 0)
                line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Password has been set to %s.", passwd);
            else
                line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Password has been unset.");
            return;
        }
        case TOX_ERR_GROUP_FOUNDER_SET_PASSWORD_TOO_LONG: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Password length must not exceed %d.", TOX_GROUP_MAX_PASSWORD_SIZE);
            return;
        }
        case TOX_ERR_GROUP_FOUNDER_SET_PASSWORD_PERMISSIONS: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "You do not have permission to set the password.");
            return;
        }
        default: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to set password (error %d).", err);
            return;
        }
    }
}

void cmd_set_peerlimit(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    int maxpeers = 0;

    if (argc < 1) {
        TOX_ERR_GROUP_STATE_QUERIES err;
        uint32_t maxpeers = tox_group_get_peer_limit(m, self->num, &err);

        if (err != TOX_ERR_GROUP_STATE_QUERIES_OK) {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to retrieve peer limit (error %d).", err);
            return;
        }

        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Peer limit is set to %d", maxpeers);
        return;
    }

    maxpeers = atoi(argv[1]);

    if (maxpeers <= 0) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Peer limit must be a value greater than 0.");
        return;
    }

    TOX_ERR_GROUP_FOUNDER_SET_PEER_LIMIT err;
    tox_group_founder_set_peer_limit(m, self->num, maxpeers, &err);

    switch (err) {
        case TOX_ERR_GROUP_FOUNDER_SET_PEER_LIMIT_OK: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Peer limit has been set to %d.", maxpeers);
            return;
        }
        case TOX_ERR_GROUP_FOUNDER_SET_PEER_LIMIT_PERMISSIONS: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "You do not have permission to set the peer limit.");
            return;
        }
        default: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to set the peer limit (error %d).", err);
            return;
        }
    }
}

void cmd_set_privacy(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    const char *pstate_str = NULL;
    TOX_GROUP_PRIVACY_STATE privacy_state;

    if (argc < 1) {
        TOX_ERR_GROUP_STATE_QUERIES err;
        privacy_state = tox_group_get_privacy_state(m, self->num, &err);

        if (err != TOX_ERR_GROUP_STATE_QUERIES_OK) {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to retrieve privacy state (error %d).", err);
            return;
        }

        pstate_str = privacy_state == TOX_GROUP_PRIVACY_STATE_PRIVATE ? "private" : "public";
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Privacy state is set to %s.", pstate_str);
        return;
    }

    pstate_str = argv[1];

    if (strcasecmp(pstate_str, "private") != 0 && strcasecmp(pstate_str, "public") != 0) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Privacy state must be \"private\" or \"public\".");
        return;
    }

    privacy_state = strcasecmp(pstate_str, "private") == 0 ? TOX_GROUP_PRIVACY_STATE_PRIVATE : TOX_GROUP_PRIVACY_STATE_PUBLIC;

    TOX_ERR_GROUP_FOUNDER_SET_PRIVACY_STATE err;
    tox_group_founder_set_privacy_state(m, self->num, privacy_state, &err);

    switch (err) {
        case TOX_ERR_GROUP_FOUNDER_SET_PRIVACY_STATE_OK: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Privacy state has been set to %s.", pstate_str);
            return;
        }
        case TOX_ERR_GROUP_FOUNDER_SET_PRIVACY_STATE_PERMISSIONS: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "You do not have permission to set the privacy state.");
            return;
        }
        default: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Error setting privacy state (error %d).", err);
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

    TOX_ERR_GROUP_MOD_SET_ROLE err;
    tox_group_mod_set_role(m, self->num, peernumber, TOX_GROUP_ROLE_OBSERVER, &err);

    switch (err) {
        case TOX_ERR_GROUP_MOD_SET_ROLE_OK: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 1, BLUE, "You have silenced %s", nick);
            return;
        }
        case TOX_ERR_GROUP_MOD_SET_ROLE_PERMISSIONS: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "You do not have permission to silence %s.", nick);
            return;
        }
        default: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to silence %s (error %d).", nick, err);
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

    if (tox_group_peer_get_role(m, self->num, peernumber, NULL) != TOX_GROUP_ROLE_OBSERVER) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "%s is not silenced.", nick);
        return;
    }

    TOX_ERR_GROUP_MOD_SET_ROLE err;
    tox_group_mod_set_role(m, self->num, peernumber, TOX_GROUP_ROLE_USER, &err);

    switch (err) {
        case TOX_ERR_GROUP_MOD_SET_ROLE_OK: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 1, BLUE, "You have unsilenced %s.", nick);
            return;
        }
        case TOX_ERR_GROUP_MOD_SET_ROLE_PERMISSIONS: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "You do not have permission to unsilence %s.", nick);
            return;
        }
        default: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to unsilence %s (error %d).", nick, err);
            return;
        }
    }
}

void cmd_rejoin(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    TOX_ERR_GROUP_RECONNECT err;
    if (!tox_group_reconnect(m, self->num, &err)) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to rejoin group (error %d).", err);
        return;
    }

    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Reconnecting to group...");
}

void cmd_set_topic(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    if (argc < 1) {
        TOX_ERR_GROUP_STATE_QUERIES err;
        size_t tlen = tox_group_get_topic_size(m, self->num, &err);

        if (err != TOX_ERR_GROUP_STATE_QUERIES_OK) {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to retrieve topic length (error %d).", err);
            return;
        }

        if (tlen > 0) {
            char cur_topic[tlen];

            if (!tox_group_get_topic(m, self->num, (uint8_t *) cur_topic, &err)) {
                line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to retrieve topic (error %d).", err);
                return;
            }

            cur_topic[tlen] = '\0';
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Topic is set to: %s", cur_topic);
        } else {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Topic is not set.");
        }

        return;
    }

    const char *topic = argv[1];

    TOX_ERR_GROUP_TOPIC_SET err;
    tox_group_set_topic(m, self->num, (uint8_t *) topic, strlen(topic), &err);

    switch (err) {
        case TOX_ERR_GROUP_TOPIC_SET_OK: {
            /* handled below switch */
            break;
        }
        case TOX_ERR_GROUP_TOPIC_SET_TOO_LONG: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Topic length must not exceed %d.", TOX_GROUP_MAX_TOPIC_LENGTH);
            return;
        }
        case TOX_ERR_GROUP_TOPIC_SET_PERMISSIONS: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "You do not have permission to set the topic.");
            return;
        }
        default: {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to set the topic (error %d).", err);
            return;
        }
    }

    char timefrmt[TIME_STR_SIZE];
    get_time_str(timefrmt, sizeof(timefrmt));

    TOX_ERR_GROUP_SELF_QUERY sn_err;
    size_t sn_len = tox_group_self_get_name_size(m, self->num, &sn_err);
    char selfnick[sn_len];

    if (!tox_group_self_get_name(m, self->num, (uint8_t *) selfnick, &sn_err)) {
         line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to retrieve your own name (error %d).", sn_err);
         return;
    }

    selfnick[sn_len] = '\0';

    line_info_add(self, timefrmt, NULL, NULL, SYS_MSG, 1, MAGENTA, "-!- You set the topic to: %s.", topic);

    char tmp_event[MAX_STR_SIZE];
    snprintf(tmp_event, sizeof(tmp_event), "set topic to %s", topic);
    write_to_log(tmp_event, selfnick, self->chatwin->log, true);
}

void cmd_unignore(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    if (argc < 1) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Peer must be specified.");
        return;
    }

    const char *nick = argv[1];
    int peernum = group_get_nick_peernumber(self->num, nick);

    if (peernum == -1) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0,  "Invalid peer name '%s'.", nick);
        return;
    }

    TOX_ERR_GROUP_TOGGLE_IGNORE err;
    if (!tox_group_toggle_ignore(m, self->num, peernum, false, &err)) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to unignore %s (error %d).", nick, err);
        return;
    }

    char timefrmt[TIME_STR_SIZE];
    get_time_str(timefrmt, sizeof(timefrmt));

    line_info_add(self, timefrmt, NULL, NULL, SYS_MSG, 1, BLUE, "-!- You are no longer ignoring %s", nick);
}

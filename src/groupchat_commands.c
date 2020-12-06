/*  groupchat_commands.c
 *
 *
 *  Copyright (C) 2020 Toxic All Rights Reserved.
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
#include "groupchats.h"

extern GroupChat groupchats[MAX_GROUPCHAT_NUM];

void cmd_chatid(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    char chatid[TOX_GROUP_CHAT_ID_SIZE * 2 + 1] = {0};
    char chat_public_key[TOX_GROUP_CHAT_ID_SIZE];

    TOX_ERR_GROUP_STATE_QUERIES err;

    if (!tox_group_get_chat_id(m, self->num, (uint8_t *) chat_public_key, &err)) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to retrieve the Chat ID (error %d).", err);
        return;
    }

    size_t i;

    for (i = 0; i < TOX_GROUP_CHAT_ID_SIZE; ++i) {
        char xx[3];
        snprintf(xx, sizeof(xx), "%02X", chat_public_key[i] & 0xff);
        strcat(chatid, xx);
    }

    line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "%s", chatid);
}

void cmd_disconnect(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    Tox_Err_Group_Disconnect err;
    tox_group_disconnect(m, self->num, &err);

    switch (err) {
        case TOX_ERR_GROUP_DISCONNECT_OK: {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0,  "Disconnected from group. Type '/rejoin' to reconnect.");
            return;
        }

        case TOX_ERR_GROUP_DISCONNECT_ALREADY_DISCONNECTED: {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0,  "Already disconnected. Type '/rejoin' to connect.");
            return;
        }

        default: {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0,  "Failed to disconnect from group. Error: %d", err);
            return;
        }
    }
}

void cmd_ignore(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    if (argc < 1) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Peer name must be specified.");
        return;
    }

    const char *nick = argv[1];
    uint32_t peer_id;

    if (group_get_nick_peer_id(self->num, nick, &peer_id) == -1) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0,  "Invalid peer name '%s'.", nick);
        return;
    }

    TOX_ERR_GROUP_TOGGLE_IGNORE err;
    tox_group_toggle_ignore(m, self->num, peer_id, true, &err);

    switch (err) {
        case TOX_ERR_GROUP_TOGGLE_IGNORE_OK: {
            break;
        }

        case TOX_ERR_GROUP_TOGGLE_IGNORE_SELF: {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "You cannot ignore yourself.");
            return;
        }

        default: {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to toggle ignore on %s (error %d).", nick, err);
            return;
        }
    }

    line_info_add(self, true, NULL, NULL, SYS_MSG, 1, BLUE, "-!- Ignoring %s", nick);
}

void cmd_kick(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    if (argc < 1) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Peer name must be specified.");
        return;
    }

    const char *nick = argv[1];

    uint32_t target_peer_id;

    if (group_get_nick_peer_id(self->num, nick, &target_peer_id) == -1) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0,  "Invalid peer name '%s'.", nick);
        return;
    }

    TOX_ERR_GROUP_MOD_KICK_PEER err;
    tox_group_mod_kick_peer(m, self->num, target_peer_id, &err);

    switch (err) {
        case TOX_ERR_GROUP_MOD_KICK_PEER_OK: {
            char self_nick[TOX_MAX_NAME_LENGTH + 1];
            get_group_self_nick_truncate(m, self_nick, self->num);

            line_info_add(self, true, NULL, NULL, SYS_MSG, 1, RED, "-!- %s has been kicked by %s", nick, self_nick);
            groupchat_onGroupPeerExit(self, m, self->num, target_peer_id, TOX_GROUP_EXIT_TYPE_KICK, nick, strlen(nick), NULL, 0);
            return;
        }

        case TOX_ERR_GROUP_MOD_KICK_PEER_PERMISSIONS: {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0,  "You do not have permission to kick %s.", nick);
            return;
        }

        case TOX_ERR_GROUP_MOD_KICK_PEER_SELF: {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0,  "You cannot kick yourself.");
            return;
        }

        default: {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0,  "Failed to kick %s from the group (error %d).", nick,
                          err);
            return;
        }
    }
}

void cmd_mod(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    if (argc < 1) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Peer name must be specified.");
        return;
    }

    const char *nick = argv[1];
    uint32_t target_peer_id;

    if (group_get_nick_peer_id(self->num, nick, &target_peer_id) == -1) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0,  "Invalid peer name '%s'.", nick);
        return;
    }

    TOX_ERR_GROUP_SELF_QUERY s_err;
    uint32_t self_peer_id = tox_group_self_get_peer_id(m, self->num, &s_err);

    if (s_err != TOX_ERR_GROUP_SELF_QUERY_OK) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0,  "Failed to fetch self peer_id.");
        return;
    }

    TOX_ERR_GROUP_MOD_SET_ROLE err;
    tox_group_mod_set_role(m, self->num, target_peer_id, TOX_GROUP_ROLE_MODERATOR, &err);

    switch (err) {
        case TOX_ERR_GROUP_MOD_SET_ROLE_OK: {
            groupchat_onGroupModeration(self, m, self->num, self_peer_id, target_peer_id, TOX_GROUP_MOD_EVENT_MODERATOR);
            return;
        }

        case TOX_ERR_GROUP_MOD_SET_ROLE_PERMISSIONS: {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0,  "You do not have permission to promote moderators.");
            return;
        }

        case TOX_ERR_GROUP_MOD_SET_ROLE_ASSIGNMENT: {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0,  "%s is already a moderator.", nick);
            return;
        }

        case TOX_ERR_GROUP_MOD_SET_ROLE_SELF: {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "You cannot make yourself a moderator.");
            return;
        }

        default: {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0,  "Failed to promote peer to moderator (error %d).", err);
            return;
        }
    }
}

void cmd_unmod(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    if (argc < 1) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Peer name must be specified.");
        return;
    }

    const char *nick = argv[1];
    uint32_t target_peer_id;

    if (group_get_nick_peer_id(self->num, nick, &target_peer_id) == -1) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0,  "Invalid peer name '%s'.", nick);
        return;
    }

    TOX_ERR_GROUP_SELF_QUERY s_err;
    uint32_t self_peer_id = tox_group_self_get_peer_id(m, self->num, &s_err);

    if (s_err != TOX_ERR_GROUP_SELF_QUERY_OK) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0,  "Failed to fetch self peer_id.");
        return;
    }

    if (tox_group_peer_get_role(m, self->num, target_peer_id, NULL) != TOX_GROUP_ROLE_MODERATOR) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "%s is not a moderator.", nick);
        return;
    }

    TOX_ERR_GROUP_MOD_SET_ROLE err;
    tox_group_mod_set_role(m, self->num, target_peer_id, TOX_GROUP_ROLE_USER, &err);

    switch (err) {
        case TOX_ERR_GROUP_MOD_SET_ROLE_OK: {
            groupchat_onGroupModeration(self, m, self->num, self_peer_id, target_peer_id, TOX_GROUP_MOD_EVENT_USER);
            return;
        }

        case TOX_ERR_GROUP_MOD_SET_ROLE_PERMISSIONS: {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0,  "You do not have permission to unmod %s.", nick);
            return;
        }

        case TOX_ERR_GROUP_MOD_SET_ROLE_SELF: {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "You cannot remove your own moderator status.");
            return;
        }

        default: {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0,  "Failed to revoke moderator powers from %s (error %d).", nick,
                          err);
            return;
        }
    }
}

void cmd_mykey(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    char pk_string[TOX_GROUP_PEER_PUBLIC_KEY_SIZE * 2 + 1] = {0};
    char pk[TOX_GROUP_PEER_PUBLIC_KEY_SIZE];

    TOX_ERR_GROUP_SELF_QUERY err;

    if (!tox_group_self_get_public_key(m, self->num, (uint8_t *) pk, &err)) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to fetch your public key (error %d)", err);
        return;
    }

    size_t i;

    for (i = 0; i < TOX_GROUP_PEER_PUBLIC_KEY_SIZE; ++i) {
        char d[3];
        snprintf(d, sizeof(d), "%02X", pk[i] & 0xff);
        strcat(pk_string, d);
    }

    line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "%s", pk_string);
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
            if (len > 0) {
                line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Password has been set to %s.", passwd);
            } else {
                line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Password has been unset.");
            }

            return;
        }

        case TOX_ERR_GROUP_FOUNDER_SET_PASSWORD_TOO_LONG: {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Password length must not exceed %d.",
                          TOX_GROUP_MAX_PASSWORD_SIZE);
            return;
        }

        case TOX_ERR_GROUP_FOUNDER_SET_PASSWORD_PERMISSIONS: {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "You do not have permission to set the password.");
            return;
        }

        default: {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to set password (error %d).", err);
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
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to retrieve peer limit (error %d).", err);
            return;
        }

        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Peer limit is set to %d", maxpeers);
        return;
    }

    maxpeers = atoi(argv[1]);

    if (maxpeers <= 0) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Peer limit must be a value greater than 0.");
        return;
    }

    TOX_ERR_GROUP_FOUNDER_SET_PEER_LIMIT err;
    tox_group_founder_set_peer_limit(m, self->num, maxpeers, &err);

    switch (err) {
        case TOX_ERR_GROUP_FOUNDER_SET_PEER_LIMIT_OK: {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Peer limit has been set to %d.", maxpeers);
            return;
        }

        case TOX_ERR_GROUP_FOUNDER_SET_PEER_LIMIT_PERMISSIONS: {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "You do not have permission to set the peer limit.");
            return;
        }

        default: {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to set the peer limit (error %d).", err);
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
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to retrieve privacy state (error %d).", err);
            return;
        }

        pstate_str = privacy_state == TOX_GROUP_PRIVACY_STATE_PRIVATE ? "private" : "public";
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Privacy state is set to %s.", pstate_str);
        return;
    }

    pstate_str = argv[1];

    if (strcasecmp(pstate_str, "private") != 0 && strcasecmp(pstate_str, "public") != 0) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Privacy state must be \"private\" or \"public\".");
        return;
    }

    privacy_state = strcasecmp(pstate_str,
                               "private") == 0 ? TOX_GROUP_PRIVACY_STATE_PRIVATE : TOX_GROUP_PRIVACY_STATE_PUBLIC;

    TOX_ERR_GROUP_FOUNDER_SET_PRIVACY_STATE err;
    tox_group_founder_set_privacy_state(m, self->num, privacy_state, &err);

    switch (err) {
        case TOX_ERR_GROUP_FOUNDER_SET_PRIVACY_STATE_OK: {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Privacy state has been set to %s.", pstate_str);
            return;
        }

        case TOX_ERR_GROUP_FOUNDER_SET_PRIVACY_STATE_PERMISSIONS: {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "You do not have permission to set the privacy state.");
            return;
        }

        default: {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Error setting privacy state (error %d).", err);
            return;
        }
    }
}

void cmd_silence(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    if (argc < 1) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Peer name must be specified.");
        return;
    }

    const char *nick = argv[1];
    uint32_t target_peer_id;

    if (group_get_nick_peer_id(self->num, nick, &target_peer_id) == -1) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0,  "Invalid peer name '%s'.", nick);
        return;
    }

    TOX_ERR_GROUP_SELF_QUERY s_err;
    uint32_t self_peer_id = tox_group_self_get_peer_id(m, self->num, &s_err);

    if (s_err != TOX_ERR_GROUP_SELF_QUERY_OK) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0,  "Failed to fetch self peer_id.");
        return;
    }

    TOX_ERR_GROUP_MOD_SET_ROLE err;
    tox_group_mod_set_role(m, self->num, target_peer_id, TOX_GROUP_ROLE_OBSERVER, &err);

    switch (err) {
        case TOX_ERR_GROUP_MOD_SET_ROLE_OK: {
            groupchat_onGroupModeration(self, m, self->num, self_peer_id, target_peer_id, TOX_GROUP_MOD_EVENT_OBSERVER);
            return;
        }

        case TOX_ERR_GROUP_MOD_SET_ROLE_PERMISSIONS: {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "You do not have permission to silence %s.", nick);
            return;
        }

        case TOX_ERR_GROUP_MOD_SET_ROLE_ASSIGNMENT: {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "%s is already silenced.", nick);
            return;
        }

        case TOX_ERR_GROUP_MOD_SET_ROLE_SELF: {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "You cannot silence yourself.");
            return;
        }

        default: {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to silence %s (error %d).", nick, err);
            return;
        }
    }
}

void cmd_unsilence(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    if (argc < 1) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Peer name must be specified.");
        return;
    }

    const char *nick = argv[1];
    uint32_t target_peer_id;

    if (group_get_nick_peer_id(self->num, nick, &target_peer_id) == -1) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0,  "Invalid peer name '%s'.", nick);
        return;
    }

    if (tox_group_peer_get_role(m, self->num, target_peer_id, NULL) != TOX_GROUP_ROLE_OBSERVER) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "%s is not silenced.", nick);
        return;
    }

    TOX_ERR_GROUP_SELF_QUERY s_err;
    uint32_t self_peer_id = tox_group_self_get_peer_id(m, self->num, &s_err);

    if (s_err != TOX_ERR_GROUP_SELF_QUERY_OK) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0,  "Failed to fetch self peer_id.");
        return;
    }

    TOX_ERR_GROUP_MOD_SET_ROLE err;
    tox_group_mod_set_role(m, self->num, target_peer_id, TOX_GROUP_ROLE_USER, &err);

    switch (err) {
        case TOX_ERR_GROUP_MOD_SET_ROLE_OK: {
            groupchat_onGroupModeration(self, m, self->num, self_peer_id, target_peer_id, TOX_GROUP_MOD_EVENT_USER);
            return;
        }

        case TOX_ERR_GROUP_MOD_SET_ROLE_PERMISSIONS: {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "You do not have permission to unsilence %s.", nick);
            return;
        }

        case TOX_ERR_GROUP_MOD_SET_ROLE_ASSIGNMENT: {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "%s is not silenced.", nick);
            return;
        }

        case TOX_ERR_GROUP_MOD_SET_ROLE_SELF: {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "You cannot unsilence yourself.");
            return;
        }

        default: {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to unsilence %s (error %d).", nick, err);
            return;
        }
    }
}

void cmd_rejoin(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    TOX_ERR_GROUP_RECONNECT err;

    if (!tox_group_reconnect(m, self->num, &err)) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to rejoin group (error %d).", err);
        return;
    }

    line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Reconnecting to group...");

    groupchat_rejoin(self, m);
}

void cmd_set_topic(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    if (argc < 1) {
        TOX_ERR_GROUP_STATE_QUERIES err;
        size_t tlen = tox_group_get_topic_size(m, self->num, &err);

        if (err != TOX_ERR_GROUP_STATE_QUERIES_OK) {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to retrieve topic length (error %d).", err);
            return;
        }

        if (tlen > 0) {
            char cur_topic[TOX_GROUP_MAX_TOPIC_LENGTH + 1];

            if (!tox_group_get_topic(m, self->num, (uint8_t *) cur_topic, &err)) {
                line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to retrieve topic (error %d).", err);
                return;
            }

            cur_topic[tlen] = 0;
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Topic is set to: %s", cur_topic);
        } else {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Topic is not set.");
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
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Topic length must not exceed %d.", TOX_GROUP_MAX_TOPIC_LENGTH);
            return;
        }

        case TOX_ERR_GROUP_TOPIC_SET_PERMISSIONS: {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "You do not have permission to set the topic.");
            return;
        }

        default: {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to set the topic (error %d).", err);
            return;
        }
    }

    char self_nick[TOX_MAX_NAME_LENGTH + 1];
    get_group_self_nick_truncate(m, self_nick, self->num);

    line_info_add(self, true, NULL, NULL, SYS_MSG, 1, MAGENTA, "-!- You set the topic to: %s", topic);

    char tmp_event[MAX_STR_SIZE];
    snprintf(tmp_event, sizeof(tmp_event), "set topic to %s", topic);
    write_to_log(tmp_event, self_nick, self->chatwin->log, true);
}

void cmd_unignore(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    if (argc < 1) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Peer must be specified.");
        return;
    }

    const char *nick = argv[1];
    uint32_t peer_id;

    if (group_get_nick_peer_id(self->num, nick, &peer_id) == -1) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0,  "Invalid peer name '%s'.", nick);
        return;
    }

    TOX_ERR_GROUP_TOGGLE_IGNORE err;
    tox_group_toggle_ignore(m, self->num, peer_id, false, &err);

    switch (err) {
        case TOX_ERR_GROUP_TOGGLE_IGNORE_OK: {
            break;
        }

        case TOX_ERR_GROUP_TOGGLE_IGNORE_SELF: {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "You cannot unignore yourself.");
            return;
        }

        default: {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to toggle ignore on %s (error %d).", nick, err);
            return;
        }
    }

    line_info_add(self, true, NULL, NULL, SYS_MSG, 1, BLUE, "-!- You are no longer ignoring %s", nick);
}

void cmd_whois(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    if (argc < 1) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Peer must be specified.");
        return;
    }

    GroupChat *chat = get_groupchat(self->num);

    if (!chat) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to fetch GroupChat object.");
        return;
    }

    const char *nick = argv[1];
    uint32_t peer_id;

    if (group_get_nick_peer_id(self->num, nick, &peer_id) == -1) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0,  "Invalid peer name '%s'.", nick);
        return;
    }

    int peer_index = get_peer_index(self->num, peer_id);

    if (peer_index < 0) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to fetch peer index.");
        return;
    }

    const char *status_str = "Online";

    if (chat->peer_list[peer_index].status == TOX_USER_STATUS_BUSY) {
        status_str = "Busy";
    } else if (chat->peer_list[peer_index].status == TOX_USER_STATUS_AWAY) {
        status_str = "Away";
    }

    const char *role_str = "User";

    if (chat->peer_list[peer_index].role == TOX_GROUP_ROLE_FOUNDER) {
        role_str = "Founder";
    } else if (chat->peer_list[peer_index].role == TOX_GROUP_ROLE_MODERATOR) {
        role_str = "Moderator";
    } else if (chat->peer_list[peer_index].role == TOX_GROUP_ROLE_OBSERVER) {
        role_str = "Observer";
    }

    char last_seen_str[128];
    get_elapsed_time_str_alt(last_seen_str, sizeof(last_seen_str),
                             get_unix_time() - chat->peer_list[peer_index].last_active);

    char pk_string[TOX_GROUP_PEER_PUBLIC_KEY_SIZE * 2 + 1] = {0};
    size_t i;

    for (i = 0; i < TOX_GROUP_PEER_PUBLIC_KEY_SIZE; ++i) {
        char d[3];
        snprintf(d, sizeof(d), "%02X", chat->peer_list[peer_index].public_key[i] & 0xff);
        strcat(pk_string, d);
    }

    line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Whois for %s", nick);
    line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Role: %s", role_str);
    line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Status: %s", status_str);
    line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Last active: %s", last_seen_str);
    line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Public key: %s", pk_string);
}

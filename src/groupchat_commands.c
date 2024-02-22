/*  groupchat_commands.c
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

#include "groupchat_commands.h"

#include <string.h>
#include <stdlib.h>

#include "toxic.h"
#include "windows.h"
#include "line_info.h"
#include "misc_tools.h"
#include "log.h"
#include "groupchats.h"

extern GroupChat groupchats[MAX_GROUPCHAT_NUM];

void cmd_chatid(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

    if (toxic == NULL || self == NULL) {
        return;
    }

    const Client_Config *c_config = toxic->c_config;

    char id_string[TOX_GROUP_CHAT_ID_SIZE * 2 + 1] = {0};
    char chat_id[TOX_GROUP_CHAT_ID_SIZE];

    Tox_Err_Group_State_Query err;

    if (!tox_group_get_chat_id(toxic->tox, self->num, (uint8_t *) chat_id, &err)) {
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to retrieve the Chat ID (error %d).", err);
        return;
    }

    char tmp[3];

    for (size_t i = 0; i < TOX_GROUP_CHAT_ID_SIZE; ++i) {
        snprintf(tmp, sizeof(tmp), "%02X", chat_id[i] & 0xff);
        strcat(id_string, tmp);
    }

    line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "%s", id_string);
}

void cmd_disconnect(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

    if (toxic == NULL || self == NULL) {
        return;
    }

    const Client_Config *c_config = toxic->c_config;

    Tox_Err_Group_Disconnect err;
    tox_group_disconnect(toxic->tox, self->num, &err);

    switch (err) {
        case TOX_ERR_GROUP_DISCONNECT_OK: {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0,
                          "Disconnected from group. Type '/rejoin' to reconnect.");
            return;
        }

        case TOX_ERR_GROUP_DISCONNECT_ALREADY_DISCONNECTED: {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0,
                          "Already disconnected. Type '/rejoin' to connect.");
            return;
        }

        default: {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0,  "Failed to disconnect from group. Error: %d",
                          err);
            return;
        }
    }
}

void cmd_group_nick(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

    if (toxic == NULL || self == NULL) {
        return;
    }

    const Client_Config *c_config = toxic->c_config;

    if (argc < 1) {
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Input required.");
        return;
    }

    char nick[MAX_STR_SIZE];
    snprintf(nick, sizeof(nick), "%s", argv[1]);
    size_t len = strlen(nick);

    if (!valid_nick(nick)) {
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Invalid name.");
        return;
    }

    len = MIN(len, TOXIC_MAX_NAME_LENGTH - 1);
    nick[len] = '\0';

    set_nick_this_group(self, toxic, nick, len);

    store_data(toxic);
}

void cmd_ignore(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

    if (toxic == NULL || self == NULL) {
        return;
    }

    const Client_Config *c_config = toxic->c_config;

    if (argc < 1) {
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Peer name or public key must be specified.");
        return;
    }

    const char *nick = argv[1];
    uint32_t peer_id;

    if (group_get_peer_id_of_identifier(self, c_config, nick, &peer_id) != 0) {
        return;
    }

    Tox_Err_Group_Set_Ignore err;
    tox_group_set_ignore(toxic->tox, self->num, peer_id, true, &err);

    switch (err) {
        case TOX_ERR_GROUP_SET_IGNORE_OK: {
            break;
        }

        case TOX_ERR_GROUP_SET_IGNORE_SELF: {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "You cannot ignore yourself.");
            return;
        }

        case TOX_ERR_GROUP_SET_IGNORE_PEER_NOT_FOUND: {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "The specified nick or public key is invalid.");
            return;
        }

        default: {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to toggle ignore on %s (error %d).",
                          nick, err);
            return;
        }
    }

    line_info_add(self, c_config, true, NULL, NULL, SYS_MSG, 1, BLUE, "-!- Ignoring %s", nick);

    group_toggle_peer_ignore(self->num, peer_id, true);
}

void cmd_kick(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

    if (toxic == NULL || self == NULL) {
        return;
    }

    Tox *tox = toxic->tox;
    const Client_Config *c_config = toxic->c_config;

    if (argc < 1) {
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Peer name or public key must be specified.");
        return;
    }

    const char *nick = argv[1];

    uint32_t target_peer_id;

    if (group_get_peer_id_of_identifier(self, c_config, nick, &target_peer_id) != 0) {
        return;
    }

    Tox_Err_Group_Mod_Kick_Peer err;
    tox_group_mod_kick_peer(tox, self->num, target_peer_id, &err);

    switch (err) {
        case TOX_ERR_GROUP_MOD_KICK_PEER_OK: {
            char self_nick[TOX_MAX_NAME_LENGTH + 1];
            get_group_self_nick_truncate(tox, self_nick, self->num);

            line_info_add(self, c_config, true, NULL, NULL, SYS_MSG, 1, RED, "-!- %s has been kicked by %s", nick,
                          self_nick);
            groupchat_onGroupPeerExit(self, toxic, self->num, target_peer_id, TOX_GROUP_EXIT_TYPE_KICK, nick, strlen(nick), NULL,
                                      0);
            return;
        }

        case TOX_ERR_GROUP_MOD_KICK_PEER_PERMISSIONS: {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0,  "You do not have permission to kick %s.", nick);
            return;
        }

        case TOX_ERR_GROUP_MOD_KICK_PEER_SELF: {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0,  "You cannot kick yourself.");
            return;
        }

        case TOX_ERR_GROUP_MOD_KICK_PEER_PEER_NOT_FOUND: {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0,  "Specified nick or public key is invalid.");
            return;
        }

        default: {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0,  "Failed to kick %s from the group (error %d).",
                          nick,
                          err);
            return;
        }
    }
}

void cmd_list(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

    if (toxic == NULL || self == NULL) {
        return;
    }

    const Client_Config *c_config = toxic->c_config;
    GroupChat *chat = get_groupchat(self->num);

    if (chat == NULL) {
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to fetch GroupChat object.");
        return;
    }

    for (size_t i = 0; i < chat->max_idx; ++i) {
        GroupPeer *peer = &chat->peer_list[i];

        if (!peer->active) {
            continue;
        }

        char pk_string[TOX_GROUP_PEER_PUBLIC_KEY_SIZE * 2 + 1] = {0};

        for (size_t j = 0; j < TOX_GROUP_PEER_PUBLIC_KEY_SIZE; ++j) {
            char d[3];
            snprintf(d, sizeof(d), "%02X", peer->public_key[j] & 0xff);
            strcat(pk_string, d);
        }

        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0,  "%s : %s", pk_string, peer->name);
    }
}

void cmd_mod(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

    if (toxic == NULL || self == NULL) {
        return;
    }

    Tox *tox = toxic->tox;
    const Client_Config *c_config = toxic->c_config;

    if (argc < 1) {
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Peer name or public key must be specified.");
        return;
    }

    const char *nick = argv[1];
    uint32_t target_peer_id;

    if (group_get_peer_id_of_identifier(self, c_config, nick, &target_peer_id) != 0) {
        return;
    }

    Tox_Err_Group_Self_Query s_err;
    uint32_t self_peer_id = tox_group_self_get_peer_id(tox, self->num, &s_err);

    if (s_err != TOX_ERR_GROUP_SELF_QUERY_OK) {
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0,  "Failed to fetch self peer_id.");
        return;
    }

    Tox_Err_Group_Mod_Set_Role err;
    tox_group_mod_set_role(tox, self->num, target_peer_id, TOX_GROUP_ROLE_MODERATOR, &err);

    switch (err) {
        case TOX_ERR_GROUP_MOD_SET_ROLE_OK: {
            groupchat_onGroupModeration(self, toxic, self->num, self_peer_id, target_peer_id, TOX_GROUP_MOD_EVENT_MODERATOR);
            return;
        }

        case TOX_ERR_GROUP_MOD_SET_ROLE_PERMISSIONS: {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0,
                          "You do not have permission to promote moderators.");
            return;
        }

        case TOX_ERR_GROUP_MOD_SET_ROLE_ASSIGNMENT: {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0,  "%s is already a moderator.", nick);
            return;
        }

        case TOX_ERR_GROUP_MOD_SET_ROLE_SELF: {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "You cannot make yourself a moderator.");
            return;
        }

        case TOX_ERR_GROUP_MOD_SET_ROLE_PEER_NOT_FOUND: {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "The specified nick or public key is invalid.");
            return;
        }

        default: {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0,
                          "Failed to promote peer to moderator (error %d).", err);
            return;
        }
    }
}

void cmd_unmod(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

    if (toxic == NULL || self == NULL) {
        return;
    }

    Tox *tox = toxic->tox;
    const Client_Config *c_config = toxic->c_config;

    if (argc < 1) {
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Peer name or public key must be specified.");
        return;
    }

    const char *nick = argv[1];
    uint32_t target_peer_id;

    if (group_get_peer_id_of_identifier(self, c_config, nick, &target_peer_id) != 0) {
        return;
    }

    Tox_Err_Group_Self_Query s_err;
    uint32_t self_peer_id = tox_group_self_get_peer_id(tox, self->num, &s_err);

    if (s_err != TOX_ERR_GROUP_SELF_QUERY_OK) {
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0,  "Failed to fetch self peer_id.");
        return;
    }

    if (tox_group_peer_get_role(tox, self->num, target_peer_id, NULL) != TOX_GROUP_ROLE_MODERATOR) {
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "%s is not a moderator.", nick);
        return;
    }

    Tox_Err_Group_Mod_Set_Role err;
    tox_group_mod_set_role(tox, self->num, target_peer_id, TOX_GROUP_ROLE_USER, &err);

    switch (err) {
        case TOX_ERR_GROUP_MOD_SET_ROLE_OK: {
            groupchat_onGroupModeration(self, toxic, self->num, self_peer_id, target_peer_id, TOX_GROUP_MOD_EVENT_USER);
            return;
        }

        case TOX_ERR_GROUP_MOD_SET_ROLE_PERMISSIONS: {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0,  "You do not have permission to unmod %s.",
                          nick);
            return;
        }

        case TOX_ERR_GROUP_MOD_SET_ROLE_SELF: {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "You cannot remove your own moderator status.");
            return;
        }

        case TOX_ERR_GROUP_MOD_SET_ROLE_PEER_NOT_FOUND: {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "The specified nick or public key is invalid.");
            return;
        }

        default: {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0,
                          "Failed to revoke moderator powers from %s (error %d).", nick,
                          err);
            return;
        }
    }
}

void cmd_set_passwd(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

    if (toxic == NULL || self == NULL) {
        return;
    }

    const Client_Config *c_config = toxic->c_config;

    const char *passwd = NULL;
    size_t len = 0;

    if (argc > 0) {
        passwd = argv[1];
        len = strlen(passwd);
    }

    Tox_Err_Group_Founder_Set_Password err;
    tox_group_founder_set_password(toxic->tox, self->num, (const uint8_t *) passwd, len, &err);

    switch (err) {
        case TOX_ERR_GROUP_FOUNDER_SET_PASSWORD_OK: {
            if (len > 0) {
                line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Password has been set to %s.", passwd);
            } else {
                line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Password has been unset.");
            }

            return;
        }

        case TOX_ERR_GROUP_FOUNDER_SET_PASSWORD_TOO_LONG: {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Password length must not exceed %d.",
                          TOX_GROUP_MAX_PASSWORD_SIZE);
            return;
        }

        case TOX_ERR_GROUP_FOUNDER_SET_PASSWORD_PERMISSIONS: {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0,
                          "You do not have permission to set the password.");
            return;
        }

        default: {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to set password (error %d).", err);
            return;
        }
    }
}

void cmd_set_peerlimit(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

    if (toxic == NULL || self == NULL) {
        return;
    }

    Tox *tox = toxic->tox;
    const Client_Config *c_config = toxic->c_config;

    int maxpeers = 0;

    if (argc < 1) {
        Tox_Err_Group_State_Query err;
        maxpeers = tox_group_get_peer_limit(tox, self->num, &err);

        if (err != TOX_ERR_GROUP_STATE_QUERY_OK) {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to retrieve peer limit (error %d).",
                          err);
            return;
        }

        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Peer limit is set to %d", maxpeers);
        return;
    }

    maxpeers = atoi(argv[1]);

    if (maxpeers <= 0) {
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Peer limit must be a value greater than 0.");
        return;
    }

    Tox_Err_Group_Founder_Set_Peer_Limit err;
    tox_group_founder_set_peer_limit(tox, self->num, maxpeers, &err);

    switch (err) {
        case TOX_ERR_GROUP_FOUNDER_SET_PEER_LIMIT_OK: {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Peer limit has been set to %d.", maxpeers);
            return;
        }

        case TOX_ERR_GROUP_FOUNDER_SET_PEER_LIMIT_PERMISSIONS: {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0,
                          "You do not have permission to set the peer limit.");
            return;
        }

        default: {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to set the peer limit (error %d).", err);
            return;
        }
    }
}

void cmd_set_voice(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

    if (toxic == NULL || self == NULL) {
        return;
    }

    Tox *tox = toxic->tox;
    const Client_Config *c_config = toxic->c_config;

    Tox_Group_Voice_State voice_state;

    if (argc < 1) {
        Tox_Err_Group_State_Query err;
        voice_state = tox_group_get_voice_state(tox, self->num, &err);

        if (err != TOX_ERR_GROUP_STATE_QUERY_OK) {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to retrieve voice state (error %d).",
                          err);
            return;
        }

        switch (voice_state) {
            case TOX_GROUP_VOICE_STATE_ALL: {
                line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Voice state is set to ALL");
                break;
            }

            case TOX_GROUP_VOICE_STATE_MODERATOR: {
                line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Voice state is set to MODERATOR");
                break;
            }

            case TOX_GROUP_VOICE_STATE_FOUNDER: {
                line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Voice state is set to FOUNDER");
                break;
            }

            default:
                line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Error: Unknown voice state: %d", voice_state);
                return;
        }

        return;
    }

    const char *vstate_str = argv[1];

    if (strcasecmp(vstate_str, "mod") == 0) {
        voice_state = TOX_GROUP_VOICE_STATE_MODERATOR;
    } else if (strcasecmp(vstate_str, "founder") == 0) {
        voice_state = TOX_GROUP_VOICE_STATE_FOUNDER;
    } else if (strcasecmp(vstate_str, "all") == 0) {
        voice_state = TOX_GROUP_VOICE_STATE_ALL;
    } else {
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0,
                      "voice state must be \"all\", \"mod\", or \"founder\".");
        return;
    }

    Tox_Err_Group_Founder_Set_Voice_State err;
    tox_group_founder_set_voice_state(tox, self->num, voice_state, &err);

    switch (err) {
        case TOX_ERR_GROUP_FOUNDER_SET_VOICE_STATE_OK: {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Voice state has been set to %s.", vstate_str);
            return;
        }

        case TOX_ERR_GROUP_FOUNDER_SET_VOICE_STATE_PERMISSIONS: {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0,
                          "You do not have permission to set the voice state.");
            return;
        }

        default: {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Error setting voice state (error %d).", err);
            return;
        }
    }
}

void cmd_set_privacy(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

    if (toxic == NULL || self == NULL) {
        return;
    }

    Tox *tox = toxic->tox;
    const Client_Config *c_config = toxic->c_config;

    const char *pstate_str = NULL;
    Tox_Group_Privacy_State privacy_state;

    if (argc < 1) {
        Tox_Err_Group_State_Query err;
        privacy_state = tox_group_get_privacy_state(tox, self->num, &err);

        if (err != TOX_ERR_GROUP_STATE_QUERY_OK) {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to retrieve privacy state (error %d).",
                          err);
            return;
        }

        pstate_str = privacy_state == TOX_GROUP_PRIVACY_STATE_PRIVATE ? "private" : "public";
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Privacy state is set to %s.", pstate_str);
        return;
    }

    pstate_str = argv[1];

    if (strcasecmp(pstate_str, "private") != 0 && strcasecmp(pstate_str, "public") != 0) {
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0,
                      "Privacy state must be \"private\" or \"public\".");
        return;
    }

    privacy_state = strcasecmp(pstate_str,
                               "private") == 0 ? TOX_GROUP_PRIVACY_STATE_PRIVATE : TOX_GROUP_PRIVACY_STATE_PUBLIC;

    Tox_Err_Group_Founder_Set_Privacy_State err;
    tox_group_founder_set_privacy_state(tox, self->num, privacy_state, &err);

    switch (err) {
        case TOX_ERR_GROUP_FOUNDER_SET_PRIVACY_STATE_OK: {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Privacy state has been set to %s.", pstate_str);
            return;
        }

        case TOX_ERR_GROUP_FOUNDER_SET_PRIVACY_STATE_PERMISSIONS: {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0,
                          "You do not have permission to set the privacy state.");
            return;
        }

        default: {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Error setting privacy state (error %d).", err);
            return;
        }
    }
}

void cmd_set_topic_lock(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

    if (toxic == NULL || self == NULL) {
        return;
    }

    Tox *tox = toxic->tox;
    const Client_Config *c_config = toxic->c_config;

    Tox_Group_Topic_Lock topic_lock;
    const char *tlock_str = NULL;

    if (argc < 1) {
        Tox_Err_Group_State_Query err;
        topic_lock = tox_group_get_topic_lock(tox, self->num, &err);

        if (err != TOX_ERR_GROUP_STATE_QUERY_OK) {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to retrieve topic lock (error %d).",
                          err);
            return;
        }

        tlock_str = topic_lock == TOX_GROUP_TOPIC_LOCK_ENABLED ? "Enabled" : "Disabled";
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Topic lock is %s.", tlock_str);
        return;
    }

    tlock_str = argv[1];

    if (strcasecmp(tlock_str, "on") != 0 && strcasecmp(tlock_str, "off") != 0) {
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Topic lock must be \"on\" or \"off\".");
        return;
    }

    topic_lock = strcasecmp(tlock_str, "on") == 0 ? TOX_GROUP_TOPIC_LOCK_ENABLED : TOX_GROUP_TOPIC_LOCK_DISABLED;
    const char *display_str = (topic_lock ==  TOX_GROUP_TOPIC_LOCK_ENABLED) ? "enabled" : "disabled";

    Tox_Err_Group_Founder_Set_Topic_Lock err;
    tox_group_founder_set_topic_lock(tox, self->num, topic_lock, &err);

    switch (err) {
        case TOX_ERR_GROUP_FOUNDER_SET_TOPIC_LOCK_OK: {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Topic lock has been %s.", display_str);
            return;
        }

        case TOX_ERR_GROUP_FOUNDER_SET_TOPIC_LOCK_PERMISSIONS: {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0,
                          "You do not have permission to set the topic lock.");
            return;
        }

        default: {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Error setting topic lock (%d).", err);
            return;
        }
    }
}

void cmd_silence(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

    if (toxic == NULL || self == NULL) {
        return;
    }

    Tox *tox = toxic->tox;
    const Client_Config *c_config = toxic->c_config;

    if (argc < 1) {
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Peer name or public key must be specified.");
        return;
    }

    const char *nick = argv[1];
    uint32_t target_peer_id;

    if (group_get_peer_id_of_identifier(self, c_config, nick, &target_peer_id) != 0) {
        return;
    }

    Tox_Err_Group_Self_Query s_err;
    uint32_t self_peer_id = tox_group_self_get_peer_id(tox, self->num, &s_err);

    if (s_err != TOX_ERR_GROUP_SELF_QUERY_OK) {
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0,  "Failed to fetch self peer_id.");
        return;
    }

    Tox_Err_Group_Mod_Set_Role err;
    tox_group_mod_set_role(tox, self->num, target_peer_id, TOX_GROUP_ROLE_OBSERVER, &err);

    switch (err) {
        case TOX_ERR_GROUP_MOD_SET_ROLE_OK: {
            groupchat_onGroupModeration(self, toxic, self->num, self_peer_id, target_peer_id, TOX_GROUP_MOD_EVENT_OBSERVER);
            return;
        }

        case TOX_ERR_GROUP_MOD_SET_ROLE_PERMISSIONS: {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "You do not have permission to silence %s.",
                          nick);
            return;
        }

        case TOX_ERR_GROUP_MOD_SET_ROLE_ASSIGNMENT: {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "%s is already silenced.", nick);
            return;
        }

        case TOX_ERR_GROUP_MOD_SET_ROLE_SELF: {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "You cannot silence yourself.");
            return;
        }

        case TOX_ERR_GROUP_MOD_SET_ROLE_PEER_NOT_FOUND: {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "The specified nick or public key is invalid.");
            return;
        }

        default: {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to silence %s (error %d).", nick, err);
            return;
        }
    }
}

void cmd_unsilence(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

    if (toxic == NULL || self == NULL) {
        return;
    }

    Tox *tox = toxic->tox;
    const Client_Config *c_config = toxic->c_config;

    if (argc < 1) {
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Peer name or public key must be specified.");
        return;
    }

    const char *nick = argv[1];
    uint32_t target_peer_id;

    if (group_get_peer_id_of_identifier(self, c_config, nick, &target_peer_id) != 0) {
        return;
    }

    if (tox_group_peer_get_role(tox, self->num, target_peer_id, NULL) != TOX_GROUP_ROLE_OBSERVER) {
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "%s is not silenced.", nick);
        return;
    }

    Tox_Err_Group_Self_Query s_err;
    uint32_t self_peer_id = tox_group_self_get_peer_id(tox, self->num, &s_err);

    if (s_err != TOX_ERR_GROUP_SELF_QUERY_OK) {
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0,  "Failed to fetch self peer_id.");
        return;
    }

    Tox_Err_Group_Mod_Set_Role err;
    tox_group_mod_set_role(tox, self->num, target_peer_id, TOX_GROUP_ROLE_USER, &err);

    switch (err) {
        case TOX_ERR_GROUP_MOD_SET_ROLE_OK: {
            groupchat_onGroupModeration(self, toxic, self->num, self_peer_id, target_peer_id, TOX_GROUP_MOD_EVENT_USER);
            return;
        }

        case TOX_ERR_GROUP_MOD_SET_ROLE_PERMISSIONS: {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "You do not have permission to unsilence %s.",
                          nick);
            return;
        }

        case TOX_ERR_GROUP_MOD_SET_ROLE_ASSIGNMENT: {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "%s is not silenced.", nick);
            return;
        }

        case TOX_ERR_GROUP_MOD_SET_ROLE_SELF: {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "You cannot unsilence yourself.");
            return;
        }

        case TOX_ERR_GROUP_MOD_SET_ROLE_PEER_NOT_FOUND: {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "The specified nick or public key is invalid.");
            return;
        }

        default: {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to unsilence %s (error %d).", nick, err);
            return;
        }
    }
}

void cmd_rejoin(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

    if (toxic == NULL || self == NULL) {
        return;
    }

    const Client_Config *c_config = toxic->c_config;

    Tox_Err_Group_Reconnect err;

    if (!tox_group_reconnect(toxic->tox, self->num, &err)) {
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to rejoin group (error %d).", err);
        return;
    }

    line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Reconnecting to group...");

    groupchat_rejoin(self, toxic);
}

void cmd_set_topic(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

    if (toxic == NULL || self == NULL) {
        return;
    }

    Tox *tox = toxic->tox;
    const Client_Config *c_config = toxic->c_config;

    if (argc < 1) {
        Tox_Err_Group_State_Query err;
        size_t tlen = tox_group_get_topic_size(tox, self->num, &err);

        if (err != TOX_ERR_GROUP_STATE_QUERY_OK) {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to retrieve topic length (error %d).",
                          err);
            return;
        }

        if (tlen > 0) {
            char cur_topic[TOX_GROUP_MAX_TOPIC_LENGTH + 1];

            if (!tox_group_get_topic(tox, self->num, (uint8_t *) cur_topic, &err)) {
                line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to retrieve topic (error %d).", err);
                return;
            }

            cur_topic[tlen] = 0;
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Topic is set to: %s", cur_topic);
        } else {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Topic is not set.");
        }

        return;
    }

    const char *topic = argv[1];

    Tox_Err_Group_Topic_Set err;
    tox_group_set_topic(tox, self->num, (const uint8_t *) topic, strlen(topic), &err);

    switch (err) {
        case TOX_ERR_GROUP_TOPIC_SET_OK: {
            /* handled below switch */
            break;
        }

        case TOX_ERR_GROUP_TOPIC_SET_TOO_LONG: {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Topic length must not exceed %d.",
                          TOX_GROUP_MAX_TOPIC_LENGTH);
            return;
        }

        case TOX_ERR_GROUP_TOPIC_SET_PERMISSIONS: {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "You do not have permission to set the topic.");
            return;
        }

        case TOX_ERR_GROUP_TOPIC_SET_DISCONNECTED: {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "You are disconnected from the group.");
            return;
        }

        default: {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to set the topic (error %d).", err);
            return;
        }
    }

    groupchat_update_statusbar_topic(self, tox);

    char self_nick[TOX_MAX_NAME_LENGTH + 1];
    get_group_self_nick_truncate(tox, self_nick, self->num);

    line_info_add(self, c_config, true, NULL, NULL, SYS_MSG, 1, MAGENTA, "-!- You set the topic to: %s", topic);

    char tmp_event[MAX_STR_SIZE];
    snprintf(tmp_event, sizeof(tmp_event), "set topic to %s", topic);
    write_to_log(self->chatwin->log, c_config, tmp_event, self_nick, true, LOG_HINT_TOPIC);
}

void cmd_unignore(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

    if (toxic == NULL || self == NULL) {
        return;
    }

    const Client_Config *c_config = toxic->c_config;

    if (argc < 1) {
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Peer name or public key must be specified.");
        return;
    }

    const char *nick = argv[1];
    uint32_t peer_id;

    if (group_get_peer_id_of_identifier(self, c_config, nick, &peer_id) != 0) {
        return;
    }

    Tox_Err_Group_Set_Ignore err;
    tox_group_set_ignore(toxic->tox, self->num, peer_id, false, &err);

    switch (err) {
        case TOX_ERR_GROUP_SET_IGNORE_OK: {
            break;
        }

        case TOX_ERR_GROUP_SET_IGNORE_SELF: {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "You cannot unignore yourself.");
            return;
        }

        case TOX_ERR_GROUP_SET_IGNORE_PEER_NOT_FOUND: {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "The specified nick or public key is invalid.");
            return;
        }

        default: {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to toggle ignore on %s (error %d).",
                          nick, err);
            return;
        }
    }

    line_info_add(self, c_config, true, NULL, NULL, SYS_MSG, 1, BLUE, "-!- You are no longer ignoring %s", nick);

    group_toggle_peer_ignore(self->num, peer_id, false);
}

void cmd_whois(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

    if (toxic == NULL || self == NULL) {
        return;
    }

    Tox *tox = toxic->tox;
    const Client_Config *c_config = toxic->c_config;

    if (argc < 1) {
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Peer name or public key must be specified.");
        return;
    }

    GroupChat *chat = get_groupchat(self->num);

    if (chat == NULL) {
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to fetch GroupChat object.");
        return;
    }

    const char *identifier = argv[1];
    bool is_public_key = false;

    for (size_t i = 0; i < chat->max_idx && !is_public_key; ++i) {
        uint32_t peer_id;

        if (group_get_public_key_peer_id(self->num, identifier, &peer_id) == 0) {
            is_public_key = true;
        } else {
            GroupPeer *peer = &chat->peer_list[i];

            if (!peer->active) {
                continue;
            }

            if (strcmp(identifier, peer->name) != 0) {
                continue;
            }

            peer_id = peer->peer_id;
        }

        int peer_index = get_peer_index(self->num, peer_id);

        if (peer_index < 0) {
            continue;
        }

        GroupPeer *peer = &chat->peer_list[peer_index];

        const char *status_str = "Online";

        if (peer->status == TOX_USER_STATUS_BUSY) {
            status_str = "Busy";
        } else if (peer->status == TOX_USER_STATUS_AWAY) {
            status_str = "Away";
        }

        const char *role_str = "User";

        if (peer->role == TOX_GROUP_ROLE_FOUNDER) {
            role_str = "Founder";
        } else if (peer->role == TOX_GROUP_ROLE_MODERATOR) {
            role_str = "Moderator";
        } else if (peer->role == TOX_GROUP_ROLE_OBSERVER) {
            role_str = "Observer";
        }

        char last_seen_str[128];
        get_elapsed_time_str_alt(last_seen_str, sizeof(last_seen_str),
                                 get_unix_time() - peer->last_active);

        char pk_string[TOX_GROUP_PEER_PUBLIC_KEY_SIZE * 2 + 1] = {0};

        for (size_t j = 0; j < TOX_GROUP_PEER_PUBLIC_KEY_SIZE; ++j) {
            char d[3];
            snprintf(d, sizeof(d), "%02X", peer->public_key[j] & 0xff);
            strcat(pk_string, d);
        }

        Tox_Err_Group_Peer_Query err;
        Tox_Connection connection_type = tox_group_peer_get_connection_status(tox, self->num, peer_id, &err);

        const char *connection_type_str = "-";

        if (err == TOX_ERR_GROUP_PEER_QUERY_OK || connection_type != TOX_CONNECTION_NONE) {
            connection_type_str = connection_type == TOX_CONNECTION_UDP ? "UDP" : "TCP";
        }

#ifdef TOX_EXPERIMENTAL
        char ip_addr[TOX_GROUP_PEER_IP_STRING_MAX_LENGTH] = {0};
        const bool ip_ret = tox_group_peer_get_ip_address(tox, self->num, peer_id, (uint8_t *)ip_addr, &err);

        if (!ip_ret) {
            snprintf(ip_addr, sizeof(ip_addr), "Error %d", err);
        } else {
            size_t ip_addr_len = tox_group_peer_get_ip_address_size(tox, self->num, peer_id, &err);

            if (err != TOX_ERR_GROUP_PEER_QUERY_OK) {
                snprintf(ip_addr, sizeof(ip_addr), "Error %d", err);
            } else {
                ip_addr[ip_addr_len] = '\0';
            }
        }

#endif  // TOX_EXPERIMENTAL

        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Public key: %s", pk_string);
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Name: %s", peer->name);
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Role: %s", role_str);
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Status: %s", status_str);
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Connection: %s", connection_type_str);
#ifdef TOX_EXPERIMENTAL
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "IP Address: %s", ip_addr);
#endif
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Last active: %s", last_seen_str);
    }
}


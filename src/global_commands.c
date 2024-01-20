/*  global_commands.c
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

#include <stdlib.h>
#include <string.h>

#include "avatars.h"
#include "conference.h"
#include "friendlist.h"
#include "groupchats.h"
#include "help.h"
#include "line_info.h"
#include "log.h"
#include "misc_tools.h"
#include "name_lookup.h"
#include "prompt.h"
#include "qr_code.h"
#include "term_mplex.h"
#include "toxic.h"
#include "toxic_strings.h"
#include "windows.h"

#ifdef GAMES
#include "game_base.h"
#endif

/* command functions */
void cmd_accept(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

    if (toxic == NULL || self == NULL) {
        return;
    }

    Tox *tox = toxic->tox;

    if (argc < 1) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Request ID required.");
        return;
    }

    long int req = strtol(argv[1], NULL, 10);

    if ((req == 0 && strcmp(argv[1], "0")) || req < 0 || req >= MAX_FRIEND_REQUESTS) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "No pending friend request with that ID.");
        return;
    }

    if (!FrndRequests.request[req].active) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "No pending friend request with that ID.");
        return;
    }

    Tox_Err_Friend_Add err;
    uint32_t friendnum = tox_friend_add_norequest(tox, FrndRequests.request[req].key, &err);

    if (err != TOX_ERR_FRIEND_ADD_OK) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to add friend (error %d\n)", err);
        return;
    } else {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Friend request accepted.");
        on_friend_added(toxic, friendnum, true);
    }

    FrndRequests.request[req] = (struct friend_request) {
        0
    };

    int i;

    for (i = FrndRequests.max_idx; i > 0; --i) {
        if (FrndRequests.request[i - 1].active) {
            break;
        }
    }

    FrndRequests.max_idx = i;
    --FrndRequests.num_requests;
}

void cmd_add_helper(ToxWindow *self, Toxic *toxic, const char *id_bin, const char *msg)
{
    const char *errmsg;

    Tox_Err_Friend_Add err;
    uint32_t f_num = tox_friend_add(toxic->tox, (const uint8_t *) id_bin, (const uint8_t *) msg, strlen(msg), &err);

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
            on_friend_added(toxic, f_num, true);
            break;

        case TOX_ERR_FRIEND_ADD_NULL:

        /* fallthrough */
        default:
            errmsg = "Failed to add friend: Unknown error.";
            break;
    }

    line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, errmsg);
}

void cmd_add(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

    if (toxic == NULL || self == NULL) {
        return;
    }

    Tox *tox = toxic->tox;

    if (argc < 1) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Tox ID or address required.");
        return;
    }

    char msg[MAX_STR_SIZE] = {0};

    const char *id = argv[1];
    const size_t arg_length = strlen(id);
    const int space_idx = char_find(0, id, ' ');

    // we have to manually parse the message due to this command being a special case
    if (space_idx > 0 && space_idx < arg_length - 1) {
        snprintf(msg, sizeof(msg), "%s", &id[space_idx + 1]);
    } else {
        char selfname[TOX_MAX_NAME_LENGTH];
        tox_self_get_name(tox, (uint8_t *) selfname);

        size_t n_len = tox_self_get_name_size(tox);
        selfname[n_len] = '\0';
        snprintf(msg, sizeof(msg), "Hello, my name is %s. Care to Tox?", selfname);
    }

    char id_bin[TOX_ADDRESS_SIZE] = {0};

    const bool is_domain = char_find(0, id, '@') != arg_length;
    const bool valid_id_size = arg_length >= TOX_ADDRESS_SIZE * 2;  // arg_length may include invite message

    if (is_domain) {
        if (!name_lookup(self, toxic, id_bin, id, msg)) {
            return;
        }
    } else if (!valid_id_size) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Invalid Tox ID.");
        return;
    }

    char xx[3];
    uint32_t x = 0;

    for (size_t i = 0; i < TOX_ADDRESS_SIZE; ++i) {
        xx[0] = id[2 * i];
        xx[1] = id[2 * i + 1];
        xx[2] = 0;

        if (sscanf(xx, "%02x", &x) != 1) {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Invalid Tox ID.");
            return;
        }

        id_bin[i] = (char) x;
    }

    if (friend_is_blocked(id_bin)) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Friend is in your block list.");
        return;
    }

    cmd_add_helper(self, toxic, id_bin, msg);
}

void cmd_avatar(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

    if (toxic == NULL || self == NULL) {
        return;
    }

    Tox *tox = toxic->tox;

    if (argc != 1 || strlen(argv[1]) < 3) {
        avatar_unset(tox);
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Avatar has been unset.");
        return;
    }

    char path[MAX_STR_SIZE];
    snprintf(path, sizeof(path), "%s", argv[1]);
    int len = strlen(path);

    if (len <= 0) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Invalid path.");
        return;
    }

    path[len] = '\0';
    char filename[MAX_STR_SIZE];
    get_file_name(filename, sizeof(filename), path);

    if (avatar_set(tox, path, len) == -1) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0,
                      "Failed to set avatar. Avatars must be in PNG format and may not exceed %d bytes.",
                      MAX_AVATAR_FILE_SIZE);
        return;
    }

    line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Avatar set to '%s'", filename);
}

void cmd_clear(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(toxic);
    UNUSED_VAR(argc);
    UNUSED_VAR(argv);

    if (self == NULL) {
        return;
    }

    line_info_clear(self->chatwin->hst);
    force_refresh(window);
}

void cmd_color(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);
    UNUSED_VAR(toxic);

    if (self == NULL) {
        return;
    }

    if (argc != 1) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0,
                      "Change the name of the focused window with: "
                      "/color [black|white|gray|brown|red|green|blue|cyan|yellow|magenta|orange|pink]");
        return;
    }

    const char *colour = argv[1];

    const int colour_val = colour_string_to_int(colour);

    if (colour_val < 0) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0,
                      "Valid colors are: black|white|gray|brown|red|green|blue|cyan|yellow|magenta|orange|pink");
        return;
    }

    self->colour = colour_val;
}

void cmd_connect(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

    if (toxic == NULL || self == NULL) {
        return;
    }

    Tox *tox = toxic->tox;

    if (argc != 3) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Require: <ip> <port> <key>");
        return;
    }

    const char *ip = argv[1];
    const char *port_str = argv[2];
    const char *ascii_key = argv[3];

    long int port = strtol(port_str, NULL, 10);

    if (port <= 0 || port > MAX_PORT_RANGE) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Invalid port.");
        return;
    }

    char key_binary[TOX_PUBLIC_KEY_SIZE];

    if (tox_pk_string_to_bytes(ascii_key, strlen(ascii_key), key_binary, sizeof(key_binary)) == -1) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Invalid key.");
        return;
    }

    Tox_Err_Bootstrap err;
    tox_bootstrap(tox, ip, port, (uint8_t *) key_binary, &err);
    tox_add_tcp_relay(tox, ip, port, (uint8_t *) key_binary, &err);

    switch (err) {
        case TOX_ERR_BOOTSTRAP_BAD_HOST:
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Bootstrap failed: Invalid IP.");
            break;

        case TOX_ERR_BOOTSTRAP_BAD_PORT:
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Bootstrap failed: Invalid port.");
            break;

        case TOX_ERR_BOOTSTRAP_NULL:
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Bootstrap failed.");
            break;

        default:
            break;
    }
}

void cmd_decline(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);
    UNUSED_VAR(toxic);

    if (self == NULL) {
        return;
    }

    if (argc < 1) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Request ID required.");
        return;
    }

    long int req = strtol(argv[1], NULL, 10);

    if ((req == 0 && strcmp(argv[1], "0")) || req < 0 || req >= MAX_FRIEND_REQUESTS) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "No pending friend request with that ID.");
        return;
    }

    if (!FrndRequests.request[req].active) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "No pending friend request with that ID.");
        return;
    }

    FrndRequests.request[req] = (struct friend_request) {
        0
    };

    int i;

    for (i = FrndRequests.max_idx; i > 0; --i) {
        if (FrndRequests.request[i - 1].active) {
            break;
        }
    }

    FrndRequests.max_idx = i;
    --FrndRequests.num_requests;
}

#ifdef GAMES

void cmd_game(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

    if (toxic == NULL || self == NULL) {
        return;
    }

    if (argc < 1) {
        game_list_print(self);
        return;
    }

    GameType type = game_get_type(argv[1]);

    if (type >= GT_Invalid) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Unknown game.");
        return;
    }

    if (get_num_active_windows() >= MAX_WINDOWS_NUM) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, RED, " * Warning: Too many windows are open.");
        return;
    }

    const unsigned int id = rand_not_secure();
    const int ret = game_initialize(self, toxic, type, id, NULL, 0, true);

    switch (ret) {
        case 0: {
            break;
        }

        case -1: {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Window is too small.");
            return;
        }

        case -2: {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Game failed to initialize: Network error.");
            return;

        }

        case -3: {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0,
                          "Game is multiplayer only. Try the command again in the chat window of the contact you wish to play with.");
            return;
        }

        default: {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Game failed to initialize (error %d)", ret);
            return;
        }
    }
}

#endif // GAMES

void cmd_conference(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

    if (toxic == NULL || self == NULL) {
        return;
    }

    Tox *tox = toxic->tox;

    if (get_num_active_windows() >= MAX_WINDOWS_NUM) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, RED, " * Warning: Too many windows are open.");
        return;
    }

    if (argc < 1) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Please specify conference type: text | audio");
        return;
    }

    uint8_t type;

    if (!strcasecmp(argv[1], "audio")) {
        type = TOX_CONFERENCE_TYPE_AV;
    } else if (!strcasecmp(argv[1], "text")) {
        type = TOX_CONFERENCE_TYPE_TEXT;
    } else {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Valid conference types are: text | audio");
        return;
    }

    uint32_t conferencenum = 0;

    if (type == TOX_CONFERENCE_TYPE_TEXT) {
        Tox_Err_Conference_New err;

        conferencenum = tox_conference_new(tox, &err);

        if (err != TOX_ERR_CONFERENCE_NEW_OK) {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Conference instance failed to initialize (error %d)", err);
            return;
        }
    } else if (type == TOX_CONFERENCE_TYPE_AV) {
#ifdef AUDIO
        conferencenum = toxav_add_av_groupchat(tox, audio_conference_callback, NULL);

        if (conferencenum == (uint32_t) -1) {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Audio conference instance failed to initialize");
            return;
        }

#else
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Audio support disabled by compile-time option.");
        return;
#endif
    }

    if (init_conference_win(toxic, conferencenum, type, NULL, 0) == -1) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Conference window failed to initialize.");
        tox_conference_delete(tox, conferencenum, NULL);
        return;
    }

#ifdef AUDIO

    if (type == TOX_CONFERENCE_TYPE_AV) {
        if (!init_conference_audio_input(tox, conferencenum)) {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Audio capture failed; use \"/audio on\" to try again.");
        }
    }

#endif

    line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Conference [%d] created.", conferencenum);
}

void cmd_groupchat(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{

    if (toxic == NULL || self == NULL) {
        return;
    }

    Tox *tox = toxic->tox;

    if (get_num_active_windows() >= MAX_WINDOWS_NUM) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, RED, " * Warning: Too many windows are open.");
        return;
    }

    if (argc < 1) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Group name required");
        return;
    }

    const char *tmp_name = argv[1];
    int len = strlen(tmp_name);

    if (len == 0 || len > TOX_GROUP_MAX_GROUP_NAME_LENGTH) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Invalid group name.");
        return;
    }

    char name[TOX_GROUP_MAX_GROUP_NAME_LENGTH];

    snprintf(name, sizeof(name), "%s", argv[1]);

    size_t nick_length = tox_self_get_name_size(tox);
    char self_nick[TOX_MAX_NAME_LENGTH + 1];
    tox_self_get_name(tox, (uint8_t *) self_nick);
    self_nick[nick_length] = '\0';

    Tox_Err_Group_New err;
    uint32_t groupnumber = tox_group_new(tox, TOX_GROUP_PRIVACY_STATE_PUBLIC, (const uint8_t *) name, len,
                                         (const uint8_t *) self_nick, nick_length, &err);

    if (err != TOX_ERR_GROUP_NEW_OK) {
        switch (err) {
            case TOX_ERR_GROUP_NEW_TOO_LONG: {
                line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Group name length cannot exceed %d.",
                              TOX_GROUP_MAX_GROUP_NAME_LENGTH);
                break;
            }

            case TOX_ERR_GROUP_NEW_EMPTY: {
                line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Group name cannot be empty.");
                break;
            }

            default: {
                line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Group chat instance failed to initialize (error %d).", err);
                break;
            }
        }

        return;
    }

    const int init = init_groupchat_win(toxic, groupnumber, name, len, Group_Join_Type_Create);

    if (init == -1) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Group chat window failed to initialize.");
        tox_group_leave(tox, groupnumber, NULL, 0, NULL);
    } else if (init == -2) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0,
                      "You have been kicked from a group. Close the window and try again.");
        tox_group_leave(tox, groupnumber, NULL, 0, NULL);
    }
}

void cmd_join(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    if (toxic == NULL || self == NULL) {
        return;
    }

    Tox *tox = toxic->tox;

    if (get_num_active_windows() >= MAX_WINDOWS_NUM) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, RED, " * Warning: Too many windows are open.");
        return;
    }

    if (argc < 1) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Chat ID is required.");
        return;
    }

    const char *chat_id = argv[1];

    if (strlen(chat_id) != TOX_GROUP_CHAT_ID_SIZE * 2) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Invalid chat ID");
        return;
    }

    char id_bin[TOX_GROUP_CHAT_ID_SIZE] = {0};

    size_t i;
    char xch[3];
    uint32_t x;

    for (i = 0; i < TOX_GROUP_CHAT_ID_SIZE; ++i) {
        xch[0] = chat_id[2 * i];
        xch[1] = chat_id[2 * i + 1];
        xch[2] = '\0';

        if (sscanf(xch, "%02x", &x) != 1) {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Invalid chat ID.");
            return;
        }

        id_bin[i] = (char) x;
    }

    const char *passwd = NULL;
    uint16_t passwd_len = 0;

    if (argc > 1) {
        passwd = argv[2];
        passwd_len = (uint16_t) strlen(passwd);
    }

    if (passwd_len > TOX_GROUP_MAX_PASSWORD_SIZE) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Password length cannot exceed %d.", TOX_GROUP_MAX_PASSWORD_SIZE);
        return;
    }

    size_t nick_length = tox_self_get_name_size(tox);
    char self_nick[TOX_MAX_NAME_LENGTH + 1];
    tox_self_get_name(tox, (uint8_t *) self_nick);
    self_nick[nick_length] = '\0';

    Tox_Err_Group_Join err;
    uint32_t groupnumber = tox_group_join(tox, (uint8_t *) id_bin, (const uint8_t *) self_nick, nick_length,
                                          (const uint8_t *) passwd, passwd_len, &err);

    if (err != TOX_ERR_GROUP_JOIN_OK) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to join group (error %d).", err);
        return;
    }

    const int init = init_groupchat_win(toxic, groupnumber, NULL, 0, Group_Join_Type_Join);

    if (init == -1) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Group chat window failed to initialize.");
        tox_group_leave(tox, groupnumber, NULL, 0, NULL);
    }
}

void cmd_log(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);
    UNUSED_VAR(toxic);

    if (self == NULL) {
        return;
    }

    const char *msg;
    struct chatlog *log = self->chatwin->log;

    if (argc == 0) {
        if (log->log_on) {
            msg = "Logging for this window is ON; type \"/log off\" to disable. (Logs are not encrypted)";
        } else {
            msg = "Logging for this window is OFF; type \"/log on\" to enable.";
        }

        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, msg);
        return;
    }

    const char *swch = argv[1];

    if (!strcmp(swch, "1") || !strcmp(swch, "on")) {
        msg = log_enable(log) == 0 ? "Logging enabled." : "Warning: Failed to enable log.";
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, msg);
        return;
    } else if (!strcmp(swch, "0") || !strcmp(swch, "off")) {
        if (self->type == WINDOW_TYPE_CHAT) {
            Friends.list[self->num].logging_on = false;
        }

        log_disable(log);

        msg = "Logging disabled.";
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, msg);
        return;
    }

    msg = "Invalid option. Use \"/log on\" and \"/log off\" to toggle logging.";
    line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, msg);
}

void cmd_myid(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);
    UNUSED_VAR(argc);
    UNUSED_VAR(argv);

    if (toxic == NULL || self == NULL) {
        return;
    }

    char id_string[TOX_ADDRESS_SIZE * 2 + 1];
    char bin_id[TOX_ADDRESS_SIZE];
    tox_self_get_address(toxic->tox, (uint8_t *) bin_id);

    if (tox_id_bytes_to_str(bin_id, sizeof(bin_id), id_string, sizeof(id_string)) == -1) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to print ID.");
        return;
    }

    line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "%s", id_string);
}

#ifdef QRCODE
void cmd_myqr(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

    if (toxic == NULL || self == NULL) {
        return;
    }

    Tox *tox = toxic->tox;

    char id_string[TOX_ADDRESS_SIZE * 2 + 1];
    char bin_id[TOX_ADDRESS_SIZE];
    tox_self_get_address(tox, (uint8_t *) bin_id);

    if (tox_id_bytes_to_str(bin_id, sizeof(bin_id), id_string, sizeof(id_string)) == -1) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to create QR code.");
        return;
    }

    char nick[TOX_MAX_NAME_LENGTH];
    tox_self_get_name(tox, (uint8_t *) nick);
    size_t nick_len = tox_self_get_name_size(tox);
    nick[nick_len] = '\0';

    const size_t data_file_len = strlen(toxic->client_data.data_path);
    char *dir = malloc(data_file_len + 1);

    if (dir == NULL) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to create QR code: Out of memory.");
        return;
    }

    const size_t dir_len = get_base_dir(toxic->client_data.data_path, data_file_len, dir);

#ifdef QRPNG

    if (argc == 0) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Required 'txt' or 'png'");
        free(dir);
        return;
    } else if (!strcmp(argv[1], "txt")) {

#endif /* QRPNG */
        size_t qr_path_buf_size = dir_len + nick_len + sizeof(QRCODE_FILENAME_EXT);
        char *qr_path = malloc(qr_path_buf_size);

        if (qr_path == NULL) {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to create QR code: Out of memory");
            free(dir);
            return;
        }

        snprintf(qr_path, qr_path_buf_size, "%s%s%s", dir, nick, QRCODE_FILENAME_EXT);

        if (ID_to_QRcode_txt(id_string, qr_path) == -1) {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to create QR code.");
            free(dir);
            free(qr_path);
            return;
        }

        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "QR code has been printed to the file '%s'", qr_path);

        free(qr_path);

#ifdef QRPNG
    } else if (!strcmp(argv[1], "png")) {
        size_t qr_path_buf_size = dir_len + nick_len + sizeof(QRCODE_FILENAME_EXT_PNG);
        char *qr_path = malloc(qr_path_buf_size);

        if (qr_path == NULL) {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to create QR code: Out of memory");
            free(dir);
            return;
        }

        snprintf(qr_path, qr_path_buf_size, "%s%s%s", dir, nick, QRCODE_FILENAME_EXT_PNG);

        if (ID_to_QRcode_png(id_string, qr_path) == -1) {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to create QR code.");
            free(dir);
            free(qr_path);
            return;
        }

        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "QR code has been printed to the file '%s'", qr_path);

        free(qr_path);

    } else {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Unknown option '%s' -- Required 'txt' or 'png'", argv[1]);
        free(dir);
        return;
    }

#endif /* QRPNG */

    free(dir);
}
#endif /* QRCODE */

void cmd_nick(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

    if (toxic == NULL || self == NULL) {
        return;
    }

    Tox *tox = toxic->tox;

    if (argc < 1) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Input required.");
        return;
    }

    char nick[MAX_STR_SIZE];
    snprintf(nick, sizeof(nick), "%s", argv[1]);
    size_t len = strlen(nick);

    if (!valid_nick(nick)) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Invalid name.");
        return;
    }

    len = MIN(len, TOXIC_MAX_NAME_LENGTH - 1);
    nick[len] = '\0';

    tox_self_set_name(tox, (uint8_t *) nick, len, NULL);
    prompt_update_nick(prompt, nick);

    store_data(toxic);
}

void cmd_note(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);
    UNUSED_VAR(self);

    if (toxic == NULL || self == NULL) {
        return;
    }

    const char *note = argc >= 1 ? argv[1] : "";

    prompt_update_statusmessage(prompt, toxic->tox, note);
}

void cmd_nospam(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    if (toxic == NULL || self == NULL) {
        return;
    }

    Tox *tox = toxic->tox;

    long int nospam = (long int)rand_not_secure();  // the nospam isn't cryptographically sensitive

    if (argc > 0) {
        nospam = strtol(argv[1], NULL, 16);

        if ((nospam == 0 && strcmp(argv[1], "0")) || nospam < 0) {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Invalid nospam value.");
            return;
        }
    }

    uint32_t old_nospam = tox_self_get_nospam(tox);
    tox_self_set_nospam(tox, (uint32_t) nospam);

    line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Your new Tox ID is:");
    cmd_myid(window, self, toxic, 0, NULL);
    line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "");
    line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0,
                  "Any services that relied on your old ID will need to be updated manually.");
    line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "If you ever want your old Tox ID back, type '/nospam %X'",
                  old_nospam);
}

void cmd_prompt_help(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);
    UNUSED_VAR(toxic);
    UNUSED_VAR(argc);
    UNUSED_VAR(argv);

    if (self == NULL) {
        return;
    }

    help_init_menu(self);
}

void cmd_quit(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);
    UNUSED_VAR(argc);
    UNUSED_VAR(argv);
    UNUSED_VAR(self);

    exit_toxic_success(toxic);
}

void cmd_requests(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);
    UNUSED_VAR(toxic);
    UNUSED_VAR(argc);
    UNUSED_VAR(argv);

    if (self == NULL) {
        return;
    }

    if (FrndRequests.num_requests == 0) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "No pending friend requests.");
        return;
    }

    int i, j;
    int count = 0;

    for (i = 0; i < FrndRequests.max_idx; ++i) {
        if (!FrndRequests.request[i].active) {
            continue;
        }

        char id[TOX_PUBLIC_KEY_SIZE * 2 + 1] = {0};

        for (j = 0; j < TOX_PUBLIC_KEY_SIZE; ++j) {
            char d[3];
            snprintf(d, sizeof(d), "%02X", FrndRequests.request[i].key[j] & 0xff);
            strcat(id, d);
        }

        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "%d : %s", i, id);
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "%s", FrndRequests.request[i].msg);

        if (++count < FrndRequests.num_requests) {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "");
        }
    }
}

void cmd_status(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

    if (toxic == NULL || self == NULL) {
        return;
    }

    Tox *tox = toxic->tox;

    const char *errmsg;

    lock_status();

    if (argc < 1) {
        errmsg = "Require a status. Statuses are: online, busy and away.";
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, errmsg);
        goto finish;
    }

    const char *status_str = argv[1];
    Tox_User_Status status;

    if (!strcasecmp(status_str, "online")) {
        status = TOX_USER_STATUS_NONE;
    } else if (!strcasecmp(status_str, "away")) {
        status = TOX_USER_STATUS_AWAY;
    } else if (!strcasecmp(status_str, "busy")) {
        status = TOX_USER_STATUS_BUSY;
    } else {
        errmsg = "Invalid status. Valid statuses are: online, busy and away.";
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, errmsg);
        goto finish;
    }

    tox_self_set_status(tox, status);
    prompt_update_status(prompt, status);
    set_status_all_groups(toxic, status);

    line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Your status has been changed to %s.", status_str);


finish:
    unlock_status();
}

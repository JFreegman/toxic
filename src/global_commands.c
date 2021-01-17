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

#include "avatars.h"
#include "conference.h"
#include "friendlist.h"
#include "game_base.h"
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

extern char *DATA_FILE;
extern ToxWindow *prompt;
extern FriendsList Friends;
extern FriendRequests FrndRequests;

/* command functions */
void cmd_accept(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

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
    uint32_t friendnum = tox_friend_add_norequest(m, FrndRequests.request[req].key, &err);

    if (err != TOX_ERR_FRIEND_ADD_OK) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to add friend (error %d\n)", err);
        return;
    } else {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Friend request accepted.");
        on_friend_added(m, friendnum, true);
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

void cmd_add_helper(ToxWindow *self, Tox *m, const char *id_bin, const char *msg)
{
    const char *errmsg;

    Tox_Err_Friend_Add err;
    uint32_t f_num = tox_friend_add(m, (const uint8_t *) id_bin, (const uint8_t *) msg, strlen(msg), &err);

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
            on_friend_added(m, f_num, true);
            break;

        case TOX_ERR_FRIEND_ADD_NULL:

        /* fallthrough */
        default:
            errmsg = "Failed to add friend: Unknown error.";
            break;
    }

    line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, errmsg);
}

void cmd_add(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

    if (argc < 1) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Tox ID or address required.");
        return;
    }

    const char *id = argv[1];
    char msg[MAX_STR_SIZE];

    if (argc > 1) {
        if (argv[2][0] != '\"') {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Message must be enclosed in quotes.");
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
                line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Invalid Tox ID.");
                return;
            }

            id_bin[i] = x;
        }

        if (friend_is_blocked(id_bin)) {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Friend is in your block list.");
            return;
        }

        cmd_add_helper(self, m, id_bin, msg);
    } else {    /* assume id is a username@domain address and do http name server lookup */
        name_lookup(self, m, id_bin, id, msg);
    }
}

void cmd_avatar(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

    if (argc != 1 || strlen(argv[1]) < 3) {
        avatar_unset(m);
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

    if (avatar_set(m, path, len) == -1) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0,
                      "Failed to set avatar. Avatars must be in PNG format and may not exceed %d bytes.",
                      MAX_AVATAR_FILE_SIZE);
        return;
    }

    line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Avatar set to '%s'", filename);
}

void cmd_clear(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(m);
    UNUSED_VAR(argc);
    UNUSED_VAR(argv);

    line_info_clear(self->chatwin->hst);
    force_refresh(window);
}

void cmd_connect(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

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

    char key_binary[TOX_PUBLIC_KEY_SIZE * 2 + 1];

    if (hex_string_to_bin(ascii_key, strlen(ascii_key), key_binary, TOX_PUBLIC_KEY_SIZE) == -1) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Invalid key.");
        return;
    }

    Tox_Err_Bootstrap err;
    tox_bootstrap(m, ip, port, (uint8_t *) key_binary, &err);
    tox_add_tcp_relay(m, ip, port, (uint8_t *) key_binary, &err);

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

void cmd_decline(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);
    UNUSED_VAR(m);

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

void cmd_game(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

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

    bool force_small = false;

    if (argc >= 2) {
        force_small = strcasecmp(argv[2], "small") == 0;

        if (!force_small) {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Unknown argument.");
            return;
        }
    }

    uint32_t id = rand();
    int ret = game_initialize(self, m, type, id, NULL, 0, force_small);

    switch (ret) {
        case 0: {
            break;
        }

        case -1: {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0,
                          "Window is too small. Try enlarging your window or re-running the command with the 'small' argument.");
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

void cmd_conference(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

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

        conferencenum = tox_conference_new(m, &err);

        if (err != TOX_ERR_CONFERENCE_NEW_OK) {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Conference instance failed to initialize (error %d)", err);
            return;
        }
    } else if (type == TOX_CONFERENCE_TYPE_AV) {
#ifdef AUDIO
        conferencenum = toxav_add_av_groupchat(m, audio_conference_callback, NULL);

        if (conferencenum == (uint32_t) -1) {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Audio conference instance failed to initialize");
            return;
        }

#else
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Audio support disabled by compile-time option.");
        return;
#endif
    }

    if (init_conference_win(m, conferencenum, type, NULL, 0) == -1) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Conference window failed to initialize.");
        tox_conference_delete(m, conferencenum, NULL);
        return;
    }

#ifdef AUDIO

    if (type == TOX_CONFERENCE_TYPE_AV) {
        if (!init_conference_audio_input(m, conferencenum)) {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Audio capture failed; use \"/audio on\" to try again.");
        }
    }

#endif

    line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Conference [%d] created.", conferencenum);
}

void cmd_log(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

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

void cmd_myid(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);
    UNUSED_VAR(argc);
    UNUSED_VAR(argv);

    char id_string[TOX_ADDRESS_SIZE * 2 + 1];
    char bin_id[TOX_ADDRESS_SIZE];
    tox_self_get_address(m, (uint8_t *) bin_id);

    if (bin_id_to_string(bin_id, sizeof(bin_id), id_string, sizeof(id_string)) == -1) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to print ID.");
        return;
    }

    line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "%s", id_string);
}

#ifdef QRCODE
void cmd_myqr(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

    char id_string[TOX_ADDRESS_SIZE * 2 + 1];
    char bin_id[TOX_ADDRESS_SIZE];
    tox_self_get_address(m, (uint8_t *) bin_id);

    if (bin_id_to_string(bin_id, sizeof(bin_id), id_string, sizeof(id_string)) == -1) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to create QR code.");
        return;
    }

    char nick[TOX_MAX_NAME_LENGTH];
    tox_self_get_name(m, (uint8_t *) nick);
    size_t nick_len = tox_self_get_name_size(m);
    nick[nick_len] = '\0';

    size_t data_file_len = strlen(DATA_FILE);
    char *dir = malloc(data_file_len + 1);

    if (dir == NULL) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to create QR code: Out of memory.");
        return;
    }

    size_t dir_len = get_base_dir(DATA_FILE, data_file_len, dir);

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

void cmd_nick(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

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

    tox_self_set_name(m, (uint8_t *) nick, len, NULL);
    prompt_update_nick(prompt, nick);

    store_data(m, DATA_FILE);
}

void cmd_note(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

    if (argc < 1) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Input required.");
        return;
    }

    prompt_update_statusmessage(prompt, m, argv[1]);
}

void cmd_nospam(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    long int nospam = rand();

    if (argc > 0) {
        nospam = strtol(argv[1], NULL, 16);

        if ((nospam == 0 && strcmp(argv[1], "0")) || nospam < 0) {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Invalid nospam value.");
            return;
        }
    }

    uint32_t old_nospam = tox_self_get_nospam(m);
    tox_self_set_nospam(m, (uint32_t) nospam);

    line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Your new Tox ID is:");
    cmd_myid(window, self, m, 0, NULL);
    line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "");
    line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0,
                  "Any services that relied on your old ID will need to be updated manually.");
    line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "If you ever want your old Tox ID back, type '/nospam %X'",
                  old_nospam);
}

void cmd_prompt_help(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);
    UNUSED_VAR(m);
    UNUSED_VAR(argc);
    UNUSED_VAR(argv);

    help_init_menu(self);
}

void cmd_quit(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);
    UNUSED_VAR(argc);
    UNUSED_VAR(argv);
    UNUSED_VAR(self);

    exit_toxic_success(m);
}

void cmd_requests(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);
    UNUSED_VAR(m);
    UNUSED_VAR(argc);
    UNUSED_VAR(argv);

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

void cmd_status(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

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

    tox_self_set_status(m, status);
    prompt_update_status(prompt, status);
    line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Your status has been changed to %s.", status_str);


finish:
    unlock_status();
}

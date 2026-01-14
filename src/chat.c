/*  chat.c
 *
 *  Copyright (C) 2014-2026 Toxic All Rights Reserved.
 *
 *  This file is part of Toxic. Toxic is free software licensed
 *  under the GNU General Public License 3.0.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE    /* needed for strcasestr() and wcswidth() */
#endif

#include "chat.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>

#include "autocomplete.h"
#include "execute.h"
#include "file_transfers.h"
#include "friendlist.h"
#include "help.h"
#include "input.h"
#include "line_info.h"
#include "log.h"
#include "message_queue.h"
#include "misc_tools.h"
#include "notify.h"
#include "settings.h"
#include "toxic.h"
#include "toxic_strings.h"
#include "windows.h"

#ifdef GAMES
#include "game_base.h"
#endif

#ifdef AUDIO
#include "audio_call.h"
#ifdef VIDEO
#include "video_call.h"
#endif /* VIDEO */
#endif /* AUDIO */

#ifdef AUDIO
static void init_infobox(ToxWindow *self, double VAD_threshold);
static void kill_infobox(ToxWindow *self);
#endif /* AUDIO */

/* Array of chat command names used for tab completion. */
static const char *const chat_cmd_list[] = {
    "/accept",
    "/add",
    "/autoaccept",
    "/avatar",
    "/cancel",
    "/cinvite",
    "/cjoin",
    "/clear",
    "/close",
    "/color",
    "/connect",
    "/exit",
    "/gaccept",
    "/conference",
    "/group",
#ifdef GAMES
    "/game",
    "/play",
#endif
    "/help",
    "/invite",
    "/join",
    "/log",
    "/myid",
#ifdef QRCODE
    "/myqr",
#endif /* QRCODE */
    "/nick",
    "/note",
    "/nospam",
    "/quit",
    "/savefile",
    "/sendfile",
    "/status",

#ifdef AUDIO

    "/call",
    "/answer",
    "/reject",
    "/hangup",
    "/sdev",
    "/mute",
    "/sense",
    "/bitrate",

#endif /* AUDIO */

#ifdef VIDEO

    "/res",
    "/vcall",
    "/video",

#endif /* VIDEO */

#ifdef PYTHON

    "/run",

#endif /* PYTHON */
};

static void set_self_typingstatus(ToxWindow *self, Toxic *toxic, bool is_typing)
{
    if (!toxic->c_config->show_typing_self) {
        return;
    }

    ChatContext *ctx = self->chatwin;

    Tox_Err_Set_Typing err;
    tox_self_set_typing(toxic->tox, self->num, is_typing, &err);

    if (err != TOX_ERR_SET_TYPING_OK) {
        fprintf(stderr, "Warning: tox_self_set_typing() failed with error %d\n", err);
        return;
    }

    ctx->self_is_typing = is_typing;
}

void kill_chat_window(ToxWindow *self, Toxic *toxic)
{
    ChatContext *ctx = self->chatwin;
    StatusBar *statusbar = self->stb;

#ifdef AUDIO
    stop_current_call(self, toxic);
#endif /* AUDIO */

    kill_all_file_transfers_friend(toxic, self->num);

    if (ctx != NULL) {
        log_disable(ctx->log);
        line_info_cleanup(ctx->hst);
        cqueue_cleanup(ctx->cqueue);

        delwin(ctx->linewin);
        delwin(ctx->history);

        free(ctx->log);
        free(ctx);
    }

    delwin(statusbar->topline);
    free(self->help);
    free(statusbar);

    disable_friend_window(self->num);
    kill_notifs(self->active_box);
    del_window(self, toxic->windows, toxic->c_config);
}

static void recv_message_helper(ToxWindow *self, const Toxic *toxic, const char *msg,
                                const char *nick)
{
    const Client_Config *c_config = toxic->c_config;
    ChatContext *ctx = self->chatwin;

    line_info_add(self, c_config, true, nick, NULL, IN_MSG, 0, 0, "%s", msg);
    write_to_log(ctx->log, c_config, msg, nick, LOG_HINT_NORMAL_I);

    if (self->active_box != -1) {
        box_notify2(self, toxic, generic_message, NT_WNDALERT_1 | NT_NOFOCUS | c_config->bell_on_message,
                    self->active_box, "%s", msg);
    } else {
        box_notify(self, toxic, generic_message, NT_WNDALERT_1 | NT_NOFOCUS | c_config->bell_on_message,
                   &self->active_box, nick, "%s", msg);
    }
}

static void recv_action_helper(ToxWindow *self, const Toxic *toxic,  const char *action, const char *nick)
{
    const Client_Config *c_config = toxic->c_config;
    ChatContext *ctx = self->chatwin;

    line_info_add(self, c_config, true, nick, NULL, IN_ACTION, 0, 0, "%s", action);
    write_to_log(ctx->log, c_config, action, nick, LOG_HINT_ACTION);

    if (self->active_box != -1) {
        box_notify2(self, toxic, generic_message, NT_WNDALERT_1 | NT_NOFOCUS | c_config->bell_on_message,
                    self->active_box, "* %s %s", nick, action);
    } else {
        box_notify(self, toxic, generic_message, NT_WNDALERT_1 | NT_NOFOCUS | c_config->bell_on_message,
                   &self->active_box, self->name, "* %s %s", nick, action);
    }
}

static void chat_onMessage(ToxWindow *self, Toxic *toxic, uint32_t num, Tox_Message_Type type, const char *msg,
                           size_t len)
{
    UNUSED_VAR(len);

    if (toxic == NULL || self == NULL) {
        return;
    }

    if (self->num != num) {
        return;
    }

    char name[TOXIC_MAX_NAME_LENGTH + 1];
    get_friend_name(name, sizeof(name), num);

    if (type == TOX_MESSAGE_TYPE_NORMAL) {
        recv_message_helper(self, toxic, msg, name);
        return;
    }

    if (type == TOX_MESSAGE_TYPE_ACTION) {
        recv_action_helper(self, toxic, msg, name);
        return;
    }
}

static void chat_pause_file_transfers(uint32_t friendnum);
static void chat_resume_file_senders(ToxWindow *self, const Toxic *toxic, uint32_t fnum);

static void chat_onConnectionChange(ToxWindow *self, Toxic *toxic, uint32_t num, Tox_Connection connection_status)
{
    if (toxic == NULL || self == NULL) {
        return;
    }

    const Client_Config *c_config = toxic->c_config;

    if (self->num != num) {
        return;
    }

    StatusBar *statusbar = self->stb;
    ChatContext *ctx = self->chatwin;
    const char *msg;

    char name[TOXIC_MAX_NAME_LENGTH + 1];
    get_friend_name(name, sizeof(name), num);

    Tox_Connection prev_status = statusbar->connection;
    statusbar->connection = connection_status;

    if (prev_status == TOX_CONNECTION_NONE) {
        chat_resume_file_senders(self, toxic, num);
        file_send_queue_check(self, toxic, self->num);

        if (c_config->show_connection_msg) {
            msg = "has come online";
            line_info_add(self, c_config, true, name, NULL, CONNECTION, 0, GREEN, "%s", msg);
            write_to_log(ctx->log, c_config, msg, name, LOG_HINT_CONNECT);
        }
    } else if (connection_status == TOX_CONNECTION_NONE) {
        Friends.list[num].is_typing = false;

        if (self->chatwin->self_is_typing) {
            set_self_typingstatus(self, toxic, false);
        }

        chat_pause_file_transfers(num);

        if (c_config->show_connection_msg) {
            msg = "has gone offline";
            line_info_add(self, c_config, true, name, NULL, DISCONNECTION, 0, RED, "%s", msg);
            write_to_log(ctx->log, c_config, msg, name, LOG_HINT_DISCONNECT);
        }
    }
}

static void chat_onTypingChange(ToxWindow *self, Toxic *toxic, uint32_t num, bool is_typing)
{
    UNUSED_VAR(toxic);

    if (self == NULL) {
        return;
    }

    if (self->num != num) {
        return;
    }

    Friends.list[num].is_typing = is_typing;
}

static void chat_onNickChange(ToxWindow *self, Toxic *toxic, uint32_t num, const char *nick, size_t length)
{
    UNUSED_VAR(toxic);

    if (self == NULL) {
        return;
    }

    if (self->num != num) {
        return;
    }

    StatusBar *statusbar = self->stb;

    snprintf(statusbar->nick, sizeof(statusbar->nick), "%s", nick);
    length = strlen(statusbar->nick);
    statusbar->nick_len = length;

    set_window_title(self, statusbar->nick, length);
}

static void chat_onNickRefresh(ToxWindow *self, Toxic *toxic)
{
    if (self == NULL || toxic == NULL) {
        return;
    }

    StatusBar *statusbar = self->stb;

    char new_name[TOXIC_MAX_NAME_LENGTH + 1];
    const uint16_t n_len = get_friend_name(new_name, sizeof(new_name), self->num);

    if (strcmp(new_name, statusbar->nick) == 0) {
        return;
    }

    char self_key[TOX_ADDRESS_SIZE];
    tox_self_get_address(toxic->tox, (uint8_t *) self_key);

    char other_key[TOX_PUBLIC_KEY_SIZE];

    if (get_friend_public_key(other_key, self->num)) {
        if (rename_logfile(toxic->windows, toxic->c_config, toxic->paths, statusbar->nick, new_name, self_key, other_key,
                           self->id) != 0) {
            fprintf(stderr, "failed to rename logfile\n");
        }
    }

    chat_onNickChange(self, toxic, self->num, new_name, n_len);
}

static void chat_onStatusChange(ToxWindow *self, Toxic *toxic, uint32_t num, Tox_User_Status status)
{
    UNUSED_VAR(toxic);

    if (self == NULL) {
        return;
    }

    if (self->num != num) {
        return;
    }

    StatusBar *statusbar = self->stb;
    statusbar->status = status;
}

static void chat_onStatusMessageChange(ToxWindow *self, uint32_t num, const char *status, size_t length)
{
    UNUSED_VAR(length);

    if (self == NULL) {
        return;
    }

    if (self->num != num) {
        return;
    }

    StatusBar *statusbar = self->stb;

    snprintf(statusbar->statusmsg, sizeof(statusbar->statusmsg), "%s", status);
    statusbar->statusmsg_len = strlen(statusbar->statusmsg);
}

static void chat_onReadReceipt(ToxWindow *self, Toxic *toxic, uint32_t num, uint32_t receipt)
{
    UNUSED_VAR(num);

    if (self == NULL) {
        return;
    }

    if (toxic == NULL) {
        return;
    }

    cqueue_remove(self, toxic, receipt);
}

/* Stops active file transfers for this friend. Called when a friend goes offline */
static void chat_pause_file_transfers(uint32_t friendnum)
{
    ToxicFriend *friend = &Friends.list[friendnum];

    for (size_t i = 0; i < MAX_FILES; ++i) {
        struct FileTransfer *fts = &friend->file_sender[i];

        if (fts->file_type == TOX_FILE_KIND_DATA && fts->state >= FILE_TRANSFER_STARTED) {
            fts->state = FILE_TRANSFER_PAUSED;
        }

        struct FileTransfer *ftr = &friend->file_receiver[i];

        if (ftr->file_type == TOX_FILE_KIND_DATA && ftr->state >= FILE_TRANSFER_STARTED) {
            ftr->state = FILE_TRANSFER_PAUSED;
        }
    }
}

/* Tries to resume broken file senders. Called when a friend comes online */
static void chat_resume_file_senders(ToxWindow *self, const Toxic *toxic, uint32_t friendnum)
{
    for (size_t i = 0; i < MAX_FILES; ++i) {
        struct FileTransfer *ft = &Friends.list[friendnum].file_sender[i];

        if (ft->state != FILE_TRANSFER_PAUSED || ft->file_type != TOX_FILE_KIND_DATA) {
            continue;
        }

        Tox_Err_File_Send err;
        ft->filenumber = tox_file_send(toxic->tox, friendnum, TOX_FILE_KIND_DATA, ft->file_size, ft->file_id,
                                       (uint8_t *) ft->file_name, strlen(ft->file_name), &err);

        if (err != TOX_ERR_FILE_SEND_OK) {
            char msg[MAX_STR_SIZE];
            snprintf(msg, sizeof(msg), "File transfer for '%s' failed.", ft->file_name);
            close_file_transfer(self, toxic, ft, TOX_FILE_CONTROL_CANCEL, msg, notif_error);
            continue;
        }
    }
}

static void chat_onFileChunkRequest(ToxWindow *self, Toxic *toxic, uint32_t friendnum, uint32_t filenumber,
                                    uint64_t position,
                                    size_t length)
{
    if (toxic == NULL || self == NULL) {
        return;
    }

    Tox *tox = toxic->tox;

    if (friendnum != self->num) {
        return;
    }

    struct FileTransfer *ft = get_file_transfer_struct(friendnum, filenumber);

    if (ft == NULL) {
        return;
    }

    if (ft->state != FILE_TRANSFER_STARTED) {
        return;
    }

    char msg[MAX_STR_SIZE];

    if (length == 0) {
        snprintf(msg, sizeof(msg), "File '%s' successfully sent.", ft->file_name);
        print_progress_bar(self, ft->bps, 100.0, ft->line_id);
        close_file_transfer(self, toxic, ft, -1, msg, transfer_completed);
        return;
    }

    if (ft->file == NULL) {
        snprintf(msg, sizeof(msg), "File transfer for '%s' failed: Null file pointer.", ft->file_name);
        close_file_transfer(self, toxic, ft, TOX_FILE_CONTROL_CANCEL, msg, notif_error);
        return;
    }

    if (ft->position != position) {
        if (fseek(ft->file, position, SEEK_SET) == -1) {
            snprintf(msg, sizeof(msg), "File transfer for '%s' failed: Seek fail.", ft->file_name);
            close_file_transfer(self, toxic, ft, TOX_FILE_CONTROL_CANCEL, msg, notif_error);
            return;
        }

        ft->position = position;
    }

    uint8_t *send_data = malloc(length);

    if (send_data == NULL) {
        snprintf(msg, sizeof(msg), "File transfer for '%s' failed: Out of memory.", ft->file_name);
        close_file_transfer(self, toxic, ft, TOX_FILE_CONTROL_CANCEL, msg, notif_error);
        return;
    }

    size_t send_length = fread(send_data, 1, length, ft->file);

    if (send_length != length) {
        snprintf(msg, sizeof(msg), "File transfer for '%s' failed: Read fail.", ft->file_name);
        close_file_transfer(self, toxic, ft, TOX_FILE_CONTROL_CANCEL, msg, notif_error);
        free(send_data);
        return;
    }

    Tox_Err_File_Send_Chunk err;
    tox_file_send_chunk(tox, ft->friendnumber, ft->filenumber, position, send_data, send_length, &err);

    free(send_data);

    if (err != TOX_ERR_FILE_SEND_CHUNK_OK) {
        fprintf(stderr, "tox_file_send_chunk failed in chat callback (error %d)\n", err);
    }

    ft->position += send_length;
    ft->bps += send_length;
}

static void chat_onFileRecvChunk(ToxWindow *self, Toxic *toxic, uint32_t friendnum, uint32_t filenumber,
                                 uint64_t position,
                                 const char *data, size_t length)
{
    UNUSED_VAR(position);

    if (toxic == NULL || self == NULL) {
        return;
    }

    if (friendnum != self->num) {
        return;
    }

    struct FileTransfer *ft = get_file_transfer_struct(friendnum, filenumber);

    if (ft == NULL) {
        return;
    }

    if (ft->state != FILE_TRANSFER_STARTED) {
        return;
    }

    char msg[MAX_STR_SIZE];

    if (length == 0) {
        snprintf(msg, sizeof(msg), "File '%s' successfully received.", ft->file_name);
        print_progress_bar(self, ft->bps, 100.0, ft->line_id);
        close_file_transfer(self, toxic, ft, -1, msg, transfer_completed);
        return;
    }

    if (ft->file == NULL) {
        snprintf(msg, sizeof(msg), "File transfer for '%s' failed: Invalid file pointer.", ft->file_name);
        close_file_transfer(self, toxic, ft, TOX_FILE_CONTROL_CANCEL, msg, notif_error);
        return;
    }

    if (fwrite(data, length, 1, ft->file) != 1) {
        snprintf(msg, sizeof(msg), "File transfer for '%s' failed: Write fail.", ft->file_name);
        close_file_transfer(self, toxic, ft, TOX_FILE_CONTROL_CANCEL, msg, notif_error);
        return;
    }

    ft->bps += length;
    ft->position += length;
}

static void chat_onFileControl(ToxWindow *self, Toxic *toxic, uint32_t friendnum, uint32_t filenumber,
                               Tox_File_Control control)
{
    if (toxic == NULL || self == NULL) {
        return;
    }

    const Client_Config *c_config = toxic->c_config;

    if (friendnum != self->num) {
        return;
    }

    struct FileTransfer *ft = get_file_transfer_struct(friendnum, filenumber);

    if (!ft) {
        return;
    }

    switch (control) {
        case TOX_FILE_CONTROL_RESUME: {    /* transfer is accepted */
            if (ft->state == FILE_TRANSFER_PENDING) {
                ft->state = FILE_TRANSFER_STARTED;
                line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "File transfer [%zu] for '%s' accepted.",
                              ft->index, ft->file_name);
                sound_notify(self, toxic, silent, NT_NOFOCUS | c_config->bell_on_filetrans_accept | NT_WNDALERT_2, NULL);
            } else if (ft->state == FILE_TRANSFER_PAUSED) {    /* transfer is resumed */
                ft->state = FILE_TRANSFER_STARTED;
            }

            break;
        }

        case TOX_FILE_CONTROL_PAUSE: {
            ft->state = FILE_TRANSFER_PAUSED;
            break;
        }

        case TOX_FILE_CONTROL_CANCEL: {
            char msg[MAX_STR_SIZE];
            snprintf(msg, sizeof(msg), "File transfer for '%s' was aborted.", ft->file_name);
            close_file_transfer(self, toxic, ft, -1, msg, notif_error);
            break;
        }
    }
}

/* Attempts to resume a broken inbound file transfer.
 *
 * Returns true if resume is successful.
 */
static bool chat_resume_broken_ft(ToxWindow *self, Toxic *toxic, uint32_t friendnum, uint32_t filenumber)
{
    if (toxic == NULL || self == NULL) {
        return false;
    }

    Tox *tox = toxic->tox;

    char msg[MAX_STR_SIZE];
    uint8_t file_id[TOX_FILE_ID_LENGTH];

    if (!tox_file_get_file_id(tox, friendnum, filenumber, file_id, NULL)) {
        return false;
    }

    bool resuming = false;
    struct FileTransfer *ft = NULL;
    size_t i;

    for (i = 0; i < MAX_FILES; ++i) {
        ft = &Friends.list[friendnum].file_receiver[i];

        if (ft->state == FILE_TRANSFER_INACTIVE) {
            continue;
        }

        if (memcmp(ft->file_id, file_id, TOX_FILE_ID_LENGTH) == 0) {
            ft->filenumber = filenumber;
            ft->state = FILE_TRANSFER_STARTED;
            resuming = true;
            break;
        }
    }

    if (!resuming || ft == NULL) {
        return false;
    }

    if (!tox_file_seek(tox, ft->friendnumber, ft->filenumber, ft->position, NULL)) {
        goto on_error;
    }

    if (!tox_file_control(tox, ft->friendnumber, ft->filenumber, TOX_FILE_CONTROL_RESUME, NULL)) {
        goto on_error;
    }

    return true;

on_error:
    snprintf(msg, sizeof(msg), "File transfer for '%s' failed.", ft->file_name);
    close_file_transfer(self, toxic, ft, TOX_FILE_CONTROL_CANCEL, msg, notif_error);
    return false;
}

/*
 * Return true if file name is valid.
 *
 * A valid file name:
 * - cannot be empty.
 * - cannot contain the '/' characters.
 * - cannot begin with a space or hyphen.
 * - cannot be "." or ".."
 */
static bool valid_file_name(const char *filename, size_t length)
{
    if (length == 0) {
        return false;
    }

    if (filename[0] == ' ' || filename[0] == '-') {
        return false;
    }

    if (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0) {
        return false;
    }

    for (size_t i = 0; i < length; ++i) {
        if (filename[i] == '/') {
            return false;
        }
    }

    return true;
}

static void chat_onFileRecv(ToxWindow *self, Toxic *toxic, uint32_t friendnum, uint32_t filenumber, uint64_t file_size,
                            const char *filename, size_t name_length)
{
    if (toxic == NULL || self == NULL) {
        return;
    }

    Tox *tox = toxic->tox;
    const Client_Config *c_config = toxic->c_config;

    if (self->num != friendnum) {
        return;
    }

    /* first check if we need to resume a broken transfer */
    if (chat_resume_broken_ft(self, toxic, friendnum, filenumber)) {
        return;
    }

    struct FileTransfer *ft = new_file_transfer(self, friendnum, filenumber, FILE_TRANSFER_RECV, TOX_FILE_KIND_DATA);

    if (ft == NULL) {
        tox_file_control(tox, friendnum, filenumber, TOX_FILE_CONTROL_CANCEL, NULL);
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0,
                      "File transfer request failed: Too many concurrent file transfers.");
        return;
    }

    char sizestr[32];
    bytes_convert_str(sizestr, sizeof(sizestr), file_size);
    line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "File transfer request for '%s' (%s)", filename,
                  sizestr);

    if (!valid_file_name(filename, name_length)) {
        close_file_transfer(self, toxic, ft, TOX_FILE_CONTROL_CANCEL, "File transfer failed: Invalid file name.", notif_error);
        return;
    }

    size_t file_path_buf_size = PATH_MAX + name_length + 1;
    char *file_path = malloc(file_path_buf_size);

    if (file_path == NULL) {
        close_file_transfer(self, toxic, ft, TOX_FILE_CONTROL_CANCEL, "File transfer failed: Out of memory.", notif_error);
        return;
    }

    size_t path_len = name_length;

    /* use specified download path in config if possible */
    if (!string_is_empty(c_config->download_path)) {
        snprintf(file_path, file_path_buf_size, "%s%s", c_config->download_path, filename);
        path_len += strlen(c_config->download_path);
    } else {
        snprintf(file_path, file_path_buf_size, "%s", filename);
    }

    if (path_len >= file_path_buf_size || path_len >= sizeof(ft->file_path) || name_length >= sizeof(ft->file_name)) {
        close_file_transfer(self, toxic, ft, TOX_FILE_CONTROL_CANCEL, "File transfer failed: File path too long.", notif_error);
        free(file_path);
        return;
    }

    /* Append a number to duplicate file names */
    FILE *filecheck = NULL;
    int count = 1;

    while ((filecheck = fopen(file_path, "r")) || file_transfer_recv_path_exists(file_path)) {
        if (filecheck) {
            fclose(filecheck);
        }

        file_path[path_len] = '\0';
        char d[5];
        snprintf(d, sizeof(d), "(%d)", count);
        size_t d_len = strlen(d);

        if (path_len + d_len >= file_path_buf_size) {
            close_file_transfer(self, toxic, ft, TOX_FILE_CONTROL_CANCEL, "File transfer failed: File path too long.", notif_error);
            free(file_path);
            return;
        }

        strcat(file_path, d);
        file_path[path_len + d_len] = '\0';

        if (++count > 99) {  // If there are this many duplicate file names we should probably give up
            close_file_transfer(self, toxic, ft, TOX_FILE_CONTROL_CANCEL, "File transfer failed: invalid file path.", notif_error);
            free(file_path);
            return;
        }
    }

    ft->file_size = file_size;
    snprintf(ft->file_path, sizeof(ft->file_path), "%s", file_path);
    snprintf(ft->file_name, sizeof(ft->file_name), "%s", filename);
    tox_file_get_file_id(tox, friendnum, filenumber, ft->file_id, NULL);

    free(file_path);

    if (self->active_box != -1) {
        box_notify2(self, toxic, transfer_pending, NT_WNDALERT_0 | NT_NOFOCUS | c_config->bell_on_filetrans,
                    self->active_box, "Incoming file: %s", filename);
    } else {
        box_notify(self, toxic, transfer_pending, NT_WNDALERT_0 | NT_NOFOCUS | c_config->bell_on_filetrans,
                   &self->active_box, self->name, "Incoming file: %s", filename);
    }

    const bool auto_accept_files = friend_get_auto_accept_files(friendnum);

    if (auto_accept_files) {
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Auto-accepting file transfer %zu", ft->index);

        char cmd[MAX_STR_SIZE];
        snprintf(cmd, sizeof(cmd), "/savefile %zu", ft->index);
        execute(self->window, self, toxic, cmd, CHAT_COMMAND_MODE);
    } else {
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0,
                      "Type '/savefile %zu' to accept the file transfer.", ft->index);
    }
}

static void chat_onConferenceInvite(ToxWindow *self, Toxic *toxic, int32_t friendnumber, uint8_t type,
                                    const char *conference_pub_key,
                                    uint16_t length)
{
    if (toxic == NULL || self == NULL) {
        return;
    }

    const Client_Config *c_config = toxic->c_config;

    if (self->num != friendnumber) {
        return;
    }

    if (Friends.list[friendnumber].conference_invite.key != NULL) {
        free(Friends.list[friendnumber].conference_invite.key);
    }

    char *k = malloc(length * sizeof(char));

    if (k == NULL) {
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Conference invite failed (OOM)");
        return;
    }

    memcpy(k, conference_pub_key, length);
    Friends.list[friendnumber].conference_invite.key = k;
    Friends.list[friendnumber].conference_invite.pending = true;
    Friends.list[friendnumber].conference_invite.length = length;
    Friends.list[friendnumber].conference_invite.type = type;

    char name[TOXIC_MAX_NAME_LENGTH + 1];
    get_friend_name(name, sizeof(name), friendnumber);

    const char *description = type == TOX_CONFERENCE_TYPE_AV ? "an audio conference" : "a conference";

    if (self->active_box != -1) {
        box_notify2(self, toxic, generic_message, NT_WNDALERT_2 | c_config->bell_on_invite, self->active_box,
                    "invites you to join %s", description);
    } else {
        box_notify(self, toxic, generic_message, NT_WNDALERT_2 | c_config->bell_on_invite, &self->active_box, name,
                   "invites you to join %s", description);
    }

    line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "%s has invited you to a conference.", name);
    line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Type \"/cjoin\" to join the chat.");
}

static void chat_onGroupInvite(ToxWindow *self, Toxic *toxic, uint32_t friendnumber, const char *invite_data,
                               size_t length,
                               const char *group_name, size_t group_name_length)
{
    UNUSED_VAR(group_name_length);

    if (self == NULL || toxic == NULL) {
        return;
    }

    const Client_Config *c_config = toxic->c_config;

    if (self->num != friendnumber) {
        return;
    }

    if (Friends.list[friendnumber].group_invite.data) {
        free(Friends.list[friendnumber].group_invite.data);
    }

    Friends.list[friendnumber].group_invite.data = malloc(length * sizeof(char));

    if (Friends.list[friendnumber].group_invite.data == NULL) {
        return;
    }

    memcpy(Friends.list[friendnumber].group_invite.data, invite_data, length);
    Friends.list[friendnumber].group_invite.length = length;

    char name[TOXIC_MAX_NAME_LENGTH + 1];
    get_friend_name(name, sizeof(name), friendnumber);

    const uint64_t flags = NT_WNDALERT_2 | c_config->bell_on_invite;

    if (self->active_box != -1) {
        box_notify2(self, toxic, generic_message, flags, self->active_box,
                    "You have been invited to a group chat.");
    } else {
        box_notify(self, toxic, generic_message, flags, &self->active_box, name,
                   "You have been invited to a group chat.");
    }

    line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "%s has invited you to join group chat \"%s\"",
                  name, group_name);
    line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0,
                  "Type \"/gaccept <password>\" to join the chat (password is optional).");
}

#ifdef GAMES
static void chat_onGameInvite(ToxWindow *self, Toxic *toxic, uint32_t friend_number, const uint8_t *data, size_t length)
{
    if (toxic == NULL || self == NULL) {
        return;
    }

    const Client_Config *c_config = toxic->c_config;

    if (self->num != friend_number) {
        return;
    }

    if (length < GAME_PACKET_HEADER_SIZE || length > GAME_MAX_DATA_SIZE) {
        return;
    }

    const uint8_t version = data[0];

    if (version != GAME_NETWORKING_VERSION) {
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0,
                      "Game invite failed. Friend has network protocol version %d, you have version %d.", version, GAME_NETWORKING_VERSION);
        return;
    }

    GameType type = data[1];

    if (!game_type_has_multiplayer(type)) {
        return;
    }

    uint32_t id;
    game_util_unpack_u32(data + 2, &id);

    const char *game_string = game_get_name_string(type);

    if (game_string == NULL) {
        return;
    }

    uint32_t data_length = length - GAME_PACKET_HEADER_SIZE;

    if (data_length > 0) {
        free(Friends.list[friend_number].game_invite.data);

        uint8_t *buf = calloc(1, data_length);

        if (buf == NULL) {
            return;
        }

        memcpy(buf, data + GAME_PACKET_HEADER_SIZE, data_length);
        Friends.list[friend_number].game_invite.data = buf;
    }

    Friends.list[friend_number].game_invite.type = type;
    Friends.list[friend_number].game_invite.id = id;
    Friends.list[friend_number].game_invite.pending = true;
    Friends.list[friend_number].game_invite.data_length = data_length;

    char name[TOXIC_MAX_NAME_LENGTH + 1];
    get_friend_name(name, sizeof(name), friend_number);

    if (self->active_box != -1) {
        box_notify2(self, toxic, generic_message, NT_WNDALERT_2 | c_config->bell_on_invite, self->active_box,
                    "invites you to play %s", game_string);
    } else {
        box_notify(self, toxic, generic_message, NT_WNDALERT_2 | c_config->bell_on_invite, &self->active_box,
                   name,
                   "invites you to play %s", game_string);
    }


    line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "%s has invited you to a game of %s.", name,
                  game_string);
    line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Type \"/play\" to join the game.");
}

#endif // GAMES

/* AV Stuff */
#ifdef AUDIO

static void chat_onInvite(ToxWindow *self, Toxic *toxic, uint32_t friend_number, int state)
{
    UNUSED_VAR(state);

    if (toxic == NULL || self == NULL) {
        return;
    }

    const Client_Config *c_config = toxic->c_config;

    if (self->num != friend_number) {
        return;
    }

    /* call is flagged active here */
    self->is_call = true;

    line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0,
                  "Incoming audio call! Type: \"/answer\" or \"/reject\"");

    uint64_t box_flags = NT_NOFOCUS | NT_WNDALERT_0;

    if (self->ringing_sound == -1) {
        sound_notify(self, toxic, call_incoming, NT_LOOP | c_config->bell_on_invite, &self->ringing_sound);
        box_flags |= NT_NO_INCREMENT;
    }

    if (self->active_box != -1) {
        box_silent_notify2(self, toxic, box_flags, self->active_box, "Incoming audio call!");
    } else {
        box_silent_notify(self, toxic, box_flags, &self->active_box, self->name, "Incoming audio call!");
    }
}

static void chat_onRinging(ToxWindow *self, Toxic *toxic, uint32_t friend_number, int state)
{
    UNUSED_VAR(state);

    if (self == NULL || toxic == NULL) {
        return;
    }

    const Client_Config *c_config = toxic->c_config;

    if (self->num != friend_number) {
        return;
    }

    line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0,
                  "Ringing...type \"/hangup\" to cancel it.");

#ifdef SOUND_NOTIFY

    if (self->ringing_sound == -1) {
        sound_notify(self, toxic, call_outgoing, NT_LOOP, &self->ringing_sound);
    }

#endif /* SOUND_NOTIFY */
}

static void chat_onStarting(ToxWindow *self, Toxic *toxic, uint32_t friend_number, int state)
{
    UNUSED_VAR(state);

    if (self == NULL || self->num != friend_number) {
        return;
    }

    if (toxic == NULL) {
        return;
    }

    const Client_Config *c_config = toxic->c_config;

    init_infobox(self, c_config->VAD_threshold);

    line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Call started! Type: \"/hangup\" to end it.");

    /* call is flagged active here */
    self->is_call = true;

#ifdef SOUND_NOTIFY
    stop_sound(self->ringing_sound);
#endif /* SOUND_NOTIFY */
}

static void chat_onError(ToxWindow *self, Toxic *toxic, uint32_t friend_number, int state)
{
    UNUSED_VAR(state);

    if (!self || self->num != friend_number) {
        return;
    }

    if (toxic == NULL) {
        return;
    }

    self->is_call = false;
    line_info_add(self, toxic->c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Error!");

#ifdef SOUND_NOTIFY
    stop_sound(self->ringing_sound);
#endif /* SOUND_NOTIFY */
}

static void chat_onStart(ToxWindow *self, Toxic *toxic, uint32_t friend_number, int state)
{
    UNUSED_VAR(state);

    if (!self || self->num != friend_number) {
        return;
    }

    if (toxic == NULL) {
        return;
    }

    const Client_Config *c_config = toxic->c_config;

    /* call is flagged active here */
    self->is_call = true;

    init_infobox(self, c_config->VAD_threshold);

    line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Call started! Type: \"/hangup\" to end it.");

#ifdef SOUND_NOTIFY
    stop_sound(self->ringing_sound);
#endif /* SOUND_NOTIFY */
}

static void chat_onCancel(ToxWindow *self, Toxic *toxic, uint32_t friend_number, int state)
{
    UNUSED_VAR(state);

    if (!self || self->num != friend_number) {
        return;
    }

    if (toxic == NULL) {
        return;
    }

    self->is_call = false;
    kill_infobox(self);
    line_info_add(self, toxic->c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Call canceled!");

#ifdef SOUND_NOTIFY
    stop_sound(self->ringing_sound);
#endif /* SOUND_NOTIFY */
}

static void chat_onReject(ToxWindow *self, Toxic *toxic, uint32_t friend_number, int state)
{
    UNUSED_VAR(state);

    if (!self  || self->num != friend_number) {
        return;
    }

    if (toxic == NULL) {
        return;
    }

    line_info_add(self, toxic->c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Rejected!");
    self->is_call = false;

#ifdef SOUND_NOTIFY
    stop_sound(self->ringing_sound);
#endif /* SOUND_NOTIFY */
}

static void chat_onEnd(ToxWindow *self, Toxic *toxic, uint32_t friend_number, int state)
{
    UNUSED_VAR(state);

    if (!self || self->num != friend_number) {
        return;
    }

    if (toxic == NULL) {
        return;
    }

    kill_infobox(self);
    line_info_add(self, toxic->c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Call ended!");
    self->is_call = false;

#ifdef SOUND_NOTIFY
    stop_sound(self->ringing_sound);
#endif /* SOUND_NOTIFY */
}

static void init_infobox(ToxWindow *self, double VAD_threshold)
{
    ChatContext *ctx = self->chatwin;

    int x2, y2;
    getmaxyx(self->window, y2, x2);

    if (y2 <= 0 || x2 <= 0) {
        return;
    }

    UNUSED_VAR(y2);

    ctx->infobox = (struct infobox) {
        0
    };

    ctx->infobox.win = newwin(INFOBOX_HEIGHT, INFOBOX_WIDTH + 1, 1, x2 - INFOBOX_WIDTH);
    ctx->infobox.starttime = get_unix_time();
    ctx->infobox.vad_lvl = VAD_threshold;
    ctx->infobox.active = true;
    strcpy(ctx->infobox.timestr, "00");
}

static void kill_infobox(ToxWindow *self)
{
    ChatContext *ctx = self->chatwin;

    if (!ctx->infobox.win) {
        return;
    }

    delwin(ctx->infobox.win);

    ctx->infobox = (struct infobox) {
        0
    };
}

/* update infobox info and draw in respective chat window */
static void draw_infobox(ToxWindow *self)
{
    struct infobox *infobox = &self->chatwin->infobox;

    if (infobox->win == NULL) {
        return;
    }

    int x2, y2;
    getmaxyx(self->window, y2, x2);

    if (x2 < INFOBOX_WIDTH || y2 < INFOBOX_HEIGHT) {
        return;
    }

    time_t curtime = get_unix_time();

    /* update interface once per second */
    if (timed_out(infobox->lastupdate, 1)) {
        get_elapsed_time_str(infobox->timestr, sizeof(infobox->timestr), curtime - infobox->starttime);
        infobox->lastupdate = curtime;
    }

    const char *in_is_muted = infobox->in_is_muted ? "yes" : "no";
    const char *out_is_muted = infobox->out_is_muted ? "yes" : "no";

    wmove(infobox->win, 1, 1);
    wattron(infobox->win, COLOR_PAIR(RED) | A_BOLD);
    wprintw(infobox->win, "    Call Active\n");
    wattroff(infobox->win, COLOR_PAIR(RED) | A_BOLD);

    wattron(infobox->win, A_BOLD);
    wprintw(infobox->win, " Duration: ");
    wattroff(infobox->win, A_BOLD);
    wprintw(infobox->win, "%s\n", infobox->timestr);

    wattron(infobox->win, A_BOLD);
    wprintw(infobox->win, " In muted: ");
    wattroff(infobox->win, A_BOLD);
    wprintw(infobox->win, "%s\n", in_is_muted);

    wattron(infobox->win, A_BOLD);
    wprintw(infobox->win, " Out muted: ");
    wattroff(infobox->win, A_BOLD);
    wprintw(infobox->win, "%s\n", out_is_muted);

    wattron(infobox->win, A_BOLD);
    wprintw(infobox->win, " VAD level: ");
    wattroff(infobox->win, A_BOLD);
    wprintw(infobox->win, "%.2f\n", (double) infobox->vad_lvl);

    wborder(infobox->win, ACS_VLINE, ' ', ACS_HLINE, ACS_HLINE, ACS_ULCORNER, ' ', ACS_LLCORNER, ' ');
    wnoutrefresh(infobox->win);
}

#endif /* AUDIO */

static void send_action(ToxWindow *self, ChatContext *ctx, Toxic *toxic, char *action)
{
    if (action == NULL) {
        return;
    }

    char selfname[TOX_MAX_NAME_LENGTH + 1];
    tox_self_get_name(toxic->tox, (uint8_t *) selfname);

    const size_t len = tox_self_get_name_size(toxic->tox);
    selfname[len] = '\0';

    const int id = line_info_add(self, toxic->c_config, true, selfname, NULL, OUT_ACTION, 0, 0, "%s", action);
    cqueue_add(ctx->cqueue, action, strlen(action), OUT_ACTION, id);
}

/*
 * Return true if input is recognized by handler
 */
static bool chat_onKey(ToxWindow *self, Toxic *toxic, wint_t key, bool ltr)
{
    if (toxic == NULL || self == NULL) {
        return false;
    }

    Tox *tox = toxic->tox;
    const Client_Config *c_config = toxic->c_config;

    ChatContext *ctx = self->chatwin;
    StatusBar *statusbar = self->stb;

    int x;
    int y;
    int y2;
    int x2;
    getyx(self->window, y, x);
    getmaxyx(self->window, y2, x2);

    UNUSED_VAR(y);

    if (y2 <= 0 || x2 <= 0) {
        return false;
    }

    if (ctx->pastemode && key == L'\r') {
        key = L'\n';
    }

    if (self->help->active) {
        help_onKey(self, key);
        return true;
    }

    if (ltr || key == L'\n') {    /* char is printable */
        input_new_char(self, toxic, key, x, x2);

        if (ctx->line[0] != '/' && !ctx->self_is_typing && statusbar->connection != TOX_CONNECTION_NONE) {
            set_self_typingstatus(self, toxic, true);
        }

        return true;
    }

    if (line_info_onKey(self, c_config, key)) {
        return true;
    }

    bool input_ret = input_handle(self, toxic, key, x, x2);

    if (key == L'\t' && ctx->len > 1 && ctx->line[0] == '/') {    /* TAB key: auto-complete */
        input_ret = true;
        int diff;

        /* TODO: make this not suck */
        if (wcsncmp(ctx->line, L"/sendfile ", wcslen(L"/sendfile ")) == 0) {
            diff = dir_match(self, toxic, ctx->line, L"/sendfile");
        } else if (wcsncmp(ctx->line, L"/avatar ", wcslen(L"/avatar ")) == 0) {
            diff = dir_match(self, toxic, ctx->line, L"/avatar");
        }

#ifdef PYTHON
        else if (wcsncmp(ctx->line, L"/run ", wcslen(L"/run ")) == 0) {
            diff = dir_match(self, toxic, ctx->line, L"/run");
        }

#endif

        else {
            diff = complete_line(self, toxic, chat_cmd_list, sizeof(chat_cmd_list) / sizeof(char *));
        }

        if (diff >= 0) {
            if (x + diff > x2 - 1) {
                const int wlen = MAX(0, wcswidth(ctx->line, sizeof(ctx->line) / sizeof(wchar_t)));
                ctx->start = wlen < x2 ? 0 : wlen - x2 + 1;
            }
        } else {
            sound_notify(self, toxic, notif_error, 0, NULL);
        }

    } else if (key == L'\r') {
        input_ret = true;
        rm_trailing_spaces_buf(ctx);

        wstrsubst(ctx->line, L'¶', L'\n');

        char line[MAX_STR_SIZE];

        if (wcs_to_mbs_buf(line, ctx->line, MAX_STR_SIZE) == -1) {
            memset(line, 0, sizeof(line));
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, RED, " * Failed to parse message.");
        }

        const bool contains_blocked_word = string_contains_blocked_word(line, &toxic->client_data);

        if (line[0] != '\0' && !contains_blocked_word) {
            add_line_to_hist(ctx);

            if (line[0] == '/') {
                if (strcmp(line, "/close") == 0) {
                    kill_chat_window(self, toxic);
                    return input_ret;
                } else if (strncmp(line, "/me ", strlen("/me ")) == 0) {
                    send_action(self, ctx, toxic, line + strlen("/me "));
                } else {
                    execute(ctx->history, self, toxic, line, CHAT_COMMAND_MODE);
                }
            } else {
                char selfname[TOX_MAX_NAME_LENGTH + 1];
                tox_self_get_name(tox, (uint8_t *) selfname);

                const size_t len = tox_self_get_name_size(tox);
                selfname[len] = '\0';

                const int id = line_info_add(self, c_config, true, selfname, NULL, OUT_MSG, 0, 0, "%s", line);
                cqueue_add(ctx->cqueue, line, strlen(line), OUT_MSG, id);
            }
        }

        if (!contains_blocked_word) {
            wclear(ctx->linewin);
            wmove(self->window, y2, 0);
            reset_buf(ctx);
        } else {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, RED, "* Message contains blocked word");
        }
    }

    if (ctx->len <= 0 && ctx->self_is_typing) {
        set_self_typingstatus(self, toxic, false);
    }

    return input_ret;
}

static void chat_onDraw(ToxWindow *self, Toxic *toxic)
{
    if (toxic == NULL || self == NULL) {
        fprintf(stderr, "chat_onDraw null param\n");
        return;
    }

    int x2;
    int y2;
    getmaxyx(self->window, y2, x2);

    if (y2 <= 0 || x2 <= 0) {
        return;
    }

    ChatContext *ctx = self->chatwin;
    StatusBar *statusbar = self->stb;

    pthread_mutex_lock(&Winthread.lock);

    line_info_print(self, toxic->c_config);

    Tox_Connection connection = statusbar->connection;
    Tox_User_Status status = statusbar->status;
    const bool is_typing = Friends.list[self->num].is_typing;

    pthread_mutex_unlock(&Winthread.lock);

    wclear(ctx->linewin);

    if (ctx->len > 0) {
        mvwprintw(ctx->linewin, 0, 0, "%ls", &ctx->line[ctx->start]);
    }

    curs_set(1);

    wmove(statusbar->topline, 0, 0);

    wattron(statusbar->topline, COLOR_PAIR(BAR_ACCENT));
    wprintw(statusbar->topline, " [");
    wattroff(statusbar->topline, COLOR_PAIR(BAR_ACCENT));

    switch (connection) {
        case TOX_CONNECTION_TCP:
            wattron(statusbar->topline, A_BOLD | COLOR_PAIR(STATUS_ONLINE));
            wprintw(statusbar->topline, "TCP");
            wattroff(statusbar->topline, A_BOLD | COLOR_PAIR(STATUS_ONLINE));
            break;

        case TOX_CONNECTION_UDP:
            wattron(statusbar->topline, A_BOLD | COLOR_PAIR(STATUS_ONLINE));
            wprintw(statusbar->topline, "UDP");
            wattroff(statusbar->topline, A_BOLD | COLOR_PAIR(STATUS_ONLINE));
            break;

        default:
            wattron(statusbar->topline, COLOR_PAIR(BAR_TEXT));
            wprintw(statusbar->topline, "Offline");
            wattroff(statusbar->topline, COLOR_PAIR(BAR_TEXT));
            break;
    }

    wattron(statusbar->topline, COLOR_PAIR(BAR_ACCENT));
    wprintw(statusbar->topline, "] ");
    wattroff(statusbar->topline, COLOR_PAIR(BAR_ACCENT));

    const char *status_text = "ERROR";
    int colour = BAR_TEXT;

    if (connection != TOX_CONNECTION_NONE) {
        switch (status) {
            case TOX_USER_STATUS_AWAY:
                colour = STATUS_AWAY;
                status_text = "Away";
                break;

            case TOX_USER_STATUS_BUSY:
                colour = STATUS_BUSY;
                status_text = "Busy";
                break;

            default:
                break;
        }
    }

    if (colour != BAR_TEXT) {
        wattron(statusbar->topline, COLOR_PAIR(BAR_ACCENT));
        wprintw(statusbar->topline, "[");
        wattroff(statusbar->topline, COLOR_PAIR(BAR_ACCENT));

        wattron(statusbar->topline, COLOR_PAIR(colour) | A_BOLD);
        wprintw(statusbar->topline, "%s", status_text);
        wattroff(statusbar->topline, COLOR_PAIR(colour) | A_BOLD);

        wattron(statusbar->topline, COLOR_PAIR(BAR_ACCENT));
        wprintw(statusbar->topline, "] ");
        wattroff(statusbar->topline, COLOR_PAIR(BAR_ACCENT));
    }

    if (is_typing) {
        wattron(statusbar->topline, A_BOLD | COLOR_PAIR(BAR_NOTIFY));
    } else {
        wattron(statusbar->topline, COLOR_PAIR(BAR_TEXT));
    }

    wprintw(statusbar->topline, "%s", statusbar->nick);

    if (is_typing) {
        wattroff(statusbar->topline, A_BOLD | COLOR_PAIR(BAR_NOTIFY));
    } else {
        wattroff(statusbar->topline, A_BOLD | COLOR_PAIR(BAR_TEXT));
    }


    /* Reset statusbar->statusmsg on window resize */
    if (x2 != self->x) {
        char statusmsg[TOX_MAX_STATUS_MESSAGE_LENGTH] = {'\0'};

        pthread_mutex_lock(&Winthread.lock);

        tox_friend_get_status_message(toxic->tox, self->num, (uint8_t *) statusmsg, NULL);
        const size_t s_len = tox_friend_get_status_message_size(toxic->tox, self->num, NULL);

        filter_string(statusmsg, s_len, false);
        snprintf(statusbar->statusmsg, sizeof(statusbar->statusmsg), "%s", statusmsg);
        statusbar->statusmsg_len = strlen(statusbar->statusmsg);

        pthread_mutex_unlock(&Winthread.lock);
    }

    self->x = x2;

    /* Truncate note if it doesn't fit in statusbar */
    const int maxlen = x2 - getcurx(statusbar->topline) - KEY_IDENT_BYTES - 6;

    pthread_mutex_lock(&Winthread.lock);
    const size_t statusmsg_len = statusbar->statusmsg_len;
    pthread_mutex_unlock(&Winthread.lock);

    if (statusmsg_len > maxlen && maxlen >= 3) {
        pthread_mutex_lock(&Winthread.lock);
        statusbar->statusmsg[maxlen - 3] = '\0';
        strcat(statusbar->statusmsg, "...");
        statusbar->statusmsg_len = maxlen;
        pthread_mutex_unlock(&Winthread.lock);
    }

    if (statusmsg_len > 0) {
        wattron(statusbar->topline, COLOR_PAIR(BAR_ACCENT));
        wprintw(statusbar->topline, " | ");
        wattroff(statusbar->topline, COLOR_PAIR(BAR_ACCENT));

        wattron(statusbar->topline, COLOR_PAIR(BAR_TEXT));
        pthread_mutex_lock(&Winthread.lock);
        wprintw(statusbar->topline, "%s ", statusbar->statusmsg);
        pthread_mutex_unlock(&Winthread.lock);
    } else {
        wattron(statusbar->topline, COLOR_PAIR(BAR_TEXT));
    }

    int s_y;
    int s_x;
    getyx(statusbar->topline, s_y, s_x);

    mvwhline(statusbar->topline, s_y, s_x, ' ', x2 - s_x - KEY_IDENT_BYTES  - 3);
    wattroff(statusbar->topline, COLOR_PAIR(BAR_TEXT));

    wmove(statusbar->topline, 0, x2 - KEY_IDENT_BYTES  - 3);

    wattron(statusbar->topline, COLOR_PAIR(BAR_ACCENT));
    wprintw(statusbar->topline, "{");
    wattroff(statusbar->topline, COLOR_PAIR(BAR_ACCENT));

    wattron(statusbar->topline, COLOR_PAIR(BAR_TEXT));

    for (size_t i = 0; i < KEY_IDENT_BYTES / 2; ++i) {
        wprintw(statusbar->topline, "%02X", Friends.list[self->num].pub_key[i] & 0xff);
    }

    wattroff(statusbar->topline, COLOR_PAIR(BAR_TEXT));

    wattron(statusbar->topline, COLOR_PAIR(BAR_ACCENT));
    wprintw(statusbar->topline, "} ");
    wattroff(statusbar->topline, COLOR_PAIR(BAR_ACCENT));

    int y;
    int x;
    getyx(self->window, y, x);

    UNUSED_VAR(x);

    const int new_x = ctx->start ? x2 - 1 : MAX(0, wcswidth(ctx->line, ctx->pos));
    wmove(self->window, y, new_x);

    draw_window_bar(self, toxic->windows);

    wnoutrefresh(self->window);

#ifdef AUDIO

    if (ctx->infobox.active) {
        draw_infobox(self);
    }

#endif

    if (self->help->active) {
        help_draw_main(self);
    }

    pthread_mutex_lock(&Winthread.lock);

    if (refresh_file_transfer_progress(self, self->num)) {
        flag_interface_refresh();
    }

    pthread_mutex_unlock(&Winthread.lock);
}

static void chat_init_log(ToxWindow *self, Toxic *toxic, const char *self_nick)
{
    Tox *tox = toxic->tox;

    ChatContext *ctx = self->chatwin;
    const Client_Config *c_config = toxic->c_config;

    char myid[TOX_ADDRESS_SIZE];
    tox_self_get_address(tox, (uint8_t *) myid);

    if (log_init(ctx->log, c_config, toxic->paths, self_nick, myid, Friends.list[self->num].pub_key, LOG_TYPE_CHAT) != 0) {
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to initialize chat log.");
        return;
    }

    if (load_chat_history(ctx->log, self, c_config) != 0) {
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to load chat history.");
    }

    if (friend_get_logging_enabled(self->num)) {
        if (log_enable(ctx->log) != 0) {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to enable chat log.");
        }
    }
}

static void chat_onInit(ToxWindow *self, Toxic *toxic)
{
    curs_set(1);

    if (toxic == NULL || self == NULL) {
        return;
    }

    Tox *tox = toxic->tox;

    int x2;
    int y2;
    getmaxyx(self->window, y2, x2);

    if (y2 <= 0 || x2 <= 0) {
        exit_toxic_err(FATALERR_CURSES, "failed in chat_onInit");
    }

    self->x = x2;

    /* Init statusbar info */
    StatusBar *statusbar = self->stb;

    statusbar->status = get_friend_status(self->num);
    statusbar->connection = get_friend_connection_status(self->num);

    const size_t s_len = tox_friend_get_status_message_size(tox, self->num, NULL);

    if (s_len > 0 && s_len <= TOX_MAX_STATUS_MESSAGE_LENGTH) {
        char statusmsg[TOX_MAX_STATUS_MESSAGE_LENGTH + 1] = {0};
        tox_friend_get_status_message(tox, self->num, (uint8_t *) statusmsg, NULL);
        statusmsg[s_len] = '\0';
        filter_string(statusmsg, s_len, false);
        snprintf(statusbar->statusmsg, sizeof(statusbar->statusmsg), "%s", statusmsg);
    }

    statusbar->statusmsg_len = strlen(statusbar->statusmsg);

    char name[TOXIC_MAX_NAME_LENGTH + 1];
    const uint16_t n_len = get_friend_name(name, sizeof(name), self->num);

    snprintf(statusbar->nick, sizeof(statusbar->nick), "%s", name);
    statusbar->nick_len = n_len;

    /* Init subwindows */
    ChatContext *ctx = self->chatwin;

    statusbar->topline = subwin(self->window, TOP_BAR_HEIGHT, x2, 0, 0);
    ctx->history = subwin(self->window, y2 - CHATBOX_HEIGHT - WINDOW_BAR_HEIGHT, x2, 0, 0);
    self->window_bar = subwin(self->window, WINDOW_BAR_HEIGHT, x2, y2 - (CHATBOX_HEIGHT + WINDOW_BAR_HEIGHT), 0);
    ctx->linewin = subwin(self->window, CHATBOX_HEIGHT, x2, y2 - WINDOW_BAR_HEIGHT, 0);

    ctx->hst = calloc(1, sizeof(struct history));
    ctx->log = calloc(1, sizeof(struct chatlog));
    ctx->cqueue = calloc(1, sizeof(struct chat_queue));

    if (ctx->log == NULL || ctx->hst == NULL || ctx->cqueue == NULL) {
        exit_toxic_err(FATALERR_MEMORY, "failed in chat_onInit");
    }

    line_info_init(ctx->hst);

    const int tab_name_colour = friend_config_get_tab_name_colour(self->num);
    self->colour = tab_name_colour > 0 ? tab_name_colour : WHITE_BAR_FG;

    friend_set_logging_enabled(self->num, friend_config_get_autolog(self->num));
    friend_set_auto_file_accept(self->num, friend_config_get_auto_accept_files(self->num));

    chat_init_log(self, toxic, name);

    execute(ctx->history, self, toxic, "/log", GLOBAL_COMMAND_MODE);  // Print log status to screen

    scrollok(ctx->history, 0);
    wmove(self->window, y2 - CURS_Y_OFFSET, 0);

    line_info_print(self, toxic->c_config);
}

ToxWindow *new_chat(Tox *tox, uint32_t friendnum)
{
    ToxWindow *ret = calloc(1, sizeof(ToxWindow));

    if (ret == NULL) {
        exit_toxic_err(FATALERR_MEMORY, "failed in new_chat");
    }

    ret->type = WINDOW_TYPE_CHAT;

    ret->onKey = &chat_onKey;
    ret->onDraw = &chat_onDraw;
    ret->onInit = &chat_onInit;
    ret->onNickRefresh = &chat_onNickRefresh;
    ret->onMessage = &chat_onMessage;
    ret->onConnectionChange = &chat_onConnectionChange;
    ret->onTypingChange = & chat_onTypingChange;
    ret->onConferenceInvite = &chat_onConferenceInvite;
    ret->onNickChange = &chat_onNickChange;
    ret->onStatusChange = &chat_onStatusChange;
    ret->onStatusMessageChange = &chat_onStatusMessageChange;
    ret->onFileChunkRequest = &chat_onFileChunkRequest;
    ret->onFileRecvChunk = &chat_onFileRecvChunk;
    ret->onFileControl = &chat_onFileControl;
    ret->onFileRecv = &chat_onFileRecv;
    ret->onReadReceipt = &chat_onReadReceipt;
    ret->onGroupInvite = &chat_onGroupInvite;

#ifdef AUDIO
    ret->onInvite = &chat_onInvite;
    ret->onRinging = &chat_onRinging;
    ret->onStarting = &chat_onStarting;
    ret->onError = &chat_onError;
    ret->onStart = &chat_onStart;
    ret->onCancel = &chat_onCancel;
    ret->onReject = &chat_onReject;
    ret->onEnd = &chat_onEnd;

    ret->is_call = false;
    ret->ringing_sound = -1;
#endif /* AUDIO */

#ifdef GAMES
    ret->onGameInvite = &chat_onGameInvite;
#endif /* GAMES */

    ret->active_box = -1;

    char name[TOXIC_MAX_NAME_LENGTH + 1];
    const uint16_t n_len = get_friend_name(name, sizeof(name), friendnum);

    set_window_title(ret, name, n_len);

    ChatContext *chatwin = calloc(1, sizeof(ChatContext));
    StatusBar *stb = calloc(1, sizeof(StatusBar));
    Help *help = calloc(1, sizeof(Help));

    if (stb == NULL || chatwin == NULL || help == NULL) {
        exit_toxic_err(FATALERR_MEMORY, "failed in new_chat");
    }

    ret->chatwin = chatwin;
    ret->stb = stb;
    ret->help = help;

    ret->num = friendnum;

    return ret;
}

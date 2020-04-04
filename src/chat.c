/*  chat.c
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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE    /* needed for wcswidth() */
#endif

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>
#include <assert.h>
#include <limits.h>

#include "toxic.h"
#include "windows.h"
#include "execute.h"
#include "misc_tools.h"
#include "file_transfers.h"
#include "friendlist.h"
#include "toxic_strings.h"
#include "log.h"
#include "line_info.h"
#include "settings.h"
#include "input.h"
#include "help.h"
#include "autocomplete.h"
#include "notify.h"
#include "message_queue.h"

#ifdef AUDIO
#include "audio_call.h"
#ifdef VIDEO
#include "video_call.h"
#endif /* VIDEO */
#endif /* AUDIO */

extern char *DATA_FILE;
extern FriendsList Friends;

extern struct Winthread Winthread;
extern struct user_settings *user_settings;

#ifdef AUDIO
static void init_infobox(ToxWindow *self);
static void kill_infobox(ToxWindow *self);
#endif /* AUDIO */

#ifdef AUDIO
#define AC_NUM_CHAT_COMMANDS_AUDIO 9
#else
#define AC_NUM_CHAT_COMMANDS_AUDIO 0
#endif /* AUDIO */
#ifdef PYTHON
#define AC_NUM_CHAT_COMMANDS_PYTHON 1
#else
#define AC_NUM_CHAT_COMMANDS_PYTHON 0
#endif /* PYTHON */
#ifdef QRCODE
#define AC_NUM_CHAT_COMMANDS_QRCODE 1
#else
#define AC_NUM_CHAT_COMMANDS_QRCODE 0
#endif /* QRCODE */
#define AC_NUM_CHAT_COMMANDS (21 + AC_NUM_CHAT_COMMANDS_AUDIO + AC_NUM_CHAT_COMMANDS_PYTHON + AC_NUM_CHAT_COMMANDS_QRCODE)

/* Array of chat command names used for tab completion. */
static const char chat_cmd_list[AC_NUM_CHAT_COMMANDS][MAX_CMDNAME_SIZE] = {
    { "/accept"     },
    { "/add"        },
    { "/avatar"     },
    { "/cancel"     },
    { "/clear"      },
    { "/close"      },
    { "/connect"    },
    { "/exit"       },
    { "/group"      },
    { "/help"       },
    { "/invite"     },
    { "/join"       },
    { "/log"        },
    { "/myid"       },
#ifdef QRCODE
    { "/myqr"       },
#endif /* QRCODE */
    { "/nick"       },
    { "/note"       },
    { "/nospam"     },
    { "/quit"       },
    { "/savefile"   },
    { "/sendfile"   },
    { "/status"     },

#ifdef AUDIO

    { "/call"       },
    { "/answer"     },
    { "/reject"     },
    { "/hangup"     },
    { "/sdev"       },
    { "/mute"       },
    { "/sense"      },
    { "/video"      },
    { "/bitrate"    },

#endif /* AUDIO */

#ifdef PYTHON

    { "/run"        },

#endif /* PYTHON */
};

static void set_self_typingstatus(ToxWindow *self, Tox *m, bool is_typing)
{
    if (user_settings->show_typing_self == SHOW_TYPING_OFF) {
        return;
    }

    ChatContext *ctx = self->chatwin;

    tox_self_set_typing(m, self->num, is_typing, NULL);
    ctx->self_is_typing = is_typing;
}

void kill_chat_window(ToxWindow *self, Tox *m)
{
    ChatContext *ctx = self->chatwin;
    StatusBar *statusbar = self->stb;

#ifdef AUDIO
#ifdef VIDEO
    stop_video_stream(self);
#endif /* VIDEO */
    stop_current_call(self);
#endif /* AUDIO */

    kill_all_file_transfers_friend(m, self->num);
    log_disable(ctx->log);
    line_info_cleanup(ctx->hst);
    cqueue_cleanup(ctx->cqueue);

    delwin(ctx->linewin);
    delwin(ctx->history);
    delwin(statusbar->topline);

    free(ctx->log);
    free(ctx);
    free(self->help);
    free(statusbar);

    disable_chatwin(self->num);
    del_window(self);
}

static void recv_message_helper(ToxWindow *self, const char *msg, const char *nick, const char *timefrmt)
{
    ChatContext *ctx = self->chatwin;

    line_info_add(self, timefrmt, nick, NULL, IN_MSG, 0, 0, "%s", msg);
    write_to_log(msg, nick, ctx->log, false);

    if (self->active_box != -1) {
        box_notify2(self, generic_message, NT_WNDALERT_1 | NT_NOFOCUS | user_settings->bell_on_message,
                    self->active_box, "%s", msg);
    } else {
        box_notify(self, generic_message, NT_WNDALERT_1 | NT_NOFOCUS | user_settings->bell_on_message,
                   &self->active_box, nick, "%s", msg);
    }
}

static void recv_action_helper(ToxWindow *self, const char *action, const char *nick, const char *timefrmt)
{
    ChatContext *ctx = self->chatwin;

    line_info_add(self, timefrmt, nick, NULL, IN_ACTION, 0, 0, "%s", action);
    write_to_log(action, nick, ctx->log, true);

    if (self->active_box != -1) {
        box_notify2(self, generic_message, NT_WNDALERT_1 | NT_NOFOCUS | user_settings->bell_on_message,
                    self->active_box, "* %s %s", nick, action);
    } else {
        box_notify(self, generic_message, NT_WNDALERT_1 | NT_NOFOCUS | user_settings->bell_on_message,
                   &self->active_box, self->name, "* %s %s", nick, action);
    }
}

static void chat_onMessage(ToxWindow *self, Tox *m, uint32_t num, Tox_Message_Type type, const char *msg, size_t len)
{
    UNUSED_VAR(len);

    if (self->num != num) {
        return;
    }

    char nick[TOX_MAX_NAME_LENGTH];
    get_nick_truncate(m, nick, num);

    char timefrmt[TIME_STR_SIZE];
    get_time_str(timefrmt, sizeof(timefrmt));

    if (type == TOX_MESSAGE_TYPE_NORMAL) {
        recv_message_helper(self, msg, nick, timefrmt);
        return;
    }

    if (type == TOX_MESSAGE_TYPE_ACTION) {
        recv_action_helper(self, msg, nick, timefrmt);
        return;
    }
}

static void chat_pause_file_transfers(uint32_t friendnum);
static void chat_resume_file_senders(ToxWindow *self, Tox *m, uint32_t fnum);

static void chat_onConnectionChange(ToxWindow *self, Tox *m, uint32_t num, Tox_Connection connection_status)
{
    if (self->num != num) {
        return;
    }

    StatusBar *statusbar = self->stb;
    ChatContext *ctx = self->chatwin;
    const char *msg;

    char timefrmt[TIME_STR_SIZE];
    get_time_str(timefrmt, sizeof(timefrmt));

    char nick[TOX_MAX_NAME_LENGTH];
    get_nick_truncate(m, nick, num);

    Tox_Connection prev_status = statusbar->connection;
    statusbar->connection = connection_status;

    if (user_settings->show_connection_msg == SHOW_WELCOME_MSG_OFF) {
        return;
    }

    if (prev_status == TOX_CONNECTION_NONE) {
        chat_resume_file_senders(self, m, num);

        msg = "has come online";
        line_info_add(self, timefrmt, nick, NULL, CONNECTION, 0, GREEN, msg);
        write_to_log(msg, nick, ctx->log, true);
    } else if (connection_status == TOX_CONNECTION_NONE) {
        Friends.list[num].is_typing = false;

        if (self->chatwin->self_is_typing) {
            set_self_typingstatus(self, m, 0);
        }

        chat_pause_file_transfers(num);

        msg = "has gone offline";
        line_info_add(self, timefrmt, nick, NULL, DISCONNECTION, 0, RED, msg);
        write_to_log(msg, nick, ctx->log, true);
    }
}

static void chat_onTypingChange(ToxWindow *self, Tox *m, uint32_t num, bool is_typing)
{
    UNUSED_VAR(m);

    if (self->num != num) {
        return;
    }

    Friends.list[num].is_typing = is_typing;
}

static void chat_onNickChange(ToxWindow *self, Tox *m, uint32_t num, const char *nick, size_t length)
{
    UNUSED_VAR(m);

    if (self->num != num) {
        return;
    }

    StatusBar *statusbar = self->stb;

    snprintf(statusbar->nick, sizeof(statusbar->nick), "%s", nick);
    length = strlen(statusbar->nick);
    statusbar->nick_len = length;

    set_window_title(self, statusbar->nick, length);
}

static void chat_onStatusChange(ToxWindow *self, Tox *m, uint32_t num, Tox_User_Status status)
{
    UNUSED_VAR(m);

    if (self->num != num) {
        return;
    }

    StatusBar *statusbar = self->stb;
    statusbar->status = status;
}

static void chat_onStatusMessageChange(ToxWindow *self, uint32_t num, const char *status, size_t length)
{
    UNUSED_VAR(length);

    if (self->num != num) {
        return;
    }

    StatusBar *statusbar = self->stb;

    snprintf(statusbar->statusmsg, sizeof(statusbar->statusmsg), "%s", status);
    statusbar->statusmsg_len = strlen(statusbar->statusmsg);
}

static void chat_onReadReceipt(ToxWindow *self, Tox *m, uint32_t num, uint32_t receipt)
{
    UNUSED_VAR(num);

    cqueue_remove(self, m, receipt);
}

/* Stops active file transfers for this friend. Called when a friend goes offline */
static void chat_pause_file_transfers(uint32_t friendnum)
{
    ToxicFriend *friend = &Friends.list[friendnum];

    size_t i;

    for (i = 0; i < MAX_FILES; ++i) {
        if (friend->file_sender[i].state >= FILE_TRANSFER_STARTED) {
            friend->file_sender[i].state = FILE_TRANSFER_PAUSED;
        }

        if (friend->file_receiver[i].state >= FILE_TRANSFER_STARTED) {
            friend->file_receiver[i].state = FILE_TRANSFER_PAUSED;
        }
    }
}

/* Tries to resume broken file senders. Called when a friend comes online */
static void chat_resume_file_senders(ToxWindow *self, Tox *m, uint32_t friendnum)
{
    size_t i;

    for (i = 0; i < MAX_FILES; ++i) {
        struct FileTransfer *ft = &Friends.list[friendnum].file_sender[i];

        if (ft->state != FILE_TRANSFER_PAUSED) {
            continue;
        }

        Tox_Err_File_Send err;
        ft->filenum = tox_file_send(m, friendnum, TOX_FILE_KIND_DATA, ft->file_size, ft->file_id,
                                    (uint8_t *) ft->file_name, strlen(ft->file_name), &err);

        if (err != TOX_ERR_FILE_SEND_OK) {
            char msg[MAX_STR_SIZE];
            snprintf(msg, sizeof(msg), "File transfer for '%s' failed.", ft->file_name);
            close_file_transfer(self, m, ft, TOX_FILE_CONTROL_CANCEL, msg, notif_error);
            continue;
        }
    }
}

static void chat_onFileChunkRequest(ToxWindow *self, Tox *m, uint32_t friendnum, uint32_t filenum, uint64_t position,
                                    size_t length)
{
    if (friendnum != self->num) {
        return;
    }

    struct FileTransfer *ft = get_file_transfer_struct(friendnum, filenum);

    if (!ft) {
        return;
    }

    if (ft->state != FILE_TRANSFER_STARTED) {
        return;
    }

    char msg[MAX_STR_SIZE];

    if (length == 0) {
        snprintf(msg, sizeof(msg), "File '%s' successfully sent.", ft->file_name);
        print_progress_bar(self, ft->bps, 100.0, ft->line_id);
        close_file_transfer(self, m, ft, -1, msg, transfer_completed);
        return;
    }

    if (ft->file == NULL) {
        snprintf(msg, sizeof(msg), "File transfer for '%s' failed: Null file pointer.", ft->file_name);
        close_file_transfer(self, m, ft, TOX_FILE_CONTROL_CANCEL, msg, notif_error);
        return;
    }

    if (ft->position != position) {
        if (fseek(ft->file, position, SEEK_SET) == -1) {
            snprintf(msg, sizeof(msg), "File transfer for '%s' failed: Seek fail.", ft->file_name);
            close_file_transfer(self, m, ft, TOX_FILE_CONTROL_CANCEL, msg, notif_error);
            return;
        }

        ft->position = position;
    }

    uint8_t send_data[length];
    size_t send_length = fread(send_data, 1, sizeof(send_data), ft->file);

    if (send_length != length) {
        snprintf(msg, sizeof(msg), "File transfer for '%s' failed: Read fail.", ft->file_name);
        close_file_transfer(self, m, ft, TOX_FILE_CONTROL_CANCEL, msg, notif_error);
        return;
    }

    Tox_Err_File_Send_Chunk err;
    tox_file_send_chunk(m, ft->friendnum, ft->filenum, position, send_data, send_length, &err);

    if (err != TOX_ERR_FILE_SEND_CHUNK_OK) {
        fprintf(stderr, "tox_file_send_chunk failed in chat callback (error %d)\n", err);
    }

    ft->position += send_length;
    ft->bps += send_length;
    ft->last_keep_alive = get_unix_time();
}

static void chat_onFileRecvChunk(ToxWindow *self, Tox *m, uint32_t friendnum, uint32_t filenum, uint64_t position,
                                 const char *data, size_t length)
{
    UNUSED_VAR(position);

    if (friendnum != self->num) {
        return;
    }

    struct FileTransfer *ft = get_file_transfer_struct(friendnum, filenum);

    if (!ft) {
        return;
    }

    if (ft->state != FILE_TRANSFER_STARTED) {
        return;
    }

    char msg[MAX_STR_SIZE];

    if (length == 0) {
        snprintf(msg, sizeof(msg), "File '%s' successfully received.", ft->file_name);
        print_progress_bar(self, ft->bps, 100.0, ft->line_id);
        close_file_transfer(self, m, ft, -1, msg, transfer_completed);
        return;
    }

    if (ft->file == NULL) {
        snprintf(msg, sizeof(msg), "File transfer for '%s' failed: Invalid file pointer.", ft->file_name);
        close_file_transfer(self, m, ft, TOX_FILE_CONTROL_CANCEL, msg, notif_error);
        return;
    }

    if (fwrite(data, length, 1, ft->file) != 1) {
        snprintf(msg, sizeof(msg), "File transfer for '%s' failed: Write fail.", ft->file_name);
        close_file_transfer(self, m, ft, TOX_FILE_CONTROL_CANCEL, msg, notif_error);
        return;
    }

    ft->bps += length;
    ft->position += length;
    ft->last_keep_alive = get_unix_time();
}

static void chat_onFileControl(ToxWindow *self, Tox *m, uint32_t friendnum, uint32_t filenum, Tox_File_Control control)
{
    if (friendnum != self->num) {
        return;
    }

    struct FileTransfer *ft = get_file_transfer_struct(friendnum, filenum);

    if (!ft) {
        return;
    }

    char msg[MAX_STR_SIZE];

    switch (control) {
        case TOX_FILE_CONTROL_RESUME: {
            ft->last_keep_alive = get_unix_time();

            /* transfer is accepted */
            if (ft->state == FILE_TRANSFER_PENDING) {
                ft->state = FILE_TRANSFER_STARTED;
                line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "File transfer [%d] for '%s' accepted.",
                              ft->index, ft->file_name);
                char progline[MAX_STR_SIZE];
                init_progress_bar(progline);
                line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "%s", progline);
                sound_notify(self, silent, NT_NOFOCUS | user_settings->bell_on_filetrans_accept | NT_WNDALERT_2, NULL);
                ft->line_id = self->chatwin->hst->line_end->id + 2;
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
            snprintf(msg, sizeof(msg), "File transfer for '%s' was aborted.", ft->file_name);
            close_file_transfer(self, m, ft, -1, msg, notif_error);
            break;
        }
    }
}

/* Attempts to resume a broken inbound file transfer.
 *
 * Returns true if resume is successful.
 */
static bool chat_resume_broken_ft(ToxWindow *self, Tox *m, uint32_t friendnum, uint32_t filenum)
{
    char msg[MAX_STR_SIZE];
    uint8_t file_id[TOX_FILE_ID_LENGTH];

    if (!tox_file_get_file_id(m, friendnum, filenum, file_id, NULL)) {
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
            ft->filenum = filenum;
            ft->state = FILE_TRANSFER_STARTED;
            ft->last_keep_alive = get_unix_time();
            resuming = true;
            break;
        }
    }

    if (!resuming || !ft) {
        return false;
    }

    if (!tox_file_seek(m, ft->friendnum, ft->filenum, ft->position, NULL)) {
        goto on_error;
    }

    if (!tox_file_control(m, ft->friendnum, ft->filenum, TOX_FILE_CONTROL_RESUME, NULL)) {
        goto on_error;
    }

    return true;

on_error:
    snprintf(msg, sizeof(msg), "File transfer for '%s' failed.", ft->file_name);
    close_file_transfer(self, m, ft, TOX_FILE_CONTROL_CANCEL, msg, notif_error);
    return false;
}

static void chat_onFileRecv(ToxWindow *self, Tox *m, uint32_t friendnum, uint32_t filenum, uint64_t file_size,
                            const char *filename, size_t name_length)
{
    if (self->num != friendnum) {
        return;
    }

    /* first check if we need to resume a broken transfer */
    if (chat_resume_broken_ft(self, m, friendnum, filenum)) {
        return;
    }

    struct FileTransfer *ft = new_file_transfer(self, friendnum, filenum, FILE_TRANSFER_RECV, TOX_FILE_KIND_DATA);

    if (!ft) {
        tox_file_control(m, friendnum, filenum, TOX_FILE_CONTROL_CANCEL, NULL);
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "File transfer failed: Too many concurrent file transfers.");
        return;
    }

    char sizestr[32];
    bytes_convert_str(sizestr, sizeof(sizestr), file_size);
    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "File transfer request for '%s' (%s)", filename, sizestr);

    char file_path[PATH_MAX + name_length + 1];
    size_t path_len = name_length;

    /* use specified download path in config if possible */
    if (!string_is_empty(user_settings->download_path)) {
        snprintf(file_path, sizeof(file_path), "%s%s", user_settings->download_path, filename);
        path_len += strlen(user_settings->download_path);
    } else {
        snprintf(file_path, sizeof(file_path), "%s", filename);
    }

    if (path_len >= sizeof(file_path) || path_len >= sizeof(ft->file_path) || name_length >= sizeof(ft->file_name)) {
        tox_file_control(m, friendnum, filenum, TOX_FILE_CONTROL_CANCEL, NULL);
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "File transfer faield: File path too long.");
        return;
    }

    /* Append a number to duplicate file names */
    FILE *filecheck = NULL;
    int count = 1;

    while ((filecheck = fopen(file_path, "r"))) {
        fclose(filecheck);
        file_path[path_len] = '\0';
        char d[9];
        sprintf(d, "(%d)", count);
        ++count;
        size_t d_len = strlen(d);

        if (path_len + d_len >= sizeof(file_path)) {
            path_len -= d_len;
            file_path[path_len] = '\0';
        }

        strcat(file_path, d);
        file_path[path_len + d_len] = '\0';

        if (count > 999) {
            tox_file_control(m, friendnum, filenum, TOX_FILE_CONTROL_CANCEL, NULL);
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "File transfer failed: invalid file path.");
            return;
        }
    }

    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Type '/savefile %d' to accept the file transfer.", ft->index);

    ft->file_size = file_size;
    snprintf(ft->file_path, sizeof(ft->file_path), "%s", file_path);
    snprintf(ft->file_name, sizeof(ft->file_name), "%s", filename);
    tox_file_get_file_id(m, friendnum, filenum, ft->file_id, NULL);

    if (self->active_box != -1) {
        box_notify2(self, transfer_pending, NT_WNDALERT_0 | NT_NOFOCUS | user_settings->bell_on_filetrans,
                    self->active_box, "Incoming file: %s", filename);
    } else {
        box_notify(self, transfer_pending, NT_WNDALERT_0 | NT_NOFOCUS | user_settings->bell_on_filetrans,
                   &self->active_box, self->name, "Incoming file: %s", filename);
    }
}

static void chat_onGroupInvite(ToxWindow *self, Tox *m, int32_t friendnumber, uint8_t type, const char *group_pub_key,
                               uint16_t length)
{
    if (self->num != friendnumber) {
        return;
    }

    if (Friends.list[friendnumber].group_invite.key != NULL) {
        free(Friends.list[friendnumber].group_invite.key);
    }

    char *k = malloc(length);

    if (k == NULL) {
        exit_toxic_err("Failed in chat_onGroupInvite", FATALERR_MEMORY);
    }

    memcpy(k, group_pub_key, length);
    Friends.list[friendnumber].group_invite.key = k;
    Friends.list[friendnumber].group_invite.pending = true;
    Friends.list[friendnumber].group_invite.length = length;
    Friends.list[friendnumber].group_invite.type = type;

    sound_notify(self, generic_message, NT_WNDALERT_2 | user_settings->bell_on_invite, NULL);

    char name[TOX_MAX_NAME_LENGTH];
    get_nick_truncate(m, name, friendnumber);

    if (self->active_box != -1) {
        box_silent_notify2(self, NT_WNDALERT_2 | NT_NOFOCUS, self->active_box, "invites you to join group chat");
    } else {
        box_silent_notify(self, NT_WNDALERT_2 | NT_NOFOCUS, &self->active_box, name, "invites you to join group chat");
    }

    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "%s has invited you to a group chat.", name);
    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Type \"/join\" to join the chat.");
}

/* AV Stuff */
#ifdef AUDIO

void chat_onInvite(ToxWindow *self, ToxAV *av, uint32_t friend_number, int state)
{
    UNUSED_VAR(av);
    UNUSED_VAR(state);

    if (!self || self->num != friend_number) {
        return;
    }

    /* call is flagged active here */
    self->is_call = true;

    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Incoming audio call! Type: \"/answer\" or \"/reject\"");

    if (self->ringing_sound == -1) {
        sound_notify(self, call_incoming, NT_LOOP | user_settings->bell_on_invite, &self->ringing_sound);
    }

    if (self->active_box != -1) {
        box_silent_notify2(self, NT_NOFOCUS | NT_WNDALERT_0, self->active_box, "Incoming audio call!");
    } else {
        box_silent_notify(self, NT_NOFOCUS | NT_WNDALERT_0, &self->active_box, self->name, "Incoming audio call!");
    }
}

void chat_onRinging(ToxWindow *self, ToxAV *av, uint32_t friend_number, int state)
{
    UNUSED_VAR(av);
    UNUSED_VAR(state);

    if (!self || self->num != friend_number) {
        return;
    }

    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Ringing...type \"/hangup\" to cancel it.");

#ifdef SOUND_NOTIFY

    if (self->ringing_sound == -1) {
        sound_notify(self, call_outgoing, NT_LOOP, &self->ringing_sound);
    }

#endif /* SOUND_NOTIFY */
}

void chat_onStarting(ToxWindow *self, ToxAV *av, uint32_t friend_number, int state)
{
    UNUSED_VAR(av);
    UNUSED_VAR(state);

    if (!self || self->num != friend_number) {
        return;
    }

    init_infobox(self);

    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Call started! Type: \"/hangup\" to end it.");

    /* call is flagged active here */
    self->is_call = true;

#ifdef SOUND_NOTIFY
    stop_sound(self->ringing_sound);
#endif /* SOUND_NOTIFY */
}

void chat_onEnding(ToxWindow *self, ToxAV *av, uint32_t friend_number, int state)
{
    UNUSED_VAR(av);
    UNUSED_VAR(state);

    if (!self || self->num != friend_number) {
        return;
    }

    kill_infobox(self);
    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Call ended!");

    self->is_call = false;

#ifdef SOUND_NOTIFY
    stop_sound(self->ringing_sound);
#endif /* SOUND_NOTIFY */
}

void chat_onError(ToxWindow *self, ToxAV *av, uint32_t friend_number, int state)
{
    UNUSED_VAR(av);
    UNUSED_VAR(state);

    if (!self || self->num != friend_number) {
        return;
    }

    self->is_call = false;
    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Error!");

#ifdef SOUND_NOTIFY
    stop_sound(self->ringing_sound);
#endif /* SOUND_NOTIFY */
}

void chat_onStart(ToxWindow *self, ToxAV *av, uint32_t friend_number, int state)
{
    UNUSED_VAR(av);
    UNUSED_VAR(state);

    if (!self || self->num != friend_number) {
        return;
    }

    /* call is flagged active here */
    self->is_call = true;

    init_infobox(self);

    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Call started! Type: \"/hangup\" to end it.");

#ifdef SOUND_NOTIFY
    stop_sound(self->ringing_sound);
#endif /* SOUND_NOTIFY */
}

void chat_onCancel(ToxWindow *self, ToxAV *av, uint32_t friend_number, int state)
{
    UNUSED_VAR(av);
    UNUSED_VAR(state);

    if (!self || self->num != friend_number) {
        return;
    }

    self->is_call = false;
    kill_infobox(self);
    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Call canceled!");

#ifdef SOUND_NOTIFY
    stop_sound(self->ringing_sound);
#endif /* SOUND_NOTIFY */
}

void chat_onReject(ToxWindow *self, ToxAV *av, uint32_t friend_number, int state)
{
    UNUSED_VAR(av);
    UNUSED_VAR(state);

    if (!self  || self->num != friend_number) {
        return;
    }

    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Rejected!");
    self->is_call = false;

#ifdef SOUND_NOTIFY
    stop_sound(self->ringing_sound);
#endif /* SOUND_NOTIFY */
}

void chat_onEnd(ToxWindow *self, ToxAV *av, uint32_t friend_number, int state)
{
    UNUSED_VAR(av);
    UNUSED_VAR(state);

    if (!self || self->num != friend_number) {
        return;
    }

    kill_infobox(self);
    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Call ended!");
    self->is_call = false;

#ifdef SOUND_NOTIFY
    stop_sound(self->ringing_sound);
#endif /* SOUND_NOTIFY */
}

static void init_infobox(ToxWindow *self)
{
    ChatContext *ctx = self->chatwin;

    int x2, y2;
    getmaxyx(self->window, y2, x2);

    if (y2 <= 0 || x2 <= 0) {
        return;
    }

    UNUSED_VAR(y2);

    memset(&ctx->infobox, 0, sizeof(struct infobox));

    ctx->infobox.win = newwin(INFOBOX_HEIGHT, INFOBOX_WIDTH + 1, 1, x2 - INFOBOX_WIDTH);
    ctx->infobox.starttime = get_unix_time();
    ctx->infobox.vad_lvl = user_settings->VAD_treshold;
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
    memset(&ctx->infobox, 0, sizeof(struct infobox));
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

    /* update elapsed time string once per second */
    if (curtime > infobox->lastupdate) {
        get_elapsed_time_str(infobox->timestr, sizeof(infobox->timestr), curtime - infobox->starttime);
    }

    infobox->lastupdate = curtime;

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

    wborder(infobox->win, ACS_VLINE, ' ', ACS_HLINE, ACS_HLINE, ACS_TTEE, ' ', ACS_LLCORNER, ' ');
    wnoutrefresh(infobox->win);
}

#endif /* AUDIO */

static void send_action(ToxWindow *self, ChatContext *ctx, Tox *m, char *action)
{
    if (action == NULL) {
        return;
    }

    char selfname[TOX_MAX_NAME_LENGTH];
    tox_self_get_name(m, (uint8_t *) selfname);

    size_t len = tox_self_get_name_size(m);
    selfname[len] = '\0';

    char timefrmt[TIME_STR_SIZE];
    get_time_str(timefrmt, sizeof(timefrmt));

    int id = line_info_add(self, timefrmt, selfname, NULL, OUT_ACTION, 0, 0, "%s", action);
    cqueue_add(ctx->cqueue, action, strlen(action), OUT_ACTION, id);
}

static void chat_onKey(ToxWindow *self, Tox *m, wint_t key, bool ltr)
{
    ChatContext *ctx = self->chatwin;
    StatusBar *statusbar = self->stb;

    int x, y, y2, x2;
    getyx(self->window, y, x);
    getmaxyx(self->window, y2, x2);

    UNUSED_VAR(y);

    if (y2 <= 0 || x2 <= 0) {
        return;
    }

    if (ctx->pastemode && key == '\r') {
        key = '\n';
    }

    if (self->help->active) {
        help_onKey(self, key);
        return;
    }

    if (ltr || key == '\n') {    /* char is printable */
        input_new_char(self, key, x, x2);

        if (ctx->line[0] != '/' && !ctx->self_is_typing && statusbar->connection != TOX_CONNECTION_NONE) {
            set_self_typingstatus(self, m, 1);
        }

        return;
    }

    if (line_info_onKey(self, key)) {
        return;
    }

    input_handle(self, key, x, x2);

    if (key == '\t' && ctx->len > 1 && ctx->line[0] == '/') {    /* TAB key: auto-complete */
        int diff = -1;

        /* TODO: make this not suck */
        if (wcsncmp(ctx->line, L"/sendfile ", wcslen(L"/sendfile ")) == 0) {
            diff = dir_match(self, m, ctx->line, L"/sendfile");
        } else if (wcsncmp(ctx->line, L"/avatar ", wcslen(L"/avatar ")) == 0) {
            diff = dir_match(self, m, ctx->line, L"/avatar");
        }

#ifdef PYTHON
        else if (wcsncmp(ctx->line, L"/run ", wcslen(L"/run ")) == 0) {
            diff = dir_match(self, m, ctx->line, L"/run");
        }

#endif

        else if (wcsncmp(ctx->line, L"/status ", wcslen(L"/status ")) == 0) {
            const char status_cmd_list[3][8] = {
                {"online"},
                {"away"},
                {"busy"},
            };
            diff = complete_line(self, status_cmd_list, 3, 8);
        } else {
            diff = complete_line(self, chat_cmd_list, AC_NUM_CHAT_COMMANDS, MAX_CMDNAME_SIZE);
        }

        if (diff != -1) {
            if (x + diff > x2 - 1) {
                int wlen = MAX(0, wcswidth(ctx->line, sizeof(ctx->line) / sizeof(wchar_t)));
                ctx->start = wlen < x2 ? 0 : wlen - x2 + 1;
            }
        } else {
            sound_notify(self, notif_error, 0, NULL);
        }

    } else if (key == '\r') {
        rm_trailing_spaces_buf(ctx);

        if (!wstring_is_empty(ctx->line)) {
            add_line_to_hist(ctx);

            wstrsubst(ctx->line, L'¶', L'\n');

            char line[MAX_STR_SIZE] = {0};

            if (wcs_to_mbs_buf(line, ctx->line, MAX_STR_SIZE) == -1) {
                memset(&line, 0, sizeof(line));
            }

            if (line[0] == '/') {
                if (strcmp(line, "/close") == 0) {
                    kill_chat_window(self, m);
                    return;
                } else if (strncmp(line, "/me ", strlen("/me ")) == 0) {
                    send_action(self, ctx, m, line + strlen("/me "));
                } else {
                    execute(ctx->history, self, m, line, CHAT_COMMAND_MODE);
                }
            } else {
                char selfname[TOX_MAX_NAME_LENGTH];
                tox_self_get_name(m, (uint8_t *) selfname);

                size_t len = tox_self_get_name_size(m);
                selfname[len] = '\0';

                char timefrmt[TIME_STR_SIZE];
                get_time_str(timefrmt, sizeof(timefrmt));

                int id = line_info_add(self, timefrmt, selfname, NULL, OUT_MSG, 0, 0, "%s", line);
                cqueue_add(ctx->cqueue, line, strlen(line), OUT_MSG, id);
            }
        }

        wclear(ctx->linewin);
        wmove(self->window, y2 - CURS_Y_OFFSET, 0);
        reset_buf(ctx);
    }

    if (ctx->len <= 0 && ctx->self_is_typing) {
        set_self_typingstatus(self, m, 0);
    }
}

static void chat_onDraw(ToxWindow *self, Tox *m)
{
    int x2, y2;
    getmaxyx(self->window, y2, x2);

    if (y2 <= 0 || x2 <= 0) {
        return;
    }

    ChatContext *ctx = self->chatwin;

    pthread_mutex_lock(&Winthread.lock);
    line_info_print(self);
    pthread_mutex_unlock(&Winthread.lock);

    wclear(ctx->linewin);

    curs_set(1);

    if (ctx->len > 0) {
        mvwprintw(ctx->linewin, 1, 0, "%ls", &ctx->line[ctx->start]);
    }

    /* Draw status bar */
    StatusBar *statusbar = self->stb;
    mvwhline(statusbar->topline, 1, 0, ACS_HLINE, x2);
    wmove(statusbar->topline, 0, 0);

    /* Draw name, status and note in statusbar */
    if (statusbar->connection != TOX_CONNECTION_NONE) {
        int colour = MAGENTA;
        Tox_User_Status status = statusbar->status;

        switch (status) {
            case TOX_USER_STATUS_NONE:
                colour = GREEN;
                break;

            case TOX_USER_STATUS_AWAY:
                colour = YELLOW;
                break;

            case TOX_USER_STATUS_BUSY:
                colour = RED;
                break;
        }

        wattron(statusbar->topline, COLOR_PAIR(colour) | A_BOLD);
        wprintw(statusbar->topline, " %s", ONLINE_CHAR);
        wattroff(statusbar->topline, COLOR_PAIR(colour) | A_BOLD);

        if (Friends.list[self->num].is_typing) {
            wattron(statusbar->topline, COLOR_PAIR(YELLOW));
        }

        wattron(statusbar->topline, A_BOLD);
        wprintw(statusbar->topline, " %s ", statusbar->nick);
        wattroff(statusbar->topline, A_BOLD);

        if (Friends.list[self->num].is_typing) {
            wattroff(statusbar->topline, COLOR_PAIR(YELLOW));
        }
    } else {
        wprintw(statusbar->topline, " %s", OFFLINE_CHAR);
        wattron(statusbar->topline, A_BOLD);
        wprintw(statusbar->topline, " %s ", statusbar->nick);
        wattroff(statusbar->topline, A_BOLD);
    }

    /* Reset statusbar->statusmsg on window resize */
    if (x2 != self->x) {
        char statusmsg[TOX_MAX_STATUS_MESSAGE_LENGTH] = {'\0'};

        pthread_mutex_lock(&Winthread.lock);
        tox_friend_get_status_message(m, self->num, (uint8_t *) statusmsg, NULL);
        size_t s_len = tox_friend_get_status_message_size(m, self->num, NULL);
        pthread_mutex_unlock(&Winthread.lock);

        filter_str(statusmsg, s_len);
        snprintf(statusbar->statusmsg, sizeof(statusbar->statusmsg), "%s", statusmsg);
        statusbar->statusmsg_len = strlen(statusbar->statusmsg);
    }

    self->x = x2;

    /* Truncate note if it doesn't fit in statusbar */
    size_t maxlen = x2 - getcurx(statusbar->topline) - (KEY_IDENT_DIGITS * 2) - 6;

    if (statusbar->statusmsg_len > maxlen) {
        statusbar->statusmsg[maxlen - 3] = '\0';
        strcat(statusbar->statusmsg, "...");
        statusbar->statusmsg_len = maxlen;
    }

    if (statusbar->statusmsg[0]) {
        wprintw(statusbar->topline, ": %s ", statusbar->statusmsg);
    }

    wclrtoeol(statusbar->topline);
    wmove(statusbar->topline, 0, x2 - (KEY_IDENT_DIGITS * 2) - 3);
    wprintw(statusbar->topline, "{");

    size_t i;

    for (i = 0; i < KEY_IDENT_DIGITS; ++i) {
        wprintw(statusbar->topline, "%02X", Friends.list[self->num].pub_key[i] & 0xff);
    }

    wprintw(statusbar->topline, "}\n");

    mvwhline(self->window, y2 - CHATBOX_HEIGHT, 0, ACS_HLINE, x2);

    int y, x;
    getyx(self->window, y, x);

    UNUSED_VAR(x);

    int new_x = ctx->start ? x2 - 1 : MAX(0, wcswidth(ctx->line, ctx->pos));
    wmove(self->window, y + 1, new_x);

    wnoutrefresh(self->window);

#ifdef AUDIO

    if (ctx->infobox.active) {
        draw_infobox(self);
    }

#endif

    if (self->help->active) {
        help_onDraw(self);
    }

    pthread_mutex_lock(&Winthread.lock);
    refresh_file_transfer_progress(self, self->num);
    pthread_mutex_unlock(&Winthread.lock);
}

static void chat_onInit(ToxWindow *self, Tox *m)
{
    curs_set(1);
    int x2, y2;
    getmaxyx(self->window, y2, x2);

    if (y2 <= 0 || x2 <= 0) {
        exit_toxic_err("failed in chat_onInit", FATALERR_CURSES);
    }

    self->x = x2;

    /* Init statusbar info */
    StatusBar *statusbar = self->stb;
    statusbar->status = get_friend_status(self->num);
    statusbar->connection = get_friend_connection_status(self->num);

    char statusmsg[TOX_MAX_STATUS_MESSAGE_LENGTH];
    tox_friend_get_status_message(m, self->num, (uint8_t *) statusmsg, NULL);

    size_t s_len = tox_friend_get_status_message_size(m, self->num, NULL);
    statusmsg[s_len] = '\0';

    filter_str(statusmsg, s_len);
    snprintf(statusbar->statusmsg, sizeof(statusbar->statusmsg), "%s", statusmsg);
    statusbar->statusmsg_len = strlen(statusbar->statusmsg);

    char nick[TOX_MAX_NAME_LENGTH + 1];
    size_t n_len = get_nick_truncate(m, nick, self->num);
    snprintf(statusbar->nick, sizeof(statusbar->nick), "%s", nick);
    statusbar->nick_len = n_len;

    /* Init subwindows */
    ChatContext *ctx = self->chatwin;

    statusbar->topline = subwin(self->window, 2, x2, 0, 0);
    ctx->history = subwin(self->window, y2 - CHATBOX_HEIGHT + 1, x2, 0, 0);
    ctx->linewin = subwin(self->window, CHATBOX_HEIGHT, x2, y2 - CHATBOX_HEIGHT, 0);

    ctx->hst = calloc(1, sizeof(struct history));
    ctx->log = calloc(1, sizeof(struct chatlog));
    ctx->cqueue = calloc(1, sizeof(struct chat_queue));

    if (ctx->log == NULL || ctx->hst == NULL || ctx->cqueue == NULL) {
        exit_toxic_err("failed in chat_onInit", FATALERR_MEMORY);
    }

    line_info_init(ctx->hst);

    char myid[TOX_ADDRESS_SIZE];
    tox_self_get_address(m, (uint8_t *) myid);

    int log_ret = log_enable(nick, myid, Friends.list[self->num].pub_key, ctx->log, LOG_CHAT);
    load_chat_history(self, ctx->log);

    if (!Friends.list[self->num].logging_on) {
        log_disable(ctx->log);
    } else if (log_ret == -1) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Warning: Log failed to initialize.");
    }

    execute(ctx->history, self, m, "/log", GLOBAL_COMMAND_MODE);

    scrollok(ctx->history, 0);
    wmove(self->window, y2 - CURS_Y_OFFSET, 0);
}

ToxWindow *new_chat(Tox *m, uint32_t friendnum)
{
    ToxWindow *ret = calloc(1, sizeof(ToxWindow));

    if (ret == NULL) {
        exit_toxic_err("failed in new_chat", FATALERR_MEMORY);
    }

    ret->is_chat = true;

    ret->onKey = &chat_onKey;
    ret->onDraw = &chat_onDraw;
    ret->onInit = &chat_onInit;
    ret->onMessage = &chat_onMessage;
    ret->onConnectionChange = &chat_onConnectionChange;
    ret->onTypingChange = & chat_onTypingChange;
    ret->onGroupInvite = &chat_onGroupInvite;
    ret->onNickChange = &chat_onNickChange;
    ret->onStatusChange = &chat_onStatusChange;
    ret->onStatusMessageChange = &chat_onStatusMessageChange;
    ret->onFileChunkRequest = &chat_onFileChunkRequest;
    ret->onFileRecvChunk = &chat_onFileRecvChunk;
    ret->onFileControl = &chat_onFileControl;
    ret->onFileRecv = &chat_onFileRecv;
    ret->onReadReceipt = &chat_onReadReceipt;

#ifdef AUDIO
    ret->onInvite = &chat_onInvite;
    ret->onRinging = &chat_onRinging;
    ret->onStarting = &chat_onStarting;
    ret->onEnding = &chat_onEnding;
    ret->onError = &chat_onError;
    ret->onStart = &chat_onStart;
    ret->onCancel = &chat_onCancel;
    ret->onReject = &chat_onReject;
    ret->onEnd = &chat_onEnd;

    ret->is_call = false;
    ret->ringing_sound = -1;
#endif /* AUDIO */

    ret->active_box = -1;

    char nick[TOX_MAX_NAME_LENGTH];
    size_t n_len = get_nick_truncate(m, nick, friendnum);
    set_window_title(ret, nick, n_len);

    ChatContext *chatwin = calloc(1, sizeof(ChatContext));
    StatusBar *stb = calloc(1, sizeof(StatusBar));
    Help *help = calloc(1, sizeof(Help));

    if (stb == NULL || chatwin == NULL || help == NULL) {
        exit_toxic_err("failed in new_chat", FATALERR_MEMORY);
    }

    ret->chatwin = chatwin;
    ret->stb = stb;
    ret->help = help;

    ret->num = friendnum;

    return ret;
}

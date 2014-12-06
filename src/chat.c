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

#include "toxic.h"
#include "windows.h"
#include "execute.h"
#include "misc_tools.h"
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
#endif /* AUDIO */


extern char *DATA_FILE;

extern FileSender file_senders[MAX_FILES];
extern FriendsList Friends;

extern struct Winthread Winthread;
extern struct user_settings *user_settings;

#ifdef AUDIO
static void init_infobox(ToxWindow *self);
static void kill_infobox(ToxWindow *self);
#endif  /* AUDIO */

#ifdef AUDIO
#define AC_NUM_CHAT_COMMANDS 27
#else
#define AC_NUM_CHAT_COMMANDS 20
#endif /* AUDIO */

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
    { "/nick"       },
    { "/note"       },
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

#endif /* AUDIO */
};

static void set_self_typingstatus(ToxWindow *self, Tox *m, uint8_t is_typing)
{
    if (user_settings->show_typing_self == SHOW_TYPING_OFF)
        return;

    ChatContext *ctx = self->chatwin;

    tox_set_user_is_typing(m, self->num, is_typing);
    ctx->self_is_typing = is_typing;
}

static void close_all_file_receivers(Tox *m, int friendnum);

void kill_chat_window(ToxWindow *self, Tox *m)
{
    ChatContext *ctx = self->chatwin;
    StatusBar *statusbar = self->stb;

    close_all_file_receivers(m, self->num);
    log_disable(ctx->log);
    line_info_cleanup(ctx->hst);
    cqueue_cleanup(ctx->cqueue);

#ifdef AUDIO
    stop_current_call(self);
#endif

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

static void chat_onMessage(ToxWindow *self, Tox *m, int32_t num, const char *msg, uint16_t len)
{
    if (self->num != num)
        return;

    ChatContext *ctx = self->chatwin;

    char nick[TOX_MAX_NAME_LENGTH];
    get_nick_truncate(m, nick, num);

    char timefrmt[TIME_STR_SIZE];
    get_time_str(timefrmt, sizeof(timefrmt));

    line_info_add(self, timefrmt, nick, NULL, IN_MSG, 0, 0, "%s", msg);
    write_to_log(msg, nick, ctx->log, false);
    
    if (self->active_box != -1) 
        box_notify2(self, generic_message, NT_WNDALERT_1 | NT_NOFOCUS, self->active_box, "%s", msg);    
    else 
        box_notify(self, generic_message, NT_WNDALERT_1 | NT_NOFOCUS, &self->active_box, nick, "%s", msg);
    
}

static void chat_resume_file_transfers(Tox *m, int fnum);
static void chat_stop_file_senders(int fnum);

static void chat_onConnectionChange(ToxWindow *self, Tox *m, int32_t num, uint8_t status)
{
    if (self->num != num)
        return;

    StatusBar *statusbar = self->stb;
    ChatContext *ctx = self->chatwin;
    const char *msg;

    char timefrmt[TIME_STR_SIZE];
    get_time_str(timefrmt, sizeof(timefrmt));

    char nick[TOX_MAX_NAME_LENGTH];
    get_nick_truncate(m, nick, num);

    if (status == 1) { /* Friend goes online */
        statusbar->is_online = true;
        Friends.list[num].is_typing = user_settings->show_typing_other == SHOW_TYPING_ON 
                                      ? tox_get_is_typing(m, num) : 0;
        chat_resume_file_transfers(m, num);

        msg = "has come online";
        line_info_add(self, timefrmt, nick, NULL, CONNECTION, 0, GREEN, msg);
        write_to_log(msg, nick, ctx->log, true);
    } else { /* Friend goes offline */
        statusbar->is_online = false;
        Friends.list[num].is_typing = 0;

        if (self->chatwin->self_is_typing)
            set_self_typingstatus(self, m, 0);

        chat_stop_file_senders(num);

        msg = "has gone offline";
        line_info_add(self, timefrmt, nick, NULL, CONNECTION, 0, RED, msg);
        write_to_log(msg, nick, ctx->log, true);
    }
}

static void chat_onTypingChange(ToxWindow *self, Tox *m, int32_t num, uint8_t is_typing)
{
    if (self->num != num)
        return;

    Friends.list[num].is_typing = is_typing;
}

static void chat_onAction(ToxWindow *self, Tox *m, int32_t num, const char *action, uint16_t len)
{
    if (self->num != num)
        return;

    ChatContext *ctx = self->chatwin;

    char nick[TOX_MAX_NAME_LENGTH];
    get_nick_truncate(m, nick, num);

    char timefrmt[TIME_STR_SIZE];
    get_time_str(timefrmt, sizeof(timefrmt));

    line_info_add(self, timefrmt, nick, NULL, IN_ACTION, 0, 0, "%s", action);
    write_to_log(action, nick, ctx->log, true);
    
    if (self->active_box != -1)
        box_notify2(self, generic_message, NT_WNDALERT_1 | NT_NOFOCUS, self->active_box, "* %s %s", nick, action );
    else
        box_notify(self, generic_message, NT_WNDALERT_1 | NT_NOFOCUS, &self->active_box, self->name, "* %s %s", nick, action );
}

static void chat_onNickChange(ToxWindow *self, Tox *m, int32_t num, const char *nick, uint16_t len)
{
    if (self->num != num)
        return;

    StatusBar *statusbar = self->stb;

    snprintf(statusbar->nick, sizeof(statusbar->nick), "%s", nick);
    len = strlen(statusbar->nick);
    statusbar->nick_len = len;

    set_window_title(self, statusbar->nick, len);
}

static void chat_onStatusChange(ToxWindow *self, Tox *m, int32_t num, uint8_t status)
{
    if (self->num != num)
        return;

    StatusBar *statusbar = self->stb;
    statusbar->status = status;
}

static void chat_onStatusMessageChange(ToxWindow *self, int32_t num, const char *status, uint16_t len)
{
    if (self->num != num)
        return;

    StatusBar *statusbar = self->stb;

    snprintf(statusbar->statusmsg, sizeof(statusbar->statusmsg), "%s", status);
    statusbar->statusmsg_len = strlen(statusbar->statusmsg);
}

static void chat_onReadReceipt(ToxWindow *self, Tox *m, int32_t num, uint32_t receipt)
{
    cqueue_remove(self, m, receipt);
}

static void chat_onFileSendRequest(ToxWindow *self, Tox *m, int32_t num, uint8_t filenum,
                                   uint64_t filesize, const char *pathname, uint16_t path_len)
{
    if (self->num != num)
        return;

    /* holds the filename appended to the user specified path */
    char filename_path[MAX_STR_SIZE] = {0};

    /* holds the lone filename */
    char filename_nopath[MAX_STR_SIZE];
    get_file_name(filename_nopath, sizeof(filename_nopath), pathname);
    char sizestr[32];
    bytes_convert_str(sizestr, sizeof(sizestr), filesize);
    int len = strlen(filename_nopath);
    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "File transfer request for '%s' (%s)",
                                                          filename_nopath, sizestr);

    if (filenum >= MAX_FILES) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Too many pending file requests; discarding.");
        return;
    }

    /* use specified path in config if possible */
    if (user_settings->download_path[0]) {
        snprintf(filename_path, sizeof(filename_path), "%s%s", user_settings->download_path, filename_nopath);
        len += strlen(user_settings->download_path);
    }

    if (len >= sizeof(Friends.list[num].file_receiver[filenum].filename)) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "File name too long; discarding.");
        return;
    }

    char filename[MAX_STR_SIZE];

    if (filename_path[0])
        strcpy(filename, filename_path);
    else
        strcpy(filename, filename_nopath);

    /* Append a number to duplicate file names */
    FILE *filecheck = NULL;
    int count = 1;

    while ((filecheck = fopen(filename, "r"))) {
        filename[len] = '\0';
        char d[9];
        sprintf(d, "(%d)", count++);
        int d_len = strlen(d);

        if (len + d_len >= sizeof(filename)) {
            len -= d_len;
            filename[len] = '\0';
        }

        strcat(filename, d);
        filename[len + d_len] = '\0';

        if (count > 999) {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Error saving file to disk.");
            return;
        }
    }

    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Type '/savefile %d' to accept the file transfer.", filenum);

    Friends.list[num].file_receiver[filenum].pending = true;
    Friends.list[num].file_receiver[filenum].size = filesize;
    Friends.list[num].file_receiver[filenum].filenum = filenum;
    strcpy(Friends.list[num].file_receiver[filenum].filename, filename);

    if (self->active_box != -1)
        box_notify2(self, transfer_pending, NT_WNDALERT_0 | NT_NOFOCUS, self->active_box, 
                    "Incoming file: %s", filename );
    else
        box_notify(self, transfer_pending, NT_WNDALERT_0 | NT_NOFOCUS, &self->active_box, self->name, 
                    "Incoming file: %s", filename );
}

/* Stops active file senders for this friend. Call when a friend goes offline */
static void chat_stop_file_senders(int fnum)
{
    int i;

    for (i = 0; i < MAX_FILES; ++i) {
        if (file_senders[i].active && file_senders[i].friendnum == fnum)
            file_senders[i].noconnection = true;
    }
}

/* Tries to resume broken file transfers. Call when a friend comes online */
static void chat_resume_file_transfers(Tox *m, int fnum)
{
    if (Friends.list[fnum].active_file_receivers == 0)
        return;

    int i;

    for (i = 0; i < MAX_FILES; ++i) {
        if (Friends.list[fnum].file_receiver[i].active) {
            uint8_t bytes_recv[sizeof(uint64_t)];
            memcpy(bytes_recv, &Friends.list[fnum].file_receiver[i].bytes_recv, sizeof(uint64_t));
            net_to_host(bytes_recv, sizeof(uint64_t));
            int filenum = Friends.list[fnum].file_receiver[i].filenum;
            tox_file_send_control(m, fnum, 1, filenum, TOX_FILECONTROL_RESUME_BROKEN, bytes_recv, sizeof(uint64_t));
        }
    }
}

/* set CTRL to -1 if we don't want to send a control signal.
   set msg to NULL if we don't want to display a message */
void chat_close_file_receiver(Tox *m, int filenum, int friendnum, int CTRL)
{
    if (CTRL > 0)
        tox_file_send_control(m, friendnum, 1, filenum, CTRL, 0, 0);

    FILE *file = Friends.list[friendnum].file_receiver[filenum].file;

    if (file != NULL)
        fclose(file);

    memset(&Friends.list[friendnum].file_receiver[filenum], 0, sizeof(struct FileReceiver));
    --Friends.list[friendnum].active_file_receivers;
}

static void close_all_file_receivers(Tox *m, int friendnum)
{
    int i;

    for (i = 0; i < MAX_FILES; ++i) {
        if (Friends.list[friendnum].file_receiver[i].active)
            chat_close_file_receiver(m, i, friendnum, TOX_FILECONTROL_KILL);
    }
}

static void chat_onFileControl(ToxWindow *self, Tox *m, int32_t num, uint8_t receive_send,
                               uint8_t filenum, uint8_t control_type, const char *data, uint16_t length)
{
    if (self->num != num)
        return;

    const char *filename;
    char msg[MAX_STR_SIZE] = {0};
    int send_idx = 0;   /* file sender index */

    if (receive_send == 0) {
        if (!Friends.list[num].file_receiver[filenum].active)
            return;

        filename = Friends.list[num].file_receiver[filenum].filename;
    } else {
        int i;

        for (i = 0; i < MAX_FILES; ++i) {
            send_idx = i;

            if (file_senders[i].active && file_senders[i].filenum == filenum)
                break;
        }

        if (!file_senders[send_idx].active)
            return;

        filename = file_senders[send_idx].filename;
    }

    switch (control_type) {
        case TOX_FILECONTROL_ACCEPT:
            if (receive_send != 1)
                break;

            /* transfer is accepted */
            if (!file_senders[send_idx].started) {
                file_senders[send_idx].started = true;
                line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "File transfer [%d] for '%s' accepted.",
                                                                      filenum, filename);
                char progline[MAX_STR_SIZE];
                prep_prog_line(progline);
                line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "%s", progline);
                file_senders[send_idx].line_id = self->chatwin->hst->line_end->id + 2;
                sound_notify(self, silent, NT_NOFOCUS | NT_BEEP | NT_WNDALERT_2, NULL);
            } else {   /* active transfer is unpaused by receiver */
                file_senders[send_idx].paused = false;
                file_senders[send_idx].timestamp = get_unix_time();
            }

            break;

        case TOX_FILECONTROL_PAUSE:
            if (receive_send == 1)
                file_senders[send_idx].paused = true;

            break;

        case TOX_FILECONTROL_KILL:
            snprintf(msg, sizeof(msg), "File transfer for '%s' failed.", filename);

            if (self->active_box != -1)
                box_notify2(self, error, NT_NOFOCUS | NT_WNDALERT_2, 
                            self->active_box, "File transfer for '%s' failed!", filename );
            else
                box_notify(self, error, NT_NOFOCUS | NT_WNDALERT_2, &self->active_box,
                           self->name, "File transfer for '%s' failed!", filename );

            if (receive_send == 0)
                chat_close_file_receiver(m, filenum, num, -1);
            else
                close_file_sender(self, m, send_idx, NULL, -1, filenum, num);

            break;

        case TOX_FILECONTROL_FINISHED:
            if (receive_send == 0) {
                print_progress_bar(self, filenum, num, 100.0);

                char filename_nopath[MAX_STR_SIZE];
                get_file_name(filename_nopath, sizeof(filename_nopath), filename);
                snprintf(msg, sizeof(msg), "File transfer for '%s' complete.", filename_nopath);
                chat_close_file_receiver(m, filenum, num, TOX_FILECONTROL_FINISHED);
            } else {
                snprintf(msg, sizeof(msg), "File '%s' successfuly sent.", filename);
                close_file_sender(self, m, send_idx, NULL, TOX_FILECONTROL_FINISHED, filenum, num);
            }

            if (self->active_box != -1)
                box_notify2(self, transfer_completed, NT_NOFOCUS | NT_WNDALERT_2, self->active_box, "%s", msg);
            else
                box_notify(self, transfer_completed, NT_NOFOCUS | NT_WNDALERT_2, &self->active_box, 
                            self->name, "%s", msg);

            break;

        case TOX_FILECONTROL_RESUME_BROKEN:
            if (receive_send == 0)
                break;

            FILE *fp = file_senders[send_idx].file;

            if (fp == NULL)
                break;

            uint8_t tmp[sizeof(uint64_t)];
            memcpy(tmp, &data, sizeof(uint64_t));
            net_to_host(tmp, sizeof(uint64_t));
            uint64_t datapos;
            memcpy(&datapos, tmp, sizeof(uint64_t));

            if (fseek(fp, datapos, SEEK_SET) == -1) {
                snprintf(msg, sizeof(msg), "File transfer for '%s' failed to resume", filename);
                close_file_sender(self, m, send_idx, NULL, TOX_FILECONTROL_FINISHED, filenum, num);
                break;
            }

            tox_file_send_control(m, num, 0, filenum, TOX_FILECONTROL_ACCEPT, 0, 0);
            file_senders[send_idx].noconnection = false;
            break;
    }

    if (msg[0])
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "%s", msg);
}

static void chat_onFileData(ToxWindow *self, Tox *m, int32_t num, uint8_t filenum, const char *data,
                            uint16_t length)
{
    if (self->num != num)
        return;

    FILE *fp = Friends.list[num].file_receiver[filenum].file;

    if (fp) {
        if (fwrite(data, length, 1, fp) != 1) {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, RED, " * Error writing to file.");
            chat_close_file_receiver(m, filenum, num, TOX_FILECONTROL_KILL);
        }
    }

    Friends.list[num].file_receiver[filenum].bps += length;
    Friends.list[num].file_receiver[filenum].bytes_recv += length;
}

static void chat_onGroupInvite(ToxWindow *self, Tox *m, int32_t friendnumber, uint8_t type, const char *group_pub_key,
                               uint16_t length)
{
    if (self->num != friendnumber)
        return;

    if (Friends.list[friendnumber].group_invite.key != NULL)
        free(Friends.list[friendnumber].group_invite.key);

    char *k = malloc(length);

    if (k == NULL)
        exit_toxic_err("Failed in chat_onGroupInvite", FATALERR_MEMORY);

    memcpy(k, group_pub_key, length);
    Friends.list[friendnumber].group_invite.key = k;
    Friends.list[friendnumber].group_invite.pending = true;
    Friends.list[friendnumber].group_invite.length = length;
    Friends.list[friendnumber].group_invite.type = type;

    sound_notify(self, generic_message, NT_WNDALERT_2, NULL);

    char name[TOX_MAX_NAME_LENGTH];
    get_nick_truncate(m, name, friendnumber);

    if (self->active_box != -1)
        box_silent_notify2(self, NT_WNDALERT_2 | NT_NOFOCUS, self->active_box, "invites you to join group chat");
    else
        box_silent_notify(self, NT_WNDALERT_2 | NT_NOFOCUS, &self->active_box, name, "invites you to join group chat");

    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "%s has invited you to a group chat.", name);
    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Type \"/join\" to join the chat.");
}

/* Av Stuff */
#ifdef AUDIO

void chat_onInvite (ToxWindow *self, ToxAv *av, int call_index)
{
    if (!self || self->num != toxav_get_peer_id(av, call_index, 0))
        return;

    /* call_index is set here and reset on call end */
    
    self->call_idx = call_index;
    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Incoming audio call! Type: \"/answer\" or \"/reject\"");
    
    if (self->ringing_sound == -1)
        sound_notify(self, call_incoming, NT_LOOP, &self->ringing_sound);
    
    
    if (self->active_box != -1)
        box_silent_notify2(self, NT_NOFOCUS | NT_WNDALERT_0, self->active_box, "Incoming audio call!");
    else
        box_silent_notify(self, NT_NOFOCUS | NT_WNDALERT_0, &self->active_box, self->name, "Incoming audio call!");
}

void chat_onRinging (ToxWindow *self, ToxAv *av, int call_index)
{
    if ( !self || self->call_idx != call_index || self->num != toxav_get_peer_id(av, call_index, 0))
        return;

    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Ringing...type \"/hangup\" to cancel it.");
    
#ifdef SOUND_NOTIFY
    if (self->ringing_sound == -1)
        sound_notify(self, call_outgoing, NT_LOOP, &self->ringing_sound);
#endif /* SOUND_NOTIFY */
}

void chat_onStarting (ToxWindow *self, ToxAv *av, int call_index)
{
    if ( !self || self->call_idx != call_index || self->num != toxav_get_peer_id(av, call_index, 0))
        return;

    init_infobox(self);

    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Call started! Type: \"/hangup\" to end it.");
    
#ifdef SOUND_NOTIFY
    stop_sound(self->ringing_sound);
#endif /* SOUND_NOTIFY */
}

void chat_onEnding (ToxWindow *self, ToxAv *av, int call_index)
{
    if (!self || self->call_idx != call_index || self->num != toxav_get_peer_id(av, call_index, 0))
        return;

    kill_infobox(self);
    self->call_idx = -1;
    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Call ended!");
    
#ifdef SOUND_NOTIFY
    stop_sound(self->ringing_sound);
#endif /* SOUND_NOTIFY */
}

void chat_onError (ToxWindow *self, ToxAv *av, int call_index)
{
    if (!self || self->call_idx != call_index || self->num != toxav_get_peer_id(av, call_index, 0))
        return;

    self->call_idx = -1;
    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Error!");
    
#ifdef SOUND_NOTIFY
    stop_sound(self->ringing_sound);
#endif /* SOUND_NOTIFY */
}

void chat_onStart (ToxWindow *self, ToxAv *av, int call_index)
{
    if ( !self || self->call_idx != call_index || self->num != toxav_get_peer_id(av, call_index, 0))
        return;

    init_infobox(self);

    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Call started! Type: \"/hangup\" to end it.");
    
#ifdef SOUND_NOTIFY
    stop_sound(self->ringing_sound);
#endif /* SOUND_NOTIFY */
}

void chat_onCancel (ToxWindow *self, ToxAv *av, int call_index)
{
    if ( !self || self->call_idx != call_index || self->num != toxav_get_peer_id(av, call_index, 0))
        return;

    kill_infobox(self);
    self->call_idx = -1;
    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Call canceled!");
    
#ifdef SOUND_NOTIFY
    stop_sound(self->ringing_sound);
#endif /* SOUND_NOTIFY */
}

void chat_onReject (ToxWindow *self, ToxAv *av, int call_index)
{
    if (!self || self->call_idx != call_index || self->num != toxav_get_peer_id(av, call_index, 0))
        return;

    self->call_idx = -1;
    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Rejected!");
    
#ifdef SOUND_NOTIFY
    stop_sound(self->ringing_sound);
#endif /* SOUND_NOTIFY */
}

void chat_onEnd (ToxWindow *self, ToxAv *av, int call_index)
{
    if (!self || self->call_idx != call_index || self->num != toxav_get_peer_id(av, call_index, 0))
        return;

    kill_infobox(self);
    self->call_idx = -1;
    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Call ended!");
    
#ifdef SOUND_NOTIFY
    stop_sound(self->ringing_sound);
#endif /* SOUND_NOTIFY */
}

void chat_onRequestTimeout (ToxWindow *self, ToxAv *av, int call_index)
{
    if (!self || self->call_idx != call_index || self->num != toxav_get_peer_id(av, call_index, 0))
        return;

    self->call_idx = -1;
    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "No answer!");
    
#ifdef SOUND_NOTIFY
    stop_sound(self->ringing_sound);
#endif /* SOUND_NOTIFY */
}

void chat_onPeerTimeout (ToxWindow *self, ToxAv *av, int call_index)
{
    if (!self || self->call_idx != call_index || self->num != toxav_get_peer_id(av, call_index, 0))
        return;

    kill_infobox(self);
    self->call_idx = -1;
    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Peer disconnected; call ended!");
    
#ifdef SOUND_NOTIFY
    stop_sound(self->ringing_sound);
#endif /* SOUND_NOTIFY */
}

static void init_infobox(ToxWindow *self)
{
    ChatContext *ctx = self->chatwin;

    int x2, y2;
    getmaxyx(self->window, y2, x2);
    (void) y2;

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

    if (!ctx->infobox.win)
        return;

    delwin(ctx->infobox.win);
    memset(&ctx->infobox, 0, sizeof(struct infobox));
}

/* update infobox info and draw in respective chat window */
static void draw_infobox(ToxWindow *self)
{
    struct infobox *infobox = &self->chatwin->infobox;

    if (infobox->win == NULL)
        return;

    int x2, y2;
    getmaxyx(self->window, y2, x2);

    if (x2 < INFOBOX_WIDTH || y2 < INFOBOX_HEIGHT)
        return;

    uint64_t curtime = get_unix_time();

    /* update elapsed time string once per second */
    if (curtime > infobox->lastupdate)
        get_elapsed_time_str(infobox->timestr, sizeof(infobox->timestr), curtime - infobox->starttime);

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
    wprintw(infobox->win, "%.2f\n", infobox->vad_lvl);

    wborder(infobox->win, ACS_VLINE, ' ', ACS_HLINE, ACS_HLINE, ACS_TTEE, ' ', ACS_LLCORNER, ' ');
    wrefresh(infobox->win);
}

#endif /* AUDIO */

static void send_action(ToxWindow *self, ChatContext *ctx, Tox *m, char *action)
{
    if (action == NULL)
        return;

    char selfname[TOX_MAX_NAME_LENGTH];
    uint16_t len = tox_get_self_name(m, (uint8_t *) selfname);
    selfname[len] = '\0';

    char timefrmt[TIME_STR_SIZE];
    get_time_str(timefrmt, sizeof(timefrmt));

    line_info_add(self, timefrmt, selfname, NULL, OUT_ACTION, 0, 0, "%s", action);
    cqueue_add(ctx->cqueue, action, strlen(action), OUT_ACTION, ctx->hst->line_end->id + 1);
}

static void chat_onKey(ToxWindow *self, Tox *m, wint_t key, bool ltr)
{
    ChatContext *ctx = self->chatwin;
    StatusBar *statusbar = self->stb;

    int x, y, y2, x2;
    getyx(self->window, y, x);
    getmaxyx(self->window, y2, x2);

    if (x2 <= 0)
        return;

    if (self->help->active) {
        help_onKey(self, key);
        return;
    }

    if (ltr) {    /* char is printable */
        input_new_char(self, key, x, y, x2, y2);

        if (ctx->line[0] != '/' && !ctx->self_is_typing && statusbar->is_online)
            set_self_typingstatus(self, m, 1);

        return;
    }

    if (line_info_onKey(self, key))
        return;

    input_handle(self, key, x, y, x2, y2);

    if (key == '\t' && ctx->len > 1 && ctx->line[0] == '/') {    /* TAB key: auto-complete */
        int diff = -1;

        /* TODO: make this not suck */
        if (wcsncmp(ctx->line, L"/sendfile \"", wcslen(L"/sendfile \"")) == 0) {
            diff = dir_match(self, m, ctx->line, L"/sendfile");
        } else if (wcsncmp(ctx->line, L"/avatar \"", wcslen(L"/avatar \"")) == 0) {
            diff = dir_match(self, m, ctx->line, L"/avatar");
        } else {
            diff = complete_line(self, chat_cmd_list, AC_NUM_CHAT_COMMANDS, MAX_CMDNAME_SIZE);
        }

        if (diff != -1) {
            if (x + diff > x2 - 1) {
                int wlen = wcswidth(ctx->line, sizeof(ctx->line));
                ctx->start = wlen < x2 ? 0 : wlen - x2 + 1;
            }
        } else {
            sound_notify(self, error, 0, NULL);
        }

    } else if (key == '\n') {
        rm_trailing_spaces_buf(ctx);

        char line[MAX_STR_SIZE];

        if (wcs_to_mbs_buf(line, ctx->line, MAX_STR_SIZE) == -1)
            memset(&line, 0, sizeof(line));

        if (!string_is_empty(line))
            add_line_to_hist(ctx);

        if (line[0] == '/') {
            if (strcmp(line, "/close") == 0) {
                kill_chat_window(self, m);
                return;
            } else if (strncmp(line, "/me ", strlen("/me ")) == 0) {
                send_action(self, ctx, m, line + strlen("/me "));
            } else {
                execute(ctx->history, self, m, line, CHAT_COMMAND_MODE);
            }
        } else if (!string_is_empty(line)) {
            char selfname[TOX_MAX_NAME_LENGTH];
            uint16_t len = tox_get_self_name(m, (uint8_t *) selfname);
            selfname[len] = '\0';

            char timefrmt[TIME_STR_SIZE];
            get_time_str(timefrmt, sizeof(timefrmt));

            line_info_add(self, timefrmt, selfname, NULL, OUT_MSG, 0, 0, "%s", line);
            cqueue_add(ctx->cqueue, line, strlen(line), OUT_MSG, ctx->hst->line_end->id + 1);
        }

        wclear(ctx->linewin);
        wmove(self->window, y2 - CURS_Y_OFFSET, 0);
        reset_buf(ctx);
    }

    if (ctx->len <= 0 && ctx->self_is_typing)
        set_self_typingstatus(self, m, 0);
}

static void chat_onDraw(ToxWindow *self, Tox *m)
{
    int x2, y2;
    getmaxyx(self->window, y2, x2);

    ChatContext *ctx = self->chatwin;

    line_info_print(self);
    wclear(ctx->linewin);

    curs_set(1);

    if (ctx->len > 0)
        mvwprintw(ctx->linewin, 1, 0, "%ls", &ctx->line[ctx->start]);

    /* Draw status bar */
    StatusBar *statusbar = self->stb;
    mvwhline(statusbar->topline, 1, 0, ACS_HLINE, x2);
    wmove(statusbar->topline, 0, 0);

    /* Draw name, status and note in statusbar */
    if (statusbar->is_online) {
        int colour = WHITE;
        uint8_t status = statusbar->status;

        switch (status) {
            case TOX_USERSTATUS_NONE:
                colour = GREEN;
                break;

            case TOX_USERSTATUS_AWAY:
                colour = YELLOW;
                break;

            case TOX_USERSTATUS_BUSY:
                colour = RED;
                break;

            case TOX_USERSTATUS_INVALID:
                colour = MAGENTA;
                break;
        }

        wattron(statusbar->topline, COLOR_PAIR(colour) | A_BOLD);
        wprintw(statusbar->topline, " %s", ONLINE_CHAR);
        wattroff(statusbar->topline, COLOR_PAIR(colour) | A_BOLD);

        if (Friends.list[self->num].is_typing)
            wattron(statusbar->topline, COLOR_PAIR(YELLOW));

        wattron(statusbar->topline, A_BOLD);
        wprintw(statusbar->topline, " %s ", statusbar->nick);
        wattroff(statusbar->topline, A_BOLD);

        if (Friends.list[self->num].is_typing)
            wattroff(statusbar->topline, COLOR_PAIR(YELLOW));
    } else {
        wprintw(statusbar->topline, " %s", OFFLINE_CHAR);
        wattron(statusbar->topline, A_BOLD);
        wprintw(statusbar->topline, " %s ", statusbar->nick);
        wattroff(statusbar->topline, A_BOLD);
    }

    /* Reset statusbar->statusmsg on window resize */
    if (x2 != self->x) {
        char statusmsg[TOX_MAX_STATUSMESSAGE_LENGTH] = {'\0'};

        pthread_mutex_lock(&Winthread.lock);
        int s_len = tox_get_status_message(m, self->num, (uint8_t *) statusmsg, TOX_MAX_STATUSMESSAGE_LENGTH);
        pthread_mutex_unlock(&Winthread.lock);

        filter_str(statusmsg, s_len);
        snprintf(statusbar->statusmsg, sizeof(statusbar->statusmsg), "%s", statusmsg);
        statusbar->statusmsg_len = strlen(statusbar->statusmsg);
    }

    self->x = x2;

    /* Truncate note if it doesn't fit in statusbar */
    uint16_t maxlen = x2 - getcurx(statusbar->topline) - (KEY_IDENT_DIGITS * 2) - 6;

    if (statusbar->statusmsg_len > maxlen) {
        statusbar->statusmsg[maxlen - 3] = '\0';
        strcat(statusbar->statusmsg, "...");
        statusbar->statusmsg_len = maxlen;
    }

    if (statusbar->statusmsg[0])
        wprintw(statusbar->topline, ": %s ", statusbar->statusmsg);

    wclrtoeol(statusbar->topline);
    wmove(statusbar->topline, 0, x2 - (KEY_IDENT_DIGITS * 2) - 3);
    wprintw(statusbar->topline, "{");

    int i;

    for (i = 0; i < KEY_IDENT_DIGITS; ++i)
        wprintw(statusbar->topline, "%02X", Friends.list[self->num].pub_key[i] & 0xff);

    wprintw(statusbar->topline, "}\n");

    mvwhline(self->window, y2 - CHATBOX_HEIGHT, 0, ACS_HLINE, x2);

    int y, x;
    getyx(self->window, y, x);
    (void) x;
    int new_x = ctx->start ? x2 - 1 : wcswidth(ctx->line, ctx->pos);
    wmove(self->window, y + 1, new_x);

    wrefresh(self->window);

#ifdef AUDIO
    if (ctx->infobox.active) {
        draw_infobox(self);
        wrefresh(self->window);
    }
#endif

    if (self->help->active)
        help_onDraw(self);
}

static void chat_onInit(ToxWindow *self, Tox *m)
{
    curs_set(1);
    int x2, y2;
    getmaxyx(self->window, y2, x2);
    self->x = x2;

    /* Init statusbar info */
    StatusBar *statusbar = self->stb;

    statusbar->status = tox_get_user_status(m, self->num);
    statusbar->is_online = tox_get_friend_connection_status(m, self->num) == 1;

    char statusmsg[TOX_MAX_STATUSMESSAGE_LENGTH] = {'\0'};
    uint16_t s_len = tox_get_status_message(m, self->num, (uint8_t *) statusmsg, TOX_MAX_STATUSMESSAGE_LENGTH);
    statusmsg[s_len] = '\0';

    filter_str(statusmsg, s_len);
    snprintf(statusbar->statusmsg, sizeof(statusbar->statusmsg), "%s", statusmsg);
    statusbar->statusmsg_len = strlen(statusbar->statusmsg);

    char nick[TOX_MAX_NAME_LENGTH + 1];
    int n_len = get_nick_truncate(m, nick, self->num);
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

    if (ctx->log == NULL || ctx->hst == NULL || ctx->cqueue == NULL)
        exit_toxic_err("failed in chat_onInit", FATALERR_MEMORY);

    line_info_init(ctx->hst);

    char myid[TOX_FRIEND_ADDRESS_SIZE];
    tox_get_address(m, (uint8_t *) myid);

    log_enable(nick, myid, Friends.list[self->num].pub_key, ctx->log, LOG_CHAT);
    load_chat_history(self, ctx->log);

    if (!Friends.list[self->num].logging_on)
        log_disable(ctx->log);

    execute(ctx->history, self, m, "/log", GLOBAL_COMMAND_MODE);

    scrollok(ctx->history, 0);
    wmove(self->window, y2 - CURS_Y_OFFSET, 0);
}

ToxWindow new_chat(Tox *m, int32_t friendnum)
{
    ToxWindow ret;
    memset(&ret, 0, sizeof(ret));

    ret.active = true;
    ret.is_chat = true;

    ret.onKey = &chat_onKey;
    ret.onDraw = &chat_onDraw;
    ret.onInit = &chat_onInit;
    ret.onMessage = &chat_onMessage;
    ret.onConnectionChange = &chat_onConnectionChange;
    ret.onTypingChange = & chat_onTypingChange;
    ret.onGroupInvite = &chat_onGroupInvite;
    ret.onNickChange = &chat_onNickChange;
    ret.onStatusChange = &chat_onStatusChange;
    ret.onStatusMessageChange = &chat_onStatusMessageChange;
    ret.onAction = &chat_onAction;
    ret.onFileSendRequest = &chat_onFileSendRequest;
    ret.onFileControl = &chat_onFileControl;
    ret.onFileData = &chat_onFileData;
    ret.onReadReceipt = &chat_onReadReceipt;

#ifdef AUDIO
    ret.onInvite = &chat_onInvite;
    ret.onRinging = &chat_onRinging;
    ret.onStarting = &chat_onStarting;
    ret.onEnding = &chat_onEnding;
    ret.onError = &chat_onError;
    ret.onStart = &chat_onStart;
    ret.onCancel = &chat_onCancel;
    ret.onReject = &chat_onReject;
    ret.onEnd = &chat_onEnd;
    ret.onRequestTimeout = &chat_onRequestTimeout;
    ret.onPeerTimeout = &chat_onPeerTimeout;
    
    ret.call_idx = -1;
    ret.device_selection[0] = ret.device_selection[1] = -1;
    ret.ringing_sound = -1;
#endif /* AUDIO */
    
    ret.active_box = -1;
    
    char nick[TOX_MAX_NAME_LENGTH];
    int n_len = get_nick_truncate(m, nick, friendnum);
    set_window_title(&ret, nick, n_len);

    ChatContext *chatwin = calloc(1, sizeof(ChatContext));
    StatusBar *stb = calloc(1, sizeof(StatusBar));
    Help *help = calloc(1, sizeof(Help));

    if (stb == NULL || chatwin == NULL || help == NULL)
        exit_toxic_err("failed in new_chat", FATALERR_MEMORY);

    ret.chatwin = chatwin;
    ret.stb = stb;
    ret.help = help;

    ret.num = friendnum;

    return ret;
}

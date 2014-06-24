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
#define _GNU_SOURCE    /* needed for strcasestr() and wcwidth() */
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>

#include "toxic.h"
#include "windows.h"
#include "execute.h"
#include "misc_tools.h"
#include "friendlist.h"
#include "toxic_strings.h"
#include "log.h"
#include "line_info.h"
#include "settings.h"

#ifdef _SUPPORT_AUDIO
#include "audio_call.h"
#endif /* _SUPPORT_AUDIO */

extern char *DATA_FILE;

extern FileSender file_senders[MAX_FILES];
extern ToxicFriend friends[MAX_FRIENDS_NUM];

extern struct _Winthread Winthread;
extern struct user_settings *user_settings;

#ifdef _SUPPORT_AUDIO
static void init_infobox(ToxWindow *self);
static void kill_infobox(ToxWindow *self);
#endif  /* _SUPPORT_AUDIO */

#ifdef _SUPPORT_AUDIO
#define AC_NUM_CHAT_COMMANDS 26
#else
#define AC_NUM_CHAT_COMMANDS 18
#endif /* _SUPPORT_AUDIO */

/* Array of chat command names used for tab completion. */
static const uint8_t chat_cmd_list[AC_NUM_CHAT_COMMANDS][MAX_CMDNAME_SIZE] = {
    { "/accept"     },
    { "/add"        },
    { "/clear"      },
    { "/close"      },
    { "/connect"    },
    { "/exit"       },
    { "/groupchat"  },
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

#ifdef _SUPPORT_AUDIO

    { "/call"       },
    { "/cancel"     },
    { "/answer"     },
    { "/reject"     },
    { "/hangup"     },
    { "/sdev"       },
    { "/mute"       },
    { "/sense"      },

#endif /* _SUPPORT_AUDIO */
};

static void set_typingstatus(ToxWindow *self, Tox *m, uint8_t is_typing)
{
    ChatContext *ctx = self->chatwin;

    tox_set_user_is_typing(m, self->num, is_typing);
    ctx->self_is_typing = is_typing;
}

void kill_chat_window(ToxWindow *self)
{
    set_active_window(0);
    ChatContext *ctx = self->chatwin;
    StatusBar *statusbar = self->stb;

    log_disable(ctx->log);
    line_info_cleanup(ctx->hst);

    int f_num = self->num;
    delwin(ctx->linewin);
    delwin(statusbar->topline);
    del_window(self);
    disable_chatwin(f_num);

    free(ctx->log);
    free(ctx->hst);
    free(ctx);
    free(statusbar);
}

static void chat_onMessage(ToxWindow *self, Tox *m, int32_t num, uint8_t *msg, uint16_t len)
{
    if (self->num != num)
        return;

    msg[len] = '\0';

    ChatContext *ctx = self->chatwin;

    uint8_t nick[TOX_MAX_NAME_LENGTH];
    int n_len = tox_get_name(m, num, nick);

    n_len = MIN(n_len, TOXIC_MAX_NAME_LENGTH - 1);
    nick[n_len] = '\0';

    uint8_t timefrmt[TIME_STR_SIZE];
    get_time_str(timefrmt, sizeof(timefrmt));

    line_info_add(self, timefrmt, nick, NULL, msg, IN_MSG, 0, 0);

    write_to_log(msg, nick, ctx->log, false);
    alert_window(self, WINDOW_ALERT_1, true);
}

static void chat_onConnectionChange(ToxWindow *self, Tox *m, int32_t num, uint8_t status)
{
    if (self->num != num)
        return;

    StatusBar *statusbar = self->stb;

    if (status == 1) {
        statusbar->is_online = true;
        friends[num].is_typing = tox_get_is_typing(m, num);
    } else {
        statusbar->is_online = false;
        friends[num].is_typing = 0;
    }
}

static void chat_onTypingChange(ToxWindow *self, Tox *m, int32_t num, uint8_t is_typing)
{
    if (self->num != num)
        return;

    friends[num].is_typing = is_typing;
}

static void chat_onAction(ToxWindow *self, Tox *m, int32_t num, uint8_t *action, uint16_t len)
{
    if (self->num != num)
        return;


    action[len] = '\0';

    ChatContext *ctx = self->chatwin;

    uint8_t nick[TOX_MAX_NAME_LENGTH];
    int n_len = tox_get_name(m, num, nick);

    n_len = MIN(n_len, TOXIC_MAX_NAME_LENGTH - 1);;
    nick[n_len] = '\0';

    uint8_t timefrmt[TIME_STR_SIZE];
    get_time_str(timefrmt, sizeof(timefrmt));

    line_info_add(self, timefrmt, nick, NULL, action, ACTION, 0, 0);
    write_to_log(action, nick, ctx->log, true);
    alert_window(self, WINDOW_ALERT_1, true);
}

static void chat_onNickChange(ToxWindow *self, Tox *m, int32_t num, uint8_t *nick, uint16_t len)
{
    if (self->num != num)
        return;

    len = MIN(len, TOXIC_MAX_NAME_LENGTH - 1);
    nick[len] = '\0';
    strcpy(self->name, nick);
}

static void chat_onStatusChange(ToxWindow *self, Tox *m, int32_t num, uint8_t status)
{
    if (self->num != num)
        return;

    StatusBar *statusbar = self->stb;
    statusbar->status = status;
}

static void chat_onStatusMessageChange(ToxWindow *self, int32_t num, uint8_t *status, uint16_t len)
{
    if (self->num != num)
        return;

    StatusBar *statusbar = self->stb;

    status[len] = '\0';
    snprintf(statusbar->statusmsg, sizeof(statusbar->statusmsg), "%s", status);
    len = strlen(statusbar->statusmsg);
    statusbar->statusmsg_len = len;
    statusbar->statusmsg[len] = '\0';
}

static void chat_onFileSendRequest(ToxWindow *self, Tox *m, int32_t num, uint8_t filenum,
                                   uint64_t filesize, uint8_t *pathname, uint16_t path_len)
{
    if (self->num != num)
        return;

    uint8_t msg[MAX_STR_SIZE * 2];
    uint8_t *errmsg;

    pathname[path_len] = '\0';


    /* holds the filename appended to the user specified path */
    uint8_t filename_path[MAX_STR_SIZE] = {0};

    /* holds the lone filename */
    uint8_t filename_nopath[MAX_STR_SIZE];
    get_file_name(filename_nopath, pathname);
    int len = strlen(filename_nopath);

    snprintf(msg, sizeof(msg), "File transfer request for '%s' (%llu bytes).", filename_nopath,
             (long long unsigned int)filesize);
    line_info_add(self, NULL, NULL, NULL, msg, SYS_MSG, 0, 0);

    if (filenum >= MAX_FILES) {
        errmsg = "Too many pending file requests; discarding.";
        line_info_add(self, NULL, NULL, NULL, errmsg, SYS_MSG, 0, 0);
        return;
    }

    /* use specified path in config if possible */
    if (user_settings->download_path[0]) {
        snprintf(filename_path, sizeof(filename_path), "%s%s", user_settings->download_path, filename_nopath);
        len += strlen(user_settings->download_path);
    }

    if (len >= sizeof(friends[num].file_receiver.filenames[filenum])) {
        errmsg = "File name too long; discarding.";
        line_info_add(self, NULL, NULL, NULL, errmsg, SYS_MSG, 0, 0);
        return;
    }

    uint8_t filename[MAX_STR_SIZE];

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
            errmsg = "Error saving file to disk.";
            line_info_add(self, NULL, NULL, NULL, errmsg, SYS_MSG, 0, 0);
            return;
        }
    }

    snprintf(msg, sizeof(msg), "Type '/savefile %d' to accept the file transfer.", filenum);
    line_info_add(self, NULL, NULL, NULL, msg, SYS_MSG, 0, 0);

    friends[num].file_receiver.pending[filenum] = true;
    friends[num].file_receiver.size[filenum] = filesize;
    strcpy(friends[num].file_receiver.filenames[filenum], filename);

    alert_window(self, WINDOW_ALERT_2, true);
}

static void chat_close_file_receiver(int num, uint8_t filenum)
{
    friends[num].file_receiver.pending[filenum] = false;
    friends[num].file_receiver.size[filenum]  = 0;
    FILE *file = friends[num].file_receiver.files[filenum];

    if (file != NULL)
        fclose(file);
}

static void chat_onFileControl(ToxWindow *self, Tox *m, int32_t num, uint8_t receive_send,
                               uint8_t filenum, uint8_t control_type, uint8_t *data, uint16_t length)
{
    if (self->num != num)
        return;

    const uint8_t *filename;
    uint8_t msg[MAX_STR_SIZE] = {0};
    int i;   /* file_sender index */

    if (receive_send == 0) {
        filename = friends[num].file_receiver.filenames[filenum];
    } else {
        for (i = 0; i < MAX_FILES; ++i) {
            if (file_senders[i].filenum == filenum)
                break;
        }

        filename = file_senders[i].pathname;
    }

    switch (control_type) {
        case TOX_FILECONTROL_ACCEPT:
            if (receive_send == 1) {
                snprintf(msg, sizeof(msg), "File transfer for '%s' accepted (%.1f%%)", filename, 0.0);
                file_senders[i].line_id = self->chatwin->hst->line_end->id + 1;
            }

            break;

        case TOX_FILECONTROL_KILL:
            snprintf(msg, sizeof(msg), "File transfer for '%s' failed.", filename);

            if (receive_send == 0)
                chat_close_file_receiver(num, filenum);

            break;

        case TOX_FILECONTROL_FINISHED:
            if (receive_send == 0) {
                snprintf(msg, sizeof(msg), "File transfer for '%s' complete.", filename);
                chat_close_file_receiver(num, filenum);
            }

            break;
    }

    line_info_add(self, NULL, NULL, NULL, msg, SYS_MSG, 0, 0);
    alert_window(self, WINDOW_ALERT_2, true);
}

static void chat_onFileData(ToxWindow *self, Tox *m, int32_t num, uint8_t filenum, uint8_t *data,
                            uint16_t length)
{
    if (self->num != num)
        return;

    if (fwrite(data, length, 1, friends[num].file_receiver.files[filenum]) != 1) {
        uint8_t *msg = " * Error writing to file.";
        line_info_add(self, NULL, NULL, NULL, msg, SYS_MSG, 0, RED);

        tox_file_send_control(m, num, 1, filenum, TOX_FILECONTROL_KILL, 0, 0);
        chat_close_file_receiver(num, filenum);
    }

    /* refresh line with percentage complete */
    uint8_t msg[MAX_STR_SIZE];
    uint64_t size = friends[num].file_receiver.size[filenum];
    long double remain = (long double) tox_file_data_remaining(m, num, filenum, 1);
    long double pct_remain = 100;

    if (remain)
        pct_remain = (1 - (remain / size)) * 100;

    const uint8_t *name = friends[num].file_receiver.filenames[filenum];
    snprintf(msg, sizeof(msg), "Saving file as: '%s' (%.1Lf%%)", name, pct_remain);
    line_info_set(self, friends[num].file_receiver.line_id[filenum], msg);

}

static void chat_onGroupInvite(ToxWindow *self, Tox *m, int32_t friendnumber, uint8_t *group_pub_key)
{
    if (self->num != friendnumber)
        return;

    uint8_t name[TOX_MAX_NAME_LENGTH];
    uint8_t msg[MAX_STR_SIZE + TOX_MAX_NAME_LENGTH];
    int n_len = tox_get_name(m, friendnumber, name);

    n_len = MIN(n_len, TOXIC_MAX_NAME_LENGTH - 1);
    name[n_len] = '\0';

    snprintf(msg, sizeof(msg), "%s has invited you to a group chat.", name);
    line_info_add(self, NULL, NULL, NULL, msg, SYS_MSG, 0, 0);
    snprintf(msg, sizeof(msg), "Type \"/join\" to join the chat.", name);
    line_info_add(self, NULL, NULL, NULL, msg, SYS_MSG, 0, 0);

    memcpy(friends[friendnumber].pending_groupchat, group_pub_key, TOX_CLIENT_ID_SIZE);
    alert_window(self, WINDOW_ALERT_2, true);
}

/* Av Stuff */
#ifdef _SUPPORT_AUDIO

void chat_onInvite (ToxWindow *self, ToxAv *av, int call_index)
{
    if (self->num != toxav_get_peer_id(av, call_index, 0))
        return;

    /* call_index is set here and reset on call end */
    
    self->call_idx = call_index;

    line_info_add(self, NULL, NULL, NULL, "Incoming audio call! Type: \"/answer\" or \"/reject\"", SYS_MSG, 0, 0);

    alert_window(self, WINDOW_ALERT_0, true);
}

void chat_onRinging (ToxWindow *self, ToxAv *av, int call_index)
{
    if ( self->call_idx != call_index || self->num != toxav_get_peer_id(av, call_index, 0))
        return;

    line_info_add(self, NULL, NULL, NULL, "Ringing...\"cancel\" ?", SYS_MSG, 0, 0);
}

void chat_onStarting (ToxWindow *self, ToxAv *av, int call_index)
{
    if ( self->call_idx != call_index || self->num != toxav_get_peer_id(av, call_index, 0))
        return;

    init_infobox(self);

    line_info_add(self, NULL, NULL, NULL, "Call started! Type: \"/hangup\" to end it.", SYS_MSG, 0, 0);
}

void chat_onEnding (ToxWindow *self, ToxAv *av, int call_index)
{
    if (self->call_idx != call_index || self->num != toxav_get_peer_id(av, call_index, 0))
        return;

    kill_infobox(self);
    self->call_idx = -1;
    line_info_add(self, NULL, NULL, NULL, "Call ended!", SYS_MSG, 0, 0);
}

void chat_onError (ToxWindow *self, ToxAv *av, int call_index)
{
    if (self->call_idx != call_index || self->num != toxav_get_peer_id(av, call_index, 0))
        return;

    self->call_idx = -1;
    line_info_add(self, NULL, NULL, NULL, "Error!", SYS_MSG, 0, 0);
}

void chat_onStart (ToxWindow *self, ToxAv *av, int call_index)
{
    if ( self->call_idx != call_index || self->num != toxav_get_peer_id(av, call_index, 0))
        return;

    init_infobox(self);

    line_info_add(self, NULL, NULL, NULL, "Call started! Type: \"/hangup\" to end it.", SYS_MSG, 0, 0);
}

void chat_onCancel (ToxWindow *self, ToxAv *av, int call_index)
{
    if ( self->call_idx != call_index || self->num != toxav_get_peer_id(av, call_index, 0))
        return;

    kill_infobox(self);
    self->call_idx = -1;
    line_info_add(self, NULL, NULL, NULL, "Call canceled!", SYS_MSG, 0, 0);
}

void chat_onReject (ToxWindow *self, ToxAv *av, int call_index)
{
    if (self->call_idx != call_index || self->num != toxav_get_peer_id(av, call_index, 0))
        return;

    self->call_idx = -1;
    line_info_add(self, NULL, NULL, NULL, "Rejected!", SYS_MSG, 0, 0);
}

void chat_onEnd (ToxWindow *self, ToxAv *av, int call_index)
{
    if (self->call_idx != call_index || self->num != toxav_get_peer_id(av, call_index, 0))
        return;

    kill_infobox(self);
    self->call_idx = -1;
    line_info_add(self, NULL, NULL, NULL, "Call ended!", SYS_MSG, 0, 0);
}

void chat_onRequestTimeout (ToxWindow *self, ToxAv *av, int call_index)
{
    if (self->call_idx != call_index || self->num != toxav_get_peer_id(av, call_index, 0))
        return;

    self->call_idx = -1;
    line_info_add(self, NULL, NULL, NULL, "No answer!", SYS_MSG, 0, 0);
}

void chat_onPeerTimeout (ToxWindow *self, ToxAv *av, int call_index)
{
    if (self->call_idx != call_index || self->num != toxav_get_peer_id(av, call_index, 0))
        return;

    kill_infobox(self);
    self->call_idx = -1;
    line_info_add(self, NULL, NULL, NULL, "Peer disconnected; call ended!", SYS_MSG, 0, 0);
}


#define INFOBOX_HEIGHT 7
#define INFOBOX_WIDTH 21

static void init_infobox(ToxWindow *self)
{
    ChatContext *ctx = self->chatwin;

    int x2, y2;
    getmaxyx(self->window, y2, x2);

    memset(&ctx->infobox, 0, sizeof(struct infobox));

    ctx->infobox.win = newwin(INFOBOX_HEIGHT, INFOBOX_WIDTH + 1, 1, x2 - INFOBOX_WIDTH);
    ctx->infobox.starttime = get_unix_time();
    ctx->infobox.vad_lvl = VAD_THRESHOLD_DEFAULT;
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

#endif /* _SUPPORT_AUDIO */

static void send_action(ToxWindow *self, ChatContext *ctx, Tox *m, uint8_t *action)
{
    if (action == NULL)
        return;

    uint8_t selfname[TOX_MAX_NAME_LENGTH];
    uint16_t len = tox_get_self_name(m, selfname);
    selfname[len] = '\0';

    uint8_t timefrmt[TIME_STR_SIZE];
    get_time_str(timefrmt, sizeof(timefrmt));

    line_info_add(self, timefrmt, selfname, NULL, action, ACTION, 0, 0);

    if (tox_send_action(m, self->num, action, strlen(action)) == 0) {
        uint8_t *errmsg = " * Failed to send action.";
        line_info_add(self, NULL, selfname, NULL, errmsg, SYS_MSG, 0, RED);
    } else {
        write_to_log(action, selfname, ctx->log, true);
    }
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

    int cur_len = 0;   /* widechar size of current char */
    int x2_is_odd = x2 % 2 != 0;

    if (ltr) {   /* char is printable */
        if (ctx->len < MAX_STR_SIZE - 1) {
            add_char_to_buf(ctx, key);

            if (x >= x2 - 1) {
                wmove(self->window, y, x2 / 2 + x2_is_odd);
                ctx->start += x2 / 2;
            } else {
                wmove(self->window, y, x + MAX(1, wcwidth(key)));
            }
        }

        if (!ctx->self_is_typing && ctx->line[0] != '/')
            set_typingstatus(self, m, 1);

    } else { /* if (!ltr) */
        if (line_info_onKey(self, key))
            return;

        if (key == 0x107 || key == 0x8 || key == 0x7f) {  /* BACKSPACE key */
            if (ctx->pos > 0) {
                cur_len = MAX(1, wcwidth(ctx->line[ctx->pos]));
                del_char_buf_bck(ctx);

                if (x == 0) {
                    ctx->start = ctx->start >= x2 ? ctx->start - x2 : 0;
                    int new_x = ctx->start == 0 ? ctx->pos : x2 - cur_len;
                    wmove(self->window, y, new_x);
                } else {
                    wmove(self->window, y, x - cur_len);
                }
            } else {
                beep();
            }
        }

        else if (key == KEY_DC) {      /* DEL key: Remove character at pos */
            if (ctx->pos != ctx->len)
                del_char_buf_frnt(ctx);
            else
                beep();
        }

        else if (key == T_KEY_DISCARD) {    /* CTRL-U: Delete entire line behind pos */
            if (ctx->pos > 0) {
                discard_buf(ctx);
                wmove(self->window, y2 - CURS_Y_OFFSET, 0);
            } else {
                beep();
            }
        }

        else if (key == T_KEY_KILL) {    /* CTRL-K: Delete entire line in front of pos */
            if (ctx->pos != ctx->len)
                kill_buf(ctx);
            else
                beep();
        }

        else if (key == KEY_HOME || key == T_KEY_C_A) {  /* HOME/C-a key: Move cursor to start of line */
            if (ctx->pos > 0) {
                ctx->pos = 0;
                ctx->start = 0;
                wmove(self->window, y2 - CURS_Y_OFFSET, 0);
            }
        }

        else if (key == KEY_END || key == T_KEY_C_E) {  /* END/C-e key: move cursor to end of line */
            if (ctx->pos != ctx->len) {
                ctx->pos = ctx->len;
                ctx->start = x2 * (ctx->len / x2);
                mv_curs_end(self->window, ctx->len, y2, x2);
            }
        }

        else if (key == KEY_LEFT) {
            if (ctx->pos > 0) {
                --ctx->pos;
                cur_len = MAX(1, wcwidth(ctx->line[ctx->pos]));

                if (x == 0) {
                    wmove(self->window, y, x2 - cur_len);
                    ctx->start = ctx->start >= x2 ? ctx->start - x2 : 0;
                    ctx->pos = ctx->start + x2 - 1;
                } else {
                    wmove(self->window, y, x - cur_len);
                }
            } else {
                beep();
            }
        }

        else if (key == KEY_RIGHT) {
            if (ctx->pos < ctx->len) {
                ++ctx->pos;

                if (x == x2 - 1) {
                    wmove(self->window, y, 0);
                    ctx->start += x2;
                    ctx->pos = ctx->start;
                } else {
                    cur_len = MAX(1, wcwidth(ctx->line[ctx->pos]));
                    wmove(self->window, y, x + cur_len);
                }
            } else {
                beep();
            }
        }

        else if (key == KEY_UP) {    /* fetches previous item in history */
            fetch_hist_item(ctx, MOVE_UP);
            ctx->start = x2 * (ctx->len / x2);
            mv_curs_end(self->window, ctx->len, y2, x2);
        }

        else if (key == KEY_DOWN) {    /* fetches next item in history */
            fetch_hist_item(ctx, MOVE_DOWN);
            ctx->start = x2 * (ctx->len / x2);
            mv_curs_end(self->window, ctx->len, y2, x2);
        }

        else if (key == '\t') {    /* TAB key: completes command */
            if (ctx->len > 1 && ctx->line[0] == '/') {
                int diff = complete_line(ctx, chat_cmd_list, AC_NUM_CHAT_COMMANDS, MAX_CMDNAME_SIZE);

                if (diff != -1) {
                    if (x + diff > x2 - 1) {
                        //int ofst = x + diff - x2;
                        wmove(self->window, y, x + diff);
                        ctx->start += x2 / 2;
                    } else {
                        wmove(self->window, y, x + diff);
                    }
                } else {
                    beep();
                }
            } else {
                beep();
            }
        }

        /* RETURN key: Execute command or print line */
        else if (key == '\n') {
            rm_trailing_spaces_buf(ctx);

            uint8_t line[MAX_STR_SIZE];

            if (wcs_to_mbs_buf(line, ctx->line, MAX_STR_SIZE) == -1)
                memset(&line, 0, sizeof(line));

            wclear(ctx->linewin);
            wmove(self->window, y2 - CURS_Y_OFFSET, 0);

            if (!string_is_empty(line))
                add_line_to_hist(ctx);

            if (line[0] == '/') {
                if (strcmp(line, "/close") == 0) {
                    if (ctx->self_is_typing)
                        set_typingstatus(self, m, 0);

                    kill_chat_window(self);
                    return;
                } else if (strncmp(line, "/me ", strlen("/me ")) == 0) {
                    send_action(self, ctx, m, line + strlen("/me "));
                } else {
                    execute(ctx->history, self, m, line, CHAT_COMMAND_MODE);
                }
            } else if (!string_is_empty(line)) {
                uint8_t selfname[TOX_MAX_NAME_LENGTH];
                uint16_t len = tox_get_self_name(m, selfname);
                selfname[len] = '\0';

                uint8_t timefrmt[TIME_STR_SIZE];
                get_time_str(timefrmt, sizeof(timefrmt));

                line_info_add(self, timefrmt, selfname, NULL, line, OUT_MSG, 0, 0);

                if (!statusbar->is_online || tox_send_message(m, self->num, line, strlen(line)) == 0) {
                    uint8_t *errmsg = " * Failed to send message.";
                    line_info_add(self, NULL, NULL, NULL, errmsg, SYS_MSG, 0, RED);
                } else {
                    write_to_log(line, selfname, ctx->log, false);
                }
            }

            reset_buf(ctx);
        }
    }

    if (ctx->len <= 0 && ctx->self_is_typing)
        set_typingstatus(self, m, 0);
}

static void chat_onDraw(ToxWindow *self, Tox *m)
{
    int x2, y2;
    getmaxyx(self->window, y2, x2);

    ChatContext *ctx = self->chatwin;

    line_info_print(self);
    wclear(ctx->linewin);

    curs_set(1);

    if (ctx->len > 0) {
        uint8_t line[MAX_STR_SIZE];

        if (wcs_to_mbs_buf(line, ctx->line, MAX_STR_SIZE) == -1) {
            reset_buf(ctx);
            wmove(self->window, y2 - CURS_Y_OFFSET, 0);
        } else {
            mvwprintw(ctx->linewin, 1, 0, "%s", &line[ctx->start]);
        }
    }

    /* Draw status bar */
    StatusBar *statusbar = self->stb;
    mvwhline(statusbar->topline, 1, 0, ACS_HLINE, x2);
    wmove(statusbar->topline, 0, 0);

    /* Draw name, status and note in statusbar */
    if (statusbar->is_online) {
        const uint8_t *status_text = "Unknown";
        int colour = WHITE;

        uint8_t status = statusbar->status;

        switch (status) {
            case TOX_USERSTATUS_NONE:
                status_text = "Online";
                colour = GREEN;
                break;

            case TOX_USERSTATUS_AWAY:
                status_text = "Away";
                colour = YELLOW;
                break;

            case TOX_USERSTATUS_BUSY:
                status_text = "Busy";
                colour = RED;
                break;

            case TOX_USERSTATUS_INVALID:
                status_text = "ERROR";
                colour = MAGENTA;
                break;
        }

        wattron(statusbar->topline, COLOR_PAIR(colour) | A_BOLD);
        wprintw(statusbar->topline, " [%s]", status_text);
        wattroff(statusbar->topline, COLOR_PAIR(colour) | A_BOLD);

        if (friends[self->num].is_typing)
            wattron(statusbar->topline, COLOR_PAIR(YELLOW));

        wattron(statusbar->topline, A_BOLD);
        wprintw(statusbar->topline, " %s ", self->name);
        wattroff(statusbar->topline, A_BOLD);

        if (friends[self->num].is_typing)
            wattroff(statusbar->topline, COLOR_PAIR(YELLOW));
    } else {
        wprintw(statusbar->topline, " [Offline]");
        wattron(statusbar->topline, A_BOLD);
        wprintw(statusbar->topline, " %s ", self->name);
        wattroff(statusbar->topline, A_BOLD);
    }

    /* Reset statusbar->statusmsg on window resize */
    if (x2 != self->x) {
        uint8_t statusmsg[TOX_MAX_STATUSMESSAGE_LENGTH] = {'\0'};

        pthread_mutex_lock(&Winthread.lock);
        uint16_t s_len = tox_get_status_message(m, self->num, statusmsg, TOX_MAX_STATUSMESSAGE_LENGTH);
        pthread_mutex_unlock(&Winthread.lock);
        statusmsg[s_len] = '\0';

        snprintf(statusbar->statusmsg, sizeof(statusbar->statusmsg), "%s", statusmsg);
        statusbar->statusmsg_len = s_len;
    }

    self->x = x2;

    /* Truncate note if it doesn't fit in statusbar */
    uint16_t maxlen = x2 - getcurx(statusbar->topline) - (KEY_IDENT_DIGITS * 2) - 7;

    if (statusbar->statusmsg_len > maxlen) {
        statusbar->statusmsg[maxlen] = '\0';
        statusbar->statusmsg_len = maxlen;
    }

    if (statusbar->statusmsg[0])
        wprintw(statusbar->topline, "- %s ", statusbar->statusmsg);

    wclrtoeol(statusbar->topline);
    wmove(statusbar->topline, 0, x2 - (KEY_IDENT_DIGITS * 2) - 3);
    wprintw(statusbar->topline, "{");

    int i;

    for (i = 0; i < KEY_IDENT_DIGITS; ++i)
        wprintw(statusbar->topline, "%02X", friends[self->num].pub_key[i] & 0xff);

    wprintw(statusbar->topline, "}\n");
    mvwhline(ctx->linewin, 0, 0, ACS_HLINE, x2);

#ifdef _SUPPORT_AUDIO
    wrefresh(self->window);

    if (ctx->infobox.active)
        draw_infobox(self);
#endif

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

    uint8_t statusmsg[TOX_MAX_STATUSMESSAGE_LENGTH] = {'\0'};
    uint16_t s_len = tox_get_status_message(m, self->num, statusmsg, TOX_MAX_STATUSMESSAGE_LENGTH);
    statusmsg[s_len] = '\0';
    snprintf(statusbar->statusmsg, sizeof(statusbar->statusmsg), "%s", statusmsg);
    statusbar->statusmsg_len = s_len;

    /* Init subwindows */
    ChatContext *ctx = self->chatwin;

    statusbar->topline = subwin(self->window, 2, x2, 0, 0);
    ctx->history = subwin(self->window, y2 - CHATBOX_HEIGHT + 1, x2, 0, 0);
    ctx->linewin = subwin(self->window, CHATBOX_HEIGHT, x2, y2 - CHATBOX_HEIGHT, 0);

    ctx->hst = malloc(sizeof(struct history));
    ctx->log = malloc(sizeof(struct chatlog));

    if (ctx->log == NULL || ctx->hst == NULL)
        exit_toxic_err("failed in chat_onInit", FATALERR_MEMORY);

    memset(ctx->hst, 0, sizeof(struct history));
    memset(ctx->log, 0, sizeof(struct chatlog));

    line_info_init(ctx->hst);

    if (friends[self->num].logging_on)
        log_enable(self->name, friends[self->num].pub_key, ctx->log);

    execute(ctx->history, self, m, "/help", CHAT_COMMAND_MODE);
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

#ifdef _SUPPORT_AUDIO
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
#endif /* _SUPPORT_AUDIO */

    uint8_t name[TOX_MAX_NAME_LENGTH] = {'\0'};
    int len = tox_get_name(m, friendnum, name);

    len = MIN(len, TOXIC_MAX_NAME_LENGTH - 1);

    name[len] = '\0';
    strcpy(ret.name, name);

    ChatContext *chatwin = calloc(1, sizeof(ChatContext));
    memset(chatwin, 0, sizeof(ChatContext));

    StatusBar *stb = calloc(1, sizeof(StatusBar));
    memset(stb, 0, sizeof(StatusBar));

    if (stb != NULL && chatwin != NULL) {
        ret.chatwin = chatwin;
        ret.stb = stb;
    } else {
        exit_toxic_err("failed in new_chat", FATALERR_MEMORY);
    }

    ret.num = friendnum;

    return ret;
}

/*  prompt.c
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
#include <wchar.h>

#include "toxic.h"
#include "windows.h"
#include "prompt.h"
#include "friendlist.h"
#include "execute.h"
#include "misc_tools.h"
#include "toxic_strings.h"
#include "log.h"
#include "line_info.h"
#include "settings.h"
#include "input.h"
#include "help.h"
#include "notify.h"
#include "autocomplete.h"

extern ToxWindow *prompt;
extern struct user_settings *user_settings;
extern struct Winthread Winthread;

extern FriendsList Friends;
FriendRequests FrndRequests;
#ifdef AUDIO
#define AC_NUM_GLOB_COMMANDS_AUDIO 2
#else
#define AC_NUM_GLOB_COMMANDS_AUDIO 0
#endif /* AUDIO */
#ifdef VIDEO
#define AC_NUM_GLOB_COMMANDS_VIDEO 2
#else
#define AC_NUM_GLOB_COMMANDS_VIDEO 0
#endif /* VIDEO */
#ifdef PYTHON
#define AC_NUM_GLOB_COMMANDS_PYTHON 1
#else
#define AC_NUM_GLOB_COMMANDS_PYTHON 0
#endif /* PYTHON */
#ifdef QRCODE
#define AC_NUM_GLOB_COMMANDS_QRCODE 1
#else
#define AC_NUM_GLOB_COMMANDS_QRCODE 0
#endif /* QRCODE */
#define AC_NUM_GLOB_COMMANDS (17 + AC_NUM_GLOB_COMMANDS_AUDIO + AC_NUM_GLOB_COMMANDS_VIDEO + AC_NUM_GLOB_COMMANDS_PYTHON + AC_NUM_GLOB_COMMANDS_QRCODE)

/* Array of global command names used for tab completion. */
static const char glob_cmd_list[AC_NUM_GLOB_COMMANDS][MAX_CMDNAME_SIZE] = {
    { "/accept"     },
    { "/add"        },
    { "/avatar"     },
    { "/clear"      },
    { "/connect"    },
    { "/decline"    },
    { "/exit"       },
    { "/group"      },
    { "/help"       },
    { "/log"        },
    { "/myid"       },
#ifdef QRCODE
    { "/myqr"       },
#endif /* QRCODE */
    { "/nick"       },
    { "/note"       },
    { "/nospam"     },
    { "/quit"       },
    { "/requests"   },
    { "/status"     },

#ifdef AUDIO

    { "/lsdev"       },
    { "/sdev"        },

#endif /* AUDIO */

#ifdef VIDEO

    { "/lsvdev"      },
    { "/svdev"       },

#endif /* VIDEO */

#ifdef PYTHON

    { "/run"         },

#endif /* PYTHON */

};

void kill_prompt_window(ToxWindow *self)
{
    ChatContext *ctx = self->chatwin;
    StatusBar *statusbar = self->stb;

    log_disable(ctx->log);
    line_info_cleanup(ctx->hst);

    delwin(ctx->linewin);
    delwin(ctx->history);
    delwin(statusbar->topline);

    free(ctx->log);
    free(ctx);
    free(self->help);
    free(statusbar);

    del_window(self);
}

/* callback: Updates own connection status in prompt statusbar */
void on_self_connection_status(Tox *m, Tox_Connection connection_status, void *userdata)
{
    StatusBar *statusbar = prompt->stb;
    statusbar->connection = connection_status;
}

/* Updates own nick in prompt statusbar */
void prompt_update_nick(ToxWindow *prompt, const char *nick)
{
    StatusBar *statusbar = prompt->stb;
    snprintf(statusbar->nick, sizeof(statusbar->nick), "%s", nick);
    statusbar->nick_len = strlen(statusbar->nick);
}

/* Updates own statusmessage */
void prompt_update_statusmessage(ToxWindow *prompt, Tox *m, const char *statusmsg)
{
    StatusBar *statusbar = prompt->stb;
    snprintf(statusbar->statusmsg, sizeof(statusbar->statusmsg), "%s", statusmsg);
    size_t len = strlen(statusbar->statusmsg);
    statusbar->statusmsg_len = len;

    Tox_Err_Set_Info err;
    tox_self_set_status_message(m, (const uint8_t *) statusmsg, len, &err);

    if (err != TOX_ERR_SET_INFO_OK) {
        line_info_add(prompt, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to set note (error %d)\n", err);
    }
}

/* Updates own status in prompt statusbar */
void prompt_update_status(ToxWindow *prompt, Tox_User_Status status)
{
    StatusBar *statusbar = prompt->stb;
    statusbar->status = status;
}

/* Returns our own connection status */
Tox_Connection prompt_selfConnectionStatus(void)
{
    StatusBar *statusbar = prompt->stb;
    return statusbar->connection;
}

/* Adds friend request to pending friend requests.
   Returns request number on success, -1 if queue is full. */
static int add_friend_request(const char *public_key, const char *data)
{
    if (FrndRequests.max_idx >= MAX_FRIEND_REQUESTS) {
        return -1;
    }

    int i;

    for (i = 0; i <= FrndRequests.max_idx; ++i) {
        if (!FrndRequests.request[i].active) {
            FrndRequests.request[i].active = true;
            memcpy(FrndRequests.request[i].key, public_key, TOX_PUBLIC_KEY_SIZE);
            snprintf(FrndRequests.request[i].msg, sizeof(FrndRequests.request[i].msg), "%s", data);

            if (i == FrndRequests.max_idx) {
                ++FrndRequests.max_idx;
            }

            ++FrndRequests.num_requests;

            return i;
        }
    }

    return -1;
}

static void prompt_onKey(ToxWindow *self, Tox *m, wint_t key, bool ltr)
{
    ChatContext *ctx = self->chatwin;

    int x, y, y2, x2;
    getyx(self->window, y, x);
    getmaxyx(self->window, y2, x2);

    if (x2 <= 0 || y2 <= 0) {
        return;
    }

    if (ctx->pastemode && key == '\r') {
        key = '\n';
    }

    /* ignore non-menu related input if active */
    if (self->help->active) {
        help_onKey(self, key);
        return;
    }

    if (ltr || key == '\n') {    /* char is printable */
        input_new_char(self, key, x, y, x2, y2);
        return;
    }

    if (line_info_onKey(self, key)) {
        return;
    }

    input_handle(self, key, x, y, x2, y2);

    if (key == '\t') {    /* TAB key: auto-completes command */
        if (ctx->len > 1 && ctx->line[0] == '/') {
            int diff = -1;

            if (wcsncmp(ctx->line, L"/avatar ", wcslen(L"/avatar ")) == 0) {
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
                diff = complete_line(self, glob_cmd_list, AC_NUM_GLOB_COMMANDS, MAX_CMDNAME_SIZE);
            }

            if (diff != -1) {
                if (x + diff > x2 - 1) {
                    int wlen = MAX(0, wcswidth(ctx->line, sizeof(ctx->line) / sizeof(wchar_t)));
                    ctx->start = wlen < x2 ? 0 : wlen - x2 + 1;
                }
            } else {
                sound_notify(self, notif_error, 0, NULL);
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

            line_info_add(self, NULL, NULL, NULL, PROMPT, 0, 0, "%s", line);
            execute(ctx->history, self, m, line, GLOBAL_COMMAND_MODE);
        }

        wclear(ctx->linewin);
        wmove(self->window, y2 - CURS_Y_OFFSET, 0);
        reset_buf(ctx);
    }
}

static void prompt_onDraw(ToxWindow *self, Tox *m)
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

    StatusBar *statusbar = self->stb;

    mvwhline(statusbar->topline, 1, 0, ACS_HLINE, x2);
    wmove(statusbar->topline, 0, 0);

    pthread_mutex_lock(&Winthread.lock);
    Tox_Connection connection = statusbar->connection;
    pthread_mutex_unlock(&Winthread.lock);

    if (connection != TOX_CONNECTION_NONE) {
        int colour = MAGENTA;
        const char *status_text = "ERROR";

        pthread_mutex_lock(&Winthread.lock);
        Tox_User_Status status = statusbar->status;
        pthread_mutex_unlock(&Winthread.lock);

        switch (status) {
            case TOX_USER_STATUS_NONE:
                status_text = "Online";
                colour = GREEN;
                break;

            case TOX_USER_STATUS_AWAY:
                status_text = "Away";
                colour = YELLOW;
                break;

            case TOX_USER_STATUS_BUSY:
                status_text = "Busy";
                colour = RED;
                break;
        }

        wattron(statusbar->topline, COLOR_PAIR(colour) | A_BOLD);
        wprintw(statusbar->topline, " [%s]", status_text);
        wattroff(statusbar->topline, COLOR_PAIR(colour) | A_BOLD);

        wattron(statusbar->topline, A_BOLD);
        pthread_mutex_lock(&Winthread.lock);
        wprintw(statusbar->topline, " %s", statusbar->nick);
        pthread_mutex_unlock(&Winthread.lock);
        wattroff(statusbar->topline, A_BOLD);
    } else {
        wprintw(statusbar->topline, " [Offline]");
        wattron(statusbar->topline, A_BOLD);
        pthread_mutex_lock(&Winthread.lock);
        wprintw(statusbar->topline, " %s", statusbar->nick);
        pthread_mutex_unlock(&Winthread.lock);
        wattroff(statusbar->topline, A_BOLD);
    }

    /* Reset statusbar->statusmsg on window resize */
    if (x2 != self->x) {
        char statusmsg[TOX_MAX_STATUS_MESSAGE_LENGTH];

        pthread_mutex_lock(&Winthread.lock);
        size_t slen = tox_self_get_status_message_size(m);
        tox_self_get_status_message(m, (uint8_t *) statusmsg);
        statusmsg[slen] = '\0';
        snprintf(statusbar->statusmsg, sizeof(statusbar->statusmsg), "%s", statusmsg);
        statusbar->statusmsg_len = strlen(statusbar->statusmsg);
        pthread_mutex_unlock(&Winthread.lock);
    }

    self->x = x2;

    /* Truncate note if it doesn't fit in statusbar */
    uint16_t maxlen = x2 - getcurx(statusbar->topline) - 3;

    pthread_mutex_lock(&Winthread.lock);

    if (statusbar->statusmsg_len > maxlen) {
        statusbar->statusmsg[maxlen - 3] = '\0';
        strcat(statusbar->statusmsg, "...");
        statusbar->statusmsg_len = maxlen;
    }

    if (statusbar->statusmsg[0]) {
        wprintw(statusbar->topline, " : %s", statusbar->statusmsg);
    }

    pthread_mutex_unlock(&Winthread.lock);

    mvwhline(self->window, y2 - CHATBOX_HEIGHT, 0, ACS_HLINE, x2);

    int y, x;
    getyx(self->window, y, x);
    (void) x;

    int new_x = ctx->start ? x2 - 1 : MAX(0, wcswidth(ctx->line, ctx->pos));
    wmove(self->window, y + 1, new_x);

    wnoutrefresh(self->window);

    if (self->help->active) {
        help_onDraw(self);
    }
}

static void prompt_onConnectionChange(ToxWindow *self, Tox *m, uint32_t friendnum, Tox_Connection connection_status)
{
    ChatContext *ctx = self->chatwin;

    char nick[TOX_MAX_NAME_LENGTH] = {0};    /* stop removing this initiation */
    get_nick_truncate(m, nick, friendnum);

    if (!nick[0]) {
        snprintf(nick, sizeof(nick), "%s", UNKNOWN_NAME);
    }

    char timefrmt[TIME_STR_SIZE];
    get_time_str(timefrmt, sizeof(timefrmt));
    const char *msg;

    if (user_settings->show_connection_msg == SHOW_WELCOME_MSG_OFF) {
        return;
    }

    if (connection_status != TOX_CONNECTION_NONE && Friends.list[friendnum].connection_status == TOX_CONNECTION_NONE) {
        msg = "has come online";
        line_info_add(self, timefrmt, nick, NULL, CONNECTION, 0, GREEN, msg);
        write_to_log(msg, nick, ctx->log, true);

        if (self->active_box != -1) {
            box_notify2(self, user_log_in, NT_WNDALERT_2 | NT_NOTIFWND | NT_RESTOL, self->active_box,
                        "%s has come online", nick);
        } else {
            box_notify(self, user_log_in, NT_WNDALERT_2 | NT_NOTIFWND | NT_RESTOL, &self->active_box,
                       "Toxic", "%s has come online", nick);
        }
    } else if (connection_status == TOX_CONNECTION_NONE) {
        msg = "has gone offline";
        line_info_add(self, timefrmt, nick, NULL, DISCONNECTION, 0, RED, msg);
        write_to_log(msg, nick, ctx->log, true);

        if (self->active_box != -1) {
            box_notify2(self, user_log_out, NT_WNDALERT_2 | NT_NOTIFWND | NT_RESTOL, self->active_box,
                        "%s has gone offline", nick);
        } else {
            box_notify(self, user_log_out, NT_WNDALERT_2 | NT_NOTIFWND | NT_RESTOL, &self->active_box,
                       "Toxic", "%s has gone offline", nick);
        }
    }
}

static void prompt_onFriendRequest(ToxWindow *self, Tox *m, const char *key, const char *data, size_t length)
{
    ChatContext *ctx = self->chatwin;

    char timefrmt[TIME_STR_SIZE];
    get_time_str(timefrmt, sizeof(timefrmt));

    line_info_add(self, timefrmt, NULL, NULL, SYS_MSG, 0, 0, "Friend request with the message '%s'", data);
    write_to_log("Friend request with the message '%s'", "", ctx->log, true);

    int n = add_friend_request(key, data);

    if (n == -1) {
        const char *errmsg = "Friend request queue is full. Discarding request.";
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, errmsg);
        return;
    }

    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Type \"/accept %d\" or \"/decline %d\"", n, n);
    sound_notify(self, generic_message, NT_WNDALERT_1 | NT_NOTIFWND, NULL);
}

void prompt_init_statusbar(ToxWindow *self, Tox *m, bool first_time_run)
{
    int x2, y2;
    getmaxyx(self->window, y2, x2);

    if (y2 <= 0 || x2 <= 0) {
        exit_toxic_err("failed in prompt_init_statusbar", FATALERR_CURSES);
    }

    (void) y2;

    /* Init statusbar info */
    StatusBar *statusbar = self->stb;
    statusbar->status = TOX_USER_STATUS_NONE;
    statusbar->connection = TOX_CONNECTION_NONE;

    char nick[TOX_MAX_NAME_LENGTH];
    char statusmsg[TOX_MAX_STATUS_MESSAGE_LENGTH];

    size_t n_len = tox_self_get_name_size(m);
    tox_self_get_name(m, (uint8_t *) nick);

    size_t s_len = tox_self_get_status_message_size(m);
    tox_self_get_status_message(m, (uint8_t *) statusmsg);

    Tox_User_Status status = tox_self_get_status(m);

    nick[n_len] = '\0';
    statusmsg[s_len] = '\0';

    if (first_time_run) {
        snprintf(statusmsg, sizeof(statusmsg), "Toxing on Toxic");
        s_len = strlen(statusmsg);
        statusmsg[s_len] = '\0';
    }

    prompt_update_statusmessage(prompt, m, statusmsg);
    prompt_update_status(prompt, status);
    prompt_update_nick(prompt, nick);

    /* Init statusbar subwindow */
    statusbar->topline = subwin(self->window, 2, x2, 0, 0);
}

static void print_welcome_msg(ToxWindow *self)
{
    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 1, BLUE, "    _____ _____  _____ ____ ");
    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 1, BLUE, "   |_   _/ _ \\ \\/ /_ _/ ___|");
    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 1, BLUE, "     | || | | \\  / | | |    ");
    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 1, BLUE, "     | || |_| /  \\ | | |___ ");
    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 1, BLUE, "     |_| \\___/_/\\_\\___\\____| v." TOXICVER);
    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "");

    const char *msg = "Welcome to Toxic, a free, open source Tox-based instant messaging client.";
    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 1, CYAN, msg);
    msg = "Type \"/help\" for assistance. Further help may be found via the man page.";
    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 1, CYAN, msg);
    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "");
}

static void prompt_onInit(ToxWindow *self, Tox *m)
{
    curs_set(1);
    int y2, x2;
    getmaxyx(self->window, y2, x2);

    if (y2 <= 0 || x2 <= 0) {
        exit_toxic_err("failed in prompt_onInit", FATALERR_CURSES);
    }

    ChatContext *ctx = self->chatwin;
    ctx->history = subwin(self->window, y2 - CHATBOX_HEIGHT + 1, x2, 0, 0);
    ctx->linewin = subwin(self->window, CHATBOX_HEIGHT, x2, y2 - CHATBOX_HEIGHT, 0);

    ctx->log = calloc(1, sizeof(struct chatlog));
    ctx->hst = calloc(1, sizeof(struct history));

    if (ctx->log == NULL || ctx->hst == NULL) {
        exit_toxic_err("failed in prompt_onInit", FATALERR_MEMORY);
    }

    line_info_init(ctx->hst);

    if (user_settings->autolog == AUTOLOG_ON) {
        char myid[TOX_ADDRESS_SIZE];
        tox_self_get_address(m, (uint8_t *) myid);

        if (log_enable(self->name, myid, NULL, ctx->log, LOG_PROMPT) == -1) {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Warning: Log failed to initialize.");
        }
    }

    scrollok(ctx->history, 0);
    wmove(self->window, y2 - CURS_Y_OFFSET, 0);

    if (user_settings->show_welcome_msg == SHOW_WELCOME_MSG_ON) {
        print_welcome_msg(self);
    }
}

ToxWindow *new_prompt(void)
{
    ToxWindow *ret = calloc(1, sizeof(ToxWindow));

    if (ret == NULL) {
        exit_toxic_err("failed in new_prompt", FATALERR_MEMORY);
    }

    ret->num = -1;
    ret->is_prompt = true;

    ret->onKey = &prompt_onKey;
    ret->onDraw = &prompt_onDraw;
    ret->onInit = &prompt_onInit;
    ret->onConnectionChange = &prompt_onConnectionChange;
    ret->onFriendRequest = &prompt_onFriendRequest;

    strcpy(ret->name, "home");

    ChatContext *chatwin = calloc(1, sizeof(ChatContext));
    StatusBar *stb = calloc(1, sizeof(StatusBar));
    Help *help = calloc(1, sizeof(Help));

    if (stb == NULL || chatwin == NULL || help == NULL) {
        exit_toxic_err("failed in new_prompt", FATALERR_MEMORY);
    }

    ret->chatwin = chatwin;
    ret->stb = stb;
    ret->help = help;

    ret->active_box = -1;

    return ret;
}

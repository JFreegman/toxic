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

char pending_frnd_requests[MAX_FRIENDS_NUM][TOX_CLIENT_ID_SIZE];
uint16_t num_frnd_requests = 0;
extern ToxWindow *prompt;
struct _Winthread Winthread;

extern struct user_settings *user_settings_;

/* Array of global command names used for tab completion. */
const char glob_cmd_list[AC_NUM_GLOB_COMMANDS][MAX_CMDNAME_SIZE] = {
    { "/accept"     },
    { "/add"        },
    { "/clear"      },
    { "/close"      },    /* rm /close when groupchats gets its own list */
    { "/connect"    },
    { "/exit"       },
    { "/groupchat"  },
    { "/help"       },
    { "/log"        },
    { "/myid"       },
    { "/nick"       },
    { "/note"       },
    { "/quit"       },
    { "/status"     },

#ifdef _AUDIO

    { "/lsdev"       },
    { "/sdev"        },

#endif /* _AUDIO */
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
    free(ctx->hst);
    free(ctx);
    free(self->help);
    free(statusbar);

    del_window(self);
}

/* Updates own nick in prompt statusbar */
void prompt_update_nick(ToxWindow *prompt, char *nick)
{
    StatusBar *statusbar = prompt->stb;
    snprintf(statusbar->nick, sizeof(statusbar->nick), "%s", nick);
    statusbar->nick_len = strlen(statusbar->nick);
}

/* Updates own statusmessage in prompt statusbar */
void prompt_update_statusmessage(ToxWindow *prompt, char *statusmsg)
{
    StatusBar *statusbar = prompt->stb;
    snprintf(statusbar->statusmsg, sizeof(statusbar->statusmsg), "%s", statusmsg);
    statusbar->statusmsg_len = strlen(statusbar->statusmsg);
}

/* Updates own status in prompt statusbar */
void prompt_update_status(ToxWindow *prompt, uint8_t status)
{
    StatusBar *statusbar = prompt->stb;
    statusbar->status = status;
}

/* Updates own connection status in prompt statusbar */
void prompt_update_connectionstatus(ToxWindow *prompt, bool is_connected)
{
    StatusBar *statusbar = prompt->stb;
    statusbar->is_online = is_connected;
}

/* Adds friend request to pending friend requests.
   Returns request number on success, -1 if queue is full or other error. */
static int add_friend_request(const char *public_key)
{
    if (num_frnd_requests >= MAX_FRIENDS_NUM)
        return -1;

    int i;

    for (i = 0; i <= num_frnd_requests; ++i) {
        if (!strlen(pending_frnd_requests[i])) {
            memcpy(pending_frnd_requests[i], public_key, TOX_CLIENT_ID_SIZE);

            if (i == num_frnd_requests)
                ++num_frnd_requests;

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

    if (x2 <= 0)
        return;

    /* ignore non-menu related input if active */
    if (self->help->active) {
        help_onKey(self, key);
        return;
    }

    if (ltr) {    /* char is printable */
        input_new_char(self, key, x, y, x2, y2);
        return;
    }

    if (line_info_onKey(self, key))
        return;

    input_handle(self, key, x, y, x2, y2);

    if (key == '\t') {    /* TAB key: auto-completes command */
        if (ctx->len > 1 && ctx->line[0] == '/') {
            int diff = complete_line(self, glob_cmd_list, AC_NUM_GLOB_COMMANDS, MAX_CMDNAME_SIZE);

            if (diff != -1) {
                if (x + diff > x2 - 1) {
                    int wlen = wcswidth(ctx->line, sizeof(ctx->line));
                    ctx->start = wlen < x2 ? 0 : wlen - x2 + 1;
                }
            } else {
                beep();
            }
        } else {
            beep();
        }
    } else if (key == '\n') {
        rm_trailing_spaces_buf(ctx);

        char line[MAX_STR_SIZE] = {0};

        if (wcs_to_mbs_buf(line, ctx->line, MAX_STR_SIZE) == -1)
            memset(&line, 0, sizeof(line));

        if (!string_is_empty(line))
            add_line_to_hist(ctx);

        line_info_add(self, NULL, NULL, NULL, line, PROMPT, 0, 0);
        execute(ctx->history, self, m, line, GLOBAL_COMMAND_MODE);

        wclear(ctx->linewin);
        wmove(self->window, y2 - CURS_Y_OFFSET, 0);
        reset_buf(ctx);
    }
}

static void prompt_onDraw(ToxWindow *self, Tox *m)
{
    int x2, y2;
    getmaxyx(self->window, y2, x2);

    ChatContext *ctx = self->chatwin;

    line_info_print(self);
    wclear(ctx->linewin);

    curs_set(1);

    if (ctx->len > 0)
        mvwprintw(ctx->linewin, 1, 0, "%ls", &ctx->line[ctx->start]);

    StatusBar *statusbar = self->stb;
    mvwhline(statusbar->topline, 1, 0, ACS_HLINE, x2);
    wmove(statusbar->topline, 0, 0);

    if (statusbar->is_online) {
        int colour = WHITE;
        const char *status_text = "Unknown";

        switch (statusbar->status) {
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

        wattron(statusbar->topline, A_BOLD);
        wprintw(statusbar->topline, " %s", statusbar->nick);
        wattroff(statusbar->topline, A_BOLD);
    } else {
        wprintw(statusbar->topline, " [Offline]");
        wattron(statusbar->topline, A_BOLD);
        wprintw(statusbar->topline, " %s ", statusbar->nick);
        wattroff(statusbar->topline, A_BOLD);
    }

    if (statusbar->statusmsg[0])
        wprintw(statusbar->topline, " - %s", statusbar->statusmsg);

    mvwhline(self->window, y2 - CHATBOX_HEIGHT, 0, ACS_HLINE, x2);

    int y, x;
    getyx(self->window, y, x);
    (void) x;

    int new_x = ctx->start ? x2 - 1 : wcswidth(ctx->line, ctx->pos);
    wmove(self->window, y + 1, new_x);

    wrefresh(self->window);

    if (self->help->active)
        help_onDraw(self);
}

static void prompt_onConnectionChange(ToxWindow *self, Tox *m, int32_t friendnum , uint8_t status)
{
    if (friendnum < 0)
        return;

    ChatContext *ctx = self->chatwin;

    char nick[TOX_MAX_NAME_LENGTH] = {0};    /* stop removing this initiation */
    get_nick_truncate(m, nick, friendnum);

    if (!nick[0])
        snprintf(nick, sizeof(nick), "%s", UNKNOWN_NAME);

    char timefrmt[TIME_STR_SIZE];
    get_time_str(timefrmt, sizeof(timefrmt));
    char *msg;

    if (status == 1) {
        msg = "has come online";
        line_info_add(self, timefrmt, nick, NULL, msg, CONNECTION, 0, GREEN);
        write_to_log(msg, nick, ctx->log, true);
        notify(self, user_log_in, NT_WNDALERT_2 | NT_NOTIFWND | NT_RESTOL);
        
    } else {
        msg = "has gone offline";
        line_info_add(self, timefrmt, nick, NULL, msg, CONNECTION, 0, RED);
        write_to_log(msg, nick, ctx->log, true);
        notify(self, user_log_out, NT_WNDALERT_2 | NT_NOTIFWND | NT_RESTOL);
    }
}

static void prompt_onFriendRequest(ToxWindow *self, Tox *m, const char *key, const char *data,
                                   uint16_t length)
{
    ChatContext *ctx = self->chatwin;

    char timefrmt[TIME_STR_SIZE];
    get_time_str(timefrmt, sizeof(timefrmt));

    char msg[MAX_STR_SIZE];
    snprintf(msg, sizeof(msg), "Friend request with the message '%s'", data);
    line_info_add(self, timefrmt, NULL, NULL, msg, SYS_MSG, 0, 0);
    write_to_log(msg, "", ctx->log, true);

    int n = add_friend_request(key);

    if (n == -1) {
        char *errmsg = "Friend request queue is full. Discarding request.";
        line_info_add(self, NULL, NULL, NULL, errmsg, SYS_MSG, 0, 0);
        write_to_log(errmsg, "", ctx->log, true);
        return;
    }

    snprintf(msg, sizeof(msg), "Type \"/accept %d\" to accept it.", n);
    line_info_add(self, NULL, NULL, NULL, msg, SYS_MSG, 0, 0);
    notify(self, generic_message, NT_WNDALERT_1 | NT_NOTIFWND);
}

void prompt_init_statusbar(ToxWindow *self, Tox *m)
{
    int x2, y2;
    getmaxyx(self->window, y2, x2);
    (void) y2;

    /* Init statusbar info */
    StatusBar *statusbar = self->stb;
    statusbar->status = TOX_USERSTATUS_NONE;
    statusbar->is_online = false;

    char nick[TOX_MAX_NAME_LENGTH];
    char statusmsg[MAX_STR_SIZE];

    pthread_mutex_lock(&Winthread.lock);
    uint16_t n_len = tox_get_self_name(m, (uint8_t *) nick);
    uint16_t s_len = tox_get_self_status_message(m, (uint8_t *) statusmsg, MAX_STR_SIZE);
    uint8_t status = tox_get_self_user_status(m);
    pthread_mutex_unlock(&Winthread.lock);

    nick[n_len] = '\0';
    statusmsg[s_len] = '\0';

    /* load prev status message or show toxic version if it has never been set */
    char ver[strlen(TOXICVER) + 1];
    strcpy(ver, TOXICVER);
    const char *toxic_ver = strtok(ver, "_");

    if ( (!strcmp("Online", statusmsg) || !strncmp("Toxing on Toxic", statusmsg, 15)) && toxic_ver != NULL) {
        snprintf(statusmsg, MAX_STR_SIZE, "Toxing on Toxic v.%s", toxic_ver);
        s_len = strlen(statusmsg);
        statusmsg[s_len] = '\0';
    }

    prompt_update_statusmessage(prompt, statusmsg);
    prompt_update_status(prompt, status);
    prompt_update_nick(prompt, nick);

    /* Init statusbar subwindow */
    statusbar->topline = subwin(self->window, 2, x2, 0, 0);
}

static void print_welcome_msg(ToxWindow *self)
{
    line_info_add(self, NULL, NULL, NULL, "    _____ _____  _____ ____ ", SYS_MSG, 1, BLUE);
    line_info_add(self, NULL, NULL, NULL, "   |_   _/ _ \\ \\/ /_ _/ ___|", SYS_MSG, 1, BLUE);
    line_info_add(self, NULL, NULL, NULL, "     | || | | \\  / | | |    ", SYS_MSG, 1, BLUE);
    line_info_add(self, NULL, NULL, NULL, "     | || |_| /  \\ | | |___ ", SYS_MSG, 1, BLUE);
    line_info_add(self, NULL, NULL, NULL, "     |_| \\___/_/\\_\\___\\____|", SYS_MSG, 1, BLUE);
    line_info_add(self, NULL, NULL, NULL, "", SYS_MSG, 0, 0);

    char *msg = "Welcome to Toxic, a free, open source Tox-based instant messenging client.";
    line_info_add(self, NULL, NULL, NULL, msg, SYS_MSG, 1, CYAN);
    msg = "Type \"/help\" for assistance. Further help may be found via the man page.";
    line_info_add(self, NULL, NULL, NULL, msg, SYS_MSG, 1, CYAN);
    line_info_add(self, NULL, NULL, NULL, "", SYS_MSG, 0, 0);
}

static void prompt_onInit(ToxWindow *self, Tox *m)
{
    curs_set(1);
    int y2, x2;
    getmaxyx(self->window, y2, x2);

    ChatContext *ctx = self->chatwin;
    ctx->history = subwin(self->window, y2 - CHATBOX_HEIGHT + 1, x2, 0, 0);
    ctx->linewin = subwin(self->window, CHATBOX_HEIGHT, x2, y2 - CHATBOX_HEIGHT, 0);

    ctx->log = calloc(1, sizeof(struct chatlog));
    ctx->hst = calloc(1, sizeof(struct history));

    if (ctx->log == NULL || ctx->hst == NULL)
        exit_toxic_err("failed in prompt_onInit", FATALERR_MEMORY);

    line_info_init(ctx->hst);

    if (user_settings_->autolog == AUTOLOG_ON) {
        char myid[TOX_FRIEND_ADDRESS_SIZE];
        tox_get_address(m, (uint8_t *) myid);
        log_enable(self->name, myid, ctx->log);
    }

    scrollok(ctx->history, 0);
    wmove(self->window, y2 - CURS_Y_OFFSET, 0);

    print_welcome_msg(self);
}

ToxWindow new_prompt(void)
{
    ToxWindow ret;
    memset(&ret, 0, sizeof(ret));

    ret.active = true;
    ret.is_prompt = true;

    ret.onKey = &prompt_onKey;
    ret.onDraw = &prompt_onDraw;
    ret.onInit = &prompt_onInit;
    ret.onConnectionChange = &prompt_onConnectionChange;
    ret.onFriendRequest = &prompt_onFriendRequest;

    strcpy(ret.name, "home");

    ChatContext *chatwin = calloc(1, sizeof(ChatContext));
    StatusBar *stb = calloc(1, sizeof(StatusBar));
    Help *help = calloc(1, sizeof(Help));

    if (stb == NULL || chatwin == NULL || help == NULL)
        exit_toxic_err("failed in new_prompt", FATALERR_MEMORY);

    ret.chatwin = chatwin;
    ret.stb = stb;
    ret.help = help;

    return ret;
}

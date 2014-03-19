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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "toxic_windows.h"
#include "prompt.h"
#include "execute.h"
#include "misc_tools.h"
#include "toxic_strings.h"

uint8_t pending_frnd_requests[MAX_FRIENDS_NUM][TOX_CLIENT_ID_SIZE] = {0};
uint8_t num_frnd_requests = 0;
extern ToxWindow *prompt;
struct _Winthread Winthread;

/* Array of global command names used for tab completion. */
const uint8_t glob_cmd_list[AC_NUM_GLOB_COMMANDS][MAX_CMDNAME_SIZE] = {
    { "/accept"     },
    { "/add"        },
    { "/clear"      },
    { "/close"      },    /* rm /close when groupchats gets its own list */
    { "/connect"    },
    { "/exit"       },
    { "/groupchat"  },
    { "/help"       },
    { "/join"       },
    { "/log"        },
    { "/myid"       },
    { "/nick"       },
    { "/note"       },
    { "/quit"       },
    { "/status"     },
    
#ifdef _SUPPORT_AUDIO
    
    { "/lsdev"       },
    { "/sdev"        },
    
#endif /* _SUPPORT_AUDIO */
};

/* prevents input string from eating system messages: call this prior to printing a prompt message
   TODO: This is only a partial fix */
void prep_prompt_win(void)
{
    PromptBuf *prt = prompt->promptbuf;

    if (prt->len <= 0)
        return;

    wprintw(prompt->window, "\n");

    if (!prt->at_bottom) {
        wmove(prompt->window, prt->orig_y - 1, X_OFST);
        ++prt->orig_y;
    } else {
        wmove(prompt->window, prt->orig_y - 2, X_OFST);
    }
}

/* Updates own nick in prompt statusbar */
void prompt_update_nick(ToxWindow *prompt, uint8_t *nick, uint16_t len)
{
    StatusBar *statusbar = prompt->stb;
    snprintf(statusbar->nick, sizeof(statusbar->nick), "%s", nick);
    statusbar->nick_len = len;
}

/* Updates own statusmessage in prompt statusbar */
void prompt_update_statusmessage(ToxWindow *prompt, uint8_t *statusmsg, uint16_t len)
{
    StatusBar *statusbar = prompt->stb;
    snprintf(statusbar->statusmsg, sizeof(statusbar->statusmsg), "%s", statusmsg);
    statusbar->statusmsg_len = len;
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
static int add_friend_request(uint8_t *public_key)
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

static void prompt_onKey(ToxWindow *self, Tox *m, wint_t key)
{
    PromptBuf *prt = self->promptbuf;

    int x, y, y2, x2;
    getyx(self->window, y, x);
    getmaxyx(self->window, y2, x2);

    /* BACKSPACE key: Remove one character from line */
    if (key == 0x107 || key == 0x8 || key == 0x7f) {
        if (prt->pos > 0) {
            del_char_buf_bck(prt->line, &prt->pos, &prt->len);
            wmove(self->window, y, x-1);    /* not necessary but fixes a display glitch */
            prt->scroll = false;
        } else {
            beep();
        }
    }

    else if (key == KEY_DC) {      /* DEL key: Remove character at pos */
        if (prt->pos != prt->len) {
            del_char_buf_frnt(prt->line, &prt->pos, &prt->len);
            prt->scroll = false;
        } else {
            beep();
        }
    }

    else if (key == T_KEY_DISCARD) {    /* CTRL-U: Delete entire line behind pos */
        if (prt->pos > 0) {
            wmove(self->window, prt->orig_y, X_OFST);
            wclrtobot(self->window);
            discard_buf(prt->line, &prt->pos, &prt->len);
        } else {
            beep();
        }
    }

    else if (key == T_KEY_KILL) {    /* CTRL-K: Delete entire line in front of pos */
        if (prt->len != prt->pos)
            kill_buf(prt->line, &prt->pos, &prt->len);
        else
            beep();
    }

    else if (key == KEY_HOME || key == T_KEY_C_A) {  /* HOME/C-a key: Move cursor to start of line */
        if (prt->pos != 0)
            prt->pos = 0;
    }

    else if (key == KEY_END || key == T_KEY_C_E) {   /* END/C-e key: move cursor to end of line */
        if (prt->pos != prt->len)
            prt->pos = prt->len;
    }

    else if (key == KEY_LEFT) {
        if (prt->pos > 0)
            --prt->pos;
        else
            beep();
    } 

    else if (key == KEY_RIGHT) {
        if (prt->pos < prt->len)
            ++prt->pos;
        else 
            beep();
    } 

    else if (key == KEY_UP) {     /* fetches previous item in history */
        wmove(self->window, prt->orig_y, X_OFST);
        fetch_hist_item(prt->line, &prt->pos, &prt->len, prt->ln_history, prt->hst_tot,
                        &prt->hst_pos, LN_HIST_MV_UP);

        /* adjust line y origin appropriately when window scrolls down */
        if (prt->at_bottom && prt->len >= x2 - X_OFST) {
            int px2 = prt->len >= x2 ? x2 : x2 - X_OFST;
            int p_ofst = px2 != x2 ? 0 : X_OFST;

            if (px2 <= 0)
                return;

            int k = prt->orig_y + ((prt->len + p_ofst) / px2);

            if (k >= y2) {
                wprintw(self->window, "\n");
                --prt->orig_y;
            }
        }
    }

    else if (key == KEY_DOWN) {    /* fetches next item in history */
        wmove(self->window, prt->orig_y, X_OFST);
        fetch_hist_item(prt->line, &prt->pos, &prt->len, prt->ln_history, prt->hst_tot,
                        &prt->hst_pos, LN_HIST_MV_DWN);
    }

    else if (key == '\t') {    /* TAB key: completes command */
        if (prt->len > 1 && prt->line[0] == '/') {
            if (complete_line(prt->line, &prt->pos, &prt->len, glob_cmd_list, AC_NUM_GLOB_COMMANDS,
                              MAX_CMDNAME_SIZE) == -1) 
                beep();
        } else {
            beep();
        }
    }

    else
#if HAVE_WIDECHAR
    if (iswprint(key))
#else
    if (isprint(key))
#endif
    {
        if (prt->len < (MAX_STR_SIZE-1)) {
            add_char_to_buf(prt->line, &prt->pos, &prt->len, key);
            prt->scroll = true;
        }
    }
    /* RETURN key: execute command */
    else if (key == '\n') {
        wprintw(self->window, "\n");
        uint8_t line[MAX_STR_SIZE];

        if (wcs_to_mbs_buf(line, prt->line, MAX_STR_SIZE) == -1)
            memset(&line, 0, sizeof(line));

        if (!string_is_empty(line))
            add_line_to_hist(prt->line, prt->len, prt->ln_history, &prt->hst_tot, &prt->hst_pos);

        execute(self->window, self, m, line, GLOBAL_COMMAND_MODE);
        reset_buf(prt->line, &prt->pos, &prt->len);
    }
}

static void prompt_onDraw(ToxWindow *self, Tox *m)
{
    PromptBuf *prt = self->promptbuf;

    curs_set(1);
    int x, y, x2, y2;
    getyx(self->window, y, x);
    getmaxyx(self->window, y2, x2);
    wclrtobot(self->window);

    /* if len is >= screen width offset max x by X_OFST to account for prompt char */
    int px2 = prt->len >= x2 ? x2 : x2 - X_OFST;

    if (px2 <= 0)
        return;

    /* len offset to account for prompt char (0 if len is < width of screen) */
    int p_ofst = px2 != x2 ? 0 : X_OFST;

    if (prt->len > 0) {
        uint8_t line[MAX_STR_SIZE];

        if (wcs_to_mbs_buf(line, prt->line, MAX_STR_SIZE) == -1)
            reset_buf(prt->line, &prt->pos, &prt->len);
        else
            mvwprintw(self->window, prt->orig_y, X_OFST, line);

        int k = prt->orig_y + ((prt->len + p_ofst) / px2);

        prt->at_bottom = k == y2 - 1;
        bool botm = k == y2;
        bool edge = (prt->len + p_ofst) % px2 == 0;

        /* move point of line origin up when input scrolls screen down */
        if (prt->scroll && edge && botm) {
            --prt->orig_y;
            prt->scroll = false;
        }

    } else {    /* Mark point of origin for new line */
        prt->orig_y = y;    
    }

    wattron(self->window, COLOR_PAIR(GREEN));
    mvwprintw(self->window, prt->orig_y, 0, "$ ");
    wattroff(self->window, COLOR_PAIR(GREEN));

    StatusBar *statusbar = self->stb;
    werase(statusbar->topline);
    mvwhline(statusbar->topline, 1, 0, ACS_HLINE, x2);
    wmove(statusbar->topline, 0, 0);

    if (statusbar->is_online) {
        int colour = WHITE;
        const uint8_t *status_text = "Unknown";

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
        wprintw(statusbar->topline, "[Offline]");
        wattron(statusbar->topline, A_BOLD);
        wprintw(statusbar->topline, " %s ", statusbar->nick);
        wattroff(statusbar->topline, A_BOLD);
    }

    if (statusbar->statusmsg[0])
        wprintw(statusbar->topline, " - %s", statusbar->statusmsg);

    wprintw(statusbar->topline, "\n");

    /* put cursor back in correct spot */
    int y_m = prt->orig_y + ((prt->pos + p_ofst) / px2);
    int x_m = (prt->pos + X_OFST) % x2;
    wmove(self->window, y_m, x_m);
}

static void prompt_onInit(ToxWindow *self, Tox *m)
{
    scrollok(self->window, true);
    PromptBuf *prt = self->promptbuf;

    prt->log = malloc(sizeof(struct chatlog));

    if (prt->log == NULL) {
        endwin();
        fprintf(stderr, "malloc() failed. Aborting...\n");
        exit(EXIT_FAILURE);
    }

    memset(prt->log, 0, sizeof(struct chatlog));

    execute(self->window, self, m, "/help", GLOBAL_COMMAND_MODE);
    wclrtoeol(self->window);
}

static void prompt_onConnectionChange(ToxWindow *self, Tox *m, int32_t friendnum , uint8_t status)
{
    if (friendnum < 0)
        return;

    PromptBuf *prt = self->promptbuf;
    prep_prompt_win();

    uint8_t nick[TOX_MAX_NAME_LENGTH] = {'\0'};

    if (tox_get_name(m, friendnum, nick) == -1)
        return;

    if (!nick[0])
        snprintf(nick, sizeof(nick), "%s", UNKNOWN_NAME);

    wprintw(self->window, "\n");
    print_time(self->window);

    const uint8_t *msg;

    if (status == 1) {
        msg = "has come online";
        wattron(self->window, COLOR_PAIR(GREEN));
        wattron(self->window, A_BOLD);
        wprintw(self->window, "* %s ", nick);
        wattroff(self->window, A_BOLD);
        wprintw(self->window, "%s\n", msg);
        wattroff(self->window, COLOR_PAIR(GREEN));

        write_to_log(msg, nick, prt->log, true);
        alert_window(self, WINDOW_ALERT_2, false);
    } else {
        msg = "has gone offline";
        wattron(self->window, COLOR_PAIR(RED));
        wattron(self->window, A_BOLD);
        wprintw(self->window, "* %s ", nick);
        wattroff(self->window, A_BOLD);
        wprintw(self->window, "%s\n", msg);
        wattroff(self->window, COLOR_PAIR(RED));

        write_to_log(msg, nick, prt->log, true);
    }
}

static void prompt_onFriendRequest(ToxWindow *self, Tox *m, uint8_t *key, uint8_t *data, uint16_t length)
{
    /* make sure message data is null-terminated */
    data[length - 1] = 0;
    PromptBuf *prt = self->promptbuf;
    prep_prompt_win();

    wprintw(self->window, "\n");
    print_time(self->window);

    uint8_t msg[MAX_STR_SIZE];
    snprintf(msg, sizeof(msg), "Friend request with the message '%s'\n", data);
    wprintw(self->window, "%s", msg);
    write_to_log(msg, "", prt->log, true);

    int n = add_friend_request(key);

    if (n == -1) {
        const uint8_t *errmsg = "Friend request queue is full. Discarding request.\n";
        wprintw(self->window, "%s", errmsg);
        write_to_log(errmsg, "", prt->log, true);
        return;
    }

    wprintw(self->window, "Type \"/accept %d\" to accept it.\n", n);
    alert_window(self, WINDOW_ALERT_1, true);
}

void prompt_init_statusbar(ToxWindow *self, Tox *m)
{
    int x, y;
    getmaxyx(self->window, y, x);

    /* Init statusbar info */
    StatusBar *statusbar = self->stb;
    statusbar->status = TOX_USERSTATUS_NONE;
    statusbar->is_online = false;

    uint8_t nick[TOX_MAX_NAME_LENGTH] = {'\0'};
    uint8_t statusmsg[MAX_STR_SIZE];

    pthread_mutex_lock(&Winthread.lock);
    tox_get_self_name(m, nick);
    tox_get_self_status_message(m, statusmsg, MAX_STR_SIZE);
    uint8_t status = tox_get_self_user_status(m);
    pthread_mutex_unlock(&Winthread.lock);

    snprintf(statusbar->nick, sizeof(statusbar->nick), "%s", nick);

    /* load prev status message or show toxic version if it has never been set */
    uint8_t ver[strlen(TOXICVER) + 1];
    strcpy(ver, TOXICVER);
    const uint8_t *toxic_ver = strtok(ver, "_");

    if ( (!strcmp("Online", statusmsg) || !strncmp("Toxing on Toxic", statusmsg, 15)) && toxic_ver != NULL)
        snprintf(statusmsg, MAX_STR_SIZE, "Toxing on Toxic v.%s", toxic_ver);

    prompt_update_statusmessage(prompt, statusmsg, strlen(statusmsg) + 1);
    prompt_update_status(prompt, status);

    /* Init statusbar subwindow */
    statusbar->topline = subwin(self->window, 2, x, 0, 0);
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

    strcpy(ret.name, "prompt");

    PromptBuf *promptbuf = calloc(1, sizeof(PromptBuf));
    StatusBar *stb = calloc(1, sizeof(StatusBar));

    if (stb != NULL && promptbuf != NULL) {
        ret.promptbuf = promptbuf;
        ret.stb = stb;
    } else {
        endwin();
        fprintf(stderr, "calloc() failed. Aborting...\n");
        exit(EXIT_FAILURE);
    }

    return ret;
}

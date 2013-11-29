/*
 * Toxic -- Tox Curses Client
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

uint8_t pending_frnd_requests[MAX_FRIENDS_NUM][TOX_CLIENT_ID_SIZE] = {0};
uint8_t num_frnd_requests = 0;

static char prompt_buf[MAX_STR_SIZE] = {'\0'};
static int prompt_buf_pos = 0;

/* Updates own nick in prompt statusbar */
void prompt_update_nick(ToxWindow *prompt, uint8_t *nick, uint16_t len)
{
    StatusBar *statusbar = (StatusBar *) prompt->stb;
    snprintf(statusbar->nick, sizeof(statusbar->nick), "%s", nick);
    statusbar->nick_len = len;
}

/* Updates own statusmessage in prompt statusbar */
void prompt_update_statusmessage(ToxWindow *prompt, uint8_t *statusmsg, uint16_t len)
{
    StatusBar *statusbar = (StatusBar *) prompt->stb;
    snprintf(statusbar->statusmsg, sizeof(statusbar->statusmsg), "%s", statusmsg);
    statusbar->statusmsg_len = len;
}

/* Updates own status in prompt statusbar */
void prompt_update_status(ToxWindow *prompt, TOX_USERSTATUS status)
{
    StatusBar *statusbar = (StatusBar *) prompt->stb;
    statusbar->status = status;
}

/* Updates own connection status in prompt statusbar */
void prompt_update_connectionstatus(ToxWindow *prompt, bool is_connected)
{
    StatusBar *statusbar = (StatusBar *) prompt->stb;
    statusbar->is_online = is_connected;
}

/* Adds friend request to pending friend requests. 
   Returns request number on success, -1 if queue is full or other error. */
static int add_friend_request(uint8_t *public_key)
{
    if (num_frnd_requests < MAX_FRIENDS_NUM) {
        int i;

        for (i = 0; i <= num_frnd_requests; ++i) {
            if (!strlen(pending_frnd_requests[i])) {
                memcpy(pending_frnd_requests[i], public_key, TOX_CLIENT_ID_SIZE);

                if (i == num_frnd_requests)
                    ++num_frnd_requests;

                return i;
            }
        }
    }

    return -1;
}

static void prompt_onKey(ToxWindow *self, Tox *m, wint_t key)
{
    int x, y, y2, x2;
    getyx(self->window, y, x);
    getmaxyx(self->window, y2, x2);

    /* BACKSPACE key: Remove one character from line */
    if (key == 0x107 || key == 0x8 || key == 0x7f) {
        if (prompt_buf_pos != 0) {
            prompt_buf[--prompt_buf_pos] = '\0';

            if (x == 0)
                mvwdelch(self->window, y - 1, x2 - 1);
            else
                mvwdelch(self->window, y, x - 1);
        }
    }

    /* Add printable characters to line */
    else if (isprint(key)) {
        if (prompt_buf_pos < (MAX_STR_SIZE-1)) {
            mvwaddch(self->window, y, x, key);
            prompt_buf[prompt_buf_pos++] = key;
            prompt_buf[prompt_buf_pos] = '\0';
        }
    }

    /* RETURN key: execute command */
    else if (key == '\n') {
        wprintw(self->window, "\n");
        execute(self->window, self, m, prompt_buf, GLOBAL_COMMAND_MODE);
        prompt_buf_pos = 0;
        prompt_buf[0] = '\0';
    }

}

static void prompt_onDraw(ToxWindow *self, Tox *m)
{
    curs_set(1);

    int x, y, x2, y2;
    getyx(self->window, y, x);
    getmaxyx(self->window, y2, x2);

    /* Someone please fix this disgusting hack */
    size_t i;

    for (i = 0; i < prompt_buf_pos; ++i) {
        if ((prompt_buf_pos + 3) >= x2)
            --y;
    }

    StatusBar *statusbar = (StatusBar *) self->stb;
    werase(statusbar->topline);
    mvwhline(statusbar->topline, 1, 0, ACS_HLINE, x2);
    wmove(statusbar->topline, 0, 0);

    if (statusbar->is_online) {
        int colour = WHITE;
        char *status_text = "Unknown";

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
        }

        wattron(statusbar->topline, A_BOLD);
        wprintw(statusbar->topline, " %s ", statusbar->nick);
        wattron(statusbar->topline, A_BOLD);
        wattron(statusbar->topline, COLOR_PAIR(colour) | A_BOLD);
        wprintw(statusbar->topline, "[%s]", status_text);
        wattroff(statusbar->topline, COLOR_PAIR(colour) | A_BOLD);
    } else {
        wattron(statusbar->topline, A_BOLD);
        wprintw(statusbar->topline, " %s ", statusbar->nick);
        wattroff(statusbar->topline, A_BOLD);
        wprintw(statusbar->topline, "[Offline]");
    }

    wattron(statusbar->topline, A_BOLD);
    wprintw(statusbar->topline, " - %s", statusbar->statusmsg);
    wattroff(statusbar->topline, A_BOLD);

    wprintw(statusbar->topline, "\n");

    wattron(self->window, COLOR_PAIR(GREEN));
    mvwprintw(self->window, y, 0, "# ");
    wattroff(self->window, COLOR_PAIR(GREEN));
    mvwprintw(self->window, y, 2, "%s", prompt_buf);
    wclrtoeol(self->window);
    
    wrefresh(self->window);
}

static void prompt_onInit(ToxWindow *self, Tox *m)
{
    scrollok(self->window, true);
    execute(self->window, self, m, "/help", GLOBAL_COMMAND_MODE);
    wclrtoeol(self->window);
}

static void prompt_onConnectionChange(ToxWindow *self, Tox *m, int friendnum , uint8_t status)
{
    if (friendnum < 0)
        return;

    uint8_t nick[TOX_MAX_NAME_LENGTH] = {'\0'};

    if (tox_getname(m, friendnum, nick) == -1)
        return;

    if (!nick[0])
        snprintf(nick, sizeof(nick), "%s", UNKNOWN_NAME);

    if (status == 1) {
        wattron(self->window, COLOR_PAIR(GREEN));
        wattron(self->window, A_BOLD);
        wprintw(self->window, "\n%s ", nick);
        wattroff(self->window, A_BOLD);
        wprintw(self->window, "has come online\n");
        wattroff(self->window, COLOR_PAIR(GREEN));
    } else {
        wattron(self->window, COLOR_PAIR(RED));
        wattron(self->window, A_BOLD);
        wprintw(self->window, "\n%s ", nick);
        wattroff(self->window, A_BOLD);
        wprintw(self->window, "has gone offline\n");
        wattroff(self->window, COLOR_PAIR(RED));
    }

}

static void prompt_onFriendRequest(ToxWindow *self, uint8_t *key, uint8_t *data, uint16_t length)
{
    // make sure message data is null-terminated
    data[length - 1] = 0;

    wprintw(self->window, "\nFriend request with the message: %s\n", data);

    int n = add_friend_request(key);

    if (n == -1) {
        wprintw(self->window, "Friend request queue is full. Discarding request.\n");
        return;
    }

    wprintw(self->window, "Type \"/accept %d\" to accept it.\n", n);
    alert_window(self, WINDOW_ALERT_2, true);
}

void prompt_init_statusbar(ToxWindow *self, Tox *m)
{
    int x, y;
    getmaxyx(self->window, y, x);

    /* Init statusbar info */
    StatusBar *statusbar = (StatusBar *) self->stb;
    statusbar->status = TOX_USERSTATUS_NONE;
    statusbar->is_online = false;

    uint8_t nick[TOX_MAX_NAME_LENGTH] = {'\0'};
    tox_getselfname(m, nick, TOX_MAX_NAME_LENGTH);
    snprintf(statusbar->nick, sizeof(statusbar->nick), "%s", nick);

    /* temporary until statusmessage saving works */
    uint8_t *statusmsg = "Toxing on Toxic v.0.2.3";
    m_set_statusmessage(m, statusmsg, strlen(statusmsg) + 1);
    snprintf(statusbar->statusmsg, sizeof(statusbar->statusmsg), "%s", statusmsg);

    /* Init statusbar subwindow */
    statusbar->topline = subwin(self->window, 2, x, 0, 0);
}

ToxWindow new_prompt(void)
{
    ToxWindow ret;
    memset(&ret, 0, sizeof(ret));

    ret.onKey = &prompt_onKey;
    ret.onDraw = &prompt_onDraw;
    ret.onInit = &prompt_onInit;
    ret.onConnectionChange = &prompt_onConnectionChange;
    ret.onFriendRequest = &prompt_onFriendRequest;

    strcpy(ret.name, "prompt");

    StatusBar *stb = calloc(1, sizeof(StatusBar));

    if (stb != NULL)
        ret.stb = stb;
    else {
        endwin();
        fprintf(stderr, "calloc() failed. Aborting...\n");
        exit(EXIT_FAILURE);
    }

    return ret;
}

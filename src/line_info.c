/*  line_info.c
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
#include <stdbool.h>
#include <stdarg.h>

#include "toxic.h"
#include "windows.h"
#include "line_info.h"
#include "groupchat.h"
#include "settings.h"
#include "notify.h"
#include "message_queue.h"
#include "misc_tools.h"

extern struct user_settings *user_settings;

void line_info_init(struct history *hst)
{
    hst->line_root = calloc(1, sizeof(struct line_info));

    if (hst->line_root == NULL)
        exit_toxic_err("failed in line_info_init", FATALERR_MEMORY);

    hst->line_start = hst->line_root;
    hst->line_end = hst->line_start;
    hst->queue_sz = 0;
}

/* resets line_start (moves to end of chat history) */
void line_info_reset_start(ToxWindow *self, struct history *hst)
{
    struct line_info *line = hst->line_end;

    if (line->prev == NULL)
        return;

    int y2, x2;
    getmaxyx(self->window, y2, x2);

    int side_offst = self->show_peerlist ? SIDEBAR_WIDTH : 0;
    int top_offst = self->is_chat || self->is_prompt ? 2 : 0;
    int max_y = (y2 - CHATBOX_HEIGHT - top_offst);

    int curlines = 0;
    int nxtlines = line->newlines + (line->len / (x2 - side_offst));

    do {
        curlines += 1 + nxtlines;
        line = line->prev;
        nxtlines = line->newlines + (line->len / (x2 - side_offst));
    } while (line->prev && curlines + nxtlines < max_y);

    hst->line_start = line;
}

void line_info_cleanup(struct history *hst)
{
    struct line_info *tmp1 = hst->line_root;

    while (tmp1) {
        struct line_info *tmp2 = tmp1->next;
        free(tmp1);
        tmp1 = tmp2;
    }

    int i;

    for (i = 0; i < hst->queue_sz; ++i) {
        if (hst->queue[i])
            free(hst->queue[i]);
    }

    free(hst);
}

/* moves root forward and frees previous root */
static void line_info_root_fwd(struct history *hst)
{
    struct line_info *tmp = hst->line_root->next;
    tmp->prev = NULL;

    if (hst->line_start->prev == NULL) {    /* if line_start is root move it forward as well */
        hst->line_start = hst->line_start->next;
        hst->line_start->prev = NULL;
        ++hst->start_id;
    }

    free(hst->line_root);
    hst->line_root = tmp;
}

/* returns ptr to queue item 0 and removes it from queue. Returns NULL if queue is empty. */
static struct line_info *line_info_ret_queue(struct history *hst)
{
    if (hst->queue_sz <= 0)
        return NULL;

    struct line_info *line = hst->queue[0];

    int i;

    for (i = 0; i < hst->queue_sz; ++i)
        hst->queue[i] = hst->queue[i + 1];

    --hst->queue_sz;

    return line;
}

/* creates new line_info line and puts it in the queue. */
void line_info_add(ToxWindow *self, const char *timestr, const char *name1, const char *name2, uint8_t type,
                   uint8_t bold, uint8_t colour, const char *msg, ...)
{
    struct history *hst = self->chatwin->hst;

    if (hst->queue_sz >= MAX_LINE_INFO_QUEUE)
        return;

    struct line_info *new_line = calloc(1, sizeof(struct line_info));

    if (new_line == NULL)
        exit_toxic_err("failed in line_info_add", FATALERR_MEMORY);

    char frmt_msg[MAX_LINE_INFO_MSG_SIZE] = {0};

    va_list args;
    va_start(args, msg);
    vsnprintf(frmt_msg, sizeof(frmt_msg), msg, args);
    va_end(args);

    int len = 1;     /* there will always be a newline */

    /* for type-specific formatting in print function */
    switch (type) {
        case IN_ACTION:
        case OUT_ACTION:
            len += strlen(user_settings->line_normal) + 2;
            break;

        case IN_MSG:
        case OUT_MSG:
            len += strlen(user_settings->line_normal) + 3;
            break;

        case CONNECTION:
            len += strlen(user_settings->line_join) + 2;
            break;

        case DISCONNECTION:
            len += strlen(user_settings->line_quit) + 2;
            break;

        case SYS_MSG:
            break;

        case NAME_CHANGE:
            len += strlen(user_settings->line_alert) + 1;
            break;

        case PROMPT:
            ++len;
            break;

        default:
            len += 2;
            break;
    }

    if (frmt_msg[0]) {
        snprintf(new_line->msg, sizeof(new_line->msg), "%s", frmt_msg);
        len += strlen(new_line->msg);

        int i;

        for (i = 0; frmt_msg[i]; ++i) {
            if (frmt_msg[i] == '\n')
                ++new_line->newlines;
        }
    }

    if (timestr) {
        snprintf(new_line->timestr, sizeof(new_line->timestr), "%s", timestr);
        len += strlen(new_line->timestr) + 1;
    }

    if (name1) {
        snprintf(new_line->name1, sizeof(new_line->name1), "%s", name1);
        len += strlen(new_line->name1);
    }

    if (name2) {
        snprintf(new_line->name2, sizeof(new_line->name2), "%s", name2);
        len += strlen(new_line->name2);
    }

    new_line->len = len;
    new_line->type = type;
    new_line->bold = bold;
    new_line->colour = colour;
    new_line->noread_flag = false;
    new_line->timestamp = get_unix_time();

    hst->queue[hst->queue_sz++] = new_line;
}

/* adds a single queue item to hst if possible. only called once per call to line_info_print() */
static void line_info_check_queue(ToxWindow *self)
{
    struct history *hst = self->chatwin->hst;
    struct line_info *line = line_info_ret_queue(hst);

    if (line == NULL)
        return;

    if (hst->start_id > user_settings->history_size)
        line_info_root_fwd(hst);

    line->id = hst->line_end->id + 1;
    line->prev = hst->line_end;
    hst->line_end->next = line;
    hst->line_end = line;

    int y, y2, x, x2;
    getmaxyx(self->window, y2, x2);
    getyx(self->chatwin->history, y, x);
    (void) x;

    if (x2 <= SIDEBAR_WIDTH)
        return;

    int offst = self->show_peerlist ? SIDEBAR_WIDTH : 0;   /* offset width of groupchat sidebar */
    int lines = 1 + line->newlines + (line->len / (x2 - offst));
    int max_y = y2 - CHATBOX_HEIGHT;

    /* move line_start forward proportionate to the number of new lines */
    if (y + lines - 1 >= max_y) {
        while (lines > 0 && hst->line_start->next) {
            lines -= 1 + hst->line_start->next->newlines + (hst->line_start->next->len / (x2 - offst));
            hst->line_start = hst->line_start->next;
            ++hst->start_id;
        }
    }
}

#define NOREAD_FLAG_TIMEOUT 5    /* seconds before a sent message with no read receipt is flagged as unread */

void line_info_print(ToxWindow *self)
{
    ChatContext *ctx = self->chatwin;

    if (ctx == NULL)
        return;

    struct history *hst = ctx->hst;

    /* Only allow one new item to be added to chat window per call to this function */
    line_info_check_queue(self);

    WINDOW *win = ctx->history;
    wclear(win);
    int y2, x2;
    getmaxyx(self->window, y2, x2);

    if (x2 <= SIDEBAR_WIDTH)
        return;

    if (self->is_groupchat)
        wmove(win, 0, 0);
    else
        wmove(win, 2, 0);

    struct line_info *line = hst->line_start->next;
    int numlines = 0;

    while (line && numlines++ <= y2) {
        uint8_t type = line->type;

        switch (type) {
            case OUT_MSG:
            case OUT_MSG_READ:
            case IN_MSG:
                wattron(win, COLOR_PAIR(BLUE));
                wprintw(win, "%s ", line->timestr);
                wattroff(win, COLOR_PAIR(BLUE));

                int nameclr = GREEN;

                if (line->colour)
                    nameclr = line->colour;
                else if (type == IN_MSG)
                    nameclr = CYAN;

                wattron(win, COLOR_PAIR(nameclr));
                wprintw(win, "%s %s: ", user_settings->line_normal, line->name1);
                wattroff(win, COLOR_PAIR(nameclr));

                if (line->msg[0] == '>')
                    wattron(win, COLOR_PAIR(GREEN));

                wprintw(win, "%s", line->msg);

                if (line->msg[0] == '>')
                    wattroff(win, COLOR_PAIR(GREEN));

                if (type == OUT_MSG && timed_out(line->timestamp, get_unix_time(), NOREAD_FLAG_TIMEOUT)) {
                    wattron(win, COLOR_PAIR(RED));
                    wprintw(win, " x", line->msg);
                    wattroff(win, COLOR_PAIR(RED));

                    if (line->noread_flag == false) {
                        line->noread_flag = true;
                        line->len += 2;
                    }
                }

                wprintw(win, "\n", line->msg);
                break;

            case OUT_ACTION_READ:
            case OUT_ACTION:
            case IN_ACTION:
                wattron(win, COLOR_PAIR(BLUE));
                wprintw(win, "%s ", line->timestr);
                wattroff(win, COLOR_PAIR(BLUE));

                wattron(win, COLOR_PAIR(YELLOW));
                wprintw(win, "%s %s %s", user_settings->line_normal, line->name1, line->msg);
                wattroff(win, COLOR_PAIR(YELLOW));

                if (type == OUT_ACTION && timed_out(line->timestamp, get_unix_time(), NOREAD_FLAG_TIMEOUT)) {
                    wattron(win, COLOR_PAIR(RED));
                    wprintw(win, " x", line->msg);
                    wattroff(win, COLOR_PAIR(RED));

                    if (line->noread_flag == false) {
                        line->noread_flag = true;
                        line->len += 2;
                    }
                }

                wprintw(win, "\n", line->msg);
                break;

            case SYS_MSG:
                if (line->timestr[0]) {
                    wattron(win, COLOR_PAIR(BLUE));
                    wprintw(win, "%s ", line->timestr);
                    wattroff(win, COLOR_PAIR(BLUE));
                }

                if (line->bold)
                    wattron(win, A_BOLD);

                if (line->colour)
                    wattron(win, COLOR_PAIR(line->colour));

                wprintw(win, "%s\n", line->msg);

                if (line->bold)
                    wattroff(win, A_BOLD);

                if (line->colour)
                    wattroff(win, COLOR_PAIR(line->colour));

                break;

            case PROMPT:
                wattron(win, COLOR_PAIR(GREEN));
                wprintw(win, "$ ");
                wattroff(win, COLOR_PAIR(GREEN));

                if (line->msg[0])
                    wprintw(win, "%s", line->msg);

                wprintw(win, "\n");
                break;

            case CONNECTION:
                wattron(win, COLOR_PAIR(BLUE));
                wprintw(win, "%s ", line->timestr);
                wattroff(win, COLOR_PAIR(BLUE));

                wattron(win, COLOR_PAIR(line->colour));
                wprintw(win, "%s ", user_settings->line_join);

                wattron(win, A_BOLD);
                wprintw(win, "%s ", line->name1);
                wattroff(win, A_BOLD);

                wprintw(win, "%s\n", line->msg);
                wattroff(win, COLOR_PAIR(line->colour));

                break;

            case DISCONNECTION:
                wattron(win, COLOR_PAIR(BLUE));
                wprintw(win, "%s ", line->timestr);
                wattroff(win, COLOR_PAIR(BLUE));

                wattron(win, COLOR_PAIR(line->colour));
                wprintw(win, "%s ", user_settings->line_quit);

                wattron(win, A_BOLD);
                wprintw(win, "%s ", line->name1);
                wattroff(win, A_BOLD);

                wprintw(win, "%s\n", line->msg);
                wattroff(win, COLOR_PAIR(line->colour));

                break;

            case NAME_CHANGE:
                wattron(win, COLOR_PAIR(BLUE));
                wprintw(win, "%s ", line->timestr);
                wattroff(win, COLOR_PAIR(BLUE));

                wattron(win, COLOR_PAIR(MAGENTA));
                wprintw(win, "%s ", user_settings->line_alert);
                wattron(win, A_BOLD);
                wprintw(win, "%s", line->name1);
                wattroff(win, A_BOLD);

                wprintw(win, "%s", line->msg);

                wattron(win, A_BOLD);
                wprintw(win, "%s\n", line->name2);
                wattroff(win, A_BOLD);
                wattroff(win, COLOR_PAIR(MAGENTA));

                break;
        }

        line = line->next;
    }

    /* keep calling until queue is empty */
    if (hst->queue_sz > 0)
        line_info_print(self);
}

/* puts msg in specified line_info msg buffer */
void line_info_set(ToxWindow *self, uint32_t id, char *msg)
{
    struct line_info *line = self->chatwin->hst->line_end;

    while (line) {
        if (line->id == id) {
            snprintf(line->msg, sizeof(line->msg), "%s", msg);
            return;
        }

        line = line->prev;
    }
}

/* static void line_info_goto_root(struct history *hst)
{
    hst->line_start = hst->line_root;
} */

static void line_info_scroll_up(struct history *hst)
{
    if (hst->line_start->prev)
        hst->line_start = hst->line_start->prev;
    else sound_notify(NULL, error, NT_ALWAYS, NULL);
}

static void line_info_scroll_down(struct history *hst)
{
    if (hst->line_start->next)
        hst->line_start = hst->line_start->next;
    else sound_notify(NULL, error, NT_ALWAYS, NULL);
}

static void line_info_page_up(ToxWindow *self, struct history *hst)
{
    int x2, y2;
    getmaxyx(self->window, y2, x2);
    (void) x2;
    int jump_dist = y2 / 2;
    int i;

    for (i = 0; i < jump_dist && hst->line_start->prev; ++i)
        hst->line_start = hst->line_start->prev;
}

static void line_info_page_down(ToxWindow *self, struct history *hst)
{
    int x2, y2;
    getmaxyx(self->window, y2, x2);
    (void) x2;
    int jump_dist = y2 / 2;
    int i;

    for (i = 0; i < jump_dist && hst->line_start->next; ++i)
        hst->line_start = hst->line_start->next;
}

bool line_info_onKey(ToxWindow *self, wint_t key)
{
    struct history *hst = self->chatwin->hst;
    bool match = true;

    if (key == user_settings->key_half_page_up) {
        line_info_page_up(self, hst);
    }
    else if (key == user_settings->key_half_page_down) {
        line_info_page_down(self, hst);
    }
    else if (key == user_settings->key_scroll_line_up) {
        line_info_scroll_up(hst);
    }
    else if (key == user_settings->key_scroll_line_down) {
        line_info_scroll_down(hst);
    }
    else if (key == user_settings->key_page_bottom) {
        line_info_reset_start(self, hst);
    }
    else {
        match = false;
    }

    return match;
}

void line_info_clear(struct history *hst)
{
    hst->line_start = hst->line_end;
    hst->start_id = hst->line_start->id;
}

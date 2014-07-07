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

#include "toxic.h"
#include "windows.h"
#include "line_info.h"
#include "groupchat.h"
#include "settings.h"

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

/* resets line_start (page end) */
static void line_info_reset_start(ToxWindow *self, struct history *hst)
{
    struct line_info *line = hst->line_end;

    if (line->prev == NULL)
        return;

    int y2, x2;
    getmaxyx(self->window, y2, x2);

    int side_offst = self->is_groupchat ? SIDEBAR_WIDTH : 0;
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

/* adds a line_info line to queue */
static void line_info_add_queue(struct history *hst, struct line_info *line)
{
    if (hst->queue_sz >= MAX_QUEUE)
        return;

    hst->queue[hst->queue_sz++] = line;
}

/* returns ptr to queue item 0 and removes it from queue */
static struct line_info *line_info_ret_queue(struct history *hst)
{
    if (hst->queue_sz <= 0)
        return NULL;

    struct line_info *ret = hst->queue[0];

    int i;

    for (i = 0; i < hst->queue_sz; ++i)
        hst->queue[i] = hst->queue[i + 1];

    --hst->queue_sz;

    return ret;
}

/* creates new line_info line and puts it in the queue */
void line_info_add(ToxWindow *self, char *tmstmp, char *name1, char *name2, const char *msg, uint8_t type, 
                   uint8_t bold, uint8_t colour)
{
    struct history *hst = self->chatwin->hst;
    struct line_info *new_line = calloc(1, sizeof(struct line_info));

    if (new_line == NULL)
        exit_toxic_err("failed in line_info_add", FATALERR_MEMORY);

    int len = 1;     /* there will always be a newline */

    /* for type-specific formatting in print function */
    switch (type) {
        case ACTION:
        case CONNECTION:
            len += 3;
            break;

        case SYS_MSG:
            break;

        case PROMPT:
            ++len;
            break;

        default:
            len += 2;
            break;
    }

    if (msg) {
        snprintf(new_line->msg, sizeof(new_line->msg), "%s", msg);
        len += strlen(new_line->msg);

        int i;

        for (i = 0; msg[i]; ++i) {
            if (msg[i] == '\n')
                ++new_line->newlines;
        }
    }

    if (tmstmp) {
        snprintf(new_line->timestamp, sizeof(new_line->timestamp), "%s", tmstmp);
        len += strlen(new_line->timestamp);
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

    line_info_add_queue(hst, new_line);
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

    int offst = self->is_groupchat ? SIDEBAR_WIDTH : 0;   /* offset width of groupchat sidebar */
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
            case IN_MSG:
                wattron(win, COLOR_PAIR(BLUE));
                wprintw(win, "%s", line->timestamp);
                wattroff(win, COLOR_PAIR(BLUE));

                int nameclr = GREEN;

                if (line->colour)
                    nameclr = line->colour;
                else if (type == IN_MSG)
                    nameclr = CYAN;

                wattron(win, COLOR_PAIR(nameclr));
                wprintw(win, "%s: ", line->name1);
                wattroff(win, COLOR_PAIR(nameclr));

                if (line->msg[0] == '>')
                    wattron(win, COLOR_PAIR(GREEN));

                wprintw(win, "%s\n", line->msg);

                if (line->msg[0] == '>')
                    wattroff(win, COLOR_PAIR(GREEN));

                break;

            case ACTION:
                wattron(win, COLOR_PAIR(BLUE));
                wprintw(win, "%s", line->timestamp);
                wattroff(win, COLOR_PAIR(BLUE));

                wattron(win, COLOR_PAIR(YELLOW));
                wprintw(win, "* %s %s\n", line->name1, line->msg);
                wattroff(win, COLOR_PAIR(YELLOW));

                break;

            case SYS_MSG:
                if (line->timestamp[0]) {
                    wattron(win, COLOR_PAIR(BLUE));
                    wprintw(win, "%s", line->timestamp);
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
                wprintw(win, "%s", line->timestamp);
                wattroff(win, COLOR_PAIR(BLUE));

                wattron(win, COLOR_PAIR(line->colour));
                wattron(win, A_BOLD);
                wprintw(win, "* %s ", line->name1);
                wattroff(win, A_BOLD);
                wprintw(win, "%s\n", line->msg);
                wattroff(win, COLOR_PAIR(line->colour));

                break;

            case NAME_CHANGE:
                wattron(win, COLOR_PAIR(BLUE));
                wprintw(win, "%s", line->timestamp);
                wattroff(win, COLOR_PAIR(BLUE));

                wattron(win, COLOR_PAIR(MAGENTA));
                wattron(win, A_BOLD);
                wprintw(win, "* %s", line->name1);
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
    else beep();
}

static void line_info_scroll_down(struct history *hst)
{
    if (hst->line_start->next)
        hst->line_start = hst->line_start->next;
    else beep();
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

    switch (key) {
        /* TODO: Find good key bindings for all this stuff */
        case T_KEY_C_F:
            line_info_page_up(self, hst);
            break;

        case T_KEY_C_V:
            line_info_page_down(self, hst);
            break; 

        case KEY_PPAGE:
            line_info_scroll_up(hst);
            break;

        case KEY_NPAGE:
            line_info_scroll_down(hst);
            break;

        /* case ?:
            line_info_goto_root(hst);
            break; */

        case T_KEY_C_H:
            line_info_reset_start(self, hst);
            break; 

        default:
            match = false;
            break;
    }

    return match;
}

void line_info_clear(struct history *hst)
{
    hst->line_start = hst->line_end;
    hst->start_id = hst->line_start->id;
}

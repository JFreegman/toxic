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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "toxic_windows.h"
#include "line_info.h"
#include "groupchat.h"
#include "settings.h"

extern struct user_settings *user_settings;

void line_info_init(struct history *hst)
{
    hst->line_root = malloc(sizeof(struct line_info));

    if (hst->line_root == NULL) {
        endwin();
        fprintf(stderr, "malloc() failed. Aborting...\n");
        exit(EXIT_FAILURE);
    }

    memset(hst->line_root, 0, sizeof(struct line_info));
    hst->line_start = hst->line_root;
    hst->line_end = hst->line_start;
}

/* resets line_start when scroll mode is disabled */
static void line_info_reset_start(struct history *hst)
{
    struct line_info *line = hst->line_end;
    uint32_t start_id = hst->start_id;

    while (line) {
        if (line->id == start_id) {
            hst->line_start = line;
            break;
        }

        line = line->prev;
    }
}

void line_info_toggle_scroll(ToxWindow *self, bool scroll)
{
    WINDOW *win = self->chatwin->history;
    struct history *hst = self->chatwin->hst;

    if (scroll) {
        hst->scroll_mode = true;
        scrollok(win, 0);
        curs_set(0);
    } else {
        hst->scroll_mode = false;
        scrollok(win, 1);
        curs_set(1);
        line_info_reset_start(hst);
    }
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

void line_info_add(ToxWindow *self, uint8_t *tmstmp, uint8_t *name1, uint8_t *name2, uint8_t *msg,
                   uint8_t type, uint8_t bold, uint8_t colour)
{
    struct history *hst = self->chatwin->hst;
    struct line_info *new_line = malloc(sizeof(struct line_info));

    if (new_line == NULL) {
        endwin();
        fprintf(stderr, "malloc() failed. Aborting...\n");
        exit(EXIT_FAILURE);
    }

    memset(new_line, 0, sizeof(struct line_info));

    int len = 1;     /* there will always be a newline */

    /* for type-specific formatting in print function */
    switch (type) {
        case ACTION:
            len += 3;
            break;

        default:
            len += 2;
            break;
    }

    if (msg) {
        strcpy(new_line->msg, msg);
        len += strlen(msg);
    }

    if (tmstmp) {
        strcpy(new_line->timestamp, tmstmp);
        len += strlen(tmstmp);
    }

    if (name1) {
        strcpy(new_line->name1, name1);
        len += strlen(name1);
    }

    if (name2) {
        strcpy(new_line->name2, name2);
        len += strlen(name2);
    }

    new_line->len = len;
    new_line->type = type;
    new_line->bold = bold;
    new_line->colour = colour;
    new_line->id = hst->line_end->id + 1;

    new_line->prev = hst->line_end;
    hst->line_end->next = new_line;
    hst->line_end = new_line;

    if (++hst->line_items > user_settings->history_size) {
        --hst->line_items;
        line_info_root_fwd(hst);
    }

    int newlines = 0;
    int i;

    for (i = 0; msg[i]; ++i) {
        if (msg[i] == '\n')
            ++newlines;
    }

    int y, y2, x, x2;
    getmaxyx(self->window, y2, x2);
    getyx(self->chatwin->history, y, x);

    if (x2 <= SIDEBAR_WIDTH)
        return;

    int offst = self->is_groupchat ? SIDEBAR_WIDTH : 0;   /* offset width of groupchat sidebar */
    int lines = (1 + newlines + (len / (x2 - offst)));
    hst->queue_lns += lines;
    ++hst->queue;

    int max_y = self->is_prompt ? y2 : y2 - CHATBOX_HEIGHT;

    /* move line_start forward proportionate to the number of new lines */
    if (y + hst->queue_lns - hst->queue >= max_y) {
        while (lines > 0) {
            ++hst->start_id;
            lines -= (1 + hst->line_start->len / (x2 - offst));

            if (!hst->scroll_mode && hst->line_start->next)
                hst->line_start = hst->line_start->next;
        }
    }
}

void line_info_print(ToxWindow *self)
{
    ChatContext *ctx = self->chatwin;

    if (ctx == NULL)
        return;

    WINDOW *win = ctx->history;

    ctx->hst->queue = 0;
    ctx->hst->queue_lns = 0;

    wclear(win);
    int y2, x2;
    getmaxyx(self->window, y2, x2);

    if (self->is_prompt)
        y2 = user_settings->history_size;   /* temporary fix to make prompt scroll */

    if (x2 <= SIDEBAR_WIDTH)
        return;

    if (self->is_groupchat)
        wmove(win, 0, 0);
    else
        wmove(win, 2, 0);

    struct line_info *line = ctx->hst->line_start->next;
    int offst = self->is_groupchat ? SIDEBAR_WIDTH : 0;
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
}

void line_info_set(ToxWindow *self, uint32_t id, uint8_t *msg)
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

static void line_info_goto_root(struct history *hst)
{
    hst->line_start = hst->line_root;
}

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
    int jump_dist = y2 / 2;
    int i;

    for (i = 0; i < jump_dist && hst->line_start->prev; ++i)
        hst->line_start = hst->line_start->prev;
}

static void line_info_page_down(ToxWindow *self, struct history *hst)
{
    int x2, y2;
    getmaxyx(self->window, y2, x2);
    int jump_dist = y2 / 2;
    int i;

    for (i = 0; i < jump_dist && hst->line_start->next; ++i)
        hst->line_start = hst->line_start->next;
}

void line_info_onKey(ToxWindow *self, wint_t key)
{
    struct history *hst = self->chatwin->hst;

    switch (key) {
        case KEY_PPAGE:
            line_info_page_up(self, hst);
            break;

        case KEY_NPAGE:
            line_info_page_down(self, hst);
            break;

        case KEY_UP:
            line_info_scroll_up(hst);
            break;

        case KEY_DOWN:
            line_info_scroll_down(hst);
            break;

        case KEY_HOME:
            line_info_goto_root(hst);
            break;

        case KEY_END:
            line_info_reset_start(hst);
            break;
    }
}

void line_info_onDraw(ToxWindow *self)
{
    ChatContext *ctx = self->chatwin;

    wattron(ctx->linewin, A_BOLD | COLOR_PAIR(BLUE));
    mvwprintw(ctx->linewin, 1, 0, "Scroll mode:\n");
    wattroff(ctx->linewin, A_BOLD | COLOR_PAIR(BLUE));
    mvwprintw(ctx->linewin, 1, 13, "Use up/down arrows, page up/page down, and home/end to navigate.\n"
              "             ESC to exit.\n");
}

void line_info_clear(struct history *hst)
{
    hst->line_start = hst->line_end;
    hst->start_id = hst->line_start->id;
}

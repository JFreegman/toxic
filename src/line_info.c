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

void line_info_init(struct history *hst)
{
    hst->line_root = malloc(sizeof(struct line_info));
    memset(hst->line_root, 0, sizeof(struct line_info));

    if (hst->line_root == NULL) {
        endwin();
        fprintf(stderr, "malloc() failed. Aborting...\n");
        exit(EXIT_FAILURE);
    }

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
    ChatContext *ctx = self->chatwin;
    struct history *hst = self->chatwin->hst;

    if (scroll) {
        hst->scroll_mode = true;
        scrollok(ctx->history, 0);
        curs_set(0);
    } else {
        hst->scroll_mode = false;
        scrollok(ctx->history, 1);
        line_info_reset_start(hst);
        curs_set(1);
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

void line_info_add(ToxWindow *self, uint8_t *tmstmp, uint8_t *name1, uint8_t *name2, uint8_t *msg, 
                   uint8_t msgtype, uint8_t bold, uint8_t colour)
{
    ChatContext *ctx = self->chatwin;
    struct history *hst = self->chatwin->hst;
    struct line_info *new_line = malloc(sizeof(struct line_info));
    memset(new_line, 0, sizeof(struct line_info));

    if (new_line == NULL) {
        endwin();
        fprintf(stderr, "malloc() failed. Aborting...\n");
        exit(EXIT_FAILURE);
    }

    int len = 1;     /* there will always be a newline */

    /* for type-specific formatting in print function */
    switch (msgtype) {
    case OUT_MSG:
    case IN_MSG:
        len += 2;
        break;
    case ACTION:
        len += 3;
        break;
    }

    if (msg) {
        memcpy(new_line->msg, msg, MAX_STR_SIZE);
        len += strlen(msg);
    } if (tmstmp) {
        memcpy(new_line->timestamp, tmstmp, TIME_STR_SIZE);
        len += strlen(tmstmp);
    } if (name1) {
        memcpy(new_line->name1, name1, TOXIC_MAX_NAME_LENGTH);
        len += strlen(name1);
    } if (name2) {
        memcpy(new_line->name2, name2, TOXIC_MAX_NAME_LENGTH);
        len += strlen(name2);
    }

    new_line->len = len;
    new_line->msgtype = msgtype;
    new_line->bold = bold;
    new_line->colour = colour;
    new_line->id = hst->line_end->id + 1;

    new_line->prev = hst->line_end;
    hst->line_end->next = new_line;
    hst->line_end = new_line;

    /* If chat history exceeds limit move root forward and free old root */
    if (++(hst->line_items) >= MAX_HISTORY) {
        --hst->line_items;
        struct line_info *tmp = hst->line_root->next;
        tmp->prev = NULL;

        if (hst->line_start->prev == NULL) {  /* if line_start is root move it forward as well */
            hst->line_start = hst->line_start->next;
            hst->line_start->prev = NULL;
        }

        free(hst->line_root);
        hst->line_root = tmp;
    }

    int y, y2, x, x2;
    getmaxyx(self->window, y2, x2);
    getyx(ctx->history, y, x);

    /* move line_start forward proportionate to the number of new rows */
    if (y >= y2 - CHATBOX_HEIGHT) {
        int i;
        int lines = 1 + (len / x2);

        while (lines > 0 && hst->line_start->next) {
            lines -= (1 + hst->line_start->len / x2);
            hst->line_start = hst->line_start->next;
            ++hst->start_id;
        }
    }
}

void line_info_print(ToxWindow *self)
{
    ChatContext *ctx = self->chatwin;
    wclear(ctx->history);
    wmove(ctx->history, 1, 0);
    int y2, x2;
    getmaxyx(self->window, y2, x2);

    struct line_info *line = ctx->hst->line_start;
    int numlines = 0;

    while(line && numlines <= x2) {
        uint8_t type = line->msgtype;
        numlines += line->len / x2;

        switch (type) {
        case OUT_MSG:
        case IN_MSG:
            wattron(ctx->history, COLOR_PAIR(BLUE));
            wprintw(ctx->history, "%s", line->timestamp);
            wattroff(ctx->history, COLOR_PAIR(BLUE));

            int nameclr = GREEN;

            if (type == IN_MSG)
                nameclr = CYAN;
            if (line->colour)
                nameclr = line->colour;

            wattron(ctx->history, COLOR_PAIR(nameclr));
            wprintw(ctx->history, "%s: ", line->name1);
            wattroff(ctx->history, COLOR_PAIR(nameclr));

            if (line->msg[0] == '>')
                wattron(ctx->history, COLOR_PAIR(GREEN));

            wprintw(ctx->history, "%s\n", line->msg);

            if (line->msg[0] == '>')
                wattroff(ctx->history, COLOR_PAIR(GREEN));

            break;

        case ACTION:
            wattron(ctx->history, COLOR_PAIR(BLUE));
            wprintw(ctx->history, "%s", line->timestamp);
            wattroff(ctx->history, COLOR_PAIR(BLUE));

            wattron(ctx->history, COLOR_PAIR(YELLOW));
            wprintw(ctx->history, "* %s %s\n", line->name1, line->msg);
            wattroff(ctx->history, COLOR_PAIR(YELLOW));

            break;

        case SYS_MSG:
            if (line->timestamp[0]) {
                wattron(ctx->history, COLOR_PAIR(BLUE));
                wprintw(ctx->history, "%s", line->timestamp);
                wattroff(ctx->history, COLOR_PAIR(BLUE));
            }

            if (line->bold)
                wattron(ctx->history, A_BOLD);
            if (line->colour)
                wattron(ctx->history, COLOR_PAIR(line->colour));

            wprintw(ctx->history, "%s\n", line->msg);

            if (line->bold)
                wattroff(ctx->history, A_BOLD);
            if (line->colour)
                wattroff(ctx->history, COLOR_PAIR(line->colour));

            break;
        }

        line = line->next;
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

/*  input.c
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
#define _GNU_SOURCE    /* needed for wcwidth() */
#endif

#include <wchar.h>

#include "toxic.h"
#include "windows.h"
#include "misc_tools.h"
#include "toxic_strings.h"
#include "line_info.h"

/* add a char to input field and buffer */
void input_new_char(ToxWindow *self, wint_t key, int x, int y, int mx_x, int mx_y)
{
    ChatContext *ctx = self->chatwin;

    if (ctx->len >= MAX_STR_SIZE - 1) {
        beep();
        return;
    }

    int cur_len = wcwidth(key);

    /* this is the only place we need to do this check */
    if (cur_len == -1)
        return;

    add_char_to_buf(ctx, key);

    if (x + cur_len >= mx_x) {
        int s_len = wcwidth(ctx->line[ctx->start]);
        int cdiff = cur_len - s_len;
        ctx->start += 1 + MAX(0, cdiff);
        wmove(self->window, y, wcswidth(&ctx->line[ctx->start], mx_x));
    } else {
        wmove(self->window, y, x + cur_len);
    }
}

/* delete a char via backspace key from input field and buffer */
static void input_backspace(ToxWindow *self, int x, int y, int mx_x, int mx_y)
{
    ChatContext *ctx = self->chatwin;

    if (ctx->pos <= 0) {
        beep();
        return;
    }

    int cur_len = wcwidth(ctx->line[ctx->pos - 1]);

    del_char_buf_bck(ctx);

    int s_len = wcwidth(ctx->line[ctx->start - 1]);
    int cdiff = s_len - cur_len;

    if (ctx->start && (x >= mx_x - cur_len)) {
        ctx->start = MAX(0, ctx->start - 1 + cdiff);
        wmove(self->window, y, wcswidth(&ctx->line[ctx->start], mx_x));
    } else if (ctx->start && (ctx->pos == ctx->len)) {
        ctx->start = MAX(0, ctx->start - cur_len);
        wmove(self->window, y, wcswidth(&ctx->line[ctx->start], mx_x));
    } else if (ctx->start) {
        ctx->start = MAX(0, ctx->start - cur_len);
    } else {
        wmove(self->window, y, x - cur_len);
    }
}

/* delete a char via delete key from input field and buffer */
static void input_delete(ToxWindow *self)
{
    ChatContext *ctx = self->chatwin;

    if (ctx->pos >= ctx->len) {
        beep();
        return;
    }

    del_char_buf_frnt(ctx);
}

/* deletes entire line before cursor from input field and buffer */
static void input_discard(ToxWindow *self, int mx_y)
{
    ChatContext *ctx = self->chatwin;

    if (ctx->pos <= 0) {
        beep();
        return;
    }

    discard_buf(ctx);
    wmove(self->window, mx_y - CURS_Y_OFFSET, 0);
}

/* deletes entire line after cursor from input field and buffer */
static void input_kill(ChatContext *ctx)
{
    if (ctx->pos != ctx->len) {
        kill_buf(ctx);
        return;
    }

    beep();
}

/* moves cursor/line position to end of line in input field and buffer */
static void input_mv_end(ToxWindow *self, int x, int y, int mx_x, int mx_y)
{
    ChatContext *ctx = self->chatwin;

    ctx->pos = ctx->len;

    int wlen = wcswidth(ctx->line, sizeof(ctx->line));
    ctx->start = MAX(0, 1 + (mx_x * (wlen / mx_x) - mx_x) + (wlen % mx_x));

    int llen = wcswidth(&ctx->line[ctx->start], mx_x);
    int new_x = wlen >= mx_x ? mx_x - 1 : llen;

    wmove(self->window, y, new_x);
}

/* moves cursor/line position to start of line in input field and buffer */
static void input_mv_home(ToxWindow *self, int mx_y)
{
    ChatContext *ctx = self->chatwin;

    if (ctx->pos <= 0) {
        beep();
        return;
    }

    ctx->pos = 0;
    ctx->start = 0;
    wmove(self->window, mx_y - CURS_Y_OFFSET, 0);
}

/* moves cursor/line position left in input field and buffer */
static void input_mv_left(ToxWindow *self, int x, int y, int mx_x, int mx_y)
{
    ChatContext *ctx = self->chatwin;

    if (ctx->pos <= 0) {
        beep();
        return;
    }

    int cur_len = wcwidth(ctx->line[ctx->pos - 1]);

    --ctx->pos;

    int s_len = wcwidth(ctx->line[ctx->start - 1]);
    int cdiff = s_len - cur_len;

    if (ctx->start && (x >= mx_x - cur_len)) {
        ctx->start = MAX(0, ctx->start - 1 + cdiff);
        wmove(self->window, y, wcswidth(&ctx->line[ctx->start], mx_x));
    } else if (ctx->start && (ctx->pos == ctx->len)) {
        ctx->start = MAX(0, ctx->start - cur_len);
        wmove(self->window, y, wcswidth(&ctx->line[ctx->start], mx_x));
    } else if (ctx->start) {
        ctx->start = MAX(0, ctx->start - cur_len);
    } else {
        wmove(self->window, y, x - cur_len);
    }
}

/* moves cursor/line position right in input field and buffer */
static void input_mv_right(ToxWindow *self, int x, int y, int mx_x, int mx_y)
{
    ChatContext *ctx = self->chatwin;

    if (ctx->pos >= ctx->len) {
        beep();
        return;
    }

    ++ctx->pos;

    int cur_len = MAX(1, wcwidth(ctx->line[ctx->pos - 1]));

    if (x + cur_len >= mx_x) {
        int s_len = wcwidth(ctx->line[ctx->start]);
        int cdiff = cur_len - s_len;
        ctx->start += 1 + MAX(0, cdiff);
        wmove(self->window, y, wcswidth(&ctx->line[ctx->start], mx_x));
    } else {
        wmove(self->window, y, x + cur_len);
    }
}

/* puts a line history item in input field and buffer */
static void input_history(ToxWindow *self, wint_t key, int x, int y, int mx_x, int mx_y)
{
    ChatContext *ctx = self->chatwin;

    fetch_hist_item(ctx, key);
    ctx->start = mx_x * (ctx->len / mx_x);
    input_mv_end(self, x, y, mx_x, mx_y);
}

/* Handles non-printable input keys that behave the same for all types of chat windows.
   return true if key matches a function, false otherwise */
bool input_handle(ToxWindow *self, wint_t key, int x, int y, int mx_x, int mx_y)
{
    bool match = true;

    switch (key) {
        case KEY_BACKSPACE:
            input_backspace(self, x, y, mx_x, mx_y);
            break;

        case KEY_DC:
            input_delete(self);
            break;

        case T_KEY_DISCARD:
            input_discard(self, mx_y);
            break;

        case T_KEY_KILL:
            input_kill(self->chatwin);
            break;

        case KEY_HOME:
        case T_KEY_C_A:
            input_mv_home(self, mx_y);
            break;

        case KEY_END:
        case T_KEY_C_E:
            input_mv_end(self, x, y, mx_x, mx_y);
            break;

        case KEY_LEFT:
            input_mv_left(self, x, y, mx_x, mx_y);
            break;

        case KEY_RIGHT:
            input_mv_right(self, x, y, mx_x, mx_y);
            break;

        case KEY_UP:
        case KEY_DOWN:
            input_history(self, key, x, y, mx_x, mx_y);
            break;

        default:
            match = false;
            break;
    }

    return match;
}

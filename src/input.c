/*  input.c
 *
 *
 *  Copyright (C) 2024 Toxic All Rights Reserved.
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

#include "conference.h"
#include "line_info.h"
#include "misc_tools.h"
#include "notify.h"
#include "groupchats.h"
#include "settings.h"
#include "toxic.h"
#include "toxic_strings.h"
#include "windows.h"

extern struct user_settings *user_settings;

/* add a char to input field and buffer */
void input_new_char(ToxWindow *self, wint_t key, int x, int mx_x)
{
    ChatContext *ctx = self->chatwin;

    /* this is the only place we need to do this check */
    if (key == '\n') {
        key = L'¶';
    }

    int cur_len = wcwidth(key);

    if (cur_len == -1) {
        sound_notify(self, notif_error, 0, NULL);
        return;
    }

    if (add_char_to_buf(ctx, key) == -1) {
        sound_notify(self, notif_error, 0, NULL);
        return;
    }

    if (x + cur_len >= mx_x) {
        int s_len = wcwidth(ctx->line[ctx->start]);
        ctx->start += 1 + MAX(0, cur_len - s_len);
    }
}

/* delete a char via backspace key from input field and buffer */
static void input_backspace(ToxWindow *self, int x, int mx_x)
{
    ChatContext *ctx = self->chatwin;

    if (del_char_buf_bck(ctx) == -1) {
        sound_notify(self, notif_error, 0, NULL);
        return;
    }

    int cur_len = ctx->pos > 0 ? wcwidth(ctx->line[ctx->pos - 1]) : 0;
    int s_len = ctx->start > 0 ? wcwidth(ctx->line[ctx->start - 1]) : 0;

    if (ctx->start && (x >= mx_x - cur_len)) {
        ctx->start = MAX(0, ctx->start - 1 + (s_len - cur_len));
    } else if (ctx->start) {
        ctx->start = MAX(0, ctx->start - cur_len);
    }
}

/* delete a char via delete key from input field and buffer */
static void input_delete(ToxWindow *self)
{
    if (del_char_buf_frnt(self->chatwin) == -1) {
        sound_notify(self, notif_error, 0, NULL);
    }
}

/* delete last typed word */
static void input_del_word(ToxWindow *self)
{
    ChatContext *ctx = self->chatwin;

    if (del_word_buf(ctx) == -1) {
        sound_notify(self, notif_error, 0, NULL);
    }
}

/* deletes entire line before cursor from input field and buffer */
static void input_discard(ToxWindow *self)
{
    if (discard_buf(self->chatwin) == -1) {
        sound_notify(self, notif_error, 0, NULL);
    }
}

/* deletes entire line after cursor from input field and buffer */
static void input_kill(ChatContext *ctx)
{
    if (kill_buf(ctx) == -1) {
        sound_notify(NULL, notif_error, NT_ALWAYS, NULL);
    }
}

static void input_yank(ToxWindow *self, int x, int mx_x)
{
    ChatContext *ctx = self->chatwin;

    if (yank_buf(ctx) == -1) {
        sound_notify(self, notif_error, 0, NULL);
        return;
    }

    int yank_cols = MAX(0, wcswidth(ctx->yank, ctx->yank_len));

    if (x + yank_cols >= mx_x) {
        int rmdr = MAX(0, (x + yank_cols) - mx_x);
        int s_len = MAX(0, wcswidth(&ctx->line[ctx->start], rmdr));
        ctx->start += s_len + 1;
    }
}

/* moves cursor/line position to end of line in input field and buffer */
static void input_mv_end(ToxWindow *self, int mx_x)
{
    ChatContext *ctx = self->chatwin;

    ctx->pos = ctx->len;

    int wlen = MAX(0, wcswidth(ctx->line, sizeof(ctx->line) / sizeof(wchar_t)));
    ctx->start = MAX(0, 1 + (mx_x * (wlen / mx_x) - mx_x) + (wlen % mx_x));
}

/* moves cursor/line position to start of line in input field and buffer */
static void input_mv_home(ToxWindow *self)
{
    ChatContext *ctx = self->chatwin;

    if (ctx->pos <= 0) {
        return;
    }

    ctx->pos = 0;
    ctx->start = 0;
}

/* moves cursor/line position left in input field and buffer */
static void input_mv_left(ToxWindow *self, int x, int mx_x)
{
    ChatContext *ctx = self->chatwin;

    if (ctx->pos <= 0) {
        return;
    }

    int cur_len = ctx->pos > 0 ? wcwidth(ctx->line[ctx->pos - 1]) : 0;

    --ctx->pos;

    if (ctx->start > 0 && (x >= mx_x - cur_len)) {
        int s_len = wcwidth(ctx->line[ctx->start - 1]);
        ctx->start = MAX(0, ctx->start - 1 + (s_len - cur_len));
    } else if (ctx->start > 0) {
        ctx->start = MAX(0, ctx->start - cur_len);
    }
}

/* moves the cursor to the beginning of the previous word in input field and buffer */
static void input_skip_left(ToxWindow *self, int x, int mx_x)
{
    ChatContext *ctx = self->chatwin;

    if (ctx->pos <= 0) {
        return;
    }

    int count = 0;

    do {
        --ctx->pos;
        count += wcwidth(ctx->line[ctx->pos]);
    } while (ctx->pos > 0 && (ctx->line[ctx->pos - 1] != L' ' || ctx->line[ctx->pos] == L' '));

    if (ctx->start > 0 && (x >= mx_x - count)) {
        int s_len = wcwidth(ctx->line[ctx->start - 1]);
        ctx->start = MAX(0, ctx->start - 1 + (s_len - count));
    } else if (ctx->start > 0) {
        ctx->start = MAX(0, ctx->start - count);
    }
}

/* moves cursor/line position right in input field and buffer */
static void input_mv_right(ToxWindow *self, int x, int mx_x)
{
    ChatContext *ctx = self->chatwin;

    if (ctx->pos >= ctx->len) {
        return;
    }

    ++ctx->pos;

    int cur_len = wcwidth(ctx->line[ctx->pos - 1]);

    if (x + cur_len >= mx_x) {
        int s_len = wcwidth(ctx->line[ctx->start]);
        ctx->start += 1 + MAX(0, cur_len - s_len);
    }
}

/* moves the cursor to the end of the next word in input field and buffer */
static void input_skip_right(ToxWindow *self, int x, int mx_x)
{
    ChatContext *ctx = self->chatwin;

    if (ctx->pos >= ctx->len) {
        return;
    }

    int count = 0;

    do {
        count += wcwidth(ctx->line[ctx->pos]);
        ++ctx->pos;
    } while (ctx->pos < ctx->len && !(ctx->line[ctx->pos] == L' ' && ctx->line[ctx->pos - 1] != L' '));

    int newpos = x + count;

    if (newpos >= mx_x) {
        ctx->start += (1 + (newpos - mx_x));
    }
}

/* puts a line history item in input field and buffer */
static void input_history(ToxWindow *self, wint_t key, int mx_x)
{
    ChatContext *ctx = self->chatwin;

    fetch_hist_item(ctx, key);
    int wlen = MAX(0, wcswidth(ctx->line, sizeof(ctx->line) / sizeof(wchar_t)));
    ctx->start = wlen < mx_x ? 0 : wlen - mx_x + 1;
}

/* Handles non-printable input keys that behave the same for all types of chat windows.
   return true if key matches a function, false otherwise */
bool input_handle(ToxWindow *self, wint_t key, int x, int mx_x)
{
    bool match = true;

    switch (key) {
        case 0x7f:
        case KEY_BACKSPACE:
            input_backspace(self, x, mx_x);
            break;

        case KEY_DC:
            input_delete(self);
            break;

        case T_KEY_DISCARD:
            input_discard(self);
            break;

        case T_KEY_KILL:
            input_kill(self->chatwin);
            break;

        case T_KEY_C_Y:
            input_yank(self, x, mx_x);
            break;

        case T_KEY_C_W:
            input_del_word(self);
            break;

        case KEY_HOME:
        case T_KEY_C_A:
            input_mv_home(self);
            break;

        case KEY_END:
        case T_KEY_C_E:
            input_mv_end(self, mx_x);
            break;

        case KEY_LEFT:
            input_mv_left(self, x, mx_x);
            break;

        case KEY_RIGHT:
            input_mv_right(self, x, mx_x);
            break;

        case KEY_UP:
        case KEY_DOWN:
            input_history(self, key, mx_x);
            break;

        case T_KEY_C_L:
            force_refresh(self->chatwin->history);
            break;

        case T_KEY_C_LEFT:
            input_skip_left(self, x, mx_x);
            break;

        case T_KEY_C_RIGHT:
            input_skip_right(self, x, mx_x);
            break;

        default:
            match = false;
            break;
    }

    /* TODO: this special case is ugly.
       maybe convert entire function to if/else and make them all customizable keys? */
    if (!match) {
        if (key == user_settings->key_toggle_peerlist) {
            if (self->type == WINDOW_TYPE_CONFERENCE) {
                self->show_peerlist ^= 1;
                redraw_conference_win(self);
            } else if (self->type == WINDOW_TYPE_GROUPCHAT) {
                self->show_peerlist ^= 1;
                redraw_groupchat_win(self);
            }

            match = true;
        } else if (key == user_settings->key_toggle_pastemode) {
            self->chatwin->pastemode ^= 1;
            match = true;
        }
    }

    if (match) {
        flag_interface_refresh();
    }

    return match;
}

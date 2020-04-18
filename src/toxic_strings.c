/*  toxic_strings.c
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
#include <wchar.h>

#include "misc_tools.h"
#include "notify.h"
#include "toxic.h"
#include "toxic_strings.h"
#include "windows.h"

/* Adds char to line at pos. Return 0 on success, -1 if line buffer is full */
int add_char_to_buf(ChatContext *ctx, wint_t ch)
{
    if (ctx->len >= MAX_STR_SIZE - 1) {
        return -1;
    }

    wmemmove(&ctx->line[ctx->pos + 1], &ctx->line[ctx->pos], ctx->len - ctx->pos);
    ctx->line[ctx->pos++] = ch;
    ctx->line[++ctx->len] = L'\0';

    return 0;
}

/* Deletes the character before pos. Return 0 on success, -1 if nothing to delete */
int del_char_buf_bck(ChatContext *ctx)
{
    if (ctx->pos <= 0) {
        return -1;
    }

    wmemmove(&ctx->line[ctx->pos - 1], &ctx->line[ctx->pos], ctx->len - ctx->pos);
    --ctx->pos;
    ctx->line[--ctx->len] = L'\0';

    return 0;
}

/* Deletes the character at pos. Return 0 on success, -1 if nothing to delete. */
int del_char_buf_frnt(ChatContext *ctx)
{
    if (ctx->pos >= ctx->len) {
        return -1;
    }

    wmemmove(&ctx->line[ctx->pos], &ctx->line[ctx->pos + 1], ctx->len - ctx->pos - 1);
    ctx->line[--ctx->len] = L'\0';

    return 0;
}

/* Deletes the line from beginning to pos and puts discarded portion in yank buffer.
   Return 0 on success, -1 if noting to discard. */
int discard_buf(ChatContext *ctx)
{
    if (ctx->pos <= 0) {
        return -1;
    }

    ctx->yank_len = ctx->pos;
    wmemcpy(ctx->yank, ctx->line, ctx->yank_len);
    ctx->yank[ctx->yank_len] = L'\0';

    wmemmove(ctx->line, &ctx->line[ctx->pos], ctx->len - ctx->pos);
    ctx->len -= ctx->pos;
    ctx->pos = 0;
    ctx->start = 0;
    ctx->line[ctx->len] = L'\0';

    return 0;
}

/* Deletes the line from pos to len and puts killed portion in yank buffer.
   Return 0 on success, -1 if nothing to kill. */
int kill_buf(ChatContext *ctx)
{
    if (ctx->len <= ctx->pos) {
        return -1;
    }

    ctx->yank_len = ctx->len - ctx->pos;
    wmemcpy(ctx->yank, &ctx->line[ctx->pos], ctx->yank_len);
    ctx->yank[ctx->yank_len] = L'\0';

    ctx->line[ctx->pos] = L'\0';
    ctx->len = ctx->pos;

    return 0;
}

/* Inserts string in ctx->yank into line at pos.
   Return 0 on success, -1 if yank buffer is empty or too long */
int yank_buf(ChatContext *ctx)
{
    if (!ctx->yank[0]) {
        return -1;
    }

    if (ctx->yank_len + ctx->len >= MAX_STR_SIZE) {
        return -1;
    }

    wmemmove(&ctx->line[ctx->pos + ctx->yank_len], &ctx->line[ctx->pos], ctx->len - ctx->pos);
    wmemcpy(&ctx->line[ctx->pos], ctx->yank, ctx->yank_len);

    ctx->pos += ctx->yank_len;
    ctx->len += ctx->yank_len;
    ctx->line[ctx->len] = L'\0';

    return 0;
}

/* Deletes all characters from line starting at pos and going backwards
   until we find a space or run out of characters.
   Return 0 on success, -1 if nothing to delete */
int del_word_buf(ChatContext *ctx)
{
    if (ctx->len == 0 || ctx->pos == 0) {
        return -1;
    }

    int i = ctx->pos, count = 0;

    /* traverse past empty space */
    while (i > 0 && ctx->line[i - 1] == L' ') {
        ++count;
        --i;
    }

    /* traverse past last entered word */
    while (i > 0 && ctx->line[i - 1] != L' ') {
        ++count;
        --i;
    }

    wmemmove(&ctx->line[i], &ctx->line[ctx->pos], ctx->len - ctx->pos);

    ctx->start = MAX(0, ctx->start - count);   /* TODO: take into account widechar */
    ctx->len -= count;
    ctx->pos -= count;
    ctx->line[ctx->len] = L'\0';

    return 0;
}

/* nulls line and sets pos, len and start to 0 */
void reset_buf(ChatContext *ctx)
{
    ctx->line[0] = L'\0';
    ctx->pos = 0;
    ctx->len = 0;
    ctx->start = 0;
}

/* Removes trailing spaces and newlines from line. */
void rm_trailing_spaces_buf(ChatContext *ctx)
{
    if (ctx->len <= 0) {
        return;
    }

    if (ctx->line[ctx->len - 1] != ' ' && ctx->line[ctx->len - 1] != L'¶') {
        return;
    }

    int i;

    for (i = ctx->len - 1; i >= 0; --i) {
        if (ctx->line[i] != ' ' && ctx->line[i] != L'¶') {
            break;
        }
    }

    ctx->len = i + 1;
    ctx->pos = MIN(ctx->pos, ctx->len);
    ctx->line[ctx->len] = L'\0';
}

#define HIST_PURGE MAX_LINE_HIST / 4

/* shifts hist items back and makes room for HIST_PURGE new entries */
static void shift_hist_back(ChatContext *ctx)
{
    int i;
    int n = MAX_LINE_HIST - HIST_PURGE;

    for (i = 0; i < n; ++i) {
        wmemcpy(ctx->ln_history[i], ctx->ln_history[i + HIST_PURGE], MAX_STR_SIZE);
    }

    ctx->hst_tot = n;
}

/* adds a line to the ln_history buffer at hst_pos and sets hst_pos to end of history. */
void add_line_to_hist(ChatContext *ctx)
{
    if (ctx->len >= MAX_STR_SIZE) {
        return;
    }

    if (ctx->hst_tot >= MAX_LINE_HIST) {
        shift_hist_back(ctx);
    }

    ++ctx->hst_tot;
    ctx->hst_pos = ctx->hst_tot;

    wmemcpy(ctx->ln_history[ctx->hst_tot - 1], ctx->line, ctx->len + 1);
}

/* copies history item at hst_pos to line. Sets pos and len to the len of the history item.
   hst_pos is decremented or incremented depending on key_dir.

   resets line if at end of history */
void fetch_hist_item(ChatContext *ctx, int key_dir)
{
    if (wcscmp(ctx->line, L"\0") != 0
            && ctx->hst_pos == ctx->hst_tot) {
        add_line_to_hist(ctx);
        ctx->hst_pos--;
    }

    if (key_dir == KEY_UP) {
        if (--ctx->hst_pos < 0) {
            ctx->hst_pos = 0;
            sound_notify(NULL, notif_error, NT_ALWAYS, NULL);
        }
    } else {
        if (++ctx->hst_pos >= ctx->hst_tot) {
            ctx->hst_pos = ctx->hst_tot;
            reset_buf(ctx);
            return;
        }
    }

    const wchar_t *hst_line = ctx->ln_history[ctx->hst_pos];
    size_t h_len = wcslen(hst_line);

    wmemcpy(ctx->line, hst_line, h_len + 1);
    ctx->pos = h_len;
    ctx->len = h_len;
}

void strsubst(char *str, char old, char new)
{
    int i;

    for (i = 0; str[i] != '\0'; ++i) {
        if (str[i] == old) {
            str[i] = new;
        }
    }
}

void wstrsubst(wchar_t *str, wchar_t old, wchar_t new)
{
    int i;

    for (i = 0; str[i] != L'\0'; ++i) {
        if (str[i] == old) {
            str[i] = new;
        }
    }
}

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

#include "toxic.h"
#include "windows.h"
#include "misc_tools.h"
#include "toxic_strings.h"

/* Adds char to line at pos */
void add_char_to_buf(ChatContext *ctx, wint_t ch)
{
    if (ctx->len >= MAX_STR_SIZE)
        return;

    wmemmove(&ctx->line[ctx->pos + 1], &ctx->line[ctx->pos], ctx->len - ctx->pos);
    ctx->line[ctx->pos++] = ch;
    ctx->line[++ctx->len] = L'\0';
}

/* Deletes the character before pos */
void del_char_buf_bck(ChatContext *ctx)
{
    if (ctx->pos == 0)
        return;

    wmemmove(&ctx->line[ctx->pos - 1], &ctx->line[ctx->pos], ctx->len - ctx->pos);
    --ctx->pos;
    ctx->line[--ctx->len] = L'\0';
}

/* Deletes the character at pos */
void del_char_buf_frnt(ChatContext *ctx)
{
    if (ctx->pos >= ctx->len)
        return;

    wmemmove(&ctx->line[ctx->pos], &ctx->line[ctx->pos + 1], ctx->len - ctx->pos - 1);
    ctx->line[--ctx->len] = L'\0';
}

/* Deletes the line from beginning to pos */
void discard_buf(ChatContext *ctx)
{
    if (ctx->pos <= 0)
        return;

    wmemmove(ctx->line, &ctx->line[ctx->pos], ctx->len - ctx->pos);
    ctx->len -= ctx->pos;
    ctx->pos = 0;
    ctx->start = 0;
    ctx->line[ctx->len] = L'\0';
}

/* Deletes the line from pos to len */
void kill_buf(ChatContext *ctx)
{
    if (ctx->len == ctx->pos)
        return;

    ctx->line[ctx->pos] = L'\0';
    ctx->len = ctx->pos;
}

/* nulls line and sets pos, len and start to 0 */
void reset_buf(ChatContext *ctx)
{
    ctx->line[0] = L'\0';
    ctx->pos = 0;
    ctx->len = 0;
    ctx->start = 0;
}

/* Removes trailing spaces from line. */
void rm_trailing_spaces_buf(ChatContext *ctx)
{
    if (ctx->len <= 0)
        return;

    if (ctx->line[ctx->len - 1] != ' ')
        return;

    int i;

    for (i = ctx->len - 1; i >= 0; --i) {
        if (ctx->line[i] != ' ')
            break;
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

    for (i = 0; i < n; ++i)
        wmemcpy(ctx->ln_history[i], ctx->ln_history[i + HIST_PURGE], MAX_STR_SIZE);

    ctx->hst_tot = n;
}

/* adds a line to the ln_history buffer at hst_pos and sets hst_pos to end of history. */
void add_line_to_hist(ChatContext *ctx)
{
    if (ctx->len > MAX_STR_SIZE)
        return;

    if (ctx->hst_tot >= MAX_LINE_HIST)
        shift_hist_back(ctx);

    ++ctx->hst_tot;
    ctx->hst_pos = ctx->hst_tot;

    wmemcpy(ctx->ln_history[ctx->hst_tot - 1], ctx->line, ctx->len + 1);
}

/* copies history item at hst_pos to line. Sets pos and len to the len of the history item.
   hst_pos is decremented or incremented depending on key_dir.

   resets line if at end of history */
void fetch_hist_item(ChatContext *ctx, int key_dir)
{
    if (key_dir == KEY_UP) {
        if (--ctx->hst_pos < 0) {
            ctx->hst_pos = 0;
            beep();
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

/* looks for the first instance in list that begins with the last entered word in line according to pos,
   then fills line with the complete word. e.g. "Hello jo" would complete the line
   with "Hello john".

   list is a pointer to the list of strings being compared, n_items is the number of items
   in the list, and size is the size of each item in the list.

   Returns the difference between the old len and new len of line on success, -1 if error */
int complete_line(ChatContext *ctx, const void *list, int n_items, int size)
{
    if (ctx->pos <= 0 || ctx->len <= 0 || ctx->len >= MAX_STR_SIZE)
        return -1;

    const char *L = (char *) list;

    char ubuf[MAX_STR_SIZE];

    /* work with multibyte string copy of buf for simplicity */
    if (wcs_to_mbs_buf(ubuf, ctx->line, sizeof(ubuf)) == -1)
        return -1;

    /* isolate substring from space behind pos to pos */
    char tmp[MAX_STR_SIZE];
    snprintf(tmp, sizeof(tmp), "%s", ubuf);
    tmp[ctx->pos] = '\0';
    char *sub = strrchr(tmp, ' ');
    int n_endchrs = 1;    /* 1 = append space to end of match, 2 = append ": " */

    if (!sub++) {
        sub = tmp;

        if (sub[0] != '/')    /* make sure it's not a command */
            n_endchrs = 2;
    }

    if (string_is_empty(sub))
        return -1;

    int s_len = strlen(sub);
    const char *match;
    bool is_match = false;
    int i;

    /* look for a match in list */
    for (i = 0; i < n_items; ++i) {
        match = &L[i * size];

        if ((is_match = strncasecmp(match, sub, s_len) == 0))
            break;
    }

    if (!is_match)
        return -1;

    /* put match in correct spot in buf and append endchars (space or ": ") */
    const char *endchrs = n_endchrs == 1 ? " " : ": ";
    int m_len = strlen(match);
    int strt = ctx->pos - s_len;
    int diff = m_len - s_len + n_endchrs;

    if (ctx->len + diff > MAX_STR_SIZE)
        return -1;

    char tmpend[MAX_STR_SIZE];
    strcpy(tmpend, &ubuf[ctx->pos]);
    strcpy(&ubuf[strt], match);
    strcpy(&ubuf[strt + m_len], endchrs);
    strcpy(&ubuf[strt + m_len + n_endchrs], tmpend);

    /* convert to widechar and copy back to original buf */
    wchar_t newbuf[MAX_STR_SIZE];

    if (mbs_to_wcs_buf(newbuf, ubuf, MAX_STR_SIZE) == -1)
        return -1;

    wcscpy(ctx->line, newbuf);

    ctx->len += diff;
    ctx->pos += diff;

    return diff;
}

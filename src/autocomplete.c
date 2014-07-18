/*  autocomplete.c
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

#ifdef __APPLE__
    #include <sys/types.h>
    #include <sys/dir.h>
#else
    #include <dirent.h>
#endif /* ifdef __APPLE__ */

#include "windows.h"
#include "toxic.h"
#include "misc_tools.h"
#include "line_info.h"
#include "execute.h"

/* puts match in match buffer. if more than one match, add first n chars that are identical.
   e.g. if matches contains: [foo, foobar, foe] we put fo in matches. */
static void get_str_match(char *match, char (*matches)[MAX_STR_SIZE], int n)
{
    if (n == 1) {
        strcpy(match, matches[0]);
        return;
    }

    int i;
    int shortest = MAX_STR_SIZE;

    for (i = 0; i < n; ++i) {
        int m_len = strlen(matches[i]);

        if (m_len < shortest)
            shortest = m_len;
    }

    for (i = 0; i < shortest; ++i) {
        char ch = matches[0][i];
        int j;

        for (j = 0; j < n; ++j) {
            if (matches[j][i] != ch) {
                strcpy(match, matches[0]);
                match[i] = '\0';
                return;
            }
        }
    }

    strcpy(match, matches[0]);
}

/* looks for the first instance in list that begins with the last entered word in line according to pos,
   then fills line with the complete word. e.g. "Hello jo" would complete the line
   with "Hello john". Works slightly differently for directory paths with the same results.

   list is a pointer to the list of strings being compared, n_items is the number of items
   in the list, and size is the size of each item in the list.

   Returns the difference between the old len and new len of line on success, -1 if error */
int complete_line(ChatContext *ctx, const void *list, int n_items, int size)
{
    if (ctx->pos <= 0 || ctx->len <= 0 || ctx->len >= MAX_STR_SIZE || size > MAX_STR_SIZE)
        return -1;

    const char *L = (char *) list;

    bool dir_search = false;
    const char *endchrs = " ";
    char ubuf[MAX_STR_SIZE];

    /* work with multibyte string copy of buf for simplicity */
    if (wcs_to_mbs_buf(ubuf, ctx->line, sizeof(ubuf)) == -1)
        return -1;

    /* isolate substring from space behind pos to pos */
    char tmp[MAX_STR_SIZE];
    snprintf(tmp, sizeof(tmp), "%s", ubuf);
    tmp[ctx->pos] = '\0';

    const char *s = strrchr(tmp, ' ');
    char *sub = malloc(strlen(ubuf) + 1);

    if (sub == NULL)
        exit_toxic_err("failed in complete_line", FATALERR_MEMORY);

    if (!s) {
        strcpy(sub, tmp);

        if (sub[0] != '/')
            endchrs = ": ";
    } else {
        strcpy(sub, &s[1]);

        if (strncmp(ubuf, "/sendfile", strlen("/sendfile")) == 0) {
            dir_search = true;
            int sub_len = strlen(sub);
            int si = char_rfind(sub, '/', sub_len);
            memmove(sub, &sub[si + 1], sub_len - si);
        }
    }

    if (string_is_empty(sub)) {
        free(sub);
        return -1;
    }

    int s_len = strlen(sub);
    const char *str;
    int n_matches = 0;
    char matches[n_items][MAX_STR_SIZE];
    int i = 0;

    /* put all list matches in matches array */
    for (i = 0; i < n_items; ++i) {
        str = &L[i * size];

        if (strncasecmp(str, sub, s_len) == 0)
            strcpy(matches[n_matches++], str);
    }

    free(sub);

    if (!n_matches)
        return -1;

    char match[size];
    get_str_match(match, matches, n_matches);

    if (dir_search) {
        if (n_matches == 1)
            endchrs = char_rfind(match, '.', strlen(match)) ? "\"" : "/";
        else
            endchrs = "";
    } else if (n_matches > 1) {
        endchrs = "";
    }

    /* put match in correct spot in buf and append endchars */
    int n_endchrs = strlen(endchrs);
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

/* matches /sendfile "<incomplete-dir>" line to matching directories.

   if only one match, auto-complete line.
   if > 1 match, print out all the matches and partially complete line
   return diff between old len and new len of ctx->line, or -1 if no matches 
*/
#define MAX_DIRS 256

int dir_match(ToxWindow *self, Tox *m, const wchar_t *line)
{
    char b_path[MAX_STR_SIZE];
    char b_name[MAX_STR_SIZE];

    if (wcs_to_mbs_buf(b_path, line, sizeof(b_path)) == -1)
        return -1;

    int si = char_rfind(b_path, '/', strlen(b_path));

    if (!b_path[0]) {    /* list everything in pwd */
        strcpy(b_path, ".");  
    } else if (!si && b_path[0] != '/') {    /* look for matches in pwd */
        char tmp[MAX_STR_SIZE];
        snprintf(tmp, sizeof(tmp), ".%s", b_path);
        strcpy(b_path, tmp);  
    }

    strcpy(b_name, &b_path[si + 1]);
    b_path[si + 1] = '\0';
    int b_name_len = strlen(b_name);

    DIR *dp = opendir(b_path);

    if (dp == NULL)
        return -1;

    char dirnames[MAX_DIRS][NAME_MAX];
    struct dirent *entry;
    int dircount = 0;

    while ((entry = readdir(dp)) && dircount < MAX_DIRS) {
        if (strncmp(entry->d_name, b_name, b_name_len) == 0) {
            snprintf(dirnames[dircount], sizeof(dirnames[dircount]), "%s", entry->d_name);
            ++dircount;
        }
    }

    if (dircount == 0)
        return -1;

    if (dircount > 1) {
        execute(self->chatwin->history, self, m, "/clear", GLOBAL_COMMAND_MODE);

        int i;

        for (i = 0; i < dircount; ++i)
            line_info_add(self, NULL, NULL, NULL, dirnames[i], SYS_MSG, 0, 0);
    }

    return complete_line(self->chatwin, dirnames, dircount, NAME_MAX);
}

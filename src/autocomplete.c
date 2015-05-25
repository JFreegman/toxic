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
#include <limits.h>

#ifdef NO_GETTEXT
#define gettext(A) (A)
#else
#include <libintl.h>
#endif

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
#include "configdir.h"

static void print_matches(ToxWindow *self, Tox *m, const void *list, int n_items, int size)
{
    if (m)
        execute(self->chatwin->history, self, m, "/clear", GLOBAL_COMMAND_MODE);

    const char *L = (char *) list;
    int i;

    for (i = 0; i < n_items; ++i)
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "%s", &L[i * size]);

    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "");   /* formatting */
}

/* puts match in match buffer. if more than one match, add first n chars that are identical.
   e.g. if matches contains: [foo, foobar, foe] we put fo in matches. */
static void get_str_match(ToxWindow *self, char *match, char (*matches)[MAX_STR_SIZE], int n)
{
    if (n == 1) {
        strcpy(match, matches[0]);
        return;
    }

    int i;

    for (i = 0; i < MAX_STR_SIZE; ++i) {
        char ch1 = matches[0][i];
        int j;

        for (j = 0; j < n; ++j) {
            char ch2 = matches[j][i];

            if (ch1 != ch2 || !ch1) {
                strcpy(match, matches[0]);
                match[i] = '\0';
                return;
            }
        }
    }

    strcpy(match, matches[0]);
}

/* looks for all instances in list that begin with the last entered word in line according to pos,
   then fills line with the complete word. e.g. "Hello jo" would complete the line
   with "Hello john". If multiple matches, prints out all the matches and semi-completes line.

   list is a pointer to the list of strings being compared, n_items is the number of items
   in the list, and size is the size of each item in the list.

   Returns the difference between the old len and new len of line on success, -1 if error */
int complete_line(ToxWindow *self, const void *list, int n_items, int size)
{
    ChatContext *ctx = self->chatwin;

    if (ctx->pos <= 0 || ctx->len <= 0 || ctx->len >= MAX_STR_SIZE || size > MAX_STR_SIZE)
        return -1;

    const char *L = (char *) list;
    const char *endchrs = " ";
    char ubuf[MAX_STR_SIZE];

    /* work with multibyte string copy of buf for simplicity */
    if (wcs_to_mbs_buf(ubuf, ctx->line, sizeof(ubuf)) == -1)
        return -1;

    /* TODO: generalize this */
    bool dir_search =    !strncmp(ubuf, "/sendfile", strlen("/sendfile"))
                      || !strncmp(ubuf, "/avatar", strlen("/avatar"));

    /* isolate substring from space behind pos to pos */
    char tmp[MAX_STR_SIZE];
    snprintf(tmp, sizeof(tmp), "%s", ubuf);
    tmp[ctx->pos] = '\0';

    const char *s = dir_search ? strchr(tmp, '\"') : strrchr(tmp, ' ');
    char *sub = malloc(strlen(ubuf) + 1);

    if (sub == NULL)
        exit_toxic_err(gettext("failed in complete_line"), FATALERR_MEMORY);

    if (!s && !dir_search) {
        strcpy(sub, tmp);

        if (sub[0] != '/')
            endchrs = ": ";
    } else if (s) {
        strcpy(sub, &s[1]);

        if (dir_search) {
            int sub_len = strlen(sub);
            int si = char_rfind(sub, '/', sub_len);

            if (si || *sub == '/')
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

    if (!dir_search && n_matches > 1)
        print_matches(self, NULL, matches, n_matches, MAX_STR_SIZE);

    char match[MAX_STR_SIZE];
    get_str_match(self, match, matches, n_matches);

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

    if (ctx->len + diff >= MAX_STR_SIZE)
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

/* transforms a tab complete starting with the shorthand "~" into the full home directory.*/
static void complt_home_dir(ToxWindow *self, char *path, int pathsize, const char *cmd, int cmdlen)
{
    ChatContext *ctx = self->chatwin;

    char homedir[MAX_STR_SIZE] = {0};
    get_home_dir(homedir, sizeof(homedir));

    char newline[MAX_STR_SIZE];
    snprintf(newline, sizeof(newline), "%s \"%s%s", cmd, homedir, path + 1);
    snprintf(path, pathsize, "%s", &newline[cmdlen]);

    wchar_t wline[MAX_STR_SIZE];

    if (mbs_to_wcs_buf(wline, newline, sizeof(wline)) == -1)
        return;

    int newlen = wcslen(wline);

    if (ctx->len + newlen >= MAX_STR_SIZE)
        return;

    wmemcpy(ctx->line, wline, newlen + 1);
    ctx->pos = newlen;
    ctx->len = ctx->pos;
}

/*  attempts to match /command "<incomplete-dir>" line to matching directories.

    if only one match, auto-complete line.
    return diff between old len and new len of ctx->line, -1 if no matches or > 1 match */
#define MAX_DIRS 512

int dir_match(ToxWindow *self, Tox *m, const wchar_t *line, const wchar_t *cmd)
{
    char b_path[MAX_STR_SIZE];
    char b_name[MAX_STR_SIZE];
    char b_cmd[MAX_STR_SIZE];
    const wchar_t *tmpline = &line[wcslen(cmd) + 2];   /* start after "/command \"" */

    if (wcs_to_mbs_buf(b_path, tmpline, sizeof(b_path)) == -1)
        return -1;

    if (wcs_to_mbs_buf(b_cmd, cmd, sizeof(b_cmd)) == -1)
        return -1;

    if (b_path[0] == '~')
        complt_home_dir(self, b_path, sizeof(b_path), b_cmd, strlen(b_cmd) + 2);

    int si = char_rfind(b_path, '/', strlen(b_path));

    if (!b_path[0]) {    /* list everything in pwd */
        b_path[0] = '.';
        b_path[1] = '\0';
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
        if (strncmp(entry->d_name, b_name, b_name_len) == 0
                                && strcmp(".", entry->d_name) && strcmp("..", entry->d_name)) {
            snprintf(dirnames[dircount], sizeof(dirnames[dircount]), "%s", entry->d_name);
            ++dircount;
        }
    }

    closedir(dp);

    if (dircount == 0)
        return -1;

    if (dircount > 1) {
        qsort(dirnames, dircount, NAME_MAX, qsort_strcasecmp_hlpr);
        print_matches(self, m, dirnames, dircount, NAME_MAX);
    }

    return complete_line(self, dirnames, dircount, NAME_MAX);
}

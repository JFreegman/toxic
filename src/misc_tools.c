/*  misc_tools.c
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
#include <time.h>
#include <limits.h>

#include "toxic_windows.h"

extern ToxWindow *prompt;

static uint64_t current_unix_time;

void update_unix_time(void)
{
    current_unix_time = (uint64_t) time(NULL);
}

uint64_t get_unix_time(void)
{
    return current_unix_time;
}

/* Get the current local time */
struct tm *get_time(void)
{
    struct tm *timeinfo;
    uint64_t t = get_unix_time();
    timeinfo = localtime(&t);
    return timeinfo;
}

/* Prints the time to given window */
void print_time(WINDOW *window)
{
    uint8_t s[MAX_STR_SIZE];
    strftime(s, MAX_STR_SIZE, "[%H:%M:%S] ", get_time());

    wattron(window, COLOR_PAIR(BLUE));
    wprintw(window, "%s", s);
    wattroff(window,COLOR_PAIR(BLUE));
}

/* XXX: FIX */
unsigned char *hex_string_to_bin(char hex_string[])
{
    size_t len = strlen(hex_string);
    unsigned char *val = malloc(len);

    if (val == NULL) {
        endwin();
        fprintf(stderr, "malloc() failed. Aborting...\n");
        exit(EXIT_FAILURE);
    }

    char *pos = hex_string;
    size_t i;

    for (i = 0; i < len; ++i, pos += 2)
        sscanf(pos, "%2hhx", &val[i]);

    return val;
}

/* Returns 1 if the string is empty, 0 otherwise */
int string_is_empty(char *string)
{
    return string[0] == '\0';
}

/* convert a multibyte string to a wide character string (must provide buffer) */
int mbs_to_wcs_buf(wchar_t *buf, const uint8_t *string, size_t n)
{
    size_t len = mbstowcs(NULL, string, 0) + 1;

    if (n < len)
        return -1;

    if ((len = mbstowcs(buf, string, n)) == (size_t) -1)
        return -1;

    return len;
}

/* converts wide character string into a multibyte string.
   Same thing as wcs_to_mbs() but caller must provide its own buffer */
int wcs_to_mbs_buf(uint8_t *buf, const wchar_t *string, size_t n)
{
    size_t len = wcstombs(NULL, string, 0) + 1;

    if (n < len)
        return -1;

    if ((len = wcstombs(buf, string, n)) == (size_t) -1)
        return -1;

    return len;
}

/* convert wide characters to multibyte string: string returned must be freed */
uint8_t *wcs_to_mbs(wchar_t *string)
{
    uint8_t *ret = NULL;
    size_t len = wcstombs(NULL, string, 0);

    if (len != (size_t) -1) {
        ret = malloc(++len);

        if (ret != NULL) {
            if (wcstombs(ret, string, len) == (size_t) -1)
                return NULL;
        }
    } else {
        ret = malloc(2);

        if (ret != NULL) {
            ret[0] = ' ';
            ret[1] = '\0';
        }
    }

    if (ret == NULL) {
        endwin();
        fprintf(stderr, "malloc() failed. Aborting...\n");
        exit(EXIT_FAILURE);
    }

    return ret;
}

/* convert a wide char to multibyte string */
char *wc_to_char(wchar_t ch)
{
    static char ret[MB_LEN_MAX + 1];
    int len = wctomb(ret, ch);

    if (len == -1) {
        ret[0] = ' ';
        ret[1] = '\0';
    } else {
        ret[len] = '\0';
    }

    return ret;
}

/* Returns 1 if connection has timed out, 0 otherwise */
int timed_out(uint64_t timestamp, uint64_t curtime, uint64_t timeout)
{
    return timestamp + timeout <= curtime;
}

/* Colours the window tab according to type. Beeps if is_beep is true */
void alert_window(ToxWindow *self, int type, bool is_beep)
{
    switch (type) {
    case WINDOW_ALERT_0:
        self->alert0 = true;
        break;
    case WINDOW_ALERT_1:
        self->alert1 = true;
        break;
    case WINDOW_ALERT_2:
        self->alert2 = true;
        break;
    }

    StatusBar *stb = prompt->stb;

    if (is_beep && stb->status != TOX_USERSTATUS_BUSY)
        beep();
}

/* case-insensitive string compare function for use with qsort */
int qsort_strcasecmp_hlpr(const void *nick1, const void *nick2)
{
    return strcasecmp((const char *) nick1, (const char *) nick2);
}

/* Returns 1 if nick is valid, 0 if not. A valid toxic nick:
      - cannot be empty
      - cannot start with a space
      - must not contain contiguous spaces */
int valid_nick(uint8_t *nick)
{
    if (!nick[0] || nick[0] == ' ')
        return 0;

    int i;

    for (i = 0; nick[i]; ++i) {
        if (nick[i] == ' ' && nick[i+1] == ' ')
            return 0;
    }

    return 1;
}

/* Moves cursor to the end of the line in given window */
void mv_curs_end(WINDOW *w, size_t len, int max_y, int max_x)
{
    int end_y = (len / max_x) + (max_y - CURS_Y_OFFSET);
    int end_x = len % max_x;
    wmove(w, end_y, end_x);  
}

/* gets base file name from path or original file name if no path is supplied */
void get_file_name(uint8_t *pathname, uint8_t *namebuf)
{
    int idx = strlen(pathname) - 1;

    while (idx >= 0 && pathname[idx] == '/')
      pathname[idx--] = '\0';

    uint8_t *filename = strrchr(pathname, '/');    /* Try unix style paths */
    
    if (filename != NULL) {
        if (!strlen(++filename))
            filename = pathname;
    } else {
        filename = strrchr(pathname, '\\');    /* Try windows style paths */

        if (filename == NULL)
            filename = pathname;
    }

    snprintf(namebuf, MAX_STR_SIZE, "%s", filename);
}

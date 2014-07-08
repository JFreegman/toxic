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

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <limits.h>

#include "toxic.h"
#include "windows.h"
#include "misc_tools.h"
#include "settings.h"

extern ToxWindow *prompt;
extern struct user_settings *user_settings;

static uint64_t current_unix_time;

void update_unix_time(void)
{
    current_unix_time = (uint64_t) time(NULL);
}

uint64_t get_unix_time(void)
{
    return current_unix_time;
}

/* Returns 1 if connection has timed out, 0 otherwise */
int timed_out(uint64_t timestamp, uint64_t curtime, uint64_t timeout)
{
    return timestamp + timeout <= curtime;
}

/* Get the current local time */
struct tm *get_time(void)
{
    struct tm *timeinfo;
    uint64_t t = get_unix_time();
    timeinfo = localtime((const time_t*) &t);
    return timeinfo;
}

/*Puts the current time in buf in the format of [HH:mm:ss] */
void get_time_str(char *buf, int bufsize)
{
    if (user_settings->timestamps == TIMESTAMPS_OFF) {
        buf[0] = '\0';
        return;
    }

    const char *t = user_settings->time == TIME_12 ? "[%-I:%M:%S] " : "[%H:%M:%S] ";
    strftime(buf, bufsize, t, get_time());
}

/* Converts seconds to string in format HH:mm:ss; truncates hours and minutes when necessary */
void get_elapsed_time_str(char *buf, int bufsize, uint64_t secs)
{
    if (!secs)
        return;

    long int seconds = secs % 60;
    long int minutes = (secs % 3600) / 60;
    long int hours = secs / 3600;

    if (!minutes && !hours)
        snprintf(buf, bufsize, "%.2ld", seconds);
    else if (!hours)
        snprintf(buf, bufsize, "%ld:%.2ld", minutes, seconds);
    else
        snprintf(buf, bufsize, "%ld:%.2ld:%.2ld", hours, minutes, seconds);
}

char *hex_string_to_bin(const char *hex_string)
{
    size_t len = strlen(hex_string);
    char *val = malloc(len);

    if (val == NULL)
        exit_toxic_err("failed in hex_string_to_bin", FATALERR_MEMORY);

    size_t i;

    for (i = 0; i < len; ++i, hex_string += 2)
        sscanf(hex_string, "%2hhx", &val[i]);

    return val;
}

/* Returns 1 if the string is empty, 0 otherwise */
int string_is_empty(char *string)
{
    return string[0] == '\0';
}

/* convert a multibyte string to a wide character string and puts in buf. */
int mbs_to_wcs_buf(wchar_t *buf, const char *string, size_t n)
{
    size_t len = mbstowcs(NULL, string, 0) + 1;

    if (n < len)
        return -1;

    if ((len = mbstowcs(buf, string, n)) == (size_t) -1)
        return -1;

    return len;
}

/* converts wide character string into a multibyte string and puts in buf. */
int wcs_to_mbs_buf(char *buf, const wchar_t *string, size_t n)
{
    size_t len = wcstombs(NULL, string, 0) + 1;

    if (n < len)
        return -1;

    if ((len = wcstombs(buf, string, n)) == (size_t) -1)
        return -1;

    return len;
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

    if (is_beep && stb->status != TOX_USERSTATUS_BUSY && user_settings->alerts == ALERTS_ENABLED)
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
      - must not contain a forward slash (for logfile naming purposes)
      - must not contain contiguous spaces */
int valid_nick(char *nick)
{
    if (!nick[0] || nick[0] == ' ')
        return 0;

    int i;

    for (i = 0; nick[i]; ++i) {
        if (nick[i] == ' ' && nick[i + 1] == ' ')
            return 0;

        if (nick[i] == '/')
            return 0;
    }

    return 1;
}

/* gets base file name from path or original file name if no path is supplied */
void get_file_name(char *namebuf, const char *pathname)
{
    int idx = strlen(pathname) - 1;

    char tmpname[MAX_STR_SIZE];
    snprintf(tmpname, sizeof(tmpname), "%s", pathname);

    while (idx >= 0 && pathname[idx] == '/')
        tmpname[idx--] = '\0';

    char *filename = strrchr(tmpname, '/');

    if (filename != NULL) {
        if (!strlen(++filename))
            filename = tmpname;
    } else {
        filename = tmpname;
    }

    snprintf(namebuf, sizeof(namebuf), "%s", filename);
}

/* converts str to all lowercase */
void str_to_lower(char *str)
{
    int i;

    for (i = 0; str[i]; ++i)
        str[i] = tolower(str[i]);
}

/* puts friendnum's nick in buf, truncating at TOXIC_MAX_NAME_LENGTH if necessary.
   Returns nick len on success, -1 on failure */
int get_nick_truncate(Tox *m, char *buf, int friendnum)
{
    int len = tox_get_name(m, friendnum, (uint8_t *) buf);
    len = MIN(len, TOXIC_MAX_NAME_LENGTH - 1);
    buf[len] = '\0';
    return len;
}

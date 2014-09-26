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
#include <dirent.h>
#include <sys/stat.h>

#include "toxic.h"
#include "windows.h"
#include "misc_tools.h"
#include "settings.h"
#include "file_senders.h"

extern ToxWindow *prompt;
extern struct user_settings *user_settings;

static uint64_t current_unix_time;

void hst_to_net(uint8_t *num, uint16_t numbytes)
{
#ifndef WORDS_BIGENDIAN
    uint32_t i;
    uint8_t buff[numbytes];

    for (i = 0; i < numbytes; ++i) {
        buff[i] = num[numbytes - i - 1];
    }

    memcpy(num, buff, numbytes);
#endif
    return;
}

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

    const char *t = user_settings->time == TIME_12 ? "%I:%M:%S " : "%H:%M:%S ";
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

int hex_string_to_bytes(char *buf, int size, const char *keystr)
{
    if (size % 2 != 0)
        return -1;

    int i, res;
    const char *pos = keystr;

    for (i = 0; i < size; ++i) {
        res = sscanf(pos, "%2hhx", &buf[i]);
        pos += 2;

        if (res == EOF || res < 1)
            return -1;
    }

    return 0;
}

/* Returns 1 if the string is empty, 0 otherwise */
int string_is_empty(const char *string)
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

/* case-insensitive string compare function for use with qsort */
int qsort_strcasecmp_hlpr(const void *str1, const void *str2)
{
    return strcasecmp((const char *) str1, (const char *) str2);
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
void get_file_name(char *namebuf, int bufsize, const char *pathname)
{
    int idx = strlen(pathname) - 1;
    char *path = strdup(pathname);

    if (path == NULL)
        exit_toxic_err("failed in get_file_name", FATALERR_MEMORY);

    while (idx >= 0 && pathname[idx] == '/')
        path[idx--] = '\0';

    char *finalname = strdup(path);

    if (finalname == NULL)
        exit_toxic_err("failed in get_file_name", FATALERR_MEMORY);

    const char *basenm = strrchr(path, '/');

    if (basenm != NULL) {
        if (basenm[1])
            strcpy(finalname, &basenm[1]);
    }

    snprintf(namebuf, bufsize, "%s", finalname);
    free(finalname);
    free(path);
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

/* returns index of the first instance of ch in s starting at idx.
   returns length of s if char not found */
int char_find(int idx, const char *s, char ch)
{
    int i = idx;

    for (i = idx; s[i]; ++i) {
        if (s[i] == ch)
            break;
    }

    return i;
}

/* returns index of the last instance of ch in s starting at len
   returns 0 if char not found (skips 0th index) */
int char_rfind(const char *s, char ch, int len)
{
    int i = 0;

    for (i = len; i > 0; --i) {
        if (s[i] == ch)
            break;
    }

    return i;
}

/* Converts bytes to appropriate unit and puts in buf as a string */
void bytes_convert_str(char *buf, int size, uint64_t bytes)
{
    double conv = bytes;
    const char *unit;

    if (conv < KiB) {
        unit = "Bytes";
    } else if (conv < MiB) {
        unit = "KiB";
        conv /= (double) KiB;
    } else if (conv < GiB) {
        unit = "MiB";
        conv /= (double) MiB;
    } else {
        unit = "GiB";
        conv /= (double) GiB;
    }

    snprintf(buf, size, "%.1f %s", conv, unit);
}

/* checks if a file exists. Returns true or false */
bool file_exists(const char *path)
{
    struct stat s;
    return stat(path, &s) == 0;
}

/* returns file size or -1 on error */
off_t file_size(const char *path)
{
    struct stat st;

    if (stat(path, &st) == -1)
        return -1;

    return st.st_size;
}

/* compares the first size bytes of fp to signature. 
   Returns 0 if they are the same, 1 if they differ, and -1 on error.

   On success this function will seek back to the beginning of fp */
int check_file_signature(const char *signature, size_t size, FILE *fp)
{
    char buf[size];

    if (fread(buf, size, 1, fp) == -1)
        return -1;

    int ret = memcmp(signature, buf, size);

    if (fseek(fp, 0L, SEEK_SET) == -1)
        return -1;

    return ret == 0 ? 0 : 1;
}

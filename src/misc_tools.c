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
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/stat.h>

#include "toxic.h"
#include "windows.h"
#include "misc_tools.h"
#include "settings.h"
#include "file_transfers.h"

extern ToxWindow *prompt;
extern struct user_settings *user_settings;

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

time_t get_unix_time(void)
{
    return time(NULL);
}

/* Returns 1 if connection has timed out, 0 otherwise */
int timed_out(time_t timestamp, time_t timeout)
{
    return timestamp + timeout <= get_unix_time();
}

/* Get the current local time */
struct tm *get_time(void)
{
    struct tm *timeinfo;
    time_t t = get_unix_time();
    timeinfo = localtime((const time_t *) &t);
    return timeinfo;
}

/*Puts the current time in buf in the format of [HH:mm:ss] */
void get_time_str(char *buf, int bufsize)
{
    if (user_settings->timestamps == TIMESTAMPS_OFF) {
        buf[0] = '\0';
        return;
    }

    const char *t = user_settings->timestamp_format;

    if (strftime(buf, bufsize, t, get_time()) == 0) {
        strftime(buf, bufsize, TIMESTAMP_DEFAULT, get_time());
    }
}

/* Converts seconds to string in format HH:mm:ss; truncates hours and minutes when necessary */
void get_elapsed_time_str(char *buf, int bufsize, time_t secs)
{
    if (!secs) {
        return;
    }

    long int seconds = secs % 60;
    long int minutes = (secs % 3600) / 60;
    long int hours = secs / 3600;

    if (!minutes && !hours) {
        snprintf(buf, bufsize, "%.2ld", seconds);
    } else if (!hours) {
        snprintf(buf, bufsize, "%ld:%.2ld", minutes, seconds);
    } else {
        snprintf(buf, bufsize, "%ld:%.2ld:%.2ld", hours, minutes, seconds);
    }
}

/*
 * Converts a hexidecimal string of length hex_len to binary format and puts the result in output.
 * output_size must be exactly half of hex_len.
 *
 * Returns 0 on success.
 * Returns -1 on failure.
 */
int hex_string_to_bin(const char *hex_string, size_t hex_len, char *output, size_t output_size)
{
    if (output_size == 0 || hex_len != output_size * 2) {
        return -1;
    }

    for (size_t i = 0; i < output_size; ++i) {
        sscanf(hex_string, "%2hhx", (unsigned char *)&output[i]);
        hex_string += 2;
    }

    return 0;
}

int hex_string_to_bytes(char *buf, int size, const char *keystr)
{
    if (size % 2 != 0) {
        return -1;
    }

    int i, res;
    const char *pos = keystr;

    for (i = 0; i < size; ++i) {
        res = sscanf(pos, "%2hhx", (unsigned char *)&buf[i]);
        pos += 2;

        if (res == EOF || res < 1) {
            return -1;
        }
    }

    return 0;
}

/* Converts a binary representation of a Tox ID into a string.
 *
 * Returns 0 on success.
 * Returns -1 on failure.
 */
int bin_id_to_string(const char *bin_id, size_t bin_id_size, char *output, size_t output_size)
{
    if (bin_id_size != TOX_ADDRESS_SIZE || output_size < (TOX_ADDRESS_SIZE * 2 + 1)) {
        return -1;
    }

    size_t i;

    for (i = 0; i < TOX_ADDRESS_SIZE; ++i) {
        snprintf(&output[i * 2], output_size - (i * 2), "%02X", bin_id[i] & 0xff);
    }

    return 0;
}

/* Returns 1 if the string is empty, 0 otherwise */
int string_is_empty(const char *string)
{
    if (!string) {
        return true;
    }

    return string[0] == '\0';
}

/* Returns 1 if the string is empty, 0 otherwise */
int wstring_is_empty(const wchar_t *string)
{
    if (!string) {
        return true;
    }

    return string[0] == L'\0';
}

/* convert a multibyte string to a wide character string and puts in buf. */
int mbs_to_wcs_buf(wchar_t *buf, const char *string, size_t n)
{
    size_t len = mbstowcs(NULL, string, 0) + 1;

    if (n < len) {
        return -1;
    }

    if ((len = mbstowcs(buf, string, n)) == (size_t) - 1) {
        return -1;
    }

    return len;
}

/* converts wide character string into a multibyte string and puts in buf. */
int wcs_to_mbs_buf(char *buf, const wchar_t *string, size_t n)
{
    size_t len = wcstombs(NULL, string, 0) + 1;

    if (n < len) {
        return -1;
    }

    if ((len = wcstombs(buf, string, n)) == (size_t) - 1) {
        return -1;
    }

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
      - must not contain contiguous spaces
      - must not contain a newline or tab seqeunce */
int valid_nick(const char *nick)
{
    if (!nick[0] || nick[0] == ' ') {
        return 0;
    }

    int i;

    for (i = 0; nick[i]; ++i) {
        if ((nick[i] == ' ' && nick[i + 1] == ' ')
                || nick[i] == '/'
                || nick[i] == '\n'
                || nick[i] == '\t'
                || nick[i] == '\v'
                || nick[i] == '\r')

        {
            return 0;
        }
    }

    return 1;
}

/* Converts all newline/tab chars to spaces (use for strings that should be contained to a single line) */
void filter_str(char *str, size_t len)
{
    size_t i;

    for (i = 0; i < len; ++i) {
        if (str[i] == '\n' || str[i] == '\r' || str[i] == '\t' || str[i] == '\v' || str[i] == '\0') {
            str[i] = ' ';
        }
    }
}

/* gets base file name from path or original file name if no path is supplied.
 * Returns the file name length
 */
size_t get_file_name(char *namebuf, size_t bufsize, const char *pathname)
{
    int len = strlen(pathname) - 1;
    char *path = strdup(pathname);

    if (path == NULL) {
        exit_toxic_err("failed in get_file_name", FATALERR_MEMORY);
    }

    while (len >= 0 && pathname[len] == '/') {
        path[len--] = '\0';
    }

    char *finalname = strdup(path);

    if (finalname == NULL) {
        exit_toxic_err("failed in get_file_name", FATALERR_MEMORY);
    }

    const char *basenm = strrchr(path, '/');

    if (basenm != NULL) {
        if (basenm[1]) {
            strcpy(finalname, &basenm[1]);
        }
    }

    snprintf(namebuf, bufsize, "%s", finalname);
    free(finalname);
    free(path);

    return strlen(namebuf);
}

/* Gets the base directory of path and puts it in dir.
 * dir must have at least as much space as path_len + 1.
 *
 * Returns the length of the base directory.
 */
size_t get_base_dir(const char *path, size_t path_len, char *dir)
{
    if (path_len == 0 || path == NULL) {
        return 0;
    }

    size_t dir_len = char_rfind(path, '/', path_len);

    if (dir_len != 0 && dir_len < path_len) {
        ++dir_len;    /* Leave trailing slash */
    }

    memcpy(dir, path, dir_len);
    dir[dir_len] = '\0';

    return dir_len;
}

/* converts str to all lowercase */
void str_to_lower(char *str)
{
    int i;

    for (i = 0; str[i]; ++i) {
        str[i] = tolower(str[i]);
    }
}

/* puts friendnum's nick in buf, truncating at TOXIC_MAX_NAME_LENGTH if necessary.
   if toxcore API call fails, put UNKNOWN_NAME in buf
   Returns nick len */
size_t get_nick_truncate(Tox *m, char *buf, uint32_t friendnum)
{
    Tox_Err_Friend_Query err;
    size_t len = tox_friend_get_name_size(m, friendnum, &err);

    if (err != TOX_ERR_FRIEND_QUERY_OK) {
        goto on_error;
    } else {
        if (!tox_friend_get_name(m, friendnum, (uint8_t *) buf, NULL)) {
            goto on_error;
        }
    }

    len = MIN(len, TOXIC_MAX_NAME_LENGTH - 1);
    buf[len] = '\0';
    filter_str(buf, len);
    return len;

on_error:
    strcpy(buf, UNKNOWN_NAME);
    len = strlen(UNKNOWN_NAME);
    buf[len] = '\0';
    return len;
}

/* same as get_nick_truncate but for groupchats */
int get_group_nick_truncate(Tox *m, char *buf, uint32_t peernum, uint32_t groupnum)
{
    Tox_Err_Conference_Peer_Query err;
    size_t len = tox_conference_peer_get_name_size(m, groupnum, peernum, &err);

    if (err != TOX_ERR_CONFERENCE_PEER_QUERY_OK) {
        goto on_error;
    } else {
        if (!tox_conference_peer_get_name(m, groupnum, peernum, (uint8_t *) buf, NULL)) {
            goto on_error;
        }
    }

    len = MIN(len, TOXIC_MAX_NAME_LENGTH - 1);
    buf[len] = '\0';
    filter_str(buf, len);
    return len;

on_error:
    strcpy(buf, UNKNOWN_NAME);
    len = strlen(UNKNOWN_NAME);
    buf[len] = '\0';
    return len;
}

/* copies data to msg buffer, removing return characters.
   returns length of msg, which will be no larger than size-1 */
size_t copy_tox_str(char *msg, size_t size, const char *data, size_t length)
{
    size_t i;
    size_t j = 0;

    for (i = 0; (i < length) && (j < size - 1); ++i) {
        if (data[i] != '\r') {
            msg[j++] = data[i];
        }
    }

    msg[j] = '\0';

    return j;
}

/* returns index of the first instance of ch in s starting at idx.
   returns length of s if char not found or 0 if s is NULL. */
int char_find(int idx, const char *s, char ch)
{
    if (!s) {
        return 0;
    }

    int i = idx;

    for (i = idx; s[i]; ++i) {
        if (s[i] == ch) {
            break;
        }
    }

    return i;
}

/* returns index of the last instance of ch in s starting at len.
   returns 0 if char not found or s is NULL (skips 0th index). */
int char_rfind(const char *s, char ch, int len)
{
    if (!s) {
        return 0;
    }

    int i = 0;

    for (i = len; i > 0; --i) {
        if (s[i] == ch) {
            break;
        }
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

/*
 * Checks the file type path points to and returns a File_Type enum value.
 *
 * Returns FILE_TYPE_DIRECTORY if path points to a directory.
 * Returns FILE_TYPE_REGULAR if path points to a regular file.
 * Returns FILE_TYPE_OTHER on any other result, including an invalid path.
 */
File_Type file_type(const char *path)
{
    struct stat s;
    stat(path, &s);

    switch (s.st_mode & S_IFMT) {
        case S_IFDIR:
            return FILE_TYPE_DIRECTORY;
        case S_IFREG:
            return FILE_TYPE_REGULAR;
        default:
            return FILE_TYPE_OTHER;
   }
}

/* returns file size. If file doesn't exist returns 0. */
off_t file_size(const char *path)
{
    struct stat st;

    if (stat(path, &st) == -1) {
        return 0;
    }

    return st.st_size;
}

/* compares the first size bytes of fp to signature.
   Returns 0 if they are the same, 1 if they differ, and -1 on error.

   On success this function will seek back to the beginning of fp */
int check_file_signature(const char *signature, size_t size, FILE *fp)
{
    char buf[size];

    if (fread(buf, size, 1, fp) != 1) {
        return -1;
    }

    int ret = memcmp(signature, buf, size);

    if (fseek(fp, 0L, SEEK_SET) == -1) {
        return -1;
    }

    return ret == 0 ? 0 : 1;
}

/* sets window title in tab bar. */
void set_window_title(ToxWindow *self, const char *title, int len)
{
    if (len <= 0 || !title) {
        return;
    }

    char cpy[TOXIC_MAX_NAME_LENGTH + 1];

    if (self->is_groupchat) { /* keep groupnumber in title */
        snprintf(cpy, sizeof(cpy), "%d %s", self->num, title);
    } else {
        snprintf(cpy, sizeof(cpy), "%s", title);
    }

    if (len > MAX_WINDOW_NAME_LENGTH) {
        strcpy(&cpy[MAX_WINDOW_NAME_LENGTH - 3], "...");
        cpy[MAX_WINDOW_NAME_LENGTH] = '\0';
    }

    snprintf(self->name, sizeof(self->name), "%s", cpy);
}

/* Return true if address appears to be a valid ipv4 address. */
bool is_ip4_address(const char *address)
{
    struct sockaddr_in s_addr;
    return inet_pton(AF_INET, address, &(s_addr.sin_addr)) != 0;
}

/* Return true if address roughly appears to be a valid ipv6 address.
 *
 * TODO: Improve this function (inet_pton behaves strangely with ipv6).
 * for now the only guarantee is that it won't return true if the
 * address is a domain or ipv4 address, and should only be used if you're
 * reasonably sure that the address is one of the three (ipv4, ipv6 or a domain).
 */
bool is_ip6_address(const char *address)
{
    size_t i;
    size_t num_colons = 0;
    char ch = 0;

    for (i = 0; (ch = address[i]); ++i) {
        if (ch == '.') {
            return false;
        }

        if (ch == ':') {
            ++num_colons;
        }
    }

    return num_colons > 1 && num_colons < 8;
}

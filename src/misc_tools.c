/*  misc_tools.c
 *
 *  Copyright (C) 2014-2024 Toxic All Rights Reserved.
 *
 *  This file is part of Toxic. Toxic is free software licensed
 *  under the GNU General Public License 3.0.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE    /* needed for strcasestr() */
#endif

#include <assert.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "file_transfers.h"
#include "friendlist.h"
#include "misc_tools.h"
#include "settings.h"
#include "toxic.h"
#include "windows.h"

void clear_screen(void)
{
    printf("\033[2J\033[1;1H");
}

void hst_to_net(uint8_t *num, uint16_t numbytes)
{
#ifndef WORDS_BIGENDIAN
    uint8_t *buff = malloc(numbytes);

    if (buff == NULL) {
        return;
    }

    for (uint32_t i = 0; i < numbytes; ++i) {
        buff[i] = num[numbytes - i - 1];
    }

    memcpy(num, buff, numbytes);
    free(buff);
#endif
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

/* Attempts to sleep the caller's thread for `usec` microseconds */
void sleep_thread(long int usec)
{
    struct timespec req;
    struct timespec rem;

    req.tv_sec = 0;
    req.tv_nsec = usec * 1000L;

    if (nanosleep(&req, &rem) == -1) {
        if (nanosleep(&rem, NULL) == -1) {
            fprintf(stderr, "nanosleep() returned -1\n");
        }
    }
}

/* Get the current local time */
struct tm *get_time(void)
{
    struct tm *timeinfo;
    time_t t = get_unix_time();
    timeinfo = localtime((const time_t *) &t);
    return timeinfo;
}

void get_time_str(char *buf, size_t bufsize, const char *format_string)
{
    if (buf == NULL || bufsize == 0) {
        return;
    }

    if (format_time_str(buf, bufsize, format_string, get_time()) > 0) {
        return;
    }

    if (format_time_str(buf, bufsize, TIMESTAMP_DEFAULT, get_time()) > 0) {
        return;
    }

    buf[0] = '\0';
}

void get_elapsed_time_str(char *buf, int bufsize, uint64_t elapsed_seconds)
{
    if (elapsed_seconds == 0) {
        snprintf(buf, bufsize, "<Invalid time format>");
        return;
    }

    long int seconds = elapsed_seconds % 60;
    long int minutes = (elapsed_seconds % 3600) / 60;
    long int hours = elapsed_seconds / 3600;

    if (minutes == 0 && hours == 0) {
        snprintf(buf, bufsize, "%.2ld", seconds);
    } else if (hours == 0) {
        snprintf(buf, bufsize, "%ld:%.2ld", minutes, seconds);
    } else {
        snprintf(buf, bufsize, "%ld:%.2ld:%.2ld", hours, minutes, seconds);
    }
}

void get_elapsed_time_str_alt(char *buf, int bufsize, uint64_t elapsed_seconds)
{
    if (elapsed_seconds == 0) {
        snprintf(buf, bufsize, "<Invalid time format>");
        return;
    }

    long int seconds = elapsed_seconds % 60;
    long int minutes = (elapsed_seconds % 3600) / 60;
    long int hours = elapsed_seconds / 3600;

    if (minutes == 0 && hours == 0) {
        snprintf(buf, bufsize, "%ld seconds", seconds);
    } else if (hours == 0) {
        snprintf(buf, bufsize, "%ld minutes, %ld seconds", minutes, seconds);
    } else {
        snprintf(buf, bufsize, "%ld hours, %ld minutes, %ld seconds", hours, minutes, seconds);
    }
}

/*
 * Converts a hexidecimal string representation of a Tox public key to binary format and puts
 * the result in output.
 *
 * `hex_len` must be exactly TOX_PUBLIC_KEY_SIZE * 2, and `output_size` must have room
 * for TOX_PUBLIC_KEY_SIZE bytes.
 *
 * Returns 0 on success.
 * Returns -1 on failure.
 */
int tox_pk_string_to_bytes(const char *hex_string, size_t hex_len, char *output, size_t output_size)
{
    if (output_size != TOX_PUBLIC_KEY_SIZE || hex_len != output_size * 2) {
        return -1;
    }

    for (size_t i = 0; i < output_size; ++i) {
        sscanf(hex_string, "%2hhx", (unsigned char *)&output[i]);
        hex_string += 2;
    }

    return 0;
}

/* Convert a hexadecimcal string of length `size` to bytes and puts the result in `keystr`.
 *
 * Returns 0 on success.
 * Returns -1 on failure.
 */
int hex_string_to_bytes(char *buf, int size, const char *keystr)
{
    if (size % 2 != 0) {
        return -1;
    }

    const char *pos = keystr;

    for (size_t i = 0; i < size; ++i) {
        int res = sscanf(pos, "%2hhx", (unsigned char *)&buf[i]);
        pos += 2;

        if (res == EOF || res < 1) {
            return -1;
        }
    }

    return 0;
}

/* Converts a binary representation of a Tox ID into a string.
 *
 * `bin_id_size` must be exactly TOX_ADDRESS_SIZE bytes in length, and
 * `output_size` must be at least TOX_ADDRESS_SIZE * 2 + 1.
 *
 * Returns 0 on success.
 * Returns -1 on failure.
 */
int tox_id_bytes_to_str(const char *bin_id, size_t bin_id_size, char *output, size_t output_size)
{
    if (bin_id_size != TOX_ADDRESS_SIZE || output_size < (TOX_ADDRESS_SIZE * 2 + 1)) {
        return -1;
    }

    for (size_t i = 0; i < TOX_ADDRESS_SIZE; ++i) {
        snprintf(&output[i * 2], output_size - (i * 2), "%02X", bin_id[i] & 0xff);
    }

    return 0;
}

/* Converts a binary representation of a Tox public key into a string.
 *
 * `bin_pubkey_size` must be exactly TOX_PUBLIC_KEY_SIZE bytes in size, and
 * `output_size` must be at least TOX_PUBLIC_KEY_SIZE * 2 + 1.
 *
 * Returns 0 on success.
 * Returns -1 on failure.
 */
int tox_pk_bytes_to_str(const uint8_t *bin_pubkey, size_t bin_pubkey_size, char *output, size_t output_size)
{
    if (bin_pubkey_size != TOX_PUBLIC_KEY_SIZE || output_size < (TOX_PUBLIC_KEY_SIZE * 2 + 1)) {
        return -1;
    }

    for (size_t i = 0; i < TOX_PUBLIC_KEY_SIZE; ++i) {
        snprintf(&output[i * 2], output_size - (i * 2), "%02X", bin_pubkey[i] & 0xff);
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

/* case-insensitive string compare function for use with qsort */
int qsort_ptr_char_array_helper(const void *str1, const void *str2)
{
    return strcasecmp(*(const char *const *)str1, *(const char *const *)str2);
}

/* List of characters we don't allow in nicks. */
static const char invalid_nick_chars[] = {':', '/', '\0', '\a', '\b', '\f', '\n', '\r', '\t', '\v'};

/* List of characters we don't allow in single-line strings. */
static const char invalid_string_chars[] = {'\0', '\a', '\b', '\f', '\n', '\r', '\t', '\v'};

/*
 * Returns true if the character `ch` is not in the supplied `invalid_chars` array.
 */
static bool is_valid_char(char ch, const char *const invalid_chars, size_t list_length)
{
    for (size_t i = 0; i < list_length; ++i) {
        if (invalid_chars[i] == ch) {
            return false;
        }
    }

    return true;
}

bool valid_nick(const char *nick)
{
    if (!nick[0] || nick[0] == ' ') {
        return false;
    }

    for (size_t i = 0; nick[i] != '\0'; ++i) {
        char ch = nick[i];

        if ((ch == ' ' && nick[i + 1] == ' ')
                || !is_valid_char(ch, invalid_nick_chars, sizeof(invalid_nick_chars))) {
            return false;
        }
    }

    return true;
}

void filter_string(char *str, size_t len, bool is_nick)
{
    const char *const invalid_chars = is_nick ? invalid_nick_chars : invalid_string_chars;
    const size_t list_length = is_nick ? sizeof(invalid_nick_chars) : sizeof(invalid_string_chars);

    for (size_t i = 0; i < len; ++i) {
        char ch = str[i];

        if (!is_valid_char(ch, invalid_chars, list_length)) {
            str[i] = ' ';
        }
    }
}

int get_file_name(char *namebuf, size_t bufsize, const char *pathname)
{
    int len = strlen(pathname) - 1;
    char *path = strdup(pathname);

    if (path == NULL) {
        return -1;
    }

    while (len >= 0 && pathname[len] == '/') {
        path[len] = '\0';
        --len;
    }

    char *finalname = strdup(path);

    if (finalname == NULL) {
        free(path);
        return -1;
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

    return (int)strlen(namebuf);
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

size_t get_nick_truncate(Tox *tox, char *buf, uint16_t buf_size, uint32_t friendnum)
{
    assert(buf_size > 0);

    Tox_Err_Friend_Query err;
    size_t len = tox_friend_get_name_size(tox, friendnum, &err);

    if (err != TOX_ERR_FRIEND_QUERY_OK) {
        goto on_error;
    } else {
        if (!tox_friend_get_name(tox, friendnum, (uint8_t *) buf, NULL)) {
            goto on_error;
        }
    }

    len = MIN(len, buf_size - 1);
    buf[len] = '\0';
    filter_string(buf, len, false);

    return len;

on_error:
    snprintf(buf, buf_size, "%s", UNKNOWN_NAME);
    return strlen(buf);
}

/* same as get_nick_truncate but for conferences */
int get_conference_nick_truncate(Tox *tox, char *buf, uint32_t peernum, uint32_t conferencenum)
{
    Tox_Err_Conference_Peer_Query err;
    size_t len = tox_conference_peer_get_name_size(tox, conferencenum, peernum, &err);

    if (err != TOX_ERR_CONFERENCE_PEER_QUERY_OK) {
        goto on_error;
    } else {
        if (!tox_conference_peer_get_name(tox, conferencenum, peernum, (uint8_t *) buf, NULL)) {
            goto on_error;
        }
    }

    len = MIN(len, TOXIC_MAX_NAME_LENGTH - 1);
    buf[len] = '\0';
    filter_string(buf, len, true);
    return len;

on_error:
    strcpy(buf, UNKNOWN_NAME);
    len = strlen(UNKNOWN_NAME);
    buf[len] = '\0';

    return len;
}

/* same as get_nick_truncate but for groupchats */
size_t get_group_nick_truncate(Tox *tox, char *buf, uint32_t peer_id, uint32_t groupnum)
{
    Tox_Err_Group_Peer_Query err;
    size_t len = tox_group_peer_get_name_size(tox, groupnum, peer_id, &err);

    if (err != TOX_ERR_GROUP_PEER_QUERY_OK || len == 0) {
        strcpy(buf, UNKNOWN_NAME);
        len = strlen(UNKNOWN_NAME);
    } else {
        tox_group_peer_get_name(tox, groupnum, peer_id, (uint8_t *) buf, &err);

        if (err != TOX_ERR_GROUP_PEER_QUERY_OK) {
            strcpy(buf, UNKNOWN_NAME);
            len = strlen(UNKNOWN_NAME);
        }
    }

    len = MIN(len, TOXIC_MAX_NAME_LENGTH - 1);
    buf[len] = '\0';

    filter_string(buf, len, true);

    return len;
}

/* same as get_group_nick_truncate() but for self. */
size_t get_group_self_nick_truncate(Tox *tox, char *buf, uint32_t groupnum)
{
    Tox_Err_Group_Self_Query err;
    size_t len = tox_group_self_get_name_size(tox, groupnum, &err);

    if (err != TOX_ERR_GROUP_SELF_QUERY_OK) {
        strcpy(buf, UNKNOWN_NAME);
        len = strlen(UNKNOWN_NAME);
    } else {
        tox_group_self_get_name(tox, groupnum, (uint8_t *) buf, &err);

        if (err != TOX_ERR_GROUP_SELF_QUERY_OK) {
            strcpy(buf, UNKNOWN_NAME);
            len = strlen(UNKNOWN_NAME);
        }
    }

    len = MIN(len, TOXIC_MAX_NAME_LENGTH - 1);
    buf[len] = '\0';

    filter_string(buf, len, true);

    return len;
}

size_t copy_tox_str(char *msg, size_t size, const char *data, size_t length)
{
    size_t j = 0;

    for (size_t i = 0; (i < length) && (j < size - 1); ++i) {
        const char ch = data[i];

        if (ch == '\t' || ch == '\v') {
            msg[j] = ' ';
            ++j;
        } else if (ch != '\r') {
            msg[j] = ch;
            ++j;
        }
    }

    msg[j] = '\0';

    return j;
}

/* returns index of the first instance of ch in s starting at idx.
   returns length of s if char not found or 0 if s is NULL. */
int char_find(int idx, const char *s, char ch)
{
    if (s == NULL) {
        return 0;
    }

    int i;

    for (i = idx; s[i] != '\0'; ++i) {
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
    if (s == NULL) {
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

    if (stat(path, &s) == -1) {
        return FILE_TYPE_OTHER;
    }

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

/* sets window title in tab bar. */
void set_window_title(ToxWindow *self, const char *title, int len)
{
    if (len <= 0 || !title) {
        return;
    }

    char cpy[TOXIC_MAX_NAME_LENGTH + 1];

    /* keep conferencenumber in title */
    if (self->type == WINDOW_TYPE_CONFERENCE || self->type == WINDOW_TYPE_GROUPCHAT) {
        snprintf(cpy, sizeof(cpy), "%u %s", self->num, title);
    } else {
        snprintf(cpy, sizeof(cpy), "%s", title);
    }

    if (len > MAX_WINDOW_NAME_LENGTH) {
        strcpy(&cpy[MAX_WINDOW_NAME_LENGTH - 3], "...");
        cpy[MAX_WINDOW_NAME_LENGTH] = '\0';
    }

    snprintf(self->name, sizeof(self->name), "%s", cpy);
}

/*
 * Frees all members of a pointer array plus `arr`.
 */
void free_ptr_array(void **arr)
{
    if (arr == NULL) {
        return;
    }

    void **tmp = arr;

    while (*arr != NULL) {
        free(*arr);
        ++arr;
    }

    free(tmp);
}

/*
 * Returns a null terminated array of `length` pointers. Each pointer is allocated `bytes` bytes.
 * Returns NULL on failure.
 *
 * The caller is responsible for freeing the array with `free_ptr_array`.
 */
void **malloc_ptr_array(size_t length, size_t bytes)
{
    void **arr = malloc((length + 1) * sizeof(void *));

    if (arr == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < length; ++i) {
        arr[i] = malloc(bytes);

        if (arr[i] == NULL) {
            free_ptr_array(arr);
            return NULL;
        }
    }

    arr[length] = NULL;

    return arr;
}

unsigned int rand_range_not_secure(unsigned int upper_bound)
{
    const unsigned int n = (unsigned int)rand();

    return n % MIN(RAND_MAX, upper_bound);
}

unsigned int rand_not_secure(void)
{
    const unsigned int n = (unsigned int)rand();

    return n;
}

int colour_string_to_int(const char *colour)
{
    if (strcasecmp(colour, "white") == 0) {
        return WHITE_BAR_FG;
    }

    if (strcasecmp(colour, "red") == 0) {
        return RED_BAR_FG;
    }

    if (strcasecmp(colour, "green") == 0) {
        return GREEN_BAR_FG;
    }

    if (strcasecmp(colour, "yellow") == 0) {
        return YELLOW_BAR_FG;
    }

    if (strcasecmp(colour, "cyan") == 0) {
        return CYAN_BAR_FG;
    }

    if (strcasecmp(colour, "magenta") == 0) {
        return MAGENTA_BAR_FG;
    }

    if (strcasecmp(colour, "black") == 0) {
        return BLACK_BAR_FG;
    }

    if (strcasecmp(colour, "blue") == 0) {
        return BLUE_BAR_FG;
    }

    if (strcasecmp(colour, "gray") == 0) {
        return GRAY_BAR_FG;
    }

    if (strcasecmp(colour, "orange") == 0) {
        return ORANGE_BAR_FG;
    }

    if (strcasecmp(colour, "pink") == 0) {
        return PINK_BAR_FG;
    }

    if (strcasecmp(colour, "brown") == 0) {
        return BROWN_BAR_FG;
    }

    return -1;
}

size_t format_time_str(char *s, size_t max, const char *format, const struct tm *tm)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
    return strftime(s, max, format, tm);
#pragma GCC diagnostic pop
}

bool string_contains_blocked_word(const char *line, const Client_Data *client_data)
{
    for (size_t i = 0; i < client_data->num_blocked_words; ++i) {
        if (strcasestr(line, client_data->blocked_words[i]) != NULL) {
            return true;
        }
    }

    return false;
}

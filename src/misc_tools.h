/*  misc_tools.h
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
#ifndef MISC_TOOLS_H
#define MISC_TOOLS_H

#include "windows.h"
#include "toxic.h"

#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

#ifndef MAX
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#endif

#ifndef net_to_host
#define net_to_host(x, y) hst_to_net(x, y)
#endif

void hst_to_net(uint8_t *num, uint16_t numbytes);

/* convert a hex string to binary */
char *hex_string_to_bin(const char *hex_string);

/* convert a hex string to bytes. returns 0 on success, -1 on failure */
int hex_string_to_bytes(char *buf, int size, const char *keystr);

/* get the current unix time */
uint64_t get_unix_time(void);

/* Puts the current time in buf in the format of [HH:mm:ss] */
void get_time_str(char *buf, int bufsize);

/* Converts seconds to string in format HH:mm:ss; truncates hours and minutes when necessary */
void get_elapsed_time_str(char *buf, int bufsize, uint64_t secs);

/* get the current local time */
struct tm *get_time(void);

/* updates current unix time (should be run once per do_toxic loop) */
void update_unix_time(void);

/* Returns 1 if the string is empty, 0 otherwise */
int string_is_empty(const char *string);

/* convert a multibyte string to a wide character string (must provide buffer) */
int char_to_wcs_buf(wchar_t *buf, const char *string, size_t n);

/* converts wide character string into a multibyte string and puts in buf. */
int wcs_to_mbs_buf(char *buf, const wchar_t *string, size_t n);

/* convert a multibyte string to a wide character string and puts in buf) */
int mbs_to_wcs_buf(wchar_t *buf, const char *string, size_t n);

/* Returns 1 if connection has timed out, 0 otherwise */
int timed_out(uint64_t timestamp, uint64_t timeout, uint64_t curtime);

/* Colours the window tab according to type. Beeps if is_beep is true */
void alert_window(ToxWindow *self, int type, bool is_beep);

/* case-insensitive string compare function for use with qsort */
int qsort_strcasecmp_hlpr(const void *str1, const void *str2);

/* Returns 1 if nick is valid, 0 if not. A valid toxic nick:
      - cannot be empty
      - cannot start with a space
      - must not contain a forward slash (for logfile naming purposes)
      - must not contain contiguous spaces */
int valid_nick(char *nick);

/* gets base file name from path or original file name if no path is supplied */
void get_file_name(char *namebuf, int bufsize, const char *pathname);

/* converts str to all lowercase */
void str_to_lower(char *str);

/* puts friendnum's nick in buf, truncating at TOXIC_MAX_NAME_LENGTH if necessary.
   Returns nick len on success, -1 on failure */
int get_nick_truncate(Tox *m, char *buf, int friendnum);

/* returns index of the first instance of ch in s starting at idx.
   returns length of s if char not found */
int char_find(int idx, const char *s, char ch);

/* returns index of the last instance of ch in s
   returns 0 if char not found */
int char_rfind(const char *s, char ch, int len);

/* Converts bytes to appropriate unit and puts in buf as a string */
void bytes_convert_str(char *buf, int size, uint64_t bytes);

/* checks if a file exists. Returns true or false */
bool file_exists(const char *path);

/* returns file size or -1 on error */
off_t file_size(const char *path);

/* compares the first size bytes of fp and signature. 
   Returns 0 if they are the same, 1 if they differ, and -1 on error.

   On success this function will seek back to the beginning of fp */
int check_file_signature(const char *signature, size_t size, FILE *fp);

#endif /* #define MISC_TOOLS_H */

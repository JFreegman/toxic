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
#ifndef _misc_tools_h
#define _misc_tools_h

#include "windows.h"
#include "toxic.h"

#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

#ifndef MAX
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#endif

/* convert a hex string to binary */
char *hex_string_to_bin(const char *hex_string);

/* get the current unix time */
uint64_t get_unix_time(void);

/*Puts the current time in buf in the format of [HH:mm:ss] */
void get_time_str(uint8_t *buf, int bufsize);

/* Converts seconds to string in format HH:mm:ss; truncates hours and minutes when necessary */
void get_elapsed_time_str(uint8_t *buf, int bufsize, uint64_t secs);

/* get the current local time */
struct tm *get_time(void);

/* updates current unix time (should be run once per do_toxic loop) */
void update_unix_time(void);

/* Returns 1 if the string is empty, 0 otherwise */
int string_is_empty(char *string);

/* convert a multibyte string to a wide character string (must provide buffer) */
int char_to_wcs_buf(wchar_t *buf, const uint8_t *string, size_t n);

/* converts wide character string into a multibyte string and puts in buf. */
int wcs_to_mbs_buf(uint8_t *buf, const wchar_t *string, size_t n);

/* convert a multibyte string to a wide character string and puts in buf) */
int mbs_to_wcs_buf(wchar_t *buf, const uint8_t *string, size_t n);

/* Returns 1 if connection has timed out, 0 otherwise */
int timed_out(uint64_t timestamp, uint64_t timeout, uint64_t curtime);

/* Colours the window tab according to type. Beeps if is_beep is true */
void alert_window(ToxWindow *self, int type, bool is_beep);

/* case-insensitive string compare function for use with qsort */
int qsort_strcasecmp_hlpr(const void *nick1, const void *nick2);

/* Returns 1 if nick is valid, 0 if not. A valid toxic nick:
      - cannot be empty
      - cannot start with a space
      - must not contain a forward slash (for logfile naming purposes)
      - must not contain contiguous spaces */
int valid_nick(uint8_t *nick);

/* gets base file name from path or original file name if no path is supplied */
void get_file_name(uint8_t *namebuf, uint8_t *pathname);

/* converts str to all lowercase */
void str_to_lower(uint8_t *str);

#endif /* #define _misc_tools_h */

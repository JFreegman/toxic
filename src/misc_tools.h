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

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#define MAX(x, y) (((x) > (y)) ? (x) : (y))

/* convert a hex string to binary */
unsigned char *hex_string_to_bin(char hex_string[]);

/* get the current unix time */
uint64_t get_unix_time(void);

/* Prints the time to given window */
void print_time(WINDOW *window);

/* get the current local time */
struct tm *get_time(void);

/* updates current unix time (should be run once per do_toxic loop) */
void update_unix_time(void);

/* Returns 1 if the string is empty, 0 otherwise */
int string_is_empty(char *string);

/* convert a multibyte string to a wide character string (must provide buffer) */
int char_to_wcs_buf(wchar_t *buf, const uint8_t *string, size_t n);

/* converts wide character string into a multibyte string.
   Same thing as wcs_to_mbs() but caller must provide its own buffer */
int wcs_to_mbs_buf(uint8_t *buf, const wchar_t *string, size_t n);

/* convert wide characters to multibyte string: string returned must be free'd */
uint8_t *wcs_to_mbs(wchar_t *string);

/* convert a wide char to multibyte char */
char *wc_to_char(wchar_t ch);

/* Returns 1 if connection has timed out, 0 otherwise */
int timed_out(uint64_t timestamp, uint64_t timeout, uint64_t curtime);

/* Colours the window tab according to type. Beeps if is_beep is true */
void alert_window(ToxWindow *self, int type, bool is_beep);

/* case-insensitive string compare function for use with qsort */
int qsort_strcasecmp_hlpr(const void *nick1, const void *nick2);

/* Returns true if nick is valid. A valid toxic nick:
      - cannot be empty
      - cannot start with a space
      - must not contain contiguous spaces */
bool valid_nick(uint8_t *nick);

/* Moves the cursor to the end of the line in given window */
void mv_curs_end(WINDOW *w, size_t len, int max_y, int max_x);

/* gets base file name from path or original file name if no path is supplied */
void get_file_name(uint8_t *pathname, uint8_t *namebuf);

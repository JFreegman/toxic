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

#include <sys/stat.h>

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

#define UNUSED_VAR(x) ((void) x)

typedef enum File_Type {
    FILE_TYPE_REGULAR,
    FILE_TYPE_DIRECTORY,
    FILE_TYPE_OTHER,
} File_Type;


void clear_screen(void);

void hst_to_net(uint8_t *num, uint16_t numbytes);

/*
 * Converts a hexidecimal string of length hex_len to binary format and puts the result in output.
 * output_size must be exactly half of hex_len.
 *
 * Returns 0 on success.
 * Returns -1 on failure.
 */
int hex_string_to_bin(const char *hex_string, size_t hex_len, char *output, size_t output_size);

/* convert a hex string to bytes. returns 0 on success, -1 on failure */
int hex_string_to_bytes(char *buf, int size, const char *keystr);

/* Converts a binary representation of a Tox ID into a string.
 *
 * Returns 0 on success.
 * Returns -1 on failure.
 */
int bin_id_to_string(const char *bin_id, size_t bin_id_size, char *output, size_t output_size);

/* get the current unix time (not thread safe) */
time_t get_unix_time(void);

/* Puts the current time in buf in the format of [HH:mm:ss] (not thread safe) */
void get_time_str(char *buf, int bufsize);

/* Converts seconds to string in format HH:mm:ss; truncates hours and minutes when necessary */
void get_elapsed_time_str(char *buf, int bufsize, time_t secs);

/* get the current local time (not thread safe) */
struct tm *get_time(void);

/* Returns 1 if the string is empty, 0 otherwise */
int string_is_empty(const char *string);

/* Same as above but for wide character strings */
int wstring_is_empty(const wchar_t *string);

/* converts a multibyte string to a wide character string (must provide buffer) */
int char_to_wcs_buf(wchar_t *buf, const char *string, size_t n);

/* converts wide character string into a multibyte string and puts in buf. */
int wcs_to_mbs_buf(char *buf, const wchar_t *string, size_t n);

/* converts a multibyte string to a wide character string and puts in buf) */
int mbs_to_wcs_buf(wchar_t *buf, const char *string, size_t n);

/* Returns 1 if connection has timed out, 0 otherwise */
int timed_out(time_t timestamp, time_t timeout);

/* Colours the window tab according to type. Beeps if is_beep is true */
void alert_window(ToxWindow *self, int type, bool is_beep);

/* case-insensitive string compare function for use with qsort */
int qsort_strcasecmp_hlpr(const void *str1, const void *str2);

/* Returns 1 if nick is valid, 0 if not. A valid toxic nick:
      - cannot be empty
      - cannot start with a space
      - must not contain a forward slash (for logfile naming purposes)
      - must not contain contiguous spaces
      - must not contain a newline or tab seqeunce */
int valid_nick(const char *nick);

/* Converts all newline/tab chars to spaces (use for strings that should be contained to a single line) */
void filter_str(char *str, size_t len);

/* gets base file name from path or original file name if no path is supplied */
size_t get_file_name(char *namebuf, size_t bufsize, const char *pathname);

/* Gets the base directory of path and puts it in dir.
 * dir must have at least as much space as path_len.
 *
 * Returns the length of the base directory on success.
 * Returns -1 on failure.
 */
size_t get_base_dir(const char *path, size_t path_len, char *dir);

/* converts str to all lowercase */
void str_to_lower(char *str);

/* puts friendnum's nick in buf, truncating at TOXIC_MAX_NAME_LENGTH if necessary.
   Returns nick len on success, -1 on failure */
size_t get_nick_truncate(Tox *m, char *buf, uint32_t friendnum);

/* same as get_nick_truncate but for groupchats */
int get_group_nick_truncate(Tox *m, char *buf, uint32_t peernum, uint32_t groupnum);

/* copies data to msg buffer.
   returns length of msg, which will be no larger than size-1 */
size_t copy_tox_str(char *msg, size_t size, const char *data, size_t length);

/* returns index of the first instance of ch in s starting at idx.
   returns length of s if char not found or 0 if s is NULL. */
int char_find(int idx, const char *s, char ch);

/* returns index of the last instance of ch in s starting at len.
   returns 0 if char not found or s is NULL (skips 0th index). */
int char_rfind(const char *s, char ch, int len);

/* Converts bytes to appropriate unit and puts in buf as a string */
void bytes_convert_str(char *buf, int size, uint64_t bytes);

/* checks if a file exists. Returns true or false */
bool file_exists(const char *path);

/*
 * Checks the file type path points to and returns a File_Type enum value.
 *
 * Returns FILE_TYPE_DIRECTORY if path points to a directory.
 * Returns FILE_TYPE_REGULAR if path points to a regular file.
 * Returns FILE_TYPE_OTHER on any other result, including an invalid path.
 */
File_Type file_type(const char *path);

/* returns file size. If file doesn't exist returns 0. */
off_t file_size(const char *path);

/* Compares the first size bytes of fp to signature.
 *
 * Returns 0 if they are the same
 * Returns 1 if they differ
 * Returns -1 on error.
 *
 * On success this function will seek back to the beginning of fp.
 */
int check_file_signature(const unsigned char *signature, size_t size, FILE *fp);

/* sets window title in tab bar. */
void set_window_title(ToxWindow *self, const char *title, int len);

/* Return true if address appears to be a valid ipv4 address. */
bool is_ip4_address(const char *address);

/* Return true if address roughly appears to be a valid ipv6 address.
 *
 * TODO: Improve this function (inet_pton behaves strangely with ipv6).
 * for now the only guarantee is that it won't return true if the
 * address is a domain or ipv4 address, and should only be used if you're
 * reasonably sure that the address is one of the three (ipv4, ipv6 or a domain).
 */
bool is_ip6_address(const char *address);


/*
 * Frees `length` members of pointer array `arr` and frees `arr`.
 */
void free_ptr_array(void **arr, size_t length);

/*
 * Returns a new array of `length` pointers of size `ptr_size`. Each pointer is allocated `bytes` bytes.
 * Returns NULL on failure.
 *
 * The caller is responsible for freeing the array with `free_ptr_array`.
 */
void **malloc_ptr_array(size_t length, size_t bytes, size_t ptr_size);

#endif /* MISC_TOOLS_H */

/*  misc_tools.h
 *
 *  Copyright (C) 2014-2024 Toxic All Rights Reserved.
 *
 *  This file is part of Toxic. Toxic is free software licensed
 *  under the GNU General Public License 3.0.
 */
#ifndef MISC_TOOLS_H
#define MISC_TOOLS_H

#include <sys/stat.h>
#include <sys/types.h>

#include "toxic.h"
#include "windows.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

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
 * Converts a hexidecimal string representation of a Tox public key to binary format and puts
 * the result in output.
 *
 * `hex_len` must be exactly TOX_PUBLIC_KEY_SIZE * 2, and `output_size` must have room
 * for TOX_PUBLIC_KEY_SIZE bytes.
 *
 * Returns 0 on success.
 * Returns -1 on failure.
 */
int tox_pk_string_to_bytes(const char *hex_string, size_t hex_len, char *output, size_t output_size);

/* Converts a binary representation of a Tox public key into a string.
 *
 * `bin_pubkey_size` must be exactly TOX_PUBLIC_KEY_SIZE bytes in size, and
 * `output_size` must be at least TOX_PUBLIC_KEY_SIZE * 2 + 1.
 *
 * Returns 0 on success.
 * Returns -1 on failure.
 */
int tox_pk_bytes_to_str(const uint8_t *bin_pubkey, size_t bin_pubkey_size, char *output, size_t output_size);

/* Convert a hexadecimcal string of length `size` to bytes and puts the result in `keystr`.
 *
 * Returns 0 on success.
 * Returns -1 on failure.
 */
int hex_string_to_bytes(char *buf, int size, const char *keystr);

/* Converts a binary representation of a Tox ID into a string.
 *
 * `bin_id_size` must be exactly TOX_ADDRESS_SIZE bytes in length, and
 * `output_size` must be at least TOX_ADDRESS_SIZE * 2 + 1.
 *
 * Returns 0 on success.
 * Returns -1 on failure.
 */
int tox_id_bytes_to_str(const char *bin_id, size_t bin_id_size, char *output, size_t output_size);

/* get the current unix time (not thread safe) */
time_t get_unix_time(void);

/* Puts the current time in `buf` in the format of specified by `format_string`.
 *
 * If the passed format string is invalid, a default format will be tried. If that
 * fails, `buf` will be set to nul.
 */
void get_time_str(char *buf, size_t bufsize, const char *format_string);

/* Converts `elapsed_seconds` to string in format "H hours, m minutes, s seconds" and puts the resulting
 * string in `buf`.
 *
 * If `elapsed_seconds` is zero, an error message is placed in `buf`.
 */
void get_elapsed_time_str_alt(char *buf, int bufsize, uint64_t elapsed_seconds);

/* Converts `elapsed_seconds` to string in format "HH:mm:ss"; truncates hours and minutes when necessary.
 * Puts the resulting string in `buf.
 *
 * If `elapsed_seconds` is zero, an error message is placed in `buf`.
 */
void get_elapsed_time_str(char *buf, int bufsize, uint64_t elapsed_seconds);

/* get the current local time (not thread safe) */
struct tm *get_time(void);

/* Returns 1 if the string is empty, 0 otherwise */
int string_is_empty(const char *string);

/* Same as above but for wide character strings */
int wstring_is_empty(const wchar_t *string);

/* converts a multibyte string to a wide character string (must provide buffer) */
int char_to_wcs_buf(wchar_t *buf, const char *string, size_t n);

/* Converts a multibyte string to a wide character string and puts in `buf`.
 *
 * `buf` must have room for at least `n` wide characters (wchar_t's).
 *
 * Return number of wide characters written on success.
 * Return -1 on failure.
 */
int mbs_to_wcs_buf(wchar_t *buf, const char *string, size_t n);

/* Converts a wide character string into a multibyte string and puts in `buf`.
 *
 * `buf` must have room for at least `n` bytes.
 *
 * Return number of bytes written on success.
 * Return -1 on failure.
 */
int wcs_to_mbs_buf(char *buf, const wchar_t *string, size_t n);

/* Returns 1 if connection has timed out, 0 otherwise */
int timed_out(time_t timestamp, time_t timeout);

/* Attempts to sleep the caller's thread for `usec` microseconds */
void sleep_thread(long int usec);

/* Colours the window tab according to type. Beeps if is_beep is true */
void alert_window(ToxWindow *self, int type, bool is_beep);

/* case-insensitive string compare function for use with qsort */
int qsort_strcasecmp_hlpr(const void *str1, const void *str2);

/* case-insensitive string compare function for use with qsort */
int qsort_ptr_char_array_helper(const void *str1, const void *str2);

/* Returns true if nick is valid.
 *
 * A valid toxic nick:
 * - cannot be empty
 * - cannot start with a space
 * - cannot contain contiguous spaces
 * - cannot contain forward slash (for log file naming purposes)
 * - cannot contain any characters in the `invalid_nick_chars` array that
 *   resides in misc_tools.c.
 */
bool valid_nick(const char *nick);

/* Converts all invalid single-line string characters to spaces (newlines,
 * tabs, nul etc.).
 *
 * If `is_nick` is true, additional characters specific to nicks are filtered.
 *
 * This function is used for strings that should be human-readable, and contained
 * to a single line, such as status messages, nicks, and group topics.
 */
void filter_string(char *str, size_t len, bool is_nick);

/* Gets base file name from path or original file name if no path is supplied.
 *
 * Returns the file name length on success.
 * Returns -1 on failure (OOM).
 */
int get_file_name(char *namebuf, size_t bufsize, const char *pathname);

/* Gets the base directory of path and puts it in dir.
 * dir must have at least as much space as path_len.
 *
 * Returns the length of the base directory on success.
 * Returns -1 on failure.
 */
size_t get_base_dir(const char *path, size_t path_len, char *dir);

/* converts str to all lowercase */
void str_to_lower(char *str);

/* Gets the name designated by `friendnum` via tox API call and puts it in buf,
 * truncating the name at `buf_size`
 *
 * If the toxcore API call fails, put UNKNOWN_NAME in buf.
 *
 * Note: This function should only be used when initially adding a new friend. In all
 * other instances, use the `get_friend_name()` function from friendlist.h.
 *
 * Returns the length of the resulting nick.
 */
size_t get_nick_truncate(Tox *tox, char *buf, uint16_t buf_size, uint32_t friendnum);

/* same as get_nick_truncate but for conferences */
int get_conference_nick_truncate(Tox *tox, char *buf, uint32_t peernum, uint32_t conferencenum);

/* same as get_nick_truncate but for groupchats */
size_t get_group_nick_truncate(Tox *tox, char *buf, uint32_t peer_id, uint32_t groupnum);

/* same as get_group_nick_truncate() but for self. */
size_t get_group_self_nick_truncate(Tox *tox, char *buf, uint32_t groupnum);

/* Copies up to `size` bytes from the `data` string of length `length` to the `msg` buffer,
 * replacing all \t and \v bytes with spaces, and removing all \r bytes.
 *
 * Returns the length of the resulting string in `msg`, which is guaranteed to be NUL-terminated.
 */
size_t copy_tox_str(char *msg, size_t size, const char *data, size_t length);

/* returns index of the first instance of ch in s starting at idx.
   returns length of s if char not found or 0 if s is NULL. */
int char_find(int idx, const char *s, char ch);

/* returns index of the last instance of ch in s starting at len.
   returns 0 if char not found or s is NULL (skips 0th index). */
int char_rfind(const char *s, char ch, int len);

/* Converts bytes to appropriate unit and puts in buf as a string */
void bytes_convert_str(char *buf, int size, uint64_t bytes);

/* Returns true if the file pointed to by `path` exists. */
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

/* sets window title in tab bar. */
void set_window_title(ToxWindow *self, const char *title, int len);

/*
 * Frees all members of a pointer array plus `arr`.
 */
void free_ptr_array(void **arr);

/*
 * Returns a null terminated array of `length` pointers. Each pointer is allocated `bytes` bytes.
 * Returns NULL on failure.
 *
 * The caller is responsible for freeing the array with `free_ptr_array`.
 */
void **malloc_ptr_array(size_t length, size_t bytes);

/*
 * Returns a non-cryptographically secure random unsigned integer between zero and `upper_bound`
 * which is limited by RAND_MAX.
 *
 * This function should only be used for non-crypto related things.
 */
unsigned int rand_range_not_secure(unsigned int upper_bound);

/*
 * Returns a non-cryptographically secure random unsigned integer between zero and RAND_MAX.
 *
 * This function should only be used for non-crypto related things.
 */
unsigned int rand_not_secure(void);

/*
 * Returns an integer associated with an ncurses foreground colour, per the COLOUR_PAIR enum
 * in windows.h.
 *
 * Valid colour strings are: black, white, gray, brown, red, green, blue, cyan, yellow, magenta,
 *   orange, pink
 *
 * Returns -1 if colour is invalid.
 */
int colour_string_to_int(const char *colour);

/*
 * A wrapper for strftime() from time.h. We use this wrapper to suppress
 * gcc compiler warnings produced by the -Wformat-nonliteral flag.
 */
size_t format_time_str(char *s, size_t max, const char *format, const struct tm *tm);

/*
 * Returns true if `line` contains a word that's in the client's blocked words list.
 */
bool string_contains_blocked_word(const char *line, const Client_Data *client_data);

#ifdef __cplusplus
} /* extern "C" */

#endif /* __cplusplus */

#endif /* MISC_TOOLS_H */

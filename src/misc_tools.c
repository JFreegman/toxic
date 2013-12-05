/*
 * Toxic -- Tox Curses Client
 */

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>

#include "toxic_windows.h"
#include "misc_tools.h"

// XXX: FIX
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

/* Get the current local time */
struct tm *get_time(void)
{
    struct tm *timeinfo;
    time_t now;
    time(&now);
    timeinfo = localtime(&now);
    return timeinfo;
}

/* Prints the time to given window */
void print_time(WINDOW *window)
{
    struct tm *timeinfo = get_time();

    wattron(window, COLOR_PAIR(BLUE));
    wprintw(window, "[%02d:%02d:%02d] ", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    wattroff(window,COLOR_PAIR(BLUE));
}

/* Returns 1 if the string is empty, 0 otherwise */
int string_is_empty(char *string)
{
    return string[0] == '\0';
}

/* convert wide characters to null terminated string */
uint8_t *wcs_to_char(wchar_t *string)
{
    uint8_t *ret = NULL;
    size_t len = wcstombs(NULL, string, 0);

    if (len != (size_t) -1) {
        ret = malloc(++len);

        if (ret != NULL)
            wcstombs(ret, string, len);
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

/* convert a wide char to null terminated string */
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

/* Returns true if connection has timed out, false otherwise */
bool timed_out(uint64_t timestamp, uint64_t curtime, uint64_t timeout)
{
    return timestamp + timeout <= curtime;
}

/* Colours the window tab according to type. Beeps if is_beep is true */
void alert_window(ToxWindow *self, int type, bool is_beep)
{
    if (type == WINDOW_ALERT_1)
        self->alert1 = true;
    else if(type == WINDOW_ALERT_2)
        self->alert2 = true;
    
    if (is_beep)
        beep();
}

/* case-insensitive string compare function for use with qsort - same return logic as strcmp */
int name_compare(const void *nick1, const void *nick2)
{
    char s[TOX_MAX_NAME_LENGTH];
    char t[TOX_MAX_NAME_LENGTH];
    strcpy(s, (const char *) nick1);
    strcpy(t, (const char *) nick2);

    int i;

    for (i = 0; s[i] && t[i]; ++i) {
        s[i] = tolower(s[i]);
        t[i] = tolower(t[i]);

        if (s[i] != t[i])
            break;
    }

    return s[i] - t[i];
}

/* Returns true if nick is valid. A valid toxic nick:
      - cannot be empty
      - cannot start with a space
      - must not contain contiguous spaces */
bool valid_nick(uint8_t *nick)
{
    if (!nick[0] || nick[0] == ' ')
        return false;

    int i;

    for (i = 0; nick[i]; ++i) {
        if (nick[i] == ' ' && nick[i+1] == ' ')
            return false;
    }

    return true;
}

/*
 * Buffer helper tools. 
 * Assumes buffers are no larger than MAX_STR_SIZE and are always null terminated at len
 */

/* Adds char to buffer at pos */
void add_char_to_buf(wchar_t *buf, size_t *pos, size_t *len, wint_t ch)
{
    if (*pos < 0 || *len >= MAX_STR_SIZE)
        return;

    /* move all chars including null in front of pos one space forward and insert char in pos */
    int i;

    for (i = *len; i >= *pos && i >= 0; --i)
        buf[i+1] = buf[i];

    buf[(*pos)++] = ch;
    ++(*len);
}

/* Deletes the character before pos */
void del_char_buf_bck(wchar_t *buf, size_t *pos, size_t *len)
{
    if (*pos <= 0)
        return;

    int i;

    /* similar to add_char_to_buf but deletes a char */
    for (i = *pos-1; i <= *len; ++i)
        buf[i] = buf[i+1];

    --(*pos);
    --(*len);
}

/* Deletes the character at pos */
void del_char_buf_frnt(wchar_t *buf, size_t *pos, size_t *len)
{
    if (*pos < 0 || *pos >= *len)
        return;

    int i;

    for (i = *pos; i < *len; ++i)
        buf[i] = buf[i+1];

    --(*len);
}

/* nulls buf and sets pos and len to 0 */
void reset_buf(wchar_t *buf, size_t *pos, size_t *len)
{
    buf[0] = L'\0';
    *pos = 0;
    *len = 0;
}

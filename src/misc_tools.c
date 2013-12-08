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

/* convert a multibyte string to a wide character string (must provide buffer) */
int char_to_wcs_buf(wchar_t *buf, const uint8_t *string, size_t n)
{
    size_t len = mbstowcs(NULL, string, 0) + 1;

    if (n < len)
        return -1;

    if ((len = mbstowcs(buf, string, n)) == (size_t) -1)
        return -1;

    return len;
}

/* converts wide character string into a multibyte string.
   Same thing as wcs_to_mbs() but caller must provide its own buffer */
int wcs_to_mbs_buf(uint8_t *buf, const wchar_t *string, size_t n)
{
    size_t len = wcstombs(NULL, string, 0) + 1;

    if (n < len)
        return -1;

    if ((len = wcstombs(buf, string, n)) == (size_t) -1)
        return -1;

    return len;
}

/* convert wide characters to multibyte string: string returned must be free'd */
uint8_t *wcs_to_mbs(wchar_t *string)
{
    uint8_t *ret = NULL;
    size_t len = wcstombs(NULL, string, 0);

    if (len != (size_t) -1) {
        ret = malloc(++len);

        if (ret != NULL) {
            if (wcstombs(ret, string, len) == (size_t) -1)
                return NULL;
        }
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

/* convert a wide char to multibyte string */
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
    
    if (is_beep)
        beep();
}

/* case-insensitive string compare function for use with qsort */
int qsort_strcasecmp_hlpr(const void *nick1, const void *nick2)
{
    return strcasecmp((const char *) nick1, (const char *) nick2);
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

/* Deletes the line from beginning to pos */
void discard_buf(wchar_t *buf, size_t *pos, size_t *len)
{
    if (*pos <= 0)
        return;

    int i;
    int c = 0;

    for (i = *pos; i <= *len; ++i)
        buf[c++] = buf[i];

    *pos = 0;
    *len = c - 1;
}

/* Deletes the line from pos to len */
void kill_buf(wchar_t *buf, size_t *pos, size_t *len)
{
    if (*len == *pos)
        return;

    buf[*pos] = L'\0';
    *len = *pos;
}

/* nulls buf and sets pos and len to 0 */
void reset_buf(wchar_t *buf, size_t *pos, size_t *len)
{
    buf[0] = L'\0';
    *pos = 0;
    *len = 0;
}

/* looks for the first instance in list that begins with the last entered word in buf according to pos, 
   then fills buf with the complete word. e.g. "Hello jo" would complete the buffer 
   with "Hello john".

   list is a pointer to the list of strings being compared, n_items is the number of items
   in the list, and size is the size of each item in the list. 

   Returns the difference between the old len and new len of buf on success, -1 if error */
int complete_line(wchar_t *buf, size_t *pos, size_t *len, const uint8_t *list, int n_items, int size)
{
    if (*pos <= 0 || *len <= 0 || *len >= MAX_STR_SIZE)
        return -1;

    uint8_t ubuf[MAX_STR_SIZE];
    /* work with multibyte string copy of buf for simplicity */
    if (wcs_to_mbs_buf(ubuf, buf, MAX_STR_SIZE) == -1)
        return -1;

    /* isolate substring from space behind pos to pos */
    uint8_t tmp[MAX_STR_SIZE];
    snprintf(tmp, sizeof(tmp), "%s", ubuf);
    tmp[*pos] = '\0';
    uint8_t *sub = strrchr(tmp, ' ');
    int n_endchrs = 1;    /* 1 = append space to end of match, 2 = append ": " */   

    if (!sub++) {
        sub = tmp;
        if (sub[0] != '/')    /* make sure it's not a command */
            n_endchrs = 2;
    }

    int s_len = strlen(sub);
    const uint8_t *match;
    bool is_match = false;
    int i;

    /* look for a match in list */
    for (i = 0; i < n_items; ++i) {
        match = &list[i*size];
        if (is_match = strncasecmp(match, sub, s_len) == 0)
            break;
    }

    if (!is_match)
        return -1;

    /* put match in correct spot in buf and append endchars (space or ": ") */
    const uint8_t *endchrs = n_endchrs == 1 ? " " : ": ";
    int m_len = strlen(match);
    int strt = (int) *pos - s_len;
    int diff = m_len - s_len + n_endchrs;

    if (*len + diff > MAX_STR_SIZE)
        return -1;

    uint8_t tmpend[MAX_STR_SIZE];
    strcpy(tmpend, &ubuf[*pos]);
    strcpy(&ubuf[strt], match);
    strcpy(&ubuf[strt+m_len], endchrs);
    strcpy(&ubuf[strt+m_len+n_endchrs], tmpend);

    /* convert to widechar and copy back to original buf */
    wchar_t newbuf[MAX_STR_SIZE];

    if (char_to_wcs_buf(newbuf, ubuf, MAX_STR_SIZE) == -1)
        return -1;

    wmemcpy(buf, newbuf, MAX_STR_SIZE);

    *len += (size_t) diff;
    *pos += (size_t) diff;

    return diff;
}

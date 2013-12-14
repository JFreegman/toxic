/*
 * Toxic -- Tox Curses Client
 */

#include <stdlib.h>
#include <string.h>

#include "toxic_windows.h"
#include "misc_tools.h"
#include "toxic_strings.h"

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

#define HIST_PURGE MAX_LINE_HIST / 4

/* shifts hist items back and makes room for HIST_PURGE new entries */
static void shift_hist_back(wchar_t (*hst)[MAX_STR_SIZE], int *hst_tot)
{
    int i;
    int n = MAX_LINE_HIST - HIST_PURGE;

    for (i = 0; i < n; ++i)
        wmemcpy(hst[i], hst[i+HIST_PURGE], MAX_STR_SIZE);

    *hst_tot = n;
}

/* adds a line to the ln_history buffer at hst_pos and sets hst_pos to end of history. */
void add_line_to_hist(const wchar_t *buf, size_t len, wchar_t (*hst)[MAX_STR_SIZE], int *hst_tot, 
                      int *hst_pos)
{
    if (len > MAX_STR_SIZE)
        return;

    if (*hst_tot >= MAX_LINE_HIST)
        shift_hist_back(hst, hst_tot);

    ++(*hst_tot);
    *hst_pos = *hst_tot;

    wmemcpy(hst[*hst_tot-1], buf, len + 1);
}

/* copies history item at hst_pos to buf. Sets pos and len to the len of the history item.
   hst_pos is decremented or incremented depending on key_dir. */
void fetch_hist_item(wchar_t *buf, size_t *pos, size_t *len, wchar_t (*hst)[MAX_STR_SIZE],
                     int hst_tot, int *hst_pos, int key_dir)
{
    if (key_dir == LN_HIST_MV_UP) {
        if (--(*hst_pos) < 0) {
            ++(*hst_pos);
            beep();
        }
    } else {
        if (++(*hst_pos) > hst_tot) {
            --(*hst_pos);
            discard_buf(buf, pos, len);
            return;
        }
    }

    const wchar_t *hst_line = hst[*hst_pos];
    size_t h_len = wcslen(hst_line);

    wmemcpy(buf, hst_line, h_len + 1);

    *pos = h_len;
    *len = h_len;
}

/* looks for the first instance in list that begins with the last entered word in buf according to pos, 
   then fills buf with the complete word. e.g. "Hello jo" would complete the buffer 
   with "Hello john".

   list is a pointer to the list of strings being compared, n_items is the number of items
   in the list, and size is the size of each item in the list. 

   Returns the difference between the old len and new len of buf on success, -1 if error */
int complete_line(wchar_t *buf, size_t *pos, size_t *len, const void *list, int n_items, int size)
{
    if (*pos <= 0 || *len <= 0 || *len >= MAX_STR_SIZE)
        return -1;

    const uint8_t *L = (uint8_t *) list;

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

    if (string_is_empty(sub))
        return -1;

    int s_len = strlen(sub);
    const uint8_t *match;
    bool is_match = false;
    int i;

    /* look for a match in list */
    for (i = 0; i < n_items; ++i) {
        match = &L[i*size];
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

    wcscpy(buf, newbuf);

    *len += (size_t) diff;
    *pos += (size_t) diff;

    return diff;
}

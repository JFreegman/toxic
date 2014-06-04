/*  toxic_strings.h
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

/* Adds char to buffer at pos */
void add_char_to_buf(wchar_t *buf, size_t *pos, size_t *len, wint_t ch);

/* Deletes the character before pos */
void del_char_buf_bck(wchar_t *buf, size_t *pos, size_t *len);

/* Deletes the character at pos */
void del_char_buf_frnt(wchar_t *buf, size_t *pos, size_t *len);

/* Deletes the line from beginning to pos */
void discard_buf(wchar_t *buf, size_t *pos, size_t *len);

/* Deletes the line from pos to len */
void kill_buf(wchar_t *buf, size_t *pos, size_t *len);

/* nulls buf and sets pos and len to 0 */
void reset_buf(wchar_t *buf, size_t *pos, size_t *len);

/* Removes trailing spaces from buf. */
void rm_trailing_spaces_buf(wchar_t *buf, size_t *pos, size_t *len);

/* looks for the first instance in list that begins with the last entered word in buf according to pos,
   then fills buf with the complete word. e.g. "Hello jo" would complete the buffer
   with "Hello john".

   list is a pointer to the list of strings being compared, n_items is the number of items
   in the list, and size is the size of each item in the list.

   Returns the difference between the old len and new len of buf on success, -1 if error */
int complete_line(wchar_t *buf, size_t *pos, size_t *len, const void *list, int n_items, int size);

/* adds a line to the ln_history buffer at hst_pos and sets hst_pos to last history item. */
void add_line_to_hist(const wchar_t *buf, size_t len, wchar_t (*hst)[MAX_STR_SIZE], int *hst_tot,
                      int *hst_pos);

/* copies history item at hst_pos to buf. Sets pos and len to the len of the history item.
   hst_pos is decremented or incremented depending on key_dir. */
void fetch_hist_item(wchar_t *buf, size_t *pos, size_t *len, wchar_t (*hst)[MAX_STR_SIZE],
                     int hst_tot, int *hst_pos, int key_dir);

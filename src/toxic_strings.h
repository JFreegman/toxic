/*  toxic_strings.h
 *
 *
 *  Copyright (C) 2024 Toxic All Rights Reserved.
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

#ifndef TOXIC_STRINGS_H
#define TOXIC_STRINGS_H

#include "windows.h"

/* Adds char to line at pos. Return 0 on success, -1 if line buffer is full */
int add_char_to_buf(ChatContext *ctx, wint_t ch);

/* Deletes the character before pos. Return 0 on success, -1 if nothing to delete */
int del_char_buf_bck(ChatContext *ctx);

/* Deletes the character at pos. Return 0 on success, -1 if nothing to delete. */
int del_char_buf_frnt(ChatContext *ctx);

/* Deletes the line from beginning to pos and puts discarded portion in yank buffer.
   Return 0 on success, -1 if noting to discard */
int discard_buf(ChatContext *ctx);

/* Deletes the line from pos to len and puts killed portion in yank buffer.
   Return 0 on success, -1 if nothing to kill. */
int kill_buf(ChatContext *ctx);

/* nulls line and sets pos, len and start to 0 */
void reset_buf(ChatContext *ctx);

/* Inserts string in ctx->yank into line at pos.
   Return 0 on success, -1 if yank buffer is empty or too long */
int yank_buf(ChatContext *ctx);

/* Deletes all characters from line starting at pos and going backwards
   until we find a space or run out of characters.
   Return 0 on success, -1 if no line or already at the beginning */
int del_word_buf(ChatContext *ctx);

/* Removes trailing spaces from line. */
void rm_trailing_spaces_buf(ChatContext *ctx);

/* adds a line to the ln_history buffer at hst_pos and sets hst_pos to last history item. */
void add_line_to_hist(ChatContext *ctx);

/* copies history item at hst_pos to line. Sets pos and len to the len of the history item.
   hst_pos is decremented or incremented depending on key_dir.

   resets line if at end of history */
void fetch_hist_item(const Client_Config *c_config, ChatContext *ctx, int key_dir);

/* Substitutes all occurrences of old with new. */
void strsubst(char *str, char old, char new);
void wstrsubst(wchar_t *str, wchar_t old, wchar_t new);

#endif /* TOXIC_STRINGS_H */

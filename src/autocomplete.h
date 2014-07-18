/*  autocomplete.h
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

#ifndef _autocomplete_h
#define _autocomplete_h

/* looks for the first instance in list that begins with the last entered word in line according to pos,
   then fills line with the complete word. e.g. "Hello jo" would complete the line
   with "Hello john". Works slightly differently for directory paths with the same results.

   list is a pointer to the list of strings being compared, n_items is the number of items
   in the list, and size is the size of each item in the list.

   Returns the difference between the old len and new len of line on success, -1 if error */
int complete_line(ChatContext *ctx, const void *list, int n_items, int size);

/* matches /sendfile "<incomplete-dir>" line to matching directories.

   if only one match, auto-complete line and return diff between old len and new len.
   return 0 if > 1 match and print out all the matches
   return -1 if no matches  */
int dir_match(ToxWindow *self, Tox *m, const wchar_t *line);

#endif  /* #define _autocomplete_h */
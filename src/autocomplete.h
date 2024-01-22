/*  autocomplete.h
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

#ifndef AUTOCOMPLETE_H
#define AUTOCOMPLETE_H

#include "toxic.h"
#include "windows.h"

/*
 * Looks for all instances in list that begin with the last entered word in line according to pos,
 * then fills line with the complete word. e.g. "Hello jo" would complete the line
 * with "Hello john". If multiple matches, prints out all the matches and semi-completes line.
 *
* `list` is a pointer to `n_items` strings.
 *
 * dir_search should be true if the line being completed is a file path.
 *
 * Returns the difference between the old len and new len of line on success.
 * Returns -1 on error.
 *
 * Note: This function should not be called directly. Use complete_line() and complete_path() instead.
 */
int complete_line(ToxWindow *self, Toxic *toxic, const char **list, size_t n_items);

/* Attempts to match /command "<incomplete-dir>" line to matching directories.
 * If there is only one match the line is auto-completed.
 *
 * Returns the diff between old len and new len of ctx->line on success.
 * Returns -1 if no matches or more than one match.
 */
int dir_match(ToxWindow *self, Toxic *toxic, const wchar_t *line, const wchar_t *cmd);

#endif /* AUTOCOMPLETE_H */

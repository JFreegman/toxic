/*  autocomplete.h
 *
 *  Copyright (C) 2014-2026 Toxic All Rights Reserved.
 *
 *  This file is part of Toxic. Toxic is free software licensed
 *  under the GNU General Public License 3.0.
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
int complete_line(ToxWindow *self, Toxic *toxic, const char *const *list, size_t n_items);

/* Attempts to match /command "<incomplete-dir>" line to matching directories.
 * If there is only one match the line is auto-completed.
 *
 * Returns the diff between old len and new len of ctx->line on success.
 * Returns -1 if no matches or more than one match.
 */
int dir_match(ToxWindow *self, Toxic *toxic, const wchar_t *line, const wchar_t *cmd);

#endif /* AUTOCOMPLETE_H */

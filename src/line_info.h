/*  line_info.h
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

#ifndef LINE_INFO_H
#define LINE_INFO_H

#include "toxic.h"
#include "windows.h"

#define MAX_HISTORY 100000
#define MIN_HISTORY 40
#define MAX_LINE_INFO_QUEUE 1024
#define MAX_LINE_INFO_MSG_SIZE MAX_STR_SIZE + TOXIC_MAX_NAME_LENGTH + 32    /* needs extra room for log loading */

typedef enum {
    SYS_MSG,
    IN_MSG,
    OUT_MSG,
    OUT_MSG_READ,    /* for sent messages that have received a read reply. don't set this with line_info_add */
    IN_ACTION,
    OUT_ACTION,
    OUT_ACTION_READ,     /* same as OUT_MSG_READ but for actions */
    PROMPT,
    CONNECTION,
    DISCONNECTION,
    NAME_CHANGE,
} LINE_TYPE;

struct line_info {
    char timestr[TIME_STR_SIZE];
    char name1[TOXIC_MAX_NAME_LENGTH + 1];
    char name2[TOXIC_MAX_NAME_LENGTH + 1];
    char msg[MAX_LINE_INFO_MSG_SIZE];
    time_t timestamp;
    uint8_t type;
    uint8_t bold;
    uint8_t colour;
    uint8_t noread_flag;   /* true if a line should be flagged as unread */
    uint32_t id;
    uint16_t len;   /* combined len of entire line */
    uint8_t newlines;

    struct line_info *prev;
    struct line_info *next;
};

/* Linked list containing chat history lines */
struct history {
    struct line_info *line_root;
    struct line_info *line_start;   /* the first line we want to start printing at */
    struct line_info *line_end;
    uint32_t start_id;    /* keeps track of where line_start should be when at bottom of history */

    struct line_info *queue[MAX_LINE_INFO_QUEUE];
    int queue_sz;
};

/* creates new line_info line and puts it in the queue.
 *
 * Returns the id of the new line.
 * Returns -1 on failure.
 */
int line_info_add(ToxWindow *self, const char *timestr, const char *name1, const char *name2, uint8_t type,
                  uint8_t bold, uint8_t colour, const char *msg, ...);

/* Prints a section of history starting at line_start */
void line_info_print(ToxWindow *self);

/* frees all history lines */
void line_info_cleanup(struct history *hst);

/* clears the screen (does not delete anything) */
void line_info_clear(struct history *hst);

/* puts msg in specified line_info msg buffer */
void line_info_set(ToxWindow *self, uint32_t id, char *msg);

/* resets line_start (moves to end of chat history) */
void line_info_reset_start(ToxWindow *self, struct history *hst);

void line_info_init(struct history *hst);
bool line_info_onKey(ToxWindow *self, wint_t key);    /* returns true if key is a match */

#endif /* LINE_INFO_H */

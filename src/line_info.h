/*  line_info.h
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

#ifndef LINE_INFO_H
#define LINE_INFO_H

#include <time.h>

#include "settings.h"
#include "toxic.h"
#include "windows.h"

#define MAX_HISTORY 100000
#define MIN_HISTORY 40
#define MAX_LINE_INFO_QUEUE 1024
#define MAX_LINE_INFO_MSG_SIZE (MAX_STR_SIZE + TOXIC_MAX_NAME_LENGTH + 32) /* needs extra room for log loading */

typedef enum LINE_TYPE {
    SYS_MSG,
    IN_MSG,
    OUT_MSG,
    OUT_MSG_READ,    /* for sent messages that have received a read reply. don't set this with line_info_add */
    IN_ACTION,
    OUT_ACTION,
    OUT_ACTION_READ,     /* same as OUT_MSG_READ but for actions */
    IN_PRVT_MSG,   /* PRVT should only be used for groups */
    OUT_PRVT_MSG,
    PROMPT,
    CONNECTION,
    DISCONNECTION,
    NAME_CHANGE,
} LINE_TYPE;

struct line_info {
    char    timestr[TIME_STR_SIZE];
    char    name1[TOXIC_MAX_NAME_LENGTH + 1];
    char    name2[TOXIC_MAX_NAME_LENGTH + 1];
    wchar_t msg[MAX_LINE_INFO_MSG_SIZE];
    time_t  timestamp;
    uint8_t type;
    uint8_t bold;
    uint8_t colour;
    bool    noread_flag;   /* true if a line should be flagged as unread */
    bool    read_flag;     /* true if a message has been flagged as read */
    uint32_t id;
    uint16_t len;        /* combined length of entire line */
    uint16_t msg_width;    /* width of the message */
    uint16_t format_lines;  /* number of lines the combined string takes up (dynamically set) */

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
    size_t queue_size;
};

/* creates new line_info line and puts it in the queue.
 *
 * Returns the id of the new line.
 * Returns -1 on failure.
 */
int line_info_add(ToxWindow *self, const Client_Config *c_config, bool show_timestamp, const char *name1,
                  const char *name2, LINE_TYPE type, uint8_t bold, uint8_t colour, const char *msg, ...);

/* Prints a section of history starting at line_start */
void line_info_print(ToxWindow *self, const Client_Config *c_config);

/* frees all history lines */
void line_info_cleanup(struct history *hst);

/* clears the screen (does not delete anything) */
void line_info_clear(struct history *hst);

/* puts msg in specified line_info msg buffer */
void line_info_set(ToxWindow *self, uint32_t id, char *msg);

/* Return the line_info object associated with `id`.
 * Return NULL if id cannot be found
 */
struct line_info *line_info_get(ToxWindow *self, uint32_t id);

/* resets line_start (moves to end of chat history) */
void line_info_reset_start(ToxWindow *self, struct history *hst);

void line_info_init(struct history *hst);

/* returns true if key is a match */
bool line_info_onKey(ToxWindow *self, const Client_Config *c_config, wint_t key);

#endif /* LINE_INFO_H */

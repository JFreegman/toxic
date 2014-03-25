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

#define MAX_HISTORY 200

enum {
    SYS_MSG,
    IN_MSG,
    OUT_MSG,
    PROMPT,
    ACTION,
    CONNECTION,
    NAME_CHANGE,
} LINE_TYPE;

struct line_info {
    uint8_t timestamp[TIME_STR_SIZE];
    uint8_t name1[TOXIC_MAX_NAME_LENGTH];
    uint8_t name2[TOXIC_MAX_NAME_LENGTH];
    uint8_t msg[MAX_STR_SIZE];
    uint8_t type;
    uint8_t bold;
    uint8_t colour;
    uint32_t id;
    int len;   /* combined len of all strings */

    struct line_info *prev;
    struct line_info *next;
};

/* Linked list containing chat history lines */
struct history {
    struct line_info *line_root;
    struct line_info *line_start;   /* the first line we want to start printing at */
    struct line_info *line_end;
    uint32_t start_id;    /* keeps track of where line_start should be when at bottom of history */
    uint32_t line_items;
    bool scroll_mode;
};

void line_info_add(ToxWindow *self, uint8_t *tmstmp, uint8_t *name1, uint8_t *name2, uint8_t *msg, 
                   uint8_t type, uint8_t bold, uint8_t colour);
void line_info_cleanup(struct history *hst);
void line_info_toggle_scroll(ToxWindow *self, bool scroll);
void line_info_init(struct history *hst);
void line_info_print(ToxWindow *self);
void line_info_onKey(ToxWindow *self, wint_t key);

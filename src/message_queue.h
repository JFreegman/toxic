/*  message_queue.h
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

enum {
    QMESSAGE,
    QACTION,
} MESSAGE_TYPE;

struct cqueue_msg {
    char message[MAX_STR_SIZE];
    int len;
    int line_id;
    uint8_t type;
    uint32_t receipt;
    struct cqueue_msg *next;
};

struct chat_queue {
    struct cqueue_msg *root;
    struct cqueue_msg *end;
    int friendnum;
};

void cqueue_cleanup(struct chat_queue *q);
void cqueue_add(struct chat_queue *q, const char *msg, int len, uint8_t type, uint32_t line_id);
void cqueue_check(Tox *m);
void cqueue_try_send(Tox *m, struct chat_queue *q);

/*  message_queue.c
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

#include <stdlib.h>

#include "toxic.h"
#include "windows.h"
#include "message_queue.h"

void cqueue_cleanup(struct chat_queue *q)
{
    struct cqueue_msg *tmp1 = q->root;

    while (tmp1) {
        struct cqueue_msg *tmp2 = tmp1->next;
        free(tmp1);
        tmp1 = tmp2;
    }

    free(q);
}

void cqueue_add(struct chat_queue *q, const char *msg, int len, uint8_t type, uint32_t line_id)
{
    struct cqueue_msg *new_m = malloc(sizeof(struct cqueue_msg));

    if (new_m == NULL)
        exit_toxic_err("failed in cqueue_message", FATALERR_MEMORY);

    snprintf(new_m->message, sizeof(new_m->message), "%s", msg);
    new_m->len = len;
    new_m->type = type;
    new_m->line_id = line_id;
    new_m->next = NULL;

    if (q->root == NULL)
        q->root = new_m;
    else
        q->end->next = new_m;

    q->end = new_m;
}

static void cqueue_remove(struct chat_queue *q)
{
    struct cqueue_msg *new_root = q->root->next;
    free(q->root);
    q->root = new_root;
}

void cqueue_try_send(Tox *m, struct chat_queue *q)
{
    if (q->root == NULL)
        return;

    struct cqueue_msg *q_msg = q->root;
    uint32_t receipt;

    if (q_msg->type == QMESSAGE)
        receipt = tox_send_message(m, q->friendnum, (uint8_t *) q_msg->message, q_msg->len);
    else
        receipt = tox_send_action(m, q->friendnum, (uint8_t *) q_msg->message, q_msg->len);

    if (receipt == 0)
        return;

    cqueue_remove(q);
}

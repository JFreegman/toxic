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
#include "misc_tools.h"
#include "line_info.h"

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
    struct cqueue_msg *new_m = calloc(1, sizeof(struct cqueue_msg));

    if (new_m == NULL)
        exit_toxic_err("failed in cqueue_message", FATALERR_MEMORY);

    snprintf(new_m->message, sizeof(new_m->message), "%s", msg);
    new_m->len = len;
    new_m->type = type;
    new_m->line_id = line_id;

    if (q->root == NULL) {
        new_m->prev = NULL;
        q->root = new_m;
    } else {
        new_m->prev = q->end;
        q->end->next = new_m;
    }

    q->end = new_m;
}

/* update line to show receipt was received after queue removal */
static void cqueue_mark_read(ToxWindow *self, uint32_t id, uint8_t type)
{
    struct line_info *line = self->chatwin->hst->line_end;

    while (line) {
        if (line->id == id) {
            line->type = type == OUT_ACTION ? OUT_ACTION_READ : OUT_MSG_READ;
            line->len -= 2;    /* removes " x" */
            return;
        }

        line = line->prev;
    }
}

/* removes the message with the same receipt number from queue and updates line to show the message was received*/
void cqueue_remove(ToxWindow *self, struct chat_queue *q, uint32_t receipt)
{
    struct cqueue_msg *q_msg = q->root;

    while (q_msg) {
        struct cqueue_msg *next = q_msg->next;

        if (q_msg->receipt == receipt) {
            uint32_t line_id = q_msg->line_id;
            uint8_t type = q_msg->type;

            if (q_msg->prev == NULL) {
                if (next)
                    next->prev = NULL;

                free(q->root);
                q->root = next;
            } else {
                q_msg->prev->next = next;
                free(q_msg);
            }

            cqueue_mark_read(self, line_id, type);
            return;
        }

        q_msg = next;
    }
}

#define CQUEUE_TRY_SEND_INTERVAL 5

/* Tries to send oldest message in queue. If fails, tries again in CQUEUE_TRY_SEND_INTERVAL seconds */
void cqueue_try_send(ToxWindow *self, Tox *m)
{
    struct chat_queue *q = self->chatwin->cqueue;

    if (q->root == NULL)
        return;

    struct cqueue_msg *q_msg = q->root;
    uint64_t curtime = get_unix_time();

    /* allow some time for a laggy read receipt before resending
       TODO: timeout should be removed when receipts are fixed in core) */
    if (q_msg->receipt != 0 && !timed_out(q_msg->last_send_try, curtime, CQUEUE_TRY_SEND_INTERVAL))
        return;

    uint32_t receipt = 0;

    if (q_msg->type == OUT_MSG)
        receipt = tox_send_message(m, q->friendnum, (uint8_t *) q_msg->message, q_msg->len);
    else
        receipt = tox_send_action(m, q->friendnum, (uint8_t *) q_msg->message, q_msg->len);

    q->root->last_send_try = curtime;
    q->root->receipt = receipt;
}

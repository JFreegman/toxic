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

#ifdef NO_GETTEXT
#define gettext(A) (A)
#else
#include <libintl.h>
#endif

#include "toxic.h"
#include "windows.h"
#include "message_queue.h"
#include "misc_tools.h"
#include "line_info.h"
#include "log.h"

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

void cqueue_add(struct chat_queue *q, const char *msg, size_t len, uint8_t type, uint32_t line_id)
{
    struct cqueue_msg *new_m = malloc(sizeof(struct cqueue_msg));

    if (new_m == NULL)
        exit_toxic_err(gettext("failed in cqueue_message"), FATALERR_MEMORY);

    snprintf(new_m->message, sizeof(new_m->message), "%s", msg);
    new_m->len = len;
    new_m->type = type;
    new_m->line_id = line_id;
    new_m->last_send_try = 0;
    new_m->receipt = 0;
    new_m->next = NULL;

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
static void cqueue_mark_read(ToxWindow *self, struct cqueue_msg *msg)
{
    struct line_info *line = self->chatwin->hst->line_end;

    while (line) {
        if (line->id != msg->line_id) {
            line = line->prev;
            continue;
        }

        line->type = msg->type == OUT_ACTION ? OUT_ACTION_READ : OUT_MSG_READ;

        if (line->noread_flag == true) {
            line->len -= 2;
            line->noread_flag = false;
        }

        return;
    }
}

/* removes message with matching receipt from queue, writes to log and updates line to show the message was received. */
void cqueue_remove(ToxWindow *self, Tox *m, uint32_t receipt)
{
    struct chat_queue *q = self->chatwin->cqueue;
    struct cqueue_msg *msg = q->root;

    while (msg) {
        if (msg->receipt != receipt) {
            msg = msg->next;
            continue;
        }

        char selfname[TOX_MAX_NAME_LENGTH];
        tox_self_get_name(m, (uint8_t *) selfname);

        size_t len = tox_self_get_name_size(m);
        selfname[len] = '\0';

        write_to_log(msg->message, selfname, self->chatwin->log, msg->type == OUT_ACTION);
        cqueue_mark_read(self, msg);

        struct cqueue_msg *next = msg->next;

        if (msg->prev == NULL) {    /* root */
            if (next)
                next->prev = NULL;

            free(msg);
            q->root = next;
        } else {
            struct cqueue_msg *prev = msg->prev;
            free(msg);
            prev->next = next;
        }

        return;
    }
}

#define CQUEUE_TRY_SEND_INTERVAL 60

/* Tries to send the oldest unsent message in queue. */
void cqueue_try_send(ToxWindow *self, Tox *m)
{
    struct chat_queue *q = self->chatwin->cqueue;
    struct cqueue_msg *msg = q->root;

    if (!msg)
        return;

    uint64_t curtime = get_unix_time();

    if (msg->receipt != 0 && !timed_out(msg->last_send_try, curtime, CQUEUE_TRY_SEND_INTERVAL))
        return;

    uint32_t receipt = 0;

    TOX_MESSAGE_TYPE type = msg->type == OUT_MSG ? TOX_MESSAGE_TYPE_NORMAL : TOX_MESSAGE_TYPE_ACTION;
    receipt = tox_friend_send_message(m, self->num, type, (uint8_t *) msg->message, msg->len, NULL);

    msg->last_send_try = curtime;
    msg->receipt = receipt;
    return;
}

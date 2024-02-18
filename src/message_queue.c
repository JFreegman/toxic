/*  message_queue.c
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

#include <stdlib.h>

#include "line_info.h"
#include "log.h"
#include "message_queue.h"
#include "misc_tools.h"
#include "toxic.h"
#include "windows.h"

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

void cqueue_add(struct chat_queue *q, const char *msg, size_t len, uint8_t type, int line_id)
{
    if (line_id < 0) {
        return;
    }

    struct cqueue_msg *new_m = calloc(1, sizeof(struct cqueue_msg));

    if (new_m == NULL) {
        exit_toxic_err("failed in cqueue_message", FATALERR_MEMORY);
    }

    snprintf(new_m->message, sizeof(new_m->message), "%s", msg);
    new_m->len = len;
    new_m->type = type;
    new_m->line_id = line_id;
    new_m->last_send_try = 0;
    new_m->time_added = get_unix_time();
    new_m->receipt = -1;
    new_m->next = NULL;
    new_m->noread_flag = false;

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
    struct line_info *line = line_info_get(self, msg->line_id);

    if (line == NULL) {
        return;
    }

    line->type = msg->type == OUT_ACTION ? OUT_ACTION_READ : OUT_MSG_READ;

    if (line->noread_flag) {
        line->noread_flag = false;
        line->read_flag = true;
        flag_interface_refresh();
    }
}

/* removes message with matching receipt from queue, writes to log and updates line to show the message was received. */
void cqueue_remove(ToxWindow *self, Toxic *toxic, uint32_t receipt)
{
    struct chatlog *log = self->chatwin->log;
    struct chat_queue *q = self->chatwin->cqueue;
    struct cqueue_msg *msg = q->root;

    Tox *tox = toxic->tox;
    const Client_Config *c_config = toxic->c_config;

    while (msg) {
        if (msg->receipt != receipt) {
            msg = msg->next;
            continue;
        }

        if (log->log_on) {
            char selfname[TOX_MAX_NAME_LENGTH + 1];
            tox_self_get_name(tox, (uint8_t *) selfname);

            const size_t len = tox_self_get_name_size(tox);
            selfname[len] = '\0';

            write_to_log(log, c_config, msg->message, selfname, msg->type == OUT_ACTION, LOG_HINT_NORMAL_O);
        }

        cqueue_mark_read(self, msg);

        struct cqueue_msg *next = msg->next;

        if (msg->prev == NULL) {    /* root */
            if (next) {
                next->prev = NULL;
            }

            q->root = next;
        } else {
            struct cqueue_msg *prev = msg->prev;
            prev->next = next;
            next->prev = prev;
        }

        free(msg);

        return;
    }
}

// We use knowledge of toxcore internals (bad!) to determine that if we haven't received a read receipt for a
// sent packet after this amount of time, the connection has been severed and the packet needs to be re-sent.
#define TRY_SEND_TIMEOUT 32

/*
 * Marks all timed out messages in queue as unsent.
 */
static void cqueue_check_timeouts(struct cqueue_msg *msg)
{
    while (msg) {
        if (timed_out(msg->last_send_try, TRY_SEND_TIMEOUT)) {
            msg->receipt = -1;
        }

        msg = msg->next;
    }
}

/*
 * Sets the noread flag for messages sent to the peer associated with `self` which have not
 * received a receipt after a period of time.
 */
#define NOREAD_TIMEOUT 5
void cqueue_check_unread(ToxWindow *self)
{
    struct chat_queue *q = self->chatwin->cqueue;
    struct cqueue_msg *msg = q->root;

    while (msg) {
        if (msg->noread_flag) {
            msg = msg->next;
            continue;
        }

        struct line_info *line = line_info_get(self, msg->line_id);

        if (line != NULL) {
            if (timed_out(msg->time_added, NOREAD_TIMEOUT)) {
                line->noread_flag = true;
                msg->noread_flag = true;
                flag_interface_refresh();
            }
        }

        msg = msg->next;
    }
}

/*
 * Tries to send all messages in the send queue in sequential order.
 * If a message fails to send the function will immediately return.
 */
void cqueue_try_send(ToxWindow *self, Tox *tox)
{
    struct chat_queue *q = self->chatwin->cqueue;
    struct cqueue_msg *msg = q->root;

    while (msg) {
        if (msg->receipt != -1) {
            // we can no longer try to send unsent messages until we get receipts for our previous sent
            // messages, but we continue to iterate the list, checking timestamps for any further
            // successfully sent messages that have not yet gotten a receipt.
            cqueue_check_timeouts(msg);
            return;
        }

        Tox_Err_Friend_Send_Message err;
        Tox_Message_Type type = msg->type == OUT_MSG ? TOX_MESSAGE_TYPE_NORMAL : TOX_MESSAGE_TYPE_ACTION;
        uint32_t receipt = tox_friend_send_message(tox, self->num, type, (uint8_t *) msg->message, msg->len, &err);

        if (err != TOX_ERR_FRIEND_SEND_MESSAGE_OK) {
            return;
        }

        msg->receipt = receipt;
        msg->last_send_try = get_unix_time();
        msg = msg->next;
    }
}

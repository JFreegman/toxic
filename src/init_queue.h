/*  init_queue.h
 *
 *  Copyright (C) 2026 Toxic All Rights Reserved.
 *
 *  This file is part of Toxic. Toxic is free software licensed
 *  under the GNU General Public License 3.0.
 */

#ifndef INIT_QUEUE_H
#define INIT_QUEUE_H

#include <stdint.h>

#include "toxic.h"
#include "windows.h"

typedef struct Init_Queue {
    char     **messages;
    uint16_t count;
} Init_Queue;

/*
 * Adds `message` to `init_q`.
 *
 * `init_queue_new()` must be called before using this function.
 *
 * If `init_q` is NULL this function has no effect.
 */
__attribute__((format(printf, 2, 3)))
void init_queue_add(Init_Queue *init_q, const char *message, ...);

/*
 * Frees all memory associated with init_q including the init_q itself.
 *
 * If `init_q` is NULL this function has no effect.
 */
void init_queue_free(Init_Queue *init_q);

/*
 * Prints all messages in `init_q` to `window`.
 *
 * If `init_q` is NULL this function has no effect.
 */
void init_queue_print(const Init_Queue *init_q, ToxWindow *window, const Client_Config *c_config);

/*
 * Returns a new Init_Queue object on success.
 * Returns NULL on memory allocation error.
 *
 * The caller is responsible for freeing the memory associated with the returned
 * object with `init_queue_free()`.
 */
Init_Queue *init_queue_new(void);

#endif  // INIT_QUEUE_H

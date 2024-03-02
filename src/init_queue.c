/*  init_queue.c
 *
 *  Copyright (C) 2024 Toxic All Rights Reserved.
 *
 *  This file is part of Toxic. Toxic is free software licensed
 *  under the GNU General Public License 3.0.
 */

#include "init_queue.h"

#include <stdint.h>
#include <stdlib.h>

#include "line_info.h"
#include "toxic.h"
#include "toxic_constants.h"
#include "windows.h"

Init_Queue *init_queue_new(void)
{
    Init_Queue *init_q = (Init_Queue *) calloc(1, sizeof(Init_Queue));
    return init_q;
}

void init_queue_free(Init_Queue *init_q)
{
    if (init_q == NULL) {
        return;
    }

    for (uint16_t i = 0; i < init_q->count; ++i) {
        free(init_q->messages[i]);
    }

    free(init_q->messages);
    free(init_q);
}

void init_queue_print(const Init_Queue *init_q, ToxWindow *window, const Client_Config *c_config)
{
    if (init_q == NULL) {
        return;
    }

    for (uint16_t i = 0; i < init_q->count; ++i) {
        line_info_add(window, c_config, NULL, NULL, NULL, SYS_MSG, 0, 0, "%s", init_q->messages[i]);
    }
}

void init_queue_add(Init_Queue *init_q, const char *message, ...)
{
    if (init_q == NULL) {
        return;
    }

    char format_message[MAX_STR_SIZE] = {0};

    va_list args;
    va_start(args, message);
    vsnprintf(format_message, sizeof(format_message), message, args);
    va_end(args);

    const uint16_t i = init_q->count;

    char **temp_messages = realloc(init_q->messages, sizeof(char *) * (i + 1));

    if (temp_messages == NULL) {
        exit_toxic_err(FATALERR_MEMORY, "Failed in init_queue_add");
    }

    temp_messages[i] = malloc(MAX_STR_SIZE);

    if (temp_messages[i] == NULL) {
        exit_toxic_err(FATALERR_MEMORY, "Failed in init_queue_add");
    }

    snprintf(temp_messages[i], MAX_STR_SIZE, "%s", format_message);

    init_q->messages = temp_messages;
    ++init_q->count;
}

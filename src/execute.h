/*  execute.h
 *
 *  Copyright (C) 2014-2024 Toxic All Rights Reserved.
 *
 *  This file is part of Toxic. Toxic is free software licensed
 *  under the GNU General Public License 3.0.
 */

#ifndef EXECUTE_H
#define EXECUTE_H

#include "toxic.h"
#include "windows.h"

#define MAX_NUM_ARGS 4     /* Includes command */

enum {
    GLOBAL_COMMAND_MODE,
    CHAT_COMMAND_MODE,
    CONFERENCE_COMMAND_MODE,
    GROUPCHAT_COMMAND_MODE,
};

void execute(WINDOW *w, ToxWindow *self, Toxic *toxic, const char *input, int mode);

#endif /* EXECUTE_H */

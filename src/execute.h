/*  execute.h
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

void execute(WINDOW *w, ToxWindow *self, Tox *m, const char *input, int mode);

#endif /* EXECUTE_H */

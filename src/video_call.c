/*  video_call.c
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

#include "toxic.h"
#include "windows.h"
#include "video_call.h"
#include "video_device.h"
#include "chat_commands.h"
#include "global_commands.h"
#include "line_info.h"
#include "notify.h"

#include <stdbool.h>
#include <curses.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>

#define cbend pthread_exit(NULL)

#define MAX_CALLS 10

ToxAV *init_video(ToxWindow *self, Tox *tox, ToxAV *av)
{

}

void terminate_video()
{
 
}

int start_video_transmission(ToxWindow *self, Call *call)
{

    return 0;
}

int stop_video_transmission(Call *call, int friend_number)
{

    return 0;
}

/*
 * Commands from chat_commands.h
 */
void cmd_enablevid(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Video Enabled");
    return;
}

void cmd_disablevid(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Video Disabled");
    return;
}
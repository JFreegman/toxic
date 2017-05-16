/*  api.c
 *
 *
 *  Copyright (C) 2017 Toxic All Rights Reserved.
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

#include <stdint.h>

#include <tox/tox.h>

#include "execute.h"
#include "friendlist.h"
#include "line_info.h"
#include "python_api.h"
#include "windows.h"

Tox              *user_tox;
static WINDOW    *cur_window;
static ToxWindow *self_window;

extern FriendsList Friends;

void api_display(const char * const msg)
{
    if (msg == NULL)
        return;

    line_info_add(self_window, NULL, NULL, NULL, SYS_MSG, 0, 0, msg);
}

FriendsList api_get_friendslist(void)
{
    return Friends;
}

char *api_get_nick(void)
{
    size_t   len  = tox_self_get_name_size(user_tox);
    uint8_t *name = malloc(len + 1);
    if (name == NULL)
        return NULL;
    tox_self_get_name(user_tox, name);
    return (char *) name;
}

TOX_USER_STATUS api_get_status(void)
{
    return tox_self_get_status(user_tox);
}

char *api_get_status_message(void)
{
    size_t   len    = tox_self_get_status_message_size(user_tox);
    uint8_t *status = malloc(len + 1);
    if (status == NULL)
        return NULL;
    tox_self_get_status_message(user_tox, status);
    return (char *) status;
}

void api_execute(const char *input, int mode)
{
    execute(cur_window, self_window, user_tox, input, mode);
}

/* TODO: Register command */

void cmd_run(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    FILE       *fp;
    const char *error_str;

    cur_window  = window;
    self_window = self;

    if ( argc != 1 ) {
        if ( argc < 1 ) error_str = "Path must be specified!";
        else error_str = "Only one argument allowed!";

        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, error_str);
        return;
    }

    fp = fopen(argv[1], "r");
    if ( fp == NULL ) {
        error_str = "Path does not exist!";

        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, error_str);
        return;
    }
    run_python(fp, argv[1]);
    fclose(fp);
}

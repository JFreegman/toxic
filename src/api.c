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

#include "line_info.h"
#include "python_api.h"
#include "windows.h"

extern ToxWindow *prompt;

void api_display(const char * const msg)
{
    if (msg == NULL)
        return;

    line_info_add(prompt, NULL, NULL, NULL, SYS_MSG, 0, 0, msg);
}

void cmd_run(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    FILE       *fp;
    const char *error_str;

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

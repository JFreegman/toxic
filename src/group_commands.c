/*  group_commands.c
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

#include <string.h>

#include "toxic.h"
#include "windows.h"
#include "line_info.h"
#include "misc_tools.h"
#include "log.h"

void cmd_set_title(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    char title[MAX_STR_SIZE];

    if (argc < 1) {
        int tlen = tox_group_get_title(m, self->num, (uint8_t *) title, TOX_MAX_NAME_LENGTH);

        if (tlen != -1) {
            title[tlen] = '\0';
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Title is set to: %s", title);
        } else {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Title is not set");
        }

        return;
    }

    if (argv[1][0] != '\"') {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Title must be enclosed in quotes.");
        return;
    }

    /* remove opening and closing quotes */
    snprintf(title, sizeof(title), "%s", &argv[1][1]);
    int len = strlen(title) - 1;
    title[len] = '\0';

    if (tox_group_set_title(m, self->num, (uint8_t *) title, len) != 0) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to set title.");
        return;
    }

    set_window_title(self, title, len);

    char timefrmt[TIME_STR_SIZE];
    char selfnick[TOX_MAX_NAME_LENGTH];

    get_time_str(timefrmt, sizeof(timefrmt));

    tox_self_get_name(m, (uint8_t *) selfnick);
    size_t sn_len = tox_self_get_name_size(m);
    selfnick[sn_len] = '\0';

    line_info_add(self, timefrmt, selfnick, NULL, NAME_CHANGE, 0, 0, " set the group title to: %s", title);

    char tmp_event[MAX_STR_SIZE];
    snprintf(tmp_event, sizeof(tmp_event), "set title to %s", title);
    write_to_log(tmp_event, selfnick, self->chatwin->log, true);
}

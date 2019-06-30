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
    Tox_Err_Conference_Title err;
    char title[MAX_STR_SIZE];

    if (argc < 1) {
        size_t tlen = tox_conference_get_title_size(m, self->num, &err);

        if (err != TOX_ERR_CONFERENCE_TITLE_OK || tlen >= sizeof(title)) {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Title is not set");
            return;
        }

        if (!tox_conference_get_title(m, self->num, (uint8_t *) title, &err)) {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Title is not set");
            return;
        }

        title[tlen] = '\0';
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Title is set to: %s", title);

        return;
    }

    snprintf(title, sizeof(title), "%s", argv[1]);
    int len = strlen(title);

    if (!tox_conference_set_title(m, self->num, (uint8_t *) title, len, &err)) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to set title (error %d)", err);
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

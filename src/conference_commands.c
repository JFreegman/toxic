/*  conference_commands.c
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
#include "conference.h"

void cmd_conference_set_title(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

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

    line_info_add(self, timefrmt, selfnick, NULL, NAME_CHANGE, 0, 0, " set the conference title to: %s", title);

    char tmp_event[MAX_STR_SIZE + 20];
    snprintf(tmp_event, sizeof(tmp_event), "set title to %s", title);
    write_to_log(tmp_event, selfnick, self->chatwin->log, true);
}

void cmd_enable_audio(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

#ifdef AUDIO
    bool enable;

    if (argc == 1 && !strcasecmp(argv[1], "on")) {
        enable = true;
    } else if (argc == 1 && !strcasecmp(argv[1], "off")) {
        enable = false;
    } else {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Please specify: on | off");
        return;
    }

    if ((enable ? enable_conference_audio : disable_conference_audio)(m, self->num)) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, enable ? "Enabled conference audio" : "Disabled conference audio");
    } else {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, enable ? "Failed to enable audio" : "Failed to disable audio");
    }

#endif
}

void cmd_conference_mute(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

#ifdef AUDIO

    if (argc < 1) {
        if (conference_mute_self(self->num)) {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Toggled self audio mute status");
        } else {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "No audio input to mute");
        }
    } else {
        NameListEntry *entries[16];
        uint32_t n = get_name_list_entries_by_prefix(self->num, argv[1], entries, 16);

        if (n == 0) {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "No such peer");
            return;
        }

        if (n > 1) {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0,
                          "Multiple matching peers (use /mute [public key] to disambiguate):");

            for (uint32_t i = 0; i < n; ++i) {
                line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "%s: %s", entries[i]->pubkey_str, entries[i]->name);
            }

            return;
        }

        if (conference_mute_peer(m, self->num, entries[0]->peernum)) {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Toggled audio mute status of %s", entries[0]->name);
        } else {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "No such audio peer");
        }
    }

#endif
}

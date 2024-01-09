/*  conference_commands.c
 *
 *
 *  Copyright (C) 2024 Toxic All Rights Reserved.
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

#include <stdlib.h>
#include <string.h>

#include "conference.h"
#include "line_info.h"
#include "log.h"
#include "misc_tools.h"
#include "toxic.h"
#include "windows.h"

static void print_err(ToxWindow *self, const char *error_str)
{
    line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "%s", error_str);
}

void cmd_conference_set_title(WINDOW *window, ToxWindow *self, Tox *tox, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

    Tox_Err_Conference_Title err;
    char title[CONFERENCE_MAX_TITLE_LENGTH + 1];

    if (argc < 1) {
        size_t tlen = tox_conference_get_title_size(tox, self->num, &err);

        if (err != TOX_ERR_CONFERENCE_TITLE_OK || tlen >= sizeof(title)) {
            print_err(self, "Title is not set");
            return;
        }

        if (!tox_conference_get_title(tox, self->num, (uint8_t *) title, &err)) {
            print_err(self, "Title is not set");
            return;
        }

        title[tlen] = '\0';
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Title is set to: %s", title);

        return;
    }

    size_t len = strlen(argv[1]);

    if (len >= sizeof(title)) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to set title: max length exceeded.");
        return;
    }

    snprintf(title, sizeof(title), "%s", argv[1]);

    if (!tox_conference_set_title(tox, self->num, (uint8_t *) title, len, &err)) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to set title (error %d)", err);
        return;
    }

    conference_rename_log_path(tox, self->num, title);  // must be called first

    conference_set_title(self, self->num, title, len);

    char selfnick[TOX_MAX_NAME_LENGTH];
    tox_self_get_name(tox, (uint8_t *) selfnick);

    size_t sn_len = tox_self_get_name_size(tox);
    selfnick[sn_len] = '\0';

    line_info_add(self, true, selfnick, NULL, NAME_CHANGE, 0, 0, " set the conference title to: %s", title);

    char tmp_event[MAX_STR_SIZE + 20];
    snprintf(tmp_event, sizeof(tmp_event), "set title to %s", title);
    write_to_log(tmp_event, selfnick, self->chatwin->log, true);
}

#ifdef AUDIO
void cmd_enable_audio(WINDOW *window, ToxWindow *self, Tox *tox, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

    bool enable;

    if (argc == 1 && !strcasecmp(argv[1], "on")) {
        enable = true;
    } else if (argc == 1 && !strcasecmp(argv[1], "off")) {
        enable = false;
    } else {
        print_err(self, "Please specify: on | off");
        return;
    }

    if (enable ? enable_conference_audio(self, tox, self->num) : disable_conference_audio(self, tox, self->num)) {
        print_err(self, enable ? "Enabled conference audio. Use the '/ptt' command to toggle Push-To-Talk."
                  : "Disabled conference audio");
    } else {
        print_err(self, enable ? "Failed to enable audio" : "Failed to disable audio");
    }
}

void cmd_conference_mute(WINDOW *window, ToxWindow *self, Tox *tox, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

    if (argc < 1) {
        if (conference_mute_self(self->num)) {
            print_err(self, "Toggled self audio mute status");
        } else {
            print_err(self, "No audio input to mute");
        }
    } else {
        NameListEntry *entries[16];
        uint32_t n = get_name_list_entries_by_prefix(self->num, argv[1], entries, 16);

        if (n == 0) {
            print_err(self, "No such peer");
            return;
        }

        if (n > 1) {
            print_err(self, "Multiple matching peers (use /mute [public key] to disambiguate):");

            for (uint32_t i = 0; i < n; ++i) {
                line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "%s: %s", entries[i]->pubkey_str, entries[i]->name);
            }

            return;
        }

        if (conference_mute_peer(tox, self->num, entries[0]->peernum)) {
            line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Toggled audio mute status of %s", entries[0]->name);
        } else {
            print_err(self, "Peer is not on the call");
        }
    }
}

void cmd_conference_sense(WINDOW *window, ToxWindow *self, Tox *tox, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);
    UNUSED_VAR(tox);

    if (argc == 0) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Current VAD threshold: %.1f",
                      (double) conference_get_VAD_threshold(self->num));
        return;
    }

    if (argc > 1) {
        print_err(self, "Only one argument allowed.");
        return;
    }

    char *end;
    float value = strtof(argv[1], &end);

    if (*end) {
        print_err(self, "Invalid input");
        return;
    }

    if (conference_set_VAD_threshold(self->num, value)) {
        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "Set VAD threshold to %.1f", (double) value);
    } else {
        print_err(self, "Failed to set conference audio input sensitivity.");
    }
}

void cmd_conference_push_to_talk(WINDOW *window, ToxWindow *self, Tox *tox, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);
    UNUSED_VAR(tox);

    bool enable;

    if (argc == 1 && !strcasecmp(argv[1], "on")) {
        enable = true;
    } else if (argc == 1 && !strcasecmp(argv[1], "off")) {
        enable = false;
    } else {
        print_err(self, "Please specify: on | off");
        return;
    }

    if (!toggle_conference_push_to_talk(self->num, enable)) {
        print_err(self, "Failed to toggle push to talk.");
        return;
    }

    print_err(self, enable ? "Push-To-Talk is enabled. Push F2 to activate" : "Push-To-Talk is disabled");
}
#endif /* AUDIO */

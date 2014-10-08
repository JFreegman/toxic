/*  settings.h
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

#ifndef SETTINGS_H
#define SETTINGS_H

#include <limits.h>

/* holds user setting values */
struct user_settings {
    int autolog;           /* boolean */
    int alerts;            /* boolean */
    int time;              /* 12 or 24 */
    int timestamps;        /* boolean */
    int colour_theme;      /* boolean (0 for default toxic colours) */
    int history_size;      /* int between MIN_HISTORY and MAX_HISTORY */
    int show_typing_self;  /* boolean */
    int show_typing_other; /* boolean */
    int show_welcome_msg;  /* boolean */

    char download_path[PATH_MAX];
    char chatlogs_path[PATH_MAX];
    char avatar_path[PATH_MAX];

	int key_next_tab;
	int key_prev_tab;
	int key_scroll_line_up;
	int key_scroll_line_down;
	int key_half_page_up;
	int key_half_page_down;
	int key_page_bottom;
	int key_peer_list_up;
	int key_peer_list_down;
    int key_toggle_peerlist;

#ifdef AUDIO
    int audio_in_dev;
    int audio_out_dev;
    double VAD_treshold;
#endif
};

enum {
    AUTOLOG_OFF = 0,
    AUTOLOG_ON = 1,

    TIME_24 = 24,
    TIME_12 = 12,

    TIMESTAMPS_OFF = 0,
    TIMESTAMPS_ON = 1,

    ALERTS_DISABLED = 0,
    ALERTS_ENABLED = 1,

    DFLT_COLS = 0,
    NATIVE_COLS = 1,

    SHOW_TYPING_OFF = 0,
    SHOW_TYPING_ON = 1,

    SHOW_WELCOME_MSG_OFF = 0,
    SHOW_WELCOME_MSG_ON = 1,

    DFLT_HST_SIZE = 700,
} settings_values;

int settings_load(struct user_settings *s, const char *patharg);
#endif /* #define SETTINGS_H */

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

#define NUM_SETTINGS 7

/* holds user setting values */
struct user_settings {
    int autolog;           /* boolean */
    int alerts;            /* boolean */
    int time;              /* 12 or 24 */
    int colour_theme;      /* boolean (0 for default toxic colours) */
    long int audio_in_dev;
    long int audio_out_dev;
    int history_size;      /* int between MIN_HISTORY and MAX_HISTORY */
};

enum {
    AUTOLOG_OFF = 0,
    AUTOLOG_ON = 1,

    TIME_24 = 24,
    TIME_12 = 12,

    ALERTS_DISABLED = 1,
    ALERTS_ENABLED = 0,

    NATIVE_COLS = 1,
    DFLT_COLS = 0,

    DFLT_HST_SIZE = 700,
};

int settings_load(struct user_settings *s, char *path);

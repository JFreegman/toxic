/*  settings.c
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

#include <stdlib.h>
#include <string.h>

#include "toxic_windows.h"
#include "configdir.h"
#include "settings.h"

static void uset_autolog(struct user_settings *s, int val);
static void uset_time(struct user_settings *s, int val);
static void uset_colours(struct user_settings *s, int val);

struct {
    const char *name;
    void (*func)(struct user_settings *s, int val);
} user_settings_list[] = { 
    { "autolog",        uset_autolog    }, 
    { "time",           uset_time       },
    { "colour_theme",   uset_colours    },
};

static void uset_autolog(struct user_settings *s, int val) 
{ 
    /* default off if invalid value */
    s->autolog = val == AUTOLOG_ON ? AUTOLOG_ON : AUTOLOG_OFF;
}

static void uset_time(struct user_settings *s, int val) 
{
    /* default to 24 hour time if invalid value */
    s->time = val == TIME_12 ? TIME_12 : TIME_24;
}

static void uset_colours(struct user_settings *s, int val)
{
    /* use default toxic colours if invalid value */
    s->colour_theme = val == NATIVE_COLS ? NATIVE_COLS : DFLT_COLS;
}

int settings_load(struct user_settings *s, char *path)
{
    char *user_config_dir = get_user_config_dir();
    FILE *fp = NULL;

    if (path) {
        fp = fopen(path, "r");
    } else {
        char dflt_path[MAX_STR_SIZE];
        snprintf(dflt_path, sizeof(dflt_path), "%s%stoxic.conf", user_config_dir, CONFIGDIR);
        fp = fopen(dflt_path, "r");
    }

    free(user_config_dir);

    if (fp == NULL)
        return -1;

    char line[MAX_STR_SIZE];

    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '#' || !line[0])
            continue;

        char *name = strtok(line, ":");
        char *val_s = strtok(NULL, ";");

        if (name == NULL || val_s == NULL)
            continue;

        int val = atoi(val_s);
        int i;

        for (i = 0; i < NUM_SETTINGS; ++i) {
            if (!strcmp(user_settings_list[i].name, name)) {
                (user_settings_list[i].func)(s, val);
                break;
            }
        }
    }

    return 0;
}

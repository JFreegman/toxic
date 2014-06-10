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

#ifdef _SUPPORT_AUDIO
    #include "audio_call.h"
#endif

#include "settings.h"
#include "line_info.h"

static void uset_autolog(struct user_settings *s, const char *val);
static void uset_time(struct user_settings *s, const char *val);
static void uset_alerts(struct user_settings *s, const char *val);
static void uset_colours(struct user_settings *s, const char *val);
static void uset_hst_size(struct user_settings *s, const char *val);
static void uset_dwnld_path(struct user_settings *s, const char *val);

#ifdef _SUPPORT_AUDIO
static void uset_ain_dev(struct user_settings *s, const char *val);
static void uset_aout_dev(struct user_settings *s, const char *val);
#endif

struct {
    const char *key;
    void (*func)(struct user_settings *s, const char *val);
} user_settings_list[] = {
    { "autolog",        uset_autolog    },
    { "time",           uset_time       },
    { "disable_alerts", uset_alerts     },
    { "colour_theme",   uset_colours    },
    { "history_size",   uset_hst_size   },
    { "download_path",  uset_dwnld_path },

#ifdef _SUPPORT_AUDIO
    { "audio_in_dev",   uset_ain_dev    },
    { "audio_out_dev",  uset_aout_dev   },
#endif
};

static void uset_autolog(struct user_settings *s, const char *val)
{
    int n = atoi(val);

    /* default off if invalid value */
    s->autolog = n == AUTOLOG_ON ? AUTOLOG_ON : AUTOLOG_OFF;
}

static void uset_time(struct user_settings *s, const char *val)
{
    int n = atoi(val);

    /* default to 24 hour time if invalid value */
    s->time = n == TIME_12 ? TIME_12 : TIME_24;
}

static void uset_alerts(struct user_settings *s, const char *val)
{
    int n = atoi(val);

    /* alerts default on if invalid value */
    s->alerts = n == ALERTS_DISABLED ? ALERTS_DISABLED : ALERTS_ENABLED;
}

static void uset_colours(struct user_settings *s, const char *val)
{
    int n = atoi(val);

    /* use default toxic colours if invalid value */
    s->colour_theme = n == NATIVE_COLS ? NATIVE_COLS : DFLT_COLS;
}

#ifdef _SUPPORT_AUDIO

static void uset_ain_dev(struct user_settings *s, const char *val)
{
    int n = atoi(val);

    if (n < 0 || n > MAX_DEVICES)
        n = (long int) 0;

    s->audio_in_dev = (long int) n;
}

static void uset_aout_dev(struct user_settings *s, const char *val)
{
    int n = atoi(val);

    if (n < 0 || n > MAX_DEVICES)
        n = (long int) 0;

    s->audio_out_dev = (long int) n;
}

#endif /* _SUPPORT_AUDIO */

static void uset_hst_size(struct user_settings *s, const char *val)
{
    int n = atoi(val);

    /* if val is out of range use default history size */
    s->history_size = (n > MAX_HISTORY || n < MIN_HISTORY) ? DFLT_HST_SIZE : n;
}

static void uset_dwnld_path(struct user_settings *s, const char *val)
{
    memset(s->download_path, 0, sizeof(s->download_path));

    if (val == NULL)
        return;

    int len = strlen(val);

    if (len >= sizeof(s->download_path) - 2)  /* leave room for null and '/' */
        return;

    FILE *fp = fopen(val, "r");

    if (fp == NULL)
        return;

    strcpy(s->download_path, val);

    if (val[len] != '/')
        strcat(s->download_path, "/");
}

static void set_default_settings(struct user_settings *s)
{
    /* see settings_values enum in settings.h for defaults */
    uset_autolog(s, "0");
    uset_time(s, "24");
    uset_alerts(s, "0");
    uset_colours(s, "0");
    uset_hst_size(s, "700");
    uset_dwnld_path(s, NULL);

#ifdef _SUPPORT_AUDIO
    uset_ain_dev(s, "0");
    uset_aout_dev(s, "0");
#endif
}

int settings_load(struct user_settings *s, char *path)
{
    char *user_config_dir = get_user_config_dir();
    FILE *fp = NULL;
    char dflt_path[MAX_STR_SIZE];

    if (path) {
        fp = fopen(path, "r");
    } else {
        snprintf(dflt_path, sizeof(dflt_path), "%s%stoxic.conf", user_config_dir, CONFIGDIR);
        fp = fopen(dflt_path, "r");
    }

    free(user_config_dir);

    set_default_settings(s);

    if (fp == NULL && !path) {
        if ((fp = fopen(dflt_path, "w")) == NULL)
            return -1;
    } else if (fp == NULL && path) {
        return -1;
    }

    char line[MAX_STR_SIZE];

    while (fgets(line, sizeof(line), fp)) {
        if (line[0] == '#' || !line[0])
            continue;

        const char *key = strtok(line, ":");
        const char *val = strtok(NULL, ";");

        if (key == NULL || val == NULL)
            continue;

        int i;

        for (i = 0; i < NUM_SETTINGS; ++i) {
            if (!strcmp(user_settings_list[i].key, key)) {
                (user_settings_list[i].func)(s, val);
                break;
            }
        }
    }

    fclose(fp);
    return 0;
}

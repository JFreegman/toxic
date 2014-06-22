/*  log.c
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "configdir.h"
#include "toxic.h"
#include "windows.h"
#include "misc_tools.h"
#include "log.h"
#include "settings.h"

extern struct user_settings *user_settings;

/* Creates/fetches log file by appending to the config dir the name and a pseudo-unique identity */
void init_logging_session(uint8_t *name, uint8_t *key, struct chatlog *log)
{
    if (!log->log_on)
        return;

    if (!valid_nick(name))
        name = UNKNOWN_NAME;

    char *user_config_dir = get_user_config_dir();
    int path_len = strlen(user_config_dir) + strlen(CONFIGDIR) + strlen(name);

    /* use first 4 digits of key as log ident. If no key use a timestamp */
    uint8_t ident[32];

    if (key != NULL) {
        path_len += (KEY_IDENT_DIGITS * 2 + 5);

        sprintf(&ident[0], "%02X", key[0] & 0xff);
        sprintf(&ident[2], "%02X", key[1] & 0xff);
        ident[KEY_IDENT_DIGITS * 2 + 1] = '\0';
    } else {
        strftime(ident, sizeof(ident), "%Y-%m-%d[%H:%M:%S]", get_time());
        path_len += strlen(ident) + 1;
    }

    if (path_len > MAX_STR_SIZE) {
        log->log_on = false;
        free(user_config_dir);
        return;
    }

    uint8_t log_path[MAX_STR_SIZE];

    snprintf(log_path, MAX_STR_SIZE, "%s%s%s-%s.log",
             user_config_dir, CONFIGDIR, name, ident);

    free(user_config_dir);

    log->file = fopen(log_path, "a");

    if (log->file == NULL) {
        log->log_on = false;
        return;
    }

    fprintf(log->file, "\n*** NEW SESSION ***\n\n");
}

void write_to_log(const uint8_t *msg, uint8_t *name, struct chatlog *log, bool event)
{
    if (!log->log_on)
        return;

    if (log->file == NULL) {
        log->log_on = false;
        return;
    }

    uint8_t name_frmt[TOXIC_MAX_NAME_LENGTH + 3];

    if (event)
        snprintf(name_frmt, sizeof(name_frmt), "* %s", name);
    else
        snprintf(name_frmt, sizeof(name_frmt), "%s:", name);

    const char *t = user_settings->time == TIME_12 ? "%Y/%m/%d [%I:%M:%S %p]" : "%Y/%m/%d [%H:%M:%S]";
    uint8_t s[MAX_STR_SIZE];
    strftime(s, MAX_STR_SIZE, t, get_time());
    fprintf(log->file, "%s %s %s\n", s, name_frmt, msg);

    uint64_t curtime = get_unix_time();

    if (timed_out(log->lastwrite, curtime, LOG_FLUSH_LIMIT)) {
        fflush(log->file);
        log->lastwrite = curtime;
    }
}

void log_enable(uint8_t *name, uint8_t *key, struct chatlog *log)
{
    log->log_on = true;

    if (log->file == NULL)
        init_logging_session(name, key, log);
}

void log_disable(struct chatlog *log)
{
    log->log_on = false;

    if (log->file != NULL) {
        fclose(log->file);
        log->file = NULL;
    }
}

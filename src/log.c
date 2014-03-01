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

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "configdir.h"
#include "toxic_windows.h"
#include "misc_tools.h"

/* gets the log path by appending to the config dir the name and a pseudo-unique identity */
void init_logging_session(uint8_t *name, uint8_t *key, struct chatlog *log)
{
    if (!log->log_on)
        return;

    char *user_config_dir = get_user_config_dir();
    int path_len = strlen(user_config_dir) + strlen(CONFIGDIR) + strlen(name); 

    /* use first 4 digits of key as log ident. If no key use a timestamp */
    uint8_t ident[32];

    if (key != NULL) {
        path_len += (KEY_IDENT_DIGITS * 2 + 5);

        sprintf(&ident[0], "%02X", key[0] & 0xff);
        sprintf(&ident[2], "%02X", key[2] & 0xff);
        ident[KEY_IDENT_DIGITS*2+1] = '\0';
    } else {
        struct tm *tminfo = get_time();
        snprintf(ident, sizeof(ident), 
                "%04d-%02d-%02d[%02d:%02d:%02d]", tminfo->tm_year+1900,tminfo->tm_mon+1, tminfo->tm_mday, 
                                                  tminfo->tm_hour, tminfo->tm_min, tminfo->tm_sec);
        path_len += strlen(ident) + 1;
    }

    if (path_len > MAX_STR_SIZE) {
        log->log_on = false;
        return;
    }

    snprintf(log->log_path, MAX_STR_SIZE, "%s%s%s-%s.log", 
             user_config_dir, CONFIGDIR, name, ident);

    FILE *logfile = fopen(log->log_path, "a");

    if (logfile == NULL) {
        log->log_on = false;
        return;
    }

    fprintf(logfile, "\n*** NEW SESSION ***\n\n");

    fclose(logfile);
    free(user_config_dir);
}

/* writes contents from a chatcontext's log buffer to respective log file and resets log pos.
   This is triggered when the log buffer is full, but may be forced. */
void write_to_log(struct chatlog *log)
{
    if (!log->log_on)
        return;

    FILE *logfile = fopen(log->log_path, "a");

    if (logfile == NULL) {
        log->log_on = false;
        return;
    }

    int i;

    for (i = 0; i < log->pos; ++i)
        fprintf(logfile, "%s", log->log_buf[i]);

    log->pos = 0;
    fclose(logfile);
}

/* Adds line/event to log_buf with timestamp and name. If buf is full, triggers write_to_log.
   If event is true, formats line as an event, e.g. * name has gone offline */
void add_to_log_buf(uint8_t *msg, uint8_t *name, struct chatlog *log, bool event)
{
    if (!log->log_on)
        return;

    uint8_t name_frmt[TOXIC_MAX_NAME_LENGTH + 3];

    if (event)
        snprintf(name_frmt, sizeof(name_frmt), "* %s", name);
    else
        snprintf(name_frmt, sizeof(name_frmt), "%s:", name);

    struct tm *tminfo = get_time();
    snprintf(log->log_buf[log->pos], MAX_LOG_LINE_SIZE, "%04d/%02d/%02d [%02d:%02d:%02d] %s %s\n", 
                                  tminfo->tm_year + 1900, tminfo->tm_mon + 1, tminfo->tm_mday, 
                                  tminfo->tm_hour, tminfo->tm_min, tminfo->tm_sec, name_frmt, msg);

    if (++(log->pos) >= MAX_LOG_BUF_LINES)
        write_to_log(log);
}

void log_enable(uint8_t *name, uint8_t *key, struct chatlog *log)
{
    log->log_on = true;

    if (!log->log_path[0])
        init_logging_session(name, key, log);
}

void log_disable(struct chatlog *log)
{
    if (log->log_on) {
        write_to_log(log);
        log->log_on = false;
    }
}

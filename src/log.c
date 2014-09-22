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
#include <sys/stat.h>

#include "configdir.h"
#include "toxic.h"
#include "windows.h"
#include "misc_tools.h"
#include "log.h"
#include "settings.h"
#include "line_info.h"

extern struct user_settings *user_settings_;

/* Creates/fetches log file by appending to the config dir the name and a pseudo-unique identity */
void init_logging_session(char *name, const char *key, struct chatlog *log)
{
    if (!log->log_on)
        return;

    if (!valid_nick(name))
        name = UNKNOWN_NAME;

    const char *set_path = user_settings_->chatlogs_path;

    char *user_config_dir = get_user_config_dir();
    int path_len = strlen(set_path) + strlen(name) ? *set_path
                   : strlen(user_config_dir) + strlen(LOGDIR) + strlen(name);

    /* use first 4 digits of key as log ident. If no key use a timestamp */
    char ident[32];

    if (key != NULL) {
        path_len += (KEY_IDENT_DIGITS * 2 + 5);
        sprintf(&ident[0], "%02X", key[0] & 0xff);
        sprintf(&ident[2], "%02X", key[1] & 0xff);
        ident[KEY_IDENT_DIGITS * 2 + 1] = '\0';
    } else {
        strftime(ident, sizeof(ident), "%Y-%m-%d[%H:%M:%S]", get_time());
        path_len += strlen(ident) + 1;
    }

    if (path_len >= MAX_STR_SIZE) {
        log->log_on = false;
        free(user_config_dir);
        return;
    }

    char log_path[MAX_STR_SIZE];

    if (*set_path)
        snprintf(log_path, sizeof(log_path), "%s%s-%s.log", set_path, name, ident);
    else
        snprintf(log_path, sizeof(log_path), "%s%s%s-%s.log", user_config_dir, LOGDIR, name, ident);

    free(user_config_dir);

    log->file = fopen(log_path, "a+");
    snprintf(log->path, sizeof(log->path), "%s", log_path);

    if (log->file == NULL) {
        log->log_on = false;
        return;
    }
}

#define LOG_FLUSH_LIMIT 1  /* limits calls to fflush to a max of one per LOG_FLUSH_LIMIT seconds */

void write_to_log(const char *msg, const char *name, struct chatlog *log, bool event)
{
    if (!log->log_on)
        return;

    if (log->file == NULL) {
        log->log_on = false;
        return;
    }

    char name_frmt[TOXIC_MAX_NAME_LENGTH + 3];

    if (event)
        snprintf(name_frmt, sizeof(name_frmt), "* %s", name);
    else
        snprintf(name_frmt, sizeof(name_frmt), "%s:", name);

    const char *t = user_settings_->time == TIME_12 ? "%Y/%m/%d [%I:%M:%S %p]" : "%Y/%m/%d [%H:%M:%S]";
    char s[MAX_STR_SIZE];
    strftime(s, MAX_STR_SIZE, t, get_time());
    fprintf(log->file, "%s %s %s\n", s, name_frmt, msg);

    uint64_t curtime = get_unix_time();

    if (timed_out(log->lastwrite, curtime, LOG_FLUSH_LIMIT)) {
        fflush(log->file);
        log->lastwrite = curtime;
    }
}

void log_enable(char *name, const char *key, struct chatlog *log)
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

/* Loads previous history from chat log */
void load_chat_history(ToxWindow *self, struct chatlog *log)
{
    if (log->file == NULL)
        return;

    struct stat st;

    if (stat(log->path, &st) == -1) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, RED, "* Failed to stat log file");
        return;
    }

    int sz = st.st_size;

    if (sz <= 0)
        return;

    char *hstbuf = malloc(sz);

    if (hstbuf == NULL)
        exit_toxic_err("failed in print_prev_chat_history", FATALERR_MEMORY);

    if (fread(hstbuf, sz, 1, log->file) != 1) {
        free(hstbuf);
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, RED, "* Failed to read log file");
        return;
    }

    /* Number of history lines to load: must not be larger than MAX_LINE_INFO_QUEUE - 2 */
    int L = MIN(MAX_LINE_INFO_QUEUE - 2, user_settings_->history_size);
    int start, count = 0;

    /* start at end and backtrace L lines or to the beginning of buffer */
    for (start = sz - 1; start >= 0 && count < L; --start) {
        if (hstbuf[start] == '\n')
            ++count;
    }

    char buf[MAX_STR_SIZE];
    const char *line = strtok(&hstbuf[start + 1], "\n");

    if (line == NULL) {
        free(hstbuf);
        return;
    }

    while (line != NULL) {
        snprintf(buf, sizeof(buf), "%s", line);
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "%s", buf);
        line = strtok(NULL, "\n");
    }

    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "");
    free(hstbuf);
}

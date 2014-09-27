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

extern struct user_settings *user_settings;

/* Opens log file or creates a new one */
static int init_logging_session(char *name, const char *selfkey, const char *otherkey, struct chatlog *log, int logtype)
{
    if (selfkey == NULL || (logtype == LOG_CHAT && otherkey == NULL))
        return -1;

    if (!valid_nick(name))
        name = UNKNOWN_NAME;

    const char *namedash = logtype == LOG_PROMPT ? "" : "-";
    const char *set_path = user_settings->chatlogs_path;

    char *user_config_dir = get_user_config_dir();
    int path_len = strlen(name) + strlen(".log") + strlen("-") + strlen(namedash);
    path_len += strlen(set_path) ? *set_path : strlen(user_config_dir) + strlen(LOGDIR);

    /* first 6 digits of selfkey */
    char self_id[32];
    path_len += KEY_IDENT_DIGITS * 2;
    sprintf(&self_id[0], "%02X", selfkey[0] & 0xff);
    sprintf(&self_id[2], "%02X", selfkey[1] & 0xff);
    sprintf(&self_id[4], "%02X", selfkey[2] & 0xff);
    self_id[KEY_IDENT_DIGITS * 2] = '\0';

    char other_id[32] = {0};

    switch (logtype) {
        case LOG_CHAT:
            path_len += KEY_IDENT_DIGITS * 2;
            sprintf(&other_id[0], "%02X", otherkey[0] & 0xff);
            sprintf(&other_id[2], "%02X", otherkey[1] & 0xff);
            sprintf(&other_id[4], "%02X", otherkey[2] & 0xff);
            other_id[KEY_IDENT_DIGITS * 2] = '\0';
            break;

        case LOG_GROUP:
            strftime(other_id, sizeof(other_id), "%Y-%m-%d[%H:%M:%S]", get_time());
            path_len += strlen(other_id);
            break;
    }

    char log_path[MAX_STR_SIZE];

    if (path_len >= sizeof(log_path)) {
        free(user_config_dir);
        return -1;
    }

    if (!string_is_empty(set_path))
        snprintf(log_path, sizeof(log_path), "%s%s-%s%s%s.log", set_path, self_id, name, namedash, other_id);
    else
        snprintf(log_path, sizeof(log_path), "%s%s%s-%s%s%s.log", user_config_dir, LOGDIR, self_id, name, namedash, other_id);

    free(user_config_dir);
    log->file = fopen(log_path, "a+");
    snprintf(log->path, sizeof(log->path), "%s", log_path);

    if (log->file == NULL)
        return -1;

    return 0;
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

    const char *t = user_settings->time == TIME_12 ? "%Y/%m/%d [%I:%M:%S %p]" : "%Y/%m/%d [%H:%M:%S]";
    char s[MAX_STR_SIZE];
    strftime(s, MAX_STR_SIZE, t, get_time());
    fprintf(log->file, "%s %s %s\n", s, name_frmt, msg);

    uint64_t curtime = get_unix_time();

    if (timed_out(log->lastwrite, curtime, LOG_FLUSH_LIMIT)) {
        fflush(log->file);
        log->lastwrite = curtime;
    }
}

void log_disable(struct chatlog *log)
{
    log->log_on = false;

    if (log->file != NULL) {
        fclose(log->file);
        log->file = NULL;
    }
}

void log_enable(char *name, const char *selfkey, const char *otherkey, struct chatlog *log, int logtype)
{
    log->log_on = true;

    if (log->file == NULL) {
        if (init_logging_session(name, selfkey, otherkey, log, logtype) == -1)
            log_disable(log);
    }
}

/* Loads previous history from chat log */
void load_chat_history(ToxWindow *self, struct chatlog *log)
{
    if (log->file == NULL)
        return;

    off_t sz = file_size(log->path);

    if (sz <= 0)
        return;

    char *hstbuf = malloc(sz);

    if (hstbuf == NULL)
        exit_toxic_err("failed in load_chat_history", FATALERR_MEMORY);

    if (fseek(log->file, 0L, SEEK_SET) == -1) {
        free(hstbuf);
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, RED, " * Failed to read log file");
        return;
    }

    if (fread(hstbuf, sz, 1, log->file) != 1) {
        free(hstbuf);
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, RED, " * Failed to read log file");
        return;
    }

    /* Number of history lines to load: must not be larger than MAX_LINE_INFO_QUEUE - 2 */
    int L = MIN(MAX_LINE_INFO_QUEUE - 2, user_settings->history_size);
    int start, count = 0;

    /* start at end and backtrace L lines or to the beginning of buffer */
    for (start = sz - 1; start >= 0 && count < L; --start) {
        if (hstbuf[start] == '\n')
            ++count;
    }

    const char *line = strtok(&hstbuf[start + 1], "\n");

    if (line == NULL) {
        free(hstbuf);
        return;
    }

    while (line != NULL && count--) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "%s", line);
        line = strtok(NULL, "\n");
    }

    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "");
    free(hstbuf);
}

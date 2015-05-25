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

#ifdef NO_GETTEXT
#define gettext(A) (A)
#else
#include <libintl.h>
#endif

#include "configdir.h"
#include "toxic.h"
#include "windows.h"
#include "misc_tools.h"
#include "log.h"
#include "settings.h"
#include "line_info.h"

extern struct user_settings *user_settings;

/* There are three types of logs: chat logs, groupchat logs, and prompt logs (see LOG_TYPE in log.h)
   A prompt log is in the format: LOGDIR/selfkey-home.log
   A chat log is in the format: LOGDIR/selfkey-friendname-otherkey.log
   A groupchat log is in the format: LOGDIR/selfkey-groupname-date[time].log

   Only the first (KEY_IDENT_DIGITS * 2) numbers of the key are used.

   Returns 0 on success, -1 if the path is too long */
static int get_log_path(char *dest, int destsize, char *name, const char *selfkey, const char *otherkey, int logtype)
{
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

    if (path_len >= destsize) {
        free(user_config_dir);
        return -1;
    }

    if (!string_is_empty(set_path))
        snprintf(dest, destsize, "%s%s-%s%s%s.log", set_path, self_id, name, namedash, other_id);
    else
        snprintf(dest, destsize, "%s%s%s-%s%s%s.log", user_config_dir, LOGDIR, self_id, name, namedash, other_id);

    free(user_config_dir);

    return 0;
}

/* Opens log file or creates a new one */
static int init_logging_session(char *name, const char *selfkey, const char *otherkey, struct chatlog *log, int logtype)
{
    if (selfkey == NULL || (logtype == LOG_CHAT && otherkey == NULL))
        return -1;

    char log_path[MAX_STR_SIZE];

    if (get_log_path(log_path, sizeof(log_path), name, selfkey, otherkey, logtype) == -1)
        return -1;

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

    const char *t = user_settings->log_timestamp_format;
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
    if (log->file != NULL)
        fclose(log->file);

    memset(log, 0, sizeof(struct chatlog));
}

void log_enable(char *name, const char *selfkey, const char *otherkey, struct chatlog *log, int logtype)
{
    log->log_on = true;

    if (log->file != NULL)
        return;

    if (init_logging_session(name, selfkey, otherkey, log, logtype) == -1)
        log_disable(log);
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
        exit_toxic_err(gettext("failed in load_chat_history"), FATALERR_MEMORY);

    if (fseek(log->file, 0L, SEEK_SET) == -1) {
        free(hstbuf);
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, RED, gettext(" * Failed to read log file"));
        return;
    }

    if (fread(hstbuf, sz, 1, log->file) != 1) {
        free(hstbuf);
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, RED, gettext(" * Failed to read log file"));
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

/* renames chatlog file replacing src with dest.
   Returns 0 on success or if no log exists, -1 on failure. */
int rename_logfile(char *src, char *dest, const char *selfkey, const char *otherkey, int winnum)
{
    ToxWindow *toxwin = get_window_ptr(winnum);
    struct chatlog *log = NULL;
    bool log_on = false;

    /* disable log if necessary and save its state */
    if (toxwin != NULL) {
        log = toxwin->chatwin->log;
        log_on = log->log_on;
    }

    if (log_on)
        log_disable(log);

    char newpath[MAX_STR_SIZE];
    char oldpath[MAX_STR_SIZE];

    if (get_log_path(oldpath, sizeof(oldpath), src, selfkey, otherkey, LOG_CHAT) == -1)
        goto on_error;

    if (!file_exists(oldpath))
        return 0;

    if (get_log_path(newpath, sizeof(newpath), dest, selfkey, otherkey, LOG_CHAT) == -1)
        goto on_error;

    if (rename(oldpath, newpath) != 0)
        goto on_error;

    if (log_on)
        log_enable(dest, selfkey, otherkey, log, LOG_CHAT);

    return 0;

on_error:
    if (log_on)
        log_enable(src, selfkey, otherkey, log, LOG_CHAT);

    return -1;
}

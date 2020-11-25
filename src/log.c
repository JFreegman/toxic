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
#include <sys/stat.h>
#include <time.h>

#include "configdir.h"
#include "line_info.h"
#include "log.h"
#include "misc_tools.h"
#include "settings.h"
#include "toxic.h"
#include "windows.h"

extern struct user_settings *user_settings;

/* Creates a log path and puts it in `dest.
 *
 * There are two types of logs: chat logs and prompt logs (see LOG_TYPE in log.h)
 * A prompt log is in the format: LOGDIR/selfkey-home.log
 * A chat log is in the format: LOGDIR/selfkey-name-otherkey.log
 *
 * For friend chats `otherkey` is the first 6 bytes of the friend's Tox ID.
 * For Conferences/groups `otherkey` is the first 6 bytes of the group's unique ID.
 *
 * Return path length on success.
 * Return -1 if the path is too long.
 */
static int get_log_path(char *dest, int destsize, const char *name, const char *selfkey, const char *otherkey)
{
    if (!valid_nick(name)) {
        name = UNKNOWN_NAME;
    }

    const char *namedash = otherkey ? "-" : "";
    const char *set_path = user_settings->chatlogs_path;

    char *user_config_dir = get_user_config_dir();
    int path_len = strlen(name) + strlen(".log") + strlen("-") + strlen(namedash);
    path_len += strlen(set_path) ? *set_path : strlen(user_config_dir) + strlen(LOGDIR);

    /* first 6 bytes of selfkey */
    char self_id[32] = {0};
    path_len += KEY_IDENT_DIGITS * 2;
    sprintf(&self_id[0], "%02X", selfkey[0] & 0xff);
    sprintf(&self_id[2], "%02X", selfkey[1] & 0xff);
    sprintf(&self_id[4], "%02X", selfkey[2] & 0xff);
    self_id[KEY_IDENT_DIGITS * 2] = '\0';

    char other_id[32] = {0};

    if (otherkey) {
        /* first 6 bytes of otherkey */
        path_len += KEY_IDENT_DIGITS * 2;
        sprintf(&other_id[0], "%02X", otherkey[0] & 0xff);
        sprintf(&other_id[2], "%02X", otherkey[1] & 0xff);
        sprintf(&other_id[4], "%02X", otherkey[2] & 0xff);
        other_id[KEY_IDENT_DIGITS * 2] = '\0';
    }

    if (path_len >= destsize) {
        free(user_config_dir);
        return -1;
    }

    if (!string_is_empty(set_path)) {
        snprintf(dest, destsize, "%s%s-%s%s%s.log", set_path, self_id, name, namedash, other_id);
    } else {
        snprintf(dest, destsize, "%s%s%s-%s%s%s.log", user_config_dir, LOGDIR, self_id, name, namedash, other_id);
    }

    free(user_config_dir);

    return path_len;
}

/* Initializes log path for `log`.
 *
 * Return 0 on success.
 * Return -1 on failure.
 */
static int init_logging_session(const char *name, const char *selfkey, const char *otherkey, struct chatlog *log,
                                LOG_TYPE type)
{
    if (log == NULL) {
        return -1;
    }

    if (selfkey == NULL || (type == LOG_TYPE_CHAT && otherkey == NULL)) {
        return -1;
    }

    char log_path[MAX_STR_SIZE];

    int path_len = get_log_path(log_path, sizeof(log_path), name, selfkey, otherkey);

    if (path_len == -1 || path_len >= sizeof(log->path)) {
        return -1;
    }

    memcpy(log->path, log_path, path_len);
    log->path[path_len] = 0;

    return 0;
}

#define LOG_FLUSH_LIMIT 1  /* limits calls to fflush to a max of one per LOG_FLUSH_LIMIT seconds */

void write_to_log(const char *msg, const char *name, struct chatlog *log, bool event)
{
    if (log == NULL) {
        return;
    }

    if (!log->log_on) {
        return;
    }

    if (log->file == NULL) {
        log->log_on = false;
        return;
    }

    char name_frmt[TOXIC_MAX_NAME_LENGTH + 3];

    if (event) {
        snprintf(name_frmt, sizeof(name_frmt), "* %s", name);
    } else {
        snprintf(name_frmt, sizeof(name_frmt), "%s:", name);
    }

    const char *t = user_settings->log_timestamp_format;
    char s[MAX_STR_SIZE];
    strftime(s, MAX_STR_SIZE, t, get_time());
    fprintf(log->file, "%s %s %s\n", s, name_frmt, msg);

    if (timed_out(log->lastwrite, LOG_FLUSH_LIMIT)) {
        fflush(log->file);
        log->lastwrite = get_unix_time();
    }
}

void log_disable(struct chatlog *log)
{
    if (log == NULL) {
        return;
    }

    if (log->file != NULL) {
        fclose(log->file);
        log->file = NULL;
    }

    log->lastwrite = 0;
    log->log_on = false;
}

int log_enable(struct chatlog *log)
{
    if (log == NULL) {
        return -1;
    }

    if (log->log_on) {
        return 0;
    }

    if (*log->path == 0) {
        return -1;
    }

    if (log->file != NULL) {
        return -1;
    }

    log->file = fopen(log->path, "a+");

    if (log->file == NULL) {
        return -1;
    }

    log->log_on = true;

    return 0;
}

/* Initializes a log. This function must be called before any other logging operations.
 *
 * Return 0 on success.
 * Return -1 on failure.
 */
int log_init(struct chatlog *log, const char *name, const char *selfkey, const char *otherkey, LOG_TYPE type)
{
    if (log == NULL) {
        return -1;
    }

    if (log->file != NULL || log->log_on) {
        fprintf(stderr, "Warning: Called log_init() on an already initialized log\n");
        return -1;
    }

    if (init_logging_session(name, selfkey, otherkey, log, type) == -1) {
        return -1;
    }

    log_disable(log);

    return 0;
}

/* Loads chat log history and prints it to `self` window.
 *
 * Return 0 on success or if log file doesn't exist.
 * Return -1 on failure.
 */
int load_chat_history(ToxWindow *self, struct chatlog *log)
{
    if (log == NULL) {
        return -1;
    }

    if (*log->path == 0) {
        return -1;
    }

    off_t sz = file_size(log->path);

    if (sz <= 0) {
        return 0;
    }

    FILE *fp = fopen(log->path, "r");

    if (fp == NULL) {
        return -1;
    }

    char *buf = malloc(sz + 1);

    if (buf == NULL) {
        fclose(fp);
        return -1;
    }

    if (fseek(fp, 0L, SEEK_SET) == -1) {
        free(buf);
        fclose(fp);
        return -1;
    }

    if (fread(buf, sz, 1, fp) != 1) {
        free(buf);
        fclose(fp);
        return -1;
    }

    fclose(fp);

    buf[sz] = 0;

    /* Number of history lines to load: must not be larger than MAX_LINE_INFO_QUEUE - 2 */
    int L = MIN(MAX_LINE_INFO_QUEUE - 2, user_settings->history_size);

    int start = 0;
    int count = 0;

    /* start at end and backtrace L lines or to the beginning of buffer */
    for (start = sz - 1; start >= 0 && count < L; --start) {
        if (buf[start] == '\n') {
            ++count;
        }
    }

    char *tmp = NULL;
    const char *line = strtok_r(&buf[start + 1], "\n", &tmp);

    if (line == NULL) {
        free(buf);
        return -1;
    }

    while (line != NULL && count--) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "%s", line);
        line = strtok_r(NULL, "\n", &tmp);
    }

    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "");

    free(buf);

    return 0;
}

/* Renames chatlog file `src` to `dest`.
 *
 * Return 0 on success or if no log exists.
 * Return -1 on failure.
 */
int rename_logfile(const char *src, const char *dest, const char *selfkey, const char *otherkey, int winnum)
{
    ToxWindow *toxwin = get_window_ptr(winnum);
    struct chatlog *log = NULL;
    bool log_on = false;

    /* disable log if necessary and save its state */
    if (toxwin != NULL) {
        log = toxwin->chatwin->log;

        if (log == NULL) {
            return -1;
        }

        log_on = log->log_on;
    }

    if (log_on) {
        log_disable(log);
    }

    char newpath[MAX_STR_SIZE];
    char oldpath[MAX_STR_SIZE];

    if (get_log_path(oldpath, sizeof(oldpath), src, selfkey, otherkey) == -1) {
        goto on_error;
    }

    if (!file_exists(oldpath)) {
        init_logging_session(dest, selfkey, otherkey, log, LOG_TYPE_CHAT);  // still need to rename path
        return 0;
    }

    int new_path_len = get_log_path(newpath, sizeof(newpath), dest, selfkey, otherkey);

    if (new_path_len == -1 || new_path_len >= MAX_STR_SIZE) {
        goto on_error;
    }

    if (file_exists(newpath)) {
        remove(oldpath);
    } else if (rename(oldpath, newpath) != 0) {
        goto on_error;
    }

    if (log != NULL) {
        memcpy(log->path, newpath, new_path_len);
        log->path[new_path_len] = 0;

        if (log_on) {
            log_enable(log);
        }
    }

    return 0;

on_error:

    if (log_on) {
        log_enable(log);
    }

    return -1;
}

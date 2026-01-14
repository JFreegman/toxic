/*  log.c
 *
 *  Copyright (C) 2014-2026 Toxic All Rights Reserved.
 *
 *  This file is part of Toxic. Toxic is free software licensed
 *  under the GNU General Public License 3.0.
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

/* Creates a log path and puts it in `dest.
 *
 * There are two types of logs: chat logs and prompt logs (see Log_Type in log.h)
 * A prompt log is in the format: LOGDIR/selfkey-home.log
 * A chat log is in the format: LOGDIR/selfkey-name-otherkey.log
 *
 * For friend chats `otherkey` is the first 6 bytes of the friend's Tox ID.
 * For Conferences/groups `otherkey` is the first 6 bytes of the group's unique ID.
 *
 * Return path length on success.
 * Return -1 if the path is too long.
 */
static int create_log_path(const Client_Config *c_config, const Paths *paths, char *dest, int destsize,
                           const char *name, const char *selfkey, const char *otherkey)
{
    if (!valid_nick(name)) {
        name = UNKNOWN_NAME;
    }

    const char *namedash = otherkey ? "-" : "";
    const char *set_path = c_config->chatlogs_path;

    char *user_config_dir = get_user_config_dir(paths);
    int path_len = strlen(name) + strlen(".log") + strlen("-") + strlen(namedash);
    path_len += strlen(set_path) ? *set_path : strlen(user_config_dir) + strlen(LOGDIR);

    /* first 6 bytes of selfkey */
    char self_id[32] = {0};
    path_len += KEY_IDENT_BYTES;
    sprintf(&self_id[0], "%02X", selfkey[0] & 0xff);
    sprintf(&self_id[2], "%02X", selfkey[1] & 0xff);
    sprintf(&self_id[4], "%02X", selfkey[2] & 0xff);
    self_id[KEY_IDENT_BYTES] = '\0';

    char other_id[32] = {0};

    if (otherkey) {
        /* first 6 bytes of otherkey */
        path_len += KEY_IDENT_BYTES;
        sprintf(&other_id[0], "%02X", otherkey[0] & 0xff);
        sprintf(&other_id[2], "%02X", otherkey[1] & 0xff);
        sprintf(&other_id[4], "%02X", otherkey[2] & 0xff);
        other_id[KEY_IDENT_BYTES] = '\0';
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
static int init_logging_session(const Client_Config *c_config, const Paths *paths, const char *name,
                                const char *selfkey, const char *otherkey, struct chatlog *log, Log_Type type)
{
    if (log == NULL) {
        return -1;
    }

    if (selfkey == NULL || (type == LOG_TYPE_CHAT && otherkey == NULL)) {
        return -1;
    }

    char log_path[MAX_STR_SIZE];

    const int path_len = create_log_path(c_config, paths, log_path, sizeof(log_path), name, selfkey, otherkey);

    if (path_len == -1 || path_len >= sizeof(log->path)) {
        return -1;
    }

    memcpy(log->path, log_path, path_len);
    log->path[path_len] = '\0';

    return 0;
}

/* limits calls to fflush to a max of one per LOG_FLUSH_LIMIT seconds */
#define LOG_FLUSH_LIMIT 1

/* We stop writing to the log after we've written at least this many bytes during the current session.
 * A new session is started with `log_enable()`, and ended with `log_disable()`.
 */
#define LOG_BYTES_THRESHOLD ((1024 << 10) * 100)  // 100 MiB

int write_to_log(struct chatlog *log, const Client_Config *c_config, const char *msg, const char *name,
                 Log_Hint log_hint)
{
    if (log == NULL) {
        return -1;
    }

    if (!log->log_on) {
        return 0;
    }

    if (log->file == NULL) {
        log->log_on = false;
        return -1;
    }

    if (log->bytes_written >= LOG_BYTES_THRESHOLD) {
        fprintf(stderr, "Warning: Log file is full (%u bytes written)\n", log->bytes_written);
        return -1;
    }

    char name_frmt[TOXIC_MAX_NAME_LENGTH + 2];

    if (name != NULL) {
        snprintf(name_frmt, sizeof(name_frmt), "%s:", name);
    }

    const char *t = c_config->log_timestamp_format;
    char s[MAX_STR_SIZE];
    get_time_str(s, sizeof(s), t);

    int bytes_written;

    if (name == NULL) {
        bytes_written = fprintf(log->file, "{%d} %s %s\n", log_hint, s, msg);
    } else {
        bytes_written = fprintf(log->file, "{%d} %s %s %s\n", log_hint, s, name_frmt, msg);
    }

    if (timed_out(log->lastwrite, LOG_FLUSH_LIMIT)) {
        fflush(log->file);
        log->lastwrite = get_unix_time();
    }

    if (bytes_written > 0) {
        log->bytes_written += bytes_written;
    }

    return 0;
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
    log->bytes_written = 0;
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
int log_init(struct chatlog *log, const Client_Config *c_config, const Paths *paths, const char *name,
             const char *selfkey, const char *otherkey, Log_Type type)
{
    if (log == NULL) {
        return -1;
    }

    if (log->file != NULL || log->log_on) {
        fprintf(stderr, "Warning: Called log_init() on an already initialized log\n");
        return -1;
    }

    if (init_logging_session(c_config, paths, name, selfkey, otherkey, log, type) == -1) {
        return -1;
    }

    log_disable(log);

    return 0;
}

/*
 * Extracts a log hint from a log line.
 *
 * A hint starts at the beginning of the line and is a 1 or 2 digit integer
 * surrounded by curly bracers.
 *
 * Return hint on success.
 * Return -1 on parsing error.
 */
static int extract_log_hint(const char *line, size_t length)
{
    const int start = char_find(0, line, '{');
    const int end = char_find(0, line, '}');
    const int num_digits = end - start - 1;

    if (start != 0 || end == length || num_digits < 1 || num_digits > 2) {
        return -1;
    }

    char digits[3];
    memcpy(digits, &line[1], num_digits);
    digits[num_digits] = '\0';

    const long int hint = strtol(digits, NULL, 10);

    return hint >= 0 && hint != LONG_MAX ? hint : -1;
}

/*
 * Extracts the timestamp from line.
 *
 * A timestamp comes after the log hint and is surrounded by square brackets.
 *
 * Return the index of the end of the timestamp on success.
 * Return -1 on parsing error.
 */
static int extract_timestamp(const char *line, size_t length, char *buf, size_t buf_size)
{
    const int start_ts = char_find(0, line, '[') + 1;
    const int end_ts = char_find(0, line, ']');
    const int ts_len = end_ts - start_ts;

    if (ts_len <= 0 || ts_len >= buf_size || start_ts < 0 || start_ts + ts_len >= length
            || start_ts >= length || end_ts <= 0 || end_ts >= length) {
        return -1;
    }

    memcpy(buf, &line[start_ts], ts_len);
    buf[ts_len] = '\0';

    return end_ts;
}

/*
 * Extracts the name from line.
 *
 * A name comes after the timestamp and is proceeded by a colon.
 *
 * Return the index of the end of the name on success.
 * Return -1 on parsing error.
 */
static int extract_log_name(const char *line, size_t length, char *buf, size_t buf_size)
{
    const int end_name = char_find(0, line, ':');

    if (end_name + 2 >= length || end_name <= 0) {
        return -1;
    }

    const int start_name = 1;
    const int name_len = end_name - start_name - 1;

    if (start_name + 1 >= length || name_len >= buf_size || start_name + 1 + name_len >= length
            || name_len <= 0) {
        return -1;
    }

    memcpy(buf, &line[start_name + 1], name_len);
    buf[name_len] = '\0';

    return end_name;
}

static bool load_line_topic(ToxWindow *self, const Client_Config *c_config, const char *line, size_t length,
                            const char *timestamp)
{
    if (length <= 2) {
        return false;
    }

    line_info_load_history(self, c_config, timestamp, NULL, SYS_MSG, true, MAGENTA, &line[2]);

    return true;
}

static bool load_line_name(ToxWindow *self, const Client_Config *c_config, const char *line, size_t length,
                           const char *timestamp)
{
    if (length <= 2) {
        return false;
    }

    char name[TOXIC_MAX_NAME_LENGTH + 1];
    const int end_name = extract_log_name(line, length, name, sizeof(name));

    if (end_name < 0 || end_name + 1 >= length) {
        return false;
    }

    line_info_load_history(self, c_config, timestamp, name, NAME_CHANGE, true, MAGENTA, &line[end_name + 1]);

    return true;
}

static bool load_line_moderation(ToxWindow *self, const Client_Config *c_config, const char *line, size_t length,
                                 const char *timestamp)
{
    if (length <= 2) {
        return false;
    }

    const int colour = strstr(line, "has been kicked by") != NULL ? RED : BLUE;
    line_info_load_history(self, c_config, timestamp, NULL, SYS_MSG, true, colour, &line[2]);

    return true;
}

static bool load_line_connection(ToxWindow *self, const Client_Config *c_config, const char *line, size_t length,
                                 const char *timestamp, Log_Hint hint)
{
    if (length <= 2) {
        return false;
    }

    char name[TOXIC_MAX_NAME_LENGTH + 1];
    const int end_name = extract_log_name(line, length, name, sizeof(name));

    if (end_name < 0 || end_name + 2 >= length) {
        return -1;
    }

    int colour;
    int type;

    if (hint == LOG_HINT_CONNECT) {
        colour = GREEN;
        type = CONNECTION;
    } else {
        colour = RED;
        type = DISCONNECTION;
    }

    line_info_load_history(self, c_config, timestamp, name, type, true, colour, &line[end_name + 2]);

    return true;
}

static bool load_line_message(ToxWindow *self, const Client_Config *c_config, const char *line, size_t length,
                              const char *timestamp, Log_Hint hint)
{
    char name[TOXIC_MAX_NAME_LENGTH + 1];

    const int end_name = extract_log_name(line, length, name, sizeof(name));

    if (end_name < 0 || end_name >= length + 2) {
        return false;
    }

    const char *message = &line[end_name + 2];


    switch (hint) {
        case LOG_HINT_NORMAL_I: {
            line_info_load_history(self, c_config, timestamp, name, IN_MSG, false, 0, message);
            break;
        }

        case LOG_HINT_NORMAL_O: {
            line_info_load_history(self, c_config, timestamp, name, OUT_MSG, false, 0, message);
            break;
        }

        case LOG_HINT_PRIVATE_I: {
            line_info_load_history(self, c_config, timestamp, name, IN_PRVT_MSG, false, MAGENTA, message);
            break;
        }

        case LOG_HINT_PRIVATE_O: {
            line_info_load_history(self, c_config, timestamp, name, OUT_PRVT_MSG, false, 0, message);
            break;
        }

        case LOG_HINT_ACTION: {
            line_info_load_history(self, c_config, timestamp, name, IN_ACTION, false, 0, message);
            break;
        }

        default: {
            fprintf(stderr, "Invalid hint in load_line_message(): %d\n", hint);
            return false;
        }
    }

    return true;
}

static void load_line(ToxWindow *self, const Client_Config *c_config, const char *line)
{
    const size_t line_length = strlen(line);

    if (line_length <= 4) {
        goto on_error;
    }

    const int hint = extract_log_hint(line, line_length);

    if (hint == -1) {
        goto on_error;
    }

    char timestamp[TIME_STR_SIZE];
    const int end_ts = extract_timestamp(line, line_length, timestamp, sizeof(timestamp));

    if (end_ts >= line_length) {
        goto on_error;
    }

    const char *line_start = &line[end_ts];
    const size_t length = line_length - end_ts;

    switch (hint) {
        case LOG_HINT_NORMAL_I:

        /* fallthrough */
        case LOG_HINT_NORMAL_O:

        /* fallthrough */
        case LOG_HINT_PRIVATE_I:

        /* fallthrough */
        case LOG_HINT_PRIVATE_O:

        /* fallthrough */
        case LOG_HINT_ACTION: {
            if (load_line_message(self, c_config, line_start, length, timestamp, hint)) {
                return;
            }

            break;
        }

        case LOG_HINT_NAME: {
            if (load_line_name(self, c_config, line_start, length, timestamp)) {
                return;
            }

            break;
        }

        case LOG_HINT_TOPIC: {
            if (load_line_topic(self, c_config, line_start, length, timestamp)) {
                return;
            }

            break;
        }

        case LOG_HINT_FOUNDER:

        /* fallthrough */
        case LOG_HINT_MOD_EVENT: {
            if (load_line_moderation(self, c_config, line_start, length, timestamp)) {
                return;
            }

            break;
        }

        case LOG_HINT_CONNECT:

        /* fallthrough */
        case LOG_HINT_DISCONNECT: {
            if (load_line_connection(self, c_config, line_start, length, timestamp, hint)) {
                return;
            }

            break;
        }

        default:
            break;
    }

on_error:
    line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "%s", line);
}

/* Loads chat log history and prints it to `self` window.
 *
 * Return 0 on success or if log file doesn't exist.
 * Return -1 on failure.
 */
int load_chat_history(struct chatlog *log, ToxWindow *self, const Client_Config *c_config)
{
    if (log == NULL) {
        return -1;
    }

    if (*log->path == 0) {
        return -1;
    }

    const off_t sz = file_size(log->path);

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

    buf[sz] = '\0';

    /* Number of history lines to load: must not be larger than MAX_LINE_INFO_QUEUE - 2 */
    int L = MIN(MAX_LINE_INFO_QUEUE - 2, c_config->history_size);

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
        load_line(self, c_config, line);
        line = strtok_r(NULL, "\n", &tmp);
    }

    line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, YELLOW, "---");

    free(buf);

    return 0;
}

/* Renames chatlog file `src` to `dest`.
 *
 * Return 0 on success or if no log exists.
 * Return -1 on failure.
 */
int rename_logfile(Windows *windows, const Client_Config *c_config, const Paths *paths, const char *src,
                   const char *dest, const char *selfkey, const char *otherkey, uint16_t window_id)
{
    ToxWindow *toxwin = get_window_pointer_by_id(windows, window_id);
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

    if (create_log_path(c_config, paths, oldpath, sizeof(oldpath), src, selfkey, otherkey) == -1) {
        goto on_error;
    }

    if (!file_exists(oldpath)) {  // still need to rename path
        init_logging_session(c_config, paths, dest, selfkey, otherkey, log, LOG_TYPE_CHAT);
        return 0;
    }

    const int new_path_len = create_log_path(c_config, paths, newpath, sizeof(newpath), dest, selfkey, otherkey);

    if (new_path_len == -1 || new_path_len >= MAX_STR_SIZE) {
        goto on_error;
    }

    if (file_exists(newpath)) {
        if (remove(oldpath) != 0) {
            fprintf(stderr, "Warning: remove() failed to remove log path `%s`\n", oldpath);
        }
    } else if (rename(oldpath, newpath) != 0) {
        goto on_error;
    }

    if (log != NULL) {
        memcpy(log->path, newpath, new_path_len);
        log->path[new_path_len] = '\0';

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

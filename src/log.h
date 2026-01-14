/*  log.h
 *
 *  Copyright (C) 2014-2026 Toxic All Rights Reserved.
 *
 *  This file is part of Toxic. Toxic is free software licensed
 *  under the GNU General Public License 3.0.
 */

#ifndef LOG_H
#define LOG_H

#include "paths.h"
#include "settings.h"

struct chatlog {
    FILE *file;
    time_t lastwrite;
    char path[MAX_STR_SIZE];
    bool log_on;    /* specific to current chat window */
    uint32_t bytes_written;
};

typedef enum Log_Type {
    LOG_TYPE_PROMPT,
    LOG_TYPE_CHAT,
} Log_Type;

typedef enum Log_Hint {
    LOG_HINT_NORMAL_I,   // normal inbound chat message
    LOG_HINT_NORMAL_O,   // normal outbound chat message
    LOG_HINT_ACTION,     // action message
    LOG_HINT_SYSTEM,     // system message
    LOG_HINT_CONNECT,    // friend online/peer join
    LOG_HINT_DISCONNECT, // friend offline/peer exit
    LOG_HINT_PRIVATE_I,  // private inbound group message
    LOG_HINT_PRIVATE_O,  // private outbound group message
    LOG_HINT_MOD_EVENT,  // group moderation event
    LOG_HINT_FOUNDER,    // group founder event
    LOG_HINT_NAME,       // name change
    LOG_HINT_TOPIC,      // group/conference topic/title change
} Log_Hint;

/* Initializes a log. This function must be called before any other logging operations.
 *
 * Return 0 on success.
 * Return -1 on failure.
 */
int log_init(struct chatlog *log, const Client_Config *c_config, const Paths *paths, const char *name,
             const char *selfkey, const char *otherkey, Log_Type type);

/* Writes a message to the log.
 *
 * `log` is the log being written to.
 * `msg` is the message being written.
 * `name` is the name of the initiator of the message. If NULL it will be ignored.
 * `log_hint` indicates the type of message.
 *
 * Return 0 on success (or if the log is disabled).
 * Return -1 on failure.
 */
int write_to_log(struct chatlog *log, const Client_Config *c_config, const char *msg, const char *name,
                 Log_Hint log_hint);

/* Enables logging for specified log.
 *
 * Calling this function on a log that's already enabled has no effect.
 *
 * Returns 0 on success.
 * Returns -1 on failure.
 */
int log_enable(struct chatlog *log);

/* Disables logging for specified log and closes file.
 *
 * Calling this function on a log that's already disabled has no effect.
 */
void log_disable(struct chatlog *log);

/* Loads chat log history and prints it to `self` window.
 *
 * Return 0 on success or if log file doesn't exist.
 * Return -1 on failure.
 */
int load_chat_history(struct chatlog *log, ToxWindow *self, const Client_Config *c_config);

/* Renames chatlog file `src` to `dest`.
 *
 * Return 0 on success or if no log exists.
 * Return -1 on failure.
 */
int rename_logfile(Windows *windows, const Client_Config *c_config, const Paths *paths, const char *src,
                   const char *dest, const char *selfkey, const char *otherkey, uint16_t window_id);

#endif /* LOG_H */

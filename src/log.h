/*  log.h
 *
 *
 *  Copyright (C) 2024 Toxic All Rights Reserved.
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

#ifndef LOG_H
#define LOG_H

#include "settings.h"

struct chatlog {
    FILE *file;
    time_t lastwrite;
    char path[MAX_STR_SIZE];
    bool log_on;    /* specific to current chat window */
};

typedef enum LOG_TYPE {
    LOG_TYPE_PROMPT,
    LOG_TYPE_CHAT,
} LOG_TYPE;

/* Initializes a log. This function must be called before any other logging operations.
 *
 * Return 0 on success.
 * Return -1 on failure.
 */
int log_init(struct chatlog *log, const Client_Config *c_config, const char *name, const char *selfkey,
             const char *otherkey,
             LOG_TYPE type);

/* formats/writes line to log file */
void write_to_log(struct chatlog *log, const Client_Config *c_config, const char *msg, const char *name,
                  bool event);

/* enables logging for specified log.
 *
 * Returns 0 on success.
 * Returns -1 on failure.
 */
int log_enable(struct chatlog *log);

/* disables logging for specified log and closes file */
void log_disable(struct chatlog *log);

/* Loads chat log history and prints it to `self` window.
 *
 * Return 0 on success or if log file doesn't exist.
 * Return -1 on failure.
 */
int load_chat_history(struct chatlog *log, ToxWindow *self, const Client_Config *c_config, const char *self_name);

/* Renames chatlog file `src` to `dest`.
 *
 * Return 0 on success or if no log exists.
 * Return -1 on failure.
 */
int rename_logfile(Windows *windows, const Client_Config *c_config, const char *src, const char *dest,
                   const char *selfkey, const char *otherkey, uint32_t window_id);

#endif /* LOG_H */

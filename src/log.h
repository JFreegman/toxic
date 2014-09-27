/*  log.h
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

#ifndef LOG_H
#define LOG_H

struct chatlog {
    FILE *file;
    uint64_t lastwrite;
    char path[MAX_STR_SIZE];
    bool log_on;    /* specific to current chat window */
};

enum {
    LOG_GROUP,
    LOG_PROMPT,
    LOG_CHAT,
} LOG_TYPE;

/* formats/writes line to log file */
void write_to_log(const char *msg, const char *name, struct chatlog *log, bool event);

/* enables logging for specified log and creates/fetches file if necessary */
void log_enable(char *name, const char *selfkey, const char *otherkey, struct chatlog *log, int logtype);

/* disables logging for specified log and closes file */
void log_disable(struct chatlog *log);

/* Loads previous history from chat log */
void load_chat_history(ToxWindow *self, struct chatlog *log);

#endif /* #define LOG_H */

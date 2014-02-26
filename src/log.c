/*  ctx->log.c
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

/* gets the log path by appending to the config dir the name and first 4 chars of key */
void init_logging_session(uint8_t *name, uint8_t *key, ChatContext *ctx)
{
    if (!ctx->log.log_on)
        return;

    char *user_config_dir = get_user_config_dir();
    int path_len = strlen(user_config_dir) + strlen(CONFIGDIR) + strlen(name)\
                                                + (KEY_IDENT_DIGITS * 2) + 5;

    if (path_len > MAX_STR_SIZE)
        return;

    uint8_t ident[KEY_IDENT_DIGITS*2+1];
    sprintf(&ident[0], "%02X", key[0] & 0xff);
    sprintf(&ident[2], "%02X", key[2] & 0xff);
    ident[KEY_IDENT_DIGITS*2+1] = '\0';

    snprintf(ctx->log.log_path, MAX_STR_SIZE, "%s%s%s-%s.log", 
             user_config_dir, CONFIGDIR, name, ident);

    FILE *logfile = fopen(ctx->log.log_path, "a");

    if (logfile == NULL) {
        ctx->log.log_on = false;
        return;
    }

    fprintf(logfile, "\n***NEW SESSION***\n\n");

    fclose(logfile);
    free(user_config_dir);
}

/* writes contents from a chatcontext's log buffer to respective log file and resets log pos.
   This is triggered when the log buffer is full, but may be forced. */
void write_to_log(ChatContext *ctx)
{
    if (!ctx->log.log_on)
        return;

    FILE *logfile = fopen(ctx->log.log_path, "a");

    if (logfile == NULL) {
        ctx->log.log_on = false;
        return;
    }

    int i;

    for (i = 0; i < ctx->log.pos; ++i)
        fprintf(logfile, "%s", ctx->log.log_buf[i]);

    ctx->log.pos = 0;
    fclose(logfile);
}

/* Adds msg to log_buf with timestamp and name. 
   If buf is full, triggers write_to_log (which sets buf pos to 0) */
void add_to_log_buf(uint8_t *msg, uint8_t *name, ChatContext *ctx)
{
    if (!ctx->log.log_on)
        return;

    struct tm *tminfo = get_time();
    snprintf(ctx->log.log_buf[ctx->log.pos], MAX_LOG_LINE_SIZE, "%04d/%02d/%02d [%02d:%02d:%02d] %s: %s\n", 
                                  tminfo->tm_year + 1900, tminfo->tm_mon + 1, tminfo->tm_mday, 
                                  tminfo->tm_hour, tminfo->tm_min, tminfo->tm_sec, name, msg);

    if (++(ctx->log.pos) >= MAX_LOG_BUF_LINES)
        write_to_log(ctx);
}

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

/* gets the log path by appending to the config dir the name and a pseudo-unique identity */
void init_logging_session(uint8_t *name, uint8_t *key, ChatContext *ctx);

/* Adds msg to log_buf with timestamp and name. 
   If buf is full, triggers write_to_log (which sets buf pos to 0) */
void add_line_log_buf(uint8_t *msg, uint8_t *name, ChatContext *ctx);

/* Adds line/event to log_buf with timestamp and name. If buf is full, triggers write_to_log.
   If event is true, formats line as an event, e.g. * name has gone offline */
void add_to_log_buf(uint8_t *msg, uint8_t *name, ChatContext *ctx, bool event);

/* writes contents from a chatcontext's log buffer to respective log file and resets log pos.
   This is triggered automatically when the log buffer is full, but may be forced. */
void write_to_log(ChatContext *ctx);

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

/* Creates/fetches log file by appending to the config dir the name and a pseudo-unique identity */
void init_logging_session(uint8_t *name, uint8_t *key, struct chatlog *log);

/* formats/writes line to log file */
void write_to_log(const uint8_t *msg, uint8_t *name, struct chatlog *log, bool event);

/* enables logging for specified log and creates/fetches file if necessary */
void log_enable(uint8_t *name, uint8_t *key, struct chatlog *log);

/* disables logging for specified log and closes file */
void log_disable(struct chatlog *log);

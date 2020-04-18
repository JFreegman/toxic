/*  avatars.h
 *
 *
 *  Copyright (C) 2015 Toxic All Rights Reserved.
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

#ifndef AVATARS_H
#define AVATARS_H

#include "file_transfers.h"

#define MAX_AVATAR_FILE_SIZE 65536

/* Sends avatar to friendnum.
 *
 * Returns 0 on success.
 * Returns -1 on failure.
 */
int avatar_send(Tox *m, uint32_t friendnum);

/* Sets avatar to path and sends it to all online contacts.
 *
 * Returns 0 on success.
 * Returns -1 on failure.
 */
int avatar_set(Tox *m, const char *path, size_t length);

/* Unsets avatar and sends to all online contacts.
 *
 * Returns 0 on success.
 * Returns -1 on failure.
 */
void avatar_unset(Tox *m);

void on_avatar_chunk_request(Tox *m, struct FileTransfer *ft, uint64_t position, size_t length);
void on_avatar_file_control(Tox *m, struct FileTransfer *ft, Tox_File_Control control);
void on_avatar_friend_connection_status(Tox *m, uint32_t friendnumber, Tox_Connection connection_status);

#endif /* AVATARS_H */

/*  avatars.h
 *
 *  Copyright (C) 2015-2024 Toxic All Rights Reserved.
 *
 *  This file is part of Toxic. Toxic is free software licensed
 *  under the GNU General Public License 3.0.
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
int avatar_send(Tox *tox, uint32_t friendnum);

/* Sets avatar to path and sends it to all online contacts.
 *
 * Returns 0 on success.
 * Returns -1 on failure.
 */
int avatar_set(Tox *tox, const char *path, size_t length);

/* Unsets avatar and sends to all online contacts.
 *
 * Returns 0 on success.
 * Returns -1 on failure.
 */
void avatar_unset(Tox *tox);

void on_avatar_chunk_request(Toxic *toxic, struct FileTransfer *ft, uint64_t position, size_t length);
void on_avatar_file_control(Toxic *toxic, struct FileTransfer *ft, Tox_File_Control control);
void on_avatar_friend_connection_status(Toxic *toxic, uint32_t friendnumber, Tox_Connection connection_status);

#endif /* AVATARS_H */

/*  avatars.c
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef NO_GETTEXT
#define gettext(A) (A)
#else
#include <libintl.h>
#endif

#include "misc_tools.h"
#include "file_transfers.h"
#include "friendlist.h"
#include "avatars.h"

extern FriendsList Friends;

static struct Avatar {
    char name[TOX_MAX_FILENAME_LENGTH + 1];
    size_t name_len;
    char path[PATH_MAX + 1];
    size_t path_len;
    off_t size;
} Avatar;


static void avatar_clear(void)
{
    memset(&Avatar, 0, sizeof(struct Avatar));
}

/* Sends avatar to friendnum.
 *
 * Returns 0 on success.
 * Returns -1 on failure.
 */
int avatar_send(Tox *m, uint32_t friendnum)
{
    TOX_ERR_FILE_SEND err;
    uint32_t filenum = tox_file_send(m, friendnum, TOX_FILE_KIND_AVATAR, (size_t) Avatar.size,
                                     NULL, (uint8_t *) Avatar.name, Avatar.name_len, &err);
    if (Avatar.size == 0)
        return 0;

    if (err != TOX_ERR_FILE_SEND_OK) {
        fprintf(stderr, gettext("tox_file_send failed for friendnumber %d (error %d)\n"), friendnum, err);
        return -1;
    }

    struct FileTransfer *ft = get_new_file_sender(friendnum);

    if (!ft)
        return -1;

    ft->file = fopen(Avatar.path, "r");

    if (ft->file == NULL)
        return -1;

    snprintf(ft->file_name, sizeof(ft->file_name), "%s", Avatar.name);
    ft->file_size = Avatar.size;
    ft->state = FILE_TRANSFER_PENDING;
    ft->filenum = filenum;
    ft->friendnum = friendnum;
    ft->direction = FILE_TRANSFER_SEND;
    ft->file_type = TOX_FILE_KIND_AVATAR;

    return 0;
}

/* Sends avatar to all friends */
static void avatar_send_all(Tox *m)
{
    size_t i;

    for (i = 0; i < Friends.max_idx; ++i) {
        if (Friends.list[i].connection_status != TOX_CONNECTION_NONE)
            avatar_send(m, Friends.list[i].num);
    }
}

/* Sets avatar to path and sends it to all online contacts.
 *
 * Returns 0 on success.
 * Returns -1 on failure.
 */
int avatar_set(Tox *m, const char *path, size_t path_len)
{
    if (path_len == 0 || path_len >= sizeof(Avatar.path))
        return -1;

    FILE *fp = fopen(path, "rb");

    if (fp == NULL)
        return -1;

    char PNG_signature[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};

    if (check_file_signature(PNG_signature, sizeof(PNG_signature), fp) != 0) {
        fclose(fp);
        return -1;
    }

    fclose(fp);

    off_t size = file_size(path);

    if (size == 0 || size > MAX_AVATAR_FILE_SIZE)
        return -1;

    get_file_name(Avatar.name, sizeof(Avatar.name), path);
    Avatar.name_len = strlen(Avatar.name);
    snprintf(Avatar.path, sizeof(Avatar.path), "%s", path);
    Avatar.path_len = path_len;
    Avatar.size = size;

    avatar_send_all(m);

    return 0;
}

/* Unsets avatar and sends to all online contacts.
 *
 * Returns 0 on success.
 * Returns -1 on failure.
 */
void avatar_unset(Tox *m)
{
    avatar_clear();
    avatar_send_all(m);
}

void on_avatar_file_control(Tox *m, struct FileTransfer *ft, TOX_FILE_CONTROL control)
{
    switch (control) {
        case TOX_FILE_CONTROL_RESUME:
            if (ft->state == FILE_TRANSFER_PENDING) {
                ft->state = FILE_TRANSFER_STARTED;
            } else if (ft->state == FILE_TRANSFER_PAUSED) {
                ft->state = FILE_TRANSFER_STARTED;
            }
            break;

        case TOX_FILE_CONTROL_PAUSE:
            ft->state = FILE_TRANSFER_PAUSED;
            break;

        case TOX_FILE_CONTROL_CANCEL:
            close_file_transfer(NULL, m, ft, -1, NULL, silent);
            break;
    }
}

void on_avatar_chunk_request(Tox *m, struct FileTransfer *ft, uint64_t position, size_t length)
{
    if (ft->state != FILE_TRANSFER_STARTED)
        return;

    if (length == 0) {
        close_file_transfer(NULL, m, ft, -1, NULL, silent);
        return;
    }

    if (ft->file == NULL) {
        close_file_transfer(NULL, m, ft, TOX_FILE_CONTROL_CANCEL, NULL, silent);
        return;
    }

    if (ft->position != position) {
        if (fseek(ft->file, position, SEEK_SET) == -1) {
            close_file_transfer(NULL, m, ft, TOX_FILE_CONTROL_CANCEL, NULL, silent);
            return;
        }

        ft->position = position;
    }

    uint8_t send_data[length];
    size_t send_length = fread(send_data, 1, sizeof(send_data), ft->file);

    if (send_length != length) {
        close_file_transfer(NULL, m, ft, TOX_FILE_CONTROL_CANCEL, NULL, silent);
        return;
    }

    TOX_ERR_FILE_SEND_CHUNK err;
    tox_file_send_chunk(m, ft->friendnum, ft->filenum, position, send_data, send_length, &err);

    if (err != TOX_ERR_FILE_SEND_CHUNK_OK)
        fprintf(stderr, gettext("tox_file_send_chunk failed in avatar callback (error %d)\n"), err);

    ft->position += send_length;
}

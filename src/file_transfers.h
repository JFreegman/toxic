/*  file_transfers.h
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

#ifndef FILE_TRANSFERS_H
#define FILE_TRANSFERS_H

#include <limits.h>

#include "notify.h"
#include "toxic.h"
#include "windows.h"

#define KiB 1024
#define MiB 1048576       /* 1024^2 */
#define GiB 1073741824    /* 1024^3 */

#define MAX_FILES 32

typedef enum FILE_TRANSFER_STATE {
    FILE_TRANSFER_INACTIVE,
    FILE_TRANSFER_PAUSED,
    FILE_TRANSFER_PENDING,
    FILE_TRANSFER_STARTED,
} FILE_TRANSFER_STATE;

typedef enum FILE_TRANSFER_DIRECTION {
    FILE_TRANSFER_SEND,
    FILE_TRANSFER_RECV
} FILE_TRANSFER_DIRECTION;

struct FileTransfer {
    ToxWindow *window;
    FILE *file;
    FILE_TRANSFER_STATE state;
    uint8_t file_type;
    char file_name[TOX_MAX_FILENAME_LENGTH + 1];
    char file_path[PATH_MAX + 1];    /* Not used by senders */
    double   bps;
    uint32_t filenumber;
    uint32_t friendnumber;
    size_t   index;
    uint64_t file_size;
    uint64_t position;
    time_t   last_line_progress;   /* The last time we updated the progress bar */
    uint32_t line_id;
    uint8_t  file_id[TOX_FILE_ID_LENGTH];
};

/* creates initial progress line that will be updated during file transfer.
   progline must be at lesat MAX_STR_SIZE bytes */
void init_progress_bar(char *progline);

/* prints a progress bar for file transfers */
void print_progress_bar(ToxWindow *self, double pct_done, double bps, uint32_t line_id);

/* refreshes active file transfer status bars. */
void refresh_file_transfer_progress(ToxWindow *self, uint32_t friendnumber);

/* Returns a pointer to friendnumber's FileTransfer struct associated with filenumber.
 * Returns NULL if filenumber is invalid.
 */
struct FileTransfer *get_file_transfer_struct(uint32_t friendnumber, uint32_t filenumber);


/* Returns a pointer to the FileTransfer struct associated with index with the direction specified.
 * Returns NULL on failure.
 */
struct FileTransfer *get_file_transfer_struct_index(uint32_t friendnumber, uint32_t index,
        FILE_TRANSFER_DIRECTION direction);

/* Initializes an unused file transfer and returns its pointer.
 * Returns NULL on failure.
 */
struct FileTransfer *new_file_transfer(ToxWindow *window, uint32_t friendnumber, uint32_t filenumber,
                                       FILE_TRANSFER_DIRECTION direction, uint8_t type);

/* Closes file transfer ft.
 *
 * Set CTRL to -1 if we don't want to send a control signal.
 * Set message or self to NULL if we don't want to display a message.
 */
void close_file_transfer(ToxWindow *self, Tox *m, struct FileTransfer *ft, int CTRL, const char *message,
                         Notification sound_type);

/* Kills active outgoing avatar file transfers for friendnumber */
void kill_avatar_file_transfers_friend(Tox *m, uint32_t friendnumber);

/* Kills all active file transfers for friendnumber */
void kill_all_file_transfers_friend(Tox *m, uint32_t friendnumber);

void kill_all_file_transfers(Tox *m);

#endif /* FILE_TRANSFERS_H */

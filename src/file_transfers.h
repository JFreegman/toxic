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

#include "toxic.h"
#include "windows.h"
#include "notify.h"

#define KiB 1024
#define MiB 1048576       /* 1024 ^ 2 */
#define GiB 1073741824    /* 1024 ^ 3 */

#define MAX_FILES 32
#define TIMEOUT_FILESENDER 120

typedef enum FILE_TRANSFER_STATE {
    FILE_TRANSFER_INACTIVE,
    FILE_TRANSFER_PENDING,
    FILE_TRANSFER_STARTED,
    FILE_TRANSFER_PAUSED
} FILE_TRANSFER_STATE;

typedef enum FILE_TRANSFER_DIRECTION {
    FILE_TRANSFER_SEND,
    FILE_TRANSFER_RECV
} FILE_TRANSFER_DIRECTION;

struct FileTransfer {
    FILE *file;
    FILE_TRANSFER_STATE state;
    FILE_TRANSFER_DIRECTION direction;
    char file_name[TOX_MAX_FILENAME_LENGTH + 1];
    char file_path[PATH_MAX + 1];    /* Not used by senders */
    double bps;
    uint32_t filenum;
    uint32_t friendnum;
    size_t   index;
    uint64_t file_size;
    uint64_t last_progress;
    uint64_t position;
    uint32_t line_id;
};

/* creates initial progress line that will be updated during file transfer.
   progline must be at lesat MAX_STR_SIZE bytes */
void prep_prog_line(char *progline);

/* prints a progress bar for file transfers */
void print_progress_bar(ToxWindow *self, double pct_done, double bps, uint32_t line_id);

/* refreshes active file transfer status bars for friendnum */
void refresh_file_transfer_progress(ToxWindow *self, Tox *m, uint32_t friendnum);

/* Returns a pointer to friendnum's FileTransfer struct associated with filenum.
 * Returns NULL if filenum is invalid.
 */
struct FileTransfer *get_file_transfer_struct(uint32_t friendnum, uint32_t filenum);


/* Returns a pointer to the FileTransfer struct associated with index with the direction specified.
 * Returns NULL on failure.
 */
struct FileTransfer *get_file_transfer_struct_index(uint32_t friendnum, uint32_t index,
                                                    FILE_TRANSFER_DIRECTION direction);

/* Returns a pointer to an unused file sender.
 * Returns NULL if all file senders are in use.
 */
struct FileTransfer *get_new_file_sender(uint32_t friendnum);

/* Returns a pointer to an unused file receiver.
 * Returns NULL if all file receivers are in use.
 */
struct FileTransfer *get_new_file_receiver(uint32_t friendnum);

/* Closes file transfer ft.
 *
 * Set CTRL to -1 if we don't want to send a control signal.
 * Set message or self to NULL if we don't want to display a message.
 */
void close_file_transfer(ToxWindow *self, Tox *m, struct FileTransfer *ft, int CTRL, const char *message,
                         Notification sound_type);

/* Kills all active file transfers for friendnum */
void kill_all_file_transfers_friend(Tox *m, uint32_t friendnum);

#endif  /* #define FILE_TRANSFERS_H */

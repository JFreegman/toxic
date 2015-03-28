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

#define FILE_PIECE_SIZE 2048
#define MAX_FILES 32
#define TIMEOUT_FILESENDER 120

struct FileSender {
    FILE *file;
    char file_name[TOX_MAX_FILENAME_LENGTH];
    bool active;
    bool noconnection;  /* set when the connection has been interrupted */
    bool paused;        /* set when transfer has been explicitly paused */
    bool started;       /* set after TOX_FILECONTROL_ACCEPT received */
    uint64_t timestamp;        /* marks the last time data was successfully transfered */
    double bps;
    uint64_t file_size;
    uint64_t last_progress;    /* marks the last time the progress bar was refreshed */
    uint64_t position;
    uint32_t line_id;
};

struct FileReceiver {
    FILE *file;
    char file_path[PATH_MAX + 1];
    bool pending;
    bool active;
    double bps;
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

/* refreshes active file receiver status bars for friendnum */
void refresh_file_transfer_progress(ToxWindow *self, Tox *m, uint32_t friendnum);

/* Returns filenum's file transfer array index */
uint32_t get_file_transfer_index(uint32_t filenum);

/* Returns the filenumber of a file receiver's index */
uint32_t get_file_receiver_filenum(uint32_t idx);

/* Return true if filenum is associated with a file receiver, false if file sender */
bool filenum_is_sending(uint32_t filenum);

/* Closes file transfer with filenum.
 * Set CTRL to -1 if we don't want to send a control signal.
 * Set message or self to NULL if we don't want to display a message.
 */
void close_file_transfer(ToxWindow *self, Tox *m, uint32_t filenum, uint32_t friendnum, int CTRL,
                         const char *message, Notification sound_type);

/* Kills all active file transfers for friendnum */
void kill_all_file_transfers_friend(Tox *m, uint32_t friendnum);

#endif  /* #define FILE_TRANSFERS_H */

/*  file_senders.h
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

#ifndef FILESENDERS_H
#define FILESENDERS_H

#include "toxic.h"
#include "windows.h"

#define KiB 1024
#define MiB 1048576       /* 1024 ^ 2 */
#define GiB 1073741824    /* 1024 ^ 3 */

#define FILE_PIECE_SIZE 2048    /* must be >= (MAX_CRYPTO_DATA_SIZE - 2) in toxcore/net_crypto.h */
#define MAX_FILES 32
#define TIMEOUT_FILESENDER 120

typedef struct {
    FILE *file;
    ToxWindow *toxwin;
    int32_t friendnum;
    bool active;
    bool noconnection;  /* set when the connection has been interrupted */
    bool paused;        /* set when transfer has been explicitly paused */
    bool finished;      /* set after entire file has been sent but no TOX_FILECONTROL_FINISHED receieved */
    bool started;       /* set after TOX_FILECONTROL_ACCEPT received */
    int filenum;
    char nextpiece[FILE_PIECE_SIZE];
    uint16_t piecelen;
    char filename[MAX_STR_SIZE];
    uint64_t timestamp;    /* marks the last time data was successfully transfered */
    uint64_t last_progress;    /* marks the last time the progress bar was refreshed */
    double bps;
    uint64_t size;
    uint32_t line_id;
    uint8_t queue_pos;
} FileSender;

/* creates initial progress line that will be updated during file transfer.
   Assumes progline is of size MAX_STR_SIZE */
void prep_prog_line(char *progline);

/* prints a progress bar for file transfers. 
   if friendnum is -1 we're sending the file, otherwise we're receiving.  */
void print_progress_bar(ToxWindow *self, int idx, int friendnum, double pct_remain);

/* set CTRL to -1 if we don't want to send a control signal.
   set msg to NULL if we don't want to display a message */
void close_file_sender(ToxWindow *self, Tox *m, int i, const char *msg, int CTRL, int filenum, int32_t friendnum);

/* called whenever a file sender is opened or closed */
void reset_file_sender_queue(void);

void close_all_file_senders(Tox *m);
void do_file_senders(Tox *m);

#endif  /* #define FILESENDERS_H */

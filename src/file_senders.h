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

#ifndef _filesenders_h
#define _filesenders_h

#include "toxic.h"
#include "windows.h"

#define FILE_PIECE_SIZE 2048    /* must be >= (MAX_CRYPTO_DATA_SIZE - 2) in toxcore/net_crypto.h */
#define MAX_FILES 256
#define TIMEOUT_FILESENDER 120

typedef struct {
    FILE *file;
    ToxWindow *toxwin;
    int32_t friendnum;
    bool active;
    int filenum;
    uint8_t nextpiece[FILE_PIECE_SIZE];
    uint16_t piecelen;
    uint8_t pathname[MAX_STR_SIZE];
    uint64_t timestamp;
    uint64_t size;
    uint32_t line_id;
} FileSender;

void close_all_file_senders(Tox *m);
void do_file_senders(Tox *m);

#endif  /* #define _filesenders_h */

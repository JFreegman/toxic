/*  file_transfers.c
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

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "toxic.h"
#include "windows.h"
#include "friendlist.h"
#include "file_transfers.h"
#include "line_info.h"
#include "misc_tools.h"
#include "notify.h"

extern FriendsList Friends;

/* number of "#"'s in file transfer progress bar. Keep well below MAX_STR_SIZE */
#define NUM_PROG_MARKS 50

/* creates initial progress line that will be updated during file transfer.
   Assumes progline has room for at least MAX_STR_SIZE bytes */
void init_progress_bar(char *progline)
{
    strcpy(progline, "0% [");
    int i;

    for (i = 0; i < NUM_PROG_MARKS; ++i) {
        strcat(progline, "-");
    }

    strcat(progline, "] 0.0 B/s");
}

/* prints a progress bar for file transfers. */
void print_progress_bar(ToxWindow *self, double bps, double pct_done, uint32_t line_id)
{
    if (bps < 0 || pct_done < 0 || pct_done > 100) {
        return;
    }

    char pct_str[24];
    snprintf(pct_str, sizeof(pct_str), "%.1f%%", pct_done);

    char bps_str[24];
    bytes_convert_str(bps_str, sizeof(bps_str), bps);

    char prog_line[NUM_PROG_MARKS + 1] = {0};
    int n = pct_done / (100 / NUM_PROG_MARKS);
    int i, j;

    for (i = 0; i < n; ++i) {
        strcat(prog_line, "=");
    }

    if (pct_done < 100) {
        strcpy(prog_line + n, ">");
    }

    for (j = i; j < NUM_PROG_MARKS - 1; ++j) {
        strcat(prog_line, "-");
    }

    size_t line_buf_size = strlen(pct_str) + NUM_PROG_MARKS + strlen(bps_str) + 7;
    char *full_line = malloc(line_buf_size);

    if (full_line == NULL) {
        return;
    }

    snprintf(full_line, line_buf_size, "%s [%s] %s/s", pct_str, prog_line, bps_str);

    line_info_set(self, line_id, full_line);

    free(full_line);
}

static void refresh_progress_helper(ToxWindow *self, struct FileTransfer *ft)
{
    if (ft->state == FILE_TRANSFER_INACTIVE) {
        return;
    }

    /* Timeout must be set to 1 second to show correct bytes per second */
    if (!timed_out(ft->last_line_progress, 1)) {
        return;
    }

    double remain = ft->file_size - ft->position;
    double pct_done = remain > 0 ? (1 - (remain / ft->file_size)) * 100 : 100;
    print_progress_bar(self, ft->bps, pct_done, ft->line_id);

    ft->bps = 0;
    ft->last_line_progress = get_unix_time();
}

/* refreshes active file transfer status bars. */
void refresh_file_transfer_progress(ToxWindow *self, uint32_t friendnum)
{
    size_t i;

    for (i = 0; i < MAX_FILES; ++i) {
        refresh_progress_helper(self, &Friends.list[friendnum].file_receiver[i]);
        refresh_progress_helper(self, &Friends.list[friendnum].file_sender[i]);
    }
}

/* Returns a pointer to friendnum's FileTransfer struct associated with filenum.
 * Returns NULL if filenum is invalid.
 */
struct FileTransfer *get_file_transfer_struct(uint32_t friendnum, uint32_t filenum)
{
    size_t i;

    for (i = 0; i < MAX_FILES; ++i) {
        struct FileTransfer *ft_send = &Friends.list[friendnum].file_sender[i];

        if (ft_send->state != FILE_TRANSFER_INACTIVE && ft_send->filenum == filenum) {
            return ft_send;
        }

        struct FileTransfer *ft_recv = &Friends.list[friendnum].file_receiver[i];

        if (ft_recv->state != FILE_TRANSFER_INACTIVE && ft_recv->filenum == filenum) {
            return ft_recv;
        }
    }

    return NULL;
}

/* Returns a pointer to the FileTransfer struct associated with index with the direction specified.
 * Returns NULL on failure.
 */
struct FileTransfer *get_file_transfer_struct_index(uint32_t friendnum, uint32_t index,
        FILE_TRANSFER_DIRECTION direction)
{
    if (direction != FILE_TRANSFER_RECV && direction != FILE_TRANSFER_SEND) {
        return NULL;
    }

    size_t i;

    for (i = 0; i < MAX_FILES; ++i) {
        struct FileTransfer *ft = direction == FILE_TRANSFER_SEND ?
                                      &Friends.list[friendnum].file_sender[i] :
                                      &Friends.list[friendnum].file_receiver[i];

        if (ft->state != FILE_TRANSFER_INACTIVE && ft->index == index) {
            return ft;
        }
    }

    return NULL;
}

/* Returns a pointer to an unused file sender.
 * Returns NULL if all file senders are in use.
 */
static struct FileTransfer *new_file_sender(ToxWindow *window, uint32_t friendnum, uint32_t filenum, uint8_t type)
{
    size_t i;

    for (i = 0; i < MAX_FILES; ++i) {
        struct FileTransfer *ft = &Friends.list[friendnum].file_sender[i];

        if (ft->state == FILE_TRANSFER_INACTIVE) {
            memset(ft, 0, sizeof(struct FileTransfer));
            ft->window = window;
            ft->index = i;
            ft->friendnum = friendnum;
            ft->filenum = filenum;
            ft->file_type = type;
            ft->last_keep_alive = get_unix_time();
            ft->state = FILE_TRANSFER_PENDING;
            ft->direction = FILE_TRANSFER_SEND;
            return ft;
        }
    }

    return NULL;
}

/* Returns a pointer to an unused file receiver.
 * Returns NULL if all file receivers are in use.
 */
static struct FileTransfer *new_file_receiver(ToxWindow *window, uint32_t friendnum, uint32_t filenum, uint8_t type)
{
    size_t i;

    for (i = 0; i < MAX_FILES; ++i) {
        struct FileTransfer *ft = &Friends.list[friendnum].file_receiver[i];

        if (ft->state == FILE_TRANSFER_INACTIVE) {
            memset(ft, 0, sizeof(struct FileTransfer));
            ft->window = window;
            ft->index = i;
            ft->friendnum = friendnum;
            ft->filenum = filenum;
            ft->file_type = type;
            ft->last_keep_alive = get_unix_time();
            ft->state = FILE_TRANSFER_PENDING;
            ft->direction = FILE_TRANSFER_RECV;
            return ft;
        }
    }

    return NULL;
}

/* Initializes an unused file transfer and returns its pointer.
 * Returns NULL on failure.
 */
struct FileTransfer *new_file_transfer(ToxWindow *window, uint32_t friendnum, uint32_t filenum,
                                       FILE_TRANSFER_DIRECTION direction, uint8_t type)
{
    if (direction == FILE_TRANSFER_RECV) {
        return new_file_receiver(window, friendnum, filenum, type);
    }

    if (direction == FILE_TRANSFER_SEND) {
        return new_file_sender(window, friendnum, filenum, type);
    }

    return NULL;
}


/* Closes file transfer ft.
 *
 * Set CTRL to -1 if we don't want to send a control signal.
 * Set message or self to NULL if we don't want to display a message.
 */
void close_file_transfer(ToxWindow *self, Tox *m, struct FileTransfer *ft, int CTRL, const char *message,
                         Notification sound_type)
{
    if (!ft) {
        return;
    }

    if (ft->state == FILE_TRANSFER_INACTIVE) {
        return;
    }

    if (ft->file) {
        fclose(ft->file);
    }

    if (CTRL >= 0) {
        tox_file_control(m, ft->friendnum, ft->filenum, (Tox_File_Control) CTRL, NULL);
    }

    if (message && self) {
        if (self->active_box != -1 && sound_type != silent) {
            box_notify2(self, sound_type, NT_NOFOCUS | NT_WNDALERT_2, self->active_box, "%s", message);
        } else {
            box_notify(self, sound_type, NT_NOFOCUS | NT_WNDALERT_2, &self->active_box, self->name, "%s", message);
        }

        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "%s", message);
    }

    memset(ft, 0, sizeof(struct FileTransfer));
}

/* Kills all active file transfers for friendnum */
void kill_all_file_transfers_friend(Tox *m, uint32_t friendnum)
{
    size_t i;

    for (i = 0; i < MAX_FILES; ++i) {
        close_file_transfer(NULL, m, &Friends.list[friendnum].file_sender[i], TOX_FILE_CONTROL_CANCEL, NULL, silent);
        close_file_transfer(NULL, m, &Friends.list[friendnum].file_receiver[i], TOX_FILE_CONTROL_CANCEL, NULL, silent);
    }
}

void kill_all_file_transfers(Tox *m)
{
    size_t i;

    for (i = 0; i < Friends.max_idx; ++i) {
        kill_all_file_transfers_friend(m, Friends.list[i].num);
    }
}

/*  file_transfers.c
 *
 *
 *  Copyright (C) 2024 Toxic All Rights Reserved.
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
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "execute.h"
#include "file_transfers.h"
#include "friendlist.h"
#include "line_info.h"
#include "misc_tools.h"
#include "notify.h"
#include "toxic.h"
#include "windows.h"

extern FriendsList Friends;

/* number of "#"'s in file transfer progress bar. Keep well below MAX_STR_SIZE */
#define NUM_PROG_MARKS 50
#define STR_BUF_SIZE 30

/* creates initial progress line that will be updated during file transfer.
   Assumes progline has room for at least MAX_STR_SIZE bytes */
void init_progress_bar(char *progline)
{
    strcpy(progline, "0% [");

    for (size_t i = 0; i < NUM_PROG_MARKS; ++i) {
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

    char pct_str[STR_BUF_SIZE];
    snprintf(pct_str, sizeof(pct_str), "%.1f%%", pct_done);

    char bps_str[STR_BUF_SIZE];
    bytes_convert_str(bps_str, sizeof(bps_str), bps);

    char prog_line[NUM_PROG_MARKS + 1];
    prog_line[0] = 0;

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

static void refresh_progress_helper(ToxWindow *self, FileTransfer *ft)
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

/* refreshes active file transfer status bars.
 *
 * Return true if there is at least one active file transfer in either direction.
 */
bool refresh_file_transfer_progress(ToxWindow *self, uint32_t friendnumber)
{
    bool active = false;

    for (size_t i = 0; i < MAX_FILES; ++i) {
        FileTransfer *ft_r = &Friends.list[friendnumber].file_receiver[i];
        FileTransfer *ft_s = &Friends.list[friendnumber].file_sender[i];

        refresh_progress_helper(self, ft_r);
        refresh_progress_helper(self, ft_s);

        if (ft_r->state !=  FILE_TRANSFER_INACTIVE || ft_s->state != FILE_TRANSFER_INACTIVE) {
            active = true;
        }
    }

    return active;
}

static void clear_file_transfer(FileTransfer *ft)
{
    *ft = (FileTransfer) {
        0
    };
}

/* Returns a pointer to friendnumber's FileTransfer struct associated with filenumber.
 * Returns NULL if filenumber is invalid.
 */
FileTransfer *get_file_transfer_struct(uint32_t friendnumber, uint32_t filenumber)
{
    for (size_t i = 0; i < MAX_FILES; ++i) {
        FileTransfer *ft_send = &Friends.list[friendnumber].file_sender[i];

        if (ft_send->state != FILE_TRANSFER_INACTIVE && ft_send->filenumber == filenumber) {
            return ft_send;
        }

        FileTransfer *ft_recv = &Friends.list[friendnumber].file_receiver[i];

        if (ft_recv->state != FILE_TRANSFER_INACTIVE && ft_recv->filenumber == filenumber) {
            return ft_recv;
        }
    }

    return NULL;
}

/* Returns a pointer to the FileTransfer struct associated with index with the direction specified.
 * Returns NULL on failure.
 */
FileTransfer *get_file_transfer_struct_index(uint32_t friendnumber, uint32_t index,
        FILE_TRANSFER_DIRECTION direction)
{
    if (direction != FILE_TRANSFER_RECV && direction != FILE_TRANSFER_SEND) {
        return NULL;
    }

    for (size_t i = 0; i < MAX_FILES; ++i) {
        FileTransfer *ft = direction == FILE_TRANSFER_SEND ?
                           &Friends.list[friendnumber].file_sender[i] :
                           &Friends.list[friendnumber].file_receiver[i];

        if (ft->state != FILE_TRANSFER_INACTIVE && ft->index == index) {
            return ft;
        }
    }

    return NULL;
}

/* Returns a pointer to an unused file sender.
 * Returns NULL if all file senders are in use.
 */
static FileTransfer *new_file_sender(ToxWindow *window, uint32_t friendnumber, uint32_t filenumber, uint8_t type)
{
    for (size_t i = 0; i < MAX_FILES; ++i) {
        FileTransfer *ft = &Friends.list[friendnumber].file_sender[i];

        if (ft->state == FILE_TRANSFER_INACTIVE) {
            clear_file_transfer(ft);
            ft->window = window;
            ft->index = i;
            ft->friendnumber = friendnumber;
            ft->filenumber = filenumber;
            ft->file_type = type;
            ft->state = FILE_TRANSFER_PENDING;
            return ft;
        }
    }

    return NULL;
}

/* Returns a pointer to an unused file receiver.
 * Returns NULL if all file receivers are in use.
 */
static FileTransfer *new_file_receiver(ToxWindow *window, uint32_t friendnumber, uint32_t filenumber,
                                       uint8_t type)
{
    for (size_t i = 0; i < MAX_FILES; ++i) {
        FileTransfer *ft = &Friends.list[friendnumber].file_receiver[i];

        if (ft->state == FILE_TRANSFER_INACTIVE) {
            clear_file_transfer(ft);
            ft->window = window;
            ft->index = i;
            ft->friendnumber = friendnumber;
            ft->filenumber = filenumber;
            ft->file_type = type;
            ft->state = FILE_TRANSFER_PENDING;
            return ft;
        }
    }

    return NULL;
}

/* Initializes an unused file transfer and returns its pointer.
 * Returns NULL on failure.
 */
FileTransfer *new_file_transfer(ToxWindow *window, uint32_t friendnumber, uint32_t filenumber,
                                FILE_TRANSFER_DIRECTION direction, uint8_t type)
{
    if (direction == FILE_TRANSFER_RECV) {
        return new_file_receiver(window, friendnumber, filenumber, type);
    }

    if (direction == FILE_TRANSFER_SEND) {
        return new_file_sender(window, friendnumber, filenumber, type);
    }

    return NULL;
}

int file_send_queue_add(uint32_t friendnumber, const char *file_path, size_t length)
{
    if (length == 0 || file_path == NULL) {
        return -1;
    }

    if (length > TOX_MAX_FILENAME_LENGTH) {
        return -2;
    }

    for (size_t i = 0; i < MAX_FILES; ++i) {
        PendingFileTransfer *pending_slot = &Friends.list[friendnumber].file_send_queue[i];

        if (pending_slot->pending) {
            continue;
        }

        pending_slot->pending = true;

        memcpy(pending_slot->file_path, file_path, length);
        pending_slot->file_path[length] = 0;
        pending_slot->length = length;

        return i;
    }

    return -3;
}

#define FILE_TRANSFER_SEND_CMD "/sendfile "
#define FILE_TRANSFER_SEND_LEN (sizeof(FILE_TRANSFER_SEND_CMD) - 1)

void file_send_queue_check(ToxWindow *self, Tox *tox, uint32_t friendnumber)
{
    for (size_t i = 0; i < MAX_FILES; ++i) {
        PendingFileTransfer *pending_slot = &Friends.list[friendnumber].file_send_queue[i];

        if (!pending_slot->pending) {
            continue;
        }

        char command[TOX_MAX_FILENAME_LENGTH + FILE_TRANSFER_SEND_LEN + 1];
        snprintf(command, sizeof(command), "%s%s", FILE_TRANSFER_SEND_CMD, pending_slot->file_path);

        execute(self->window, self, tox, command, CHAT_COMMAND_MODE);

        *pending_slot = (PendingFileTransfer) {
            0,
        };
    }
}

int file_send_queue_remove(uint32_t friendnumber, size_t index)
{
    if (index >= MAX_FILES) {
        return -1;
    }

    PendingFileTransfer *pending_slot = &Friends.list[friendnumber].file_send_queue[index];

    if (!pending_slot->pending) {
        return -1;
    }

    *pending_slot = (PendingFileTransfer) {
        0,
    };

    return 0;
}

/* Closes file transfer ft.
 *
 * Set CTRL to -1 if we don't want to send a control signal.
 * Set message or self to NULL if we don't want to display a message.
 */
void close_file_transfer(ToxWindow *self, Tox *tox, FileTransfer *ft, int CTRL, const char *message,
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
        Tox_Err_File_Control err;

        if (!tox_file_control(tox, ft->friendnumber, ft->filenumber, (Tox_File_Control) CTRL, &err)) {
            fprintf(stderr, "Failed to cancel file transfer: %d\n", err);
        }
    }

    if (message && self) {
        if (self->active_box != -1 && sound_type != silent) {
            box_notify2(self, sound_type, NT_NOFOCUS | NT_WNDALERT_2, self->active_box, "%s", message);
        } else {
            box_notify(self, sound_type, NT_NOFOCUS | NT_WNDALERT_2, &self->active_box, self->name, "%s", message);
        }

        line_info_add(self, false, NULL, NULL, SYS_MSG, 0, 0, "%s", message);
    }

    clear_file_transfer(ft);
}

/* Kills active outgoing avatar file transfers for friendnumber */
void kill_avatar_file_transfers_friend(Tox *tox, uint32_t friendnumber)
{
    for (size_t i = 0; i < MAX_FILES; ++i) {
        FileTransfer *ft = &Friends.list[friendnumber].file_sender[i];

        if (ft->file_type == TOX_FILE_KIND_AVATAR) {
            close_file_transfer(NULL, tox, ft, TOX_FILE_CONTROL_CANCEL, NULL, silent);
        }
    }
}

/* Kills all active file transfers for friendnumber */
void kill_all_file_transfers_friend(Tox *tox, uint32_t friendnumber)
{
    for (size_t i = 0; i < MAX_FILES; ++i) {
        close_file_transfer(NULL, tox, &Friends.list[friendnumber].file_sender[i], TOX_FILE_CONTROL_CANCEL, NULL, silent);
        close_file_transfer(NULL, tox, &Friends.list[friendnumber].file_receiver[i], TOX_FILE_CONTROL_CANCEL, NULL, silent);
        file_send_queue_remove(friendnumber, i);
    }
}

void kill_all_file_transfers(Tox *tox)
{
    for (size_t i = 0; i < Friends.max_idx; ++i) {
        kill_all_file_transfers_friend(tox, Friends.list[i].num);
    }
}

bool file_transfer_recv_path_exists(const char *path)
{
    for (size_t friendnumber = 0; friendnumber < Friends.max_idx; ++friendnumber) {
        if (!Friends.list[friendnumber].active) {
            continue;
        }

        for (size_t i = 0; i < MAX_FILES; ++i) {
            FileTransfer *ft = &Friends.list[friendnumber].file_receiver[i];

            if (ft->state == FILE_TRANSFER_INACTIVE) {
                continue;
            }

            if (strcmp(path, ft->file_path) == 0) {
                return true;
            }
        }
    }

    return false;
}

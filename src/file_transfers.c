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

#define NUM_PROG_MARKS 50    /* number of "#"'s in file transfer progress bar. Keep well below MAX_STR_SIZE */

/* creates initial progress line that will be updated during file transfer.
   Assumes progline is of size MAX_STR_SIZE */
void prep_prog_line(char *progline)
{
    strcpy(progline, "0.0 B/s [");
    int i;

    for (i = 0; i < NUM_PROG_MARKS; ++i)
        strcat(progline, "-");

    strcat(progline, "] 0%");
}

/* prints a progress bar for file transfers.
   if friendnum is -1 we're sending the file, otherwise we're receiving.  */
void print_progress_bar(ToxWindow *self, double bps, double pct_done, uint32_t line_id)
{
    char msg[MAX_STR_SIZE];
    bytes_convert_str(msg, sizeof(msg), bps);
    strcat(msg, "/s [");

    int n = pct_done / (100 / NUM_PROG_MARKS);
    int i, j;

    for (i = 0; i < n; ++i)
        strcat(msg, "#");

    for (j = i; j < NUM_PROG_MARKS; ++j)
        strcat(msg, "-");

    strcat(msg, "] ");

    char pctstr[16];
    const char *frmt = pct_done == 100 ? "%.f%%" : "%.1f%%";
    snprintf(pctstr, sizeof(pctstr), frmt, pct_done);
    strcat(msg, pctstr);

    line_info_set(self, line_id, msg);
}

/* Filenumbers >= this number are receiving, otherwise sending.
 * Warning: This behaviour is not defined by the Tox API and is subject to change at any time.
 */
#define FILE_NUMBER_MAGIC_NUM (1 << 16)

/* Returns filenum's file transfer array index */
uint32_t get_file_transfer_index(uint32_t filenum)
{
    return filenum >= FILE_NUMBER_MAGIC_NUM ? (filenum >> 16) - 1 : filenum;
}

/* Returns the filenumber of a file receiver's index */
uint32_t get_file_receiver_filenum(uint32_t idx)
{
    return (idx + 1) << 16;
}

/* Return true if filenum is associated with a file receiver, false if file sender */
bool filenum_is_sending(uint32_t filenum)
{
    return filenum < FILE_NUMBER_MAGIC_NUM;
}

/* refreshes active file receiver status bars for friendnum */
void refresh_file_transfer_progress(ToxWindow *self, Tox *m, uint32_t friendnum)
{
    uint64_t curtime = get_unix_time();
    size_t i;

    for (i = 0; i < MAX_FILES; ++i) {
        if (Friends.list[friendnum].file_receiver[i].active) {
            if (timed_out(Friends.list[friendnum].file_receiver[i].last_progress, curtime, 1)) {
                uint64_t size = Friends.list[friendnum].file_receiver[i].file_size;
                double remain = size - Friends.list[friendnum].file_receiver[i].position;
                double pct_done = remain > 0 ? (1 - (remain / size)) * 100 : 100;

                print_progress_bar(self, Friends.list[friendnum].file_receiver[i].bps, pct_done,
                                   Friends.list[friendnum].file_receiver[i].line_id);

                Friends.list[friendnum].file_receiver[i].bps = 0;
                Friends.list[friendnum].file_receiver[i].last_progress = curtime;
            }
        }

        if (Friends.list[friendnum].file_sender[i].active) {
            if (timed_out(Friends.list[friendnum].file_sender[i].last_progress, curtime, 1)) {
                uint64_t size = Friends.list[friendnum].file_sender[i].file_size;
                double remain = size - Friends.list[friendnum].file_sender[i].position;
                double pct_done = remain > 0 ? (1 - (remain / size)) * 100 : 100;

                print_progress_bar(self, Friends.list[friendnum].file_sender[i].bps, pct_done,
                                   Friends.list[friendnum].file_sender[i].line_id);

                Friends.list[friendnum].file_sender[i].bps = 0;
                Friends.list[friendnum].file_sender[i].last_progress = curtime;
            }
        }
    }
}

/* Closes file transfer with filenum.
 * Set CTRL to -1 if we don't want to send a control signal.
 * Set message or self to NULL if we don't want to display a message.
 */
void close_file_transfer(ToxWindow *self, Tox *m, uint32_t filenum, uint32_t friendnum, int CTRL,
                        const char *message, Notification sound_type)
{
    uint32_t idx = get_file_transfer_index(filenum);
    bool sending = filenum_is_sending(filenum);

    if (sending && Friends.list[friendnum].file_sender[idx].active) {
        FILE *fp = Friends.list[friendnum].file_sender[idx].file;

        if (fp)
            fclose(fp);

        memset(&Friends.list[friendnum].file_sender[idx], 0, sizeof(struct FileSender));
    }
    else if (!sending && Friends.list[friendnum].file_receiver[idx].active) {
        FILE *fp = Friends.list[friendnum].file_receiver[idx].file;

        if (fp)
            fclose(fp);

        memset(&Friends.list[friendnum].file_receiver[idx], 0, sizeof(struct FileReceiver));
    }
    else
        return;

    if (CTRL >= 0)
        tox_file_control(m, friendnum, filenum, CTRL, NULL);

    if (message && self) {
        if (self->active_box != -1)
            box_notify2(self, sound_type, NT_NOFOCUS | NT_WNDALERT_2, self->active_box, "%s", message);
        else
            box_notify(self, sound_type, NT_NOFOCUS | NT_WNDALERT_2, &self->active_box, self->name, "%s", message);

        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "%s", message);
    }
}

/* Kills all active file transfers for friendnum */
void kill_all_file_transfers_friend(Tox *m, uint32_t friendnum)
{
    size_t i;

    for (i = 0; i < MAX_FILES; ++i) {
        fprintf(stderr, "%lu\n", i);
        if (Friends.list[friendnum].file_sender[i].active)
            close_file_transfer(NULL, m, i, friendnum, -1, NULL, silent);
        if (Friends.list[friendnum].file_receiver[i].active)
            close_file_transfer(NULL, m, get_file_receiver_filenum(i), friendnum, -1, NULL, silent);
    }
}

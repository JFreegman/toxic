/*  file_senders.c
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
#include "file_senders.h"
#include "line_info.h"
#include "misc_tools.h"
#include "notify.h"

FileSender file_senders[MAX_FILES];
uint8_t max_file_senders_index;
uint8_t num_active_file_senders;
extern ToxicFriend friends[MAX_FRIENDS_NUM];

#define KiB 1024
#define MiB 1048576       /* 1024 ^ 2 */
#define GiB 1073741824    /* 1024 ^ 3 */

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
void print_progress_bar(ToxWindow *self, int idx, int friendnum, double pct_done)
{
    double bps;
    uint32_t line_id;

    if (friendnum < 0) {
        bps = file_senders[idx].bps;
        line_id = file_senders[idx].line_id;
    } else {
        bps = friends[friendnum].file_receiver.bps[idx];
        line_id = friends[friendnum].file_receiver.line_id[idx];
    }

    const char *unit;

    if (bps < KiB) {
        unit = "B/s";
    } else if (bps < MiB) {
        unit = "KiB/s";
        bps /= (double) KiB;
    } else if (bps < GiB) {
        unit = "MiB/s";
        bps /= (double) MiB;
    } else {
        unit = "GiB/s";
        bps /= (double) GiB;
    }

    char msg[MAX_STR_SIZE];
    snprintf(msg, sizeof(msg), "%.1f %s [", bps, unit);
    int n = pct_done / (100 / NUM_PROG_MARKS);
    int i;

    for (i = 0; i < n; ++i)
        strcat(msg, "#");

    int j;

    for (j = i; j < NUM_PROG_MARKS; ++j)
        strcat(msg, "-");

    strcat(msg, "] ");

    char pctstr[16];
    const char *frmt = pct_done == 100 ? "%.f%%" : "%.1f%%";
    snprintf(pctstr, sizeof(pctstr), frmt, pct_done);
    strcat(msg, pctstr);

    line_info_set(self, line_id, msg);
}

static void set_max_file_senders_index(void)
{
    int j;

    for (j = max_file_senders_index; j > 0; --j) {
        if (file_senders[j - 1].active)
            break;
    }

    max_file_senders_index = j;
}

/* set CTRL to -1 if we don't want to send a control signal.
   set msg to NULL if we don't want to display a message */
void close_file_sender(ToxWindow *self, Tox *m, int i, const char *msg, int CTRL, int filenum, int32_t friendnum)
{
    if (msg != NULL) 
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "%s", msg);
    
    if (CTRL > 0)
        tox_file_send_control(m, friendnum, 0, filenum, CTRL, 0, 0);

    fclose(file_senders[i].file);
    memset(&file_senders[i], 0, sizeof(FileSender));
    set_max_file_senders_index();
    --num_active_file_senders;
}

void close_all_file_senders(Tox *m)
{
    int i;

    for (i = 0; i < max_file_senders_index; ++i) {
        if (file_senders[i].active) {
            fclose(file_senders[i].file);
            tox_file_send_control(m, file_senders[i].friendnum, 0, file_senders[i].filenum,
                                  TOX_FILECONTROL_KILL, 0, 0);
            memset(&file_senders[i], 0, sizeof(FileSender));
        }

        set_max_file_senders_index();
    }
}

static void send_file_data(ToxWindow *self, Tox *m, int i, int32_t friendnum, int filenum, const char *filename)
{
    FILE *fp = file_senders[i].file;

    while (true) {
        if (tox_file_send_data(m, friendnum, filenum, (uint8_t *) file_senders[i].nextpiece,
                               file_senders[i].piecelen) == -1)
            return;

        uint64_t curtime = get_unix_time();
        file_senders[i].timestamp = curtime;
        file_senders[i].bps += file_senders[i].piecelen;            
        file_senders[i].piecelen = fread(file_senders[i].nextpiece, 1,
                                         tox_file_data_size(m, friendnum), fp);

        double remain = (double) tox_file_data_remaining(m, friendnum, filenum, 0);

        /* refresh line with percentage complete and transfer speed (must be called once per second) */
        if ( (self->chatwin != NULL && timed_out(file_senders[i].last_progress, curtime, 1))
             || (!remain && !file_senders[i].finished) ) {
            file_senders[i].last_progress = curtime;
            double pct_done = remain > 0 ? (1 - (remain / file_senders[i].size)) * 100 : 100;
            print_progress_bar(self, i, -1, pct_done);
            file_senders[i].bps = 0;
        }

        /* file sender is closed in chat_onFileControl callback after receiving reply */
        if (file_senders[i].piecelen == 0 && !file_senders[i].finished) {
            tox_file_send_control(m, friendnum, 0, filenum, TOX_FILECONTROL_FINISHED, 0, 0);
            file_senders[i].finished = true;
        }
    }
}

void do_file_senders(Tox *m)
{
    int i;

    for (i = 0; i < max_file_senders_index; ++i) {
        if (!file_senders[i].active)
            continue;

        if (file_senders[i].queue_pos > 0) {
            --file_senders[i].queue_pos;
            continue;
        }

        ToxWindow *self = file_senders[i].toxwin;
        char *filename = file_senders[i].filename;
        int filenum = file_senders[i].filenum;
        int32_t friendnum = file_senders[i].friendnum;

        /* kill file transfer if chatwindow is closed */
        if (self->chatwin == NULL) {
            close_file_sender(self, m, i, NULL, TOX_FILECONTROL_KILL, filenum, friendnum);
            continue;
        }

        /* If file transfer has timed out kill transfer and send kill control */
        if (timed_out(file_senders[i].timestamp, get_unix_time(), TIMEOUT_FILESENDER)) {
            char msg[MAX_STR_SIZE];
            snprintf(msg, sizeof(msg), "File transfer for '%s' timed out.", filename);
            close_file_sender(self, m, i, msg, TOX_FILECONTROL_KILL, filenum, friendnum);
            sound_notify(self, error, NT_NOFOCUS | NT_WNDALERT_2, NULL);
            
            if (self->active_box != -1)
                box_notify2(self, error, NT_NOFOCUS | NT_WNDALERT_2, 
                            self->active_box, "File transfer for '%s' timed out.", filename );
            else
                box_notify(self, error, NT_NOFOCUS | NT_WNDALERT_2, &self->active_box,
                           self->name, "File transfer for '%s' timed out.", filename );
            continue;
        }

        file_senders[i].queue_pos = num_active_file_senders - 1;
        send_file_data(self, m, i, friendnum, filenum, filename);
    }
}

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
extern FriendsList Friends;

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
        bps = Friends.list[friendnum].file_receiver[idx].bps;
        line_id = Friends.list[friendnum].file_receiver[idx].line_id;
    }

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

/* refreshes active file receiver status bars */
static void refresh_recv_prog(Tox *m)
{
    int i;
    uint64_t curtime = get_unix_time();

    for (i = 2; i < MAX_WINDOWS_NUM; ++i) {
        ToxWindow *toxwin = get_window_ptr(i);

        if (toxwin == NULL || !toxwin->is_chat)
            continue;

        int fnum = toxwin->num;
        int j;

        for (j = 0; j < MAX_FILES; ++j) {
            if (!Friends.list[fnum].file_receiver[j].active)
                continue;

            int filenum = Friends.list[fnum].file_receiver[j].filenum;
            double remain = (double) tox_file_data_remaining(m, fnum, filenum, 1);

            /* must be called once per second */
            if (timed_out(Friends.list[fnum].file_receiver[filenum].last_progress, curtime, 1)) {
                Friends.list[fnum].file_receiver[filenum].last_progress = curtime;
                uint64_t size = Friends.list[fnum].file_receiver[filenum].size;
                double pct_done = remain > 0 ? (1 - (remain / size)) * 100 : 100;
                print_progress_bar(toxwin, filenum, fnum, pct_done);
                Friends.list[fnum].file_receiver[filenum].bps = 0;
            }
        }
    }
}

/* refreshes active file sender status bars */
static void refresh_sender_prog(Tox *m)
{
    int i;
    uint64_t curtime = get_unix_time();

    for (i = 0; i < max_file_senders_index; ++i) {
        if (!file_senders[i].active || file_senders[i].finished)
            continue;

        int filenum = file_senders[i].filenum;
        int32_t friendnum = file_senders[i].friendnum;
        double remain = (double) tox_file_data_remaining(m, friendnum, filenum, 0);

        /* must be called once per second */
        if (timed_out(file_senders[i].last_progress, curtime, 1)) {
            file_senders[i].last_progress = curtime;
            double pct_done = remain > 0 ? (1 - (remain / file_senders[i].size)) * 100 : 100;
            print_progress_bar(file_senders[i].toxwin, i, -1, pct_done);
            file_senders[i].bps = 0;
        }
    }
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

/* called whenever a file sender is opened or closed */
void reset_file_sender_queue(void)
{
    int i;
    int pos = 0;

    for (i = 0; i < max_file_senders_index; ++i) {
        if (file_senders[i].active)
            file_senders[i].queue_pos = pos++;
    }
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
    reset_file_sender_queue();
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

        file_senders[i].timestamp = get_unix_time();
        file_senders[i].bps += file_senders[i].piecelen;            
        file_senders[i].piecelen = fread(file_senders[i].nextpiece, 1,
                                         tox_file_data_size(m, friendnum), fp);

        /* note: file sender is closed in chat_onFileControl callback after receiving reply */
        if (file_senders[i].piecelen == 0) {
            if (feof(fp) != 0) {   /* make sure we're really at eof */
                print_progress_bar(self, i, -1, 100.0);
                tox_file_send_control(m, friendnum, 0, filenum, TOX_FILECONTROL_FINISHED, 0, 0);
                file_senders[i].finished = true;
            } else {
                char msg[MAX_STR_SIZE];
                snprintf(msg, sizeof(msg), "File transfer for '%s' failed: Read error.", file_senders[i].filename);
                close_file_sender(self, m, i, msg, TOX_FILECONTROL_KILL, filenum, friendnum);
                sound_notify(self, error, NT_NOFOCUS | NT_WNDALERT_2, NULL);

                if (self->active_box != -1)
                    box_notify2(self, error, NT_NOFOCUS | NT_WNDALERT_2, self->active_box, "%s", msg);
                else
                    box_notify(self, error, NT_NOFOCUS | NT_WNDALERT_2, &self->active_box, self->name, "%s", msg);
            }

            return;
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
            
            if (self->active_box != -1)
                box_notify2(self, error, NT_NOFOCUS | NT_WNDALERT_2, self->active_box, "%s", msg);
            else
                box_notify(self, error, NT_NOFOCUS | NT_WNDALERT_2, &self->active_box, self->name, "%s", msg);

            continue;
        }

        if (!file_senders[i].noconnection && !file_senders[i].finished)
            send_file_data(self, m, i, friendnum, filenum, filename);

        file_senders[i].queue_pos = num_active_file_senders - 1;
    }

    refresh_sender_prog(m);
    refresh_recv_prog(m);
}

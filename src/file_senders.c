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
#include "file_senders.h"
#include "line_info.h"
#include "misc_tools.h"

FileSender file_senders[MAX_FILES];
uint8_t max_file_senders_index;

static void set_max_file_senders_index(void)
{
    int j;

    for (j = max_file_senders_index; j > 0; --j) {
        if (file_senders[j - 1].active)
            break;
    }

    max_file_senders_index = j;
}

static void close_file_sender(ToxWindow *self, Tox *m, int i, uint8_t *msg, int CTRL, int filenum, int32_t friendnum)
{
    if (self->chatwin != NULL) {
        line_info_add(self, NULL, NULL, NULL, msg, SYS_MSG, 0, 0);
        alert_window(file_senders[i].toxwin, WINDOW_ALERT_2, true);
    }

    tox_file_send_control(m, friendnum, 0, filenum, CTRL, 0, 0);
    fclose(file_senders[i].file);
    memset(&file_senders[i], 0, sizeof(FileSender));
    set_max_file_senders_index();
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

void do_file_senders(Tox *m)
{
    uint8_t msg[MAX_STR_SIZE];
    int i;

    for (i = 0; i < max_file_senders_index; ++i) {
        if (!file_senders[i].active)
            continue;

        ToxWindow *self = file_senders[i].toxwin;
        uint8_t *pathname = file_senders[i].pathname;
        int filenum = file_senders[i].filenum;
        int32_t friendnum = file_senders[i].friendnum;
        FILE *fp = file_senders[i].file;

        /* If file transfer has timed out kill transfer and send kill control */
        if (timed_out(file_senders[i].timestamp, get_unix_time(), TIMEOUT_FILESENDER)) {
            snprintf(msg, sizeof(msg), "File transfer for '%s' timed out.", pathname);
            close_file_sender(self, m, i, msg, TOX_FILECONTROL_KILL, filenum, friendnum);
            continue;
        }

        while (true) {
            if (tox_file_send_data(m, friendnum, filenum, file_senders[i].nextpiece,
                                   file_senders[i].piecelen) == -1)
                break;

            uint64_t curtime = get_unix_time();
            file_senders[i].timestamp = curtime;
            file_senders[i].piecelen = fread(file_senders[i].nextpiece, 1,
                                             tox_file_data_size(m, friendnum), fp);

            long double remain = (long double) tox_file_data_remaining(m, friendnum, filenum, 0);

            /* refresh line with percentage complete */
            if ((self->chatwin != NULL && timed_out(file_senders[i].last_progress, curtime, 1)) || !remain) {
                file_senders[i].last_progress = curtime;
                uint64_t size = file_senders[i].size;
                long double pct_remain = remain ? (1 - (remain / size)) * 100 : 100;

                snprintf(msg, sizeof(msg), "File transfer for '%s' accepted (%.1Lf%%)", pathname, pct_remain);
                line_info_set(self, file_senders[i].line_id, msg);
            }

            if (file_senders[i].piecelen == 0) {
                snprintf(msg, sizeof(msg), "File '%s' successfuly sent.", pathname);
                close_file_sender(self, m, i, msg, TOX_FILECONTROL_FINISHED, filenum, friendnum);
                break;
            }
        }
    }
}

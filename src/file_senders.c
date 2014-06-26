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

extern struct _Winthread Winthread;

#define TIMEOUT_FILESENDER 120

FSenderThread file_senders[MAX_FILES];
int max_file_senders_index;

static void exit_file_sender(int i)
{
    pthread_mutex_lock(&file_senders[i].lock);
    fclose(file_senders[i].file);

    int j;

    for (j = max_file_senders_index; j > 0; --j) {
        if (file_senders[j - 1].active)
            break;
    }

    max_file_senders_index = j;
    pthread_mutex_unlock(&file_senders[i].lock);
    memset(&file_senders[i], 0, sizeof(FSenderThread));

    pthread_exit(0);
}

/* Should only be called on exit */
void close_all_file_senders(void)
{
    int i;

    for (i = 0; i < max_file_senders_index; ++i) {
        if (file_senders[i].active)
            fclose(file_senders[i].file);
    }
}

void *do_file_sender(void *data)
{
    int i = *(int *) data;

    uint8_t msg[MAX_STR_SIZE];

    ToxWindow *self = file_senders[i].toxwin;
    Tox *m = file_senders[i].m;
    uint8_t *pathname = file_senders[i].pathname;
    int filenum = file_senders[i].filenum;
    int32_t friendnum = file_senders[i].friendnum;
    FILE *fp = file_senders[i].file;

    while (file_senders[i].piecelen > 0) {
        uint64_t current_time = get_unix_time();

        /* If file transfer has timed out kill transfer and send kill control */
        if (timed_out(file_senders[i].timestamp, current_time, TIMEOUT_FILESENDER)) {
            pthread_mutex_lock(&file_senders[i].lock);

            if (self->chatwin != NULL) {
                snprintf(msg, sizeof(msg), "File transfer for '%s' timed out.", pathname);
                line_info_add(self, NULL, NULL, NULL, msg, SYS_MSG, 0, 0);
                alert_window(file_senders[i].toxwin, WINDOW_ALERT_2, true);
            }

            tox_file_send_control(m, friendnum, 0, filenum, TOX_FILECONTROL_KILL, 0, 0);
            pthread_mutex_unlock(&file_senders[i].lock);

            exit_file_sender(i);
        }

        pthread_mutex_lock(&file_senders[i].lock);

        while (tox_file_send_data(m, friendnum, filenum, file_senders[i].nextpiece,
                                                    file_senders[i].piecelen) != -1) {
            file_senders[i].timestamp = current_time;
            file_senders[i].piecelen = fread(file_senders[i].nextpiece, 1,
                                             tox_file_data_size(m, friendnum), fp);

            /* refresh line with percentage complete */
            if (self->chatwin != NULL) {
                uint64_t size = file_senders[i].size;
                long double remain = (long double) tox_file_data_remaining(m, friendnum, filenum, 0);
                long double pct_remain = remain ? (1 - (remain / size)) * 100 : 100;

                const uint8_t *name = file_senders[i].pathname;
                snprintf(msg, sizeof(msg), "File transfer for '%s' accepted (%.1Lf%%)", name, pct_remain);
                line_info_set(self, file_senders[i].line_id, msg);
            }
        }

        pthread_mutex_unlock(&file_senders[i].lock);
        usleep(40000);  /* Probably not optimal */
    }

    pthread_mutex_lock(&file_senders[i].lock);

    if (self->chatwin != NULL) {
        snprintf(msg, sizeof(msg), "File '%s' successfuly sent.", pathname);
        line_info_add(self, NULL, NULL, NULL, msg, SYS_MSG, 0, 0);
        alert_window(file_senders[i].toxwin, WINDOW_ALERT_2, true);
    }

    tox_file_send_control(m, friendnum, 0, filenum, TOX_FILECONTROL_FINISHED, 0, 0);
    pthread_mutex_unlock(&file_senders[i].lock);

    exit_file_sender(i);
    return 0;
}

void new_filesender_thread(ToxWindow *self, Tox *m, uint8_t *path, int path_len, FILE *file_to_send, 
                           int filenum, uint64_t filesize)
{
    pthread_t tid;
    pthread_mutex_t lock;
    int i;

    for (i = 0; i < MAX_FILES; ++i) {
        if (file_senders[i].active)
            continue;

        if (i == max_file_senders_index)
            ++max_file_senders_index;

        break;
    }

    memcpy(file_senders[i].pathname, path, path_len + 1);
    file_senders[i].tid = tid;
    file_senders[i].lock = lock;
    file_senders[i].active = true;
    file_senders[i].toxwin = self;
    file_senders[i].m = m;
    file_senders[i].file = file_to_send;
    file_senders[i].filenum = filenum;
    file_senders[i].friendnum = self->num;
    file_senders[i].timestamp = get_unix_time();
    file_senders[i].size = filesize;
    file_senders[i].piecelen = fread(file_senders[i].nextpiece, 1,
                                     tox_file_data_size(m, self->num), file_to_send);
    uint8_t msg[MAX_STR_SIZE];
    snprintf(msg, sizeof(msg), "Sending file: '%s'", path);
    line_info_add(self, NULL, NULL, NULL, msg, SYS_MSG, 0, 0);

    if (pthread_create(&file_senders[i].tid, NULL, do_file_sender, (void *) &i) != 0)
        exit_toxic_err("failed in new_filesenders_thread", FATALERR_THREAD_CREATE);

    if (pthread_mutex_init(&file_senders[i].lock, NULL) != 0)
        exit_toxic_err("failed in new_filesende", FATALERR_MUTEX_INIT);
}
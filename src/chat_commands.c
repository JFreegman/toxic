/*  chat_commands.c
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

#include <stdlib.h>
#include <string.h>

#include "toxic.h"
#include "windows.h"
#include "misc_tools.h"
#include "friendlist.h"
#include "execute.h"
#include "line_info.h"
#include "groupchat.h"
#include "chat.h"
#include "file_senders.h"

extern ToxWindow *prompt;

extern ToxicFriend friends[MAX_FRIENDS_NUM];

extern FileSender file_senders[MAX_FILES];
extern uint8_t max_file_senders_index;
extern uint8_t num_active_file_senders;

void cmd_cancelfile(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    if (argc < 2) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Invalid syntax.");
        return;
    }

    const char *inoutstr = argv[1];
    int filenum = atoi(argv[2]);

    if (filenum > MAX_FILES || filenum < 0) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Invalid file ID.");
        return;
    }

    int inout;

    if (strcasecmp(inoutstr, "in") == 0) {
        inout = 1;
    } else if (strcasecmp(inoutstr, "out") == 0) {
        inout = 0;
    } else {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Error: Type must be 'in' or 'out'.");
        return;
    }

    if (inout == 1) {    /* cancel an incoming file transfer */
        if (!friends[self->num].file_receiver.active[filenum]) {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Invalid file ID.");
            return;  
        }

        const char *filepath = friends[self->num].file_receiver.filenames[filenum];
        char name[MAX_STR_SIZE];
        get_file_name(name, sizeof(name), filepath);
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "File transfer for '%s' canceled.", name);
        tox_file_send_control(m, self->num, 1, filenum, TOX_FILECONTROL_KILL, 0, 0);
        chat_close_file_receiver(self->num, filenum);
    } else {    /* cancel an outgoing file transfer */
        int i;
        bool match = false;

        for (i = 0; i < MAX_FILES; ++i) {
            if (file_senders[i].active && file_senders[i].filenum == filenum) {
                match = true;
                break;
            }
        }

        if (!match) {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Invalid file ID.");
            return;  
        }

        const char *filename = file_senders[i].filename;
        char msg[MAX_STR_SIZE];
        snprintf(msg, sizeof(msg), "File transfer for '%s' canceled.", filename);
        close_file_sender(self, m, i, msg, TOX_FILECONTROL_KILL, filenum, self->num);
    }
}

void cmd_groupinvite(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    const char *errmsg;

    if (argc < 1) {
        errmsg = "Invalid syntax";
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, errmsg);
        return;
    }

    int groupnum = atoi(argv[1]);

    if (groupnum == 0 && strcmp(argv[1], "0")) {    /* atoi returns 0 value on invalid input */
        errmsg = "Invalid syntax.";
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, errmsg);
        return;
    }

    if (tox_invite_friend(m, self->num, groupnum) == -1) {
        errmsg = "Failed to invite contact to group.";
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, errmsg);
        return;
    }

    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Invited contact to Group %d.", groupnum);
}

void cmd_join_group(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    const char *errmsg;

    if (get_num_active_windows() >= MAX_WINDOWS_NUM) {
        errmsg = " * Warning: Too many windows are open.";
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, RED, errmsg);
        return;
    }

    const char *groupkey = friends[self->num].groupchat_key;

    if (!friends[self->num].groupchat_pending) {
        errmsg = "No pending group chat invite.";
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, errmsg);
        return;
    }

    int groupnum = tox_join_groupchat(m, self->num, (uint8_t *) groupkey);

    if (groupnum == -1) {
        errmsg = "Group chat instance failed to initialize.";
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, errmsg);
        return;
    }

    if (init_groupchat_win(prompt, m, groupnum) == -1) {
        errmsg = "Group chat window failed to initialize.";
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, errmsg);
        tox_del_groupchat(m, groupnum);
        return;
    }
}

void cmd_savefile(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    const char *errmsg;

    if (argc != 1) {
        errmsg = "Invalid syntax.";
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, errmsg);
        return;
    }

    uint8_t filenum = atoi(argv[1]);

    if ((filenum == 0 && strcmp(argv[1], "0")) || filenum >= MAX_FILES) {
        errmsg = "No pending file transfers with that number.";
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, errmsg);
        return;
    }

    if (!friends[self->num].file_receiver.pending[filenum]) {
        errmsg = "No pending file transfers with that number.";
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, errmsg);
        return;
    }

    const char *filename = friends[self->num].file_receiver.filenames[filenum];

    if (tox_file_send_control(m, self->num, 1, filenum, TOX_FILECONTROL_ACCEPT, 0, 0) == 0) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Saving file as [%d]: '%s'", filenum, filename);

        /* prep progress bar line */
        char progline[MAX_STR_SIZE];
        prep_prog_line(progline);
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "%s", progline);
        friends[self->num].file_receiver.line_id[filenum] = self->chatwin->hst->line_end->id + 2;

        if ((friends[self->num].file_receiver.files[filenum] = fopen(filename, "a")) == NULL) {
            errmsg = "* Error writing to file.";
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, RED, errmsg);
            tox_file_send_control(m, self->num, 1, filenum, TOX_FILECONTROL_KILL, 0, 0);
        }
    } else {
        errmsg = "File transfer failed.";
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, errmsg);
    }

    friends[self->num].file_receiver.pending[filenum] = false;
    friends[self->num].file_receiver.active[filenum] = true;
}

void cmd_sendfile(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    const char *errmsg;

    if (max_file_senders_index >= (MAX_FILES - 1)) {
        errmsg = "Please wait for some of your outgoing file transfers to complete.";
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, errmsg);
        return;
    }

    if (argc < 1) {
        errmsg = "Invalid syntax.";
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, errmsg);
        return;
    }

    if (argv[1][0] != '\"') {
        errmsg = "File path must be enclosed in quotes.";
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, errmsg);
        return;
    }

    /* remove opening and closing quotes */
    char path[MAX_STR_SIZE];
    snprintf(path, sizeof(path), "%s", &argv[1][1]);
    int path_len = strlen(path) - 1;
    path[path_len] = '\0';

    if (path_len >= MAX_STR_SIZE) {
        errmsg = "File path exceeds character limit.";
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, errmsg);
        return;
    }

    FILE *file_to_send = fopen(path, "r");

    if (file_to_send == NULL) {
        errmsg = "File not found.";
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, errmsg);
        return;
    }

    fseek(file_to_send, 0, SEEK_END);
    uint64_t filesize = ftell(file_to_send);
    fseek(file_to_send, 0, SEEK_SET);

    char filename[MAX_STR_SIZE] = {0};
    get_file_name(filename, sizeof(filename), path);
    int namelen = strlen(filename);
    int filenum = tox_new_file_sender(m, self->num, filesize, (const uint8_t *) filename, namelen);

    if (filenum == -1) {
        errmsg = "Error sending file.";
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, errmsg);
        return;
    }

    int i;

    for (i = 0; i < MAX_FILES; ++i) {
        if (!file_senders[i].active) {
            file_senders[i].queue_pos = num_active_file_senders;
            memcpy(file_senders[i].filename, filename, namelen + 1);
            file_senders[i].active = true;
            file_senders[i].toxwin = self;
            file_senders[i].file = file_to_send;
            file_senders[i].filenum = filenum;
            file_senders[i].friendnum = self->num;
            file_senders[i].timestamp = get_unix_time();
            file_senders[i].size = filesize;
            file_senders[i].piecelen = fread(file_senders[i].nextpiece, 1,
                                             tox_file_data_size(m, self->num), file_to_send);

            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Sending file [%d]: '%s'", filenum, filename);

            ++num_active_file_senders;

            if (i == max_file_senders_index)
                ++max_file_senders_index;

            return;
        }
    }
}

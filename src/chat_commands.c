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

extern ToxWindow *prompt;

extern ToxicFriend friends[MAX_FRIENDS_NUM];

extern FileSender file_senders[MAX_FILES];
extern uint8_t max_file_senders_index;

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

    const char *msg = "Invited contact to Group %d.";
    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, msg, groupnum);
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
        const char *msg = "Saving file as: '%s' (%.1f%%)";
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, msg, filename, 0.0);
        friends[self->num].file_receiver.line_id[filenum] = self->chatwin->hst->line_end->id + 1;

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

    char *path = argv[1];

    if (path[0] != '\"') {
        errmsg = "File path must be enclosed in quotes.";
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, errmsg);
        return;
    }

    ++path;
    int path_len = strlen(path) - 1;
    path[path_len] = '\0';

    if (path_len > MAX_STR_SIZE) {
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

    char filename[MAX_STR_SIZE];
    get_file_name(filename, sizeof(filename), path);
    int filenum = tox_new_file_sender(m, self->num, filesize, (const uint8_t *) filename, strlen(filename));

    if (filenum == -1) {
        errmsg = "Error sending file.";
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, errmsg);
        return;
    }

    int i;

    for (i = 0; i < MAX_FILES; ++i) {
        if (!file_senders[i].active) {
            memcpy(file_senders[i].pathname, path, path_len + 1);
            file_senders[i].active = true;
            file_senders[i].toxwin = self;
            file_senders[i].file = file_to_send;
            file_senders[i].filenum = filenum;
            file_senders[i].friendnum = self->num;
            file_senders[i].timestamp = get_unix_time();
            file_senders[i].size = filesize;
            file_senders[i].piecelen = fread(file_senders[i].nextpiece, 1,
                                             tox_file_data_size(m, self->num), file_to_send);

            const char *msg = "Sending file: '%s'";
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, msg, path);

            if (i == max_file_senders_index)
                ++max_file_senders_index;

            return;
        }
    }
}

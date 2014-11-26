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

#ifdef AUDIO
#include "audio_call.h"
#endif

extern ToxWindow *prompt;
extern FriendsList Friends;

extern FileSender file_senders[MAX_FILES];
extern uint8_t max_file_senders_index;
extern uint8_t num_active_file_senders;

void cmd_cancelfile(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    if (argc < 2) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Requires type in|out and the file ID.");
        return;
    }

    const char *inoutstr = argv[1];
    int filenum = atoi(argv[2]);

    if (filenum >= MAX_FILES || filenum < 0) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Invalid file ID.");
        return;
    }

    if (strcasecmp(inoutstr, "in") == 0) {    /* cancel an incoming file transfer */
        if (!Friends.list[self->num].file_receiver[filenum].active) {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Invalid file ID.");
            return;  
        }

        const char *filepath = Friends.list[self->num].file_receiver[filenum].filename;
        char name[MAX_STR_SIZE];
        get_file_name(name, sizeof(name), filepath);
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "File transfer for '%s' canceled.", name);
        chat_close_file_receiver(m, filenum, self->num, TOX_FILECONTROL_KILL);
        return;
    } else if (strcasecmp(inoutstr, "out") == 0) {    /* cancel an outgoing file transfer */
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
        return;
    } else {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Type must be 'in' or 'out'.");
        return;
    }
}

void cmd_groupinvite(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    if (argc < 1) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Group number required.");
        return;
    }

    int groupnum = atoi(argv[1]);

    if (groupnum == 0 && strcmp(argv[1], "0")) {    /* atoi returns 0 value on invalid input */
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Invalid group number.");
        return;
    }

    if (tox_invite_friend(m, self->num, groupnum) == -1) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to invite contact to group.");
        return;
    }

    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Invited contact to Group %d.", groupnum);
}

void cmd_join_group(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    if (get_num_active_windows() >= MAX_WINDOWS_NUM) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, RED, " * Warning: Too many windows are open.");
        return;
    }

    const char *groupkey = Friends.list[self->num].group_invite.key;
    uint16_t length = Friends.list[self->num].group_invite.length;
    uint8_t type = Friends.list[self->num].group_invite.type;

    if (!Friends.list[self->num].group_invite.pending) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "No pending group chat invite.");
        return;
    }

    int groupnum = -1;

    if (type == TOX_GROUPCHAT_TYPE_TEXT)
        groupnum = tox_join_groupchat(m, self->num, (uint8_t *) groupkey, length);
#ifdef AUDIO
    else
        groupnum = toxav_join_av_groupchat(m, self->num, (uint8_t *) groupkey, length,
                                           write_device_callback_group, self);
#endif

    if (groupnum == -1) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Group chat instance failed to initialize.");
        return;
    }

    if (init_groupchat_win(prompt, m, groupnum, type) == -1) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Group chat window failed to initialize.");
        tox_del_groupchat(m, groupnum);
        return;
    }

}

void cmd_savefile(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    if (argc < 1) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "File ID required.");
        return;
    }

    uint8_t filenum = atoi(argv[1]);

    if ((filenum == 0 && strcmp(argv[1], "0")) || filenum >= MAX_FILES) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "No pending file transfers with that ID.");
        return;
    }

    if (!Friends.list[self->num].file_receiver[filenum].pending) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "No pending file transfers with that ID.");
        return;
    }

    const char *filename = Friends.list[self->num].file_receiver[filenum].filename;

    if (tox_file_send_control(m, self->num, 1, filenum, TOX_FILECONTROL_ACCEPT, 0, 0) == 0) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Saving file [%d] as: '%s'", filenum, filename);

        /* prep progress bar line */
        char progline[MAX_STR_SIZE];
        prep_prog_line(progline);
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "%s", progline);
        Friends.list[self->num].file_receiver[filenum].line_id = self->chatwin->hst->line_end->id + 2;

        if ((Friends.list[self->num].file_receiver[filenum].file = fopen(filename, "a")) == NULL) {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, RED, "* Error writing to file.");
            tox_file_send_control(m, self->num, 1, filenum, TOX_FILECONTROL_KILL, 0, 0);
        } else {
            Friends.list[self->num].file_receiver[filenum].active = true;
            ++Friends.list[self->num].active_file_receivers;
        }
    } else {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "File transfer failed.");
    }

    Friends.list[self->num].file_receiver[filenum].pending = false;
}

void cmd_sendfile(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    if (max_file_senders_index >= (MAX_FILES - 1)) {
        const char *errmsg = "Please wait for some of your outgoing file transfers to complete.";
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, errmsg);
        return;
    }

    if (argc < 1) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "File path required.");
        return;
    }

    if (argv[1][0] != '\"') {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "File path must be enclosed in quotes.");
        return;
    }

    /* remove opening and closing quotes */
    char path[MAX_STR_SIZE];
    snprintf(path, sizeof(path), "%s", &argv[1][1]);
    int path_len = strlen(path) - 1;
    path[path_len] = '\0';

    if (path_len >= MAX_STR_SIZE) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "File path exceeds character limit.");
        return;
    }

    FILE *file_to_send = fopen(path, "r");

    if (file_to_send == NULL) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "File not found.");
        return;
    }

    off_t filesize = file_size(path);

    if (filesize == -1) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "File corrupt.");
        fclose(file_to_send);
        return;
    }

    char filename[MAX_STR_SIZE] = {0};
    get_file_name(filename, sizeof(filename), path);
    int namelen = strlen(filename);
    int filenum = tox_new_file_sender(m, self->num, filesize, (const uint8_t *) filename, namelen);

    if (filenum == -1) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Error sending file.");
        fclose(file_to_send);
        return;
    }

    int i;

    for (i = 0; i < MAX_FILES; ++i) {
        if (!file_senders[i].active) {
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

            char sizestr[32];
            bytes_convert_str(sizestr, sizeof(sizestr), filesize);
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, 
                          "Sending file [%d]: '%s' (%s)", filenum, filename, sizestr);

            ++num_active_file_senders;

            if (i == max_file_senders_index)
                ++max_file_senders_index;

            reset_file_sender_queue();
            return;
        }
    }
}

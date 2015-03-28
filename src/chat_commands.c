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
#include "file_transfers.h"

extern ToxWindow *prompt;
extern FriendsList Friends;

void cmd_cancelfile(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    if (argc < 2) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Requires type in|out and the file ID.");
        return;
    }

    char msg[MAX_STR_SIZE];
    const char *inoutstr = argv[1];
    int idx = atoi(argv[2]);

    if (idx >= MAX_FILES || idx < 0) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Invalid file ID.");
        return;
    }

    if (strcasecmp(inoutstr, "in") == 0) {    /* cancel an incoming file transfer */
        if (!Friends.list[self->num].file_receiver[idx].active) {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Invalid file ID.");
            return;
        }

        const char *file_path = Friends.list[self->num].file_receiver[idx].file_path;
        char file_name[MAX_STR_SIZE];
        get_file_name(file_name, sizeof(file_name), file_path);
        snprintf(msg, sizeof(msg), "File transfer for '%s' canceled.", file_name);
        close_file_transfer(self, m, get_file_receiver_filenum(idx), self->num, TOX_FILE_CONTROL_CANCEL, msg, silent);
        return;
    } else if (strcasecmp(inoutstr, "out") == 0) {    /* cancel an outgoing file transfer */
        if (!Friends.list[self->num].file_sender[idx].active) {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Invalid file ID.");
            return;
        }

        snprintf(msg, sizeof(msg), "File transfer for '%s' canceled.", Friends.list[self->num].file_sender[idx].file_name);
        close_file_transfer(self, m, idx, self->num, TOX_FILE_CONTROL_CANCEL, msg, silent);
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
                                           write_device_callback_group, NULL);
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

    int idx = atoi(argv[1]);

    if ((idx == 0 && strcmp(argv[1], "0")) || idx >= MAX_FILES) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "No pending file transfers with that ID.");
        return;
    }

    uint32_t filenum = get_file_receiver_filenum(idx);

    if (!Friends.list[self->num].file_receiver[idx].pending) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "No pending file transfers with that ID.");
        return;
    }

    const char *file_path = Friends.list[self->num].file_receiver[idx].file_path;

    TOX_ERR_FILE_CONTROL err;
    tox_file_control(m, self->num, filenum, TOX_FILE_CONTROL_RESUME, &err);

    if (err != TOX_ERR_FILE_CONTROL_OK)
        goto on_recv_error;

    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Saving file [%d] as: '%s'", idx, file_path);

    /* prep progress bar line */
    char progline[MAX_STR_SIZE];
    prep_prog_line(progline);
    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "%s", progline);
    Friends.list[self->num].file_receiver[idx].line_id = self->chatwin->hst->line_end->id + 2;
    Friends.list[self->num].file_receiver[idx].pending = false;

    if ((Friends.list[self->num].file_receiver[idx].file = fopen(file_path, "a")) == NULL) {
        tox_file_control(m, self->num, filenum, TOX_FILE_CONTROL_CANCEL, NULL);
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "File transfer failed: Invalid file path.");
    } else {
        Friends.list[self->num].file_receiver[idx].active = true;
    }

    return;

on_recv_error:
    switch (err) {
        case TOX_ERR_FILE_CONTROL_FRIEND_NOT_FOUND:
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "File transfer failed: Friend not found.");
            return;

        case TOX_ERR_FILE_CONTROL_FRIEND_NOT_CONNECTED:
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "File transfer failed: Friend is not online.");
            return;

        case TOX_ERR_FILE_CONTROL_NOT_FOUND:
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "File transfer failed: Invalid filenumber.");
            return;

        case TOX_ERR_FILE_CONTROL_SENDQ:
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "File transfer failed: Connection error.");
            return;

        default:
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "File transfer failed (error %d)\n", err);
            return;
    }
}

void cmd_sendfile(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    const char *errmsg = NULL;

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

    if (filesize == 0) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Invalid file.");
        fclose(file_to_send);
        return;
    }

    char file_name[TOX_MAX_FILENAME_LENGTH];
    get_file_name(file_name, sizeof(file_name), path);
    size_t namelen = strlen(file_name);

    TOX_ERR_FILE_SEND err;
    uint32_t filenum = tox_file_send(m, self->num, TOX_FILE_KIND_DATA, (uint64_t) filesize,
                                     NULL, (uint8_t *) file_name, namelen, &err);

    if (err != TOX_ERR_FILE_SEND_OK)
        goto on_send_error;

    uint32_t idx = get_file_transfer_index(filenum);

    if (idx >= MAX_FILES) {
        errmsg = "File transfer failed: Too many concurrent file transfers";
        goto on_send_error;
    }

    memcpy(Friends.list[self->num].file_sender[idx].file_name, file_name, namelen + 1);
    Friends.list[self->num].file_sender[idx].active = true;
    Friends.list[self->num].file_sender[idx].started = false;
    Friends.list[self->num].file_sender[idx].file = file_to_send;
    Friends.list[self->num].file_sender[idx].timestamp = get_unix_time();
    Friends.list[self->num].file_sender[idx].file_size = filesize;

    char sizestr[32];
    bytes_convert_str(sizestr, sizeof(sizestr), filesize);
    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Sending file [%d]: '%s' (%s)", idx, file_name, sizestr);

    return;

on_send_error:
    switch (err) {
        case TOX_ERR_FILE_SEND_FRIEND_NOT_FOUND:
            errmsg = "File transfer failed: Invalid friend.";
            break;

        case TOX_ERR_FILE_SEND_FRIEND_NOT_CONNECTED:
            errmsg = "File transfer failed: Friend is offline.";
            break;

        case TOX_ERR_FILE_SEND_NAME_TOO_LONG:
            errmsg = "File transfer failed: Filename is too long.";
            break;

        case TOX_ERR_FILE_SEND_TOO_MANY:
            errmsg = "File transfer failed: Too many concurrent file transfers.";
            break;

        default:
            errmsg = "File transfer failed";
            break;
    }

    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "%s", errmsg);
    tox_file_control(m, self->num, filenum, TOX_FILE_CONTROL_CANCEL, NULL);
    fclose(file_to_send);
}

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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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

void cmd_chat_help(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    struct history *hst = self->chatwin->hst;
    line_info_clear(hst);
    struct line_info *start = hst->line_start;

    if (argc == 1) {
        if (!strcmp(argv[1], "global")) {
            execute(window, self, m, "/help", GLOBAL_COMMAND_MODE);
            return;
        }
    }

    uint8_t *msg = "Chat commands:";
    line_info_add(self, NULL, NULL, NULL, msg, SYS_MSG, 1, CYAN);

#ifdef _SUPPORT_AUDIO
#define NUMLINES 16
#else
#define NUMLINES 9
#endif

    uint8_t lines[NUMLINES][MAX_STR_SIZE] = {

#ifdef _SUPPORT_AUDIO
        { "    /call                      : Audio call"                           },
        { "    /cancel                    : Cancel call"                          },
        { "    /answer                    : Answer incomming call"                },
        { "    /reject                    : Reject incoming call"                 },
        { "    /hangup                    : Hangup active call"                   },
        { "    /sdev <type> <id>          : Change active device"                 },
        { "    /mute <type>               : Mute active device if in call"        },
        { "    /sense <value>             : VAD sensitivity treshold"             },
#endif /* _SUPPORT_AUDIO */
        { "    /invite <n>                : Invite friend to a group chat"        },
        { "    /join                      : Join a pending group chat"            },
        { "    /log <on> or <off>         : Enable/disable logging"               },
        { "    /sendfile <filepath>       : Send a file"                          },
        { "    /savefile <n>              : Receive a file"                       },
        { "    /close                     : Close the current chat window"        },
        { "    /help                      : Print this message again"             },
        { "    /help global               : Show a list of global commands"       },
    };

    int i;

    for (i = 0; i < NUMLINES; ++i)
        line_info_add(self, NULL, NULL, NULL, lines[i], SYS_MSG, 0, 0);

    msg = " * Use Page Up/Page Down to scroll chat history\n";
    line_info_add(self, NULL, NULL, NULL, msg, SYS_MSG, 1, CYAN);

    hst->line_start = start;
}

void cmd_groupinvite(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    uint8_t *errmsg;

    if (argc < 1) {
        errmsg = "Invalid syntax";
        line_info_add(self, NULL, NULL, NULL, errmsg, SYS_MSG, 0, 0);
        return;
    }

    int groupnum = atoi(argv[1]);

    if (groupnum == 0 && strcmp(argv[1], "0")) {    /* atoi returns 0 value on invalid input */
        errmsg = "Invalid syntax.";
        line_info_add(self, NULL, NULL, NULL, errmsg, SYS_MSG, 0, 0);
        return;
    }

    if (tox_invite_friend(m, self->num, groupnum) == -1) {
        errmsg = "Failed to invite friend.";
        line_info_add(self, NULL, NULL, NULL, errmsg, SYS_MSG, 0, 0);
        return;
    }

    uint8_t msg[MAX_STR_SIZE];
    snprintf(msg, sizeof(msg), "Invited friend to Room #%d.", groupnum);
    line_info_add(self, NULL, NULL, NULL, msg, SYS_MSG, 0, 0);
}

void cmd_join_group(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    uint8_t *errmsg;

    if (get_num_active_windows() >= MAX_WINDOWS_NUM) {
        errmsg = " * Warning: Too many windows are open.";
        line_info_add(self, NULL, NULL, NULL, errmsg, SYS_MSG, 0, RED);
        return;
    }

    uint8_t *groupkey = friends[self->num].pending_groupchat;

    if (groupkey[0] == '\0') {
        errmsg = "No pending group chat invite.";
        line_info_add(self, NULL, NULL, NULL, errmsg, SYS_MSG, 0, 0);
        return;
    }

    int groupnum = tox_join_groupchat(m, self->num, groupkey);

    if (groupnum == -1) {
        errmsg = "Group chat instance failed to initialize.";
        line_info_add(self, NULL, NULL, NULL, errmsg, SYS_MSG, 0, 0);
        return;
    }

    if (init_groupchat_win(prompt, m, groupnum) == -1) {
        errmsg = "Group chat window failed to initialize.";
        line_info_add(self, NULL, NULL, NULL, errmsg, SYS_MSG, 0, 0);
        tox_del_groupchat(m, groupnum);
        return;
    }
}

void cmd_savefile(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    uint8_t *errmsg;

    if (argc != 1) {
        errmsg = "Invalid syntax.";
        line_info_add(self, NULL, NULL, NULL, errmsg, SYS_MSG, 0, 0);
        return;
    }

    uint8_t filenum = atoi(argv[1]);

    if ((filenum == 0 && strcmp(argv[1], "0")) || filenum >= MAX_FILES) {
        errmsg = "No pending file transfers with that number.";
        line_info_add(self, NULL, NULL, NULL, errmsg, SYS_MSG, 0, 0);
        return;
    }

    if (!friends[self->num].file_receiver.pending[filenum]) {
        errmsg = "No pending file transfers with that number.";
        line_info_add(self, NULL, NULL, NULL, errmsg, SYS_MSG, 0, 0);
        return;
    }

    uint8_t *filename = friends[self->num].file_receiver.filenames[filenum];

    if (tox_file_send_control(m, self->num, 1, filenum, TOX_FILECONTROL_ACCEPT, 0, 0) == 0) {
        uint8_t msg[MAX_STR_SIZE];
        snprintf(msg, sizeof(msg), "Saving file as: '%s' (%.1f%%)", filename, 0.0);
        line_info_add(self, NULL, NULL, NULL, msg, SYS_MSG, 0, 0);
        friends[self->num].file_receiver.line_id[filenum] = self->chatwin->hst->line_end->id + 1;

        if ((friends[self->num].file_receiver.files[filenum] = fopen(filename, "a")) == NULL) {
            errmsg = "* Error writing to file.";
            line_info_add(self, NULL, NULL, NULL, errmsg, SYS_MSG, 0, RED);
            tox_file_send_control(m, self->num, 1, filenum, TOX_FILECONTROL_KILL, 0, 0);
        }
    } else {
        errmsg = "File transfer failed.";
        line_info_add(self, NULL, NULL, NULL, errmsg, SYS_MSG, 0, 0);
    }

    friends[self->num].file_receiver.pending[filenum] = false;
}

void cmd_sendfile(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    uint8_t *errmsg;

    if (max_file_senders_index >= (MAX_FILES - 1)) {
        errmsg = "Please wait for some of your outgoing file transfers to complete.";
        line_info_add(self, NULL, NULL, NULL, errmsg, SYS_MSG, 0, 0);
        return;
    }

    if (argc < 1) {
        errmsg = "Invalid syntax.";
        line_info_add(self, NULL, NULL, NULL, errmsg, SYS_MSG, 0, 0);
        return;
    }

    uint8_t *path = argv[1];

    if (path[0] != '\"') {
        errmsg = "File path must be enclosed in quotes.";
        line_info_add(self, NULL, NULL, NULL, errmsg, SYS_MSG, 0, 0);
        return;
    }

    path[strlen(++path) - 1] = L'\0';
    int path_len = strlen(path);

    if (path_len > MAX_STR_SIZE) {
        errmsg = "File path exceeds character limit.";
        line_info_add(self, NULL, NULL, NULL, errmsg, SYS_MSG, 0, 0);
        return;
    }

    FILE *file_to_send = fopen(path, "r");

    if (file_to_send == NULL) {
        errmsg = "File not found.";
        line_info_add(self, NULL, NULL, NULL, errmsg, SYS_MSG, 0, 0);
        return;
    }

    fseek(file_to_send, 0, SEEK_END);
    uint64_t filesize = ftell(file_to_send);
    fseek(file_to_send, 0, SEEK_SET);

    uint8_t filename[MAX_STR_SIZE];
    get_file_name(filename, path);
    int filenum = tox_new_file_sender(m, self->num, filesize, filename, strlen(filename));

    if (filenum == -1) {
        errmsg = "Error sending file.";
        line_info_add(self, NULL, NULL, NULL, errmsg, SYS_MSG, 0, 0);
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

            uint8_t msg[MAX_STR_SIZE];
            snprintf(msg, sizeof(msg), "Sending file: '%s'", path);
            line_info_add(self, NULL, NULL, NULL, msg, SYS_MSG, 0, 0);

            if (i == max_file_senders_index)
                ++max_file_senders_index;

            return;
        }
    }
}

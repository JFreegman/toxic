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

#include "toxic_windows.h"
#include "misc_tools.h"
#include "friendlist.h"
#include "execute.h"

extern ToxWindow *prompt;

extern ToxicFriend friends[MAX_FRIENDS_NUM];

extern FileSender file_senders[MAX_FILES];
extern uint8_t max_file_senders_index;

void cmd_chat_help(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    if (argc == 1) {
        if (!strcmp(argv[1], "global")) {
            execute(window, self, m, "/help", GLOBAL_COMMAND_MODE);
            return;
        }
    }

    wattron(window, COLOR_PAIR(CYAN) | A_BOLD);
    wprintw(window, "Chat commands:\n");
    wattroff(window, COLOR_PAIR(CYAN) | A_BOLD);

#ifdef _SUPPORT_AUDIO

    wprintw(window, "    /call                      : Audio call\n");
    wprintw(window, "    /cancel                    : Cancel call\n");
    wprintw(window, "    /answer                    : Answer incomming call\n");
    wprintw(window, "    /hangup                    : Hangup active call\n");

#endif /* _SUPPORT_AUDIO */
    
    wprintw(window, "    /invite <n>                : Invite friend to a group chat\n");
    wprintw(window, "    /join                      : Join a pending group chat\n");
    wprintw(window, "    /log <on> or <off>         : Enable/disable logging\n");
    wprintw(window, "    /sendfile <filepath>       : Send a file\n");
    wprintw(window, "    /savefile <n>              : Receive a file\n");
    wprintw(window, "    /close                     : Close the current chat window\n");
    wprintw(window, "    /help                      : Print this message again\n");
    wprintw(window, "    /help global               : Show a list of global commands\n");
    
    wattron(window, COLOR_PAIR(CYAN) | A_BOLD);
    wprintw(window, " * Argument messages must be enclosed in quotation marks.\n\n");
    wattroff(window, COLOR_PAIR(CYAN) | A_BOLD);
}

void cmd_groupinvite(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    if (argc < 1) {
      wprintw(window, "Invalid syntax.\n");
      return;
    }

    int groupnum = atoi(argv[1]);

    if (groupnum == 0 && strcmp(argv[1], "0")) {    /* atoi returns 0 value on invalid input */
        wprintw(window, "Invalid syntax.\n");
        return;
    }

    if (tox_invite_friend(m, self->num, groupnum) == -1) {
        wprintw(window, "Failed to invite friend.\n");
        return;
    }

    wprintw(window, "Invited friend to group chat %d.\n", groupnum);
}

void cmd_join_group(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    if (get_num_active_windows() >= MAX_WINDOWS_NUM) {
        wattron(window, COLOR_PAIR(RED));
        wprintw(window, " * Warning: Too many windows are open.\n");
        wattron(window, COLOR_PAIR(RED));
        return;
    }

    uint8_t *groupkey = friends[self->num].pending_groupchat;

    if (groupkey[0] == '\0') {
        wprintw(window, "No pending group chat invite.\n");
        return;
    }

    int groupnum = tox_join_groupchat(m, self->num, groupkey);

    if (groupnum == -1) {
        wprintw(window, "Group chat instance failed to initialize.\n");
        return;
    }

    if (init_groupchat_win(prompt, m, groupnum) == -1) {
        wprintw(window, "Group chat window failed to initialize.\n");
        tox_del_groupchat(m, groupnum);
        return;
    }
}

void cmd_savefile(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    if (argc != 1) {
      wprintw(window, "Invalid syntax.\n");
      return;
    }

    uint8_t filenum = atoi(argv[1]);

    if ((filenum == 0 && strcmp(argv[1], "0")) || filenum >= MAX_FILES) {
        wprintw(window, "No pending file transfers with that number.\n");
        return;
    }

    if (!friends[self->num].file_receiver.pending[filenum]) {
        wprintw(window, "No pending file transfers with that number.\n");
        return;
    }

    uint8_t *filename = friends[self->num].file_receiver.filenames[filenum];

    if (tox_file_send_control(m, self->num, 1, filenum, TOX_FILECONTROL_ACCEPT, 0, 0) == 0) {
        wprintw(window, "Accepted file transfer %u. Saving file as: '%s'\n", filenum, filename);

        if ((friends[self->num].file_receiver.files[filenum] = fopen(filename, "a")) == NULL) {
            wattron(window, COLOR_PAIR(RED));
            wprintw(window, "* Error writing to file.\n");
            wattroff(window, COLOR_PAIR(RED));
            tox_file_send_control(m, self->num, 1, filenum, TOX_FILECONTROL_KILL, 0, 0);
        }
    } else {
        wprintw(window, "File transfer failed.\n");
    }

    friends[self->num].file_receiver.pending[filenum] = false;
}

void cmd_sendfile(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    if (max_file_senders_index >= (MAX_FILES-1)) {
        wprintw(window,"Please wait for some of your outgoing file transfers to complete.\n");
        return;
    }

    if (argc < 1) {
      wprintw(window, "Invalid syntax.\n");
      return;
    }

    uint8_t *path = argv[1];

    if (path[0] != '\"') {
        wprintw(window, "File path must be enclosed in quotes.\n");
        return;
    }

    path[strlen(++path)-1] = L'\0';
    int path_len = strlen(path);

    if (path_len > MAX_STR_SIZE) {
        wprintw(window, "File path exceeds character limit.\n");
        return;
    }

    FILE *file_to_send = fopen(path, "r");

    if (file_to_send == NULL) {
        wprintw(window, "File '%s' not found.\n", path);
        return;
    }

    fseek(file_to_send, 0, SEEK_END);
    uint64_t filesize = ftell(file_to_send);
    fseek(file_to_send, 0, SEEK_SET);

    uint8_t filename[MAX_STR_SIZE];
    get_file_name(path, filename);
    int filenum = tox_new_file_sender(m, self->num, filesize, filename, strlen(filename) + 1);

    if (filenum == -1) {
        wprintw(window, "Error sending file.\n");
        return;
    }

    int i;

    for (i = 0; i < MAX_FILES; ++i) {
        if (!file_senders[i].active) {
            memcpy(file_senders[i].pathname, path, path_len + 1);
            file_senders[i].active = true;
            file_senders[i].toxwin = self;
            file_senders[i].file = file_to_send;
            file_senders[i].filenum = (uint8_t) filenum;
            file_senders[i].friendnum = self->num;
            file_senders[i].timestamp = get_unix_time();
            file_senders[i].piecelen = fread(file_senders[i].nextpiece, 1,
                                             tox_file_data_size(m, self->num), file_to_send);

            wprintw(window, "Sending file: '%s'\n", path);

            if (i == max_file_senders_index)
                ++max_file_senders_index;

            return;
        }
    } 
}

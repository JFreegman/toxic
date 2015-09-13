/*  execute.c
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
#include <assert.h>

#include "toxic.h"
#include "windows.h"
#include "execute.h"
#include "chat_commands.h"
#include "global_commands.h"
#include "group_commands.h"
#include "line_info.h"
#include "misc_tools.h"
#include "notify.h"

#define MAX_NUM_ARGS 10     /* Includes command */

struct cmd_func {
    const char *name;
    void (*func)(WINDOW *w, ToxWindow *, Tox *m, int argc, char (*argv)[MAX_STR_SIZE]);
};

static struct cmd_func global_commands[] = {
    { "/accept",    cmd_accept        },
    { "/add",       cmd_add           },
    { "/avatar",    cmd_avatar        },
    { "/clear",     cmd_clear         },
    { "/connect",   cmd_connect       },
    { "/decline",   cmd_decline       },
    { "/exit",      cmd_quit          },
    { "/group",     cmd_groupchat     },
    { "/help",      cmd_prompt_help   },
    { "/join",      cmd_join          },
    { "/log",       cmd_log           },
    { "/myid",      cmd_myid          },
    { "/nick",      cmd_nick          },
    { "/note",      cmd_note          },
    { "/q",         cmd_quit          },
    { "/quit",      cmd_quit          },
    { "/requests",  cmd_requests      },
    { "/status",    cmd_status        },
#ifdef AUDIO
    { "/lsdev",     cmd_list_devices  },
    { "/sdev",      cmd_change_device },
#endif /* AUDIO */
    { NULL,         NULL              },
};

static struct cmd_func chat_commands[] = {
    { "/cancel",    cmd_cancelfile  },
    { "/gaccept",   cmd_groupaccept },
    { "/invite",    cmd_groupinvite },
    { "/savefile",  cmd_savefile    },
    { "/sendfile",  cmd_sendfile    },
#ifdef AUDIO
    { "/call",      cmd_call        },
    { "/answer",    cmd_answer      },
    { "/reject",    cmd_reject      },
    { "/hangup",    cmd_hangup      },
    { "/mute",      cmd_mute        },
    { "/sense",     cmd_sense       },
#endif /* AUDIO */
    { NULL,         NULL            },
};

static struct cmd_func group_commands[] = {
    { "/ban",       cmd_ban            },
    { "/chatid",    cmd_chatid         },
    { "/ignore",    cmd_ignore         },
    { "/kick",      cmd_kick           },
    { "/mod",       cmd_mod            },
    { "/mykey",     cmd_mykey          },
    { "/passwd",    cmd_set_passwd     },
    { "/peerlimit", cmd_set_peerlimit  },
    { "/privacy",   cmd_set_privacy    },
    { "/rejoin",    cmd_rejoin         },
    { "/silence",   cmd_silence        },
    { "/topic",     cmd_set_topic      },
    { "/unban",     cmd_unban          },
    { "/unignore",  cmd_unignore       },
    { "/unmod",     cmd_unmod          },
    { "/unsilence", cmd_unsilence      },
#ifdef AUDIO
    { "/mute",      cmd_mute           },
    { "/sense",     cmd_sense          },
#endif /* AUDIO */
    { NULL,         NULL               },
};

#define NUM_SPECIAL_COMMANDS 14
static const char special_commands[NUM_SPECIAL_COMMANDS][MAX_CMDNAME_SIZE] = {
    "/ban",
    "/gaccept",
    "/group",
    "/ignore",
    "/kick",
    "/mod",
    "/nick",
    "/note",
    "/passwd",
    "/silence",
    "/topic",
    "/unignore",
    "/unmod",
    "/unsilence",
};

/* return true if input command is in the special_commands array. False otherwise.*/
static bool is_special_command(const char *input)
{
    int s = char_find(0, input, ' ');

    if (s == strlen(input))
        return false;

    int i;

    for (i = 0; i < NUM_SPECIAL_COMMANDS; ++i) {
        if (strncmp(input, special_commands[i], s) == 0)
            return true;
    }

    return false;
}

/* Parses commands in the special_commands array which take exactly one argument that may contain spaces.
 * Unlike parse_command, this function does not split the input string at spaces.
 *
 * Returns number of arguments on success
 * Returns -1 on failure
 */
static int parse_special_command(WINDOW *w, ToxWindow *self, const char *input, char (*args)[MAX_STR_SIZE])
{
    int len = strlen(input);
    int s = char_find(0, input, ' ');

    if (s + 1 >= len)
        return -1;

    memcpy(args[0], input, s);
    args[0][s++] = '\0';    /* increment to remove space after /command */
    memcpy(args[1], input + s, len - s);
    args[1][len - s] = '\0';

    return 2;
}

/* Parses input command and puts args (split by spaces) into args array.
 *
 * Returns number of arguments on success
 * Returns -1 on failure.
 */
static int parse_command(WINDOW *w, ToxWindow *self, const char *input, char (*args)[MAX_STR_SIZE])
{
    if (is_special_command(input))
        return parse_special_command(w, self, input, args);

    char *cmd = strdup(input);

    if (cmd == NULL)
        exit_toxic_err("failed in parse_command", FATALERR_MEMORY);

    int num_args = 0;
    int i = 0;    /* index of last char in an argument */

    /* characters wrapped in double quotes count as one arg */
    while (num_args < MAX_NUM_ARGS) {
        int qt_ofst = 0;    /* set to 1 to offset index for quote char at end of arg */

        if (*cmd == '\"') {
            qt_ofst = 1;
            i = char_find(1, cmd, '\"');

            if (cmd[i] == '\0') {
                const char *errmsg = "Invalid argument. Did you forget a closing \"?";
                line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, errmsg);
                free(cmd);
                return -1;
            }
        } else {
            i = char_find(0, cmd, ' ');
        }

        memcpy(args[num_args], cmd, i + qt_ofst);
        args[num_args++][i + qt_ofst] = '\0';

        if (cmd[i] == '\0')    /* no more args */
            break;

        char tmp[MAX_STR_SIZE];
        snprintf(tmp, sizeof(tmp), "%s", &cmd[i + 1]);
        strcpy(cmd, tmp);    /* tmp will always fit inside cmd */
    }

    /* Ugly special case concatinates all args after arg1 for multi-word group passwords */
    if (num_args > 2 && strcmp(args[0], "/join") == 0)
        strcpy(args[2], input + strlen(args[0]) + 1 + strlen(args[1]) + 1);

    free(cmd);
    return num_args;
}

/* Matches command to respective function.
 *
 * Returns 0 on match,
 * Returns -1 on no match
 */
static int do_command(WINDOW *w, ToxWindow *self, Tox *m, int num_args, struct cmd_func *commands,
                      char (*args)[MAX_STR_SIZE])
{
    int i;

    for (i = 0; commands[i].name != NULL; ++i) {
        if (strcmp(args[0], commands[i].name) == 0) {
            (commands[i].func)(w, self, m, num_args - 1, args);
            return 0;
        }
    }

    return -1;
}

void execute(WINDOW *w, ToxWindow *self, Tox *m, const char *input, int mode)
{
    if (string_is_empty(input))
        return;

    char args[MAX_NUM_ARGS][MAX_STR_SIZE];
    int num_args = parse_command(w, self, input, args);

    if (num_args == -1)
        return;

    /* Try to match input command to command functions. If non-global command mode is specified,
       try specified mode's commands first, then upon failure try global commands.

       Note: Global commands must come last in case of duplicate command names */
    switch (mode) {
        case CHAT_COMMAND_MODE:
            if (do_command(w, self, m, num_args, chat_commands, args) == 0)
                return;

            break;

        case GROUPCHAT_COMMAND_MODE:
            if (do_command(w, self, m, num_args, group_commands, args) == 0)
                return;

            break;
    }

    if (do_command(w, self, m, num_args, global_commands, args) == 0)
        return;

    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Invalid command.");
}

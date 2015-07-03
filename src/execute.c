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
    { "/invite",    cmd_groupinvite },
    { "/join",      cmd_join_group  },
    { "/savefile",  cmd_savefile    },
    { "/sendfile",  cmd_sendfile    },
#ifdef AUDIO
    { "/call",      cmd_call        },
    { "/answer",    cmd_answer      },
    { "/reject",    cmd_reject      },
    { "/hangup",    cmd_hangup      },
    { "/mute",      cmd_mute        },
    { "/sense",     cmd_sense       },
//#ifdef VIDEO
    //{ "/enablevid", cmd_enablevid   },
    //{ "/disablevid",cmd_disablevid  },
//#endif /* VIDEO */
#endif /* AUDIO */
    { NULL,         NULL            },
};

static struct cmd_func group_commands[] = {
    { "/title",     cmd_set_title   },

#ifdef AUDIO
    { "/mute",      cmd_mute        },
    { "/sense",     cmd_sense       },
#endif /* AUDIO */
    { NULL,         NULL            },
};

/* Parses input command and puts args into arg array.
   Returns number of arguments on success, -1 on failure. */
static int parse_command(WINDOW *w, ToxWindow *self, const char *input, char (*args)[MAX_STR_SIZE])
{
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

    free(cmd);
    return num_args;
}

/* Matches command to respective function. Returns 0 on match, 1 on no match */
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

    return 1;
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

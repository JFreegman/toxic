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

#include "toxic_windows.h"
#include "execute.h"
#include "chat_commands.h"
#include "global_commands.h"

struct cmd_func {
    const char *name;
    void (*func)(WINDOW *w, ToxWindow *, Tox *m, int argc, char (*argv)[MAX_STR_SIZE]);
};

static struct cmd_func global_commands[] = {
    { "/accept",    cmd_accept        },
    { "/add",       cmd_add           },
    { "/clear",     cmd_clear         },
    { "/connect",   cmd_connect       },
    { "/exit",      cmd_quit          },
    { "/groupchat", cmd_groupchat     },
    { "/help",      cmd_prompt_help   },
    { "/log",       cmd_log           },
    { "/myid",      cmd_myid          },
    { "/nick",      cmd_nick          },
    { "/note",      cmd_note          },
    { "/q",         cmd_quit          },
    { "/quit",      cmd_quit          },
    { "/status",    cmd_status        },
#ifdef _SUPPORT_AUDIO
    { "/lsdev",     cmd_list_devices  },
    { "/sdev",      cmd_change_device },
#endif /* _SUPPORT_AUDIO */
};

static struct cmd_func chat_commands[] = {
    { "/help",      cmd_chat_help   },
    { "/invite",    cmd_groupinvite },
    { "/join",      cmd_join_group  },
    { "/savefile",  cmd_savefile    },
    { "/sendfile",  cmd_sendfile    },
    
#ifdef _SUPPORT_AUDIO
    { "/call",      cmd_call        },
    { "/cancel",    cmd_cancel      },
    { "/answer",    cmd_answer      },
    { "/hangup",    cmd_hangup      },
#endif /* _SUPPORT_AUDIO */
};

/* Parses input command and puts args into arg array. 
   Returns number of arguments on success, -1 on failure. */
static int parse_command(WINDOW *w, char *cmd, char (*args)[MAX_STR_SIZE])
{
    int num_args = 0;
    bool cmd_end = false;    // flags when we get to the end of cmd
    char *end;               // points to the end of the current arg

    /* characters wrapped in double quotes count as one arg */
    while (!cmd_end && num_args < MAX_NUM_ARGS) {
        if (*cmd == '\"') {
            end = strchr(cmd+1, '\"');

            if (end++ == NULL) {    /* Increment past the end quote */
                wprintw(w, "Invalid argument. Did you forget a closing \"?\n");
                return -1;
            }

            cmd_end = *end == '\0';
        } else {
            end = strchr(cmd, ' ');
            cmd_end = end == NULL;
        }

        if (!cmd_end)
            *end++ = '\0';    /* mark end of current argument */

        /* Copy from start of current arg to where we just inserted the null byte */
        strcpy(args[num_args++], cmd);
        cmd = end;
    }

    return num_args;
}

/* Matches command to respective function. Returns 0 on match, 1 on no match */
static int do_command(WINDOW *w, ToxWindow *self, Tox *m, int num_args, int num_cmds,
                      struct cmd_func *commands, char (*args)[MAX_STR_SIZE])
{
    int i;

    for (i = 0; i < num_cmds; ++i) {
        if (strcmp(args[0], commands[i].name) == 0) {
            (commands[i].func)(w, self, m, num_args-1, args);
            return 0;
        }
    }

    return 1;
}

void execute(WINDOW* w, ToxWindow *self, Tox *m, char *cmd, int mode)
{
    if (string_is_empty(cmd))
        return;

    char args[MAX_NUM_ARGS][MAX_STR_SIZE] = {0};
    int num_args = parse_command(w, cmd, args);

    if (num_args == -1)
        return;

    /* Try to match input command to command functions. If non-global command mode is specified, 
       try specified mode's commands first, then upon failure try global commands. 

       Note: Global commands must come last in case of duplicate command names */
    switch (mode) {
    case CHAT_COMMAND_MODE:
        if (do_command(w, self, m, num_args, CHAT_NUM_COMMANDS, chat_commands, args) == 0)
            return;
        break;

    case GROUPCHAT_COMMAND_MODE:
        break;
    }

    if (do_command(w, self, m, num_args, GLOBAL_NUM_COMMANDS, global_commands, args) == 0)
        return;

    wprintw(w, "Invalid command.\n");
}

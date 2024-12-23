/*  execute.c
 *
 *  Copyright (C) 2014-2024 Toxic All Rights Reserved.
 *
 *  This file is part of Toxic. Toxic is free software licensed
 *  under the GNU General Public License 3.0.
 */

#include <stdlib.h>
#include <string.h>

#include "api.h"
#include "chat_commands.h"
#include "execute.h"
#include "global_commands.h"
#include "conference_commands.h"
#include "groupchat_commands.h"
#include "line_info.h"
#include "misc_tools.h"
#include "notify.h"
#include "toxic.h"
#include "windows.h"

struct cmd_func {
    const char *const name;
    void (*func)(WINDOW *w, ToxWindow *, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE]);
};

static struct cmd_func global_commands[] = {
    { "/accept",    cmd_accept        },
    { "/add",       cmd_add           },
    { "/avatar",    cmd_avatar        },
    { "/clear",     cmd_clear         },
    { "/color",     cmd_color         },
    { "/connect",   cmd_connect       },
    { "/decline",   cmd_decline       },
    { "/exit",      cmd_quit          },
    { "/conference", cmd_conference    },
    { "/group",     cmd_groupchat     },
#ifdef GAMES
    { "/game",      cmd_game          },
#endif
    { "/help",      cmd_prompt_help   },
    { "/join",      cmd_join          },
    { "/log",       cmd_log           },
    { "/myid",      cmd_myid          },
#ifdef QRCODE
    { "/myqr",      cmd_myqr          },
#endif /* QRCODE */
    { "/nick",      cmd_nick          },
    { "/note",      cmd_note          },
    { "/nospam",    cmd_nospam        },
    { "/q",         cmd_quit          },
    { "/quit",      cmd_quit          },
    { "/requests",  cmd_requests      },
    { "/status",    cmd_status        },
#ifdef AUDIO
    { "/lsdev",     cmd_list_devices  },
    { "/sdev",      cmd_change_device },
#endif /* AUDIO */
#ifdef VIDEO
    { "/lsvdev",    cmd_list_video_devices  },
    { "/svdev",     cmd_change_video_device },
#endif /* VIDEO */
#ifdef PYTHON
    { "/run",       cmd_run           },
#endif /* PYTHON */
    { NULL,         NULL              },
};

static struct cmd_func chat_commands[] = {
    { "/autoaccept", cmd_autoaccept_files  },
    { "/cancel",    cmd_cancelfile        },
    { "/cinvite",   cmd_invite_to_conference },
    { "/cjoin",     cmd_conference_join   },
    { "/gaccept",   cmd_group_accept      },
    { "/invite",    cmd_invite_to_group   },
#ifdef GAMES
    { "/play",      cmd_game_join         },
#endif
    { "/savefile",  cmd_savefile          },
    { "/sendfile",  cmd_sendfile          },
#ifdef AUDIO
    { "/call",      cmd_call              },
    { "/answer",    cmd_answer            },
    { "/reject",    cmd_reject            },
    { "/hangup",    cmd_hangup            },
    { "/mute",      cmd_mute              },
    { "/sense",     cmd_sense             },
    { "/bitrate",   cmd_bitrate           },
#endif /* AUDIO */
#ifdef VIDEO
    { "/vcall",     cmd_vcall             },
    { "/video",     cmd_video             },
    { "/res",       cmd_res               },
#endif /* VIDEO */
    { NULL,         NULL                  },
};

static struct cmd_func conference_commands[] = {
    { "/chatid",    cmd_conference_chatid    },
    { "/cinvite",   cmd_conference_invite    },
    { "/title",     cmd_conference_set_title },

#ifdef AUDIO
    { "/audio",     cmd_enable_audio },
    { "/mute",      cmd_conference_mute   },
    { "/ptt",       cmd_conference_push_to_talk },
    { "/sense",     cmd_conference_sense  },
#endif /* AUDIO */
    { NULL,         NULL             },
};

static struct cmd_func groupchat_commands[] = {
    { "/chatid",    cmd_chatid         },
    { "/disconnect", cmd_disconnect    },
    { "/ignore",    cmd_ignore         },
    { "/invite",    cmd_group_invite   },
    { "/kick",      cmd_kick           },
    { "/list",      cmd_list           },
    { "/locktopic", cmd_set_topic_lock },
    { "/mod",       cmd_mod            },
    { "/nick",      cmd_group_nick     },
    { "/passwd",    cmd_set_passwd     },
    { "/peerlimit", cmd_set_peerlimit  },
    { "/privacy",   cmd_set_privacy    },
    { "/rejoin",    cmd_rejoin         },
    { "/silence",   cmd_silence        },
    { "/topic",     cmd_set_topic      },
    { "/unignore",  cmd_unignore       },
    { "/unmod",     cmd_unmod          },
    { "/unsilence", cmd_unsilence      },
    { "/voice",     cmd_set_voice      },
    { "/whois",     cmd_whois          },
    { NULL,         NULL               },
};

/* Special commands are commands that only take one argument even if it contains spaces */
static const char special_commands[][MAX_CMDNAME_SIZE] = {
    "/add",
    "/avatar",
    "/cinvite",
    "/gaccept",
    "/group",
    "/ignore",
    "/invite",
    "/kick",
    "/mod",
    "/nick",
    "/note",
    "/passwd",
    "/rejoin",
    "/silence",
    "/topic",
    "/unignore",
    "/unmod",
    "/unsilence",
    "/whois",
#ifdef PYTHON
    "/run",
#endif /* PYTHON */
    "/sendfile",
    "/title",
    "/mute",
    "",
};

/* Returns true if input command is in the special_commands array. */
static bool is_special_command(const char *input)
{
    const int s = char_find(0, input, ' ');

    for (int i = 0; special_commands[i][0] != '\0'; ++i) {
        if (strncmp(input, special_commands[i], s) == 0) {
            return true;
        }
    }

    return false;
}

/* Parses commands in the special_commands array. Unlike parse_command, this function
 * does not split the input string at spaces.
 *
 * Returns the number of arguments.
 */
static int parse_special_command(const char *input, char (*args)[MAX_STR_SIZE])
{
    int len = strlen(input);
    int s = char_find(0, input, ' ');

    memcpy(args[0], input, s);
    args[0][s++] = '\0';    // increment to remove space after "/command "

    if (s >= len) {
        return 1;  // No additional args
    }

    memcpy(args[1], input + s, len - s);
    args[1][len - s] = '\0';

    return 2;
}

/* Parses input command and puts args into arg array.
 *
 * Returns the number of arguments.
 */
static int parse_command(const char *input, char (*args)[MAX_STR_SIZE])
{
    if (is_special_command(input)) {
        return parse_special_command(input, args);
    }

    char *cmd = strdup(input);

    if (cmd == NULL) {
        return -1;
    }

    int num_args = 0;

    /* characters wrapped in double quotes count as one arg */
    while (num_args < MAX_NUM_ARGS) {
        int i = char_find(0, cmd, ' ');    // index of last char in an argument
        memcpy(args[num_args], cmd, i);
        args[num_args++][i] = '\0';

        if (cmd[i] == '\0') {  // no more args
            break;
        }

        char tmp[MAX_STR_SIZE];
        snprintf(tmp, sizeof(tmp), "%s", &cmd[i + 1]);
        strcpy(cmd, tmp);    // tmp will always fit inside cmd
    }

    free(cmd);
    return num_args;
}

/* Matches command to respective function.
 *
 * Returns 0 on match.
 * Returns 1 on no match
 */
static int do_command(WINDOW *w, ToxWindow *self, Toxic *toxic, int num_args, struct cmd_func *commands,
                      char (*args)[MAX_STR_SIZE])
{
    for (size_t i = 0; commands[i].name != NULL; ++i) {
        if (strcmp(args[0], commands[i].name) == 0) {
            (commands[i].func)(w, self, toxic, num_args - 1, args);
            return 0;
        }
    }

    return 1;
}

void execute(WINDOW *w, ToxWindow *self, Toxic *toxic, const char *input, int mode)
{
    if (string_is_empty(input)) {
        return;
    }

    char args[MAX_NUM_ARGS][MAX_STR_SIZE];
    const int num_args = parse_command(input, args);

    if (num_args <= 0) {
        fprintf(stderr, "Warning: parse_command(%s, %d) failed in execute()\n", input, mode);
        return;
    }

    /* Try to match input command to command functions. If non-global command mode is specified,
     * try specified mode's commands first, then upon failure try global commands.
     *
     * Note: Global commands must come last in case of duplicate command names
     */
    switch (mode) {
        case CHAT_COMMAND_MODE: {
            if (do_command(w, self, toxic, num_args, chat_commands, args) == 0) {
                return;
            }

            break;
        }

        case CONFERENCE_COMMAND_MODE: {
            if (do_command(w, self, toxic, num_args, conference_commands, args) == 0) {
                return;
            }

            break;
        }

        case GROUPCHAT_COMMAND_MODE: {
            if (do_command(w, self, toxic, num_args, groupchat_commands, args) == 0) {
                return;
            }

            break;
        }
    }

    if (do_command(w, self, toxic, num_args, global_commands, args) == 0) {
        return;
    }

#ifdef PYTHON

    if (do_plugin_command(num_args, args) == 0) {
        return;
    }

#endif

    line_info_add(self, toxic->c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Invalid command.");
}

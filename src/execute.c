/*
 * Toxic -- Tox Curses Client
 */

#include <stdlib.h>
#include <string.h>

#include "toxic_windows.h"
#include "global_commands.h"
#include "chat_commands.h"
#include "execute.h"

static int parse_command(WINDOW *window, char *cmd, char (*args)[MAX_STR_SIZE])
{
    int num_args = 0;
    bool cmd_end = false;    // flags when we get to the end of cmd
    char *end;               // points to the end of the current arg

    /* Put arguments into args array (characters wrapped in double quotes count as one arg) */
    while (!cmd_end && num_args < MAX_NUM_ARGS) {
        if (*cmd == '\"') {
            end = strchr(cmd+1, '\"');

            if (end++ == NULL) {    /* Increment past the end quote */
                wprintw(window, "Invalid argument. Did you forget a closing \"?\n");
                return;
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

void execute(WINDOW *window, ToxWindow *prompt, Tox *m, char *cmd, int mode)
{
    if (string_is_empty(cmd))
        return;

    char args[MAX_NUM_ARGS][MAX_STR_SIZE] = {0};
    int num_args = parse_command(window, cmd, args);

    /* Attempt to match input to command functions. If non-global command mode is specified, 
       try the specified mode's commands first, then upon failure try global commands.

       TODO: Generalize command matching loop in a separate function */
    int i;

    switch(mode) {
    case CHAT_COMMAND_MODE:
        for (i = 0; i < CHAT_NUM_COMMANDS; ++i) {
            if (strcmp(args[0], chat_commands[i].name) == 0) {
                (chat_commands[i].func)(window, prompt, m, num_args-1, args);
                return;
            }
        }
        break;

    case GROUPCHAT_COMMAND_MODE:
        break;
    }

    for (i = 0; i < GLOBAL_NUM_COMMANDS; ++i) {
        if (strcmp(args[0], global_commands[i].name) == 0) {
            (global_commands[i].func)(window, prompt, m, num_args-1, args);
            return;
        }
    }

    wprintw(window, "Invalid command.\n");
}

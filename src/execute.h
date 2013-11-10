/*
 * Toxic -- Tox Curses Client
 */

#define MAX_NUM_ARGS 4     /* Includes command */
#define GLOBAL_COMMAND_MODE 0
#define CHAT_COMMAND_MODE 1
#define GROUPCHAT_COMMAND_MODE 2

void execute(WINDOW *window, ToxWindow *prompt, Tox *m, char *cmd, int mode);
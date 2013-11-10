/*
 * Toxic -- Tox Curses Client
 */

#define MAX_NUM_ARGS 4     /* Includes command */

enum {
    GLOBAL_COMMAND_MODE,
    CHAT_COMMAND_MODE,
    GROUPCHAT_COMMAND_MODE,
};

void execute(WINDOW *window, ToxWindow *prompt, Tox *m, char *cmd, int mode);
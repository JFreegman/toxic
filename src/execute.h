/*
 * Toxic -- Tox Curses Client
 */

#define MAX_NUM_ARGS 4     /* Includes command */
#define GLOBAL_NUM_COMMANDS 13

#ifdef _SUPPORT_AUDIO 
#define CHAT_NUM_COMMANDS 9
#else 
#define CHAT_NUM_COMMANDS 5
#endif /* _SUPPORT_AUDIO */ 

enum {
    GLOBAL_COMMAND_MODE,
    CHAT_COMMAND_MODE,
    GROUPCHAT_COMMAND_MODE,
};

void execute(WINDOW *w, ToxWindow *self, Tox *m, char *cmd, int mode);

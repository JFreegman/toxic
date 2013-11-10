/*
 * Toxic -- Tox Curses Client
 */

/* commands */
void cmd_accept(WINDOW *, ToxWindow *, Tox *m, int, char (*argv)[MAX_STR_SIZE]);
void cmd_add(WINDOW *, ToxWindow *, Tox *m, int, char (*argv)[MAX_STR_SIZE]);
void cmd_clear(WINDOW *, ToxWindow *, Tox *m, int, char (*argv)[MAX_STR_SIZE]);
void cmd_connect(WINDOW *, ToxWindow *, Tox *m, int, char (*argv)[MAX_STR_SIZE]);
void cmd_groupchat(WINDOW *, ToxWindow *, Tox *m, int, char (*argv)[MAX_STR_SIZE]);
void cmd_myid(WINDOW *, ToxWindow *, Tox *m, int, char (*argv)[MAX_STR_SIZE]);
void cmd_nick(WINDOW *, ToxWindow *, Tox *m, int, char (*argv)[MAX_STR_SIZE]);
void cmd_note(WINDOW *, ToxWindow *, Tox *m, int, char (*argv)[MAX_STR_SIZE]);
void cmd_quit(WINDOW *, ToxWindow *, Tox *m, int, char (*argv)[MAX_STR_SIZE]);
void cmd_status(WINDOW *, ToxWindow *, Tox *m, int, char (*argv)[MAX_STR_SIZE]);

#define GLOBAL_NUM_COMMANDS 12

static struct {
    char *name;
    void (*func)(WINDOW *, ToxWindow *, Tox *m, int argc, char (*argv)[MAX_STR_SIZE]);
} global_commands[] = {
    { "/accept",    cmd_accept    },
    { "/add",       cmd_add       },
    { "/clear",     cmd_clear     },
    { "/connect",   cmd_connect   },
    { "/exit",      cmd_quit      },
    { "/groupchat", cmd_groupchat },
    { "/myid",      cmd_myid      },
    { "/nick",      cmd_nick      },
    { "/note",      cmd_note      },
    { "/q",         cmd_quit      },
    { "/quit",      cmd_quit      },
    { "/status",    cmd_status    },
};

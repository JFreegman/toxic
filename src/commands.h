/*
 * Toxic -- Tox Curses Client
 */

/* commands */
void cmd_accept(WINDOW *, ToxWindow *, Tox *m, int, char **);
void cmd_add(WINDOW *, ToxWindow *, Tox *m, int, char **);
void cmd_clear(WINDOW *, ToxWindow *, Tox *m, int, char **);
void cmd_connect(WINDOW *, ToxWindow *, Tox *m, int, char **);
void cmd_groupchat(WINDOW *, ToxWindow *, Tox *m, int, char **);
void cmd_join(WINDOW *, ToxWindow *, Tox *m, int, char **);
void cmd_msg(WINDOW *, ToxWindow *, Tox *m, int, char **);
void cmd_myid(WINDOW *, ToxWindow *, Tox *m, int, char **);
void cmd_nick(WINDOW *, ToxWindow *, Tox *m, int, char **);
void cmd_note(WINDOW *, ToxWindow *, Tox *m, int, char **);
void cmd_quit(WINDOW *, ToxWindow *, Tox *m, int, char **);
void cmd_status(WINDOW *, ToxWindow *, Tox *m, int, char **);

void execute(WINDOW *window, ToxWindow *prompt, Tox *m, char *u_cmd, int buf_len);

#define NUM_COMMANDS 14

static struct {
    char *name;
    void (*func)(WINDOW *, ToxWindow *, Tox *m, int, char **);
} commands[] = {
    { "/accept",    cmd_accept    },
    { "/add",       cmd_add       },
    { "/clear",     cmd_clear     },
    { "/connect",   cmd_connect   },
    { "/exit",      cmd_quit      },
    { "/groupchat", cmd_groupchat },
    { "/join",      cmd_join      },
    { "/msg",       cmd_msg       },
    { "/myid",      cmd_myid      },
    { "/nick",      cmd_nick      },
    { "/note",      cmd_note      },
    { "/q",         cmd_quit      },
    { "/quit",      cmd_quit      },
    { "/status",    cmd_status    },
};

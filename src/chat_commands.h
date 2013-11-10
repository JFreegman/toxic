/*
 * Toxic -- Tox Curses Client
 */

/* commands */
void cmd_help(WINDOW *window, ToxWindow *chatwin, Tox *m, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_groupinvite(WINDOW *window, ToxWindow *chatwin, Tox *m, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_join_group(WINDOW *window, ToxWindow *chatwin, Tox *m, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_savefile(WINDOW *window, ToxWindow *chatwin, Tox *m, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_sendfile(WINDOW *window, ToxWindow *chatwin, Tox *m, int argc, char (*argv)[MAX_STR_SIZE]);

#define CHAT_NUM_COMMANDS 5

static struct {
    char *name;
    void (*func)(WINDOW *, ToxWindow *, Tox *m, int argc, char (*argv)[MAX_STR_SIZE]);
} chat_commands[] = {
    { "/help",      cmd_help        },
    { "/invite",    cmd_groupinvite },
    { "/join",      cmd_join_group  },
    { "/savefile",  cmd_savefile    },
    { "/sendfile",  cmd_sendfile    },
};

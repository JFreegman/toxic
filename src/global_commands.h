/*
 * Toxic -- Tox Curses Client
 */

void cmd_accept(WINDOW *, ToxWindow *, Tox *m, int num, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_add(WINDOW *, ToxWindow *, Tox *m, int num, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_clear(WINDOW *, ToxWindow *, Tox *m, int num, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_connect(WINDOW *, ToxWindow *, Tox *m, int num, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_groupchat(WINDOW *, ToxWindow *, Tox *m, int num, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_myid(WINDOW *, ToxWindow *, Tox *m, int num, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_nick(WINDOW *, ToxWindow *, Tox *m, int num, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_note(WINDOW *, ToxWindow *, Tox *m, int num, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_prompt_help(WINDOW *window, ToxWindow *prompt, Tox *m, int num, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_quit(WINDOW *, ToxWindow *, Tox *m, int num, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_status(WINDOW *, ToxWindow *, Tox *m, int num, int argc, char (*argv)[MAX_STR_SIZE]);

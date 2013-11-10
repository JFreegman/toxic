/*
 * Toxic -- Tox Curses Client
 */

void cmd_chat_help(WINDOW *w, ToxWindow *chatwin, Tox *m, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_groupinvite(WINDOW *w, ToxWindow *chatwin, Tox *m, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_join_group(WINDOW *w, ToxWindow *chatwin, Tox *m, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_savefile(WINDOW *w, ToxWindow *chatwin, Tox *m, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_sendfile(WINDOW *w, ToxWindow *chatwin, Tox *m, int argc, char (*argv)[MAX_STR_SIZE]);

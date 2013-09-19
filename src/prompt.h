#ifndef PROMPT_H_UZYGWFFL
#define PROMPT_H_UZYGWFFL

#include "toxic_windows.h"

ToxWindow new_prompt();
int add_req(uint8_t *public_key);
unsigned char *hex_string_to_bin(char hex_string[]);
void prompt_init_statusbar(ToxWindow *self, Tox *m);
void prompt_update_nick(ToxWindow *prompt, uint8_t *nick, uint16_t len);
void prompt_update_statusmessage(ToxWindow *prompt, uint8_t *statusmsg, uint16_t len);
void prompt_update_status(ToxWindow *prompt, TOX_USERSTATUS status);
void prompt_update_connectionstatus(ToxWindow *prompt, bool is_connected);

/* commands */
void cmd_accept(WINDOW *, ToxWindow *, Tox *m, int, char **);
void cmd_add(WINDOW *, ToxWindow *, Tox *m, int, char **);
void cmd_clear(WINDOW *, ToxWindow *, Tox *m, int, char **);
void cmd_connect(WINDOW *, ToxWindow *, Tox *m, int, char **);
void cmd_groupchat(WINDOW *, ToxWindow *, Tox *m, int, char **);
void cmd_help(WINDOW *, ToxWindow *, Tox *m, int, char **);
void cmd_invite(WINDOW *, ToxWindow *, Tox *m, int, char **);
void cmd_join(WINDOW *, ToxWindow *, Tox *m, int, char **);
void cmd_msg(WINDOW *, ToxWindow *, Tox *m, int, char **);
void cmd_myid(WINDOW *, ToxWindow *, Tox *m, int, char **);
void cmd_nick(WINDOW *, ToxWindow *, Tox *m, int, char **);
void cmd_quit(WINDOW *, ToxWindow *, Tox *m, int, char **);
void cmd_status(WINDOW *, ToxWindow *, Tox *m, int, char **);
void cmd_note(WINDOW *, ToxWindow *, Tox *m, int, char **);

void execute(WINDOW *window, ToxWindow *prompt, Tox *m, char *u_cmd, int buf_len);

#endif /* end of include guard: PROMPT_H_UZYGWFFL */

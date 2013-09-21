/*
 * Toxic -- Tox Curses Client
 */

#ifndef PROMPT_H_UZYGWFFL
#define PROMPT_H_UZYGWFFL

ToxWindow new_prompt(void);
void prompt_init_statusbar(ToxWindow *self, Tox *m);
void prompt_update_nick(ToxWindow *prompt, uint8_t *nick, uint16_t len);
void prompt_update_statusmessage(ToxWindow *prompt, uint8_t *statusmsg, uint16_t len);
void prompt_update_status(ToxWindow *prompt, TOX_USERSTATUS status);
void prompt_update_connectionstatus(ToxWindow *prompt, bool is_connected);

#endif /* end of include guard: PROMPT_H_UZYGWFFL */

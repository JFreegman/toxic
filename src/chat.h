#ifndef CHAT_H_6489PZ13
#define CHAT_H_6489PZ13

struct tm *get_time(void);
char *wc_to_char(wchar_t ch);
uint8_t *wcs_to_char(wchar_t *string);
int string_is_empty(char *string);
ToxWindow new_chat(Tox *m, ToxWindow *prompt, int friendnum);

#endif /* end of include guard: CHAT_H_6489PZ13 */

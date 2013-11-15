#ifndef FRIENDLIST_H_53I41IM
#define FRIENDLIST_H_53I41IM

#include "toxic_windows.h"
#include "chat.h"

ToxWindow new_friendlist();
void disable_chatwin(int f_num);
int get_friendnum(uint8_t *name);

#endif /* end of include guard: FRIENDLIST_H_53I41IM */

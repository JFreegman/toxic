#ifndef FRIENDLIST_H_53I41IM
#define FRIENDLIST_H_53I41IM

#include "toxic_windows.h"

ToxWindow new_friendlist(void);
void disable_chatwin(int f_num);
int get_friendnum(uint8_t *name);
void sort_friendlist_index(void);

#endif /* end of include guard: FRIENDLIST_H_53I41IM */

#ifndef FRIENDLIST_H_53I41IM
#define FRIENDLIST_H_53I41IM

#include "toxic_windows.h"
#include "chat.h"

ToxWindow new_friendlist();
int friendlist_onFriendAdded(Messenger *m, int num);
void disable_chatwin(int f_num);

#endif /* end of include guard: FRIENDLIST_H_53I41IM */

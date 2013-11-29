#ifndef FRIENDLIST_H_53I41IM
#define FRIENDLIST_H_53I41IM

#include "toxic_windows.h"

typedef struct {
    uint8_t name[TOX_MAX_NAME_LENGTH];
    uint16_t namelength;
    uint8_t statusmsg[TOX_MAX_STATUSMESSAGE_LENGTH];
    uint16_t statusmsg_len;
    uint8_t pending_groupchat[TOX_CLIENT_ID_SIZE];
    int num;
    int chatwin;
    bool active;
    bool online;
    TOX_USERSTATUS status;
    struct FileReceiver file_receiver;
} ToxicFriend;

ToxWindow new_friendlist(void);
void disable_chatwin(int f_num);
int get_friendnum(uint8_t *name);

/* sorts friendlist_index first by connection status then alphabetically */
void sort_friendlist_index(void);

#endif /* end of include guard: FRIENDLIST_H_53I41IM */

/*
 * Toxic -- Tox Curses Client
 */

#define SIDEBAR_WIDTH 16

/* If this limit is reached the chat will still work 
   but the side panel and channel updates will be frozen.

   TODO: Make this not necessary */
#define MAX_GROUP_PEERS 1000    

typedef struct {
    int chatwin;
    bool active;
    int num_peers;
    uint8_t peer_names[MAX_GROUP_PEERS][TOX_MAX_NAME_LENGTH];
    uint8_t oldpeer_names[MAX_GROUP_PEERS][TOX_MAX_NAME_LENGTH];
} GroupChat;

int init_groupchat_win(ToxWindow *prompt, Tox *m, int groupnum);

/*
 * Toxic -- Tox Curses Client
 */

#define SIDEBAR_WIDTH 16
#define CHATBOX_HEIGHT 4

/* Limits # of peers in sidepanel (make this go away) */
#define MAX_GROUP_PEERS 100    

typedef struct {
    int chatwin;
    bool active;
    int num_peers;
    uint8_t peer_names[MAX_GROUP_PEERS][TOX_MAX_NAME_LENGTH];
} GroupChat;

int init_groupchat_win(ToxWindow *prompt, Tox *m, int groupnum);

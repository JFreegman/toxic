/*
 * Toxic -- Tox Curses Client
 */

/* The sidebar will take up y/n of the window width where x is the full width of the window */
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

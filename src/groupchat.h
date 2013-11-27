/*
 * Toxic -- Tox Curses Client
 */

#define SIDEBAR_WIDTH 16

typedef struct {
    int chatwin;
    bool active;
    int num_peers;
    uint8_t *peer_names;
    uint8_t *oldpeer_names;
} GroupChat;

int init_groupchat_win(ToxWindow *prompt, Tox *m, int groupnum);

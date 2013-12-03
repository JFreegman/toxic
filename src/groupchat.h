/*
 * Toxic -- Tox Curses Client
 */

#define SIDEBAR_WIDTH 16
#define SDBAR_OFST 2    /* Offset for the peer number box at the top of the statusbar */

typedef struct {
    int chatwin;
    bool active;
    int num_peers;
    int side_pos;    /* current position of the sidebar - used for scrolling up and down */
    uint8_t *peer_names;
    uint8_t *oldpeer_names;
} GroupChat;

int init_groupchat_win(ToxWindow *prompt, Tox *m, int groupnum);
ToxWindow new_group_chat(Tox *m, int groupnum);

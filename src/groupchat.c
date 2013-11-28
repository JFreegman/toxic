/*
 * Toxic -- Tox Curses Client
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "toxic_windows.h"
#include "execute.h"
#include "misc_tools.h"
#include "groupchat.h"

static GroupChat groupchats[MAX_GROUPCHAT_NUM];
static int max_groupchat_index = 0;
int num_groupchats = 0;

ToxWindow new_group_chat(Tox *m, int groupnum);

extern char *DATA_FILE;
extern int store_data(Tox *m, char *path);

int init_groupchat_win(ToxWindow *prompt, Tox *m, int groupnum)
{
    int i;

    for (i = 0; i <= max_groupchat_index; ++i) {
        if (!groupchats[i].active) {
            groupchats[i].chatwin = add_window(m, new_group_chat(m, groupnum));
            groupchats[i].active = true;
            groupchats[i].num_peers = 0;
            groupchats[i].peer_names = malloc(sizeof(uint8_t) * TOX_MAX_NAME_LENGTH);
            groupchats[i].oldpeer_names = malloc(sizeof(uint8_t) * TOX_MAX_NAME_LENGTH);
            memset(groupchats[i].peer_names, 0, sizeof(groupchats[i].peer_names));
            memset(groupchats[i].oldpeer_names, 0, sizeof(groupchats[i].oldpeer_names));

            set_active_window(groupchats[i].chatwin);

            if (i == max_groupchat_index)
                ++max_groupchat_index;

            ++num_groupchats;

            return 0;
        }
    }

    return -1;
}

static void close_groupchatwin(Tox *m, int groupnum)
{
    tox_del_groupchat(m, groupnum);

    free(groupchats[groupnum].peer_names);
    free(groupchats[groupnum].oldpeer_names);
    memset(&groupchats[groupnum], 0, sizeof(GroupChat));

    int i;

    for (i = max_groupchat_index; i > 0; --i) {
        if (groupchats[i-1].active)
            break;
    }

    --num_groupchats;
    max_groupchat_index = i;
}

static void print_groupchat_help(ChatContext *ctx)
{
    wattron(ctx->history, COLOR_PAIR(CYAN) | A_BOLD);
    wprintw(ctx->history, "Group chat commands:\n");
    wattroff(ctx->history, A_BOLD);

    wprintw(ctx->history, "      /add <id> <msg>     : Add friend with optional message\n");
    wprintw(ctx->history, "      /status <type> <msg>: Set your status with optional note\n");
    wprintw(ctx->history, "      /note  <msg>        : Set a personal note\n");
    wprintw(ctx->history, "      /nick <nick>        : Set your nickname\n");
    wprintw(ctx->history, "      /groupchat          : Create a group chat\n");
    wprintw(ctx->history, "      /myid               : Print your ID\n");
    wprintw(ctx->history, "      /clear              : Clear the screen\n");
    wprintw(ctx->history, "      /close              : Close the current group chat\n");
    wprintw(ctx->history, "      /quit or /exit      : Exit Toxic\n");
    wprintw(ctx->history, "      /help               : Print this message again\n");
    
    wattron(ctx->history, A_BOLD);
    wprintw(ctx->history, "\n * Argument messages must be enclosed in quotation marks.\n");
    wattroff(ctx->history, A_BOLD);
    
    wattroff(ctx->history, COLOR_PAIR(CYAN));
}

static void groupchat_onGroupMessage(ToxWindow *self, Tox *m, int groupnum, int peernum,
                                     uint8_t *msg, uint16_t len)
{
    if (self->num != groupnum)
        return;

    ChatContext *ctx = (ChatContext *) self->chatwin;

    uint8_t nick[TOX_MAX_NAME_LENGTH] = {'\0'};
    tox_group_peername(m, groupnum, peernum, nick);
    nick[TOXIC_MAX_NAME_LENGTH] = '\0';    /* enforce client max name length */

    print_time(ctx->history);
    wattron(ctx->history, COLOR_PAIR(BLUE));
    wprintw(ctx->history, "%s: ", nick);
    wattroff(ctx->history, COLOR_PAIR(BLUE));
    
    if (msg[0] == '>') {
        wattron(ctx->history, COLOR_PAIR(GREEN));
        wprintw(ctx->history, "%s\n", msg);
        wattroff(ctx->history, COLOR_PAIR(GREEN));
    } else
        wprintw(ctx->history, "%s\n", msg);

    self->blink = true;
}

/* Puts two copies of peerlist in chat instance */
static void copy_peernames(int gnum, int npeers, uint8_t tmp_peerlist[][TOX_MAX_NAME_LENGTH])
{
    /* Assumes these are initiated in init_groupchat_win */
    free(groupchats[gnum].peer_names);
    free(groupchats[gnum].oldpeer_names);

    int N = TOX_MAX_NAME_LENGTH;

    groupchats[gnum].peer_names = malloc(sizeof(uint8_t) * npeers * N);
    groupchats[gnum].oldpeer_names = malloc(sizeof(uint8_t) * npeers * N);

    if (groupchats[gnum].peer_names == NULL || groupchats[gnum].oldpeer_names == NULL) {
        endwin();
        fprintf(stderr, "malloc() failed. Aborting...\n");
        exit(EXIT_FAILURE);
    }

    int i;

    for (i = 0; i < npeers; ++i) {
        if (string_is_empty(tmp_peerlist[i]))
            memcpy(&groupchats[gnum].peer_names[i*N], UNKNOWN_NAME, sizeof(UNKNOWN_NAME));
        else
            memcpy(&groupchats[gnum].peer_names[i*N], tmp_peerlist[i], N);
    }

    memcpy(groupchats[gnum].oldpeer_names, groupchats[gnum].peer_names, N*npeers);
}

static void groupchat_onGroupNamelistChange(ToxWindow *self, Tox *m, int groupnum, int peernum,
                                            uint8_t change)
{
    if (self->num != groupnum)
        return;

    groupchats[groupnum].num_peers = tox_group_number_peers(m, groupnum);
    int num_peers = groupchats[groupnum].num_peers;

    /* get old peer name before updating name list */
    uint8_t oldpeername[TOX_MAX_NAME_LENGTH] = {0};
    memcpy(oldpeername, &groupchats[groupnum].oldpeer_names[peernum*TOX_MAX_NAME_LENGTH], sizeof(oldpeername));

    if (string_is_empty(oldpeername))
        strcpy(oldpeername, (uint8_t *) UNKNOWN_NAME);

    /* Update name lists */
    uint8_t tmp_peerlist[num_peers][TOX_MAX_NAME_LENGTH];
    tox_group_copy_names(m, groupnum, tmp_peerlist, num_peers);
    copy_peernames(groupnum, num_peers, tmp_peerlist);

    /* get current peername then sort namelist */
    uint8_t peername[TOX_MAX_NAME_LENGTH] = {0};
    memcpy(peername, &groupchats[groupnum].peer_names[peernum*TOX_MAX_NAME_LENGTH], sizeof(peername));

    qsort(groupchats[groupnum].peer_names, groupchats[groupnum].num_peers, TOX_MAX_NAME_LENGTH, name_compare);

    ChatContext *ctx = (ChatContext *) self->chatwin;
    print_time(ctx->history);

    switch (change) {
    case TOX_CHAT_CHANGE_PEER_ADD:
        wattron(ctx->history, COLOR_PAIR(GREEN));
        wattron(ctx->history, A_BOLD);
        wprintw(ctx->history, "* %s", peername);
        wattroff(ctx->history, A_BOLD);
        wprintw(ctx->history, " has joined the room\n");
        wattroff(ctx->history, COLOR_PAIR(GREEN));
        break;
    case TOX_CHAT_CHANGE_PEER_DEL:
        wattron(ctx->history, COLOR_PAIR(RED));
        wattron(ctx->history, A_BOLD);
        wprintw(ctx->history, "* %s", oldpeername);
        wattroff(ctx->history, A_BOLD);
        wprintw(ctx->history, " has left the room\n");
        wattroff(ctx->history, COLOR_PAIR(RED));
        break;
    case TOX_CHAT_CHANGE_PEER_NAME:
        wattron(ctx->history, COLOR_PAIR(MAGENTA));
        wattron(ctx->history, A_BOLD);
        wprintw(ctx->history, "* %s", oldpeername);
        wattroff(ctx->history, A_BOLD);

        wprintw(ctx->history, " is now known as ");

        wattron(ctx->history, A_BOLD);
        wprintw(ctx->history, "%s\n", peername);
        wattroff(ctx->history, A_BOLD);
        wattroff(ctx->history, COLOR_PAIR(MAGENTA));
        break;
    }
}

static void groupchat_onKey(ToxWindow *self, Tox *m, wint_t key)
{
    ChatContext *ctx = (ChatContext *) self->chatwin;

    int x, y, y2, x2;
    getyx(self->window, y, x);
    getmaxyx(self->window, y2, x2);

    /* BACKSPACE key: Remove one character from line */
    if (key == 0x107 || key == 0x8 || key == 0x7f) {
        if (ctx->pos > 0) {
            ctx->line[--ctx->pos] = L'\0';

            if (x == 0)
                mvwdelch(self->window, y - 1, x2 - 1);
            else
                mvwdelch(self->window, y, x - 1);
        }
    } else 
    /* Add printable chars to buffer and print on input space */
#if HAVE_WIDECHAR
    if (iswprint(key)) {
#else
    if (isprint(key)) {
#endif
        if (ctx->pos < (MAX_STR_SIZE-1)) {
            mvwaddstr(self->window, y, x, wc_to_char(key));
            ctx->line[ctx->pos++] = key;
            ctx->line[ctx->pos] = L'\0';
        }
    }

    /* RETURN key: Execute command or print line */
    else if (key == '\n') {
        uint8_t *line = wcs_to_char(ctx->line);
        wclear(ctx->linewin);
        wmove(self->window, y2 - CURS_Y_OFFSET, 0);
        wclrtobot(self->window);
        bool close_win = false;

        if (line[0] == '/') {
            if (close_win = strcmp(line, "/close") == 0) {
                set_active_window(0);
                int groupnum = self->num;
                delwin(ctx->linewin);
                del_window(self);
                close_groupchatwin(m, groupnum);
            } else if (strncmp(line, "/help", strlen("/help")) == 0)
                print_groupchat_help(ctx);
              else
                execute(ctx->history, self, m, line, GROUPCHAT_COMMAND_MODE);
        } else {
            /* make sure the string has at least non-space character */
            if (!string_is_empty(line)) {
                // uint8_t selfname[TOX_MAX_NAME_LENGTH];
                // tox_getselfname(m, selfname, TOX_MAX_NAME_LENGTH);

                // print_time(ctx->history);
                // wattron(ctx->history, COLOR_PAIR(GREEN));
                // wprintw(ctx->history, "%s: ", selfname);
                // wattroff(ctx->history, COLOR_PAIR(GREEN));
                // wprintw(ctx->history, "%s\n", line);

                if (tox_group_message_send(m, self->num, line, strlen(line) + 1) == -1) {
                    wattron(ctx->history, COLOR_PAIR(RED));
                    wprintw(ctx->history, " * Failed to send message.\n");
                    wattroff(ctx->history, COLOR_PAIR(RED));
                }
            }
        }

        if (close_win)
            free(ctx);
        else {
            ctx->line[0] = L'\0';
            ctx->pos = 0;
        }

        free(line);
    }
}

static void groupchat_onDraw(ToxWindow *self, Tox *m)
{
    curs_set(1);
    int x, y;
    getmaxyx(self->window, y, x);

    ChatContext *ctx = (ChatContext *) self->chatwin;
    wclrtobot(ctx->sidebar);
    mvwhline(ctx->linewin, 0, 0, ACS_HLINE, x);
    mvwvline(ctx->sidebar, 0, 0, ACS_VLINE, y-CHATBOX_HEIGHT);
    mvwaddch(ctx->sidebar, y-CHATBOX_HEIGHT, 0, ACS_BTEE);  

    int num_peers = groupchats[self->num].num_peers;
    int N = TOX_MAX_NAME_LENGTH;
    int maxlines = y - CHATBOX_HEIGHT;
    int i;

    for (i = 0; i < num_peers && i < maxlines; ++i) {
        wmove(ctx->sidebar, i, 1);
        groupchats[self->num].peer_names[i*N+SIDEBAR_WIDTH-2] = '\0';
        wprintw(ctx->sidebar, "%s\n", &groupchats[self->num].peer_names[i*N]);
    }

    wrefresh(self->window);
}

static void groupchat_onInit(ToxWindow *self, Tox *m)
{
    int x, y;
    getmaxyx(self->window, y, x);

    ChatContext *ctx = (ChatContext *) self->chatwin;
    ctx->history = subwin(self->window, y-CHATBOX_HEIGHT+1, x-SIDEBAR_WIDTH-1, 0, 0);
    scrollok(ctx->history, 1);
    ctx->linewin = subwin(self->window, 2, x, y-CHATBOX_HEIGHT, 0);
    ctx->sidebar = subwin(self->window, y-CHATBOX_HEIGHT+1, SIDEBAR_WIDTH, 0, x-SIDEBAR_WIDTH);

    print_groupchat_help(ctx);
    wmove(self->window, y-CURS_Y_OFFSET, 0);
}

ToxWindow new_group_chat(Tox *m, int groupnum)
{
    ToxWindow ret;
    memset(&ret, 0, sizeof(ret));

    ret.onKey = &groupchat_onKey;
    ret.onDraw = &groupchat_onDraw;
    ret.onInit = &groupchat_onInit;
    ret.onGroupMessage = &groupchat_onGroupMessage;
    ret.onGroupNamelistChange = &groupchat_onGroupNamelistChange;
    // ret.onAction = &groupchat_onAction;

    snprintf(ret.name, sizeof(ret.name), "Room #%d", groupnum);

    ChatContext *chatwin = calloc(1, sizeof(ChatContext));

    if (chatwin != NULL)
        ret.chatwin = chatwin;
    else {
        endwin();
        fprintf(stderr, "calloc() failed. Aborting...\n");
        exit(EXIT_FAILURE);
    }

    ret.num = groupnum;

    return ret;
}

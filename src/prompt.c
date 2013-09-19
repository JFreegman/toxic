/*
 * Toxic -- Tox Curses Client
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "prompt.h"
#include "commands.h"

uint8_t pending_frnd_requests[MAX_FRIENDS_NUM][TOX_CLIENT_ID_SIZE];
uint8_t num_frnd_requests = 0;

uint8_t pending_grp_requests[MAX_GROUPCHAT_NUM][TOX_CLIENT_ID_SIZE];
uint8_t num_grp_requests = 0;

static char prompt_buf[MAX_STR_SIZE] = {'\0'};
static int prompt_buf_pos = 0;

/* Updates own nick in prompt statusbar */
void prompt_update_nick(ToxWindow *prompt, uint8_t *nick, uint16_t len)
{
    StatusBar *statusbar = (StatusBar *) prompt->stb;
    snprintf(statusbar->nick, sizeof(statusbar->nick), "%s", nick);
    statusbar->nick_len = len;
}

/* Updates own statusmessage in prompt statusbar */
void prompt_update_statusmessage(ToxWindow *prompt, uint8_t *statusmsg, uint16_t len)
{
    StatusBar *statusbar = (StatusBar *) prompt->stb;
    snprintf(statusbar->statusmsg, sizeof(statusbar->statusmsg), "%s", statusmsg);
    statusbar->statusmsg_len = len;
}

/* Updates own status in prompt statusbar */
void prompt_update_status(ToxWindow *prompt, TOX_USERSTATUS status)
{
    StatusBar *statusbar = (StatusBar *) prompt->stb;
    statusbar->status = status;
}

/* Updates own connection status in prompt statusbar */
void prompt_update_connectionstatus(ToxWindow *prompt, bool is_connected)
{
    StatusBar *statusbar = (StatusBar *) prompt->stb;
    statusbar->is_online = is_connected;
}

/* Adds friend request to pending friend requests. 
   Returns friend number on success, -1 if queue is full or other error. */
int add_friend_req(uint8_t *public_key)
{
    if (num_frnd_requests < MAX_FRIENDS_NUM) {
        memcpy(pending_frnd_requests[num_frnd_requests++], public_key, TOX_CLIENT_ID_SIZE);
        return num_frnd_requests - 1;
    }

    return -1;
}

/* Adds group chat invite to pending group chat requests. 
   Returns group number on success, -1 if queue is full or other error. */
int add_group_req(uint8_t *group_pub_key)
{
    if (num_grp_requests < MAX_GROUPCHAT_NUM) {
        memcpy(pending_grp_requests[num_grp_requests++], group_pub_key, TOX_CLIENT_ID_SIZE);
        return num_grp_requests - 1;
    }

    return -1;
}

// XXX: FIX
unsigned char *hex_string_to_bin(char hex_string[])
{
    size_t len = strlen(hex_string);
    unsigned char *val = malloc(len);

    if (val == NULL) {
        endwin();
        fprintf(stderr, "malloc() failed. Aborting...\n");
        exit(EXIT_FAILURE);
    }

    char *pos = hex_string;
    size_t i;

    for (i = 0; i < len; ++i, pos += 2)
        sscanf(pos, "%2hhx", &val[i]);

    return val;
}

void print_prompt_help(ToxWindow *self)
{
    wclear(self->window);
    wattron(self->window, COLOR_PAIR(CYAN) | A_BOLD);
    wprintw(self->window, "\n\nCommands:\n");
    wattroff(self->window, A_BOLD);

    wprintw(self->window, "      /add <id> <message>        : Add friend with optional message\n");
    wprintw(self->window, "      /accept <n>                : Accept friend request\n");
    wprintw(self->window, "      /status <type> <message>   : Set your status with optional note\n");
    wprintw(self->window, "      /note  <message>           : Set a personal note\n");
    wprintw(self->window, "      /nick <nickname>           : Set your nickname\n");
    wprintw(self->window, "      /join <n>                  : Join a group chat\n");
    wprintw(self->window, "      /invite <nickname> <n>     : Invite friend to a groupchat\n");
    wprintw(self->window, "      /groupchat                 : Create a group chat\n");
    wprintw(self->window, "      /myid                      : Print your ID\n");
    wprintw(self->window, "      /quit or /exit             : Exit Toxic\n");
    wprintw(self->window, "      /help                      : Print this message again\n");
    wprintw(self->window, "      /clear                     : Clear this window\n");

    wattron(self->window, A_BOLD);
    wprintw(self->window, " * Messages must be enclosed in quotation marks.\n");
    wprintw(self->window, " * Use the TAB key to navigate through the tabs.\n");
    wattroff(self->window, A_BOLD);

    wattroff(self->window, COLOR_PAIR(CYAN));
}

static void prompt_onKey(ToxWindow *self, Tox *m, wint_t key)
{
    /* Add printable characters to line */
    if (isprint(key)) {
        if (prompt_buf_pos == (sizeof(prompt_buf) - 1)) {
            return;
        } else if (!(prompt_buf_pos == 0) && (prompt_buf_pos < COLS)
                   && (prompt_buf_pos % (COLS - 3) == 0)) {
            wprintw(self->window, "\n");
            prompt_buf[prompt_buf_pos++] = '\n';
        } else if (!(prompt_buf_pos == 0) && (prompt_buf_pos > COLS)
                   && ((prompt_buf_pos - (COLS - 3)) % (COLS) == 0)) {
            wprintw(self->window, "\n");
            prompt_buf[prompt_buf_pos++] = '\n';
        }

        prompt_buf[prompt_buf_pos++] = key;
        prompt_buf[prompt_buf_pos] = 0;
    }

    /* RETURN key: execute command */
    else if (key == '\n') {
        wprintw(self->window, "\n");

        if (!strncmp(prompt_buf, "/help", strlen("/help")))
            print_prompt_help(self);
        else
            execute(self->window, self, m, prompt_buf, prompt_buf_pos);

        prompt_buf_pos = 0;
        prompt_buf[0] = 0;
    }

    /* BACKSPACE key: Remove one character from line */
    else if (key == 0x107 || key == 0x8 || key == 0x7f) {
        if (prompt_buf_pos != 0)
            prompt_buf[--prompt_buf_pos] = 0;
    }
}

static void prompt_onDraw(ToxWindow *self, Tox *m)
{
    curs_set(1);
    int x, y;
    size_t i;
    getyx(self->window, y, x);

    for (i = 0; i < (strlen(prompt_buf)); ++i) {
        if ((prompt_buf[i] == '\n') && (y != 0))
            --y;
    }

    StatusBar *statusbar = (StatusBar *) self->stb;

    werase(statusbar->topline);

    if (statusbar->is_online) {
        int colour = WHITE;
        char *status_text = "Unknown";

        switch(statusbar->status) {
        case TOX_USERSTATUS_NONE:
            status_text = "Online";
            colour = GREEN;
            break;
        case TOX_USERSTATUS_AWAY:
            status_text = "Away";
            colour = YELLOW;
            break;
        case TOX_USERSTATUS_BUSY:
            status_text = "Busy";
            colour = RED;
            break;
        }

        wattron(statusbar->topline, A_BOLD);
        wprintw(statusbar->topline, " %s ", statusbar->nick);
        wattron(statusbar->topline, A_BOLD);
        wattron(statusbar->topline, COLOR_PAIR(colour) | A_BOLD);
        wprintw(statusbar->topline, "[%s]", status_text);
        wattroff(statusbar->topline, COLOR_PAIR(colour) | A_BOLD);
    } else {
        wattron(statusbar->topline, A_BOLD);
        wprintw(statusbar->topline, "%s ", statusbar->nick);
        wattroff(statusbar->topline, A_BOLD);
        wprintw(statusbar->topline, "[Offline]");
    }

    wattron(statusbar->topline, A_BOLD);
    wprintw(statusbar->topline, " | %s |", statusbar->statusmsg);
    wattroff(statusbar->topline, A_BOLD);

    wprintw(statusbar->topline, "\n");

    wattron(self->window, COLOR_PAIR(GREEN));
    mvwprintw(self->window, y, 0, "# ");
    wattroff(self->window, COLOR_PAIR(GREEN));
    mvwprintw(self->window, y, 2, "%s", prompt_buf);
    wclrtoeol(self->window);
    
    wrefresh(self->window);
}

static void prompt_onInit(ToxWindow *self, Tox *m)
{
    scrollok(self->window, 1);
    print_prompt_help(self);
    wclrtoeol(self->window);
}

void prompt_onFriendRequest(ToxWindow *self, uint8_t *key, uint8_t *data, uint16_t length)
{
    int n = add_friend_req(key);

    if (n == -1) {
        wprintw(self->window, "Friend request queue is full. Discarding request.\n");
        return;
    }

    wprintw(self->window, "\nFriend request from:\n");

    int i;

    for (i = 0; i < KEY_SIZE_BYTES; ++i) {
        wprintw(self->window, "%02x", key[i] & 0xff);
    }

    wprintw(self->window, "\n\nWith the message: %s\n\n", data);
    wprintw(self->window, "Type \"/accept %d\" to accept it.\n", n);

    self->blink = true;
    beep();
}

void prompt_onGroupInvite(ToxWindow *self, Tox *m, int friendnumber, uint8_t *group_pub_key)
{
    if (friendnumber < 0)
        return;

    uint8_t name[TOX_MAX_NAME_LENGTH] = {'\0'};

    if (tox_getname(m, friendnumber, name) == -1)
        return;

    name[TOXIC_MAX_NAME_LENGTH] = '\0';    /* enforce client max name length */
    wprintw(self->window, "\nGroup chat invite from %s.\n", name);

    int ngc = get_num_groupchats();

    if (ngc < 0 || ngc > MAX_GROUPCHAT_NUM) {
        wprintw(self->window, "\nMaximum number of group chats has been reached. Discarding invite.\n");
        return;
    }

    int n = add_group_req(group_pub_key);

    if (n == -1) {
        wprintw(self->window, "\nGroup chat queue is full. Discarding invite.\n");
        return;
    }

    wprintw(self->window, "Type \"/join %d\" to join the chat.\n", n);

    self->blink = true;
    beep();
}

void prompt_init_statusbar(ToxWindow *self, Tox *m)
{
    int x, y;
    getmaxyx(self->window, y, x);

    /* Init statusbar info */
    StatusBar *statusbar = (StatusBar *) self->stb;
    statusbar->status = TOX_USERSTATUS_NONE;
    statusbar->is_online = false;

    uint8_t nick[TOX_MAX_NAME_LENGTH] = {'\0'};
    tox_getselfname(m, nick, TOX_MAX_NAME_LENGTH);
    snprintf(statusbar->nick, sizeof(statusbar->nick), "%s", nick);

    /* temporary until statusmessage saving works */
    uint8_t statusmsg[TOX_MAX_STATUSMESSAGE_LENGTH];
    char toxic_ver[strlen(TOXICVER)+1];
    strcpy(toxic_ver, TOXICVER);
    char *L = strchr(toxic_ver, '_');
    toxic_ver[L-toxic_ver] = '\0';
    snprintf(statusmsg, sizeof(statusmsg), "Toxing on Toxic v.%s", toxic_ver); 
    m_set_statusmessage(m, statusmsg, strlen(statusmsg) + 1);
    snprintf(statusbar->statusmsg, sizeof(statusbar->statusmsg), "%s", statusmsg);

    /* Init statusbar subwindow */
    statusbar->topline = subwin(self->window, 2, x, 0, 0);
}

ToxWindow new_prompt(void)
{
    ToxWindow ret;
    memset(&ret, 0, sizeof(ret));

    ret.onKey = &prompt_onKey;
    ret.onDraw = &prompt_onDraw;
    ret.onInit = &prompt_onInit;
    ret.onFriendRequest = &prompt_onFriendRequest;
    ret.onGroupInvite = &prompt_onGroupInvite;

    strcpy(ret.name, "prompt");

    StatusBar *stb = calloc(1, sizeof(StatusBar));

    if (stb != NULL)
        ret.stb = stb;
    else {
        endwin();
        fprintf(stderr, "calloc() failed. Aborting...\n");
        exit(EXIT_FAILURE);
    }

    return ret;
}

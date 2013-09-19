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
#include "groupchat.h"

extern char *DATA_FILE;

uint8_t pending_frnd_requests[MAX_FRIENDS_NUM][TOX_CLIENT_ID_SIZE];
uint8_t num_frnd_requests = 0;

uint8_t pending_grp_requests[MAX_GROUPCHAT_NUM][TOX_CLIENT_ID_SIZE];
uint8_t num_grp_requests = 0;

static char prompt_buf[MAX_STR_SIZE] = {'\0'};
static int prompt_buf_pos = 0;

/* commands */
void cmd_accept(WINDOW *, ToxWindow *, Tox *m, int, char **);
void cmd_add(WINDOW *, ToxWindow *, Tox *m, int, char **);
void cmd_clear(WINDOW *, ToxWindow *, Tox *m, int, char **);
void cmd_connect(WINDOW *, ToxWindow *, Tox *m, int, char **);
void cmd_groupchat(WINDOW *, ToxWindow *, Tox *m, int, char **);
void cmd_help(WINDOW *, ToxWindow *, Tox *m, int, char **);
void cmd_invite(WINDOW *, ToxWindow *, Tox *m, int, char **);
void cmd_join(WINDOW *, ToxWindow *, Tox *m, int, char **);
void cmd_msg(WINDOW *, ToxWindow *, Tox *m, int, char **);
void cmd_myid(WINDOW *, ToxWindow *, Tox *m, int, char **);
void cmd_nick(WINDOW *, ToxWindow *, Tox *m, int, char **);
void cmd_quit(WINDOW *, ToxWindow *, Tox *m, int, char **);
void cmd_status(WINDOW *, ToxWindow *, Tox *m, int, char **);
void cmd_note(WINDOW *, ToxWindow *, Tox *m, int, char **);

#define NUM_COMMANDS 16

static struct {
    char *name;
    void (*func)(WINDOW *, ToxWindow *, Tox *m, int, char **);
} commands[] = {
    { "/accept",    cmd_accept    },
    { "/add",       cmd_add       },
    { "/clear",     cmd_clear     },
    { "/connect",   cmd_connect   },
    { "/exit",      cmd_quit      },
    { "/groupchat", cmd_groupchat },
    { "/help",      cmd_help      },
    { "/invite",    cmd_invite    },
    { "/join",      cmd_join      },
    { "/msg",       cmd_msg       },
    { "/myid",      cmd_myid      },
    { "/nick",      cmd_nick      },
    { "/q",         cmd_quit      },
    { "/quit",      cmd_quit      },
    { "/status",    cmd_status    },
    { "/note",      cmd_note      },
};

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

/* command functions */
void cmd_accept(WINDOW *window, ToxWindow *prompt, Tox *m, int argc, char **argv)
{
    /* check arguments */
    if (argc != 1) {
      wprintw(window, "Invalid syntax.\n");
      return;
    }

    int num = atoi(argv[1]);

    if (num < 0 || num >= num_frnd_requests) {
        wprintw(window, "No pending friend request with that number.\n");
        return;
    }

    int friendnum = tox_addfriend_norequest(m, pending_frnd_requests[num]);

    if (friendnum == -1)
        wprintw(window, "Failed to add friend.\n");
    else {
        wprintw(window, "Friend request accepted.\n");
        on_friendadded(m, friendnum);
    }
}

void cmd_add(WINDOW *window, ToxWindow *prompt, Tox *m, int argc, char **argv)
{
    if (argc < 1 || argc > 2) {
        wprintw(window, "Invalid syntax.\n");
        return;
    }

    char *id = argv[1];

    if (id == NULL) {
        wprintw(window, "Invalid syntax.\n");
        return;
    }

    uint8_t *msg;

    if (argc == 2) {
        msg = argv[2];

        if (msg == NULL) {
            wprintw(window, "Invalid syntax.\n");
            return;
        }

        if (msg[0] != '\"') {
            wprintw(window, "Messages must be enclosed in quotes.\n");
            return;
        }

        msg[strlen(++msg)-1] = L'\0';

    } else
        msg = "Let's tox.";

    if (strlen(id) != 2 * TOX_FRIEND_ADDRESS_SIZE) {
        wprintw(window, "Invalid ID length.\n");
        return;
    }

    size_t i;
    char xx[3];
    uint32_t x;
    uint8_t id_bin[TOX_FRIEND_ADDRESS_SIZE];

    for (i = 0; i < TOX_FRIEND_ADDRESS_SIZE; ++i) {
        xx[0] = id[2 * i];
        xx[1] = id[2 * i + 1];
        xx[2] = '\0';

        if (sscanf(xx, "%02x", &x) != 1) {
            wprintw(window, "Invalid ID.\n");
            return;
        }

        id_bin[i] = x;
    }

    for (i = 0; i < TOX_FRIEND_ADDRESS_SIZE; i++) {
        id[i] = toupper(id[i]);
    }

    int num = tox_addfriend(m, id_bin, msg, strlen(msg) + 1);

    switch (num) {
        case TOX_FAERR_TOOLONG:
            wprintw(window, "Message is too long.\n");
            break;

        case TOX_FAERR_NOMESSAGE:
            wprintw(window, "Please add a message to your request.\n");
            break;

        case TOX_FAERR_OWNKEY:
            wprintw(window, "That appears to be your own ID.\n");
            break;

        case TOX_FAERR_ALREADYSENT:
            wprintw(window, "Friend request already sent.\n");
            break;

        case TOX_FAERR_UNKNOWN:
            wprintw(window, "Undefined error when adding friend.\n");
            break;

        case TOX_FAERR_BADCHECKSUM:
            wprintw(window, "Bad checksum in address.\n");
            break;

        case TOX_FAERR_SETNEWNOSPAM:
            wprintw(window, "Nospam was different.\n");
            break;

        default:
            wprintw(window, "Friend added as %d.\n", num);
            on_friendadded(m, num);
            break;
    }
}

void cmd_clear(WINDOW *window, ToxWindow *prompt, Tox *m, int argc, char **argv)
{
    wclear(window);
    wprintw(window, "\n\n");
}

void cmd_connect(WINDOW *window, ToxWindow *prompt, Tox *m, int argc, char **argv)
{
    /* check arguments */
    if (argc != 3) {
      wprintw(window, "Invalid syntax.\n");
      return;
    }

    tox_IP_Port dht;
    char *ip = argv[1];
    char *port = argv[2];
    char *key = argv[3];

    if (ip == NULL || port == NULL || key == NULL) {
        wprintw(window, "Invalid syntax.\n");
        return;
    }

    if (atoi(port) == 0) {
        wprintw(window, "Invalid syntax.\n");
        return;
    }

    uint8_t *binary_string = hex_string_to_bin(key);
    tox_bootstrap_from_address(m, ip, TOX_ENABLE_IPV6_DEFAULT,
                               htons(atoi(port)), binary_string);
    free(binary_string);
}

void cmd_quit(WINDOW *window, ToxWindow *prompt, Tox *m, int argc, char **argv)
{
    exit_toxic(m);
}

void cmd_groupchat(WINDOW *window, ToxWindow *prompt, Tox *m, int argc, char **argv)
{
    int ngc = get_num_groupchats();

    if (ngc < 0 || ngc > MAX_GROUPCHAT_NUM) {
        wprintw(window, "\nMaximum number of group chats has been reached.\n");
        return;
    }

    int groupnum = tox_add_groupchat(m);

    if (groupnum == -1) {
        wprintw(window, "Group chat failed to initialize.\n");
        return;
    }

    if (init_groupchat_win(prompt, m, groupnum) == -1) {
        wprintw(window, "Group chat failed to initialize.\n");
        tox_del_groupchat(m, groupnum);
        return;
    }

    wprintw(window, "Group chat created as %d.\n", groupnum);
}

void cmd_help(WINDOW *window, ToxWindow *prompt, Tox *m, int argc, char **argv)
{
    wclear(window);
    wattron(window, COLOR_PAIR(CYAN) | A_BOLD);
    wprintw(window, "\n\nCommands:\n");
    wattroff(window, A_BOLD);

    wprintw(window, "      /connect <ip> <port> <key> : Connect to DHT server\n");
    wprintw(window, "      /add <id> <message>        : Add friend with optional message\n");
    wprintw(window, "      /accept <n>                : Accept friend request\n");
    wprintw(window, "      /status <type> <message>   : Set your status with optional note\n");
    wprintw(window, "      /note  <message>           : Set a personal note\n");
    wprintw(window, "      /nick <nickname>           : Set your nickname\n");
    wprintw(window, "      /join <n>                  : Join a group chat\n");
    wprintw(window, "      /invite <nickname> <n>     : Invite friend to a groupchat\n");
    wprintw(window, "      /groupchat                 : Create a group chat\n");
    wprintw(window, "      /myid                      : Print your ID\n");
    wprintw(window, "      /quit or /exit             : Exit Toxic\n");
    wprintw(window, "      /help                      : Print this message again\n");
    wprintw(window, "      /clear                     : Clear this window\n");

    wattron(window, A_BOLD);
    wprintw(window, "\n * Messages must be enclosed in quotation marks.\n");
    wprintw(window, " * Use the TAB key to navigate through the tabs.\n\n");
    wattroff(window, A_BOLD);

    wattroff(window, COLOR_PAIR(CYAN));
}

void cmd_invite(WINDOW *window, ToxWindow *prompt, Tox *m, int argc, char **argv)
{
    if (argc != 2) {
        wprintw(window, "Invalid syntax.\n");
        return;
    }

    if (argv[1] == NULL || argv[2] == NULL) {
        wprintw(window, "Invalid syntax.\n");
        return;
    }

    uint8_t *friendname = argv[1];
    int groupnum = atoi(argv[2]);

    if (friendname[0] == '\"')
        friendname[strlen(++friendname)-1] = L'\0';

    int friendnum = get_friendnum(friendname);

    if (friendnum == -1) {
        wprintw(window, "Friend '%s' not found.\n", friendname);
        return;
    }

    if (tox_invite_friend(m, friendnum, groupnum) == -1) {
        wprintw(window, "Failed to invite friend.\n");
        return;
    }

    wprintw(window, "Invited friend %s to group chat %d.\n", friendname, groupnum);
}

void cmd_join(WINDOW *window, ToxWindow *prompt, Tox *m, int argc, char **argv)
{
    if (argc != 1) {
      wprintw(window, "Invalid syntax.\n");
      return;
    }

    if (argv[1] == NULL) {
      wprintw(window, "Invalid syntax.\n");
      return;
    }

    int num = atoi(argv[1]);

    if (num < 0 || num >= num_grp_requests) {
        wprintw(window, "No pending group chat invites with that number.\n");
        return;
    }

    int groupnum = tox_join_groupchat(m, num, pending_grp_requests[num]);

    if (groupnum == -1) {
        wprintw(window, "Group chat failed to initialize.\n");
        return;
    }

    if (init_groupchat_win(prompt, m, groupnum) == -1) {
        wprintw(window, "Group chat window failed to initialize.\n");
        tox_del_groupchat(m, groupnum);
        return;
    }
}

void cmd_msg(WINDOW *window, ToxWindow *prompt, Tox *m, int argc, char **argv)
{
    /* check arguments */
    if (argc != 2) {
      wprintw(window, "Invalid syntax.\n");
      return;
    }

    char *id = argv[1];
    uint8_t *msg = argv[2];

    if (id == NULL || msg == NULL) {
      wprintw(window, "Invalid syntax.\n");
      return;
    }

    msg[strlen(++msg)-1] = L'\0';

    if (tox_sendmessage(m, atoi(id), msg, strlen(msg) + 1) == 0)
        wprintw(window, "Failed to send message.\n");
    else
        wprintw(window, "Message successfully sent.\n");
}

void cmd_myid(WINDOW *window, ToxWindow *prompt, Tox *m, int argc, char **argv)
{
    char id[TOX_FRIEND_ADDRESS_SIZE * 2 + 1] = {0};
    uint8_t address[TOX_FRIEND_ADDRESS_SIZE];
    tox_getaddress(m, address);

    size_t i;

    for (i = 0; i < TOX_FRIEND_ADDRESS_SIZE; ++i) {
        char xx[3];
        snprintf(xx, sizeof(xx), "%02X", address[i] & 0xff);
        strcat(id, xx);
    }

    wprintw(window, "%s\n", id);
}

void cmd_nick(WINDOW *window, ToxWindow *prompt, Tox *m, int argc, char **argv)
{
    /* check arguments */
    if (argc != 1) {
      wprintw(window, "Invalid syntax.\n");
      return;
    }

    uint8_t *nick = argv[1];

    if (nick == NULL) {
        wprintw(window, "Invalid syntax.\n");
        return;
    }

    int len = strlen(nick);

    if (nick[0] == '\"') {
        ++nick;
        len -= 2;
        nick[len] = L'\0';
    }

    if (len > TOXIC_MAX_NAME_LENGTH) {
        nick[TOXIC_MAX_NAME_LENGTH] = L'\0';
        len = TOXIC_MAX_NAME_LENGTH;
    }

    tox_setname(m, nick, len+1);
    prompt_update_nick(prompt, nick, len+1);

    store_data(m, DATA_FILE);
}

void cmd_status(WINDOW *window, ToxWindow *prompt, Tox *m, int argc, char **argv)
{
    uint8_t *msg = NULL;

    if (argc == 2) {

        msg = argv[2];

        if (msg == NULL) {
            wprintw(window, "Invalid syntax.\n");
            return;
        }

        if (msg[0] != '\"') {
            wprintw(window, "Messages must be enclosed in quotes.\n");
            return;
        }
    } else if (argc != 1) {
        wprintw(window, "Wrong number of arguments.\n");
        return;
    }

    char *status = argv[1];

    if (status == NULL) {
        wprintw(window, "Invalid syntax.\n");
        return;
    }

    TOX_USERSTATUS status_kind;

    if (!strncmp(status, "online", strlen("online")))
        status_kind = TOX_USERSTATUS_NONE;

    else if (!strncmp(status, "away", strlen("away")))
        status_kind = TOX_USERSTATUS_AWAY;

    else if (!strncmp(status, "busy", strlen("busy")))
        status_kind = TOX_USERSTATUS_BUSY;

    else
        wprintw(window, "Invalid status.\n");

    tox_set_userstatus(m, status_kind);
    prompt_update_status(prompt, status_kind);

    if (msg != NULL) {
        msg[strlen(++msg)-1] = L'\0';   /* remove opening and closing quotes */
        uint16_t len = strlen(msg) + 1;
        tox_set_statusmessage(m, msg, len);
        prompt_update_statusmessage(prompt, msg, len);
    }
}

void cmd_note(WINDOW *window, ToxWindow *prompt, Tox *m, int argc, char **argv)
{
    if (argc != 1) {
        wprintw(window, "Wrong number of arguments.\n");
        return;
    }

    uint8_t *msg = argv[1];

    if (msg == NULL) {
        wprintw(window, "Invalid syntax.\n");
        return;
    }

    if (msg[0] != '\"') {
        wprintw(window, "Messages must be enclosed in quotes.\n");
        return;
    }

    msg[strlen(++msg)-1] = L'\0';
    uint16_t len = strlen(msg) + 1;
    tox_set_statusmessage(m, msg, len);
    prompt_update_statusmessage(prompt, msg, len);
}

void execute(WINDOW *window, ToxWindow *prompt, Tox *m, char *u_cmd, int buf_len)
{
    int newlines = 0;
    char cmd[MAX_STR_SIZE] = {'\0'};
    size_t i;

    for (i = 0; i < buf_len; ++i) {
        if (u_cmd[i] == '\n')
            ++newlines;
        else
            cmd[i - newlines] = u_cmd[i];
    }

    int leading_spc = 0;

    for (i = 0; i < MAX_STR_SIZE && isspace(cmd[i]); ++i)
        leading_spc++;

    memmove(cmd, cmd + leading_spc, MAX_STR_SIZE - leading_spc);

    int cmd_end = strlen(cmd);

    while (cmd_end > 0 && cmd_end--)
        if (!isspace(cmd[cmd_end]))
            break;

    cmd[cmd_end + 1] = '\0';

    /* insert \0 at argument boundaries */
    int numargs = 0;
    for (i = 0; i < MAX_STR_SIZE; i++) {
        if (cmd[i] == ' ') {
            cmd[i] = '\0';
            numargs++;
        }
        /* skip over strings */
        else if (cmd[i] == '\"') {
            while (cmd[++i] != '\"') {
                if (cmd[i] == '\0') {
                    wprintw(window, "Invalid command: did you forget an opening or closing \"?\n");
                    return;
                }
            }
        }
    }

    /* read arguments into array */
    char **cmdargs = malloc((numargs + 1) * sizeof(char *));
    if (!cmdargs) {
        wprintw(window, "Invalid command: too many arguments.\n");
        return;
    }

    int pos = 0;
    
    for (i = 0; i < numargs + 1; i++) {
        cmdargs[i] = cmd + pos;
        pos += strlen(cmdargs[i]) + 1;
        /* replace empty strings with NULL for easier error checking */
        if (strlen(cmdargs[i]) == 0)
            cmdargs[i] = NULL;
    }

    /* no input */
    if (!cmdargs[0]) {
        free(cmdargs);
        return;
    }

    /* match input to command list */
    for (i = 0; i < NUM_COMMANDS; i++) {
        if (!strcmp(cmdargs[0], commands[i].name)) {
            (commands[i].func)(window, prompt, m, numargs, cmdargs);
            free(cmdargs);
            return;
        }
    }

    /* no match */
    free(cmdargs);
    wprintw(window, "Invalid command.\n");
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
    cmd_help(self->window, self, m, 0, NULL);
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
    uint8_t *statusmsg = "Toxing on Toxic v0.2.0";
    m_set_statusmessage(m, statusmsg, strlen(statusmsg) + 1);
    snprintf(statusbar->statusmsg, sizeof(statusbar->statusmsg), "%s", statusmsg);

    /* Init statusbar subwindow */
    statusbar->topline = subwin(self->window, 2, x, 0, 0);
}

ToxWindow new_prompt()
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

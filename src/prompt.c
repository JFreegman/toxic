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

extern char *DATA_FILE;

uint8_t pending_requests[MAX_STR_SIZE][TOX_CLIENT_ID_SIZE]; // XXX
uint8_t num_requests = 0; // XXX

static char prompt_buf[MAX_STR_SIZE] = {'\0'};
static int prompt_buf_pos = 0;

/* commands */
void cmd_accept(ToxWindow *, Tox *m, int, char **);
void cmd_add(ToxWindow *, Tox *m, int, char **);
void cmd_clear(ToxWindow *, Tox *m, int, char **);
void cmd_connect(ToxWindow *, Tox *m, int, char **);
void cmd_help(ToxWindow *, Tox *m, int, char **);
void cmd_msg(ToxWindow *, Tox *m, int, char **);
void cmd_myid(ToxWindow *, Tox *m, int, char **);
void cmd_nick(ToxWindow *, Tox *m, int, char **);
void cmd_quit(ToxWindow *, Tox *m, int, char **);
void cmd_status(ToxWindow *, Tox *m, int, char **);
void cmd_note(ToxWindow *, Tox *m, int, char **);

#define NUM_COMMANDS 13

static struct {
    char *name;
    void (*func)(ToxWindow *, Tox *m, int, char **);
} commands[] = {
    { "accept",    cmd_accept    },
    { "add",       cmd_add       },
    { "clear",     cmd_clear     },
    { "connect",   cmd_connect   },
    { "exit",      cmd_quit      },
    { "help",      cmd_help      },
    { "msg",       cmd_msg       },
    { "myid",      cmd_myid      },
    { "nick",      cmd_nick      },
    { "q",         cmd_quit      },
    { "quit",      cmd_quit      },
    { "status",    cmd_status    },
    { "note",      cmd_note      },
};

/* Updates own nick in prompt statusbar */
void prompt_update_nick(ToxWindow *prompt, uint8_t *nick)
{
    StatusBar *statusbar = (StatusBar *) prompt->stb;
    snprintf(statusbar->nick, sizeof(statusbar->nick), "%s", nick);
}

/* Updates own statusmessage in prompt statusbar */
void prompt_update_statusmessage(ToxWindow *prompt, uint8_t *statusmsg)
{
    StatusBar *statusbar = (StatusBar *) prompt->stb;
    snprintf(statusbar->statusmsg, sizeof(statusbar->statusmsg), "%s", statusmsg);
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

void prompt_onFriendRequest(ToxWindow *prompt, uint8_t *key, uint8_t *data, uint16_t length)
{
    int n = add_req(key);
    wprintw(prompt->window, "\nFriend request from:\n");

    int i;

    for (i = 0; i < KEY_SIZE_BYTES; ++i) {
        wprintw(prompt->window, "%02x", key[i] & 0xff);
    }

    wprintw(prompt->window, "\n\nWith the message: %s\n\n", data);
    wprintw(prompt->window, "Type \"accept %d\" to accept it.\n", n);

    prompt->blink = true;
    beep();
}

// XXX:
int add_req(uint8_t *public_key)
{
    memcpy(pending_requests[num_requests], public_key, TOX_CLIENT_ID_SIZE);
    ++num_requests;
    return num_requests - 1;
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
    int i;

    for (i = 0; i < len; ++i, pos += 2)
        sscanf(pos, "%2hhx", &val[i]);

    return val;
}

/* command functions */
void cmd_accept(ToxWindow *self, Tox *m, int argc, char **argv)
{
    int num;

    /* check arguments */
    if (argc != 1) {
      wprintw(self->window, "Invalid syntax.\n");
      return;
    }

    num = atoi(argv[1]);

    if (num < 0 || num >= num_requests) {
        wprintw(self->window, "No pending request with that number.\n");
        return;
    }

    num = tox_addfriend_norequest(m, pending_requests[num]);

    if (num == -1)
        wprintw(self->window, "Failed to add friend.\n");
    else {
        wprintw(self->window, "Friend accepted as: %d.\n", num);
        on_friendadded(m, num);
    }
}

void cmd_add(ToxWindow *self, Tox *m, int argc, char **argv)
{
    if (argc < 1 || argc > 2) {
        wprintw(self->window, "Invalid syntax.\n");
        return;
    }

    uint8_t id_bin[TOX_FRIEND_ADDRESS_SIZE];
    char xx[3];
    uint32_t x;
    uint8_t *msg;
    int i, num;

    char *id = argv[1];

    if (id == NULL) {
        wprintw(self->window, "Invalid syntax.\n");
        return;
    }

    if (argc == 2) {
        msg = argv[2];

        if (msg == NULL) {
            wprintw(self->window, "Invalid syntax.\n");
            return;
        }

        if (msg[0] != '\"') {
            wprintw(self->window, "Messages must be enclosed in quotes.\n");
            return;
        }

        msg[strlen(++msg)-1] = L'\0';

    } else
        msg = "Let's tox.";

    if (strlen(id) != 2 * TOX_FRIEND_ADDRESS_SIZE) {
        wprintw(self->window, "Invalid ID length.\n");
        return;
    }

    for (i = 0; i < TOX_FRIEND_ADDRESS_SIZE; ++i) {
        xx[0] = id[2 * i];
        xx[1] = id[2 * i + 1];
        xx[2] = '\0';

        if (sscanf(xx, "%02x", &x) != 1) {
            wprintw(self->window, "Invalid ID.\n");
            return;
        }

        id_bin[i] = x;
    }

    for (i = 0; i < TOX_FRIEND_ADDRESS_SIZE; i++) {
        id[i] = toupper(id[i]);
    }

    num = tox_addfriend(m, id_bin, msg, strlen(msg) + 1);

    switch (num) {
        case TOX_FAERR_TOOLONG:
            wprintw(self->window, "Message is too long.\n");
            break;

        case TOX_FAERR_NOMESSAGE:
            wprintw(self->window, "Please add a message to your request.\n");
            break;

        case TOX_FAERR_OWNKEY:
            wprintw(self->window, "That appears to be your own ID.\n");
            break;

        case TOX_FAERR_ALREADYSENT:
            wprintw(self->window, "Friend request already sent.\n");
            break;

        case TOX_FAERR_UNKNOWN:
            wprintw(self->window, "Undefined error when adding friend.\n");
            break;

        case TOX_FAERR_BADCHECKSUM:
            wprintw(self->window, "Bad checksum in address.\n");
            break;

        case TOX_FAERR_SETNEWNOSPAM:
            wprintw(self->window, "Nospam was different.\n");
            break;

        default:
            wprintw(self->window, "Friend added as %d.\n", num);
            on_friendadded(m, num);
            break;
    }
}

void cmd_clear(ToxWindow *self, Tox *m, int argc, char **argv)
{
    wclear(self->window);
    wprintw(self->window, "\n\n");
}

void cmd_connect(ToxWindow *self, Tox *m, int argc, char **argv)
{
    /* check arguments */
    if (argc != 3) {
      wprintw(self->window, "Invalid syntax.\n");
      return;
    }

    tox_IP_Port dht;
    char *ip = argv[1];
    char *port = argv[2];
    char *key = argv[3];

    if (!ip || !port || !key) {
        wprintw(self->window, "Invalid syntax.\n");
        return;
    }

    if (atoi(port) == 0) {
        wprintw(self->window, "Invalid syntax.\n");
        return;
    }

    dht.port = htons(atoi(port));
    uint32_t resolved_address = resolve_addr(ip);

    if (resolved_address == 0) {
        return;
    }

    dht.ip.i = resolved_address;
    uint8_t *binary_string = hex_string_to_bin(key);
    tox_bootstrap(m, dht, binary_string);
    free(binary_string);
}

void cmd_quit(ToxWindow *self, Tox *m, int argc, char **argv)
{
    exit_toxic(m);
}

void cmd_help(ToxWindow *self, Tox *m, int argc, char **argv)
{
    wclear(self->window);
    wattron(self->window, COLOR_PAIR(CYAN) | A_BOLD);
    wprintw(self->window, "\n\nCommands:\n");
    wattroff(self->window, A_BOLD);

    wprintw(self->window, "      connect <ip> <port> <key> : Connect to DHT server\n");
    wprintw(self->window, "      add <id> <message>        : Add friend with optional message\n");
    wprintw(self->window, "      status <type> <message>   : Set your status with optional note\n");
    wprintw(self->window, "      note  <message>           : Set a personal note\n");
    wprintw(self->window, "      nick <nickname>           : Set your nickname\n");
    wprintw(self->window, "      accept <number>           : Accept friend request\n");
    wprintw(self->window, "      myid                      : Print your ID\n");
    wprintw(self->window, "      quit/exit                 : Exit Toxic\n");
    wprintw(self->window, "      help                      : Print this message again\n");
    wprintw(self->window, "      clear                     : Clear this window\n");

    wattron(self->window, A_BOLD);
    wprintw(self->window, " * Messages must be enclosed in quotation marks.\n");
    wprintw(self->window, " * Use the TAB key to navigate through the tabs.\n\n");
    wattroff(self->window, A_BOLD);

    wattroff(self->window, COLOR_PAIR(CYAN));
}

void cmd_msg(ToxWindow *self, Tox *m, int argc, char **argv)
{
    /* check arguments */
    if (argc != 2) {
      wprintw(self->window, "Invalid syntax.\n");
      return;
    }

    char *id = argv[1];
    uint8_t *msg = argv[2];

    if (id == NULL || msg == NULL) {
      wprintw(self->window, "Invalid syntax.\n");
      return;
    }

    msg[strlen(++msg)-1] = L'\0';

    if (tox_sendmessage(m, atoi(id), msg, strlen(msg) + 1) == 0)
        wprintw(self->window, "Failed to send message.\n");
    else
        wprintw(self->window, "Message successfully sent.\n");
}

void cmd_myid(ToxWindow *self, Tox *m, int argc, char **argv)
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

    wprintw(self->window, "%s\n", id);
}

void cmd_nick(ToxWindow *self, Tox *m, int argc, char **argv)
{
    /* check arguments */
    if (argc != 1) {
      wprintw(self->window, "Invalid syntax.\n");
      return;
    }

    uint8_t *nick = argv[1];

    if (nick == NULL) {
        wprintw(self->window, "Invalid syntax.\n");
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
    prompt_update_nick(self, nick);

    store_data(m, DATA_FILE);
}

void cmd_status(ToxWindow *self, Tox *m, int argc, char **argv)
{
    if (argc < 1 || argc > 2) {
        wprintw(self->window, "Wrong number of arguments.\n");
        return;
    }

    uint8_t *msg = NULL;

    if (argc == 2) {

        msg = argv[2];

        if (msg == NULL) {
            wprintw(self->window, "Invalid syntax.\n");
            return;
        }

        if (msg[0] != '\"') {
            wprintw(self->window, "Messages must be enclosed in quotes.\n");
            return;
        }
    }

    char *status = argv[1];

    if (status == NULL) {
        wprintw(self->window, "Invalid syntax.\n");
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
        wprintw(self->window, "Invalid status.\n");

    tox_set_userstatus(m, status_kind);
    prompt_update_status(self, status_kind);

    if (msg != NULL) {
        msg[strlen(++msg)-1] = L'\0';   /* remove opening and closing quotes */
        tox_set_statusmessage(m, msg, strlen(msg) + 1);
        prompt_update_statusmessage(self, msg);
    }
}

void cmd_note(ToxWindow *self, Tox *m, int argc, char **argv)
{
    if (argc != 1) {
        wprintw(self->window, "Wrong number of arguments.\n");
        return;
    }

    uint8_t *msg = argv[1];

    if (msg == NULL) {
        wprintw(self->window, "Invalid syntax.\n");
        return;
    }

    if (msg[0] != '\"') {
        wprintw(self->window, "Messages must be enclosed in quotes.\n");
        return;
    }

    msg[strlen(++msg)-1] = L'\0';

    tox_set_statusmessage(m, msg, strlen(msg) + 1);
    prompt_update_statusmessage(self, msg);
}

static void execute(ToxWindow *self, Tox *m, char *u_cmd)
{
    int newlines = 0;
    char cmd[MAX_STR_SIZE] = {'\0'};
    int i;

    for (i = 0; i < strlen(prompt_buf); ++i) {
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
                    wprintw(self->window, "Invalid command: did you forget an opening or closing \"?\n");
                    return;
                }
            }
        }
    }

    /* read arguments into array */
    char **cmdargs = malloc((numargs + 1) * sizeof(char *));
    if (!cmdargs) {
        wprintw(self->window, "Invalid command: too many arguments.\n");
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
            (commands[i].func)(self, m, numargs, cmdargs);
            free(cmdargs);
            return;
        }
    }

    /* no match */
    free(cmdargs);
    wprintw(self->window, "Invalid command.\n");
}

static void prompt_onKey(ToxWindow *self, Tox *m, wint_t key)
{
    /* Add printable characters to line */
    if (isprint(key)) {
        if (prompt_buf_pos == (sizeof(prompt_buf) - 1)) {
            wprintw(self->window, "\nToo Long.\n");
            prompt_buf_pos = 0;
            prompt_buf[0] = 0;
        } else if (!(prompt_buf_pos == 0) && (prompt_buf_pos < COLS)
                   && (prompt_buf_pos % (COLS - 3) == 0)) {
            prompt_buf[prompt_buf_pos++] = '\n';
        } else if (!(prompt_buf_pos == 0) && (prompt_buf_pos > COLS)
                   && ((prompt_buf_pos - (COLS - 3)) % (COLS) == 0)) {
            prompt_buf[prompt_buf_pos++] = '\n';
        }

        prompt_buf[prompt_buf_pos++] = key;
        prompt_buf[prompt_buf_pos] = 0;
    }

    /* RETURN key: execute command */
    else if (key == '\n') {
        wprintw(self->window, "\n");
        execute(self, m, prompt_buf);
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
    int i;
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
    cmd_help(self, m, 0, NULL);
    wclrtoeol(self->window);
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

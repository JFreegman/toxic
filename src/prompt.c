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
extern int store_data(Tox *m, char *path);

uint8_t pending_requests[MAX_STR_SIZE][TOX_CLIENT_ID_SIZE]; // XXX
uint8_t num_requests = 0; // XXX

static char prompt_buf[MAX_STR_SIZE] = {0};
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
void cmd_mynick(ToxWindow *, Tox *m, int, char **);
void cmd_quit(ToxWindow *, Tox *m, int, char **);
void cmd_status(ToxWindow *, Tox *m, int, char **);
void cmd_note(ToxWindow *, Tox *m, int, char **);

#define NUM_COMMANDS 14

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
    { "mynick",    cmd_mynick    },
    { "q",         cmd_quit      },
    { "quit",      cmd_quit      },
    { "status",    cmd_status    },
    { "note",      cmd_note      },
};

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
    if (argc != 1 && argc != 2) {
        wprintw(self->window, "Invalid syntax.\n");
        return;
    }

    uint8_t id_bin[TOX_FRIEND_ADDRESS_SIZE];
    char xx[3];
    uint32_t x;
    char *id;
    char *msg;
    int i, num;

    id = argv[1];

    if (argc == 2) {
        if (argv[2][0] != '\"') {
            wprintw(self->window, "Messages must be enclosed in quotes.\n");
            return;
        }

        msg = argv[2];
        msg[strlen(++msg)-1] = L'\0';
    } else
        msg = "";

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

    num = tox_addfriend(m, id_bin, (uint8_t *) msg, strlen(msg) + 1);

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
}

void cmd_connect(ToxWindow *self, Tox *m, int argc, char **argv)
{
    tox_IP_Port dht;
    char *ip, *port, *key;

    /* check arguments */
    if (argc != 3) {
      wprintw(self->window, "Invalid syntax.\n");
      return;
    }

    ip = argv[1];
    port = argv[2];
    key = argv[3];

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
    endwin();
    exit(0);
}

void cmd_help(ToxWindow *self, Tox *m, int argc, char **argv)
{
    wclear(self->window);
    wattron(self->window, COLOR_PAIR(2) | A_BOLD);
    wprintw(self->window, "Commands:\n");
    wattroff(self->window, A_BOLD);

    wprintw(self->window, "      connect <ip> <port> <key> : Connect to DHT server\n");
    wprintw(self->window, "      add <id> <message>        : Add friend with optional message\n");
    wprintw(self->window, "      status <type> <message>   : Set your status with optional note\n");
    wprintw(self->window, "      note  <message>           : Set a personal note\n");
    wprintw(self->window, "      nick <nickname>           : Set your nickname\n");
    wprintw(self->window, "      mynick                    : Print your current nickname\n");
    wprintw(self->window, "      accept <number>           : Accept friend request\n");
    wprintw(self->window, "      myid                      : Print your ID\n");
    wprintw(self->window, "      quit/exit                 : Exit Toxic\n");
    wprintw(self->window, "      help                      : Print this message again\n");
    wprintw(self->window, "      clear                     : Clear this window\n");

    wattron(self->window, A_BOLD);
    wprintw(self->window, " * Messages must be enclosed in quotation marks.\n");
    wprintw(self->window, " * Use the TAB key to navigate through the tabs.\n\n");
    wattroff(self->window, A_BOLD);

    wattroff(self->window, COLOR_PAIR(2));
}

void cmd_msg(ToxWindow *self, Tox *m, int argc, char **argv)
{
    char *id, *msg;

    /* check arguments */
    if (argc != 2) {
      wprintw(self->window, "Invalid syntax.\n");
      return;
    }

    id = argv[1];
    msg = argv[2];
    msg[strlen(++msg)-1] = L'\0';

    if (tox_sendmessage(m, atoi(id), (uint8_t *) msg, strlen(msg) + 1) == 0)
        wprintw(self->window, "Failed to send message.\n");
    else
        wprintw(self->window, "Message successfully sent.\n");
}

void cmd_myid(ToxWindow *self, Tox *m, int argc, char **argv)
{
    char id[TOX_FRIEND_ADDRESS_SIZE * 2 + 1] = {0};
    size_t i;
    uint8_t address[TOX_FRIEND_ADDRESS_SIZE];
    tox_getaddress(m, address);

    for (i = 0; i < TOX_FRIEND_ADDRESS_SIZE; ++i) {
        char xx[3];
        snprintf(xx, sizeof(xx), "%02X", address[i] & 0xff);
        strcat(id, xx);
    }

    wprintw(self->window, "%s\n", id);
}

void cmd_nick(ToxWindow *self, Tox *m, int argc, char **argv)
{
    char *nick;

    /* check arguments */
    if (argc != 1) {
      wprintw(self->window, "Invalid syntax.\n");
      return;
    }

    nick = argv[1];

    tox_setname(m, (uint8_t *) nick, strlen(nick) + 1);
    wprintw(self->window, "Nickname set to: %s\n", nick);

    if (store_data(m, DATA_FILE)) {
        wprintw(self->window, "\nCould not store Messenger data\n");
    }
}

void cmd_mynick(ToxWindow *self, Tox *m, int argc, char **argv)
{
    uint8_t *nick = malloc(TOX_MAX_NAME_LENGTH);
    tox_getselfname(m, nick, TOX_MAX_NAME_LENGTH);
    wprintw(self->window, "Current nickname: %s\n", nick);
    free(nick);
}

void cmd_status(ToxWindow *self, Tox *m, int argc, char **argv)
{
    if (argc != 1 && argc != 2) {
        wprintw(self->window, "Wrong number of arguments.\n");
        return;
    }

    char *status, *status_text;
    char *msg = NULL;

    /* check arguments */
    if (argc == 2) {
        msg = argv[2];
        if (msg[0] != '\"') {
            wprintw(self->window, "Messages must be enclosed in quotes.\n");
            return;
        }
    }


    status = argv[1];

    TOX_USERSTATUS status_kind;

    if (!strncmp(status, "online", strlen("online"))) {
        status_kind = TOX_USERSTATUS_NONE;
        status_text = "Online";
    } else if (!strncmp(status, "away", strlen("away"))) {
        status_kind = TOX_USERSTATUS_AWAY;
        status_text = "Away";
    } else if (!strncmp(status, "busy", strlen("busy"))) {
        status_kind = TOX_USERSTATUS_BUSY;
        status_text = "Busy";
    } else {
        wprintw(self->window, "Invalid status.\n");
        return;
    }

    wprintw(self->window, "Status set to: %s\n", status_text);
    tox_set_userstatus(m, status_kind);

    if (msg != NULL) {
        msg[strlen(++msg)-1] = L'\0';   /* remove opening and closing quotes */
        tox_set_statusmessage(m, (uint8_t *) msg, strlen(msg) + 1);
        wprintw(self->window, "Personal note set to: %s\n", msg);
    }
}

void cmd_note(ToxWindow *self, Tox *m, int argc, char **argv)
{
    if (argc != 1) {
        wprintw(self->window, "Wrong number of arguments.\n");
        return;
    }

    char *msg;

    /* check arguments */
    if (argv[1] && argv[1][0] != '\"') {
        wprintw(self->window, "Messages must be enclosed in quotes.\n");
        return;
    }

    msg = argv[1];
    msg[strlen(++msg)-1] = L'\0';

    tox_set_statusmessage(m, (uint8_t *) msg, strlen(msg) + 1);
    wprintw(self->window, "Personal note set to: %s\n", msg);
}

static void execute(ToxWindow *self, Tox *m, char *u_cmd)
{
    int newlines = 0;
    char cmd[MAX_STR_SIZE] = {0};
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
                    wprintw(self->window, "Invalid command: did you forget a closing \"?\n");
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
    if (!cmdargs[0])
        return;

    /* match input to command list */
    for (i = 0; i < NUM_COMMANDS; i++) {
        if (!strcmp(cmdargs[0], commands[i].name)) {
            (commands[i].func)(self, m, numargs, cmdargs);
            return;
        }
    }

    /* no match */
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
        if (prompt_buf_pos != 0) {
            prompt_buf[--prompt_buf_pos] = 0;
        }
    }
}

static void prompt_onDraw(ToxWindow *self, Tox *m)
{
    curs_set(1);
    int x, y;
    getyx(self->window, y, x);
    (void) x;
    int i;

    for (i = 0; i < (strlen(prompt_buf)); ++i) {
        if ((prompt_buf[i] == '\n') && (y != 0))
            --y;
    }

    wattron(self->window, COLOR_PAIR(1));
    mvwprintw(self->window, y, 0, "# ");
    wattroff(self->window, COLOR_PAIR(1));
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

ToxWindow new_prompt()
{
    ToxWindow ret;
    memset(&ret, 0, sizeof(ret));
    ret.onKey = &prompt_onKey;
    ret.onDraw = &prompt_onDraw;
    ret.onInit = &prompt_onInit;
    strcpy(ret.title, "[prompt]");
    return ret;
}

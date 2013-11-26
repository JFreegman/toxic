/*
 * Toxic -- Tox Curses Client
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "toxic_windows.h"
#include "misc_tools.h"

extern char *DATA_FILE;
extern ToxWindow *prompt;

extern uint8_t pending_frnd_requests[MAX_FRIENDS_NUM][TOX_CLIENT_ID_SIZE];
extern uint8_t num_frnd_requests;
extern int num_groupchats;

/* command functions */
void cmd_accept(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    /* check arguments */
    if (argc != 1) {
      wprintw(window, "Invalid syntax.\n");
      return;
    }

    int req = atoi(argv[1]);

    if ((req == 0 && strcmp(argv[1], "0"))|| req >= MAX_FRIENDS_NUM) {
        wprintw(window, "No pending friend request with that number.\n");
        return;
    }

    if (!strlen(pending_frnd_requests[req])) {
        wprintw(window, "No pending friend request with that number.\n");
        return;
    }

    int friendnum = tox_addfriend_norequest(m, pending_frnd_requests[req]);

    if (friendnum == -1)
        wprintw(window, "Failed to add friend.\n");
    else {
        wprintw(window, "Friend request accepted.\n");
        on_friendadded(m, friendnum);
    }

    memset(&pending_frnd_requests[req], 0, TOX_CLIENT_ID_SIZE);

    int i;

    for (i = num_frnd_requests; i > 0; --i) {
        if (!strlen(pending_frnd_requests[i-1]))
            break;
    }

    num_frnd_requests = i;
}

void cmd_add(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    if (argc < 1) {
        wprintw(window, "Invalid syntax.\n");
        return;
    }

    char *id = argv[1];
    uint8_t msg[MAX_STR_SIZE];

    if (argc > 1) {
        uint8_t *temp = argv[2];

        if (temp[0] != '\"') {
            wprintw(window, "Message must be enclosed in quotes.\n");
            return;
        }

        temp[strlen(++temp)-1] = L'\0';
        snprintf(msg, sizeof(msg), "%s", temp);
    } else {
        uint8_t selfname[TOX_MAX_NAME_LENGTH];
        tox_getselfname(m, selfname, TOX_MAX_NAME_LENGTH);
        snprintf(msg, sizeof(msg), "Hello, my name is %s. Care to Tox?", selfname);
    }

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

    int f_num = tox_addfriend(m, id_bin, msg, strlen(msg) + 1);

    switch (f_num) {
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
        wprintw(window, "Friend request has already been sent.\n");
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
        wprintw(window, "Friend request sent.\n");
        on_friendadded(m, f_num);
        break;
    }
}

void cmd_clear(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    wclear(window);
    wprintw(window, "\n\n");
}

void cmd_connect(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
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

    if (atoi(port) == 0) {
        wprintw(window, "Invalid syntax.\n");
        return;
    }

    uint8_t *binary_string = hex_string_to_bin(key);
    tox_bootstrap_from_address(m, ip, TOX_ENABLE_IPV6_DEFAULT,
                               htons(atoi(port)), binary_string);
    free(binary_string);
}

void cmd_groupchat(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    if (num_groupchats >= MAX_GROUPCHAT_NUM) {
        wprintw(window, "\nMaximum number of group chats has been reached.\n");
        return;
    }

    int groupnum = tox_add_groupchat(m);

    if (groupnum == -1) {
        wprintw(window, "Group chat instance failed to initialize.\n");
        return;
    }

    if (init_groupchat_win(prompt, m, groupnum) == -1) {
        wprintw(window, "Group chat window failed to initialize.\n");
        tox_del_groupchat(m, groupnum);
        return;
    }

    wprintw(window, "Group chat created as %d.\n", groupnum);
}

void cmd_myid(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
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

void cmd_nick(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    /* check arguments */
    if (argc < 1) {
      wprintw(window, "Invalid syntax.\n");
      return;
    }

    uint8_t *nick = argv[1];
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

void cmd_note(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    if (argc < 1) {
        wprintw(window, "Wrong number of arguments.\n");
        return;
    }

    uint8_t *msg = argv[1];

    if (msg[0] != '\"') {
        wprintw(window, "Note must be enclosed in quotes.\n");
        return;
    }

    msg[strlen(++msg)-1] = L'\0';
    uint16_t len = strlen(msg) + 1;
    tox_set_statusmessage(m, msg, len);

    prompt_update_statusmessage(prompt, msg, len);
}

void cmd_prompt_help(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    wclear(window);
    wattron(window, COLOR_PAIR(CYAN) | A_BOLD);
    wprintw(window, "\n\nGlobal commands:\n");
    wattroff(window, A_BOLD);

    wprintw(window, "      /add <id> <msg>            : Add friend with optional message\n");
    wprintw(window, "      /accept <n>                : Accept friend request\n");
    wprintw(window, "      /connect <ip> <port> <key> : Manually connect to a DHT server\n");
    wprintw(window, "      /status <type> <msg>       : Set your status with optional note\n");
    wprintw(window, "      /note <msg>                : Set a personal note\n");
    wprintw(window, "      /nick <nick>               : Set your nickname\n");
    wprintw(window, "      /groupchat                 : Create a group chat\n");
    wprintw(window, "      /myid                      : Print your ID\n");
    wprintw(window, "      /quit or /exit             : Exit Toxic\n");
    wprintw(window, "      /help                      : Print this message again\n");
    wprintw(window, "      /clear                     : Clear the window\n");

    wattron(window, A_BOLD);
    wprintw(window, " * Argument messages must be enclosed in quotation marks.\n");
    wprintw(window, " * Use TAB and Shift-TAB to navigate through the tabs.\n");
    wattroff(window, A_BOLD);

    wattroff(window, COLOR_PAIR(CYAN));
}

void cmd_quit(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    exit_toxic(m);
}

void cmd_status(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    uint8_t *msg = NULL;

    if (argc >= 2) {
        msg = argv[2];

        if (msg[0] != '\"') {
            wprintw(window, "Note must be enclosed in quotes.\n");
            return;
        }
    } else if (argc != 1) {
        wprintw(window, "Wrong number of arguments.\n");
        return;
    }

    char *status = argv[1];
    int len = strlen(status);
    char l_status[len+1];
    int i;

    for (i = 0; i <= len; ++i)
        l_status[i] = tolower(status[i]);

    TOX_USERSTATUS status_kind;

    if (!strcmp(l_status, "online"))
        status_kind = TOX_USERSTATUS_NONE;
    else if (!strcmp(l_status, "away"))
        status_kind = TOX_USERSTATUS_AWAY;
    else if (!strcmp(l_status, "busy"))
        status_kind = TOX_USERSTATUS_BUSY;
    else {
        wprintw(window, "Invalid status. Valid statuses are: online, busy and away.\n");
        return;
    }

    tox_set_userstatus(m, status_kind);
    prompt_update_status(prompt, status_kind);

    if (msg != NULL) {
        msg[strlen(++msg)-1] = L'\0';   /* remove opening and closing quotes */
        uint16_t len = strlen(msg) + 1;
        tox_set_statusmessage(m, msg, len);
        prompt_update_statusmessage(prompt, msg, len);
    }
}

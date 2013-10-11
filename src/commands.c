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
#include "commands.h"

extern char *DATA_FILE;

extern uint8_t pending_frnd_requests[MAX_FRIENDS_NUM][TOX_CLIENT_ID_SIZE];
extern uint8_t num_frnd_requests;

extern uint8_t pending_grp_requests[MAX_FRIENDS_NUM][TOX_CLIENT_ID_SIZE];

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
    if (argc < 1) {
        wprintw(window, "Invalid syntax.\n");
        return;
    }

    char *id = argv[1];

    if (id == NULL) {
        wprintw(window, "Invalid syntax.\n");
        return;
    }

    uint8_t *msg;

    if (argc > 1) {
        msg = argv[2];

        if (msg == NULL) {
            wprintw(window, "Invalid syntax.\n");
            return;
        }

        if (msg[0] != '\"') {
            wprintw(window, "Message must be enclosed in quotes.\n");
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

void cmd_file(WINDOW *window, ToxWindow *prompt, Tox *m, int argc, char **argv)
{
    if (argc < 1) {
        wprintw(window, "Wrong number of arguments.\n");
        return;
    }

    uint8_t filenum = atoi(argv[1]);

    if (filenum < 0 || filenum > MAX_FILENUMBER) {
        wprintw(window, "File transfer failed.\n");
        return;
    }

    int friendnum = pending_file_transfers[filenum];

    if (tox_file_sendcontrol(m, friendnum, 1, filenum, 0, 0, 0))
        wprintw(window, "Accepted file transfer %u. Saving file as %d.%u.bin.\n", filenum, friendnum, filenum);
    else
        wprintw(window, "File transfer failed.\n");
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

    wprintw(window, "Invited '%s' to group chat %d.\n", friendname, groupnum);
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

    if (num < 0 || num >= MAX_FRIENDS_NUM) {
        wprintw(window, "No pending group chat invite with that number.\n");
        return;
    }

    uint8_t *groupkey = pending_grp_requests[num];

    if (!strlen(groupkey)) {
        wprintw(window, "No pending group chat invite with that number.\n");
        return;
    }

    int groupnum = tox_join_groupchat(m, num, groupkey);

    if (groupnum == -1) {
        wprintw(window, "Group chat instance failed to initialize.\n");
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
    if (argc < 2) {
      wprintw(window, "Invalid syntax.\n");
      return;
    }

    uint8_t *name = argv[1];
    uint8_t *msg = argv[2];

    if (name == NULL || msg == NULL) {
      wprintw(window, "Invalid syntax.\n");
      return;
    }

    if (msg[0] != '\"') {
        wprintw(window, "Messages must be enclosed in quotes.\n");
        return;
    }

    msg[strlen(++msg)-1] = L'\0';
    int friendnum = get_friendnum(name);

    if (friendnum == -1) {
        wprintw(window, "Friend '%s' not found.\n", name);
        return;
    }

    if (tox_sendmessage(m, friendnum, msg, strlen(msg) + 1) == 0)
        wprintw(window, "Failed to send message.\n");
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

void cmd_note(WINDOW *window, ToxWindow *prompt, Tox *m, int argc, char **argv)
{
    if (argc < 1) {
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

void cmd_quit(WINDOW *window, ToxWindow *prompt, Tox *m, int argc, char **argv)
{
    exit_toxic(m);
}

void cmd_sendfile(WINDOW *window, ToxWindow *prompt, Tox *m, int argc, char **argv)
{
    if (argc < 1) {
        wprintw(window, "Wrong number of arguments.\n");
        return;
    }

    uint8_t *friendname = argv[1];

    int friendnum = get_friendnum(friendname);

    if (friendnum == -1) {
        wprintw(window, "Friend '%s' not found.\n", friendname);
        return;
    }

    if (friendname[0] == '\"')
        friendname[strlen(++friendname)-1] = L'\0';

    uint8_t *filename = argv[2];
    int filename_len = strlen(filename);

    if (filename[0] != '\"') {
        wprintw(window, "File name must be enclosed in quotes.\n");
        return;
    }

    filename[strlen(++filename)-1] = L'\0';

    if (filename_len > MAX_STR_SIZE) {
        wprintw(window, "File path exceeds character limit.\n");
        return;
    }

    FILE *file_to_send = fopen(filename, "r");

    if (file_to_send == NULL) {
        wprintw(window, "File '%s' not found.\n", filename);
        return;
    }

    fseek(file_to_send, 0, SEEK_END);
    uint64_t filesize = ftell(file_to_send);
    fseek(file_to_send, 0, SEEK_SET);

    int filenum = tox_new_filesender(m, friendnum, filesize, filename, filename_len + 1);

    if (filenum == -1) {
        wprintw(window, "Error sending file\n");
        return;
    }

    memcpy(file_senders[num_file_senders].filename, filename, filename_len + 1);
    memcpy(file_senders[num_file_senders].friendname, friendname, strlen(friendname) + 1);
    file_senders[num_file_senders].file = file_to_send;
    file_senders[num_file_senders].filenum = filenum;
    file_senders[num_file_senders].friendnum = friendnum;
    file_senders[num_file_senders].piecelen = fread(file_senders[num_file_senders].nextpiece, 1,
                                                    tox_filedata_size(m, friendnum), file_to_send);


    wprintw(window, "Sending file '%s' to %s...\n", filename, friendname);
    ++num_file_senders;
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

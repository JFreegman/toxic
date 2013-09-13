/*
 * Toxic -- Tox Curses Client
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>
#include <limits.h>

#include "toxic_windows.h"
#include "friendlist.h"
#include "chat.h"

#define CURS_Y_OFFSET 3

extern char *DATA_FILE;
extern int store_data(Tox *m, char *path);

typedef struct {
    wchar_t line[MAX_STR_SIZE];
    size_t pos;
    WINDOW *history;
    WINDOW *linewin;
} ChatContext;

struct tm *get_time(void)
{
    struct tm *timeinfo;
    time_t now;
    time(&now);
    timeinfo = localtime(&now);
    return timeinfo;
}

static void chat_onMessage(ToxWindow *self, Tox *m, int num, uint8_t *msg, uint16_t len)
{
    if (self->friendnum != num)
        return;

    ChatContext *ctx = (ChatContext *) self->x;

    struct tm *timeinfo = get_time();

    uint8_t nick[TOX_MAX_NAME_LENGTH] = {'\0'};
    tox_getname(m, num, nick);

    wattron(ctx->history, COLOR_PAIR(CYAN));
    wprintw(ctx->history, "[%02d:%02d:%02d] ", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    wattroff(ctx->history, COLOR_PAIR(CYAN));
    wattron(ctx->history, COLOR_PAIR(4));
    wprintw(ctx->history, "%s: ", nick);
    wattroff(ctx->history, COLOR_PAIR(4));
    wprintw(ctx->history, "%s\n", msg);

    self->blink = true;
    beep();
}

void chat_onConnectionChange(ToxWindow *self, Tox *m, int num, uint8_t status)
{
    if (self->friendnum != num)
        return;

    StatusBar *statusbar = (StatusBar *) self->s;
    statusbar->is_online = status == 1 ? true : false;
}

static void chat_onAction(ToxWindow *self, Tox *m, int num, uint8_t *action, uint16_t len)
{
    if (self->friendnum != num)
        return;

    ChatContext *ctx = (ChatContext *) self->x;
    struct tm *timeinfo = get_time();

    uint8_t nick[TOX_MAX_NAME_LENGTH] = {'\0'};
    tox_getname(m, num, nick);

    wattron(ctx->history, COLOR_PAIR(CYAN));
    wprintw(ctx->history, "[%02d:%02d:%02d] ", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    wattroff(ctx->history, COLOR_PAIR(CYAN));

    wattron(ctx->history, COLOR_PAIR(YELLOW));
    wprintw(ctx->history, "* %s %s\n", nick, action);
    wattroff(ctx->history, COLOR_PAIR(YELLOW));

    self->blink = true;
    beep();
}

static void chat_onNickChange(ToxWindow *self, int num, uint8_t *nick, uint16_t len)
{
    if (self->friendnum != num)
        return;

    snprintf(self->name, sizeof(self->name), "%s", nick);
}

static void chat_onStatusChange(ToxWindow *self, Tox *m, int num, TOX_USERSTATUS status)
{
    if (self->friendnum != num)
        return;

    StatusBar *statusbar = (StatusBar *) self->s;
    statusbar->status = status;
}

static void chat_onStatusMessageChange(ToxWindow *self, int num, uint8_t *status, uint16_t len)
{
    if (self->friendnum != num)
        return;

    StatusBar *statusbar = (StatusBar *) self->s;
    statusbar->statusmsg_len = len;
    snprintf(statusbar->statusmsg, len, "%s", status);
}

/* check that the string has one non-space character */
int string_is_empty(char *string)
{
    int rc = 0;
    char *copy = strdup(string);
    rc = ((strtok(copy, " ") == NULL) ? 1 : 0);
    free(copy);
    return rc;
}

/* convert wide characters to null terminated string */
static uint8_t *wcs_to_char(wchar_t *string)
{
    size_t len = 0;
    char *ret = NULL;

    len = wcstombs(NULL, string, 0);
    if (len != (size_t) -1) {
        len++;
        ret = malloc(len);
        if (ret != NULL)
            wcstombs(ret, string, len);
    } else {
        ret = malloc(2);
        if (ret != NULL) {
            ret[0] = ' ';
            ret[1] = '\0';
        }
    }

    if (ret == NULL) {
        endwin();
        fprintf(stderr, "malloc() failed. Aborting...\n");
        exit(EXIT_FAILURE);
    }

    return ret;
}

/* convert a wide char to null terminated string */
static char *wc_to_char(wchar_t ch)
{
    int len = 0;
    static char ret[MB_LEN_MAX + 1];

    len = wctomb(ret, ch);
    if (len == -1) {
        ret[0] = ' ';
        ret[1] = '\0';
    } else {
        ret[len] = '\0';
    }

    return ret;
}

static void print_help(ChatContext *self)
{
    wattron(self->history, COLOR_PAIR(CYAN) | A_BOLD);
    wprintw(self->history, "Commands:\n");
    wattroff(self->history, A_BOLD);

    wprintw(self->history, "      /status <type> <message>   : Set your status with optional note\n");
    wprintw(self->history, "      /note <message>            : Set a personal note\n");
    wprintw(self->history, "      /nick <nickname>           : Set your nickname\n");
    wprintw(self->history, "      /me <action>               : Do an action\n");
    wprintw(self->history, "      /myid                      : Print your ID\n");
    wprintw(self->history, "      /clear                     : Clear the screen\n");
    wprintw(self->history, "      /close                     : Close the current chat window\n");
    wprintw(self->history, "      /quit or /exit             : Exit Toxic\n");
    wprintw(self->history, "      /help                      : Print this message again\n\n");

    wattroff(self->history, COLOR_PAIR(CYAN));
}

static void execute(ToxWindow *self, ChatContext *ctx, StatusBar *statusbar, Tox *m, char *cmd)
{
    if (!strcmp(cmd, "/clear") || !strcmp(cmd, "/c")) {
        wclear(self->window);
        wclear(ctx->history);
        wprintw(ctx->history, "\n\n");
        int x, y;
        getmaxyx(self->window, y, x);
        (void) x;
        wmove(self->window, y - CURS_Y_OFFSET, 0);
    }

    else if (!strcmp(cmd, "/help") || !strcmp(cmd, "/h"))
        print_help(ctx);

    else if (!strcmp(cmd, "/quit") || !strcmp(cmd, "/exit") || !strcmp(cmd, "/q")) {
        exit_toxic(m);
    }

    else if (!strncmp(cmd, "/me ", strlen("/me "))) {
        struct tm *timeinfo = get_time();
        uint8_t *action = strchr(cmd, ' ');

        if (action == NULL) {
            wprintw(self->window, "Invalid syntax.\n");
            return;
        }

        action++;

        wattron(ctx->history, COLOR_PAIR(CYAN));
        wprintw(ctx->history, "[%02d:%02d:%02d] ", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
        wattroff(ctx->history, COLOR_PAIR(CYAN));

        uint8_t selfname[TOX_MAX_NAME_LENGTH];
        tox_getselfname(m, selfname, TOX_MAX_NAME_LENGTH);

        wattron(ctx->history, COLOR_PAIR(YELLOW));
        wprintw(ctx->history, "* %s %s\n", selfname, action);
        wattroff(ctx->history, COLOR_PAIR(YELLOW));

        if (!statusbar->is_online
                || tox_sendaction(m, self->friendnum, action, strlen(action) + 1) == 0) {
            wattron(ctx->history, COLOR_PAIR(RED));
            wprintw(ctx->history, " * Failed to send action\n");
            wattroff(ctx->history, COLOR_PAIR(RED));
        }
    }

    else if (!strncmp(cmd, "/status ", strlen("/status "))) {
        char *status = strchr(cmd, ' ');

        if (status == NULL) {
            wprintw(ctx->history, "Invalid syntax.\n");
            return;
        }

        status++;
        TOX_USERSTATUS status_kind;

        if (!strncmp(status, "online", strlen("online"))) {
            status_kind = TOX_USERSTATUS_NONE;
            wprintw(ctx->history, "Status set to: ");
            wattron(ctx->history, COLOR_PAIR(GREEN) | A_BOLD);
            wprintw(ctx->history, "[Online]\n");
            wattroff(ctx->history, COLOR_PAIR(GREEN) | A_BOLD);
        }

        else if (!strncmp(status, "away", strlen("away"))) {
            status_kind = TOX_USERSTATUS_AWAY;
            wprintw(ctx->history, "Status set to: ");
            wattron(ctx->history, COLOR_PAIR(YELLOW) | A_BOLD);
            wprintw(ctx->history, "[Away]\n");
            wattroff(ctx->history, COLOR_PAIR(YELLOW) | A_BOLD);
        }

        else if (!strncmp(status, "busy", strlen("busy"))) {
            status_kind = TOX_USERSTATUS_BUSY;
            wprintw(ctx->history, "Status set to: ");
            wattron(ctx->history, COLOR_PAIR(RED) | A_BOLD);
            wprintw(ctx->history, "[Busy]\n");
            wattroff(ctx->history, COLOR_PAIR(RED) | A_BOLD);
        }

        else {
            wprintw(ctx->history, "Invalid status.\n");
            return;
        }

        tox_set_userstatus(m, status_kind);
        prompt_update_status(self->prompt, status_kind); 

        uint8_t *msg = strchr(status, ' ');
        if (msg != NULL) {
            msg++;
            uint16_t len = strlen(msg) + 1;
            tox_set_statusmessage(m, msg, len);
            prompt_update_statusmessage(self->prompt, msg, len);
            wprintw(ctx->history, "Personal note set to: %s\n", msg);
        }
    }

    else if (!strncmp(cmd, "/note ", strlen("/note "))) {
        uint8_t *msg = strchr(cmd, ' ');
        msg++;
        uint16_t len = strlen(msg) + 1;
        tox_set_statusmessage(m, msg, len);
        prompt_update_statusmessage(self->prompt, msg, len);
        wprintw(ctx->history, "Personal note set to: %s\n", msg);
    }

    else if (!strncmp(cmd, "/nick ", strlen("/nick "))) {
        uint8_t *nick = strchr(cmd, ' ');

        if (nick == NULL) {
            wprintw(ctx->history, "Invalid syntax.\n");
            return;
        }

        int len = strlen(++nick);

        if (len > TOXIC_MAX_NAME_LENGTH) {
            nick[TOXIC_MAX_NAME_LENGTH] = L'\0';
            len = TOXIC_MAX_NAME_LENGTH;
        }

        tox_setname(m, nick, len+1);
        prompt_update_nick(self->prompt, nick);
        wprintw(ctx->history, "Nickname set to: %s\n", nick);
    }

    else if (!strcmp(cmd, "/myid")) {
        char id[TOX_FRIEND_ADDRESS_SIZE * 2 + 1] = {'\0'};
        int i;
        uint8_t address[TOX_FRIEND_ADDRESS_SIZE];
        tox_getaddress(m, address);

        for (i = 0; i < TOX_FRIEND_ADDRESS_SIZE; i++) {
            char xx[3];
            snprintf(xx, sizeof(xx), "%02X",  address[i] & 0xff);
            strcat(id, xx);
        }

        wprintw(ctx->history, "%s\n", id);
    }

    else
        wprintw(ctx->history, "Invalid command.\n");
}

static void chat_onKey(ToxWindow *self, Tox *m, wint_t key)
{
    ChatContext *ctx = (ChatContext *) self->x;
    StatusBar *statusbar = (StatusBar *) self->s;

    struct tm *timeinfo = get_time();

    int x, y, y2, x2;
    getyx(self->window, y, x);
    getmaxyx(self->window, y2, x2);

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

    /* BACKSPACE key: Remove one character from line */
    else if (key == 0x107 || key == 0x8 || key == 0x7f) {
        if (ctx->pos > 0) {
            ctx->line[--ctx->pos] = L'\0';

            if (x == 0)
                mvwdelch(self->window, y - 1, x2 - 1);
            else
                mvwdelch(self->window, y, x - 1);
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
            if (close_win = !strncmp(line, "/close", strlen("/close"))) {
                int f_num = self->friendnum;
                delwin(ctx->linewin);
                delwin(statusbar->topline);
                del_window(self);
                disable_chatwin(f_num);
            } else
                execute(self, ctx, statusbar, m, line);
        } else {
            /* make sure the string has at least non-space character */
            if (!string_is_empty(line)) {
                uint8_t selfname[TOX_MAX_NAME_LENGTH];
                tox_getselfname(m, selfname, TOX_MAX_NAME_LENGTH);

                wattron(ctx->history, COLOR_PAIR(CYAN));
                wprintw(ctx->history, "[%02d:%02d:%02d] ", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
                wattroff(ctx->history, COLOR_PAIR(CYAN));
                wattron(ctx->history, COLOR_PAIR(GREEN));
                wprintw(ctx->history, "%s: ", selfname);
                wattroff(ctx->history, COLOR_PAIR(GREEN));
                wprintw(ctx->history, "%s\n", line);

                if (!statusbar->is_online
                        || tox_sendmessage(m, self->friendnum, line, strlen(line) + 1) == 0) {
                    wattron(ctx->history, COLOR_PAIR(RED));
                    wprintw(ctx->history, " * Failed to send message.\n");
                    wattroff(ctx->history, COLOR_PAIR(RED));
                }
            }
        }

        if (close_win) {
            free(ctx);
            free(statusbar);
        } else {
            ctx->line[0] = L'\0';
            ctx->pos = 0;
        }

        free(line);
    }
}

static void chat_onDraw(ToxWindow *self, Tox *m)
{
    curs_set(1);

    int x, y;
    getmaxyx(self->window, y, x);

    ChatContext *ctx = (ChatContext *) self->x;

    /* Draw status bar */
    StatusBar *statusbar = (StatusBar *) self->s;
    mvwhline(statusbar->topline, 1, 0, '-', x);
    wmove(statusbar->topline, 0, 0);

    /* Draw name, status and note in statusbar */
    if (statusbar->is_online) {
        char *status_text = "Unknown";
        int colour = WHITE;

        TOX_USERSTATUS status = statusbar->status;

        switch(status) {
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
        wprintw(statusbar->topline, " %s ", self->name);
        wattroff(statusbar->topline, A_BOLD);
        wattron(statusbar->topline, COLOR_PAIR(colour) | A_BOLD);
        wprintw(statusbar->topline, "[%s]", status_text);
        wattroff(statusbar->topline, COLOR_PAIR(colour) | A_BOLD);

    } else {
        wattron(statusbar->topline, A_BOLD);
        wprintw(statusbar->topline, " %s ", self->name);
        wattroff(statusbar->topline, A_BOLD);
        wprintw(statusbar->topline, "[Offline]");
    }

    /* Truncate note if it doesn't fit in statusbar */
    uint16_t maxlen = x - getcurx(statusbar->topline) - 5;
    if (statusbar->statusmsg_len > maxlen) {
        statusbar->statusmsg[maxlen] = '\0';
        statusbar->statusmsg_len = maxlen;
    }

    wattron(statusbar->topline, A_BOLD);
    wprintw(statusbar->topline, " | %s |", statusbar->statusmsg);
    wattroff(statusbar->topline, A_BOLD);

    wprintw(statusbar->topline, "\n");

    mvwhline(ctx->linewin, 0, 0, '_', x);
    wrefresh(self->window);
}

static void chat_onInit(ToxWindow *self, Tox *m)
{
    int x, y;
    getmaxyx(self->window, y, x);

    /* Init statusbar info */
    StatusBar *statusbar = (StatusBar *) self->s;
    statusbar->status = tox_get_userstatus(m, self->friendnum);
    statusbar->is_online = tox_get_friend_connectionstatus(m, self->friendnum) == 1;

    uint8_t statusmsg[TOX_MAX_STATUSMESSAGE_LENGTH] = {'\0'};
    tox_copy_statusmessage(m, self->friendnum, statusmsg, TOX_MAX_STATUSMESSAGE_LENGTH);
    snprintf(statusbar->statusmsg, sizeof(statusbar->statusmsg), "%s", statusmsg);
    statusbar->statusmsg_len = tox_get_statusmessage_size(m, self->friendnum);

    /* Init subwindows */
    ChatContext *ctx = (ChatContext *) self->x;
    statusbar->topline = subwin(self->window, 2, x, 0, 0);
    ctx->history = subwin(self->window, y-3, x, 0, 0);
    scrollok(ctx->history, 1);
    ctx->linewin = subwin(self->window, 0, x, y-4, 0);
    wprintw(ctx->history, "\n\n");
    print_help(ctx);
    wmove(self->window, y - CURS_Y_OFFSET, 0);
}

ToxWindow new_chat(Tox *m, ToxWindow *prompt, int friendnum)
{
    ToxWindow ret;
    memset(&ret, 0, sizeof(ret));

    ret.onKey = &chat_onKey;
    ret.onDraw = &chat_onDraw;
    ret.onInit = &chat_onInit;
    ret.onMessage = &chat_onMessage;
    ret.onConnectionChange = &chat_onConnectionChange;
    ret.onNickChange = &chat_onNickChange;
    ret.onStatusChange = &chat_onStatusChange;
    ret.onStatusMessageChange = &chat_onStatusMessageChange;
    ret.onAction = &chat_onAction;

    uint8_t name[TOX_MAX_NAME_LENGTH] = {'\0'};
    tox_getname(m, friendnum, name);
    snprintf(ret.name, sizeof(ret.name), "%s", name);

    ChatContext *x = calloc(1, sizeof(ChatContext));
    StatusBar *s = calloc(1, sizeof(StatusBar));

    if (s != NULL && x != NULL) {
        ret.x = x;
        ret.s = s;
    } else {
        endwin();
        fprintf(stderr, "calloc() failed. Aborting...\n");
        exit(EXIT_FAILURE);
    }

    ret.prompt = prompt;
    ret.friendnum = friendnum;

    return ret;
}

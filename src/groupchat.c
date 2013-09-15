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

#include "toxic_windows.h"
#include "chat.h"

static GroupChat groupchats[MAX_GROUPCHAT_NUM];
static int group_chat_index = 0;

ToxWindow new_groupchat(Tox *m, ToxWindow *prompt, int groupnum);

extern char *DATA_FILE;
extern int store_data(Tox *m, char *path);

int get_num_groupchats(void)
{
    int count = 0;
    int i;

    for (i = 0; i < group_chat_index; ++i) {
        if (groupchats[i].active)
            ++count;
    }

    return count;
}

int init_groupchat_win(ToxWindow *prompt, Tox *m)
{
    int i;

    for (i = 0; i <= group_chat_index; ++i) {
        if (!groupchats[i].active) {
            groupchats[i].active = true;
            groupchats[i].chatwin = add_window(m, new_groupchat(m, prompt, i));
            set_active_window(groupchats[i].chatwin);

            if (i == group_chat_index)
                ++group_chat_index;

            return 0;
        }
    }

    return -1;
}

static void close_groupchatwin(Tox *m, int groupnum)
{
    tox_del_groupchat(m, groupnum);
    memset(&(groupchats[groupnum]), 0, sizeof(GroupChat));

    int i;

    for (i = group_chat_index; i > 0; --i) {
        if (groupchats[i-1].active)
            break;
    }

    group_chat_index = i;
}

static void groupchat_onGroupMessage(ToxWindow *self, Tox *m, int groupnum, uint8_t *msg, uint16_t len)
{
    if (self->num != groupnum)
        return;

    ChatContext *ctx = (ChatContext *) self->chatwin;
    struct tm *timeinfo = get_time();

    // uint8_t nick[TOX_MAX_NAME_LENGTH] = {'\0'};
    // tox_getname(m, num, nick);

    wattron(ctx->history, COLOR_PAIR(CYAN));
    wprintw(ctx->history, "[%02d:%02d:%02d] ", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    wattroff(ctx->history, COLOR_PAIR(CYAN));
    wattron(ctx->history, COLOR_PAIR(4));
    wprintw(ctx->history, "Toxicle: ");
    wattroff(ctx->history, COLOR_PAIR(4));
    wprintw(ctx->history, "%s\n", msg);

    self->blink = true;
    beep();
}

static void groupchat_onKey(ToxWindow *self, Tox *m, wint_t key)
{
    ChatContext *ctx = (ChatContext *) self->chatwin;
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
                set_active_window(0);
                int group_num = groupchats[self->num].chatwin;
                delwin(ctx->linewin);
                del_window(self);
                close_groupchatwin(m, group_num);
            } //else
                //execute(self, ctx, statusbar, m, line);
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
    mvwhline(ctx->linewin, 0, 0, '_', x);
    wrefresh(self->window);
}

static void groupchat_onInit(ToxWindow *self, Tox *m)
{
    int x, y;
    ChatContext *ctx = (ChatContext *) self->chatwin;
    getmaxyx(self->window, y, x);
    ctx->history = subwin(self->window, y-4, x, 0, 0);
    scrollok(ctx->history, 1);
    ctx->linewin = subwin(self->window, 2, x, y-4, 0);
    // print_help(ctx);
    wmove(self->window, y - CURS_Y_OFFSET, 0);
}

ToxWindow new_groupchat(Tox *m, ToxWindow *prompt, int groupnum)
{
    ToxWindow ret;
    memset(&ret, 0, sizeof(ret));

    ret.onKey = &groupchat_onKey;
    ret.onDraw = &groupchat_onDraw;
    ret.onInit = &groupchat_onInit;
    ret.onGroupMessage = &groupchat_onGroupMessage;
    // ret.onNickChange = &groupchat_onNickChange;
    // ret.onStatusChange = &groupchat_onStatusChange;
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

    ret.prompt = prompt;
    ret.num = groupnum;

    return ret;
}

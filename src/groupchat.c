/*  groupchat.c
 *
 *
 *  Copyright (C) 2014 Toxic All Rights Reserved.
 *
 *  This file is part of Toxic.
 *
 *  Toxic is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Toxic is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Toxic.  If not, see <http://www.gnu.org/licenses/>.
 *
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
#include "prompt.h"
#include "toxic_strings.h"
#include "log.h"

extern char *DATA_FILE;
extern int store_data(Tox *m, char *path);

static GroupChat groupchats[MAX_WINDOWS_NUM];
static int max_groupchat_index = 0;

/* temporary until group chats have unique commands */
extern const uint8_t glob_cmd_list[AC_NUM_GLOB_COMMANDS][MAX_CMDNAME_SIZE];

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

            /* temp fix */
            memcpy(&groupchats[i].oldpeer_names[0], UNKNOWN_NAME, sizeof(UNKNOWN_NAME));

            set_active_window(groupchats[i].chatwin);

            if (i == max_groupchat_index)
                ++max_groupchat_index;

            return 0;
        }
    }

    return -1;
}

void kill_groupchat_window(ToxWindow *self)
{
    ChatContext *ctx = self->chatwin;

    log_disable(ctx->log);
    delwin(ctx->linewin);
    del_window(self);
    free(ctx->log);
    free(ctx);
}

static void close_groupchat(ToxWindow *self, Tox *m, int groupnum)
{
    set_active_window(0);
    tox_del_groupchat(m, groupnum);

    free(groupchats[groupnum].peer_names);
    free(groupchats[groupnum].oldpeer_names);
    memset(&groupchats[groupnum], 0, sizeof(GroupChat));

    int i;

    for (i = max_groupchat_index; i > 0; --i) {
        if (groupchats[i-1].active)
            break;
    }

    max_groupchat_index = i;
    kill_groupchat_window(self);
}

static void print_groupchat_help(ChatContext *ctx)
{
    wattron(ctx->history, COLOR_PAIR(CYAN) | A_BOLD);
    wprintw(ctx->history, "Group chat commands:\n");
    wattroff(ctx->history, COLOR_PAIR(CYAN) | A_BOLD);

    wprintw(ctx->history, "    /add <id> <msg>     : Add friend with optional message\n");
    wprintw(ctx->history, "    /status <type> <msg>: Set your status with optional note\n");
    wprintw(ctx->history, "    /note <msg>         : Set a personal note\n");
    wprintw(ctx->history, "    /nick <nick>        : Set your nickname\n");
    wprintw(ctx->history, "    /groupchat          : Create a group chat\n");
    wprintw(ctx->history, "    /log <on> or <off>  : Enable/disable logging\n");
    wprintw(ctx->history, "    /close              : Close the current group chat\n");
    wprintw(ctx->history, "    /help               : Print this message again\n");
    wprintw(ctx->history, "    /help global        : Show a list of global commands\n");
    
    wattron(ctx->history, COLOR_PAIR(CYAN) | A_BOLD);
    wprintw(ctx->history, " * Argument messages must be enclosed in quotation marks.\n");
    wprintw(ctx->history, " * Scroll peer list with the Page Up/Page Down keys.\n\n");
    wattroff(ctx->history, COLOR_PAIR(CYAN) | A_BOLD);
    wattron(ctx->history, COLOR_PAIR(WHITE) | A_BOLD);
    wprintw(ctx->history, "    Notice, some friends will be missing names while finding peers\n\n");
    wattroff(ctx->history, COLOR_PAIR(WHITE) | A_BOLD);
}

static void groupchat_onGroupMessage(ToxWindow *self, Tox *m, int groupnum, int peernum,
                                     uint8_t *msg, uint16_t len)
{
    if (self->num != groupnum)
        return;

    ChatContext *ctx = self->chatwin;

    uint8_t nick[TOX_MAX_NAME_LENGTH] = {'\0'};
    tox_group_peername(m, groupnum, peernum, nick);
    nick[TOXIC_MAX_NAME_LENGTH] = '\0';    /* enforce client max name length */

    /* check if message contains own name and alert appropriately */
    int alert_type = WINDOW_ALERT_1;
    bool beep = false;

    uint8_t selfnick[TOX_MAX_NAME_LENGTH] = {'\0'};
    tox_get_self_name(m, selfnick);
    int nick_clr = strcmp(nick, selfnick) == 0 ? GREEN : CYAN;

    bool nick_match = strcasestr(msg, selfnick) && strncmp(selfnick, nick, TOXIC_MAX_NAME_LENGTH);

    if (nick_match) {
        alert_type = WINDOW_ALERT_0;
        beep = true;
        nick_clr = RED;
    }

    alert_window(self, alert_type, beep);

    print_time(ctx->history);
    wattron(ctx->history, COLOR_PAIR(nick_clr));
    wprintw(ctx->history, "%s: ", nick);
    wattroff(ctx->history, COLOR_PAIR(nick_clr));
    
    if (msg[0] == '>') {
        wattron(ctx->history, COLOR_PAIR(GREEN));
        wprintw(ctx->history, "%s\n", msg);
        wattroff(ctx->history, COLOR_PAIR(GREEN));
    } else {
        wprintw(ctx->history, "%s\n", msg);
    }

    write_to_log(msg, nick, ctx->log, false);
}

static void groupchat_onGroupAction(ToxWindow *self, Tox *m, int groupnum, int peernum, uint8_t *action,
                                    uint16_t len)
{
    if (self->num != groupnum)
        return;

    ChatContext *ctx = self->chatwin;

    /* check if message contains own name and alert appropriately */
    int alert_type = WINDOW_ALERT_1;
    bool beep = false;

    uint8_t selfnick[TOX_MAX_NAME_LENGTH] = {'\0'};
    tox_get_self_name(m, selfnick);

    bool nick_match = strcasestr(action, selfnick);

    if (nick_match) {
        alert_type = WINDOW_ALERT_0;
        beep = true;
    }

    alert_window(self, alert_type, beep);

    uint8_t nick[TOX_MAX_NAME_LENGTH] = {'\0'};
    tox_group_peername(m, groupnum, peernum, nick);
    nick[TOXIC_MAX_NAME_LENGTH] = '\0';    /* enforce client max name length */

    print_time(ctx->history);
    wattron(ctx->history, COLOR_PAIR(YELLOW));
    wprintw(ctx->history, "* %s %s\n", nick, action);
    wattroff(ctx->history, COLOR_PAIR(YELLOW));

    write_to_log(action, nick, ctx->log, true);
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
        if (string_is_empty(tmp_peerlist[i])) {
            memcpy(&groupchats[gnum].peer_names[i*N], UNKNOWN_NAME, sizeof(UNKNOWN_NAME));
        } else {
            memcpy(&groupchats[gnum].peer_names[i*N], tmp_peerlist[i], N);
            groupchats[gnum].peer_names[i*N+TOXIC_MAX_NAME_LENGTH] = '\0';
        }
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

    if (change != TOX_CHAT_CHANGE_PEER_ADD)
        memcpy(oldpeername, &groupchats[groupnum].oldpeer_names[peernum*TOX_MAX_NAME_LENGTH], 
               sizeof(oldpeername));

    /* Update name lists */
    uint8_t tmp_peerlist[num_peers][TOX_MAX_NAME_LENGTH];
    tox_group_get_names(m, groupnum, tmp_peerlist, num_peers);
    copy_peernames(groupnum, num_peers, tmp_peerlist);

    /* get current peername then sort namelist */
    uint8_t peername[TOX_MAX_NAME_LENGTH] = {0};
    memcpy(peername, &groupchats[groupnum].peer_names[peernum*TOX_MAX_NAME_LENGTH], sizeof(peername));

    qsort(groupchats[groupnum].peer_names, groupchats[groupnum].num_peers, TOX_MAX_NAME_LENGTH, qsort_strcasecmp_hlpr);

    ChatContext *ctx = self->chatwin;
    print_time(ctx->history);

    const uint8_t *event;

    switch (change) {
    case TOX_CHAT_CHANGE_PEER_ADD:
        event = "has joined the room";

        wattron(ctx->history, COLOR_PAIR(GREEN));
        wattron(ctx->history, A_BOLD);
        wprintw(ctx->history, "* %s", peername);
        wattroff(ctx->history, A_BOLD);
        wprintw(ctx->history, " %s\n", event);
        wattroff(ctx->history, COLOR_PAIR(GREEN));

        write_to_log(event, peername, ctx->log, true);
        break;

    case TOX_CHAT_CHANGE_PEER_DEL:
        event = "has left the room";

        wattron(ctx->history, A_BOLD);
        wprintw(ctx->history, "* %s", oldpeername);
        wattroff(ctx->history, A_BOLD);
        wprintw(ctx->history, " %s\n", event);

        if (groupchats[self->num].side_pos > 0)
            --groupchats[self->num].side_pos;

        write_to_log(event, oldpeername, ctx->log, true);
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

        uint8_t tmp_event[TOXIC_MAX_NAME_LENGTH + 32];
        snprintf(tmp_event, sizeof(tmp_event), "is now known as %s", peername);
        write_to_log(tmp_event, oldpeername, ctx->log, true);
        break;
    }

    alert_window(self, WINDOW_ALERT_2, false);
}

static void send_group_action(ToxWindow *self, ChatContext *ctx, Tox *m, uint8_t *action) {
    if (action == NULL) {
        wprintw(ctx->history, "Invalid syntax.\n");
        return;
    }

    if (tox_group_action_send(m, self->num, action, strlen(action) + 1) == -1) {
        wattron(ctx->history, COLOR_PAIR(RED));
        wprintw(ctx->history, " * Failed to send action\n");
        wattroff(ctx->history, COLOR_PAIR(RED));
    }
}

static void groupchat_onKey(ToxWindow *self, Tox *m, wint_t key)
{
    ChatContext *ctx = self->chatwin;

    int x, y, y2, x2;
    getyx(self->window, y, x);
    getmaxyx(self->window, y2, x2);
    int cur_len = 0;

    if (key == 0x107 || key == 0x8 || key == 0x7f) {  /* BACKSPACE key: Remove character behind pos */
        if (ctx->pos > 0) {
            cur_len = MAX(1, wcwidth(ctx->line[ctx->pos - 1]));
            del_char_buf_bck(ctx->line, &ctx->pos, &ctx->len);

            if (x == 0)
                wmove(self->window, y-1, x2 - cur_len);
            else
                wmove(self->window, y, x - cur_len);
        } else {
            beep();
        }
    } 

    else if (key == KEY_DC) {      /* DEL key: Remove character at pos */
        if (ctx->pos != ctx->len)
            del_char_buf_frnt(ctx->line, &ctx->pos, &ctx->len);
        else
            beep();
    }

    else if (key == T_KEY_DISCARD) {    /* CTRL-U: Delete entire line behind pos */
        if (ctx->pos > 0) {
            discard_buf(ctx->line, &ctx->pos, &ctx->len);
            wmove(self->window, y2 - CURS_Y_OFFSET, 0);
        } else {
            beep();
        }
    }

    else if (key == T_KEY_KILL) {    /* CTRL-K: Delete entire line in front of pos */
        if (ctx->pos != ctx->len)
            kill_buf(ctx->line, &ctx->pos, &ctx->len);
        else
            beep();
    }

    else if (key == KEY_HOME || key == T_KEY_C_A) {  /* HOME/C-a key: Move cursor to start of line */
        if (ctx->pos > 0) {
            ctx->pos = 0;
            wmove(self->window, y2 - CURS_Y_OFFSET, 0);
        }
    }

    else if (key == KEY_END || key == T_KEY_C_E) {  /* END/C-e key: move cursor to end of line */
        if (ctx->pos != ctx->len) {
            ctx->pos = ctx->len;
            mv_curs_end(self->window, MAX(0, wcswidth(ctx->line, (CHATBOX_HEIGHT-1)*x2)), y2, x2);
        }
    }

    else if (key == KEY_LEFT) {
        if (ctx->pos > 0) {
            --ctx->pos;
            cur_len = MAX(1, wcwidth(ctx->line[ctx->pos]));

            if (x == 0)
                wmove(self->window, y-1, x2 - cur_len);
            else
                wmove(self->window, y, x - cur_len);
        } else {
            beep();
        }
    } 

    else if (key == KEY_RIGHT) {
        if (ctx->pos < ctx->len) {
            cur_len = MAX(1, wcwidth(ctx->line[ctx->pos]));
            ++ctx->pos;

            if (x == x2-1)
                wmove(self->window, y+1, 0);
            else
                wmove(self->window, y, x + cur_len);
        } else {
            beep();
        }
    }

    else if (key == KEY_UP) {    /* fetches previous item in history */
        fetch_hist_item(ctx->line, &ctx->pos, &ctx->len, ctx->ln_history, ctx->hst_tot,
                        &ctx->hst_pos, LN_HIST_MV_UP);
        mv_curs_end(self->window, ctx->len, y2, x2);
    }

    else if (key == KEY_DOWN) {    /* fetches next item in history */
        fetch_hist_item(ctx->line, &ctx->pos, &ctx->len, ctx->ln_history, ctx->hst_tot,
                        &ctx->hst_pos, LN_HIST_MV_DWN);
        mv_curs_end(self->window, ctx->len, y2, x2);
    }

    else if (key == '\t') {    /* TAB key: completes peer name */
        if (ctx->len > 0) {
            int diff;

            if ((ctx->line[0] != '/') || (ctx->line[1] == 'm' && ctx->line[2] == 'e'))
                diff = complete_line(ctx->line, &ctx->pos, &ctx->len, groupchats[self->num].peer_names, 
                                     groupchats[self->num].num_peers, TOX_MAX_NAME_LENGTH);
            else
                diff = complete_line(ctx->line, &ctx->pos, &ctx->len, glob_cmd_list, AC_NUM_GLOB_COMMANDS,
                                     MAX_CMDNAME_SIZE);

            if (diff != -1) {
                if (x + diff > x2 - 1) {
                    int ofst = (x + diff - 1) - (x2 - 1);
                    wmove(self->window, y+1, ofst);
                } else {
                    wmove(self->window, y, x+diff);
                }
            } else {
                beep();
            }
        } else {
            beep();
        }
    }

    /* Scroll peerlist up and down one position if list overflows window */
    else if (key == KEY_NPAGE) {
        int L = y2 - CHATBOX_HEIGHT - SDBAR_OFST;

        if (groupchats[self->num].side_pos < groupchats[self->num].num_peers - L)
            ++groupchats[self->num].side_pos;
    }

    else if (key == KEY_PPAGE) {
        if (groupchats[self->num].side_pos > 0)
            --groupchats[self->num].side_pos;
    } 

    else
#if HAVE_WIDECHAR
    if (iswprint(key))
#else
    if (isprint(key))
#endif
    {   /* prevents buffer overflows and strange behaviour when cursor goes past the window */
        if ( (ctx->len < MAX_STR_SIZE-1) && (ctx->len < (x2 * (CHATBOX_HEIGHT - 1)-1)) ) {
            add_char_to_buf(ctx->line, &ctx->pos, &ctx->len, key);

            if (x == x2-1)
                wmove(self->window, y+1, 0);
            else
                wmove(self->window, y, x + MAX(1, wcwidth(key)));
        }
    }

    /* RETURN key: Execute command or print line */
    else if (key == '\n') {
        uint8_t line[MAX_STR_SIZE];

        if (wcs_to_mbs_buf(line, ctx->line, MAX_STR_SIZE) == -1)
            memset(&line, 0, sizeof(line));

        wclear(ctx->linewin);
        wmove(self->window, y2 - CURS_Y_OFFSET, 0);
        wclrtobot(self->window);

        if (!string_is_empty(line))
            add_line_to_hist(ctx->line, ctx->len, ctx->ln_history, &ctx->hst_tot, &ctx->hst_pos);

        if (line[0] == '/') {
            if (strcmp(line, "/close") == 0) {
                close_groupchat(self, m, self->num);
                return;
            } else if (strcmp(line, "/help") == 0) {
                if (strcmp(line, "help global") == 0)
                    execute(ctx->history, self, m, "/help", GLOBAL_COMMAND_MODE);
                else
                    print_groupchat_help(ctx);

            } else if (strncmp(line, "/me ", strlen("/me ")) == 0) {
                send_group_action(self, ctx, m, line + strlen("/me "));
            } else {
                execute(ctx->history, self, m, line, GROUPCHAT_COMMAND_MODE);
            }
        } else if (!string_is_empty(line)) {
            if (tox_group_message_send(m, self->num, line, strlen(line) + 1) == -1) {
                wattron(ctx->history, COLOR_PAIR(RED));
                wprintw(ctx->history, " * Failed to send message.\n");
                wattroff(ctx->history, COLOR_PAIR(RED));
            }
        }

        reset_buf(ctx->line, &ctx->pos, &ctx->len);
    }
}

static void groupchat_onDraw(ToxWindow *self, Tox *m)
{
    curs_set(1);
    int x2, y2;
    getmaxyx(self->window, y2, x2);

    ChatContext *ctx = self->chatwin;

    wclear(ctx->linewin);

    if (ctx->len > 0) {
        uint8_t line[MAX_STR_SIZE];

        if (wcs_to_mbs_buf(line, ctx->line, MAX_STR_SIZE) == -1) {
            reset_buf(ctx->line, &ctx->pos, &ctx->len);
            wmove(self->window, y2 - CURS_Y_OFFSET, 0);
        } else {
            mvwprintw(ctx->linewin, 1, 0, "%s", line);
        }
    }

    wclear(ctx->sidebar);
    mvwhline(ctx->linewin, 0, 0, ACS_HLINE, x2);
    mvwvline(ctx->sidebar, 0, 0, ACS_VLINE, y2-CHATBOX_HEIGHT);
    mvwaddch(ctx->sidebar, y2-CHATBOX_HEIGHT, 0, ACS_BTEE);  

    int num_peers = groupchats[self->num].num_peers;

    wmove(ctx->sidebar, 0, 1);
    wattron(ctx->sidebar, A_BOLD);
    wprintw(ctx->sidebar, "Peers: %d\n", num_peers);
    wattroff(ctx->sidebar, A_BOLD);

    mvwaddch(ctx->sidebar, 1, 0, ACS_LTEE);
    mvwhline(ctx->sidebar, 1, 1, ACS_HLINE, SIDEBAR_WIDTH-1);

    int N = TOX_MAX_NAME_LENGTH;
    int maxlines = y2 - SDBAR_OFST - CHATBOX_HEIGHT;
    int i;

    for (i = 0; i < num_peers && i < maxlines; ++i) {
        wmove(ctx->sidebar, i+2, 1);
        int peer = i + groupchats[self->num].side_pos;

        /* truncate nick to fit in side panel without modifying list */
        uint8_t tmpnck[TOX_MAX_NAME_LENGTH];
        memcpy(tmpnck, &groupchats[self->num].peer_names[peer*N], SIDEBAR_WIDTH-2);
        tmpnck[SIDEBAR_WIDTH-2] = '\0';

        wprintw(ctx->sidebar, "%s\n", tmpnck);
    }
}

static void groupchat_onInit(ToxWindow *self, Tox *m)
{
    int x, y;
    getmaxyx(self->window, y, x);

    ChatContext *ctx = self->chatwin;
    ctx->history = subwin(self->window, y-CHATBOX_HEIGHT+1, x-SIDEBAR_WIDTH-1, 0, 0);
    scrollok(ctx->history, 1);
    ctx->linewin = subwin(self->window, CHATBOX_HEIGHT, x, y-CHATBOX_HEIGHT, 0);
    ctx->sidebar = subwin(self->window, y-CHATBOX_HEIGHT+1, SIDEBAR_WIDTH, 0, x-SIDEBAR_WIDTH);

    ctx->log = malloc(sizeof(struct chatlog));

    if (ctx->log == NULL) {
        endwin();
        fprintf(stderr, "malloc() failed. Aborting...\n");
        exit(EXIT_FAILURE);
    }

    memset(ctx->log, 0, sizeof(struct chatlog));

    print_groupchat_help(ctx);
    execute(ctx->history, self, m, "/log", GLOBAL_COMMAND_MODE);

    wmove(self->window, y-CURS_Y_OFFSET, 0);
}

ToxWindow new_group_chat(Tox *m, int groupnum)
{
    ToxWindow ret;
    memset(&ret, 0, sizeof(ret));

    ret.active = true;
    ret.is_groupchat = true;

    ret.onKey = &groupchat_onKey;
    ret.onDraw = &groupchat_onDraw;
    ret.onInit = &groupchat_onInit;
    ret.onGroupMessage = &groupchat_onGroupMessage;
    ret.onGroupNamelistChange = &groupchat_onGroupNamelistChange;
    ret.onGroupAction = &groupchat_onGroupAction;

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

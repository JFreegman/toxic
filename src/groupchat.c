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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE    /* needed for strcasestr() and wcwidth() */
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>

#include "windows.h"
#include "toxic.h"
#include "execute.h"
#include "misc_tools.h"
#include "groupchat.h"
#include "prompt.h"
#include "toxic_strings.h"
#include "log.h"
#include "line_info.h"
#include "settings.h"

extern char *DATA_FILE;

static GroupChat groupchats[MAX_GROUPCHAT_NUM];
static int max_groupchat_index = 0;

extern struct user_settings *user_settings;

/* temporary until group chats have unique commands */
extern const uint8_t glob_cmd_list[AC_NUM_GLOB_COMMANDS][MAX_CMDNAME_SIZE];

int init_groupchat_win(ToxWindow *prompt, Tox *m, int groupnum)
{
    if (groupnum > MAX_GROUPCHAT_NUM)
        return -1;

    int i;

    for (i = 0; i <= max_groupchat_index; ++i) {
        if (!groupchats[i].active) {
            groupchats[i].chatwin = add_window(m, new_group_chat(m, groupnum));
            groupchats[i].active = true;
            groupchats[i].num_peers = 0;
            groupchats[i].peer_names = malloc(sizeof(uint8_t) * TOX_MAX_NAME_LENGTH);
            groupchats[i].oldpeer_names = malloc(sizeof(uint8_t) * TOX_MAX_NAME_LENGTH);
            groupchats[i].peer_name_lengths = malloc(sizeof(uint16_t));
            groupchats[i].oldpeer_name_lengths = malloc(sizeof(uint16_t));

            memset(groupchats[i].peer_names, 0, sizeof(uint8_t) * TOX_MAX_NAME_LENGTH);
            memset(groupchats[i].peer_name_lengths, 0, sizeof(uint16_t));

            memcpy(&groupchats[i].oldpeer_names[0], UNKNOWN_NAME, sizeof(UNKNOWN_NAME));
            groupchats[i].oldpeer_name_lengths[0] = (uint16_t) strlen(UNKNOWN_NAME);

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
    line_info_cleanup(ctx->hst);
    delwin(ctx->linewin);
    del_window(self);
    free(ctx->log);
    free(ctx->hst);
    free(ctx);
}

static void close_groupchat(ToxWindow *self, Tox *m, int groupnum)
{
    set_active_window(0);
    tox_del_groupchat(m, groupnum);

    free(groupchats[groupnum].peer_names);
    free(groupchats[groupnum].oldpeer_names);
    free(groupchats[groupnum].peer_name_lengths);
    free(groupchats[groupnum].oldpeer_name_lengths);
    memset(&groupchats[groupnum], 0, sizeof(GroupChat));

    int i;

    for (i = max_groupchat_index; i > 0; --i) {
        if (groupchats[i - 1].active)
            break;
    }

    max_groupchat_index = i;
    kill_groupchat_window(self);
}

static void print_groupchat_help(ToxWindow *self)
{
    struct history *hst = self->chatwin->hst;
    line_info_clear(hst);
    struct line_info *start = hst->line_start;

    uint8_t *msg = "Group chat commands:";
    line_info_add(self, NULL, NULL, NULL, msg, SYS_MSG, 1, CYAN);

#define NUMLINES 9

    uint8_t lines[NUMLINES][MAX_STR_SIZE] = {

        { "    /add <id> <msg>     : Add friend with optional message"               },
        { "    /status <type> <msg>: Set your status with optional note"             },
        { "    /note <msg>         : Set a personal note"                            },
        { "    /nick <nick>        : Set your nickname"                              },
        { "    /groupchat          : Create a group chat"                            },
        { "    /log <on> or <off>  : Enable/disable logging"                         },
        { "    /close              : Close the current group chat"                   },
        { "    /help               : Print this message again"                       },
        { "    /help global        : Show a list of global commands"                 },

    };

    int i;

    for (i = 0; i < NUMLINES; ++i)
        line_info_add(self, NULL, NULL, NULL, lines[i], SYS_MSG, 0, 0);

    msg = " * Use Page Up/Page Down keys to scroll chat history";
    line_info_add(self, NULL, NULL, NULL, msg, SYS_MSG, 1, CYAN);
    msg = " * Scroll peer list with the ctrl-] and ctrl-[ keys.";
    line_info_add(self, NULL, NULL, NULL, msg, SYS_MSG, 1, CYAN);
    msg = " * Notice, some friends will be missing names while finding peers";
    line_info_add(self, NULL, NULL, NULL, msg, SYS_MSG, 1, 0);
    line_info_add(self, NULL, NULL, NULL, "", SYS_MSG, 0, 0);

    hst->line_start = start;
}

static void groupchat_onGroupMessage(ToxWindow *self, Tox *m, int groupnum, int peernum,
                                     uint8_t *msg, uint16_t len)
{
    if (self->num != groupnum)
        return;

    msg[len] = '\0';

    ChatContext *ctx = self->chatwin;

    uint8_t nick[TOX_MAX_NAME_LENGTH] = {'\0'};
    int n_len = tox_group_peername(m, groupnum, peernum, nick);

    n_len = MIN(n_len, TOXIC_MAX_NAME_LENGTH - 1);  /* enforce client max name length */
    nick[n_len] = '\0';

    /* check if message contains own name and alert appropriately */
    int alert_type = WINDOW_ALERT_1;
    bool beep = false;

    uint8_t selfnick[TOX_MAX_NAME_LENGTH];
    uint16_t sn_len = tox_get_self_name(m, selfnick);
    selfnick[sn_len] = '\0';

    int nick_clr = strcmp(nick, selfnick) == 0 ? GREEN : CYAN;

    bool nick_match = strcasestr(msg, selfnick) && strncmp(selfnick, nick, TOXIC_MAX_NAME_LENGTH);

    if (nick_match) {
        alert_type = WINDOW_ALERT_0;
        beep = true;
        nick_clr = RED;
    }

    alert_window(self, alert_type, beep);

    uint8_t timefrmt[TIME_STR_SIZE];
    get_time_str(timefrmt, sizeof(timefrmt));

    line_info_add(self, timefrmt, nick, NULL, msg, IN_MSG, 0, nick_clr);
    write_to_log(msg, nick, ctx->log, false);
}

static void groupchat_onGroupAction(ToxWindow *self, Tox *m, int groupnum, int peernum, uint8_t *action,
                                    uint16_t len)
{
    if (self->num != groupnum)
        return;

    action[len] = '\0';

    ChatContext *ctx = self->chatwin;

    /* check if message contains own name and alert appropriately */
    int alert_type = WINDOW_ALERT_1;
    bool beep = false;

    uint8_t selfnick[TOX_MAX_NAME_LENGTH];
    uint16_t n_len = tox_get_self_name(m, selfnick);
    selfnick[n_len] = '\0';

    bool nick_match = strcasestr(action, selfnick);

    if (nick_match) {
        alert_type = WINDOW_ALERT_0;
        beep = true;
    }

    alert_window(self, alert_type, beep);

    uint8_t nick[TOX_MAX_NAME_LENGTH] = {'\0'};
    n_len = tox_group_peername(m, groupnum, peernum, nick);

    n_len = MIN(n_len, TOXIC_MAX_NAME_LENGTH - 1);
    nick[n_len] = '\0';

    uint8_t timefrmt[TIME_STR_SIZE];
    get_time_str(timefrmt, sizeof(timefrmt));

    line_info_add(self, timefrmt, nick, NULL, action, ACTION, 0, 0);
    write_to_log(action, nick, ctx->log, true);
}

/* Puts two copies of peerlist/lengths in chat instance */
static void copy_peernames(int gnum, uint8_t peerlist[][TOX_MAX_NAME_LENGTH], uint16_t lengths[], int npeers)
{
    /* Assumes these are initiated in init_groupchat_win */
    free(groupchats[gnum].peer_names);
    free(groupchats[gnum].oldpeer_names);
    free(groupchats[gnum].peer_name_lengths);
    free(groupchats[gnum].oldpeer_name_lengths);

    int N = TOX_MAX_NAME_LENGTH;

    groupchats[gnum].peer_names = malloc(sizeof(uint8_t) * npeers * N);
    groupchats[gnum].oldpeer_names = malloc(sizeof(uint8_t) * npeers * N);
    groupchats[gnum].peer_name_lengths = malloc(sizeof(uint16_t) * npeers);
    groupchats[gnum].oldpeer_name_lengths = malloc(sizeof(uint16_t) * npeers);

    if (groupchats[gnum].peer_names == NULL || groupchats[gnum].oldpeer_names == NULL
        || groupchats[gnum].peer_name_lengths == NULL || groupchats[gnum].oldpeer_name_lengths == NULL) {
        exit_toxic_err("failed in copy_peernames", FATALERR_MEMORY);
    }

    memset(groupchats[gnum].peer_names, 0, sizeof(uint8_t) * npeers * N);
    memset(groupchats[gnum].peer_name_lengths, 0, sizeof(uint16_t) * npeers);

    uint16_t unknown_len = strlen(UNKNOWN_NAME);
    int i;

    for (i = 0; i < npeers; ++i) {
        if (string_is_empty(peerlist[i])) {
            memcpy(&groupchats[gnum].peer_names[i * N], UNKNOWN_NAME, sizeof(UNKNOWN_NAME));
            groupchats[gnum].peer_name_lengths[i] = unknown_len;
        } else {
            memcpy(&groupchats[gnum].peer_names[i * N], peerlist[i], N);
            uint16_t n_len = lengths[i];

            n_len = MIN(n_len, TOXIC_MAX_NAME_LENGTH - 1);

            groupchats[gnum].peer_names[i * N + n_len] = '\0';
            groupchats[gnum].peer_name_lengths[i] = n_len;
        }
    }

    memcpy(groupchats[gnum].oldpeer_names, groupchats[gnum].peer_names, N * npeers);
    memcpy(groupchats[gnum].oldpeer_name_lengths, groupchats[gnum].peer_name_lengths,
           sizeof(uint16_t) * npeers);
}

static void groupchat_onGroupNamelistChange(ToxWindow *self, Tox *m, int groupnum, int peernum,
        uint8_t change)
{
    if (self->num != groupnum)
        return;

    if (groupnum > max_groupchat_index)
        return;

    groupchats[groupnum].num_peers = tox_group_number_peers(m, groupnum);
    int num_peers = groupchats[groupnum].num_peers;

    if (peernum >= num_peers)
        return;

    /* get old peer name before updating name list */
    uint8_t oldpeername[TOX_MAX_NAME_LENGTH];

    if (change != TOX_CHAT_CHANGE_PEER_ADD) {
        memcpy(oldpeername, &groupchats[groupnum].oldpeer_names[peernum * TOX_MAX_NAME_LENGTH],
               sizeof(oldpeername));
        uint16_t old_n_len = groupchats[groupnum].oldpeer_name_lengths[peernum];
        oldpeername[old_n_len] = '\0';
    }

    /* Update name/len lists */
    uint8_t tmp_peerlist[num_peers][TOX_MAX_NAME_LENGTH];
    uint16_t tmp_peerlens[num_peers];
    tox_group_get_names(m, groupnum, tmp_peerlist, tmp_peerlens, num_peers);
    copy_peernames(groupnum, tmp_peerlist, tmp_peerlens, num_peers);

    /* get current peername then sort namelist */
    uint8_t peername[TOX_MAX_NAME_LENGTH];

    if (change != TOX_CHAT_CHANGE_PEER_DEL) {
        uint16_t n_len = groupchats[groupnum].peer_name_lengths[peernum];
        memcpy(peername, &groupchats[groupnum].peer_names[peernum * TOX_MAX_NAME_LENGTH], sizeof(peername));
        peername[n_len] = '\0';
    }

    qsort(groupchats[groupnum].peer_names, groupchats[groupnum].num_peers, TOX_MAX_NAME_LENGTH, qsort_strcasecmp_hlpr);

    ChatContext *ctx = self->chatwin;

    uint8_t *event;
    uint8_t timefrmt[TIME_STR_SIZE];
    get_time_str(timefrmt, sizeof(timefrmt));

    switch (change) {
        case TOX_CHAT_CHANGE_PEER_ADD:
            event = "has joined the room";
            line_info_add(self, timefrmt, peername, NULL, event, CONNECTION, 0, GREEN);
            write_to_log(event, peername, ctx->log, true);
            break;

        case TOX_CHAT_CHANGE_PEER_DEL:
            event = "has left the room";
            line_info_add(self, timefrmt, oldpeername, NULL, event, CONNECTION, 0, 0);

            if (groupchats[self->num].side_pos > 0)
                --groupchats[self->num].side_pos;

            write_to_log(event, oldpeername, ctx->log, true);
            break;

        case TOX_CHAT_CHANGE_PEER_NAME:
            event = " is now known as ";
            line_info_add(self, timefrmt, oldpeername, peername, event, NAME_CHANGE, 0, 0);

            uint8_t tmp_event[TOXIC_MAX_NAME_LENGTH * 2 + 32];
            snprintf(tmp_event, sizeof(tmp_event), "is now known as %s", peername);
            write_to_log(tmp_event, oldpeername, ctx->log, true);
            break;
    }

    alert_window(self, WINDOW_ALERT_2, false);
}

static void send_group_action(ToxWindow *self, ChatContext *ctx, Tox *m, uint8_t *action)
{
    if (action == NULL) {
        wprintw(ctx->history, "Invalid syntax.\n");
        return;
    }

    if (tox_group_action_send(m, self->num, action, strlen(action)) == -1) {
        uint8_t *errmsg = " * Failed to send action.";
        line_info_add(self, NULL, NULL, NULL, errmsg, SYS_MSG, 0, RED);
    }
}

static void groupchat_onKey(ToxWindow *self, Tox *m, wint_t key, bool ltr)
{
    ChatContext *ctx = self->chatwin;

    int x, y, y2, x2;
    getyx(self->window, y, x);
    getmaxyx(self->window, y2, x2);

    if (x2 <= 0)
        return;

    int cur_len = 0;    /* widechar len of current char */
    int x2_is_odd = x2 % 2 != 0;

    if (ltr) {    /* char is printable */
        if (ctx->len < MAX_STR_SIZE - 1) {
            add_char_to_buf(ctx, key);

            if (x >= x2 - 1) {
                wmove(self->window, y, x2 / 2 + x2_is_odd);
                ctx->start += x2 / 2;
            } else {
                wmove(self->window, y, x + MAX(1, wcwidth(key)));
            }
        }

    } else { /* if (!ltr) */

        if (line_info_onKey(self, key))
            return;

        if (key == 0x107 || key == 0x8 || key == 0x7f) {  /* BACKSPACE key */
            if (ctx->pos > 0) {
                cur_len = MAX(1, wcwidth(ctx->line[ctx->pos]));
                del_char_buf_bck(ctx);

                if (x == 0) {
                    ctx->start = ctx->start >= x2 ? ctx->start - x2 : 0;
                    int new_x = ctx->start == 0 ? ctx->pos : x2 - cur_len;
                    wmove(self->window, y, new_x);
                } else {
                    wmove(self->window, y, x - cur_len);
                }
            } else {
                beep();
            }
        }

        else if (key == KEY_DC) {      /* DEL key: Remove character at pos */
            if (ctx->pos != ctx->len)
                del_char_buf_frnt(ctx);
            else
                beep();
        }

        else if (key == T_KEY_DISCARD) {    /* CTRL-U: Delete entire line behind pos */
            if (ctx->pos > 0) {
                discard_buf(ctx);
                wmove(self->window, y2 - CURS_Y_OFFSET, 0);
            } else {
                beep();
            }
        }

        else if (key == T_KEY_KILL) {    /* CTRL-K: Delete entire line in front of pos */
            if (ctx->pos != ctx->len)
                kill_buf(ctx);
            else
                beep();
        }

        else if (key == KEY_HOME || key == T_KEY_C_A) {  /* HOME/C-a key: Move cursor to start of line */
            if (ctx->pos > 0) {
                ctx->pos = 0;
                ctx->start = 0;
                wmove(self->window, y2 - CURS_Y_OFFSET, 0);
            }
        }

        else if (key == KEY_END || key == T_KEY_C_E) {  /* END/C-e key: move cursor to end of line */
            if (ctx->pos != ctx->len) {
                ctx->pos = ctx->len;
                ctx->start = x2 * (ctx->len / x2);
                mv_curs_end(self->window, ctx->len, y2, x2);
            }
        }

        else if (key == KEY_LEFT) {
            if (ctx->pos > 0) {
                --ctx->pos;
                cur_len = MAX(1, wcwidth(ctx->line[ctx->pos]));

                if (x == 0) {
                    wmove(self->window, y, x2 - cur_len);
                    ctx->start = ctx->start >= x2 ? ctx->start - x2 : 0;
                    ctx->pos = ctx->start + x2 - 1;
                } else {
                    wmove(self->window, y, x - cur_len);
                }
            } else {
                beep();
            }
        }

        else if (key == KEY_RIGHT) {
            if (ctx->pos < ctx->len) {
                ++ctx->pos;

                if (x == x2 - 1) {
                    wmove(self->window, y, 0);
                    ctx->start += x2;
                    ctx->pos = ctx->start;
                } else {
                    cur_len = MAX(1, wcwidth(ctx->line[ctx->pos]));
                    wmove(self->window, y, x + cur_len);
                }
            } else {
                beep();
            }
        }

        else if (key == KEY_UP) {    /* fetches previous item in history */
            fetch_hist_item(ctx, MOVE_UP);
            ctx->start = x2 * (ctx->len / x2);
            mv_curs_end(self->window, ctx->len, y2, x2);
        }

        else if (key == KEY_DOWN) {    /* fetches next item in history */
            fetch_hist_item(ctx, MOVE_DOWN);
            ctx->start = x2 * (ctx->len / x2);
            mv_curs_end(self->window, ctx->len, y2, x2);
        }

        else if (key == '\t') {    /* TAB key: completes peer name */
            if (ctx->len > 0) {
                int diff;

                if ((ctx->line[0] != '/') || (ctx->line[1] == 'm' && ctx->line[2] == 'e'))
                    diff = complete_line(ctx, groupchats[self->num].peer_names,
                                         groupchats[self->num].num_peers, TOX_MAX_NAME_LENGTH);
                else
                    diff = complete_line(ctx, glob_cmd_list, AC_NUM_GLOB_COMMANDS, MAX_CMDNAME_SIZE);

                if (diff != -1) {
                    if (x + diff > x2 - 1) {
                        int ofst = (x + diff - 1) - (x2 - 1);
                        wmove(self->window, y + 1, ofst);
                    } else {
                        wmove(self->window, y, x + diff);
                    }
                } else {
                    beep();
                }
            } else {
                beep();
            }
        }

        /* Scroll peerlist up and down one position if list overflows window */
        else if (key == T_KEY_C_RB) {
            int L = y2 - CHATBOX_HEIGHT - SDBAR_OFST;

            if (groupchats[self->num].side_pos < groupchats[self->num].num_peers - L)
                ++groupchats[self->num].side_pos;
        }

        else if (key == T_KEY_C_LB) {
            if (groupchats[self->num].side_pos > 0)
                --groupchats[self->num].side_pos;
        }

        /* RETURN key: Execute command or print line */
        else if (key == '\n') {
            rm_trailing_spaces_buf(ctx);

            uint8_t line[MAX_STR_SIZE];

            if (wcs_to_mbs_buf(line, ctx->line, MAX_STR_SIZE) == -1)
                memset(&line, 0, sizeof(line));

            wclear(ctx->linewin);
            wmove(self->window, y2 - CURS_Y_OFFSET, 0);


            if (!string_is_empty(line))
                add_line_to_hist(ctx);

            if (line[0] == '/') {
                if (strcmp(line, "/close") == 0) {
                    close_groupchat(self, m, self->num);
                    return;
                } else if (strcmp(line, "/help") == 0) {
                    if (strcmp(line, "help global") == 0)
                        execute(ctx->history, self, m, "/help", GLOBAL_COMMAND_MODE);
                    else
                        print_groupchat_help(self);

                } else if (strncmp(line, "/me ", strlen("/me ")) == 0) {
                    send_group_action(self, ctx, m, line + strlen("/me "));
                } else {
                    execute(ctx->history, self, m, line, GROUPCHAT_COMMAND_MODE);
                }
            } else if (!string_is_empty(line)) {
                if (tox_group_message_send(m, self->num, line, strlen(line)) == -1) {
                    uint8_t *errmsg = " * Failed to send message.";
                    line_info_add(self, NULL, NULL, NULL, errmsg, SYS_MSG, 0, RED);
                }
            }

            reset_buf(ctx);
        }
    }
}

static void groupchat_onDraw(ToxWindow *self, Tox *m)
{

    int x2, y2;
    getmaxyx(self->window, y2, x2);

    ChatContext *ctx = self->chatwin;

    line_info_print(self);
    wclear(ctx->linewin);

    scrollok(ctx->history, 0);
    curs_set(1);

    if (ctx->len > 0) {
        uint8_t line[MAX_STR_SIZE];

        if (wcs_to_mbs_buf(line, ctx->line, MAX_STR_SIZE) == -1) {
            reset_buf(ctx);
            wmove(self->window, y2 - CURS_Y_OFFSET, 0);
        } else {
            mvwprintw(ctx->linewin, 1, 0, "%s", &line[ctx->start]);
        }
    }

    wclear(ctx->sidebar);
    mvwhline(ctx->linewin, 0, 0, ACS_HLINE, x2);
    mvwvline(ctx->sidebar, 0, 0, ACS_VLINE, y2 - CHATBOX_HEIGHT);
    mvwaddch(ctx->sidebar, y2 - CHATBOX_HEIGHT, 0, ACS_BTEE);

    int num_peers = groupchats[self->num].num_peers;

    wmove(ctx->sidebar, 0, 1);
    wattron(ctx->sidebar, A_BOLD);
    wprintw(ctx->sidebar, "Peers: %d\n", num_peers);
    wattroff(ctx->sidebar, A_BOLD);

    mvwaddch(ctx->sidebar, 1, 0, ACS_LTEE);
    mvwhline(ctx->sidebar, 1, 1, ACS_HLINE, SIDEBAR_WIDTH - 1);

    int N = TOX_MAX_NAME_LENGTH;
    int maxlines = y2 - SDBAR_OFST - CHATBOX_HEIGHT;
    int i;

    for (i = 0; i < num_peers && i < maxlines; ++i) {
        wmove(ctx->sidebar, i + 2, 1);
        int peer = i + groupchats[self->num].side_pos;

        /* truncate nick to fit in side panel without modifying list */
        uint8_t tmpnck[TOX_MAX_NAME_LENGTH];
        memcpy(tmpnck, &groupchats[self->num].peer_names[peer * N], SIDEBAR_WIDTH - 2);
        tmpnck[SIDEBAR_WIDTH - 2] = '\0';

        wprintw(ctx->sidebar, "%s\n", tmpnck);
    }
}

static void groupchat_onInit(ToxWindow *self, Tox *m)
{
    int x, y;
    getmaxyx(self->window, y, x);

    ChatContext *ctx = self->chatwin;

    ctx->history = subwin(self->window, y - CHATBOX_HEIGHT + 1, x - SIDEBAR_WIDTH - 1, 0, 0);
    ctx->linewin = subwin(self->window, CHATBOX_HEIGHT, x, y - CHATBOX_HEIGHT, 0);
    ctx->sidebar = subwin(self->window, y - CHATBOX_HEIGHT + 1, SIDEBAR_WIDTH, 0, x - SIDEBAR_WIDTH);

    ctx->hst = malloc(sizeof(struct history));
    ctx->log = malloc(sizeof(struct chatlog));

    if (ctx->log == NULL || ctx->hst == NULL)
        exit_toxic_err("failed in groupchat_onInit", FATALERR_MEMORY);

    memset(ctx->hst, 0, sizeof(struct history));
    memset(ctx->log, 0, sizeof(struct chatlog));

    line_info_init(ctx->hst);
    print_groupchat_help(self);

    if (user_settings->autolog == AUTOLOG_ON)
        log_enable(self->name, NULL, ctx->log);

    execute(ctx->history, self, m, "/log", GLOBAL_COMMAND_MODE);

    wmove(self->window, y - CURS_Y_OFFSET, 0);
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

    if (chatwin == NULL)
        exit_toxic_err("failed in new_group_chat", FATALERR_MEMORY);

    ret.chatwin = chatwin;
    ret.num = groupnum;

    return ret;
}

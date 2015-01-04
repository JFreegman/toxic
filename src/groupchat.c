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
#define _GNU_SOURCE    /* needed for strcasestr() and wcswidth() */
#endif

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <wchar.h>
#include <unistd.h>

#ifdef AUDIO
#ifdef __APPLE__
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
/* compatibility with older versions of OpenAL */
#ifndef ALC_ALL_DEVICES_SPECIFIER
#include <AL/alext.h>
#endif  /* ALC_ALL_DEVICES_SPECIFIER */
#endif  /* __APPLE__ */
#endif  /* AUDIO */

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
#include "input.h"
#include "help.h"
#include "notify.h"
#include "autocomplete.h"
#include "device.h"

extern char *DATA_FILE;

static GroupChat groupchats[MAX_GROUPCHAT_NUM];
static int max_groupchat_index = 0;

extern struct user_settings *user_settings;
extern struct Winthread Winthread;

#ifdef AUDIO
#define AC_NUM_GROUP_COMMANDS 24
#else
#define AC_NUM_GROUP_COMMANDS 20
#endif /* AUDIO */

/* groupchat command names used for tab completion. */
static const char group_cmd_list[AC_NUM_GROUP_COMMANDS][MAX_CMDNAME_SIZE] = {
    { "/accept"     },
    { "/add"        },
    { "/avatar"     },
    { "/chatid"     },
    { "/clear"      },
    { "/close"      },
    { "/connect"    },
    { "/decline"    },
    { "/exit"       },
    { "/group"      },
    { "/help"       },
    { "/join"       },
    { "/log"        },
    { "/myid"       },
    { "/nick"       },
    { "/note"       },
    { "/quit"       },
    { "/requests"   },
    { "/status"     },
    { "/topic"      },

#ifdef AUDIO

    { "/lsdev"       },
    { "/sdev"        },
    { "/mute"        },
    { "/sense"       },

#endif /* AUDIO */
};

ToxWindow new_group_chat(Tox *m, int groupnum, const char *groupname, int length);

#ifdef AUDIO
static int group_audio_open_out_device(int groupnum);
static int group_audio_close_out_device(int groupnum);
#endif  /* AUDIO */

int init_groupchat_win(ToxWindow *prompt, Tox *m, int groupnum, const char *groupname, int length)
{
    if (groupnum > MAX_GROUPCHAT_NUM)
        return -1;

    ToxWindow self = new_group_chat(m, groupnum, groupname, length);

    int i;

    for (i = 0; i <= max_groupchat_index; ++i) {
        if (!groupchats[i].active) {
            groupchats[i].chatwin = add_window(m, self);
            groupchats[i].active = true;
            groupchats[i].peer_names = malloc(sizeof(uint8_t) * TOX_MAX_NAME_LENGTH);
            groupchats[i].peer_name_lengths = malloc(sizeof(uint32_t));

            if (groupchats[i].peer_names == NULL || groupchats[i].peer_name_lengths == NULL)
                exit_toxic_err("failed in init_groupchat_win", FATALERR_MEMORY);

            set_active_window(groupchats[i].chatwin);

            if (i == max_groupchat_index)
                ++max_groupchat_index;

            return 0;
        }
    }

    return -1;
}

static void kill_groupchat_window(ToxWindow *self)
{
    ChatContext *ctx = self->chatwin;

    log_disable(ctx->log);
    line_info_cleanup(ctx->hst);
    delwin(ctx->linewin);
    delwin(ctx->history);
    delwin(ctx->sidebar);
    free(ctx->log);
    free(ctx);
    free(self->help);
    del_window(self);
}

void close_groupchat(ToxWindow *self, Tox *m, int groupnum, const char *partmessage, int length)
{
    tox_group_delete(m, groupnum, (const uint8_t *) partmessage, (uint16_t) length);

#ifdef AUDIO
    group_audio_close_out_device(groupnum);
#endif

    free(groupchats[groupnum].peer_names);
    free(groupchats[groupnum].peer_name_lengths);
    memset(&groupchats[groupnum], 0, sizeof(GroupChat));

    int i;

    for (i = max_groupchat_index; i > 0; --i) {
        if (groupchats[i - 1].active)
            break;
    }

    max_groupchat_index = i;
    kill_groupchat_window(self);
}

/* destroys and re-creates groupchat window with or without the peerlist */
void redraw_groupchat_win(ToxWindow *self)
{
    ChatContext *ctx = self->chatwin;

    endwin();
    refresh();
    clear();

    int x2, y2;
    getmaxyx(stdscr, y2, x2);
    y2 -= 2;

    if (ctx->sidebar) {
        delwin(ctx->sidebar);
        ctx->sidebar = NULL;
    }

    delwin(ctx->linewin);
    delwin(ctx->history);
    delwin(self->window);

    self->window = newwin(y2, x2, 0, 0);
    ctx->linewin = subwin(self->window, CHATBOX_HEIGHT, x2, y2 - CHATBOX_HEIGHT, 0);

    if (self->show_peerlist) {
        ctx->history = subwin(self->window, y2 - CHATBOX_HEIGHT + 1, x2 - SIDEBAR_WIDTH - 1, 0, 0);
        ctx->sidebar = subwin(self->window, y2 - CHATBOX_HEIGHT + 1, SIDEBAR_WIDTH, 0, x2 - SIDEBAR_WIDTH);
    } else {
        ctx->history = subwin(self->window, y2 - CHATBOX_HEIGHT + 1, x2, 0, 0);
    }

    scrollok(ctx->history, 0);

}

static void groupchat_onGroupMessage(ToxWindow *self, Tox *m, int groupnum, int peernum,
                                     const char *msg, uint16_t len)
{
    if (self->num != groupnum)
        return;

    ChatContext *ctx = self->chatwin;

    char nick[TOX_MAX_NAME_LENGTH];
    get_group_nick_truncate(m, nick, peernum, groupnum);

    char selfnick[TOX_MAX_NAME_LENGTH];
    uint16_t sn_len = tox_group_get_self_name(m, groupnum, (uint8_t *) selfnick);
    selfnick[sn_len] = '\0';

    int nick_clr = CYAN;

    /* Only play sound if mentioned by someone else */
    if (strcasestr(msg, selfnick) && strcmp(selfnick, nick)) {
        sound_notify(self, generic_message, NT_WNDALERT_0, NULL);

        if (self->active_box != -1)
            box_silent_notify2(self, NT_NOFOCUS, self->active_box, "%s %s", nick, msg);
        else
            box_silent_notify(self, NT_NOFOCUS, &self->active_box, self->name, "%s %s", nick, msg);

        nick_clr = RED;
    }
    else {
        sound_notify(self, silent, NT_WNDALERT_1, NULL);
    }

    char timefrmt[TIME_STR_SIZE];
    get_time_str(timefrmt, sizeof(timefrmt));

    line_info_add(self, timefrmt, nick, NULL, IN_MSG, 0, nick_clr, "%s", msg);
    write_to_log(msg, nick, ctx->log, false);
}

static void groupchat_onGroupAction(ToxWindow *self, Tox *m, int groupnum, int peernum, const char *action,
                                    uint16_t len)
{
    if (self->num != groupnum)
        return;

    ChatContext *ctx = self->chatwin;

    char nick[TOX_MAX_NAME_LENGTH];
    get_group_nick_truncate(m, nick, peernum, groupnum);

    char selfnick[TOX_MAX_NAME_LENGTH];
    uint16_t n_len = tox_group_get_self_name(m, groupnum, (uint8_t *) selfnick);
    selfnick[n_len] = '\0';

    if (strcasestr(action, selfnick)) {
        sound_notify(self, generic_message, NT_WNDALERT_0, NULL);

        if (self->active_box != -1)
            box_silent_notify2(self, NT_NOFOCUS, self->active_box, "* %s %s", nick, action );
        else
            box_silent_notify(self, NT_NOFOCUS, &self->active_box, self->name, "* %s %s", nick, action);
    }
    else {
        sound_notify(self, silent, NT_WNDALERT_1, NULL);
    }

    char timefrmt[TIME_STR_SIZE];
    get_time_str(timefrmt, sizeof(timefrmt));

    line_info_add(self, timefrmt, nick, NULL, IN_ACTION, 0, 0, "%s", action);
    write_to_log(action, nick, ctx->log, true);
}

static void groupchat_onGroupPrivateMessage(ToxWindow *self, Tox *m, int groupnum, uint32_t peernum,
                                            const char *action, uint16_t len)
{
    if (self->num != groupnum)
        return;

    ChatContext *ctx = self->chatwin;

    char nick[TOX_MAX_NAME_LENGTH];
    get_group_nick_truncate(m, nick, peernum, groupnum);

    char timefrmt[TIME_STR_SIZE];
    get_time_str(timefrmt, sizeof(timefrmt));

    line_info_add(self, timefrmt, nick, NULL, IN_MSG, 0, RED, "%s", action);
    write_to_log(action, nick, ctx->log, false);
    sound_notify(self, silent, NT_WNDALERT_1, NULL);
}

static void groupchat_onGroupTopicChange(ToxWindow *self, Tox *m, int groupnum, uint32_t peernum,
                                         const char *topic, uint16_t length)
{
    ChatContext *ctx = self->chatwin;

    if (self->num != groupnum)
        return;

    char timefrmt[TIME_STR_SIZE];
    get_time_str(timefrmt, sizeof(timefrmt));

    char nick[TOX_MAX_NAME_LENGTH];
    get_group_nick_truncate(m, nick, peernum, groupnum);
    line_info_add(self, timefrmt, nick, NULL, NAME_CHANGE, 0, 0, " set the group topic to: %s", topic);

    char tmp_event[MAX_STR_SIZE];
    snprintf(tmp_event, sizeof(tmp_event), "set topic to %s", topic);
    write_to_log(tmp_event, nick, ctx->log, true);
}

/* Copies peer names/lengths  */
static void copy_peernames(int gnum, uint8_t peerlist[][TOX_MAX_NAME_LENGTH], uint32_t lengths[], int npeers)
{
    free(groupchats[gnum].peer_names);
    free(groupchats[gnum].peer_name_lengths);

    int N = TOX_MAX_NAME_LENGTH;

    groupchats[gnum].peer_names = calloc(1, sizeof(uint8_t) * npeers * N);
    groupchats[gnum].peer_name_lengths = calloc(1, sizeof(uint32_t) * npeers);

    if (groupchats[gnum].peer_names == NULL || groupchats[gnum].peer_name_lengths == NULL)
        exit_toxic_err("failed in copy_peernames", FATALERR_MEMORY);

    if (npeers == 0)
        return;

    uint16_t u_len = strlen(UNKNOWN_NAME);
    int i;

    for (i = 0; i < npeers; ++i) {
         if (!lengths[i]) {
            memcpy(&groupchats[gnum].peer_names[i * N], UNKNOWN_NAME, u_len);
            groupchats[gnum].peer_names[i * N + u_len] = '\0';
            groupchats[gnum].peer_name_lengths[i] = u_len;
        } else {
            uint16_t n_len = MIN(lengths[i], TOXIC_MAX_NAME_LENGTH - 1);
            memcpy(&groupchats[gnum].peer_names[i * N], peerlist[i], n_len);
            groupchats[gnum].peer_names[i * N + n_len] = '\0';
            groupchats[gnum].peer_name_lengths[i] = n_len;
            filter_str((char *) &groupchats[gnum].peer_names[i * N], n_len);
        }
    }
}

static void groupchat_onGroupNamelistChange(ToxWindow *self, Tox *m, int groupnum)
{
    if (self->num != groupnum)
        return;

    int num_peers = tox_group_get_number_peers(m, groupnum);
    groupchats[groupnum].num_peers = num_peers;

    uint8_t tmp_peerlist[num_peers][TOX_MAX_NAME_LENGTH];
    uint32_t tmp_peerlens[num_peers];

    if (tox_group_get_names(m, groupnum, tmp_peerlist, tmp_peerlens, num_peers) == -1)
        return;

    copy_peernames(groupnum, tmp_peerlist, tmp_peerlens, num_peers);
}

static void groupchat_onGroupPeerJoin(ToxWindow *self, Tox *m, int groupnum, uint32_t peernum)
{
    if (groupnum != self->num)
        return;

    if (peernum > groupchats[groupnum].num_peers)
        return;

    char name[TOX_MAX_NAME_LENGTH];
    get_group_nick_truncate(m, name, peernum, groupnum);

    char timefrmt[TIME_STR_SIZE];
    get_time_str(timefrmt, sizeof(timefrmt));

    line_info_add(self, timefrmt, name, NULL, CONNECTION, 0, GREEN, "has joined the room");

    char log_str[TOXIC_MAX_NAME_LENGTH + 32];
    snprintf(log_str, sizeof(log_str), "%s has joined the room", name);

    write_to_log(log_str, name, self->chatwin->log, true);
    sound_notify(self, silent, NT_WNDALERT_2, NULL);
}

static void groupchat_onGroupPeerExit(ToxWindow *self, Tox *m, int groupnum, uint32_t peernum,
                                      const char *partmessage, uint16_t len)
{
    if (groupnum != self->num)
        return;

    if (peernum > groupchats[groupnum].num_peers)
        return;

    char name[TOX_MAX_NAME_LENGTH];
    get_group_nick_truncate(m, name, peernum, groupnum);

    char timefrmt[TIME_STR_SIZE];
    get_time_str(timefrmt, sizeof(timefrmt));

    line_info_add(self, timefrmt, name, NULL, CONNECTION, 0, GREEN, "has left the room (%s)", partmessage);

    char log_str[TOXIC_MAX_NAME_LENGTH + MAX_STR_SIZE];
    snprintf(log_str, sizeof(log_str), "%s has left the room (%s)", name, partmessage);

    write_to_log(log_str, name, self->chatwin->log, true);
    sound_notify(self, silent, NT_WNDALERT_2, NULL);
}

static void groupchat_onGroupSelfJoin(ToxWindow *self, Tox *m, int groupnum)
{
    if (groupnum != self->num)
        return;

    char groupname[TOX_MAX_GROUP_NAME_LENGTH];
    int len = tox_group_get_group_name(m, groupnum, (uint8_t *) groupname);

    if (len > 0)
        set_window_title(self, groupname, len);

    char topic[TOX_MAX_GROUP_TOPIC_LENGTH];
    tox_group_get_topic(m, groupnum, (uint8_t *) topic);

    char timefrmt[TIME_STR_SIZE];
    get_time_str(timefrmt, sizeof(timefrmt));

    line_info_add(self, timefrmt, NULL, NULL, SYS_MSG, 0, 0, "Connected.");
    line_info_add(self, timefrmt, NULL, NULL, SYS_MSG, 0, 0, "Topic set to: %s", topic);
}

static void groupchat_onGroupSelfTimeout(ToxWindow *self, Tox *m, int groupnum)
{
    if (groupnum != self->num)
        return;

    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Disconnected from group. Attempting to reconnect...");
}

static void groupchat_onGroupOpCertificate(ToxWindow *self, Tox *m, int groupnum, uint32_t src_peernum,
                                           uint32_t tgt_peernum, uint8_t type)
{
    if (groupnum != self->num)
        return;

    char src_name[TOX_MAX_NAME_LENGTH];
    get_group_nick_truncate(m, src_name, src_peernum, groupnum);

    char tgt_name[TOX_MAX_NAME_LENGTH];
    get_group_nick_truncate(m, tgt_name, tgt_peernum, groupnum);

    const char *msg = NULL;

    switch (type) {
        case TOX_GC_BAN:
            msg = "has banned";
            break;
        case TOX_GC_PROMOTE_OP:
            msg = "has given operator status to";
            break;
        case TOX_GC_REVOKE_OP:
            msg = "has removed operator status from";
            break;
        case TOX_GC_SILENCE:
            msg = "has silenced";
            break;
        default:
            return;
    }

    char timefrmt[TIME_STR_SIZE];
    get_time_str(timefrmt, sizeof(timefrmt));
    line_info_add(self, timefrmt, src_name, tgt_name, NAME_CHANGE, 0, MAGENTA, "%s", msg);
}

static void groupchat_onGroupNickChange(ToxWindow *self, Tox *m, int groupnum, uint32_t peernum,
                                        const char *newnick, uint16_t len)
{
    if (groupnum != self->num)
        return;

    char oldnick[TOX_MAX_NAME_LENGTH];
    get_group_nick_truncate(m, oldnick, peernum, groupnum);

    char timefrmt[TIME_STR_SIZE];
    get_time_str(timefrmt, sizeof(timefrmt));
    line_info_add(self, timefrmt, oldnick, (char *) newnick, NAME_CHANGE, 0, MAGENTA, "is now known as");
}


static void send_group_message(ToxWindow *self, Tox *m, int groupnum, const char *msg)
{
    ChatContext *ctx = self->chatwin;

    if (tox_group_message_send(m, self->num, (uint8_t *) msg, strlen(msg)) == -1) {
        const char *errmsg = " * Failed to send message.";
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, RED, errmsg);
        return;
    }

    char selfname[TOX_MAX_NAME_LENGTH];
    uint16_t len = tox_group_get_self_name(m, groupnum, (uint8_t *) selfname);
    selfname[len] = '\0';

    char timefrmt[TIME_STR_SIZE];
    get_time_str(timefrmt, sizeof(timefrmt));

    line_info_add(self, timefrmt, selfname, NULL, OUT_MSG_READ, 0, 0, msg);
    write_to_log(msg, selfname, ctx->log, false);
}

static void send_group_action(ToxWindow *self, Tox *m, int groupnum, char *action)
{
    ChatContext *ctx = self->chatwin;

    if (action == NULL) {
        wprintw(ctx->history, "Invalid syntax.\n");
        return;
    }

    if (tox_group_action_send(m, groupnum, (uint8_t *) action, strlen(action)) == -1) {
        const char *errmsg = " * Failed to send action.";
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, RED, errmsg);
        return;
    }

    char selfname[TOX_MAX_NAME_LENGTH];
    uint16_t len = tox_group_get_self_name(m, groupnum, (uint8_t *) selfname);
    selfname[len] = '\0';

    char timefrmt[TIME_STR_SIZE];
    get_time_str(timefrmt, sizeof(timefrmt));

    line_info_add(self, timefrmt, selfname, NULL, OUT_ACTION_READ, 0, 0, action);
    write_to_log(action, selfname, ctx->log, true);
}

static void groupchat_onKey(ToxWindow *self, Tox *m, wint_t key, bool ltr)
{
    ChatContext *ctx = self->chatwin;

    int x, y, y2, x2;
    getyx(self->window, y, x);
    getmaxyx(self->window, y2, x2);

    if (x2 <= 0)
        return;

    if (self->help->active) {
        help_onKey(self, key);
        return;
    }

    if (ltr) {    /* char is printable */
        input_new_char(self, key, x, y, x2, y2);
        return;
    }

    if (line_info_onKey(self, key))
        return;

    if (input_handle(self, key, x, y, x2, y2))
        return;

    if (key == '\t') {  /* TAB key: auto-completes peer name or command */
        if (ctx->len > 0) {
            int diff;

            /* TODO: make this not suck */
            if (ctx->line[0] != L'/' || wcscmp(ctx->line, L"/me") == 0) {
                diff = complete_line(self, groupchats[self->num].peer_names, groupchats[self->num].num_peers,
                                     TOX_MAX_NAME_LENGTH);
            } else if (wcsncmp(ctx->line, L"/avatar \"", wcslen(L"/avatar \"")) == 0) {
                diff = dir_match(self, m, ctx->line, L"/avatar");
            } else {
                diff = complete_line(self, group_cmd_list, AC_NUM_GROUP_COMMANDS, MAX_CMDNAME_SIZE);
            }

            if (diff != -1) {
                if (x + diff > x2 - 1) {
                    int wlen = wcswidth(ctx->line, sizeof(ctx->line));
                    ctx->start = wlen < x2 ? 0 : wlen - x2 + 1;
                }
            } else {
                sound_notify(self, error, 0, NULL);
            }
        } else {
            sound_notify(self, error, 0, NULL);
        }
    } else if (key == user_settings->key_peer_list_down) {    /* Scroll peerlist up and down one position */
        int L = y2 - CHATBOX_HEIGHT - SDBAR_OFST;

        if (groupchats[self->num].side_pos < groupchats[self->num].num_peers - L)
            ++groupchats[self->num].side_pos;
    } else if (key == user_settings->key_peer_list_up) {
        if (groupchats[self->num].side_pos > 0)
            --groupchats[self->num].side_pos;
    } else if (key == '\n') {
        rm_trailing_spaces_buf(ctx);

        char line[MAX_STR_SIZE];

        if (wcs_to_mbs_buf(line, ctx->line, MAX_STR_SIZE) == -1)
            memset(&line, 0, sizeof(line));

        if (!string_is_empty(line))
            add_line_to_hist(ctx);

        if (line[0] == '/') {
            if (strncmp(line, "/close", 6) == 0) {
                close_groupchat(self, m, self->num, line + 6, ctx->len - 6);
                return;
            } else if (strncmp(line, "/me ", strlen("/me ")) == 0) {
                send_group_action(self, m, self->num, line + strlen("/me "));
            } else {
                execute(ctx->history, self, m, line, GROUPCHAT_COMMAND_MODE);
            }
        } else if (!string_is_empty(line)) {
            send_group_message(self, m, self->num, line);
        }

        wclear(ctx->linewin);
        wmove(self->window, y2 - CURS_Y_OFFSET, 0);
        reset_buf(ctx);
    }
}

static void groupchat_onDraw(ToxWindow *self, Tox *m)
{
    int x2, y2;
    getmaxyx(self->window, y2, x2);

    ChatContext *ctx = self->chatwin;

    line_info_print(self);
    wclear(ctx->linewin);

    curs_set(1);

    if (ctx->len > 0)
        mvwprintw(ctx->linewin, 1, 0, "%ls", &ctx->line[ctx->start]);

    wclear(ctx->sidebar);
    mvwhline(self->window, y2 - CHATBOX_HEIGHT, 0, ACS_HLINE, x2);

    if (self->show_peerlist) {
        mvwvline(ctx->sidebar, 0, 0, ACS_VLINE, y2 - CHATBOX_HEIGHT);
        mvwaddch(ctx->sidebar, y2 - CHATBOX_HEIGHT, 0, ACS_BTEE);

        int num_peers = groupchats[self->num].num_peers;

        wmove(ctx->sidebar, 0, 1);
        wattron(ctx->sidebar, A_BOLD);
        wprintw(ctx->sidebar, "Peers: %d\n", num_peers);
        wattroff(ctx->sidebar, A_BOLD);

        mvwaddch(ctx->sidebar, 1, 0, ACS_LTEE);
        mvwhline(ctx->sidebar, 1, 1, ACS_HLINE, SIDEBAR_WIDTH - 1);

        int maxlines = y2 - SDBAR_OFST - CHATBOX_HEIGHT;
        int i;

        for (i = 0; i < num_peers && i < maxlines; ++i) {
            wmove(ctx->sidebar, i + 2, 1);
            int peer = i + groupchats[self->num].side_pos;

            /* truncate nick to fit in side panel without modifying list */
            char tmpnck[TOX_MAX_NAME_LENGTH];
            int maxlen = SIDEBAR_WIDTH - 2;
            memcpy(tmpnck, &groupchats[self->num].peer_names[peer * TOX_MAX_NAME_LENGTH], maxlen);
            tmpnck[maxlen] = '\0';

            wprintw(ctx->sidebar, "%s\n", tmpnck);
        }
    }

    int y, x;
    getyx(self->window, y, x);
    (void) x;
    int new_x = ctx->start ? x2 - 1 : wcswidth(ctx->line, ctx->pos);
    wmove(self->window, y + 1, new_x);

    wrefresh(self->window);

    if (self->help->active)
        help_onDraw(self);
}

static void groupchat_onInit(ToxWindow *self, Tox *m)
{
    int x2, y2;
    getmaxyx(self->window, y2, x2);

    ChatContext *ctx = self->chatwin;

    ctx->history = subwin(self->window, y2 - CHATBOX_HEIGHT + 1, x2 - SIDEBAR_WIDTH - 1, 0, 0);
    ctx->linewin = subwin(self->window, CHATBOX_HEIGHT, x2, y2 - CHATBOX_HEIGHT, 0);
    ctx->sidebar = subwin(self->window, y2 - CHATBOX_HEIGHT + 1, SIDEBAR_WIDTH, 0, x2 - SIDEBAR_WIDTH);

    ctx->hst = calloc(1, sizeof(struct history));
    ctx->log = calloc(1, sizeof(struct chatlog));

    if (ctx->log == NULL || ctx->hst == NULL)
        exit_toxic_err("failed in groupchat_onInit", FATALERR_MEMORY);

    line_info_init(ctx->hst);

    if (user_settings->autolog == AUTOLOG_ON) {
        char myid[TOX_FRIEND_ADDRESS_SIZE];
        tox_get_address(m, (uint8_t *) myid);
        log_enable(self->name, myid, NULL, ctx->log, LOG_GROUP);
    }

    execute(ctx->history, self, m, "/log", GLOBAL_COMMAND_MODE);

    char selfname[TOX_MAX_NAME_LENGTH];
    uint16_t len = tox_get_self_name(m, (uint8_t *) selfname);
    selfname[len] = '\0';
    tox_group_set_name(m, self->num, (uint8_t *) selfname, len);

    uint8_t status = tox_get_self_user_status(m);
    tox_group_set_status(m, self->num, status);

    scrollok(ctx->history, 0);
    wmove(self->window, y2 - CURS_Y_OFFSET, 0);
}


#ifdef AUDIO
static int group_audio_open_out_device(int groupnum)
{
    char dname[MAX_STR_SIZE];
    get_primary_device_name(output, dname, sizeof(dname));
    dname[MAX_STR_SIZE - 1] = '\0';

    groupchats[groupnum].audio.dvhandle = alcOpenDevice(dname);

    if (groupchats[groupnum].audio.dvhandle == NULL)
        return -1;

    groupchats[groupnum].audio.dvctx = alcCreateContext(groupchats[groupnum].audio.dvhandle, NULL);
    alcMakeContextCurrent(groupchats[groupnum].audio.dvctx);
    alGenBuffers(OPENAL_BUFS, groupchats[groupnum].audio.buffers);
    alGenSources((uint32_t) 1, &groupchats[groupnum].audio.source);
    alSourcei(groupchats[groupnum].audio.source, AL_LOOPING, AL_FALSE);

    if (alcGetError(groupchats[groupnum].audio.dvhandle) != AL_NO_ERROR) {
        group_audio_close_out_device(groupnum);
        groupchats[groupnum].audio.dvhandle = NULL;
        groupchats[groupnum].audio.dvctx = NULL;
        return -1;
    }

    alSourceQueueBuffers(groupchats[groupnum].audio.source, OPENAL_BUFS, groupchats[groupnum].audio.buffers);
    alSourcePlay(groupchats[groupnum].audio.source);

    return 0;
}

static int group_audio_close_out_device(int groupnum)
{
    if (!groupchats[groupnum].audio.dvhandle)
        return -1;

    if (!groupchats[groupnum].audio.dvctx)
        return -1;

    if (alcGetCurrentContext() != groupchats[groupnum].audio.dvctx)
        alcMakeContextCurrent(groupchats[groupnum].audio.dvctx);

    alDeleteSources((uint32_t) 1, &groupchats[groupnum].audio.source);
    alDeleteBuffers(OPENAL_BUFS, groupchats[groupnum].audio.buffers);

    alcMakeContextCurrent(NULL);
    alcDestroyContext(groupchats[groupnum].audio.dvctx);

    if (!alcCloseDevice(groupchats[groupnum].audio.dvhandle))
        return -1;

    return 0;
}

static int group_audio_write(int peernum, int groupnum, const int16_t *pcm, unsigned int samples, uint8_t channels,
                             unsigned int sample_rate)
{
    if (!pcm)
        return -1;

    if (channels == 0 || channels > 2)
        return -2;

    ALuint bufid;
    ALint processed = 0, queued = 0;

    alGetSourcei(groupchats[groupnum].audio.source, AL_BUFFERS_PROCESSED, &processed);
    alGetSourcei(groupchats[groupnum].audio.source, AL_BUFFERS_QUEUED, &queued);
    fprintf(stderr, "source: %d, queued: %d, processed: %d\n", groupchats[groupnum].audio.source, queued, processed);

    if (processed) {
        ALuint bufids[processed];
        alSourceUnqueueBuffers(groupchats[groupnum].audio.source, processed, bufids);
        alDeleteBuffers(processed - 1, bufids + 1);
        bufid = bufids[0];
    } else if (queued < 16) {
        alGenBuffers(1, &bufid);
    } else {
        return -3;
    }

    int length = samples * channels * sizeof(int16_t);

    alBufferData(bufid, (channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16, pcm, length, sample_rate);
    alSourceQueueBuffers(groupchats[groupnum].audio.source, 1, &bufid);

    ALint state;
    alGetSourcei(groupchats[groupnum].audio.source, AL_SOURCE_STATE, &state);

    if (state != AL_PLAYING)
        alSourcePlay(groupchats[groupnum].audio.source);

    return 0;
}

static void groupchat_onWriteDevice(ToxWindow *self, Tox *m, int groupnum, int peernum, const int16_t *pcm,
                                    unsigned int samples, uint8_t channels, unsigned int sample_rate)
{
    return;

    if (groupnum != self->num)
        return;

    if (peernum < 0)
        return;

    if (groupchats[groupnum].audio.dvhandle == NULL)
        fprintf(stderr, "dvhandle is null)\n");

    if (groupchats[groupnum].audio.dvctx == NULL)
        fprintf(stderr, "ctx is null\n");

    int ret = group_audio_write(peernum, groupnum, pcm, samples, channels, sample_rate);
    fprintf(stderr, "write: %d\n", ret);
}
#endif  /* AUDIO */

ToxWindow new_group_chat(Tox *m, int groupnum, const char *groupname, int length)
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
    ret.onGroupPrivateMessage = &groupchat_onGroupPrivateMessage;
    ret.onGroupAction = &groupchat_onGroupAction;
    ret.onGroupPeerJoin = &groupchat_onGroupPeerJoin;
    ret.onGroupPeerExit = &groupchat_onGroupPeerExit;
    ret.onGroupTopicChange = &groupchat_onGroupTopicChange;
    ret.onGroupOpCertificate = &groupchat_onGroupOpCertificate;
    ret.onGroupNickChange = &groupchat_onGroupNickChange;
    ret.onGroupSelfJoin = &groupchat_onGroupSelfJoin;
    ret.onGroupSelfTimeout = &groupchat_onGroupSelfTimeout;

#ifdef AUDIO
    ret.onWriteDevice = &groupchat_onWriteDevice;
#endif

    if (groupname && length)
        set_window_title(&ret, groupname, length);
    else
        snprintf(ret.name, sizeof(ret.name), "Group %d", groupnum);

    ChatContext *chatwin = calloc(1, sizeof(ChatContext));
    Help *help = calloc(1, sizeof(Help));

    if (chatwin == NULL || help == NULL)
        exit_toxic_err("failed in new_group_chat", FATALERR_MEMORY);

    ret.chatwin = chatwin;
    ret.help = help;

    ret.num = groupnum;
    ret.show_peerlist = true;
    ret.active_box = -1;

    return ret;
}

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
#include "audio_device.h"

extern char *DATA_FILE;

static GroupChat groupchats[MAX_GROUPCHAT_NUM];
static int max_groupchat_index = 0;

extern struct user_settings *user_settings;
extern struct Winthread Winthread;

#ifdef AUDIO
#define AC_NUM_GROUP_COMMANDS 22
#else
#define AC_NUM_GROUP_COMMANDS 18
#endif /* AUDIO */

/* Array of groupchat command names used for tab completion. */
static const char group_cmd_list[AC_NUM_GROUP_COMMANDS][MAX_CMDNAME_SIZE] = {
    { "/accept"     },
    { "/add"        },
    { "/avatar"     },
    { "/clear"      },
    { "/close"      },
    { "/connect"    },
    { "/decline"    },
    { "/exit"       },
    { "/group"      },
    { "/help"       },
    { "/log"        },
    { "/myid"       },
    { "/nick"       },
    { "/note"       },
    { "/quit"       },
    { "/requests"   },
    { "/status"     },
    { "/title"      },

#ifdef AUDIO

    { "/lsdev"       },
    { "/sdev"        },
    { "/mute"        },
    { "/sense"       },

#endif /* AUDIO */
};

#ifdef AUDIO
static int group_audio_open_out_device(int groupnum);
static int group_audio_close_out_device(int groupnum);
#endif  /* AUDIO */

int init_groupchat_win(ToxWindow *prompt, Tox *m, int groupnum, uint8_t type)
{
    if (groupnum > MAX_GROUPCHAT_NUM)
        return -1;

    ToxWindow self = new_group_chat(m, groupnum);
    int i;

    for (i = 0; i <= max_groupchat_index; ++i) {
        if (!groupchats[i].active) {
            groupchats[i].chatwin = add_window(m, self);
            groupchats[i].active = true;
            groupchats[i].num_peers = 0;
            groupchats[i].type = type;
            groupchats[i].start_time = get_unix_time();

            groupchats[i].peer_names = malloc(sizeof(uint8_t) * TOX_MAX_NAME_LENGTH);
            groupchats[i].oldpeer_names = malloc(sizeof(uint8_t) * TOX_MAX_NAME_LENGTH);
            groupchats[i].peer_name_lengths = malloc(sizeof(uint16_t));
            groupchats[i].oldpeer_name_lengths = malloc(sizeof(uint16_t));

            if (groupchats[i].peer_names == NULL || groupchats[i].oldpeer_names == NULL
                || groupchats[i].peer_name_lengths == NULL || groupchats[i].oldpeer_name_lengths == NULL)
                exit_toxic_err("failed in init_groupchat_win", FATALERR_MEMORY);

            memcpy(&groupchats[i].oldpeer_names[0], UNKNOWN_NAME, sizeof(UNKNOWN_NAME));
            groupchats[i].oldpeer_name_lengths[0] = (uint16_t) strlen(UNKNOWN_NAME);

#ifdef AUDIO
            if (type == TOX_GROUPCHAT_TYPE_AV)
                if (group_audio_open_out_device(i) == -1)
                    fprintf(stderr, "Group Audio failed to init\n");
#endif /* AUDIO */

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

void close_groupchat(ToxWindow *self, Tox *m, int groupnum)
{
    tox_del_groupchat(m, groupnum);
#ifdef AUDIO
    group_audio_close_out_device(groupnum);
#endif

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
    tox_self_get_name(m, (uint8_t *) selfnick);

    size_t sn_len = tox_self_get_name_size(m);
    selfnick[sn_len] = '\0';

    int nick_clr = strcmp(nick, selfnick) == 0 ? GREEN : CYAN;

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
    tox_self_get_name(m, (uint8_t *) selfnick);

    size_t n_len = tox_self_get_name_size(m);
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

static void groupchat_onGroupTitleChange(ToxWindow *self, Tox *m, int groupnum, int peernum, const char *title,
                                         uint8_t length)
{
    ChatContext *ctx = self->chatwin;

    if (self->num != groupnum)
        return;

    set_window_title(self, title, length);

    char timefrmt[TIME_STR_SIZE];
    get_time_str(timefrmt, sizeof(timefrmt));

    /* don't announce title when we join the room */
    if (!timed_out(groupchats[self->num].start_time, get_unix_time(), GROUP_EVENT_WAIT))
        return;

    char nick[TOX_MAX_NAME_LENGTH];
    get_group_nick_truncate(m, nick, peernum, groupnum);
    line_info_add(self, timefrmt, nick, NULL, NAME_CHANGE, 0, 0, " set the group title to: %s", title);

    char tmp_event[MAX_STR_SIZE];
    snprintf(tmp_event, sizeof(tmp_event), "set title to %s", title);
    write_to_log(tmp_event, nick, ctx->log, true);
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

    memcpy(groupchats[gnum].oldpeer_names, groupchats[gnum].peer_names, N * npeers);
    memcpy(groupchats[gnum].oldpeer_name_lengths, groupchats[gnum].peer_name_lengths, sizeof(uint16_t) * npeers);
}

struct group_add_thrd {
    Tox *m;
    ToxWindow *self;
    int peernum;
    int groupnum;
    uint64_t timestamp;
    pthread_t tid;
    pthread_attr_t attr;
};

/* Waits GROUP_EVENT_WAIT seconds for a new peer to set their name before announcing them */
void *group_add_wait(void *data)
{
    struct group_add_thrd *thrd = (struct group_add_thrd *) data;
    ToxWindow *self = thrd->self;
    Tox *m = thrd->m;
    char peername[TOX_MAX_NAME_LENGTH];

    /* keep polling for a name that differs from the default until we run out of time */
    while (true) {
        usleep(100000);

        pthread_mutex_lock(&Winthread.lock);
        get_group_nick_truncate(m, peername, thrd->peernum, thrd->groupnum);

        if (strcmp(peername, DEFAULT_TOX_NAME) || timed_out(thrd->timestamp, get_unix_time(), GROUP_EVENT_WAIT)) {
            pthread_mutex_unlock(&Winthread.lock);
            break;
        }

        pthread_mutex_unlock(&Winthread.lock);
    }

    const char *event = "has joined the room";
    char timefrmt[TIME_STR_SIZE];
    get_time_str(timefrmt, sizeof(timefrmt));

    pthread_mutex_lock(&Winthread.lock);
    line_info_add(self, timefrmt, (char *) peername, NULL, CONNECTION, 0, GREEN, event);
    write_to_log(event, (char *) peername, self->chatwin->log, true);
    pthread_mutex_unlock(&Winthread.lock);

    pthread_attr_destroy(&thrd->attr);
    free(thrd);
    pthread_exit(NULL);
}

static void groupchat_onGroupNamelistChange(ToxWindow *self, Tox *m, int groupnum, int peernum, uint8_t change)
{
    if (self->num != groupnum)
        return;

    if (groupnum > max_groupchat_index)
        return;

    groupchats[groupnum].num_peers = tox_group_number_peers(m, groupnum);
    int num_peers = groupchats[groupnum].num_peers;

    if (peernum > num_peers)
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

    if (tox_group_get_names(m, groupnum, tmp_peerlist, tmp_peerlens, num_peers) == -1) {
        memset(tmp_peerlist, 0, sizeof(tmp_peerlist));
        memset(tmp_peerlens, 0, sizeof(tmp_peerlens));
    }

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

    const char *event;
    char timefrmt[TIME_STR_SIZE];
    get_time_str(timefrmt, sizeof(timefrmt));

    switch (change) {
        case TOX_CHAT_CHANGE_PEER_ADD:
            if (!timed_out(groupchats[groupnum].start_time, get_unix_time(), GROUP_EVENT_WAIT))
                break;

            struct group_add_thrd *thrd = malloc(sizeof(struct group_add_thrd));
            thrd->m = m;
            thrd->peernum = peernum;
            thrd->groupnum = groupnum;
            thrd->self = self;
            thrd->timestamp = get_unix_time();

            if (pthread_attr_init(&thrd->attr) != 0) {
                free(thrd);
                return;
            }

            if (pthread_attr_setdetachstate(&thrd->attr, PTHREAD_CREATE_DETACHED) != 0) {
                pthread_attr_destroy(&thrd->attr);
                free(thrd);
                return;
            }

            if (pthread_create(&thrd->tid, &thrd->attr, group_add_wait, (void *) thrd) != 0) {
                pthread_attr_destroy(&thrd->attr);
                free(thrd);
                return;
            }

            break;

        case TOX_CHAT_CHANGE_PEER_DEL:
            event = "has left the room";
            line_info_add(self, timefrmt, (char *) oldpeername, NULL, DISCONNECTION, 0, RED, event);

            if (groupchats[self->num].side_pos > 0)
                --groupchats[self->num].side_pos;

            write_to_log(event, (char *) oldpeername, ctx->log, true);
            break;

        case TOX_CHAT_CHANGE_PEER_NAME:
            if (!timed_out(groupchats[self->num].start_time, get_unix_time(), GROUP_EVENT_WAIT))
                return;

            /* ignore initial name change (TODO: this is a bad way to do this) */
            if (strcmp((char *) oldpeername, DEFAULT_TOX_NAME) == 0)
                return;

            event = " is now known as ";
            line_info_add(self, timefrmt, (char *) oldpeername, (char *) peername, NAME_CHANGE, 0, 0, event);

            char tmp_event[TOXIC_MAX_NAME_LENGTH * 2 + 32];
            snprintf(tmp_event, sizeof(tmp_event), "is now known as %s", (char *) peername);
            write_to_log(tmp_event, (char *) oldpeername, ctx->log, true);
            break;
    }

    sound_notify(self, silent, NT_WNDALERT_2, NULL);
}

static void send_group_action(ToxWindow *self, ChatContext *ctx, Tox *m, char *action)
{
    if (action == NULL) {
        wprintw(ctx->history, "Invalid syntax.\n");
        return;
    }

    if (tox_group_action_send(m, self->num, (uint8_t *) action, strlen(action)) == -1) {
        const char *errmsg = " * Failed to send action.";
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, RED, errmsg);
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
                sound_notify(self, notif_error, 0, NULL);
            }
        } else {
            sound_notify(self, notif_error, 0, NULL);
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
            if (strcmp(line, "/close") == 0) {
                close_groupchat(self, m, self->num);
                return;
            } else if (strncmp(line, "/me ", strlen("/me ")) == 0) {
                send_group_action(self, ctx, m, line + strlen("/me "));
            } else {
                execute(ctx->history, self, m, line, GROUPCHAT_COMMAND_MODE);
            }
        } else if (!string_is_empty(line)) {
            if (tox_group_message_send(m, self->num, (uint8_t *) line, strlen(line)) == -1) {
                const char *errmsg = " * Failed to send message.";
                line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, RED, errmsg);
            }
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
        char myid[TOX_ADDRESS_SIZE];
        tox_self_get_address(m, (uint8_t *) myid);
        log_enable(self->name, myid, NULL, ctx->log, LOG_GROUP);
    }

    execute(ctx->history, self, m, "/log", GLOBAL_COMMAND_MODE);

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
    ret.onGroupTitleChange = &groupchat_onGroupTitleChange;

#ifdef AUDIO
    ret.onWriteDevice = &groupchat_onWriteDevice;
#endif

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

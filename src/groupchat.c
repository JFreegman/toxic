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
#include <inttypes.h>
#include <math.h>

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
#endif /* ALC_ALL_DEVICES_SPECIFIER */
#endif /* __APPLE__ */
#endif /* AUDIO */

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

extern pthread_mutex_t tox_lock;

#ifdef PYTHON
#define AC_NUM_GROUP_COMMANDS_PYTHON 1
#else
#define AC_NUM_GROUP_COMMANDS_PYTHON 0
#endif /* PYTHON */
#ifdef QRCODE
#define AC_NUM_GROUP_COMMANDS_QRCODE 1
#else
#define AC_NUM_GROUP_COMMANDS_QRCODE 0
#endif /* QRCODE */
#define AC_NUM_GROUP_COMMANDS (19 + AC_NUM_GROUP_COMMANDS_PYTHON + AC_NUM_GROUP_COMMANDS_QRCODE)

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
#ifdef QRCODE
    { "/myqr"       },
#endif /* QRCODE */
    { "/nick"       },
    { "/note"       },
    { "/nospam"     },
    { "/quit"       },
    { "/requests"   },
    { "/status"     },
    { "/title"      },

#ifdef PYTHON

    { "/run"        },

#endif /* PYTHON */
};

static ToxWindow *new_group_chat(uint32_t groupnum);

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

int init_groupchat_win(Tox *m, uint32_t groupnum, uint8_t type, const char *title,
                       size_t title_length)
{
    if (groupnum > MAX_GROUPCHAT_NUM) {
        return -1;
    }

    ToxWindow *self = new_group_chat(groupnum);

    for (int i = 0; i <= max_groupchat_index; ++i) {
        if (!groupchats[i].active) {
            // FIXME: it is assumed at various points in the code that
            // toxcore's groupnums agree with toxic's indices to groupchats;
            // probably it so happens that this will (at least typically) be
            // the case, because toxic and tox maintain the indices in
            // parallel ways. But it isn't guaranteed by the API.
            groupchats[i].chatwin = add_window(m, self);
            groupchats[i].active = true;
            groupchats[i].num_peers = 0;
            groupchats[i].type = type;
            groupchats[i].start_time = get_unix_time();
            groupchats[i].audio_enabled = false;
            groupchats[i].last_sent_audio = 0;

            set_active_window_index(groupchats[i].chatwin);
            set_window_title(self, title, title_length);

            if (i == max_groupchat_index) {
                ++max_groupchat_index;
            }

            return groupchats[i].chatwin;
        }
    }

    kill_groupchat_window(self);

    return -1;
}

static void free_peer(GroupPeer *peer)
{
    if (peer->sending_audio) {
        close_device(output, peer->audio_out_idx);
    }
}

void free_groupchat(ToxWindow *self, uint32_t groupnum)
{
    GroupChat *chat = &groupchats[groupnum];

    for (uint32_t i = 0; i < chat->num_peers; ++i) {
        GroupPeer *peer = &chat->peer_list[i];

        if (peer->active) {
            free_peer(peer);
        }
    }

    if (chat->audio_enabled) {
        close_device(input, chat->audio_in_idx);
    }

    free(chat->name_list);
    free(chat->peer_list);
    memset(chat, 0, sizeof(GroupChat));

    int i;

    for (i = max_groupchat_index; i > 0; --i) {
        if (groupchats[i - 1].active) {
            break;
        }
    }

    max_groupchat_index = i;
    kill_groupchat_window(self);
}

static void delete_groupchat(ToxWindow *self, Tox *m, uint32_t groupnum)
{
    tox_conference_delete(m, groupnum, NULL);
    free_groupchat(self, groupnum);
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

    if (y2 <= 0 || x2 <= 0) {
        return;
    }

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

static void groupchat_onGroupMessage(ToxWindow *self, Tox *m, uint32_t groupnum, uint32_t peernum,
                                     Tox_Message_Type type, const char *msg, size_t len)
{
    UNUSED_VAR(len);

    if (self->num != groupnum) {
        return;
    }

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
        sound_notify(self, generic_message, NT_WNDALERT_0 | user_settings->bell_on_message, NULL);

        if (self->active_box != -1) {
            box_silent_notify2(self, NT_NOFOCUS, self->active_box, "%s %s", nick, msg);
        } else {
            box_silent_notify(self, NT_NOFOCUS, &self->active_box, self->name, "%s %s", nick, msg);
        }

        nick_clr = RED;
    } else {
        sound_notify(self, silent, NT_WNDALERT_1, NULL);
    }

    char timefrmt[TIME_STR_SIZE];
    get_time_str(timefrmt, sizeof(timefrmt));

    line_info_add(self, timefrmt, nick, NULL, type == TOX_MESSAGE_TYPE_NORMAL ? IN_MSG : IN_ACTION, 0, nick_clr, "%s", msg);
    write_to_log(msg, nick, ctx->log, false);
}

static void groupchat_onGroupTitleChange(ToxWindow *self, Tox *m, uint32_t groupnum, uint32_t peernum,
        const char *title,
        size_t length)
{
    ChatContext *ctx = self->chatwin;

    if (self->num != groupnum) {
        return;
    }

    set_window_title(self, title, length);

    char timefrmt[TIME_STR_SIZE];
    get_time_str(timefrmt, sizeof(timefrmt));

    /* don't announce title when we join the room */
    if (!timed_out(groupchats[self->num].start_time, GROUP_EVENT_WAIT)) {
        return;
    }

    char nick[TOX_MAX_NAME_LENGTH];
    get_group_nick_truncate(m, nick, peernum, groupnum);
    line_info_add(self, timefrmt, nick, NULL, NAME_CHANGE, 0, 0, " set the group title to: %s", title);

    char tmp_event[MAX_STR_SIZE];
    snprintf(tmp_event, sizeof(tmp_event), "set title to %s", title);
    write_to_log(tmp_event, nick, ctx->log, true);
}

struct NameListEntry {
    char name[TOX_MAX_NAME_LENGTH];
    uint32_t peernum;
};

static int compare_name_list_entries(const void *a, const void *b)
{
    return qsort_strcasecmp_hlpr(
               ((NameListEntry *)a)->name,
               ((NameListEntry *)b)->name);
}

static void group_update_name_list(uint32_t groupnum)
{
    GroupChat *chat = &groupchats[groupnum];

    if (!chat->active) {
        return;
    }

    if (chat->name_list) {
        free(chat->name_list);
    }

    chat->name_list = malloc(chat->num_peers * sizeof(NameListEntry));

    if (chat->name_list == NULL) {
        exit_toxic_err("failed in group_update_name_list", FATALERR_MEMORY);
    }

    uint32_t count = 0;

    for (uint32_t i = 0; i < chat->max_idx; ++i) {
        if (chat->peer_list[i].active) {
            memcpy(chat->name_list[count].name, chat->peer_list[i].name, chat->peer_list[i].name_length + 1);
            chat->name_list[count].peernum = i;
            ++count;
        }
    }

    qsort(chat->name_list, count, sizeof(NameListEntry), compare_name_list_entries);
}

/* Reallocates groupnum's peer list.
 *
 * Returns 0 on success.
 * Returns -1 on failure.
 */
static int realloc_peer_list(GroupChat *chat, uint32_t num_peers)
{
    if (!chat) {
        return -1;
    }

    if (num_peers == 0) {
        free(chat->peer_list);
        chat->peer_list = NULL;
        return 0;
    }

    struct GroupPeer *tmp_list = realloc(chat->peer_list, num_peers * sizeof(struct GroupPeer));

    if (!tmp_list) {
        return -1;
    }

    chat->peer_list = tmp_list;

    return 0;
}

static void set_peer_audio_position(Tox *m, uint32_t groupnum, uint32_t peernum)
{
    GroupChat *chat = &groupchats[groupnum];
    GroupPeer *peer = &chat->peer_list[peernum];

    if (!peer->sending_audio) {
        return;
    }

    // Position peers at distance 1 in front of listener,
    // ordered left to right by order in peerlist excluding self.
    uint32_t num_posns = chat->num_peers;
    uint32_t peer_posn = peernum;

    for (uint32_t i = 0; i < chat->num_peers; ++i) {
        if (tox_conference_peer_number_is_ours(m, groupnum, peernum, NULL)) {
            if (i == peernum) {
                return;
            }

            --num_posns;

            if (i < peernum) {
                --peer_posn;
            }
        }
    }

    const float angle = asinf(peer_posn - (float)(num_posns - 1) / 2);
    set_source_position(peer->audio_out_idx, sinf(angle), cosf(angle), 0);
}


static bool find_peer_by_pubkey(GroupPeer *list, uint32_t num_peers, uint8_t *pubkey, uint32_t *idx)
{
    for (uint32_t i = 0; i < num_peers; ++i) {
        GroupPeer *peer = &list[i];

        if (peer->active && memcmp(peer->pubkey, pubkey, TOX_PUBLIC_KEY_SIZE) == 0) {
            *idx = i;
            return true;
        }
    }

    return false;
}

static void update_peer_list(Tox *m, uint32_t groupnum, uint32_t num_peers, uint32_t old_num_peers)
{
    GroupChat *chat = &groupchats[groupnum];

    if (!chat->active) {
        return;
    }

    GroupPeer *old_peer_list = malloc(old_num_peers * sizeof(GroupPeer));

    if (!old_peer_list) {
        exit_toxic_err("failed in update_peer_list", FATALERR_MEMORY);
        return;
    }

    memcpy(old_peer_list, chat->peer_list, old_num_peers * sizeof(GroupPeer));

    realloc_peer_list(chat, num_peers);
    memset(chat->peer_list, 0, num_peers * sizeof(GroupPeer));

    for (uint32_t i = 0; i < num_peers; ++i) {
        GroupPeer *peer = &chat->peer_list[i];

        Tox_Err_Conference_Peer_Query err;
        tox_conference_peer_get_public_key(m, groupnum, i, peer->pubkey, &err);

        if (err != TOX_ERR_CONFERENCE_PEER_QUERY_OK) {
            continue;
        }

        uint32_t j;

        if (find_peer_by_pubkey(old_peer_list, old_num_peers, peer->pubkey, &j)) {
            GroupPeer *old_peer = &old_peer_list[j];
            memcpy(peer, old_peer, sizeof(GroupPeer));
            old_peer->active = false;
        }

        size_t length = tox_conference_peer_get_name_size(m, groupnum, i, &err);

        if (err != TOX_ERR_CONFERENCE_PEER_QUERY_OK || length >= TOX_MAX_NAME_LENGTH) {
            // FIXME: length == TOX_MAX_NAME_LENGTH should not be an error!
            continue;
        }

        tox_conference_peer_get_name(m, groupnum, i, (uint8_t *) peer->name, &err);
        peer->name[length] = 0;

        if (err != TOX_ERR_CONFERENCE_PEER_QUERY_OK) {
            continue;
        }

        peer->active = true;
        peer->name_length = length;
        peer->peernumber = i;

        set_peer_audio_position(m, groupnum, i);
    }

    for (uint32_t i = 0; i < old_num_peers; ++i) {
        GroupPeer *old_peer = &old_peer_list[i];

        if (old_peer->active) {
            free_peer(old_peer);
        }
    }

    free(old_peer_list);

    group_update_name_list(groupnum);
}

static void groupchat_onGroupNameListChange(ToxWindow *self, Tox *m, uint32_t groupnum)
{
    if (self->num != groupnum) {
        return;
    }

    if (groupnum > max_groupchat_index) {
        return;
    }

    GroupChat *chat = &groupchats[groupnum];

    if (!chat->active) {
        return;
    }

    Tox_Err_Conference_Peer_Query err;

    uint32_t num_peers = tox_conference_peer_count(m, groupnum, &err);
    uint32_t old_num = chat->num_peers;

    if (err == TOX_ERR_CONFERENCE_PEER_QUERY_OK) {
        chat->num_peers = num_peers;
    } else {
        num_peers = old_num;
    }

    chat->max_idx = num_peers;
    update_peer_list(m, groupnum, num_peers, old_num);
}

static void groupchat_onGroupPeerNameChange(ToxWindow *self, Tox *m, uint32_t groupnum, uint32_t peernum,
        const char *name, size_t length)
{
    UNUSED_VAR(length);

    if (self->num != groupnum) {
        return;
    }

    GroupChat *chat = &groupchats[groupnum];

    if (!chat->active) {
        return;
    }

    uint32_t i;

    for (i = 0; i < chat->max_idx; ++i) {
        GroupPeer *peer = &chat->peer_list[i];

        if (peer->active && peer->peernumber == peernum && peer->name_length > 0) {
            ChatContext *ctx = self->chatwin;
            char timefrmt[TIME_STR_SIZE];
            get_time_str(timefrmt, sizeof(timefrmt));

            char tmp_event[TOXIC_MAX_NAME_LENGTH * 2 + 32];
            snprintf(tmp_event, sizeof(tmp_event), "is now known as %s", (const char *) name);

            write_to_log(tmp_event, peer->name, ctx->log, true);
            line_info_add(self, timefrmt, peer->name, (const char *) name, NAME_CHANGE, 0, 0, " is now known as ");

            break;
        }
    }

    groupchat_onGroupNameListChange(self, m, groupnum);
}

static void send_group_action(ToxWindow *self, ChatContext *ctx, Tox *m, char *action)
{
    if (action == NULL) {
        wprintw(ctx->history, "Invalid syntax.\n");
        return;
    }

    Tox_Err_Conference_Send_Message err;

    if (!tox_conference_send_message(m, self->num, TOX_MESSAGE_TYPE_ACTION, (uint8_t *) action, strlen(action), &err)) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, RED, " * Failed to send action (error %d)", err);
    }
}

/* Offset for the peer number box at the top of the statusbar */
static int sidebar_offset(uint32_t groupnum)
{
    return 2 + groupchats[groupnum].audio_enabled;
}


static void groupchat_onKey(ToxWindow *self, Tox *m, wint_t key, bool ltr)
{
    ChatContext *ctx = self->chatwin;

    int x, y, y2, x2;
    getyx(self->window, y, x);
    getmaxyx(self->window, y2, x2);

    UNUSED_VAR(y);

    if (x2 <= 0 || y2 <= 0) {
        return;
    }

    if (self->help->active) {
        help_onKey(self, key);
        return;
    }

    if (ctx->pastemode && key == '\r') {
        key = '\n';
    }

    if (ltr || key == '\n') {    /* char is printable */
        input_new_char(self, key, x, x2);
        return;
    }

    if (line_info_onKey(self, key)) {
        return;
    }

    if (input_handle(self, key, x, x2)) {
        return;
    }

    if (key == '\t') {  /* TAB key: auto-completes peer name or command */
        if (ctx->len > 0) {
            int diff = -1;

            /* TODO: make this not suck */
            if (ctx->line[0] != L'/' || wcscmp(ctx->line, L"/me") == 0) {
                char *names = malloc(groupchats[self->num].num_peers * TOX_MAX_NAME_LENGTH);

                if (names) {
                    for (uint32_t i = 0; i < groupchats[self->num].num_peers; ++i) {
                        memcpy(&names[i * TOX_MAX_NAME_LENGTH], groupchats[self->num].name_list[i].name, TOX_MAX_NAME_LENGTH);
                    }

                    diff = complete_line(self, names, groupchats[self->num].num_peers, TOX_MAX_NAME_LENGTH);
                    free(names);
                }
            } else if (wcsncmp(ctx->line, L"/avatar ", wcslen(L"/avatar ")) == 0) {
                diff = dir_match(self, m, ctx->line, L"/avatar");
            }

#ifdef PYTHON
            else if (wcsncmp(ctx->line, L"/run ", wcslen(L"/run ")) == 0) {
                diff = dir_match(self, m, ctx->line, L"/run");
            }

#endif

            else {
                diff = complete_line(self, group_cmd_list, AC_NUM_GROUP_COMMANDS, MAX_CMDNAME_SIZE);
            }

            if (diff != -1) {
                if (x + diff > x2 - 1) {
                    int wlen = MAX(0, wcswidth(ctx->line, sizeof(ctx->line) / sizeof(wchar_t)));
                    ctx->start = wlen < x2 ? 0 : wlen - x2 + 1;
                }
            } else {
                sound_notify(self, notif_error, 0, NULL);
            }
        } else {
            sound_notify(self, notif_error, 0, NULL);
        }
    } else if (key == user_settings->key_peer_list_down) {    /* Scroll peerlist up and down one position */
        const int L = y2 - CHATBOX_HEIGHT - sidebar_offset(self->num);

        if (groupchats[self->num].side_pos < (int64_t) groupchats[self->num].num_peers - L) {
            ++groupchats[self->num].side_pos;
        }
    } else if (key == user_settings->key_peer_list_up) {
        if (groupchats[self->num].side_pos > 0) {
            --groupchats[self->num].side_pos;
        }
    } else if (key == '\r') {
        rm_trailing_spaces_buf(ctx);

        if (!wstring_is_empty(ctx->line)) {
            add_line_to_hist(ctx);

            wstrsubst(ctx->line, L'¶', L'\n');

            char line[MAX_STR_SIZE];

            if (wcs_to_mbs_buf(line, ctx->line, MAX_STR_SIZE) == -1) {
                memset(&line, 0, sizeof(line));
            }

            if (line[0] == '/') {
                if (strcmp(line, "/close") == 0) {
                    delete_groupchat(self, m, self->num);
                    return;
                } else if (strncmp(line, "/me ", strlen("/me ")) == 0) {
                    send_group_action(self, ctx, m, line + strlen("/me "));
                } else {
                    execute(ctx->history, self, m, line, GROUPCHAT_COMMAND_MODE);
                }
            } else {
                Tox_Err_Conference_Send_Message err;

                if (!tox_conference_send_message(m, self->num, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) line, strlen(line), &err)) {
                    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, RED, " * Failed to send message (error %d)", err);
                }
            }
        }

        wclear(ctx->linewin);
        wmove(self->window, y2 - CURS_Y_OFFSET, 0);
        reset_buf(ctx);
    }
}

static void groupchat_onDraw(ToxWindow *self, Tox *m)
{
    UNUSED_VAR(m);

    int x2, y2;
    getmaxyx(self->window, y2, x2);

    if (x2 <= 0 || y2 <= 0) {
        return;
    }

    ChatContext *ctx = self->chatwin;

    pthread_mutex_lock(&Winthread.lock);
    line_info_print(self);
    pthread_mutex_unlock(&Winthread.lock);

    wclear(ctx->linewin);

    curs_set(1);

    if (ctx->len > 0) {
        mvwprintw(ctx->linewin, 1, 0, "%ls", &ctx->line[ctx->start]);
    }

    wclear(ctx->sidebar);
    mvwhline(self->window, y2 - CHATBOX_HEIGHT, 0, ACS_HLINE, x2);

    if (self->show_peerlist) {
        mvwvline(ctx->sidebar, 0, 0, ACS_VLINE, y2 - CHATBOX_HEIGHT);
        mvwaddch(ctx->sidebar, y2 - CHATBOX_HEIGHT, 0, ACS_BTEE);

        pthread_mutex_lock(&Winthread.lock);
        const uint32_t num_peers = groupchats[self->num].num_peers;
        const bool audio = groupchats[self->num].audio_enabled;
        const bool self_mute = groupchats[self->num].mute;
        const int header_lines = sidebar_offset(self->num);
        pthread_mutex_unlock(&Winthread.lock);

        int line = 0;

        if (audio) {
            wmove(ctx->sidebar, line, 1);
            wattron(ctx->sidebar, A_BOLD);
            wprintw(ctx->sidebar, "Mic: ");
            const bool mic_on = audio && !self_mute;
            const int color = mic_on ? GREEN : RED;
            wattron(ctx->sidebar, COLOR_PAIR(color));
            wprintw(ctx->sidebar, mic_on ? "ON" : "OFF");
            wattroff(ctx->sidebar, COLOR_PAIR(color));
            wattroff(ctx->sidebar, A_BOLD);
            ++line;
        }

        wmove(ctx->sidebar, line, 1);
        wattron(ctx->sidebar, A_BOLD);
        wprintw(ctx->sidebar, "Peers: %"PRIu32"\n", num_peers);
        wattroff(ctx->sidebar, A_BOLD);
        ++line;

        mvwaddch(ctx->sidebar, line, 0, ACS_LTEE);
        mvwhline(ctx->sidebar, line, 1, ACS_HLINE, SIDEBAR_WIDTH - 1);
        ++line;

        int maxlines = y2 - header_lines - CHATBOX_HEIGHT;
        uint32_t i;

        for (i = 0; i < num_peers && i < maxlines; ++i) {
            wmove(ctx->sidebar, i + header_lines, 1);

            pthread_mutex_lock(&Winthread.lock);
            const uint32_t peer = i + groupchats[self->num].side_pos;
            const uint32_t peernum = groupchats[self->num].name_list[peer].peernum;
            const bool is_self = tox_conference_peer_number_is_ours(m, self->num, peernum, NULL);
            pthread_mutex_unlock(&Winthread.lock);

            /* truncate nick to fit in side panel without modifying list */
            char tmpnck[TOX_MAX_NAME_LENGTH];
            int maxlen = SIDEBAR_WIDTH - 2 - audio;

            if (audio) {
                pthread_mutex_lock(&Winthread.lock);
                const GroupPeer *peer = &groupchats[self->num].peer_list[peernum];
                const bool audio_active = is_self
                                          ? !timed_out(groupchats[self->num].last_sent_audio, 2)
                                          : peer->active && peer->sending_audio && !timed_out(peer->last_audio_time, 2);
                const bool mute = is_self ? self_mute : peer->mute;
                pthread_mutex_unlock(&Winthread.lock);

                const int aud_attr = A_BOLD | COLOR_PAIR(audio_active && !mute ? GREEN : RED);
                wattron(ctx->sidebar, aud_attr);
                waddch(ctx->sidebar, audio_active ? (mute ? 'M' : '*') : '-');
                wattroff(ctx->sidebar, aud_attr);
            }

            pthread_mutex_lock(&Winthread.lock);
            memcpy(tmpnck, &groupchats[self->num].name_list[peer].name, maxlen);
            pthread_mutex_unlock(&Winthread.lock);

            tmpnck[maxlen] = '\0';

            if (is_self) {
                wattron(ctx->sidebar, COLOR_PAIR(GREEN));
            }

            wprintw(ctx->sidebar, "%s\n", tmpnck);

            if (is_self) {
                wattroff(ctx->sidebar, COLOR_PAIR(GREEN));
            }
        }
    }

    int y, x;
    getyx(self->window, y, x);

    UNUSED_VAR(x);

    int new_x = ctx->start ? x2 - 1 : MAX(0, wcswidth(ctx->line, ctx->pos));
    wmove(self->window, y + 1, new_x);

    wnoutrefresh(self->window);

    if (self->help->active) {
        help_onDraw(self);
    }
}

static void groupchat_onInit(ToxWindow *self, Tox *m)
{
    int x2, y2;
    getmaxyx(self->window, y2, x2);

    if (x2 <= 0 || y2 <= 0) {
        exit_toxic_err("failed in groupchat_onInit", FATALERR_CURSES);
    }

    ChatContext *ctx = self->chatwin;

    ctx->history = subwin(self->window, y2 - CHATBOX_HEIGHT + 1, x2 - SIDEBAR_WIDTH - 1, 0, 0);
    ctx->linewin = subwin(self->window, CHATBOX_HEIGHT, x2, y2 - CHATBOX_HEIGHT, 0);
    ctx->sidebar = subwin(self->window, y2 - CHATBOX_HEIGHT + 1, SIDEBAR_WIDTH, 0, x2 - SIDEBAR_WIDTH);

    ctx->hst = calloc(1, sizeof(struct history));
    ctx->log = calloc(1, sizeof(struct chatlog));

    if (ctx->log == NULL || ctx->hst == NULL) {
        exit_toxic_err("failed in groupchat_onInit", FATALERR_MEMORY);
    }

    line_info_init(ctx->hst);

    if (user_settings->autolog == AUTOLOG_ON) {
        char myid[TOX_ADDRESS_SIZE];
        tox_self_get_address(m, (uint8_t *) myid);

        if (log_enable(self->name, myid, NULL, ctx->log, LOG_GROUP) == -1) {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Warning: Log failed to initialize.");
        }
    }

    execute(ctx->history, self, m, "/log", GLOBAL_COMMAND_MODE);

    scrollok(ctx->history, 0);
    wmove(self->window, y2 - CURS_Y_OFFSET, 0);
}

static ToxWindow *new_group_chat(uint32_t groupnum)
{
    ToxWindow *ret = calloc(1, sizeof(ToxWindow));

    if (ret == NULL) {
        exit_toxic_err("failed in new_group_chat", FATALERR_MEMORY);
    }

    ret->is_groupchat = true;

    ret->onKey = &groupchat_onKey;
    ret->onDraw = &groupchat_onDraw;
    ret->onInit = &groupchat_onInit;
    ret->onGroupMessage = &groupchat_onGroupMessage;
    ret->onGroupNameListChange = &groupchat_onGroupNameListChange;
    ret->onGroupPeerNameChange = &groupchat_onGroupPeerNameChange;
    ret->onGroupTitleChange = &groupchat_onGroupTitleChange;

    snprintf(ret->name, sizeof(ret->name), "Group %u", groupnum);

    ChatContext *chatwin = calloc(1, sizeof(ChatContext));
    Help *help = calloc(1, sizeof(Help));

    if (chatwin == NULL || help == NULL) {
        exit_toxic_err("failed in new_group_chat", FATALERR_MEMORY);
    }

    ret->chatwin = chatwin;
    ret->help = help;

    ret->num = groupnum;
    ret->show_peerlist = true;
    ret->active_box = -1;

    return ret;
}

#define GROUPAV_SAMPLE_RATE 48000
#define GROUPAV_FRAME_DURATION 20
#define GROUPAV_AUDIO_CHANNELS 1
#define GROUPAV_SAMPLES_PER_FRAME (GROUPAV_SAMPLE_RATE * GROUPAV_FRAME_DURATION / 1000)

void audio_group_callback(void *tox, uint32_t groupnumber, uint32_t peernumber, const int16_t *pcm,
                          unsigned int samples, uint8_t channels, uint32_t sample_rate, void *userdata)
{
    GroupChat *chat = &groupchats[groupnumber];

    if (!chat->active) {
        return;
    }

    for (uint32_t i = 0; i < chat->max_idx; ++i) {
        GroupPeer *peer = &chat->peer_list[i];

        if (!peer->active || peer->peernumber != peernumber) {
            continue;
        }

        if (!peer->sending_audio) {
            if (open_output_device(&peer->audio_out_idx,
                                   sample_rate, GROUPAV_FRAME_DURATION, channels) != de_None) {
                // TODO: error message?
                return;
            }

            peer->sending_audio = true;
            peer->mute = false;

            set_peer_audio_position(tox, groupnumber, i);
        }

        write_out(peer->audio_out_idx, pcm, samples, channels, sample_rate);

        peer->last_audio_time = get_unix_time();

        return;
    }
}

static void group_read_device_callback(const int16_t *captured, uint32_t size, void *data)
{
    UNUSED_VAR(size);

    AudioInputCallbackData *audio_input_callback_data = (AudioInputCallbackData *)data;

    pthread_mutex_lock(&Winthread.lock);
    groupchats[audio_input_callback_data->groupnumber].last_sent_audio = get_unix_time();
    pthread_mutex_unlock(&Winthread.lock);

    pthread_mutex_lock(&tox_lock);
    toxav_group_send_audio(audio_input_callback_data->tox,
                           audio_input_callback_data->groupnumber,
                           captured, GROUPAV_SAMPLES_PER_FRAME,
                           GROUPAV_AUDIO_CHANNELS, GROUPAV_SAMPLE_RATE);
    pthread_mutex_unlock(&tox_lock);
}

bool init_group_audio_input(Tox *tox, uint32_t groupnumber)
{
    GroupChat *chat = &groupchats[groupnumber];

    if (!chat->active) {
        return false;
    }

    const AudioInputCallbackData audio_input_callback_data = { tox, groupnumber };
    chat->audio_input_callback_data = audio_input_callback_data;

    bool success = (open_input_device(&chat->audio_in_idx,
                                      group_read_device_callback, &chat->audio_input_callback_data, true,
                                      GROUPAV_SAMPLE_RATE, GROUPAV_FRAME_DURATION, GROUPAV_AUDIO_CHANNELS)
                    == de_None);

    chat->audio_enabled = success;
    chat->mute = false;

    return success;
}

bool enable_group_audio(Tox *tox, uint32_t groupnumber)
{
    if (!toxav_groupchat_av_enabled(tox, groupnumber)) {
        if (toxav_groupchat_enable_av(tox, groupnumber, audio_group_callback, NULL) != 0) {
            return false;
        }
    }

    return init_group_audio_input(tox, groupnumber);
}

bool disable_group_audio(Tox *tox, uint32_t groupnumber)
{
    GroupChat *chat = &groupchats[groupnumber];

    if (!chat->active) {
        return false;
    }

    if (chat->audio_enabled) {
        close_device(input, chat->audio_in_idx);
        chat->audio_enabled = false;
    }

    return (toxav_groupchat_disable_av(tox, groupnumber) == 0);
}

bool group_mute_self(uint32_t groupnumber)
{
    GroupChat *chat = &groupchats[groupnumber];

    if (!chat->active || !chat->audio_enabled) {
        return false;
    }

    device_mute(input, chat->audio_in_idx);
    chat->mute = !chat->mute;

    return true;
}

bool group_mute_peer(uint32_t groupnumber, const char *prefix)
{
    GroupChat *chat = &groupchats[groupnumber];

    if (!chat->active || !chat->audio_enabled) {
        return false;
    }

    const int len = strlen(prefix);

    for (uint32_t i = 0; i < chat->max_idx; ++i) {
        GroupPeer *peer = &chat->peer_list[i];

        if (peer->active && peer->sending_audio
                && peer->name_length >= len && strncmp(prefix, peer->name, len) == 0) {
            device_mute(output, peer->audio_out_idx);
            peer->mute = !peer->mute;
            return true;
        }
    }

    return false;
}

/*  conference.c
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
#include "conference.h"
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

static ConferenceChat conferences[MAX_CONFERENCE_NUM];
static int max_conference_index = 0;

extern struct user_settings *user_settings;
extern struct Winthread Winthread;

/* Array of conference command names used for tab completion. */
static const char *conference_cmd_list[] = {
    "/accept",
    "/add",
    "/avatar",
    "/clear",
    "/close",
    "/connect",
    "/decline",
    "/exit",
    "/conference",
    "/help",
    "/log",
    "/myid",
#ifdef QRCODE
    "/myqr",
#endif /* QRCODE */
    "/nick",
    "/note",
    "/nospam",
    "/quit",
    "/requests",
    "/status",
    "/title",

#ifdef PYTHON

    "/run",

#endif /* PYTHON */
};

static ToxWindow *new_conference_chat(uint32_t conferencenum);

static void kill_conference_window(ToxWindow *self)
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
    kill_notifs(self->active_box);
    del_window(self);
}

int init_conference_win(Tox *m, uint32_t conferencenum, uint8_t type, const char *title,
                        size_t title_length)
{
    if (conferencenum > MAX_CONFERENCE_NUM) {
        return -1;
    }

    ToxWindow *self = new_conference_chat(conferencenum);

    for (int i = 0; i <= max_conference_index; ++i) {
        if (!conferences[i].active) {
            conferences[i].chatwin = add_window(m, self);
            conferences[i].active = true;
            conferences[i].num_peers = 0;
            conferences[i].type = type;
            conferences[i].start_time = get_unix_time();

            set_active_window_index(conferences[i].chatwin);
            set_window_title(self, title, title_length);

            if (i == max_conference_index) {
                ++max_conference_index;
            }

            return 0;
        }
    }

    kill_conference_window(self);

    return -1;
}

void free_conference(ToxWindow *self, uint32_t conferencenum)
{
    free_ptr_array((void **) conferences[conferencenum].name_list);
    free(conferences[conferencenum].peer_list);
    memset(&conferences[conferencenum], 0, sizeof(ConferenceChat));

    int i;

    for (i = max_conference_index; i > 0; --i) {
        if (conferences[i - 1].active) {
            break;
        }
    }

    max_conference_index = i;
    kill_conference_window(self);
}

static void delete_conference(ToxWindow *self, Tox *m, uint32_t conferencenum)
{
    tox_conference_delete(m, conferencenum, NULL);
    free_conference(self, conferencenum);
}

/* destroys and re-creates conference window with or without the peerlist */
void redraw_conference_win(ToxWindow *self)
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

static void conference_onConferenceMessage(ToxWindow *self, Tox *m, uint32_t conferencenum, uint32_t peernum,
        Tox_Message_Type type, const char *msg, size_t len)
{
    UNUSED_VAR(len);

    if (self->num != conferencenum) {
        return;
    }

    ChatContext *ctx = self->chatwin;

    char nick[TOX_MAX_NAME_LENGTH];
    get_conference_nick_truncate(m, nick, peernum, conferencenum);

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

static void conference_onConferenceTitleChange(ToxWindow *self, Tox *m, uint32_t conferencenum, uint32_t peernum,
        const char *title,
        size_t length)
{
    ChatContext *ctx = self->chatwin;

    if (self->num != conferencenum) {
        return;
    }

    set_window_title(self, title, length);

    char timefrmt[TIME_STR_SIZE];
    get_time_str(timefrmt, sizeof(timefrmt));

    /* don't announce title when we join the room */
    if (!timed_out(conferences[self->num].start_time, CONFERENCE_EVENT_WAIT)) {
        return;
    }

    char nick[TOX_MAX_NAME_LENGTH];
    get_conference_nick_truncate(m, nick, peernum, conferencenum);
    line_info_add(self, timefrmt, nick, NULL, NAME_CHANGE, 0, 0, " set the conference title to: %s", title);

    char tmp_event[MAX_STR_SIZE];
    snprintf(tmp_event, sizeof(tmp_event), "set title to %s", title);
    write_to_log(tmp_event, nick, ctx->log, true);
}

static void conference_update_name_list(uint32_t conferencenum)
{
    ConferenceChat *chat = &conferences[conferencenum];

    if (!chat->active) {
        return;
    }

    if (!chat->name_list) {
        fprintf(stderr, "WARNING: name_list is NULL\n");
        return;
    }

    size_t count = 0;

    for (uint32_t i = 0; i < chat->max_idx && count < chat->num_peers; ++i) {
        if (chat->peer_list[i].active) {
            memcpy(chat->name_list[count], chat->peer_list[i].name, chat->peer_list[i].name_length);
            chat->name_list[count][chat->peer_list[i].name_length] = 0;
            ++count;
        }
    }

    if (count != chat->num_peers) {
        fprintf(stderr, "WARNING: count != chat->num_peers\n");
    }

    qsort(chat->name_list, count, sizeof(char *), qsort_ptr_char_array_helper);
}

/* Reallocates conferencenum's peer list. Increase is true if the list needs to grow.
 *
 * Returns 0 on success.
 * Returns -1 on failure.
 */
static int realloc_peer_list(ConferenceChat *chat, uint32_t num_peers)
{
    if (!chat) {
        return -1;
    }

    if (num_peers == 0) {
        free(chat->peer_list);
        chat->peer_list = NULL;
        return 0;
    }

    struct ConferencePeer *tmp_list = realloc(chat->peer_list, num_peers * sizeof(struct ConferencePeer));

    if (!tmp_list) {
        return -1;
    }

    chat->peer_list = tmp_list;

    return 0;
}

static void update_peer_list(Tox *m, uint32_t conferencenum, uint32_t num_peers)
{
    ConferenceChat *chat = &conferences[conferencenum];

    if (!chat->active) {
        return;
    }

    realloc_peer_list(chat, num_peers);

    for (uint32_t i = 0; i < num_peers; ++i) {
        ConferencePeer *peer = &chat->peer_list[i];

        Tox_Err_Conference_Peer_Query err;
        size_t length = tox_conference_peer_get_name_size(m, conferencenum, i, &err);

        if (err != TOX_ERR_CONFERENCE_PEER_QUERY_OK || length >= TOX_MAX_NAME_LENGTH) {
            continue;
        }

        tox_conference_peer_get_name(m, conferencenum, i, (uint8_t *) peer->name, &err);
        peer->name[length] = 0;

        if (err != TOX_ERR_CONFERENCE_PEER_QUERY_OK) {
            continue;
        }

        peer->active = true;
        peer->name_length = length;
        peer->peernumber = i;
    }

    conference_update_name_list(conferencenum);
}

static void conference_onConferenceNameListChange(ToxWindow *self, Tox *m, uint32_t conferencenum)
{
    if (self->num != conferencenum) {
        return;
    }

    if (conferencenum > max_conference_index) {
        return;
    }

    ConferenceChat *chat = &conferences[conferencenum];

    if (!chat->active) {
        return;
    }

    if (chat->name_list) {
        free_ptr_array((void **) chat->name_list);
        chat->name_list = NULL;
        chat->num_peers = 0;
    }

    Tox_Err_Conference_Peer_Query err;

    uint32_t num_peers = tox_conference_peer_count(m, conferencenum, &err);

    if (err != TOX_ERR_CONFERENCE_PEER_QUERY_OK) {
        fprintf(stderr, "conference_onConferenceNameListChange() failed with error: %d\n", err);
        return;
    }

    chat->name_list = (char **) malloc_ptr_array(num_peers, TOX_MAX_NAME_LENGTH + 1);

    if (chat->name_list == NULL) {
        fprintf(stderr, "conference_onConferenceNameListChange(): Out of memory.\n");
        return;
    }

    chat->num_peers = num_peers;
    chat->max_idx = num_peers;
    update_peer_list(m, conferencenum, num_peers);
}

static void conference_onConferencePeerNameChange(ToxWindow *self, Tox *m, uint32_t conferencenum, uint32_t peernum,
        const char *name, size_t length)
{
    UNUSED_VAR(length);

    if (self->num != conferencenum) {
        return;
    }

    ConferenceChat *chat = &conferences[conferencenum];

    if (!chat->active) {
        return;
    }

    for (uint32_t i = 0; i < chat->max_idx; ++i) {
        ConferencePeer *peer = &chat->peer_list[i];

        // Test against default tox name to prevent nick change spam on initial join (TODO: this is disgusting)
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

    conference_onConferenceNameListChange(self, m, conferencenum);
}

static void send_conference_action(ToxWindow *self, ChatContext *ctx, Tox *m, char *action)
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

/*
 * Return true if input is recognized by handler
 */
static bool conference_onKey(ToxWindow *self, Tox *m, wint_t key, bool ltr)
{
    ChatContext *ctx = self->chatwin;

    int x, y, y2, x2;
    getyx(self->window, y, x);
    getmaxyx(self->window, y2, x2);

    UNUSED_VAR(y);

    if (x2 <= 0 || y2 <= 0) {
        return false;
    }

    if (self->help->active) {
        help_onKey(self, key);
        return true;
    }

    if (ctx->pastemode && key == L'\r') {
        key = L'\n';
    }

    if (ltr || key == L'\n') {    /* char is printable */
        input_new_char(self, key, x, x2);
        return true;
    }

    if (line_info_onKey(self, key)) {
        return true;
    }

    if (input_handle(self, key, x, x2)) {
        return true;
    }

    bool input_ret = false;

    if (key == L'\t') {  /* TAB key: auto-completes peer name or command */
        input_ret = true;

        if (ctx->len > 0) {
            int diff;

            /* TODO: make this not suck */
            if (ctx->line[0] != L'/' || wcscmp(ctx->line, L"/me") == 0) {
                diff = complete_line(self, (const char **) conferences[self->num].name_list, conferences[self->num].num_peers);
            } else if (wcsncmp(ctx->line, L"/avatar ", wcslen(L"/avatar ")) == 0) {
                diff = dir_match(self, m, ctx->line, L"/avatar");
            }

#ifdef PYTHON
            else if (wcsncmp(ctx->line, L"/run ", wcslen(L"/run ")) == 0) {
                diff = dir_match(self, m, ctx->line, L"/run");
            }

#endif
            else {
                diff = complete_line(self, conference_cmd_list, sizeof(conference_cmd_list) / sizeof(char *));
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
    } else if (key == T_KEY_C_DOWN) {    /* Scroll peerlist up and down one position */
        input_ret = true;
        const int L = y2 - CHATBOX_HEIGHT - SDBAR_OFST;

        if (conferences[self->num].side_pos < (int64_t) conferences[self->num].num_peers - L) {
            ++conferences[self->num].side_pos;
        }
    } else if (key == T_KEY_C_UP) {
        input_ret = true;

        if (conferences[self->num].side_pos > 0) {
            --conferences[self->num].side_pos;
        }
    } else if (key == L'\r') {
        input_ret = true;
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
                    delete_conference(self, m, self->num);
                    return true;
                } else if (strncmp(line, "/me ", strlen("/me ")) == 0) {
                    send_conference_action(self, ctx, m, line + strlen("/me "));
                } else {
                    execute(ctx->history, self, m, line, CONFERENCE_COMMAND_MODE);
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

    return input_ret;
}

static void conference_onDraw(ToxWindow *self, Tox *m)
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
        uint32_t num_peers = conferences[self->num].num_peers;
        pthread_mutex_unlock(&Winthread.lock);

        wmove(ctx->sidebar, 0, 1);
        wattron(ctx->sidebar, A_BOLD);
        wprintw(ctx->sidebar, "Peers: %"PRIu32"\n", num_peers);
        wattroff(ctx->sidebar, A_BOLD);

        mvwaddch(ctx->sidebar, 1, 0, ACS_LTEE);
        mvwhline(ctx->sidebar, 1, 1, ACS_HLINE, SIDEBAR_WIDTH - 1);

        int maxlines = y2 - SDBAR_OFST - CHATBOX_HEIGHT;

        for (uint32_t i = 0; i < num_peers && i < maxlines; ++i) {
            wmove(ctx->sidebar, i + 2, 1);

            pthread_mutex_lock(&Winthread.lock);
            uint32_t peer = i + conferences[self->num].side_pos;
            pthread_mutex_unlock(&Winthread.lock);

            /* truncate nick to fit in side panel without modifying list */
            char tmpnck[TOX_MAX_NAME_LENGTH];
            int maxlen = SIDEBAR_WIDTH - 2;

            pthread_mutex_lock(&Winthread.lock);
            memcpy(tmpnck, conferences[self->num].name_list[peer], maxlen);
            pthread_mutex_unlock(&Winthread.lock);

            tmpnck[maxlen] = '\0';

            wprintw(ctx->sidebar, "%s\n", tmpnck);
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

static void conference_onInit(ToxWindow *self, Tox *m)
{
    int x2, y2;
    getmaxyx(self->window, y2, x2);

    if (x2 <= 0 || y2 <= 0) {
        exit_toxic_err("failed in conference_onInit", FATALERR_CURSES);
    }

    ChatContext *ctx = self->chatwin;

    ctx->history = subwin(self->window, y2 - CHATBOX_HEIGHT + 1, x2 - SIDEBAR_WIDTH - 1, 0, 0);
    ctx->linewin = subwin(self->window, CHATBOX_HEIGHT, x2, y2 - CHATBOX_HEIGHT, 0);
    ctx->sidebar = subwin(self->window, y2 - CHATBOX_HEIGHT + 1, SIDEBAR_WIDTH, 0, x2 - SIDEBAR_WIDTH);

    ctx->hst = calloc(1, sizeof(struct history));
    ctx->log = calloc(1, sizeof(struct chatlog));

    if (ctx->log == NULL || ctx->hst == NULL) {
        exit_toxic_err("failed in conference_onInit", FATALERR_MEMORY);
    }

    line_info_init(ctx->hst);

    if (user_settings->autolog == AUTOLOG_ON) {
        char myid[TOX_ADDRESS_SIZE];
        tox_self_get_address(m, (uint8_t *) myid);

        if (log_enable(self->name, myid, NULL, ctx->log, LOG_CONFERENCE) == -1) {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Warning: Log failed to initialize.");
        }
    }

    execute(ctx->history, self, m, "/log", GLOBAL_COMMAND_MODE);

    scrollok(ctx->history, 0);
    wmove(self->window, y2 - CURS_Y_OFFSET, 0);
}

static ToxWindow *new_conference_chat(uint32_t conferencenum)
{
    ToxWindow *ret = calloc(1, sizeof(ToxWindow));

    if (ret == NULL) {
        exit_toxic_err("failed in new_conference_chat", FATALERR_MEMORY);
    }

    ret->type = WINDOW_TYPE_CONFERENCE;

    ret->onKey = &conference_onKey;
    ret->onDraw = &conference_onDraw;
    ret->onInit = &conference_onInit;
    ret->onConferenceMessage = &conference_onConferenceMessage;
    ret->onConferenceNameListChange = &conference_onConferenceNameListChange;
    ret->onConferencePeerNameChange = &conference_onConferencePeerNameChange;
    ret->onConferenceTitleChange = &conference_onConferenceTitleChange;

    snprintf(ret->name, sizeof(ret->name), "Conference %u", conferencenum);

    ChatContext *chatwin = calloc(1, sizeof(ChatContext));
    Help *help = calloc(1, sizeof(Help));

    if (chatwin == NULL || help == NULL) {
        exit_toxic_err("failed in new_conference_chat", FATALERR_MEMORY);
    }

    ret->chatwin = chatwin;
    ret->help = help;

    ret->num = conferencenum;
    ret->show_peerlist = true;
    ret->active_box = -1;

    return ret;
}

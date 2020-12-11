/*  windows.c
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

#include <ctype.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "avatars.h"
#include "chat.h"
#include "conference.h"
#include "file_transfers.h"
#include "friendlist.h"
#include "game_base.h"
#include "line_info.h"
#include "misc_tools.h"
#include "prompt.h"
#include "settings.h"
#include "toxic.h"
#include "windows.h"

extern char *DATA_FILE;
extern struct Winthread Winthread;

ToxWindow *windows[MAX_WINDOWS_NUM];
static uint8_t active_window_index;
static int num_active_windows;

extern ToxWindow *prompt;
extern struct user_settings *user_settings;

/* CALLBACKS START */
void on_friend_request(Tox *m, const uint8_t *public_key, const uint8_t *data, size_t length, void *userdata)
{
    UNUSED_VAR(userdata);

    char msg[MAX_STR_SIZE + 1];
    length = copy_tox_str(msg, sizeof(msg), (const char *) data, length);

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onFriendRequest != NULL) {
            windows[i]->onFriendRequest(windows[i], m, (const char *) public_key, msg, length);
        }
    }
}

void on_friend_connection_status(Tox *m, uint32_t friendnumber, Tox_Connection connection_status, void *userdata)
{
    UNUSED_VAR(userdata);

    on_avatar_friend_connection_status(m, friendnumber, connection_status);

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onConnectionChange != NULL) {
            windows[i]->onConnectionChange(windows[i], m, friendnumber, connection_status);
        }
    }
}

void on_friend_typing(Tox *m, uint32_t friendnumber, bool is_typing, void *userdata)
{
    UNUSED_VAR(userdata);

    if (user_settings->show_typing_other == SHOW_TYPING_OFF) {
        return;
    }

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onTypingChange != NULL) {
            windows[i]->onTypingChange(windows[i], m, friendnumber, is_typing);
        }
    }
}

void on_friend_message(Tox *m, uint32_t friendnumber, Tox_Message_Type type, const uint8_t *string, size_t length,
                       void *userdata)
{
    UNUSED_VAR(userdata);

    char msg[MAX_STR_SIZE + 1];
    length = copy_tox_str(msg, sizeof(msg), (const char *) string, length);

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onMessage != NULL) {
            windows[i]->onMessage(windows[i], m, friendnumber, type, msg, length);
        }
    }
}

void on_friend_name(Tox *m, uint32_t friendnumber, const uint8_t *string, size_t length, void *userdata)
{
    UNUSED_VAR(userdata);

    char nick[TOXIC_MAX_NAME_LENGTH + 1];
    length = copy_tox_str(nick, sizeof(nick), (const char *) string, length);
    filter_str(nick, length);

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onNickChange != NULL) {
            windows[i]->onNickChange(windows[i], m, friendnumber, nick, length);
        }
    }

    store_data(m, DATA_FILE);
}

void on_friend_status_message(Tox *m, uint32_t friendnumber, const uint8_t *string, size_t length, void *userdata)
{
    UNUSED_VAR(userdata);
    UNUSED_VAR(m);

    char msg[TOX_MAX_STATUS_MESSAGE_LENGTH + 1];
    length = copy_tox_str(msg, sizeof(msg), (const char *) string, length);
    filter_str(msg, length);

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onStatusMessageChange != NULL) {
            windows[i]->onStatusMessageChange(windows[i], friendnumber, msg, length);
        }
    }
}

void on_friend_status(Tox *m, uint32_t friendnumber, Tox_User_Status status, void *userdata)
{
    UNUSED_VAR(userdata);

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onStatusChange != NULL) {
            windows[i]->onStatusChange(windows[i], m, friendnumber, status);
        }
    }
}

void on_friend_added(Tox *m, uint32_t friendnumber, bool sort)
{
    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onFriendAdded != NULL) {
            windows[i]->onFriendAdded(windows[i], m, friendnumber, sort);
        }
    }

    store_data(m, DATA_FILE);
}

void on_conference_message(Tox *m, uint32_t conferencenumber, uint32_t peernumber, Tox_Message_Type type,
                           const uint8_t *message, size_t length, void *userdata)
{
    UNUSED_VAR(userdata);

    char msg[MAX_STR_SIZE + 1];
    length = copy_tox_str(msg, sizeof(msg), (const char *) message, length);

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onConferenceMessage != NULL) {
            windows[i]->onConferenceMessage(windows[i], m, conferencenumber, peernumber, type, msg, length);
        }
    }
}

void on_conference_invite(Tox *m, uint32_t friendnumber, Tox_Conference_Type type, const uint8_t *conference_pub_key,
                          size_t length, void *userdata)
{
    UNUSED_VAR(userdata);

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onConferenceInvite != NULL) {
            windows[i]->onConferenceInvite(windows[i], m, friendnumber, type, (char *) conference_pub_key, length);
        }
    }
}

void on_conference_peer_list_changed(Tox *m, uint32_t conferencenumber, void *userdata)
{
    UNUSED_VAR(userdata);

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onConferenceNameListChange != NULL) {
            windows[i]->onConferenceNameListChange(windows[i], m, conferencenumber);
        }
    }
}

void on_conference_peer_name(Tox *m, uint32_t conferencenumber, uint32_t peernumber, const uint8_t *name,
                             size_t length, void *userdata)
{
    UNUSED_VAR(userdata);

    char nick[TOXIC_MAX_NAME_LENGTH + 1];
    length = copy_tox_str(nick, sizeof(nick), (const char *) name, length);
    filter_str(nick, length);

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onConferencePeerNameChange != NULL) {
            windows[i]->onConferencePeerNameChange(windows[i], m, conferencenumber, peernumber, nick, length);
        }
    }
}

void on_conference_title(Tox *m, uint32_t conferencenumber, uint32_t peernumber, const uint8_t *title, size_t length,
                         void *userdata)
{
    UNUSED_VAR(userdata);

    char data[MAX_STR_SIZE + 1];
    length = copy_tox_str(data, sizeof(data), (const char *) title, length);

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onConferenceTitleChange != NULL) {
            windows[i]->onConferenceTitleChange(windows[i], m, conferencenumber, peernumber, data, length);
        }
    }
}

void on_file_chunk_request(Tox *m, uint32_t friendnumber, uint32_t filenumber, uint64_t position,
                           size_t length, void *userdata)
{
    UNUSED_VAR(userdata);

    struct FileTransfer *ft = get_file_transfer_struct(friendnumber, filenumber);

    if (!ft) {
        return;
    }

    if (ft->file_type == TOX_FILE_KIND_AVATAR) {
        on_avatar_chunk_request(m, ft, position, length);
        return;
    }

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onFileChunkRequest != NULL) {
            windows[i]->onFileChunkRequest(windows[i], m, friendnumber, filenumber, position, length);
        }
    }
}

void on_file_recv_chunk(Tox *m, uint32_t friendnumber, uint32_t filenumber, uint64_t position,
                        const uint8_t *data, size_t length, void *userdata)
{
    UNUSED_VAR(userdata);

    struct FileTransfer *ft = get_file_transfer_struct(friendnumber, filenumber);

    if (!ft) {
        return;
    }

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onFileRecvChunk != NULL) {
            windows[i]->onFileRecvChunk(windows[i], m, friendnumber, filenumber, position, (char *) data, length);
        }
    }
}

void on_file_recv_control(Tox *m, uint32_t friendnumber, uint32_t filenumber, Tox_File_Control control,
                          void *userdata)
{
    UNUSED_VAR(userdata);

    struct FileTransfer *ft = get_file_transfer_struct(friendnumber, filenumber);

    if (!ft) {
        return;
    }

    if (ft->file_type == TOX_FILE_KIND_AVATAR) {
        on_avatar_file_control(m, ft, control);
        return;
    }

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onFileControl != NULL) {
            windows[i]->onFileControl(windows[i], m, friendnumber, filenumber, control);
        }
    }
}

void on_file_recv(Tox *m, uint32_t friendnumber, uint32_t filenumber, uint32_t kind, uint64_t file_size,
                  const uint8_t *filename, size_t filename_length, void *userdata)
{
    UNUSED_VAR(userdata);

    /* We don't care about receiving avatars */
    if (kind != TOX_FILE_KIND_DATA) {
        tox_file_control(m, friendnumber, filenumber, TOX_FILE_CONTROL_CANCEL, NULL);
        return;
    }

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onFileRecv != NULL) {
            windows[i]->onFileRecv(windows[i], m, friendnumber, filenumber, file_size, (char *) filename,
                                   filename_length);
        }
    }
}

void on_friend_read_receipt(Tox *m, uint32_t friendnumber, uint32_t receipt, void *userdata)
{
    UNUSED_VAR(userdata);

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onReadReceipt != NULL) {
            windows[i]->onReadReceipt(windows[i], m, friendnumber, receipt);
        }
    }
}
/* CALLBACKS END */

int add_window(Tox *m, ToxWindow *w)
{
    if (LINES < 2) {
        return -1;
    }

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; i++) {
        if (windows[i] != NULL) {
            continue;
        }

        w->index = i;
        w->window = newwin(LINES, COLS, 0, 0);

        if (w->window == NULL) {
            return -1;
        }

#ifdef URXVT_FIX
        /* Fixes text color problem on some terminals. */
        wbkgd(w->window, COLOR_PAIR(6));
#endif
        windows[i] = w;

        if (w->onInit) {
            w->onInit(w, m);
        }

        ++num_active_windows;

        return i;
    }

    return -1;
}

void set_active_window_index(uint8_t index)
{
    if (index < MAX_WINDOWS_NUM) {
        active_window_index = index;
    }
}

/* Displays the next window if `ch` is equal to the next window key binding.
 * Otherwise displays the previous window.
 */
void set_next_window(int ch)
{
    if (ch == user_settings->key_next_tab) {
        for (uint8_t i = active_window_index + 1; i < MAX_WINDOWS_NUM; ++i) {
            if (windows[i] != NULL) {
                set_active_window_index(i);
                return;
            }
        }
    } else {
        uint8_t start = active_window_index == 0 ? MAX_WINDOWS_NUM - 1 : active_window_index - 1;

        for (uint8_t i = start; i > 0; --i) {
            if (windows[i] != NULL) {
                set_active_window_index(i);
                return;
            }
        }
    }

    set_active_window_index(0);
}

/* Deletes window w and cleans up */
void del_window(ToxWindow *w)
{
    uint8_t idx = w->index;
    delwin(w->window_bar);
    delwin(w->window);
    free(w);
    windows[idx] = NULL;

    clear();
    refresh();

    if (num_active_windows > 0) {
        set_next_window(-1);
        --num_active_windows;
    }
}

ToxWindow *init_windows(Tox *m)
{
    if (COLS <= CHATBOX_HEIGHT + WINDOW_BAR_HEIGHT) {
        exit_toxic_err("add_window() for prompt failed in init_windows", FATALERR_WININIT);
    }

    prompt = new_prompt();

    int n_prompt = add_window(m, prompt);

    if (n_prompt < 0) {
        exit_toxic_err("add_window() for prompt failed in init_windows", FATALERR_WININIT);
    }

    if (add_window(m, new_friendlist()) == -1) {
        exit_toxic_err("add_window() for friendlist failed in init_windows", FATALERR_WININIT);
    }

    set_active_window_index(n_prompt);

    return prompt;
}

void on_window_resize(void)
{
    endwin();
    refresh();
    clear();

    int x2;
    int y2;

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        ToxWindow *w = windows[i];

        if (w == NULL) {
            continue;
        }

        if (w->type == WINDOW_TYPE_FRIEND_LIST)  {
            delwin(w->window_bar);
            delwin(w->window);
            w->window = newwin(LINES, COLS, 0, 0);
            w->window_bar = subwin(w->window, WINDOW_BAR_HEIGHT, COLS, LINES - 2, 0);
            continue;
        }

        if (w->type == WINDOW_TYPE_GAME) {
            delwin(w->window_bar);
            delwin(w->window);
            delwin(w->game->window);
            w->window = newwin(LINES, COLS, 0, 0);

            getmaxyx(w->window, y2, x2);

            w->window_bar = subwin(w->window, WINDOW_BAR_HEIGHT, COLS, LINES - 2, 0);
            w->game->window = subwin(w->window, y2 - CHATBOX_HEIGHT - WINDOW_BAR_HEIGHT, x2, 0, 0);
            continue;
        }

        if (w->help->active) {
            wclear(w->help->win);
        }

        if (w->type == WINDOW_TYPE_CONFERENCE) {
            delwin(w->chatwin->sidebar);
            w->chatwin->sidebar = NULL;
        } else {
            delwin(w->stb->topline);
        }

        delwin(w->chatwin->linewin);
        delwin(w->chatwin->history);
        delwin(w->window_bar);
        delwin(w->window);

        w->window = newwin(LINES, COLS, 0, 0);

        getmaxyx(w->window, y2, x2);

        if (y2 <= 0 || x2 <= 0) {
            fprintf(stderr, "Failed to resize window: max_x: %d, max_y: %d\n", x2, y2);
            delwin(w->window);
            return;
        }

        if (w->show_peerlist) {
            w->chatwin->history = subwin(w->window, y2 - CHATBOX_HEIGHT - WINDOW_BAR_HEIGHT, x2 - SIDEBAR_WIDTH - 1, 0, 0);
            w->chatwin->sidebar = subwin(w->window, y2 - CHATBOX_HEIGHT - WINDOW_BAR_HEIGHT, SIDEBAR_WIDTH, 0, x2 - SIDEBAR_WIDTH);
        } else {
            w->chatwin->history =  subwin(w->window, y2 - CHATBOX_HEIGHT - WINDOW_BAR_HEIGHT, x2, 0, 0);

            if (w->type != WINDOW_TYPE_CONFERENCE) {
                w->stb->topline = subwin(w->window, TOP_BAR_HEIGHT, x2, 0, 0);
            }
        }

        w->window_bar = subwin(w->window, WINDOW_BAR_HEIGHT, x2, y2 - (CHATBOX_HEIGHT + WINDOW_BAR_HEIGHT), 0);
        w->chatwin->linewin = subwin(w->window, CHATBOX_HEIGHT, x2, y2 - WINDOW_BAR_HEIGHT, 0);

#ifdef AUDIO

        if (w->chatwin->infobox.active) {
            delwin(w->chatwin->infobox.win);
            w->chatwin->infobox.win = newwin(INFOBOX_HEIGHT, INFOBOX_WIDTH + 1, 1, x2 - INFOBOX_WIDTH);
        }

#endif /* AUDIO */

        scrollok(w->chatwin->history, 0);
        wmove(w->window, y2 - CURS_Y_OFFSET, 0);
    }
}

static void draw_window_tab(WINDOW *win, ToxWindow *toxwin, bool active_window)
{
    pthread_mutex_lock(&Winthread.lock);

    bool has_alert = toxwin->alert != WINDOW_ALERT_NONE;
    unsigned int pending_messages = toxwin->pending_messages;

    pthread_mutex_unlock(&Winthread.lock);

    WINDOW_TYPE type = toxwin->type;

    if (active_window) {
        wattron(win, A_BOLD | COLOR_PAIR(BAR_ACCENT));
        wprintw(win, " [");
        wattroff(win, COLOR_PAIR(BAR_ACCENT));
        wattron(win, COLOR_PAIR(BAR_TEXT));
    } else {
        if (has_alert) {
            wattron(win, COLOR_PAIR(BAR_ACCENT));
            wprintw(win, " [");
            wattroff(win, COLOR_PAIR(BAR_ACCENT));
            wattron(win, A_BOLD | COLOR_PAIR(toxwin->alert));
        } else {
            wattron(win, COLOR_PAIR(BAR_ACCENT));
            wprintw(win, " [");
            wattroff(win, COLOR_PAIR(BAR_ACCENT));
            wattron(win, COLOR_PAIR(BAR_TEXT));
        }
    }

    if (active_window || (type == WINDOW_TYPE_PROMPT || type == WINDOW_TYPE_FRIEND_LIST)) {
        wprintw(win, "%s", toxwin->name);
    } else {
        if (pending_messages > 0) {
            wprintw(win, "%u", pending_messages);
        } else {
            wprintw(win, "-");
        }
    }

    if (active_window) {
        wattroff(win, COLOR_PAIR(BAR_TEXT));
        wattron(win, COLOR_PAIR(BAR_ACCENT));
        wprintw(win, "]");
        wattroff(win, A_BOLD | COLOR_PAIR(BAR_ACCENT));
    } else {
        if (has_alert) {
            wattroff(win, A_BOLD | COLOR_PAIR(toxwin->alert));
            wattron(win, COLOR_PAIR(BAR_ACCENT));
            wprintw(win, "]");
            wattroff(win, COLOR_PAIR(BAR_ACCENT));
        } else {
            wattroff(win, COLOR_PAIR(BAR_TEXT));
            wattron(win, COLOR_PAIR(BAR_ACCENT));
            wprintw(win, "]");
            wattroff(win, COLOR_PAIR(BAR_ACCENT));
        }
    }
}

void draw_window_bar(ToxWindow *self)
{
    WINDOW *win = self->window_bar;
    wclear(win);

    if (self->scroll_pause) {
        wattron(win, A_BLINK | A_BOLD | COLOR_PAIR(BAR_NOTIFY));
        wprintw(win, "^");
        wattroff(win, A_BLINK | A_BOLD | COLOR_PAIR(BAR_NOTIFY));
    } else {
        wattron(win, COLOR_PAIR(BAR_TEXT));
        wprintw(win, " ");
        wattroff(win, COLOR_PAIR(BAR_TEXT));
    }

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] == NULL) {
            continue;
        }

        bool active_window = i == active_window_index;
        draw_window_tab(win, windows[i], active_window);
    }

    int cur_x;
    int cur_y;
    UNUSED_VAR(cur_y);

    getyx(win, cur_y, cur_x);

    wattron(win, COLOR_PAIR(BAR_TEXT));
    mvwhline(win, 0, cur_x, ' ', COLS - cur_x);
    wattroff(win, COLOR_PAIR(BAR_TEXT));
}

/*
 * Gets current char from stdscr and puts it in ch.
 *
 * Return 1 if char is printable.
 * Return 0 if char is not printable.
 * Return -1 on error.
 */
static int get_current_char(wint_t *ch)
{
    wint_t tmpchar = 0;
    bool is_printable = false;

#ifdef HAVE_WIDECHAR
    int status = wget_wch(stdscr, &tmpchar);

    if (status == ERR) {
        return -1;
    }

    if (status == OK) {
        is_printable = iswprint(tmpchar);
    }

#else
    tmpchar = getch();

    if (tmpchar == ERR) {
        return -1;
    }

    is_printable = isprint(tmpchar);
#endif /* HAVE_WIDECHAR */

    *ch = tmpchar;

    return (int) is_printable;
}

static struct key_sequence_codes {
    wchar_t *code;
    wint_t   key;
} Keys[] = {
    { L"[1;5A", T_KEY_C_UP    },
    { L"[1;5B", T_KEY_C_DOWN  },
    { L"[1;5C", T_KEY_C_RIGHT },
    { L"[1;5D", T_KEY_C_LEFT  },
    { NULL, 0 }
};

/*
 * Return key code corresponding to character sequence queued in stdscr.
 * Return -1 if sequence is unknown.
 */
#define MAX_SEQUENCE_SIZE 5
static wint_t get_input_sequence_code(void)
{
    wchar_t code[MAX_SEQUENCE_SIZE + 1];

    size_t length = 0;
    wint_t ch = 0;

    for (size_t i = 0; i < MAX_SEQUENCE_SIZE; ++i) {
        int res = get_current_char(&ch);

        if (res < 0) {
            break;
        }

        ++length;
        code[i] = (wchar_t) ch;
    }

    if (length == 0) {
        return -1;
    }

    code[length] = L'\0';

    for (size_t i = 0; Keys[i].key != 0; ++i) {
        if (wcscmp(code, Keys[i].code) == 0) {
            return Keys[i].key;
        }
    }

    return -1;
}

void draw_active_window(Tox *m)
{
    ToxWindow *a = windows[active_window_index];

    if (a == NULL) {
        return;
    }

    pthread_mutex_lock(&Winthread.lock);
    a->alert = WINDOW_ALERT_NONE;
    a->pending_messages = 0;
    pthread_mutex_unlock(&Winthread.lock);

    touchwin(a->window);
    a->onDraw(a, m);
    wrefresh(a->window);

    if (a->type == WINDOW_TYPE_GAME) {
        int ch = getch();

        if (ch == ERR) {
            return;
        }

        if (ch == user_settings->key_next_tab || ch == user_settings->key_prev_tab) {
            set_next_window(ch);
            ch = KEY_F(2);
        }

        a->onKey(a, m, ch, false);

        return;
    }

    wint_t ch = 0;
    int printable = get_current_char(&ch);

    if (printable < 0) {
        return;
    }

    if (printable == 0 && (ch == user_settings->key_next_tab || ch == user_settings->key_prev_tab)) {
        set_next_window((int) ch);
        return;
    } else if ((printable == 0) && (a->type != WINDOW_TYPE_FRIEND_LIST)) {
        pthread_mutex_lock(&Winthread.lock);
        bool input_ret = a->onKey(a, m, ch, (bool) printable);
        pthread_mutex_unlock(&Winthread.lock);

        if (input_ret) {
            return;
        }

        // if an unprintable key code is unrecognized by input handler we attempt to manually decode char sequence
        wint_t tmp = get_input_sequence_code();

        if (tmp != (wint_t) -1) {
            ch = tmp;
        }
    }

    pthread_mutex_lock(&Winthread.lock);
    a->onKey(a, m, ch, (bool) printable);
    pthread_mutex_unlock(&Winthread.lock);
}

/* Refresh inactive windows to prevent scrolling bugs.
 * Call at least once per second.
 */
void refresh_inactive_windows(void)
{
    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        ToxWindow *toxwin = windows[i];

        if (toxwin == NULL) {
            continue;
        }

        if ((i != active_window_index) && (toxwin->type != WINDOW_TYPE_FRIEND_LIST)) {
            pthread_mutex_lock(&Winthread.lock);
            line_info_print(toxwin);
            pthread_mutex_unlock(&Winthread.lock);
        }
    }
}

/* Returns a pointer to the ToxWindow in the ith index.
 * Returns NULL if no ToxWindow exists.
 */
ToxWindow *get_window_ptr(size_t index)
{
    if (index >= MAX_WINDOWS_NUM) {
        return NULL;
    }

    return windows[index];
}

/* Returns a pointer to the currently active ToxWindow. */
ToxWindow *get_active_window(void)
{
    return windows[active_window_index];
}

void force_refresh(WINDOW *w)
{
    wclear(w);
    endwin();
    refresh();
}

int get_num_active_windows(void)
{
    return num_active_windows;
}

/* destroys all chat and conference windows (should only be called on shutdown) */
void kill_all_windows(Tox *m)
{
    for (uint8_t i = 2; i < MAX_WINDOWS_NUM; ++i) {
        ToxWindow *w = windows[i];

        if (w == NULL) {
            continue;
        }

        if (w->type == WINDOW_TYPE_CHAT) {
            kill_chat_window(w, m);
        } else if (w->type == WINDOW_TYPE_CONFERENCE) {
            free_conference(w, w->num);
        }  else if (w->type == WINDOW_TYPE_GAME) {
            game_kill(w);
        }
    }

    /* TODO: use enum instead of magic indices */
    kill_friendlist(windows[1]);
    kill_prompt_window(windows[0]);
}

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

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <ctype.h>

#include "friendlist.h"
#include "prompt.h"
#include "toxic.h"
#include "windows.h"
#include "groupchat.h"
#include "chat.h"
#include "line_info.h"
#include "misc_tools.h"
#include "avatars.h"
#include "settings.h"
#include "file_transfers.h"

extern char *DATA_FILE;
extern struct Winthread Winthread;
static ToxWindow windows[MAX_WINDOWS_NUM];
static ToxWindow *active_window;

extern ToxWindow *prompt;
extern struct user_settings *user_settings;

static int num_active_windows;

/* CALLBACKS START */
void on_friend_request(Tox *m, const uint8_t *public_key, const uint8_t *data, size_t length, void *userdata)
{
    char msg[MAX_STR_SIZE + 1];
    length = copy_tox_str(msg, sizeof(msg), (const char *) data, length);

    size_t i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].onFriendRequest != NULL) {
            windows[i].onFriendRequest(&windows[i], m, (const char *) public_key, msg, length);
        }
    }
}

void on_friend_connection_status(Tox *m, uint32_t friendnumber, TOX_CONNECTION connection_status, void *userdata)
{
    size_t i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].onConnectionChange != NULL) {
            windows[i].onConnectionChange(&windows[i], m, friendnumber, connection_status);
        }
    }
}

void on_friend_typing(Tox *m, uint32_t friendnumber, bool is_typing, void *userdata)
{
    if (user_settings->show_typing_other == SHOW_TYPING_OFF) {
        return;
    }

    size_t i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].onTypingChange != NULL) {
            windows[i].onTypingChange(&windows[i], m, friendnumber, is_typing);
        }
    }
}

void on_friend_message(Tox *m, uint32_t friendnumber, TOX_MESSAGE_TYPE type, const uint8_t *string, size_t length,
                       void *userdata)
{
    char msg[MAX_STR_SIZE + 1];
    length = copy_tox_str(msg, sizeof(msg), (const char *) string, length);

    size_t i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].onMessage != NULL) {
            windows[i].onMessage(&windows[i], m, friendnumber, type, msg, length);
        }
    }
}

void on_friend_name(Tox *m, uint32_t friendnumber, const uint8_t *string, size_t length, void *userdata)
{
    char nick[TOXIC_MAX_NAME_LENGTH + 1];
    length = copy_tox_str(nick, sizeof(nick), (const char *) string, length);
    filter_str(nick, length);

    size_t i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].onNickChange != NULL) {
            windows[i].onNickChange(&windows[i], m, friendnumber, nick, length);
        }
    }

    store_data(m, DATA_FILE);
}

void on_friend_status_message(Tox *m, uint32_t friendnumber, const uint8_t *string, size_t length, void *userdata)
{
    char msg[TOX_MAX_STATUS_MESSAGE_LENGTH + 1];
    length = copy_tox_str(msg, sizeof(msg), (const char *) string, length);
    filter_str(msg, length);

    size_t i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].onStatusMessageChange != NULL) {
            windows[i].onStatusMessageChange(&windows[i], friendnumber, msg, length);
        }
    }
}

void on_friend_status(Tox *m, uint32_t friendnumber, TOX_USER_STATUS status, void *userdata)
{
    size_t i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].onStatusChange != NULL) {
            windows[i].onStatusChange(&windows[i], m, friendnumber, status);
        }
    }
}

void on_friend_added(Tox *m, uint32_t friendnumber, bool sort)
{
    size_t i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].onFriendAdded != NULL) {
            windows[i].onFriendAdded(&windows[i], m, friendnumber, sort);
        }
    }

    store_data(m, DATA_FILE);
}

void on_conference_message(Tox *m, uint32_t groupnumber, uint32_t peernumber, TOX_MESSAGE_TYPE type,
                           const uint8_t *message, size_t length, void *userdata)
{
    char msg[MAX_STR_SIZE + 1];
    length = copy_tox_str(msg, sizeof(msg), (const char *) message, length);

    size_t i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].onGroupMessage != NULL) {
            windows[i].onGroupMessage(&windows[i], m, groupnumber, peernumber, type, msg, length);
        }
    }
}

void on_conference_invite(Tox *m, uint32_t friendnumber, TOX_CONFERENCE_TYPE type, const uint8_t *group_pub_key,
                          size_t length, void *userdata)
{
    size_t i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].onGroupInvite != NULL) {
            windows[i].onGroupInvite(&windows[i], m, friendnumber, type, (char *) group_pub_key, length);
        }
    }
}

void on_conference_peer_list_changed(Tox *m, uint32_t groupnumber, void *userdata)
{
    size_t i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].onGroupNameListChange != NULL) {
            windows[i].onGroupNameListChange(&windows[i], m, groupnumber);
        }
    }
}

void on_conference_peer_name(Tox *m, uint32_t groupnumber, uint32_t peernumber, const uint8_t *name,
                             size_t length, void *userdata)
{
    char nick[TOXIC_MAX_NAME_LENGTH + 1];
    length = copy_tox_str(nick, sizeof(nick), (const char *) name, length);
    filter_str(nick, length);

    size_t i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].onGroupPeerNameChange != NULL) {
            windows[i].onGroupPeerNameChange(&windows[i], m, groupnumber, peernumber, nick, length);
        }
    }
}

void on_conference_title(Tox *m, uint32_t groupnumber, uint32_t peernumber, const uint8_t *title, size_t length,
                         void *userdata)
{
    char data[MAX_STR_SIZE + 1];
    length = copy_tox_str(data, sizeof(data), (const char *) title, length);

    size_t i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].onGroupTitleChange != NULL) {
            windows[i].onGroupTitleChange(&windows[i], m, groupnumber, peernumber, data, length);
        }
    }
}

void on_file_chunk_request(Tox *m, uint32_t friendnumber, uint32_t filenumber, uint64_t position,
                           size_t length, void *userdata)
{
    struct FileTransfer *ft = get_file_transfer_struct(friendnumber, filenumber);

    if (!ft) {
        return;
    }

    if (ft->file_type == TOX_FILE_KIND_AVATAR) {
        on_avatar_chunk_request(m, ft, position, length);
        return;
    }

    size_t i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].onFileChunkRequest != NULL) {
            windows[i].onFileChunkRequest(&windows[i], m, friendnumber, filenumber, position, length);
        }
    }
}

void on_file_recv_chunk(Tox *m, uint32_t friendnumber, uint32_t filenumber, uint64_t position,
                        const uint8_t *data, size_t length, void *user_data)
{
    struct FileTransfer *ft = get_file_transfer_struct(friendnumber, filenumber);

    if (!ft) {
        return;
    }

    size_t i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].onFileRecvChunk != NULL) {
            windows[i].onFileRecvChunk(&windows[i], m, friendnumber, filenumber, position, (char *) data, length);
        }
    }
}

void on_file_recv_control(Tox *m, uint32_t friendnumber, uint32_t filenumber, TOX_FILE_CONTROL control,
                          void *userdata)
{
    struct FileTransfer *ft = get_file_transfer_struct(friendnumber, filenumber);

    if (!ft) {
        return;
    }

    if (ft->file_type == TOX_FILE_KIND_AVATAR) {
        on_avatar_file_control(m, ft, control);
        return;
    }

    size_t i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].onFileControl != NULL) {
            windows[i].onFileControl(&windows[i], m, friendnumber, filenumber, control);
        }
    }
}

void on_file_recv(Tox *m, uint32_t friendnumber, uint32_t filenumber, uint32_t kind, uint64_t file_size,
                  const uint8_t *filename, size_t filename_length, void *userdata)
{
    /* We don't care about receiving avatars */
    if (kind != TOX_FILE_KIND_DATA) {
        tox_file_control(m, friendnumber, filenumber, TOX_FILE_CONTROL_CANCEL, NULL);
        return;
    }

    size_t i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].onFileRecv != NULL) {
            windows[i].onFileRecv(&windows[i], m, friendnumber, filenumber, file_size, (char *) filename,
                                  filename_length);
        }
    }
}

void on_friend_read_receipt(Tox *m, uint32_t friendnumber, uint32_t receipt, void *userdata)
{
    size_t i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].onReadReceipt != NULL) {
            windows[i].onReadReceipt(&windows[i], m, friendnumber, receipt);
        }
    }
}
/* CALLBACKS END */

int add_window(Tox *m, ToxWindow w)
{
    if (LINES < 2) {
        return -1;
    }

    size_t i;

    for (i = 0; i < MAX_WINDOWS_NUM; i++) {
        if (windows[i].active) {
            continue;
        }

        w.window = newwin(LINES - 2, COLS, 0, 0);

        if (w.window == NULL) {
            return -1;
        }

#ifdef URXVT_FIX
        /* Fixes text color problem on some terminals. */
        wbkgd(w.window, COLOR_PAIR(6));
#endif
        windows[i] = w;

        if (w.onInit) {
            w.onInit(&w, m);
        }

        ++num_active_windows;

        return i;
    }

    return -1;
}

void set_active_window(int index)
{
    if (index < 0 || index >= MAX_WINDOWS_NUM) {
        return;
    }

    active_window = windows + index;
}

/* Shows next window when tab or back-tab is pressed */
void set_next_window(int ch)
{
    ToxWindow *end = windows + MAX_WINDOWS_NUM - 1;
    ToxWindow *inf = active_window;

    while (true) {
        if (ch == user_settings->key_next_tab) {
            if (++active_window > end) {
                active_window = windows;
            }
        } else if (--active_window < windows) {
            active_window = end;
        }

        if (active_window->window) {
            return;
        }

        if (active_window == inf) {  /* infinite loop check */
            exit_toxic_err("failed in set_next_window", FATALERR_INFLOOP);
        }
    }
}

/* Deletes window w and cleans up */
void del_window(ToxWindow *w)
{
    set_active_window(0);    /* Go to prompt screen */

    delwin(w->window);
    memset(w, 0, sizeof(ToxWindow));

    clear();
    refresh();
    --num_active_windows;
}

ToxWindow *init_windows(Tox *m)
{
    int n_prompt = add_window(m, new_prompt());

    if (n_prompt == -1 || add_window(m, new_friendlist()) == -1) {
        exit_toxic_err("failed in init_windows", FATALERR_WININIT);
    }

    prompt = &windows[n_prompt];
    active_window = prompt;

    return prompt;
}

void on_window_resize(void)
{
    endwin();
    refresh();
    clear();

    /* equivalent to LINES and COLS */
    int x2, y2;
    getmaxyx(stdscr, y2, x2);
    y2 -= 2;

    if (y2 <= 0 || x2 <= 0) {
        return;
    }

    size_t i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (!windows[i].active) {
            continue;
        }

        ToxWindow *w = &windows[i];

        if (windows[i].is_friendlist)  {
            delwin(w->window);
            w->window = newwin(y2, x2, 0, 0);
            continue;
        }

        if (w->help->active) {
            wclear(w->help->win);
        }

        if (w->is_groupchat) {
            delwin(w->chatwin->sidebar);
            w->chatwin->sidebar = NULL;
        } else {
            delwin(w->stb->topline);
        }

        delwin(w->chatwin->linewin);
        delwin(w->chatwin->history);
        delwin(w->window);

        w->window = newwin(y2, x2, 0, 0);
        w->chatwin->linewin = subwin(w->window, CHATBOX_HEIGHT, x2, y2 - CHATBOX_HEIGHT, 0);

        if (w->show_peerlist) {
            w->chatwin->history = subwin(w->window, y2 - CHATBOX_HEIGHT + 1, x2 - SIDEBAR_WIDTH - 1, 0, 0);
            w->chatwin->sidebar = subwin(w->window, y2 - CHATBOX_HEIGHT + 1, SIDEBAR_WIDTH, 0, x2 - SIDEBAR_WIDTH);
        } else {
            w->chatwin->history = subwin(w->window, y2 - CHATBOX_HEIGHT + 1, x2, 0, 0);

            if (!w->is_groupchat) {
                w->stb->topline = subwin(w->window, 2, x2, 0, 0);
            }
        }

#ifdef AUDIO

        if (w->chatwin->infobox.active) {
            delwin(w->chatwin->infobox.win);
            w->chatwin->infobox.win = newwin(INFOBOX_HEIGHT, INFOBOX_WIDTH + 1, 1, x2 - INFOBOX_WIDTH);
        }

#endif   /* AUDIO */

        scrollok(w->chatwin->history, 0);
    }
}

static void draw_window_tab(ToxWindow *toxwin)
{
    pthread_mutex_lock(&Winthread.lock);

    if (toxwin->alert != WINDOW_ALERT_NONE) {
        attron(COLOR_PAIR(toxwin->alert));
    }

    pthread_mutex_unlock(&Winthread.lock);

    clrtoeol();
    printw(" [%s]", toxwin->name);

    pthread_mutex_lock(&Winthread.lock);

    if (toxwin->alert != WINDOW_ALERT_NONE) {
        attroff(COLOR_PAIR(toxwin->alert));
    }

    pthread_mutex_unlock(&Winthread.lock);
}

static void draw_bar(void)
{
    int y, x;

    // save current cursor position
    getyx(active_window->window, y, x);

    attron(COLOR_PAIR(BLUE));
    mvhline(LINES - 2, 0, '_', COLS);
    attroff(COLOR_PAIR(BLUE));

    move(LINES - 1, 0);

    size_t i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (!windows[i].active) {
            continue;
        }

        if (windows + i == active_window) {

#ifdef URXVT_FIX
            attron(A_BOLD | COLOR_PAIR(GREEN));
        } else {
#endif

            attron(A_BOLD);
        }

        draw_window_tab(&windows[i]);

        if (windows + i == active_window) {

#ifdef URXVT_FIX
            attroff(A_BOLD | COLOR_PAIR(GREEN));
        } else {
#endif

            attroff(A_BOLD);
        }
    }

    // restore cursor position after drawing
    move(y, x);

    refresh();
}

void draw_active_window(Tox *m)
{
    ToxWindow *a = active_window;

    pthread_mutex_lock(&Winthread.lock);
    a->alert = WINDOW_ALERT_NONE;
    pthread_mutex_unlock(&Winthread.lock);

    wint_t ch = 0;

    draw_bar();

    touchwin(a->window);
    a->onDraw(a, m);
    wrefresh(a->window);

    /* Handle input */
    bool ltr;
#ifdef HAVE_WIDECHAR
    int status = wget_wch(stdscr, &ch);

    if (status == ERR) {
        return;
    }

    if (status == OK) {
        ltr = iswprint(ch);
    } else { /* if (status == KEY_CODE_YES) */
        ltr = false;
    }

#else
    ch = getch();

    if (ch == ERR) {
        return;
    }

    /* TODO verify if this works */
    ltr = isprint(ch);
#endif /* HAVE_WIDECHAR */

    if (!ltr && (ch == user_settings->key_next_tab || ch == user_settings->key_prev_tab)) {
        set_next_window((int) ch);
    } else {
        pthread_mutex_lock(&Winthread.lock);
        a->onKey(a, m, ch, ltr);
        pthread_mutex_unlock(&Winthread.lock);
    }
}

/* refresh inactive windows to prevent scrolling bugs.
   call at least once per second */
void refresh_inactive_windows(void)
{
    size_t i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        ToxWindow *a = &windows[i];

        if (a->active && a != active_window && !a->is_friendlist) {
            pthread_mutex_lock(&Winthread.lock);
            line_info_print(a);
            pthread_mutex_unlock(&Winthread.lock);
        }
    }
}

/* returns a pointer to the ToxWindow in the ith index. Returns NULL if no ToxWindow exists */
ToxWindow *get_window_ptr(int i)
{
    ToxWindow *toxwin = NULL;

    if (i >= 0 && i < MAX_WINDOWS_NUM && windows[i].active) {
        toxwin = &windows[i];
    }

    return toxwin;
}

/* returns a pointer to the currently open ToxWindow. */
ToxWindow *get_active_window(void)
{
    return active_window;
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

/* destroys all chat and groupchat windows (should only be called on shutdown) */
void kill_all_windows(Tox *m)
{
    size_t i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].is_chat) {
            kill_chat_window(&windows[i], m);
        } else if (windows[i].is_groupchat) {
            free_groupchat(&windows[i], m, windows[i].num);
        }
    }

    kill_prompt_window(prompt);
    kill_friendlist();
}

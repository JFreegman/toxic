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

#include "settings.h"
extern char *DATA_FILE;
extern struct Winthread Winthread;
static ToxWindow windows[MAX_WINDOWS_NUM];
static ToxWindow *active_window;

extern ToxWindow *prompt;
extern struct user_settings *user_settings;

static int num_active_windows;

/* CALLBACKS START */
void on_request(Tox *m, const uint8_t *public_key, const uint8_t *data, uint16_t length, void *userdata)
{
    char msg[MAX_STR_SIZE + 1];
    length = copy_tox_str(msg, sizeof(msg), (const char *) data, length);

    int i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].onFriendRequest != NULL)
            windows[i].onFriendRequest(&windows[i], m, (const char *) public_key, msg, length);
    }
}

void on_connectionchange(Tox *m, int32_t friendnumber, uint8_t status, void *userdata)
{
    int i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].onConnectionChange != NULL)
            windows[i].onConnectionChange(&windows[i], m, friendnumber, status);
    }
}

void on_typing_change(Tox *m, int32_t friendnumber, uint8_t is_typing, void *userdata)
{
    if (user_settings->show_typing_other == SHOW_TYPING_OFF)
        return;

    int i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].onTypingChange != NULL)
            windows[i].onTypingChange(&windows[i], m, friendnumber, is_typing);
    }
}

void on_message(Tox *m, int32_t friendnumber, const uint8_t *string, uint16_t length, void *userdata)
{
    char msg[MAX_STR_SIZE + 1];
    length = copy_tox_str(msg, sizeof(msg), (const char *) string, length);

    int i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].onMessage != NULL)
            windows[i].onMessage(&windows[i], m, friendnumber, msg, length);
    }
}

void on_action(Tox *m, int32_t friendnumber, const uint8_t *string, uint16_t length, void *userdata)
{
    char msg[MAX_STR_SIZE + 1];
    length = copy_tox_str(msg, sizeof(msg), (const char *) string, length);

    int i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].onAction != NULL)
            windows[i].onAction(&windows[i], m, friendnumber, msg, length);
    }
}

void on_nickchange(Tox *m, int32_t friendnumber, const uint8_t *string, uint16_t length, void *userdata)
{
    char nick[TOXIC_MAX_NAME_LENGTH + 1];
    length = copy_tox_str(nick, sizeof(nick), (const char *) string, length);
    filter_str(nick, length);

    int i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].onNickChange != NULL)
            windows[i].onNickChange(&windows[i], m, friendnumber, nick, length);
    }

    store_data(m, DATA_FILE);
}

void on_statusmessagechange(Tox *m, int32_t friendnumber, const uint8_t *string, uint16_t length, void *userdata)
{
    char msg[TOX_MAX_STATUSMESSAGE_LENGTH + 1];
    length = copy_tox_str(msg, sizeof(msg), (const char *) string, length);
    filter_str(msg, length);

    int i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].onStatusMessageChange != NULL)
            windows[i].onStatusMessageChange(&windows[i], friendnumber, msg, length);
    }
}

void on_statuschange(Tox *m, int32_t friendnumber, uint8_t status, void *userdata)
{
    int i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].onStatusChange != NULL)
            windows[i].onStatusChange(&windows[i], m, friendnumber, status);
    }
}

void on_friendadded(Tox *m, int32_t friendnumber, bool sort)
{
    int i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].onFriendAdded != NULL)
            windows[i].onFriendAdded(&windows[i], m, friendnumber, sort);
    }

    store_data(m, DATA_FILE);
}

void on_groupmessage(Tox *m, int groupnumber, int peernumber, const uint8_t *message, uint16_t length,
                     void *userdata)
{
    char msg[MAX_STR_SIZE + 1];
    length = copy_tox_str(msg, sizeof(msg), (const char *) message, length);

    int i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].onGroupMessage != NULL)
            windows[i].onGroupMessage(&windows[i], m, groupnumber, peernumber, msg, length);
    }
}

void on_groupaction(Tox *m, int groupnumber, int peernumber, const uint8_t *action, uint16_t length,
                    void *userdata)
{
    char msg[MAX_STR_SIZE + 1];
    length = copy_tox_str(msg, sizeof(msg), (const char *) action, length);

    int i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].onGroupAction != NULL)
            windows[i].onGroupAction(&windows[i], m, groupnumber, peernumber, msg, length);
    }
}

void on_groupinvite(Tox *m, int32_t friendnumber, uint8_t type, const uint8_t *group_pub_key, uint16_t length,
                    void *userdata)
{
    int i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].onGroupInvite != NULL)
            windows[i].onGroupInvite(&windows[i], m, friendnumber, type, (char *) group_pub_key, length);
    }
}

void on_group_namelistchange(Tox *m, int groupnumber, int peernumber, uint8_t change, void *userdata)
{
    int i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].onGroupNamelistChange != NULL)
            windows[i].onGroupNamelistChange(&windows[i], m, groupnumber, peernumber, change);
    }
}

void on_group_titlechange(Tox *m, int groupnumber, int peernumber, const uint8_t *title, uint8_t length,
                          void *userdata)
{
    char data[MAX_STR_SIZE + 1];
    length = copy_tox_str(data, sizeof(data), (const char *) title, length);

    int i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].onGroupTitleChange != NULL)
            windows[i].onGroupTitleChange(&windows[i], m, groupnumber, peernumber, data, length);
    }
}

void on_file_sendrequest(Tox *m, int32_t friendnumber, uint8_t filenumber, uint64_t filesize,
                         const uint8_t *filename, uint16_t filename_length, void *userdata)
{
    int i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].onFileSendRequest != NULL)
            windows[i].onFileSendRequest(&windows[i], m, friendnumber, filenumber, filesize,
                                         (const char *) filename, filename_length);
    }
}

void on_file_control (Tox *m, int32_t friendnumber, uint8_t receive_send, uint8_t filenumber,
                      uint8_t control_type, const uint8_t *data, uint16_t length, void *userdata)
{
    int i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].onFileControl != NULL)
            windows[i].onFileControl(&windows[i], m, friendnumber, receive_send, filenumber,
                                     control_type, (const char *) data, length);
    }
}

void on_file_data(Tox *m, int32_t friendnumber, uint8_t filenumber, const uint8_t *data, uint16_t length,
                  void *userdata)
{
    int i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].onFileData != NULL)
            windows[i].onFileData(&windows[i], m, friendnumber, filenumber, (const char *) data, length);
    }
}

void on_read_receipt(Tox *m, int32_t friendnumber, uint32_t receipt, void *userdata)
{
    int i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].onReadReceipt != NULL)
            windows[i].onReadReceipt(&windows[i], m, friendnumber, receipt);
    }
}

/* CALLBACKS END */

int add_window(Tox *m, ToxWindow w)
{
    if (LINES < 2)
        return -1;

    int i;

    for (i = 0; i < MAX_WINDOWS_NUM; i++) {
        if (windows[i].active)
            continue;

        w.window = newwin(LINES - 2, COLS, 0, 0);

        if (w.window == NULL)
            return -1;

#ifdef URXVT_FIX
        /* Fixes text color problem on some terminals. */
        wbkgd(w.window, COLOR_PAIR(6));
#endif
        windows[i] = w;

        if (w.onInit)
            w.onInit(&w, m);

        ++num_active_windows;

        return i;
    }

    return -1;
}

void set_active_window(int index)
{
    if (index < 0 || index >= MAX_WINDOWS_NUM)
        return;

    active_window = windows + index;
}

/* Shows next window when tab or back-tab is pressed */
void set_next_window(int ch)
{
    ToxWindow *end = windows + MAX_WINDOWS_NUM - 1;
    ToxWindow *inf = active_window;

    while (true) {
        if (ch == user_settings->key_next_tab) {
            if (++active_window > end)
                active_window = windows;
        } else if (--active_window < windows)
            active_window = end;

        if (active_window->window)
            return;

        if (active_window == inf)    /* infinite loop check */
            exit_toxic_err("failed in set_next_window", FATALERR_INFLOOP);
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

    if (n_prompt == -1 || add_window(m, new_friendlist()) == -1)
        exit_toxic_err("failed in init_windows", FATALERR_WININIT);

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

    int i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (!windows[i].active)
            continue;

        ToxWindow *w = &windows[i];

        if (windows[i].is_friendlist)  {
            delwin(w->window);
            w->window = newwin(y2, x2, 0, 0);
            continue;
        }

        if (w->help->active)
            wclear(w->help->win);

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

            if (!w->is_groupchat)
                w->stb->topline = subwin(w->window, 2, x2, 0, 0);
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
    if (toxwin->alert != WINDOW_ALERT_NONE) attron(COLOR_PAIR(toxwin->alert));
    clrtoeol();
    printw(" [%s]", toxwin->name);
    if (toxwin->alert != WINDOW_ALERT_NONE) attroff(COLOR_PAIR(toxwin->alert));
}

static void draw_bar(void)
{
    attron(COLOR_PAIR(BLUE));
    mvhline(LINES - 2, 0, '_', COLS);
    attroff(COLOR_PAIR(BLUE));

    move(LINES - 1, 0);

    attron(COLOR_PAIR(BLUE) | A_BOLD);
    printw(" TOXIC " TOXICVER " |");
    attroff(COLOR_PAIR(BLUE) | A_BOLD);

    int i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (!windows[i].active)
            continue;

        if (windows + i == active_window)

#ifdef URXVT_FIX
            attron(A_BOLD | COLOR_PAIR(GREEN));
        else
#endif

            attron(A_BOLD);

        draw_window_tab(&windows[i]);

        if (windows + i == active_window)

#ifdef URXVT_FIX
            attroff(A_BOLD | COLOR_PAIR(GREEN));
        else
#endif

            attroff(A_BOLD);
    }

    refresh();
}

void draw_active_window(Tox *m)
{
    ToxWindow *a = active_window;
    a->alert = WINDOW_ALERT_NONE;

    wint_t ch = 0;

    draw_bar();

    touchwin(a->window);
    a->onDraw(a, m);

    /* Handle input */
    bool ltr;
#ifdef HAVE_WIDECHAR
    int status = wget_wch(stdscr, &ch);

    if (status == ERR)
        return;

    if (status == OK)
        ltr = iswprint(ch);
    else /* if (status == KEY_CODE_YES) */
        ltr = false;

#else
    ch = getch();

    if (ch == ERR)
        return;

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
    int i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        ToxWindow *a = &windows[i];

        if (a->active && a != active_window && !a->is_friendlist)
            line_info_print(a);
    }
}

/* returns a pointer to the ToxWindow in the ith index. Returns NULL if no ToxWindow exists */
ToxWindow *get_window_ptr(int i)
{
    ToxWindow *toxwin = NULL;

    if (i >= 0 && i <= MAX_WINDOWS_NUM && windows[i].active)
        toxwin = &windows[i];

    return toxwin;
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
    int i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].is_chat)
            kill_chat_window(&windows[i], m);
        else if (windows[i].is_groupchat)
            kill_groupchat_window(&windows[i]);
    }

    kill_prompt_window(prompt);
    kill_friendlist();
}

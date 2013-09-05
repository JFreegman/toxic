#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "friendlist.h"
#include "prompt.h"
#include "toxic_windows.h"

extern char *DATA_FILE;
extern int store_data(Tox *m, char *path);

static ToxWindow windows[MAX_WINDOWS_NUM];
static ToxWindow *active_window;
static ToxWindow *prompt;
static Tox *m;

#define UNKNOWN_NAME "Unknown"

/* CALLBACKS START */
void on_request(uint8_t *public_key, uint8_t *data, uint16_t length, void *userdata)
{
    int n = add_req(public_key);
    wprintw(prompt->window, "\nFriend request from:\n");

    int i;

    for (i = 0; i < KEY_SIZE_BYTES; ++i) {
        wprintw(prompt->window, "%02x", public_key[i] & 0xff);
    }

    wprintw(prompt->window, "\nWith the message: %s\n", data);
    wprintw(prompt->window, "Type \"accept %d\" to accept it.\n", n);

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].onFriendRequest != NULL)
            windows[i].onFriendRequest(&windows[i], public_key, data, length);
    }
}

void on_connectionchange(Tox *m, int friendnumber, uint8_t status, void *userdata)
{
    uint8_t nick[TOX_MAX_NAME_LENGTH] = {0};
    tox_getname(m, friendnumber, (uint8_t *) &nick);

    if (!nick[0])
        snprintf(nick, sizeof(nick), "%s", UNKNOWN_NAME);

    if (status == 1) {
        wattron(prompt->window, A_BOLD);
        wprintw(prompt->window, "\n%s ", nick);
        wattroff(prompt->window, A_BOLD);
        wprintw(prompt->window, "has come online\n");
    } else {
        wattron(prompt->window, A_BOLD);
        wprintw(prompt->window, "\n%s ", nick);
        wattroff(prompt->window, A_BOLD);
        wprintw(prompt->window, "has gone offline\n");
    }

    int i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].onConnectionChange != NULL)
            windows[i].onConnectionChange(&windows[i], m, friendnumber, status);
    }
}

void on_message(Tox *m, int friendnumber, uint8_t *string, uint16_t length, void *userdata)
{
    int i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].onMessage != NULL)
            windows[i].onMessage(&windows[i], m, friendnumber, string, length);
    }
}

void on_action(Tox *m, int friendnumber, uint8_t *string, uint16_t length, void *userdata)
{
    int i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].onAction != NULL)
            windows[i].onAction(&windows[i], m, friendnumber, string, length);
    }
}

void on_nickchange(Tox *m, int friendnumber, uint8_t *string, uint16_t length, void *userdata)
{
    int i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].onNickChange != NULL)
            windows[i].onNickChange(&windows[i], friendnumber, string, length);
    }
}

void on_statusmessagechange(Tox *m, int friendnumber, uint8_t *string, uint16_t length, void *userdata)
{
    int i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].onStatusMessageChange != NULL)
            windows[i].onStatusMessageChange(&windows[i], friendnumber, string, length);
    }
}

void on_statuschange(Tox *m, int friendnumber, TOX_USERSTATUS status, void *userdata)
{
    int i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].onStatusChange != NULL)
            windows[i].onStatusChange(&windows[i], m, friendnumber, status);
    }
}

void on_friendadded(Tox *m, int friendnumber)
{
    friendlist_onFriendAdded(m, friendnumber);

    if (store_data(m, DATA_FILE))
        wprintw(prompt->window, "\nCould not store Tox data\n");
}
/* CALLBACKS END */

int add_window(Tox *m, ToxWindow w)
{
    if (LINES < 2)
        return -1;

    int i;

    for (i = 0; i < MAX_WINDOWS_NUM; i++) {
        if (windows[i].window)
            continue;

        w.window = newwin(LINES - 2, COLS, 0, 0);

        if (w.window == NULL)
            return -1;

        windows[i] = w;
        w.onInit(&w, m);

        return i;
    }

    return -1;
}

/* Deletes window w and cleans up */
void del_window(ToxWindow *w)
{
    active_window = windows; // Go to prompt screen

    delwin(w->window);
    memset(w, 0, sizeof(ToxWindow));

    clear();
    refresh();
}

/* Shows next window when tab or back-tab is pressed */
void set_next_window(int ch)
{
    ToxWindow *end = windows + MAX_WINDOWS_NUM - 1;
    ToxWindow *inf = active_window;

    while (true) {
        if (ch == '\t') {
            if (++active_window > end)
                active_window = windows;
        } else if (--active_window < windows)
            active_window = end;

        if (active_window->window)
            return;

        if (active_window == inf) {    // infinite loop check
            endwin();
            exit(2);
        }
    }
}

void set_active_window(int index)
{
    if (index < 0 || index >= MAX_WINDOWS_NUM)
        return;

    active_window = windows + index;
}

ToxWindow *init_windows()
{
    int n_prompt = add_window(m, new_prompt());

    if (n_prompt == -1 || add_window(m, new_friendlist()) == -1) {
        fprintf(stderr, "add_window() failed.\n");
        endwin();
        exit(1);
    }

    prompt = &windows[n_prompt];
    active_window = prompt;

    return prompt;
}

static void draw_bar()
{
    static int odd = 0;
    int blinkrate = 30;

    attron(COLOR_PAIR(BLUE));
    mvhline(LINES - 2, 0, '_', COLS);
    attroff(COLOR_PAIR(BLUE));

    move(LINES - 1, 0);

    attron(COLOR_PAIR(BLUE) | A_BOLD);
    printw(" TOXIC " TOXICVER " |");
    attroff(COLOR_PAIR(BLUE) | A_BOLD);

    int i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].window) {
            if (windows + i == active_window)
                attron(A_BOLD);

            odd = (odd + 1) % blinkrate;

            if (windows[i].blink && (odd < (blinkrate / 2)))
                attron(COLOR_PAIR(RED));

            clrtoeol();
            printw(" [%s]", windows[i].name);

            if (windows[i].blink && (odd < (blinkrate / 2)))
                attroff(COLOR_PAIR(RED));

            if (windows + i == active_window) {
                attroff(A_BOLD);
            }
        }
    }

    refresh();
}

void prepare_window(WINDOW *w)
{
    mvwin(w, 0, 0);
    wresize(w, LINES - 2, COLS);
}

void draw_active_window(Tox *m)
{

    ToxWindow *a = active_window;
    wint_t ch = 0;

    prepare_window(a->window);
    a->blink = false;
    draw_bar();
    a->onDraw(a, m);

    /* Handle input */
#ifdef HAVE_WIDECHAR
    wget_wch(stdscr, &ch);
#else
    ch = getch();
#endif

    if (ch == '\t' || ch == KEY_BTAB)
        set_next_window((int) ch);
    else if (ch != ERR)
        a->onKey(a, m, ch);
}

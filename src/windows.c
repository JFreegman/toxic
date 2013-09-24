#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "friendlist.h"
#include "prompt.h"
#include "toxic_windows.h"

extern char *DATA_FILE;

static ToxWindow windows[MAX_WINDOWS_NUM];
static ToxWindow *active_window;
static ToxWindow *prompt;
static Tox *m;

/* CALLBACKS START */
void on_request(uint8_t *public_key, uint8_t *data, uint16_t length, void *userdata)
{
    int i;
    
    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].onFriendRequest != NULL)
            windows[i].onFriendRequest(&windows[i], public_key, data, length);
    }
}

void on_connectionchange(Tox *m, int friendnumber, uint8_t status, void *userdata)
{
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
    if (friendnumber < 0 || friendnumber > MAX_FRIENDS_NUM)
        return;

    if (length >= TOXIC_MAX_NAME_LENGTH) {    /* length includes null byte */
        string[TOXIC_MAX_NAME_LENGTH] = L'\0';
        length = TOXIC_MAX_NAME_LENGTH + 1;
        tox_setfriendname(m, friendnumber, string, length);
    }

    /* Append friendnumber to duplicate nicks to guarantee uniqueness */
    int n = get_friendnum(string);

    if (n != friendnumber && n != -1) {
        char n_buf[strlen(string)+4];    /* must have room for friendnum chars relative to MAX_FRIENDS_NUM */
        snprintf(n_buf, sizeof(n_buf), "%s%d", string, friendnumber);
        strcpy(string, n_buf);
        length = strlen(n_buf) + 1;
        tox_setfriendname(m, friendnumber, string, length);
    }

    int i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].onNickChange != NULL)
            windows[i].onNickChange(&windows[i], friendnumber, string, length);
    }

    if (store_data(m, DATA_FILE))
        wprintw(prompt->window, "\nCould not store Tox data\n");
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

void on_groupmessage(Tox *m, int groupnumber, int peernumber, uint8_t *message, uint16_t length, void *userdata)
{
    int i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].onGroupMessage != NULL)
            windows[i].onGroupMessage(&windows[i], m, groupnumber, peernumber, message, length);
    }
}

void on_groupinvite(Tox *m, int friendnumber, uint8_t *group_pub_key, void *userdata)
{
    int i;

    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].onGroupInvite != NULL)
            windows[i].onGroupInvite(&windows[i], m, friendnumber, group_pub_key);
    }
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
            fprintf(stderr, "set_next_window() failed. Aborting...\n");
            exit(EXIT_FAILURE);
        }
    }
}

void set_active_window(int index)
{
    if (index < 0 || index >= MAX_WINDOWS_NUM)
        return;

    active_window = windows + index;
}

ToxWindow *init_windows(Tox *mToAssign)
{
    m = mToAssign;
    int n_prompt = add_window(m, new_prompt());

    if (n_prompt == -1 || add_window(m, new_friendlist()) == -1) {
        endwin();
        fprintf(stderr, "add_window() failed. Aborting...\n");
        exit(EXIT_FAILURE);
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
#ifndef WIN32
    wresize(w, LINES - 2, COLS);
#endif
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

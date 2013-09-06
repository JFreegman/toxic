/*
 * Toxic -- Tox Curses Client
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>

#include <tox/tox.h>

#include "friendlist.h"

extern char *DATA_FILE;
extern int store_data(Tox *m, char *path);

typedef struct {
    uint8_t name[TOX_MAX_NAME_LENGTH];
    uint8_t statusmsg[TOX_MAX_STATUSMESSAGE_LENGTH];
    int num;
    int chatwin;
    bool active;
    bool online;
    TOX_USERSTATUS status;
} friend_t;

static friend_t friends[MAX_FRIENDS_NUM];
static int num_friends = 0;
static int num_selected = 0;


void friendlist_onMessage(ToxWindow *self, Tox *m, int num, uint8_t *str, uint16_t len)
{
    if (num < 0 || num >= num_friends)
        return;

    if (friends[num].chatwin == -1)
        friends[num].chatwin = add_window(m, new_chat(m, friends[num].num));
}

void friendlist_onConnectionChange(ToxWindow *self, Tox *m, int num, uint8_t status)
{
    if (num < 0 || num >= num_friends)
        return;

    if (status == 1)
        friends[num].online = true;
    else
        friends[num].online = false;
}

void friendlist_onNickChange(ToxWindow *self, int num, uint8_t *str, uint16_t len)
{
    if (len >= TOX_MAX_NAME_LENGTH || num < 0 || num >= num_friends)
        return;

    memcpy((char *) &friends[num].name, (char *) str, len);
    friends[num].name[len] = 0;
}

void friendlist_onStatusChange(ToxWindow *self, Tox *m, int num, TOX_USERSTATUS status)
{
    if (num < 0 || num >= num_friends)
        return;

    friends[num].status = status;
}

void friendlist_onStatusMessageChange(ToxWindow *self, int num, uint8_t *str, uint16_t len)
{
    if (len >= TOX_MAX_STATUSMESSAGE_LENGTH || num < 0 || num >= num_friends)
        return;

    memcpy((char *) &friends[num].statusmsg, (char *) str, len);
    friends[num].statusmsg[len] = 0;
}

int friendlist_onFriendAdded(Tox *m, int num)
{
    if (num_friends < 0 || num_friends >= MAX_FRIENDS_NUM)
        return -1;

    int i;

    for (i = 0; i <= num_friends; ++i) {
        if (!friends[i].active) {
            friends[i].num = num;
            friends[i].active = true;
            friends[i].chatwin = -1;
            friends[i].online = false;
            friends[i].status = TOX_USERSTATUS_NONE;

            if (tox_getname(m, num, friends[i].name) != 0 || friends[i].name[0] == '\0')
                strcpy((char *) friends[i].name, "unknown");

            if (i == num_friends)
                ++num_friends;

            return 0;
        }
    }

    return -1;
}

static void select_friend(Tox *m, wint_t key)
{
    if (num_friends < 1)
        return;

    int n = num_selected;

    if (key == KEY_UP) {
        while (--n != num_selected) {
            if (n < 0) n = num_friends - 1;
            if (friends[n].active) {
                num_selected = n;
                return;
            }
        }
    } else if (key == KEY_DOWN) {
        while (++n != num_selected) {
            n = n % num_friends;
            if (friends[n].active) {
                num_selected = n;
                return;
            }
        }
    } else return;    /* Bad key input */

    /* If we reach this something is wrong */
    endwin();
    tox_kill(m);
    exit(2);
}

static void delete_friend(Tox *m, ToxWindow *self, int f_num, wint_t key)
{
    tox_delfriend(m, f_num);
    memset(&(friends[f_num]), 0, sizeof(friend_t));
    
    int i;

    for (i = num_friends; i > 0; --i) {
        if (friends[i-1].active)
            break;
    }

    num_friends = i;

    store_data(m, DATA_FILE);
    select_friend(m, KEY_DOWN);
}

static void friendlist_onKey(ToxWindow *self, Tox *m, wint_t key)
{
    if (key == KEY_UP || key == KEY_DOWN) {
        select_friend(m, key);
    } else if (key == '\n') {
        /* Jump to chat window if already open */
        if (friends[num_selected].chatwin != -1) {
            set_active_window(friends[num_selected].chatwin);
        } else {
            friends[num_selected].chatwin = add_window(m, new_chat(m, friends[num_selected].num));
            set_active_window(friends[num_selected].chatwin);
        }
    } else if (key == 0x107 || key == 0x8 || key == 0x7f)
        delete_friend(m, self, num_selected, key);
}

static void friendlist_onDraw(ToxWindow *self, Tox *m)
{
    curs_set(0);
    werase(self->window);

    if (num_friends == 0) {
        wprintw(self->window, "Empty. Add some friends! :-)\n");
    } else {
        wattron(self->window, COLOR_PAIR(CYAN) | A_BOLD);
        wprintw(self->window, " Open chat with up/down keys and enter.\n");
        wprintw(self->window, " Delete friends with the backspace key.\n\n");
        wattroff(self->window, COLOR_PAIR(CYAN) | A_BOLD);
    }

    int i;

    for (i = 0; i < num_friends; ++i) {
        if (friends[i].active) {
            if (i == num_selected)
                wprintw(self->window, " > ");
            else
                wprintw(self->window, "   ");

            if (friends[i].online) {
                TOX_USERSTATUS status = friends[i].status;
                int colour = WHITE;

                switch(status) {
                case TOX_USERSTATUS_NONE:
                    colour = GREEN;
                    break;
                case TOX_USERSTATUS_AWAY:
                    colour = YELLOW;
                    break;
                case TOX_USERSTATUS_BUSY:
                    colour = RED;
                    break;
                }

                wprintw(self->window, "[");
                wattron(self->window, COLOR_PAIR(colour) | A_BOLD);
                wprintw(self->window, "O");
                wattroff(self->window, COLOR_PAIR(colour) | A_BOLD);
                wprintw(self->window, "]%s", friends[i].name);

                if (friends[i].statusmsg[0])
                    wprintw(self->window, " (%s)\n", friends[i].statusmsg);
                else
                    wprintw(self->window, "\n");
            } else {
                wprintw(self->window, "[O]%s\n", friends[i].name);
            }
        }
    }

    wrefresh(self->window);
}

void disable_chatwin(int f_num)
{
    friends[f_num].chatwin = -1;
}

static void friendlist_onInit(ToxWindow *self, Tox *m)
{

}

ToxWindow new_friendlist()
{
    ToxWindow ret;
    memset(&ret, 0, sizeof(ret));

    ret.onKey = &friendlist_onKey;
    ret.onDraw = &friendlist_onDraw;
    ret.onInit = &friendlist_onInit;
    ret.onMessage = &friendlist_onMessage;
    ret.onConnectionChange = &friendlist_onConnectionChange;
    ret.onAction = &friendlist_onMessage;    // Action has identical behaviour to message
    ret.onNickChange = &friendlist_onNickChange;
    ret.onStatusChange = &friendlist_onStatusChange;
    ret.onStatusMessageChange = &friendlist_onStatusMessageChange;

    strcpy(ret.name, "friends");
    return ret;
}

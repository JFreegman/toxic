/*  friendlist.c
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include <tox/tox.h>

#include "chat.h"
#include "friendlist.h"
#include "misc_tools.h"

extern char *DATA_FILE;
extern ToxWindow *prompt;

static int max_friends_index = 0;    /* marks the index of the last friend in friends array */
static int num_selected = 0;
static int num_friends = 0;

ToxicFriend friends[MAX_FRIENDS_NUM];
static int friendlist_index[MAX_FRIENDS_NUM] = {0};

#define S_WEIGHT 100

static int index_name_cmp(const void *n1, const void *n2)
{
    int res = qsort_strcasecmp_hlpr(friends[*(int *) n1].name, friends[*(int *) n2].name);

    /* Use weight to make qsort always put online friends before offline */
    res = friends[*(int *) n1].online ? (res - S_WEIGHT) : (res + S_WEIGHT);
    res = friends[*(int *) n2].online ? (res + S_WEIGHT) : (res - S_WEIGHT);

    return res;
}

/* sorts friendlist_index first by connection status then alphabetically */
void sort_friendlist_index(Tox *m)
{
    int i;
    int n = 0;

    for (i = 0; i < max_friends_index; ++i) {
        if (friends[i].active)
            friendlist_index[n++] = friends[i].num;
    }

    qsort(friendlist_index, num_friends, sizeof(int), index_name_cmp);
}

static void friendlist_onMessage(ToxWindow *self, Tox *m, int num, uint8_t *str, uint16_t len)
{
    if (num >= max_friends_index)
        return;

    if (friends[num].chatwin == -1) {
        if (num_active_windows() < MAX_WINDOWS_NUM) {
            friends[num].chatwin = add_window(m, new_chat(m, friends[num].num));
        } else {
            uint8_t nick[TOX_MAX_NAME_LENGTH] = {'\0'};
            tox_get_name(m, num, nick);
            nick[TOXIC_MAX_NAME_LENGTH] = '\0';
            wprintw(prompt->window, "%s: %s\n", nick, str);

            prep_prompt_win();
            wattron(prompt->window, COLOR_PAIR(RED));
            wprintw(prompt->window, "* Warning: Too many windows are open.\n");
            wattron(prompt->window, COLOR_PAIR(RED));

            alert_window(prompt, WINDOW_ALERT_1, true);
        }
    }
}

static void friendlist_onConnectionChange(ToxWindow *self, Tox *m, int num, uint8_t status)
{
    if (num >= max_friends_index)
        return;

    friends[num].online = status == 1 ? true : false;
    sort_friendlist_index(m);
}

static void friendlist_onNickChange(ToxWindow *self, Tox *m, int num, uint8_t *str, uint16_t len)
{
    if (len > TOX_MAX_NAME_LENGTH || num >= max_friends_index)
        return;

    str[TOXIC_MAX_NAME_LENGTH] = '\0';
    len = strlen(str) + 1;
    memcpy(friends[num].name, str, len);
    friends[num].namelength = len;
    sort_friendlist_index(m);
}

static void friendlist_onStatusChange(ToxWindow *self, Tox *m, int num, TOX_USERSTATUS status)
{
    if (num >= max_friends_index)
        return;

    friends[num].status = status;
}

static void friendlist_onStatusMessageChange(ToxWindow *self, int num, uint8_t *str, uint16_t len)
{
    if (len > TOX_MAX_STATUSMESSAGE_LENGTH || num >= max_friends_index)
        return;

    memcpy(friends[num].statusmsg, str, len);
    friends[num].statusmsg_len = len;
}

void friendlist_onFriendAdded(ToxWindow *self, Tox *m, int num, bool sort)
{
    if (max_friends_index < 0 || max_friends_index >= MAX_FRIENDS_NUM)
        return;

    int i;

    for (i = 0; i <= max_friends_index; ++i) {
        if (!friends[i].active) {
            friends[i].num = num;
            friends[i].active = true;
            friends[i].chatwin = -1;
            friends[i].online = false;
            friends[i].status = TOX_USERSTATUS_NONE;
            friends[i].namelength = tox_get_name(m, num, friends[i].name);
            tox_get_client_id(m, num, friends[i].pub_key);

            if (friends[i].namelength == -1 || friends[i].name[0] == '\0') {
                strcpy(friends[i].name, (uint8_t *) UNKNOWN_NAME);
                friends[i].namelength = strlen(UNKNOWN_NAME) + 1;
            } else {    /* Enforce toxic's maximum name length */
                friends[i].name[TOXIC_MAX_NAME_LENGTH] = '\0';
                friends[i].namelength = strlen(friends[i].name) + 1;
            }

            num_friends = tox_count_friendlist(m);

            if (i == max_friends_index)
                ++max_friends_index;

            if (sort)
                sort_friendlist_index(m);

            return;
        }
    }
}

static void friendlist_onFileSendRequest(ToxWindow *self, Tox *m, int num, uint8_t filenum, 
                                         uint64_t filesize, uint8_t *filename, uint16_t filename_len)
{
    if (num >= max_friends_index)
        return;

    if (friends[num].chatwin == -1) {
        if (num_active_windows() < MAX_WINDOWS_NUM) {
            friends[num].chatwin = add_window(m, new_chat(m, friends[num].num));
        } else {
            tox_file_send_control(m, num, 1, filenum, TOX_FILECONTROL_KILL, 0, 0);

            uint8_t nick[TOX_MAX_NAME_LENGTH] = {'\0'};
            tox_get_name(m, num, nick);
            nick[TOXIC_MAX_NAME_LENGTH] = '\0';

            prep_prompt_win();
            wattron(prompt->window, COLOR_PAIR(RED));
            wprintw(prompt->window, "* File transfer from %s failed: too many windows are open.\n", nick);
            wattron(prompt->window, COLOR_PAIR(RED));

            alert_window(prompt, WINDOW_ALERT_1, true);
        }
    }
}

static void friendlist_onGroupInvite(ToxWindow *self, Tox *m, int num, uint8_t *group_pub_key)
{
    if (num >= max_friends_index)
        return;

    if (friends[num].chatwin == -1) {
        if (num_active_windows() < MAX_WINDOWS_NUM) {
            friends[num].chatwin = add_window(m, new_chat(m, friends[num].num));
        } else {
            uint8_t nick[TOX_MAX_NAME_LENGTH] = {'\0'};
            tox_get_name(m, num, nick);
            nick[TOXIC_MAX_NAME_LENGTH] = '\0';

            prep_prompt_win();
            wattron(prompt->window, COLOR_PAIR(RED));
            wprintw(prompt->window, "* Group chat invite from %s failed: too many windows are open.\n", nick);
            wattron(prompt->window, COLOR_PAIR(RED));

            alert_window(prompt, WINDOW_ALERT_1, true);
        }
    }
}

static void select_friend(ToxWindow *self, Tox *m, wint_t key)
{
    if (key == KEY_UP) {
        if (--num_selected < 0)
            num_selected = num_friends - 1;
    } else if (key == KEY_DOWN) {
        num_selected = (num_selected + 1) % num_friends;
    }
}

static void delete_friend(Tox *m, ToxWindow *self, int f_num, wint_t key)
{
    tox_del_friend(m, f_num);
    memset(&friends[f_num], 0, sizeof(ToxicFriend));
    
    int i;

    for (i = max_friends_index; i > 0; --i) {
        if (friends[i-1].active)
            break;
    }

    max_friends_index = i;
    num_friends = tox_count_friendlist(m);

    /* make sure num_selected stays within num_friends range */
    if (num_friends && num_selected == num_friends)
        --num_selected;

    sort_friendlist_index(m);
    store_data(m, DATA_FILE);
}

static void friendlist_onKey(ToxWindow *self, Tox *m, wint_t key, wint_t key_code)
{
    if (num_friends == 0)
        return;

    int f = friendlist_index[num_selected];

    if (key == '\n') {
        /* Jump to chat window if already open */
        if (friends[f].chatwin != -1) {
            set_active_window(friends[f].chatwin);
        } else if (num_active_windows() < MAX_WINDOWS_NUM) {
            friends[f].chatwin = add_window(m, new_chat(m, friends[f].num));
            set_active_window(friends[f].chatwin);
        } else {
            prep_prompt_win();
            wattron(prompt->window, COLOR_PAIR(RED));
            wprintw(prompt->window, "* Warning: Too many windows are open.\n");
            wattron(prompt->window, COLOR_PAIR(RED));

            alert_window(prompt, WINDOW_ALERT_1, true);
        }
    } else if (key == KEY_DC) {
        delete_friend(m, self, f, key);
    } else {
        select_friend(self, m, key);
    }
}

#define FLIST_OFST 4    /* Accounts for the lines at top */

static void friendlist_onDraw(ToxWindow *self, Tox *m)
{
    curs_set(0);
    werase(self->window);
    int x2, y2;
    getmaxyx(self->window, y2, x2);

    bool fix_statuses = x2 != self->x;    /* true if window x axis has changed */

    wattron(self->window, COLOR_PAIR(CYAN));
    wprintw(self->window, " Open a chat window with the");
    wattron(self->window, A_BOLD);
    wprintw(self->window, " Enter ");
    wattroff(self->window, A_BOLD);
    wprintw(self->window, "key. Delete a friend with the");
    wattron(self->window, A_BOLD);
    wprintw(self->window, " Delete ");
    wattroff(self->window, A_BOLD);
    wprintw(self->window, "key.\n\n");
    wattroff(self->window, COLOR_PAIR(CYAN));

    wattron(self->window, A_BOLD);
    wprintw(self->window, " Friends: %d/%d \n\n", tox_get_num_online_friends(m), num_friends);
    wattroff(self->window, A_BOLD);

    if ((y2 - FLIST_OFST) <= 0)    /* don't allow division by zero */
        return;

    /* Determine which portion of friendlist to draw based on current position */
    int page = num_selected / (y2 - FLIST_OFST);
    int start = (y2 - FLIST_OFST) * page;
    int end = y2 - FLIST_OFST + start;

    int i;

    for (i = start; i < num_friends && i < end; ++i) {
        int f = friendlist_index[i];
        bool f_selected = false;

        if (friends[f].active) {
            if (i == num_selected) {
                wattron(self->window, A_BOLD);
                wprintw(self->window, " > ");
                wattroff(self->window, A_BOLD);
                f_selected = true;
            } else {
                wprintw(self->window, "   ");
            }
            
            if (friends[f].online) {
                TOX_USERSTATUS status = friends[f].status;
                int colour = WHITE;

                switch (status) {
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
                wprintw(self->window, "]");

                if (f_selected)
                    wattron(self->window, A_BOLD);

                wprintw(self->window, "%s", friends[f].name);

                if (f_selected)
                    wattroff(self->window, A_BOLD);

                /* Reset friends[f].statusmsg on window resize */
                if (fix_statuses) {
                    uint8_t statusmsg[TOX_MAX_STATUSMESSAGE_LENGTH] = {'\0'};
                    tox_get_status_message(m, friends[f].num, statusmsg, TOX_MAX_STATUSMESSAGE_LENGTH);
                    snprintf(friends[f].statusmsg, sizeof(friends[f].statusmsg), "%s", statusmsg);
                    friends[f].statusmsg_len = tox_get_status_message_size(m, f);
                }

                /* Truncate note if it doesn't fit on one line */
                uint16_t maxlen = x2 - getcurx(self->window) - 4;
                if (friends[f].statusmsg_len > maxlen) {
                    friends[f].statusmsg[maxlen-3] = '\0';
                    strcat(friends[f].statusmsg, "...");
                    friends[f].statusmsg[maxlen] = '\0';
                    friends[f].statusmsg_len = maxlen;
                }

                wprintw(self->window, " (%s)\n", friends[f].statusmsg);
            } else {
                wprintw(self->window, "[");
                wattron(self->window, A_BOLD);
                wprintw(self->window, "O");
                wattroff(self->window, A_BOLD);
                wprintw(self->window, "]");

                if (f_selected)
                    wattron(self->window, A_BOLD);

                wprintw(self->window, "%s\n", friends[f].name);

                if (f_selected)
                    wattroff(self->window, A_BOLD);
            }
        }
    }

    self->x = x2;
    wrefresh(self->window);
}

void disable_chatwin(int f_num)
{
    friends[f_num].chatwin = -1;
}

static void friendlist_onInit(ToxWindow *self, Tox *m)
{

}

ToxWindow new_friendlist(void)
{
    ToxWindow ret;
    memset(&ret, 0, sizeof(ret));

    ret.active = true;

    ret.onKey = &friendlist_onKey;
    ret.onDraw = &friendlist_onDraw;
    ret.onInit = &friendlist_onInit;
    ret.onFriendAdded = &friendlist_onFriendAdded;
    ret.onMessage = &friendlist_onMessage;
    ret.onConnectionChange = &friendlist_onConnectionChange;
    ret.onAction = &friendlist_onMessage;    // Action has identical behaviour to message
    ret.onNickChange = &friendlist_onNickChange;
    ret.onStatusChange = &friendlist_onStatusChange;
    ret.onStatusMessageChange = &friendlist_onStatusMessageChange;
    ret.onFileSendRequest = &friendlist_onFileSendRequest;
    ret.onGroupInvite = &friendlist_onGroupInvite;

    strcpy(ret.name, "friends");
    return ret;
}

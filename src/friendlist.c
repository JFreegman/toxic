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
#include <time.h>

#include <tox/tox.h>

#include "chat.h"
#include "friendlist.h"
#include "misc_tools.h"

#ifdef _SUPPORT_AUDIO
#include "audio_call.h"
#endif

extern char *DATA_FILE;
extern ToxWindow *prompt;

static int max_friends_index = 0;    /* marks the index of the last friend in friends array */
static int num_selected = 0;
static int num_friends = 0;

extern struct _Winthread Winthread;
ToxicFriend friends[MAX_FRIENDS_NUM];
static int friendlist_index[MAX_FRIENDS_NUM] = {0};

struct _pendingDel {
    int num;
    bool active;
} pendingdelete;

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
void sort_friendlist_index(void)
{
    int i;
    int n = 0;

    for (i = 0; i < max_friends_index; ++i) {
        if (friends[i].active)
            friendlist_index[n++] = friends[i].num;
    }

    qsort(friendlist_index, num_friends, sizeof(int), index_name_cmp);
}

static void update_friend_last_online(int num, uint64_t timestamp)
{
    friends[num].last_online.last_on = timestamp;
    friends[num].last_online.tm = *localtime(&timestamp);
    strftime(friends[num].last_online.hour_min_str, TIME_STR_SIZE, "%I:%M %p", 
            &friends[num].last_online.tm);
}

static void friendlist_onMessage(ToxWindow *self, Tox *m, int num, uint8_t *str, uint16_t len)
{
    if (num >= max_friends_index)
        return;

    if (friends[num].chatwin == -1) {
        if (get_num_active_windows() < MAX_WINDOWS_NUM) {
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
    update_friend_last_online(num, get_unix_time());
    store_data(m, DATA_FILE);
    sort_friendlist_index();
}

static void friendlist_onNickChange(ToxWindow *self, Tox *m, int num, uint8_t *str, uint16_t len)
{
    if (len > TOX_MAX_NAME_LENGTH || num >= max_friends_index)
        return;

    str[TOXIC_MAX_NAME_LENGTH] = '\0';
    len = strlen(str) + 1;
    memcpy(friends[num].name, str, len);
    friends[num].namelength = len;
    sort_friendlist_index();
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
            update_friend_last_online(i, tox_get_last_online(m, i));

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
                sort_friendlist_index();

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
        if (get_num_active_windows() < MAX_WINDOWS_NUM) {
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
        if (get_num_active_windows() < MAX_WINDOWS_NUM) {
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

static void delete_friend(Tox *m, int f_num)
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

    sort_friendlist_index();
    store_data(m, DATA_FILE);
}

/* activates delete friend popup */
static void del_friend_activate(ToxWindow *self, Tox *m, int f_num)
{
    int x2, y2;
    getmaxyx(self->window, y2, x2);
    self->popup = newwin(3, 22 + TOXIC_MAX_NAME_LENGTH, 8, 8);

    pendingdelete.active = true;
    pendingdelete.num = f_num;
}

/* deactivates delete friend popup and deletes friend if instructed */
static void del_friend_deactivate(ToxWindow *self, Tox *m, wint_t key)
{
    if (key == 'y')
        delete_friend(m, pendingdelete.num);
 
    memset(&pendingdelete, 0, sizeof(pendingdelete));
    delwin(self->popup);
    self->popup = NULL;
    clear();
    refresh();
}

static void draw_popup(ToxWindow *self, Tox *m)
{
    if (self->popup == NULL)
        return;

    wattron(self->popup, A_BOLD);
    box(self->popup, ACS_VLINE, ACS_HLINE);
    wattroff(self->popup, A_BOLD);

    wmove(self->popup, 1, 1);
    wprintw(self->popup, "Delete contact ");
    wattron(self->popup, A_BOLD);
    wprintw(self->popup, "%s", friends[pendingdelete.num].name);
    wattroff(self->popup, A_BOLD);
    wprintw(self->popup, "? y/n");

    wrefresh(self->popup);
}

static void friendlist_onKey(ToxWindow *self, Tox *m, wint_t key)
{
    if (num_friends == 0)
        return;

    int f = friendlist_index[num_selected];

    /* lock screen and force decision on deletion popup */
    if (pendingdelete.active) {
        if (key == 'y' || key == 'n')
            del_friend_deactivate(self, m, key);

        return;
    }

    if (key == '\n') {
        /* Jump to chat window if already open */
        if (friends[f].chatwin != -1) {
            set_active_window(friends[f].chatwin);
        } else if (get_num_active_windows() < MAX_WINDOWS_NUM) {
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
        del_friend_activate(self, m, f);
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

    uint64_t cur_time = get_unix_time();
    struct tm cur_loc_tm = *localtime(&cur_time);

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

    pthread_mutex_lock(&Winthread.lock);
    int nf = tox_get_num_online_friends(m);
    pthread_mutex_unlock(&Winthread.lock);

    wattron(self->window, A_BOLD);
    wprintw(self->window, " Online: ");
    wattroff(self->window, A_BOLD);
    wprintw(self->window, "%d/%d \n\n", nf, num_friends);

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

                wattron(self->window, COLOR_PAIR(colour) | A_BOLD);
                wprintw(self->window, "O ");
                wattroff(self->window, COLOR_PAIR(colour) | A_BOLD);

                if (f_selected)
                    wattron(self->window, COLOR_PAIR(BLUE));

                wattron(self->window, A_BOLD);
                wprintw(self->window, "%s", friends[f].name);
                wattroff(self->window, A_BOLD);

                if (f_selected)
                    wattroff(self->window, COLOR_PAIR(BLUE));

                /* Reset friends[f].statusmsg on window resize */
                if (fix_statuses) {
                    uint8_t statusmsg[TOX_MAX_STATUSMESSAGE_LENGTH] = {'\0'};

                    pthread_mutex_lock(&Winthread.lock);
                    tox_get_status_message(m, friends[f].num, statusmsg, TOX_MAX_STATUSMESSAGE_LENGTH);
                    friends[f].statusmsg_len = tox_get_status_message_size(m, f);
                    pthread_mutex_unlock(&Winthread.lock);

                    snprintf(friends[f].statusmsg, sizeof(friends[f].statusmsg), "%s", statusmsg);
                }

                /* Truncate note if it doesn't fit on one line */
                uint16_t maxlen = x2 - getcurx(self->window) - 2;
                if (friends[f].statusmsg_len > maxlen) {
                    friends[f].statusmsg[maxlen-3] = '\0';
                    strcat(friends[f].statusmsg, "...");
                    friends[f].statusmsg[maxlen] = '\0';
                    friends[f].statusmsg_len = maxlen;
                }

                if (friends[f].statusmsg[0])
                    wprintw(self->window, " %s", friends[f].statusmsg);

                wprintw(self->window, "\n");
            } else {
                wprintw(self->window, "o ");

                if (f_selected)
                    wattron(self->window, COLOR_PAIR(BLUE));

                wattron(self->window, A_BOLD);
                wprintw(self->window, "%s", friends[f].name);
                wattroff(self->window, A_BOLD);

                if (f_selected)
                    wattroff(self->window, COLOR_PAIR(YELLOW));
    
                uint64_t last_seen = friends[f].last_online.last_on;

                if (last_seen != 0) {
                    int day_dist = (cur_loc_tm.tm_yday - friends[f].last_online.tm.tm_yday) % 365;
                    const uint8_t *hourmin = friends[f].last_online.hour_min_str;

                    switch (day_dist) {
                    case 0:
                        wprintw(self->window, " Last seen: Today %s\n", hourmin);
                        break;
                    case 1:
                        wprintw(self->window, " Last seen: Yesterday %s\n", hourmin);
                        break;
                    default:
                        wprintw(self->window, " Last seen: %d days ago\n", day_dist);
                        break;
                    } 
                } else {
                    wprintw(self->window, " Last seen: Never\n");
                }
            }
        }
    }

    self->x = x2;
    wrefresh(self->window);
    draw_popup(self, m);
}

void disable_chatwin(int f_num)
{
    friends[f_num].chatwin = -1;
}

static void friendlist_onInit(ToxWindow *self, Tox *m)
{

}

#ifdef _SUPPORT_AUDIO
static void friendlist_onAv(ToxWindow *self, ToxAv *av)
{
    int id = toxav_get_peer_id(av, 0);
    /*id++;*/
    if ( id != ErrorInternal && id >= max_friends_index)
        return;
    
    Tox* m = toxav_get_tox(av);
    
    if (friends[id].chatwin == -1) {
        if (get_num_active_windows() < MAX_WINDOWS_NUM) {
            friends[id].chatwin = add_window(m, new_chat(m, friends[id].num));
        } else {
            uint8_t nick[TOX_MAX_NAME_LENGTH] = {'\0'};
            tox_get_name(m, id, nick);
            nick[TOXIC_MAX_NAME_LENGTH] = '\0';
            wprintw(prompt->window, "Audio action from: %s!\n", nick);
            
            prep_prompt_win();
            wattron(prompt->window, COLOR_PAIR(RED));
            wprintw(prompt->window, "* Warning: Too many windows are open.\n");
            wattron(prompt->window, COLOR_PAIR(RED));
            
            alert_window(prompt, WINDOW_ALERT_0, true);
        }
    }
}
#endif /* _SUPPORT_AUDIO */

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
    ret.onAction = &friendlist_onMessage;    /* Action has identical behaviour to message */
    ret.onNickChange = &friendlist_onNickChange;
    ret.onStatusChange = &friendlist_onStatusChange;
    ret.onStatusMessageChange = &friendlist_onStatusMessageChange;
    ret.onFileSendRequest = &friendlist_onFileSendRequest;
    ret.onGroupInvite = &friendlist_onGroupInvite;
    
#ifdef _SUPPORT_AUDIO
    ret.onInvite = &friendlist_onAv;
    ret.onRinging = &friendlist_onAv;
    ret.onStarting = &friendlist_onAv;
    ret.onEnding = &friendlist_onAv;
    ret.onError = &friendlist_onAv;
    ret.onStart = &friendlist_onAv;
    ret.onCancel = &friendlist_onAv;
    ret.onReject = &friendlist_onAv;
    ret.onEnd = &friendlist_onAv;
    ret.onRequestTimeout = &friendlist_onAv;
    ret.onPeerTimeout = &friendlist_onAv;
#endif /* _SUPPORT_AUDIO */

    strcpy(ret.name, "friends");
    return ret;
}

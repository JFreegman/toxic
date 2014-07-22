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

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#include <tox/tox.h>

#include "toxic.h"
#include "windows.h"
#include "chat.h"
#include "friendlist.h"
#include "misc_tools.h"
#include "line_info.h"
#include "settings.h"
#include "notify.h"

#ifdef _AUDIO
#include "audio_call.h"
#endif


extern char *DATA_FILE;
extern ToxWindow *prompt;

static int max_friends_index = 0;    /* marks the index of the last friend in friends array */
static int num_selected = 0;
static int num_friends = 0;

extern struct _Winthread Winthread;
extern struct user_settings *user_settings_;

ToxicFriend friends[MAX_FRIENDS_NUM];
static int friendlist_index[MAX_FRIENDS_NUM] = {0};

static struct _pendingDel {
    int num;
    bool active;
    WINDOW *popup;
} pendingdelete;

#define S_WEIGHT 100000

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

static void update_friend_last_online(int32_t num, uint64_t timestamp)
{
    friends[num].last_online.last_on = timestamp;
    friends[num].last_online.tm = *localtime((const time_t*)&timestamp);

    /* if the format changes make sure TIME_STR_SIZE is the correct size */
    const char *t = user_settings_->time == TIME_12 ? "%I:%M %p" : "%H:%M";
    strftime(friends[num].last_online.hour_min_str, TIME_STR_SIZE, t,
             &friends[num].last_online.tm);
}

static void friendlist_onMessage(ToxWindow *self, Tox *m, int32_t num, const char *str, uint16_t len)
{
    if (num >= max_friends_index)
        return;

    if (friends[num].chatwin == -1) {
        if (get_num_active_windows() < MAX_WINDOWS_NUM) {
            friends[num].chatwin = add_window(m, new_chat(m, friends[num].num));
            notify(self, generic_message, NT_NOFOCUS);
        } else {
            char nick[TOX_MAX_NAME_LENGTH];
            get_nick_truncate(m, nick, num);

            char timefrmt[TIME_STR_SIZE];
            get_time_str(timefrmt, sizeof(timefrmt));

            line_info_add(prompt, timefrmt, nick, NULL, str, IN_MSG, 0, 0);

            char *msg = "* Warning: Too many windows are open.";
            line_info_add(prompt, NULL, NULL, NULL, msg, SYS_MSG, 0, RED);
            notify(prompt, error, NT_WNDALERT_1);
        }
    }
}

static void friendlist_onConnectionChange(ToxWindow *self, Tox *m, int32_t num, uint8_t status)
{
    if (num >= max_friends_index)
        return;

    friends[num].online = status;
    update_friend_last_online(num, get_unix_time());
    store_data(m, DATA_FILE);
    sort_friendlist_index();
}

static void friendlist_onNickChange(ToxWindow *self, Tox *m, int32_t num, const char *nick, uint16_t len)
{
    if (len > TOX_MAX_NAME_LENGTH || num >= max_friends_index)
        return;

    char tempname[TOX_MAX_NAME_LENGTH];
    strcpy(tempname, nick);
    len = MIN(len, TOXIC_MAX_NAME_LENGTH - 1);
    tempname[len] = '\0';
    snprintf(friends[num].name, sizeof(friends[num].name), "%s", tempname);
    friends[num].namelength = len;
    sort_friendlist_index();
}

static void friendlist_onStatusChange(ToxWindow *self, Tox *m, int32_t num, uint8_t status)
{
    if (num >= max_friends_index)
        return;

    friends[num].status = status;
}

static void friendlist_onStatusMessageChange(ToxWindow *self, int32_t num, const char *status, uint16_t len)
{
    if (len > TOX_MAX_STATUSMESSAGE_LENGTH || num >= max_friends_index)
        return;

    snprintf(friends[num].statusmsg, sizeof(friends[num].statusmsg), "%s", status);
    friends[num].statusmsg_len = strlen(friends[num].statusmsg);
}

void friendlist_onFriendAdded(ToxWindow *self, Tox *m, int32_t num, bool sort)
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
            friends[i].logging_on = (bool) user_settings_->autolog == AUTOLOG_ON;
            tox_get_client_id(m, num, (uint8_t *) friends[i].pub_key);
            update_friend_last_online(i, tox_get_last_online(m, i));

            char tempname[TOX_MAX_NAME_LENGTH] = {0};
            int len = get_nick_truncate(m, tempname, num);

            if (len == -1 || tempname[0] == '\0') {
                strcpy(friends[i].name, UNKNOWN_NAME);
                friends[i].namelength = strlen(UNKNOWN_NAME);
            } else {    /* Enforce toxic's maximum name length */
                friends[i].namelength = len;
                snprintf(friends[i].name, sizeof(friends[i].name), "%s", tempname);
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

static void friendlist_onFileSendRequest(ToxWindow *self, Tox *m, int32_t num, uint8_t filenum,
        uint64_t filesize, const char *filename, uint16_t filename_len)
{
    if (num >= max_friends_index)
        return;

    if (friends[num].chatwin == -1) {
        if (get_num_active_windows() < MAX_WINDOWS_NUM) {
            friends[num].chatwin = add_window(m, new_chat(m, friends[num].num));
            notify(self, transfer_pending, NT_NOFOCUS);
        } else {
            tox_file_send_control(m, num, 1, filenum, TOX_FILECONTROL_KILL, 0, 0);

            char nick[TOX_MAX_NAME_LENGTH];
            get_nick_truncate(m, nick, num);

            char msg[MAX_STR_SIZE];
            snprintf(msg, sizeof(msg), "* File transfer from %s failed: too many windows are open.", nick);
            line_info_add(prompt, NULL, NULL, NULL, msg, SYS_MSG, 0, RED);
            
            notify(prompt, error, NT_WNDALERT_1);
        }
    }
}

static void friendlist_onGroupInvite(ToxWindow *self, Tox *m, int32_t num, const char *group_pub_key)
{
    if (num >= max_friends_index)
        return;

    if (friends[num].chatwin == -1) {
        if (get_num_active_windows() < MAX_WINDOWS_NUM) {
            friends[num].chatwin = add_window(m, new_chat(m, friends[num].num));
            notify(self, generic_message, NT_NOFOCUS);
            
        } else {
            char nick[TOX_MAX_NAME_LENGTH];
            get_nick_truncate(m, nick, num);

            char msg[MAX_STR_SIZE];
            snprintf(msg, sizeof(msg), "* Group chat invite from %s failed: too many windows are open.", nick);
            line_info_add(prompt, NULL, NULL, NULL, msg, SYS_MSG, 0, RED);
            
            notify(prompt, error, NT_WNDALERT_1);
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

static void delete_friend(Tox *m, int32_t f_num)
{
    tox_del_friend(m, f_num);
    memset(&friends[f_num], 0, sizeof(ToxicFriend));

    int i;

    for (i = max_friends_index; i > 0; --i) {
        if (friends[i - 1].active)
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
static void del_friend_activate(ToxWindow *self, Tox *m, int32_t f_num)
{
    pendingdelete.popup = newwin(3, 22 + TOXIC_MAX_NAME_LENGTH - 1, 8, 8);
    pendingdelete.active = true;
    pendingdelete.num = f_num;
}

/* deactivates delete friend popup and deletes friend if instructed */
static void del_friend_deactivate(ToxWindow *self, Tox *m, wint_t key)
{
    if (key == 'y')
        delete_friend(m, pendingdelete.num);

    delwin(pendingdelete.popup);
    memset(&pendingdelete, 0, sizeof(pendingdelete));
    clear();
    refresh();
}

static void draw_popup(void)
{
    if (!pendingdelete.active)
        return;

    wattron(pendingdelete.popup, A_BOLD);
    box(pendingdelete.popup, ACS_VLINE, ACS_HLINE);
    wattroff(pendingdelete.popup, A_BOLD);

    wmove(pendingdelete.popup, 1, 1);
    wprintw(pendingdelete.popup, "Delete contact ");
    wattron(pendingdelete.popup, A_BOLD);
    wprintw(pendingdelete.popup, "%s", friends[pendingdelete.num].name);
    wattroff(pendingdelete.popup, A_BOLD);
    wprintw(pendingdelete.popup, "? y/n");

    wrefresh(pendingdelete.popup);
}

static void friendlist_onKey(ToxWindow *self, Tox *m, wint_t key, bool ltr)
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

    if (key != ltr) {
        if (key == '\n') {
            /* Jump to chat window if already open */
            if (friends[f].chatwin != -1) {
                set_active_window(friends[f].chatwin);
            } else if (get_num_active_windows() < MAX_WINDOWS_NUM) {
                friends[f].chatwin = add_window(m, new_chat(m, friends[f].num));
                set_active_window(friends[f].chatwin);
            } else {
                char *msg = "* Warning: Too many windows are open.";
                line_info_add(prompt, NULL, NULL, NULL, msg, SYS_MSG, 0, RED);

                notify(prompt, error, NT_WNDALERT_1);
            }
        } else if (key == KEY_DC) {
            del_friend_activate(self, m, f);
        } else {
            select_friend(self, m, key);
        }
    }
}

#define FLIST_OFST 6    /* Accounts for space at top and bottom */

static void friendlist_onDraw(ToxWindow *self, Tox *m)
{
    curs_set(0);
    werase(self->window);
    int x2, y2;
    getmaxyx(self->window, y2, x2);

    uint64_t cur_time = get_unix_time();
    struct tm cur_loc_tm = *localtime((const time_t*)&cur_time);

    bool fix_statuses = x2 != self->x;    /* true if window max x value has changed */

    wattron(self->window, COLOR_PAIR(CYAN));
    wprintw(self->window, " Open a chat window with the");
    wattron(self->window, A_BOLD);
    wprintw(self->window, " Enter ");
    wattroff(self->window, A_BOLD);
    wprintw(self->window, "key. Delete a contact with the");
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

    if ((y2 - FLIST_OFST) <= 0)
        return;

    int selected_num = 0;

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
                selected_num = f;
                f_selected = true;
            } else {
                wprintw(self->window, "   ");
            }

            if (friends[f].online) {
                uint8_t status = friends[f].status;
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

                    case TOX_USERSTATUS_INVALID:
                        colour = MAGENTA;
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
                    char statusmsg[TOX_MAX_STATUSMESSAGE_LENGTH];

                    pthread_mutex_lock(&Winthread.lock);
                    tox_get_status_message(m, friends[f].num, (uint8_t *) statusmsg, TOX_MAX_STATUSMESSAGE_LENGTH);
                    pthread_mutex_unlock(&Winthread.lock);

                    snprintf(friends[f].statusmsg, sizeof(friends[f].statusmsg), "%s", statusmsg);
                    friends[f].statusmsg_len = strlen(friends[f].statusmsg);
                }

                /* Truncate note if it doesn't fit on one line */
                uint16_t maxlen = x2 - getcurx(self->window) - 2;

                if (friends[f].statusmsg_len > maxlen) {
                    friends[f].statusmsg[maxlen - 3] = '\0';
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
                    wattroff(self->window, COLOR_PAIR(BLUE));

                uint64_t last_seen = friends[f].last_online.last_on;

                if (last_seen != 0) {
                    int day_dist = (cur_loc_tm.tm_yday - friends[f].last_online.tm.tm_yday) % 365;
                    const char *hourmin = friends[f].last_online.hour_min_str;

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

    if (num_friends) {
        wmove(self->window, y2 - 1, 1);

        wattron(self->window, A_BOLD);
        wprintw(self->window, "ID: ");
        wattroff(self->window, A_BOLD);

        int i;

        for (i = 0; i < TOX_CLIENT_ID_SIZE; ++i)
            wprintw(self->window, "%02X", friends[selected_num].pub_key[i] & 0xff);
    }

    wrefresh(self->window);
    draw_popup();
}

void disable_chatwin(int32_t f_num)
{
    friends[f_num].chatwin = -1;
}

#ifdef _AUDIO
static void friendlist_onAv(ToxWindow *self, ToxAv *av, int call_index)
{
    int id = toxav_get_peer_id(av, call_index, 0);

    /*id++;*/
    if ( id != ErrorInternal && id >= max_friends_index)
        return;

    Tox *m = toxav_get_tox(av);

    if (friends[id].chatwin == -1) {
        if (get_num_active_windows() < MAX_WINDOWS_NUM) {
            if (toxav_get_call_state(av, call_index) == av_CallStarting) { /* Only open windows when call is incoming */
                friends[id].chatwin = add_window(m, new_chat(m, friends[id].num));
            }            
        } else {
            char nick[TOX_MAX_NAME_LENGTH];
            get_nick_truncate(m, nick, friends[id].num);

            char msg[MAX_STR_SIZE];
            snprintf(msg, sizeof(msg), "Audio action from: %s!", nick);
            line_info_add(prompt, NULL, NULL, NULL, msg, SYS_MSG, 0, 0);

            char *errmsg = "* Warning: Too many windows are open.";
            line_info_add(prompt, NULL, NULL, NULL, errmsg, SYS_MSG, 0, RED);
            
            notify(prompt, error, NT_WNDALERT_1);
        }
    }
}
#endif /* _AUDIO */

ToxWindow new_friendlist(void)
{
    ToxWindow ret;
    memset(&ret, 0, sizeof(ret));

    ret.active = true;
    ret.is_friendlist = true;

    ret.onKey = &friendlist_onKey;
    ret.onDraw = &friendlist_onDraw;
    ret.onFriendAdded = &friendlist_onFriendAdded;
    ret.onMessage = &friendlist_onMessage;
    ret.onConnectionChange = &friendlist_onConnectionChange;
    ret.onAction = &friendlist_onMessage;    /* Action has identical behaviour to message */
    ret.onNickChange = &friendlist_onNickChange;
    ret.onStatusChange = &friendlist_onStatusChange;
    ret.onStatusMessageChange = &friendlist_onStatusMessageChange;
    ret.onFileSendRequest = &friendlist_onFileSendRequest;
    ret.onGroupInvite = &friendlist_onGroupInvite;

#ifdef _AUDIO
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
    
    ret.call_idx = -1;
    ret.device_selection[0] = ret.device_selection[1] = -1;
#endif /* _AUDIO */

#ifdef _SOUND_NOTIFY
    ret.active_sound = -1;
#endif /* _SOUND_NOTIFY */
    
    strcpy(ret.name, "contacts");
    return ret;
}

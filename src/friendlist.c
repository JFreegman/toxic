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
#include <arpa/inet.h>

#include <tox/tox.h>

#include "toxic.h"
#include "windows.h"
#include "chat.h"
#include "friendlist.h"
#include "misc_tools.h"
#include "line_info.h"
#include "settings.h"
#include "notify.h"
#include "help.h"

#ifdef AUDIO
#include "audio_call.h"
#endif


extern char *DATA_FILE;
extern char *BLOCK_FILE;
extern ToxWindow *prompt;
extern struct Winthread Winthread;
extern struct user_settings *user_settings;
extern struct arg_opts arg_opts;

static uint8_t blocklist_view = 0;   /* 0 if we're in friendlist view, 1 if we're in blocklist view */

_Friends Friends;

static struct _Blocked {
    int num_selected;
    int max_idx;
    int num_blocked;

    int *index;
    BlockedFriend *list;
} Blocked;

static struct _pendingDel {
    int num;
    bool active;
    WINDOW *popup;
} pendingdelete;

static void realloc_friends(int n)
{
    if (n <= 0) {
        free(Friends.list);
        free(Friends.index);
        Friends.list = NULL;
        Friends.index = NULL;
        return;
    }

    ToxicFriend *f = realloc(Friends.list, n * sizeof(ToxicFriend));
    int *f_idx = realloc(Friends.index, n * sizeof(int));

    if (f == NULL || f_idx == NULL)
        exit_toxic_err("failed in realloc_friends", FATALERR_MEMORY);

    Friends.list = f;
    Friends.index = f_idx;
}

static void realloc_blocklist(int n)
{
    if (n <= 0) {
        free(Blocked.list);
        free(Blocked.index);
        Blocked.list = NULL;
        Blocked.index = NULL;
        return;
    }

    BlockedFriend *b = realloc(Blocked.list, n * sizeof(BlockedFriend));
    int *b_idx = realloc(Blocked.index, n * sizeof(int));

    if (b == NULL || b_idx == NULL)
        exit_toxic_err("failed in realloc_blocklist", FATALERR_MEMORY);

    Blocked.list = b;
    Blocked.index = b_idx;
}

void kill_friendlist(void)
{
    realloc_blocklist(0);
    realloc_friends(0);
}

static int save_blocklist(char *path)
{
    if (arg_opts.ignore_data_file)
        return 0;

    if (path == NULL)
        return -1;

    int len = sizeof(BlockedFriend) * Blocked.num_blocked;
    char *data = malloc(len);

    if (data == NULL)
        exit_toxic_err("Failed in save_blocklist", FATALERR_MEMORY);

    int i;
    int ret = -1;
    int count = 0;

    for (i = 0; i < Blocked.max_idx; ++i) {
        if (count > Blocked.num_blocked)
            goto on_error;

        if (Blocked.list[i].active) {
            BlockedFriend tmp;
            memset(&tmp, 0, sizeof(BlockedFriend));
            tmp.namelength = htons(Blocked.list[i].namelength);
            memcpy(tmp.name, Blocked.list[i].name, Blocked.list[i].namelength + 1);
            memcpy(tmp.pub_key, Blocked.list[i].pub_key, TOX_CLIENT_ID_SIZE);

            uint8_t lastonline[sizeof(uint64_t)];
            memcpy(lastonline, &Blocked.list[i].last_on, sizeof(uint64_t));
            hst_to_net(lastonline, sizeof(uint64_t));
            memcpy(&tmp.last_on, lastonline, sizeof(uint64_t));

            memcpy(data + count * sizeof(BlockedFriend), &tmp, sizeof(BlockedFriend));
            ++count;
        }
    }

    FILE *fp = fopen(path, "wb");

    if (fp == NULL)
        goto on_error;

    if (fwrite(data, len, 1, fp) == 1)
        ret = 0;

    fclose(fp);

on_error:
    free(data);
    return ret;
}

static void sort_blocklist_index(void);

int load_blocklist(char *path)
{
    if (path == NULL)
        return -1;

    FILE *fp = fopen(path, "rb");

    if (fp == NULL)
        return -1;

    off_t len = file_size(path);

    if (len == -1) {
        fclose(fp);
        return -1;
    }

    char *data = malloc(len);

    if (data == NULL) {            
        fclose(fp);
        exit_toxic_err("Failed in load_blocklist", FATALERR_MEMORY);
    }

    if (fread(data, len, 1, fp) != 1) {
        fclose(fp);
        free(data);
        return -1;
    }

    if (len % sizeof(BlockedFriend) != 0) {
        fclose(fp);
        free(data);
        return -1;
    }

    int num = len / sizeof(BlockedFriend);
    Blocked.max_idx = num;
    realloc_blocklist(num);

    int i;

    for (i = 0; i < num; ++i) {
        memset(&Blocked.list[i], 0, sizeof(BlockedFriend));

        BlockedFriend tmp;
        memcpy(&tmp, data + i * sizeof(BlockedFriend), sizeof(BlockedFriend));
        Blocked.list[i].active = true;
        Blocked.list[i].num = i;
        Blocked.list[i].namelength = ntohs(tmp.namelength);
        memcpy(Blocked.list[i].name, tmp.name, Blocked.list[i].namelength + 1);
        memcpy(Blocked.list[i].pub_key, tmp.pub_key, TOX_CLIENT_ID_SIZE);

        uint8_t lastonline[sizeof(uint64_t)];
        memcpy(lastonline, &tmp.last_on, sizeof(uint64_t));
        net_to_host(lastonline, sizeof(uint64_t));
        memcpy(&Blocked.list[i].last_on, lastonline, sizeof(uint64_t));

        ++Blocked.num_blocked;
    }

    free(data);
    fclose(fp);
    sort_blocklist_index();

    return 0;
}

#define S_WEIGHT 100000
static int index_name_cmp(const void *n1, const void *n2)
{
    int res = qsort_strcasecmp_hlpr(Friends.list[*(int *) n1].name, Friends.list[*(int *) n2].name);

    /* Use weight to make qsort always put online friends before offline */
    res = Friends.list[*(int *) n1].online ? (res - S_WEIGHT) : (res + S_WEIGHT);
    res = Friends.list[*(int *) n2].online ? (res + S_WEIGHT) : (res - S_WEIGHT);

    return res;
}

/* sorts Friends.index first by connection status then alphabetically */
void sort_friendlist_index(void)
{
    int i;
    int n = 0;

    for (i = 0; i < Friends.max_idx; ++i) {
        if (Friends.list[i].active)
            Friends.index[n++] = Friends.list[i].num;
    }

    qsort(Friends.index, Friends.num_friends, sizeof(int), index_name_cmp);
}

static int index_name_cmp_block(const void *n1, const void *n2)
{
    return qsort_strcasecmp_hlpr(Blocked.list[*(int *) n1].name, Blocked.list[*(int *) n2].name);
}

static void sort_blocklist_index(void)
{
    int i;
    int n = 0;

    for (i = 0; i < Blocked.max_idx; ++i) {
        if (Blocked.list[i].active)
            Blocked.index[n++] = Blocked.list[i].num;
    }

    qsort(Blocked.index, Blocked.num_blocked, sizeof(int), index_name_cmp_block);
}

static void update_friend_last_online(int32_t num, uint64_t timestamp)
{
    Friends.list[num].last_online.last_on = timestamp;
    Friends.list[num].last_online.tm = *localtime((const time_t*)&timestamp);

    /* if the format changes make sure TIME_STR_SIZE is the correct size */
    const char *t = user_settings->time == TIME_12 ? "%I:%M %p" : "%H:%M";
    strftime(Friends.list[num].last_online.hour_min_str, TIME_STR_SIZE, t,
             &Friends.list[num].last_online.tm);
}

static void friendlist_onMessage(ToxWindow *self, Tox *m, int32_t num, const char *str, uint16_t len)
{
    if (num >= Friends.max_idx)
        return;

    if (Friends.list[num].chatwin == -1) {
        if (get_num_active_windows() < MAX_WINDOWS_NUM) {
            Friends.list[num].chatwin = add_window(m, new_chat(m, Friends.list[num].num));            
        } else {
            char nick[TOX_MAX_NAME_LENGTH];
            get_nick_truncate(m, nick, num);

            char timefrmt[TIME_STR_SIZE];
            get_time_str(timefrmt, sizeof(timefrmt));

            line_info_add(prompt, timefrmt, nick, NULL, IN_MSG, 0, 0, "%s", str);

            const char *msg = "* Warning: Too many windows are open.";
            line_info_add(prompt, NULL, NULL, NULL, SYS_MSG, 0, RED, msg);
            sound_notify(prompt, error, NT_WNDALERT_1, NULL);
        }
    }
}

static void friendlist_onConnectionChange(ToxWindow *self, Tox *m, int32_t num, uint8_t status)
{
    if (num >= Friends.max_idx)
        return;

    Friends.list[num].online = status;
    update_friend_last_online(num, get_unix_time());
    store_data(m, DATA_FILE);
    sort_friendlist_index();
}

static void friendlist_onNickChange(ToxWindow *self, Tox *m, int32_t num, const char *nick, uint16_t len)
{
    if (len > TOX_MAX_NAME_LENGTH || num >= Friends.max_idx)
        return;

    char tempname[TOX_MAX_NAME_LENGTH];
    strcpy(tempname, nick);
    len = MIN(len, TOXIC_MAX_NAME_LENGTH - 1);
    tempname[len] = '\0';
    snprintf(Friends.list[num].name, sizeof(Friends.list[num].name), "%s", tempname);
    Friends.list[num].namelength = len;
    sort_friendlist_index();
}

static void friendlist_onStatusChange(ToxWindow *self, Tox *m, int32_t num, uint8_t status)
{
    if (num >= Friends.max_idx)
        return;

    Friends.list[num].status = status;
}

static void friendlist_onStatusMessageChange(ToxWindow *self, int32_t num, const char *status, uint16_t len)
{
    if (len > TOX_MAX_STATUSMESSAGE_LENGTH || num >= Friends.max_idx)
        return;

    snprintf(Friends.list[num].statusmsg, sizeof(Friends.list[num].statusmsg), "%s", status);
    Friends.list[num].statusmsg_len = strlen(Friends.list[num].statusmsg);
}

void friendlist_onFriendAdded(ToxWindow *self, Tox *m, int32_t num, bool sort)
{
    if (Friends.max_idx < 0)
        return;


    Friends.num_friends = tox_count_friendlist(m);
    realloc_friends(Friends.max_idx + 1);
    memset(&Friends.list[Friends.max_idx], 0, sizeof(ToxicFriend));

    int i;

    for (i = 0; i <= Friends.max_idx; ++i) {
        if (Friends.list[i].active)
            continue;

        Friends.list[i].num = num;
        Friends.list[i].active = true;
        Friends.list[i].chatwin = -1;
        Friends.list[i].online = false;
        Friends.list[i].status = TOX_USERSTATUS_NONE;
        Friends.list[i].logging_on = (bool) user_settings->autolog == AUTOLOG_ON;
        tox_get_client_id(m, num, (uint8_t *) Friends.list[i].pub_key);
        update_friend_last_online(i, tox_get_last_online(m, i));

        char tempname[TOX_MAX_NAME_LENGTH] = {0};
        int len = get_nick_truncate(m, tempname, num);

        if (len == -1 || tempname[0] == '\0') {
            strcpy(Friends.list[i].name, UNKNOWN_NAME);
            Friends.list[i].namelength = strlen(UNKNOWN_NAME);
        } else {    /* Enforce toxic's maximum name length */
            Friends.list[i].namelength = len;
            snprintf(Friends.list[i].name, sizeof(Friends.list[i].name), "%s", tempname);
        }

        if (i == Friends.max_idx)
            ++Friends.max_idx;

        if (sort)
            sort_friendlist_index();

        return;
    }
}

/* puts blocked friend back in friendlist. fnum is new friend number, bnum is blocked number */
static void friendlist_add_blocked(Tox *m, int32_t fnum, int32_t bnum)
{
    Friends.num_friends = tox_count_friendlist(m);
    realloc_friends(Friends.max_idx + 1);
    memset(&Friends.list[Friends.max_idx], 0, sizeof(ToxicFriend));

    int i;

    for (i = 0; i <= Friends.max_idx; ++i) {
        if (Friends.list[i].active)
            continue;

        Friends.list[i].num = fnum;
        Friends.list[i].active = true;
        Friends.list[i].chatwin = -1;
        Friends.list[i].status = TOX_USERSTATUS_NONE;
        Friends.list[i].logging_on = (bool) user_settings->autolog == AUTOLOG_ON;
        Friends.list[i].namelength = Blocked.list[bnum].namelength;
        update_friend_last_online(i, Blocked.list[bnum].last_on);
        memcpy(Friends.list[i].name, Blocked.list[bnum].name, Friends.list[i].namelength + 1);
        memcpy(Friends.list[i].pub_key, Blocked.list[bnum].pub_key, TOX_CLIENT_ID_SIZE);

        if (i == Friends.max_idx)
            ++Friends.max_idx;

        sort_blocklist_index();
        sort_friendlist_index();

        return;
    }
}

static void friendlist_onFileSendRequest(ToxWindow *self, Tox *m, int32_t num, uint8_t filenum,
        uint64_t filesize, const char *filename, uint16_t filename_len)
{
    if (num >= Friends.max_idx)
        return;

    if (Friends.list[num].chatwin == -1) {
        if (get_num_active_windows() < MAX_WINDOWS_NUM) {
            Friends.list[num].chatwin = add_window(m, new_chat(m, Friends.list[num].num));
        } else {
            tox_file_send_control(m, num, 1, filenum, TOX_FILECONTROL_KILL, 0, 0);

            char nick[TOX_MAX_NAME_LENGTH];
            get_nick_truncate(m, nick, num);

            line_info_add(prompt, NULL, NULL, NULL, SYS_MSG, 0, RED,
                          "* File transfer from %s failed: too many windows are open.", nick);
            
            sound_notify(prompt, error, NT_WNDALERT_1, NULL);
        }
    }
}

static void friendlist_onGroupInvite(ToxWindow *self, Tox *m, int32_t num, const char *group_pub_key)
{
    if (num >= Friends.max_idx)
        return;

    if (Friends.list[num].chatwin == -1) {
        if (get_num_active_windows() < MAX_WINDOWS_NUM) {
            Friends.list[num].chatwin = add_window(m, new_chat(m, Friends.list[num].num));
        } else {
            char nick[TOX_MAX_NAME_LENGTH];
            get_nick_truncate(m, nick, num);
            line_info_add(prompt, NULL, NULL, NULL, SYS_MSG, 0, RED,
                         "* Group chat invite from %s failed: too many windows are open.", nick);
            sound_notify(prompt, error, NT_WNDALERT_1, NULL);
        }
    }
}

/* move friendlist/blocklist cursor up and down */
static void select_friend(ToxWindow *self, wint_t key, int *selected, int num)
{
    if (num <= 0)
        return;

    if (key == KEY_UP) {
        if (--(*selected) < 0)
            *selected = num - 1;
    } else if (key == KEY_DOWN) {
        *selected = (*selected + 1) % num;
    }
}

static void delete_friend(Tox *m, int32_t f_num)
{
    if (Friends.list[f_num].chatwin >= 0) {
        ToxWindow *toxwin = get_window_ptr(Friends.list[f_num].chatwin);

        if (toxwin != NULL) {
            kill_chat_window(toxwin, m);
            set_active_window(1);   /* keep friendlist focused */
        }
    }

    tox_del_friend(m, f_num);
    memset(&Friends.list[f_num], 0, sizeof(ToxicFriend));

    int i;

    for (i = Friends.max_idx; i > 0; --i) {
        if (Friends.list[i - 1].active)
            break;
    }

    Friends.max_idx = i;
    Friends.num_friends = tox_count_friendlist(m);
    realloc_friends(i);

    /* make sure num_selected stays within Friends.num_friends range */
    if (Friends.num_friends && Friends.num_selected == Friends.num_friends)
        --Friends.num_selected;

    store_data(m, DATA_FILE);
}

/* activates delete friend popup */
static void del_friend_activate(ToxWindow *self, Tox *m, int32_t f_num)
{
    pendingdelete.popup = newwin(3, 22 + TOXIC_MAX_NAME_LENGTH - 1, 8, 8);
    pendingdelete.active = true;
    pendingdelete.num = f_num;
}

static void delete_blocked_friend(int32_t bnum);

/* deactivates delete friend popup and deletes friend if instructed */
static void del_friend_deactivate(ToxWindow *self, Tox *m, wint_t key)
{
    if (key == 'y') {
        if (blocklist_view == 0) {
            delete_friend(m, pendingdelete.num);
            sort_friendlist_index();
        } else {
            delete_blocked_friend(pendingdelete.num);
            sort_blocklist_index();
        }
    }

    delwin(pendingdelete.popup);
    memset(&pendingdelete, 0, sizeof(pendingdelete));
    clear();
    refresh();
}

static void draw_del_popup(void)
{
    if (!pendingdelete.active)
        return;

    wattron(pendingdelete.popup, A_BOLD);
    box(pendingdelete.popup, ACS_VLINE, ACS_HLINE);
    wattroff(pendingdelete.popup, A_BOLD);

    wmove(pendingdelete.popup, 1, 1);
    wprintw(pendingdelete.popup, "Delete contact ");
    wattron(pendingdelete.popup, A_BOLD);

    if (blocklist_view == 0)
        wprintw(pendingdelete.popup, "%s", Friends.list[pendingdelete.num].name);
    else
        wprintw(pendingdelete.popup, "%s", Blocked.list[pendingdelete.num].name);

    wattroff(pendingdelete.popup, A_BOLD);
    wprintw(pendingdelete.popup, "? y/n");

    wrefresh(pendingdelete.popup);
}

/* deletes contact from blocked list */
static void delete_blocked_friend(int32_t bnum)
{
    memset(&Blocked.list[bnum], 0, sizeof(BlockedFriend));

    int i;

    for (i = Blocked.max_idx; i > 0; --i) {
        if (Blocked.list[i - 1].active)
            break;
    }

    --Blocked.num_blocked;
    Blocked.max_idx = i;
    realloc_blocklist(i);
    save_blocklist(BLOCK_FILE);

    if (Blocked.num_blocked && Blocked.num_selected == Blocked.num_blocked)
        --Blocked.num_selected;
}

/* deletes contact from friendlist and puts in blocklist */
void block_friend(Tox *m, int32_t fnum)
{
    if (Friends.num_friends <= 0)
        return;

    realloc_blocklist(Blocked.max_idx + 1);    
    memset(&Blocked.list[Blocked.max_idx], 0, sizeof(BlockedFriend));

    int i;

    for (i = 0; i <= Blocked.max_idx; ++i) {
        if (Blocked.list[i].active)
            continue;

        Blocked.list[i].active = true;
        Blocked.list[i].num = i;
        Blocked.list[i].namelength = Friends.list[fnum].namelength;
        Blocked.list[i].last_on = Friends.list[fnum].last_online.last_on;
        memcpy(Blocked.list[i].pub_key, Friends.list[fnum].pub_key, TOX_CLIENT_ID_SIZE);
        memcpy(Blocked.list[i].name, Friends.list[fnum].name, Friends.list[fnum].namelength  + 1);

        ++Blocked.num_blocked;

        if (i == Blocked.max_idx)
            ++Blocked.max_idx;

        delete_friend(m, fnum);
        save_blocklist(BLOCK_FILE);
        sort_blocklist_index();
        sort_friendlist_index();

        return;
    }
}

/* removes friend from blocklist, puts back in friendlist */
static void unblock_friend(Tox *m, int32_t bnum)
{
    if (Blocked.num_blocked <= 0)
        return;

    int32_t friendnum = tox_add_friend_norequest(m, (uint8_t *) Blocked.list[bnum].pub_key);

    if (friendnum == -1) {
        line_info_add(prompt, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to unblock friend");
        return;
    }

    friendlist_add_blocked(m, friendnum, bnum);
    delete_blocked_friend(bnum);
    sort_blocklist_index();
    sort_friendlist_index();
}

static void friendlist_onKey(ToxWindow *self, Tox *m, wint_t key, bool ltr)
{

    if (self->help->active) {
        help_onKey(self, key);
        return;
    }

    if (key == 'h') {
        help_init_menu(self);
        return;
    }

    if (!blocklist_view && !Friends.num_friends && (key != KEY_RIGHT && key != KEY_LEFT))
        return;

    if (blocklist_view && !Blocked.num_blocked && (key != KEY_RIGHT && key != KEY_LEFT))
        return;

    int f = 0;

    if (blocklist_view == 1 && Blocked.num_blocked)
        f = Blocked.index[Blocked.num_selected];
    else if (Friends.num_friends)
        f = Friends.index[Friends.num_selected];

    /* lock screen and force decision on deletion popup */
    if (pendingdelete.active) {
        if (key == 'y' || key == 'n')
            del_friend_deactivate(self, m, key);

        return;
    }

    if (key == ltr)
        return;

    switch (key) {
        case '\n':
            if (blocklist_view)
                break;

            /* Jump to chat window if already open */
            if (Friends.list[f].chatwin != -1) {
                set_active_window(Friends.list[f].chatwin);
            } else if (get_num_active_windows() < MAX_WINDOWS_NUM) {
                Friends.list[f].chatwin = add_window(m, new_chat(m, Friends.list[f].num));
                set_active_window(Friends.list[f].chatwin);
            } else {
                const char *msg = "* Warning: Too many windows are open.";
                line_info_add(prompt, NULL, NULL, NULL, SYS_MSG, 0, RED, msg);
                sound_notify(prompt, error, NT_WNDALERT_1, NULL);
            }

            break;

        case KEY_DC:
            del_friend_activate(self, m, f);
            break;

        case 'b':
            if (!blocklist_view)
                block_friend(m, f);
            else
                unblock_friend(m, f);
            break;

        case KEY_RIGHT:
        case KEY_LEFT:
            blocklist_view ^= 1;
            break;

        default:
            if (blocklist_view == 0)
                select_friend(self, key, &Friends.num_selected, Friends.num_friends);
            else
                select_friend(self, key, &Blocked.num_selected, Blocked.num_blocked);
            break;
    }
}

#define FLIST_OFST 6    /* Accounts for space at top and bottom */

static void blocklist_onDraw(ToxWindow *self, Tox *m, int y2, int x2)
{
    wattron(self->window, A_BOLD);
    wprintw(self->window, " Blocked: ");
    wattroff(self->window, A_BOLD);
    wprintw(self->window, "%d\n\n", Blocked.num_blocked);

    if ((y2 - FLIST_OFST) <= 0)
        return;

    int selected_num = 0;

    /* Determine which portion of friendlist to draw based on current position */
    int page = Blocked.num_selected / (y2 - FLIST_OFST);
    int start = (y2 - FLIST_OFST) * page;
    int end = y2 - FLIST_OFST + start;

    int i;

    for (i = start; i < Blocked.num_blocked && i < end; ++i) {
        int f = Blocked.index[i];
        bool f_selected = false;

        if (i == Blocked.num_selected) {
            wattron(self->window, A_BOLD);
            wprintw(self->window, " > ");
            wattroff(self->window, A_BOLD);
            selected_num = f;
            f_selected = true;
        } else {
            wprintw(self->window, "   ");
        }

        wattron(self->window, COLOR_PAIR(RED));
        wprintw(self->window, "x");
        wattroff(self->window, COLOR_PAIR(RED));

        if (f_selected)
            wattron(self->window, COLOR_PAIR(BLUE));

        wattron(self->window, A_BOLD);
        wprintw(self->window, " %s\n", Blocked.list[f].name);
        wattroff(self->window, A_BOLD);

        if (f_selected)
            wattroff(self->window, COLOR_PAIR(BLUE));
    }

    wprintw(self->window, "\n");
    self->x = x2;

    if (Blocked.num_blocked) {
        wmove(self->window, y2 - 1, 1);

        wattron(self->window, A_BOLD);
        wprintw(self->window, "Key: ");
        wattroff(self->window, A_BOLD);

        int i;

        for (i = 0; i < TOX_CLIENT_ID_SIZE; ++i)
            wprintw(self->window, "%02X", Blocked.list[selected_num].pub_key[i] & 0xff);
    }

    wrefresh(self->window);
    draw_del_popup();

    if (self->help->active)
        help_onDraw(self);
}

static void friendlist_onDraw(ToxWindow *self, Tox *m)
{
    curs_set(0);
    werase(self->window);
    int x2, y2;
    getmaxyx(self->window, y2, x2);

    bool fix_statuses = x2 != self->x;    /* true if window max x value has changed */

    wattron(self->window, COLOR_PAIR(CYAN));
    wprintw(self->window, " Press the");
    wattron(self->window, A_BOLD);
    wprintw(self->window, " h ");
    wattroff(self->window, A_BOLD);
    wprintw(self->window, "key for help\n\n");
    wattroff(self->window, COLOR_PAIR(CYAN));

    if (blocklist_view == 1) {
        blocklist_onDraw(self, m, y2, x2);
        return;
    }

    uint64_t cur_time = get_unix_time();
    struct tm cur_loc_tm = *localtime((const time_t *) &cur_time);

    pthread_mutex_lock(&Winthread.lock);
    int nf = tox_get_num_online_friends(m);
    pthread_mutex_unlock(&Winthread.lock);

    wattron(self->window, A_BOLD);
    wprintw(self->window, " Online: ");
    wattroff(self->window, A_BOLD);
    wprintw(self->window, "%d/%d \n\n", nf, Friends.num_friends);

    if ((y2 - FLIST_OFST) <= 0)
        return;

    int selected_num = 0;

    /* Determine which portion of friendlist to draw based on current position */
    int page = Friends.num_selected / (y2 - FLIST_OFST);
    int start = (y2 - FLIST_OFST) * page;
    int end = y2 - FLIST_OFST + start;

    int i;

    for (i = start; i < Friends.num_friends && i < end; ++i) {
        int f = Friends.index[i];
        bool f_selected = false;

        if (Friends.list[f].active) {
            if (i == Friends.num_selected) {
                wattron(self->window, A_BOLD);
                wprintw(self->window, " > ");
                wattroff(self->window, A_BOLD);
                selected_num = f;
                f_selected = true;
            } else {
                wprintw(self->window, "   ");
            }

            if (Friends.list[f].online) {
                uint8_t status = Friends.list[f].status;
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
                wprintw(self->window, "%s ", ONLINE_CHAR);
                wattroff(self->window, COLOR_PAIR(colour) | A_BOLD);

                if (f_selected)
                    wattron(self->window, COLOR_PAIR(BLUE));

                wattron(self->window, A_BOLD);
                wprintw(self->window, "%s", Friends.list[f].name);
                wattroff(self->window, A_BOLD);

                if (f_selected)
                    wattroff(self->window, COLOR_PAIR(BLUE));

                /* Reset Friends.list[f].statusmsg on window resize */
                if (fix_statuses) {
                    char statusmsg[TOX_MAX_STATUSMESSAGE_LENGTH];

                    pthread_mutex_lock(&Winthread.lock);
                    tox_get_status_message(m, Friends.list[f].num, (uint8_t *) statusmsg, TOX_MAX_STATUSMESSAGE_LENGTH);
                    pthread_mutex_unlock(&Winthread.lock);

                    snprintf(Friends.list[f].statusmsg, sizeof(Friends.list[f].statusmsg), "%s", statusmsg);
                    Friends.list[f].statusmsg_len = strlen(Friends.list[f].statusmsg);
                }

                /* Truncate note if it doesn't fit on one line */
                uint16_t maxlen = x2 - getcurx(self->window) - 2;

                if (Friends.list[f].statusmsg_len > maxlen) {
                    Friends.list[f].statusmsg[maxlen - 3] = '\0';
                    strcat(Friends.list[f].statusmsg, "...");
                    Friends.list[f].statusmsg[maxlen] = '\0';
                    Friends.list[f].statusmsg_len = maxlen;
                }

                if (Friends.list[f].statusmsg[0])
                    wprintw(self->window, " %s", Friends.list[f].statusmsg);

                wprintw(self->window, "\n");
            } else {
                wprintw(self->window, "%s ", OFFLINE_CHAR);

                if (f_selected)
                    wattron(self->window, COLOR_PAIR(BLUE));

                wattron(self->window, A_BOLD);
                wprintw(self->window, "%s", Friends.list[f].name);
                wattroff(self->window, A_BOLD);

                if (f_selected)
                    wattroff(self->window, COLOR_PAIR(BLUE));

                uint64_t last_seen = Friends.list[f].last_online.last_on;

                if (last_seen != 0) {
                    int day_dist = (cur_loc_tm.tm_yday - Friends.list[f].last_online.tm.tm_yday) % 365;
                    const char *hourmin = Friends.list[f].last_online.hour_min_str;

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

    if (Friends.num_friends) {
        wmove(self->window, y2 - 1, 1);

        wattron(self->window, A_BOLD);
        wprintw(self->window, "Key: ");
        wattroff(self->window, A_BOLD);

        int i;

        for (i = 0; i < TOX_CLIENT_ID_SIZE; ++i)
            wprintw(self->window, "%02X", Friends.list[selected_num].pub_key[i] & 0xff);
    }

    wrefresh(self->window);
    draw_del_popup();

    if (self->help->active)
        help_onDraw(self);
}

void disable_chatwin(int32_t f_num)
{
    Friends.list[f_num].chatwin = -1;
}

#ifdef AUDIO
static void friendlist_onAv(ToxWindow *self, ToxAv *av, int call_index)
{
    int id = toxav_get_peer_id(av, call_index, 0);

    if ( id != ErrorInternal && id >= Friends.max_idx)
        return;

    Tox *m = toxav_get_tox(av);

    if (Friends.list[id].chatwin == -1) {
        if (get_num_active_windows() < MAX_WINDOWS_NUM) {
            if (toxav_get_call_state(av, call_index) == av_CallStarting) { /* Only open windows when call is incoming */
                Friends.list[id].chatwin = add_window(m, new_chat(m, Friends.list[id].num));
            }            
        } else {
            char nick[TOX_MAX_NAME_LENGTH];
            get_nick_truncate(m, nick, Friends.list[id].num);
            line_info_add(prompt, NULL, NULL, NULL, SYS_MSG, 0, 0, "Audio action from: %s!", nick);

            const char *errmsg = "* Warning: Too many windows are open.";
            line_info_add(prompt, NULL, NULL, NULL, SYS_MSG, 0, RED, errmsg);
            
            sound_notify(prompt, error, NT_WNDALERT_1, NULL);
        }
    }
}
#endif /* AUDIO */

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

#ifdef AUDIO
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
#endif /* AUDIO */
    
    ret.active_box = -1;

    Help *help = calloc(1, sizeof(Help));

    if (help == NULL)
        exit_toxic_err("failed in new_friendlist", FATALERR_MEMORY);
    
    ret.help = help;
    strcpy(ret.name, "contacts");
    return ret;
}

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

#include <arpa/inet.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <tox/tox.h>

#include "avatars.h"
#include "chat.h"
#include "friendlist.h"
#include "help.h"
#include "line_info.h"
#include "log.h"
#include "misc_tools.h"
#include "notify.h"
#include "settings.h"
#include "toxic.h"
#include "windows.h"

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

FriendsList Friends;

static struct Blocked {
    int num_selected;
    int max_idx;
    int num_blocked;
    uint32_t *index;
    BlockedFriend *list;
} Blocked;

static struct PendingDel {
    uint32_t num;
    bool active;
    WINDOW *popup;
} PendingDelete;

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
    uint32_t *f_idx = realloc(Friends.index, n * sizeof(uint32_t));

    if (f == NULL || f_idx == NULL) {
        exit_toxic_err("failed in realloc_friends", FATALERR_MEMORY);
    }

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
    uint32_t *b_idx = realloc(Blocked.index, n * sizeof(uint32_t));

    if (b == NULL || b_idx == NULL) {
        exit_toxic_err("failed in realloc_blocklist", FATALERR_MEMORY);
    }

    Blocked.list = b;
    Blocked.index = b_idx;
}

void kill_friendlist(ToxWindow *self)
{
    for (size_t i = 0; i < Friends.max_idx; ++i) {
        if (Friends.list[i].active) {
            free(Friends.list[i].conference_invite.key);
#ifdef GAMES
            free(Friends.list[i].game_invite.data);
#endif
        }
    }

    realloc_blocklist(0);
    realloc_friends(0);
    free(self->help);
    del_window(self);
}

static void clear_blocklist_index(size_t idx)
{
    Blocked.list[idx] = (BlockedFriend) {
        0
    };
}

static void clear_friendlist_index(size_t idx)
{
    Friends.list[idx] = (ToxicFriend) {
        0
    };
}

/* Saves the blocklist to path. If there are no items in the blocklist the
 * empty file will be removed.
 *
 * Returns 0 if stored successfully.
 * Returns -1 on failure.
 */
#define TEMP_BLOCKLIST_EXT ".tmp"
static int save_blocklist(char *path)
{
    if (path == NULL) {
        return -1;
    }

    int len = sizeof(BlockedFriend) * Blocked.num_blocked;
    char *data = malloc(len * sizeof(char));

    if (data == NULL) {
        return -1;
    }

    int i, count = 0;

    for (i = 0; i < Blocked.max_idx; ++i) {
        if (count > Blocked.num_blocked) {
            free(data);
            return -1;
        }

        if (Blocked.list[i].active) {
            if (Blocked.list[i].namelength > TOXIC_MAX_NAME_LENGTH) {
                continue;
            }

            BlockedFriend tmp = {0};
            tmp.namelength = htons(Blocked.list[i].namelength);
            memcpy(tmp.name, Blocked.list[i].name, Blocked.list[i].namelength + 1);  // Include null byte
            memcpy(tmp.pub_key, Blocked.list[i].pub_key, TOX_PUBLIC_KEY_SIZE);

            uint8_t lastonline[sizeof(uint64_t)];
            memcpy(lastonline, &Blocked.list[i].last_on, sizeof(uint64_t));
            hst_to_net(lastonline, sizeof(uint64_t));
            memcpy(&tmp.last_on, lastonline, sizeof(uint64_t));

            memcpy(data + count * sizeof(BlockedFriend), &tmp, sizeof(BlockedFriend));
            ++count;
        }
    }

    /* Blocklist is empty, we can remove the empty file */
    if (count == 0) {
        free(data);

        if (remove(path) != 0) {
            return -1;
        }

        return 0;
    }

    size_t temp_buf_size = strlen(path) + strlen(TEMP_BLOCKLIST_EXT) + 1;
    char *temp_path = malloc(temp_buf_size);

    if (temp_path == NULL) {
        free(data);
        return -1;
    }

    snprintf(temp_path, temp_buf_size, "%s%s", path, TEMP_BLOCKLIST_EXT);

    FILE *fp = fopen(temp_path, "wb");

    if (fp == NULL) {
        free(data);
        free(temp_path);
        return -1;
    }

    if (fwrite(data, len, 1, fp) != 1) {
        fprintf(stderr, "Failed to write blocklist data.\n");
        fclose(fp);
        free(data);
        free(temp_path);
        return -1;
    }

    fclose(fp);
    free(data);

    if (rename(temp_path, path) != 0) {
        free(temp_path);
        return -1;
    }

    free(temp_path);

    return 0;
}

static void sort_blocklist_index(void);

int load_blocklist(char *path)
{
    if (path == NULL) {
        return -1;
    }

    FILE *fp = fopen(path, "rb");

    if (fp == NULL) {
        return -1;
    }

    off_t len = file_size(path);

    if (len == 0) {
        fclose(fp);
        return -1;
    }

    char *data = malloc(len);

    if (data == NULL) {
        fclose(fp);
        return -1;
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

    for (int i = 0; i < num; ++i) {
        BlockedFriend tmp = {0};
        clear_blocklist_index(i);

        memcpy(&tmp, data + i * sizeof(BlockedFriend), sizeof(BlockedFriend));
        Blocked.list[i].namelength = ntohs(tmp.namelength);

        if (Blocked.list[i].namelength > TOXIC_MAX_NAME_LENGTH) {
            continue;
        }

        Blocked.list[i].active = true;
        Blocked.list[i].num = i;
        memcpy(Blocked.list[i].name, tmp.name, Blocked.list[i].namelength + 1);   // copy null byte
        memcpy(Blocked.list[i].pub_key, tmp.pub_key, TOX_PUBLIC_KEY_SIZE);

        uint8_t lastonline[sizeof(uint64_t)];
        memcpy(lastonline, &tmp.last_on, sizeof(uint64_t));
        net_to_host(lastonline, sizeof(uint64_t));
        memcpy(&Blocked.list[i].last_on, lastonline, sizeof(uint64_t));

        ++Blocked.num_blocked;
    }

    fclose(fp);
    free(data);

    sort_blocklist_index();

    return 0;
}

#define S_WEIGHT 100000
static int index_name_cmp(const void *n1, const void *n2)
{
    int res = qsort_strcasecmp_hlpr(Friends.list[*(int *) n1].name, Friends.list[*(int *) n2].name);

    /* Use weight to make qsort always put online friends before offline */
    res = Friends.list[*(int *) n1].connection_status ? (res - S_WEIGHT) : (res + S_WEIGHT);
    res = Friends.list[*(int *) n2].connection_status ? (res + S_WEIGHT) : (res - S_WEIGHT);

    return res;
}

/* sorts Friends.index first by connection status then alphabetically */
void sort_friendlist_index(void)
{
    size_t i;
    uint32_t n = 0;

    for (i = 0; i < Friends.max_idx; ++i) {
        if (Friends.list[i].active) {
            Friends.index[n++] = Friends.list[i].num;
        }
    }

    if (Friends.num_friends > 0) {
        qsort(Friends.index, Friends.num_friends, sizeof(uint32_t), index_name_cmp);
    }
}

static int index_name_cmp_block(const void *n1, const void *n2)
{
    return qsort_strcasecmp_hlpr(Blocked.list[*(int *) n1].name, Blocked.list[*(int *) n2].name);
}

static void sort_blocklist_index(void)
{
    size_t i;
    uint32_t n = 0;

    for (i = 0; i < Blocked.max_idx; ++i) {
        if (Blocked.list[i].active) {
            Blocked.index[n++] = Blocked.list[i].num;
        }
    }

    qsort(Blocked.index, Blocked.num_blocked, sizeof(uint32_t), index_name_cmp_block);
}

static void update_friend_last_online(uint32_t num, time_t timestamp)
{
    Friends.list[num].last_online.last_on = timestamp;
    Friends.list[num].last_online.tm = *localtime((const time_t *)&timestamp);

    /* if the format changes make sure TIME_STR_SIZE is the correct size */
    const char *t = user_settings->timestamp_format;
    strftime(Friends.list[num].last_online.hour_min_str, TIME_STR_SIZE, t,
             &Friends.list[num].last_online.tm);
}

static void friendlist_onMessage(ToxWindow *self, Tox *m, uint32_t num, Tox_Message_Type type, const char *str,
                                 size_t length)
{
    UNUSED_VAR(self);
    UNUSED_VAR(type);
    UNUSED_VAR(length);

    if (num >= Friends.max_idx) {
        return;
    }

    if (Friends.list[num].chatwin != -1) {
        return;
    }

    if (get_num_active_windows() < MAX_WINDOWS_NUM) {
        Friends.list[num].chatwin = add_window(m, new_chat(m, Friends.list[num].num));
        return;
    }

    char nick[TOX_MAX_NAME_LENGTH];
    get_nick_truncate(m, nick, num);

    line_info_add(prompt, true, nick, NULL, IN_MSG, 0, 0, "%s", str);
    line_info_add(prompt, false, NULL, NULL, SYS_MSG, 0, RED, "* Warning: Too many windows are open.");
    sound_notify(prompt, notif_error, NT_WNDALERT_1, NULL);
}

static void friendlist_onConnectionChange(ToxWindow *self, Tox *m, uint32_t num, Tox_Connection connection_status)
{
    UNUSED_VAR(self);

    if (num >= Friends.max_idx) {
        return;
    }

    if (connection_status == TOX_CONNECTION_NONE) {
        --Friends.num_online;
    } else if (Friends.list[num].connection_status == TOX_CONNECTION_NONE) {
        ++Friends.num_online;

        if (avatar_send(m, num) == -1) {
            fprintf(stderr, "avatar_send failed for friend %u\n", num);
        }
    }

    Friends.list[num].connection_status = connection_status;
    update_friend_last_online(num, get_unix_time());
    store_data(m, DATA_FILE);
    sort_friendlist_index();
}

static void friendlist_onNickChange(ToxWindow *self, Tox *m, uint32_t num, const char *nick, size_t length)
{
    UNUSED_VAR(self);
    UNUSED_VAR(length);

    if (num >= Friends.max_idx) {
        return;
    }

    /* save old name for log renaming */
    char oldname[TOXIC_MAX_NAME_LENGTH + 1];
    snprintf(oldname, sizeof(oldname), "%s", Friends.list[num].name);

    /* update name */
    snprintf(Friends.list[num].name, sizeof(Friends.list[num].name), "%s", nick);
    Friends.list[num].namelength = strlen(Friends.list[num].name);

    /* get data for chatlog renaming */
    char newnamecpy[TOXIC_MAX_NAME_LENGTH + 1];
    char myid[TOX_ADDRESS_SIZE];
    strcpy(newnamecpy, Friends.list[num].name);
    tox_self_get_address(m, (uint8_t *) myid);

    if (strcmp(oldname, newnamecpy) != 0) {
        if (rename_logfile(oldname, newnamecpy, myid, Friends.list[num].pub_key, Friends.list[num].chatwin) != 0) {
            fprintf(stderr, "Failed to rename friend chat log from `%s` to `%s`\n", oldname, newnamecpy);
        }
    }

    sort_friendlist_index();
}

static void friendlist_onStatusChange(ToxWindow *self, Tox *m, uint32_t num, Tox_User_Status status)
{
    UNUSED_VAR(self);
    UNUSED_VAR(m);

    if (num >= Friends.max_idx) {
        return;
    }

    Friends.list[num].status = status;
}

static void friendlist_onStatusMessageChange(ToxWindow *self, uint32_t num, const char *note, size_t length)
{
    UNUSED_VAR(self);

    if (length > TOX_MAX_STATUS_MESSAGE_LENGTH || num >= Friends.max_idx) {
        return;
    }

    snprintf(Friends.list[num].statusmsg, sizeof(Friends.list[num].statusmsg), "%s", note);
    Friends.list[num].statusmsg_len = strlen(Friends.list[num].statusmsg);
}

void friendlist_onFriendAdded(ToxWindow *self, Tox *m, uint32_t num, bool sort)
{
    UNUSED_VAR(self);

    realloc_friends(Friends.max_idx + 1);
    clear_friendlist_index(Friends.max_idx);

    uint32_t i;

    for (i = 0; i <= Friends.max_idx; ++i) {
        if (Friends.list[i].active) {
            continue;
        }

        ++Friends.num_friends;

        Friends.list[i].num = num;
        Friends.list[i].active = true;
        Friends.list[i].chatwin = -1;
        Friends.list[i].connection_status = TOX_CONNECTION_NONE;
        Friends.list[i].status = TOX_USER_STATUS_NONE;
        Friends.list[i].logging_on = (bool) user_settings->autolog == AUTOLOG_ON;

        Tox_Err_Friend_Get_Public_Key pkerr;
        tox_friend_get_public_key(m, num, (uint8_t *) Friends.list[i].pub_key, &pkerr);

        if (pkerr != TOX_ERR_FRIEND_GET_PUBLIC_KEY_OK) {
            fprintf(stderr, "tox_friend_get_public_key failed (error %d)\n", pkerr);
        }

        Tox_Err_Friend_Get_Last_Online loerr;
        time_t t = tox_friend_get_last_online(m, num, &loerr);

        if (loerr != TOX_ERR_FRIEND_GET_LAST_ONLINE_OK) {
            t = 0;
        }

        update_friend_last_online(i, t);

        char tempname[TOX_MAX_NAME_LENGTH + 1];
        int name_len = get_nick_truncate(m, tempname, num);
        memcpy(Friends.list[i].name, tempname, name_len);
        Friends.list[i].name[name_len] = 0;
        Friends.list[i].namelength = name_len;

        if (i == Friends.max_idx) {
            ++Friends.max_idx;
        }

        if (sort) {
            sort_friendlist_index();
        }

#ifdef AUDIO
        init_friend_AV(i);
#endif

        return;
    }
}

/* Puts blocked friend back in friendlist. fnum is new friend number, bnum is blocked number. */
static void friendlist_add_blocked(uint32_t fnum, uint32_t bnum)
{
    realloc_friends(Friends.max_idx + 1);
    clear_friendlist_index(Friends.max_idx);

    int i;

    for (i = 0; i <= Friends.max_idx; ++i) {
        if (Friends.list[i].active) {
            continue;
        }

        ++Friends.num_friends;

        Friends.list[i].num = fnum;
        Friends.list[i].active = true;
        Friends.list[i].chatwin = -1;
        Friends.list[i].status = TOX_USER_STATUS_NONE;
        Friends.list[i].logging_on = (bool) user_settings->autolog == AUTOLOG_ON;
        Friends.list[i].namelength = Blocked.list[bnum].namelength;
        update_friend_last_online(i, Blocked.list[bnum].last_on);
        memcpy(Friends.list[i].name, Blocked.list[bnum].name, Friends.list[i].namelength + 1);
        memcpy(Friends.list[i].pub_key, Blocked.list[bnum].pub_key, TOX_PUBLIC_KEY_SIZE);

        if (i == Friends.max_idx) {
            ++Friends.max_idx;
        }

        sort_blocklist_index();
        sort_friendlist_index();

#ifdef AUDIO
        init_friend_AV(i);
#endif
        return;
    }
}

#ifdef GAMES

static void friendlist_onGameInvite(ToxWindow *self, Tox *m, uint32_t friend_number, const uint8_t *data, size_t length)
{
    UNUSED_VAR(self);
    UNUSED_VAR(data);
    UNUSED_VAR(length);

    if (friend_number >= Friends.max_idx) {
        return;
    }

    if (Friends.list[friend_number].chatwin != -1) {
        return;
    }

    if (get_num_active_windows() < MAX_WINDOWS_NUM) {
        Friends.list[friend_number].chatwin = add_window(m, new_chat(m, Friends.list[friend_number].num));
        return;
    }

    char nick[TOX_MAX_NAME_LENGTH];
    get_nick_truncate(m, nick, friend_number);

    line_info_add(prompt, false, NULL, NULL, SYS_MSG, 0, RED,
                  "* Game invite from %s failed: Too many windows are open.", nick);

    sound_notify(prompt, notif_error, NT_WNDALERT_1, NULL);
}

#endif // GAMES

static void friendlist_onFileRecv(ToxWindow *self, Tox *m, uint32_t num, uint32_t filenum,
                                  uint64_t file_size, const char *filename, size_t name_length)
{
    UNUSED_VAR(self);
    UNUSED_VAR(file_size);
    UNUSED_VAR(filename);
    UNUSED_VAR(name_length);

    if (num >= Friends.max_idx) {
        return;
    }

    if (Friends.list[num].chatwin != -1) {
        return;
    }

    if (get_num_active_windows() < MAX_WINDOWS_NUM) {
        Friends.list[num].chatwin = add_window(m, new_chat(m, Friends.list[num].num));
        return;
    }

    tox_file_control(m, num, filenum, TOX_FILE_CONTROL_CANCEL, NULL);

    char nick[TOX_MAX_NAME_LENGTH];
    get_nick_truncate(m, nick, num);

    line_info_add(prompt, false, NULL, NULL, SYS_MSG, 0, RED,
                  "* File transfer from %s failed: too many windows are open.", nick);

    sound_notify(prompt, notif_error, NT_WNDALERT_1, NULL);
}

static void friendlist_onConferenceInvite(ToxWindow *self, Tox *m, int32_t num, uint8_t type,
        const char *conference_pub_key,
        uint16_t length)
{
    UNUSED_VAR(self);
    UNUSED_VAR(type);
    UNUSED_VAR(conference_pub_key);
    UNUSED_VAR(length);

    if (num >= Friends.max_idx) {
        return;
    }

    if (Friends.list[num].chatwin != -1) {
        return;
    }

    if (get_num_active_windows() < MAX_WINDOWS_NUM) {
        Friends.list[num].chatwin = add_window(m, new_chat(m, Friends.list[num].num));
        return;
    }

    char nick[TOX_MAX_NAME_LENGTH];
    get_nick_truncate(m, nick, num);

    line_info_add(prompt, false, NULL, NULL, SYS_MSG, 0, RED,
                  "* Conference chat invite from %s failed: too many windows are open.", nick);

    sound_notify(prompt, notif_error, NT_WNDALERT_1, NULL);
}

/* move friendlist/blocklist cursor up and down */
static void select_friend(wint_t key, int *selected, int num)
{
    if (num <= 0) {
        return;
    }

    if (key == KEY_UP) {
        if (--(*selected) < 0) {
            *selected = num - 1;
        }
    } else if (key == KEY_DOWN) {
        *selected = (*selected + 1) % num;
    }
}

static void delete_friend(Tox *m, uint32_t f_num)
{
    Tox_Err_Friend_Delete err;

    if (tox_friend_delete(m, f_num, &err) != true) {
        fprintf(stderr, "tox_friend_delete failed with error %d\n", err);
        return;
    }

    --Friends.num_friends;

    if (Friends.list[f_num].connection_status != TOX_CONNECTION_NONE) {
        --Friends.num_online;
    }

    /* close friend's chatwindow if it's currently open */
    if (Friends.list[f_num].chatwin >= 0) {
        ToxWindow *toxwin = get_window_ptr(Friends.list[f_num].chatwin);

        if (toxwin != NULL) {
            kill_chat_window(toxwin, m);
            set_active_window_index(1);   /* keep friendlist focused */
        }
    }

    if (Friends.list[f_num].conference_invite.key != NULL) {
        free(Friends.list[f_num].conference_invite.key);
    }

    clear_friendlist_index(f_num);

    int i;

    for (i = Friends.max_idx; i > 0; --i) {
        if (Friends.list[i - 1].active) {
            break;
        }
    }

    Friends.max_idx = i;
    realloc_friends(i);

#ifdef AUDIO
    del_friend_AV(i);
#endif

    /* make sure num_selected stays within Friends.num_friends range */
    if (Friends.num_friends && Friends.num_selected == Friends.num_friends) {
        --Friends.num_selected;
    }

    store_data(m, DATA_FILE);
}

/* activates delete friend popup */
static void del_friend_activate(uint32_t f_num)
{
    PendingDelete.popup = newwin(3, 22 + TOXIC_MAX_NAME_LENGTH, 8, 8);
    PendingDelete.active = true;
    PendingDelete.num = f_num;
}

static void delete_blocked_friend(uint32_t bnum);

/* deactivates delete friend popup and deletes friend if instructed */
static void del_friend_deactivate(Tox *m, wint_t key)
{
    if (key == L'y') {
        if (blocklist_view == 0) {
            delete_friend(m, PendingDelete.num);
            sort_friendlist_index();
        } else {
            delete_blocked_friend(PendingDelete.num);
            sort_blocklist_index();
        }
    }

    delwin(PendingDelete.popup);

    PendingDelete = (struct PendingDel) {
        0
    };

    clear();
    refresh();
}

static void draw_del_popup(void)
{
    if (!PendingDelete.active) {
        return;
    }

    wattron(PendingDelete.popup, A_BOLD);
    box(PendingDelete.popup, ACS_VLINE, ACS_HLINE);
    wattroff(PendingDelete.popup, A_BOLD);

    wmove(PendingDelete.popup, 1, 1);
    wprintw(PendingDelete.popup, "Delete contact ");
    wattron(PendingDelete.popup, A_BOLD);

    pthread_mutex_lock(&Winthread.lock);

    if (blocklist_view == 0) {
        wprintw(PendingDelete.popup, "%s", Friends.list[PendingDelete.num].name);
    } else {
        wprintw(PendingDelete.popup, "%s", Blocked.list[PendingDelete.num].name);
    }

    pthread_mutex_unlock(&Winthread.lock);

    wattroff(PendingDelete.popup, A_BOLD);
    wprintw(PendingDelete.popup, "? y/n");

    wnoutrefresh(PendingDelete.popup);
}

/* deletes contact from blocked list */
static void delete_blocked_friend(uint32_t bnum)
{
    clear_blocklist_index(bnum);

    int i;

    for (i = Blocked.max_idx; i > 0; --i) {
        if (Blocked.list[i - 1].active) {
            break;
        }
    }

    --Blocked.num_blocked;
    Blocked.max_idx = i;
    realloc_blocklist(i);
    save_blocklist(BLOCK_FILE);

    if (Blocked.num_blocked && Blocked.num_selected == Blocked.num_blocked) {
        --Blocked.num_selected;
    }
}

/* deletes contact from friendlist and puts in blocklist */
void block_friend(Tox *m, uint32_t fnum)
{
    if (Friends.num_friends == 0) {
        return;
    }

    realloc_blocklist(Blocked.max_idx + 1);
    clear_blocklist_index(Blocked.max_idx);

    int i;

    for (i = 0; i <= Blocked.max_idx; ++i) {
        if (Blocked.list[i].active) {
            continue;
        }

        Blocked.list[i].active = true;
        Blocked.list[i].num = i;
        Blocked.list[i].namelength = Friends.list[fnum].namelength;
        Blocked.list[i].last_on = Friends.list[fnum].last_online.last_on;
        memcpy(Blocked.list[i].pub_key, Friends.list[fnum].pub_key, TOX_PUBLIC_KEY_SIZE);
        memcpy(Blocked.list[i].name, Friends.list[fnum].name, Friends.list[fnum].namelength + 1);

        ++Blocked.num_blocked;

        if (i == Blocked.max_idx) {
            ++Blocked.max_idx;
        }

        delete_friend(m, fnum);
        save_blocklist(BLOCK_FILE);
        sort_blocklist_index();
        sort_friendlist_index();

        return;
    }
}

/* removes friend from blocklist, puts back in friendlist */
static void unblock_friend(Tox *m, uint32_t bnum)
{
    if (Blocked.num_blocked <= 0) {
        return;
    }

    Tox_Err_Friend_Add err;
    uint32_t friendnum = tox_friend_add_norequest(m, (uint8_t *) Blocked.list[bnum].pub_key, &err);

    if (err != TOX_ERR_FRIEND_ADD_OK) {
        line_info_add(prompt, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to unblock friend (error %d)", err);
        return;
    }

    friendlist_add_blocked(friendnum, bnum);
    delete_blocked_friend(bnum);
    sort_blocklist_index();
    sort_friendlist_index();
}

/*
 * Return true if input is recognized by handler
 */
static bool friendlist_onKey(ToxWindow *self, Tox *m, wint_t key, bool ltr)
{

    if (self->help->active) {
        help_onKey(self, key);
        return true;
    }

    if (key == L'h') {
        help_init_menu(self);
        return true;
    }

    if (!blocklist_view && !Friends.num_friends && (key != KEY_RIGHT && key != KEY_LEFT)) {
        return true;
    }

    if (blocklist_view && !Blocked.num_blocked && (key != KEY_RIGHT && key != KEY_LEFT)) {
        return true;
    }

    int f = 0;

    if (blocklist_view == 1 && Blocked.num_blocked) {
        f = Blocked.index[Blocked.num_selected];
    } else if (Friends.num_friends) {
        f = Friends.index[Friends.num_selected];
    }

    /* lock screen and force decision on deletion popup */
    if (PendingDelete.active) {
        if (key == L'y' || key == L'n') {
            del_friend_deactivate(m, key);
        }

        return true;
    }

    if (key == ltr) {
        return true;
    }

    switch (key) {
        case L'\r':
            if (blocklist_view) {
                break;
            }

            /* Jump to chat window if already open */
            if (Friends.list[f].chatwin != -1) {
                set_active_window_index(Friends.list[f].chatwin);
            } else if (get_num_active_windows() < MAX_WINDOWS_NUM) {
                Friends.list[f].chatwin = add_window(m, new_chat(m, Friends.list[f].num));
                set_active_window_index(Friends.list[f].chatwin);
            } else {
                const char *msg = "* Warning: Too many windows are open.";
                line_info_add(prompt, false, NULL, NULL, SYS_MSG, 0, RED, msg);
                sound_notify(prompt, notif_error, NT_WNDALERT_1, NULL);
            }

            break;

        case KEY_DC:
            del_friend_activate(f);
            break;

        case L'b':
            if (!blocklist_view) {
                block_friend(m, f);
            } else {
                unblock_friend(m, f);
            }

            break;

        case KEY_RIGHT:
        case KEY_LEFT:
            blocklist_view ^= 1;
            break;

        default:
            if (blocklist_view == 0) {
                select_friend(key, &Friends.num_selected, Friends.num_friends);
            } else {
                select_friend(key, &Blocked.num_selected, Blocked.num_blocked);
            }

            break;
    }

    return true;
}

#define FLIST_OFST 6    /* Accounts for space at top and bottom */

static void blocklist_onDraw(ToxWindow *self, Tox *m, int y2, int x2)
{
    UNUSED_VAR(m);

    wattron(self->window, A_BOLD);
    wprintw(self->window, " Blocked: ");
    wattroff(self->window, A_BOLD);
    wprintw(self->window, "%d\n\n", Blocked.num_blocked);

    if ((y2 - FLIST_OFST) <= 0) {
        return;
    }

    uint32_t selected_num = 0;

    /* Determine which portion of friendlist to draw based on current position */
    int page = Blocked.num_selected / (y2 - FLIST_OFST);
    int start = (y2 - FLIST_OFST) * page;
    int end = y2 - FLIST_OFST + start;

    int i;

    for (i = start; i < Blocked.num_blocked && i < end; ++i) {
        uint32_t f = Blocked.index[i];
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

        if (f_selected) {
            wattron(self->window, COLOR_PAIR(BLUE));
        }

        wattron(self->window, A_BOLD);
        wprintw(self->window, " %s\n", Blocked.list[f].name);
        wattroff(self->window, A_BOLD);

        if (f_selected) {
            wattroff(self->window, COLOR_PAIR(BLUE));
        }
    }

    wprintw(self->window, "\n");
    self->x = x2;

    if (Blocked.num_blocked) {
        wmove(self->window, y2 - 1, 1);

        wattron(self->window, A_BOLD);
        wprintw(self->window, "Public key: ");
        wattroff(self->window, A_BOLD);

        int i;

        for (i = 0; i < TOX_PUBLIC_KEY_SIZE; ++i) {
            wprintw(self->window, "%02X", Blocked.list[selected_num].pub_key[i] & 0xff);
        }
    }

    wnoutrefresh(self->window);
    draw_del_popup();

    if (self->help->active) {
        help_onDraw(self);
    }
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

    draw_window_bar(self);

    if (blocklist_view == 1) {
        blocklist_onDraw(self, m, y2, x2);
        return;
    }

    time_t cur_time = get_unix_time();
    struct tm cur_loc_tm = *localtime((const time_t *) &cur_time);

    wattron(self->window, A_BOLD);
    wprintw(self->window, " Online: ");
    wattroff(self->window, A_BOLD);

    wprintw(self->window, "%zu/%zu \n\n", Friends.num_online, Friends.num_friends);

    if ((y2 - FLIST_OFST) <= 0) {
        return;
    }

    uint32_t selected_num = 0;

    /* Determine which portion of friendlist to draw based on current position */
    pthread_mutex_lock(&Winthread.lock);
    int page = Friends.num_selected / (y2 - FLIST_OFST);
    pthread_mutex_unlock(&Winthread.lock);

    int start = (y2 - FLIST_OFST) * page;
    int end = y2 - FLIST_OFST + start;

    pthread_mutex_lock(&Winthread.lock);
    size_t num_friends = Friends.num_friends;
    pthread_mutex_unlock(&Winthread.lock);

    int i;

    for (i = start; i < num_friends && i < end; ++i) {
        pthread_mutex_lock(&Winthread.lock);
        uint32_t f = Friends.index[i];
        bool is_active = Friends.list[f].active;
        int num_selected = Friends.num_selected;
        pthread_mutex_unlock(&Winthread.lock);

        if (is_active) {
            bool f_selected = false;

            if (i == num_selected) {
                wattron(self->window, A_BOLD);
                wprintw(self->window, " > ");
                wattroff(self->window, A_BOLD);
                selected_num = f;
                f_selected = true;
            } else {
                wprintw(self->window, "   ");
            }

            pthread_mutex_lock(&Winthread.lock);
            Tox_Connection connection_status = Friends.list[f].connection_status;
            Tox_User_Status status = Friends.list[f].status;
            pthread_mutex_unlock(&Winthread.lock);

            if (connection_status != TOX_CONNECTION_NONE) {
                int colour = MAGENTA;

                switch (status) {
                    case TOX_USER_STATUS_NONE:
                        colour = GREEN;
                        break;

                    case TOX_USER_STATUS_AWAY:
                        colour = YELLOW;
                        break;

                    case TOX_USER_STATUS_BUSY:
                        colour = RED;
                        break;
                }

                wattron(self->window, COLOR_PAIR(colour) | A_BOLD);
                wprintw(self->window, "%s ", ONLINE_CHAR);
                wattroff(self->window, COLOR_PAIR(colour) | A_BOLD);

                if (f_selected) {
                    wattron(self->window, COLOR_PAIR(BLUE));
                }

                wattron(self->window, A_BOLD);
                pthread_mutex_lock(&Winthread.lock);
                wprintw(self->window, "%s", Friends.list[f].name);
                pthread_mutex_unlock(&Winthread.lock);
                wattroff(self->window, A_BOLD);

                if (f_selected) {
                    wattroff(self->window, COLOR_PAIR(BLUE));
                }

                /* Reset Friends.list[f].statusmsg on window resize */
                if (fix_statuses) {
                    char statusmsg[TOX_MAX_STATUS_MESSAGE_LENGTH];

                    pthread_mutex_lock(&Winthread.lock);
                    tox_friend_get_status_message(m, Friends.list[f].num, (uint8_t *) statusmsg, NULL);
                    size_t s_len = tox_friend_get_status_message_size(m, Friends.list[f].num, NULL);
                    pthread_mutex_unlock(&Winthread.lock);

                    statusmsg[s_len] = '\0';

                    filter_str(statusmsg, s_len);

                    pthread_mutex_lock(&Winthread.lock);
                    snprintf(Friends.list[f].statusmsg, sizeof(Friends.list[f].statusmsg), "%s", statusmsg);
                    Friends.list[f].statusmsg_len = strlen(Friends.list[f].statusmsg);
                    pthread_mutex_unlock(&Winthread.lock);
                }

                /* Truncate note if it doesn't fit on one line */
                size_t maxlen = x2 - getcurx(self->window) - 2;

                pthread_mutex_lock(&Winthread.lock);

                if (Friends.list[f].statusmsg_len > maxlen) {
                    Friends.list[f].statusmsg[maxlen - 3] = '\0';
                    strcat(Friends.list[f].statusmsg, "...");
                    Friends.list[f].statusmsg[maxlen] = '\0';
                    Friends.list[f].statusmsg_len = maxlen;
                }

                if (Friends.list[f].statusmsg_len > 0) {
                    wprintw(self->window, " %s", Friends.list[f].statusmsg);
                }

                pthread_mutex_unlock(&Winthread.lock);

                wprintw(self->window, "\n");
            } else {
                wprintw(self->window, "%s ", OFFLINE_CHAR);

                if (f_selected) {
                    wattron(self->window, COLOR_PAIR(BLUE));
                }

                wattron(self->window, A_BOLD);
                pthread_mutex_lock(&Winthread.lock);
                wprintw(self->window, "%s", Friends.list[f].name);
                pthread_mutex_unlock(&Winthread.lock);
                wattroff(self->window, A_BOLD);

                if (f_selected) {
                    wattroff(self->window, COLOR_PAIR(BLUE));
                }

                pthread_mutex_lock(&Winthread.lock);
                time_t last_seen = Friends.list[f].last_online.last_on;
                pthread_mutex_unlock(&Winthread.lock);

                if (last_seen != 0) {
                    pthread_mutex_lock(&Winthread.lock);

                    int day_dist = (
                                       cur_loc_tm.tm_yday - Friends.list[f].last_online.tm.tm_yday
                                       + ((cur_loc_tm.tm_year - Friends.list[f].last_online.tm.tm_year) * 365)
                                   );
                    const char *hourmin = Friends.list[f].last_online.hour_min_str;

                    pthread_mutex_unlock(&Winthread.lock);

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
        wprintw(self->window, "Public key: ");
        wattroff(self->window, A_BOLD);

        int i;

        for (i = 0; i < TOX_PUBLIC_KEY_SIZE; ++i) {
            wprintw(self->window, "%02X", Friends.list[selected_num].pub_key[i] & 0xff);
        }
    }

    wnoutrefresh(self->window);
    draw_del_popup();

    if (self->help->active) {
        help_onDraw(self);
    }
}

void friendlist_onInit(ToxWindow *self, Tox *m)
{
    UNUSED_VAR(m);

    int x2;
    int y2;
    getmaxyx(self->window, y2, x2);

    if (y2 <= 0 || x2 <= 0) {
        exit_toxic_err("failed in friendlist_onInit", FATALERR_CURSES);
    }

    self->window_bar = subwin(self->window, WINDOW_BAR_HEIGHT, x2, y2 - 2, 0);
}

void disable_chatwin(uint32_t f_num)
{
    Friends.list[f_num].chatwin = -1;
}

#ifdef AUDIO
static void friendlist_onAV(ToxWindow *self, ToxAV *av, uint32_t friend_number, int state)
{
    UNUSED_VAR(self);

    if (friend_number >= Friends.max_idx) {
        return;
    }

    Tox *m = toxav_get_tox(av);

    if (Friends.list[friend_number].chatwin == -1) {
        if (get_num_active_windows() < MAX_WINDOWS_NUM) {
            if (state != TOXAV_FRIEND_CALL_STATE_FINISHED) {
                Friends.list[friend_number].chatwin = add_window(m, new_chat(m, Friends.list[friend_number].num));
                set_active_window_index(Friends.list[friend_number].chatwin);
            }
        } else {
            char nick[TOX_MAX_NAME_LENGTH];
            get_nick_truncate(m, nick, Friends.list[friend_number].num);
            line_info_add(prompt, false, NULL, NULL, SYS_MSG, 0, 0, "Audio action from: %s!", nick);

            const char *errmsg = "* Warning: Too many windows are open.";
            line_info_add(prompt, false, NULL, NULL, SYS_MSG, 0, RED, errmsg);

            sound_notify(prompt, notif_error, NT_WNDALERT_1, NULL);
        }
    }
}
#endif /* AUDIO */

/* Returns a friend's status */
Tox_User_Status get_friend_status(uint32_t friendnumber)
{
    return Friends.list[friendnumber].status;
}

/* Returns a friend's connection status */
Tox_Connection get_friend_connection_status(uint32_t friendnumber)
{
    return Friends.list[friendnumber].connection_status;
}

/*
 * Returns true if friend associated with `public_key` is in the block list.
 *
 * `public_key` must be at least TOX_PUBLIC_KEY_SIZE bytes.
 */
bool friend_is_blocked(const char *public_key)
{
    for (size_t i = 0; i < Blocked.max_idx; ++i) {
        if (!Blocked.list[i].active) {
            continue;
        }

        if (memcmp(public_key, Blocked.list[i].pub_key, TOX_PUBLIC_KEY_SIZE) == 0) {
            return true;
        }
    }

    return false;
}

ToxWindow *new_friendlist(void)
{
    ToxWindow *ret = calloc(1, sizeof(ToxWindow));

    if (ret == NULL) {
        exit_toxic_err("failed in new_friendlist", FATALERR_MEMORY);
    }

    ret->type = WINDOW_TYPE_FRIEND_LIST;

    ret->onInit = &friendlist_onInit;
    ret->onKey = &friendlist_onKey;
    ret->onDraw = &friendlist_onDraw;
    ret->onFriendAdded = &friendlist_onFriendAdded;
    ret->onMessage = &friendlist_onMessage;
    ret->onConnectionChange = &friendlist_onConnectionChange;
    ret->onNickChange = &friendlist_onNickChange;
    ret->onStatusChange = &friendlist_onStatusChange;
    ret->onStatusMessageChange = &friendlist_onStatusMessageChange;
    ret->onFileRecv = &friendlist_onFileRecv;
    ret->onConferenceInvite = &friendlist_onConferenceInvite;

#ifdef AUDIO
    ret->onInvite = &friendlist_onAV;
    ret->onRinging = &friendlist_onAV;
    ret->onStarting = &friendlist_onAV;
    ret->onEnding = &friendlist_onAV;
    ret->onError = &friendlist_onAV;
    ret->onStart = &friendlist_onAV;
    ret->onCancel = &friendlist_onAV;
    ret->onReject = &friendlist_onAV;
    ret->onEnd = &friendlist_onAV;

    ret->is_call = false;
#endif /* AUDIO */

#ifdef GAMES
    ret->onGameInvite = &friendlist_onGameInvite;
#endif

    ret->num = -1;
    ret->active_box = -1;

    Help *help = calloc(1, sizeof(Help));

    if (help == NULL) {
        exit_toxic_err("failed in new_friendlist", FATALERR_MEMORY);
    }

    ret->help = help;
    strcpy(ret->name, "Contacts");
    return ret;
}

/*  friendlist.c
 *
 *  Copyright (C) 2014-2026 Toxic All Rights Reserved.
 *
 *  This file is part of Toxic. Toxic is free software licensed
 *  under the GNU General Public License 3.0.
 */

#include <arpa/inet.h>
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
#include "prompt.h"
#include "settings.h"
#include "toxic.h"
#include "windows.h"

#ifdef AUDIO
#include "audio_call.h"
#endif

struct BlockedList {
    int num_selected;
    int max_idx;
    int num_blocked;
    uint32_t *index;
    BlockedFriend *list;
};

void init_friendlist(Toxic *toxic)
{
    toxic->friends = calloc(1, sizeof(FriendsList));

    if (toxic->friends == NULL) {
        exit_toxic_err(FATALERR_MEMORY, "failed in init_friendlist");
    }

    toxic->blocked = calloc(1, sizeof(BlockedList));

    if (toxic->blocked == NULL) {
        exit_toxic_err(FATALERR_MEMORY, "failed in init_friendlist");
    }
}

static struct PendingDel {
    uint32_t num;
    bool active;
    WINDOW *popup;
} PendingDelete;

typedef enum Default_Conf {
    Default_Conf_Auto_Accept_Files = 0,
    Default_Conf_Tab_Name_Colour = BAR_TEXT,
    Default_Conf_Alias_Set = 0,
} Default_Conf;

static void set_default_friend_config_settings(ToxicFriend *friend, const Client_Config *c_config)
{
    if (friend == NULL) {
        return;
    }

    Friend_Settings *settings = &friend->settings;

    settings->auto_accept_files = Default_Conf_Auto_Accept_Files != 0;
    settings->autolog = c_config->autolog;
    settings->show_connection_msg = c_config->show_connection_msg;
    settings->tab_name_colour = Default_Conf_Tab_Name_Colour;
    settings->alias_set = Default_Conf_Alias_Set != 0;
}

void friend_reset_default_config_settings(FriendsList *friends, const Client_Config *c_config)
{
    for (size_t i = 0; i < friends->max_idx; ++i) {
        ToxicFriend *friend = &friends->list[i];

        if (!friend->active) {
            continue;
        }

        set_default_friend_config_settings(friend, c_config);
    }
}

static void realloc_friends(FriendsList *friends, int n)
{
    if (n <= 0) {
        free(friends->list);
        free(friends->index);
        friends->list = NULL;
        friends->index = NULL;
        return;
    }

    ToxicFriend *f = realloc(friends->list, n * sizeof(ToxicFriend));
    uint32_t *f_idx = realloc(friends->index, n * sizeof(uint32_t));

    if (f == NULL || f_idx == NULL) {
        exit_toxic_err(FATALERR_MEMORY, "failed in realloc_friends");
    }

    friends->list = f;
    friends->index = f_idx;
}

static void realloc_blocklist(BlockedList *blocked, int n)
{
    if (n <= 0) {
        free(blocked->list);
        free(blocked->index);
        blocked->list = NULL;
        blocked->index = NULL;
        return;
    }

    BlockedFriend *b = realloc(blocked->list, n * sizeof(BlockedFriend));
    uint32_t *b_idx = realloc(blocked->index, n * sizeof(uint32_t));

    if (b == NULL || b_idx == NULL) {
        exit_toxic_err(FATALERR_MEMORY, "failed in realloc_blocklist");
    }

    blocked->list = b;
    blocked->index = b_idx;
}

void kill_friendlist(ToxWindow *self, FriendsList *friends, BlockedList *blocked, Windows *windows,
                     const Client_Config *c_config)
{
    if (friends == NULL) {
        return;
    }

    for (size_t i = 0; i < friends->max_idx; ++i) {
        if (friends->list[i].active) {
            free(friends->list[i].conference_invite.key);
#ifdef GAMES
            free(friends->list[i].game_invite.data);
#endif
        }

        free(friends->list[i].group_invite.data);
    }

    realloc_blocklist(blocked, 0);
    realloc_friends(friends, 0);
    free(self->help);
    del_window(self, windows, c_config);
}


static void clear_blocklist_index(BlockedList *blocked, size_t idx)
{
    blocked->list[idx] = (BlockedFriend) {
        0
    };
}

static void clear_friendlist_index(FriendsList *friends, size_t idx)
{
    friends->list[idx] = (ToxicFriend) {
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
static int save_blocklist(const char *path, BlockedList *blocked)
{
    if (path == NULL) {
        return -1;
    }

    int len = sizeof(BlockedFriend) * blocked->num_blocked;
    char *data = malloc(len * sizeof(char));

    if (data == NULL) {
        return -1;
    }

    int count = 0;

    for (int i = 0; i < blocked->max_idx; ++i) {
        if (count > blocked->num_blocked) {
            free(data);
            return -1;
        }

        if (blocked->list[i].active) {
            if (blocked->list[i].namelength > TOXIC_MAX_NAME_LENGTH) {
                continue;
            }

            BlockedFriend tmp = {0};
            tmp.namelength = htons(blocked->list[i].namelength);
            memcpy(tmp.name, blocked->list[i].name, blocked->list[i].namelength + 1);  // Include null byte
            memcpy(tmp.pub_key, blocked->list[i].pub_key, TOX_PUBLIC_KEY_SIZE);

            uint8_t lastonline[sizeof(uint64_t)];
            memcpy(lastonline, &blocked->list[i].last_on, sizeof(uint64_t));
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

static void sort_blocklist_index(BlockedList *blocked);

int load_blocklist(const char *path, BlockedList *blocked)
{
    if (path == NULL) {
        return -1;
    }

    if (!file_exists(path)) {
        fprintf(stderr, "block list file `%s` doesn't exist\n", path);
        return 0;
    }

    FILE *fp = fopen(path, "rb");

    if (fp == NULL) {
        return -1;
    }

    const off_t len = file_size(path);

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

    const int num = len / sizeof(BlockedFriend);
    blocked->max_idx = num;
    realloc_blocklist(blocked, num);

    for (int i = 0; i < num; ++i) {
        BlockedFriend tmp = {0};
        clear_blocklist_index(blocked, i);

        memcpy(&tmp, data + i * sizeof(BlockedFriend), sizeof(BlockedFriend));

        blocked->list[i].namelength = ntohs(tmp.namelength);

        if (blocked->list[i].namelength > TOXIC_MAX_NAME_LENGTH) {
            continue;
        }

        blocked->list[i].active = true;
        blocked->list[i].num = i;
        memcpy(blocked->list[i].name, tmp.name, blocked->list[i].namelength + 1);   // copy null byte
        memcpy(blocked->list[i].pub_key, tmp.pub_key, TOX_PUBLIC_KEY_SIZE);

        uint8_t lastonline[sizeof(uint64_t)];
        memcpy(lastonline, &tmp.last_on, sizeof(uint64_t));
        net_to_host(lastonline, sizeof(uint64_t));
        memcpy(&blocked->list[i].last_on, lastonline, sizeof(uint64_t));

        ++blocked->num_blocked;
    }

    fclose(fp);
    free(data);
    sort_blocklist_index(blocked);

    return 0;
}

#define S_WEIGHT 100000
static int index_name_cmp(const void *n1, const void *n2, void *arg)
{
    FriendsList *friends = arg;
    int res = qsort_strcasecmp_hlpr(friends->list[*(const uint32_t *) n1].name,
                                    friends->list[*(const uint32_t *) n2].name);

    /* Use weight to make qsort always put online friends before offline */
    res = friends->list[*(const uint32_t *) n1].connection_status ? (res - S_WEIGHT) : (res + S_WEIGHT);
    res = friends->list[*(const uint32_t *) n2].connection_status ? (res + S_WEIGHT) : (res - S_WEIGHT);

    return res;
}

/* sorts friends->index first by connection status then alphabetically */
void sort_friendlist_index(FriendsList *friends)
{
    size_t i;
    uint32_t n = 0;

    for (i = 0; i < friends->max_idx; ++i) {
        if (friends->list[i].active) {
            friends->index[n++] = friends->list[i].num;
        }
    }

    if (friends->num_friends > 0) {
        toxic_qsort_r(friends->index, friends->num_friends, sizeof(uint32_t), index_name_cmp, friends);
    }
}

static int index_name_cmp_block(const void *n1, const void *n2, void *arg)
{
    BlockedList *blocked = arg;
    return qsort_strcasecmp_hlpr(blocked->list[*(const uint32_t *) n1].name,
                                 blocked->list[*(const uint32_t *) n2].name);
}

static void sort_blocklist_index(BlockedList *blocked)
{
    size_t i;
    uint32_t n = 0;

    for (i = 0; i < blocked->max_idx; ++i) {
        if (blocked->list[i].active) {
            blocked->index[n++] = blocked->list[i].num;
        }
    }

    toxic_qsort_r(blocked->index, blocked->num_blocked, sizeof(uint32_t), index_name_cmp_block, blocked);
}

static void update_friend_last_online(FriendsList *friends, uint32_t num, time_t timestamp,
                                      const char *timestamp_format)
{
    friends->list[num].last_online.last_on = timestamp;
    friends->list[num].last_online.tm = *localtime((const time_t *)&timestamp);

    /* if the format changes make sure TIME_STR_SIZE is the correct size */
    format_time_str(friends->list[num].last_online.hour_min_str, TIME_STR_SIZE, timestamp_format,
                    &friends->list[num].last_online.tm);
}

static void friendlist_onMessage(ToxWindow *self, Toxic *toxic, uint32_t num, Tox_Message_Type type, const char *str,
                                 size_t length)
{
    UNUSED_VAR(self);
    UNUSED_VAR(type);
    UNUSED_VAR(length);

    if (toxic == NULL) {
        return;
    }

    FriendsList *friends = toxic->friends;

    if (num >= friends->max_idx) {
        return;
    }

    if (friends->list[num].window_id != -1) {
        return;
    }

    const int window_id = add_window(toxic, new_chat(friends, friends->list[num].num));

    if (window_id < 0) {
        fprintf(stderr, "Failed to create new chat window in friendlist_onMessage\n");
        return;
    }

    friends->list[num].window_id = window_id;
}

static void friendlist_onConnectionChange(ToxWindow *self, Toxic *toxic, uint32_t num, Tox_Connection connection_status)
{
    UNUSED_VAR(self);

    if (toxic == NULL) {
        return;
    }

    FriendsList *friends = toxic->friends;

    if (num >= friends->max_idx) {
        return;
    }

    if (connection_status == TOX_CONNECTION_NONE) {
        --friends->num_online;
    } else if (friends->list[num].connection_status == TOX_CONNECTION_NONE) {
        ++friends->num_online;

        if (avatar_send(friends, toxic->tox, num) == -1) {
            fprintf(stderr, "avatar_send failed for friend %u\n", num);
        }
    }

    friends->list[num].connection_status = connection_status;
    update_friend_last_online(friends, num, get_unix_time(), toxic->c_config->timestamp_format);
    store_data(toxic);
    sort_friendlist_index(friends);
}

static void friendlist_onNickChange(ToxWindow *self, Toxic *toxic, uint32_t num, const char *nick, size_t length)
{
    UNUSED_VAR(self);
    UNUSED_VAR(length);

    FriendsList *friends = toxic->friends;

    if (num >= friends->max_idx) {
        return;
    }

    /* save old name for log renaming */
    char oldname[TOXIC_MAX_NAME_LENGTH + 1];
    snprintf(oldname, sizeof(oldname), "%s", friends->list[num].name);

    /* update name */
    snprintf(friends->list[num].name, sizeof(friends->list[num].name), "%s", nick);
    friends->list[num].namelength = strlen(friends->list[num].name);

    /* get data for chatlog renaming */
    char newnamecpy[TOXIC_MAX_NAME_LENGTH + 1];
    char myid[TOX_ADDRESS_SIZE];
    strcpy(newnamecpy, friends->list[num].name);
    tox_self_get_address(toxic->tox, (uint8_t *) myid);

    if (strcmp(oldname, newnamecpy) != 0) {
        if (rename_logfile(toxic->windows, toxic->c_config, toxic->paths, oldname, newnamecpy, myid,
                           friends->list[num].pub_key, friends->list[num].window_id) != 0) {
            fprintf(stderr, "Failed to rename friend chat log from `%s` to `%s`\n", oldname, newnamecpy);
        }
    }

    sort_friendlist_index(friends);
}

static void friendlist_onNickRefresh(ToxWindow *self, Toxic *toxic)
{
    UNUSED_VAR(self);

    sort_friendlist_index(toxic->friends);
}

static void friendlist_onStatusChange(ToxWindow *self, Toxic *toxic, uint32_t num, Tox_User_Status status)
{
    UNUSED_VAR(self);

    FriendsList *friends = toxic->friends;

    if (num >= friends->max_idx) {
        return;
    }

    friends->list[num].status = status;
}

static void friendlist_onStatusMessageChange(ToxWindow *self, Toxic *toxic, uint32_t num, const char *note,
        size_t length)
{
    UNUSED_VAR(self);

    if (toxic == NULL) {
        return;
    }

    FriendsList *friends = toxic->friends;

    if (length > TOX_MAX_STATUS_MESSAGE_LENGTH || num >= friends->max_idx) {
        return;
    }

    snprintf(friends->list[num].statusmsg, sizeof(friends->list[num].statusmsg), "%s", note);
    friends->list[num].statusmsg_len = strlen(friends->list[num].statusmsg);
}

void friendlist_onFriendAdded(ToxWindow *self, Toxic *toxic, uint32_t num, bool sort)
{
    UNUSED_VAR(self);

    if (toxic == NULL) {
        fprintf(stderr, "friendlist_onFriendAdded null param\n");
        return;
    }

    Tox *tox = toxic->tox;
    const Client_Config *c_config = toxic->c_config;
    FriendsList *friends = toxic->friends;

    realloc_friends(friends, friends->max_idx + 1);
    clear_friendlist_index(friends, friends->max_idx);

    for (uint32_t i = 0; i <= friends->max_idx; ++i) {
        if (friends->list[i].active) {
            continue;
        }

        ++friends->num_friends;

        friends->list[i].num = num;
        friends->list[i].active = true;
        friends->list[i].window_id = -1;
        friends->list[i].auto_accept_files = false;  // do not change
        friends->list[i].connection_status = TOX_CONNECTION_NONE;
        friends->list[i].status = TOX_USER_STATUS_NONE;
        set_default_friend_config_settings(&friends->list[i], c_config);

        Tox_Err_Friend_Get_Public_Key pkerr;
        tox_friend_get_public_key(tox, num, (uint8_t *) friends->list[i].pub_key, &pkerr);

        if (pkerr != TOX_ERR_FRIEND_GET_PUBLIC_KEY_OK) {
            fprintf(stderr, "tox_friend_get_public_key failed (error %d)\n", pkerr);
        }

        Tox_Err_Friend_Get_Last_Online loerr;
        time_t t = tox_friend_get_last_online(tox, num, &loerr);

        if (loerr != TOX_ERR_FRIEND_GET_LAST_ONLINE_OK) {
            t = 0;
        }

        update_friend_last_online(friends, i, t, c_config->timestamp_format);

        char tempname[TOXIC_MAX_NAME_LENGTH + 1];
        const size_t name_len = get_nick_truncate(tox, tempname, sizeof(tempname), num);

        snprintf(friends->list[i].name, sizeof(friends->list[i].name), "%s", tempname);
        friends->list[i].namelength = name_len;

        if (i == friends->max_idx) {
            ++friends->max_idx;
        }

        if (sort) {
            sort_friendlist_index(friends);
        }

#ifdef AUDIO

        if (!init_friend_AV(toxic->call_control, i)) {
            fprintf(stderr, "Failed to init AV for friend %u\n", i);
        }

#endif

        return;
    }
}

/* Puts blocked friend back in friendlist. fnum is new friend number, bnum is blocked number. */
static void friendlist_add_blocked(FriendsList *friends, BlockedList *blocked, const Client_Config *c_config,
                                   struct CallControl *cc, uint32_t fnum, uint32_t bnum)
{
    realloc_friends(friends, friends->max_idx + 1);
    clear_friendlist_index(friends, friends->max_idx);

    for (int i = 0; i <= (int) friends->max_idx; ++i) {
        if (friends->list[i].active) {
            continue;
        }

        ++friends->num_friends;

        friends->list[i].num = fnum;
        friends->list[i].active = true;
        friends->list[i].window_id = -1;
        friends->list[i].status = TOX_USER_STATUS_NONE;
        friends->list[i].namelength = blocked->list[bnum].namelength;
        update_friend_last_online(friends, i, blocked->list[bnum].last_on, c_config->timestamp_format);
        memcpy(friends->list[i].name, blocked->list[bnum].name, friends->list[i].namelength + 1);
        memcpy(friends->list[i].pub_key, blocked->list[bnum].pub_key, TOX_PUBLIC_KEY_SIZE);
        set_default_friend_config_settings(&friends->list[i], c_config);

        if (i == (int) friends->max_idx) {
            ++friends->max_idx;
        }

        sort_blocklist_index(blocked);
        sort_friendlist_index(friends);

#ifdef AUDIO

        if (!init_friend_AV(cc, i)) {
            fprintf(stderr, "Failed to init AV for friend %d\n", i);
        }

#endif
        return;
    }
}

#ifdef GAMES

static void friendlist_onGameInvite(ToxWindow *self, Toxic *toxic, uint32_t friend_number, const uint8_t *data,
                                    size_t length)
{
    UNUSED_VAR(self);
    UNUSED_VAR(data);
    UNUSED_VAR(length);

    if (toxic == NULL) {
        return;
    }

    FriendsList *friends = toxic->friends;

    if (friend_number >= friends->max_idx) {
        return;
    }

    if (friends->list[friend_number].window_id != -1) {
        return;
    }

    const int window_id = add_window(toxic, new_chat(friends, friends->list[friend_number].num));

    if (window_id < 0) {
        fprintf(stderr, "Failed to create new chat window in friendlist_onGameInvite\n");
        return;
    }

    friends->list[friend_number].window_id = window_id;
}

#endif // GAMES

static void friendlist_onFileRecv(ToxWindow *self, Toxic *toxic, uint32_t num, uint32_t filenum,
                                  uint64_t file_size, const char *filename, size_t name_length)
{
    UNUSED_VAR(self);
    UNUSED_VAR(file_size);
    UNUSED_VAR(filename);
    UNUSED_VAR(name_length);

    FriendsList *friends = toxic->friends;

    if (num >= friends->max_idx) {
        return;
    }

    if (friends->list[num].window_id != -1) {
        return;
    }

    const int window_id = add_window(toxic, new_chat(friends, friends->list[num].num));

    if (window_id < 0) {
        fprintf(stderr, "Failed to create new chat window in friendlist_onFileRecv\n");
        return;
    }

    friends->list[num].window_id = window_id;
}

static void friendlist_onConferenceInvite(ToxWindow *self, Toxic *toxic, int32_t num, uint8_t type,
        const char *conference_pub_key,
        uint16_t length)
{
    UNUSED_VAR(self);
    UNUSED_VAR(type);
    UNUSED_VAR(conference_pub_key);
    UNUSED_VAR(length);

    if (toxic == NULL) {
        return;
    }

    FriendsList *friends = toxic->friends;

    if (num >= friends->max_idx) {
        return;
    }

    if (friends->list[num].window_id != -1) {
        return;
    }

    const int window_id = add_window(toxic, new_chat(friends, friends->list[num].num));

    if (window_id < 0) {
        fprintf(stderr, "Failed to create new chat window in friendlist_onConferenceInvite\n");
        return;
    }

    friends->list[num].window_id = window_id;
}

static void friendlist_onGroupInvite(ToxWindow *self, Toxic *toxic, uint32_t num, const char *data, size_t length,
                                     const char *group_name, size_t group_name_length)
{
    UNUSED_VAR(self);
    UNUSED_VAR(data);
    UNUSED_VAR(length);

    if (toxic == NULL) {
        return;
    }

    FriendsList *friends = toxic->friends;

    if (num >= friends->max_idx) {
        return;
    }

    if (friends->list[num].window_id != -1) {
        return;
    }

    const int window_id = add_window(toxic, new_chat(friends, friends->list[num].num));

    if (window_id < 0) {
        fprintf(stderr, "Failed to create new chat window in friendlist_onGroupInvite\n");
        return;
    }

    friends->list[num].window_id = window_id;
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

static void delete_friend(Toxic *toxic, uint32_t f_num)
{
    if (toxic == NULL) {
        return;
    }

    Tox *tox = toxic->tox;
    FriendsList *friends = toxic->friends;

    kill_all_file_transfers_friend(toxic, f_num);
    kill_avatar_file_transfers_friend(toxic, f_num);

    Tox_Err_Friend_Delete err;

    if (tox_friend_delete(tox, f_num, &err) != true) {
        fprintf(stderr, "tox_friend_delete failed with error %d\n", err);
        return;
    }

    --friends->num_friends;

    if (friends->list[f_num].connection_status != TOX_CONNECTION_NONE) {
        --friends->num_online;
    }

    /* close friend's window if it's currently open */
    if (friends->list[f_num].window_id >= 0) {
        ToxWindow *toxwin = get_window_pointer_by_id(toxic->windows, friends->list[f_num].window_id);

        if (toxwin != NULL) {
            kill_chat_window(toxwin, toxic);
            set_active_window_by_type(toxic->windows, WINDOW_TYPE_FRIEND_LIST);
        }
    }

    free(friends->list[f_num].conference_invite.key);

    clear_friendlist_index(friends, f_num);

    int i;

    for (i = friends->max_idx; i > 0; --i) {
        if (friends->list[i - 1].active) {
            break;
        }
    }

    friends->max_idx = i;
    realloc_friends(friends, i);

#ifdef AUDIO
    del_friend_AV(toxic->call_control, i);
#endif

    /* make sure num_selected stays within friends->num_friends range */
    if (friends->num_friends && friends->num_selected == friends->num_friends) {
        --friends->num_selected;
    }

    store_data(toxic);
}

/* activates delete friend popup */
static void del_friend_activate(uint32_t f_num)
{
    PendingDelete.popup = newwin(3, 22 + TOXIC_MAX_NAME_LENGTH, 8, 8);
    PendingDelete.active = true;
    PendingDelete.num = f_num;
}

static void delete_blocked_friend(Toxic *toxic, uint32_t bnum);

/* deactivates delete friend popup and deletes friend if instructed */
static void del_friend_deactivate(Toxic *toxic, wint_t key)
{
    if (key == L'y') {
        if (toxic->blocklist_view == 0) {
            delete_friend(toxic, PendingDelete.num);
            sort_friendlist_index(toxic->friends);
        } else {
            delete_blocked_friend(toxic, PendingDelete.num);
            sort_blocklist_index(toxic->blocked);
        }
    }

    delwin(PendingDelete.popup);

    PendingDelete = (struct PendingDel) {
        0
    };

    clear();
    refresh();
}

static void draw_del_popup(Toxic *toxic)
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

    if (toxic->blocklist_view == 0) {
        wprintw(PendingDelete.popup, "%s", toxic->friends->list[PendingDelete.num].name);
    } else {
        wprintw(PendingDelete.popup, "%s", toxic->blocked->list[PendingDelete.num].name);
    }

    pthread_mutex_unlock(&Winthread.lock);

    wattroff(PendingDelete.popup, A_BOLD);
    wprintw(PendingDelete.popup, "? y/n");

    wnoutrefresh(PendingDelete.popup);
}

/* deletes contact from blocked list */
static void delete_blocked_friend(Toxic *toxic, uint32_t bnum)
{
    BlockedList *blocked = toxic->blocked;
    int i;

    for (i = blocked->max_idx; i > 0; --i) {
        if (blocked->list[i - 1].active) {
            break;
        }
    }

    clear_blocklist_index(blocked, bnum);

    --blocked->num_blocked;
    blocked->max_idx = i;
    realloc_blocklist(blocked, i);

    /* make sure num_selected stays within blocked->num_blocked range */
    if (blocked->num_blocked && blocked->num_selected == blocked->num_blocked) {
        --blocked->num_selected;
    }

    save_blocklist(toxic->client_data.block_path, blocked);
}

/* deletes contact from friendlist and puts in blocklist */
static void block_friend(Toxic *toxic, uint32_t fnum)
{
    FriendsList *friends = toxic->friends;
    BlockedList *blocked = toxic->blocked;

    if (friends->num_friends == 0) {
        return;
    }

    realloc_blocklist(blocked, blocked->max_idx + 1);
    clear_blocklist_index(blocked, blocked->max_idx);

    for (int i = 0; i <= (int) blocked->max_idx; ++i) {
        if (blocked->list[i].active) {
            continue;
        }

        blocked->list[i].active = true;
        blocked->list[i].num = i;
        blocked->list[i].namelength = friends->list[fnum].namelength;
        blocked->list[i].last_on = friends->list[fnum].last_online.last_on;
        memcpy(blocked->list[i].pub_key, friends->list[fnum].pub_key, TOX_PUBLIC_KEY_SIZE);
        memcpy(blocked->list[i].name, friends->list[fnum].name, friends->list[fnum].namelength + 1);

        ++blocked->num_blocked;

        if (i == (int) blocked->max_idx) {
            ++blocked->max_idx;
        }

        delete_friend(toxic, fnum);
        save_blocklist(toxic->client_data.block_path, blocked);
        sort_blocklist_index(blocked);
        sort_friendlist_index(friends);

        return;
    }
}

/* removes friend from blocklist, puts back in friendlist */
static void unblock_friend(Toxic *toxic, uint32_t bnum)
{
    if (toxic->blocked->num_blocked <= 0) {
        return;
    }

    Tox_Err_Friend_Add err;
    uint32_t friendnum = tox_friend_add_norequest(toxic->tox, (uint8_t *) toxic->blocked->list[bnum].pub_key, &err);

    if (err != TOX_ERR_FRIEND_ADD_OK) {
        line_info_add(toxic->home_window, toxic->c_config, false, NULL, NULL, SYS_MSG, 0, 0,
                      "Failed to unblock friend (error %d)", err);
        return;
    }

    friendlist_add_blocked(toxic->friends, toxic->blocked, toxic->c_config, toxic->call_control, friendnum, bnum);
    delete_blocked_friend(toxic, bnum);
    sort_blocklist_index(toxic->blocked);
    sort_friendlist_index(toxic->friends);
}

/*
 * Return true if input is recognized by handler
 */
static bool friendlist_onKey(ToxWindow *self, Toxic *toxic, wint_t key, bool ltr)
{
    if (toxic == NULL || self == NULL) {
        return false;
    }

    FriendsList *friends = toxic->friends;

    if (self->help->active) {
        help_onKey(self, key);
        return true;
    }

    if (key == L'h') {
        help_init_menu(self);
        return true;
    }

    if (!toxic->blocklist_view && !friends->num_friends && (key != KEY_RIGHT && key != KEY_LEFT)) {
        return true;
    }

    if (toxic->blocklist_view && !toxic->blocked->num_blocked && (key != KEY_RIGHT && key != KEY_LEFT)) {
        return true;
    }

    int f = 0;

    if (toxic->blocklist_view == 1 && toxic->blocked->num_blocked) {
        f = toxic->blocked->index[toxic->blocked->num_selected];
    } else if (friends->num_friends) {
        f = friends->index[friends->num_selected];
    }

    /* lock screen and force decision on deletion popup */
    if (PendingDelete.active) {
        if (key == L'y' || key == L'n') {
            del_friend_deactivate(toxic, key);
        }

        return true;
    }

    if (key == ltr) {
        return true;
    }

    switch (key) {
        case L'\r':
            if (toxic->blocklist_view) {
                break;
            }

            /* Jump to chat window if already open */
            if (friends->list[f].window_id < 0) {
                const int window_id = add_window(toxic, new_chat(friends, friends->list[f].num));

                if (window_id < 0) {
                    fprintf(stderr, "Failed to create new chat window in friendlist_onKey\n");
                    return true;
                }

                friends->list[f].window_id = window_id;
            }

            set_active_window_by_id(toxic->windows, friends->list[f].window_id);
            break;

        case KEY_DC:
            del_friend_activate(f);
            break;

        case L'b':
            if (!toxic->blocklist_view) {
                block_friend(toxic, f);
            } else {
                unblock_friend(toxic, f);
            }

            break;

        case KEY_RIGHT:
        case KEY_LEFT:
            toxic->blocklist_view ^= 1;
            break;

        default:
            if (toxic->blocklist_view == 0) {
                select_friend(key, &friends->num_selected, friends->num_friends);
            } else {
                select_friend(key, &toxic->blocked->num_selected, toxic->blocked->num_blocked);
            }

            break;
    }

    return true;
}

#define FLIST_OFST 6    /* Accounts for space at top and bottom */

static void blocklist_onDraw(ToxWindow *self, Toxic *toxic, int y2, int x2)
{
    wattron(self->window, A_BOLD);
    wprintw(self->window, " Blocked: ");
    wattroff(self->window, A_BOLD);
    wprintw(self->window, "%d\n\n", toxic->blocked->num_blocked);

    if ((y2 - FLIST_OFST) <= 0) {
        return;
    }

    uint32_t selected_num = 0;

    /* Determine which portion of friendlist to draw based on current position */
    int page = toxic->blocked->num_selected / (y2 - FLIST_OFST);
    int start = (y2 - FLIST_OFST) * page;
    int end = y2 - FLIST_OFST + start;

    for (int i = start; i < toxic->blocked->num_blocked && i < end; ++i) {
        uint32_t f = toxic->blocked->index[i];
        bool f_selected = false;

        if (i == toxic->blocked->num_selected) {
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
        wprintw(self->window, " %s\n", toxic->blocked->list[f].name);
        wattroff(self->window, A_BOLD);

        if (f_selected) {
            wattroff(self->window, COLOR_PAIR(BLUE));
        }
    }

    wprintw(self->window, "\n");
    self->x = x2;

    if (toxic->blocked->num_blocked) {
        wmove(self->window, y2 - 1, 1);
        wattron(self->window, A_BOLD);
        wprintw(self->window, "Public key: ");
        wattroff(self->window, A_BOLD);

        for (int i = 0; i < TOX_PUBLIC_KEY_SIZE; ++i) {
            wprintw(self->window, "%02X", toxic->blocked->list[selected_num].pub_key[i] & 0xff);
        }
    }

    wnoutrefresh(self->window);
    draw_del_popup(toxic);

    if (self->help->active) {
        help_draw_main(self);
    }
}

static void friendlist_onDraw(ToxWindow *self, Toxic *toxic)
{
    if (toxic == NULL || self == NULL) {
        fprintf(stderr, "friendlist_onDraw null param\n");
        return;
    }

    FriendsList *friends = toxic->friends;

    curs_set(0);
    werase(self->window);
    int x2, y2;
    getmaxyx(self->window, y2, x2);

    const bool fix_statuses = x2 != self->x;    /* true if window max x value has changed */

    wattron(self->window, COLOR_PAIR(CYAN));
    wprintw(self->window, " Press the");
    wattron(self->window, A_BOLD);
    wprintw(self->window, " h ");
    wattroff(self->window, A_BOLD);
    wprintw(self->window, "key for help\n\n");
    wattroff(self->window, COLOR_PAIR(CYAN));

    draw_window_bar(self, toxic->windows);

    if (toxic->blocklist_view == 1) {
        blocklist_onDraw(self, toxic, y2, x2);
        return;
    }

    const time_t cur_time = get_unix_time();
    struct tm cur_loc_tm = *localtime((const time_t *) &cur_time);

    wattron(self->window, A_BOLD);
    wprintw(self->window, " Online: ");
    wattroff(self->window, A_BOLD);

    wprintw(self->window, "%zu/%zu \n\n", friends->num_online, friends->num_friends);

    if ((y2 - FLIST_OFST) <= 0) {
        return;
    }

    uint32_t selected_num = 0;

    /* Determine which portion of friendlist to draw based on current position */
    pthread_mutex_lock(&Winthread.lock);
    const int page = friends->num_selected / (y2 - FLIST_OFST);
    pthread_mutex_unlock(&Winthread.lock);

    const int start = (y2 - FLIST_OFST) * page;
    const int end = y2 - FLIST_OFST + start;

    pthread_mutex_lock(&Winthread.lock);
    const size_t num_friends = friends->num_friends;
    pthread_mutex_unlock(&Winthread.lock);

    for (int i = start; i < num_friends && i < end; ++i) {
        pthread_mutex_lock(&Winthread.lock);
        uint32_t f = friends->index[i];
        bool is_active = friends->list[f].active;
        int num_selected = friends->num_selected;
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
            Tox_Connection connection_status = friends->list[f].connection_status;
            Tox_User_Status status = friends->list[f].status;
            pthread_mutex_unlock(&Winthread.lock);

            if (connection_status != TOX_CONNECTION_NONE) {
                int colour;

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

                    default:
                        colour = BAR_TEXT;
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
                wprintw(self->window, "%s", friends->list[f].name);
                pthread_mutex_unlock(&Winthread.lock);
                wattroff(self->window, A_BOLD);

                if (f_selected) {
                    wattroff(self->window, COLOR_PAIR(BLUE));
                }

                /* Reset friends->list[f].statusmsg on window resize */
                if (fix_statuses) {
                    char statusmsg[TOX_MAX_STATUS_MESSAGE_LENGTH];

                    pthread_mutex_lock(&Winthread.lock);
                    tox_friend_get_status_message(toxic->tox, friends->list[f].num, (uint8_t *) statusmsg, NULL);
                    const size_t s_len = tox_friend_get_status_message_size(toxic->tox, friends->list[f].num, NULL);
                    pthread_mutex_unlock(&Winthread.lock);

                    statusmsg[s_len] = '\0';

                    filter_string(statusmsg, s_len, false);

                    pthread_mutex_lock(&Winthread.lock);
                    snprintf(friends->list[f].statusmsg, sizeof(friends->list[f].statusmsg), "%s", statusmsg);
                    friends->list[f].statusmsg_len = strlen(friends->list[f].statusmsg);
                    pthread_mutex_unlock(&Winthread.lock);
                }

                /* Truncate note if it doesn't fit on one line */
                size_t maxlen = x2 - getcurx(self->window) - 2;

                pthread_mutex_lock(&Winthread.lock);

                if (friends->list[f].statusmsg_len > maxlen) {
                    friends->list[f].statusmsg[maxlen - 3] = '\0';
                    strcat(friends->list[f].statusmsg, "...");
                    friends->list[f].statusmsg[maxlen] = '\0';
                    friends->list[f].statusmsg_len = maxlen;
                }

                if (friends->list[f].statusmsg_len > 0) {
                    wprintw(self->window, " %s", friends->list[f].statusmsg);
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
                wprintw(self->window, "%s", friends->list[f].name);
                pthread_mutex_unlock(&Winthread.lock);
                wattroff(self->window, A_BOLD);

                if (f_selected) {
                    wattroff(self->window, COLOR_PAIR(BLUE));
                }

                pthread_mutex_lock(&Winthread.lock);
                time_t last_seen = friends->list[f].last_online.last_on;
                pthread_mutex_unlock(&Winthread.lock);

                if (last_seen != 0) {
                    pthread_mutex_lock(&Winthread.lock);

                    int day_dist = (
                                       cur_loc_tm.tm_yday - friends->list[f].last_online.tm.tm_yday
                                       + ((cur_loc_tm.tm_year - friends->list[f].last_online.tm.tm_year) * 365)
                                   );
                    const char *hourmin = friends->list[f].last_online.hour_min_str;

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

        for (int i = 0; i < TOX_PUBLIC_KEY_SIZE; ++i) {
            wprintw(self->window, "%02X", friends->list[selected_num].pub_key[i] & 0xff);
        }
    }

    wnoutrefresh(self->window);
    draw_del_popup(toxic);

    if (self->help->active) {
        help_draw_main(self);
    }
}

void friendlist_onInit(ToxWindow *self, Toxic *toxic)
{
    UNUSED_VAR(toxic);

    if (self == NULL) {
        return;
    }

    int x2;
    int y2;
    getmaxyx(self->window, y2, x2);

    if (y2 <= 0 || x2 <= 0) {
        exit_toxic_err(FATALERR_CURSES, "failed in friendlist_onInit");
    }

    self->window_bar = subwin(self->window, WINDOW_BAR_HEIGHT, x2, y2 - 2, 0);
}

void disable_friend_window(FriendsList *friends, uint32_t f_num)
{
    if (f_num >= friends->max_idx) {
        return;
    }

    friends->list[f_num].window_id = -1;
}

size_t friendlist_get_count(const FriendsList *friends)
{
    if (friends == NULL) {
        return 0;
    }

    return friends->num_friends;
}

void friendlist_get_names(const FriendsList *friends, char **names, size_t max_names, size_t max_name_size)
{
    if (friends == NULL || friends->num_friends == 0) {
        return;
    }

    const size_t bytes_to_copy = MIN(sizeof(friends->list[0].name), max_name_size);

    for (size_t i = 0; i < max_names && i < friends->num_friends; ++i) {
        snprintf(names[i], bytes_to_copy, "%s", friends->list[i].name);
    }
}

#ifdef AUDIO
static void friendlist_onAV(ToxWindow *self, Toxic *toxic, uint32_t friend_number, int state)
{
    UNUSED_VAR(self);

    if (toxic == NULL) {
        return;
    }

    FriendsList *friends = toxic->friends;

    if (friend_number >= friends->max_idx) {
        return;
    }

    if (friends->list[friend_number].window_id >= 0) {
        return;
    }

    if (state != TOXAV_FRIEND_CALL_STATE_FINISHED) {
        const int window_id = add_window(toxic, new_chat(friends, friends->list[friend_number].num));

        if (window_id < 0) {
            fprintf(stderr, "Failed to create new chat window in friendlist_onAV");
            return;
        }

        friends->list[friend_number].window_id = window_id;
        set_active_window_by_id(toxic->windows, window_id);
    }
}

#endif /* AUDIO */

/* Returns a friend's status */
Tox_User_Status get_friend_status(const FriendsList *friends, uint32_t friendnumber)
{
    if (friendnumber >= friends->max_idx) {
        return TOX_USER_STATUS_NONE;
    }

    return friends->list[friendnumber].status;
}

/* Returns a friend's connection status */
Tox_Connection get_friend_connection_status(const FriendsList *friends, uint32_t friendnumber)
{
    if (friendnumber >= friends->max_idx) {
        return TOX_CONNECTION_NONE;
    }

    return friends->list[friendnumber].connection_status;
}

int64_t get_friend_number_name(const FriendsList *friends, const char *name, uint16_t length)
{
    if (friends == NULL) {
        return -1;
    }

    int64_t num = -1;
    bool match_found = false;

    for (size_t i = 0; i < friends->max_idx; ++i) {
        if (length != friends->list[i].namelength) {
            continue;
        }

        if (memcmp(name, friends->list[i].name, length) == 0) {
            if (match_found) {
                return -2;
            }

            num = friends->list[i].num;
            match_found = true;
        }
    }

    return num;
}

/*
 * Returns true if friend associated with `public_key` is in the block list.
 *
 * `public_key` must be at least TOX_PUBLIC_KEY_SIZE bytes.
 */
bool friend_is_blocked(const BlockedList *blocked, const char *public_key)
{
    for (size_t i = 0; i < blocked->max_idx; ++i) {
        if (!blocked->list[i].active) {
            continue;
        }

        if (memcmp(public_key, blocked->list[i].pub_key, TOX_PUBLIC_KEY_SIZE) == 0) {
            return true;
        }
    }

    return false;
}

void friend_set_logging_enabled(FriendsList *friends, uint32_t friendnumber, bool enable_log)
{
    if (friendnumber >= friends->max_idx) {
        return;
    }

    ToxicFriend *friend = &friends->list[friendnumber];

    if (!friend->active) {
        return;
    }

    friend->logging_on = enable_log;
}

bool friend_get_logging_enabled(const FriendsList *friends, uint32_t friendnumber)
{
    if (friendnumber >= friends->max_idx) {
        return false;
    }

    const ToxicFriend *friend = &friends->list[friendnumber];

    if (!friend->active) {
        return false;
    }

    return friend->logging_on;
}

void friend_set_auto_file_accept(FriendsList *friends, uint32_t friendnumber, bool auto_accept)
{
    if (friendnumber >= friends->max_idx) {
        return;
    }

    ToxicFriend *friend = &friends->list[friendnumber];

    if (!friend->active) {
        return;
    }

    friend->auto_accept_files = auto_accept;
}

bool friend_get_auto_accept_files(const FriendsList *friends, uint32_t friendnumber)
{
    if (friendnumber >= friends->max_idx) {
        return false;
    }

    const ToxicFriend *friend = &friends->list[friendnumber];

    if (!friend->active) {
        return false;
    }

    return friend->auto_accept_files;
}

bool get_friend_public_key(const FriendsList *friends, char *pk, uint32_t friendnumber)
{
    if (friendnumber >= friends->max_idx) {
        return false;
    }

    const ToxicFriend *friend = &friends->list[friendnumber];

    if (!friend->active) {
        return false;
    }

    memcpy(pk, friend->pub_key, TOX_PUBLIC_KEY_SIZE);
    return true;
}

uint16_t get_friend_name(const FriendsList *friends, char *buf, size_t buf_size, uint32_t friendnumber)
{
    if (friends == NULL) {
        goto on_error;
    }

    if (friendnumber >= friends->max_idx) {
        goto on_error;
    }

    const ToxicFriend *friend = &friends->list[friendnumber];

    if (!friend->active) {
        goto on_error;
    }

    snprintf(buf, buf_size, "%s", friend->name);
    return (uint16_t) strlen(buf);

on_error:
    snprintf(buf, buf_size, "%s", UNKNOWN_NAME);
    return (uint16_t) strlen(buf);
}

/*
 * Returns a pointer to the Friend_Settings object associated with `public_key`.
 * If non-null, the friendnumber of `public_key` will be copied to the `friendnumber`
 * pointer on success.
 *
 * Returns NULL on failure.
 */
static Friend_Settings *get_friend_settings_by_key(FriendsList *friends, const char *public_key,
        uint32_t *friendnumber)
{
    char pk_bin[TOX_PUBLIC_KEY_SIZE];

    if (tox_pk_string_to_bytes(public_key, strlen(public_key), pk_bin, sizeof(pk_bin)) != 0) {
        return NULL;
    }

    for (size_t i = 0; i < friends->max_idx; ++i) {
        ToxicFriend *friend = &friends->list[i];

        if (!friend->active) {
            continue;
        }

        if (memcmp(pk_bin, friend->pub_key, sizeof(friend->pub_key)) == 0) {
            if (friendnumber != NULL) {
                *friendnumber = friend->num;
            }

            return &friend->settings;
        }
    }

    return NULL;
}

bool friend_config_set_show_connection_msg(FriendsList *friends, const char *public_key, bool show_connection_msg)
{
    Friend_Settings *settings = get_friend_settings_by_key(friends, public_key, NULL);

    if (settings == NULL) {
        return false;
    }

    settings->show_connection_msg = show_connection_msg;

    return true;
}

bool friend_config_get_show_connection_msg(const FriendsList *friends, uint32_t friendnumber)
{
    if (friendnumber >= friends->max_idx) {
        fprintf(stderr, "failed to get friend tab name colour (invalid friendnumber %u)\n", friendnumber);
        return false;
    }

    const ToxicFriend *friend = &friends->list[friendnumber];

    if (!friend->active) {
        return false;
    }

    return friend->settings.show_connection_msg;
}

bool friend_config_set_tab_name_colour(FriendsList *friends, const char *public_key, const char *colour)
{
    Friend_Settings *settings = get_friend_settings_by_key(friends, public_key, NULL);

    if (settings == NULL) {
        return false;
    }

    const int colour_val = colour_string_to_int(colour);

    if (colour_val < 0) {
        return false;
    }

    settings->tab_name_colour = colour_val;

    return true;
}

int friend_config_get_tab_name_colour(const FriendsList *friends, uint32_t friendnumber)
{
    if (friendnumber >= friends->max_idx) {
        fprintf(stderr, "failed to get friend tab name colour (invalid friendnumber %u)\n", friendnumber);
        return -1;
    }

    const ToxicFriend *friend = &friends->list[friendnumber];

    if (!friend->active) {
        return -1;
    }

    return friend->settings.tab_name_colour;
}

bool friend_config_set_autolog(FriendsList *friends, const char *public_key, bool autolog_enabled)
{
    Friend_Settings *settings = get_friend_settings_by_key(friends, public_key, NULL);

    if (settings == NULL) {
        return false;
    }

    settings->autolog = autolog_enabled;

    return true;
}

bool friend_config_get_autolog(const FriendsList *friends, uint32_t friendnumber)
{
    if (friendnumber >= friends->max_idx) {
        fprintf(stderr, "failed to get autolog setting (invalid friendnumber %u)\n", friendnumber);
        return false;
    }

    const ToxicFriend *friend = &friends->list[friendnumber];

    if (!friend->active) {
        return false;
    }

    return friend->settings.autolog;
}

bool friend_config_set_auto_accept_files(FriendsList *friends, const char *public_key, bool auto_accept_files)
{
    Friend_Settings *settings = get_friend_settings_by_key(friends, public_key, NULL);

    if (settings == NULL) {
        return false;
    }

    settings->auto_accept_files = auto_accept_files;

    return true;
}

bool friend_config_get_auto_accept_files(const FriendsList *friends, uint32_t friendnumber)
{
    if (friendnumber >= friends->max_idx) {
        fprintf(stderr, "failed to get auto-accept files autolog setting (invalid friendnumber %u)\n", friendnumber);
        return false;
    }

    const ToxicFriend *friend = &friends->list[friendnumber];

    if (!friend->active) {
        return false;
    }

    return friend->settings.auto_accept_files;
}

bool friend_config_alias_is_set(const FriendsList *friends, uint32_t friendnumber)
{
    if (friendnumber >= friends->max_idx) {
        fprintf(stderr, "failed to get alias setting (invalid friendnumber %u)\n", friendnumber);
        return false;
    }

    const ToxicFriend *friend = &friends->list[friendnumber];

    if (!friend->active) {
        return false;
    }

    return friend->settings.alias_set;
}

bool friend_config_set_alias(FriendsList *friends, const char *public_key, const char *alias, uint16_t length)
{
    uint32_t friendnumber;
    Friend_Settings *settings = get_friend_settings_by_key(friends, public_key, &friendnumber);

    if (settings == NULL) {
        return false;
    }

    if (friendnumber >= friends->max_idx) {
        return false;
    }

    if (alias == NULL || string_is_empty(alias)) {
        fprintf(stderr, "Failed to set alias with NULL name for: %s\n", public_key);
        return false;
    }

    if (length == 0 || length > TOXIC_MAX_NAME_LENGTH) {
        fprintf(stderr, "Failed to set alias '%s' (invalid length: %u)\n", alias, length);
        return false;
    }

    char tmp[TOXIC_MAX_NAME_LENGTH + 1];
    const uint16_t tmp_len = copy_tox_str(tmp, sizeof(tmp), alias, length);
    filter_string(tmp, tmp_len, true);

    if (tmp_len == 0 || tmp_len > TOXIC_MAX_NAME_LENGTH) {
        return false;
    }

    ToxicFriend *friend = &friends->list[friendnumber];

    snprintf(friend->name, sizeof(friend->name), "%s", tmp);
    friend->namelength = strlen(friend->name);

    settings->alias_set = true;

    return true;
}

ToxWindow *new_friendlist(void)
{
    ToxWindow *ret = calloc(1, sizeof(ToxWindow));

    if (ret == NULL) {
        exit_toxic_err(FATALERR_MEMORY, "failed in new_friendlist");
    }

    ret->type = WINDOW_TYPE_FRIEND_LIST;

    ret->onInit = &friendlist_onInit;
    ret->onKey = &friendlist_onKey;
    ret->onDraw = &friendlist_onDraw;
    ret->onNickRefresh = &friendlist_onNickRefresh;
    ret->onFriendAdded = &friendlist_onFriendAdded;
    ret->onMessage = &friendlist_onMessage;
    ret->onConnectionChange = &friendlist_onConnectionChange;
    ret->onNickChange = &friendlist_onNickChange;
    ret->onStatusChange = &friendlist_onStatusChange;
    ret->onStatusMessageChange = &friendlist_onStatusMessageChange;
    ret->onFileRecv = &friendlist_onFileRecv;
    ret->onConferenceInvite = &friendlist_onConferenceInvite;
    ret->onGroupInvite = &friendlist_onGroupInvite;

#ifdef AUDIO
    ret->onInvite = &friendlist_onAV;
    ret->onRinging = &friendlist_onAV;
    ret->onStarting = &friendlist_onAV;
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
        exit_toxic_err(FATALERR_MEMORY, "failed in new_friendlist");
    }

    ret->help = help;
    strcpy(ret->name, "Contacts");
    return ret;
}

/*  friendlist.h
 *
 *
 *  Copyright (C) 2024 Toxic All Rights Reserved.
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

#ifndef FRIENDLIST_H
#define FRIENDLIST_H

#include <time.h>

#include "file_transfers.h"
#include "toxic.h"
#include "windows.h"

#ifdef GAMES
#include "game_base.h"
#endif

struct LastOnline {
    uint64_t last_on;
    struct tm tm;
    char hour_min_str[TIME_STR_SIZE];    /* holds 12/24-hour time string e.g. "10:43 PM" */
};

struct ConferenceInvite {
    char *key;
    uint16_t length;
    uint8_t type;
    bool pending;
};

struct GroupInvite {
    uint8_t *data;
    uint16_t length;
};

#ifdef GAMES

struct GameInvite {
    uint8_t *data;
    size_t data_length;
    GameType type;
    uint32_t id;
    bool pending;
};

#endif // GAMES

/* Config settings for this friend.
 *
 * These values are set on initialization and should not change afterwards.
 * If a client modifies one of these settings via command and then closes
 * the chat window, these values will override the client's modified settings
 * upon re-opening the respective friend's chat window.
 */
typedef struct Friend_Settings {
    int   tab_name_colour;
    bool  autolog;
    bool  auto_accept_files;
} Friend_Settings;

typedef struct {
    char name[TOXIC_MAX_NAME_LENGTH + 1];
    uint16_t namelength;
    char statusmsg[TOX_MAX_STATUS_MESSAGE_LENGTH + 1];
    size_t statusmsg_len;
    char pub_key[TOX_PUBLIC_KEY_SIZE];
    uint32_t num;
    int chatwin;
    bool active;
    Tox_Connection connection_status;
    bool is_typing;
    bool logging_on;    /* saves preference for friend irrespective of config settings */
    bool auto_accept_files;  /* same as above; default should always be false */
    Tox_User_Status status;

    struct LastOnline last_online;

#ifdef GAMES
    struct GameInvite game_invite;
#endif

    struct ConferenceInvite conference_invite;
    struct GroupInvite group_invite;

    struct FileTransfer file_receiver[MAX_FILES];
    struct FileTransfer file_sender[MAX_FILES];
    PendingFileTransfer file_send_queue[MAX_FILES];

    Friend_Settings settings;
} ToxicFriend;

typedef struct {
    char name[TOXIC_MAX_NAME_LENGTH + 1];
    uint16_t namelength;
    char pub_key[TOX_PUBLIC_KEY_SIZE];
    uint32_t num;
    bool active;
    uint64_t last_on;
} BlockedFriend;

typedef struct {
    int num_selected;
    size_t num_friends;
    size_t num_online;
    size_t max_idx;    /* 1 + the index of the last friend in list */
    uint32_t *index;
    ToxicFriend *list;
} FriendsList;

extern FriendsList Friends;

ToxWindow *new_friendlist(void);
void friendlist_onInit(ToxWindow *self, Toxic *toxic);
void disable_chatwin(uint32_t f_num);
int get_friendnum(uint8_t *name);
void kill_friendlist(ToxWindow *self, const Client_Config *c_config);
void friendlist_onFriendAdded(ToxWindow *self, Toxic *toxic, uint32_t num, bool sort);
Tox_User_Status get_friend_status(uint32_t friendnumber);
Tox_Connection get_friend_connection_status(uint32_t friendnumber);

/*
 * Loads the list of blocked peers from `path`.
 *
 * Returns 0 on success or if the file doesn't exist (a file will only exist if
 *   the client has actually blocked someone).
 * Returns -1 on failure.
 */
int load_blocklist(char *path);

/* sorts friendlist_index first by connection status then alphabetically */
void sort_friendlist_index(void);

/*
 * Returns true if friend associated with `public_key` is in the block list.
 *
 * `public_key` must be at least TOX_PUBLIC_KEY_SIZE bytes.
 */
bool friend_is_blocked(const char *public_key);

/*
 * Enable or disable auto-accepting file transfers for this friend.
 */
void friend_set_auto_file_accept(uint32_t friendnumber, bool auto_accept);

/*
 * Return true if auto-accepting file transfers is enabled for this friend.
 */
bool friend_get_auto_accept_files(uint32_t friendnumber);

/*
 * Enable or disable logging for this friend.
 */
void friend_set_logging_enabled(uint32_t friendnumber, bool enable_log);

/*
 * Return true if logging is currently enabled for this friend.
 */
bool friend_get_logging_enabled(uint32_t friendnumber);

/*
 * Sets the tab name colour config option for the friend associated with `public_key` to `colour`.
 *
 * Return true on success.
 */
bool friend_config_set_tab_name_colour(const char *public_key, const char *colour);

/*
 * Returns a friend's tab name colour.
 * Returns -1 on error.
 */
int friend_config_get_tab_name_colour(uint32_t friendnumber);

/*
 * Sets the autolog config option for the friend associated with `public_key`.
 *
 * Return true on success.
 */
bool friend_config_set_autolog(const char *public_key, bool autolog_enabled);

/*
 * Returns the friend's config setting for autologging.
 *
 * Note: To determine if logging for a friend is currently enabled,
 * use `friend_get_logging_enabled()`.
 */
bool friend_config_get_autolog(uint32_t friendnumber);

/*
 * Sets the auto-accept file transfers config option for the friend associated with
 * `public_key`.
 *
 * Return true on success.
 */
bool friend_config_set_auto_accept_files(const char *public_key, bool autoaccept_files);

/*
 * Returns the friend's config setting for auto-accept file transfers.
 *
 * Note: To determine if auto-accepting files for a friend is currently enabled,
 * use `friend_get_auto_accept_files()`.
 */
bool friend_config_get_auto_accept_files(uint32_t friendnumber);

#endif /* end of include guard: FRIENDLIST_H */

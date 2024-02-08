/*  settings.h
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

#ifndef SETTINGS_H
#define SETTINGS_H

#include <limits.h>

#include <tox/tox.h>

#include "run_options.h"
#include "toxic_constants.h"

/* Represents line_* hints max strlen */
#define LINE_HINT_MAX 3

#define PASSWORD_EVAL_MAX 512

/* Holds user setting values defined in the toxic config file. */
typedef struct Client_Config {
    int autolog;           /* boolean */
    int alerts;            /* boolean */
    int show_notification_content; /* boolean */

    /* boolean (is set to NT_BEEP or 0 after loading) */
    int bell_on_message;
    int bell_on_filetrans;
    int bell_on_filetrans_accept;
    int bell_on_invite;

    int timestamps;        /* boolean */
    char timestamp_format[TIME_STR_SIZE];
    char log_timestamp_format[TIME_STR_SIZE];

    int colour_theme;      /* boolean (0 for default toxic colours) */
    int history_size;      /* int between MIN_HISTORY and MAX_HISTORY */
    int notification_timeout;
    int show_typing_self;  /* boolean */
    int show_typing_other; /* boolean */
    int show_welcome_msg;  /* boolean */
    int show_connection_msg;  /* boolean */
    int show_group_connection_msg;  /* boolean */
    int nodeslist_update_freq;  /* int (<= 0 to disable updates) */
    int autosave_freq; /* int (<= 0 to disable autosave) */

    char line_join[LINE_HINT_MAX + 1];
    char line_quit[LINE_HINT_MAX + 1];
    char line_alert[LINE_HINT_MAX + 1];
    char line_normal[LINE_HINT_MAX + 1];
    char line_special[LINE_HINT_MAX + 1];

    char download_path[PATH_MAX];
    char chatlogs_path[PATH_MAX];
    char avatar_path[PATH_MAX];
    char autorun_path[PATH_MAX];
    char password_eval[PASSWORD_EVAL_MAX];

    char color_bar_bg[COLOR_STR_SIZE];
    char color_bar_fg[COLOR_STR_SIZE];
    char color_bar_accent[COLOR_STR_SIZE];
    char color_bar_notify[COLOR_STR_SIZE];

    int key_next_tab;
    int key_prev_tab;
    int key_scroll_line_up;
    int key_scroll_line_down;
    int key_half_page_up;
    int key_half_page_down;
    int key_page_bottom;
    int key_toggle_peerlist;
    int key_toggle_pastemode;

    int mplex_away; /* boolean (1 for reaction to terminal attach/detach) */
    char mplex_away_note [TOX_MAX_STATUS_MESSAGE_LENGTH];
    char group_part_message[TOX_GROUP_MAX_PART_LENGTH];

#ifdef AUDIO
    int audio_in_dev;
    int audio_out_dev;
    double VAD_threshold;
    int conference_audio_channels;
    int chat_audio_channels;
    int push_to_talk;      /* boolean */
#endif
} Client_Config;

enum settings_values {
    AUTOLOG_OFF = 0,
    AUTOLOG_ON = 1,

    TIMESTAMPS_OFF = 0,
    TIMESTAMPS_ON = 1,

    ALERTS_DISABLED = 0,
    ALERTS_ENABLED = 1,

    SHOW_NOTIFICATION_CONTENT_ON = 1,

    DFLT_COLS = 0,
    NATIVE_COLS = 1,

    SHOW_TYPING_OFF = 0,
    SHOW_TYPING_ON = 1,

    SHOW_WELCOME_MSG_OFF = 0,
    SHOW_WELCOME_MSG_ON = 1,

    SHOW_CONNECTION_MSG_OFF = 0,
    SHOW_CONNECTION_MSG_ON = 1,

    SHOW_GROUP_CONNECTION_MSG_OFF = 0,
    SHOW_GROUP_CONNECTION_MSG_ON = 1,

    DFLT_HST_SIZE = 700,

    MPLEX_OFF = 0,
    MPLEX_ON = 1,
};

#define LINE_JOIN    "-->"
#define LINE_QUIT    "<--"
#define LINE_ALERT   "-!-"
#define LINE_NORMAL  "-"
#define LINE_SPECIAL ">"
#define TIMESTAMP_DEFAULT      "%H:%M"
#define LOG_TIMESTAMP_DEFAULT  "%Y/%m/%d [%H:%M:%S]"
#define MPLEX_AWAY_NOTE "Away from keyboard, be back soon!"

/*
 * Loads the config file into `run_opts` and creates an empty file if it does not
 * already exist. This function must be called before any other `settings_load` function.
 *
 * If the client has set a custom config file via the run option it will be
 * prioritized.
 *
 * If the client is using the default tox data file and has not specified a custom
 * config file, the default config file in the user config directory is used
 *
 * If the client is using a custom tox data file, the config path and filename will be
 * identical to those of the data file, but with a `.conf` file extension.
 *
 * `data_path` is the name of the tox profile being used.
 *
 * Returns true if config file is successfully loaded.
 */
bool settings_load_config_file(Run_Options *run_opts, const char *data_path);

/*
 * Loads general toxic settings from the toxic config file pointed to by `patharg'.
 *
 * Return 0 on success.
 * Return -1 if we fail to open the file path.
 * Return -2 if libconfig fails to read the config file.
 */
int settings_load_main(Client_Config *s, const Run_Options *run_opts);

/*
 * Loads friend config settings from the toxic config file pointed to by `patharg`.
 * If `patharg` is null, the default config path is used.
 *
 * Return 0 on success (or if no config entry for friends exists).
 * Return -1 if we fail to open the file path.
 * Return -2 if libconfig fails to read the config file.
 *
 * This function will have no effect on friends that are added in the future.
 */
int settings_load_friends(const Run_Options *run_opts);

/*
 * Loads groupchat config settings from the toxic config file pointed to by `patharg`.
 * If `patharg` is null, the default config path is used.
 *
 * Return 0 on success (or if no config entry for groupchats exists).
 * Return -1 if we fail to open the file path.
 * Return -2 if libconfig fails to read the config file.
 *
 * This function will have no effect on groupchat instances that are created in the future.
 */
int settings_load_groups(const Run_Options *run_opts);

/*
 * Loads conference config settings from the toxic config file pointed to by `patharg`.
 * If `patharg` is null, the default config path is used.
 *
 * Return 0 on success (or if no config entry for conferences exists).
 * Return -1 if we fail to open the file path.
 * Return -2 if libconfig fails to read the config file.
 *
 * This function will have no effect on conference instances that are created in the future.
 */
int settings_load_conferences(const Run_Options *run_opts);

#endif /* SETTINGS_H */

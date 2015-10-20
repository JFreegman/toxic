/*  settings.c
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

#include <stdlib.h>
#include <string.h>
#include <libconfig.h>
#include <ctype.h>

#include "toxic.h"
#include "windows.h"
#include "configdir.h"
#include "notify.h"
#include "misc_tools.h"

#ifdef AUDIO
    #include "audio_device.h"
#endif /* AUDIO */

#include "settings.h"
#include "line_info.h"

#ifndef PACKAGE_DATADIR
    #define PACKAGE_DATADIR "."
#endif

#define NO_SOUND "silent"

static struct ui_strings {
    const char* self;
    const char* timestamps;
    const char* time_format;
    const char* timestamp_format;
    const char* log_timestamp_format;
    const char* alerts;
    const char* native_colors;
    const char* autolog;
    const char* history_size;
    const char* show_typing_self;
    const char* show_typing_other;
    const char* show_welcome_msg;

    const char* line_join;
    const char* line_quit;
    const char* line_alert;
    const char* line_normal;

    const char* mplex_away;
    const char* mplex_away_note;
} ui_strings = {
    "ui",
    "timestamps",
    "time_format",
    "timestamp_format",
    "log_timestamp_format",
    "alerts",
    "native_colors",
    "autolog",
    "history_size",
    "show_typing_self",
    "show_typing_other",
    "show_welcome_msg",
    "line_join",
    "line_quit",
    "line_alert",
    "line_normal",
    "mplex_away",
    "mplex_away_note",
};

static void ui_defaults(struct user_settings* settings)
{
    settings->timestamps = TIMESTAMPS_ON;
    snprintf(settings->timestamp_format, sizeof(settings->timestamp_format), "%s", TIMESTAMP_DEFAULT);
    snprintf(settings->log_timestamp_format, sizeof(settings->log_timestamp_format), "%s", LOG_TIMESTAMP_DEFAULT);

    settings->autolog = AUTOLOG_OFF;
    settings->alerts = ALERTS_ENABLED;
    settings->colour_theme = DFLT_COLS;
    settings->history_size = 700;
    settings->show_typing_self = SHOW_TYPING_ON;
    settings->show_typing_other = SHOW_TYPING_ON;
    settings->show_welcome_msg = SHOW_WELCOME_MSG_ON;

    snprintf(settings->line_join, LINE_HINT_MAX + 1, "%s", LINE_JOIN);
    snprintf(settings->line_quit, LINE_HINT_MAX + 1, "%s", LINE_QUIT);
    snprintf(settings->line_alert, LINE_HINT_MAX + 1, "%s", LINE_ALERT);
    snprintf(settings->line_normal, LINE_HINT_MAX + 1, "%s", LINE_NORMAL);

    settings->mplex_away = MPLEX_ON;
    snprintf (settings->mplex_away_note,
              sizeof (settings->mplex_away_note),
              "%s",
              MPLEX_AWAY_NOTE);
}

static const struct keys_strings {
    const char* self;
    const char* next_tab;
    const char* prev_tab;
    const char* scroll_line_up;
    const char* scroll_line_down;
    const char* half_page_up;
    const char* half_page_down;
    const char* page_bottom;
    const char* peer_list_up;
    const char* peer_list_down;
    const char* toggle_peerlist;
} key_strings = {
    "keys",
    "next_tab",
    "prev_tab",
    "scroll_line_up",
    "scroll_line_down",
    "half_page_up",
    "half_page_down",
    "page_bottom",
    "peer_list_up",
    "peer_list_down",
    "toggle_peerlist",
};

/* defines from toxic.h */
static void key_defaults(struct user_settings* settings)
{
    settings->key_next_tab = T_KEY_NEXT;
    settings->key_prev_tab = T_KEY_PREV;
    settings->key_scroll_line_up = KEY_PPAGE;
    settings->key_scroll_line_down = KEY_NPAGE;
    settings->key_half_page_up = T_KEY_C_F;
    settings->key_half_page_down = T_KEY_C_V;
    settings->key_page_bottom = T_KEY_C_H;
    settings->key_peer_list_up = T_KEY_C_LB;
    settings->key_peer_list_down = T_KEY_C_RB;
    settings->key_toggle_peerlist = T_KEY_C_B;
}

static const struct tox_strings {
    const char* self;
    const char* download_path;
    const char* chatlogs_path;
    const char* avatar_path;
} tox_strings = {
    "tox",
    "download_path",
    "chatlogs_path",
    "avatar_path",
};

static void tox_defaults(struct user_settings* settings)
{
    strcpy(settings->download_path, "");
    strcpy(settings->chatlogs_path, "");
    strcpy(settings->avatar_path, "");
}

#ifdef AUDIO
static const struct audio_strings {
    const char* self;
    const char* input_device;
    const char* output_device;
    const char* VAD_treshold;
} audio_strings = {
    "audio",
    "input_device",
    "output_device",
    "VAD_treshold",
};

static void audio_defaults(struct user_settings* settings)
{
    settings->audio_in_dev = 0;
    settings->audio_out_dev = 0;
    settings->VAD_treshold = 40.0;
}
#endif

#ifdef SOUND_NOTIFY
static const struct sound_strings {
    const char* self;
    const char* notif_error;
    const char* self_log_in;
    const char* self_log_out;
    const char* user_log_in;
    const char* user_log_out;
    const char* call_incoming;
    const char* call_outgoing;
    const char* generic_message;
    const char* transfer_pending;
    const char* transfer_completed;
} sound_strings = {
    "sounds",
    "notif_error",
    "self_log_in",
    "self_log_out",
    "user_log_in",
    "user_log_out",
    "call_incoming",
    "call_outgoing",
    "generic_message",
    "transfer_pending",
    "transfer_completed",
};
#endif

static int key_parse(const char** bind){
    int len = strlen(*bind);

    if (len > 5) {
        if(strncasecmp(*bind, "ctrl+", 5) == 0)
            return toupper(bind[0][5]) - 'A' + 1;
    }

    if (strncasecmp(*bind, "tab", 3) == 0)
        return T_KEY_TAB;

    if (strncasecmp(*bind, "page", 4) == 0)
        return len == 6 ? KEY_PPAGE : KEY_NPAGE;

    return -1;
}

int settings_load(struct user_settings *s, const char *patharg)
{
    config_t cfg[1];
    config_setting_t *setting;
    const char *str = NULL;

    /* Load default settings */
    ui_defaults(s);
    tox_defaults(s);
    key_defaults(s);

#ifdef AUDIO
    audio_defaults(s);
#endif

    config_init(cfg);

    char path[MAX_STR_SIZE];

    /* use default config file path */
    if (patharg == NULL) {
        char *user_config_dir = get_user_config_dir();
        snprintf(path, sizeof(path), "%s%stoxic.conf", user_config_dir, CONFIGDIR);
        free(user_config_dir);

        /* make sure path exists or is created on first time running */
        if (!file_exists(path)) {
            FILE *fp = fopen(path, "w");

            if (fp == NULL)
                return -1;

            fclose(fp);
        }
    } else {
        snprintf(path, sizeof(path), "%s", patharg);
    }

    if (!config_read_file(cfg, path)) {
        config_destroy(cfg);
        return -1;
    }

    /* ui */
    if ((setting = config_lookup(cfg, ui_strings.self)) != NULL) {
        config_setting_lookup_bool(setting, ui_strings.timestamps, &s->timestamps);

        int time = 24;
        if ( config_setting_lookup_int(setting, ui_strings.time_format, &time) ) {
            if (time == 12) {
                snprintf(s->timestamp_format, sizeof(s->timestamp_format), "%s", "%I:%M:%S %p");
                snprintf(s->log_timestamp_format, sizeof(s->log_timestamp_format), "%s", "%Y/%m/%d [%I:%M:%S %p]");
            }
        }

        if ( config_setting_lookup_string(setting, ui_strings.timestamp_format, &str) ) {
            snprintf(s->timestamp_format, sizeof(s->timestamp_format), "%s", str);
        }

        if ( config_setting_lookup_string(setting, ui_strings.log_timestamp_format, &str) ) {
            snprintf(s->log_timestamp_format, sizeof(s->log_timestamp_format), "%s", str);
        }

        config_setting_lookup_bool(setting, ui_strings.alerts, &s->alerts);
        config_setting_lookup_bool(setting, ui_strings.autolog, &s->autolog);
        config_setting_lookup_bool(setting, ui_strings.native_colors, &s->colour_theme);
        config_setting_lookup_int(setting, ui_strings.history_size, &s->history_size);
        config_setting_lookup_bool(setting, ui_strings.show_typing_self, &s->show_typing_self);
        config_setting_lookup_bool(setting, ui_strings.show_typing_other, &s->show_typing_other);
        config_setting_lookup_bool(setting, ui_strings.show_welcome_msg, &s->show_welcome_msg);

        if ( config_setting_lookup_string(setting, ui_strings.line_join, &str) ) {
            snprintf(s->line_join, sizeof(s->line_join), "%s", str);
        }
        if ( config_setting_lookup_string(setting, ui_strings.line_quit, &str) ) {
            snprintf(s->line_quit, sizeof(s->line_quit), "%s", str);
        }
        if ( config_setting_lookup_string(setting, ui_strings.line_alert, &str) ) {
            snprintf(s->line_alert, sizeof(s->line_alert), "%s", str);
        }
        if ( config_setting_lookup_string(setting, ui_strings.line_normal, &str) ) {
            snprintf(s->line_normal, sizeof(s->line_normal), "%s", str);
        }

        config_setting_lookup_bool (setting, ui_strings.mplex_away, &s->mplex_away);

        if (config_setting_lookup_string (setting, ui_strings.mplex_away_note, &str)) {
            snprintf (s->mplex_away_note, sizeof (s->mplex_away_note), "%s", str);
        }
    }

    /* paths */
    if ((setting = config_lookup(cfg, tox_strings.self)) != NULL) {
        if ( config_setting_lookup_string(setting, tox_strings.download_path, &str) ) {
            snprintf(s->download_path, sizeof(s->download_path), "%s", str);
            int len = strlen(s->download_path);

            /* make sure path ends with a '/' */
            if (len >= sizeof(s->download_path) - 2)
                s->download_path[0] = '\0';
            else if (s->download_path[len - 1] != '/')
                strcat(&s->download_path[len - 1], "/");
        }

        if ( config_setting_lookup_string(setting, tox_strings.chatlogs_path, &str) ) {
            snprintf(s->chatlogs_path, sizeof(s->chatlogs_path), "%s", str);
            int len = strlen(s->chatlogs_path);

            if (len >= sizeof(s->chatlogs_path) - 2)
                s->chatlogs_path[0] = '\0';
            else if (s->chatlogs_path[len - 1] != '/')
                strcat(&s->chatlogs_path[len - 1], "/");
        }

        if ( config_setting_lookup_string(setting, tox_strings.avatar_path, &str) ) {
            snprintf(s->avatar_path, sizeof(s->avatar_path), "%s", str);
            int len = strlen(str);

            if (len >= sizeof(s->avatar_path))
                s->avatar_path[0] = '\0';
        }
    }

    /* keys */
    if ((setting = config_lookup(cfg, key_strings.self)) != NULL) {
        const char* tmp = NULL;
        if (config_setting_lookup_string(setting, key_strings.next_tab, &tmp))
            s->key_next_tab = key_parse(&tmp);
        if (config_setting_lookup_string(setting, key_strings.prev_tab, &tmp))
            s->key_prev_tab = key_parse(&tmp);
        if (config_setting_lookup_string(setting, key_strings.scroll_line_up, &tmp))
            s->key_scroll_line_up = key_parse(&tmp);
        if (config_setting_lookup_string(setting, key_strings.scroll_line_down, &tmp))
            s->key_scroll_line_down= key_parse(&tmp);
        if (config_setting_lookup_string(setting, key_strings.half_page_up, &tmp))
            s->key_half_page_up = key_parse(&tmp);
        if (config_setting_lookup_string(setting, key_strings.half_page_down, &tmp))
            s->key_half_page_down = key_parse(&tmp);
        if (config_setting_lookup_string(setting, key_strings.page_bottom, &tmp))
            s->key_page_bottom = key_parse(&tmp);
        if (config_setting_lookup_string(setting, key_strings.peer_list_up, &tmp))
            s->key_peer_list_up = key_parse(&tmp);
        if (config_setting_lookup_string(setting, key_strings.peer_list_down, &tmp))
            s->key_peer_list_down = key_parse(&tmp);
        if (config_setting_lookup_string(setting, key_strings.toggle_peerlist, &tmp))
            s->key_toggle_peerlist = key_parse(&tmp);
    }

#ifdef AUDIO
    if ((setting = config_lookup(cfg, audio_strings.self)) != NULL) {
        config_setting_lookup_int(setting, audio_strings.input_device, &s->audio_in_dev);
        s->audio_in_dev = s->audio_in_dev < 0 || s->audio_in_dev > MAX_DEVICES ? 0 : s->audio_in_dev;

        config_setting_lookup_int(setting, audio_strings.output_device, &s->audio_out_dev);
        s->audio_out_dev = s->audio_out_dev < 0 || s->audio_out_dev > MAX_DEVICES ? 0 : s->audio_out_dev;

        config_setting_lookup_float(setting, audio_strings.VAD_treshold, &s->VAD_treshold);
    }
#endif

#ifdef SOUND_NOTIFY
    if ((setting = config_lookup(cfg, sound_strings.self)) != NULL) {
        if ( (config_setting_lookup_string(setting, sound_strings.notif_error, &str) != CONFIG_TRUE) ||
                !set_sound(notif_error, str) ) {
            if (str && strcasecmp(str, NO_SOUND) != 0)
                set_sound(notif_error, PACKAGE_DATADIR "/sounds/ToxicError.wav");
        }

        if ( !config_setting_lookup_string(setting, sound_strings.user_log_in, &str) ||
                !set_sound(user_log_in, str) ) {
            if (str && strcasecmp(str, NO_SOUND) != 0)
                set_sound(user_log_in, PACKAGE_DATADIR "/sounds/ToxicContactOnline.wav");
        }

        if ( !config_setting_lookup_string(setting, sound_strings.user_log_out, &str) ||
                !set_sound(user_log_out, str) ) {
            if (str && strcasecmp(str, NO_SOUND) != 0)
                set_sound(user_log_out, PACKAGE_DATADIR "/sounds/ToxicContactOffline.wav");
        }

        if ( !config_setting_lookup_string(setting, sound_strings.call_incoming, &str) ||
                !set_sound(call_incoming, str) ) {
            if (str && strcasecmp(str, NO_SOUND) != 0)
                set_sound(call_incoming, PACKAGE_DATADIR "/sounds/ToxicIncomingCall.wav");
        }

        if ( !config_setting_lookup_string(setting, sound_strings.call_outgoing, &str) ||
                !set_sound(call_outgoing, str) ) {
            if (str && strcasecmp(str, NO_SOUND) != 0)
                set_sound(call_outgoing, PACKAGE_DATADIR "/sounds/ToxicOutgoingCall.wav");
        }

        if ( !config_setting_lookup_string(setting, sound_strings.generic_message, &str) ||
                !set_sound(generic_message, str) ) {
            if (str && strcasecmp(str, NO_SOUND) != 0)
                set_sound(generic_message, PACKAGE_DATADIR "/sounds/ToxicRecvMessage.wav");
        }

        if ( !config_setting_lookup_string(setting, sound_strings.transfer_pending, &str) ||
                !set_sound(transfer_pending, str) ) {
            if (str && strcasecmp(str, NO_SOUND) != 0)
                set_sound(transfer_pending, PACKAGE_DATADIR "/sounds/ToxicTransferStart.wav");
        }

        if ( !config_setting_lookup_string(setting, sound_strings.transfer_completed, &str) ||
                !set_sound(transfer_completed, str) ) {
            if (str && strcasecmp(str, NO_SOUND) != 0)
                set_sound(transfer_completed, PACKAGE_DATADIR "/sounds/ToxicTransferComplete.wav");
        }
    }
    else {
        set_sound(notif_error, PACKAGE_DATADIR "/sounds/ToxicError.wav");
        set_sound(user_log_in, PACKAGE_DATADIR "/sounds/ToxicContactOnline.wav");
        set_sound(user_log_out, PACKAGE_DATADIR "/sounds/ToxicContactOffline.wav");
        set_sound(call_incoming, PACKAGE_DATADIR "/sounds/ToxicIncomingCall.wav");
        set_sound(call_outgoing, PACKAGE_DATADIR "/sounds/ToxicOutgoingCall.wav");
        set_sound(generic_message, PACKAGE_DATADIR "/sounds/ToxicRecvMessage.wav");
        set_sound(transfer_pending, PACKAGE_DATADIR "/sounds/ToxicTransferStart.wav");
        set_sound(transfer_completed, PACKAGE_DATADIR "/sounds/ToxicTransferComplete.wav");
    }
#endif

    config_destroy(cfg);
    return 0;
}

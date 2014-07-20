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

#include "toxic.h"
#include "windows.h"
#include "configdir.h"
#include "notify.h"

#ifdef _SUPPORT_AUDIO
    #include "device.h"
#endif /* _SUPPORT_AUDIO */

#include "settings.h"
#include "line_info.h"

const struct _ui_strings {
    const char* self;
    const char* timestamps;
    const char* alerts;
    const char* native_colors;
    const char* autolog;
    const char* time_format;
    const char* history_size;
} ui_strings = {
    "ui",
    "timestamps",
    "alerts",
    "native_colors",
    "autolog",
    "time_format",
    "history_size"
};
static void ui_defaults(struct user_settings* settings) 
{
    settings->timestamps = TIMESTAMPS_ON;
    settings->time = TIME_24;
    settings->autolog = AUTOLOG_OFF;
    settings->alerts = ALERTS_ENABLED;
    settings->colour_theme = DFLT_COLS;
    settings->history_size = 700;
}

const struct _tox_strings {
    const char* self;
    const char* download_path;
} tox_strings = {
    "tox",
    "download_path",
};

static void tox_defaults(struct user_settings* settings)
{
    /*settings->download_path;*/ /* TODO: Set this? */
}

#ifdef _SUPPORT_AUDIO
const struct _audio_strings {
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

#ifdef _ENABLE_SOUND_NOTIFY
const struct _sound_strings {
    const char* self;
    const char* error;
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
    "error",
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

int settings_load(struct user_settings *s, char *path)
{
    config_t cfg[1];
    config_setting_t *setting;
    const char *str;
    
    /* Load default settings */
    ui_defaults(s);
    tox_defaults(s);
#ifdef _SUPPORT_AUDIO
    audio_defaults(s);
#endif
    
    config_init(cfg);
    
    if(!config_read_file(cfg, path))
    {
        config_destroy(cfg);
        return -1;
    }
    
    /* ui */
    if ((setting = config_lookup(cfg, ui_strings.self)) != NULL) {
        config_setting_lookup_bool(setting, ui_strings.timestamps, &s->timestamps);
        config_setting_lookup_bool(setting, ui_strings.alerts, &s->alerts);
        config_setting_lookup_bool(setting, ui_strings.autolog, &s->autolog);
        config_setting_lookup_bool(setting, ui_strings.native_colors, &s->colour_theme);
        
        config_setting_lookup_int(setting, ui_strings.history_size, &s->history_size);
        config_setting_lookup_int(setting, ui_strings.time_format, &s->time);
        s->time = s->time == TIME_24 || s->time == TIME_12 ? s->time : TIME_24; /* Check defaults */
    }
    
    if ((setting = config_lookup(cfg, tox_strings.self)) != NULL) {
        if ( config_setting_lookup_string(setting, tox_strings.download_path, &str) ) {
            s->download_path = calloc(1, strlen(str) + 1);
            strcpy(s->download_path, str);
        }
    }
    
#ifdef _SUPPORT_AUDIO
    if ((setting = config_lookup(cfg, audio_strings.self)) != NULL) {
        config_setting_lookup_int(setting, audio_strings.input_device, &s->audio_in_dev);
        s->audio_in_dev = s->audio_in_dev < 0 || s->audio_in_dev > MAX_DEVICES ? 0 : s->audio_in_dev;
        
        config_setting_lookup_int(setting, audio_strings.output_device, &s->audio_out_dev);
        s->audio_out_dev = s->audio_out_dev < 0 || s->audio_out_dev > MAX_DEVICES ? 0 : s->audio_out_dev;
        
        config_setting_lookup_float(setting, audio_strings.VAD_treshold, &s->VAD_treshold);
    }    
#endif

#ifdef _ENABLE_SOUND_NOTIFY
    if ((setting = config_lookup(cfg, sound_strings.self)) != NULL) {
        if ( config_setting_lookup_string(setting, sound_strings.error, &str) == CONFIG_TRUE ) 
            set_sound(error, str);
                
        if ( config_setting_lookup_string(setting, sound_strings.user_log_in, &str) ) 
            set_sound(user_log_in, str);
        
        if ( config_setting_lookup_string(setting, sound_strings.self_log_in, &str) ) 
            set_sound(self_log_in, str);
        
        if ( config_setting_lookup_string(setting, sound_strings.user_log_out, &str) ) 
            set_sound(user_log_out, str);
        
        if ( config_setting_lookup_string(setting, sound_strings.self_log_out, &str) ) 
            set_sound(self_log_out, str);
        
        if ( config_setting_lookup_string(setting, sound_strings.call_incoming, &str) ) 
            set_sound(call_incoming, str);        
        
        if ( config_setting_lookup_string(setting, sound_strings.call_outgoing, &str) ) 
            set_sound(call_outgoing, str);
        
        if ( config_setting_lookup_string(setting, sound_strings.generic_message, &str) ) 
            set_sound(generic_message, str);
        
        if ( config_setting_lookup_string(setting, sound_strings.transfer_pending, &str) ) 
            set_sound(transfer_pending, str);
        
        if ( config_setting_lookup_string(setting, sound_strings.transfer_completed, &str) ) 
            set_sound(transfer_completed, str);
    }
#endif

    config_destroy(cfg);
    return 0;
}
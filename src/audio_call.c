/*  audio_call.c
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

#include "audio_call.h"
#include "audio_device.h"
#include "chat_commands.h"
#include "chat.h"
#include "friendlist.h"
#include "global_commands.h"
#include "line_info.h"
#include "misc_tools.h"
#include "notify.h"
#include "settings.h"
#include "toxic.h"
#include "windows.h"

#ifdef AUDIO

#ifdef VIDEO
#include "video_call.h"
#endif /* VIDEO */

#include <assert.h>
#include <curses.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#ifdef __APPLE__
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
/* compatibility with older versions of OpenAL */
#ifndef ALC_ALL_DEVICES_SPECIFIER
#include <AL/alext.h>
#endif /* ALC_ALL_DEVICES_SPECIFIER */
#endif /* __APPLE__ */

extern FriendsList Friends;
extern ToxWindow *windows[MAX_WINDOWS_NUM];

struct CallControl CallControl;

extern struct user_settings *user_settings;
extern struct Winthread Winthread;

void on_call(ToxAV *av, uint32_t friend_number, bool audio_enabled, bool video_enabled,
             void *user_data);
void on_call_state(ToxAV *av, uint32_t friend_number, uint32_t state, void *user_data);
void on_audio_receive_frame(ToxAV *av, uint32_t friend_number, int16_t const *pcm, size_t sample_count,
                            uint8_t channels, uint32_t sampling_rate, void *user_data);

void callback_recv_invite(Tox *m, uint32_t friend_number);
void callback_recv_ringing(uint32_t friend_number);
void callback_recv_starting(uint32_t friend_number);
void callback_recv_ending(uint32_t friend_number);
void callback_call_started(uint32_t friend_number);
void callback_call_canceled(uint32_t friend_number);
void callback_call_rejected(uint32_t friend_number);
void callback_call_ended(uint32_t friend_number);

void write_device_callback(uint32_t friend_number, const int16_t *PCM, uint16_t sample_count, uint8_t channels,
                           uint32_t sample_rate);

void audio_bit_rate_callback(ToxAV *av, uint32_t friend_number, uint32_t audio_bit_rate, void *user_data);

static void print_err(ToxWindow *self, const char *error_str)
{
    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "%s", error_str);
}

ToxAV *init_audio(ToxWindow *self, Tox *tox)
{
    Toxav_Err_New error;
    CallControl.audio_errors = ae_None;
    CallControl.prompt = self;

    CallControl.av = toxav_new(tox, &error);

    CallControl.audio_enabled = true;
    CallControl.default_audio_bit_rate = 64;
    CallControl.audio_sample_rate = 48000;
    CallControl.audio_frame_duration = 20;
    CallControl.audio_channels = user_settings->chat_audio_channels;

    CallControl.video_enabled = false;
    CallControl.default_video_bit_rate = 0;
    CallControl.video_frame_duration = 0;

    if (!CallControl.av) {
        CallControl.audio_errors |= ae_StartingCoreAudio;
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to init ToxAV");

        return NULL;
    }

    if (init_devices() == de_InternalError) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to init devices");
        toxav_kill(CallControl.av);

        return CallControl.av = NULL;
    }

    toxav_callback_call(CallControl.av, on_call, tox);
    toxav_callback_call_state(CallControl.av, on_call_state, NULL);
    toxav_callback_audio_receive_frame(CallControl.av, on_audio_receive_frame, NULL);
    toxav_callback_audio_bit_rate(CallControl.av, audio_bit_rate_callback, NULL);

    return CallControl.av;
}

void read_device_callback(const int16_t *captured, uint32_t size, void *data)
{
    UNUSED_VAR(size);

    Toxav_Err_Send_Frame error;
    uint32_t friend_number = *((uint32_t *)data); /* TODO: Or pass an array of call_idx's */
    int64_t sample_count = ((int64_t) CallControl.audio_sample_rate) * \
                           ((int64_t) CallControl.audio_frame_duration) / 1000;

    if (sample_count <= 0) {
        return;
    }

    toxav_audio_send_frame(CallControl.av, friend_number,
                           captured, sample_count,
                           CallControl.audio_channels,
                           CallControl.audio_sample_rate, &error);
}

void write_device_callback(uint32_t friend_number, const int16_t *PCM, uint16_t sample_count, uint8_t channels,
                           uint32_t sample_rate)
{
    if (CallControl.calls[friend_number].status == cs_Active) {
        write_out(CallControl.calls[friend_number].out_idx, PCM, sample_count, channels, sample_rate);
    }
}

bool init_call(Call *call)
{
    if (call->status != cs_None) {
        return false;
    }

    *call = (struct Call) {
        0
    };

    call->status = cs_Pending;

    call->in_idx = -1;
    call->out_idx = -1;
    call->audio_bit_rate = CallControl.default_audio_bit_rate;
#ifdef VIDEO
    call->vin_idx = -1;
    call->vout_idx = -1;
    call->video_width = CallControl.default_video_width;
    call->video_height = CallControl.default_video_height;
    call->video_bit_rate = CallControl.default_video_bit_rate;
#endif /* VIDEO */

    return true;
}

static bool cancel_call(Call *call)
{
    if (call->status != cs_Pending) {
        return false;
    }

    call->status = cs_None;

    return true;
}

static int start_transmission(ToxWindow *self, Call *call)
{
    if (!self || !CallControl.av) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to prepare audio transmission");
        return -1;
    }

    DeviceError error = open_input_device(&call->in_idx, read_device_callback, &self->num, false,
                                          CallControl.audio_sample_rate, CallControl.audio_frame_duration, CallControl.audio_channels);

    if (error != de_None) {
        if (error == de_FailedStart) {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to start audio input device");
        }

        if (error == de_InternalError) {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Internal error with opening audio input device");
        }
    }

    if (open_output_device(&call->out_idx,
                           CallControl.audio_sample_rate, CallControl.audio_frame_duration, CallControl.audio_channels) != de_None) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to open audio output device!");
    }

    return 0;
}

static void start_call(ToxWindow *self, Call *call)
{
    if (call->status != cs_Pending) {
        return;
    }

    if (start_transmission(self, call) != 0) {
        return;
    }

    call->status = cs_Active;

#ifdef VIDEO

    if (call->state & TOXAV_FRIEND_CALL_STATE_SENDING_V) {
        callback_recv_video_starting(self->num);
    }

    if (call->video_bit_rate) {
        start_video_transmission(self, CallControl.av, call);
    }

#endif
}

static int stop_transmission(Call *call, uint32_t friend_number)
{
    if (call->status != cs_Active) {
        return -1;
    }

    call->status = cs_None;

    if (call->in_idx != -1) {
        close_device(input, call->in_idx);
    }

    if (call->out_idx != -1) {
        close_device(output, call->out_idx);
    }

    Toxav_Err_Call_Control error = TOXAV_ERR_CALL_CONTROL_OK;

    if (call->state > TOXAV_FRIEND_CALL_STATE_FINISHED) {
        toxav_call_control(CallControl.av, friend_number, TOXAV_CALL_CONTROL_CANCEL, &error);
    }

    if (error != TOXAV_ERR_CALL_CONTROL_OK) {
        return -1;
    }

    return 0;
}

void terminate_audio(void)
{
    for (int i = 0; i < CallControl.max_calls; ++i) {
        stop_transmission(&CallControl.calls[i], i);
    }

    if (CallControl.av) {
        toxav_kill(CallControl.av);
    }

    terminate_devices();
}

/*
 * End of transmission
 */





/*
 * Callbacks
 */
void on_call(ToxAV *av, uint32_t friend_number, bool audio_enabled, bool video_enabled, void *user_data)
{
    UNUSED_VAR(av);

    Tox *m = (Tox *) user_data;

    Call *call = &CallControl.calls[friend_number];
    init_call(call);

    call->state = TOXAV_FRIEND_CALL_STATE_ACCEPTING_A | TOXAV_FRIEND_CALL_STATE_ACCEPTING_V;

    if (audio_enabled) {
        call->state |= TOXAV_FRIEND_CALL_STATE_SENDING_A;
    }

    if (video_enabled) {
        call->state |= TOXAV_FRIEND_CALL_STATE_SENDING_V;
    }

    callback_recv_invite(m, friend_number);
}

void on_call_state(ToxAV *av, uint32_t friend_number, uint32_t state, void *user_data)
{
    UNUSED_VAR(av);
    UNUSED_VAR(user_data);

    Call *call = &CallControl.calls[friend_number];

    if (call->status == cs_None) {
        return;
    }

    call->state = state;

    switch (state) {
        case TOXAV_FRIEND_CALL_STATE_ERROR:
        case TOXAV_FRIEND_CALL_STATE_FINISHED:
            if (state == TOXAV_FRIEND_CALL_STATE_ERROR) {
                line_info_add(CallControl.prompt, NULL, NULL, NULL, SYS_MSG, 0, 0, "ToxAV callstate error!");
            }

            if (call->status == cs_Pending) {
                cancel_call(call);
                callback_call_rejected(friend_number);
            } else {

#ifdef VIDEO
                callback_recv_video_end(friend_number);
                callback_video_end(friend_number);
#endif /* VIDEO */

                stop_transmission(call, friend_number);
                callback_call_ended(friend_number);
            }

            break;

        default:
            if (call->status == cs_Pending) {
                /* Start answered call */
                callback_call_started(friend_number);
            }

#ifdef VIDEO

            /* Handle receiving client video call states */
            if (state & TOXAV_FRIEND_CALL_STATE_SENDING_V) {
                callback_recv_video_starting(friend_number);
            } else {
                callback_recv_video_end(friend_number);
            }

#endif /* VIDEO */

            break;
    }
}

void on_audio_receive_frame(ToxAV *av, uint32_t friend_number,
                            int16_t const *pcm, size_t sample_count,
                            uint8_t channels, uint32_t sampling_rate, void *user_data)
{
    UNUSED_VAR(av);
    UNUSED_VAR(user_data);

    write_device_callback(friend_number, pcm, sample_count, channels, sampling_rate);
}

void audio_bit_rate_callback(ToxAV *av, uint32_t friend_number, uint32_t audio_bit_rate, void *user_data)
{
    UNUSED_VAR(user_data);

    Call *call = &CallControl.calls[friend_number];
    call->audio_bit_rate = audio_bit_rate;
    toxav_audio_set_bit_rate(av, friend_number, audio_bit_rate, NULL);
}

void callback_recv_invite(Tox *m, uint32_t friend_number)
{
    if (friend_number >= Friends.max_idx) {
        return;
    }

    if (Friends.list[friend_number].chatwin == -1) {
        if (get_num_active_windows() >= MAX_WINDOWS_NUM) {
            return;
        }

        Friends.list[friend_number].chatwin = add_window(m, new_chat(m, Friends.list[friend_number].num));
    }

    const Call *call = &CallControl.calls[friend_number];

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onInvite != NULL && windows[i]->num == friend_number) {
            windows[i]->onInvite(windows[i], CallControl.av, friend_number, call->state);
        }
    }
}
void callback_recv_ringing(uint32_t friend_number)
{
    const Call *call = &CallControl.calls[friend_number];

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onRinging != NULL && windows[i]->num == friend_number) {
            windows[i]->onRinging(windows[i], CallControl.av, friend_number, call->state);
        }
    }
}
void callback_recv_starting(uint32_t friend_number)
{
    Call *call = &CallControl.calls[friend_number];

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onStarting != NULL && windows[i]->num == friend_number) {
            windows[i]->onStarting(windows[i], CallControl.av, friend_number, call->state);

            start_call(windows[i], call);
        }
    }
}
void callback_recv_ending(uint32_t friend_number)
{
    const Call *call = &CallControl.calls[friend_number];

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onEnding != NULL && windows[i]->num == friend_number) {
            windows[i]->onEnding(windows[i], CallControl.av, friend_number, call->state);
        }
    }
}
void callback_call_started(uint32_t friend_number)
{
    Call *call = &CallControl.calls[friend_number];

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onStart != NULL && windows[i]->num == friend_number) {
            windows[i]->onStart(windows[i], CallControl.av, friend_number, call->state);

            start_call(windows[i], call);
        }
    }
}
void callback_call_canceled(uint32_t friend_number)
{
    const Call *call = &CallControl.calls[friend_number];

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onCancel != NULL && windows[i]->num == friend_number) {
            windows[i]->onCancel(windows[i], CallControl.av, friend_number, call->state);
        }
    }
}
void callback_call_rejected(uint32_t friend_number)
{
    const Call *call = &CallControl.calls[friend_number];

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onReject != NULL && windows[i]->num == friend_number) {
            windows[i]->onReject(windows[i], CallControl.av, friend_number, call->state);
        }
    }
}
void callback_call_ended(uint32_t friend_number)
{
    const Call *call = &CallControl.calls[friend_number];

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onEnd != NULL && windows[i]->num == friend_number) {
            windows[i]->onEnd(windows[i], CallControl.av, friend_number, call->state);
        }
    }
}

/*
 * End of Callbacks
 */


/*
 * Commands from chat_commands.h
 */
void cmd_call(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);
    UNUSED_VAR(m);
    UNUSED_VAR(argv);

    if (argc != 0) {
        print_err(self, "Unknown arguments.");
        return;
    }

    if (!CallControl.av) {
        print_err(self, "ToxAV not supported!");
        return;
    }

    if (!self->stb->connection) {
        print_err(self, "Friend is offline.");
        return;
    }

    Call *call = &CallControl.calls[self->num];

    if (call->status != cs_None) {
        print_err(self, "Already calling.");
        return;
    }

    init_call(call);

    place_call(self);
}

void cmd_answer(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);
    UNUSED_VAR(m);
    UNUSED_VAR(argv);

    Toxav_Err_Answer error;

    if (argc != 0) {
        print_err(self, "Unknown arguments.");
        return;
    }

    if (!CallControl.av) {
        print_err(self, "Audio not supported!");
        return;
    }

    Call *call = &CallControl.calls[self->num];

    if (call->status != cs_Pending) {
        print_err(self, "No incoming call!");
        return;
    }

    toxav_answer(CallControl.av, self->num, call->audio_bit_rate, call->video_bit_rate, &error);

    if (error != TOXAV_ERR_ANSWER_OK) {
        if (error == TOXAV_ERR_ANSWER_FRIEND_NOT_CALLING) {
            print_err(self, "No incoming call!");
        } else if (error == TOXAV_ERR_ANSWER_CODEC_INITIALIZATION) {
            print_err(self, "Failed to initialize codecs!");
        } else if (error == TOXAV_ERR_ANSWER_FRIEND_NOT_FOUND) {
            print_err(self, "Friend not found!");
        } else if (error == TOXAV_ERR_ANSWER_INVALID_BIT_RATE) {
            print_err(self, "Invalid bit rate!");
        } else {
            print_err(self, "Internal error!");
        }

        return;
    }

    /* Callback will print status... */
    callback_recv_starting(self->num);
}

void cmd_reject(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);
    UNUSED_VAR(m);
    UNUSED_VAR(argv);

    if (argc != 0) {
        print_err(self, "Unknown arguments.");
        return;
    }

    if (!CallControl.av) {
        print_err(self, "Audio not supported!");
        return;
    }

    Call *call = &CallControl.calls[self->num];

    if (call->status != cs_Pending) {
        print_err(self, "No incoming call!");
        return;
    }

    /* Manually send a cancel call control because call hasn't started */
    toxav_call_control(CallControl.av, self->num, TOXAV_CALL_CONTROL_CANCEL, NULL);
    cancel_call(call);

    /* Callback will print status... */
    callback_call_rejected(self->num);
}

void cmd_hangup(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);
    UNUSED_VAR(m);
    UNUSED_VAR(argv);

    if (!CallControl.av) {
        print_err(self, "Audio not supported!");
        return;
    }

    if (argc != 0) {
        print_err(self, "Unknown arguments.");
        return;
    }

    Call *call = &CallControl.calls[self->num];

    if (call->status == cs_None) {
        print_err(self, "Not in a call.");
        return;
    }

    stop_current_call(self);
}

void cmd_list_devices(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);
    UNUSED_VAR(m);

    if (argc != 1) {
        if (argc < 1) {
            print_err(self, "Type must be specified!");
        } else {
            print_err(self, "Only one argument allowed!");
        }

        return;
    }

    DeviceType type;

    if (strcasecmp(argv[1], "in") == 0) { /* Input devices */
        type = input;
    }

    else if (strcasecmp(argv[1], "out") == 0) { /* Output devices */
        type = output;
    }

    else {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Invalid type: %s", argv[1]);
        return;
    }

    // Refresh device list.
    get_al_device_names();

    print_al_devices(self, type);
}

/* This changes primary device only */
void cmd_change_device(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);
    UNUSED_VAR(m);

    if (argc != 2) {
        if (argc < 1) {
            print_err(self, "Type must be specified!");
        } else if (argc < 2) {
            print_err(self, "Must have id!");
        } else {
            print_err(self, "Only two arguments allowed!");
        }

        return;
    }

    DeviceType type;

    if (strcmp(argv[1], "in") == 0) { /* Input devices */
        type = input;
    }

    else if (strcmp(argv[1], "out") == 0) { /* Output devices */
        type = output;
    }

    else {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Invalid type: %s", argv[1]);
        return;
    }


    char *end;
    long int selection = strtol(argv[2], &end, 10);

    if (*end) {
        print_err(self, "Invalid input");
        return;
    }

    if (set_al_device(type, selection) == de_InvalidSelection) {
        print_err(self, "Invalid selection!");
        return;
    }
}

void cmd_mute(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);
    UNUSED_VAR(m);

    if (argc != 1) {
        print_err(self, "Specify type: \"/mute in\" or \"/mute out\".");
        return;
    }

    DeviceType type;

    if (strcasecmp(argv[1], "in") == 0) { /* Input devices */
        type = input;
    }

    else if (strcasecmp(argv[1], "out") == 0) { /* Output devices */
        type = output;
    }

    else {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Invalid type: %s", argv[1]);
        return;
    }


    /* If call is active, use this_call values */
    Call *this_call = &CallControl.calls[self->num];

    if (this_call->status == cs_Active) {
        if (type == input) {
            device_mute(type, this_call->in_idx);
            self->chatwin->infobox.in_is_muted = device_is_muted(type, this_call->in_idx);
        } else {
            device_mute(type, this_call->out_idx);
            self->chatwin->infobox.out_is_muted = device_is_muted(type, this_call->out_idx);
        }
    }

    return;
}

void cmd_sense(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);
    UNUSED_VAR(m);

    if (argc != 1) {
        if (argc < 1) {
            print_err(self, "Must have value!");
        } else {
            print_err(self, "Only two arguments allowed!");
        }

        return;
    }

    char *end;
    float value = strtof(argv[1], &end);

    if (*end) {
        print_err(self, "Invalid input");
        return;
    }

    const Call *call = &CallControl.calls[self->num];

    /* Call must be active */
    if (call->status == cs_Active) {
        device_set_VAD_threshold(call->in_idx, value);
        self->chatwin->infobox.vad_lvl = value;
    }

    return;
}

void cmd_bitrate(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);
    UNUSED_VAR(m);

    Call *call = &CallControl.calls[self->num];

    if (call->status != cs_Active) {
        print_err(self, "Must be in a call");
        return;
    }

    if (argc == 0) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0,
                      "Current audio encoding bitrate: %u", call->audio_bit_rate);
        return;
    }

    if (argc > 1) {
        print_err(self, "Too many arguments.");
        return;
    }

    char *end;
    const long int bit_rate = strtol(argv[1], &end, 10);

    if (*end || bit_rate < 0 || bit_rate > UINT32_MAX) {
        print_err(self, "Invalid input");
        return;
    }

    Toxav_Err_Bit_Rate_Set error;
    toxav_audio_set_bit_rate(CallControl.av, self->num, bit_rate, &error);

    if (error != TOXAV_ERR_BIT_RATE_SET_OK) {
        if (error == TOXAV_ERR_BIT_RATE_SET_SYNC) {
            print_err(self, "Synchronization error occured");
        } else if (error == TOXAV_ERR_BIT_RATE_SET_INVALID_BIT_RATE) {
            print_err(self, "Invalid audio bit rate value (valid is 6-510)");
        } else if (error == TOXAV_ERR_BIT_RATE_SET_FRIEND_NOT_FOUND) {
            print_err(self, "Friend not found");
        } else if (error == TOXAV_ERR_BIT_RATE_SET_FRIEND_NOT_IN_CALL) {
            print_err(self, "Friend is not in the call");
        } else {
            print_err(self, "Unknown error");
        }

        return;
    }

    call->audio_bit_rate = bit_rate;

    return;
}

void place_call(ToxWindow *self)
{
    Call *call = &CallControl.calls[self->num];

    if (call->status != cs_Pending) {
        return;
    }

    Toxav_Err_Call error;

    toxav_call(CallControl.av, self->num, call->audio_bit_rate, call->video_bit_rate, &error);

    if (error != TOXAV_ERR_CALL_OK) {
        if (error == TOXAV_ERR_CALL_FRIEND_ALREADY_IN_CALL) {
            print_err(self, "Already in a call!");
        } else if (error == TOXAV_ERR_CALL_MALLOC) {
            print_err(self, "Memory allocation issue");
        } else if (error == TOXAV_ERR_CALL_FRIEND_NOT_FOUND) {
            print_err(self, "Friend number invalid");
        } else if (error == TOXAV_ERR_CALL_FRIEND_NOT_CONNECTED) {
            print_err(self, "Friend is valid but not currently connected");
        } else {
            print_err(self, "Internal error!");
        }

        cancel_call(call);
        return;
    }

    callback_recv_ringing(self->num);
}

void stop_current_call(ToxWindow *self)
{
    Call *call = &CallControl.calls[self->num];

    if (call->status == cs_Pending) {
        toxav_call_control(CallControl.av, self->num, TOXAV_CALL_CONTROL_CANCEL, NULL);

        cancel_call(call);
        callback_call_canceled(self->num);
    } else {

#ifdef VIDEO
        callback_recv_video_end(self->num);
        callback_video_end(self->num);
#endif /* VIDEO */

        stop_transmission(call, self->num);
        callback_call_ended(self->num);
    }
}

/**
 * Reallocates the Calls list according to n.
 */
static void realloc_calls(uint32_t n)
{
    if (n == 0) {
        free(CallControl.calls);
        CallControl.calls = NULL;
        return;
    }

    Call *temp = realloc(CallControl.calls, n * sizeof(Call));

    if (temp == NULL) {
        exit_toxic_err("failed in realloc_calls", FATALERR_MEMORY);
    }

    CallControl.calls = temp;
}

/**
 * Inits the call structure for a given friend. Called when a friend is added to the friends list.
 * Index must be equivalent to the friend's friendlist index.
 */
void init_friend_AV(uint32_t index)
{
    if (index == CallControl.max_calls) {
        realloc_calls(CallControl.max_calls + 1);
        CallControl.calls[CallControl.max_calls] = (Call) {
            0
        };
        ++CallControl.max_calls;
    }
}

/**
 * Deletes a call structure from the Calls list. Called when a friend is deleted from the friends list.
 * Index must be equivalent to the size of the Calls list.
 */
void del_friend_AV(uint32_t index)
{
    realloc_calls(index);
    CallControl.max_calls = index;
}

#endif /* AUDIO */

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

#include "toxic.h"
#include "windows.h"
#include "audio_call.h"
#include "audio_device.h"
#include "chat_commands.h"
#include "global_commands.h"
#include "line_info.h"
#include "notify.h"
#include "friendlist.h"
#include "chat.h"
#include "misc_tools.h"

#ifdef AUDIO

#ifdef VIDEO
#include "video_call.h"
#endif /* VIDEO */

#include <stdbool.h>
#include <curses.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>

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

extern pthread_mutex_t tox_lock;

struct CallControl CallControl;

#define cbend pthread_exit(NULL)

#define frame_size (CallControl.audio_sample_rate * CallControl.audio_frame_duration / 1000)

static int set_call(Call *call, bool start)
{
    call->in_idx = -1;
    call->out_idx = -1;
#ifdef VIDEO
    call->vin_idx = -1;
    call->vout_idx = -1;
#endif /* VIDEO */

    if (start) {
        call->ttas = true;

        if (pthread_mutex_init(&call->mutex, NULL) != 0) {
            return -1;
        }
    } else {
        call->ttid = 0;

        if (pthread_mutex_destroy(&call->mutex) != 0) {
            return -1;
        }
    }

    return 0;
}

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

static void print_err(ToxWindow *self, const char *error_str)
{
    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "%s", error_str);
}

ToxAV *init_audio(ToxWindow *self, Tox *tox)
{
    Toxav_Err_New error;
    CallControl.audio_errors = ae_None;
    CallControl.prompt = self;
    CallControl.pending_call = false;

    CallControl.av = toxav_new(tox, &error);

    CallControl.audio_enabled = true;
    CallControl.audio_bit_rate = 64;
    CallControl.audio_sample_rate = 48000;
    CallControl.audio_frame_duration = 20;
    CallControl.audio_channels = 1;

#ifndef VIDEO
    CallControl.video_enabled = false;
    CallControl.video_bit_rate = 0;
    CallControl.video_frame_duration = 0;
#endif /* VIDEO */

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

    return CallControl.av;
}

void terminate_audio(void)
{
    int i;

    for (i = 0; i < CallControl.max_calls; ++i) {
        stop_transmission(&CallControl.calls[i], i);
    }

    if (CallControl.av) {
        toxav_kill(CallControl.av);
    }

    terminate_devices();
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

    pthread_mutex_lock(&tox_lock);

    toxav_audio_send_frame(CallControl.av, friend_number,
                           captured, sample_count,
                           CallControl.audio_channels,
                           CallControl.audio_sample_rate, &error);

    pthread_mutex_unlock(&tox_lock);
}

void write_device_callback(uint32_t friend_number, const int16_t *PCM, uint16_t sample_count, uint8_t channels,
                           uint32_t sample_rate)
{
    if (CallControl.calls[friend_number].ttas) {
        write_out(CallControl.calls[friend_number].out_idx, PCM, sample_count, channels, sample_rate);
    }
}

int start_transmission(ToxWindow *self, Call *call)
{
    if (!self || !CallControl.av) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to prepare transmission");
        return -1;
    }

    if (set_call(call, true) == -1) {
        return -1;
    }

    DeviceError error = open_input_device(&call->in_idx, read_device_callback, &self->num, true,
                                          CallControl.audio_sample_rate, CallControl.audio_frame_duration, CallControl.audio_channels);
    /* Set VAD as true for all; TODO: Make it more dynamic */

    if (error != de_None) {
        if (error == de_FailedStart) {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to start input device");
        }

        if (error == de_InternalError) {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Internal error with opening input device");
        }
    }

    if (open_output_device(&call->out_idx,
                           CallControl.audio_sample_rate, CallControl.audio_frame_duration, CallControl.audio_channels) != de_None) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to open output device!");
        call->has_output = 0;
    }

    return 0;
}

int stop_transmission(Call *call, uint32_t friend_number)
{
    if (call->ttas) {
        Toxav_Err_Call_Control error = TOXAV_ERR_CALL_CONTROL_OK;

        if (CallControl.call_state > TOXAV_FRIEND_CALL_STATE_FINISHED) {
            toxav_call_control(CallControl.av, friend_number, TOXAV_CALL_CONTROL_CANCEL, &error);
        }

        if (error == TOXAV_ERR_CALL_CONTROL_OK) {
            call->ttas = false;

            if (call->in_idx != -1) {
                close_device(input, call->in_idx);
            }

            if (call->out_idx != -1) {
                close_device(output, call->out_idx);
            }

            if (set_call(call, false) == -1) {
                return -1;
            }

            return 0;
        } else {

            return -1;
        }
    }

    return -1;
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
    UNUSED_VAR(audio_enabled);
    UNUSED_VAR(video_enabled);

    Tox *m = (Tox *) user_data;
    CallControl.pending_call = true;
    callback_recv_invite(m, friend_number);
}

void on_call_state(ToxAV *av, uint32_t friend_number, uint32_t state, void *user_data)
{
    UNUSED_VAR(av);
    UNUSED_VAR(user_data);

    CallControl.call_state = state;

    switch (state) {
        case (TOXAV_FRIEND_CALL_STATE_ERROR):
            line_info_add(CallControl.prompt, NULL, NULL, NULL, SYS_MSG, 0, 0, "ToxAV callstate error!");

#ifdef VIDEO
            callback_video_end(friend_number);
#endif /* VIDEO */

            stop_transmission(&CallControl.calls[friend_number], friend_number);
            callback_call_ended(friend_number);
            CallControl.pending_call = false;

            break;

        case (TOXAV_FRIEND_CALL_STATE_FINISHED):
            if (CallControl.pending_call) {
                callback_call_rejected(friend_number);
            } else {
                callback_call_ended(friend_number);
            }

#ifdef VIDEO
            callback_recv_video_end(friend_number);
            callback_video_end(friend_number);
#endif /* VIDEO */

            stop_transmission(&CallControl.calls[friend_number], friend_number);

            /* Reset stored call state after finishing */
            CallControl.call_state = 0;
            CallControl.pending_call = false;

            break;

        default:
            if (CallControl.pending_call) {
                /* Start answered call */
                callback_call_started(friend_number);
                CallControl.pending_call = false;

            } else {
#ifdef VIDEO

                /* Handle receiving client video call states */
                if (state & TOXAV_FRIEND_CALL_STATE_SENDING_V) {
                    callback_recv_video_starting(friend_number);
                } else if (state & ~TOXAV_FRIEND_CALL_STATE_SENDING_V) {
                    callback_recv_video_end(friend_number);
                }

#endif /* VIDEO */
            }

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

void audio_bit_rate_status_cb(ToxAV *av, uint32_t friend_number, uint32_t audio_bit_rate, void *user_data)
{
    CallControl.audio_bit_rate = audio_bit_rate;
    toxav_audio_set_bit_rate(av, friend_number, audio_bit_rate, user_data);
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

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onInvite != NULL && windows[i]->num == friend_number) {
            windows[i]->onInvite(windows[i], CallControl.av, friend_number, CallControl.call_state);
        }
    }
}
void callback_recv_ringing(uint32_t friend_number)
{
    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onRinging != NULL && windows[i]->num == friend_number) {
            windows[i]->onRinging(windows[i], CallControl.av, friend_number, CallControl.call_state);
        }
    }
}
void callback_recv_starting(uint32_t friend_number)
{
    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onStarting != NULL && windows[i]->num == friend_number) {
            windows[i]->onStarting(windows[i], CallControl.av, friend_number, CallControl.call_state);

            if (0 != start_transmission(windows[i], &CallControl.calls[friend_number])) { /* YEAH! */
                line_info_add(windows[i], NULL, NULL, NULL, SYS_MSG, 0, 0, "Error starting transmission!");
            }

            return;
        }
    }
}
void callback_recv_ending(uint32_t friend_number)
{
    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onEnding != NULL && windows[i]->num == friend_number) {
            windows[i]->onEnding(windows[i], CallControl.av, friend_number, CallControl.call_state);
        }
    }
}
void callback_call_started(uint32_t friend_number)
{
    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onStart != NULL && windows[i]->num == friend_number) {
            windows[i]->onStart(windows[i], CallControl.av, friend_number, CallControl.call_state);

            if (0 != start_transmission(windows[i], &CallControl.calls[friend_number])) {  /* YEAH! */
                line_info_add(windows[i], NULL, NULL, NULL, SYS_MSG, 0, 0, "Error starting transmission!");
                return;
            }
        }
    }
}
void callback_call_canceled(uint32_t friend_number)
{
    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onCancel != NULL && windows[i]->num == friend_number) {
            windows[i]->onCancel(windows[i], CallControl.av, friend_number, CallControl.call_state);
        }
    }
}
void callback_call_rejected(uint32_t friend_number)
{
    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onReject != NULL && windows[i]->num == friend_number) {
            windows[i]->onReject(windows[i], CallControl.av, friend_number, CallControl.call_state);
        }
    }
}
void callback_call_ended(uint32_t friend_number)
{
    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onEnd != NULL && windows[i]->num == friend_number) {
            windows[i]->onEnd(windows[i], CallControl.av, friend_number, CallControl.call_state);
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

    Toxav_Err_Call error;
    const char *error_str;

    if (argc != 0) {
        error_str = "Unknown arguments.";
        goto on_error;
    }

    if (!CallControl.av) {
        error_str = "ToxAV not supported!";
        goto on_error;
    }

    if (!self->stb->connection) {
        error_str = "Friend is offline.";
        goto on_error;
    }

    if (CallControl.pending_call) {
        error_str = "Already a pending call!";
        goto on_error;
    }

    toxav_call(CallControl.av, self->num, CallControl.audio_bit_rate, CallControl.video_bit_rate, &error);

    if (error != TOXAV_ERR_CALL_OK) {
        if (error == TOXAV_ERR_CALL_FRIEND_ALREADY_IN_CALL) {
            error_str = "Already in a call!";
        } else if (error == TOXAV_ERR_CALL_MALLOC) {
            error_str = "Memory allocation issue";
        } else if (error == TOXAV_ERR_CALL_FRIEND_NOT_FOUND) {
            error_str = "Friend number invalid";
        } else if (error == TOXAV_ERR_CALL_FRIEND_NOT_CONNECTED) {
            error_str = "Friend is valid but not currently connected";
        } else {
            error_str = "Internal error!";
        }

        goto on_error;
    }

    CallControl.pending_call = true;
    callback_recv_ringing(self->num);

    return;
on_error:
    print_err(self, error_str);
}

void cmd_answer(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);
    UNUSED_VAR(m);
    UNUSED_VAR(argv);

    Toxav_Err_Answer error;
    const char *error_str;

    if (argc != 0) {
        error_str = "Unknown arguments.";
        goto on_error;
    }

    if (!CallControl.av) {
        error_str = "Audio not supported!";
        goto on_error;
    }

    if (!CallControl.pending_call) {
        error_str = "No incoming call!";
        goto on_error;
    }

    toxav_answer(CallControl.av, self->num, CallControl.audio_bit_rate, CallControl.video_bit_rate, &error);

    if (error != TOXAV_ERR_ANSWER_OK) {
        if (error == TOXAV_ERR_ANSWER_FRIEND_NOT_CALLING) {
            error_str = "No incoming call!";
        } else if (error == TOXAV_ERR_ANSWER_CODEC_INITIALIZATION) {
            error_str = "Failed to initialize codecs!";
        } else if (error == TOXAV_ERR_ANSWER_FRIEND_NOT_FOUND) {
            error_str = "Friend not found!";
        } else if (error == TOXAV_ERR_ANSWER_INVALID_BIT_RATE) {
            error_str = "Invalid bit rate!";
        } else {
            error_str = "Internal error!";
        }

        goto on_error;
    }

    /* Callback will print status... */
    callback_recv_starting(self->num);
    CallControl.pending_call = false;

    return;
on_error:
    print_err(self, error_str);
}

void cmd_reject(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);
    UNUSED_VAR(m);
    UNUSED_VAR(argv);

    const char *error_str;

    if (argc != 0) {
        error_str = "Unknown arguments.";
        goto on_error;
    }

    if (!CallControl.av) {
        error_str = "Audio not supported!";
        goto on_error;
    }

    if (!CallControl.pending_call) {
        error_str = "No incoming call!";
        goto on_error;
    }

    /* Manually send a cancel call control because call hasn't started */
    toxav_call_control(CallControl.av, self->num, TOXAV_CALL_CONTROL_CANCEL, NULL);
    CallControl.pending_call = false;

    /* Callback will print status... */
    callback_call_rejected(self->num);

    return;
on_error:
    print_err(self, error_str);
}

void cmd_hangup(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);
    UNUSED_VAR(m);
    UNUSED_VAR(argv);

    const char *error_str = NULL;

    if (!CallControl.av) {
        error_str = "Audio not supported!";
        goto on_error;
    }

    if (argc != 0) {
        error_str = "Unknown arguments.";
        goto on_error;
    }

    if (!self->is_call && !CallControl.pending_call) {
        error_str = "Not in a call.";
        goto on_error;
    }

#ifdef VIDEO
    callback_video_end(self->num);
#endif /* VIDEO */

    stop_current_call(self);
    return;
on_error:
    print_err(self, error_str);
}

void cmd_list_devices(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);
    UNUSED_VAR(m);

    const char *error_str;

    if (argc != 1) {
        if (argc < 1) {
            error_str = "Type must be specified!";
        } else {
            error_str = "Only one argument allowed!";
        }

        goto on_error;
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

    return;
on_error:
    print_err(self, error_str);
}

/* This changes primary device only */
void cmd_change_device(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);
    UNUSED_VAR(m);

    const char *error_str;

    if (argc != 2) {
        if (argc < 1) {
            error_str = "Type must be specified!";
        } else if (argc < 2) {
            error_str = "Must have id!";
        } else {
            error_str = "Only two arguments allowed!";
        }

        goto on_error;
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
        error_str = "Invalid input";
        goto on_error;
    }

    if (set_al_device(type, selection) == de_InvalidSelection) {
        error_str = "Invalid selection!";
        goto on_error;
    }

    return;
on_error:
    print_err(self, error_str);
}

void cmd_mute(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);
    UNUSED_VAR(m);

    const char *error_str;

    if (argc != 1) {
        if (argc < 1) {
            error_str = "Type must be specified!";
        } else {
            error_str = "Only two arguments allowed!";
        }

        goto on_error;
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
    if (self->is_call) {
        Call *this_call = &CallControl.calls[self->num];

        pthread_mutex_lock(&this_call->mutex);

        if (type == input) {
            device_mute(type, this_call->in_idx);
            self->chatwin->infobox.in_is_muted ^= 1;
        } else {
            device_mute(type, this_call->out_idx);
            self->chatwin->infobox.out_is_muted ^= 1;
        }

        pthread_mutex_unlock(&this_call->mutex);
    }

    return;

on_error:
    print_err(self, error_str);
}

void cmd_sense(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);
    UNUSED_VAR(m);

    const char *error_str;

    if (argc != 1) {
        if (argc < 1) {
            error_str = "Must have value!";
        } else {
            error_str = "Only two arguments allowed!";
        }

        goto on_error;
    }

    char *end;
    float value = strtof(argv[1], &end);

    if (*end) {
        error_str = "Invalid input";
        goto on_error;
    }

    /* Call must be active */
    if (self->is_call) {
        device_set_VAD_treshold(CallControl.calls[self->num].in_idx, value);
        self->chatwin->infobox.vad_lvl = value;
    }

    return;

on_error:
    print_err(self, error_str);
}

void cmd_bitrate(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);
    UNUSED_VAR(m);

    char *error_str;

    if (argc != 1) {
        error_str = "Must have value!";
        goto on_error;
    }

    if (self->is_call == false) {
        error_str = "Must be in a call";
        goto on_error;
    }

    const uint32_t bitrate = strtol(argv[1], NULL, 10);

    Toxav_Err_Bit_Rate_Set error;
    audio_bit_rate_status_cb(CallControl.av, self->num, bitrate, &error);

    if (error != TOXAV_ERR_BIT_RATE_SET_OK) {
        switch (error) {
            case TOXAV_ERR_BIT_RATE_SET_SYNC:
                error_str = "Syncronization error occured";
                break;

            case TOXAV_ERR_BIT_RATE_SET_INVALID_BIT_RATE:
                error_str = "Invalid audio bit rate value (valid is 6-510)";
                break;

            case TOXAV_ERR_BIT_RATE_SET_FRIEND_NOT_FOUND:
                error_str = "Friend not found";
                break;

            case TOXAV_ERR_BIT_RATE_SET_FRIEND_NOT_IN_CALL:
                error_str = "Friend is not in the call";
                break;

            default:
                error_str = "Unknown error";
        }

        goto on_error;
    }

    return;

on_error:
    print_err(self, error_str);
}

void stop_current_call(ToxWindow *self)
{
    toxav_call_control(CallControl.av, self->num, TOXAV_CALL_CONTROL_CANCEL, NULL);

    if (CallControl.pending_call) {
        callback_call_canceled(self->num);
    } else {
        stop_transmission(&CallControl.calls[self->num], self->num);
        callback_call_ended(self->num);
    }

    CallControl.pending_call = false;
}

/**
 * Reallocates the Calls list according to n.
 */
static void realloc_calls(uint32_t n)
{
    if (n <= 0) {
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
    realloc_calls(CallControl.max_calls + 1);
    memset(&CallControl.calls[CallControl.max_calls], 0, sizeof(Call));

    if (index == CallControl.max_calls) {
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

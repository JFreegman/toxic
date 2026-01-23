/*  audio_call.c
 *
 *  Copyright (C) 2014-2026 Toxic All Rights Reserved.
 *
 *  This file is part of Toxic. Toxic is free software licensed
 *  under the GNU General Public License 3.0.
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

void on_call(ToxAV *av, uint32_t friend_number, bool audio_enabled, bool video_enabled,
             void *user_data);
void on_call_state(ToxAV *av, uint32_t friend_number, uint32_t state, void *user_data);
void on_audio_receive_frame(ToxAV *av, uint32_t friend_number, int16_t const *pcm, size_t sample_count,
                            uint8_t channels, uint32_t sampling_rate, void *user_data);

void callback_recv_invite(Toxic *toxic, uint32_t friend_number);
void callback_recv_ringing(Toxic *toxic, uint32_t friend_number);
void callback_recv_starting(Toxic *toxic, uint32_t friend_number);
void callback_call_started(Toxic *toxic, uint32_t friend_number);
void callback_call_canceled(Toxic *toxic, uint32_t friend_number);
void callback_call_rejected(Toxic *toxic, uint32_t friend_number);
void callback_call_ended(Toxic *toxic, uint32_t friend_number);

void write_device_callback(struct CallControl *cc, uint32_t friend_number, const int16_t *PCM, uint16_t sample_count,
                           uint8_t channels,
                           uint32_t sample_rate);

void audio_bit_rate_callback(ToxAV *av, uint32_t friend_number, uint32_t audio_bit_rate, void *user_data);

static void print_err(ToxWindow *self, const Client_Config *c_config, const char *error_str)
{
    line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "%s", error_str);
}

ToxAV *init_audio(Toxic *toxic)
{
    if (toxic == NULL) {
        return NULL;
    }

    ToxWindow *self = toxic->home_window;
    const Client_Config *c_config = toxic->c_config;
    struct CallControl *cc = toxic->call_control;

    Toxav_Err_New error;
    cc->audio_errors = ae_None;

    toxic->av = toxav_new(toxic->tox, &error);

    cc->audio_enabled = true;
    cc->default_audio_bit_rate = 64;
    cc->audio_sample_rate = 48000;
    cc->audio_frame_duration = 20;
    cc->audio_channels = c_config->chat_audio_channels;

    cc->video_enabled = false;
    cc->default_video_bit_rate = 0;
    cc->video_frame_duration = 0;

    if (toxic->av == NULL) {
        cc->audio_errors |= ae_StartingCoreAudio;
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to init ToxAV");
        return NULL;
    }

    if (init_devices() == de_InternalError) {
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to init devices");
        toxav_kill(toxic->av);
        toxic->av = NULL;
        return NULL;
    }

    toxav_callback_call(toxic->av, on_call, (void *) toxic);
    toxav_callback_call_state(toxic->av, on_call_state, (void *) toxic);
    toxav_callback_audio_receive_frame(toxic->av, on_audio_receive_frame, (void *) toxic);
    toxav_callback_audio_bit_rate(toxic->av, audio_bit_rate_callback, (void *) toxic);

    return toxic->av;
}

static void read_device_callback(const int16_t *captured, uint32_t size, void *data)
{
    UNUSED_VAR(size);

    Toxav_Err_Send_Frame error;
    AudioTransmissionContext *ctx = (AudioTransmissionContext *)data;
    int64_t sample_count = ((int64_t) ctx->audio_sample_rate) * \
                           ((int64_t) ctx->audio_frame_duration) / 1000;

    if (sample_count <= 0) {
        return;
    }

    toxav_audio_send_frame(ctx->av, ctx->friend_number,
                           captured, sample_count,
                           ctx->audio_channels,
                           ctx->audio_sample_rate, &error);
}

void write_device_callback(struct CallControl *cc, uint32_t friend_number, const int16_t *PCM, uint16_t sample_count,
                           uint8_t channels, uint32_t sample_rate)
{
    if (friend_number >= cc->max_calls || !cc->calls[friend_number]) {
        return;
    }

    if (cc->calls[friend_number]->status == cs_Active) {
        write_out(cc->calls[friend_number]->out_idx, PCM, sample_count, channels, sample_rate);
    }
}

bool init_call(struct CallControl *cc, Call *call, uint32_t friend_number)
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
    call->audio_bit_rate = cc->default_audio_bit_rate;

    call->audio_tx_ctx.friend_number = friend_number;

#ifdef VIDEO
    call->vin_idx = -1;
    call->vout_idx = -1;
    call->video_width = cc->default_video_width;
    call->video_height = cc->default_video_height;
    call->video_bit_rate = cc->default_video_bit_rate;
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

static int start_transmission(ToxWindow *self, Toxic *toxic, Call *call)
{
    if (self == NULL || toxic == NULL) {
        return -1;
    }

    const Client_Config *c_config = toxic->c_config;
    struct CallControl *cc = toxic->call_control;

    if (toxic->av == NULL) {
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to prepare audio transmission");
        return -1;
    }

    call->audio_tx_ctx.av = toxic->av;
    call->audio_tx_ctx.audio_frame_duration = cc->audio_frame_duration;
    call->audio_tx_ctx.audio_sample_rate = cc->audio_sample_rate;
    call->audio_tx_ctx.audio_channels = cc->audio_channels;

    DeviceError error = open_input_device(&call->in_idx, read_device_callback, &call->audio_tx_ctx,
                                          cc->audio_sample_rate, cc->audio_frame_duration,
                                          cc->audio_channels, c_config->VAD_threshold);

    if (error != de_None) {
        if (error == de_FailedStart) {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to start audio input device");
        }

        if (error == de_InternalError) {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0,
                          "Internal error with opening audio input device");
        }
    }

    if (open_output_device(&call->out_idx,
                           cc->audio_sample_rate,
                           cc->audio_frame_duration,
                           cc->audio_channels,
                           c_config->VAD_threshold) != de_None) {
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to open audio output device!");
    }

    return 0;
}

static void start_call(ToxWindow *self, Toxic *toxic, Call *call)
{
    if (call->status != cs_Pending) {
        return;
    }

    if (start_transmission(self, toxic, call) != 0) {
        return;
    }

    call->status = cs_Active;

#ifdef VIDEO

    if (call->state & TOXAV_FRIEND_CALL_STATE_SENDING_V) {
        callback_recv_video_starting(toxic, self->num);
    }

    if (call->video_bit_rate) {
        start_video_transmission(self, toxic, call);
    }

#endif
}

static int stop_transmission(ToxAV *av, Call *call, uint32_t friend_number)
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

    if (av && call->state > TOXAV_FRIEND_CALL_STATE_FINISHED) {
        toxav_call_control(av, friend_number, TOXAV_CALL_CONTROL_CANCEL, &error);
    }

    if (error != TOXAV_ERR_CALL_CONTROL_OK) {
        return -1;
    }

    return 0;
}

void terminate_audio(ToxAV *av, struct CallControl *cc)
{
    if (!cc) {
        return;
    }

    for (int i = 0; i < cc->max_calls; ++i) {
        if (cc->calls[i]) {
            stop_transmission(av, cc->calls[i], i);
        }
    }

    if (av) {
        toxav_kill(av);
    }

    terminate_devices();
}

/*
 * End of transmission
 */


/*
 * Callbacks
 */
void on_call(ToxAV *av, uint32_t friend_number, bool audio_enabled, bool video_enabled,
             void *user_data)
{
    UNUSED_VAR(av);

    Toxic *toxic = (Toxic *) user_data;
    struct CallControl *cc = toxic->call_control;

    if (!init_friend_AV(cc, friend_number)) {
        fprintf(stderr, "Failed to receive call: Insufficient memory\n");
        return;
    }

    Call *call = cc->calls[friend_number];
    init_call(cc, call, friend_number);

    call->state = TOXAV_FRIEND_CALL_STATE_ACCEPTING_A | TOXAV_FRIEND_CALL_STATE_ACCEPTING_V;

    if (audio_enabled) {
        call->state |= TOXAV_FRIEND_CALL_STATE_SENDING_A;
    }

    if (video_enabled) {
        call->state |= TOXAV_FRIEND_CALL_STATE_SENDING_V;
    }

    callback_recv_invite(toxic, friend_number);
}

void on_call_state(ToxAV *av, uint32_t friend_number, uint32_t state, void *user_data)
{
    Toxic *toxic = (Toxic *) user_data;

    if (toxic == NULL) {
        return;
    }

    struct CallControl *cc = toxic->call_control;

    UNUSED_VAR(av);

    if (friend_number >= cc->max_calls || !cc->calls[friend_number]) {
        fprintf(stderr, "Failed to handle call state: Invalid friend number\n");
        return;
    }

    Call *call = cc->calls[friend_number];

    if (call->status == cs_None) {
        return;
    }

    call->state = state;

    switch (state) {
        case TOXAV_FRIEND_CALL_STATE_ERROR:
        case TOXAV_FRIEND_CALL_STATE_FINISHED:
            if (state == TOXAV_FRIEND_CALL_STATE_ERROR) {
                line_info_add(toxic->home_window, toxic->c_config, false, NULL, NULL, SYS_MSG, 0, 0,
                              "ToxAV callstate error!");
            }

            if (call->status == cs_Pending) {
                cancel_call(call);
                callback_call_rejected(toxic, friend_number);
            } else {

#ifdef VIDEO
                callback_recv_video_end(toxic, friend_number);
                callback_video_end(av, toxic->call_control, friend_number);
#endif /* VIDEO */

                stop_transmission(av, call, friend_number);
                callback_call_ended(toxic, friend_number);
            }

            break;

        default:
            if (call->status == cs_Pending) {
                /* Start answered call */
                callback_call_started(toxic, friend_number);
            }

#ifdef VIDEO

            /* Handle receiving client video call states */
            if (state & TOXAV_FRIEND_CALL_STATE_SENDING_V) {
                callback_recv_video_starting(toxic, friend_number);
            } else {
                callback_recv_video_end(toxic, friend_number);
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
    Toxic *toxic = (Toxic *) user_data;

    write_device_callback(toxic->call_control, friend_number, pcm, sample_count, channels, sampling_rate);
}

void audio_bit_rate_callback(ToxAV *av, uint32_t friend_number, uint32_t audio_bit_rate, void *user_data)
{
    Toxic *toxic = (Toxic *) user_data;
    struct CallControl *cc = toxic->call_control;

    if (friend_number >= cc->max_calls) {
        return;
    }

    Call *call = cc->calls[friend_number];
    call->audio_bit_rate = audio_bit_rate;
    toxav_audio_set_bit_rate(av, friend_number, audio_bit_rate, NULL);
}

void callback_recv_invite(Toxic *toxic, uint32_t friend_number)
{
    if (toxic == NULL || friend_number >= toxic->friends->max_idx) {
        return;
    }

    struct CallControl *cc = toxic->call_control;

    if (friend_number >= cc->max_calls) {
        return;
    }

    if (toxic->friends->list[friend_number].window_id == -1) {
        const int window_id = add_window(toxic, new_chat(toxic->friends, toxic->friends->list[friend_number].num));

        if (window_id < 0) {
            fprintf(stderr, "Failed to create new chat window in callback_recv_invite()\n");
            return;
        }

        toxic->friends->list[friend_number].window_id = window_id;
    }

    const Call *call = cc->calls[friend_number];
    Windows *windows = toxic->windows;

    for (uint16_t i = 0; i < windows->count; ++i) {
        ToxWindow *w = windows->list[i];

        if (w->onInvite != NULL && w->num == friend_number) {
            w->onInvite(w, toxic, friend_number, call->state);
        }
    }
}

void callback_recv_ringing(Toxic *toxic, uint32_t friend_number)
{
    struct CallControl *cc = toxic->call_control;

    if (friend_number >= cc->max_calls) {
        return;
    }

    const Call *call = cc->calls[friend_number];
    Windows *windows = toxic->windows;

    for (uint16_t i = 0; i < windows->count; ++i) {
        ToxWindow *w = windows->list[i];

        if (w->onRinging != NULL && w->num == friend_number) {
            w->onRinging(w, toxic, friend_number, call->state);
        }
    }
}

void callback_recv_starting(Toxic *toxic, uint32_t friend_number)
{
    struct CallControl *cc = toxic->call_control;

    if (friend_number >= cc->max_calls) {
        return;
    }

    Call *call = cc->calls[friend_number];
    Windows *windows = toxic->windows;

    for (uint16_t i = 0; i < windows->count; ++i) {
        ToxWindow *w = windows->list[i];

        if (w->onStarting != NULL && w->num == friend_number) {
            w->onStarting(w, toxic, friend_number, call->state);
            start_call(w, toxic, call);
        }
    }
}

void callback_call_started(Toxic *toxic, uint32_t friend_number)
{
    struct CallControl *cc = toxic->call_control;

    if (friend_number >= cc->max_calls) {
        return;
    }

    Call *call = cc->calls[friend_number];
    Windows *windows = toxic->windows;

    for (uint16_t i = 0; i < windows->count; ++i) {
        ToxWindow *w = windows->list[i];

        if (w->onStart != NULL && w->num == friend_number) {
            w->onStart(w, toxic, friend_number, call->state);
            start_call(w, toxic, call);
        }
    }
}

void callback_call_canceled(Toxic *toxic, uint32_t friend_number)
{
    struct CallControl *cc = toxic->call_control;

    if (friend_number >= cc->max_calls) {
        return;
    }

    const Call *call = cc->calls[friend_number];
    Windows *windows = toxic->windows;

    for (uint16_t i = 0; i < windows->count; ++i) {
        ToxWindow *w = windows->list[i];

        if (w->onCancel != NULL && w->num == friend_number) {
            w->onCancel(w, toxic, friend_number, call->state);
        }
    }
}

void callback_call_rejected(Toxic *toxic, uint32_t friend_number)
{
    struct CallControl *cc = toxic->call_control;

    if (friend_number >= cc->max_calls) {
        return;
    }

    const Call *call = cc->calls[friend_number];
    Windows *windows = toxic->windows;

    for (uint16_t i = 0; i < windows->count; ++i) {
        ToxWindow *w = windows->list[i];

        if (w->onReject != NULL && w->num == friend_number) {
            w->onReject(w, toxic, friend_number, call->state);
        }
    }
}

void callback_call_ended(Toxic *toxic, uint32_t friend_number)
{
    struct CallControl *cc = toxic->call_control;

    if (friend_number >= cc->max_calls) {
        return;
    }

    const Call *call = cc->calls[friend_number];
    Windows *windows = toxic->windows;

    for (uint16_t i = 0; i < windows->count; ++i) {
        ToxWindow *w = windows->list[i];

        if (w->onEnd != NULL && w->num == friend_number) {
            w->onEnd(w, toxic, friend_number, call->state);
        }
    }
}

/*
 * End of Callbacks
 */


/*
 * Commands from chat_commands.h
 */
void cmd_call(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);
    UNUSED_VAR(argv);

    if (toxic == NULL || self == NULL) {
        return;
    }

    const Client_Config *c_config = toxic->c_config;

    if (argc != 0) {
        print_err(self, c_config, "Unknown arguments.");
        return;
    }

    if (toxic->av == NULL) {
        print_err(self, c_config, "ToxAV not supported!");
        return;
    }

    if (!self->stb->connection) {
        print_err(self, c_config, "Friend is offline.");
        return;
    }

    if (self->num >= toxic->call_control->max_calls) {
        print_err(self, c_config, "Invalid call index");
        return;
    }

    Call *call = toxic->call_control->calls[self->num];

    if (call->status != cs_None) {
        print_err(self, c_config, "Already calling.");
        return;
    }

    init_call(toxic->call_control, call, self->num);

    place_call(self, toxic);
}

void cmd_answer(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);
    UNUSED_VAR(argv);

    if (toxic == NULL || self == NULL) {
        return;
    }

    const Client_Config *c_config = toxic->c_config;

    Toxav_Err_Answer error;

    if (argc != 0) {
        print_err(self, c_config, "Unknown arguments.");
        return;
    }

    if (toxic->av == NULL) {
        print_err(self, c_config, "Audio not supported!");
        return;
    }

    if (self->num >= toxic->call_control->max_calls) {
        print_err(self, c_config, "Invalid call index");
        return;
    }

    Call *call = toxic->call_control->calls[self->num];

    if (call->status != cs_Pending) {
        print_err(self, c_config, "No incoming call!");
        return;
    }

    toxav_answer(toxic->av, self->num, call->audio_bit_rate, call->video_bit_rate, &error);

    if (error != TOXAV_ERR_ANSWER_OK) {
        if (error == TOXAV_ERR_ANSWER_FRIEND_NOT_CALLING) {
            print_err(self, c_config, "No incoming call!");
        } else if (error == TOXAV_ERR_ANSWER_CODEC_INITIALIZATION) {
            print_err(self, c_config, "Failed to initialize codecs!");
        } else if (error == TOXAV_ERR_ANSWER_FRIEND_NOT_FOUND) {
            print_err(self, c_config, "Friend not found!");
        } else if (error == TOXAV_ERR_ANSWER_INVALID_BIT_RATE) {
            print_err(self, c_config, "Invalid bit rate!");
        } else {
            print_err(self, c_config, "Internal error!");
        }

        return;
    }

    /* Callback will print status... */
    callback_recv_starting(toxic, self->num);
}

void cmd_reject(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);
    UNUSED_VAR(argv);

    if (toxic == NULL || self == NULL) {
        return;
    }

    const Client_Config *c_config = toxic->c_config;

    if (argc != 0) {
        print_err(self, c_config, "Unknown arguments.");
        return;
    }

    if (toxic->av == NULL) {
        print_err(self, c_config, "Audio not supported!");
        return;
    }

    if (self->num >= toxic->call_control->max_calls) {
        print_err(self, c_config, "Invalid call index.");
        return;
    }

    Call *call = toxic->call_control->calls[self->num];

    if (call->status != cs_Pending) {
        print_err(self, c_config, "No incoming call!");
        return;
    }

    /* Manually send a cancel call control because call hasn't started */
    toxav_call_control(toxic->av, self->num, TOXAV_CALL_CONTROL_CANCEL, NULL);
    cancel_call(call);

    /* Callback will print status... */
    callback_call_rejected(toxic, self->num);
}

void cmd_hangup(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);
    UNUSED_VAR(argv);

    if (toxic == NULL || self == NULL) {
        return;
    }

    const Client_Config *c_config = toxic->c_config;

    if (toxic->av == NULL) {
        print_err(self, c_config, "Audio not supported!");
        return;
    }

    if (argc != 0) {
        print_err(self, c_config, "Unknown arguments.");
        return;
    }

    if (self->num >= toxic->call_control->max_calls) {
        print_err(self, c_config, "Invalid call index.");
        return;
    }

    Call *call = toxic->call_control->calls[self->num];

    if (call->status == cs_None) {
        print_err(self, c_config, "Not in a call.");
        return;
    }

    stop_current_call(self, toxic);
}

void cmd_list_devices(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

    if (toxic == NULL || self == NULL) {
        return;
    }

    const Client_Config *c_config = toxic->c_config;

    if (argc != 1) {
        if (argc < 1) {
            print_err(self, c_config, "Type must be specified!");
        } else {
            print_err(self, c_config, "Only one argument allowed!");
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
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Invalid type: %s", argv[1]);
        return;
    }

    // Refresh device list.
    get_al_device_names();

    print_al_devices(self, c_config, type);
}

/* This changes primary device only */
void cmd_change_device(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

    if (toxic == NULL || self == NULL) {
        return;
    }

    const Client_Config *c_config = toxic->c_config;

    if (argc != 2) {
        if (argc < 1) {
            print_err(self, c_config, "Type must be specified!");
        } else if (argc < 2) {
            print_err(self, c_config, "Must have id!");
        } else {
            print_err(self, c_config, "Only two arguments allowed!");
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
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Invalid type: %s", argv[1]);
        return;
    }


    char *end;
    long int selection = strtol(argv[2], &end, 10);

    if (*end) {
        print_err(self, c_config, "Invalid input");
        return;
    }

    if (set_al_device(type, selection) == de_InvalidSelection) {
        print_err(self, c_config, "Invalid selection!");
        return;
    }
}

void cmd_mute(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

    if (toxic == NULL || self == NULL) {
        return;
    }

    const Client_Config *c_config = toxic->c_config;

    if (argc != 1) {
        print_err(self, c_config, "Specify type: \"/mute in\" or \"/mute out\".");
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
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Invalid type: %s", argv[1]);
        return;
    }

    if (self->num >= toxic->call_control->max_calls) {
        print_err(self, c_config, "Invalid call index.");
        return;
    }

    /* If call is active, use this_call values */
    Call *this_call = toxic->call_control->calls[self->num];

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

void cmd_sense(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

    if (toxic == NULL || self == NULL) {
        return;
    }

    const Client_Config *c_config = toxic->c_config;

    if (argc != 1) {
        if (argc < 1) {
            print_err(self, c_config, "Must have value!");
        } else {
            print_err(self, c_config, "Only two arguments allowed!");
        }

        return;
    }

    char *end;
    float value = strtof(argv[1], &end);

    if (*end) {
        print_err(self, c_config, "Invalid input");
        return;
    }

    if (self->num >= toxic->call_control->max_calls) {
        print_err(self, c_config, "Invalid call index.");
        return;
    }

    const Call *call = toxic->call_control->calls[self->num];

    /* Call must be active */
    if (call->status == cs_Active) {
        device_set_VAD_threshold(call->in_idx, value);
        self->chatwin->infobox.vad_lvl = value;
    }

    return;
}

void cmd_bitrate(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

    if (toxic == NULL || self == NULL) {
        return;
    }

    const Client_Config *c_config = toxic->c_config;

    if (self->num >= toxic->call_control->max_calls) {
        print_err(self, c_config, "Invalid call index.");
        return;
    }

    Call *call = toxic->call_control->calls[self->num];

    if (call->status != cs_Active) {
        print_err(self, c_config, "Must be in a call");
        return;
    }

    if (argc == 0) {
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0,
                      "Current audio encoding bitrate: %u", call->audio_bit_rate);
        return;
    }

    if (argc > 1) {
        print_err(self, c_config, "Too many arguments.");
        return;
    }

    char *end;
    const long int bit_rate = strtol(argv[1], &end, 10);

    if (*end || bit_rate < 0 || bit_rate > UINT32_MAX) {
        print_err(self, c_config, "Invalid input");
        return;
    }

    Toxav_Err_Bit_Rate_Set error;
    toxav_audio_set_bit_rate(toxic->av, self->num, bit_rate, &error);

    if (error != TOXAV_ERR_BIT_RATE_SET_OK) {
        if (error == TOXAV_ERR_BIT_RATE_SET_SYNC) {
            print_err(self, c_config, "Synchronization error occured");
        } else if (error == TOXAV_ERR_BIT_RATE_SET_INVALID_BIT_RATE) {
            print_err(self, c_config, "Invalid audio bit rate value (valid is 6-510)");
        } else if (error == TOXAV_ERR_BIT_RATE_SET_FRIEND_NOT_FOUND) {
            print_err(self, c_config, "Friend not found");
        } else if (error == TOXAV_ERR_BIT_RATE_SET_FRIEND_NOT_IN_CALL) {
            print_err(self, c_config, "Friend is not in the call");
        } else {
            print_err(self, c_config, "Unknown error");
        }

        return;
    }

    call->audio_bit_rate = bit_rate;

    return;
}

void place_call(ToxWindow *self, Toxic *toxic)
{
    const Client_Config *c_config = toxic->c_config;

    if (self->num >= toxic->call_control->max_calls) {
        print_err(self, c_config, "Invalid call index.");
        return;
    }

    Call *call = toxic->call_control->calls[self->num];

    if (call->status != cs_Pending) {
        print_err(self, toxic->c_config, "No pending call.");
        return;
    }

    Toxav_Err_Call error;

    toxav_call(toxic->av, self->num, call->audio_bit_rate, call->video_bit_rate, &error);

    if (error != TOXAV_ERR_CALL_OK) {
        if (error == TOXAV_ERR_CALL_FRIEND_ALREADY_IN_CALL) {
            print_err(self, c_config, "Already in a call!");
        } else if (error == TOXAV_ERR_CALL_MALLOC) {
            print_err(self, c_config, "Memory allocation issue");
        } else if (error == TOXAV_ERR_CALL_FRIEND_NOT_FOUND) {
            print_err(self, c_config, "Friend number invalid");
        } else if (error == TOXAV_ERR_CALL_FRIEND_NOT_CONNECTED) {
            print_err(self, c_config, "Friend is valid but not currently connected");
        } else {
            print_err(self, c_config, "Internal error!");
        }

        cancel_call(call);
        return;
    }

    callback_recv_ringing(toxic, self->num);
}

void stop_current_call(ToxWindow *self, Toxic *toxic)
{
    if (self->num >= toxic->call_control->max_calls) {
        print_err(self, toxic->c_config, "Invalid call index.");
        return;
    }

    Call *call = toxic->call_control->calls[self->num];

    if (call->status == cs_Pending) {
        toxav_call_control(toxic->av, self->num, TOXAV_CALL_CONTROL_CANCEL, NULL);
        cancel_call(call);
        callback_call_canceled(toxic, self->num);
    } else {

#ifdef VIDEO
        callback_recv_video_end(toxic, self->num);
        callback_video_end(toxic->av, toxic->call_control, self->num);
#endif /* VIDEO */

        stop_transmission(toxic->av, call, self->num);
        callback_call_ended(toxic, self->num);
    }
}

/**
 * Reallocates the Calls list according to n.
 */
static bool realloc_calls(struct CallControl *cc, uint32_t n)
{
    if (n == 0) {
        free(cc->calls);
        cc->calls = NULL;
        return true;
    }

    uint32_t old_n = cc->max_calls;
    Call **temp = realloc(cc->calls, n * sizeof(Call *));

    if (temp == NULL) {
        return false;
    }

    cc->calls = temp;

    if (n > old_n) {
        memset(cc->calls + old_n, 0, (n - old_n) * sizeof(Call *));
    }

    return true;
}

bool init_friend_AV(struct CallControl *cc, uint32_t index)
{
    if (index >= cc->max_calls) {
        if (!realloc_calls(cc, index + 1)) {
            fprintf(stderr, "Warning: realloc_calls(%u) failed\n", index + 1);
            return false;
        }

        cc->max_calls = index + 1;
    }

    if (cc->calls[index] == NULL) {
        cc->calls[index] = calloc(1, sizeof(Call));

        if (cc->calls[index] == NULL) {
            fprintf(stderr, "Warning: calloc failed for friend %u\n", index);
            return false;
        }
    }

    return true;
}

/**
 * Deletes a call structure from the Calls list. Called when a friend is deleted from the friends list.
 * Index must be equivalent to the size of the Calls list.
 */
void del_friend_AV(struct CallControl *cc, uint32_t index)
{
    if (index >= cc->max_calls) {
        return;
    }

    for (uint32_t i = index; i < cc->max_calls; ++i) {
        free(cc->calls[i]);
    }

    if (!realloc_calls(cc, index)) {
        fprintf(stderr, "Warning: realloc_calls(%u) failed\n", index);
        return;
    }

    cc->max_calls = index;
}

#endif /* AUDIO */

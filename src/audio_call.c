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
#endif
#endif

#define cbend pthread_exit(NULL)

#define MAX_CALLS 10

#define frame_size (CallContrl.audio_bit_rate * CallContrl.audio_frame_duration / 1000)

static int set_call(Call* call, bool start)
{
    call->in_idx = -1;
    call->out_idx = -1;

    if ( start ) {
        call->ttas = true;

        if (pthread_mutex_init(&call->mutex, NULL) != 0)
            return -1;
    }
    else {
        call->ttid = 0;
        if (pthread_mutex_destroy(&call->mutex) != 0)
               return -1;
    }

    return 0;
}

typedef struct CallControl {
    AudioError errors;

    ToxAV *av;
    ToxWindow *window;

    Call calls[MAX_CALLS];
    bool pending_call;
    uint32_t call_state;

    bool audio_enabled;
    bool video_enabled;

    uint32_t audio_bit_rate;
    uint32_t video_bit_rate;

    uint32_t audio_sample_rate;

    int32_t audio_frame_duration;
    int32_t video_frame_duration;

    uint8_t audio_channels;

} CallControl;

CallControl CallContrl;

void call_cb(ToxAV *av, uint32_t friend_number, bool audio_enabled, bool video_enabled, void *user_data);
void callstate_cb(ToxAV *av, uint32_t friend_number, uint32_t state, void *user_data);
void receive_audio_frame_cb(ToxAV *av, uint32_t friend_number, int16_t const *pcm, size_t sample_count, uint8_t channels, uint32_t sampling_rate, void *user_data);
void audio_bit_rate_status_cb(ToxAV *av, uint32_t friend_number, bool stable, uint32_t bit_rate, void *user_data);
void receive_video_frame_cb(ToxAV *av, uint32_t friend_number,
                                    uint16_t width, uint16_t height,
                                    uint8_t const *y, uint8_t const *u, uint8_t const *v, uint8_t const *a,
                                    int32_t ystride, int32_t ustride, int32_t vstride, int32_t astride,
                                    void *user_data);
void video_bit_rate_status_cb(ToxAV *av, uint32_t friend_number, 
                                      bool stable, uint32_t bit_rate, void *user_data);

void callback_recv_invite   ( void* av, uint32_t friend_number, void *arg );
void callback_recv_ringing  ( void* av, uint32_t friend_number, void *arg );
void callback_recv_starting ( void* av, uint32_t friend_number, void *arg );
void callback_recv_ending   ( void* av, uint32_t friend_number, void *arg );
void callback_call_started  ( void* av, uint32_t friend_number, void *arg );
void callback_call_canceled ( void* av, uint32_t friend_number, void *arg );
void callback_call_rejected ( void* av, uint32_t friend_number, void *arg );
void callback_call_ended    ( void* av, uint32_t friend_number, void *arg );
void callback_requ_timeout  ( void* av, uint32_t friend_number, void *arg );
void callback_peer_timeout  ( void* av, uint32_t friend_number, void *arg );
void callback_media_change  ( void* av, uint32_t friend_number, void *arg );

void write_device_callback( void* agent, int32_t friend_number, const int16_t* PCM, uint16_t size, void* arg );

static void print_err (ToxWindow *self, const char *error_str)
{
    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "%s", error_str);
}

ToxAV *init_audio(ToxWindow *self, Tox *tox)
{
    TOXAV_ERR_NEW error;

    CallContrl.window = self;

    CallContrl.audio_enabled = true;
    CallContrl.audio_bit_rate = 48;
    CallContrl.audio_sample_rate = 48000;
    CallContrl.audio_frame_duration = 10;
    CallContrl.audio_channels = 1;

    CallContrl.errors = ae_None;

    memset(CallContrl.calls, 0, sizeof(CallContrl.calls));

    /* Streaming stuff from core */

    CallContrl.av = toxav_new(tox, &error);

    if( error == TOXAV_ERR_NEW_OK) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "TOXAV_ERR_NEW_OK");
    }

    if(tox == toxav_get_tox(CallContrl.av)) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Tox instance matches");
    }

    if ( !CallContrl.av ) {
        CallContrl.errors |= ae_StartingCoreAudio;
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to init ToxAV");
        return NULL;
    }

    if ( init_devices(CallContrl.av) == de_InternalError ) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to init devices");
        toxav_kill(CallContrl.av);
        return CallContrl.av = NULL;
    }

    
    /*toxav_register_callstate_callback(CallContrl.av, callback_call_started, av_OnStart, self);
    toxav_register_callstate_callback(CallContrl.av, callback_call_canceled, av_OnCancel, self);
    toxav_register_callstate_callback(CallContrl.av, callback_call_rejected, av_OnReject, self);
    toxav_register_callstate_callback(CallContrl.av, callback_call_ended, av_OnEnd, self);
    toxav_register_callstate_callback(CallContrl.av, callback_recv_invite, av_OnInvite, self);

    toxav_register_callstate_callback(CallContrl.av, callback_recv_ringing, av_OnRinging, self);
    toxav_register_callstate_callback(CallContrl.av, callback_recv_starting, av_OnStart, self);
    toxav_register_callstate_callback(CallContrl.av, callback_recv_ending, av_OnEnd, self);

    toxav_register_callstate_callback(CallContrl.av, callback_requ_timeout, av_OnRequestTimeout, self);
    toxav_register_callstate_callback(CallContrl.av, callback_peer_timeout, av_OnPeerTimeout, self);
    //toxav_register_callstate_callback(CallContrl.av, callback_media_change, av_OnMediaChange, self);

    toxav_register_audio_callback(CallContrl.av, write_device_callback, NULL);
    */

    toxav_callback_call(CallContrl.av, call_cb, &CallContrl);
    toxav_callback_call_state(CallContrl.av, callstate_cb, &CallContrl);
    toxav_callback_audio_receive_frame(CallContrl.av, receive_audio_frame_cb, &CallContrl);
    toxav_callback_audio_bit_rate_status(CallContrl.av, audio_bit_rate_status_cb, &CallContrl);
    toxav_callback_video_receive_frame(CallContrl.av, receive_video_frame_cb, &CallContrl);
    toxav_callback_video_bit_rate_status(CallContrl.av, video_bit_rate_status_cb, &CallContrl);

    return CallContrl.av;
}

void terminate_audio()
{
    int i;
    for (i = 0; i < MAX_CALLS; ++i)
        stop_transmission(&CallContrl.calls[i], i);

    if ( CallContrl.av )
        toxav_kill(CallContrl.av);

    terminate_devices();
}

void read_device_callback (const int16_t* captured, uint32_t size, void* data)
{
    int32_t friend_number = *((int32_t*)data); /* TODO: Or pass an array of call_idx's */

    //uint8_t encoded_payload[RTP_PAYLOAD_SIZE];
    //int32_t payload_size = toxav_prepare_audio_frame(CallContrl.av, friend_number, encoded_payload, RTP_PAYLOAD_SIZE, captured, size);
    //if ( payload_size <= 0 || toxav_send_audio(CallContrl.av, friend_number, encoded_payload, payload_size) < 0 ) {
    //    /*fprintf(stderr, "Could not encode audio packet\n");*/
    //}
}

void write_device_callback(void *agent, int32_t friend_number, const int16_t* PCM, uint16_t size, void* arg)
{
    (void)arg;
    (void)agent;


    if (friend_number >= 0 && CallContrl.calls[friend_number].ttas) {
        //ToxAvCSettings csettings = CallContrl.cs;
        //toxav_get_peer_csettings(CallContrl.av, friend_number, 0, &csettings);
        write_out(CallContrl.calls[friend_number].out_idx, PCM, size, CallContrl.audio_channels);
    }
}

int start_transmission(ToxWindow *self, Call *call)
{
    if ( !self || !CallContrl.av || self->num == -1 ) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Could not prepare transmission");
        return -1;
    }

    /* Don't provide support for video */
    /*
    if ( 0 != toxav_prepare_transmission(CallContrl.av, self->num, 0) ) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Could not prepare transmission");
        return -1;
    }

    if ( !toxav_capability_supported(CallContrl.av, self->num, av_AudioDecoding) ||
         !toxav_capability_supported(CallContrl.av, self->num, av_AudioEncoding) )
        return -1;
    */

    if (set_call(call, true) == -1)
        return -1;

    //ToxAvCSettings csettings;
    //toxav_get_peer_csettings(CallContrl.av, self->call_idx, 0, &csettings);

    DeviceError test = open_primary_device(input, &call->in_idx, CallContrl.audio_bit_rate, CallContrl.audio_frame_duration, CallContrl.audio_channels);

    if ( test == de_FailedStart)
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed Start");

    if ( test == de_InternalError )
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Internal Error");

    if (  test != de_None )
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to open input device!");

    if ( register_device_callback(self->num, call->in_idx,
         read_device_callback, &self->num, true) != de_None)
        /* Set VAD as true for all; TODO: Make it more dynamic */
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to register input handler!");

    if ( open_primary_device(output, &call->out_idx,
            CallContrl.audio_bit_rate, CallContrl.audio_frame_duration, CallContrl.audio_channels) != de_None ) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to open output device!");
        call->has_output = 0;
    }

    return 0;
}

int stop_transmission(Call *call, int32_t friend_number)
{
    if ( call->ttas ) {
        TOXAV_ERR_CALL_CONTROL error;
        bool call_running = toxav_call_control(CallContrl.av, friend_number, TOXAV_CALL_CONTROL_CANCEL, &error); 
        //toxav_kill_transmission(CallContrl.av, friend_number);
        if(error == TOXAV_ERR_CALL_CONTROL_OK) {
            call->ttas = false;

            if ( call->in_idx != -1 )
                close_device(input, call->in_idx);

            if ( call->out_idx != -1 )
                close_device(output, call->out_idx);

            if (set_call(call, false) == -1)
                return -1;

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

void call_cb(ToxAV *av, uint32_t friend_number, bool audio_enabled, bool video_enabled, void *user_data)
{
    TOXAV_ERR_ANSWER error;
    CallControl* cc = user_data;
    ToxWindow* window = cc->window;
    cc->pending_call = true;

    line_info_add(window, NULL, NULL, NULL, SYS_MSG, 0, 0, "Incoming call callback! Friend number: %d", friend_number);

    callback_recv_invite(av, friend_number, user_data);
    //toxav_answer(CallContrl.av, friend_number, 48, 0, &error);
}

void callstate_cb(ToxAV *av, uint32_t friend_number, uint32_t state, void *user_data)
{
    CallControl* cc = user_data;
    ToxWindow* window = cc->window;
    cc->call_state = state;

    line_info_add(window, NULL, NULL, NULL, SYS_MSG, 0, 0, "Callstate callback! Friend number: %d", friend_number);

    switch(state) {
        case TOXAV_CALL_STATE_ERROR:
            line_info_add(window, NULL, NULL, NULL, SYS_MSG, 0, 0, "TOXAV_CALL_STATE_ERROR");
        break;
        case TOXAV_CALL_STATE_FINISHED:
            line_info_add(window, NULL, NULL, NULL, SYS_MSG, 0, 0, "TOXAV_CALL_STATE_FINISHED");

            if(CallContrl.pending_call) {
                line_info_add(window, NULL, NULL, NULL, SYS_MSG, 0, 0, "Pending Call Rejected");        
                CallContrl.pending_call = false;
                callback_call_rejected(CallContrl.av, window->num, &CallContrl);
            } else {
                line_info_add(window, NULL, NULL, NULL, SYS_MSG, 0, 0, "Existing Call Cancelled");
                callback_call_canceled(CallContrl.av, window->num, &CallContrl);
            }
        break;
        case TOXAV_CALL_STATE_SENDING_A:
            line_info_add(window, NULL, NULL, NULL, SYS_MSG, 0, 0, "TOXAV_CALL_STATE_SENDING_A");
        break;
        case TOXAV_CALL_STATE_SENDING_V:
            line_info_add(window, NULL, NULL, NULL, SYS_MSG, 0, 0, "TOXAV_CALL_STATE_SENDING_V");
        break;
        case TOXAV_CALL_STATE_RECEIVING_A:
            line_info_add(window, NULL, NULL, NULL, SYS_MSG, 0, 0, "TOXAV_CALL_STATE_RECEIVING_A");
        break;
        case TOXAV_CALL_STATE_RECEIVING_V:
            line_info_add(window, NULL, NULL, NULL, SYS_MSG, 0, 0, "TOXAV_CALL_STATE_RECEIVING_V");
        break;
        default:
        break;
    }
}

void receive_audio_frame_cb(ToxAV *av, uint32_t friend_number, int16_t const *pcm, size_t sample_count, uint8_t channels, uint32_t sampling_rate, void *user_data)
{
    CallControl* cc = user_data;

    write_device_callback(CallContrl.calls[friend_number].out_idx, friend_number, pcm, frame_size, cc);
}

void audio_bit_rate_status_cb(ToxAV *av, uint32_t friend_number, bool stable, uint32_t bit_rate, void *user_data)
{
    
    CallControl* cc = user_data;

    if (stable)
        cc->audio_bit_rate = bit_rate;
}

void receive_video_frame_cb(ToxAV *av, uint32_t friend_number,
                                    uint16_t width, uint16_t height,
                                    uint8_t const *y, uint8_t const *u, uint8_t const *v, uint8_t const *a,
                                    int32_t ystride, int32_t ustride, int32_t vstride, int32_t astride,
                                    void *user_data)
{

}

void video_bit_rate_status_cb(ToxAV *av, uint32_t friend_number, 
                                      bool stable, uint32_t bit_rate, void *user_data)
{
    CallControl* cc = user_data;

    if (stable)
        cc->video_bit_rate = bit_rate;
}



#define CB_BODY(friend_number, Arg, onFunc) do { ToxWindow* windows = (Arg); int i;\
for (i = 0; i < MAX_WINDOWS_NUM; ++i) if (windows[i].onFunc != NULL) windows[i].onFunc(&windows[i], CallContrl.av, friend_number); } while (0)


void callback_recv_invite ( void* av, uint32_t friend_number, void* arg )
{
    //CB_BODY(friend_number, arg, onInvite);
    CallControl* cc = (CallControl*)arg;
    ToxWindow* windows = cc->window;

    int i;
    for (i = 0; i < MAX_WINDOWS_NUM; ++i)
        if (windows[i].onInvite != NULL && windows[i].num == friend_number) {
            windows[i].onInvite(&windows[i], cc->av, friend_number, cc->call_state);
        } else {
            //if(windows[i].num != friend_number)
                //line_info_add(&windows, NULL, NULL, NULL, SYS_MSG, 0, 0, "Non-matching friend number");
        }
}

void callback_recv_ringing ( void* av, uint32_t friend_number, void* arg )
{
    //CB_BODY(friend_number, arg, onRinging);
    CallControl* cc = arg;
    ToxWindow* windows = cc->window;
    int i;
    for (i = 0; i < MAX_WINDOWS_NUM; ++i)
        if (windows[i].onRinging != NULL && windows[i].num == friend_number) {
            windows[i].onRinging(&windows[i], cc->av, friend_number, cc->call_state);
        }
}
void callback_recv_starting ( void* av, uint32_t friend_number, void* arg )
{
    CallControl* cc = arg;
    ToxWindow* windows = cc->window;
    int i;
    for (i = 0; i < MAX_WINDOWS_NUM; ++i)
        if (windows[i].onStarting != NULL && windows[i].num == friend_number) {
            windows[i].onStarting(&windows[i], cc->av, friend_number, cc->call_state);
            if ( 0 != start_transmission(&windows[i], &CallContrl.calls[friend_number])) {/* YEAH! */
                line_info_add(&windows[i], NULL, NULL, NULL, SYS_MSG, 0, 0 , "Error starting transmission!");
            }
            return;
        }
}
void callback_recv_ending ( void* av, uint32_t friend_number, void* arg )
{
    //CB_BODY(friend_number, arg, onEnding);

    CallControl* cc = arg;
    ToxWindow* windows = cc->window;
    int i;
    for (i = 0; i < MAX_WINDOWS_NUM; ++i)
        if (windows[i].onEnding != NULL && windows[i].num == friend_number) {
            windows[i].onEnding(&windows[i], cc->av, friend_number, cc->call_state);
        }

    stop_transmission(&CallContrl.calls[friend_number], friend_number);
}

void callback_call_started ( void* av, uint32_t friend_number, void* arg )
{
    CallControl* cc = arg;
    ToxWindow* windows = cc->window;
    int i;
    for (i = 0; i < MAX_WINDOWS_NUM; ++i)
        if (windows[i].onStart != NULL && windows[i].num == friend_number) {
            windows[i].onStart(&windows[i], cc->av, friend_number, cc->call_state);
            if ( 0 != start_transmission(&windows[i], &CallContrl.calls[friend_number]) ) {/* YEAH! */
                line_info_add(&windows[i], NULL, NULL, NULL, SYS_MSG, 0, 0, "Error starting transmission!");
                return;
            }
        }
}
void callback_call_canceled ( void* av, uint32_t friend_number, void* arg )
{
    //CB_BODY(friend_number, arg, onCancel);
    CallControl* cc = arg;
    ToxWindow* windows = cc->window;

    int i;
    for (i = 0; i < MAX_WINDOWS_NUM; ++i)
        if (windows[i].onCancel != NULL && windows[i].num == friend_number) {
            windows[i].onCancel(&windows[i], cc->av, friend_number, cc->call_state);
        }

    /* In case call is active */
    stop_transmission(&CallContrl.calls[friend_number], friend_number);
}
void callback_call_rejected ( void* av, uint32_t friend_number, void* arg )
{
    //CB_BODY(friend_number, arg, onReject);

    CallControl* cc = arg;
    ToxWindow* windows = cc->window;

    int i;
    for (i = 0; i < MAX_WINDOWS_NUM; ++i)
        if (windows[i].onReject != NULL && windows[i].num == friend_number) {
            windows[i].onReject(&windows[i], cc->av, friend_number, cc->call_state);
        }

}
void callback_call_ended ( void* av, uint32_t friend_number, void* arg )
{
    //CB_BODY(friend_number, arg, onEnd);
    CallControl* cc = arg;
    ToxWindow* windows = cc->window;

    int i;
    for (i = 0; i < MAX_WINDOWS_NUM; ++i)
        if (windows[i].onEnd != NULL && windows[i].num == friend_number) {
            windows[i].onEnd(&windows[i], cc->av, friend_number, cc->call_state);
        }

    stop_transmission(&CallContrl.calls[friend_number], friend_number);
}

void callback_requ_timeout ( void* av, uint32_t friend_number, void* arg )
{
    //CB_BODY(friend_number, arg, onRequestTimeout);
    CallControl* cc = arg;
    ToxWindow* windows = cc->window;

    int i;
    for (i = 0; i < MAX_WINDOWS_NUM; ++i)
        if (windows[i].onRequestTimeout != NULL && windows[i].num == friend_number) {
            windows[i].onRequestTimeout(&windows[i], cc->av, friend_number, cc->call_state);
        }
}
void callback_peer_timeout ( void* av, uint32_t friend_number, void* arg )
{
    //CB_BODY(friend_number, arg, onPeerTimeout);
    CallControl* cc = arg;
    ToxWindow* windows = cc->window;

    int i;
    for (i = 0; i < MAX_WINDOWS_NUM; ++i)
        if (windows[i].onPeerTimeout != NULL && windows[i].num == friend_number) {
            windows[i].onPeerTimeout(&windows[i], cc->av, friend_number, cc->call_state);
        }

    stop_transmission(&CallContrl.calls[friend_number], friend_number);
    /* Call is stopped manually since there might be some other
     * actions that one can possibly take on timeout
     */
    TOXAV_ERR_CALL_CONTROL error;
    bool call_running = toxav_call_control(cc->av, friend_number, TOXAV_CALL_CONTROL_CANCEL, &error);
}

// void callback_media_change(void* av, int32_t friend_number, void* arg)
// {
  /*... TODO cancel all media change requests */
// }

/*
 * End of Callbacks
 */


/*
 * Commands from chat_commands.h
 */
void cmd_call(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    TOXAV_ERR_CALL error;
    const char *error_str;

    if (argc != 0) {
        error_str = "Unknown arguments.";
        goto on_error;
    }

    if ( !CallContrl.av ) {
        error_str = "ToxAV not supported!";
        goto on_error;
    }

    if (!self->stb->connection) {
        error_str = "Friend is offline.";
        goto on_error;
    }

    bool call_running;
    call_running = toxav_call(CallContrl.av, self->num, CallContrl.audio_bit_rate, CallContrl.video_bit_rate, &error);

    if ( !call_running ) {
        if ( error == TOXAV_ERR_CALL_FRIEND_ALREADY_IN_CALL ) error_str = "Already in a call!";
        else if ( error == TOXAV_ERR_CALL_MALLOC ) error_str = "Resource allocation issue";
        else if ( error == TOXAV_ERR_CALL_FRIEND_NOT_FOUND) error_str = "Friend number invalid";
        else if ( error == TOXAV_ERR_CALL_FRIEND_NOT_CONNECTED) error_str = "Friend is valid but not currently connected";
        else error_str = "Internal error!";

        goto on_error;
    }

    CallContrl.pending_call = true;
    callback_recv_ringing(CallContrl.av, self->num, &CallContrl);

    return;
on_error:
    print_err(self, error_str);
}

void cmd_answer(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    TOXAV_ERR_ANSWER error;
    const char *error_str;

    if (argc != 0) {
        error_str = "Unknown arguments.";
        goto on_error;
    }

    if ( !CallContrl.av ) {
        error_str = "Audio not supported!";
        goto on_error;
    }

    bool call_running = toxav_answer(CallContrl.av, self->num, 48, 0, &error);

    if ( !call_running ) {
        //if ( error != TOXAV_ERR_ANSWER_OK ) error_str = "Cannot answer in invalid state!";
        if ( error == TOXAV_ERR_ANSWER_FRIEND_NOT_CALLING ) error_str = "No incoming call!";
        else if ( error == TOXAV_ERR_ANSWER_CODEC_INITIALIZATION ) error_str = "Failed to initialize codecs";
        else if ( error == TOXAV_ERR_ANSWER_FRIEND_NOT_FOUND ) error_str = "Friend not found";
        else if ( error == TOXAV_ERR_ANSWER_INVALID_BIT_RATE ) error_str = "Invalid bit rate";
        else error_str = "Internal error!";

        goto on_error;
    }

    /* Callback will print status... */

    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Starting call");
    CallContrl.pending_call = false;
    callback_recv_starting(CallContrl.av, self->num, &CallContrl);

    return;
on_error:
    print_err (self, error_str);
}

void cmd_reject(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    TOXAV_ERR_CALL_CONTROL error;
    const char *error_str;

    if (argc != 0) {
        error_str = "Unknown arguments.";
        goto on_error;
    }

    if ( !CallContrl.av ) {
        error_str = "Audio not supported!";
        goto on_error;
    }

    bool call_running;
    call_running = toxav_call_control(CallContrl.av, self->num, TOXAV_CALL_CONTROL_CANCEL, &error);
    //ToxAvError error = toxav_reject(CallContrl.av, self->call_idx, "Why not?");

    if ( error != TOXAV_ERR_CALL_CONTROL_OK ) {
        if ( error == TOXAV_ERR_CALL_CONTROL_INVALID_TRANSITION ) error_str = "Cannot reject in invalid state!";
        else if ( CallContrl.pending_call == false ) error_str = "No incoming call!";
        else error_str = "Internal error!";

        goto on_error;
    }

    /* Callback will print status... */

    CallContrl.pending_call = false;
    callback_call_rejected(CallContrl.av, self->num, &CallContrl);
    return;
on_error:
    print_err (self, error_str);
}

void cmd_hangup(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    TOXAV_ERR_CALL_CONTROL error;
    const char *error_str;

    if (argc != 0) {
        error_str = "Unknown arguments.";
        goto on_error;
    }

    if ( !CallContrl.av ) {
        error_str = "Audio not supported!";
        goto on_error;
    }

    bool call_running;
    if( CallContrl.call_state != TOXAV_CALL_STATE_SENDING_A | TOXAV_CALL_STATE_RECEIVING_A | TOXAV_CALL_STATE_SENDING_V | TOXAV_CALL_STATE_RECEIVING_V ) {
        call_running = toxav_call_control(CallContrl.av, self->num, TOXAV_CALL_CONTROL_CANCEL, &error);
#ifdef SOUND_NOTIFY
        stop_sound(self->ringing_sound);
#endif
        CallContrl.pending_call = false;
        callback_call_canceled(CallContrl.av, self->num, &CallContrl);
    } else {
        call_running = toxav_call_control(CallContrl.av, &self->num, TOXAV_CALL_CONTROL_CANCEL, &error);
        callback_call_ended(CallContrl.av, self->num, &CallContrl);
    }

    if ( error != TOXAV_ERR_CALL_CONTROL_OK ) {
        if ( error == TOXAV_ERR_CALL_CONTROL_INVALID_TRANSITION ) error_str = "Cannot hangup in invalid state!";
        else if ( error == TOXAV_ERR_CALL_CONTROL_FRIEND_NOT_IN_CALL ) error_str = "No call!";
        else error_str = "Internal error!";

        goto on_error;
    }

    return;
on_error:
    print_err (self, error_str);
}

void cmd_list_devices(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    const char *error_str;

    if ( argc != 1 ) {
        if ( argc < 1 ) error_str = "Type must be specified!";
        else error_str = "Only one argument allowed!";

        goto on_error;
    }

    DeviceType type;

    if ( strcasecmp(argv[1], "in") == 0 ) /* Input devices */
        type = input;

    else if ( strcasecmp(argv[1], "out") == 0 ) /* Output devices */
        type = output;

    else {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Invalid type: %s", argv[1]);
        return;
    }

    print_devices(self, type);

    return;
on_error:
    print_err (self, error_str);
}

/* This changes primary device only */
void cmd_change_device(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    const char *error_str;

    if ( argc != 2 ) {
        if ( argc < 1 ) error_str = "Type must be specified!";
        else if ( argc < 2 ) error_str = "Must have id!";
        else error_str = "Only two arguments allowed!";

        goto on_error;
    }

    DeviceType type;

    if ( strcmp(argv[1], "in") == 0 ) /* Input devices */
        type = input;

    else if ( strcmp(argv[1], "out") == 0 ) /* Output devices */
        type = output;

    else {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Invalid type: %s", argv[1]);
        return;
    }


    char *end;
    long int selection = strtol(argv[2], &end, 10);

    if ( *end ) {
        error_str = "Invalid input";
        goto on_error;
    }

    if ( set_primary_device(type, selection) == de_InvalidSelection ) {
        error_str="Invalid selection!";
        goto on_error;
    }

    return;
on_error:
    print_err (self, error_str);
}

void cmd_ccur_device(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    const char *error_str;

    if ( argc != 2 ) {
        if ( argc < 1 ) error_str = "Type must be specified!";
        else if ( argc < 2 ) error_str = "Must have id!";
        else error_str = "Only two arguments allowed!";

        goto on_error;
    }

    DeviceType type;

    if ( strcmp(argv[1], "in") == 0 ) /* Input devices */
        type = input;

    else if ( strcmp(argv[1], "out") == 0 ) /* Output devices */
        type = output;

    else {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Invalid type: %s", argv[1]);
        return;
    }


    char *end;
    long int selection = strtol(argv[2], &end, 10);

    if ( *end ) {
        error_str = "Invalid input";
        goto on_error;
    }

    if ( selection_valid(type, selection) == de_InvalidSelection ) {
        error_str="Invalid selection!";
        goto on_error;
    }

    /* If call is active, change device */
    if ( self->num > -1) {
        Call* this_call = &CallContrl.calls[self->num];
        if (this_call->ttas) {

            //ToxAvCSettings csettings;
            //toxav_get_peer_csettings(CallContrl.av, self->num, 0, &csettings);

            if (type == output) {
                pthread_mutex_lock(&this_call->mutex);
                close_device(output, this_call->out_idx);
                this_call->has_output = open_device(output, selection, &this_call->out_idx,
                    CallContrl.audio_bit_rate, CallContrl.audio_frame_duration, CallContrl.audio_channels)
                    == de_None ? 1 : 0;
                pthread_mutex_unlock(&this_call->mutex);
            }
            else {
                /* TODO: check for failure */
                close_device(input, this_call->in_idx);
                open_device(input, selection, &this_call->in_idx, CallContrl.audio_bit_rate,
                    CallContrl.audio_frame_duration, CallContrl.audio_channels);
                /* Set VAD as true for all; TODO: Make it more dynamic */
                register_device_callback(self->num, this_call->in_idx, read_device_callback, &self->num, true);
            }
        }
    }

    self->device_selection[type] = selection;

    return;
    on_error:
    print_err (self, error_str);
}

void cmd_mute(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    const char *error_str;

    if ( argc != 1 ) {
        if ( argc < 1 ) error_str = "Type must be specified!";
        else error_str = "Only two arguments allowed!";

        goto on_error;
    }

    DeviceType type;

    if ( strcasecmp(argv[1], "in") == 0 ) /* Input devices */
        type = input;

    else if ( strcasecmp(argv[1], "out") == 0 ) /* Output devices */
        type = output;

    else {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Invalid type: %s", argv[1]);
        return;
    }


    /* If call is active, use this_call values */
    if ( self->num > -1) {
        Call* this_call = &CallContrl.calls[self->num];

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
    print_err (self, error_str);
}

void cmd_sense(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    const char *error_str;

    if ( argc != 1 ) {
        if ( argc < 1 ) error_str = "Must have value!";
        else error_str = "Only two arguments allowed!";

        goto on_error;
    }

    char *end;
    float value = strtof(argv[1], &end);

    if ( *end ) {
        error_str = "Invalid input";
        goto on_error;
    }

    /* Call must be active */
    if ( self->num > -1) {
        device_set_VAD_treshold(CallContrl.calls[self->num].in_idx, value);
        self->chatwin->infobox.vad_lvl = value;
    }

    return;

on_error:
    print_err (self, error_str);
}


void stop_current_call(ToxWindow* self)
{
    TOXAV_ERR_CALL_CONTROL error;
    bool call_running = toxav_call_control(CallContrl.av, &self->num, TOXAV_CALL_CONTROL_CANCEL, &error);

    /*
    ToxAvCallState callstate;
    if ( CallContrl.av != NULL && self->num != -1 &&
        CallContrl.call_state != TOXAV_CALL_STATE_SENDING_A | TOXAV_CALL_STATE_RECEIVING_A | TOXAV_CALL_STATE_SENDING_V | TOXAV_CALL_STATE_RECEIVING_V) {
        switch (callstate)
        {
        case av_CallActive:
        case av_CallHold:
            //toxav_hangup(CallContrl.av, self->num);
            toxav_call_control(CallContrl.av, &self->num, TOXAV_CALL_CONTROL_CANCEL, &error);
            break;
        case av_CallInviting:
            //toxav_cancel(CallContrl.av, self->num, 0, "Not interested anymore");
            toxav_call_control(CallContrl.av, &self->num, TOXAV_CALL_CONTROL_CANCEL, &error);
            break;
        case av_CallStarting:
            //toxav_reject(CallContrl.av, self->num, "Not interested");
            toxav_call_control(CallContrl.av, &self->num, TOXAV_CALL_CONTROL_CANCEL, &error);
            break;
        default:
            break;
        }
    }
    */
}

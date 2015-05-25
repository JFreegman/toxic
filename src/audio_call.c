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

#define frame_size (av_DefaultSettings.audio_sample_rate * av_DefaultSettings.audio_frame_duration / 1000)

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

struct ASettings {
    AudioError errors;

    ToxAv *av;

    ToxAvCSettings cs;

    Call calls[MAX_CALLS];
} ASettins;

void callback_recv_invite   ( void* av, int32_t call_index, void *arg );
void callback_recv_ringing  ( void* av, int32_t call_index, void *arg );
void callback_recv_starting ( void* av, int32_t call_index, void *arg );
void callback_recv_ending   ( void* av, int32_t call_index, void *arg );
void callback_call_started  ( void* av, int32_t call_index, void *arg );
void callback_call_canceled ( void* av, int32_t call_index, void *arg );
void callback_call_rejected ( void* av, int32_t call_index, void *arg );
void callback_call_ended    ( void* av, int32_t call_index, void *arg );
void callback_requ_timeout  ( void* av, int32_t call_index, void *arg );
void callback_peer_timeout  ( void* av, int32_t call_index, void *arg );
void callback_media_change  ( void* av, int32_t call_index, void *arg );

void write_device_callback( void* agent, int32_t call_index, const int16_t* PCM, uint16_t size, void* arg );

static void print_err (ToxWindow *self, const char *error_str)
{
    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "%s", error_str);
}

ToxAv *init_audio(ToxWindow *self, Tox *tox)
{
    ASettins.cs = av_DefaultSettings;
    ASettins.cs.max_video_height = ASettins.cs.max_video_width = 0;

    ASettins.errors = ae_None;

    memset(ASettins.calls, 0, sizeof(ASettins.calls));


    /* Streaming stuff from core */

    ASettins.av = toxav_new(tox, MAX_CALLS);

    if ( !ASettins.av ) {
        ASettins.errors |= ae_StartingCoreAudio;
        return NULL;
    }

    if ( init_devices(ASettins.av) == de_InternalError ) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to init devices");
        toxav_kill(ASettins.av);
        return ASettins.av = NULL;
    }

    toxav_register_callstate_callback(ASettins.av, callback_call_started, av_OnStart, self);
    toxav_register_callstate_callback(ASettins.av, callback_call_canceled, av_OnCancel, self);
    toxav_register_callstate_callback(ASettins.av, callback_call_rejected, av_OnReject, self);
    toxav_register_callstate_callback(ASettins.av, callback_call_ended, av_OnEnd, self);
    toxav_register_callstate_callback(ASettins.av, callback_recv_invite, av_OnInvite, self);

    toxav_register_callstate_callback(ASettins.av, callback_recv_ringing, av_OnRinging, self);
    toxav_register_callstate_callback(ASettins.av, callback_recv_starting, av_OnStart, self);
    toxav_register_callstate_callback(ASettins.av, callback_recv_ending, av_OnEnd, self);

    toxav_register_callstate_callback(ASettins.av, callback_requ_timeout, av_OnRequestTimeout, self);
    toxav_register_callstate_callback(ASettins.av, callback_peer_timeout, av_OnPeerTimeout, self);
    //toxav_register_callstate_callback(ASettins.av, callback_media_change, av_OnMediaChange, self);

    toxav_register_audio_callback(ASettins.av, write_device_callback, NULL);

    return ASettins.av;
}

void terminate_audio()
{
    int i;
    for (i = 0; i < MAX_CALLS; ++i)
        stop_transmission(&ASettins.calls[i], i);

    if ( ASettins.av )
        toxav_kill(ASettins.av);

    terminate_devices();
}

void read_device_callback (const int16_t* captured, uint32_t size, void* data)
{
    int32_t call_index = *((int32_t*)data); /* TODO: Or pass an array of call_idx's */

    uint8_t encoded_payload[RTP_PAYLOAD_SIZE];
    int32_t payload_size = toxav_prepare_audio_frame(ASettins.av, call_index, encoded_payload, RTP_PAYLOAD_SIZE, captured, size);
    if ( payload_size <= 0 || toxav_send_audio(ASettins.av, call_index, encoded_payload, payload_size) < 0 ) {
        /*fprintf(stderr, "Could not encode audio packet\n");*/
    }
}

void write_device_callback(void *agent, int32_t call_index, const int16_t* PCM, uint16_t size, void* arg)
{
    (void)arg;
    (void)agent;

    if (call_index >= 0 && ASettins.calls[call_index].ttas) {
        ToxAvCSettings csettings = ASettins.cs;
        toxav_get_peer_csettings(ASettins.av, call_index, 0, &csettings);
        write_out(ASettins.calls[call_index].out_idx, PCM, size, csettings.audio_channels);
    }
}

int start_transmission(ToxWindow *self, Call *call)
{
    if ( !self || !ASettins.av || self->call_idx == -1 ) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Could not prepare transmission");
        return -1;
    }

    /* Don't provide support for video */
    if ( 0 != toxav_prepare_transmission(ASettins.av, self->call_idx, 0) ) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Could not prepare transmission");
        return -1;
    }

    if ( !toxav_capability_supported(ASettins.av, self->call_idx, av_AudioDecoding) ||
         !toxav_capability_supported(ASettins.av, self->call_idx, av_AudioEncoding) )
        return -1;

    if (set_call(call, true) == -1)
        return -1;

    ToxAvCSettings csettings;
    toxav_get_peer_csettings(ASettins.av, self->call_idx, 0, &csettings);

    if ( open_primary_device(input, &call->in_idx,
            csettings.audio_sample_rate, csettings.audio_frame_duration, csettings.audio_channels) != de_None )
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to open input device!");

    if ( register_device_callback(self->call_idx, call->in_idx,
         read_device_callback, &self->call_idx, true) != de_None)
        /* Set VAD as true for all; TODO: Make it more dynamic */
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to register input handler!");

    if ( open_primary_device(output, &call->out_idx,
            csettings.audio_sample_rate, csettings.audio_frame_duration, csettings.audio_channels) != de_None ) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to open output device!");
        call->has_output = 0;
    }

    return 0;
}

int stop_transmission(Call *call, int32_t call_index)
{
    if ( call->ttas ) {
        toxav_kill_transmission(ASettins.av, call_index);
        call->ttas = false;

        if ( call->in_idx != -1 )
            close_device(input, call->in_idx);

        if ( call->out_idx != -1 )
            close_device(output, call->out_idx);

        if (set_call(call, false) == -1)
            return -1;

        return 0;
    }

    return -1;
}
/*
 * End of transmission
 */





/*
 * Callbacks
 */

#define CB_BODY(call_idx, Arg, onFunc) do { ToxWindow* windows = (Arg); int i;\
for (i = 0; i < MAX_WINDOWS_NUM; ++i) if (windows[i].onFunc != NULL) windows[i].onFunc(&windows[i], ASettins.av, call_idx); } while (0)

void callback_recv_invite ( void* av, int32_t call_index, void* arg )
{
    CB_BODY(call_index, arg, onInvite);
}
void callback_recv_ringing ( void* av, int32_t call_index, void* arg )
{
    CB_BODY(call_index, arg, onRinging);
}
void callback_recv_starting ( void* av, int32_t call_index, void* arg )
{
    ToxWindow* windows = arg;
    int i;
    for (i = 0; i < MAX_WINDOWS_NUM; ++i)
        if (windows[i].onStarting != NULL && windows[i].call_idx == call_index) {
            windows[i].onStarting(&windows[i], ASettins.av, call_index);
            if ( 0 != start_transmission(&windows[i], &ASettins.calls[call_index])) {/* YEAH! */
                line_info_add(&windows[i], NULL, NULL, NULL, SYS_MSG, 0, 0 , "Error starting transmission!");
            }
            return;
        }
}
void callback_recv_ending ( void* av, int32_t call_index, void* arg )
{
    CB_BODY(call_index, arg, onEnding);
    stop_transmission(&ASettins.calls[call_index], call_index);
}

void callback_call_started ( void* av, int32_t call_index, void* arg )
{
    ToxWindow* windows = arg;
    int i;
    for (i = 0; i < MAX_WINDOWS_NUM; ++i)
        if (windows[i].onStart != NULL && windows[i].call_idx == call_index) {
            windows[i].onStart(&windows[i], ASettins.av, call_index);
            if ( 0 != start_transmission(&windows[i], &ASettins.calls[call_index]) ) {/* YEAH! */
                line_info_add(&windows[i], NULL, NULL, NULL, SYS_MSG, 0, 0, "Error starting transmission!");
                return;
            }
        }
}
void callback_call_canceled ( void* av, int32_t call_index, void* arg )
{
    CB_BODY(call_index, arg, onCancel);

    /* In case call is active */
    stop_transmission(&ASettins.calls[call_index], call_index);
}
void callback_call_rejected ( void* av, int32_t call_index, void* arg )
{
    CB_BODY(call_index, arg, onReject);
}
void callback_call_ended ( void* av, int32_t call_index, void* arg )
{
    CB_BODY(call_index, arg, onEnd);
    stop_transmission(&ASettins.calls[call_index], call_index);
}

void callback_requ_timeout ( void* av, int32_t call_index, void* arg )
{
    CB_BODY(call_index, arg, onRequestTimeout);
}
void callback_peer_timeout ( void* av, int32_t call_index, void* arg )
{
    CB_BODY(call_index, arg, onPeerTimeout);
    stop_transmission(&ASettins.calls[call_index], call_index);
    /* Call is stopped manually since there might be some other
     * actions that one can possibly take on timeout
     */
    toxav_stop_call(ASettins.av, call_index);
}

// void callback_media_change(void* av, int32_t call_index, void* arg)
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
    const char *error_str;

    if (argc != 0) {
        error_str = "Unknown arguments.";
        goto on_error;
    }

    if ( !ASettins.av ) {
        error_str = "Audio not supported!";
        goto on_error;
    }

    if (!self->stb->connection) {
        error_str = "Friend is offline.";
        goto on_error;
    }

    ToxAvError error = toxav_call(ASettins.av, &self->call_idx, self->num, &ASettins.cs, 30);

    if ( error != av_ErrorNone ) {
        if ( error == av_ErrorAlreadyInCallWithPeer ) error_str = "Already in a call!";
        else error_str = "Internal error!";

        goto on_error;
    }

    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Calling... idx: %d", self->call_idx);

    return;
on_error:
    print_err(self, error_str);
}

void cmd_answer(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    const char *error_str;

    if (argc != 0) {
        error_str = "Unknown arguments.";
        goto on_error;
    }

    if ( !ASettins.av ) {
        error_str = "Audio not supported!";
        goto on_error;
    }

    ToxAvError error = toxav_answer(ASettins.av, self->call_idx, &ASettins.cs);

    if ( error != av_ErrorNone ) {
        if ( error == av_ErrorInvalidState ) error_str = "Cannot answer in invalid state!";
        else if ( error == av_ErrorNoCall ) error_str = "No incoming call!";
        else error_str = "Internal error!";

        goto on_error;
    }

    /* Callback will print status... */

    return;
on_error:
    print_err (self, error_str);
}

void cmd_reject(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    const char *error_str;

    if (argc != 0) {
        error_str = "Unknown arguments.";
        goto on_error;
    }

    if ( !ASettins.av ) {
        error_str = "Audio not supported!";
        goto on_error;
    }

    ToxAvError error = toxav_reject(ASettins.av, self->call_idx, "Why not?");

    if ( error != av_ErrorNone ) {
        if ( error == av_ErrorInvalidState ) error_str = "Cannot reject in invalid state!";
        else if ( error == av_ErrorNoCall ) error_str = "No incoming call!";
        else error_str = "Internal error!";

        goto on_error;
    }

    /* Callback will print status... */

    return;
on_error:
    print_err (self, error_str);
}

void cmd_hangup(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    const char *error_str;

    if (argc != 0) {
        error_str = "Unknown arguments.";
        goto on_error;
    }

    if ( !ASettins.av ) {
        error_str = "Audio not supported!";
        goto on_error;
    }

    ToxAvError error;

    if (toxav_get_call_state(ASettins.av, self->call_idx) == av_CallInviting) {
        error = toxav_cancel(ASettins.av, self->call_idx, self->num,
                                        "Only those who appreciate small things know the beauty that is life");
#ifdef SOUND_NOTIFY
        stop_sound(self->ringing_sound);
#endif
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Call canceled!");
    } else {
        error = toxav_hangup(ASettins.av, self->call_idx);
    }

    if ( error != av_ErrorNone ) {
        if ( error == av_ErrorInvalidState ) error_str = "Cannot hangup in invalid state!";
        else if ( error == av_ErrorNoCall ) error_str = "No call!";
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
    if ( self->call_idx > -1) {
        Call* this_call = &ASettins.calls[self->call_idx];
        if (this_call->ttas) {

            ToxAvCSettings csettings;
            toxav_get_peer_csettings(ASettins.av, self->call_idx, 0, &csettings);

            if (type == output) {
                pthread_mutex_lock(&this_call->mutex);
                close_device(output, this_call->out_idx);
                this_call->has_output = open_device(output, selection, &this_call->out_idx,
                    csettings.audio_sample_rate, csettings.audio_frame_duration, csettings.audio_channels)
                    == de_None ? 1 : 0;
                pthread_mutex_unlock(&this_call->mutex);
            }
            else {
                /* TODO: check for failure */
                close_device(input, this_call->in_idx);
                open_device(input, selection, &this_call->in_idx, csettings.audio_sample_rate,
                    csettings.audio_frame_duration, csettings.audio_channels);
                /* Set VAD as true for all; TODO: Make it more dynamic */
                register_device_callback(self->call_idx, this_call->in_idx, read_device_callback, &self->call_idx, true);
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
    if ( self->call_idx > -1) {
        Call* this_call = &ASettins.calls[self->call_idx];

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
    if ( self->call_idx > -1) {
        device_set_VAD_treshold(ASettins.calls[self->call_idx].in_idx, value);
        self->chatwin->infobox.vad_lvl = value;
    }

    return;

on_error:
    print_err (self, error_str);
}


void stop_current_call(ToxWindow* self)
{
    ToxAvCallState callstate;
    if ( ASettins.av != NULL && self->call_idx != -1 &&
       ( callstate = toxav_get_call_state(ASettins.av, self->call_idx) ) != av_CallNonExistent) {
        switch (callstate)
        {
        case av_CallActive:
        case av_CallHold:
            toxav_hangup(ASettins.av, self->call_idx);
            break;
        case av_CallInviting:
            toxav_cancel(ASettins.av, self->call_idx, 0, "Not interested anymore");
            break;
        case av_CallStarting:
            toxav_reject(ASettins.av, self->call_idx, "Not interested");
            break;
        default:
            break;
        }
    }
}

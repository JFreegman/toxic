/*
 * Toxic -- Tox Curses Client
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "toxic_windows.h"
#include "audio_call.h"
#include "chat_commands.h"
#include "global_commands.h"
#include "toxic_windows.h"
#include "line_info.h"

#include <curses.h>
#include <AL/al.h>
#include <AL/alc.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>

#define _cbend pthread_exit(NULL)

#define AUDIO_FRAME_SIZE (av_DefaultSettings.audio_sample_rate * av_DefaultSettings.audio_frame_duration / 1000)

typedef struct _DeviceIx {

    ALCdevice *dhndl; /* Handle of device selected/opened */
    ALCcontext *ctx; /* Device context */
    const char *devices[MAX_DEVICES]; /* Container of available devices */
    int size; /* Size of above container */
    int dix; /* Index of default device */
    int index; /* Current index */
} DeviceIx;

struct _ASettings {

    DeviceIx device[2];

    AudioError errors;

    ToxAv *av;

    pthread_t ttid; /* Transmission thread id */
    int ttas; /* Transmission thread active status (0 - stopped, 1- running) */
} ASettins;

void callback_recv_invite ( void *arg );
void callback_recv_ringing ( void *arg );
void callback_recv_starting ( void *arg );
void callback_recv_ending ( void *arg );
void callback_recv_error ( void *arg );
void callback_call_started ( void *arg );
void callback_call_canceled ( void *arg );
void callback_call_rejected ( void *arg );
void callback_call_ended ( void *arg );
void callback_requ_timeout ( void *arg );
void callback_peer_timeout ( void *arg );


static void print_err (ToxWindow *self, uint8_t *error_str)
{
    line_info_add(self, NULL, NULL, NULL, error_str, SYS_MSG, 0, 0);
}

int device_set(ToxWindow *self, _Devices type, long int selection)
{
    uint8_t error_str[MAX_STR_SIZE];
    uint8_t *s_type = type == input ? "input" : "output";

    if ( selection < 0 || selection >= ASettins.device[type].size ) {
        snprintf(error_str, sizeof(error_str), "Cannot set audio %s device: Invalid index", s_type);
        line_info_add(self, NULL, NULL, NULL, error_str, SYS_MSG, 0, 0);
        return -1;
    }

    ASettins.device[type].index = selection;
    return 0;
}

/* Opens device under current index
 */
int device_open (ToxWindow *self, _Devices type)
{
    WINDOW *window = self->chatwin->history;

    /* Do not error if no device */
    if ( !ASettins.device[type].size ) return 0;

    ALCdevice *prev_device = ASettins.device[type].dhndl;

    uint8_t msg[MAX_STR_SIZE];
    uint8_t *error = NULL;

    if ( type == input ) {
        ASettins.device[type].dhndl = alcCaptureOpenDevice(
                                          ASettins.device[type].devices[ASettins.device[type].index],
                                          av_DefaultSettings.audio_sample_rate,
                                          AL_FORMAT_MONO16,
                                          AUDIO_FRAME_SIZE * 4);

        if (alcGetError(ASettins.device[type].dhndl) != AL_NO_ERROR) {

            /* Now check if we have previous device and act acording to it */
            if ( !prev_device ) {
                error = "Error starting input device!";

                ASettins.errors |= ErrorStartingCaptureDevice;
            } else {
                error = "Could not start input device, falling back to previous";

                /* NOTE: What if device is opened? */
                ASettins.device[type].dhndl = prev_device;
            }
        } else {
            /* Close previous */
            if ( prev_device )
                alcCaptureCloseDevice(prev_device);

            if ( window ) {
                snprintf(msg, sizeof(msg), "Input device: %s", ASettins.device[type].devices[ASettins.device[type].index]);
                line_info_add(self, NULL, NULL, NULL, msg, SYS_MSG, 0, 0);
            }
        }

        ASettins.device[type].ctx = NULL;
    } else {
        ASettins.device[type].dhndl = alcOpenDevice(ASettins.device[type].devices[ASettins.device[type].index]);

        if (alcGetError(ASettins.device[type].dhndl) != AL_NO_ERROR) {

            /* Now check if we have previous device and act acording to it */
            if ( !prev_device ) {
                error = "Error starting output device!";

                ASettins.errors |= ErrorStartingOutputDevice;
                ASettins.device[type].ctx = NULL;
            } else {
                error = "Could not start output device, falling back to previous";

                /* NOTE: What if device is opened? */
                ASettins.device[type].dhndl = prev_device;
            }
        } else {

            /* Close previous */
            if ( prev_device ) {
                alcCaptureCloseDevice(prev_device);
                alcMakeContextCurrent(NULL);
                alcDestroyContext(ASettins.device[type].ctx);
            }

            ASettins.device[type].ctx = alcCreateContext(ASettins.device[type].dhndl, NULL);

            if ( window ) {
                snprintf(msg, sizeof(msg), "Output device: %s", ASettins.device[type].devices[ASettins.device[type].index]);
                line_info_add(self, NULL, NULL, NULL, msg, SYS_MSG, 0, 0);
            }
        }
    }

    if ( error ) {
        if ( window ) {
            snprintf(msg, sizeof(msg), "Error: %s", error);
            line_info_add(self, NULL, NULL, NULL, msg, SYS_MSG, 0, 0);
        }

        return -1;
    } else return 0;
}

int device_close (ToxWindow *self, _Devices type)
{
    uint8_t *device = NULL;

    if ( ASettins.device[type].dhndl ) {
        if (type == input) {
            alcCaptureCloseDevice(ASettins.device[type].dhndl);
            device = "input";
        } else {
            alcCloseDevice(ASettins.device[type].dhndl);
            alcMakeContextCurrent(NULL);

            if ( ASettins.device[type].ctx )
                alcDestroyContext(ASettins.device[type].ctx);

            device = "output";
        }

        ASettins.device[type].index = ASettins.device[type].dix;
    }

    if ( self && device ) {
        uint8_t msg[MAX_STR_SIZE];
        snprintf(msg, sizeof(msg), "Closed %s device", device);
        line_info_add(self, NULL, NULL, NULL, msg, SYS_MSG, 0, 0);
    }
}

ToxAv *init_audio(ToxWindow *self, Tox *tox)
{
    ASettins.errors = NoError;
    ASettins.ttas = 0; /* Not running */

    /* Capture devices */
    const char *stringed_device_list = alcGetString(NULL, ALC_CAPTURE_DEVICE_SPECIFIER);
    ASettins.device[input].size = 0;

    if ( stringed_device_list ) {
        const char *default_device = alcGetString(NULL, ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER);

        for ( ; *stringed_device_list; ++ASettins.device[input].size ) {
            ASettins.device[input].devices[ASettins.device[input].size] = stringed_device_list;

            if ( strcmp( default_device , ASettins.device[input].devices[ASettins.device[input].size] ) == 0 )
                ASettins.device[input].index = ASettins.device[input].dix = ASettins.device[input].size;

            stringed_device_list += strlen( stringed_device_list ) + 1;
        }
    }


    /* Output devices */
    stringed_device_list = alcGetString(NULL, ALC_DEVICE_SPECIFIER);
    ASettins.device[output].size = 0;

    if ( stringed_device_list ) {
        const char *default_device = alcGetString(NULL, ALC_DEFAULT_DEVICE_SPECIFIER);

        for ( ; *stringed_device_list; ++ASettins.device[output].size ) {
            ASettins.device[output].devices[ASettins.device[output].size] = stringed_device_list;

            if ( strcmp( default_device , ASettins.device[output].devices[ASettins.device[output].size] ) == 0 )
                ASettins.device[output].index = ASettins.device[output].dix = ASettins.device[output].size;

            stringed_device_list += strlen( stringed_device_list ) + 1;
        }
    }

    if (!ASettins.device[input].size && !ASettins.device[output].size) {
        uint8_t *msg = "No devices: disabling audio!";
        line_info_add(self, NULL, NULL, NULL, msg, SYS_MSG, 0, 0);
        ASettins.av = NULL;
    } else {
        /* Streaming stuff from core */

        ToxAvCodecSettings cs = av_DefaultSettings;
        cs.video_height = cs.video_width = 0;

        ASettins.av = toxav_new(tox, &cs);

        if ( !ASettins.av ) {
            ASettins.errors |= ErrorStartingCoreAudio;
            return NULL;
        }

        toxav_register_callstate_callback(callback_call_started, av_OnStart, self);
        toxav_register_callstate_callback(callback_call_canceled, av_OnCancel, self);
        toxav_register_callstate_callback(callback_call_rejected, av_OnReject, self);
        toxav_register_callstate_callback(callback_call_ended, av_OnEnd, self);
        toxav_register_callstate_callback(callback_recv_invite, av_OnInvite, self);

        toxav_register_callstate_callback(callback_recv_ringing, av_OnRinging, self);
        toxav_register_callstate_callback(callback_recv_starting, av_OnStarting, self);
        toxav_register_callstate_callback(callback_recv_ending, av_OnEnding, self);

        toxav_register_callstate_callback(callback_recv_error, av_OnError, self);
        toxav_register_callstate_callback(callback_requ_timeout, av_OnRequestTimeout, self);
        toxav_register_callstate_callback(callback_peer_timeout, av_OnPeerTimeout, self);
    }

    return ASettins.av;
}

void terminate_audio()
{
    stop_transmission();

    if ( ASettins.av )
        toxav_kill(ASettins.av);
}

int errors()
{
    return ASettins.errors;
}



/*
 * Transmission
 */

void *transmission(void *arg)
{
    (void)arg; /* Avoid warning */

    /* Missing audio support */
    if ( !ASettins.av ) _cbend;

    ASettins.ttas = 1;

    /* Prepare devices */
    alcCaptureStart(ASettins.device[input].dhndl);
    alcMakeContextCurrent(ASettins.device[output].ctx);

    int32_t dec_frame_len;
    int16_t frame[4096];
    int32_t sample = 0;
    uint32_t buffer;
    int32_t ready;
    int32_t openal_buffers = 5;
    uint32_t source, *buffers;

    /* Prepare buffers */
    buffers = calloc(sizeof(uint32_t), openal_buffers);
    alGenBuffers(openal_buffers, buffers);
    alGenSources((uint32_t)1, &source);
    alSourcei(source, AL_LOOPING, AL_FALSE);


    uint16_t zeros[AUDIO_FRAME_SIZE];
    memset(zeros, 0, AUDIO_FRAME_SIZE);
    int16_t PCM[AUDIO_FRAME_SIZE];

    int32_t i = 0;

    for (; i < openal_buffers; ++i) {
        alBufferData(buffers[i], AL_FORMAT_MONO16, zeros, AUDIO_FRAME_SIZE, 48000);
    }

    alSourceQueueBuffers(source, openal_buffers, buffers);
    alSourcePlay(source);

    if (alGetError() != AL_NO_ERROR) {
        /* Print something? */
        /*fprintf(stderr, "Error starting audio\n");*/
        goto cleanup;
    }

    /* Start transmission */
    while (ASettins.ttas) {

        alcGetIntegerv(ASettins.device[input].dhndl, ALC_CAPTURE_SAMPLES, (int32_t) sizeof(int32_t), &sample);

        /* RECORD AND SEND */
        if (sample >= AUDIO_FRAME_SIZE) {
            alcCaptureSamples(ASettins.device[input].dhndl, frame, AUDIO_FRAME_SIZE);

            if (toxav_send_audio(ASettins.av, frame, AUDIO_FRAME_SIZE) < 0)
                /*fprintf(stderr, "Could not encode or send audio packet\n")*/;

        } else usleep(1000);



        /* PLAYBACK */

        alGetSourcei(source, AL_BUFFERS_PROCESSED, &ready);

        if (ready <= 0)
            continue;

        dec_frame_len = toxav_recv_audio(ASettins.av, AUDIO_FRAME_SIZE, PCM);

        /* Play the packet */
        if (dec_frame_len > 0) {
            alSourceUnqueueBuffers(source, 1, &buffer);
            alBufferData(buffer, AL_FORMAT_MONO16, PCM, dec_frame_len * 2 * 1, 48000);
            int32_t error = alGetError();

            if (error != AL_NO_ERROR) {
                /*fprintf(stderr, "Error setting buffer %d\n", error);*/
                break;
            }

            alSourceQueueBuffers(source, 1, &buffer);

            if (alGetError() != AL_NO_ERROR) {
                /*fprintf(stderr, "Error: could not buffer audio\n");*/
                break;
            }

            alGetSourcei(source, AL_SOURCE_STATE, &ready);

            if (ready != AL_PLAYING) alSourcePlay(source);
        }

        usleep(1000);
    }

cleanup:

    alDeleteSources(1, &source);
    alDeleteBuffers(openal_buffers, buffers);

    device_close(NULL, input);
    device_close(NULL, output);

    _cbend;
}

int start_transmission(ToxWindow *self)
{
    if ( !ASettins.av ) return -1;

    if ( !toxav_capability_supported(ASettins.av, AudioDecoding) ||
            !toxav_capability_supported(ASettins.av, AudioEncoding) )
        return -1;

    /* Now open our devices */
    if ( -1 == device_open(self, input) )
        return -1;

    if ( -1 == device_open(self, output))
        return -1;

    /* Don't provide support for video */
    toxav_prepare_transmission(ASettins.av, 0);

    if ( 0 != pthread_create(&ASettins.ttid, NULL, transmission, NULL ) &&
            0 != pthread_detach(ASettins.ttid) ) {
        return -1;
    }
}

int stop_transmission()
{
    ASettins.ttas = 0;
}
/*
 * End of transmission
 */





/*
 * Callbacks
 */

#define CB_BODY(Arg, onFunc) do { ToxWindow* windows = (Arg); int i;\
for (i = 0; i < MAX_WINDOWS_NUM; ++i) if (windows[i].onFunc != NULL) windows[i].onFunc(&windows[i], ASettins.av); } while (0)

void callback_recv_invite ( void *arg )
{
    CB_BODY(arg, onInvite);
}
void callback_recv_ringing ( void *arg )
{
    CB_BODY(arg, onRinging);
}
void callback_recv_starting ( void *arg )
{
    CB_BODY(arg, onStarting);
}
void callback_recv_ending ( void *arg )
{
    CB_BODY(arg, onEnding);
    stop_transmission();
}
void callback_recv_error ( void *arg )
{
    CB_BODY(arg, onError);
}
void callback_call_started ( void *arg )
{
    CB_BODY(arg, onStart);
}
void callback_call_canceled ( void *arg )
{
    CB_BODY(arg, onCancel);

    /* In case call is active */
    stop_transmission();
}
void callback_call_rejected ( void *arg )
{
    CB_BODY(arg, onReject);
}
void callback_call_ended ( void *arg )
{
    CB_BODY(arg, onEnd);
    stop_transmission();
}

void callback_requ_timeout ( void *arg )
{
    CB_BODY(arg, onRequestTimeout);
}
void callback_peer_timeout ( void *arg )
{
    CB_BODY(arg, onPeerTimeout);
    stop_transmission();
    /* Call is stopped manually since there might be some other
     * actions that one can possibly take on timeout
     */
    toxav_stop_call(ASettins.av);
}
/*
 * End of Callbacks
 */


/*
 * Commands from chat_commands.h
 */
void cmd_call(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    uint8_t msg[MAX_STR_SIZE];
    uint8_t *error_str;

    if (argc != 0) {
        error_str = "Invalid syntax!";
        goto on_error;
    }

    if ( !ASettins.av ) {
        error_str = "Audio not supported!";
        goto on_error;
    }

    ToxAvError error = toxav_call(ASettins.av, self->num, TypeAudio, 30);

    if ( error != ErrorNone ) {
        if ( error == ErrorAlreadyInCall ) error_str = "Already in a call!";
        else error_str = "Internal error!";

        goto on_error;
    }

    error_str = "Calling...";
    line_info_add(self, NULL, NULL, NULL, error_str, SYS_MSG, 0, 0);

    return;
on_error:
    snprintf(msg, sizeof(msg), "%s", error_str);
    line_info_add(self, NULL, NULL, NULL, msg, SYS_MSG, 0, 0);
}

void cmd_answer(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    uint8_t *error_str;

    if (argc != 0) {
        error_str = "Invalid syntax!";
        goto on_error;
    }

    if ( !ASettins.av ) {
        error_str = "Audio not supported!";
        goto on_error;
    }

    ToxAvError error = toxav_answer(ASettins.av, TypeAudio);

    if ( error != ErrorNone ) {
        if ( error == ErrorInvalidState ) error_str = "Cannot answer in invalid state!";
        else if ( error == ErrorNoCall ) error_str = "No incoming call!";
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
    uint8_t *error_str;

    if (argc != 0) {
        error_str = "Invalid syntax!";
        goto on_error;
    }

    if ( !ASettins.av ) {
        error_str = "Audio not supported!";
        goto on_error;
    }

    ToxAvError error = toxav_reject(ASettins.av, "Why not?");

    if ( error != ErrorNone ) {
        if ( error == ErrorInvalidState ) error_str = "Cannot reject in invalid state!";
        else if ( error == ErrorNoCall ) error_str = "No incoming call!";
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
    uint8_t *error_str;

    if (argc != 0) {
        error_str = "Invalid syntax!";
        goto on_error;
    }

    if ( !ASettins.av ) {
        error_str = "Audio not supported!";
        goto on_error;
    }

    ToxAvError error = toxav_hangup(ASettins.av);

    if ( error != ErrorNone ) {
        if ( error == ErrorInvalidState ) error_str = "Cannot hangup in invalid state!";
        else if ( error == ErrorNoCall ) error_str = "No call!";
        else error_str = "Internal error!";

        goto on_error;
    }

    return;
on_error:
    print_err (self, error_str);
}

void cmd_cancel(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    uint8_t *error_str;

    if (argc != 0) {
        error_str = "Invalid syntax!";
        goto on_error;
    }

    if ( !ASettins.av ) {
        error_str = "Audio not supported!";
        goto on_error;
    }

    ToxAvError error = toxav_cancel(ASettins.av, self->num,
                                    "Only those who appreciate small things know the beauty of life");

    if ( error != ErrorNone ) {
        if ( error == ErrorNoCall ) error_str = "No call!";
        else error_str = "Internal error!";

        goto on_error;
    }

    /* Callback will print status... */

    return;
on_error:
    print_err (self, error_str);
}


void cmd_list_devices(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    uint8_t msg[MAX_STR_SIZE];
    uint8_t *error_str;

    if ( argc != 1 ) {
        if ( argc < 1 ) error_str = "Type must be specified!";
        else error_str = "Only one argument allowed!";

        goto on_error;
    }

    _Devices type;

    if ( strcmp(argv[1], "in") == 0 ) /* Input devices */
        type = input;

    else if ( strcmp(argv[1], "out") == 0 ) /* Output devices */
        type = output;

    else {
        snprintf(msg, sizeof(msg), "Invalid type: %s", argv[1]);
        line_info_add(self, NULL, NULL, NULL, msg, SYS_MSG, 0, 0);
        return;
    }

    int i = 0;

    for ( ; i < ASettins.device[type].size; i ++) {
        snprintf(msg, sizeof(msg), "%d: %s", i, ASettins.device[type].devices[i]);
        line_info_add(self, NULL, NULL, NULL, msg, SYS_MSG, 0, 0);
    }

    return;
on_error:
    print_err (self, error_str);
}

void cmd_change_device(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    uint8_t msg[MAX_STR_SIZE];
    uint8_t *error_str;

    if ( argc != 2 ) {
        if ( argc < 1 ) error_str = "Type must be specified!";
        else if ( argc < 2 ) error_str = "Must have id!";
        else error_str = "Only two arguments allowed!";

        goto on_error;
    }

    if ( ASettins.ttas ) { /* Transmission is active */
        error_str = "Cannot change device while active transmission";
        goto on_error;
    }

    _Devices type;

    if ( strcmp(argv[1], "in") == 0 ) /* Input devices */
        type = input;

    else if ( strcmp(argv[1], "out") == 0 ) /* Output devices */
        type = output;

    else {
        snprintf(msg, sizeof(msg), "Invalid type: %s", argv[1]);
        line_info_add(self, NULL, NULL, NULL, msg, SYS_MSG, 0, 0);
        return;
    }


    char *end;
    long int selection = strtol(argv[2], &end, 10);

    if ( *end ) {
        error_str = "Invalid input";
        goto on_error;
    }

    if ( device_set(self, type, selection ) == 0) {
        snprintf(msg, sizeof(msg), "Selected: %s", ASettins.device[type].devices[selection]);
        line_info_add(self, NULL, NULL, NULL, msg, SYS_MSG, 0, 0);
    }

    return;
on_error:
    print_err (self, error_str);
}

/*
 * Toxic -- Tox Curses Client
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef _SUPPORT_AUDIO


#include "audio_call.h"
#include "toxic_windows.h"
#include "chat_commands.h"
#include "toxic_windows.h"
#include <curses.h>
#include <AL/al.h>
#include <AL/alc.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>

#define MAX_DEVICES 32
#define _cbend pthread_exit(NULL)

typedef struct _DeviceIx {
    
    ALCdevice* dhndl; /* Handle of device selected/opened */
    ALCcontext* ctx; /* Device context */
    const char* devices[MAX_DEVICES]; /* Container  */
    int size;
    int dix; /* Index of default device */
} DeviceIx;

enum _devices
{
    input,
    output,
};

struct _ASettings {
    
    DeviceIx device[2];
    
    AudioError errors;
    
    ToxAv* av;
    
    pthread_t ttid; /* Transmission thread id */
    int ttas; /* Transmission thread active status (0 - stopped, 1- running) */
} ASettins;


void *callback_recv_invite ( void *arg );
void *callback_recv_ringing ( void *arg );
void *callback_recv_starting ( void *arg );
void *callback_recv_ending ( void *arg );
void *callback_recv_error ( void *arg );
void *callback_call_started ( void *arg );
void *callback_call_canceled ( void *arg );
void *callback_call_rejected ( void *arg );
void *callback_call_ended ( void *arg );
void *callback_requ_timeout ( void *arg );





ToxAv* init_audio(ToxWindow* window, Tox* tox)
{
    ASettins.errors = NoError;
    ASettins.ttas = 0; /* Not running */
    
    /* Capture device */
    const char* stringed_device_list = alcGetString(NULL, ALC_CAPTURE_DEVICE_SPECIFIER);
    ASettins.device[input].size = 0;
    
    if ( stringed_device_list ) {
        const char* default_device = alcGetString(NULL, ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER);
        
        for ( ; *stringed_device_list; ++ASettins.device[input].size ) {
            ASettins.device[input].devices[ASettins.device[input].size] = stringed_device_list;
            
            if ( strcmp( default_device , ASettins.device[input].devices[ASettins.device[input].size] ) == 0 )
                ASettins.device[input].dix = ASettins.device[input].size;
            
            stringed_device_list += strlen( stringed_device_list ) + 1;
        }
        
        ++ASettins.device[input].size;
    }
    
    if ( ASettins.device[input].size ) { /* Have device */
        ASettins.device[input].dhndl = alcCaptureOpenDevice(
            ASettins.device[input].devices[ASettins.device[input].dix], AUDIO_SAMPLE_RATE, AL_FORMAT_MONO16, AUDIO_FRAME_SIZE * 4);
        
        if (alcGetError(ASettins.device[input].dhndl) != AL_NO_ERROR) {
            ASettins.errors |= ErrorStartingCaptureDevice;
        }
        
        wprintw(window->window, "Input device: %s\n", ASettins.device[input].devices[ASettins.device[input].dix]);
    } else { /* No device */
        wprintw(window->window, "No input device!\n");
        ASettins.device[input].dhndl = NULL;
    }
    
    ASettins.device[input].ctx = NULL;
    
    
    
    
    /* Output device */
    stringed_device_list = alcGetString(NULL, ALC_DEVICE_SPECIFIER);
    ASettins.device[output].size = 0;
    
    if ( stringed_device_list ) {
        const char* default_device = alcGetString(NULL, ALC_DEFAULT_DEVICE_SPECIFIER);
        
        for ( ; *stringed_device_list; ++ASettins.device[output].size ) {
            ASettins.device[output].devices[ASettins.device[output].size] = stringed_device_list;            
            
            if ( strcmp( default_device , ASettins.device[output].devices[ASettins.device[output].size] ) == 0 )
                ASettins.device[output].dix = ASettins.device[output].size;
            
            stringed_device_list += strlen( stringed_device_list ) + 1;
        }
        
        ++ASettins.device[output].size;
    }
    
    if ( ASettins.device[output].size ) { /* Have device */
        ASettins.device[output].dhndl = alcOpenDevice(ASettins.device[output].devices[ASettins.device[output].dix]);
        
        if (alcGetError(ASettins.device[output].dhndl) != AL_NO_ERROR) {
            ASettins.errors |= ErrorStartingOutputDevice;
        }
        
        wprintw(window->window, "Output device: %s\n", ASettins.device[output].devices[ASettins.device[output].dix]);
        ASettins.device[output].ctx = alcCreateContext(ASettins.device[output].dhndl, NULL);
    } else { /* No device */
        wprintw(window->window, "No output device!\n");
        ASettins.device[output].dhndl = NULL;
        ASettins.device[output].ctx = NULL;
    }
    
    
    /* Streaming stuff from core */
    ASettins.av = toxav_new(tox, window, 0, 0);
    
    if ( !ASettins.av ) {
        ASettins.errors |= ErrorStartingCoreAudio;
        return NULL;
    }    
    
    toxav_register_callstate_callback(callback_call_started, av_OnStart);
    toxav_register_callstate_callback(callback_call_canceled, av_OnCancel);
    toxav_register_callstate_callback(callback_call_rejected, av_OnReject);
    toxav_register_callstate_callback(callback_call_ended, av_OnEnd);
    toxav_register_callstate_callback(callback_recv_invite, av_OnInvite);
    
    toxav_register_callstate_callback(callback_recv_ringing, av_OnRinging);
    toxav_register_callstate_callback(callback_recv_starting, av_OnStarting);
    toxav_register_callstate_callback(callback_recv_ending, av_OnEnding);
    
    toxav_register_callstate_callback(callback_recv_error, av_OnError);
    toxav_register_callstate_callback(callback_requ_timeout, av_OnRequestTimeout);
    
    return ASettins.av;
}

void terminate_audio()
{
    if ( ASettins.device[input].dhndl ) {
        alcCaptureCloseDevice(ASettins.device[input].dhndl);
    }
    
    if ( ASettins.device[output].dhndl ) {
        alcCloseDevice(ASettins.device[output].dhndl);
        alcMakeContextCurrent(NULL);
        alcDestroyContext(ASettins.device[output].ctx);
    }
    
    toxav_kill(ASettins.av);
}


int errors()
{
    return ASettins.errors;
}



/*
 * Transmission
 */

void* transmission(void* arg)
{
    (void)arg; /* Avoid warning */
    
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
    alcMakeContextCurrent(NULL);
        
    _cbend;
}

int start_transmission()
{
    if ( !toxav_audio_decoding(ASettins.av) ||
         !toxav_audio_encoding(ASettins.av) )
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

#define CB_BODY(Arg, onFunc) do { ToxWindow* windows = toxav_get_agent_handler(Arg); int i;\
for (i = 0; i < MAX_WINDOWS_NUM; ++i) if (windows[i].onFunc != NULL) windows[i].onFunc(&windows[i], Arg); } while (0)

void *callback_recv_invite ( void* arg )
{    
    CB_BODY(arg, onInvite);
    _cbend;
}
void *callback_recv_ringing ( void* arg )
{
    CB_BODY(arg, onRinging);
    _cbend;
}
void *callback_recv_starting ( void* arg )
{
    CB_BODY(arg, onStarting);
    _cbend;
}
void *callback_recv_ending ( void* arg )
{   
    CB_BODY(arg, onEnding);
    stop_transmission();
}
void *callback_recv_error ( void* arg )
{
    CB_BODY(arg, onError);
    _cbend;
}
void *callback_call_started ( void* arg )
{
    CB_BODY(arg, onStart);
    _cbend;
}
void *callback_call_canceled ( void* arg )
{
    CB_BODY(arg, onCancel);
    _cbend;
}
void *callback_call_rejected ( void* arg )
{
    CB_BODY(arg, onReject);
    _cbend;
}
void *callback_call_ended ( void* arg )
{  
    CB_BODY(arg, onEnd);
    stop_transmission();
    _cbend;
}

void *callback_requ_timeout ( void* arg )
{
    CB_BODY(arg, onTimeout);
    _cbend;
}

/* 
 * End of Callbacks 
 */


/*
 * Commands from chat_commands.h
 */
void cmd_call(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    const char* error_str;
    
    if (argc != 0) { error_str = "Invalid syntax!"; goto on_error; }
    
    if ( !ASettins.av ) { error_str = "No audio supported!"; goto on_error; }
        
    ToxAvError error = toxav_call(ASettins.av, self->num, TypeAudio, 30);
        
    if ( error != ErrorNone ) {
        if ( error == ErrorAlreadyInCall ) error_str = "Already in a call!";
        else error_str = "Internal error!";
        
        goto on_error;
    }
    
    wprintw(window, "Calling...\n");
    
    return;
on_error: 
    wprintw(window, "%s %d\n", error_str, argc);
}

void cmd_answer(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    const char* error_str;
    
    if (argc != 0) { error_str = "Invalid syntax!"; goto on_error; }
    
    if ( !ASettins.av ) { error_str = "No audio supported!"; goto on_error; }
    
    ToxAvError error = toxav_answer(ASettins.av, TypeAudio);
    
    if ( error != ErrorNone ) {
        if ( error == ErrorInvalidState ) error_str = "Cannot answer in invalid state!";
        else if ( error == ErrorNoCall ) error_str = "No incomming call!";
        else error_str = "Internal error!";
        
        goto on_error;
    }
    
    /* Callback will print status... */
    
    return;
on_error: 
    wprintw(window, "%s\n", error_str);
}

void cmd_hangup(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    const char* error_str;
    
    if (argc != 0) { error_str = "Invalid syntax!"; goto on_error; }
    
    if ( !ASettins.av ) { error_str = "No audio supported!"; goto on_error; }
    
    ToxAvError error = toxav_hangup(ASettins.av);
    
    if ( error != ErrorNone ) {
        if ( error == ErrorInvalidState ) error_str = "Cannot hangup in invalid state!";
        else if ( error == ErrorNoCall ) error_str = "No call!";
        else error_str = "Internal error!";
        
        goto on_error;
    }
        
    return;
on_error: 
    wprintw(window, "%s\n", error_str);
}

void cmd_cancel(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    const char* error_str;
    
    if (argc != 0) { error_str = "Invalid syntax!"; goto on_error; }
    
    if ( !ASettins.av ) { error_str = "No audio supported!"; goto on_error; }
    
    ToxAvError error = toxav_hangup(ASettins.av);
    
    if ( error != ErrorNone ) {
        if ( error == ErrorNoCall ) error_str = "No call!";
        else error_str = "Internal error!";
        
        goto on_error;
    }
    
    /* Callback will print status... */
    
    return;
on_error: 
    wprintw(window, "%s\n", error_str);
}





#endif /* _SUPPORT_AUDIO */
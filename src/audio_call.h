/*
 * Toxic -- Tox Curses Client
 */

#ifndef _audio_h
#define _audio_h

#include <tox/toxav.h>

#define MAX_DEVICES 32

typedef struct ToxWindow ToxWindow;

typedef enum _AudioError
{
    NoError = 0,
    ErrorStartingCaptureDevice = 1 << 0,
    ErrorStartingOutputDevice = 1 << 1,
    ErrorStartingCoreAudio = 1 << 2
} AudioError;

typedef enum _Devices
{
    input,
    output,
} _Devices;

/* You will have to pass pointer to first member of 'windows' 
 * declared in windows.c otherwise undefined behaviour will 
 */
ToxAv* init_audio(ToxWindow* self, Tox* tox);
void terminate_audio();

int errors();

int start_transmission(ToxWindow *self);
int device_set(ToxWindow *self, _Devices type, long int selection);

#endif /* _audio_h */
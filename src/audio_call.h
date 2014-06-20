/*
 * Toxic -- Tox Curses Client
 */

#ifndef _audio_h
#define _audio_h

#include <tox/toxav.h>

#include "device.h"

typedef enum _AudioError {
    ae_None = 0,
    ae_StartingCaptureDevice = 1 << 0,
    ae_StartingOutputDevice = 1 << 1,
    ae_StartingCoreAudio = 1 << 2
} AudioError;


/* You will have to pass pointer to first member of 'windows'
 * declared in windows.c otherwise undefined behaviour will
 */
ToxAv *init_audio(ToxWindow *self, Tox *tox);
void terminate_audio();

int clear_call_settings_per_se(ToxWindow *self);

int start_transmission(ToxWindow *self);
int stop_transmission(int call_index);
int device_set(ToxWindow* self, DeviceType type, long int selection);


#endif /* _audio_h */

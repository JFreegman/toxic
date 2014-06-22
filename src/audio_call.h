/*  audio_call.h
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

int start_transmission(ToxWindow *self);
int stop_transmission(int call_index);
int device_set(ToxWindow* self, DeviceType type, long int selection);


#endif /* _audio_h */

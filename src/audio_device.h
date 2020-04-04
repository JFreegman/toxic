/*  audio_device.h
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

/*
 * You can have multiple sources (Input devices) but only one output device.
 * Pass buffers to output device via write();
 * Read from running input device(s) via select()/callback combo.
 */

#ifndef AUDIO_DEVICE_H
#define AUDIO_DEVICE_H

#define OPENAL_BUFS 5
#define MAX_OPENAL_DEVICES 32
#define MAX_DEVICES 32

#include "windows.h"

typedef enum DeviceType {
    input,
    output,
} DeviceType;

typedef enum DeviceError {
    de_None,
    de_InternalError = -1,
    de_InvalidSelection = -2,
    de_FailedStart = -3,
    de_Busy = -4,
    de_AllDevicesBusy = -5,
    de_DeviceNotActive = -6,
    de_BufferError = -7,
    de_UnsupportedMode = -8,
    de_AlError = -9,
} DeviceError;

typedef void (*DataHandleCallback)(const int16_t *, uint32_t size, void *data);


DeviceError init_devices(void);

void get_al_device_names(void);
DeviceError terminate_devices(void);

/* toggle device mute */
DeviceError device_mute(DeviceType type, uint32_t device_idx);

#ifdef AUDIO
DeviceError device_set_VAD_treshold(uint32_t device_idx, float value);
#endif

DeviceError set_al_device(DeviceType type, int32_t selection);

/* Start device */
DeviceError open_input_device(uint32_t *device_idx,
                              DataHandleCallback cb, void *cb_data, bool enable_VAD,
                              uint32_t sample_rate, uint32_t frame_duration, uint8_t channels);
DeviceError open_output_device(uint32_t *device_idx,
                               uint32_t sample_rate, uint32_t frame_duration, uint8_t channels);

/* Stop device */
DeviceError close_device(DeviceType type, uint32_t device_idx);

/* Write data to output device */
DeviceError write_out(uint32_t device_idx, const int16_t *data, uint32_t length, uint8_t channels,
                      uint32_t sample_rate);

void print_al_devices(ToxWindow *self, DeviceType type);

DeviceError selection_valid(DeviceType type, int32_t selection);
#endif /* AUDIO_DEVICE_H */

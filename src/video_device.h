/*  video_device.h
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

#ifndef VIDEO_DEVICE_H
#define VIDEO_DEVICE_H

#define MAX_DEVICES 32
#include <inttypes.h>
#include "windows.h"

typedef enum VideoDeviceType {
    input,
    output,
} VideoDeviceType;

typedef enum VideoDeviceError {
    vde_None,
    vde_InternalError = -1,
    vde_InvalidSelection = -2,
    vde_FailedStart = -3,
    vde_Busy = -4,
    vde_AllDevicesBusy = -5,
    vde_DeviceNotActive = -6,
    vde_BufferError = -7,
    vde_UnsupportedMode = -8,
    vde_CaptureError = -9,
} VideoDeviceError;

typedef void (*DataHandleCallback) (int16_t width, int16_t height, const uint8_t* y, const uint8_t* u, const uint8_t* v, void* data);


#ifdef VIDEO
VideoDeviceError init_video_devices(ToxAV* av);
#else
VideoDeviceError init_video_devices();
#endif /* VIDEO */

VideoDeviceError terminate_video_devices();

/* Callback handles ready data from INPUT device */
VideoDeviceError register_video_device_callback(int32_t call_idx, uint32_t device_idx, DataHandleCallback callback, void* data);
void* get_video_device_callback_data(uint32_t device_idx);

VideoDeviceError set_primary_video_device(VideoDeviceType type, int32_t selection);
VideoDeviceError open_primary_video_device(VideoDeviceType type, uint32_t* device_idx);
/* Start device */
VideoDeviceError open_video_device(VideoDeviceType type, int32_t selection, uint32_t* device_idx);
/* Stop device */
VideoDeviceError close_video_device(VideoDeviceType type, uint32_t device_idx);

/* Write data to device */
VideoDeviceError write_out(uint32_t device_idx, const int16_t* data, uint32_t length, uint8_t channels);

void print_video_devices(ToxWindow* self, VideoDeviceType type);
void get_primary_video_device_name(VideoDeviceType type, char *buf, int size);

VideoDeviceError selection_valid(VideoDeviceType type, int32_t selection);
#endif /* VIDEO_DEVICE_H */
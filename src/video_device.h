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

/*
 * You can have multiple sources (Input devices) but only one output device.
 * Pass buffers to output device via write(); 
 * Read from running input device(s) via select()/callback combo.
 */

#ifndef VIDEO_DEVICE_H
#define VIDEO_DEVICE_H

#define MAX_DEVICES 32
#include <inttypes.h>
#include "windows.h"

#include "audio_device.h"

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
    vde_OpenCVError = -9,
} VideoDeviceError;

#ifdef VIDEO
VideoDeviceError init_video_devices(ToxAV* av);
#else
VideoDeviceError init_video_devices();
#endif /* VIDEO */

VideoDeviceError terminate_video_devices();

VideoDeviceError set_primary_video_device(DeviceType type, int32_t selection);
VideoDeviceError open_primary_video_device(DeviceType type);
/* Start device */
VideoDeviceError open_video_device(DeviceType type, int32_t selection, uint32_t* device_idx);
/* Stop device */
VideoDeviceError close_video_device(DeviceType type, uint32_t device_idx);

#endif /* VIDEO_DEVICE_H */
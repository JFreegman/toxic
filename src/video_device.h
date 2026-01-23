/*  video_device.h
 *
 *  Copyright (C) 2014-2026 Toxic All Rights Reserved.
 *
 *  This file is part of Toxic. Toxic is free software licensed
 *  under the GNU General Public License 3.0.
 */

#ifndef VIDEO_DEVICE_H
#define VIDEO_DEVICE_H

#define MAX_DEVICES 32

#include <inttypes.h>

#include "settings.h"
#include "windows.h"

typedef enum VideoDeviceType {
    vdt_input,
    vdt_output,
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

typedef void (*VideoDataHandleCallback)(Toxic *toxic, uint32_t friend_number, int16_t width, int16_t height,
                                        const uint8_t *y, const uint8_t *u, const uint8_t *v, void *data);

VideoDeviceError init_video_devices(Toxic *toxic);

VideoDeviceError terminate_video_devices(void);

/* Callback handles ready data from INPUT device */
VideoDeviceError register_video_device_callback(int32_t call_idx, uint32_t device_idx, VideoDataHandleCallback callback,
        void *data);
void *get_video_device_callback_data(uint32_t device_idx);

VideoDeviceError set_primary_video_device(VideoDeviceType type, int32_t selection);
VideoDeviceError open_primary_video_device(VideoDeviceType type, uint32_t *device_idx,
        uint32_t *width, uint32_t *height);
/* Start device */
VideoDeviceError open_video_device(VideoDeviceType type, int32_t selection, uint32_t *device_idx,
                                   uint32_t *width, uint32_t *height);
/* Stop device */
VideoDeviceError close_video_device(VideoDeviceType type, uint32_t device_idx);

/* Write data to device */
VideoDeviceError write_video_out(uint16_t width, uint16_t height, uint8_t const *y, uint8_t const *u, uint8_t const *v,
                                 int32_t ystride, int32_t ustride, int32_t vstride, void *user_data);

void print_video_devices(ToxWindow *self, const Client_Config *c_config, VideoDeviceType type);
void get_primary_video_device_name(VideoDeviceType type, char *buf, int size);

VideoDeviceError video_selection_valid(VideoDeviceType type, int32_t selection);
#endif /* VIDEO_DEVICE_H */

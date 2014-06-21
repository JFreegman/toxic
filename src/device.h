/*
 * Toxic -- Tox Curses Client
 */

/*
 * You can have multiple sources (Input devices) but only one output device.
 * Pass buffers to output device via write(); 
 * Read from running input device(s) via select()/callback combo.
 */


#ifndef _device_h
#define _device_h

#define MAX_DEVICES 32
#include <inttypes.h>
#include "toxic_windows.h"

#define _True 1
#define _False 0

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
    de_AlError = -8,
} DeviceError;

typedef void (*DataHandleCallback) (const int16_t*, uint32_t size, void* data);


DeviceError init_devices(ToxAv* av);
DeviceError terminate_devices();

/* Callback handles ready data from INPUT device */
DeviceError register_device_callback(uint32_t device_idx, DataHandleCallback callback, void* data, _Bool enable_VAD);
void* get_device_callback_data(uint32_t device_idx);

DeviceError set_primary_device(DeviceType type, int32_t selection);
DeviceError open_primary_device(DeviceType type, uint32_t* device_idx);
/* Start device */
DeviceError open_device(DeviceType type, int32_t selection, uint32_t* device_idx);
/* Stop device */
DeviceError close_device(DeviceType type, uint32_t device_idx);

DeviceError playback_device_ready(uint32_t device_idx);
/* Write data to device */
DeviceError write_out(uint32_t device_idx, int16_t* data, uint32_t lenght, uint8_t channels);

void print_devices(ToxWindow* self, DeviceType type);

DeviceError selection_valid(DeviceType type, int32_t selection);
#endif /* _device_h */
/*  osx_video.m
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

#ifdef __OBJC__
#include "osx_video.h"

#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>

#include "line_info.h"
#include "settings.h"

#include <errno.h>

#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>

/*
 * Helper video format functions
 */
static uint8_t rgb_to_y(int r, int g, int b)
{
    int y = ((9798 * r + 19235 * g + 3736 * b) >> 15);
    return y>255? 255 : y<0 ? 0 : y;
}

static uint8_t rgb_to_u(int r, int g, int b)
{
    int u = ((-5538 * r + -10846 * g + 16351 * b) >> 15) + 128;
    return u>255? 255 : u<0 ? 0 : u;
}

static uint8_t rgb_to_v(int r, int g, int b)
{
    int v = ((16351 * r + -13697 * g + -2664 * b) >> 15) + 128;
    return v>255? 255 : v<0 ? 0 : v;
}

void bgrtoyuv420(uint8_t *plane_y, uint8_t *plane_u, uint8_t *plane_v, uint8_t *rgb, uint16_t width, uint16_t height)
{
    uint16_t x, y;
    uint8_t *p;
    uint8_t r, g, b;

    for(y = 0; y != height; y += 2) {
        p = rgb;
        for(x = 0; x != width; x++) {
            b = *rgb++;
            g = *rgb++;
            r = *rgb++;
            *plane_y++ = rgb_to_y(r, g, b);
        }

        for(x = 0; x != width / 2; x++) {
            b = *rgb++;
            g = *rgb++;
            r = *rgb++;
            *plane_y++ = rgb_to_y(r, g, b);

            b = *rgb++;
            g = *rgb++;
            r = *rgb++;
            *plane_y++ = rgb_to_y(r, g, b);

            b = ((int)b + (int)*(rgb - 6) + (int)*p + (int)*(p + 3) + 2) / 4; p++;
            g = ((int)g + (int)*(rgb - 5) + (int)*p + (int)*(p + 3) + 2) / 4; p++;
            r = ((int)r + (int)*(rgb - 4) + (int)*p + (int)*(p + 3) + 2) / 4; p++;

            *plane_u++ = rgb_to_u(r, g, b);
            *plane_v++ = rgb_to_v(r, g, b);

            p += 3;
        }
    }
}
/*
 * End of helper video format functions
 */



/*
 * Implementation for OSXVideo
 */
@implementation OSXVideo {
    dispatch_queue_t _processingQueue;
    AVCaptureSession *_session;
    AVCaptureVideoDataOutput *_linkerVideo;

    CVImageBufferRef _currentFrame;
    pthread_mutex_t _frameLock;
}

- (instancetype)initWithDeviceNames: (char **)device_names AmtDevices: (int *)size {
    _session = [[AVCaptureSession alloc] init];

    NSArray *devices = [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo];
    int i = 0;
    for (AVCaptureDevice *device in devices) {
        char *video_input_name;
        NSString *localizedName = [device localizedName];
        video_input_name = (char*)malloc(strlen([localizedName cStringUsingEncoding:NSUTF8StringEncoding]) + 1);
        strcpy(video_input_name, (char*)[localizedName cStringUsingEncoding:NSUTF8StringEncoding]);
        device_names[i] = video_input_name;
        ++i;
    }
    if ( i <= 0 )
        return nil;
    *size = i;

    return self;
}

- (void)release {
    [_session release];
    [super release];
}

- (int)openVideoDeviceIndex: (uint32_t)device_idx Width: (uint16_t *)width Height: (uint16_t *)height {
    pthread_mutex_init(&_frameLock, NULL);
    _processingQueue = dispatch_queue_create("Toxic processing queue", DISPATCH_QUEUE_SERIAL);
    NSArray *devices = [AVCaptureDevice devicesWithMediaType:AVMediaTypeVideo];
    int i = 0;
    for (AVCaptureDevice *device in devices) {
        if ( i == device_idx ) {
            NSError *error = NULL;
            AVCaptureInput *input = [[AVCaptureDeviceInput alloc] initWithDevice:device error:&error];

            if ( error != NULL )
                return -1;

            /* Obtain device resolution */
            /*AVCaptureInputPort *port = [input.ports objectAtIndex:0];
            CMFormatDescriptionRef format_description = port.formatDescription;
            if ( format_description ) {
                CMVideoDimensions dimensions = CMVideoFormatDescriptionGetDimensions(format_description);
                *width = dimensions.width;
                *height = dimensions.height;
            } else {
                *width = 0;
                *height = 0;
            }*/

            [_session beginConfiguration];
            [_session addInput:input];
            //session.sessionPreset = AVCaptureSessionPreset640x480;
            [_session commitConfiguration];
            [input release];

            break;
        } else {
            ++i;
        }
    }

    _linkerVideo = [[AVCaptureVideoDataOutput alloc] init];
    [_linkerVideo setSampleBufferDelegate:self queue:_processingQueue];
    // TODO possibly get a better pixel format
    [_linkerVideo setVideoSettings:@{(id)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA)}];
    [_session addOutput:_linkerVideo];

    [_session startRunning];

    return 0;
}

- (void)closeVideoDeviceIndex: (uint32_t)device_idx {
    [_session stopRunning];
    [_linkerVideo release];
}

- (void)getVideoFrameY: (uint8_t *)y U: (uint8_t *)u V: (uint8_t *)v Width: (uint16_t *)width Height: (uint16_t *)height {
    /*CVImageBufferRef currentFrame = NULL;

    CFRetain(currentFrame);

    CFTypeID imageType = CFGetTypeID(currentFrame);
    if (imageType == CVPixelBufferGetTypeID()) {
        // TODO maybe handle other formats
        bgrxtoyuv420(y, u, v, CVPixelBufferGetBaseAddress(currentFrame), width, height);
    } else if (imageType == CVOpenGLBufferGetTypeID()) {
        // OpenGL pbuffer
    } else if (imageType == CVOpenGLTextureGetTypeID()) {
        // OpenGL Texture (Do we need to handle these?)
    }

    CVPixelBufferRelease(currentFrame);*/
}

@end
/*
 * End of implementation for OSXVideo
 */


/*
 * C-interface for OSXVideo
 */
static OSXVideo* _OSXVideo = nil;

int osx_video_init(char **device_names, int *size)
{
    _OSXVideo = [[OSXVideo alloc] initWithDeviceNames: device_names AmtDevices: size];

    if ( _OSXVideo == nil )
        return -1;

    return 0;
}

void osx_video_release()
{
    [_OSXVideo release];
}

int osx_video_open_device(uint32_t device_idx, uint16_t *width, uint16_t *height)
{
    return [_OSXVideo openVideoDeviceIndex: device_idx Width: width Height: height];
}

void osx_video_close_device(uint32_t device_idx)
{
    [_OSXVideo closeVideoDeviceIndex: device_idx];
}

void osx_video_read_device(uint8_t *y, uint8_t *u, uint8_t *v, uint16_t *width, uint16_t *height)
{
    [_OSXVideo getVideoFrameY: y U: u V: v Width: width Height: height];
}
/*
 * End of C-interface for OSXVideo
 */

#endif /* __OBJC__ */
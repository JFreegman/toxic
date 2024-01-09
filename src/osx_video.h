/*  osx_video.h
 *
 *
 *  Copyright (C) 2024 Toxic All Rights Reserved.
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

#ifndef OSX_VIDEO_H
#define OSX_VIDEO_H

#include <netinet/in.h>

#ifdef __OBJC__
#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>
#endif /* __OBJC__ */

#define RELEASE_CHK(func, obj) if ((obj))\
    func((obj));

void bgrtoyuv420(uint8_t *plane_y, uint8_t *plane_u, uint8_t *plane_v, uint8_t *rgb, uint16_t width, uint16_t height);

#ifdef __OBJC__
@interface OSXVideo :
    NSObject <AVCaptureVideoDataOutputSampleBufferDelegate>
- (instancetype)initWithDeviceNames:
    (char **)device_names AmtDevices:
    (int *)size;
@end
#endif /* __OBJC__ */

int osx_video_init(char **device_names, int *size);
void osx_video_release(void);
/* Start device */
int osx_video_open_device(uint32_t selection, uint16_t *width, uint16_t *height);
/* Stop device */
void osx_video_close_device(uint32_t device_idx);
/* Read data from device */
int osx_video_read_device(uint8_t *y, uint8_t *u, uint8_t *v, uint16_t *width, uint16_t *height);


#endif /* OSX_VIDEO_H */

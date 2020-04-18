/*  video_call.c
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

#include "chat_commands.h"
#include "global_commands.h"
#include "line_info.h"
#include "misc_tools.h"
#include "notify.h"
#include "toxic.h"
#include "video_call.h"
#include "video_device.h"
#include "windows.h"

#include <assert.h>
#include <curses.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef VIDEO

#define DEFAULT_VIDEO_BIT_RATE 5000

void on_video_receive_frame(ToxAV *av, uint32_t friend_number,
                            uint16_t width, uint16_t height,
                            uint8_t const *y, uint8_t const *u, uint8_t const *v,
                            int32_t ystride, int32_t ustride, int32_t vstride,
                            void *user_data);

void on_video_bit_rate(ToxAV *av, uint32_t friend_number, uint32_t video_bit_rate, void *user_data);

static void print_err(ToxWindow *self, const char *error_str)
{
    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "%s", error_str);
}

ToxAV *init_video(ToxWindow *self, Tox *tox)
{
    UNUSED_VAR(tox);

    CallControl.video_errors = ve_None;

    CallControl.video_enabled = true;
    CallControl.default_video_bit_rate = 0;
    CallControl.video_frame_duration = 10;

    if (!CallControl.av) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Video failed to init with ToxAV instance");

        return NULL;
    }

    if (init_video_devices(CallControl.av) == vde_InternalError) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to init video devices");

        return NULL;
    }

    toxav_callback_video_receive_frame(CallControl.av, on_video_receive_frame, &CallControl);
    toxav_callback_video_bit_rate(CallControl.av, on_video_bit_rate, &CallControl);

    return CallControl.av;
}

void terminate_video(void)
{
    int i;

    for (i = 0; i < CallControl.max_calls; ++i) {
        Call *this_call = &CallControl.calls[i];

        stop_video_transmission(this_call, i);

        if (this_call->status == cs_Active && this_call->vout_idx != -1) {
            close_video_device(vdt_output, this_call->vout_idx);
            this_call->vout_idx = -1;
        }
    }

    terminate_video_devices();
}

void read_video_device_callback(int16_t width, int16_t height, const uint8_t *y, const uint8_t *u, const uint8_t *v,
                                void *data)
{
    uint32_t friend_number = *((uint32_t *)data); /* TODO: Or pass an array of call_idx's */
    Call *this_call = &CallControl.calls[friend_number];
    Toxav_Err_Send_Frame error;

    /* Drop frame if video sending is disabled */
    if (this_call->video_bit_rate == 0 || this_call->status != cs_Active || this_call->vin_idx == -1) {
        line_info_add(CallControl.prompt, NULL, NULL, NULL, SYS_MSG, 0, 0, "Video frame dropped.");
        return;
    }

    if (toxav_video_send_frame(CallControl.av, friend_number, width, height, y, u, v, &error) == false) {
        line_info_add(CallControl.prompt, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to send video frame");

        if (error == TOXAV_ERR_SEND_FRAME_NULL) {
            line_info_add(CallControl.prompt, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to capture video frame");
        } else if (error == TOXAV_ERR_SEND_FRAME_INVALID) {
            line_info_add(CallControl.prompt, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to prepare video frame");
        }
    }
}

void write_video_device_callback(uint32_t friend_number, uint16_t width, uint16_t height,
                                 uint8_t const *y, uint8_t const *u, uint8_t const *v,
                                 int32_t ystride, int32_t ustride, int32_t vstride,
                                 void *user_data)
{
    UNUSED_VAR(friend_number);

    write_video_out(width, height, y, u, v, ystride, ustride, vstride, user_data);
}

int start_video_transmission(ToxWindow *self, ToxAV *av, Call *call)
{
    if (!self || !av) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to prepare video transmission");
        return -1;
    }

    if (open_primary_video_device(vdt_input, &call->vin_idx, &call->video_width, &call->video_height) != vde_None) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to open input video device!");
        return -1;
    }

    if (register_video_device_callback(self->num, call->vin_idx, read_video_device_callback, &self->num) != vde_None) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to register input video handler!");
        return -1;
    }

    if (!toxav_video_set_bit_rate(CallControl.av, self->num, call->video_bit_rate, NULL)) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to set video bit rate");
        return -1;
    }

    return 0;
}

int stop_video_transmission(Call *call, int friend_number)
{
    if (call->status != cs_Active) {
        return -1;
    }

    call->video_bit_rate = 0;
    toxav_video_set_bit_rate(CallControl.av, friend_number, call->video_bit_rate, NULL);

    if (call->vin_idx != -1) {
        close_video_device(vdt_input, call->vin_idx);
        call->vin_idx = -1;
    }

    return 0;
}
/*
 * End of transmission
 */





/*
 * Callbacks
 */
void on_video_receive_frame(ToxAV *av, uint32_t friend_number,
                            uint16_t width, uint16_t height,
                            uint8_t const *y, uint8_t const *u, uint8_t const *v,
                            int32_t ystride, int32_t ustride, int32_t vstride,
                            void *user_data)
{
    UNUSED_VAR(av);

    write_video_device_callback(friend_number, width, height, y, u, v, ystride, ustride, vstride, user_data);
}

void on_video_bit_rate(ToxAV *av, uint32_t friend_number, uint32_t video_bit_rate, void *user_data)
{
    UNUSED_VAR(av);
    UNUSED_VAR(user_data);

    Call *call = &CallControl.calls[friend_number];
    call->video_bit_rate = video_bit_rate;

    /* TODO: with current toxav using one-pass VP8, the value of
     * video_bit_rate has no effect, except to disable video if it is 0.
     * Automatically change resolution instead? */
    toxav_video_set_bit_rate(CallControl.av, friend_number, call->video_bit_rate, NULL);
}

void callback_recv_video_starting(uint32_t friend_number)
{
    Call *this_call = &CallControl.calls[friend_number];

    if (this_call->status != cs_Active || this_call->vout_idx != -1) {
        return;
    }

    open_primary_video_device(vdt_output, &this_call->vout_idx, NULL, NULL);
}
void callback_recv_video_end(uint32_t friend_number)
{
    Call *this_call = &CallControl.calls[friend_number];

    if (this_call->status != cs_Active || this_call->vout_idx == -1) {
        return;
    }

    close_video_device(vdt_output, this_call->vout_idx);
    this_call->vout_idx = -1;
}
void callback_video_starting(uint32_t friend_number)
{
    Call *this_call = &CallControl.calls[friend_number];

    Toxav_Err_Call_Control error = TOXAV_ERR_CALL_CONTROL_OK;
    toxav_call_control(CallControl.av, friend_number, TOXAV_CALL_CONTROL_SHOW_VIDEO, &error);

    if (error == TOXAV_ERR_CALL_CONTROL_OK) {
        size_t i;

        for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
            ToxWindow *window = get_window_ptr(i);

            if (window != NULL && window->is_call && window->num == friend_number) {
                if (start_video_transmission(window, CallControl.av, this_call) == 0) {
                    line_info_add(window, NULL, NULL, NULL, SYS_MSG, 0, 0, "Video capture starting.");
                }
            }
        }
    }
}
void callback_video_end(uint32_t friend_number)
{
    stop_video_transmission(&CallControl.calls[friend_number], friend_number);
}
/*
 * End of Callbacks
 */



/*
 * Commands from chat_commands.h
 */
void cmd_vcall(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);
    UNUSED_VAR(m);
    UNUSED_VAR(argv);

    if (argc != 0) {
        print_err(self, "Unknown arguments.");
        return;
    }

    if (!CallControl.av) {
        print_err(self, "ToxAV not supported!");
        return;
    }

    if (!self->stb->connection) {
        print_err(self, "Friend is offline.");
        return;
    }

    Call *call = &CallControl.calls[self->num];

    if (call->status != cs_None) {
        print_err(self, "Already calling.");
        return;
    }

    init_call(call);

    call->video_bit_rate = DEFAULT_VIDEO_BIT_RATE;

    place_call(self);
}

void cmd_video(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);
    UNUSED_VAR(m);
    UNUSED_VAR(argv);

    Call *this_call = &CallControl.calls[self->num];

    if (argc != 0) {
        print_err(self, "Unknown arguments.");
        return;
    }

    if (!CallControl.av) {
        print_err(self, "ToxAV not supported!");
        return;
    }

    if (!self->stb->connection) {
        print_err(self, "Friend is offline.");
        return;
    }

    if (this_call->status != cs_Active) {
        print_err(self, "Not in call!");
        return;
    }

    if (this_call->vin_idx == -1) {
        this_call->video_bit_rate = DEFAULT_VIDEO_BIT_RATE;
        callback_video_starting(self->num);
    } else {
        callback_video_end(self->num);
    }
}

void cmd_res(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);
    UNUSED_VAR(m);

    Call *call = &CallControl.calls[self->num];

    if (argc == 0) {
        if (call->status == cs_Active && call->vin_idx != -1) {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0,
                          "Resolution of current call: %u x %u",
                          call->video_width, call->video_height);
        } else {
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0,
                          "Initial resolution for video calls: %u x %u",
                          CallControl.default_video_width, CallControl.default_video_height);
        }

        return;
    }

    if (argc != 2) {
        print_err(self, "Require 0 or 2 arguments.");
        return;
    }

    char *endw, *endh;
    const long int width = strtol(argv[1], &endw, 10);
    const long int height = strtol(argv[2], &endh, 10);

    if (*endw || *endh || width < 0 || height < 0) {
        print_err(self, "Invalid input");
        return;
    }

    if (call->status == cs_Active && call->vin_idx != -1) {
        stop_video_transmission(call, self->num);
        call->video_width = width;
        call->video_height = height;
        call->video_bit_rate = DEFAULT_VIDEO_BIT_RATE;
        start_video_transmission(self, CallControl.av, call);
    } else {
        CallControl.default_video_width = width;
        CallControl.default_video_height = height;
    }
}

void cmd_list_video_devices(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);
    UNUSED_VAR(m);

    if (argc != 1) {
        if (argc < 1) {
            print_err(self, "Type must be specified!");
        } else {
            print_err(self, "Only one argument allowed!");
        }

        return;
    }

    VideoDeviceType type;

    if (strcasecmp(argv[1], "in") == 0) { /* Input devices */
        type = vdt_input;
    }

    else if (strcasecmp(argv[1], "out") == 0) { /* Output devices */
        type = vdt_output;
    }

    else {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Invalid type: %s", argv[1]);
        return;
    }

    print_video_devices(self, type);
}

/* This changes primary video device only */
void cmd_change_video_device(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);
    UNUSED_VAR(m);

    if (argc != 2) {
        if (argc < 1) {
            print_err(self, "Type must be specified!");
        } else if (argc < 2) {
            print_err(self, "Must have id!");
        } else {
            print_err(self, "Only two arguments allowed!");
        }

        return;
    }

    VideoDeviceType type;

    if (strcmp(argv[1], "in") == 0) { /* Input devices */
        type = vdt_input;
    }

    else if (strcmp(argv[1], "out") == 0) { /* Output devices */
        type = vdt_output;
    }

    else {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Invalid type: %s", argv[1]);
        return;
    }


    char *end;
    long int selection = strtol(argv[2], &end, 10);

    if (*end) {
        print_err(self, "Invalid input");
        return;
    }

    if (set_primary_video_device(type, selection) == vde_InvalidSelection) {
        print_err(self, "Invalid selection!");
        return;
    }
}

#endif /* VIDEO */

/*  video_call.c
 *
 *  Copyright (C) 2014-2024 Toxic All Rights Reserved.
 *
 *  This file is part of Toxic. Toxic is free software licensed
 *  under the GNU General Public License 3.0.
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

#include <curses.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef VIDEO

#define DEFAULT_VIDEO_BIT_RATE 5000
#define DEFAULT_VIDEO_HEIGHT 400
#define DEFAULT_VIDEO_WIDTH 400

void on_video_receive_frame(ToxAV *av, uint32_t friend_number,
                            uint16_t width, uint16_t height,
                            uint8_t const *y, uint8_t const *u, uint8_t const *v,
                            int32_t ystride, int32_t ustride, int32_t vstride,
                            void *user_data);

void on_video_bit_rate(ToxAV *av, uint32_t friend_number, uint32_t video_bit_rate, void *user_data);

static void print_err(ToxWindow *self, const Client_Config *c_config, const char *error_str)
{
    line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "%s", error_str);
}

ToxAV *init_video(Toxic *toxic)
{
    ToxWindow *home_window = toxic->home_window;
    const Client_Config *c_config = toxic->c_config;

    CallControl.video_errors = ve_None;

    CallControl.video_enabled = true;
    CallControl.default_video_bit_rate = 0;
    CallControl.video_frame_duration = 10;
    CallControl.default_video_height = DEFAULT_VIDEO_HEIGHT;
    CallControl.default_video_width = DEFAULT_VIDEO_WIDTH;

    if (toxic->av == NULL) {
        line_info_add(home_window, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Video failed to init with ToxAV instance");
        return NULL;
    }

    if (init_video_devices(toxic) == vde_InternalError) {
        line_info_add(home_window, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to init video devices");
        return NULL;
    }

    toxav_callback_video_receive_frame(toxic->av, on_video_receive_frame, &CallControl);
    toxav_callback_video_bit_rate(toxic->av, on_video_bit_rate, &CallControl);

    return toxic->av;
}

void terminate_video(void)
{
    for (int i = 0; i < CallControl.max_calls; ++i) {
        Call *this_call = &CallControl.calls[i];

        stop_video_transmission(this_call, i);

        if (this_call->status == cs_Active && this_call->vout_idx != -1) {
            close_video_device(vdt_output, this_call->vout_idx);
            this_call->vout_idx = -1;
        }
    }

    terminate_video_devices();
}

static void read_video_device_callback(Toxic *toxic, int16_t width, int16_t height, const uint8_t *y, const uint8_t *u,
                                       const uint8_t *v, void *data)
{
    if (toxic == NULL) {
        return;
    }

    const Client_Config *c_config = toxic->c_config;

    ToxWindow *home_window = toxic->home_window;

    uint32_t friend_number = *((uint32_t *)data); /* TODO: Or pass an array of call_idx's */

    if (friend_number >= CallControl.max_calls) {
        line_info_add(home_window, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Invalid call index.");
        return;
    }

    Call *this_call = &CallControl.calls[friend_number];
    Toxav_Err_Send_Frame error;

    /* Drop frame if video sending is disabled */
    if (this_call->video_bit_rate == 0 || this_call->status != cs_Active || this_call->vin_idx == -1) {
        line_info_add(home_window, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Video frame dropped.");
        return;
    }

    if (toxav_video_send_frame(toxic->av, friend_number, width, height, y, u, v, &error) == false) {
        line_info_add(home_window, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to send video frame");

        if (error == TOXAV_ERR_SEND_FRAME_NULL) {
            line_info_add(home_window, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to capture video frame");
        } else if (error == TOXAV_ERR_SEND_FRAME_INVALID) {
            line_info_add(home_window, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to prepare video frame");
        }
    }
}

static void write_video_device_callback(uint32_t friend_number, uint16_t width, uint16_t height,
                                        uint8_t const *y, uint8_t const *u, uint8_t const *v,
                                        int32_t ystride, int32_t ustride, int32_t vstride,
                                        void *user_data)
{
    UNUSED_VAR(friend_number);

    write_video_out(width, height, y, u, v, ystride, ustride, vstride, user_data);
}

int start_video_transmission(ToxWindow *self, Toxic *toxic, Call *call)
{
    if (self == NULL || toxic == NULL) {
        return -1;
    }

    const Client_Config *c_config = toxic->c_config;

    if (toxic->av == NULL) {
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to prepare video transmission");
        return -1;
    }

    if (open_primary_video_device(vdt_input, &call->vin_idx, &call->video_width, &call->video_height) != vde_None) {
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to open input video device!");
        return -1;
    }

    if (register_video_device_callback(self->num, call->vin_idx, read_video_device_callback, &self->num) != vde_None) {
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to register input video handler!");
        return -1;
    }

    if (!toxav_video_set_bit_rate(toxic->av, self->num, call->video_bit_rate, NULL)) {
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Failed to set video bit rate");
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

    if (friend_number >= CallControl.max_calls) {
        return;
    }

    Call *call = &CallControl.calls[friend_number];
    call->video_bit_rate = video_bit_rate;

    /* TODO: with current toxav using one-pass VP8, the value of
     * video_bit_rate has no effect, except to disable video if it is 0.
     * Automatically change resolution instead? */
    toxav_video_set_bit_rate(CallControl.av, friend_number, call->video_bit_rate, NULL);
}

void callback_recv_video_starting(uint32_t friend_number)
{
    if (friend_number >= CallControl.max_calls) {
        return;
    }

    Call *this_call = &CallControl.calls[friend_number];

    if (this_call->status != cs_Active || this_call->vout_idx != -1) {
        return;
    }

    open_primary_video_device(vdt_output, &this_call->vout_idx, NULL, NULL);
}

void callback_recv_video_end(uint32_t friend_number)
{
    if (friend_number >= CallControl.max_calls) {
        return;
    }

    Call *this_call = &CallControl.calls[friend_number];

    if (this_call->status != cs_Active || this_call->vout_idx == -1) {
        return;
    }

    close_video_device(vdt_output, this_call->vout_idx);
    this_call->vout_idx = -1;
}

static void callback_video_starting(Toxic *toxic, uint32_t friend_number)
{
    if (friend_number >= CallControl.max_calls) {
        return;
    }

    Call *this_call = &CallControl.calls[friend_number];

    Toxav_Err_Call_Control error = TOXAV_ERR_CALL_CONTROL_OK;
    toxav_call_control(toxic->av, friend_number, TOXAV_CALL_CONTROL_SHOW_VIDEO, &error);

    Windows *windows = toxic->windows;

    if (error == TOXAV_ERR_CALL_CONTROL_OK) {
        for (uint16_t i = 0; i < windows->count; ++i) {
            ToxWindow *window = windows->list[i];

            if (window->is_call && window->num != friend_number) {
                continue;
            }

            if (start_video_transmission(window, toxic, this_call) == 0) {
                line_info_add(window, toxic->c_config, NULL, NULL, NULL, SYS_MSG, 0, 0, "Video capture starting.");
            }
        }
    }
}

void callback_video_end(uint32_t friend_number)
{
    if (friend_number >= CallControl.max_calls) {
        return;
    }

    stop_video_transmission(&CallControl.calls[friend_number], friend_number);
}

/*
 * End of Callbacks
 */



/*
 * Commands from chat_commands.h
 */
void cmd_vcall(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);
    UNUSED_VAR(argv);

    if (toxic == NULL || self == NULL) {
        return;
    }

    const Client_Config *c_config = toxic->c_config;

    if (argc != 0) {
        print_err(self, c_config, "Unknown arguments.");
        return;
    }

    if (toxic->av == NULL) {
        print_err(self, c_config, "ToxAV not supported!");
        return;
    }

    if (!self->stb->connection) {
        print_err(self, c_config, "Friend is offline.");
        return;
    }

    if (self->num >= CallControl.max_calls) {
        print_err(self, c_config, "Invalid call index.");
        return;
    }

    Call *call = &CallControl.calls[self->num];

    if (call->status != cs_None) {
        print_err(self, c_config, "Already calling.");
        return;
    }

    init_call(call);

    call->video_bit_rate = DEFAULT_VIDEO_BIT_RATE;

    place_call(self, toxic);
}

void cmd_video(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);
    UNUSED_VAR(argv);

    if (toxic == NULL || self == NULL) {
        return;
    }

    const Client_Config *c_config = toxic->c_config;

    if (self->num >= CallControl.max_calls) {
        print_err(self, c_config, "Invalid call index.");
        return;
    }

    Call *this_call = &CallControl.calls[self->num];

    if (argc != 0) {
        print_err(self, c_config, "Unknown arguments.");
        return;
    }

    if (toxic->av == NULL) {
        print_err(self, c_config, "ToxAV not supported!");
        return;
    }

    if (!self->stb->connection) {
        print_err(self, c_config, "Friend is offline.");
        return;
    }

    if (this_call->status != cs_Active) {
        print_err(self, c_config, "Not in call!");
        return;
    }

    if (this_call->vin_idx == -1) {
        this_call->video_bit_rate = DEFAULT_VIDEO_BIT_RATE;
        callback_video_starting(toxic, self->num);
    } else {
        callback_video_end(self->num);
    }
}

void cmd_res(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

    if (toxic == NULL || self == NULL) {
        return;
    }

    const Client_Config *c_config = toxic->c_config;

    if (self->num >= CallControl.max_calls) {
        print_err(self, c_config, "Invalid call index.");
        return;
    }

    Call *call = &CallControl.calls[self->num];

    if (argc == 0) {
        if (call->status == cs_Active && call->vin_idx != -1) {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0,
                          "Resolution of current call: %u x %u",
                          call->video_width, call->video_height);
        } else {
            line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0,
                          "Initial resolution for video calls: %u x %u",
                          CallControl.default_video_width, CallControl.default_video_height);
        }

        return;
    }

    if (argc != 2) {
        print_err(self, c_config, "Require 0 or 2 arguments.");
        return;
    }

    char *endw, *endh;
    const long int width = strtol(argv[1], &endw, 10);
    const long int height = strtol(argv[2], &endh, 10);

    if (*endw || *endh || width < 0 || height < 0) {
        print_err(self, c_config, "Invalid input");
        return;
    }

    if (call->status == cs_Active && call->vin_idx != -1) {
        stop_video_transmission(call, self->num);
        call->video_width = width;
        call->video_height = height;
        call->video_bit_rate = DEFAULT_VIDEO_BIT_RATE;
        start_video_transmission(self, toxic, call);
    } else {
        CallControl.default_video_width = width;
        CallControl.default_video_height = height;
    }
}

void cmd_list_video_devices(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

    if (toxic == NULL || self == NULL) {
        return;
    }

    const Client_Config *c_config = toxic->c_config;

    if (argc != 1) {
        if (argc < 1) {
            print_err(self, c_config, "Type must be specified!");
        } else {
            print_err(self, c_config, "Only one argument allowed!");
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
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Invalid type: %s", argv[1]);
        return;
    }

    print_video_devices(self, c_config, type);
}

/* This changes primary video device only */
void cmd_change_video_device(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    UNUSED_VAR(window);

    if (toxic == NULL || self == NULL) {
        return;
    }

    const Client_Config *c_config = toxic->c_config;

    if (argc != 2) {
        if (argc < 1) {
            print_err(self, c_config, "Type must be specified!");
        } else if (argc < 2) {
            print_err(self, c_config, "Must have id!");
        } else {
            print_err(self, c_config, "Only two arguments allowed!");
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
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Invalid type: %s", argv[1]);
        return;
    }


    char *end;
    long int selection = strtol(argv[2], &end, 10);

    if (*end) {
        print_err(self, c_config, "Invalid input");
        return;
    }

    if (set_primary_video_device(type, selection) == vde_InvalidSelection) {
        print_err(self, c_config, "Invalid selection!");
        return;
    }
}

#endif /* VIDEO */

#include "toxic.h"
#include "windows.h"
#include "video_call.h"
#include "video_device.h"
#include "chat_commands.h"
#include "global_commands.h"
#include "line_info.h"
#include "notify.h"

#include "assert.h"

void receive_video_frame_cb( ToxAV *av, uint32_t friend_number,
                                    uint16_t width, uint16_t height,
                                    uint8_t const *y, uint8_t const *u, uint8_t const *v, uint8_t const *a,
                                    int32_t ystride, int32_t ustride, int32_t vstride, int32_t astride,
                                    void *user_data );
void video_bit_rate_status_cb( ToxAV *av, uint32_t friend_number, 
                                      bool stable, uint32_t bit_rate, void *user_data);

void callback_video_starting ( void* av, uint32_t friend_number, void *arg );
void callback_video_ending   ( void* av, uint32_t friend_number, void *arg );

static void print_err (ToxWindow *self, const char *error_str)
{
    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "%s", error_str);
}

ToxAV *init_video(ToxWindow *self, Tox *tox, ToxAV *av, CallControl *user_data)
{

    user_data->video_enabled = true;

    if ( !user_data->av ) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Video failed to init ToxAV");
        return NULL;
    }

    if ( init_video_devices(user_data->av) == vde_InternalError ) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to init video devices");
        return NULL;
    }

    toxav_callback_video_receive_frame(user_data->av, receive_video_frame_cb, &user_data);
    toxav_callback_video_bit_rate_status(user_data->av, video_bit_rate_status_cb, &user_data);

    return av;
}

void read_video_device_callback(int16_t width, int16_t height, const uint8_t* y, const uint8_t* u, const uint8_t* v, void* data)
{
    TOXAV_ERR_SEND_FRAME error;
    int32_t friend_number = *((int32_t*)data); /* TODO: Or pass an array of call_idx's */

    line_info_add(CallContrl.window, NULL, NULL, NULL, SYS_MSG, 0, 0, "Read video device");
}

void write_video_device_callback(void *agent, int32_t friend_number, const int16_t* PCM, uint16_t size, void* arg)
{
    (void)arg;
    (void)agent;

    if (friend_number >= 0 && CallContrl.calls[friend_number].ttas)
        write_out(CallContrl.calls[friend_number].out_idx, PCM, size, CallContrl.audio_channels);
}

int start_video_transmission(ToxWindow *self, ToxAV *av, Call *call)
{
    if ( !self || !av) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Could not prepare transmission");
        return -1;
    }

    //if (set_call(call, true) == -1)
    //    return -1;

    VideoDeviceError error = open_primary_video_device(input, &call->in_idx);

    if ( error == vde_FailedStart)
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to start input video device");

    if ( error == vde_InternalError )
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Internal error with opening input video device");

    if ( error != vde_None )
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to open input video device!");

    
    if ( register_video_device_callback(self->num, call->in_idx,
         read_video_device_callback, &self->num) != vde_None)
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to register input video handler!");

    /*
    if ( open_primary_device(output, &call->out_idx,
            CallContrl.audio_sample_rate, CallContrl.audio_frame_duration, CallContrl.audio_channels) != de_None ) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to open output video device!");
        call->has_output = 0;
    }*/

    return 0;
}

int stop_video_transmission(Call *call, int friend_number)
{
    if ( call->ttas ) {
        if ( call->in_idx != -1 )
            close_video_device(input, call->in_idx);
        return 0;
    }

    return -1;
}

void receive_video_frame_cb(ToxAV *av, uint32_t friend_number,
                                    uint16_t width, uint16_t height,
                                    uint8_t const *y, uint8_t const *u, uint8_t const *v, uint8_t const *a,
                                    int32_t ystride, int32_t ustride, int32_t vstride, int32_t astride,
                                    void *user_data)
{
    CallControl* cc = user_data;
}


void video_bit_rate_status_cb(ToxAV *av, uint32_t friend_number, 
                                      bool stable, uint32_t bit_rate, void *user_data)
{
    CallControl* cc = user_data;

    if ( stable )
        cc->video_bit_rate = bit_rate;
}


void callback_video_starting(void* av, uint32_t friend_number, void *arg)
{
    CallControl *cc = (CallControl*)arg;
    ToxWindow* windows = cc->window;

    int i;
    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].is_call && windows[i].num == friend_number) {
            line_info_add(&windows[i], NULL, NULL, NULL, SYS_MSG, 0, 0, "Video starting");
            if(0 != start_video_transmission(&windows[i], av, &cc->calls[friend_number])) {
                line_info_add(&windows[i], NULL, NULL, NULL, SYS_MSG, 0, 0, "Error starting transmission!");
                return;
            }
        }
    }
}
void callback_video_ending(void* av, uint32_t friend_number, void *arg)
{
    CallControl *cc = (CallControl*)arg;
    ToxWindow* windows = cc->window;

    line_info_add(&windows[friend_number], NULL, NULL, NULL, SYS_MSG, 0, 0, "Video ending");

    int i;
    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].is_call && windows[i].num == friend_number) {
        line_info_add(&windows[i], NULL, NULL, NULL, SYS_MSG, 0, 0, "Video ending"); 
        }
    }

    stop_video_transmission(&cc->calls[friend_number], friend_number);
}




void cmd_video(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "cmd_video");
    const char *error_str;

    if ( argc != 0 ) {
        error_str = "Unknown arguments.";
        goto on_error;
    }

    if ( !CallContrl.av ) {
        error_str = "ToxAV not supported!";
        goto on_error;
    }

    if ( !self->stb->connection ) {
        error_str = "Friend is offline.";
        goto on_error;
    }

    callback_video_starting(CallContrl.av, self->num, &CallContrl);

    return;
on_error:
    print_err (self, error_str);
}

void cmd_end_video(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "cmd_end_video");
    const char *error_str;

    if ( argc != 0 ) {
        error_str = "Unknown arguments.";
        goto on_error;
    }

    if ( !CallContrl.av ) {
        error_str = "ToxAV not supported!";
        goto on_error;
    }

    callback_video_ending(CallContrl.av, self->num, &CallContrl);

    return;
on_error:
    print_err (self, error_str);
}

void cmd_list_video_devices(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "List video devices");
    const char *error_str;

    if ( argc != 1 ) {
        if ( argc < 1 ) error_str = "Type must be specified!";
        else error_str = "Only one argument allowed!";

        goto on_error;
    }

    VideoDeviceType type;

    if ( strcasecmp(argv[1], "in") == 0 ) /* Input devices */
        type = input;

    else if ( strcasecmp(argv[1], "out") == 0 ) /* Output devices */
        type = output;

    else {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Invalid type: %s", argv[1]);
        return;
    }

    print_video_devices(self, type);

    return;
on_error:
    print_err (self, error_str);
}

/* This changes primary device only */
void cmd_change_video_device(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Change video device");
    const char *error_str;

    if ( argc != 2 ) {
        if ( argc < 1 ) error_str = "Type must be specified!";
        else if ( argc < 2 ) error_str = "Must have id!";
        else error_str = "Only two arguments allowed!";

        goto on_error;
    }

    VideoDeviceType type;

    if ( strcmp(argv[1], "in") == 0 ) /* Input devices */
        type = input;

    else if ( strcmp(argv[1], "out") == 0 ) /* Output devices */
        type = output;

    else {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Invalid type: %s", argv[1]);
        return;
    }


    char *end;
    long int selection = strtol(argv[2], &end, 10);

    if ( *end ) {
        error_str = "Invalid input";
        goto on_error;
    }

    if ( set_primary_video_device(type, selection) == vde_InvalidSelection ) {
        error_str="Invalid selection!";
        goto on_error;
    }

    return;
on_error:
    print_err (self, error_str);
}

void cmd_ccur_video_device(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Change current video device");
    const char *error_str;

    if ( argc != 2 ) {
        if ( argc < 1 ) error_str = "Type must be specified!";
        else if ( argc < 2 ) error_str = "Must have id!";
        else error_str = "Only two arguments allowed!";

        goto on_error;
    }

    VideoDeviceType type;

    if ( strcmp(argv[1], "in") == 0 ) /* Input devices */
        type = input;

    else if ( strcmp(argv[1], "out") == 0 ) /* Output devices */
        type = output;

    else {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Invalid type: %s", argv[1]);
        return;
    }


    char *end;
    long int selection = strtol(argv[2], &end, 10);

    if ( *end ) {
        error_str = "Invalid input";
        goto on_error;
    }

    if ( selection_valid(type, selection) == vde_InvalidSelection ) {
        error_str="Invalid selection!";
        goto on_error;
    }

    /* If call is active, change device */
    if ( self->is_call ) {
        Call* this_call = &CallContrl.calls[self->num];
        if (this_call->ttas) {


            if (type == output) {
            }
            else {
                /* TODO: check for failure */
                close_video_device(input, this_call->in_idx);
                open_video_device(input, selection, &this_call->in_idx);
                register_video_device_callback(self->num, this_call->in_idx, read_video_device_callback, &self->num);
            }
        }
    }

    self->video_device_selection[type] = selection;

    return;
    on_error:
    print_err (self, error_str);
}
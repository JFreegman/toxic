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
                                    uint8_t const *y, uint8_t const *u, uint8_t const *v,
                                    int32_t ystride, int32_t ustride, int32_t vstride,
                                    void *user_data );
void video_bit_rate_status_cb( ToxAV *av, uint32_t friend_number, 
                                      bool stable, uint32_t bit_rate, void *user_data);

void callback_recv_video_starting(void* av, uint32_t friend_number, void *arg);
void callback_video_starting ( void* av, uint32_t friend_number, void *arg );
void callback_video_ending   ( void* av, uint32_t friend_number, void *arg );

static void print_err (ToxWindow *self, const char *error_str)
{
    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "%s", error_str);
}

ToxAV *init_video(ToxWindow *self, Tox *tox, ToxAV *av, CallControl *user_data)
{

    user_data->video_enabled = true;
    user_data->video_bit_rate = 5000;
    user_data->video_frame_duration = 10;

    if ( !av ) {
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

void terminate_video()
{
    int i;
    for (i = 0; i < MAX_CALLS; ++i)
        stop_video_transmission(&CallContrl.calls[i], i);

    terminate_video_devices();
}

void read_video_device_callback(int16_t width, int16_t height, const uint8_t* y, const uint8_t* u, const uint8_t* v, void* data)
{
    TOXAV_ERR_SEND_FRAME error;
    int32_t friend_number = *((int32_t*)data); /* TODO: Or pass an array of call_idx's */

    if ( toxav_video_send_frame(CallContrl.av, friend_number, width, height, y, u, v, &error ) == false ) {
        line_info_add(CallContrl.window, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to send video frame");
        if ( error == TOXAV_ERR_SEND_FRAME_NULL ) {
            line_info_add(CallContrl.window, NULL, NULL, NULL, SYS_MSG, 0, 0, "Error NULL video frame");
        } else if ( error == TOXAV_ERR_SEND_FRAME_FRIEND_NOT_FOUND ) {
            line_info_add(CallContrl.window, NULL, NULL, NULL, SYS_MSG, 0, 0, "Error friend not found");
        } else if ( error = TOXAV_ERR_SEND_FRAME_INVALID ) {
            line_info_add(CallContrl.window, NULL, NULL, NULL, SYS_MSG, 0, 0, "Error invalid video frame");
        }
    }
}

void write_video_device_callback(uint32_t friend_number, uint16_t width, uint16_t height,
                                           uint8_t const *y, uint8_t const *u, uint8_t const *v,
                                           int32_t ystride, int32_t ustride, int32_t vstride,
                                           void *user_data)
{
    CallControl* cc = (CallControl*)user_data;
    Call* this_call = &cc->calls[friend_number];

    if(write_video_out(width, height, y, u, v, ystride, ustride, vstride, user_data) == vde_DeviceNotActive)
        callback_recv_video_starting(cc->av, friend_number, cc);
}

int start_video_transmission(ToxWindow *self, ToxAV *av, Call *call)
{
    if ( !self || !av) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Could not prepare transmission");
        return -1;
    }

    if ( open_primary_video_device(vdt_input, &call->in_idx) != vde_None ) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to open input video device!");
        return -1;
    }
    
    if ( register_video_device_callback(self->num, call->in_idx,
         read_video_device_callback, &self->num) != vde_None)
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to register input video handler!");

    return 0;
}

int stop_video_transmission(Call *call, int friend_number)
{
    if ( call->in_idx != -1 )
        close_video_device(vdt_input, call->in_idx);
    if ( call->out_idx != -1 )
        close_video_device(vdt_output, call->out_idx);
    return 0;
}

/*
 * End of transmission
 */





/*
 * Callbacks
 */
void receive_video_frame_cb(ToxAV *av, uint32_t friend_number,
                                    uint16_t width, uint16_t height,
                                    uint8_t const *y, uint8_t const *u, uint8_t const *v,
                                    int32_t ystride, int32_t ustride, int32_t vstride,
                                    void *user_data)
{
    CallControl* cc = (CallControl*)user_data;

    write_video_device_callback(friend_number, width, height, y, u, v, ystride, ustride, vstride, user_data);
}

void video_bit_rate_status_cb(ToxAV *av, uint32_t friend_number, 
                                      bool stable, uint32_t bit_rate, void *user_data)
{
    CallControl* cc = (CallControl*)user_data;

    if ( stable ) {
        cc->video_bit_rate = bit_rate;
        toxav_video_bit_rate_set(CallContrl.av, friend_number, CallContrl.video_bit_rate, false, NULL);
    }
}

void callback_recv_video_starting(void* av, uint32_t friend_number, void *arg)
{
    CallControl *cc = (CallControl*)arg;
    ToxWindow* windows = cc->window;
    Call* this_call = &cc->calls[friend_number];

    open_primary_video_device(vdt_output, &this_call->out_idx);
    CallContrl.video_call = true;
}

void callback_video_starting(void* av, uint32_t friend_number, void *arg)
{
    CallControl *cc = (CallControl*)arg;
    ToxWindow* windows = cc->window;
    Call* this_call = &cc->calls[friend_number];

    int i;
    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].is_call && windows[i].num == friend_number) {
            if(0 != start_video_transmission(&windows[i], av, this_call)) {
                line_info_add(&windows[i], NULL, NULL, NULL, SYS_MSG, 0, 0, "Error starting transmission!");
                return;
            }

            line_info_add(&windows[i], NULL, NULL, NULL, SYS_MSG, 0, 0, "Video capture starting.");
            cc->video_call = true;
        }
    }
}
void callback_video_ending(void* av, uint32_t friend_number, void *arg)
{
    CallControl *cc = (CallControl*)arg;
    ToxWindow* windows = cc->window;
    Call* this_call = &cc->calls[friend_number];

    int i;
    for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i].is_call && windows[i].num == friend_number) {
            line_info_add(&windows[i], NULL, NULL, NULL, SYS_MSG, 0, 0, "Video operations ending.");

        }
    }

    cc->video_call = false;
    terminate_video();
}
/*
 * End of Callbacks
 */



/*
 * Commands from chat_commands.h
 */
void cmd_video(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
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

    if ( !self->is_call ) {
        error_str = "Not in call!";
        goto on_error;
    }

    if ( CallContrl.video_call ) {
        error_str = "Video is already sending in this call.";
        goto on_error;
    }

    if ( toxav_video_bit_rate_set(CallContrl.av, self->num, CallContrl.video_bit_rate, false, NULL) == false ) {
        error_str = "ToxAV video bit rate uninitialized.";
        goto on_error;
    }

    callback_video_starting(CallContrl.av, self->num, &CallContrl);

    return;
on_error:
    print_err (self, error_str);
}

void cmd_end_video(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    const char *error_str;

    if ( argc != 0 ) {
        error_str = "Unknown arguments.";
        goto on_error;
    }

    if ( !CallContrl.av ) {
        error_str = "ToxAV not supported!";
        goto on_error;
    }

    if ( !CallContrl.video_call ) {
        error_str = "Video is not running in this call.";
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

/* This changes primary video device only */
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
                close_video_device(input, &this_call->in_idx);
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
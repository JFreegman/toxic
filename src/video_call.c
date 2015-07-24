#include "toxic.h"
#include "windows.h"
#include "video_call.h"
#include "video_device.h"
#include "chat_commands.h"
#include "global_commands.h"
#include "line_info.h"
#include "notify.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>

#include "assert.h"

void receive_video_frame_cb( ToxAV *av, uint32_t friend_number,
                                    uint16_t width, uint16_t height,
                                    uint8_t const *y, uint8_t const *u, uint8_t const *v, uint8_t const *a,
                                    int32_t ystride, int32_t ustride, int32_t vstride, int32_t astride,
                                    void *user_data );
void video_bit_rate_status_cb( ToxAV *av, uint32_t friend_number, 
                                      bool stable, uint32_t bit_rate, void *user_data);

static void print_err (ToxWindow *self, const char *error_str)
{
    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "%s", error_str);
}

ToxAV *init_video(ToxWindow *self, Tox *tox, ToxAV *av, CallControl *user_data)
{
    XInitThreads();

    Display *display;
    if ((display = XOpenDisplay(NULL)) == NULL) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to open X11 display");
        return NULL;
    }

    int screen;
    screen = DefaultScreen(display);

    Window win;
    if ((win = XCreateSimpleWindow(display, RootWindow(display, screen), 400, 400, 800, 600, 0, 
    BlackPixel(display, screen), WhitePixel(display, screen))) == NULL) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to create X11 window");
        return NULL;
    }

    XSelectInput(display, win, ExposureMask|ButtonPressMask|KeyPressMask);

    GC default_gc;
    if ((default_gc = DefaultGC(display, screen)) == NULL) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to create X11 graphics context");
        return NULL;
    }

    XMapWindow(display, win);
    XClearWindow(display, win);
    XMapRaised(display, win);
    XFlush(display);

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

void receive_video_frame_cb(ToxAV *av, uint32_t friend_number,
                                    uint16_t width, uint16_t height,
                                    uint8_t const *y, uint8_t const *u, uint8_t const *v, uint8_t const *a,
                                    int32_t ystride, int32_t ustride, int32_t vstride, int32_t astride,
                                    void *user_data)
{
}

void video_bit_rate_status_cb(ToxAV *av, uint32_t friend_number, 
                                      bool stable, uint32_t bit_rate, void *user_data)
{
    CallControl* cc = user_data;

    if ( stable )
        cc->video_bit_rate = bit_rate;
}

void cmd_list_video_devices(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    const char *error_str;

    if ( argc != 1 ) {
        if ( argc < 1 ) error_str = "Type must be specified!";
        else error_str = "Only one argument allowed!";

        goto on_error;
    }

    DeviceType type;

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
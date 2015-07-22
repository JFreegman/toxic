#include "toxic.h"
#include "windows.h"
#include "video_call.h"
#include "chat_commands.h"
#include "global_commands.h"
#include "line_info.h"
#include "notify.h"

#include <X11/Xlib.h>

ToxAV *init_video(ToxWindow *self, Tox *tox, ToxAV *av, CallControl *user_data)
{
    Display *display;
    if ((display = XOpenDisplay(NULL)) == NULL) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Failed to open X11 display");
        return NULL;
    }

    user_data->video_enabled = true;

    //toxav_callback_video_receive_frame(CallContrl.av, receive_video_frame_cb, &CallContrl);
    //toxav_callback_video_bit_rate_status(CallContrl.av, video_bit_rate_status_cb, &CallContrl);

    return av;
}



void receive_video_frame_cb(ToxAV *av, uint32_t friend_number,
                                    uint16_t width, uint16_t height,
                                    uint8_t const *y, uint8_t const *u, uint8_t const *v, uint8_t const *a,
                                    int32_t ystride, int32_t ustride, int32_t vstride, int32_t astride,
                                    void *user_data)
{}

void video_bit_rate_status_cb(ToxAV *av, uint32_t friend_number, 
                                      bool stable, uint32_t bit_rate, void *user_data)
{
    CallControl* cc = user_data;

    if ( stable )
        cc->video_bit_rate = bit_rate;
}
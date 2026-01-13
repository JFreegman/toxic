/*  x11focus.c
 *
 *  Copyright (C) 2020-2026 Toxic All Rights Reserved.
 *
 *  This file is part of Toxic. Toxic is free software licensed
 *  under the GNU General Public License 3.0.
 */

#include "x11focus.h"

#ifndef __APPLE__

#include <X11/Xlib.h>

static long unsigned int focused_window_id(const X11_Focus *focus)
{
    if (!focus->display) {
        return 0;
    }

    Window window_focus;
    int revert;

    XLockDisplay(focus->display);
    XGetInputFocus(focus->display, &window_focus, &revert);
    XUnlockDisplay(focus->display);

    return window_focus;
}

bool is_focused(const X11_Focus *focus)
{
    if (!focus->display) {
        return false;
    }

    return focus->terminal_window == focused_window_id(focus);
}

int init_x11focus(X11_Focus *focus)
{
    if (XInitThreads() == 0) {
        return -1;
    }

    focus->display = XOpenDisplay(NULL);

    if (!focus->display) {
        return -1;
    }

    focus->terminal_window = focused_window_id(focus);

    return 0;
}

void terminate_x11focus(X11_Focus *focus)
{
    if (!focus->display || !focus->terminal_window) {
        return;
    }

    XLockDisplay(focus->display);
    XCloseDisplay(focus->display);
    XUnlockDisplay(focus->display);
}

#endif /* !__APPLE__ */

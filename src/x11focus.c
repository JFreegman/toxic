/*  x11focus.c
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

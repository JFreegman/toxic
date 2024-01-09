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

static struct Focus {
    Display *display;
    Window terminal_window;
} Focus;

static long unsigned int focused_window_id(void)
{
    if (!Focus.display) {
        return 0;
    }

    Window focus;
    int revert;

    XLockDisplay(Focus.display);
    XGetInputFocus(Focus.display, &focus, &revert);
    XUnlockDisplay(Focus.display);

    return focus;
}

bool is_focused(void)
{
    if (!Focus.display) {
        return false;
    }

    return Focus.terminal_window == focused_window_id();
}

int init_x11focus(void)
{
    if (XInitThreads() == 0) {
        return -1;
    }

    Focus.display = XOpenDisplay(NULL);

    if (!Focus.display) {
        return -1;
    }

    Focus.terminal_window = focused_window_id();

    return 0;
}

void terminate_x11focus(void)
{
    if (!Focus.display || !Focus.terminal_window) {
        return;
    }

    XLockDisplay(Focus.display);
    XCloseDisplay(Focus.display);
    XUnlockDisplay(Focus.display);
}

#endif /* !__APPLE__ */

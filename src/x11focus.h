/*  x11focus.h
 *
 *  Copyright (C) 2020-2024 Toxic All Rights Reserved.
 *
 *  This file is part of Toxic. Toxic is free software licensed
 *  under the GNU General Public License 3.0.
 */

#ifndef X11FOCUS_H
#define X11FOCUS_H

#include <stdbool.h>

#ifndef __APPLE__
#include <X11/Xlib.h>

struct X11_Focus {
    Display *display;
    Window terminal_window;
};

typedef struct X11_Focus X11_Focus;

int init_x11focus(X11_Focus *focus);
void terminate_x11focus(X11_Focus *focus);
bool is_focused(const X11_Focus *focus);

#endif /* __APPLE__ */
#endif /* X11FOCUS */

/*  xtra.c
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

#include "xtra.h"

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <pthread.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>


const  Atom XtraTerminate = 1;
const  Atom XtraNil = 0;

static Atom XdndAware;
static Atom XdndEnter;
static Atom XdndLeave;
static Atom XdndPosition;
static Atom XdndStatus;
static Atom XdndDrop;
static Atom XdndSelection;
static Atom XdndDATA;
static Atom XdndTypeList;
static Atom XdndActionCopy;
static Atom XdndFinished;

struct Xtra {
    drop_callback on_drop;
    Display *display;
    Window terminal_window;
    Window proxy_window;
    Window source_window; /* When we have a drop */
    Atom handling_version;
    Atom expecting_type;
} Xtra;

typedef struct Property {
    unsigned char *data;
    int            read_format;
    unsigned long  read_num;
    Atom           read_type;
} Property;

static Property read_property(Window s, Atom p)
{
    Atom read_type;
    int  read_format;
    unsigned long read_num;
    unsigned long left_bytes;
    unsigned char *data = NULL;

    int read_bytes = 1024;

    /* Keep trying to read the property until there are no bytes unread */
    do {
        if (data) {
            XFree(data);
        }

        XGetWindowProperty(Xtra.display, s,
                           p, 0,
                           read_bytes,
                           False, AnyPropertyType,
                           &read_type, &read_format,
                           &read_num, &left_bytes,
                           &data);

        read_bytes *= 2;
    } while (left_bytes != 0);

    Property property = {data, read_format, read_num, read_type};
    return property;
}

static Atom get_dnd_type(long *a, int l)
{
    int i = 0;

    for (; i < l; i ++) {
        if (a[i] != XtraNil) {
            return a[i];    /* Get first valid */
        }
    }

    return XtraNil;
}

/* TODO maybe support only certain types in the future */
static void handle_xdnd_enter(XClientMessageEvent *e)
{
    Xtra.handling_version = (e->data.l[1] >> 24);

    if ((e->data.l[1] & 1)) {
        // Fetch the list of possible conversions
        Property p = read_property(e->data.l[0], XdndTypeList);
        Xtra.expecting_type = get_dnd_type((long *)p.data, p.read_num);
        XFree(p.data);
    } else {
        // Use the available list
        Xtra.expecting_type = get_dnd_type(e->data.l + 2, 3);
    }
}

static void handle_xdnd_position(XClientMessageEvent *e)
{
    XEvent ev = {
        .xclient = {
            .type = ClientMessage,
            .display = e->display,
            .window = e->data.l[0],
            .message_type = XdndStatus,
            .format = 32,
            .data = {
                .l = {
                    Xtra.proxy_window,
                    (Xtra.expecting_type != XtraNil),
                    0, 0,
                    XdndActionCopy
                }
            }
        }
    };

    XSendEvent(Xtra.display, e->data.l[0], False, NoEventMask, &ev);
    XFlush(Xtra.display);
}

static void handle_xdnd_drop(XClientMessageEvent *e)
{
    /* Not expecting any type */
    if (Xtra.expecting_type == XtraNil) {
        XEvent ev = {
            .xclient = {
                .type = ClientMessage,
                .display = e->display,
                .window = e->data.l[0],
                .message_type = XdndFinished,
                .format = 32,
                .data = {
                    .l = {Xtra.proxy_window, 0, 0}
                }
            }
        };

        XSendEvent(Xtra.display, e->data.l[0], False, NoEventMask, &ev);
    } else {
        Xtra.source_window = e->data.l[0];
        XConvertSelection(Xtra.display,
                          XdndSelection,
                          Xtra.expecting_type,
                          XdndSelection,
                          Xtra.proxy_window,
                          Xtra.handling_version >= 1 ? e->data.l[2] : CurrentTime);
    }
}

static void handle_xdnd_selection(XSelectionEvent *e)
{
    /* DnD succesfully finished, send finished and call callback */
    XEvent ev = {
        .xclient = {
            .type = ClientMessage,
            .display = Xtra.display,
            .window = Xtra.source_window,
            .message_type = XdndFinished,
            .format = 32,
            .data = {
                .l = {Xtra.proxy_window, 1, XdndActionCopy}
            }
        }
    };
    XSendEvent(Xtra.display, Xtra.source_window, False, NoEventMask, &ev);

    Property p = read_property(Xtra.proxy_window, XdndSelection);
    DropType dt;

    if (strcmp(XGetAtomName(Xtra.display, p.read_type), "text/uri-list") == 0) {
        dt = DT_file_list;
    } else { /* text/uri-list */
        dt = DT_plain;
    }


    /* Call callback for every entry */
    if (Xtra.on_drop && p.read_num) {
        char *sptr;
        char *str = strtok_r((char *) p.data, "\n\r", &sptr);

        if (str) {
            Xtra.on_drop(str, dt);
        }

        while ((str = strtok_r(NULL, "\n\r", &sptr))) {
            Xtra.on_drop(str, dt);
        }
    }

    if (p.data) {
        XFree(p.data);
    }
}

void *event_loop(void *p)
{
    /* Handle events like a real nigga */

    (void) p; /* DINDUNOTHIN */

    XEvent event;
    int pending;

    while (Xtra.display) {
        /* NEEDMOEVENTSFODEMPROGRAMS */

        XLockDisplay(Xtra.display);

        if ((pending = XPending(Xtra.display))) {
            XNextEvent(Xtra.display, &event);
        }

        if (!pending) {
            XUnlockDisplay(Xtra.display);
            usleep(10000);
            continue;
        }

        if (event.type == ClientMessage) {
            Atom type = event.xclient.message_type;

            if (type == XdndEnter) {
                handle_xdnd_enter(&event.xclient);
            } else if (type == XdndPosition) {
                handle_xdnd_position(&event.xclient);
            } else if (type == XdndDrop) {
                handle_xdnd_drop(&event.xclient);
            } else if (type == XtraTerminate) {
                break;
            }
        } else if (event.type == SelectionNotify) {
            handle_xdnd_selection(&event.xselection);
        }
        /* AINNOBODYCANHANDLEDEMEVENTS*/
        else {
            XSendEvent(Xtra.display, Xtra.terminal_window, 0, 0, &event);
        }

        XUnlockDisplay(Xtra.display);
    }

    /* Actual XTRA termination
     * Please call xtra_terminate() at exit
     * otherwise HEWUSAGUDBOI happens
     */
    if (Xtra.display) {
        XCloseDisplay(Xtra.display);
    }

    return (Xtra.display = NULL);
}

static long unsigned int focused_window_id(void)
{
    if (!Xtra.display) {
        return 0;
    }

    Window focus;
    int revert;
    XLockDisplay(Xtra.display);
    XGetInputFocus(Xtra.display, &focus, &revert);
    XUnlockDisplay(Xtra.display);
    return focus;
}

int is_focused(void)
{
    return Xtra.proxy_window == focused_window_id() || Xtra.terminal_window == focused_window_id();
}

int init_xtra(drop_callback d)
{
    if (!d) {
        return -1;
    } else {
        Xtra.on_drop = d;
    }

    XInitThreads();

    if (!(Xtra.display = XOpenDisplay(NULL))) {
        return -1;
    }

    Xtra.terminal_window = focused_window_id();

    /* OSX: if focused window is 0, it means toxic is ran from
     * native terminal and not X11 terminal window, silently exit */
    if (!Xtra.terminal_window) {
        return 0;
    }

    {
        /* Create an invisible window which will act as proxy for the DnD operation. */
        XSetWindowAttributes attr  = {0};
        attr.event_mask            = EnterWindowMask |
                                     LeaveWindowMask |
                                     ButtonMotionMask |
                                     ButtonPressMask |
                                     ButtonReleaseMask |
                                     ResizeRedirectMask;

        attr.do_not_propagate_mask = NoEventMask;

        Window root;
        int x, y;
        unsigned int wht, hht, b, d;

        /* Since we cannot capture resize events for parent window we will have to create
         * this window to have maximum size as defined in root window
         */
        XGetGeometry(Xtra.display,
                     XDefaultRootWindow(Xtra.display),
                     &root, &x, &y, &wht, &hht, &b, &d);

        if (!(Xtra.proxy_window = XCreateWindow
                                  (Xtra.display, Xtra.terminal_window,       /* Parent */
                                   0, 0,                                     /* Position */
                                   wht, hht,                                 /* Width + height */
                                   0,                                        /* Border width */
                                   CopyFromParent,                           /* Depth */
                                   InputOnly,                                /* Class */
                                   CopyFromParent,                           /* Visual */
                                   CWEventMask | CWCursor,                   /* Value mask */
                                   &attr))) {                                /* Attributes for value mask */
            return -1;
        }
    }

    XMapWindow(Xtra.display, Xtra.proxy_window);   /* Show window (sandwich) */
    XLowerWindow(Xtra.display, Xtra.proxy_window); /* Don't interfere with parent lmao */

    XdndAware = XInternAtom(Xtra.display, "XdndAware", False);
    XdndEnter = XInternAtom(Xtra.display, "XdndEnter", False);
    XdndLeave = XInternAtom(Xtra.display, "XdndLeave", False);
    XdndPosition = XInternAtom(Xtra.display, "XdndPosition", False);
    XdndStatus = XInternAtom(Xtra.display, "XdndStatus", False);
    XdndDrop = XInternAtom(Xtra.display, "XdndDrop", False);
    XdndSelection = XInternAtom(Xtra.display, "XdndSelection", False);
    XdndDATA = XInternAtom(Xtra.display, "XdndDATA", False);
    XdndTypeList = XInternAtom(Xtra.display, "XdndTypeList", False);
    XdndActionCopy = XInternAtom(Xtra.display, "XdndActionCopy", False);
    XdndFinished = XInternAtom(Xtra.display, "XdndFinished", False);

    /* Inform my nigga windows that we are aware of dnd */
    Atom XdndVersion = 3;
    XChangeProperty(Xtra.display,
                    Xtra.proxy_window,
                    XdndAware,
                    XA_ATOM,
                    32,
                    PropModeReplace,
                    (unsigned char *)&XdndVersion, 1);

    pthread_t id;

    if (pthread_create(&id, NULL, event_loop, NULL) != 0) {
        return -1;
    }

    pthread_detach(id);

    return 0;
}

void terminate_xtra(void)
{
    if (!Xtra.display || !Xtra.terminal_window) {
        return;
    }

    XEvent terminate = {
        .xclient = {
            .type = ClientMessage,
            .display = Xtra.display,
            .message_type = XtraTerminate,
        }
    };

    XLockDisplay(Xtra.display);
    XDeleteProperty(Xtra.display, Xtra.proxy_window, XdndAware);
    XSendEvent(Xtra.display, Xtra.proxy_window, 0, NoEventMask, &terminate);
    XUnlockDisplay(Xtra.display);

    while (Xtra.display); /* Wait for termination */
}

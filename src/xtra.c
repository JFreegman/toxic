#include "xtra.h"

#include <X11/Xlib.h>
#include <X11/Xatom.h>
// #include <X11/X.h>
// #include <X11/Xutil.h>

#include <pthread.h>
#include <assert.h>

static Display *the_d = NULL;
static Window self_window; /* We will assume that during initialization
                            * we have this window focused */

/* For dnd protocol NOTE: copied from utox's code */
Atom XdndAware, 
     XdndEnter, 
     XdndLeave, 
     XdndPosition, 
     XdndStatus, 
     XdndDrop, 
     XdndSelection, 
     XdndDATA, 
     XdndActionCopy;

void *event_loop(void* p)
{
    (void) p;
    
    XEvent event;
    while (the_d)
    {
        XNextEvent(the_d, &event);
        
        switch (event.type)
        {
            case ClientMessage:
            {
                
                assert(0);
                if(event.xclient.message_type == XdndEnter) {
                } else if(event.xclient.message_type == XdndPosition) {
                    Window src = event.xclient.data.l[0];
                    XEvent event = {
                        .xclient = {
                            .type = ClientMessage,
                            .display = the_d,
                            .window = src,
                            .message_type = XdndStatus,
                            .format = 32,
                            .data = {
                                .l = {self_window, 1, 0, 0, XdndActionCopy}
                            }
                        }
                    };
                    XSendEvent(the_d, src, 0, 0, &event);
                } else if(event.xclient.message_type == XdndStatus) {
                } else if(event.xclient.message_type == XdndDrop) {
                    XConvertSelection(the_d, XdndSelection, XA_STRING, XdndDATA, self_window, CurrentTime);
                } else if(event.xclient.message_type == XdndLeave) {
                } else {
                }
                
                goto exit;
            } break;
            
            default:
                break;
        }
    }
    
exit:
    /* Actual XTRA termination 
     * Please call xtra_terminate() at exit
     * otherwise bad stuff happens
     */
    if (the_d) XCloseDisplay(the_d);
    return NULL;
}

void xtra_init()
{
    the_d = XOpenDisplay(NULL);
    self_window = xtra_focused_window_id();
    
    XSelectInput(the_d, self_window, ExposureMask | ButtonPressMask | ButtonReleaseMask | EnterWindowMask | LeaveWindowMask |
    PointerMotionMask | StructureNotifyMask | KeyPressMask | KeyReleaseMask | FocusChangeMask |
    PropertyChangeMask);

    XdndAware = XInternAtom(the_d, "XdndAware", False);
    XdndEnter = XInternAtom(the_d, "XdndEnter", False);
    XdndLeave = XInternAtom(the_d, "XdndLeave", False);
    XdndPosition = XInternAtom(the_d, "XdndPosition", False);
    XdndStatus = XInternAtom(the_d, "XdndStatus", False);
    XdndDrop = XInternAtom(the_d, "XdndDrop", False);
    XdndSelection = XInternAtom(the_d, "XdndSelection", False);
    XdndDATA = XInternAtom(the_d, "XdndDATA", False);
    XdndActionCopy = XInternAtom(the_d, "XdndActionCopy", False);
    
    Atom Xdndversion = 3;
    XChangeProperty(the_d, self_window, XdndAware, XA_ATOM, 32, 0, (unsigned char*)&Xdndversion, 1);
    /*XSelectInput(the_d, self_window, ExposureMask | ButtonPressMask | ButtonReleaseMask | EnterWindowMask | LeaveWindowMask | PointerMotionMask | StructureNotifyMask | KeyPressMask | KeyReleaseMask);*/
    pthread_t id;
    pthread_create(&id, NULL, event_loop, NULL);
    pthread_detach(id);
}

void xtra_terminate()
{
    XEvent terminate_event;
    terminate_event.xclient.display = the_d;
    terminate_event.type = ClientMessage;
    
    
    XDeleteProperty(the_d, self_window, XdndAware);
    XSendEvent(the_d, self_window, 0, NoEventMask, &terminate_event);
}

long unsigned int xtra_focused_window_id()
{
    if (!the_d) return 0;
    
    Window focus;
    int revert;
    XGetInputFocus(the_d, &focus, &revert);
    return focus;
}

int xtra_is_this_focused()
{
    return self_window == xtra_focused_window_id();
}

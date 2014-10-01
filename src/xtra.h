#ifndef XTRA_H
#define XTRA_H

/* NOTE: If no xlib present don't compile */

void xtra_init();
void xtra_terminate();

long unsigned int xtra_focused_window_id();
int xtra_is_this_focused(); /* returns bool */

#endif /* DND_H */
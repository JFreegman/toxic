#ifndef XTRA_H
#define XTRA_H

/* NOTE: If no xlib present don't compile */

typedef enum {
    DT_plain,
    DT_file_list
} 
DropType;

typedef void (*drop_callback) (const char*, DropType);

int               xtra_init(drop_callback d);
void              xtra_terminate();
long unsigned int xtra_focused_window_id();
int               xtra_is_this_focused(); /* returns bool */

#endif /* XTRA_H */
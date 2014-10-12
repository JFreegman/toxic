#ifndef XTRA_H
#define XTRA_H

/* NOTE: If no xlib present don't compile */

typedef enum {
    DT_plain,
    DT_file_list
} 
DropType;

typedef void (*drop_callback) (const char*, DropType);

int               init_xtra(drop_callback d);
void              terminate_xtra();
long unsigned int focused_window_id();
int               is_focused(); /* returns bool */

#endif /* XTRA_H */
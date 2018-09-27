/*  xtra.h
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

#ifndef XTRA_H
#define XTRA_H

/* NOTE: If no xlib present don't compile */

typedef enum {
    DT_plain,
    DT_file_list
}
DropType;

typedef void (*drop_callback)(const char *, DropType);

int               init_xtra(drop_callback d);
void              terminate_xtra(void);
int               is_focused(void); /* returns bool */

#endif /* XTRA_H */

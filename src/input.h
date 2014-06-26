/*  input.h
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

#ifndef _input_h
#define _input_h

/* add a char to input field and buffer for given chatcontext */
void input_new_char(ToxWindow *self, wint_t key, int x, int y, int mx_x, int mx_y);

/* Handles non-printable input keys that behave the same for all types of chat windows.
   return true if key matches a function, false otherwise */
bool input_handle(ToxWindow *self, wint_t key, int x, int y, int mx_x, int mx_y);

#endif /* #define _input_h */
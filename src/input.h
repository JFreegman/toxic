/*  input.h
 *
 *  Copyright (C) 2014-2024 Toxic All Rights Reserved.
 *
 *  This file is part of Toxic. Toxic is free software licensed
 *  under the GNU General Public License 3.0.
 */

#ifndef INPUT_H
#define INPUT_H

#include "settings.h"
#include "windows.h"

/* add a char to input field and buffer for given chatcontext */
void input_new_char(ToxWindow *self, const Toxic *toxic, wint_t key, int x, int mx_x);

/* Handles non-printable input keys that behave the same for all types of chat windows.
   return true if key matches a function, false otherwise */
bool input_handle(ToxWindow *self, Toxic *toxic, wint_t key, int x, int mx_x);

#endif /* INPUT_H */

/*  help.h
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

#ifndef HELP_H
#define HELP_H

#include "toxic.h"
#include "windows.h"

typedef enum {
    HELP_MENU,
    HELP_GLOBAL,
    HELP_GROUP,
    HELP_CHAT,
    HELP_CONFERENCE,
    HELP_KEYS,
    HELP_CONTACTS,
#ifdef PYTHON
    HELP_PLUGIN,
#endif
} HELP_TYPES;

void help_onDraw(ToxWindow *self);
void help_init_menu(ToxWindow *self);
void help_onKey(ToxWindow *self, wint_t key);

#endif /* HELP_H */

/*  help.h
 *
 *  Copyright (C) 2014-2024 Toxic All Rights Reserved.
 *
 *  This file is part of Toxic. Toxic is free software licensed
 *  under the GNU General Public License 3.0.
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

void help_draw_main(ToxWindow *self);
void help_init_menu(ToxWindow *self);
void help_onKey(ToxWindow *self, wint_t key);

#endif /* HELP_H */

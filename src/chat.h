/*  chat.h
 *
 *  Copyright (C) 2014-2024 Toxic All Rights Reserved.
 *
 *  This file is part of Toxic. Toxic is free software licensed
 *  under the GNU General Public License 3.0.
 */

#ifndef CHAT_H
#define CHAT_H

#include "toxic.h"
#include "windows.h"

/* set CTRL to -1 if we don't want to send a control signal.
   set msg to NULL if we don't want to display a message */
void chat_close_file_receiver(Tox *tox, int filenum, int friendnum, int CTRL);
void kill_chat_window(ToxWindow *self, Toxic *tox);
ToxWindow *new_chat(Tox *tox, uint32_t friendnum);

#endif /* end of include guard: CHAT_H */

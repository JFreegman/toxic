/*  chat_commands.h
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

#ifndef _chat_commands_h
#define _chat_commands_h

#include "windows.h"
#include "toxic.h"

void cmd_groupinvite(WINDOW *, ToxWindow *, Tox *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_join_group(WINDOW *, ToxWindow *, Tox *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_savefile(WINDOW *, ToxWindow *, Tox *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_sendfile(WINDOW *, ToxWindow *, Tox *, int argc, char (*argv)[MAX_STR_SIZE]);

#ifdef _SUPPORT_AUDIO
void cmd_call(WINDOW *, ToxWindow *, Tox *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_answer(WINDOW *, ToxWindow *, Tox *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_reject(WINDOW *, ToxWindow *, Tox *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_hangup(WINDOW *, ToxWindow *, Tox *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_cancel(WINDOW *, ToxWindow *, Tox *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_ccur_device(WINDOW *, ToxWindow *, Tox *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_mute(WINDOW *, ToxWindow *, Tox *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_sense(WINDOW *, ToxWindow *, Tox *, int argc, char (*argv)[MAX_STR_SIZE]);
#endif /* _SUPPORT_AUDIO */

#endif /* #define _chat_commands_h */

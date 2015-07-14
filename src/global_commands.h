/*  global_commands.h
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

#ifndef GLOBAL_COMMANDS_H
#define GLOBAL_COMMANDS_H

#include "windows.h"
#include "toxic.h"

void cmd_accept(WINDOW *, ToxWindow *, Tox *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_add(WINDOW *, ToxWindow *, Tox *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_avatar(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_clear(WINDOW *, ToxWindow *, Tox *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_connect(WINDOW *, ToxWindow *, Tox *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_decline(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_groupchat(WINDOW *, ToxWindow *, Tox *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_join(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_log(WINDOW *, ToxWindow *, Tox *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_myid(WINDOW *, ToxWindow *, Tox *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_nick(WINDOW *, ToxWindow *, Tox *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_note(WINDOW *, ToxWindow *, Tox *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_prompt_help(WINDOW *, ToxWindow *, Tox *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_quit(WINDOW *, ToxWindow *, Tox *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_requests(WINDOW *, ToxWindow *, Tox *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_status(WINDOW *, ToxWindow *, Tox *, int argc, char (*argv)[MAX_STR_SIZE]);

void cmd_add_helper(ToxWindow *self, Tox *m, const char *id_bin, const char *msg);

#ifdef AUDIO
void cmd_list_devices(WINDOW *, ToxWindow *, Tox *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_change_device(WINDOW *, ToxWindow *, Tox *, int argc, char (*argv)[MAX_STR_SIZE]);
#endif /* AUDIO */

#endif /* #define GLOBAL_COMMANDS_H */

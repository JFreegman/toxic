/*  conference_commands.h
 *
 *  Copyright (C) 2014-2024 Toxic All Rights Reserved.
 *
 *  This file is part of Toxic. Toxic is free software licensed
 *  under the GNU General Public License 3.0.
 */

#ifndef CONFERENCE_COMMANDS_H
#define CONFERENCE_COMMANDS_H

#include "toxic.h"
#include "windows.h"

void cmd_conference_chatid(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_conference_invite(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_conference_set_title(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_enable_audio(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_conference_mute(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_conference_sense(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_conference_push_to_talk(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);

#endif /* CONFERENCE_COMMANDS_H */


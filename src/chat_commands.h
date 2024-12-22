/*  chat_commands.h
 *
 *  Copyright (C) 2014-2024 Toxic All Rights Reserved.
 *
 *  This file is part of Toxic. Toxic is free software licensed
 *  under the GNU General Public License 3.0.
 */

#ifndef CHAT_COMMANDS_H
#define CHAT_COMMANDS_H

#include "toxic.h"
#include "windows.h"

void cmd_autoaccept_files(WINDOW *window, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_cancelfile(WINDOW *window, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_invite_to_conference(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_conference_join(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_group_accept(WINDOW *window, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_invite_to_group(WINDOW *window, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_game_join(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_savefile(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_sendfile(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);

#ifdef AUDIO
void cmd_call(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_answer(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_reject(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_hangup(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_cancel(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_ccur_device(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_mute(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_sense(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_bitrate(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
#endif /* AUDIO */

#ifdef VIDEO
void cmd_vcall(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_video(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_res(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
#endif /* VIDEO */

#endif /* CHAT_COMMANDS_H */

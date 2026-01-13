/*  global_commands.h
 *
 *  Copyright (C) 2014-2026 Toxic All Rights Reserved.
 *
 *  This file is part of Toxic. Toxic is free software licensed
 *  under the GNU General Public License 3.0.
 */

#ifndef GLOBAL_COMMANDS_H
#define GLOBAL_COMMANDS_H

#include "toxic.h"
#include "windows.h"

void cmd_accept(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_add(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_avatar(WINDOW *window, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_clear(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_color(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_conference(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_connect(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_decline(WINDOW *window, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_groupchat(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_join(WINDOW *window, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_log(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_myid(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
#ifdef QRCODE
void cmd_myqr(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE]);
#endif /* QRCODE */
void cmd_nick(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_note(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_nospam(WINDOW *window, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_prompt_help(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_quit(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_requests(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_status(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);

void cmd_add_helper(ToxWindow *self, Toxic *, const char *id_bin, const char *msg);

#ifdef AUDIO
void cmd_list_devices(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_change_device(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
#endif /* AUDIO */

#ifdef VIDEO
void cmd_list_video_devices(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_change_video_device(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
#endif /* VIDEO */

#ifdef PYTHON
void cmd_run(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
#endif /* PYTHON */

#ifdef GAMES
void cmd_game(WINDOW *window, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
#endif /* GAMES */

#endif /* GLOBAL_COMMANDS_H */

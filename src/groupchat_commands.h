/*  groupchat_commands.h
 *
 *  Copyright (C) 2020-2026 Toxic All Rights Reserved.
 *
 *  This file is part of Toxic. Toxic is free software licensed
 *  under the GNU General Public License 3.0.
 */

#ifndef GROUPCHAT_COMMANDS_H
#define GROUPCHAT_COMMANDS_H

#include "windows.h"
#include "toxic.h"

void cmd_chatid(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_disconnect(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_group_invite(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_group_nick(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_ignore(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_kick(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_list(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_mod(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_prune(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_set_passwd(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_set_peerlimit(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_set_privacy(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_set_voice(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_set_topic_lock(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_rejoin(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_set_topic(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_silence(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_unsilence(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_unignore(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_unmod(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);
void cmd_whois(WINDOW *, ToxWindow *, Toxic *, int argc, char (*argv)[MAX_STR_SIZE]);

#endif  /* GROUPCHAT_COMMANDS_H */

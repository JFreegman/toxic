/*  api.h
 *
 *  Copyright (C) 2017 Jakob Kreuze <jakob@memeware.net>
 *  Copyright (C) 2017-2026 Toxic All Rights Reserved.
 *
 *  This file is part of Toxic. Toxic is free software licensed
 *  under the GNU General Public License 3.0.
 */

#ifndef API_H
#define API_H

#include "friendlist.h"
#include "init_queue.h"
#include "windows.h"

void api_display(const char *const msg);
FriendsList api_get_friendslist(void);
char *api_get_nick(void);
Tox_User_Status api_get_status(void);
char *api_get_status_message(void);
void api_send(const char *msg);
void api_execute(const char *input, int mode);
int do_plugin_command(int num_args, char (*args)[MAX_STR_SIZE]);
int num_registered_handlers(void);
int help_max_width(void);
void draw_handler_help(WINDOW *win);
void invoke_autoruns(ToxWindow *self, const char *autorun_path, Init_Queue *init_q);
void cmd_run(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE]);

#endif /* API_H */

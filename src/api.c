/*  api.c
 *
 *
 *  Copyright (C) 2017 Jakob Kreuze <jakob@memeware.net>
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

#include <dirent.h>
#include <stdint.h>

#include <tox/tox.h>

#include "execute.h"
#include "friendlist.h"
#include "line_info.h"
#include "message_queue.h"
#include "misc_tools.h"
#include "settings.h"
#include "toxic_strings.h"
#include "windows.h"

#ifdef PYTHON
#include "python_api.h"

Tox              *user_tox;
static WINDOW    *cur_window;
static ToxWindow *self_window;

extern FriendsList Friends;
extern struct user_settings *user_settings;

void api_display(const char *const msg)
{
    if (msg == NULL) {
        return;
    }

    self_window = get_active_window();
    line_info_add(self_window, NULL, NULL, NULL, SYS_MSG, 0, 0, msg);
}

FriendsList api_get_friendslist(void)
{
    return Friends;
}

char *api_get_nick(void)
{
    size_t   len  = tox_self_get_name_size(user_tox);
    uint8_t *name = malloc(len + 1);

    if (name == NULL) {
        return NULL;
    }

    tox_self_get_name(user_tox, name);
    name[len] = '\0';
    return (char *) name;
}

Tox_User_Status api_get_status(void)
{
    return tox_self_get_status(user_tox);
}

char *api_get_status_message(void)
{
    size_t   len    = tox_self_get_status_message_size(user_tox);
    uint8_t *status = malloc(len + 1);

    if (status == NULL) {
        return NULL;
    }

    tox_self_get_status_message(user_tox, status);
    status[len] = '\0';
    return (char *) status;
}

void api_send(const char *msg)
{
    if (msg == NULL || self_window->chatwin->cqueue == NULL) {
        return;
    }

    char *name = api_get_nick();
    char  timefrmt[TIME_STR_SIZE];

    if (name == NULL) {
        return;
    }

    self_window = get_active_window();
    get_time_str(timefrmt, sizeof(timefrmt));

    strncpy((char *) self_window->chatwin->line, msg, sizeof(self_window->chatwin->line));
    add_line_to_hist(self_window->chatwin);
    int id = line_info_add(self_window, timefrmt, name, NULL, OUT_MSG, 0, 0, "%s", msg);
    cqueue_add(self_window->chatwin->cqueue, msg, strlen(msg), OUT_MSG, id);
    free(name);
}

void api_execute(const char *input, int mode)
{
    self_window = get_active_window();
    execute(cur_window, self_window, user_tox, input, mode);
}

int do_plugin_command(int num_args, char (*args)[MAX_STR_SIZE])
{
    return do_python_command(num_args, args);
}

int num_registered_handlers(void)
{
    return python_num_registered_handlers();
}

int help_max_width(void)
{
    return python_help_max_width();
}

void draw_handler_help(WINDOW *win)
{
    python_draw_handler_help(win);
}

void cmd_run(WINDOW *window, ToxWindow *self, Tox *m, int argc, char (*argv)[MAX_STR_SIZE])
{
    FILE       *fp;
    const char *error_str;

    cur_window  = window;
    self_window = self;

    if (argc != 1) {
        if (argc < 1) {
            error_str = "Path must be specified.";
        } else {
            error_str = "Only one argument allowed.";
        }

        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, error_str);
        return;
    }

    fp = fopen(argv[1], "r");

    if (fp == NULL) {
        error_str = "Path does not exist.";

        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, error_str);
        return;
    }

    run_python(fp, argv[1]);
    fclose(fp);
}

void invoke_autoruns(WINDOW *window, ToxWindow *self)
{
    char abspath_buf[PATH_MAX + 256];
    char err_buf[PATH_MAX + 128];

    if (user_settings->autorun_path[0] == '\0') {
        return;
    }

    DIR *d = opendir(user_settings->autorun_path);

    if (d == NULL) {
        snprintf(err_buf, sizeof(err_buf), "Autorun path does not exist: %s", user_settings->autorun_path);
        api_display(err_buf);
        return;
    }

    struct dirent *dir = NULL;

    cur_window  = window;

    self_window = self;

    while ((dir = readdir(d)) != NULL) {
        size_t path_len = strlen(dir->d_name);

        if (!strcmp(dir->d_name + path_len - 3, ".py")) {
            snprintf(abspath_buf, sizeof(abspath_buf), "%s%s", user_settings->autorun_path, dir->d_name);
            FILE *fp = fopen(abspath_buf, "r");

            if (fp == NULL) {
                snprintf(err_buf, sizeof(err_buf), "Invalid path: %s", abspath_buf);
                api_display(err_buf);
                continue;
            }

            run_python(fp, abspath_buf);
            fclose(fp);
        }
    }

    closedir(d);
}
#endif /* PYTHON */

/*  api.c
 *
 *  Copyright (C) 2017 Jakob Kreuze <jakob@memeware.net>
 *  Copyright (C) 2017-2024 Toxic All Rights Reserved.
 *
 *  This file is part of Toxic. Toxic is free software licensed
 *  under the GNU General Public License 3.0.
 */

#include "api.h"

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

Toxic            *user_toxic;
static WINDOW    *cur_window;
static ToxWindow *self_window;

void api_display(const char *const msg)
{
    if (msg == NULL) {
        return;
    }

    self_window = get_active_window(user_toxic->windows);

    if (self_window != NULL) {
        line_info_add(self_window, user_toxic->c_config, NULL, NULL, NULL, SYS_MSG, 0, 0, "%s", msg);
    }
}

FriendsList api_get_friendslist(void)
{
    return Friends;
}

char *api_get_nick(void)
{
    size_t   len  = tox_self_get_name_size(user_toxic->tox);
    uint8_t *name = malloc(len + 1);

    if (name == NULL) {
        return NULL;
    }

    tox_self_get_name(user_toxic->tox, name);
    name[len] = '\0';
    return (char *) name;
}

Tox_User_Status api_get_status(void)
{
    return tox_self_get_status(user_toxic->tox);
}

char *api_get_status_message(void)
{
    size_t   len    = tox_self_get_status_message_size(user_toxic->tox);
    uint8_t *status = malloc(len + 1);

    if (status == NULL) {
        return NULL;
    }

    tox_self_get_status_message(user_toxic->tox, status);
    status[len] = '\0';
    return (char *) status;
}

void api_send(const char *msg)
{
    if (msg == NULL || self_window->chatwin->cqueue == NULL) {
        return;
    }

    char *name = api_get_nick();

    if (name == NULL) {
        return;
    }

    self_window = get_active_window(user_toxic->windows);

    if (self_window == NULL) {
        return;
    }

    snprintf((char *) self_window->chatwin->line, sizeof(self_window->chatwin->line), "%s", msg);
    add_line_to_hist(self_window->chatwin);
    const int id = line_info_add(self_window, user_toxic->c_config, true, name, NULL, OUT_MSG, 0, 0, "%s", msg);
    cqueue_add(self_window->chatwin->cqueue, msg, strlen(msg), OUT_MSG, id);
    free(name);
}

void api_execute(const char *input, int mode)
{
    self_window = get_active_window(user_toxic->windows);

    if (self_window != NULL) {
        execute(cur_window, self_window, user_toxic, input, mode);
    }
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

void cmd_run(WINDOW *window, ToxWindow *self, Toxic *toxic, int argc, char (*argv)[MAX_STR_SIZE])
{
    if (toxic == NULL || self == NULL) {
        return;
    }

    const Client_Config *c_config = toxic->c_config;

    FILE       *fp;

    cur_window  = window;
    self_window = self;

    if (argc != 1) {
        const char *error_str;

        if (argc < 1) {
            error_str = "Path must be specified.";
        } else {
            error_str = "Only one argument allowed.";
        }

        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "%s", error_str);
        return;
    }

    fp = fopen(argv[1], "r");

    if (fp == NULL) {
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Path does not exist.");
        return;
    }

    run_python(fp, argv[1]);
    fclose(fp);
}

void invoke_autoruns(ToxWindow *self, const char *autorun_path)
{
    char abspath_buf[PATH_MAX + 256];
    char err_buf[PATH_MAX + 128];

    if (autorun_path == NULL) {
        return;
    }

    if (string_is_empty(autorun_path)) {
        return;
    }

    DIR *d = opendir(autorun_path);

    if (d == NULL) {
        snprintf(err_buf, sizeof(err_buf), "Autorun path does not exist: %s", autorun_path);
        api_display(err_buf);
        return;
    }

    struct dirent *dir = NULL;

    cur_window  = self->chatwin->history;

    self_window = self;

    while ((dir = readdir(d)) != NULL) {
        size_t path_len = strlen(dir->d_name);

        if (!strcmp(dir->d_name + path_len - 3, ".py")) {
            snprintf(abspath_buf, sizeof(abspath_buf), "%s%s", autorun_path, dir->d_name);
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

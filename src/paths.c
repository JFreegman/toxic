/*  paths.c
 *
 *  Copyright (C) 2014-2026 Toxic All Rights Reserved.
 *
 *  This file is part of Toxic. Toxic is free software licensed
 *  under the GNU General Public License 3.0.
 */

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>
#include <stdio.h>

#include "paths.h"

#ifndef NSS_BUFLEN_PASSWD
#define NSS_BUFLEN_PASSWD 4096
#endif

static char *get_home_dir_env(void)
{
    struct passwd pwd;
    struct passwd *pwdbuf = NULL;
    char buf[NSS_BUFLEN_PASSWD];

    const int rc = getpwuid_r(getuid(), &pwd, buf, NSS_BUFLEN_PASSWD, &pwdbuf);
    const char *hmstr = (rc == 0 && pwdbuf != NULL) ? pwd.pw_dir : getenv("HOME");

    if (hmstr == NULL) {
        return NULL;
    }

    return strdup(hmstr);
}

Paths *paths_init(void)
{
    Paths *paths = calloc(1, sizeof(Paths));

    if (paths == NULL) {
        return NULL;
    }

    paths->home_dir = get_home_dir_env();

    if (paths->home_dir == NULL) {
        fprintf(stderr, "Failed to get home directory\n");
    }

    const char *xdg = getenv("XDG_CONFIG_HOME");

    if (xdg != NULL) {
        paths->xdg_config_home = strdup(xdg);
    }

    const char *sty = getenv("STY");

    if (sty != NULL) {
        paths->screen_socket = strdup(sty);
    }

    const char *tmux = getenv("TMUX");

    if (tmux != NULL) {
        paths->tmux_socket = strdup(tmux);
    }

    return paths;
}

void paths_free(Paths *paths)
{
    if (paths == NULL) {
        return;
    }

    free(paths->home_dir);
    free(paths->xdg_config_home);
    free(paths->screen_socket);
    free(paths->tmux_socket);
    free(paths);
}

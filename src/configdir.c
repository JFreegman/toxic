/*  configdir.c
 *
 *  Copyright (C) 2014-2026 Toxic All Rights Reserved.
 *
 *  This file is part of Toxic. Toxic is free software licensed
 *  under the GNU General Public License 3.0.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "configdir.h"
#include "misc_tools.h"
#include "toxic.h"

/* get the user's home directory. */
void get_home_dir(const Paths *paths, char *home, int size)
{
    if (paths && paths->home_dir) {
        snprintf(home, size, "%s", paths->home_dir);
    } else {
        if (size > 0) {
            home[0] = '\0';
        }
    }
}

/**
 * @brief Get the user's config directory.
 *
 * This is without a trailing slash. Resulting string must be freed.
 *
 * @return The users config dir or NULL on error.
 */
char *get_user_config_dir(const Paths *paths)
{
    char home[NSS_BUFLEN_PASSWD] = {0};
    get_home_dir(paths, home, sizeof(home));

    char *user_config_dir = NULL;
    size_t len = 0;

# if defined(__APPLE__)
    len = strlen(home) + strlen("/Library/Application Support") + 1;
    user_config_dir = malloc(len);

    if (user_config_dir == NULL) {
        return NULL;
    }

    snprintf(user_config_dir, len, "%s/Library/Application Support", home);
# else /* __APPLE__ */

    const char *tmp = paths ? paths->xdg_config_home : NULL;

    if (tmp == NULL) {
        len = strlen(home) + strlen("/.config") + 1;
        user_config_dir = malloc(len);

        if (user_config_dir == NULL) {
            return NULL;
        }

        snprintf(user_config_dir, len, "%s/.config", home);
    } else {
        user_config_dir = strdup(tmp);
    }

# endif /* __APPLE__ */

    return user_config_dir;
}

/* Creates the config and chatlog directories.
 *
 * Returns 0 on success.
 * Returns -1 on failure.
 */
int create_user_config_dirs(char *path)
{
    struct stat buf;
    int mkdir_err = mkdir(path, 0700);

    if (mkdir_err && (errno != EEXIST || stat(path, &buf) || !S_ISDIR(buf.st_mode))) {
        return -1;
    }

    char *fullpath = malloc(strlen(path) + strlen(CONFIGDIR) + 1);

    if (fullpath == NULL) {
        return -1;
    }

    char *logpath = malloc(strlen(path) + strlen(LOGDIR) + 1);

    if (logpath == NULL) {
        free(fullpath);
        return -1;
    }

    strcpy(fullpath, path);
    strcat(fullpath, CONFIGDIR);

    strcpy(logpath, path);
    strcat(logpath, LOGDIR);

    mkdir_err = mkdir(fullpath, 0700);

    if (mkdir_err && (errno != EEXIST || stat(fullpath, &buf) || !S_ISDIR(buf.st_mode))) {
        free(fullpath);
        free(logpath);
        return -1;
    }

    mkdir_err = mkdir(logpath, 0700);

    if (mkdir_err && (errno != EEXIST || stat(logpath, &buf) || !S_ISDIR(buf.st_mode))) {
        free(fullpath);
        free(logpath);
        return -1;
    }

    free(logpath);
    free(fullpath);
    return 0;
}

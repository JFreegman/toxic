/*  configdir.c
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

#include <errno.h>
#include <pwd.h>
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
void get_home_dir(char *home, int size)
{
    struct passwd pwd;
    struct passwd *pwdbuf;
    const char *hmstr;
    char buf[NSS_BUFLEN_PASSWD];

    int rc = getpwuid_r(getuid(), &pwd, buf, NSS_BUFLEN_PASSWD, &pwdbuf);

    if (rc == 0) {
        hmstr = pwd.pw_dir;
    } else {
        hmstr = getenv("HOME");

        if (hmstr == NULL) {
            return;
        }

        snprintf(buf, sizeof(buf), "%s", hmstr);
        hmstr = buf;
    }

    snprintf(home, size, "%s", hmstr);
}

/**
 * @brief Get the user's config directory.
 *
 * This is without a trailing slash. Resulting string must be freed.
 *
 * @return The users config dir or NULL on error.
 */
char *get_user_config_dir(void)
{
    char home[NSS_BUFLEN_PASSWD] = {0};
    get_home_dir(home, sizeof(home));

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

    const char *tmp = getenv("XDG_CONFIG_HOME");

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
    char *logpath = malloc(strlen(path) + strlen(LOGDIR) + 1);

    if (fullpath == NULL || logpath == NULL) {
        exit_toxic_err("failed in load_data_structures", FATALERR_MEMORY);
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

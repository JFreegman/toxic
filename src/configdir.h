/*  configdir.h
 *
 *  Copyright (C) 2014-2024 Toxic All Rights Reserved.
 *
 *  This file is part of Toxic. Toxic is free software licensed
 *  under the GNU General Public License 3.0.
 */

#ifndef CONFIGDIR_H
#define CONFIGDIR_H

#ifndef NSS_BUFLEN_PASSWD
#define NSS_BUFLEN_PASSWD 4096
#endif

#define CONFIGDIR "/tox/"
#define LOGDIR "/tox/chatlogs/"

#ifndef S_ISDIR
#define S_ISDIR(mode)  (((mode) & S_IFMT) == S_IFDIR)
#endif

/**
 * @brief Get the user's config directory.
 *
 * This is without a trailing slash. Resulting string must be freed.
 *
 * @return The users config dir or NULL on error.
 */
char *get_user_config_dir(void);

/* get the user's home directory. */
void get_home_dir(char *home, int size);

/* Creates the config and chatlog directories.
 *
 * Returns 0 on success.
 * Returns -1 on failure.
 */
int create_user_config_dirs(char *path);

#endif /* CONFIGDIR_H */

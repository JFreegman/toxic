/*  configdir.h
 *
 *
 *  Copyright (C) 2024 Toxic All Rights Reserved.
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

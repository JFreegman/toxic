/*  paths.h
 *
 *  Copyright (C) 2014-2026 Toxic All Rights Reserved.
 *
 *  This file is part of Toxic. Toxic is free software licensed
 *  under the GNU General Public License 3.0.
 */

#ifndef PATHS_H
#define PATHS_H

typedef struct Paths {
    char *home_dir;
    char *xdg_config_home;
    char *screen_socket; /* STY */
    char *tmux_socket;   /* TMUX */
} Paths;

/**
 * @brief Initialize paths by reading environment variables.
 *
 * @return A new Paths object, or NULL on error.
 */
Paths *paths_init(void);

/**
 * @brief Free the Paths object.
 *
 * @param paths The object to free.
 */
void paths_free(Paths *paths);

#endif /* PATHS_H */

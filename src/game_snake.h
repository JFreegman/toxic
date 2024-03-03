/*  game_snake.h
 *
 *  Copyright (C) 2020-2024 Toxic All Rights Reserved.
 *
 *  This file is part of Toxic. Toxic is free software licensed
 *  under the GNU General Public License 3.0.
 */

#ifndef GAME_SNAKE
#define GAME_SNAKE

#include "game_base.h"

/**
 * Initializes a game of snake.
 *
 * `is_online` should be true if the game is multipayer.
 * `self_host` should be true for the one sending the game invite, and
 *    false for the one who received the game invite.
 */
int snake_initialize(GameData *game, bool is_online, bool self_host);

#endif // GAME_SNAKE

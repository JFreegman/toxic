/*  game_snake.h
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

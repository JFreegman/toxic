/*  game_chess.h
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

#ifndef GAME_CHESS
#define GAME_CHESS

#include "game_base.h"

/*
 * Initializes chess game state.
 *
 * If `self_host` is false, this indicates that we were invited to the game.
 *
 * `init_data` of length `length` is the game data sent to us by the inviter
 * needed to start the game.
 *
 * Return 0 on success.
 * Return -1 if window is too small.
 * Return -2 on network related error.
 * Return -3 on other error.
 */
int chess_initialize(GameData *game, const uint8_t *init_data, size_t length, bool self_host);

#endif // GAME_CHESS


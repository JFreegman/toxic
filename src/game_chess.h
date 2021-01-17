/*  game_chess.h
 *
 *
 *  Copyright (C) 2020 Toxic All Rights Reserved.
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
 * If `init_data` is non-null, this indicates that we were invited to the game.
 *
 * If we're the inviter, we send an invite packet after initialization. If we're the
 * invitee, we send a handshake response packet to the inviter.
 *
 * Return 0 on success.
 * Return -1 if window is too small.
 * Return -2 on network related error.
 * Return -3 on other error.
 */
int chess_initialize(GameData *game, const uint8_t *init_data, size_t length);

#endif // GAME_CHESS


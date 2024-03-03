/*  game_chess.h
 *
 *  Copyright (C) 2020-2024 Toxic All Rights Reserved.
 *
 *  This file is part of Toxic. Toxic is free software licensed
 *  under the GNU General Public License 3.0.
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


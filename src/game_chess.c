/*  game_chess.c
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game_base.h"
#include "game_util.h"
#include "game_chess.h"
#include "misc_tools.h"

#define CHESS_WHITE_SQUARE_COLOUR WHITE_GREEN
#define CHESS_BLACK_SQUARE_COLOUR WHITE_BLUE

#define CHESS_BOARD_ROWS    8
#define CHESS_BOARD_COLUMNS 8
#define CHESS_TILE_SIZE_X 4
#define CHESS_TILE_SIZE_Y 2
#define CHESS_SQUARES (CHESS_BOARD_ROWS * CHESS_BOARD_COLUMNS)
#define CHESS_MAX_MESSAGE_SIZE 64

typedef struct ChessCoords {
    char         L;
    int          N;
} ChessCoords;

typedef enum ChessColour {
    White = 0u,
    Black,
} ChessColour;

typedef enum ChessStatus {
    Playing = 0u,
    Checkmate,
    Stalemate,
} ChessStatus;

typedef enum PieceType {
    Pawn = 0u,
    Rook,
    Knight,
    Bishop,
    King,
    Queen,
    NoPiece,
} PieceType;

typedef struct Piece {
    char        display_char;
    ChessColour colour;
    PieceType   type;
} Piece;

typedef struct Tile {
    Piece       piece;
    Coords      coords;       // xy position on window
    ChessCoords chess_coords; // chess notation pair
    int         colour;       // display colour (not to be confused with White/Black ChessColour)
} Tile;

typedef struct Board {
    Tile        tiles[CHESS_SQUARES];
    int         x_right_bound;
    int         x_left_bound;
    int         y_top_bound;
    int         y_bottom_bound;
} Board;

typedef struct Player {
    Tile        *holding_tile;

    ChessColour colour;

    bool        can_castle_qs;
    bool        can_castle_ks;
    bool        in_check;

    Tile        *en_passant;              // the tile holding the pawn that passed us
    int          en_passant_move_number;  // the move number the last en passant was on

    Piece       captured[CHESS_SQUARES];
    size_t      number_captured;
} Player;

typedef struct ChessState {
    Player      self;
    Player      other;

    Board       board;
    int         curs_x;
    int         curs_y;

    char        status_message[CHESS_MAX_MESSAGE_SIZE + 1];
    size_t      message_length;

    bool        black_to_move;
    size_t      move_number;
    ChessStatus status;
} ChessState;

static const char Board_Letters[] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'};

#define CHESS_NUM_BOARD_LETTERS (sizeof(Board_Letters) / sizeof(char))

static int chess_get_letter_index(char letter)
{
    for (int i = 0; i < CHESS_NUM_BOARD_LETTERS; ++i) {
        if (Board_Letters[i] == letter) {
            return i;
        }
    }

    return -1;
}

static void chess_set_piece(Piece *piece, PieceType type, ChessColour colour)
{
    piece->type = type;
    piece->colour = colour;

    switch (type) {
        case Pawn:
            piece->display_char = 'o';
            break;

        case Bishop:
            piece->display_char = 'B';
            break;

        case Rook:
            piece->display_char = 'R';
            break;

        case Knight:
            piece->display_char = 'N';
            break;

        case King:
            piece->display_char = 'K';
            break;

        case Queen:
            piece->display_char = 'Q';
            break;

        default:
            piece->display_char = '?';
            break;
    }
}

static void chess_set_status_message(ChessState *state, const char *message, size_t length)
{
    if (length > CHESS_MAX_MESSAGE_SIZE) {
        return;
    }

    memcpy(state->status_message, message, length);
    state->status_message[length] = 0;
    state->message_length = length;
}

/*
 * Return true if `pair_a` is the same as `pair_b`.
 */
static bool chess_chess_coords_overlap(const ChessCoords *pair_a, const ChessCoords *pair_b)
{
    return pair_a->L == pair_b->L && pair_a->N == pair_b->N;
}

/*
 * Return the player whose turn it currently is not.
 */
static Player *chess_get_other_player(ChessState *state)
{
    if (state->black_to_move) {
        return state->self.colour == Black ? &state->other : &state->self;
    } else {
        return state->self.colour == White ? &state->other : &state->self;
    }
}

/*
 * Return the player whose turn it currently is.
 */
static Player *chess_get_player_to_move(ChessState *state)
{
    if (state->black_to_move) {
        return state->self.colour == Black ? &state->self : & state->other;
    } else {
        return state->self.colour == Black ? &state->other : & state->self;
    }
}

/*
 * Puts coordinates associated with tile at x y coordinates in `chess_coords`.
 *
 * Return 0 on success.
 * Return -1 if coordinates are out of bounds.
 */
static int chess_get_chess_coords(const Board *board, int x, int y, ChessCoords *chess_coords, bool self_is_white)
{
    if (x < board->x_left_bound || x > board->x_right_bound || y < board->y_top_bound || y > board->y_bottom_bound) {
        return -1;
    }

    size_t idx = (x - board->x_left_bound) / CHESS_TILE_SIZE_X;

    if (idx >= CHESS_NUM_BOARD_LETTERS) {
        return -1;
    }

    if (self_is_white) {
        chess_coords->L = Board_Letters[idx];
        chess_coords->N = ((board->y_bottom_bound + 1) - y) / CHESS_TILE_SIZE_Y;
    } else {
        chess_coords->L = Board_Letters[7 - idx];
        chess_coords->N = 8 - (((board->y_bottom_bound + 1) - y) / CHESS_TILE_SIZE_Y) + 1;
    }

    return 0;
}

/*
 * Returns the tile located at given coordinates.
 */
static Tile *chess_get_tile(ChessState *state, int x, int y)
{
    Board *board = &state->board;

    ChessCoords pair;

    if (chess_get_chess_coords(board, x, y, &pair, state->self.colour == White) == -1) {
        return NULL;
    }

    for (size_t i = 0; i < CHESS_SQUARES; ++i) {
        Tile *tile = &board->tiles[i];

        if (tile->chess_coords.N == pair.N && tile->chess_coords.L == pair.L) {
            return tile;
        }
    }

    return NULL;
}

/*
 * Returns tile associated with `chess_coords`.
 */
static Tile *chess_get_tile_at_chess_coords(Board *board, const ChessCoords *chess_coords)
{
    for (size_t i = 0; i < CHESS_SQUARES; ++i) {
        Tile *tile = &board->tiles[i];

        if (tile->chess_coords.N == chess_coords->N && tile->chess_coords.L == chess_coords->L) {
            return tile;
        }
    }

    return NULL;
}

/*
 * Puts the absolute difference between `from` and `to` chess coordinates in `l_diff` and `n_diff`.
 *
 * Return 0 on success.
 * Return -1 on failure.
 */
static int chess_get_chess_coords_diff(const Tile *from, const Tile *to, int *l_diff, int *n_diff)
{
    int from_letter_idx = chess_get_letter_index(from->chess_coords.L);
    int to_letter_idx = chess_get_letter_index(to->chess_coords.L);

    if (from_letter_idx == -1 || to_letter_idx == -1) {
        return -1;
    }

    *l_diff = abs(from_letter_idx - to_letter_idx);
    *n_diff = abs(from->chess_coords.N - to->chess_coords.N);

    return 0;
}

/*
 * Return true if `piece` can occupy `tile`.
 */
static bool chess_piece_can_occupy_tile(const Piece *piece, const Tile *tile)
{
    return tile->piece.colour != piece->colour || tile->piece.type == NoPiece;
}

/*
 * Return true if all squares in a horizontal or vertical line between `from` and `to`
 * are vacant, excluding each square respectively.
 */
static bool chess_path_line_clear(Board *board, const Tile *from, const Tile *to, int l_diff, int n_diff)
{
    if (l_diff < 0 || n_diff < 0) {
        return false;
    }

    if (l_diff != 0 && n_diff != 0) {
        return false;
    }

    ChessCoords chess_coords;
    size_t start;
    size_t end;

    if (l_diff == 0) {
        start = 1 + MIN(from->chess_coords.N, to->chess_coords.N);
        end = start + n_diff - 1;
        chess_coords.L = from->chess_coords.L;
    } else {
        int from_idx = chess_get_letter_index(from->chess_coords.L);
        int to_idx = chess_get_letter_index(to->chess_coords.L);

        if (to_idx == -1 || from_idx == -1) {
            return false;
        }

        start = 1 + MIN(from_idx, to_idx);
        end = start + l_diff - 1;
        chess_coords.N = from->chess_coords.N;
    }

    for (size_t i = start; i < end; ++i) {
        if (l_diff == 0) {
            chess_coords.N = i;
        } else {
            if (i >= CHESS_NUM_BOARD_LETTERS) {
                return false;
            }

            chess_coords.L = Board_Letters[i];
        }

        Tile *tile = chess_get_tile_at_chess_coords(board, &chess_coords);

        if (tile == NULL) {
            return false;
        }

        if (tile->piece.type != NoPiece) {
            return false;
        }
    }

    return true;
}

/*
 * Return true if all tiles in a diagonal line between `from` and `to` are
 * unoccupied, excluding each respective tile.
 */
static bool chess_path_diagonal_clear(Board *board, const Tile *from, const Tile *to, int l_diff, int n_diff)
{
    if (l_diff < 0 || n_diff < 0) {
        return false;
    }

    if (l_diff != n_diff || l_diff == 0) {
        return false;
    }

    size_t start = 1 + MIN(from->chess_coords.N, to->chess_coords.N);
    size_t end = start + n_diff - 1;

    // we're caluclating from south-east to north-west, or from south-west to north-east
    bool left_diag = (from->chess_coords.N > to->chess_coords.N && from->chess_coords.L < to->chess_coords.L)
                     || (from->chess_coords.N < to->chess_coords.N && from->chess_coords.L > to->chess_coords.L);

    size_t from_l_idx = chess_get_letter_index(from->chess_coords.L);
    size_t to_l_idx = chess_get_letter_index(to->chess_coords.L);
    size_t start_l_idx = left_diag ? MAX(from_l_idx, to_l_idx) - 1 : MIN(from_l_idx, to_l_idx) + 1;

    if (start_l_idx == -1) {
        return -1;
    }

    ChessCoords chess_coords;

    for (size_t i = start; i < end; ++i) {
        chess_coords.N = i;

        if (start_l_idx >= CHESS_NUM_BOARD_LETTERS) {
            return false;
        }

        chess_coords.L = Board_Letters[start_l_idx];

        Tile *tile = chess_get_tile_at_chess_coords(board, &chess_coords);

        if (tile == NULL) {
            return false;
        }

        if (tile->piece.type != NoPiece) {
            return false;
        }

        start_l_idx = left_diag ? start_l_idx - 1 : start_l_idx + 1;
    }

    return true;
}

/*
 * Removes en passant'd pawn and resets player's en passant flag.
 *
 * Should be called after every successful move.
 */
static void chess_player_clear_en_passant(Player *player)
{
    if (player->en_passant_move_number == -1) {
        chess_set_piece(&player->en_passant->piece, NoPiece, White);
        player->en_passant = NULL;
    }

    player->en_passant_move_number = 0;
}

/*
 * Flags a pawn at `to` for a possible en passant take for other player.
 */
static void chess_pawn_en_passant_flag(ChessState *state, Tile *to)
{
    Player *other = chess_get_other_player(state);
    other->en_passant = to;
    other->en_passant_move_number = state->move_number;
}

/*
 * Return true and flag opposing pawn to be removed if `to` is a valid en passant move.
 */
static bool chess_pawn_en_passant_move(ChessState *state, Player *player, const Tile *to)
{
    if (player->en_passant == NULL || player->en_passant_move_number <= 0) {
        return false;
    }

    int delta = 0;

    if (player->colour == White) {
        if (player->en_passant_move_number != state->move_number) {
            return false;
        }

        delta = 1;
    } else {
        if (player->en_passant_move_number != state->move_number - 1) {
            return false;
        }

        delta = -1;
    }

    if (player->en_passant->piece.type == Pawn && to->chess_coords.N == player->en_passant->chess_coords.N + delta
            && to->chess_coords.L == player->en_passant->chess_coords.L) {
        player->en_passant_move_number = -1;  // flag opponent's pawn to be removed after move is validated
        return true;
    }

    return false;
}

static bool chess_valid_pawn_move(ChessState *state, Tile *from,  Tile *to)
{
    Board *board = &state->board;

    Piece from_piece = from->piece;
    Piece to_piece = to->piece;

    // Can't go backwards
    if (from_piece.colour == Black && from->chess_coords.N <= to->chess_coords.N) {
        return false;
    }

    if (from_piece.colour == White && from->chess_coords.N >= to->chess_coords.N) {
        return false;
    }

    int l_diff;
    int n_diff;

    if (chess_get_chess_coords_diff(from, to, &l_diff, &n_diff) == -1) {
        return false;
    }

    // can't move more than two spaces forward or one space diagonally
    if (n_diff < 1 || n_diff > 2 || l_diff > 1) {
        return false;
    }

    // If moving two spaces vertically it must be from starting position
    if (n_diff == 2) {
        if (l_diff != 0) {
            return false;
        }

        if (from_piece.colour == Black && from->chess_coords.N != 7) {
            return false;
        }

        if (from_piece.colour == White && from->chess_coords.N != 2) {
            return false;
        }

        if (to_piece.type != NoPiece) {
            return false;
        }
    }

    // if moving diagonally, to square must contain an enemy piece or have a valid en passant
    if (l_diff == 1) {
        Player *self = chess_get_player_to_move(state);

        if (chess_pawn_en_passant_move(state, self, to)) {
            return true;
        }

        bool ret = to_piece.type != NoPiece && to_piece.colour != from->piece.colour;

        if (ret && (to->chess_coords.N == 1 || to->chess_coords.N == 8)) {  // promote to queen
            chess_set_piece(&from->piece, Queen, from->piece.colour);
        }

       return ret;
    }

    if (to_piece.type != NoPiece) {
        return false;
    }

    bool ret = chess_path_line_clear(board, from, to, l_diff, n_diff);

    if (ret && n_diff == 2) {
        chess_pawn_en_passant_flag(state, to);
    } else if (ret && (to->chess_coords.N == 1 || to->chess_coords.N == 8)) {
        chess_set_piece(&from->piece, Queen, from->piece.colour);
    }

    return ret;
}

static bool chess_valid_rook_move(Board *board, const Tile *from, const Tile *to)
{
    int l_diff;
    int n_diff;

    if (chess_get_chess_coords_diff(from, to, &l_diff, &n_diff) == -1) {
        return false;
    }

    if (!chess_path_line_clear(board, from, to, l_diff, n_diff)) {
        return false;
    }

    if (!chess_piece_can_occupy_tile(&from->piece, to)) {
        return false;
    }

    if (from->chess_coords.N != to->chess_coords.N && from->chess_coords.L != to->chess_coords.L) {
        return false;
    }

    return true;
}

static bool chess_valid_knight_move(const Tile *from, const Tile *to)
{
    if (!chess_piece_can_occupy_tile(&from->piece, to)) {
        return false;
    }

    int l_diff;
    int n_diff;

    if (chess_get_chess_coords_diff(from, to, &l_diff, &n_diff) == -1) {
        return false;
    }

    if (!((l_diff == 2 && n_diff == 1) || (l_diff == 1 && n_diff == 2))) {
        return false;
    }

    return true;
}

static bool chess_valid_bishop_move(Board *board, const Tile *from, const Tile *to)
{
    if (!chess_piece_can_occupy_tile(&from->piece, to)) {
        return false;
    }

    int l_diff;
    int n_diff;

    if (chess_get_chess_coords_diff(from, to, &l_diff, &n_diff) == -1) {
        return false;
    }

    return chess_path_diagonal_clear(board, from, to, l_diff, n_diff);
}

static bool chess_valid_queen_move(Board *board, const Tile *from, const Tile *to)
{
    if (!chess_piece_can_occupy_tile(&from->piece, to)) {
        return false;
    }

    int l_diff;
    int n_diff;

    if (chess_get_chess_coords_diff(from, to, &l_diff, &n_diff) == -1) {
        return false;
    }

    if (l_diff != 0 && n_diff != 0) {
        return chess_path_diagonal_clear(board, from, to, l_diff, n_diff);
    }

    return chess_path_line_clear(board, from, to, l_diff, n_diff);
}

static bool chess_valid_king_move(const Tile *from, const Tile *to)
{
    if (!chess_piece_can_occupy_tile(&from->piece, to)) {
        return false;
    }

    int l_diff;
    int n_diff;

    if (chess_get_chess_coords_diff(from, to, &l_diff, &n_diff) == -1) {
        return false;
    }

    if (l_diff > 1 || n_diff > 1) {
        return false;
    }

    return true;
}

/*
 * Copies `piece_b` to `piece_a`.
 */
static void chess_copy_piece(Piece *piece_a, Piece *piece_b)
{
    memcpy(piece_a, piece_b, sizeof(Piece));
}

static bool chess_piece_attacking_square(ChessState *state, ChessColour colour, Tile *to);

/*
 * Return true if `player` is in check.
 */
static bool chess_player_in_check(ChessState *state, const Player *player)
{
    Board *board = &state->board;

    for (size_t i = 0; i < CHESS_SQUARES; ++i) {
        Tile *tile = &board->tiles[i];

        if (tile->piece.type == King && tile->piece.colour == player->colour) {
            return chess_piece_attacking_square(state, player->colour == Black ? White : Black, tile);
        }
    }

    return false;
}

/*
 * Return 1 if we can legally move `from` to `to`.
 * Return 0 if move is legal but we're in check.
 * Return -1 if move is not legal.
 *
 * If `player` is null we don't check if move puts player in check.
 */
static int chess_valid_move(ChessState *state, const Player *player, Tile *from, Tile *to)
{
    if (chess_chess_coords_overlap(&from->chess_coords, &to->chess_coords)) {
        return false;
    }

    bool valid = false;
    bool is_pawn = false;

    Board *board = &state->board;

    switch (from->piece.type) {
        case Pawn:
            is_pawn = true;
            valid = chess_valid_pawn_move(state, from, to);
            break;

        case Rook:
            valid = chess_valid_rook_move(board, from, to);
            break;

        case Knight:
            valid = chess_valid_knight_move(from, to);
            break;

        case Bishop:
            valid = chess_valid_bishop_move(board, from, to);
            break;

        case Queen:
            valid = chess_valid_queen_move(board, from, to);
            break;

        case King:
            valid = chess_valid_king_move(from, to);
            break;

        default:
            valid = false;
            break;
    }

    int ret = valid ? 1 : -1;

    // make mock move and see if we're in check
    if (player != NULL && valid) {
        Piece from_piece;
        Piece to_piece;
        chess_copy_piece(&from_piece, &from->piece);
        chess_copy_piece(&to_piece, &to->piece);

        chess_copy_piece(&to->piece, &from->piece);
        from->piece.type = NoPiece;

        if (chess_player_in_check(state, player)) {
            ret = 0;
        }

        from->piece.type = from_piece.type;
        chess_copy_piece(&to->piece, &to_piece);

        // if we promoted a pawn and we're in check we have to turn it back into a pawn
        // TODO: Maybe validating functions shouldn't be allowed to modify the game state
        if (ret == 0 && is_pawn && from->piece.type == Queen) {
            chess_set_piece(&from->piece, Pawn, player->colour);
        }
    }

    return ret;
}

/*
 * Return true if any piece of `colour` is attacking tile designated by `to`.
 */
static bool chess_piece_attacking_square(ChessState *state, ChessColour colour, Tile *to)
{
    Board *board = &state->board;

    for (size_t i = 0; i < CHESS_SQUARES; ++i) {
        Tile *from = &board->tiles[i];

        if (from->piece.colour != colour || from->piece.type == NoPiece) {
            continue;
        }

        if (chess_valid_move(state, NULL, from, to) == 1) {
            return true;
        }
    }

    return false;
}

/*
 * Disables castling if king or rook moves.
 */
static void chess_player_set_can_castle(Player *player, const Tile *tile)
{
    if (!player->can_castle_ks && !player->can_castle_qs) {
        return;
    }

    if (tile->piece.type == King) {
        player->can_castle_ks = false;
        player->can_castle_qs = false;
        return;
    }

    if (tile->piece.type != Rook) {
        return;
    }

    if ((player->colour == White && tile->chess_coords.N != 1) || (player->colour == Black && tile->chess_coords.N != 8)) {
        return;
    }

    if (tile->chess_coords.L == 'a') {
        player->can_castle_qs = false;
    } else if (tile->chess_coords.L == 'h') {
        player->can_castle_ks = false;
    }
}

/*
 * Attempts to castle king for `player`.
 *
 * Return true if successfully castled.
 */
static bool chess_player_castle(ChessState *state, Player *player, Tile *to)
{
    if (!player->can_castle_ks && !player->can_castle_qs) {
        return false;
    }

    Board *board = &state->board;

    Tile *holding_tile = player->holding_tile;

    if (holding_tile == NULL) {
        return false;
    }

    if (!(holding_tile->piece.type == King && to->piece.type == NoPiece)) {
        return false;
    }

    int l_diff;
    int n_diff;

    if (chess_get_chess_coords_diff(holding_tile, to, &l_diff, &n_diff) == -1) {
        return false;
    }

    if (!(l_diff == 2 && n_diff == 0)) {
        return false;
    }

    ChessCoords coords;
    coords.N = to->chess_coords.N;

    bool queen_side = false;
    Tile *rook_to_tile = NULL;

    if (to->chess_coords.L == 'g') {
        if (!player->can_castle_ks) {
            return false;
        }

        coords.L = 'f';
        rook_to_tile = chess_get_tile_at_chess_coords(board, &coords);

        if (rook_to_tile == NULL) {
            return false;
        }

        if (rook_to_tile->piece.type != NoPiece) {
            return false;
        }
    } else if (to->chess_coords.L == 'c') {
        if (!player->can_castle_qs) {
            return false;
        }

        coords.L = 'd';
        rook_to_tile = chess_get_tile_at_chess_coords(board, &coords);

        coords.L = 'b';
        const Tile *tmp_b = chess_get_tile_at_chess_coords(board, &coords);

        if (rook_to_tile == NULL || tmp_b == NULL) {
            return false;
        }

        if (!(rook_to_tile->piece.type == NoPiece && tmp_b->piece.type == NoPiece)) {
            return false;
        }

        queen_side = true;
    } else {
        return false;
    }

    ChessColour other_colour = player->colour == Black ? White : Black;

    // Make sure a piece isn't attacking either square the king traverses
    if (chess_piece_attacking_square(state, other_colour, rook_to_tile)) {
        return false;
    }

    if (chess_piece_attacking_square(state, other_colour, to)) {
        return false;
    }

    Tile *rook_from_tile = NULL;

    // move rook
    coords.L = queen_side ? 'a' : 'h';
    rook_from_tile = chess_get_tile_at_chess_coords(board, &coords);

    if (rook_from_tile == NULL || rook_from_tile->piece.type != Rook) {
        return false;
    }

    chess_copy_piece(&rook_to_tile->piece, &rook_from_tile->piece);
    chess_set_piece(&rook_from_tile->piece, NoPiece, White);

    // move king
    Piece old_king;
    chess_copy_piece(&old_king, &to->piece);
    chess_copy_piece(&to->piece, &holding_tile->piece);

    chess_set_piece(&holding_tile->piece, NoPiece, White);
    player->holding_tile = NULL;

    if (chess_player_in_check(state, player)) {
        chess_copy_piece(&to->piece, &old_king);
        chess_set_piece(&rook_to_tile->piece, NoPiece, White);
        chess_set_piece(&rook_from_tile->piece, Rook, player->colour);
        chess_set_piece(&holding_tile->piece, King, player->colour);
        return false;
    }

    player->can_castle_qs = false;
    player->can_castle_ks = false;

    return true;
}

static void chess_capture_piece(Player *player, Piece *piece)
{
    size_t idx = player->number_captured;

    if (idx < CHESS_SQUARES) {
        chess_copy_piece(&player->captured[idx], piece);
        ++player->number_captured;
    }
}

static void chess_try_move_piece(ChessState *state, Player *player)
{
    Tile *tile = chess_get_tile(state, state->curs_x, state->curs_y);

    if (tile == NULL) {
        return;
    }

    Tile *holding_tile = player->holding_tile;

    if (holding_tile == NULL) {
        return;
    }

    if (chess_chess_coords_overlap(&holding_tile->chess_coords, &tile->chess_coords)) {
        state->message_length = 0;
        player->holding_tile = NULL;
        return;
    }

    int valid = chess_valid_move(state, player, holding_tile, tile);

    if (valid != 1) {
        if (!player->in_check && chess_player_castle(state, player, tile)) {
            chess_set_piece(&holding_tile->piece, NoPiece, White);
            goto on_success;
        }

        player->holding_tile = NULL;

        const char *message = valid == -1 ? "Invalid move" : "Invalid move (check)";
        chess_set_status_message(state, message, strlen(message));
        return;
    }

    Tile tmp_holding;
    memcpy(&tmp_holding, holding_tile, sizeof(Tile));

    if (tile->piece.type != NoPiece) {
        chess_capture_piece(player, &tile->piece);
    }

    chess_copy_piece(&tile->piece, &player->holding_tile->piece);

    player->holding_tile = NULL;
    chess_set_piece(&holding_tile->piece, NoPiece, White);

    chess_player_set_can_castle(player, &tmp_holding);

on_success:
    chess_player_clear_en_passant(player);

    player->in_check = false;

    Player *other = chess_get_other_player(state);

    if (chess_player_in_check(state, other)) {
        other->in_check = true;
    }

    state->message_length = 0;
    state->black_to_move ^= 1;

    if (state->black_to_move) {
        ++state->move_number;
    }
}

static void chess_pick_up_piece(ChessState *state, Player *player)
{
    Tile *tile = chess_get_tile(state, state->curs_x, state->curs_y);

    if (tile == NULL) {
        return;
    }

    if (tile->piece.type == NoPiece) {
        return;
    }

    if (tile->piece.colour != player->colour) {
        return;
    }

    player->holding_tile = tile;
}

/*
 * Return 0 if `player` does not have sufficient material to check mate his opponent.
 * Return 1 if `player` has sufficient material.
 * Return 2 if `player` has sufficient material but no major or minor pieces.
 */
static int chess_player_can_mate(ChessState *state, const Player *player)
{
    Board *board = &state->board;

    size_t bishop_or_knight = 0;
    bool has_pawn = false;

    for (size_t i = 0; i < CHESS_SQUARES; ++i) {
        Tile tile = board->tiles[i];

        if (tile.piece.type == NoPiece || tile.piece.colour != player->colour) {
            continue;
        }

        if (tile.piece.type == Queen || tile.piece.type == Rook) {
            return 1;
        }

        if (tile.piece.type == Bishop || tile.piece.type == Knight) {
            if (++bishop_or_knight > 1) {
                return 1;
            }
        }

        if (tile.piece.type == Pawn) {
            has_pawn = true;

            if (bishop_or_knight > 0) {
                return 1;
            }
        }
    }

    return has_pawn ? 2 : 0;
}

/*
 * Return true if piece on `from` tile can move to any other square on the board.
 */
static bool chess_piece_can_move(ChessState *state, const Player *player, Tile *from)
{
    Board *board = &state->board;

    for (size_t i = 0; i < CHESS_SQUARES; ++i) {
        Tile *to = &board->tiles[i];

        if (chess_valid_move(state, player, from, to) == 1) {
            return true;
        }
    }

    return false;
}

/*
 * Return true if any piece for `player` can make a legal move.
 */
static bool chess_any_piece_can_move(ChessState *state, const Player *player)
{
    Board *board = &state->board;

    for (size_t i = 0; i < CHESS_SQUARES; ++i) {
        Tile *from = &board->tiles[i];

        if (from->piece.colour != player->colour || from->piece.type == NoPiece) {
            continue;
        }

        if (chess_piece_can_move(state, player, from)) {
            return true;
        }
    }

    return false;
}

/*
 * Return true if game is in stalemate.
 *
 * A game is considered to be in stalemate if neither side has sufficient material to checkmake
 * the opponent's king, or if current turn's player is unable to move any pieces and is not in check.
 */
static bool chess_game_is_statemate(ChessState *state)
{
    const Player *self = chess_get_player_to_move(state);
    const Player *other = chess_get_other_player(state);

    if (self->in_check || other->in_check) {
        return false;
    }

    int self_can_mate = chess_player_can_mate(state, self);
    int other_can_mate = chess_player_can_mate(state, other);

    if (self_can_mate == 0 && other_can_mate == 0) {
        return true;
    }

    if (self_can_mate == 1) {
        return false;
    }

    // player only has pawns and/or king, see if any remaining piece can move
    return !chess_any_piece_can_move(state, self);
}

/*
 * Return true if game is in checkmate.
 */
static bool chess_game_checkmate(ChessState *state)
{
    const Player *self = chess_get_player_to_move(state);

    return !chess_any_piece_can_move(state, self);
}

/*
 * Checks if we have a checkmate or stalemate and updates game status.
 */
static void chess_update_status(ChessState *state)
{
    if (chess_game_is_statemate(state)) {
        state->status = Stalemate;
        const char *message = "Game over: Stalemate";
        chess_set_status_message(state, message, strlen(message));
        return;
    }

    if (chess_game_checkmate(state)) {
        state->status = Checkmate;
        const char *message = "Checkmate!";
        chess_set_status_message(state, message, strlen(message));
        return;
    }
}

static void chess_do_input(ChessState *state)
{
    if (state->status == Checkmate || state->status == Stalemate) {
        return;
    }

    Player *player = chess_get_player_to_move(state);

    if (player->holding_tile == NULL) {
        chess_pick_up_piece(state, player);
    } else {
        chess_try_move_piece(state, player);
        chess_update_status(state);
    }
}

static void chess_move_curs_left(ChessState *state)
{
    Board *board = &state->board;

    size_t new_x = state->curs_x - CHESS_TILE_SIZE_X;

    if (new_x < board->x_left_bound) {
        return;
    }

    state->curs_x = new_x;
}

static void chess_move_curs_right(ChessState *state)
{
    Board *board = &state->board;

    size_t new_x = state->curs_x + CHESS_TILE_SIZE_X;

    if (new_x > board->x_right_bound) {
        return;
    }

    state->curs_x = new_x;
}

static void chess_move_curs_up(ChessState *state)
{
    Board *board = &state->board;

    size_t new_y = state->curs_y - CHESS_TILE_SIZE_Y;

    if (new_y < board->y_top_bound) {
        return;
    }

    state->curs_y = new_y;
}

static void chess_move_curs_down(ChessState *state)
{
    Board *board = &state->board;

    size_t new_y = state->curs_y + CHESS_TILE_SIZE_Y;

    if (new_y >= board->y_bottom_bound) {
        return;
    }

    state->curs_y = new_y;
}

static int chess_get_display_colour(ChessColour p_colour, int tile_colour)
{
    if (tile_colour == CHESS_WHITE_SQUARE_COLOUR) {
        if (p_colour == White) {
            return BLACK_WHITE;  // white square, white piece
        } else {
            return YELLOW;   // white square, black piece
        }
    }

    if (p_colour == White) {
        return BLACK_WHITE;  // black square, white piece
    } else {
        return YELLOW;  // black square, black piece
    }
}

static void chess_draw_board_white(WINDOW *win, const Board *board)
{
    for (size_t i = 0; i < CHESS_BOARD_COLUMNS; ++i) {
        mvwaddch(win, board->y_bottom_bound, board->x_left_bound + 1 + (i * CHESS_TILE_SIZE_X), Board_Letters[i]);
    }

    for (size_t i = 0; i < CHESS_BOARD_ROWS; ++i) {
        mvwprintw(win, board->y_bottom_bound - 1 - (i * CHESS_TILE_SIZE_Y), board->x_left_bound - 1, "%zu", i + 1);
    }
}

static void chess_draw_board_black(WINDOW *win, const Board *board)
{
    size_t l_idx = CHESS_NUM_BOARD_LETTERS - 1;

    for (size_t i = 0; i < CHESS_BOARD_COLUMNS && l_idx >= 0; ++i, --l_idx) {
        mvwaddch(win, board->y_bottom_bound, board->x_left_bound + 1 + (i * CHESS_TILE_SIZE_X), Board_Letters[l_idx]);
    }

    size_t n_idx = CHESS_BOARD_ROWS;

    for (size_t i = 0; i < CHESS_BOARD_ROWS && n_idx > 0; ++i, --n_idx) {
        mvwprintw(win, board->y_bottom_bound - 1 - (i * CHESS_TILE_SIZE_Y), board->x_left_bound - 1, "%zu", n_idx);
    }
}

static void chess_draw_board(WINDOW *win, ChessState *state)
{
    const Player *player = chess_get_player_to_move(state);
    Board *board = &state->board;

    for (size_t i = 0; i < CHESS_SQUARES; ++i) {
        Tile tile = board->tiles[i];
        wattron(win, COLOR_PAIR(tile.colour));

        for (size_t x = 0; x < CHESS_TILE_SIZE_X; ++x) {
            for (size_t y = 0; y < CHESS_TILE_SIZE_Y; ++y) {
                mvwaddch(win, tile.coords.y + y, tile.coords.x + x, ' ');
            }
        }

        wattroff(win, COLOR_PAIR(tile.colour));

        // don't draw the piece we're currently holding
        if (player->holding_tile != NULL) {
            if (chess_chess_coords_overlap(&tile.chess_coords, &player->holding_tile->chess_coords)) {
                continue;
            }
        }

        Piece piece = tile.piece;

        if (piece.type != NoPiece) {
            int colour = chess_get_display_colour(piece.colour, tile.colour);

            wattron(win, COLOR_PAIR(colour));
            mvwaddch(win, tile.coords.y, tile.coords.x + 1, piece.display_char);
            wattroff(win, COLOR_PAIR(colour));
        }
    }


    if (state->self.colour == White) {
        chess_draw_board_white(win, board);
    } else {
        chess_draw_board_black(win, board);
    }

    // if holding a piece draw it at cursor position
    if (player->holding_tile != NULL) {
        Tile *tile = player->holding_tile;

        wattron(win, A_BOLD | COLOR_PAIR(BLACK));
        mvwaddch(win, state->curs_y, state->curs_x, tile->piece.display_char);
        wattroff(win, A_BOLD | COLOR_PAIR(BLACK));
    }
}

static void chess_print_status(WINDOW *win, ChessState *state)
{
    const Board *board = &state->board;

    wattron(win, A_BOLD);

    const Player *player = chess_get_player_to_move(state);

    char message[CHESS_MAX_MESSAGE_SIZE + 1];
    snprintf(message, sizeof(message), "%s to move %s", state->black_to_move ? "Black" : "White",
             player->in_check ? "(check)" : "");

    int x_mid = (board->x_left_bound + (CHESS_TILE_SIZE_X * (CHESS_BOARD_COLUMNS / 2))) - (strlen(message) / 2);
    mvwprintw(win, board->y_top_bound  - 2, x_mid, message);

    if (state->message_length > 0) {
        x_mid = (board->x_left_bound + (CHESS_TILE_SIZE_X * (CHESS_BOARD_COLUMNS / 2))) - (state->message_length / 2);
        mvwprintw(win, board->y_bottom_bound + 2, x_mid, state->status_message);
    }

    wattroff(win, A_BOLD);
}

static void chess_print_captured(const GameData *game, WINDOW *win, ChessState *state)
{
    const Board *board = &state->board;

    const Player *self = &state->self;
    const Player *other = &state->other;

    int self_top_y_start = board->y_bottom_bound - (CHESS_TILE_SIZE_Y * 3);
    int other_top_y_start = board->y_top_bound;

    const int left_x_start = board->x_right_bound + 1;
    const int right_x_border = game_x_right_bound(game) - 1;

    size_t idx = 0;
    int colour = self->colour == White ? YELLOW : WHITE;

    wattron(win, COLOR_PAIR(colour));

    for (size_t y = self_top_y_start; y < board->y_bottom_bound; ++y) {
        for (size_t x = left_x_start; x < right_x_border && idx < self->number_captured; x += 2, ++idx) {
            Piece piece = self->captured[idx];
            mvwaddch(win, y, x, piece.display_char);
        }
    }

    wattroff(win, COLOR_PAIR(colour));

    colour = colour == YELLOW ? WHITE : YELLOW;
    idx = 0;

    wattron(win, COLOR_PAIR(colour));

    for (size_t y = other_top_y_start; y < board->y_bottom_bound; ++y) {
        for (size_t x = left_x_start; x < right_x_border && idx < other->number_captured; x += 2, ++idx) {
            Piece piece = other->captured[idx];
            mvwaddch(win, y, x, piece.display_char);
        }
    }

    wattroff(win, COLOR_PAIR(colour));
}

static void chess_draw_interface(const GameData *game, WINDOW *win, ChessState *state)
{
    chess_print_status(win, state);
    chess_print_captured(game, win, state);
}

void chess_cb_render_window(GameData *game, WINDOW *win, void *cb_data)
{
    if (!cb_data) {
        return;
    }

    ChessState *state = (ChessState *)cb_data;

    move(state->curs_y, state->curs_x);


    curs_set(1);

    chess_draw_board(win, state);
    chess_draw_interface(game, win, state);
}

void chess_cb_on_keypress(GameData *game, int key, void *cb_data)
{
    if (!cb_data) {
        return;
    }

    ChessState *state = (ChessState *)cb_data;

    switch (key) {
        case KEY_LEFT: {
            chess_move_curs_left(state);
            break;
        }

        case KEY_RIGHT: {
            chess_move_curs_right(state);
            break;
        }

        case KEY_DOWN: {
            chess_move_curs_down(state);
            break;
        }

        case KEY_UP: {
            chess_move_curs_up(state);
            break;
        }

        case '\r': {
            chess_do_input(state);
            break;
        }

        default: {
            return;
        }
    }
}

void chess_cb_kill(GameData *game, void *cb_data)
{
    if (!cb_data) {
        return;
    }

    ChessState *state = (ChessState *)cb_data;

    free(state);

    game_set_cb_render_window(game, NULL, NULL);
    game_set_cb_kill(game, NULL, NULL);
}

static int chess_init_board(GameData *game, ChessState *state, bool self_is_white)
{
    Board *board = &state->board;

    const int x_left = game_x_left_bound(game);
    const int x_right = game_x_right_bound(game);
    const int y_top = game_y_top_bound(game);
    const int y_bottom = game_y_bottom_bound(game);
    const int x_mid = x_left + ((x_right - x_left) / 2);
    const int y_mid = y_top + ((y_bottom - y_top) / 2);

    const int board_width = CHESS_TILE_SIZE_X * CHESS_BOARD_COLUMNS;
    const int board_height = CHESS_TILE_SIZE_Y * CHESS_BOARD_ROWS;

    state->curs_x = x_mid + 1;
    state->curs_y = y_mid;

    board->x_left_bound = x_mid - (board_width / 2);
    board->x_right_bound = x_mid + (board_width / 2);
    board->y_bottom_bound = y_mid + (board_height / 2);
    board->y_top_bound = y_mid - (board_height / 2);

    if (board->y_bottom_bound > y_bottom || board->x_left_bound < x_left) {
        return -1;
    }

    size_t colour_rotation = 1;
    size_t board_idx = 0;
    size_t letter_idx = self_is_white ? 0 : 7;

    for (size_t i = board->x_left_bound; i < board->x_right_bound; i += CHESS_TILE_SIZE_X) {
        size_t number_idx = self_is_white ? 8 : 1;
        char letter = Board_Letters[letter_idx];
        letter_idx = self_is_white ? letter_idx + 1 : letter_idx - 1;
        colour_rotation ^= 1;

        for (size_t j = board->y_top_bound; j < board->y_bottom_bound; j += CHESS_TILE_SIZE_Y) {
            Tile *tile = &board->tiles[board_idx];

            tile->colour = (board_idx + colour_rotation) % 2 == 0 ? CHESS_WHITE_SQUARE_COLOUR : CHESS_BLACK_SQUARE_COLOUR;
            tile->coords.x = i;
            tile->coords.y = j;
            tile->chess_coords.L = letter;
            tile->chess_coords.N = number_idx;

            number_idx = self_is_white ? number_idx - 1 : number_idx + 1;
            ++board_idx;
        }
    }

    for (size_t i = 0; i < CHESS_SQUARES; ++i) {
        Tile *tile = &board->tiles[i];
        chess_set_piece(&tile->piece, NoPiece, White);

        if (tile->chess_coords.N == 2 || tile->chess_coords.N == 7) {
            chess_set_piece(&tile->piece, Pawn, tile->chess_coords.N == 2 ? White : Black);
            continue;
        }

        if (tile->chess_coords.N == 1 || tile->chess_coords.N == 8) {
            PieceType type;

            switch (tile->chess_coords.L) {
                case 'a':

                // fallthrough
                case 'h':
                    type = Rook;
                    break;

                case 'b':

                // fallthrough
                case 'g':
                    type = Knight;
                    break;

                case 'c':

                // fallthrough
                case 'f':
                    type = Bishop;
                    break;

                case 'd':
                    type = Queen;
                    break;

                case 'e':
                    type = King;
                    break;

                default:
                    type = NoPiece;
                    break;
            }

            chess_set_piece(&tile->piece, type, tile->chess_coords.N == 1 ? White : Black);
        }
    }

    return 0;
}

int chess_initialize(GameData *game)
{
    if (game_set_window_shape(game, GW_ShapeSquare) == -1) {
        return -1;
    }

    ChessState *state = calloc(1, sizeof(ChessState));

    if (state == NULL) {
        return -1;
    }

    bool self_is_white = rand() % 2 == 0;
    state->self.colour = self_is_white ? White : Black;
    state->other.colour = !self_is_white ? White : Black;

    if (chess_init_board(game, state, self_is_white) == -1) {
        free(state);
        return -1;
    }

    state->self.can_castle_ks = true;
    state->self.can_castle_qs = true;
    state->other.can_castle_ks = true;
    state->other.can_castle_qs = true;

    game_set_update_interval(game, 20);

    game_set_cb_render_window(game, chess_cb_render_window, state);
    game_set_cb_on_keypress(game, chess_cb_on_keypress, state);
    game_set_cb_kill(game, chess_cb_kill, state);

    return 0;
}


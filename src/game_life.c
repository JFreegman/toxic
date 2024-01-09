/*  game_life.c
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game_life.h"

#define LIFE_DEFAULT_CELL_CHAR     'o'
#define LIFE_CELL_DEFAULT_COLOUR    CYAN
#define LIFE_DEFAULT_SPEED          25
#define LIFE_MAX_SPEED              40

/* Determines the additional size of the grid beyond the visible boundaries.
 *
 * This buffer allows cells to continue growing off-screen giving the illusion of an
 * infinite grid to a certain point.
 */
#define LIFE_BOUNDARY_BUFFER        50


typedef struct Cell {
    Coords     coords;
    bool       alive;
    bool       marked;  // true if cell should invert alive status at end of current cycle
    int        display_char;
    size_t     age;
} Cell;

typedef struct LifeState {
    TIME_MS    time_last_cycle;
    size_t     speed;
    size_t     generation;
    bool       paused;

    Cell       **cells;
    int        num_columns;
    int        num_rows;

    int        curs_x;
    int        curs_y;

    int        x_left_bound;
    int        x_right_bound;
    int        y_top_bound;
    int        y_bottom_bound;

    short      display_candy;
    int        colour;
} LifeState;


static void life_increase_speed(LifeState *state)
{
    if (state->speed < LIFE_MAX_SPEED) {
        ++state->speed;
    }
}

static void life_decrease_speed(LifeState *state)
{
    if (state->speed > 1) {
        --state->speed;
    }
}

static int life_get_display_char(const LifeState *state, const Cell *cell)
{
    if (state->display_candy == 1) {
        if (cell->age == 1) {
            return '.';
        }

        return '+';
    }

    if (state->display_candy == 2) {
        if (cell->age == 1) {
            return '.';
        }

        if (cell->age == 2) {
            return '-';
        }

        if (cell->age == 3) {
            return 'o';
        }

        return 'O';
    }

    return 'o';
}

static void life_toggle_display_candy(LifeState *state)
{
    state->display_candy = (state->display_candy + 1) % 3;  // magic number depends on life_get_display_char()
}

static void life_cycle_colour(LifeState *state)
{
    switch (state->colour) {
        case RED: {
            state->colour = YELLOW;
            break;
        }

        case YELLOW: {
            state->colour = GREEN;
            break;
        }

        case GREEN: {
            state->colour = CYAN;
            break;
        }

        case CYAN: {
            state->colour = BLUE;
            break;
        }

        case BLUE: {
            state->colour = MAGENTA;
            break;
        }

        case MAGENTA: {
            state->colour = RED;
            break;
        }

        default: {
            state->colour = RED;
            break;
        }
    }
}

static Cell *life_get_cell_at_coords(const LifeState *state, const int x, const int y)
{
    const int i = y - (state->y_top_bound - (LIFE_BOUNDARY_BUFFER / 2));
    const int j = x - (state->x_left_bound - (LIFE_BOUNDARY_BUFFER / 2));

    if (i >= 0 && j >= 0) {
        return &state->cells[i][j];
    }

    return NULL;
}

static void life_draw_cells(const GameData *game, WINDOW *win, LifeState *state)
{
    wattron(win, A_BOLD | COLOR_PAIR(state->colour));

    for (int i = LIFE_BOUNDARY_BUFFER / 2; i < state->num_rows - (LIFE_BOUNDARY_BUFFER / 2); ++i) {
        for (int j = LIFE_BOUNDARY_BUFFER / 2; j < state->num_columns + 1 - (LIFE_BOUNDARY_BUFFER / 2); ++j) {
            Cell *cell = &state->cells[i][j];

            if (cell->alive) {
                Coords coords = cell->coords;
                mvwaddch(win, coords.y, coords.x, cell->display_char);
            }
        }
    }

    wattroff(win, A_BOLD | COLOR_PAIR(state->colour));
}

static void life_toggle_cell(LifeState *state)
{
    Cell *cell = life_get_cell_at_coords(state, state->curs_x, state->curs_y);

    if (cell == NULL) {
        return;
    }

    cell->alive ^= 1;
}

/*
 * Returns the number of live neighbours of cell at `i` `j` position.
 *
 * Returns NULL if cell is touching a border.
 */
static int life_get_live_neighbours(const LifeState *state, const int i, const int j)
{
    Cell *n[8] = {0};

    if (i > 0 && j > 0) {
        n[0] = &state->cells[i - 1][j - 1];
    }

    if (i > 0) {
        n[1] = &state->cells[i - 1][j];
    }

    if (i > 0 && j < state->num_columns - 1) {
        n[2] = &state->cells[i - 1][j + 1];
    }

    if (j > 0) {
        n[3] = &state->cells[i][j - 1];
    }

    if (j < state->num_columns - 1) {
        n[4] = &state->cells[i][j + 1];
    }

    if (i < state->num_rows - 1 && j > 0) {
        n[5] = &state->cells[i + 1][j - 1];
    }

    if (i < state->num_rows - 1) {
        n[6] = &state->cells[i + 1][j];
    }

    if (i < state->num_rows - 1 && j < state->num_columns - 1) {
        n[7] = &state->cells[i + 1][j + 1];
    }

    int count = 0;

    for (size_t i = 0; i < 8; ++i) {
        if (n[i] == NULL) {
            return 0; // If we're at a boundary kill cell
        }

        if (n[i]->alive) {
            ++count;
        }
    }

    return count;
}

static void life_restart(GameData *game, LifeState *state)
{
    for (int i = 0; i < state->num_rows; ++i) {
        for (int j = 0; j < state->num_columns; ++j) {
            Cell *cell = &state->cells[i][j];
            cell->alive = false;
            cell->marked = false;
            cell->display_char = LIFE_DEFAULT_CELL_CHAR;
            cell->age = 0;
        }
    }

    game_set_score(game, 0);

    state->generation = 0;
}

static void life_do_cells(LifeState *state)
{

    for (int i = 0; i < state->num_rows; ++i) {
        for (int j = 0; j < state->num_columns; ++j) {
            Cell *cell = &state->cells[i][j];

            if (cell->marked) {
                cell->marked = false;
                cell->alive ^= 1;
                cell->age = cell->alive;
                cell->display_char = life_get_display_char(state, cell);
            } else if (cell->alive) {
                ++cell->age;
                cell->display_char = life_get_display_char(state, cell);
            }
        }
    }
}

static void life_cycle(GameData *game, LifeState *state)
{
    if (state->generation == 0) {
        return;
    }

    TIME_MS cur_time = get_time_millis();

    if (!game_do_object_state_update(game, cur_time, state->time_last_cycle, state->speed)) {
        return;
    }

    state->time_last_cycle = get_time_millis();

    ++state->generation;

    size_t live_cells = 0;

    for (int i = 0; i < state->num_rows; ++i) {
        for (int j = 0; j < state->num_columns; ++j) {
            Cell *cell = &state->cells[i][j];

            int live_neighbours = life_get_live_neighbours(state, i, j);

            if (cell->alive) {
                if (!(live_neighbours == 2 || live_neighbours == 3)) {
                    cell->marked = true;
                } else {
                    ++live_cells;
                }
            } else {
                if (live_neighbours == 3) {
                    cell->marked = true;
                    ++live_cells;
                }
            }
        }
    }

    if (live_cells == 0) {
        life_restart(game, state);
        return;
    }

    life_do_cells(state);

    game_update_score(game, 1);
}

static void life_start(GameData *game, LifeState *state)
{
    state->generation = 1;
}

void life_cb_update_game_state(GameData *game, void *cb_data)
{
    if (!cb_data) {
        return;
    }

    LifeState *state = (LifeState *)cb_data;

    life_cycle(game, state);
}

void life_cb_render_window(GameData *game, WINDOW *win, void *cb_data)
{
    if (!cb_data) {
        return;
    }

    LifeState *state = (LifeState *)cb_data;

    move(state->curs_y, state->curs_x);

    if (state->generation == 0 || state->paused) {
        curs_set(1);
    }

    life_draw_cells(game, win, state);
}

static void life_move_curs_left(LifeState *state)
{
    int new_x = state->curs_x - 1;

    if (new_x < state->x_left_bound) {
        return;
    }

    state->curs_x = new_x;
}

static void life_move_curs_right(LifeState *state)
{
    int new_x = state->curs_x + 1;

    if (new_x > state->x_right_bound) {
        return;
    }

    state->curs_x = new_x;
}

static void life_move_curs_up(LifeState *state)
{
    int new_y = state->curs_y - 1;

    if (new_y < state->y_top_bound) {
        return;
    }

    state->curs_y = new_y;
}

static void life_move_curs_down(LifeState *state)
{
    int new_y = state->curs_y + 1;

    if (new_y >= state->y_bottom_bound) {
        return;
    }

    state->curs_y = new_y;
}

static void life_move_curs_up_left(LifeState *state)
{
    life_move_curs_up(state);
    life_move_curs_left(state);
}

static void life_move_curs_up_right(LifeState *state)
{
    life_move_curs_up(state);
    life_move_curs_right(state);
}

static void life_move_curs_down_right(LifeState *state)
{
    life_move_curs_down(state);
    life_move_curs_right(state);
}

static void life_move_curs_down_left(LifeState *state)
{
    life_move_curs_down(state);
    life_move_curs_left(state);
}

void life_cb_on_keypress(GameData *game, int key, void *cb_data)
{
    if (!cb_data) {
        return;
    }

    LifeState *state = (LifeState *)cb_data;

    switch (key) {
        case KEY_LEFT: {
            life_move_curs_left(state);
            break;
        }

        case KEY_RIGHT: {
            life_move_curs_right(state);
            break;
        }

        case KEY_DOWN: {
            life_move_curs_down(state);
            break;
        }

        case KEY_UP: {
            life_move_curs_up(state);
            break;
        }

        case KEY_HOME: {
            life_move_curs_up_left(state);
            break;
        }

        case KEY_END: {
            life_move_curs_down_left(state);
            break;
        }

        case KEY_PPAGE: {
            life_move_curs_up_right(state);
            break;
        }

        case KEY_NPAGE: {
            life_move_curs_down_right(state);
            break;
        }

        case '\r': {
            if (state->generation > 0) {
                life_restart(game, state);
            } else {
                life_start(game, state);
            }

            break;
        }

        case ' ': {
            life_toggle_cell(state);
            break;
        }

        case '=':

        /* intentional fallthrough */

        case '+': {
            life_increase_speed(state);
            break;
        }

        case '-':

        /* intentional fallthrough */

        case '_': {
            life_decrease_speed(state);
            break;
        }

        case '\t': {
            life_toggle_display_candy(state);
            break;
        }

        case '`': {
            life_cycle_colour(state);
            break;
        }

        default: {
            return;
        }
    }
}

static void life_free_cells(LifeState *state)
{
    if (state->cells == NULL) {
        return;
    }

    for (int i = 0; i < state->num_rows; ++i) {
        if (state->cells[i]) {
            free(state->cells[i]);
        }
    }

    free(state->cells);
}

void life_cb_pause(GameData *game, bool is_paused, void *cb_data)
{
    if (!cb_data) {
        return;
    }

    LifeState *state = (LifeState *)cb_data;

    state->paused = is_paused;
}

void life_cb_kill(GameData *game, void *cb_data)
{
    if (!cb_data) {
        return;
    }

    LifeState *state = (LifeState *)cb_data;

    life_free_cells(state);
    free(state);

    game_set_cb_update_state(game, NULL, NULL);
    game_set_cb_render_window(game, NULL, NULL);
    game_set_cb_kill(game, NULL, NULL);
    game_set_cb_on_keypress(game, NULL, NULL);
}

static int life_init_state(GameData *game, LifeState *state)
{
    const int x_left = game_x_left_bound(game) ;
    const int x_right = game_x_right_bound(game);
    const int y_top = game_y_top_bound(game);
    const int y_bottom = game_y_bottom_bound(game) + 1;

    state->x_left_bound = x_left;
    state->x_right_bound = x_right;
    state->y_top_bound = y_top;
    state->y_bottom_bound = y_bottom;

    const int x_mid = x_left + ((x_right - x_left) / 2);
    const int y_mid = y_top + ((y_bottom - y_top) / 2);

    state->curs_x = x_mid;
    state->curs_y = y_mid;

    const int num_rows = (y_bottom - y_top) + LIFE_BOUNDARY_BUFFER;
    const int num_columns = (x_right - x_left) + LIFE_BOUNDARY_BUFFER;

    if (num_rows <= 0 || num_columns <= 0) {
        return -1;
    }

    state->num_columns = num_columns;
    state->num_rows = num_rows;

    state->cells = calloc(1, num_rows * sizeof(Cell *));

    if (state->cells == NULL) {
        return -1;
    }

    for (int i = 0; i < num_rows; ++i) {
        state->cells[i] = calloc(1, num_columns * sizeof(Cell));

        if (state->cells[i] == NULL) {
            return -1;
        }

        for (int j = 0; j < num_columns; ++j) {
            state->cells[i][j].coords.y = i + (state->y_top_bound - (LIFE_BOUNDARY_BUFFER / 2));
            state->cells[i][j].coords.x = j + (state->x_left_bound - (LIFE_BOUNDARY_BUFFER / 2));
        }
    }

    state->speed = LIFE_DEFAULT_SPEED;
    state->colour = LIFE_CELL_DEFAULT_COLOUR;

    life_restart(game, state);

    return 0;
}

int life_initialize(GameData *game)
{
    // Try best fit from largest to smallest before giving up
    if (game_set_window_shape(game, GW_ShapeRectangleLarge) == -1) {
        if (game_set_window_shape(game, GW_ShapeSquareLarge) == -1) {
            if (game_set_window_shape(game, GW_ShapeRectangle) == -1) {
                if (game_set_window_shape(game, GW_ShapeSquare) == -1) {
                    return -1;
                }
            }
        }
    }

    LifeState *state = calloc(1, sizeof(LifeState));

    if (state == NULL) {
        return -1;
    }

    if (life_init_state(game, state) == -1) {
        life_free_cells(state);
        free(state);
        return -1;
    }

    game_set_update_interval(game, 40);
    game_show_score(game, true);

    game_set_cb_update_state(game, life_cb_update_game_state, state);
    game_set_cb_render_window(game, life_cb_render_window, state);
    game_set_cb_on_keypress(game, life_cb_on_keypress, state);
    game_set_cb_on_pause(game, life_cb_pause, state);
    game_set_cb_kill(game, life_cb_kill, state);

    return 0;
}

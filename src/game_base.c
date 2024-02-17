/*  game_base.c
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "friendlist.h"
#include "game_centipede.h"
#include "game_base.h"
#include "game_chess.h"
#include "game_life.h"
#include "game_snake.h"
#include "line_info.h"
#include "misc_tools.h"
#include "notify.h"
#include "settings.h"
#include "windows.h"

extern struct Winthread Winthread;

/*
 * Determines the base rate at which game objects should update their state.
 * Inversely correlated with frame rate.
 */
#define GAME_OBJECT_UPDATE_INTERVAL_MULTIPLIER 25

/* Determines overall game speed; lower makes it faster and vice versa.
 * Inversely correlated with frame rate.
 */
#define GAME_DEFAULT_UPDATE_INTERVAL 10
#define GAME_MAX_UPDATE_INTERVAL 50



/* Determines if window is large enough for a respective window type */
#define WINDOW_SIZE_SQUARE_VALID(max_x, max_y)((((max_y) - 4) >= (GAME_MAX_SQUARE_Y_DEFAULT))\
                                             && ((max_x) >= (GAME_MAX_SQUARE_X_DEFAULT)))

#define WINDOW_SIZE_LARGE_SQUARE_VALID(max_x, max_y)((((max_y) - 4) >= (GAME_MAX_SQUARE_Y_LARGE))\
                                                   && ((max_x) >= (GAME_MAX_SQUARE_X_LARGE)))

#define WINDOW_SIZE_RECT_VALID(max_x, max_y)((((max_y) - 4) >= (GAME_MAX_RECT_Y_DEFAULT))\
                                           && ((max_x) >= (GAME_MAX_RECT_X_DEFAULT)))

#define WINDOW_SIZE_LARGE_RECT_VALID(max_x, max_y)((((max_y) - 4) >= (GAME_MAX_RECT_Y_LARGE))\
                                                 && ((max_x) >= (GAME_MAX_RECT_X_LARGE)))


static ToxWindow *game_new_window(Tox *tox, GameType type, uint32_t friendnumber);

struct GameList {
    const char *name;
    GameType   type;
};

static struct GameList game_list[] = {
    { "centipede", GT_Centipede  },
    { "chess",     GT_Chess      },
    { "life",      GT_Life       },
    { "snake",     GT_Snake      },
    {  NULL,       GT_Invalid    },
};

/*
 * Returns the GameType associated with `game_string`.
 */
GameType game_get_type(const char *game_string)
{
    const char *match_string = NULL;

    for (size_t i = 0; (match_string = game_list[i].name); ++i) {
        if (strcasecmp(game_string, match_string) == 0) {
            return game_list[i].type;
        }
    }

    return GT_Invalid;
}

const char *game_get_name_string(GameType type)
{
    GameType match_type;

    for (size_t i = 0; (match_type = game_list[i].type) < GT_Invalid; ++i) {
        if (match_type == type) {
            return game_list[i].name;
        }
    }

    return NULL;
}

void game_list_print(ToxWindow *self, const Client_Config *c_config)
{
    line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "Available games:");

    const char *name = NULL;

    for (size_t i = 0; (name = game_list[i].name); ++i) {
        line_info_add(self, c_config, false, NULL, NULL, SYS_MSG, 0, 0, "- %s", name);
    }
}

bool game_type_has_multiplayer(GameType type)
{
    return type == GT_Chess || type == GT_Snake;
}

static bool game_type_is_multi_only(GameType type)
{
    return type == GT_Chess;
}

static bool game_type_is_multi_and_single(const ToxWindow *window, GameType type)
{
    if (window->type != WINDOW_TYPE_CHAT) {
        return false;
    }

    return type == GT_Snake;
}

/*
 * Sends a notification to the window associated with `game`.
 *
 * `message` - the notification message that will be displayed.
 */
void game_window_notify(const GameData *game, const char *message)
{
    const Toxic *toxic = game->toxic;
    const Client_Config *c_config = toxic->c_config;

    ToxWindow *self = get_window_pointer_by_id(toxic->windows, game->window_id);

    if (self == NULL) {
        return;
    }

    const int bell_on_message = c_config->bell_on_message;

    if (self->active_box != -1) {
        box_notify2(self, toxic, generic_message, NT_WNDALERT_0 | NT_NOFOCUS | bell_on_message,
                    self->active_box, "%s", message);
    } else {
        box_notify(self, toxic, generic_message, NT_WNDALERT_0 | NT_NOFOCUS | bell_on_message,
                   &self->active_box, self->name, "%s", message);
    }
}

/* Returns the current wall time in milliseconds */
TIME_MS get_time_millis(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return ((TIME_MS) t.tv_sec) * 1000 + ((TIME_MS) t.tv_nsec) / 1000000;
}

void game_kill(ToxWindow *self, Windows *windows, const Client_Config *c_config)
{
    GameData *game = self->game;

    if (game != NULL) {
        if (game->cb_game_kill) {
            game->cb_game_kill(game, game->cb_game_kill_data);
        }

        delwin(game->window);
        free(game->messages);
        free(game);
    }

    kill_notifs(self->active_box);
    del_window(self, windows, c_config);

    if (get_num_active_windows_type(windows, WINDOW_TYPE_GAME) == 0) {
        set_window_refresh_rate(NCURSES_DEFAULT_REFRESH_RATE);
    }
}

static void game_init_abort(const ToxWindow *parent, ToxWindow *self, Windows *windows,
                            const Client_Config *c_config)
{
    game_kill(self, windows, c_config);
    set_active_window_by_id(windows, parent->id);
}

static void game_toggle_pause(GameData *game)
{
    GameStatus status = game->status;

    if (status == GS_Running) {
        game->status = GS_Paused;
    } else if (status == GS_Paused) {
        game->status = GS_Running;
    } else {
        return;
    }

    if (game->cb_game_pause) {
        game->cb_game_pause(game, game->status == GS_Paused, game->cb_game_pause_data);
    }
}

static int game_initialize_type(GameData *game, const uint8_t *data, size_t length, bool self_host)
{
    int ret = -3;

    switch (game->type) {
        case GT_Snake: {
            ret = snake_initialize(game, game->is_multiplayer, self_host);
            break;
        }

        case GT_Centipede: {
            ret = centipede_initialize(game);
            break;
        }

        case GT_Chess: {
            ret = chess_initialize(game, data, length, self_host);
            break;
        }

        case GT_Life: {
            ret = life_initialize(game);
            break;
        }

        default: {
            break;
        }
    }

    return ret;
}

int game_initialize(const ToxWindow *parent, Toxic *toxic, GameType type, uint32_t id, const uint8_t *multiplayer_data,
                    size_t length, bool self_host)
{
    const Client_Config *c_config = toxic->c_config;

    int max_x;
    int max_y;
    getmaxyx(parent->window, max_y, max_x);

    max_y -= (CHATBOX_HEIGHT + WINDOW_BAR_HEIGHT);

    ToxWindow *self = game_new_window(toxic->tox, type, parent->num);

    if (self == NULL) {
        return -4;
    }

    GameData *game = self->game;

    const int64_t window_id = add_window(toxic, self);

    if (window_id < 0) {
        free(game);
        free(self);
        return -4;
    }

    game->is_multiplayer = game_type_is_multi_only(type) || game_type_is_multi_and_single(parent, type);

    if (game->is_multiplayer) {
        if (parent->type != WINDOW_TYPE_CHAT) {
            game_init_abort(parent, self, toxic->windows, c_config);
            return -3;
        }

        if (get_friend_connection_status(parent->num) == TOX_CONNECTION_NONE) {
            game_init_abort(parent, self, toxic->windows, c_config);
            return -2;
        }

        game->is_multiplayer = true;
    }

    game->toxic = toxic;
    game->window_shape = GW_ShapeSquare;
    game->parent_max_x = max_x;
    game->parent_max_y = max_y;
    game->update_interval = GAME_DEFAULT_UPDATE_INTERVAL;
    game->type = type;
    game->window_id = window_id;
    game->window = subwin(self->window, max_y, max_x, 0, 0);
    game->id = id;
    game->friend_number = parent->num;

    if (game->window == NULL) {
        game_init_abort(parent, self, toxic->windows, c_config);
        return -4;
    }

    const int init_ret = game_initialize_type(game, multiplayer_data, length, self_host);

    if (init_ret < 0) {
        game_init_abort(parent, self, toxic->windows, c_config);
        return init_ret;
    }

    game->status = GS_Running;

    set_active_window_by_id(toxic->windows, game->window_id);

    set_window_refresh_rate(NCURSES_GAME_REFRESH_RATE);

    return 0;
}

int game_set_window_shape(GameData *game, GameWindowShape shape)
{
    if (shape >= GW_ShapeInvalid) {
        return -1;
    }

    if (game->status != GS_None) {
        return -2;
    }

    const int max_x = game->parent_max_x;
    const int max_y = game->parent_max_y;

    switch (shape) {
        case GW_ShapeSquare: {
            if (WINDOW_SIZE_SQUARE_VALID(max_x, max_y)) {
                game->game_max_x = GAME_MAX_SQUARE_X_DEFAULT;
                game->game_max_y = GAME_MAX_SQUARE_Y_DEFAULT;
                return 0;
            }

            break;
        }

        case GW_ShapeSquareLarge: {
            if (WINDOW_SIZE_LARGE_SQUARE_VALID(max_x, max_y)) {
                game->game_max_x = GAME_MAX_SQUARE_X_LARGE;
                game->game_max_y = GAME_MAX_SQUARE_Y_LARGE;
                return 0;
            }

            break;
        }

        case GW_ShapeRectangle: {
            if (WINDOW_SIZE_RECT_VALID(max_x, max_y)) {
                game->game_max_x = GAME_MAX_RECT_X_DEFAULT;
                game->game_max_y = GAME_MAX_RECT_Y_DEFAULT;
                return 0;
            }

            break;
        }

        case GW_ShapeRectangleLarge: {
            if (WINDOW_SIZE_LARGE_RECT_VALID(max_x, max_y)) {
                game->game_max_x = GAME_MAX_RECT_X_LARGE;
                game->game_max_y = GAME_MAX_RECT_Y_LARGE;
                return 0;
            }

            break;
        }

        default: {
            return -1;
        }
    }

    return -1;
}

static void game_fix_message_coords(const GameData *game, Direction direction, Coords *coords, size_t length)
{
    if (direction >= INVALID_DIRECTION) {
        return;
    }

    if (direction == EAST || direction == WEST) {
        coords->y = game_coordinates_in_bounds(game, coords->x, coords->y + 2) ? coords->y + 2 : coords->y - 2;
    } else {
        coords->x = game_coordinates_in_bounds(game, coords->x + 2, coords->y)
                    ? coords->x + 2 : coords->x - (length + 2);
    }

    if (!game_coordinates_in_bounds(game, coords->x + length, coords->y)
            || !game_coordinates_in_bounds(game, coords->x, coords->y)) {
        int max_x;
        int max_y;
        getmaxyx(game->window, max_y, max_x);

        const int x_left_bound   = (max_x - game->game_max_x) / 2;
        const int x_right_bound  = (max_x + game->game_max_x) / 2;
        const int y_top_bound    = (max_y - game->game_max_y) / 2;
        const int y_bottom_bound = (max_y + game->game_max_y) / 2;

        if (coords->x + length >= x_right_bound) {
            coords->x -= (length + 2);
        } else if (coords->x <= x_left_bound) {
            coords->x = x_left_bound + 2;
        }

        if (coords->y >= y_bottom_bound) {
            coords->y -= 2;
        } else if (coords->y <= y_top_bound) {
            coords->y += 2;
        }
    }
}

static void game_clear_message(GameData *game, size_t index)
{
    memset(&game->messages[index], 0, sizeof(GameMessage));
}

static void game_clear_all_messages(GameData *game)
{
    for (size_t i = 0; i < game->messages_size; ++i) {
        game_clear_message(game, i);
    }
}

static GameMessage *game_get_new_message_holder(GameData *game)
{
    size_t i;

    for (i = 0; i < game->messages_size; ++i) {
        GameMessage *m = &game->messages[i];

        if (m->length == 0) {
            break;
        }
    }

    if (i == game->messages_size) {
        GameMessage *tmp = realloc(game->messages, sizeof(GameMessage) * (i + 1));

        if (tmp == NULL) {
            return NULL;
        }

        memset(&tmp[i], 0, sizeof(GameMessage));

        game->messages = tmp;
        game->messages_size = i + 1;
    }

    return &game->messages[i];

}

int game_set_message(GameData *game, const char *message, size_t length, Direction dir, int attributes, int colour,
                     TIME_S timeout, const Coords *coords, bool sticky, bool priority)
{
    if (length == 0 || length > GAME_MAX_MESSAGE_SIZE) {
        return -1;
    }

    if (coords == NULL) {
        return -1;
    }

    int max_x;
    int max_y;
    getmaxyx(game->window, max_y, max_x);

    if (coords->x > max_x || coords->x < 0 || coords->y > max_y || coords->y < 0) {
        return -1;
    }

    GameMessage *m = game_get_new_message_holder(game);

    if (m == NULL) {
        return -1;
    }

    memcpy(m->message, message, length);
    m->message[length] = 0;

    m->length = length;
    m->timeout = timeout > 0 ? timeout : GAME_MESSAGE_DEFAULT_TIMEOUT;
    m->set_time = get_unix_time();
    m->attributes = attributes;
    m->colour = colour;
    m->direction = dir;
    m->coords = coords;
    m->sticky = sticky;
    m->priority = priority;

    m->original_coords = (Coords) {
        coords->x, coords->y
    };

    if (GAME_UTIL_DIRECTION_VALID(dir)) {
        game_fix_message_coords(game, dir, &m->original_coords, length);
    }

    return 0;
}

static int game_restart(GameData *game)
{
    if (game->cb_game_kill) {
        game->cb_game_kill(game, game->cb_game_kill_data);
    }

    game->update_interval = GAME_DEFAULT_UPDATE_INTERVAL;
    game->status = GS_Running;
    game->score = 0;
    game->level = 0;
    game->lives = 0;
    game->last_frame_time = 0;

    game_clear_all_messages(game);

    if (game_initialize_type(game, NULL, 0, false) == -1) {
        return -1;
    }

    return 0;
}

static void game_draw_help_bar(const GameData *game, WINDOW *win)
{
    int max_x;
    int max_y;
    getmaxyx(win, max_y, max_x);

    UNUSED_VAR(max_x);

    wmove(win, max_y - 1, 1);

    if (!game->is_multiplayer) {
        wprintw(win, "Pause: ");
        wattron(win, A_BOLD);
        wprintw(win, "F2  ");
        wattroff(win, A_BOLD);
    }

    wprintw(win, "Quit: ");
    wattron(win, A_BOLD);
    wprintw(win, "F9");
    wattroff(win, A_BOLD);
}

static void game_draw_border(const GameData *game, const int max_x, const int max_y)
{
    WINDOW *win = game->window;

    const int game_max_x = game->game_max_x;
    const int game_max_y = game->game_max_y;

    const int x = (max_x - game_max_x) / 2;
    const int y = (max_y - game_max_y) / 2;

    wattron(win, COLOR_PAIR(GAME_BORDER_COLOUR));

    mvwaddch(win, y, x, ACS_ULCORNER);
    mvwhline(win, y, x + 1, ACS_HLINE, game_max_x - 1);
    mvwvline(win, y + 1, x, ACS_VLINE, game_max_y - 1);
    mvwvline(win, y, x - 1, ACS_VLINE, game_max_y + 1);
    mvwaddch(win, y, x + game_max_x, ACS_URCORNER);
    mvwvline(win, y + 1, x + game_max_x, ACS_VLINE, game_max_y - 1);
    mvwvline(win, y, x + game_max_x + 1, ACS_VLINE, game_max_y + 1);
    mvwaddch(win, y + game_max_y, x, ACS_LLCORNER);
    mvwhline(win, y + game_max_y, x + 1, ACS_HLINE, game_max_x - 1);
    mvwaddch(win, y + game_max_y, x + game_max_x, ACS_LRCORNER);

    wattroff(win, COLOR_PAIR(GAME_BORDER_COLOUR));
}

static void game_draw_status(const GameData *game, const int max_x, const int max_y)
{
    WINDOW *win = game->window;

    int x = ((max_x - game->game_max_x) / 2) - 1;
    const int y = ((max_y - game->game_max_y) / 2) - 1;

    wattron(win, A_BOLD);

    if (game->show_score) {
        mvwprintw(win, y, x, "Score: %ld", game->score);
    }

    if (game->show_high_score) {
        mvwprintw(win, y + game->game_max_y + 2, x, "High Score: %zu", game->high_score);
    }

    x = ((max_x / 2) + (game->game_max_x / 2)) - 7;

    if (game->show_level) {
        mvwprintw(win, y, x, "Level: %zu", game->level);
    }

    if (game->show_lives) {
        mvwprintw(win, y + game->game_max_y + 2, x, "Lives: %d", game->lives);
    }

    wattroff(win, A_BOLD);
}

static void game_draw_game_over(const GameData *game)
{
    WINDOW *win = game->window;

    int max_x;
    int max_y;
    getmaxyx(win, max_y, max_x);

    const int x = max_x / 2;
    const int y = max_y / 2;

    const char *message;
    int colour = RED;

    if (game->is_multiplayer) {
        message = game->winner ? "You win!" : "You lose!";
        colour = game->winner ? YELLOW : RED;
    } else {
        message = "GAME OVER!";
    }

    size_t length = strlen(message);

    wattron(win, A_BOLD | COLOR_PAIR(colour));
    mvwprintw(win, y - 1, x - (length / 2), "%s", message);
    wattroff(win, A_BOLD | COLOR_PAIR(colour));

    if (!game->is_multiplayer) {
        message = "Press F5 to play again";
        length = strlen(message);

        mvwprintw(win, y + 1, x - (length / 2), "%s", message);
    }
}

static void game_draw_pause_screen(const GameData *game)
{
    WINDOW *win = game->window;

    int max_x;
    int max_y;
    getmaxyx(win, max_y, max_x);

    const int x = max_x / 2;
    const int y = max_y / 2;

    wattron(win, A_BOLD | COLOR_PAIR(YELLOW));
    mvwprintw(win, y, x - 3, "PAUSED");
    wattroff(win, A_BOLD | COLOR_PAIR(YELLOW));
}

static void game_draw_messages(GameData *game, bool priority)
{
    WINDOW *win = game->window;

    for (size_t i = 0; i < game->messages_size; ++i) {
        GameMessage *m = &game->messages[i];

        if (m->length == 0 || m->coords == NULL) {
            continue;
        }

        if (timed_out(m->set_time, m->timeout)) {
            game_clear_message(game, i);
            continue;
        }

        if (m->priority != priority) {
            continue;
        }

        if (!m->sticky) {
            wattron(win, m->attributes | COLOR_PAIR(m->colour));
            mvwprintw(win, m->original_coords.y, m->original_coords.x, "%s", m->message);
            wattroff(win, m->attributes | COLOR_PAIR(m->colour));
            continue;
        }

        Coords fixed_coords = {
            m->coords->x,
            m->coords->y
        };

        // TODO: we should only have to do this if the coordinates changed
        game_fix_message_coords(game, m->direction, &fixed_coords, m->length);

        wattron(win, m->attributes | COLOR_PAIR(m->colour));
        mvwprintw(win, fixed_coords.y, fixed_coords.x, "%s", m->message);
        wattroff(win, m->attributes | COLOR_PAIR(m->colour));
    }
}

static void game_update_state(GameData *game)
{
    if (game->cb_game_update_state == NULL) {
        return;
    }

    TIME_MS cur_time = get_time_millis();

    if (cur_time - game->last_frame_time > 500) {
        game->last_frame_time = cur_time;
    }

    size_t iterations = (cur_time - game->last_frame_time) / game->update_interval;

    for (size_t i = 0; i < iterations; ++i) {
        game->cb_game_update_state(game, game->cb_game_update_state_data);
        game->last_frame_time += game->update_interval;
    }
}

static void game_onDraw(ToxWindow *self, Toxic *toxic)
{
    UNUSED_VAR(toxic);   // Note: This function is not thread safe if we ever need to use `toxic`

    if (self == NULL) {
        fprintf(stderr, "game_onDraw null param\n");
        return;
    }

    GameData *game = self->game;

    game_draw_help_bar(game, self->window);
    draw_window_bar(self, toxic->windows);

    curs_set(0);

    int max_x;
    int max_y;
    getmaxyx(game->window, max_y, max_x);

    wclear(game->window);

    game_draw_border(game, max_x, max_y);

    game_draw_messages(game, false);

    if (game->cb_game_render_window) {
        game->cb_game_render_window(game, game->window, game->cb_game_render_window_data);
    }

    game_draw_status(game, max_x, max_y);

    switch (game->status) {
        case GS_Running: {
            game_update_state(game);
            break;
        }

        case GS_Paused: {
            game_draw_pause_screen(game);
            break;
        }

        case GS_Finished: {
            game_draw_game_over(game);
            break;
        }

        default: {
            break;
        }
    }

    game_draw_messages(game, true);
}

static bool game_onKey(ToxWindow *self, Toxic *toxic, wint_t key, bool is_printable)
{
    UNUSED_VAR(is_printable);

    GameData *game = self->game;

    if (key == KEY_F(9)) {
        pthread_mutex_lock(&Winthread.lock);
        game_kill(self, toxic->windows, toxic->c_config);
        pthread_mutex_unlock(&Winthread.lock);
        return true;
    }

    if (key == KEY_F(2) && !game->is_multiplayer) {
        game_toggle_pause(self->game);
        return true;
    }

    if (!game->is_multiplayer && game->status == GS_Finished && key == KEY_F(5)) {
        if (game_restart(self->game) == -1) {
            fprintf(stderr, "Warning: game_restart() failed\n");
        }

        return true;
    }

    if (game->cb_game_key_press) {
        if (game->is_multiplayer) {
            pthread_mutex_lock(&Winthread.lock);  // we use the tox instance when we send packets
        }

        game->cb_game_key_press(game, key, game->cb_game_key_press_data);

        if (game->is_multiplayer) {
            pthread_mutex_unlock(&Winthread.lock);
        }
    }

    return true;
}

static void game_onInit(ToxWindow *self, Toxic *toxic)
{
    UNUSED_VAR(toxic);

    if (self == NULL) {
        return;
    }

    int max_x;
    int max_y;
    getmaxyx(self->window, max_y, max_x);

    if (max_y <= 0 || max_x <= 0) {
        exit_toxic_err("failed in game_onInit", FATALERR_CURSES);
    }

    self->window_bar = subwin(self->window, WINDOW_BAR_HEIGHT, max_x, max_y - 2, 0);
}

/*
 * Byte 0:   Version
 * Byte 1:   Game type
 * Byte 2-5: Game ID
 * Byte 6-*  Game data
 */
static void game_onPacket(ToxWindow *self, Toxic *toxic, uint32_t friendnumber, const uint8_t *data, size_t length)
{
    UNUSED_VAR(toxic);

    GameData *game = self->game;

    if (friendnumber != self->num) {
        return;
    }

    if (data == NULL) {
        return;
    }

    if (length < GAME_PACKET_HEADER_SIZE || length > GAME_MAX_PACKET_SIZE) {
        return;
    }

    if (data[0] != GAME_NETWORKING_VERSION) {
        fprintf(stderr, "Game packet rejected: wrong networking version (got %d, expected %d)\n", data[0],
                GAME_NETWORKING_VERSION);
        return;
    }

    GameType type = (GameType)data[1];

    if (game->type != type) {
        return;
    }

    uint32_t id;
    game_util_unpack_u32(data + 2, &id);

    if (game->id != id) {
        return;
    }

    data += GAME_PACKET_HEADER_SIZE;
    length -= GAME_PACKET_HEADER_SIZE;

    if (game->cb_game_on_packet) {
        game->cb_game_on_packet(game, data, length, game->cb_game_on_packet_data);
    }
}

static ToxWindow *game_new_window(Tox *tox, GameType type, uint32_t friendnumber)
{
    const char *window_name = game_get_name_string(type);

    if (window_name == NULL) {
        return NULL;
    }

    ToxWindow *ret = calloc(1, sizeof(ToxWindow));

    if (ret == NULL) {
        return NULL;
    }

    ret->num = friendnumber;
    ret->type = WINDOW_TYPE_GAME;

    ret->onInit = &game_onInit;
    ret->onDraw = &game_onDraw;
    ret->onKey = &game_onKey;
    ret->onGameData = &game_onPacket;

    ret->game = calloc(1, sizeof(GameData));

    if (ret->game == NULL) {
        free(ret);
        return NULL;
    }

    ret->active_box = -1;

    if (game_type_is_multi_only(type)) {
        char nick[TOX_MAX_NAME_LENGTH];
        get_nick_truncate(tox, nick, friendnumber);

        char buf[sizeof(nick) + sizeof(ret->name) + 4];
        snprintf(buf, sizeof(buf), "%s (%s)", window_name, nick);

        const size_t name_size = sizeof(ret->name);

        buf[name_size - 1] = '\0';

        snprintf(ret->name, name_size, "%s", buf);
    } else {
        snprintf(ret->name, sizeof(ret->name), "%s", window_name);
    }

    return ret;
}

bool game_coordinates_in_bounds(const GameData *game, int x, int y)
{
    const int game_max_x = game->game_max_x;
    const int game_max_y = game->game_max_y;

    int max_x;
    int max_y;
    getmaxyx(game->window, max_y, max_x);

    const int x_left_bound   = (max_x - game_max_x) / 2;
    const int x_right_bound  = (max_x + game_max_x) / 2;
    const int y_top_bound    = (max_y - game_max_y) / 2;
    const int y_bottom_bound = (max_y + game_max_y) / 2;

    return x > x_left_bound && x < x_right_bound && y > y_top_bound && y < y_bottom_bound;
}

void game_random_coords(const GameData *game, Coords *coords)
{
    const int game_max_x = game->game_max_x;
    const int game_max_y = game->game_max_y;

    int max_x;
    int max_y;
    getmaxyx(game->window, max_y, max_x);

    const int x_left_bound   = ((max_x - game_max_x) / 2) + 1;
    const int x_right_bound  = ((max_x + game_max_x) / 2) - 1;
    const int y_top_bound    = ((max_y - game_max_y) / 2) + 1;
    const int y_bottom_bound = ((max_y + game_max_y) / 2) - 1;

    coords->x = (int)rand_range_not_secure(x_right_bound - x_left_bound + 1) + x_left_bound;
    coords->y = (int)rand_range_not_secure(y_bottom_bound - y_top_bound + 1) + y_top_bound;
}

void game_max_x_y(const GameData *game, int *x, int *y)
{
    getmaxyx(game->window, *y, *x);
}

int game_y_bottom_bound(const GameData *game)
{
    int max_x;
    int max_y;
    getmaxyx(game->window, max_y, max_x);

    UNUSED_VAR(max_x);

    return ((max_y + game->game_max_y) / 2) - 1;
}

int game_y_top_bound(const GameData *game)
{
    int max_x;
    int max_y;
    getmaxyx(game->window, max_y, max_x);

    UNUSED_VAR(max_x);

    return ((max_y - game->game_max_y) / 2) + 1;
}

int game_x_right_bound(const GameData *game)
{
    int max_x;
    int max_y;
    getmaxyx(game->window, max_y, max_x);

    UNUSED_VAR(max_y);

    return ((max_x + game->game_max_x) / 2) - 1;
}

int game_x_left_bound(const GameData *game)
{
    int max_x;
    int max_y;
    getmaxyx(game->window, max_y, max_x);

    UNUSED_VAR(max_y);

    return ((max_x - game->game_max_x) / 2) + 1;
}

void game_show_score(GameData *game, bool show_score)
{
    game->show_score = show_score;
}

void game_show_high_score(GameData *game, bool show_high_score)
{
    game->show_high_score = show_high_score;
}

void game_show_lives(GameData *game, bool show_lives)
{
    game->show_lives = show_lives;
}

void game_show_level(GameData *game, bool show_level)
{
    game->show_level = show_level;
}

void game_update_score(GameData *game, long int points)
{
    game->score += points;

    if (game->score > game->high_score) {
        game->high_score = game->score;
    }
}

void game_set_score(GameData *game, long int val)
{
    game->score = val;
}

long int game_get_score(const GameData *game)
{
    return game->score;
}

void game_increment_level(GameData *game)
{
    ++game->level;
}

void game_update_lives(GameData *game, int lives)
{
    game->lives += lives;
}

int game_get_lives(const GameData *game)
{
    return game->lives;
}

size_t game_get_current_level(const GameData *game)
{
    return game->level;
}

void game_set_status(GameData *game, GameStatus status)
{
    if (status < GS_Invalid) {
        game->status = status;
    }
}

void game_set_winner(GameData *game, bool winner)
{
    if (game->status == GS_Finished) {
        game->winner = winner;
    }
}

void game_set_update_interval(GameData *game, TIME_MS update_interval)
{
    game->update_interval = MIN(update_interval, GAME_MAX_UPDATE_INTERVAL);
}

bool game_do_object_state_update(const GameData *game, TIME_MS current_time, TIME_MS last_moved_time, TIME_MS speed)
{
    TIME_MS delta = (current_time - last_moved_time) * speed;
    return delta > game->update_interval * GAME_OBJECT_UPDATE_INTERVAL_MULTIPLIER;
}

void game_set_cb_update_state(GameData *game, cb_game_update_state *func, void *cb_data)
{
    game->cb_game_update_state = func;
    game->cb_game_update_state_data = cb_data;
}

void game_set_cb_on_keypress(GameData *game, cb_game_key_press *func, void *cb_data)
{
    game->cb_game_key_press = func;
    game->cb_game_key_press_data = cb_data;
}

void game_set_cb_render_window(GameData *game, cb_game_render_window *func, void *cb_data)
{
    game->cb_game_render_window = func;
    game->cb_game_render_window_data = cb_data;
}

void game_set_cb_kill(GameData *game, cb_game_kill *func, void *cb_data)
{
    game->cb_game_kill = func;
    game->cb_game_kill_data = cb_data;
}

void game_set_cb_on_pause(GameData *game, cb_game_pause *func, void *cb_data)
{
    game->cb_game_pause = func;
    game->cb_game_pause_data = cb_data;
}

void game_set_cb_on_packet(GameData *game, cb_game_on_packet *func, void *cb_data)
{
    game->cb_game_on_packet = func;
    game->cb_game_on_packet_data = cb_data;
}

/*
 * Wraps `packet` in a header comprised of the custom packet type, game type and game id.
 */
static int game_packet_wrap(const GameData *game, uint8_t *packet, size_t size, GamePacketType packet_type)
{
    if (size < GAME_PACKET_HEADER_SIZE + 1) {
        return -1;
    }

    if (packet_type != GP_Invite && packet_type != GP_Data) {
        return -1;
    }

    packet[0] = packet_type == GP_Data ? CUSTOM_PACKET_GAME_DATA : CUSTOM_PACKET_GAME_INVITE;
    packet[1] = GAME_NETWORKING_VERSION;
    packet[2] = game->type;

    game_util_pack_u32(packet + 3, game->id);

    return 0;
}

int game_packet_send(const GameData *game, const uint8_t *data, size_t length, GamePacketType packet_type)
{
    if (length > GAME_MAX_DATA_SIZE) {
        return -1;
    }

    uint8_t packet[GAME_MAX_PACKET_SIZE];

    if (game_packet_wrap(game, packet, sizeof(packet), packet_type) == -1) {
        return -1;
    }

    size_t packet_length = 1 + GAME_PACKET_HEADER_SIZE;  // 1 extra byte for custom packet type

    memcpy(packet + 1 + GAME_PACKET_HEADER_SIZE, data, length);
    packet_length += length;

    Tox_Err_Friend_Custom_Packet err;

    if (!tox_friend_send_lossless_packet(game->toxic->tox, game->friend_number, packet, packet_length, &err)) {
        fprintf(stderr, "failed to send game packet: error %d\n", err);
        return -1;
    }

    return 0;
}

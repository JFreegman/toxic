/*  game_snake.c
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
#include "game_snake.h"
#include "misc_tools.h"

#define SNAKE_MAX_SNAKE_LENGTH     (GAME_MAX_SQUARE_X * GAME_MAX_SQUARE_Y)
#define SNAKE_AGENT_MAX_LIST_SIZE  (GAME_MAX_SQUARE_X * GAME_MAX_SQUARE_Y)

#define SNAKE_DEFAULT_SNAKE_SPEED 6
#define SNAKE_DEFAULT_AGENT_SPEED 1
#define SNAKE_MAX_SNAKE_SPEED 12
#define SNAKE_MAX_AGENT_SPEED (SNAKE_MAX_SNAKE_SPEED / 2)

#define SNAKE_DEFAULT_UPDATE_INTERVAL 20

/* How often we update frames independent of the speed of the game or objects */
#define SNAKE_FRAME_DRAW_SPEED 5

/* How long a regular message stays on the screen */
#define SNAKE_DEFAULT_MESSAGE_TIMER 5

/* Increment snake speed by 1 every time level increases by this amount */
#define SNAKE_LEVEL_SPEED_INTERVAL 5

/* Increment level by 1 every time snake eats this many foods */
#define SNAKE_LEVEL_UP_FOOD_LIMIT 4

/* Increment agent speed by 1 every time level increases by this amount */
#define SNAKE_AGENT_LEVEL_SPEED_INTERVAL 2

/* Points multiplier for getting a powerup */
#define SNAKE_POWERUP_BONUS_MULTIPLIER 5

/* Extra bonus for running over a glowing agent */
#define SNAKE_AGENT_GLOWING_MULTIPLIER 2

/* Agents begin glowing if their speed is greater than this */
#define SNAKE_AGENT_GLOWING_SPEED (SNAKE_DEFAULT_AGENT_SPEED + 2)

/* A new powerup is placed on the board after this many seconds since last one wore off */
#define SNAKE_POWERUP_INTERVAL 45

/* How long a powerup lasts */
#define SNAKE_POWERUP_TIMER 12

/* Number of key presses to queue; one key press is retrieved per state update */
#define SNAKE_KEY_PRESS_QUEUE_SIZE 3

/* These decide how many points to decay and how often to penalize camping for powerups */
#define SNAKE_DECAY_POINTS_INTERVAL  1
#define SNAKE_DECAY_POINTS_FRACTION 10

#define SNAKE_HEAD_COLOUR       GREEN

#define SNAKE_DEAD_BODY_CHAR    'o'
#define SNAKE_DEAD_BODY_COLOUR  RED

#define SNAKE_BODY_COLOUR       CYAN
#define SNAKE_BODY_CHAR         'o'

#define SNAKE_FOOD_COLOUR    YELLOW
#define SNAKE_FOOD_CHAR      '*'

#define SNAKE_AGENT_NORMAL_COLOUR   RED
#define SNAKE_AGENT_GLOWING_COLOUR  GREEN
#define SNAKE_AGENT_NORMAL_CHAR     'x'
#define SNAKE_AGENT_GLOWING_CHAR    'X'

#define SNAKE_POWERUP_CHAR   'P'

typedef struct NasaAgent {
    Coords      coords;
    bool        is_alive;
    bool        is_glowing;
    TIME_MS     last_time_moved;
    size_t      speed;
    char        display_char;
    int         colour;
    int         attributes;
} NasaAgent;

typedef struct Snake {
    Coords      coords;
    char        display_char;
    int         colour;
    int         attributes;
} Snake;

typedef struct SnakeState {
    Snake      *snake;
    size_t      snake_length;
    size_t      snake_speed;
    TIME_MS     snake_time_last_moved;
    bool        has_powerup;
    Direction   direction;

    Coords      powerup;
    TIME_S      powerup_timer;
    TIME_S      last_powerup_time;

    Coords      food;

    NasaAgent  *agents;
    size_t      agent_list_size;

    TIME_S      last_time_points_decayed;
    TIME_S      pause_time;

    int         key_press_queue[SNAKE_KEY_PRESS_QUEUE_SIZE];
    size_t      keys_skip_counter;

    TIME_MS     last_draw_update;
    bool        game_over;
} SnakeState;


static void snake_create_points_message(GameData *game, Direction dir, long int points, const Coords *coords)
{
    char buf[GAME_MAX_MESSAGE_SIZE + 1];
    snprintf(buf, sizeof(buf), "%ld", points);

    if (game_set_message(game, buf, strlen(buf), dir, A_BOLD, WHITE, 0, coords, false, false) == -1) {
        fprintf(stderr, "failed to set points message\n");
    }
}

static void snake_create_message(GameData *game, Direction dir, const char *message, int attributes,
                                 int colour, TIME_S timeout, const Coords *coords, bool priority)
{
    if (game_set_message(game, message, strlen(message), dir, attributes, colour, timeout, coords, false, priority) == -1) {
        fprintf(stderr, "failed to set message\n");
    }
}

static Coords *snake_get_head_coords(const SnakeState *state)
{
    return &state->snake[0].coords;
}

static void snake_set_head_char(SnakeState *state)
{
    Snake *snake_head = &state->snake[0];

    switch (state->direction) {
        case NORTH:
            snake_head->display_char = '^';
            break;

        case SOUTH:
            snake_head->display_char = 'v';
            break;

        case EAST:
            snake_head->display_char = '>';
            return;

        case WEST:
            snake_head->display_char = '<';
            break;

        default:
            snake_head->display_char = '?';
            break;
    }
}

static bool snake_validate_direction(const SnakeState *state, Direction dir)
{
    if (!GAME_UTIL_DIRECTION_VALID(dir)) {
        return false;
    }

    const int diff = abs((int)state->direction - (int)dir);

    return diff != 1;
}

static void snake_update_direction(SnakeState *state)
{
    for (size_t i = 0; i < SNAKE_KEY_PRESS_QUEUE_SIZE; ++i) {
        int key = state->key_press_queue[i];

        if (key == 0) {
            continue;
        }

        Direction dir = game_util_get_direction(key);

        if (!GAME_UTIL_DIRECTION_VALID(dir)) {
            state->key_press_queue[i] = 0;
            continue;
        }

        if (snake_validate_direction(state, dir)) {
            state->direction = dir;
            snake_set_head_char(state);
            state->key_press_queue[i] = 0;
            state->keys_skip_counter = 0;
            break;
        }

        if (state->keys_skip_counter++ >= SNAKE_KEY_PRESS_QUEUE_SIZE) {
            state->keys_skip_counter = 0;
            memset(state->key_press_queue, 0, sizeof(state->key_press_queue));
        }
    }
}

static void snake_set_key_press(SnakeState *state, int key)
{
    for (size_t i = 0; i < SNAKE_KEY_PRESS_QUEUE_SIZE; ++i) {
        if (state->key_press_queue[i] == 0) {
            state->key_press_queue[i] = key;
            return;
        }
    }

    memset(state->key_press_queue, 0, sizeof(state->key_press_queue));
    state->key_press_queue[0] = key;
}

static void snake_update_score(GameData *game, const SnakeState *state, long int points)
{
    const Coords *head = snake_get_head_coords(state);

    snake_create_points_message(game, state->direction, points, head);

    game_update_score(game, points);
}

static long int snake_get_move_points(const SnakeState *state)
{
    return state->snake_length + (2 * state->snake_speed);
}

/* Return true if snake body is occupying given coordinates */
static bool snake_coords_contain_body(const SnakeState *state, const Coords *coords)
{
    for (size_t i = 1; i < state->snake_length; ++i) {
        Coords snake_coords = state->snake[i].coords;

        if (COORDINATES_OVERLAP(coords->x, coords->y, snake_coords.x, snake_coords.y)) {
            return true;
        }
    }

    return false;
}

static bool snake_self_consume(const SnakeState *state)
{
    const Coords *head = snake_get_head_coords(state);
    return snake_coords_contain_body(state, head);
}

/*
 * Returns a pointer to the agent at x and y coordinates.
 *
 * Returns NULL if no living agent is at coords.
 */
static NasaAgent *snake_get_agent_at_coords(const SnakeState *state, const Coords *coords)
{
    for (size_t i = 0; i < state->agent_list_size; ++i) {
        NasaAgent *agent = &state->agents[i];

        if (!agent->is_alive) {
            continue;
        }

        if (COORDINATES_OVERLAP(coords->x, coords->y, agent->coords.x, agent->coords.y)) {
            return agent;
        }
    }

    return NULL;
}

/*
 * Return true if snake got caught by an agent and doesn't have a powerup.
 *
 * If snake runs over agent and does have a powerup the agent is killed and score updated.
 */
static bool snake_agent_caught(GameData *game, SnakeState *state, const Coords *coords)
{
    NasaAgent *agent = snake_get_agent_at_coords(state, coords);

    if (agent == NULL) {
        return false;
    }

    if (!state->has_powerup) {
        return true;
    }

    agent->is_alive = false;

    long int points = snake_get_move_points(state) * (agent->speed + 1);

    if (agent->is_glowing) {
        points *= SNAKE_AGENT_GLOWING_MULTIPLIER;
    }

    snake_update_score(game, state, points);

    return false;
}

static bool snake_state_valid(GameData *game, SnakeState *state)
{
    const Coords *head = snake_get_head_coords(state);

    if (!game_coordinates_in_bounds(game, head->x, head->y)) {
        snake_create_message(game, state->direction, "Ouch!", A_BOLD, WHITE, SNAKE_DEFAULT_MESSAGE_TIMER, head, true);
        return false;
    }

    if (snake_self_consume(state)) {
        snake_create_message(game, state->direction, "Tastes like chicken", A_BOLD, WHITE, SNAKE_DEFAULT_MESSAGE_TIMER, head,
                             true);
        return false;
    }

    if (snake_agent_caught(game, state, head)) {
        snake_create_message(game, state->direction, "ARGH they got me!", A_BOLD, WHITE, SNAKE_DEFAULT_MESSAGE_TIMER, head,
                             true);
        return false;
    }

    return true;
}

/*
 * Sets colour and attributes for entire snake except head.
 *
 * If colour is set to -1 a random colour is chosen for each body part.
 */
static void snake_set_body_attributes(Snake *snake, size_t length, int colour, int attributes)
{
    for (size_t i = 1; i < length; ++i) {
        Snake *body = &snake[i];

        if (colour == -1) {
            body->colour = game_util_random_colour();
        } else {
            body->colour = colour;
        }

        body->attributes = attributes;
    }
}

static void snake_move_body(SnakeState *state)
{
    for (size_t i = state->snake_length - 1; i > 0; --i) {
        Coords *curr = &state->snake[i].coords;
        Coords prev = state->snake[i - 1].coords;
        curr->x = prev.x;
        curr->y = prev.y;
    }
}

static void snake_move_head(SnakeState *state)
{
    Coords *head = snake_get_head_coords(state);
    game_util_move_coords(state->direction, head);
}

static void snake_grow(SnakeState *state)
{
    size_t index = state->snake_length;

    if (index >= SNAKE_MAX_SNAKE_LENGTH) {
        return;
    }

    state->snake[index].coords.x = -1;
    state->snake[index].coords.y = -1;
    state->snake[index].display_char = SNAKE_BODY_CHAR;
    state->snake[index].colour = SNAKE_BODY_COLOUR;
    state->snake[index].attributes = A_BOLD;

    state->snake_length = index + 1;
}

static long int snake_check_food(const GameData *game, SnakeState *state)
{
    Coords *food = &state->food;
    const Coords *head = snake_get_head_coords(state);

    if (!COORDINATES_OVERLAP(head->x, head->y, food->x, food->y)) {
        return 0;
    }

    snake_grow(state);

    game_random_coords(game, food);

    return snake_get_move_points(state);
}

static long int snake_check_powerup(GameData *game, SnakeState *state)
{
    Coords *powerup = &state->powerup;
    const Coords *head = snake_get_head_coords(state);

    if (!COORDINATES_OVERLAP(head->x, head->y, powerup->x, powerup->y)) {
        return 0;
    }

    snake_create_message(game, state->direction, "AAAAA", A_BOLD, RED, 2, head, false);

    TIME_S t = get_unix_time();

    state->has_powerup = true;
    state->powerup_timer = t;

    powerup->x = -1;
    powerup->y = -1;

    return snake_get_move_points(state) * SNAKE_POWERUP_BONUS_MULTIPLIER;
}

/*
 * Returns the first unoccupied index in agent array.
 */
static size_t snake_get_empty_agent_index(const NasaAgent *agents)
{
    for (size_t i = 0; i < SNAKE_AGENT_MAX_LIST_SIZE; ++i) {
        if (!agents[i].is_alive) {
            return i;
        }
    }

    fprintf(stderr, "Warning: Agent array is full. This should be impossible\n");

    return 0;
}

static void snake_initialize_agent(SnakeState *state, const Coords *coords)
{
    size_t idx = snake_get_empty_agent_index(state->agents);

    if ((idx >= state->agent_list_size) && (idx + 1 <= SNAKE_AGENT_MAX_LIST_SIZE)) {
        state->agent_list_size = idx + 1;
    }

    NasaAgent *agent = &state->agents[idx];

    agent->coords = (Coords) {
        coords->x,
               coords->y
    };

    agent->is_alive = true;
    agent->is_glowing = false;
    agent->display_char = SNAKE_AGENT_NORMAL_CHAR;
    agent->colour = SNAKE_AGENT_NORMAL_COLOUR;
    agent->attributes = A_BOLD;
    agent->last_time_moved = 0;
    agent->speed = SNAKE_DEFAULT_AGENT_SPEED;
}

static void snake_dispatch_new_agent(const GameData *game, SnakeState *state)
{
    Coords new_coords;
    const Coords *head = snake_get_head_coords(state);

    size_t tries = 0;

    do {
        if (tries++ >= 10) {
            return;
        }

        game_random_coords(game, &new_coords);
    } while (COORDINATES_OVERLAP(new_coords.x, new_coords.y, head->x, head->y)
             || snake_get_agent_at_coords(state, &new_coords) != NULL);

    snake_initialize_agent(state, &new_coords);
}

static void snake_place_powerup(const GameData *game, SnakeState *state)
{
    Coords *powerup = &state->powerup;

    if (powerup->x != -1) {
        return;
    }

    if (!timed_out(state->last_powerup_time, SNAKE_POWERUP_INTERVAL)) {
        return;
    }

    game_random_coords(game, powerup);
}

static void snake_do_powerup(const GameData *game, SnakeState *state)
{
    if (!state->has_powerup) {
        snake_place_powerup(game, state);
        return;
    }

    if (timed_out(state->powerup_timer, SNAKE_POWERUP_TIMER)) {
        state->last_powerup_time = get_unix_time();
        state->has_powerup = false;
        snake_set_body_attributes(state->snake, state->snake_length, SNAKE_BODY_COLOUR, A_BOLD);
    }
}

static void snake_decay_points(GameData *game, SnakeState *state)
{

    long int score = game_get_score(game);
    long int decay = snake_get_move_points(state) / SNAKE_DECAY_POINTS_FRACTION;

    if (score > decay && timed_out(state->last_time_points_decayed, SNAKE_DECAY_POINTS_INTERVAL)) {
        game_update_score(game, -decay);
        state->last_time_points_decayed = get_unix_time();
    }
}

static void snake_do_points_update(GameData *game, SnakeState *state, long int points)
{
    snake_update_score(game, state, points);

    if (state->snake_length % SNAKE_LEVEL_UP_FOOD_LIMIT != 0) {
        return;
    }

    game_increment_level(game);

    size_t level = game_get_current_level(game);

    if (level % SNAKE_LEVEL_SPEED_INTERVAL == 0 && state->snake_speed < SNAKE_MAX_SNAKE_SPEED) {
        ++state->snake_speed;
    }

    if (level % SNAKE_AGENT_LEVEL_SPEED_INTERVAL == 0) {
        for (size_t i = 0; i < state->agent_list_size; ++i) {
            NasaAgent *agent = &state->agents[i];

            if (!agent->is_alive) {
                continue;
            }

            if (agent->speed < SNAKE_MAX_AGENT_SPEED) {
                ++agent->speed;
            }

            if (agent->speed > SNAKE_AGENT_GLOWING_SPEED && !agent->is_glowing) {
                agent->is_glowing = true;
                agent->display_char = SNAKE_AGENT_GLOWING_CHAR;
                agent->colour = SNAKE_AGENT_GLOWING_COLOUR;
                snake_create_message(game, state->direction, "*glows*", A_BOLD, GREEN, 2, &agent->coords, false);
            }
        }
    }

    snake_dispatch_new_agent(game, state);
}

static void snake_game_over(SnakeState *state)
{
    state->game_over = true;
    state->has_powerup = false;
    state->snake[0].colour = SNAKE_DEAD_BODY_COLOUR;
    state->snake[0].attributes = A_BOLD | A_BLINK;

    snake_set_body_attributes(state->snake, state->snake_length, SNAKE_DEAD_BODY_COLOUR, A_BOLD | A_BLINK);
}

static void snake_move(GameData *game, SnakeState *state, TIME_MS cur_time)
{
    const TIME_MS real_speed = GAME_UTIL_REAL_SPEED(state->direction, state->snake_speed);

    if (!game_do_object_state_update(game, cur_time, state->snake_time_last_moved, real_speed)) {
        return;
    }

    state->snake_time_last_moved = cur_time;

    snake_update_direction(state);
    snake_move_body(state);
    snake_move_head(state);

    if (!snake_state_valid(game, state)) {
        snake_game_over(state);
        game_set_status(game, GS_Finished);
        return;
    }

    long int points = snake_check_food(game, state) + snake_check_powerup(game, state);

    if (points > 0) {
        snake_do_points_update(game, state, points);
    }
}

/*
 * Attempts to move every agent in list.
 *
 * If an agent is normal it will move in a random direction. If it's glowing it will
 * move towards the snake.
 */
static void snake_agent_move(GameData *game, SnakeState *state, TIME_MS cur_time)
{
    const Coords *head = snake_get_head_coords(state);

    for (size_t i = 0; i < state->agent_list_size; ++i) {
        NasaAgent *agent = &state->agents[i];

        if (!agent->is_alive) {
            continue;
        }

        Coords *coords = &agent->coords;

        Coords new_coords = (Coords) {
            coords->x,
                   coords->y
        };

        Direction dir = !agent->is_glowing ? game_util_random_direction()
                        : game_util_move_towards(coords, head, state->has_powerup);

        const TIME_MS real_speed = agent->is_glowing ? GAME_UTIL_REAL_SPEED(dir, agent->speed) : agent->speed;

        if (!game_do_object_state_update(game, cur_time, agent->last_time_moved, real_speed)) {
            continue;
        }

        agent->last_time_moved = cur_time;

        game_util_move_coords(dir, &new_coords);

        if (!game_coordinates_in_bounds(game, new_coords.x, new_coords.y)) {
            continue;
        }

        if (snake_coords_contain_body(state, &new_coords)) {
            continue;
        }

        if (snake_get_agent_at_coords(state, &new_coords) != NULL) {
            continue;
        }

        coords->x = new_coords.x;
        coords->y = new_coords.y;

        if (!state->has_powerup && COORDINATES_OVERLAP(head->x, head->y, new_coords.x, new_coords.y)) {
            snake_game_over(state);
            game_set_status(game, GS_Finished);
            return;
        }
    }
}

static void snake_update_frames(const GameData *game, SnakeState *state, TIME_MS cur_time)
{
    if (!game_do_object_state_update(game, cur_time, state->last_draw_update, SNAKE_FRAME_DRAW_SPEED)) {
        return;
    }

    state->last_draw_update = cur_time;

    if (state->has_powerup) {
        const int time_left = SNAKE_POWERUP_TIMER - (get_unix_time() - state->powerup_timer);

        if (time_left <= 5 && time_left % 2 == 0) {
            snake_set_body_attributes(state->snake, state->snake_length, SNAKE_BODY_COLOUR, A_BOLD);
        } else {
            snake_set_body_attributes(state->snake, state->snake_length, -1, A_BOLD);
        }
    }
}

static void snake_draw_self(WINDOW *win, const SnakeState *state)
{
    for (size_t i = 0; i < state->snake_length; ++i) {
        const Snake *body = &state->snake[i];

        if (body->coords.x <= 0 || body->coords.y <= 0) {
            continue;
        }

        wattron(win, body->attributes | COLOR_PAIR(body->colour));
        mvwaddch(win, body->coords.y, body->coords.x, body->display_char);
        wattroff(win, body->attributes | COLOR_PAIR(body->colour));
    }
}

static void snake_draw_food(WINDOW *win, const SnakeState *state)
{
    wattron(win, A_BOLD | COLOR_PAIR(SNAKE_FOOD_COLOUR));
    mvwaddch(win, state->food.y, state->food.x, SNAKE_FOOD_CHAR);
    wattroff(win, A_BOLD | COLOR_PAIR(SNAKE_FOOD_COLOUR));
}

static void snake_draw_agent(WINDOW *win, const SnakeState *state)
{
    for (size_t i = 0; i < state->agent_list_size; ++i) {
        NasaAgent agent = state->agents[i];

        if (agent.is_alive) {
            Coords coords = agent.coords;

            wattron(win, agent.attributes | COLOR_PAIR(agent.colour));
            mvwaddch(win, coords.y, coords.x, agent.display_char);
            wattroff(win, agent.attributes | COLOR_PAIR(agent.colour));
        }
    }
}

static void snake_draw_powerup(WINDOW *win, const SnakeState *state)
{
    Coords powerup = state->powerup;

    if (powerup.x != -1) {
        int colour = game_util_random_colour();

        wattron(win, A_BOLD | COLOR_PAIR(colour));
        mvwaddch(win, powerup.y, powerup.x, SNAKE_POWERUP_CHAR);
        wattroff(win, A_BOLD | COLOR_PAIR(colour));
    }
}

void snake_cb_update_game_state(GameData *game, void *cb_data)
{
    if (!cb_data) {
        return;
    }

    SnakeState *state = (SnakeState *)cb_data;

    TIME_MS cur_time = get_time_millis();

    snake_do_powerup(game, state);
    snake_agent_move(game, state, cur_time);
    snake_move(game, state, cur_time);
    snake_decay_points(game, state);

    if (!state->game_over) {
        snake_update_frames(game, state, cur_time);
    }
}

void snake_cb_render_window(GameData *game, WINDOW *win, void *cb_data)
{
    if (!cb_data) {
        return;
    }

    SnakeState *state = (SnakeState *)cb_data;

    snake_draw_food(win, state);
    snake_draw_powerup(win, state);
    snake_draw_agent(win, state);
    snake_draw_self(win, state);
}

void snake_cb_kill(GameData *game, void *cb_data)
{
    if (!cb_data) {
        return;
    }

    SnakeState *state = (SnakeState *)cb_data;

    free(state->snake);
    free(state->agents);
    free(state);

    game_set_cb_update_state(game, NULL, NULL);
    game_set_cb_render_window(game, NULL, NULL);
    game_set_cb_kill(game, NULL, NULL);
    game_set_cb_on_pause(game, NULL, NULL);
}

void snake_cb_on_keypress(GameData *game, int key, void *cb_data)
{
    if (!cb_data) {
        return;
    }

    SnakeState *state = (SnakeState *)cb_data;

    snake_set_key_press(state, key);
}

void snake_cb_pause(GameData *game, bool is_paused, void *cb_data)
{
    UNUSED_VAR(game);

    if (!cb_data) {
        return;
    }

    SnakeState *state = (SnakeState *)cb_data;

    TIME_S t = get_unix_time();

    if (is_paused) {
        state->pause_time = t;
    } else {
        state->powerup_timer += (t - state->pause_time);
        state->last_powerup_time += (t - state->pause_time);
    }
}

static void snake_initialize_snake_head(const GameData *game, Snake *snake)
{
    int max_x;
    int max_y;
    game_max_x_y(game, &max_x, &max_y);

    snake[0].coords.x = max_x / 2;
    snake[0].coords.y = max_y / 2;
    snake[0].colour = SNAKE_HEAD_COLOUR;
    snake[0].attributes = A_BOLD;
}

int snake_initialize(GameData *game)
{
    if (game_set_window_shape(game, GW_ShapeSquare) == -1) {
        return -1;
    }

    SnakeState *state = calloc(1, sizeof(SnakeState));

    if (state == NULL) {
        return -1;
    }

    state->snake = calloc(1, SNAKE_MAX_SNAKE_LENGTH * sizeof(Snake));

    if (state->snake == NULL) {
        free(state);
        return -1;
    }

    state->agents = calloc(1, SNAKE_AGENT_MAX_LIST_SIZE * sizeof(NasaAgent));

    if (state->agents == NULL) {
        free(state->snake);
        free(state);
        return -1;
    }

    snake_initialize_snake_head(game, state->snake);

    state->snake_speed = SNAKE_DEFAULT_SNAKE_SPEED;
    state->snake_length = 1;
    state->direction = NORTH;
    snake_set_head_char(state);

    state->powerup.x = -1;
    state->powerup.y = -1;

    state->last_powerup_time = get_unix_time();

    game_show_level(game, true);
    game_show_score(game, true);
    game_show_high_score(game, true);

    game_increment_level(game);
    game_set_update_interval(game, SNAKE_DEFAULT_UPDATE_INTERVAL);
    game_random_coords(game, &state->food);

    game_set_cb_update_state(game, snake_cb_update_game_state, state);
    game_set_cb_render_window(game, snake_cb_render_window, state);
    game_set_cb_on_keypress(game, snake_cb_on_keypress, state);
    game_set_cb_kill(game, snake_cb_kill, state);
    game_set_cb_on_pause(game, snake_cb_pause, state);

    return 0;
}

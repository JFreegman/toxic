/*  game_snake.c
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

#include "game_base.h"
#include "game_util.h"
#include "game_snake.h"
#include "misc_tools.h"

#define SNAKE_MAX_SNAKE_LENGTH     (GAME_MAX_SQUARE_X_DEFAULT * GAME_MAX_SQUARE_Y_DEFAULT)
#define SNAKE_AGENT_MAX_LIST_SIZE  (GAME_MAX_SQUARE_X_DEFAULT * GAME_MAX_SQUARE_Y_DEFAULT)

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

/* Multiplayer constants */
#define SNAKE_HEAD_OTHER_COLOUR BLUE
#define SNAKE_BODY_OTHER_COLOUR MAGENTA
#define SNAKE_ONLINE_SNAKE_SPEED 8

/* Set to true to have the host controlled by a naive AI bot */
#define USE_AI false

#define SNAKE_ONLINE_VERSION 0x01u


typedef enum SnakeOnlienStatus {
    SnakeStatusInitializing = 0u,
    SnakeStatusPlaying,
    SnakeStatusFinished,
} SnakeOnlineStatus;

typedef enum SnakePacketType {
    SNAKE_PACKET_INVITE_REQUEST  = 0x0A,
    SNAKE_PACKET_INVITE_RESPONSE = 0x0B,
    SNAKE_PACKET_STATE           = 0x0C,
    SNAKE_PACKET_ABORT           = 0x0D,
} SnakePacketType;

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
    int         head_colour;
    int         body_colour;
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

    // Multiplayer
    int         x_left_bound;
    int         y_top_bound;

    bool        is_online;
    bool        self_host;
    bool        send_flag;  // true if it's our turn to send a packet
    SnakeOnlineStatus status;

    Snake       *other_snake;
    size_t      other_snake_length;
    Direction   other_direction;
} SnakeState;


static bool snake_packet_send_state(const GameData *game, SnakeState *state);
static bool snake_packet_invite_request(const GameData *game);
static bool snake_packet_invite_respond(const GameData *game, const SnakeState *state);
static bool snake_packet_abort(const GameData *game);
static void snake_cb_on_packet(GameData *game, const uint8_t *data, size_t length, void *cb_data);


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
    if (game_set_message(game, message, strlen(message), dir, attributes, colour,
                         timeout, coords, false, priority) == -1) {
        fprintf(stderr, "failed to set message\n");
    }
}

static Coords *snake_get_head_coords(Snake *snake)
{
    return &snake->coords;
}

static void snake_set_head_char(Snake *snake, Direction dir)
{
    Snake *snake_head = snake;

    switch (dir) {
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

/*
 * Return true if `new_dir` is a valid direction relative to `old_dir`.
 */
static bool snake_validate_direction(const Direction old_dir, const Direction new_dir)
{
    if (!GAME_UTIL_DIRECTION_VALID(new_dir)) {
        return false;
    }

    const int diff = abs((int)old_dir - (int)new_dir);

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

        if (snake_validate_direction(state->direction, dir)) {
            state->direction = dir;
            snake_set_head_char(state->snake, state->direction);
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
    const Coords *head = snake_get_head_coords(state->snake);

    snake_create_points_message(game, state->direction, points, head);

    game_update_score(game, points);
}

static long int snake_get_move_points(const SnakeState *state)
{
    return state->snake_length + (2 * state->snake_speed);
}

/* Return true if snake body is occupying given coordinates */
static bool snake_coords_contain_body(const Snake *snake, int snake_length, const Coords *coords)
{
    for (size_t i = 1; i < snake_length; ++i) {
        const Coords *snake_coords = &snake[i].coords;

        if (COORDINATES_OVERLAP(coords->x, coords->y, snake_coords->x, snake_coords->y)) {
            return true;
        }
    }

    return false;
}

static bool snake_self_consume(const SnakeState *state)
{
    const Coords *head = snake_get_head_coords(state->snake);
    return snake_coords_contain_body(state->snake, state->snake_length, head);
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

/*
 * Sets colour and attributes for entire snake except head.
 *
 * If colour is set to -1 a random colour is chosen for each body part.
 */
static void snake_set_body_attributes(Snake *snake, size_t length, int colour, int attributes)
{
    for (size_t i = 1; i < length; ++i) {
        Snake *body = &snake[i];

        if (colour < 0) {
            body->head_colour = game_util_random_colour();
        } else {
            body->head_colour = colour;
        }

        body->attributes = attributes;
    }
}

static void snake_game_over(SnakeState *state, Snake *snake, size_t snake_length)
{
    state->game_over = true;
    state->status = SnakeStatusFinished;
    state->has_powerup = false;

    Snake *head = snake;
    head->head_colour = SNAKE_DEAD_BODY_COLOUR;
    head->attributes = A_BOLD | A_BLINK;

    snake_set_body_attributes(snake, snake_length, SNAKE_DEAD_BODY_COLOUR, A_BOLD | A_BLINK);
}

/*
 * Return true if the game can be continued from the current game state.
 *
 * If `testrun` is true, the game state won't be modified and snakes will
 * not create messages.
 */
static bool snake_state_valid(GameData *game, SnakeState *state, bool testrun)
{
    const Coords *head = snake_get_head_coords(state->snake);

    if (!game_coordinates_in_bounds(game, head->x, head->y)) {
        if (!testrun) {
            snake_create_message(game, state->direction, "Ouch!", A_BOLD, WHITE,
                                 SNAKE_DEFAULT_MESSAGE_TIMER, head, true);
        }

        return false;
    }

    if (snake_self_consume(state)) {
        if (!testrun) {
            snake_create_message(game, state->direction, "Tastes like chicken", A_BOLD, WHITE,
                                 SNAKE_DEFAULT_MESSAGE_TIMER, head, true);
        }

        return false;
    }

    if (snake_agent_caught(game, state, head)) {
        if (!testrun) {
            snake_create_message(game, state->direction, "ARGH they got me!", A_BOLD, WHITE,
                                 SNAKE_DEFAULT_MESSAGE_TIMER, head, true);
        }

        return false;
    }

    if (!state->is_online) {
        return true;
    }

    const Coords *other_head = snake_get_head_coords(state->other_snake);

    if (snake_coords_contain_body(state->other_snake, state->other_snake_length, head)) {
        if (!testrun) {
            snake_create_message(game, state->direction, "AAAAA my tooth!", A_BOLD, WHITE,
                                 SNAKE_DEFAULT_MESSAGE_TIMER, other_head, true);
        }

        return false;
    }

    if (COORDINATES_OVERLAP(other_head->x, other_head->y, head->x, head->y)) {
        if (!testrun) {
            snake_create_message(game, state->direction, "Ouch!",
                                 A_BOLD, WHITE, SNAKE_DEFAULT_MESSAGE_TIMER, other_head, true);
            snake_create_message(game, state->direction, "Ouch!",
                                 A_BOLD, WHITE, SNAKE_DEFAULT_MESSAGE_TIMER, head, true);
            snake_game_over(state, state->other_snake, state->other_snake_length);
        }

        return false;
    }

    return true;
}

static void snake_move_body(Snake *snake, size_t length)
{
    if (length == 0) {
        return;
    }

    for (size_t i = length - 1; i > 0; --i) {
        const Coords *prev = &snake[i - 1].coords;
        Coords *curr = &snake[i].coords;
        curr->x = prev->x;
        curr->y = prev->y;
    }
}

static void snake_move_head(Snake *snake, const Direction dir)
{
    Coords *head = snake_get_head_coords(snake);
    game_util_move_coords(dir, head);
}

static void snake_grow(Snake *snake, size_t *length)
{
    size_t index = *length;

    if (*length >= SNAKE_MAX_SNAKE_LENGTH) {
        return;
    }

    snake[index].coords.x = -1;
    snake[index].coords.y = -1;
    snake[index].display_char = SNAKE_BODY_CHAR;
    snake[index].head_colour = snake->body_colour;
    snake[index].attributes = A_BOLD;

    *length = index + 1;
}

/*
 * Return true if food overlaps with any segment of snake.
 */
static bool snake_food_overlaps_snake(const Snake *snake, const size_t length, const Coords *food)
{
    for (size_t i = 0; i < length; ++i) {
        if (COORDINATES_OVERLAP(snake[i].coords.x, snake[i].coords.y, food->x, food->y)) {
            return true;
        }
    }

    return false;
}

/*
 * Tries to put the food in random coordinates that are not occupied by a snake.
 * If the board is full this might give up after a few tries, in which case the
 * food will be placed in a random coordinate that overlaps with a snake segment.
 *
 * TODO: make this never fail.
 */
static void snake_get_random_food_coords(const GameData *game, SnakeState *state)
{
    for (size_t tries = 0; tries < 3; ++tries) {
        game_random_coords(game, &state->food);

        if (!snake_food_overlaps_snake(state->snake, state->snake_length, &state->food)
                && !snake_food_overlaps_snake(state->other_snake, state->other_snake_length, &state->food)) {
            return;
        }
    }
}

static long int snake_check_food(const GameData *game, SnakeState *state)
{
    Coords *food = &state->food;
    const Coords *head = snake_get_head_coords(state->snake);

    if (!COORDINATES_OVERLAP(head->x, head->y, food->x, food->y)) {
        return 0;
    }

    snake_grow(state->snake, &state->snake_length);

    snake_get_random_food_coords(game, state);

    return snake_get_move_points(state);
}

static long int snake_check_powerup(GameData *game, SnakeState *state)
{
    Coords *powerup = &state->powerup;
    const Coords *head = snake_get_head_coords(state->snake);

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
    const Coords *head = snake_get_head_coords(state->snake);

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
        snake_set_body_attributes(state->snake, state->snake_length, state->snake->body_colour, A_BOLD);
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

/*
 * Return a score >= 0 of the current state based on the proximity of the snake to the food after
 * moving one step towards `new_dir`.
 *
 * Return -1 if the snake dies or `new_dir` is an invalid direction.
 */
static int snake_state_score(GameData *game, SnakeState *state, const Snake *old_snake, Direction new_dir)
{
    if (!snake_validate_direction(state->direction, new_dir)) {
        return -1;
    }

    const Direction old_dir = state->direction;
    state->direction = new_dir;

    snake_move_body(state->snake, state->snake_length);
    snake_move_head(state->snake, state->direction);

    if (!snake_state_valid(game, state, true)) {
        state->direction = old_dir;
        return -1;
    }

    state->direction = old_dir;

    const Coords *head = snake_get_head_coords(state->snake);

    const int x_diff = abs(head->x - state->food.x);
    const int y_diff = abs(head->y - state->food.y);

    return 1000 - (x_diff + y_diff);
}

/*
 * Moves snake towards food and tries not to die.
 */
static void snake_naive_ai(GameData *game, SnakeState *state)
{
    int best_score = -1;
    size_t new_dir = state->direction;

    Snake *old_snake = calloc(1, SNAKE_MAX_SNAKE_LENGTH * sizeof(Snake));

    if (old_snake == NULL) {
        return;
    }

    memcpy(old_snake, state->snake, sizeof(Snake) * SNAKE_MAX_SNAKE_LENGTH);

    for (size_t dir = 0; dir < INVALID_DIRECTION; ++dir) {
        const int new_score = snake_state_score(game, state, old_snake, dir);

        if (new_score > best_score) {
            best_score = new_score;
            new_dir = dir;
        }

        memcpy(state->snake, old_snake, sizeof(Snake) * SNAKE_MAX_SNAKE_LENGTH);
    }

    free(old_snake);

    switch (new_dir) {
        case NORTH:
            snake_set_key_press(state, KEY_UP);
            break;

        case SOUTH:
            snake_set_key_press(state, KEY_DOWN);
            break;

        case EAST:
            snake_set_key_press(state, KEY_RIGHT);
            break;

        case WEST:
            snake_set_key_press(state, KEY_LEFT);
            break;

        default:
            break;
    }
}

static void snake_move(GameData *game, SnakeState *state, TIME_MS cur_time)
{
    const TIME_MS real_speed = GAME_UTIL_REAL_SPEED(state->direction, state->snake_speed);

    if (!game_do_object_state_update(game, cur_time, state->snake_time_last_moved, real_speed)) {
        return;
    }

    if (state->is_online && !state->send_flag) {
        return;
    }

    if (USE_AI && state->self_host) {
        snake_naive_ai(game, state);
    }

    state->snake_time_last_moved = cur_time;

    snake_update_direction(state);
    snake_move_body(state->snake, state->snake_length);
    snake_move_head(state->snake, state->direction);

    if (!snake_state_valid(game, state, false)) {
        if (state->is_online) {
            snake_packet_send_state(game, state);
        }

        snake_game_over(state, state->snake, state->snake_length);
        game_set_status(game, GS_Finished);
        game_set_winner(game, false);
        return;
    }

    long int points = snake_check_food(game, state) + snake_check_powerup(game, state);

    if (!state->is_online && points > 0) {
        snake_do_points_update(game, state, points);
    }

    if (state->is_online && !snake_packet_send_state(game, state)) {
        fprintf(stderr, "failed to send state\n");
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
    const Coords *head = snake_get_head_coords(state->snake);

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

        if (snake_coords_contain_body(state->snake, state->snake_length, &new_coords)) {
            continue;
        }

        if (snake_get_agent_at_coords(state, &new_coords) != NULL) {
            continue;
        }

        coords->x = new_coords.x;
        coords->y = new_coords.y;

        if (!state->has_powerup && COORDINATES_OVERLAP(head->x, head->y, new_coords.x, new_coords.y)) {
            snake_game_over(state, state->snake, state->snake_length);
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

    if (!state->is_online && state->has_powerup) {
        const int time_left = SNAKE_POWERUP_TIMER - (get_unix_time() - state->powerup_timer);

        if (time_left <= 5 && time_left % 2 == 0) {
            snake_set_body_attributes(state->snake, state->snake_length, state->snake->body_colour, A_BOLD);
        } else {
            snake_set_body_attributes(state->snake, state->snake_length, -1, A_BOLD);
        }
    }
}

static void snake_draw_snake(WINDOW *win, const Snake *snake, size_t length)
{
    for (size_t i = 0; i < length; ++i) {
        const Snake *body = &snake[i];

        if (body->coords.x <= 0 || body->coords.y <= 0) {
            continue;
        }

        wattron(win, body->attributes | COLOR_PAIR(body->head_colour));
        mvwaddch(win, body->coords.y, body->coords.x, body->display_char);
        wattroff(win, body->attributes | COLOR_PAIR(body->head_colour));
    }
}

static void snake_draw_food(WINDOW *win, const SnakeState *state)
{
    if (state->is_online && state->status != SnakeStatusPlaying) {
        return;
    }

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

static void snake_cb_update_game_state(GameData *game, void *cb_data)
{
    SnakeState *state = (SnakeState *)cb_data;

    if (state == NULL) {
        return;
    }

    TIME_MS cur_time = get_time_millis();

    if (!state->is_online) {
        snake_decay_points(game, state);
        snake_do_powerup(game, state);
        snake_agent_move(game, state, cur_time);
    } else if (state->status != SnakeStatusPlaying) {
        return;
    }

    snake_move(game, state, cur_time);

    if (!state->game_over) {
        snake_update_frames(game, state, cur_time);
    }
}

static void snake_cb_render_window(GameData *game, WINDOW *win, void *cb_data)
{
    SnakeState *state = (SnakeState *)cb_data;

    if (state == NULL) {
        return;
    }

    snake_draw_food(win, state);

    if (!state->is_online) {
        snake_draw_powerup(win, state);
        snake_draw_agent(win, state);
    } else {
        snake_draw_snake(win, state->other_snake, state->other_snake_length);
    }

    snake_draw_snake(win, state->snake, state->snake_length);
}

static void snake_cb_kill(GameData *game, void *cb_data)
{
    SnakeState *state = (SnakeState *)cb_data;

    if (state == NULL) {
        return;
    }

    if (state->is_online && state->status == SnakeStatusPlaying) {
        snake_packet_abort(game);
    }

    free(state->snake);
    free(state->agents);
    free(state->other_snake);
    free(state);

    game_set_cb_update_state(game, NULL, NULL);
    game_set_cb_render_window(game, NULL, NULL);
    game_set_cb_kill(game, NULL, NULL);
    game_set_cb_on_keypress(game, NULL, NULL);
    game_set_cb_on_pause(game, NULL, NULL);
}

static void snake_cb_on_keypress(GameData *game, int key, void *cb_data)
{
    SnakeState *state = (SnakeState *)cb_data;

    if (state == NULL) {
        return;
    }

    snake_set_key_press(state, key);
}

static void snake_cb_pause(GameData *game, bool is_paused, void *cb_data)
{
    SnakeState *state = (SnakeState *)cb_data;

    if (state == NULL) {
        return;
    }

    UNUSED_VAR(game);

    TIME_S t = get_unix_time();

    if (is_paused) {
        state->pause_time = t;
    } else {
        state->powerup_timer += (t - state->pause_time);
        state->last_powerup_time += (t - state->pause_time);
    }
}

static void snake_initialize_snake_head(const GameData *game, SnakeState *state, bool is_online, bool self_host)
{
    int max_x;
    int max_y;
    game_max_x_y(game, &max_x, &max_y);

    state->snake_length = 1;

    Snake *head = state->snake;

    head->body_colour = SNAKE_BODY_COLOUR;
    head->head_colour = SNAKE_HEAD_COLOUR;
    head->attributes = A_BOLD;

    if (!is_online) {
        head->coords.x = max_x / 2;
        head->coords.y = max_y / 2;
        state->direction = SOUTH;
        snake_set_head_char(head, SOUTH);
        return;
    }

    state->other_snake_length = 1;

    Snake *other_head = state->other_snake;

    other_head->body_colour = SNAKE_BODY_OTHER_COLOUR;
    other_head->head_colour = SNAKE_HEAD_OTHER_COLOUR;
    other_head->attributes = A_BOLD;

    int self_x = max_x / 2 - 2;
    int self_y = max_y / 2 + 2;
    int other_x = max_x / 2 + 2;
    int other_y = max_y / 2 - 2;
    Direction self_dir = NORTH;
    Direction other_dir = SOUTH;

    if (!self_host) {
        other_x = max_x / 2 - 2;
        other_y = max_y / 2 + 2;
        self_x = max_x / 2 + 2;
        self_y = max_y / 2 - 2;
        self_dir = SOUTH;
        other_dir = NORTH;
    }

    head->coords.x = self_x;
    head->coords.y = self_y;
    state->direction = self_dir;
    snake_set_head_char(head, self_dir);

    other_head->coords.x = other_x;
    other_head->coords.y = other_y;
    state->other_direction = other_dir;
    snake_set_head_char(other_head, other_dir);
}

int snake_initialize(GameData *game, bool is_online, bool self_host)
{
    // note: if this changes we must update SNAKE_MAX_SNAKE_LENGTH and SNAKE_AGENT_MAX_LIST_SIZE
    if (game_set_window_shape(game, GW_ShapeSquare) == -1) {
        return -1;
    }

    SnakeState *state = calloc(1, sizeof(SnakeState));

    if (state == NULL) {
        return -1;
    }

    state->snake = calloc(1, SNAKE_MAX_SNAKE_LENGTH * sizeof(Snake));

    int err = -4;

    if (state->snake == NULL) {
        goto on_error;
    }

    state->agents = calloc(1, SNAKE_AGENT_MAX_LIST_SIZE * sizeof(NasaAgent));

    if (state->agents == NULL) {
        goto on_error;
    }

    state->is_online = is_online;
    state->self_host = self_host;

    state->snake_speed = is_online ? SNAKE_ONLINE_SNAKE_SPEED : SNAKE_DEFAULT_SNAKE_SPEED;

    state->powerup.x = -1;
    state->powerup.y = -1;

    state->last_powerup_time = get_unix_time();

    game_show_level(game, !is_online);
    game_show_score(game, !is_online);
    game_show_high_score(game, !is_online);

    game_increment_level(game);
    game_set_update_interval(game, SNAKE_DEFAULT_UPDATE_INTERVAL);
    game_random_coords(game, &state->food);

    if (!state->is_online) {
        snake_set_head_char(state->snake, SOUTH);
    } else {
        state->other_snake = calloc(1, SNAKE_MAX_SNAKE_LENGTH * sizeof(Snake));

        if (state->other_snake == NULL) {
            goto on_error;
        }
    }

    snake_initialize_snake_head(game, state, state->is_online, state->self_host);

    game_set_cb_update_state(game, snake_cb_update_game_state, state);
    game_set_cb_render_window(game, snake_cb_render_window, state);
    game_set_cb_on_keypress(game, snake_cb_on_keypress, state);
    game_set_cb_on_pause(game, snake_cb_pause, state);

    state->x_left_bound = game_x_left_bound(game);
    state->y_top_bound = game_y_top_bound(game);

    if (!state->is_online) {
        game_set_cb_kill(game, snake_cb_kill, state);
        return 0;
    }

    if (state->self_host) {
        state->status = SnakeStatusInitializing;

        if (!snake_packet_invite_request(game)) {
            err = -2;
            goto on_error;
        }
    } else {
        state->status = SnakeStatusPlaying;

        if (!snake_packet_invite_respond(game, state)) {
            err = -2;
            goto on_error;
        }
    }

    game_set_cb_kill(game, snake_cb_kill, state);
    game_set_cb_on_packet(game, snake_cb_on_packet, state);

    return 0;

on_error:
    free(state->snake);
    free(state->other_snake);
    free(state->agents);
    free(state);
    return err;
}

/**
 * START MULTIPLAYER
 */

/*
 * Sends an invite response packet to friend. Packet is comprised of:
 * [
 *  invite_type   (1 byte)
 *  snake_version (1 byte)
 *  food x coords (4 bytes)
 *  food y coords (4 bytes)
 *
 *  Return true on success.
 * ]
 */
#define SNAKE_PACKET_INVITE_RESPONSE_LENGTH (1 + 1 + sizeof(uint32_t) + sizeof(uint32_t))
static bool snake_packet_invite_respond(const GameData *game, const SnakeState *state)
{
    size_t length = 0;

    uint8_t data[SNAKE_PACKET_INVITE_RESPONSE_LENGTH];
    data[length] = SNAKE_PACKET_INVITE_RESPONSE;
    ++length;

    data[length] = SNAKE_ONLINE_VERSION;
    ++length;

    Coords food_coords;
    game_util_win_coords_to_board(state->food.x, state->food.y, state->x_left_bound, state->y_top_bound,
                                  &food_coords);

    game_util_pack_u32(data + length, food_coords.x);
    length += sizeof(uint32_t);

    game_util_pack_u32(data + length, food_coords.y);
    length += sizeof(uint32_t);

    if (game_packet_send(game, data, length, GP_Data) == 0) {
        return length == SNAKE_PACKET_INVITE_RESPONSE_LENGTH;
    }

    return false;
}

/*
 * Sends an invite request packet to friend. Packet is comprised of:
 * [
 *  invite_type (1 byte)
 * ]
 *
 * Return true on success.
 */
#define SNAKE_PACKET_INVITE_REQUEST_LENGTH 1
static bool snake_packet_invite_request(const GameData *game)
{
    uint8_t data[SNAKE_PACKET_INVITE_REQUEST_LENGTH];
    data[0] = SNAKE_PACKET_INVITE_REQUEST;

    return game_packet_send(game, data, sizeof(data), GP_Invite) == 0;
}

/*
 * Sends an abort packet to friend.
 *
 * Return true on success.
 */
#define SNAKE_PACKET_ABORT_LENGTH 1
static bool snake_packet_abort(const GameData *game)
{
    uint8_t data[SNAKE_PACKET_ABORT_LENGTH];
    data[0] = SNAKE_PACKET_ABORT;

    return game_packet_send(game, data, sizeof(data), GP_Data) == 0;
}

/*
 * Return true if the received state is valid.
 */
static bool snake_recv_state_valid(GameData *game, SnakeState *state, Direction other_dir,
                                   const Coords *other_coords, const Coords *food_coords)
{
    if (state->other_direction != other_dir &&
            !snake_validate_direction(state->other_direction, other_dir)) {
        snake_create_message(game, state->direction, "I think I'm lost...",
                             A_BOLD, WHITE, SNAKE_DEFAULT_MESSAGE_TIMER, other_coords, true);
        fprintf(stderr, "Invalid other direction %d\n", other_dir);
        return false;
    }

    if (!game_coordinates_in_bounds(game, food_coords->x, food_coords->y)) {
        snake_create_message(game, other_dir, "I'm not feeling so well",
                             A_BOLD, WHITE, SNAKE_DEFAULT_MESSAGE_TIMER, other_coords, true);
        fprintf(stderr, "Invalid food coords: %d %d\n", food_coords->x, food_coords->y);
        return false;
    }

    return true;
}

/*
 * Return true if the received state indicates that the game is over.
 */
static bool snake_recv_state_game_over(GameData *game, SnakeState *state, const Coords *other_coords,
                                       Direction other_dir)
{
    if (!game_coordinates_in_bounds(game, other_coords->x, other_coords->y)) {
        snake_create_message(game, other_dir, "Ouch!",
                             A_BOLD, WHITE, SNAKE_DEFAULT_MESSAGE_TIMER, other_coords, true);
        game_set_status(game, GS_Finished);
        game_set_winner(game, true);
        return true;
    }

    if (snake_coords_contain_body(state->other_snake, state->other_snake_length, other_coords)) {
        snake_create_message(game, other_dir, "Tastes like chicken!",
                             A_BOLD, WHITE, SNAKE_DEFAULT_MESSAGE_TIMER, other_coords, true);
        game_set_status(game, GS_Finished);
        game_set_winner(game, true);
        return true;
    }

    if (snake_coords_contain_body(state->snake, state->snake_length, other_coords)) {
        snake_create_message(game, other_dir, "AAAAA my tooth!",
                             A_BOLD, WHITE, SNAKE_DEFAULT_MESSAGE_TIMER, other_coords, true);
        game_set_status(game, GS_Finished);
        game_set_winner(game, true);
        return true;
    }

    const Coords *self_head = snake_get_head_coords(state->snake);

    if (COORDINATES_OVERLAP(other_coords->x, other_coords->y, self_head->x, self_head->y)) {
        snake_create_message(game, other_dir, "Ouch!",
                             A_BOLD, WHITE, SNAKE_DEFAULT_MESSAGE_TIMER, other_coords, true);
        snake_create_message(game, state->direction, "Ouch!",
                             A_BOLD, WHITE, SNAKE_DEFAULT_MESSAGE_TIMER, self_head, true);
        game_set_status(game, GS_Finished);
        game_set_winner(game, false);
        snake_game_over(state, state->snake, state->snake_length);
        return true;
    }

    return false;
}

/*
 * Applies the game state given to us by the other player.
 *
 * Return true if state is valid.
 */
static bool snake_apply_state(GameData *game, SnakeState *state, Direction other_direction,
                              const Coords *other_coords, const Coords *food_coords)

{
    if (state->status == SnakeStatusFinished) {
        return false;
    }

    if (!snake_recv_state_valid(game, state, other_direction, other_coords, food_coords)) {
        snake_game_over(state, state->other_snake, state->other_snake_length);
        game_set_status(game, GS_Finished);
        game_set_winner(game, true);
        return false;
    }

    state->other_direction = other_direction;
    snake_set_head_char(state->other_snake, state->other_direction);
    snake_move_body(state->other_snake, state->other_snake_length);
    snake_move_head(state->other_snake, state->other_direction);

    if (snake_recv_state_game_over(game, state, other_coords, other_direction)) {
        snake_game_over(state, state->other_snake, state->other_snake_length);
        return true;
    }

    if (COORDINATES_OVERLAP(other_coords->x, other_coords->y, state->food.x, state->food.y)) {
        snake_grow(state->other_snake, &state->other_snake_length);
        memcpy(&state->food, food_coords, sizeof(Coords));
    } else if (!COORDINATES_OVERLAP(state->food.x, state->food.y, food_coords->x, food_coords->y)) {
        fprintf(stderr, "Warning: Food coordinates don't overlap\n");
    }

    return true;
}

/*
 * Send your current game state to the other peer. This packet includes:
 * [
 *  packet_type    (1 byte)
 *  self direction (1 byte)
 *  self x coord   (4 bytes)
 *  self y coord   (4 bytes)
 *  food x coord   (4 bytes)
 *  food y coord   (4 bytes)
 * ]
 *
 * Return true on success.
 */
#define SNAKE_PACKET_STATE_LENGTH (1 + 1 + (sizeof(uint32_t) * 4))
static bool snake_packet_send_state(const GameData *game, SnakeState *state)
{
    // Convert our relative coordinates to real coordinates before sending
    const Snake *snake = state->snake;

    Coords snake_coords;
    game_util_win_coords_to_board(snake->coords.x, snake->coords.y, state->x_left_bound, state->y_top_bound,
                                  &snake_coords);

    Coords food_coords;
    game_util_win_coords_to_board(state->food.x, state->food.y, state->x_left_bound, state->y_top_bound,
                                  &food_coords);

    uint8_t data[SNAKE_PACKET_STATE_LENGTH];

    size_t length = 0;
    data[length] = SNAKE_PACKET_STATE;
    ++length;

    data[length] = (uint8_t)state->direction;
    ++length;

    game_util_pack_u32(data + length, snake_coords.x);
    length += sizeof(uint32_t);

    game_util_pack_u32(data + length, snake_coords.y);
    length += sizeof(uint32_t);

    game_util_pack_u32(data + length, food_coords.x);
    length += sizeof(uint32_t);

    game_util_pack_u32(data + length, food_coords.y);
    length += sizeof(uint32_t);

    if (game_packet_send(game, data, length, GP_Data) == 0) {
        state->send_flag = false;
        return length == SNAKE_PACKET_STATE_LENGTH;
    }


    return false;
}

/*
 * Handles a state packet and updates the game state accordingly.
 *
 * Return true on success.
 */
static bool snake_handle_state_packet(GameData *game, SnakeState *state, const uint8_t *data, size_t length)
{
    size_t unpacked = 0;

    const Direction other_direction = (Direction)data[unpacked];
    ++unpacked;

    uint32_t other_x_coord;
    uint32_t other_y_coord;
    uint32_t food_x_coord;
    uint32_t food_y_coord;

    game_util_unpack_u32(data + unpacked, &other_x_coord);
    unpacked += sizeof(uint32_t);

    game_util_unpack_u32(data + unpacked, &other_y_coord);
    unpacked += sizeof(uint32_t);

    game_util_unpack_u32(data + unpacked, &food_x_coord);
    unpacked += sizeof(uint32_t);

    game_util_unpack_u32(data + unpacked, &food_y_coord);
    unpacked += sizeof(uint32_t);

    Coords other_coords;
    game_util_board_to_win_coords(other_x_coord, other_y_coord, state->x_left_bound, state->y_top_bound,
                                  &other_coords);
    Coords food_coords;
    game_util_board_to_win_coords(food_x_coord, food_y_coord, state->x_left_bound, state->y_top_bound,
                                  &food_coords);

    if (!snake_apply_state(game, state, other_direction, &other_coords, &food_coords)) {
        return false;
    }

    return unpacked == SNAKE_PACKET_STATE_LENGTH - 1;
}

static bool snake_handle_invite_response(const GameData *game, SnakeState *state, const uint8_t *data, size_t length)
{
    uint32_t food_x_coord;
    uint32_t food_y_coord;

    size_t unpacked = 0;
    uint8_t snake_version = data[0];
    unpacked += 1;

    if (snake_version != SNAKE_ONLINE_VERSION) {
        fprintf(stderr, "Snake versions are incompatible: yours: %u, theirs: %u\n",
                SNAKE_ONLINE_VERSION, snake_version);
        return false;
    }

    game_util_unpack_u32(data + unpacked, &food_x_coord);
    unpacked += sizeof(uint32_t);

    game_util_unpack_u32(data + unpacked, &food_y_coord);
    unpacked += sizeof(uint32_t);

    Coords food_coords;
    game_util_board_to_win_coords(food_x_coord, food_y_coord, state->x_left_bound, state->y_top_bound,
                                  &food_coords);

    if (!game_coordinates_in_bounds(game, food_coords.x, food_coords.y)) {
        fprintf(stderr, "invalid food coords on initialization (x %d, y %d)\n", food_coords.x, food_coords.y);
        return false;
    }

    memcpy(&state->food, &food_coords, sizeof(food_coords));

    state->status = SnakeStatusPlaying;

    return unpacked == SNAKE_PACKET_INVITE_RESPONSE_LENGTH - 1;
}

static void snake_handle_abort_packet(GameData *game, SnakeState *state)
{
    snake_game_over(state, state->other_snake, state->other_snake_length);
    game_set_status(game, GS_Finished);
    game_set_winner(game, true);

    const Coords *other_coords = snake_get_head_coords(state->other_snake);
    snake_create_message(game, state->direction, "I'm scared",  A_BOLD, WHITE,
                         SNAKE_DEFAULT_MESSAGE_TIMER, other_coords, true);
}

static void snake_cb_on_packet(GameData *game, const uint8_t *data, size_t length, void *cb_data)
{
    SnakeState *state = (SnakeState *)cb_data;

    if (state == NULL) {
        return;
    }

    if (length == 0 || data == NULL) {
        return;
    }

    if (!state->is_online) {
        return;
    }

    SnakePacketType type = data[0];

    switch (type) {
        case SNAKE_PACKET_INVITE_RESPONSE: {
            if (length != SNAKE_PACKET_INVITE_RESPONSE_LENGTH) {
                fprintf(stderr, "Got invalid length invite response (%zu)\n", length);
                break;
            }

            if (state->status != SnakeStatusInitializing) {
                fprintf(stderr, "Got unsolicited snake invite response\n");
                break;
            }

            if (!snake_handle_invite_response(game, state, data + 1, length - 1)) {
                fprintf(stderr, "Failed to handle invite response\n");
                break;
            }

            state->send_flag = true;
            break;
        }

        case SNAKE_PACKET_STATE: {
            if (state->status != SnakeStatusPlaying) {
                fprintf(stderr, "Got state packet but status is %d\n", state->status);
                break;
            }

            if (length != SNAKE_PACKET_STATE_LENGTH) {
                snake_handle_abort_packet(game, state);
                fprintf(stderr, "Got invalid state packet length (%zu)\n", length);
                break;
            }

            if (state->send_flag) {
                fprintf(stderr, "Got multiple state packets before responding\n");
                break;
            }

            if (!snake_handle_state_packet(game, state, data + 1, length - 1)) {
                fprintf(stderr, "Failed to handle snake packet\n");
                break;
            }

            state->send_flag = true;
            break;
        }

        case SNAKE_PACKET_ABORT: {
            if (state->status != SnakeStatusPlaying) {
                break;
            }

            snake_handle_abort_packet(game, state);
            break;
        }

        default: {
            return;
        }
    }
}

/**
 * END MULTIPLAYER
 */

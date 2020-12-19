/*  game_centipede.c
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
#include <string.h>
#include <stdlib.h>

#include "game_centipede.h"
#include "game_base.h"
#include "game_util.h"
#include "misc_tools.h"

/* Determines how many mushrooms are spawned at the start of a game relative to window size (higher values means fewer) */
#define CENT_MUSHROOMS_POP_CONSTANT 35000

/* Max number of mushrooms */
#define CENT_MUSHROOMS_LENGTH (GAME_MAX_SQUARE_X * GAME_MAX_SQUARE_Y)

/* Max number of individual centipedes at any given time */
#define CENT_MAX_NUM_HEADS    20

/* Max number of segments that a centipede can have */
#define CENT_MAX_NUM_SEGMENTS 12

/* Get a free life every time we get this many points. Needs to be > the most points we can get in a single shot. */
#define CENT_SCORE_ONE_UP 5000

/* Max number of lives we can have */
#define CENT_MAX_LIVES    6

/* How many lives we start with */
#define CENT_START_LIVES  3

/* Max speed of an enemy agent */
#define CENT_MAX_ENEMY_AGENT_SPEED 8

/* How often a head that reaches the bottom can repdoduce */
#define CENT_REPRODUCE_TIMEOUT 10

#define CENT_CENTIPEDE_DEFAULT_SPEED  5
#define CENT_CENTIPEDE_ATTR           A_BOLD
#define CENT_CENTIPEDE_SEG_CHAR      '8'
#define CENT_CENTIPEDE_HEAD_CHAR     '8'

#define CENT_BULLET_COLOUR    YELLOW
#define CENT_BULLET_ATTR      A_BOLD
#define CENT_BULLET_CHAR      '|'
#define CENT_BULLET_SPEED     150

#define CENT_BLASTER_ATTR     A_BOLD
#define CENT_BLASTER_CHAR     'U'
#define CENT_BLASTER_SPEED    10

#define CENT_MUSH_DEFAULT_HEALTH 4
#define CENT_MUSH_DEFAULT_ATTR   A_BOLD
#define CENT_MUSH_DEFAULT_CHAR   '0'

#define CENT_SPIDER_SPAWN_TIMER     7    // has a 75% chance of spawning each timeout
#define CENT_SPIDER_DEFAULT_SPEED   1
#define CENT_SPIDER_DEFAULT_ATTR    A_BOLD
#define CENT_SPIDER_CHAR            'X'
#define CENT_SPIDER_START_HEALTH    1

#define CENT_FLEA_SPAWN_TIMER     15   // has a 75% chance of spawning each timeout
#define CENT_FLEA_DEFAULT_SPEED   2
#define CENT_FLEA_CHAR            'Y'
#define CENT_FLEA_DEFAULT_ATTR    A_BOLD
#define CENT_FLEA_POINTS          200
#define CENT_FLEA_START_HEALTH    2

#define CENT_SCORPION_BASE_SPAWN_TIMER    30
#define CENT_SCORPION_DEFAULT_SPEED  2
#define CENT_SCORPION_DEFAULT_ATTR   A_BOLD
#define CENT_SCORPTION_CHAR          '&'
#define CENT_SCORPTION_POINTS        1000
#define CENT_SCORPTION_START_HEALTH  1


/*
 * Determines how far north on the Y axis the blaster can move, how far north centipedes can move when
 * moving north, and the point at which fleas will stop creating mushrooms.
 */
#define CENT_INVISIBLE_H_WALL  5

#define CENT_KEY_FIRE ' '


typedef struct EnemyAgent {
    Coords      coords;
    Direction   direction;
    Direction   start_direction;
    int         colour;
    int         attributes;
    char        display_char;
    size_t      speed;
    TIME_MS     last_time_moved;
    TIME_S      last_time_despawned;
    bool        was_killed;
    size_t      health;
} EnemyAgent;

typedef struct Mushroom {
    Coords      coords;
    size_t      health;
    int         colour;
    int         attributes;
    char        display_char;
    bool        is_poisonous;
} Mushroom;

typedef struct Blaster {
    Coords      coords;
    Coords      start_coords;
    size_t      speed;
    TIME_MS     last_time_moved;
    int         colour;
    int         attributes;
    Direction   direction;
} Blaster;

typedef struct Projectile {
    Coords      coords;
    size_t      speed;
    TIME_MS     last_time_moved;
    int         colour;
    int         attributes;
} Projectile;

/*
 * A centipede is a doubly linked list comprised of one or more Segments. The head of the centipede
 * is the root. All non-head segments follow their `prev` segment.  When a non-tail segment is destroyed,
 * its `next` segment becomes a head.
 */
typedef struct Segment Segment;
struct Segment {
    Coords      coords;
    Direction   h_direction;
    Direction   v_direction;
    int         colour;
    int         attributes;
    int         display_char;
    size_t      poison_rot;
    bool        is_fertile;
    TIME_S      last_time_reproduced;
    TIME_MS     last_time_moved;
    Segment     *prev;
    Segment     *next;
};

typedef struct Centipedes {
    Segment     *heads[CENT_MAX_NUM_HEADS];
    size_t      heads_length;
} Centipedes;

typedef struct CentState {
    Centipedes  centipedes;
    Mushroom    *mushrooms;
    size_t      mushrooms_length;
    EnemyAgent  spider;
    EnemyAgent  flea;
    EnemyAgent  scorpion;
    Projectile  bullet;
    Blaster     blaster;

    TIME_S      pause_time;
    bool        game_over;
} CentState;


/*
 * Evaluates to true if vertically moving bullet (x2,y2) has hit or passed vertically moving object (x1,y1).
 * This should only be used if both the bullet and object never move on the x axis.
 */
#define CENT_VERTICAL_IMPACT(x1, y1, x2, y2)(((x1) == (x2)) && ((y1) >= (y2)))


#define CENT_LEVEL_COLOURS_SIZE 19
static const int cent_level_colours[] = {
    RED,
    CYAN,
    MAGENTA,
    BLUE,
    BLUE,
    RED,
    YELLOW,
    GREEN,
    GREEN,
    CYAN,
    YELLOW,
    MAGENTA,
    BLUE,
    GREEN,
    RED,
    MAGENTA,
    CYAN,
    YELLOW,
    WHITE,
};

static int cent_mushroom_colour(size_t level)
{
    return cent_level_colours[level % CENT_LEVEL_COLOURS_SIZE];
}

static int cent_head_colour(size_t level)
{
    return cent_level_colours[(level + 1) % CENT_LEVEL_COLOURS_SIZE];
}

static int cent_spider_colour(size_t level)
{
    return cent_level_colours[(level + 2) % CENT_LEVEL_COLOURS_SIZE];
}

static int cent_segment_colour(size_t level)
{
    return cent_level_colours[(level + 3) % CENT_LEVEL_COLOURS_SIZE];
}

static int cent_flea_colour(size_t level)
{
    return cent_level_colours[(level + 4) % CENT_LEVEL_COLOURS_SIZE];
}

static int cent_scorpion_colour(size_t level)
{
    return cent_level_colours[(level + 5) % CENT_LEVEL_COLOURS_SIZE];
}

static int cent_blaster_colour(size_t level)
{
    return cent_level_colours[(level + 6) % CENT_LEVEL_COLOURS_SIZE];
}

static int cent_poisonous_mush_colour(size_t level)
{
    int colour = cent_mushroom_colour(level);

    switch (colour) {
        case RED:
            return YELLOW;

        case YELLOW:
            return RED;

        case CYAN:
            return MAGENTA;

        case MAGENTA:
            return CYAN;

        case BLUE:
            return GREEN;

        case GREEN:
            return BLUE;

        default:
            return RED;
    }
}

static size_t cent_enemy_agent_speed(size_t base_speed, size_t level)
{
    if (level < 2) {
        return base_speed;
    }

    int r = rand() % (level / 2);

    return MIN(base_speed + r, CENT_MAX_ENEMY_AGENT_SPEED);
}

static void cent_update_score(GameData *game, const CentState *state, long int points, const Coords *coords)
{
    char buf[GAME_MAX_MESSAGE_SIZE + 1];

    long int prev_score = game_get_score(game);
    game_update_score(game, points);
    long int score = game_get_score(game);

    int lives = game_get_lives(game);

    if ((lives < CENT_MAX_LIVES) && ((score % CENT_SCORE_ONE_UP) < (prev_score % CENT_SCORE_ONE_UP))) {
        game_update_lives(game, 1);

        snprintf(buf, sizeof(buf), "%s", "1UP!");

        if (game_set_message(game, buf, strlen(buf), NORTH, A_BOLD, WHITE, 0, &state->blaster.coords, false, false) == -1) {
            fprintf(stderr, "failed to set points message\n");
        }
    }

    if (coords == NULL) {
        return;
    }

    snprintf(buf, sizeof(buf), "%ld", points);

    if (game_set_message(game, buf, strlen(buf), NORTH, A_BOLD, WHITE, 0, coords, false, true) == -1) {
        fprintf(stderr, "failed to set points message\n");
    }
}

static void cent_enemy_despawn(EnemyAgent *agent, bool was_killed)
{
    memset(agent, 0, sizeof(EnemyAgent));
    agent->last_time_despawned = get_unix_time();
    agent->was_killed = was_killed;
}

static void cent_bullet_reset(Projectile *bullet)
{
    bullet->coords.x = -1;
    bullet->coords.y = -1;
}

static long int cent_spider_points(const Coords *spider_coords, const Coords *blaster_coords)
{
    const int y_dist = blaster_coords->y - spider_coords->y;

    if (y_dist > 3) {
        return 300;
    }

    if (y_dist > 1) {
        return 600;
    }

    return 900;
}

static bool cent_centipedes_are_dead(Centipedes *centipedes)
{
    for (size_t i = 0; i < centipedes->heads_length; ++i) {
        if (centipedes->heads[i] != NULL) {
            return false;
        }
    }

    return true;
}

static void cent_kill_centipede(Centipedes *centipedes, size_t index)
{
    Segment *head = centipedes->heads[index];

    while (head) {
        Segment *tmp1 = head->next;
        free(head);
        head = tmp1;
    }

    centipedes->heads[index] = NULL;
}

static void cent_poison_centipede(Segment *segment)
{
    if (segment->poison_rot == 0) {
        while (segment != NULL) {
            segment->poison_rot = 1;
            segment = segment->next;
        }
    }
}

static void cent_cure_centipede(Segment *segment)
{
    if (segment->poison_rot > 0) {
        while (segment != NULL) {
            segment->poison_rot = 0;
            segment = segment->next;
        }
    }
}

static void cent_exterminate_centipedes(Centipedes *centipedes)
{
    for (size_t i = 0; i < centipedes->heads_length; ++i) {
        if (centipedes->heads[i] != NULL) {
            cent_kill_centipede(centipedes, i);
        }
    }

    centipedes->heads_length = 0;
}

static void cent_move_segments(Segment *head)
{
    if (head == NULL) {
        return;
    }

    Segment *current = head;

    while (current->next) {
        current = current->next;
    }

    Segment *prev = current->prev;

    while (prev) {
        current->coords.x = prev->coords.x;
        current->coords.y = prev->coords.y;
        current->h_direction = prev->h_direction;
        current->v_direction = prev->v_direction;
        current = prev;
        prev = prev->prev;
    }
}

static int cent_new_centipede_head_index(Centipedes *centipedes)
{
    for (size_t i = 0; i < CENT_MAX_NUM_HEADS; ++i) {
        if (centipedes->heads[i] != NULL) {
            continue;
        }

        if (i == centipedes->heads_length) {
            ++centipedes->heads_length;
        }

        return i;
    }

    return -1;
}

static int cent_birth_centipede(const GameData *game, CentState *state, size_t length, Direction direction,
                                const Coords *coords)
{
    if (length > CENT_MAX_NUM_SEGMENTS) {
        return -1;
    }

    Centipedes *centipedes = &state->centipedes;

    int head_idx = cent_new_centipede_head_index(centipedes);

    if (head_idx == -1) {
        return -1;
    }

    Segment *new_head = calloc(1, sizeof(Segment));

    if (new_head == NULL) {
        return -1;
    }

    if (coords == NULL) {
        const int x_left = game_x_left_bound(game);
        const int x_right = game_x_right_bound(game);
        const int y_top = game_y_top_bound(game);

        new_head->coords.x = direction == EAST ? x_left : x_right;
        new_head->coords.y = y_top;
    } else {
        new_head->coords.x = coords->x;
        new_head->coords.y = coords->y;
    }

    size_t level = game_get_current_level(game);

    new_head->h_direction = direction;
    new_head->v_direction = SOUTH;
    new_head->colour = cent_head_colour(level);
    new_head->attributes = CENT_CENTIPEDE_ATTR;
    new_head->display_char = CENT_CENTIPEDE_HEAD_CHAR;
    new_head->prev = NULL;

    centipedes->heads[head_idx] = new_head;

    Segment *prev = new_head;

    for (size_t i = 0; i < length; ++i) {
        Segment *new_seg = calloc(1, sizeof(Segment));

        if (new_seg == NULL) {
            cent_kill_centipede(centipedes, head_idx);
            return -1;
        }

        new_seg->colour = cent_segment_colour(level);
        new_seg->attributes = CENT_CENTIPEDE_ATTR;
        new_seg->display_char = CENT_CENTIPEDE_SEG_CHAR;
        new_seg->coords.x = -1;  // coords will update as it moves
        new_seg->coords.y = -1;
        new_seg->h_direction = direction;
        new_seg->v_direction = SOUTH;

        new_seg->prev = prev;
        prev->next = new_seg;
        prev = new_seg;
    }

    return 0;
}

static int cent_init_level_centipedes(const GameData *game, CentState *state, size_t level)
{
    Direction dir = rand() % 2 == 0 ? WEST : EAST;

    // First level we spawn one full size centipede
    if (level == 1) {
        if (cent_birth_centipede(game, state, CENT_MAX_NUM_SEGMENTS, dir, NULL) == -1) {
            return -1;
        }

        return 0;
    }

    const int c = ((int)level - 1);
    const int diff = CENT_MAX_NUM_SEGMENTS - c;

    const int y_top = game_y_top_bound(game);
    const int x_left = game_x_left_bound(game);
    const int x_right = game_x_right_bound(game);

    const int remainder = diff > 4 ? c : CENT_MAX_NUM_SEGMENTS;

    // for the next few levels we spawn one multi-seg centipede
    // decreasing in size and progressively more lone heads
    if (diff > 4) {
        if (cent_birth_centipede(game, state, diff, dir, NULL) == -1) {
            return -1;
        }
    }

    // Spawn loan heads
    for (size_t i = 0; i < remainder; ++i) {
        dir = i % 2 == 0 ? EAST : WEST;

        Coords coords;
        coords.x = dir == EAST ? x_left - i : x_right + i;
        coords.y = i % 2 == 0 ? y_top + 1 : y_top + 2;

        if (cent_birth_centipede(game, state, 0, dir, &coords) == -1) {
            return -1;
        }
    }

    return 0;
}

static int cent_restart_level(GameData *game, CentState *state)
{
    Mushroom *mushrooms = state->mushrooms;

    for (size_t i = 0; i < state->mushrooms_length; ++i) {
        Mushroom *mush = &mushrooms[i];

        if (mush->health > 0) {
            if (mush->health < CENT_MUSH_DEFAULT_HEALTH) {
                cent_update_score(game, state, 5, NULL);
            }

            mush->health = CENT_MUSH_DEFAULT_HEALTH;
            mush->display_char = CENT_MUSH_DEFAULT_CHAR;
            mush->attributes = CENT_MUSH_DEFAULT_ATTR;
        }
    }

    cent_enemy_despawn(&state->spider, false);
    cent_enemy_despawn(&state->flea, false);
    cent_enemy_despawn(&state->scorpion, false);
    cent_exterminate_centipedes(&state->centipedes);

    size_t level = game_get_current_level(game);

    if (cent_init_level_centipedes(game, state, level) == -1) {
        return -1;
    }


    state->blaster.coords.x = state->blaster.start_coords.x;
    state->blaster.coords.y = state->blaster.start_coords.y;

    cent_bullet_reset(&state->bullet);

    return 0;
}

static int cent_next_level(GameData *game, CentState *state)
{
    game_increment_level(game);

    size_t level = game_get_current_level(game);

    Mushroom *mushrooms = state->mushrooms;

    for (size_t i = 0; i < state->mushrooms_length; ++i) {
        Mushroom *mush = &mushrooms[i];

        if (mush->health > 0) {
            mush->colour = mush->is_poisonous ? cent_poisonous_mush_colour(level) : cent_mushroom_colour(level);
        }
    }

    cent_exterminate_centipedes(&state->centipedes);

    if (cent_init_level_centipedes(game, state, level) == -1) {
        return -1;
    }

    state->blaster.colour = cent_blaster_colour(level);
    state->spider.colour = cent_spider_colour(level);
    state->flea.colour = cent_flea_colour(level);
    state->scorpion.colour = cent_scorpion_colour(level);

    cent_bullet_reset(&state->bullet);

    return 0;
}

static void cent_deduct_life(GameData *game, CentState *state)
{
    game_update_lives(game, -1);

    int lives = game_get_lives(game);

    if (lives == 0) {
        game_set_status(game, GS_Finished);
        state->game_over = true;
    } else {
        if (cent_restart_level(game, state) != 0) {
            fprintf(stderr, "Failed to restart level\n");
        }
    }
}

static void cent_update_mush_appearance(const GameData *game, const CentState *state, Mushroom *mushroom)
{
    if (mushroom->health > CENT_MUSH_DEFAULT_HEALTH) {
        return;
    }

    switch (mushroom->health) {
        case CENT_MUSH_DEFAULT_HEALTH: {
            size_t level = game_get_current_level(game);
            mushroom->colour = mushroom->is_poisonous ? cent_poisonous_mush_colour(level) : cent_mushroom_colour(level);
            mushroom->attributes = CENT_MUSH_DEFAULT_ATTR;
            mushroom->display_char = CENT_MUSH_DEFAULT_CHAR;
            break;
        }

        case 3: {
            mushroom->display_char = 'o';
            break;
        }

        case 2: {
            mushroom->display_char = 'c';
            break;
        }

        case 1: {
            mushroom->display_char = ';';
            break;
        }

        default: {
            return;
        }
    }
}

static Mushroom *cent_get_mushroom_at_coords(CentState *state, const Coords *coords)
{
    for (size_t i = 0; i < state->mushrooms_length; ++i) {
        Mushroom *mush = &state->mushrooms[i];

        if (mush->health == 0) {
            continue;
        }

        if (COORDINATES_OVERLAP(coords->x, coords->y, mush->coords.x, mush->coords.y)) {
            return mush;
        }
    }

    return NULL;
}

static Mushroom *cent_mushroom_new(CentState *state)
{
    Mushroom *mushrooms = state->mushrooms;

    for (size_t i = 0; i < CENT_MUSHROOMS_LENGTH; ++i) {
        Mushroom *mush = &mushrooms[i];

        if (mush->health != 0) {
            continue;
        }

        if (i == state->mushrooms_length) {
            state->mushrooms_length = i + 1;
        }

        return &mushrooms[i];
    }

    return NULL;
}

static void cent_mushroom_grow(const GameData *game, CentState *state, const Coords *coords, bool is_poisonous)
{
    if (cent_get_mushroom_at_coords(state, coords) != NULL) {
        return;
    }

    if (!game_coordinates_in_bounds(game, coords->x, coords->y)) {
        return;
    }

    if (game_y_bottom_bound(game) == coords->y) {  // can't be hit by blaster on the floor
        return;
    }

    Mushroom *mush = cent_mushroom_new(state);

    if (mush == NULL) {
        return;
    }

    mush->is_poisonous = is_poisonous;
    mush->health = CENT_MUSH_DEFAULT_HEALTH;
    mush->coords.x = coords->x;
    mush->coords.y = coords->y;

    cent_update_mush_appearance(game, state, mush);
}

static bool cent_blaster_enemy_collision(CentState *state, const EnemyAgent *enemy)
{
    Blaster *blaster = &state->blaster;

    if (enemy->health == 0) {
        return false;
    }

    if (COORDINATES_OVERLAP(blaster->coords.x, blaster->coords.y, enemy->coords.x, enemy->coords.y)) {
        return true;
    }

    return false;
}

static void cent_blaster_move(GameData *game, CentState *state, TIME_MS cur_time)
{
    Blaster *blaster = &state->blaster;

    if (!GAME_UTIL_DIRECTION_VALID(blaster->direction)) {
        return;
    }

    const TIME_MS real_speed = GAME_UTIL_REAL_SPEED(blaster->direction, blaster->speed);

    if (!game_do_object_state_update(game, cur_time, blaster->last_time_moved, real_speed)) {
        return;
    }

    blaster->last_time_moved = cur_time;

    Coords new_coords = (Coords) {
        blaster->coords.x,
                blaster->coords.y
    };

    game_util_move_coords(blaster->direction, &new_coords);

    blaster->direction = INVALID_DIRECTION;

    const int y_bottom = game_y_bottom_bound(game);

    if (new_coords.y < y_bottom - CENT_INVISIBLE_H_WALL) {
        return;
    }

    if (!game_coordinates_in_bounds(game, new_coords.x, new_coords.y)) {
        return;
    }

    if (cent_get_mushroom_at_coords(state, &new_coords) != NULL) {
        return;
    }

    blaster->coords.x = new_coords.x;
    blaster->coords.y = new_coords.y;
}

static bool cent_bullet_mushroom_collision(GameData *game, CentState *state, const Coords *coords)
{
    Mushroom *mush = cent_get_mushroom_at_coords(state, coords);

    if (mush == NULL) {
        return false;
    }

    --mush->health;
    cent_update_mush_appearance(game, state, mush);

    if (mush->health == 0) {
        memset(mush, 0, sizeof(Mushroom));
        cent_update_score(game, state, 1, NULL);
    }

    return true;
}


static void cent_bullet_move(const GameData *game, CentState *state, TIME_MS cur_time)
{
    Projectile *bullet = &state->bullet;

    if (!game_do_object_state_update(game, cur_time, bullet->last_time_moved, bullet->speed)) {
        return;
    }

    bullet->last_time_moved = cur_time;

    const int y_top = game_y_top_bound(game);

    --bullet->coords.y;

    if (bullet->coords.y < y_top) {
        cent_bullet_reset(bullet);
    }
}

void cent_bullet_spawn(CentState *state)
{
    Projectile *bullet = &state->bullet;

    if (bullet->coords.x > 0) {
        return;
    }

    Blaster blaster = state->blaster;

    bullet->coords.x = blaster.coords.x;
    bullet->coords.y = blaster.coords.y;
    bullet->speed = CENT_BULLET_SPEED;
}

/* Destroys centipede segment `seg`. If seg is a head, its `next` segment is assigned as the new head for
 * the rest of the centipede. If seg is a tail, its `prev` segment is assigned as the new tail.
 * if seg is somewher in the middle, the centipede is split into two; its prev is assigned as a new
 * tail, and its `next` is assigned as a new head.
 *
 * Returns points value for the segment (100 for head, 10 for non-head).
 * Returns 0 if head limit has been reached.
 */
static long int cent_kill_centipede_segment(const GameData *game, Centipedes *centipedes, Segment *seg, size_t index)
{
    if (seg->prev == NULL) {  // head
        if (seg->next == NULL) {  // lone head
            free(seg);
            centipedes->heads[index] = NULL;
            return 100;
        }

        seg->next->display_char = seg->display_char;
        seg->next->colour = seg->colour;
        seg->next->attributes = seg->attributes;
        seg->next->poison_rot = seg->poison_rot;
        seg->next->prev = NULL;

        centipedes->heads[index] = seg->next;
        free(seg);

        return 100;
    }

    if (seg->next == NULL) {  // tail
        seg->prev->next = NULL;
        free(seg);
        return 10;
    }

    // somewhere in the middle

    int idx = cent_new_centipede_head_index(centipedes);

    if (idx == -1) {
        return 0;
    }

    seg->next->prev = NULL; // next becomes head
    seg->prev->next = NULL; // prev becomes tail

    seg->next->display_char = CENT_CENTIPEDE_HEAD_CHAR;
    seg->next->attributes = CENT_CENTIPEDE_ATTR;
    seg->next->poison_rot = seg->prev->poison_rot;

    size_t level = game_get_current_level(game);
    seg->next->colour = cent_head_colour(level);

    centipedes->heads[idx] = seg->next;

    free(seg);

    return 10;
}

static bool cent_bullet_centipede_collision(GameData *game, CentState *state)
{
    Centipedes *centipedes = &state->centipedes;
    Projectile *bullet = &state->bullet;

    for (size_t i = 0; i < centipedes->heads_length; ++i) {
        Segment *seg = centipedes->heads[i];

        if (seg == NULL) {
            continue;
        }

        while (seg) {
            if (COORDINATES_OVERLAP(seg->coords.x, seg->coords.y, bullet->coords.x, bullet->coords.y)) {
                Coords mush_coords;
                mush_coords.x = seg->h_direction == WEST ? seg->coords.x - 1 : seg->coords.x + 1;
                mush_coords.y = seg->coords.y;
                cent_mushroom_grow(game, state, &mush_coords, false);

                long int points = cent_kill_centipede_segment(game, centipedes, seg, i);
                cent_update_score(game, state, points, NULL);

                return true;
            }

            seg = seg->next;
        }
    }

    return false;
}

/*
 * Checks if bullet has collided with a mushroom or enemy and updates score appropriately.
 * If objects overlap they're all hit.
 */
static void cent_bullet_collision_check(GameData *game, CentState *state)
{
    Projectile *bullet = &state->bullet;

    if (bullet->coords.x <= 0) {
        return;
    }

    bool collision = false;

    if (cent_bullet_mushroom_collision(game, state, &bullet->coords)) {
        collision = true;
    }

    EnemyAgent *spider = &state->spider;

    if (spider->health > 0 && COORDINATES_OVERLAP(spider->coords.x, spider->coords.y, bullet->coords.x, bullet->coords.y)) {
        long int points = cent_spider_points(&spider->coords, &state->blaster.coords);
        cent_update_score(game, state, points, &spider->coords);
        cent_enemy_despawn(spider, true);
        collision = true;
    }

    EnemyAgent *scorpion = &state->scorpion;

    if (scorpion->health > 0
            && COORDINATES_OVERLAP(scorpion->coords.x, scorpion->coords.y, bullet->coords.x, bullet->coords.y)) {
        cent_update_score(game, state, CENT_SCORPTION_POINTS, NULL);
        cent_enemy_despawn(scorpion, true);
        collision = true;
    }

    EnemyAgent *flea = &state->flea;

    if (flea->health > 0 && CENT_VERTICAL_IMPACT(flea->coords.x, flea->coords.y, bullet->coords.x, bullet->coords.y)) {
        if (--flea->health == 0) {
            cent_update_score(game, state, CENT_FLEA_POINTS, NULL);
            cent_enemy_despawn(flea, true);
        } else {
            flea->speed += 5;
        }

        collision = true;
    }

    if (cent_bullet_centipede_collision(game, state)) {
        collision = true;

        if (cent_centipedes_are_dead(&state->centipedes)) {
            cent_next_level(game, state);
        }
    }

    if (collision) {
        cent_bullet_reset(bullet);
    }
}

static void cent_set_head_direction(CentState *state, Segment *head, int y_bottom, int x_left, int x_right)
{
    Coords next_coords = (Coords) {
        head->coords.x, head->coords.y
    };

    // Move horizontally until we hit a mushroom or a wall, at which point we move down one square
    // and continue in the other direction.
    if (head->h_direction == WEST) {
        next_coords.x = head->coords.x - 1;
        Mushroom *mush = cent_get_mushroom_at_coords(state, &next_coords);

        if (head->coords.x <= x_left || mush != NULL) {
            if (mush && mush->is_poisonous) {
                cent_poison_centipede(head);
            }

            head->h_direction = EAST;
            head->coords.y = head->v_direction == SOUTH ? head->coords.y + 1 : head->coords.y - 1;
        } else {
            --head->coords.x;
        }
    } else if (head->h_direction == EAST) {
        next_coords.x = head->coords.x + 1;
        Mushroom *mush = cent_get_mushroom_at_coords(state, &next_coords);

        if (head->coords.x >= x_right || mush != NULL) {
            if (mush && mush->is_poisonous) {
                cent_poison_centipede(head);
            }

            head->h_direction = WEST;
            head->coords.y = head->v_direction == SOUTH ? head->coords.y + 1 : head->coords.y - 1;
        } else {
            ++head->coords.x;
        }
    }

    // if we touched a poison mushroom we move south every two steps
    if (head->poison_rot == 2) {
        ++head->coords.y;
        head->h_direction = head->h_direction == EAST ? WEST : EAST;
        head->poison_rot = 1;
    } else if (head->poison_rot > 0) {
        ++head->poison_rot;
    }

    // if we hit the bottom boundary we turn north. if we're going north we only go up to the invisible wall
    // and turn back around.
    if (head->v_direction == SOUTH && head->coords.y > y_bottom) {
        head->coords.y -= 2;
        head->v_direction = NORTH;
        cent_cure_centipede(head);
        head->is_fertile = true;
        head->last_time_reproduced = get_unix_time();
    } else if (head->v_direction == NORTH && head->coords.y < y_bottom - CENT_INVISIBLE_H_WALL) {
        head->coords.y += 2;
        head->v_direction = SOUTH;
    }
}

/*
 * If a head has reached the bottom it reproduces (spawns an additional head) on a timer.
 */
static void cent_do_reproduce(const GameData *game, CentState *state, Segment *head, int x_right, int x_left,
                              int y_bottom)
{
    if (!head->is_fertile) {
        return;
    }

    if (!timed_out(head->last_time_reproduced, CENT_REPRODUCE_TIMEOUT)) {
        return;
    }

    Direction dir = rand() % 2 == 0 ? WEST : EAST;

    Coords new_coords;
    new_coords.x = dir == EAST ? x_left : x_right;
    new_coords.y = y_bottom - (rand() % CENT_INVISIBLE_H_WALL);

    if (cent_birth_centipede(game, state, 0, dir, &new_coords) == 0) {
        head->last_time_reproduced = get_unix_time();
    }
}

static void cent_do_centipede(const GameData *game, CentState *state, TIME_MS cur_time)
{
    Centipedes *centipedes = &state->centipedes;

    if (centipedes->heads_length == 0) {
        return;
    }

    const int y_bottom = game_y_bottom_bound(game);
    const int x_left = game_x_left_bound(game);
    const int x_right = game_x_right_bound(game);

    for (size_t i = 0; i < centipedes->heads_length; ++i) {
        Segment *head = centipedes->heads[i];

        if (head == NULL) {
            continue;
        }

        // half speed if poisoned
        TIME_MS real_speed = head->poison_rot > 0 ? (CENT_CENTIPEDE_DEFAULT_SPEED / 2) + 1 : CENT_CENTIPEDE_DEFAULT_SPEED;

        if (!game_do_object_state_update(game, cur_time, head->last_time_moved, real_speed)) {
            continue;
        }

        head->last_time_moved = cur_time;

        cent_move_segments(head);
        cent_set_head_direction(state, head, y_bottom, x_left, x_right);

        if (head->coords.x == x_left || head->coords.x == x_right) {
            cent_do_reproduce(game, state, head, x_right, x_left, y_bottom);
        }
    }
}

static void cent_try_spawn_flea(const GameData *game, EnemyAgent *flea)
{
    if (!flea->was_killed) {
        if (!timed_out(flea->last_time_despawned, CENT_FLEA_SPAWN_TIMER)) {
            return;
        }
    }

    flea->was_killed = false;

    if (rand() % 4 == 0) {
        return;
    }

    size_t level = game_get_current_level(game);

    flea->colour = cent_flea_colour(level);
    flea->speed = cent_enemy_agent_speed(CENT_FLEA_DEFAULT_SPEED, level);
    flea->attributes = CENT_FLEA_DEFAULT_ATTR;
    flea->display_char = CENT_FLEA_CHAR;
    flea->direction = SOUTH;
    flea->health = CENT_FLEA_START_HEALTH;

    const int y_top = game_y_top_bound(game);
    const int x_left = game_x_left_bound(game);
    const int x_right = game_x_right_bound(game);

    flea->coords.y = y_top;
    flea->coords.x = (rand() % (x_right - x_left + 1)) + x_left;
}

static void cent_do_flea(GameData *game, CentState *state, TIME_MS cur_time)
{
    EnemyAgent *flea = &state->flea;

    if (flea->health == 0) {
        cent_try_spawn_flea(game, flea);
        return;
    }

    if (!game_do_object_state_update(game, cur_time, flea->last_time_moved, flea->speed)) {
        return;
    }

    flea->last_time_moved = cur_time;

    const int y_bottom = game_y_bottom_bound(game);

    if (flea->coords.y < (y_bottom - 5) && rand() % 4 == 0) {
        cent_mushroom_grow(game, state, &flea->coords, false);
    }

    ++flea->coords.y;

    if (flea->coords.y > game_y_bottom_bound(game)) {
        cent_enemy_despawn(flea, false);
    }
}

/*
 * Scorption spawn timeout is reduced linearly according to the level. She has a 75% chance of
 * spawning when the timeout expires, at which point it's reset whether she spawns or not.
 */
static bool cent_scorpion_spawn_check(EnemyAgent *scorpion, size_t level)
{
    size_t decay = level * 2;
    TIME_S timeout = CENT_SCORPION_BASE_SPAWN_TIMER > decay ? CENT_SCORPION_BASE_SPAWN_TIMER - decay : 1;

    if (!timed_out(scorpion->last_time_despawned, timeout)) {
        return false;
    }

    scorpion->last_time_despawned = get_unix_time();

    return (rand() % 4) < 3;
}

static void cent_try_spawn_scorpion(const GameData *game, CentState *state, EnemyAgent *scorpion)
{
    size_t level = game_get_current_level(game);

    if (level < 2) {
        return;
    }

    if (!cent_scorpion_spawn_check(scorpion, level)) {
        return;
    }

    scorpion->colour = cent_scorpion_colour(level);
    scorpion->speed = CENT_SCORPION_DEFAULT_SPEED;
    scorpion->attributes = CENT_SCORPION_DEFAULT_ATTR;
    scorpion->display_char = CENT_SCORPTION_CHAR;
    scorpion->health = CENT_SCORPTION_START_HEALTH;
    scorpion->direction = rand() % 2 == 0 ? WEST : EAST;

    const int y_bottom = game_y_bottom_bound(game);
    const int x_left = game_x_left_bound(game);
    const int x_right = game_x_right_bound(game);
    const int y_top = game_y_top_bound(game);
    const int y_mid = y_top + ((y_bottom - y_top) / 2);

    scorpion->coords.x = scorpion->direction == WEST ? x_right : x_left;
    scorpion->coords.y = (y_mid - 5) + (rand() % 5);
}

static void cent_do_scorpion(GameData *game, CentState *state, TIME_MS cur_time)
{
    EnemyAgent *scorpion = &state->scorpion;

    if (scorpion->health == 0) {
        cent_try_spawn_scorpion(game, state, scorpion);
        return;
    }

    if (!game_do_object_state_update(game, cur_time, scorpion->last_time_moved, scorpion->speed)) {
        return;
    }

    scorpion->last_time_moved = cur_time;

    Mushroom *mush = cent_get_mushroom_at_coords(state, &scorpion->coords);

    if (mush != NULL) {
        size_t level = game_get_current_level(game);
        mush->is_poisonous = true;
        mush->colour = cent_poisonous_mush_colour(level);
    }

    const int x_left = game_x_left_bound(game);
    const int x_right = game_x_right_bound(game);

    scorpion->coords.x = scorpion->direction == WEST ? scorpion->coords.x - 1 : scorpion->coords.x + 1;

    if (scorpion->coords.x > x_right || scorpion->coords.x < x_left) {
        cent_enemy_despawn(scorpion, false);
    }
}

static void cent_try_spawn_spider(const GameData *game, EnemyAgent *spider)
{
    if (!timed_out(spider->last_time_despawned, CENT_SPIDER_SPAWN_TIMER)) {
        return;
    }

    if (rand() % 4 == 0) {
        spider->last_time_despawned = get_unix_time();
        return;
    }

    size_t level = game_get_current_level(game);

    spider->colour = cent_spider_colour(level);
    spider->speed = cent_enemy_agent_speed(CENT_SPIDER_DEFAULT_SPEED, level);
    spider->attributes = CENT_SPIDER_DEFAULT_ATTR;
    spider->display_char = CENT_SPIDER_CHAR;
    spider->start_direction = rand() % 2 == 0 ? WEST : EAST;
    spider->direction = spider->start_direction;
    spider->health = CENT_SPIDER_START_HEALTH;

    const int y_bottom = game_y_bottom_bound(game);
    const int x_left = game_x_left_bound(game);
    const int x_right = game_x_right_bound(game);

    spider->coords.x = spider->direction == WEST ? x_right : x_left;
    spider->coords.y = (rand() % (y_bottom - (y_bottom - CENT_INVISIBLE_H_WALL))) + (y_bottom - CENT_INVISIBLE_H_WALL);
}

static void cent_do_spider(GameData *game, CentState *state, TIME_MS cur_time)
{
    EnemyAgent *spider = &state->spider;

    if (spider->health == 0) {
        cent_try_spawn_spider(game, spider);
        return;
    }

    if (!game_do_object_state_update(game, cur_time, spider->last_time_moved, spider->speed)) {
        return;
    }

    spider->last_time_moved = cur_time;

    Coords new_coords = (Coords) {
        spider->coords.x,
               spider->coords.y,
    };

    int r = rand();

    if (spider->direction == spider->start_direction) {
        if (r % 4 == 0) {
            spider->direction = r % 3 == 0 ? NORTH : SOUTH;
        }
    } else {
        if (r % 5 == 0) {
            spider->direction = spider->start_direction;
        }
    }

    game_util_move_coords(spider->direction, &new_coords);

    const int y_bottom = game_y_bottom_bound(game);
    const int x_left = game_x_left_bound(game);
    const int x_right = game_x_right_bound(game);
    const int top_limit = y_bottom - CENT_INVISIBLE_H_WALL;

    if (new_coords.x > x_right || new_coords.x < x_left) {
        cent_enemy_despawn(spider, false);
        return;
    }

    if (new_coords.y > y_bottom) {
        new_coords.y = y_bottom;
        spider->direction = NORTH;
    } else if (new_coords.y < top_limit) {
        new_coords.y = new_coords.y + 1;
        spider->direction = SOUTH;
    }

    spider->coords = (Coords) {
        new_coords.x,
        new_coords.y
    };

    Mushroom *mush = cent_get_mushroom_at_coords(state, &new_coords);

    if (mush != NULL) {
        memset(mush, 0, sizeof(Mushroom));
    }
}

static bool cent_blaster_centipede_collision(CentState *state)
{
    Centipedes *centipedes = &state->centipedes;
    Blaster blaster = state->blaster;

    for (size_t i = 0; i < centipedes->heads_length; ++i) {
        Segment *seg = centipedes->heads[i];

        if (seg == NULL) {
            continue;
        }

        while (seg) {
            if (COORDINATES_OVERLAP(seg->coords.x, seg->coords.y, blaster.coords.x, blaster.coords.y)) {
                return true;
            }

            seg = seg->next;
        }
    }

    return false;
}

static void cent_blaster_collision_check(GameData *game, CentState *state)
{
    if (state->blaster.coords.x <= 0) {
        return;
    }

    if (cent_blaster_enemy_collision(state, &state->flea)) {
        cent_deduct_life(game, state);
        return;
    }

    if (cent_blaster_enemy_collision(state, &state->spider)) {
        cent_deduct_life(game, state);
        return;
    }

    if (cent_blaster_centipede_collision(state)) {
        cent_deduct_life(game, state);
        return;
    }
}

static void cent_blaster_draw(WINDOW *win, const CentState *state)
{
    Blaster blaster = state->blaster;

    wattron(win, blaster.attributes | COLOR_PAIR(blaster.colour));
    mvwaddch(win, blaster.coords.y, blaster.coords.x, CENT_BLASTER_CHAR);
    wattroff(win, blaster.attributes | COLOR_PAIR(blaster.colour));
}

static void cent_projectiles_draw(WINDOW *win, const CentState *state)
{
    Projectile bullet = state->bullet;
    Coords blaster_coords = state->blaster.coords;

    if (bullet.coords.x > 0 && bullet.coords.y != blaster_coords.y) {
        wattron(win, bullet.attributes | COLOR_PAIR(bullet.colour));
        mvwaddch(win, bullet.coords.y, bullet.coords.x, CENT_BULLET_CHAR);
        wattroff(win, bullet.attributes | COLOR_PAIR(bullet.colour));
    }
}

static void cent_enemy_draw(WINDOW *win, const CentState *state)
{
    EnemyAgent spider = state->spider;

    if (spider.health > 0) {
        wattron(win, spider.attributes | COLOR_PAIR(spider.colour));
        mvwaddch(win, spider.coords.y, spider.coords.x, spider.display_char);
        wattroff(win, spider.attributes | COLOR_PAIR(spider.colour));
    }

    EnemyAgent flea = state->flea;

    if (flea.health > 0) {
        wattron(win, flea.attributes | COLOR_PAIR(flea.colour));
        mvwaddch(win, flea.coords.y, flea.coords.x, CENT_FLEA_CHAR);
        wattroff(win, flea.attributes | COLOR_PAIR(flea.colour));
    }

    EnemyAgent scorpion = state->scorpion;

    if (scorpion.health > 0) {
        wattron(win, scorpion.attributes | COLOR_PAIR(scorpion.colour));
        mvwaddch(win, scorpion.coords.y, scorpion.coords.x, scorpion.display_char);
        wattroff(win, scorpion.attributes | COLOR_PAIR(scorpion.colour));
    }
}

static void cent_centipede_draw(const GameData *game, WINDOW *win, CentState *state)
{
    Centipedes *centipedes = &state->centipedes;

    for (size_t i = 0; i < centipedes->heads_length; ++i) {
        Segment *seg = centipedes->heads[i];

        while (seg) {
            // sometimes we spawn heads outside of game bounds
            if (game_coordinates_in_bounds(game, seg->coords.x, seg->coords.y)) {
                wattron(win, seg->attributes | COLOR_PAIR(seg->colour));
                mvwaddch(win, seg->coords.y, seg->coords.x, seg->display_char);
                wattroff(win, seg->attributes | COLOR_PAIR(seg->colour));
            }

            seg = seg->next;
        }
    }
}

static void cent_mushrooms_draw(WINDOW *win, const CentState *state)
{
    Mushroom *mushrooms = state->mushrooms;

    for (size_t i = 0; i < state->mushrooms_length; ++i) {
        Mushroom mush = mushrooms[i];

        if (mush.health == 0) {
            continue;
        }

        wattron(win, mush.attributes | COLOR_PAIR(mush.colour));
        mvwaddch(win, mush.coords.y, mush.coords.x, mush.display_char);
        wattroff(win, mush.attributes | COLOR_PAIR(mush.colour));
    }
}

void cent_cb_update_game_state(GameData *game, void *cb_data)
{
    if (!cb_data) {
        return;
    }

    CentState *state = (CentState *)cb_data;

    if (state->game_over) {
        return;
    }

    TIME_MS cur_time = get_time_millis();

    cent_blaster_collision_check(game, state);
    cent_bullet_collision_check(game, state);
    cent_blaster_move(game, state, cur_time);
    cent_bullet_move(game, state, cur_time);
    cent_do_centipede(game, state, cur_time);
    cent_do_spider(game, state, cur_time);
    cent_do_flea(game, state, cur_time);
    cent_do_scorpion(game, state, cur_time);
}

void cent_cb_render_window(GameData *game, WINDOW *win, void *cb_data)
{
    if (!cb_data) {
        return;
    }

    CentState *state = (CentState *)cb_data;

    cent_blaster_draw(win, state);
    cent_projectiles_draw(win, state);
    cent_mushrooms_draw(win, state);
    cent_enemy_draw(win, state);
    cent_centipede_draw(game, win, state);
}

void cent_cb_on_keypress(GameData *game, int key, void *cb_data)
{
    if (!cb_data) {
        return;
    }

    CentState *state = (CentState *)cb_data;

    if (key == CENT_KEY_FIRE) {
        cent_bullet_spawn(state);
        return;
    }

    Direction dir = game_util_get_direction(key);

    if (dir < INVALID_DIRECTION) {
        state->blaster.direction = dir;
    }
}

void cent_cb_pause(GameData *game, bool is_paused, void *cb_data)
{
    if (!cb_data) {
        return;
    }

    CentState *state = (CentState *)cb_data;

    TIME_S t = get_unix_time();

    if (is_paused) {
        state->pause_time = t;
    } else {
        state->spider.last_time_despawned += (t - state->pause_time);
    }
}

void cent_cb_kill(GameData *game, void *cb_data)
{
    if (!cb_data) {
        return;
    }

    CentState *state = (CentState *)cb_data;

    cent_exterminate_centipedes(&state->centipedes);

    free(state->mushrooms);
    free(state);

    game_set_cb_update_state(game, NULL, NULL);
    game_set_cb_render_window(game, NULL, NULL);
    game_set_cb_kill(game, NULL, NULL);
    game_set_cb_on_pause(game, NULL, NULL);
}

/* Gives `mush` new coordinates that don't overlap with another mushroom and are within boundaries.
 *
 * Return false if we fail to find a vacant coordinate.
 */
static bool cent_new_mush_coordinates(const GameData *game, CentState *state, Mushroom *mush, int y_floor_bound)
{
    size_t tries = 0;
    Coords new_coords;

    do {
        if (++tries > 10) {
            return false;
        }

        game_random_coords(game, &new_coords);
    } while (new_coords.y >= (y_floor_bound - 1) || cent_get_mushroom_at_coords(state, &new_coords) != NULL);

    mush->coords = (Coords) {
        new_coords.x,
        new_coords.y
    };

    return true;
}

static void cent_populate_mushrooms(const GameData *game, CentState *state, int population_const)
{
    int max_x;
    int max_y;
    game_max_x_y(game, &max_x, &max_y);

    const int y_floor_bound = game_y_bottom_bound(game);

    for (size_t i = 0; i < CENT_MUSHROOMS_LENGTH; ++i) {
        if (rand() % population_const != 0) {
            continue;
        }

        size_t idx = state->mushrooms_length;
        Mushroom *mush = &state->mushrooms[idx];

        if (!cent_new_mush_coordinates(game, state, mush, y_floor_bound)) {
            continue;
        }

        mush->is_poisonous = false;
        mush->health = CENT_MUSH_DEFAULT_HEALTH;

        cent_update_mush_appearance(game, state, mush);

        ++state->mushrooms_length;
    }
}

static int cent_init_state(GameData *game, CentState *state)
{
    game_update_lives(game, CENT_START_LIVES);

    const int y_bottom = game_y_bottom_bound(game);
    const int x_left = game_x_left_bound(game);
    const int x_right = game_x_right_bound(game);
    const int y_top = game_y_top_bound(game);

    Mushroom *mushrooms = calloc(1, sizeof(Mushroom) * CENT_MUSHROOMS_LENGTH);

    if (mushrooms == NULL) {
        return -1;
    }

    state->mushrooms = mushrooms;

    Centipedes *centipedes = &state->centipedes;
    memset(centipedes->heads, 0, sizeof(centipedes->heads));

    Direction dir = rand() % 2 == 0 ? WEST : EAST;

    if (cent_birth_centipede(game, state, CENT_MAX_NUM_SEGMENTS, dir, NULL) == -1) {
        free(mushrooms);
        return -1;
    }

    state->spider.last_time_despawned = get_unix_time();
    state->flea.last_time_despawned = get_unix_time();
    state->scorpion.last_time_despawned = get_unix_time();

    state->blaster.colour = cent_blaster_colour(0);
    state->blaster.attributes = CENT_BLASTER_ATTR;
    state->blaster.direction = INVALID_DIRECTION;
    state->blaster.speed = CENT_BLASTER_SPEED;

    state->blaster.coords = (Coords) {
        (x_left + x_right) / 2,
        y_bottom
    };

    state->blaster.start_coords = (Coords) {
        (x_left + x_right) / 2,
        y_bottom
    };

    state->bullet.colour = CENT_BULLET_COLOUR;
    state->bullet.attributes = CENT_BULLET_ATTR;

    const int grid_size = (y_bottom - y_top) * (x_right - x_left);

    if (grid_size >= CENT_MUSHROOMS_POP_CONSTANT) {
        return -1;
    }

    const int population = CENT_MUSHROOMS_POP_CONSTANT / grid_size;
    cent_populate_mushrooms(game, state, population);

    return 0;
}

int centipede_initialize(GameData *game)
{
    if (game_set_window_shape(game, GW_ShapeSquare) == -1) {
        return -1;
    }

    CentState *state = calloc(1, sizeof(CentState));

    if (state == NULL) {
        return -1;
    }

    game_show_level(game, true);
    game_show_score(game, true);
    game_show_lives(game, true);
    game_show_high_score(game, true);
    game_increment_level(game);
    game_set_update_interval(game, 10);

    if (cent_init_state(game, state) == -1) {
        free(state);
        return -1;
    }

    game_set_cb_update_state(game, cent_cb_update_game_state, state);
    game_set_cb_render_window(game, cent_cb_render_window, state);
    game_set_cb_on_keypress(game, cent_cb_on_keypress, state);
    game_set_cb_kill(game, cent_cb_kill, state);
    game_set_cb_on_pause(game, cent_cb_pause, state);

    return 0;
}

/*  game_util.c
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

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "game_util.h"
#include "toxic.h"
#include "windows.h"

Direction game_util_get_direction(int key)
{
    switch (key) {
        case KEY_UP: {
            return NORTH;
        }

        case KEY_DOWN: {
            return SOUTH;
        }

        case KEY_RIGHT: {
            return EAST;
        }

        case KEY_LEFT: {
            return WEST;
        }

        default: {
            return INVALID_DIRECTION;
        }
    }
}

Direction game_util_move_towards(const Coords *coords_a, const Coords *coords_b, bool inverse)
{
    const int x1 = coords_a->x;
    const int y1 = coords_a->y;
    const int x2 = coords_b->x;
    const int y2 = coords_b->y;

    const int x_diff = abs(x1 - x2);
    const int y_diff = abs(y1 - y2);

    if (inverse) {
        if (x_diff > y_diff) {
            return x2 >= x1 ? WEST : EAST;
        } else {
            return y2 >= y1 ? NORTH : SOUTH;
        }
    } else {
        if (x_diff > y_diff) {
            return x2 < x1 ? WEST : EAST;
        } else {
            return y2 < y1 ? NORTH : SOUTH;
        }
    }
}

Direction game_util_random_direction(void)
{
    int r = rand() % 4;

    switch (r) {
        case 0:
            return NORTH;

        case 1:
            return SOUTH;

        case 2:
            return EAST;

        case 3:
            return WEST;

        default: // impossible
            return NORTH;
    }
}

void game_util_move_coords(Direction direction, Coords *coords)
{
    switch (direction) {
        case NORTH: {
            --(coords->y);
            break;
        }

        case SOUTH: {
            ++(coords->y);
            break;
        }

        case EAST: {
            ++(coords->x);
            break;
        }

        case WEST: {
            --(coords->x);
            break;
        }

        default: {
            fprintf(stderr, "Warning: tried to move in an invalid direction\n");
            return;
        }
    }
}

int game_util_random_colour(void)
{
    int r = rand() % 6;

    switch (r) {
        case 0:
            return GREEN;

        case 1:
            return CYAN;

        case 2:
            return RED;

        case 3:
            return BLUE;

        case 4:
            return YELLOW;

        case 5:
            return MAGENTA;

        default:  // impossible
            return RED;
    }
}

static size_t net_pack_u16(uint8_t *bytes, uint16_t v)
{
    bytes[0] = (v >> 8) & 0xff;
    bytes[1] = v & 0xff;
    return sizeof(v);
}

static size_t net_unpack_u16(const uint8_t *bytes, uint16_t *v)
{
    uint8_t hi = bytes[0];
    uint8_t lo = bytes[1];
    *v = ((uint16_t)hi << 8) | lo;
    return sizeof(*v);
}

size_t game_util_pack_u32(uint8_t *bytes, uint32_t v)
{
    uint8_t *p = bytes;
    p += net_pack_u16(p, (v >> 16) & 0xffff);
    p += net_pack_u16(p, v & 0xffff);
    return p - bytes;
}

size_t game_util_unpack_u32(const uint8_t *bytes, uint32_t *v)
{
    const uint8_t *p = bytes;
    uint16_t hi;
    uint16_t lo;
    p += net_unpack_u16(p, &hi);
    p += net_unpack_u16(p, &lo);
    *v = ((uint32_t)hi << 16) | lo;
    return p - bytes;
}


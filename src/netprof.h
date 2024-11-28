/*  netprof.h
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

#ifndef TOXIC_NETPROF
#define TOXIC_NETPROF

#include <stdio.h>
#include <time.h>
#include <tox/tox.h>

void netprof_log_dump(const Tox *m, FILE *fp, time_t run_time);

uint64_t netprof_get_bytes_down(const Tox *m);
uint64_t netprof_get_bytes_up(const Tox *m);

#endif  // TOXIC_NETPROF

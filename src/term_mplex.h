/*  term_mplex.h
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

#ifndef TERM_MPLEX_H
#define TERM_MPLEX_H

/* Checks if Toxic runs inside a terminal multiplexer (GNU screen or tmux). If
 * yes, it initializes a timer which periodically checks the attached/detached
 * state of the terminal and updates away status accordingly.
 */
int init_mplex_away_timer(Toxic *toxic);

void lock_status(void);
void unlock_status(void);

#endif /* TERM_MPLEX_H */

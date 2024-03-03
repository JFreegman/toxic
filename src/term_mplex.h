/*  term_mplex.h
 *
 *  Copyright (C) 2015-2024 Toxic All Rights Reserved.
 *
 *  This file is part of Toxic. Toxic is free software licensed
 *  under the GNU General Public License 3.0.
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

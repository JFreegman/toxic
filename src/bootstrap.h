/*  bootstrap.h
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

#ifndef BOOTSTRAP_H
#define BOOTSTRAP_H

/* Manages connection to the Tox DHT network. */
void do_tox_connection(Toxic *toxic);

/* Creates a new thread that will load the DHT nodeslist to memory
 * from json encoded nodes file obtained at NODES_LIST_URL. Only one
 * thread may run at a time.
 *
 * Return 0 on success.
 * Return -1 if a thread is already active.
 * Return -2 if mutex fails to init.
 * Return -3 if pthread attribute fails to init.
 * Return -4 if pthread fails to set detached state.
 * Return -5 if thread creation fails.
 */
int load_DHT_nodeslist(const Toxic *toxic);

#endif /* BOOTSTRAP_H */

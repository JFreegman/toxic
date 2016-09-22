/*  bootstrap.h
 *
 *
 *  Copyright (C) 2016 Toxic All Rights Reserved.
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
void do_tox_connection(Tox *m);

/* Load the DHT nodeslist to memory from json encoded nodes file obtained at NODES_LIST_URL.
 * TODO: Parse json using a proper library?
 *
 * Return 0 on success.
 * Return -1 if nodeslist file cannot be opened or created.
 * Return -2 if nodeslist file cannot be parsed.
 * Return -3 if nodeslist file does not contain any valid node entries.
 */
int load_DHT_nodeslist(void);

#endif  /* BOOTSTRAP_H */

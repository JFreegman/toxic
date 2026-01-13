/*  bootstrap.h
 *
 *  Copyright (C) 2016-2026 Toxic All Rights Reserved.
 *
 *  This file is part of Toxic. Toxic is free software licensed
 *  under the GNU General Public License 3.0.
 */

#ifndef BOOTSTRAP_H
#define BOOTSTRAP_H

#include "toxic.h"

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
int load_DHT_nodeslist(Toxic *toxic);

#endif /* BOOTSTRAP_H */

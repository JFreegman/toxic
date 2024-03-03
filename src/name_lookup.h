/*  name_lookup.h
 *
 *  Copyright (C) 2015-2024 Toxic All Rights Reserved.
 *
 *  This file is part of Toxic. Toxic is free software licensed
 *  under the GNU General Public License 3.0.
 */

#ifndef NAME_LOOKUP
#define NAME_LOOKUP

#include "toxic.h"
#include "windows.h"

#include <stdbool.h>

/* Initializes http based name lookups. Note: This function must be called only once before additional
 * threads are spawned.
 *
 * Returns 0 on success.
 * Returns -1 if curl failed to init.
 * Returns -2 if the nameserver list cannot be found.
 * Returns -3 if the nameserver list does not contain any valid entries.
 */
int name_lookup_init(const char *nameserver_path, int curl_init_status);

/* Attempts to do a tox name lookup.
 *
 * Returns true on success.
 */
bool name_lookup(ToxWindow *self, Toxic *toxic, const char *id_bin, const char *addr, const char *message);

#endif /* NAME_LOOKUP */

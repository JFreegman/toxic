/*  name_lookup.h
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

#ifndef NAME_LOOKUP
#define NAME_LOOKUP

/* Initializes http based name lookups.
 *
 * Note: This function must be called only once before additional threads are spawned.
 *
 * Returns 0 on success.
 * Returns -1 on failure.
 */
int name_lookup_init(int curl_init_status);

/* Attempts to do a tox name lookup.
 *
 * Returns true on success.
 */
bool name_lookup(ToxWindow *self, Toxic *toxic, const char *id_bin, const char *addr, const char *message);

#endif /* NAME_LOOKUP */

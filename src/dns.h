/*  dns.c
 *
 *
 *  Copyright (C) 2014 Toxic All Rights Reserved.
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

/* Does DNS lookup for addr and puts resulting tox id in id_bin.
   Return 0 on success, -1 on failure. */

#ifndef _dns_h
#define _dns_h

/* creates new thread for dns3 lookup. Only allows one lookup at a time. */
void dns3_lookup(ToxWindow *self, Tox *m, uint8_t *id_bin, uint8_t *addr, uint8_t *msg);

#endif /* #define _dns_h */

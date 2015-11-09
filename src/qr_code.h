/*  qr_code.h
 *
 *
 *  Copyright (C) 2015 Toxic All Rights Reserved.
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

#ifndef QR_CODE
#define QR_CODE

#define QRCODE_FILENAME_EXT ".QRcode"

/* Converts a tox ID string into a QRcode and prints it to the output stream.
 *
 * Returns 0 on success.
 * Returns -1 on failure.
 */
int ID_to_QRcode_string(const char *tox_id, FILE *output);

#endif /* QR_CODE */

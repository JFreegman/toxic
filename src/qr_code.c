/*  qr_code.c
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

#include <stdlib.h>
#include <qrencode.h>
#include <stdbool.h>

#include "toxic.h"
#include "windows.h"
#include "qr_code.h"

#define BORDER "|"

static void add_border(FILE *output, unsigned width)
{
    unsigned x, y;

    for (y = 0; y < 4; y += 2) {
        fprintf(output, BORDER);

        for (x = 0; x < 4 + width + 4; x++)
            fprintf(output, "\342\226\210");

        fprintf(output, BORDER "\n");
    }
}

/* Converts a tox ID string into a QRcode and prints it to the output stream.
 *
 * Returns 0 on success.
 * Returns -1 on failure.
 */
int ID_to_QRcode_string(const char *tox_id, FILE *output)
{
    if (output == NULL)
        return -1;

    QRcode *qr = QRcode_encodeString(tox_id, 0, QR_ECLEVEL_L, QR_MODE_8, 0);

    if (qr == NULL)
        return -1;

    add_border(output, qr->width);

    unsigned y, x;

    for (y = 0; y < (unsigned) qr->width; y += 2) {
        const unsigned char *row1 = qr->data + qr->width * y;
        const unsigned char *row2 = row1 + qr->width;

        fprintf(output, BORDER);
        for (x = 0; x < 4; ++x)
            fprintf(output, "\342\226\210");

        for (x = 0; x < (unsigned) qr->width; ++x) {
            bool a = row1[x] & 1;
            bool b = (y + 1) < (unsigned) qr->width ? (row2[x] & 1) : false;

            if (a && b)
                fprintf(output, " ");
            else if (a)
                fprintf(output, "\342\226\204");
            else if (b)
                fprintf(output, "\342\226\200");
            else
                fprintf(output, "\342\226\210");
        }

        for (x = 0; x < 4; ++x)
            fprintf(output, "\342\226\210");

        fprintf(output, BORDER "\n");
    }

    QRcode_free(qr);

    return 0;
}

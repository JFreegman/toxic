/*  qr_obj_code.c
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

#define BORDER_CHAR "|"
#define BORDER_LEN 1
#define CHAR_1 "\342\226\210"
#define CHAR_2 "\342\226\204"
#define CHAR_3 "\342\226\200"

/* Converts a tox ID string into a QRcode and prints it to the given file stream.
 *
 * Returns 0 on success.
 * Returns -1 on failure.
 */
int ID_to_QRcode(const char *tox_id, FILE *fp)
{
    if (fp == NULL)
        return -1;

    QRcode *qr_obj = QRcode_encodeString(tox_id, 0, QR_ECLEVEL_L, QR_MODE_8, 0);

    if (qr_obj == NULL)
        return -1;

    size_t width = qr_obj->width;
    size_t i, j;

    for (i = 0; i < width + BORDER_LEN * 2; ++i)
        fprintf(fp, "%s", CHAR_1);

    fprintf(fp, "\n");

    for (i = 0; i < width; i += 2) {
        for (j = 0; j < BORDER_LEN; ++j)
            fprintf(fp, "%s", CHAR_1);

        const unsigned char *row_1 = qr_obj->data + width * i;
        const unsigned char *row_2 = row_1 + width;

        for (j = 0; j < width; ++j) {
            bool x = row_1[j] & 1;
            bool y = (i + 1) < width ? (row_2[j] & 1) : false;

            if (x && y)
                fprintf(fp, " ");
            else if (x)
                fprintf(fp, "%s", CHAR_2);
            else if (y)
                fprintf(fp, "%s", CHAR_3);
            else
                fprintf(fp, "%s", CHAR_1);
        }

        for (j = 0; j < BORDER_LEN; ++j)
            fprintf(fp, "%s", CHAR_1);

        fprintf(fp, "\n");
    }

    QRcode_free(qr_obj);

    return 0;
}

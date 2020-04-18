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

#ifdef QRCODE

#include <qrencode.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "qr_code.h"
#include "toxic.h"
#include "windows.h"

#ifdef QRPNG
#include <png.h>
#define INCHES_PER_METER (100.0/2.54)
#define DPI 72
#define SQUARE_SIZE 5
#endif /* QRPNG */

#define BORDER_LEN 1
#define CHAR_1 "\342\226\210"
#define CHAR_2 "\342\226\204"
#define CHAR_3 "\342\226\200"

/* Converts a tox ID string into a QRcode and prints it into the given filename.
 *
 * Returns 0 on success.
 * Returns -1 on failure.
 */
int ID_to_QRcode_txt(const char *tox_id, const char *outfile)
{
    FILE *fp = fopen(outfile, "wb");

    if (fp == NULL) {
        return -1;
    }

    QRcode *qr_obj = QRcode_encodeString(tox_id, 0, QR_ECLEVEL_L, QR_MODE_8, 0);

    if (qr_obj == NULL) {
        fclose(fp);
        return -1;
    }

    size_t width = qr_obj->width;
    size_t i, j;

    for (i = 0; i < width + BORDER_LEN * 2; ++i) {
        fprintf(fp, "%s", CHAR_1);
    }

    fprintf(fp, "\n");

    for (i = 0; i < width; i += 2) {
        for (j = 0; j < BORDER_LEN; ++j) {
            fprintf(fp, "%s", CHAR_1);
        }

        const unsigned char *row_1 = qr_obj->data + width * i;
        const unsigned char *row_2 = row_1 + width;

        for (j = 0; j < width; ++j) {
            bool x = row_1[j] & 1;
            bool y = (i + 1) < width ? (row_2[j] & 1) : false;

            if (x && y) {
                fprintf(fp, " ");
            } else if (x) {
                fprintf(fp, "%s", CHAR_2);
            } else if (y) {
                fprintf(fp, "%s", CHAR_3);
            } else {
                fprintf(fp, "%s", CHAR_1);
            }
        }

        for (j = 0; j < BORDER_LEN; ++j) {
            fprintf(fp, "%s", CHAR_1);
        }

        fprintf(fp, "\n");
    }

    fclose(fp);
    QRcode_free(qr_obj);

    return 0;
}

#ifdef QRPNG
/* Converts a tox ID string into a QRcode and prints it into the given filename as png.
 *
 * Returns 0 on success.
 * Returns -1 on failure.
 */
int ID_to_QRcode_png(const char *tox_id, const char *outfile)
{
    static FILE *fp;
    unsigned char *p;
    unsigned char black[4] = {0, 0, 0, 255};
    size_t x, y, xx, yy, real_width;
    png_structp png_ptr;
    png_infop info_ptr;

    fp = fopen(outfile, "wb");

    if (fp == NULL) {
        return -1;
    }

    QRcode *qr_obj = QRcode_encodeString(tox_id, 0, QR_ECLEVEL_L, QR_MODE_8, 0);

    if (qr_obj == NULL) {
        fclose(fp);
        return -1;
    }

    real_width = (qr_obj->width + BORDER_LEN * 2) * SQUARE_SIZE;
    size_t row_size = real_width * 4;
    unsigned char *row = malloc(row_size);

    if (row == NULL) {
        fclose(fp);
        QRcode_free(qr_obj);
        return -1;
    }

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

    if (png_ptr == NULL) {
        fclose(fp);
        free(row);
        QRcode_free(qr_obj);
        return -1;
    }

    info_ptr = png_create_info_struct(png_ptr);

    if (info_ptr == NULL) {
        fclose(fp);
        free(row);
        QRcode_free(qr_obj);
        return -1;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        fclose(fp);
        free(row);
        QRcode_free(qr_obj);
        png_destroy_write_struct(&png_ptr, &info_ptr);
        return -1;
    }

    png_init_io(png_ptr, fp);
    png_set_IHDR(png_ptr, info_ptr, real_width, real_width, 8,
                 PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_set_pHYs(png_ptr, info_ptr, DPI * INCHES_PER_METER,
                 DPI * INCHES_PER_METER, PNG_RESOLUTION_METER);
    png_write_info(png_ptr, info_ptr);

    /* top margin */
    memset(row, 0xff, row_size);

    for (y = 0; y < BORDER_LEN * SQUARE_SIZE; y++) {
        png_write_row(png_ptr, row);
    }

    /* data */
    p = qr_obj->data;

    for (y = 0; y < qr_obj->width; y++) {
        memset(row, 0xff, row_size);

        for (x = 0; x < qr_obj->width; x++) {
            for (xx = 0; xx < SQUARE_SIZE; xx++) {
                if (*p & 1) {
                    memcpy(&row[((BORDER_LEN + x) * SQUARE_SIZE + xx) * 4], black, 4);
                }
            }

            p++;
        }

        for (yy = 0; yy < SQUARE_SIZE; yy++) {
            png_write_row(png_ptr, row);
        }
    }

    /* bottom margin */
    memset(row, 0xff, row_size);

    for (y = 0; y < BORDER_LEN * SQUARE_SIZE; y++) {
        png_write_row(png_ptr, row);
    }

    free(row);
    fclose(fp);

    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);

    QRcode_free(qr_obj);

    return 0;
}
#endif /* QRPNG */

#endif /* QRCODE */

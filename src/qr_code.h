/*  qr_code.h
 *
 *  Copyright (C) 2015-2024 Toxic All Rights Reserved.
 *
 *  This file is part of Toxic. Toxic is free software licensed
 *  under the GNU General Public License 3.0.
 */

#ifndef QR_CODE
#define QR_CODE

#ifdef QRCODE

#define QRCODE_FILENAME_EXT ".QRcode"

/* Converts a tox ID string into a QRcode and prints it into the given filename.
 *
 * Returns 0 on success.
 * Returns -1 on failure.
 */
int ID_to_QRcode_txt(const char *tox_id, const char *outfile);

#ifdef QRPNG
#define QRCODE_FILENAME_EXT_PNG ".QRcode.png"
/* Converts a tox ID string into a QRcode and prints it into the given filename as png.
 *
 * Returns 0 on success.
 * Returns -1 on failure.
 */
int ID_to_QRcode_png(const char *tox_id, const char *outfile);
#endif /* QRPNG */

#endif /* QRCODE */

#endif /* QR_CODE */

/*  curl_util.h
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

#ifndef CURL_UTIL_H
#define CURL_UTIL_H

#include <stdint.h>

/* List based on Mozilla's recommended configurations for modern browsers */
#define TLS_CIPHER_SUITE_LIST "ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-AES256-GCM-SHA384:DHE-RSA-AES128-GCM-SHA256:DHE-DSS-AES128-GCM-SHA256:kEDH+AESGCM:ECDHE-RSA-AES128-SHA256:ECDHE-ECDSA-AES128-SHA256:ECDHE-RSA-AES128-SHA:ECDHE-ECDSA-AES128-SHA:ECDHE-RSA-AES256-SHA384:ECDHE-ECDSA-AES256-SHA384:ECDHE-RSA-AES256-SHA:ECDHE-ECDSA-AES256-SHA:DHE-RSA-AES128-SHA256:DHE-RSA-AES128-SHA:DHE-DSS-AES128-SHA256:DHE-RSA-AES256-SHA256:DHE-DSS-AES256-SHA:DHE-RSA-AES256-SHA:!aNULL:!eNULL:!EXPORT:!DES:!RC4:!3DES:!MD5:!PSK"

/* Max size of an http response that we can store in Recv_Data */
#define MAX_RECV_CURL_DATA_SIZE 32767

/* Holds data received from curl lookup */
struct Recv_Curl_Data {
    char data[MAX_RECV_CURL_DATA_SIZE + 1];   /* Data received from curl write data callback */
    size_t length;  /* Total number of bytes written to data buffer (doesn't include null) */
};

/* Sets proxy info for given CURL handler.
 *
 * Returns 0 on success or if no proxy is set by the client.
 * Returns -1 if proxy info is invalid.
 * Returns an int > 0 on curl error (see: https://curl.haxx.se/libcurl/c/libcurl-errors.html)
 */
int set_curl_proxy(CURL *c_handle, const char *proxy_address, uint16_t port, uint8_t proxy_type);

/* Callback function for CURL to write received data.
 *
 * This function will append data from an http request to the data buffer
 * until the request is complete or the buffer is full. Buffer will be null terminated.
 *
 * Returns size of bytes written to the data buffer.
 */
size_t curl_cb_write_data(void *data, size_t size, size_t nmemb, void *user_pointer);

#endif /* CURL_UTIL_H */

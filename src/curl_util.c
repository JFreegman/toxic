/*  curl_util.c
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

#include <stdint.h>
#include <string.h>

#include <curl/curl.h>
#include <tox/tox.h>

#include "curl_util.h"

/* Sets proxy info for given CURL handler.
 *
 * Returns 0 on success or if no proxy is set by the client.
 * Returns -1 if proxy info is invalid.
 * Returns an int > 0 on curl error (see: https://curl.haxx.se/libcurl/c/libcurl-errors.html)
 */
int set_curl_proxy(CURL *c_handle, const char *proxy_address, uint16_t port, uint8_t proxy_type)
{
    if (proxy_type == TOX_PROXY_TYPE_NONE) {
        return 0;
    }

    if (proxy_address == NULL || port == 0) {
        return -1;
    }

    int ret = curl_easy_setopt(c_handle, CURLOPT_PROXYPORT, (long) port);

    if (ret != CURLE_OK) {
        return ret;
    }

    long int type = proxy_type == TOX_PROXY_TYPE_SOCKS5 ? CURLPROXY_SOCKS5_HOSTNAME : CURLPROXY_HTTP;

    ret = curl_easy_setopt(c_handle, CURLOPT_PROXYTYPE, type);

    if (ret != CURLE_OK) {
        return ret;
    }

    ret = curl_easy_setopt(c_handle, CURLOPT_PROXY, proxy_address);

    if (ret != CURLE_OK) {
        return ret;
    }

    return 0;
}

/* Callback function for CURL to write received data.
 *
 * This function will append data from an http request to the data buffer
 * until the request is complete or the buffer is full. Buffer will be null terminated.
 *
 * Returns number of bytes received from http request on success (don't change this).
 * Returns 0 if data exceeds buffer size.
 */
size_t curl_cb_write_data(void *data, size_t size, size_t nmemb, void *user_pointer)
{
    struct Recv_Curl_Data *recv_data = (struct Recv_Curl_Data *) user_pointer;

    size_t length = size * nmemb;
    size_t total_size = length + recv_data->length;

    if (total_size > MAX_RECV_CURL_DATA_SIZE) {
        return 0;
    }

    memcpy(recv_data->data + recv_data->length, data, length);
    recv_data->data[total_size] = '\0';
    recv_data->length += length;

    return length;
}

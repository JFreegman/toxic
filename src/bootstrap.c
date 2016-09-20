/*  bootstrap.c
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

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>

#include <curl/curl.h>
#include <tox/tox.h>

#include "line_info.h"
#include "windows.h"
#include "misc_tools.h"
#include "configdir.h"
#include "curl_util.h"


extern struct arg_opts arg_opts;

/* URL that we get the JSON encoded nodes list from. */
#define NODES_LIST_URL "https://nodes.tox.chat/json"

#define DEFAULT_NODES_FILENAME "DHTnodes.json"

/* Time to wait between bootstrap attempts */
#define TRY_BOOTSTRAP_INTERVAL 5

/* Number of nodes to bootstrap to per try */
#define NUM_BOOTSTRAP_NODES 5

/* How often we should update the nodeslist file. */
#define NODELIST_UPDATE_TIMEOUT (60*24*30)

#define IPv4_MAX_SIZE 64
#define PORT_MAX_SIZE 5

#define LAST_SCAN_JSON_VALUE "\"last_scan\":"
#define LAST_SCAN_JSON_VALUE_LEN (sizeof(LAST_SCAN_JSON_VALUE) - 1)

#define IPV4_JSON_VALUE "\"ipv4\":\""
#define IPV4_JSON_VALUE_LEN (sizeof(IPV4_JSON_VALUE) - 1)

#define PORT_JSON_VALUE "\"port\":"
#define PORT_JSON_VALUE_LEN (sizeof(PORT_JSON_VALUE) - 1)

#define KEY_JSON_VALUE "\"public_key\":\""
#define KEY_JSON_VALUE_LEN (sizeof(KEY_JSON_VALUE) - 1)

#define MIN_NODE_LINE  50   /* IP: 7 + port: 5 + key: 38 + spaces: 2 = 70. ! (& e.g. tox.chat = 8) */
#define MAX_NODE_LINE  300  /* Max number of chars in a sever line (name + port + key) */
#define MAXNODES 50
#define NODELEN (MAX_NODE_LINE - TOX_PUBLIC_KEY_SIZE - 7)
#define MAX_NODELIST_SIZE (1024 * MAXNODES)

static struct toxNodes {
    size_t lines;
    char nodes[MAXNODES][NODELEN];
    uint16_t ports[MAXNODES];
    char keys[MAXNODES][TOX_PUBLIC_KEY_SIZE];
} toxNodes;


/* Return true if nodeslist pointed to by fp needs to be updated.
 * This will be the case if the file is empty, has an invalid format,
 * or if the file is older than the given timeout.
 */
static bool nodeslist_needs_update(const char *nodes_path)
{
    FILE *fp = fopen(nodes_path, "r+");

    if (fp == NULL) {
        return false;
    }

    /* last_scan value should be at beginning of file */
    char line[LAST_SCAN_JSON_VALUE_LEN + 32];

    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        return true;
    }

    fclose(fp);

    const char *last_scan_val = strstr(line, LAST_SCAN_JSON_VALUE);

    if (last_scan_val == NULL) {
        return true;
    }

    last_scan_val += LAST_SCAN_JSON_VALUE_LEN;
    long long int last_scan = strtoll(last_scan_val, NULL, 10);

    if (timed_out(last_scan, NODELIST_UPDATE_TIMEOUT)) {
        return true;
    }

    return false;
}

/* Fetches the JSON encoded DHT nodeslist from NODES_LIST_URL.
 *
 * Return 0 on success.
 * Return -1 on failure.
 */
static int curl_fetch_nodes_JSON(struct Recv_Data *recv_data)
{
    CURL *c_handle = curl_easy_init();

    if (c_handle == NULL) {
        return -1;
    }

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "charsets: utf-8");

    curl_easy_setopt(c_handle, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(c_handle, CURLOPT_URL, NODES_LIST_URL);
    curl_easy_setopt(c_handle, CURLOPT_WRITEFUNCTION, write_lookup_data);
    curl_easy_setopt(c_handle, CURLOPT_WRITEDATA, recv_data);
    curl_easy_setopt(c_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    curl_easy_setopt(c_handle, CURLOPT_HTTPGET, 1L);

    int proxy_ret = set_curl_proxy(c_handle, arg_opts.proxy_address, arg_opts.proxy_port, arg_opts.proxy_type);

    if (proxy_ret != 0) {
        fprintf(stderr, "set_curl_proxy() failed with error %d\n", proxy_ret);
        return -1;
    }

    int ret = curl_easy_setopt(c_handle, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);

    if (ret != CURLE_OK) {
        fprintf(stderr, "TLSv1.2 could not be set (libcurl error %d)", ret);
        return -1;
    }

    ret = curl_easy_setopt(c_handle, CURLOPT_SSL_CIPHER_LIST, TLS_CIPHER_SUITE_LIST);

    if (ret != CURLE_OK) {
        fprintf(stderr, "Failed to set TLS cipher list (libcurl error %d)", ret);
        return -1;
    }

    ret = curl_easy_perform(c_handle);

    if (ret != CURLE_OK) {
        /* If system doesn't support any of the specified ciphers suites, fall back to default */
        if (ret == CURLE_SSL_CIPHER) {
            curl_easy_setopt(c_handle, CURLOPT_SSL_CIPHER_LIST, NULL);
            ret = curl_easy_perform(c_handle);
        }

        if (ret != CURLE_OK) {
            fprintf(stderr, "HTTPS lookup error (libcurl error %d)\n", ret);
            return -1;
        }
    }

    return 0;
}

/* Attempts to update the DHT nodeslist.
 *
 * Return 1 if list was updated successfully.
 * Return 0 if list does not need to be updated.
 * Return -1 if file cannot be opened.
 * Return -2 if http lookup failed.
 * Return -3 if http reponse was empty.
 * Return -4 if data could not be written to disk.
 */
static int update_DHT_nodeslist(const char *nodes_path)
{
    if (!nodeslist_needs_update(nodes_path)) {
        return 0;
    }

    FILE *fp = fopen(nodes_path, "r+");

    if (fp == NULL) {
        return -1;
    }

    struct Recv_Data recv_data;
    memset(&recv_data, 0, sizeof(struct Recv_Data));

    if (curl_fetch_nodes_JSON(&recv_data) == -1) {
        fclose(fp);
        return -2;
    }

    if (recv_data.length == 0) {
        fclose(fp);
        return -3;
    }

    if (fwrite(recv_data.data, recv_data.length, 1, fp) != 1) {
        fclose(fp);
        return -4;
    }

    fclose(fp);
    return 1;
}

static void get_nodeslist_path(char *buf, size_t buf_size)
{
    char *config_dir = NULL;

    if (arg_opts.nodes_path[0]) {
        snprintf(buf, buf_size, "%s", arg_opts.nodes_path);
    } else if ((config_dir = get_user_config_dir()) != NULL) {
        snprintf(buf, buf_size, "%s%s%s", config_dir, CONFIGDIR, DEFAULT_NODES_FILENAME);
        free(config_dir);
    } else {
        snprintf(buf, buf_size, "%s", DEFAULT_NODES_FILENAME);
    }
}

/* Load the DHT nodeslist to memory from json encoded nodes file obtained at NODES_LIST_URL.
 * TODO: Parse json using a proper library?
 *
 * Return 0 on success.
 * Return -1 if nodeslist file cannot be opened or created.
 * Return -2 if nodeslist file cannot be parsed.
 * Return -3 if nodeslist file does not contain any valid node entries.
 */
int load_DHT_nodeslist(void)
{
    char nodes_path[PATH_MAX];
    get_nodeslist_path(nodes_path, sizeof(nodes_path));

    FILE *fp = NULL;

    if (!file_exists(nodes_path)) {
        if ((fp = fopen(nodes_path, "w+")) == NULL) {
            return -1;
        }
    } else if ((fp = fopen(nodes_path, "r+")) == NULL) {
        return -1;
    }

    int update_err = update_DHT_nodeslist(nodes_path);

    if (update_err < 0) {
        fprintf(stderr, "update_DHT_nodeslist() failed with error %d\n", update_err);
    }

    char line[MAX_NODELIST_SIZE];

    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        return -2;
    }

    const char *line_start = line;

    while ((line_start = strstr(line_start + 1, IPV4_JSON_VALUE)) && toxNodes.lines < MAXNODES) {
        /* Extract IPv4 address */
        const char *ip_start = strstr(line_start, IPV4_JSON_VALUE);

        if (ip_start == NULL) {
            continue;
        }

        ip_start += IPV4_JSON_VALUE_LEN;
        int ip_len = char_find(0, ip_start, '"');

        if (ip_len == 0 || ip_len > IPv4_MAX_SIZE) {
            continue;
        }

        char ipv4_string[ip_len + 1];
        memcpy(ipv4_string, ip_start, ip_len);
        ipv4_string[ip_len] = 0;

        /* Extract port */
        const char *port_start = strstr(ip_start, PORT_JSON_VALUE);

        if (!port_start) {
            continue;
        }

        port_start += PORT_JSON_VALUE_LEN;
        int port_len = char_find(0, port_start, ',');

        if (port_len == 0 || port_len > PORT_MAX_SIZE) {
            continue;
        }

        char port_string[port_len + 1];
        memcpy(port_string, port_start, port_len);
        port_string[port_len] = 0;

        long int port = strtol(port_string, NULL, 10);

        if (port <= 0 || port > MAX_PORT_RANGE) {
            continue;
        }

        /* Extract key */
        const char *key_start = strstr(port_start, KEY_JSON_VALUE);

        if (!key_start) {
            continue;
        }

        key_start += KEY_JSON_VALUE_LEN;
        int key_len = char_find(0, key_start, '"');

        if (key_len != TOX_PUBLIC_KEY_SIZE * 2) {
            continue;
        }

        char key_string[TOX_PUBLIC_KEY_SIZE * 2 + 1];
        memcpy(key_string, key_start, TOX_PUBLIC_KEY_SIZE * 2);
        key_string[TOX_PUBLIC_KEY_SIZE * 2] = 0;

        /* Add entry to nodes list */
        snprintf(toxNodes.nodes[toxNodes.lines], sizeof(toxNodes.nodes[toxNodes.lines]), "%s", ipv4_string);
        toxNodes.ports[toxNodes.lines] = port;

        if (hex_string_to_bin(key_string, key_len, toxNodes.keys[toxNodes.lines], TOX_PUBLIC_KEY_SIZE) == -1)
            continue;

        toxNodes.lines++;
    }

    /* If nodeslist does not contain any valid entries we set the last_scan value
     * to 0 so that it will fetch a new list the next time this function is called.
     */
    if (toxNodes.lines == 0) {
        const char *s = "{\"last_scan\":0}";
        rewind(fp);
        fwrite(s, strlen(s), 1, fp);  // Not much we can do if it fails
        fclose(fp);
        return -3;
    }

    fclose(fp);
    return 0;
}

/* Connects to NUM_BOOTSTRAP_NODES random DHT nodes listed in the DHTnodes file. */
static void DHT_bootstrap(Tox *m)
{
    if (toxNodes.lines == 0) {
        return;
    }

    size_t i;

    for (i = 0; i < NUM_BOOTSTRAP_NODES; ++i) {
        size_t node = rand() % toxNodes.lines;

        TOX_ERR_BOOTSTRAP err;
        tox_bootstrap(m, toxNodes.nodes[node], toxNodes.ports[node], (uint8_t *) toxNodes.keys[node], &err);

        if (err != TOX_ERR_BOOTSTRAP_OK) {
            fprintf(stderr, "Failed to bootstrap %s:%d\n", toxNodes.nodes[node], toxNodes.ports[node]);
        }

        tox_add_tcp_relay(m, toxNodes.nodes[node], toxNodes.ports[node], (uint8_t *) toxNodes.keys[node], &err);

        if (err != TOX_ERR_BOOTSTRAP_OK) {
            fprintf(stderr, "Failed to add TCP relay %s:%d\n", toxNodes.nodes[node], toxNodes.ports[node]);
        }
    }
}

/* Manages connection to the Tox DHT network. */
void do_tox_connection(Tox *m)
{
    static uint64_t last_bootstrap_time = 0;
    bool connected = tox_self_get_connection_status(m) != TOX_CONNECTION_NONE;

    if (!connected && timed_out(last_bootstrap_time, TRY_BOOTSTRAP_INTERVAL)) {
        DHT_bootstrap(m);
        last_bootstrap_time = get_unix_time();
    }
}

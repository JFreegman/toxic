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

#include <tox/tox.h>

#include "line_info.h"
#include "windows.h"
#include "misc_tools.h"


extern struct arg_opts arg_opts;


/* Time to wait between bootstrap attempts */
#define TRY_BOOTSTRAP_INTERVAL 5

#define IPv4_MAX_SIZE 64
#define PORT_MAX_SIZE 5

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

/* Load the DHT nodelist to memory from json formatted nodes file obtained at https://nodes.tox.chat/json.
 *
 * Return 0 on success.
 * Return -1 if nodelist file cannot be opened.
 * Return -2 if nodelist file cannot be parsed.
 * Return -3 if nodelist file does not contain any valid node entries.
 */
int load_DHT_nodelist(void)
{
    const char *filename = !arg_opts.nodes_path[0] ? PACKAGE_DATADIR "/DHTnodes" : arg_opts.nodes_path;
    FILE *fp = fopen(filename, "r");

    if (fp == NULL)
        return -1;

    char line[MAX_NODELIST_SIZE];

    if (fgets(line, sizeof(line), fp) == NULL) {
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

        if (port <= 0 || port > MAX_PORT_RANGE)
            continue;

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

        /* Add IP-Port-Key to nodes list */
        snprintf(toxNodes.nodes[toxNodes.lines], sizeof(toxNodes.nodes[toxNodes.lines]), "%s", ipv4_string);
        toxNodes.ports[toxNodes.lines] = port;

        if (hex_string_to_bin(key_string, key_len, toxNodes.keys[toxNodes.lines], TOX_PUBLIC_KEY_SIZE) == -1)
            continue;

        toxNodes.lines++;
    }

    fclose(fp);

    if (toxNodes.lines == 0)
        return -3;

    return 0;
}

/* Connects to a random DHT node listed in the DHTnodes file. */
static void DHT_bootstrap(Tox *m)
{
    if (toxNodes.lines == 0) {
        return;
    }

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

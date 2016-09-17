/*  bootstrap.c
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


#define MIN_NODE_LINE  50 /* IP: 7 + port: 5 + key: 38 + spaces: 2 = 70. ! (& e.g. tox.chat = 8) */
#define MAX_NODE_LINE  256 /* Approx max number of chars in a sever line (name + port + key) */
#define MAXNODES 50
#define NODELEN (MAX_NODE_LINE - TOX_PUBLIC_KEY_SIZE - 7)

static struct toxNodes {
    size_t lines;
    char nodes[MAXNODES][NODELEN];
    uint16_t ports[MAXNODES];
    char keys[MAXNODES][TOX_PUBLIC_KEY_SIZE];
} toxNodes;

/* Load the DHT nodelist to memory.
 *
 * Return 0 on success.
 * Return -1 if nodelist file cannot be opened.
 * Return -2 if nodelist file does not contain any valid node entries.
 */
int load_DHT_nodelist(void)
{

    const char *filename = !arg_opts.nodes_path[0] ? PACKAGE_DATADIR "/DHTnodes" : arg_opts.nodes_path;

    FILE *fp = fopen(filename, "r");

    if (fp == NULL)
        return -1;

    char line[MAX_NODE_LINE];

    while (fgets(line, sizeof(line), fp) && toxNodes.lines < MAXNODES) {
        size_t line_len = strlen(line);

        if (line_len >= MIN_NODE_LINE && line_len <= MAX_NODE_LINE) {
            const char *name = strtok(line, " ");
            const char *port_str = strtok(NULL, " ");
            const char *key_ascii = strtok(NULL, " ");

            if (name == NULL || port_str == NULL || key_ascii == NULL)
                continue;

            long int port = strtol(port_str, NULL, 10);

            if (port <= 0 || port > MAX_PORT_RANGE)
                continue;

            size_t key_len = strlen(key_ascii);
            size_t name_len = strlen(name);

            if (key_len < TOX_PUBLIC_KEY_SIZE * 2 || name_len >= NODELEN)
                continue;

            snprintf(toxNodes.nodes[toxNodes.lines], sizeof(toxNodes.nodes[toxNodes.lines]), "%s", name);
            toxNodes.nodes[toxNodes.lines][NODELEN - 1] = 0;
            toxNodes.ports[toxNodes.lines] = port;

            /* remove possible trailing newline from key string */
            char real_ascii_key[TOX_PUBLIC_KEY_SIZE * 2 + 1];
            memcpy(real_ascii_key, key_ascii, TOX_PUBLIC_KEY_SIZE * 2);
            key_len = TOX_PUBLIC_KEY_SIZE * 2;
            real_ascii_key[key_len] = '\0';

            if (hex_string_to_bin(real_ascii_key, key_len, toxNodes.keys[toxNodes.lines], TOX_PUBLIC_KEY_SIZE) == -1)
                continue;

            toxNodes.lines++;
        }
    }

    fclose(fp);

    if (toxNodes.lines == 0)
        return -2;

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

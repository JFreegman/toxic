/*  bootstrap.c
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

#include <arpa/inet.h>
#include <limits.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include <curl/curl.h>
#include <tox/tox.h>

#include "configdir.h"
#include "curl_util.h"
#include "line_info.h"
#include "misc_tools.h"
#include "prompt.h"
#include "settings.h"
#include "windows.h"

/* URL that we get the JSON encoded nodes list from. */
#define NODES_LIST_URL "https://nodes.tox.chat/json"

#define DEFAULT_NODES_FILENAME "DHTnodes.json"

/* Time to wait between bootstrap attempts */
#define TRY_BOOTSTRAP_INTERVAL 5

/* Number of nodes to bootstrap to per try */
#define NUM_BOOTSTRAP_NODES 5

/* Number of seconds since last successful ping before we consider a node offline */
#define NODE_OFFLINE_TIMOUT (60*60*24*2)

#define IP_MAX_SIZE 45
#define IP_MIN_SIZE 7

#define LAST_SCAN_JSON_KEY "\"last_scan\":"
#define LAST_SCAN_JSON_KEY_LEN (sizeof(LAST_SCAN_JSON_KEY) - 1)

#define IPV4_JSON_KEY "\"ipv4\":\""
#define IPV4_JSON_KEY_LEN (sizeof(IPV4_JSON_KEY) - 1)

#define IPV6_JSON_KEY "\"ipv6\":\""
#define IPV6_JSON_KEY_LEN (sizeof(IPV6_JSON_KEY) - 1)

#define PORT_JSON_KEY "\"port\":"
#define PORT_JSON_KEY_LEN (sizeof(PORT_JSON_KEY) - 1)

#define PK_JSON_KEY "\"public_key\":\""
#define PK_JSON_KEY_LEN (sizeof(PK_JSON_KEY) - 1)

#define LAST_PING_JSON_KEY "\"last_ping\":"
#define LAST_PING_JSON_KEY_LEN (sizeof(LAST_PING_JSON_KEY) - 1)

/* Maximum allowable size of the nodes list */
#define MAX_NODELIST_SIZE (MAX_RECV_CURL_DATA_SIZE)

static struct Thread_Data {
    pthread_t tid;
    pthread_attr_t attr;
    pthread_mutex_t lock;
    volatile bool active;
} thread_data;

#define MAX_NODES 50
struct Node {
    char ip4[IP_MAX_SIZE + 1];
    bool have_ip4;

    char ip6[IP_MAX_SIZE + 1];
    bool have_ip6;

    char key[TOX_PUBLIC_KEY_SIZE];
    uint16_t port;
};

static struct DHT_Nodes {
    struct Node list[MAX_NODES];
    size_t count;
    time_t last_updated;
} Nodes;

/* Return true if address appears to be a valid ipv4 address. */
static bool is_ip4_address(const char *address)
{
    struct sockaddr_in s_addr;
    return inet_pton(AF_INET, address, &(s_addr.sin_addr)) != 0;
}

/* Return true if address roughly appears to be a valid ipv6 address.
 *
 * TODO: Improve this function (inet_pton behaves strangely with ipv6).
 * for now the only guarantee is that it won't return true if the
 * address is a domain or ipv4 address, and should only be used if you're
 * reasonably sure that the address is one of the three (ipv4, ipv6 or a domain).
 */
static bool is_ip6_address(const char *address)
{
    size_t num_colons = 0;
    char ch = 0;

    for (size_t i = 0; (ch = address[i]); ++i) {
        if (ch == '.') {
            return false;
        }

        if (ch == ':') {
            ++num_colons;
        }
    }

    return num_colons > 1 && num_colons < 8;
}

/* Determine if a node is offline by comparing the age of the nodeslist
 * to the last time the node was successfully pinged.
 */
static bool node_is_offline(unsigned long long int last_ping)
{
    return last_ping + NODE_OFFLINE_TIMOUT <= last_ping;
}

/* Return true if nodeslist pointed to by fp needs to be updated.
 * This will be the case if the file is empty, has an invalid format,
 * or if the file is older than the given timeout.
 */
static bool nodeslist_needs_update(const char *nodes_path, int update_frequency)
{
    if (update_frequency <= 0) {
        return false;
    }

    FILE *fp = fopen(nodes_path, "r+");

    if (fp == NULL) {
        return false;
    }

    /* last_scan value should be at beginning of file */
    char line[LAST_SCAN_JSON_KEY_LEN + 32];

    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        return true;
    }

    fclose(fp);

    const char *last_scan_val = strstr(line, LAST_SCAN_JSON_KEY);

    if (last_scan_val == NULL) {
        return true;
    }

    long long int last_scan = strtoll(last_scan_val + LAST_SCAN_JSON_KEY_LEN, NULL, 10);

    pthread_mutex_lock(&thread_data.lock);
    Nodes.last_updated = last_scan;
    pthread_mutex_unlock(&thread_data.lock);

    pthread_mutex_lock(&Winthread.lock);
    bool is_timeout = timed_out(last_scan, update_frequency * 24 * 60 * 60);
    pthread_mutex_unlock(&Winthread.lock);

    if (is_timeout) {
        return true;
    }

    return false;
}

/* Fetches the JSON encoded DHT nodeslist from NODES_LIST_URL.
 *
 * Return 0 on success.
 * Return -1 on failure.
 */
static int curl_fetch_nodes_JSON(struct Recv_Curl_Data *recv_data)
{
    CURL *c_handle = curl_easy_init();

    if (c_handle == NULL) {
        return -1;
    }

    int err = -1;

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "charsets: utf-8");

    int ret = curl_easy_setopt(c_handle, CURLOPT_HTTPHEADER, headers);

    if (ret != CURLE_OK) {
        fprintf(stderr, "Failed to set http headers (libcurl error %d)", ret);
        goto on_exit;
    }

    ret = curl_easy_setopt(c_handle, CURLOPT_URL, NODES_LIST_URL);

    if (ret != CURLE_OK) {
        fprintf(stderr, "Failed to set url (libcurl error %d)", ret);
        goto on_exit;
    }

    ret = curl_easy_setopt(c_handle, CURLOPT_WRITEFUNCTION, curl_cb_write_data);

    if (ret != CURLE_OK) {
        fprintf(stderr, "Failed to set write function callback (libcurl error %d)", ret);
        goto on_exit;
    }

    ret = curl_easy_setopt(c_handle, CURLOPT_WRITEDATA, recv_data);

    if (ret != CURLE_OK) {
        fprintf(stderr, "Failed to set write data (libcurl error %d)", ret);
        goto on_exit;
    }

    ret = curl_easy_setopt(c_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    if (ret != CURLE_OK) {
        fprintf(stderr, "Failed to set useragent (libcurl error %d)", ret);
        goto on_exit;
    }

    ret = curl_easy_setopt(c_handle, CURLOPT_HTTPGET, 1L);

    if (ret != CURLE_OK) {
        fprintf(stderr, "Failed to set get request (libcurl error %d)", ret);
        goto on_exit;
    }

    int proxy_ret = set_curl_proxy(c_handle, arg_opts.proxy_address, arg_opts.proxy_port, arg_opts.proxy_type);

    if (proxy_ret != 0) {
        fprintf(stderr, "set_curl_proxy() failed with error %d\n", proxy_ret);
        goto on_exit;
    }

    ret = curl_easy_setopt(c_handle, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);

    if (ret != CURLE_OK) {
        fprintf(stderr, "TLSv1.2 could not be set (libcurl error %d)", ret);
        goto on_exit;
    }

    ret = curl_easy_setopt(c_handle, CURLOPT_SSL_CIPHER_LIST, TLS_CIPHER_SUITE_LIST);

    if (ret != CURLE_OK) {
        fprintf(stderr, "Failed to set TLS cipher list (libcurl error %d)", ret);
        goto on_exit;
    }

    ret = curl_easy_perform(c_handle);

    if (ret != CURLE_OK) {
        /* If system doesn't support any of the specified ciphers suites, fall back to default */
        if (ret == CURLE_SSL_CIPHER) {
            ret = curl_easy_setopt(c_handle, CURLOPT_SSL_CIPHER_LIST, NULL);

            if (ret != CURLE_OK) {
                fprintf(stderr, "Failed to set SSL cipher list (libcurl error %d)\n", ret);
                goto on_exit;
            }

            ret = curl_easy_perform(c_handle);
        }

        if (ret != CURLE_OK) {
            fprintf(stderr, "HTTPS lookup error (libcurl error %d)\n", ret);
            goto on_exit;
        }
    }

    err = 0;

on_exit:
    curl_slist_free_all(headers);
    curl_easy_cleanup(c_handle);
    return err;
}

/* Attempts to update the DHT nodeslist.
 *
 * Return 1 if list was updated successfully.
 * Return 0 if list does not need to be updated.
 * Return -1 if file cannot be opened.
 * Return -2 if http lookup failed.
 * Return -3 if http reponse was empty.
 * Return -4 if data could not be written to disk.
 * Return -5 if memory allocation fails.
 */
static int update_DHT_nodeslist(const char *nodes_path, int update_frequency)
{
    if (!nodeslist_needs_update(nodes_path, update_frequency)) {
        return 0;
    }

    FILE *fp = fopen(nodes_path, "r+");

    if (fp == NULL) {
        return -1;
    }

    struct Recv_Curl_Data *recv_data = calloc(1, sizeof(struct Recv_Curl_Data));

    if (recv_data == NULL) {
        fclose(fp);
        return -5;
    }

    if (curl_fetch_nodes_JSON(recv_data) == -1) {
        free(recv_data);
        fclose(fp);
        return -2;
    }

    if (recv_data->length == 0) {
        free(recv_data);
        fclose(fp);
        return -3;
    }

    if (fwrite(recv_data->data, recv_data->length, 1, fp) != 1) {
        free(recv_data);
        fclose(fp);
        return -4;
    }

    free(recv_data);
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

/* Return true if json encoded string s contains a valid IP address and puts address in ip_buf.
 *
 * ip_type should be set to 1 for ipv4 address, or 0 for ipv6 addresses.
 * ip_buf must have room for at least IP_MAX_SIZE + 1 bytes.
 */
static bool extract_val_ip(const char *s, char *ip_buf, unsigned short int ip_type)
{
    int ip_len = char_find(0, s, '"');

    if (ip_len < IP_MIN_SIZE || ip_len > IP_MAX_SIZE) {
        return false;
    }

    memcpy(ip_buf, s, ip_len);
    ip_buf[ip_len] = 0;

    return (ip_type == 1) ? is_ip4_address(ip_buf) : is_ip6_address(ip_buf);
}

/* Extracts the port from json encoded string s.
 *
 * Return port number on success.
 * Return 0 on failure.
 */
static uint16_t extract_val_port(const char *s)
{
    long int port = strtol(s, NULL, 10);
    return (port > 0 && port <= MAX_PORT_RANGE) ? port : 0;
}

/* Extracts the last pinged value from json encoded string s.
 *
 * Return timestamp on success.
 * Return -1 on failure.
 */
static long long int extract_val_last_pinged(const char *s)
{
    long long int last_pinged = strtoll(s, NULL, 10);
    return (last_pinged <= 0) ? -1 : last_pinged;
}

/* Extracts DHT public key from json encoded string s and puts key in key_buf.
 * key_buf must have room for at least TOX_PUBLIC_KEY_SIZE * 2 + 1 bytes.
 *
 * Return number of bytes copied to key_buf on success.
 * Return -1 on failure.
 */
static int extract_val_pk(const char *s, char *key_buf, size_t buf_length)
{
    if (buf_length < TOX_PUBLIC_KEY_SIZE * 2 + 1) {
        return -1;
    }

    int key_len = char_find(0, s, '"');

    if (key_len != TOX_PUBLIC_KEY_SIZE * 2) {
        return -1;
    }

    memcpy(key_buf, s, key_len);
    key_buf[key_len] = 0;

    return key_len;
}

/* Extracts values from json formatted string, validats them, and puts them in node.
 *
 * Return 0 on success.
 * Return -1 if line is empty.
 * Return -2 if line does not appear to be a valid nodes list entry.
 * Return -3 if node appears to be offline.
 * Return -4 if entry does not contain either a valid ipv4 or ipv6 address.
 * Return -5 if port value is invalid.
 * Return -6 if public key is invalid.
 */
static int extract_node(const char *line, struct Node *node)
{
    if (!line) {
        return -1;
    }

    const char *ip4_start = strstr(line, IPV4_JSON_KEY);
    const char *ip6_start = strstr(line, IPV6_JSON_KEY);
    const char *port_start = strstr(line, PORT_JSON_KEY);
    const char *key_start = strstr(line, PK_JSON_KEY);
    const char *last_pinged_str = strstr(line, LAST_PING_JSON_KEY);

    if (!ip4_start || !ip6_start || !port_start || !key_start || !last_pinged_str) {
        return -2;
    }

    long long int last_pinged = extract_val_last_pinged(last_pinged_str + LAST_PING_JSON_KEY_LEN);

    if (last_pinged <= 0 || node_is_offline(last_pinged)) {
        return -3;
    }

    char ip4_string[IP_MAX_SIZE + 1];
    bool have_ip4 = extract_val_ip(ip4_start + IPV4_JSON_KEY_LEN, ip4_string, 1);

    char ip6_string[IP_MAX_SIZE + 1];
    bool have_ip6 = extract_val_ip(ip6_start + IPV6_JSON_KEY_LEN, ip6_string, 0);

    if (!have_ip6 && !have_ip4) {
        return -4;
    }

    uint16_t port = extract_val_port(port_start + PORT_JSON_KEY_LEN);

    if (port == 0) {
        return -5;
    }

    char key_string[TOX_PUBLIC_KEY_SIZE * 2 + 1];
    int key_len = extract_val_pk(key_start + PK_JSON_KEY_LEN, key_string, sizeof(key_string));

    if (key_len == -1) {
        return -6;
    }

    if (tox_pk_string_to_bytes(key_string, key_len, node->key, sizeof(node->key)) == -1) {
        return -6;
    }

    if (have_ip4) {
        snprintf(node->ip4, sizeof(node->ip4), "%s", ip4_string);
        node->have_ip4 = true;
    }

    if (have_ip6) {
        snprintf(node->ip6, sizeof(node->ip6), "%s", ip6_string);
        node->have_ip6 = true;
    }

    node->port = port;

    return 0;
}

/* Loads the DHT nodeslist to memory from json encoded nodes file. */
void *load_nodeslist_thread(void *data)
{
    const Client_Config *c_config = (Client_Config *) data;

    if (c_config == NULL) {
        goto on_exit;
    }

    char nodes_path[PATH_MAX];
    get_nodeslist_path(nodes_path, sizeof(nodes_path));

    FILE *fp = NULL;

    if (!file_exists(nodes_path)) {
        if ((fp = fopen(nodes_path, "w+")) == NULL) {
            fprintf(stderr, "nodeslist load error: failed to create file '%s'\n", nodes_path);
            goto on_exit;
        }
    } else if ((fp = fopen(nodes_path, "r+")) == NULL) {
        fprintf(stderr, "nodeslist load error: failed to open file '%s'\n", nodes_path);
        goto on_exit;
    }

    const int update_err = update_DHT_nodeslist(nodes_path, c_config->nodeslist_update_freq);

    if (update_err < 0) {
        fprintf(stderr, "update_DHT_nodeslist() failed with error %d\n", update_err);
    }

    char line[MAX_NODELIST_SIZE + 1];

    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        fprintf(stderr, "nodeslist load error: file empty.\n");
        goto on_exit;
    }

    size_t idx = 0;
    const char *line_start = line;

    while ((line_start = strstr(line_start + 1, IPV4_JSON_KEY))) {
        pthread_mutex_lock(&thread_data.lock);
        idx = Nodes.count;

        if (idx >= MAX_NODES) {
            pthread_mutex_unlock(&thread_data.lock);
            break;
        }

        if (extract_node(line_start, &Nodes.list[idx]) == 0) {
            ++Nodes.count;
        }

        pthread_mutex_unlock(&thread_data.lock);
    }

    pthread_mutex_lock(&thread_data.lock);
    const size_t num_nodes = Nodes.count;
    pthread_mutex_unlock(&thread_data.lock);

    /* If nodeslist does not contain any valid entries we set the last_scan value
     * to 0 so that it will fetch a new list the next time this function is called.
     */
    if (num_nodes == 0) {
        const char *s = "{\"last_scan\":0}";
        rewind(fp);
        fwrite(s, strlen(s), 1, fp);  // Not much we can do if it fails
        fclose(fp);
        fprintf(stderr, "nodeslist load error: List did not contain any valid entries.\n");
        goto on_exit;
    }

    fclose(fp);

on_exit:
    thread_data.active = false;
    pthread_attr_destroy(&thread_data.attr);
    pthread_exit(0);
}

/* Creates a new thread that will load the DHT nodeslist to memory
 * from json encoded nodes file obtained at NODES_LIST_URL. Only one
 * thread may run at a time.
 *
 * Return 0 on success.
 * Return -1 if a thread is already active.
 * Return -2 if mutex fails to init.
 * Return -3 if pthread attribute fails to init.
 * Return -4 if pthread fails to set detached state.
 * Return -5 if thread creation fails.
 */
int load_DHT_nodeslist(const Client_Config *c_config)
{
    if (thread_data.active) {
        return -1;
    }

    if (pthread_mutex_init(&thread_data.lock, NULL) != 0) {
        return -2;
    }

    if (pthread_attr_init(&thread_data.attr) != 0) {
        return -3;
    }

    if (pthread_attr_setdetachstate(&thread_data.attr, PTHREAD_CREATE_DETACHED) != 0) {
        return -4;
    }

    thread_data.active = true;

    if (pthread_create(&thread_data.tid, &thread_data.attr, load_nodeslist_thread, (void *) c_config) != 0) {
        thread_data.active = false;
        return -5;
    }

    return 0;
}

/* Connects to NUM_BOOTSTRAP_NODES random DHT nodes listed in the DHTnodes file. */
static void DHT_bootstrap(Tox *tox)
{
    pthread_mutex_lock(&thread_data.lock);
    size_t num_nodes = Nodes.count;
    pthread_mutex_unlock(&thread_data.lock);

    if (num_nodes == 0) {
        return;
    }

    pthread_mutex_lock(&thread_data.lock);

    for (size_t i = 0; i < NUM_BOOTSTRAP_NODES; ++i) {
        const unsigned int idx = rand_range_not_secure(Nodes.count);
        struct Node *node = &Nodes.list[idx];

        const char *addr = node->have_ip4 ? node->ip4 : node->ip6;

        if (!addr) {
            continue;
        }

        Tox_Err_Bootstrap err;
        tox_bootstrap(tox, addr, node->port, (uint8_t *) node->key, &err);

        if (err != TOX_ERR_BOOTSTRAP_OK) {
            fprintf(stderr, "Failed to bootstrap %s:%d\n", addr, node->port);
        }

        tox_add_tcp_relay(tox, addr, node->port, (uint8_t *) node->key, &err);

        if (err != TOX_ERR_BOOTSTRAP_OK) {
            fprintf(stderr, "Failed to add TCP relay %s:%d\n", addr, node->port);
        }
    }

    pthread_mutex_unlock(&thread_data.lock);
}

/* Manages connection to the Tox DHT network. */
void do_tox_connection(Tox *tox)
{
    static time_t last_bootstrap_time = 0;  // TODO: Put this in Toxic
    const bool connected = prompt_selfConnectionStatus() != TOX_CONNECTION_NONE;

    if (!connected && timed_out(last_bootstrap_time, TRY_BOOTSTRAP_INTERVAL)) {
        DHT_bootstrap(tox);
        last_bootstrap_time = get_unix_time();
    }
}

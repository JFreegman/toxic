/*  name_lookup.c
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
#include <string.h>
#include <sys/types.h> /* for u_char */
#include <curl/curl.h>

#include "toxic.h"
#include "windows.h"
#include "line_info.h"
#include "global_commands.h"
#include "misc_tools.h"
#include "configdir.h"

extern struct arg_opts arg_opts;
extern struct Winthread Winthread;;

#define NAMESERVER_API_PATH "api"
#define SERVER_KEY_SIZE 32
#define MAX_SERVERS 50
#define MAX_DOMAIN_SIZE 32
#define MAX_SERVER_LINE MAX_DOMAIN_SIZE + (SERVER_KEY_SIZE * 2) + 3

/* List based on Mozilla's recommended configurations for modern browsers */
#define TLS_CIPHER_SUITE_LIST "ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-AES256-GCM-SHA384:DHE-RSA-AES128-GCM-SHA256:DHE-DSS-AES128-GCM-SHA256:kEDH+AESGCM:ECDHE-RSA-AES128-SHA256:ECDHE-ECDSA-AES128-SHA256:ECDHE-RSA-AES128-SHA:ECDHE-ECDSA-AES128-SHA:ECDHE-RSA-AES256-SHA384:ECDHE-ECDSA-AES256-SHA384:ECDHE-RSA-AES256-SHA:ECDHE-ECDSA-AES256-SHA:DHE-RSA-AES128-SHA256:DHE-RSA-AES128-SHA:DHE-DSS-AES128-SHA256:DHE-RSA-AES256-SHA256:DHE-DSS-AES256-SHA:DHE-RSA-AES256-SHA:!aNULL:!eNULL:!EXPORT:!DES:!RC4:!3DES:!MD5:!PSK"

struct Nameservers {
    int     lines;
    char    names[MAX_SERVERS][MAX_DOMAIN_SIZE];
    char    keys[MAX_SERVERS][SERVER_KEY_SIZE];
} Nameservers;

static struct thread_data {
    Tox       *m;
    ToxWindow *self;
    char    id_bin[TOX_ADDRESS_SIZE];
    char    addr[MAX_STR_SIZE];
    char    msg[MAX_STR_SIZE];
    bool    busy;
    bool    disabled;
} t_data;

static struct lookup_thread {
    pthread_t tid;
    pthread_attr_t attr;
} lookup_thread;

static int lookup_error(ToxWindow *self, const char *errmsg, ...)
{
    char frmt_msg[MAX_STR_SIZE];

    va_list args;
    va_start(args, errmsg);
    vsnprintf(frmt_msg, sizeof(frmt_msg), errmsg, args);
    va_end(args);

    pthread_mutex_lock(&Winthread.lock);
    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "name lookup failed: %s", frmt_msg);
    pthread_mutex_unlock(&Winthread.lock);

    return -1;
}

static void kill_lookup_thread(void)
{
    memset(&t_data, 0, sizeof(struct thread_data));
    pthread_attr_destroy(&lookup_thread.attr);
    pthread_exit(NULL);
}

/* Attempts to load the nameserver list pointed at by path into the Nameservers structure.
 *
 * Returns 0 on success.
 * -1 is reserved.
 * Returns -2 if the supplied path does not exist.
 * Returns -3 if the list does not contain any valid entries.
 */
static int load_nameserver_list(const char *path)
{
    FILE *fp = fopen(path, "r");

    if (fp == NULL)
        return -2;

    char line[MAX_SERVER_LINE];

    while (fgets(line, sizeof(line), fp) && Nameservers.lines < MAX_SERVERS) {
        int linelen = strlen(line);

        if (linelen < SERVER_KEY_SIZE * 2 + 5)
            continue;

        if (line[linelen - 1] == '\n')
            line[--linelen] = '\0';

        const char *name = strtok(line, " ");
        const char *keystr = strtok(NULL, " ");

        if (name == NULL || keystr == NULL)
            continue;

        if (strlen(keystr) != SERVER_KEY_SIZE * 2)
            continue;

        snprintf(Nameservers.names[Nameservers.lines], sizeof(Nameservers.names[Nameservers.lines]), "%s", name);
        int res = hex_string_to_bytes(Nameservers.keys[Nameservers.lines], SERVER_KEY_SIZE, keystr);

        if (res == -1)
            continue;

        ++Nameservers.lines;
    }

    fclose(fp);

    if (Nameservers.lines < 1)
        return -3;

    return 0;
}

/* Takes address addr in the form "username@domain", puts the username in namebuf,
 * and the domain in dombuf.
 *
 * Returns 0 on success.
 * Returns -1 on failure
 */
static int parse_addr(const char *addr, char *namebuf, size_t namebuf_sz, char *dombuf, size_t dombuf_sz)
{
    if (strlen(addr) >= (MAX_STR_SIZE - strlen(NAMESERVER_API_PATH)))
        return -1;

    char tmpaddr[MAX_STR_SIZE];
    char *tmpname = NULL;
    char *tmpdom = NULL;

    snprintf(tmpaddr, sizeof(tmpaddr), "%s", addr);
    tmpname = strtok(tmpaddr, "@");
    tmpdom = strtok(NULL, "");

    if (tmpname == NULL || tmpdom == NULL)
        return -1;

    str_to_lower(tmpdom);
    snprintf(namebuf, namebuf_sz, "%s", tmpname);
    snprintf(dombuf, dombuf_sz, "%s", tmpdom);

    return 0;
}

/* matches input domain name with domains in list and obtains key.
 * Turns out_domain into the full domain we need to make a POST request.
 *
 * Return true on match.
 * Returns false on no match.
 */
static bool get_domain_match(char *pubkey, char *out_domain, size_t out_domain_size, const char *inputdomain)
{
    int i;

    for (i = 0; i < Nameservers.lines; ++i) {
        if (strcmp(Nameservers.names[i], inputdomain) == 0) {
            memcpy(pubkey, Nameservers.keys[i], SERVER_KEY_SIZE);
            snprintf(out_domain, out_domain_size, "https://%s/%s", Nameservers.names[i], NAMESERVER_API_PATH);
            return true;
        }
    }

    return false;
}

#define MAX_RECV_LOOKUP_DATA_SIZE 1024

/* Holds raw data received from name server */
struct Recv_Data {
    char data[MAX_RECV_LOOKUP_DATA_SIZE];
    size_t size;
};

size_t write_lookup_data(void *data, size_t size, size_t nmemb, void *user_pointer)
{
    struct Recv_Data *recv_data = (struct Recv_Data *) user_pointer;
    size_t real_size = size * nmemb;

    if (real_size > MAX_RECV_LOOKUP_DATA_SIZE)
        return 0;

    memcpy(&recv_data->data, data, real_size);
    recv_data->size = real_size;
    recv_data->data[real_size] = 0;

    return real_size;
}

/* Converts Tox ID string contained in recv_data to binary format and puts it in thread's ID buffer.
 *
 * Returns 0 on success.
 * Returns -1 on failure.
 */
#define ID_PREFIX "\"tox_id\": \""
static int process_response(struct Recv_Data *recv_data)
{
    size_t prefix_size = strlen(ID_PREFIX);

    if (recv_data->size < TOX_ADDRESS_SIZE * 2 + prefix_size)
        return -1;

    const char *IDstart = strstr(recv_data->data, ID_PREFIX);

    if (IDstart == NULL)
        return -1;

    if (strlen(IDstart) < TOX_ADDRESS_SIZE * 2 + prefix_size)
        return -1;

    char ID_string[TOX_ADDRESS_SIZE * 2 + 1];
    memcpy(ID_string, IDstart + prefix_size, TOX_ADDRESS_SIZE * 2);
    ID_string[TOX_ADDRESS_SIZE * 2] = 0;

    if (hex_string_to_bin(ID_string, strlen(ID_string), t_data.id_bin, sizeof(t_data.id_bin)) == -1)
        return -1;

    return 0;
}

/* Sets proxy info for given CURL handler.
 *
 * Returns 0 on success or if no proxy is set by the client.
 * Returns -1 on failure.
 */
static int set_lookup_proxy(ToxWindow *self, CURL *c_handle, const char *proxy_address, uint16_t port, uint8_t proxy_type)
{
    if (proxy_type == TOX_PROXY_TYPE_NONE)
        return 0;

    if (proxy_address == NULL || port == 0) {
        lookup_error(self, "Unknown proxy error");
        return -1;
    }

    int ret = curl_easy_setopt(c_handle, CURLOPT_PROXYPORT, (long) port);

    if (ret != CURLE_OK) {
        lookup_error(self, "Failed to set proxy port (libcurl error %d)", ret);
        return -1;
    }

    long int type = proxy_type == TOX_PROXY_TYPE_SOCKS5 ? CURLPROXY_SOCKS5_HOSTNAME : CURLPROXY_HTTP;

    ret = curl_easy_setopt(c_handle, CURLOPT_PROXYTYPE, type);

    if (ret != CURLE_OK) {
        lookup_error(self, "Failed to set proxy type (libcurl error %d)", ret);
        return -1;
    }

    ret = curl_easy_setopt(c_handle, CURLOPT_PROXY, proxy_address);

    if (ret != CURLE_OK) {
        lookup_error(self, "Failed to set proxy (libcurl error %d)", ret);
        return -1;
    }

    return 0;
}

void *lookup_thread_func(void *data)
{
    ToxWindow *self = t_data.self;

    char input_domain[MAX_STR_SIZE];
    char name[MAX_STR_SIZE];

    if (parse_addr(t_data.addr, name, sizeof(name), input_domain, sizeof(input_domain)) == -1) {
        lookup_error(self, "Input must be a 76 character Tox ID or an address in the form: username@domain");
        kill_lookup_thread();
    }

    char nameserver_key[SERVER_KEY_SIZE];
    char real_domain[MAX_DOMAIN_SIZE];

    if (!get_domain_match(nameserver_key, real_domain, sizeof(real_domain), input_domain)) {
        if (!strcasecmp(input_domain, "utox.org"))
            lookup_error(self, "utox.org uses deprecated DNS-based lookups and is no longer supported by Toxic");
        else
            lookup_error(self, "Name server domain not found.");

        kill_lookup_thread();
    }

    CURL *c_handle = curl_easy_init();

    if (!c_handle) {
        lookup_error(self, "curl handler error");
        kill_lookup_thread();
    }

    struct Recv_Data recv_data;
    memset(&recv_data, 0, sizeof(struct Recv_Data));

    char post_data[MAX_STR_SIZE];
    snprintf(post_data, sizeof(post_data), "{\"action\": 3, \"name\": \"%s\"}", name);

    struct curl_slist *headers = NULL;

    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "charsets: utf-8");

    curl_easy_setopt(c_handle, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(c_handle, CURLOPT_URL, real_domain);
    curl_easy_setopt(c_handle, CURLOPT_WRITEFUNCTION, write_lookup_data);
    curl_easy_setopt(c_handle, CURLOPT_WRITEDATA, (void *) &recv_data);
    curl_easy_setopt(c_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    curl_easy_setopt(c_handle, CURLOPT_POSTFIELDS, post_data);

    if (set_lookup_proxy(self, c_handle, arg_opts.proxy_address, arg_opts.proxy_port, arg_opts.proxy_type) == -1)
        goto on_exit;

    int ret = curl_easy_setopt(c_handle, CURLOPT_USE_SSL, CURLUSESSL_ALL);

    if (ret != CURLE_OK) {
        lookup_error(self, "TLS could not be enabled (libcurl error %d)", ret);
        goto on_exit;
    }

    ret = curl_easy_setopt(c_handle, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);

    if (ret != CURLE_OK) {
        lookup_error(self, "TLSv1.2 could not be set (libcurl error %d)", ret);
        goto on_exit;
    }

    ret = curl_easy_setopt(c_handle, CURLOPT_SSL_CIPHER_LIST, TLS_CIPHER_SUITE_LIST);

    if (ret != CURLE_OK) {
        lookup_error(self, "Failed to set TLS cipher list (libcurl error %d)", ret);
        goto on_exit;
    }

    ret = curl_easy_perform(c_handle);

    if (ret != CURLE_OK) {
        /* If system doesn't support any of the specified ciphers suites, fall back to default */
        if (ret == CURLE_SSL_CIPHER) {
            curl_easy_setopt(c_handle, CURLOPT_SSL_CIPHER_LIST, NULL);
            ret = curl_easy_perform(c_handle);
        }

        if (ret != CURLE_OK) {
            lookup_error(self, "HTTPS lookup error (libcurl error %d)", ret);
            goto on_exit;
        }
    }

    if (process_response(&recv_data) == -1) {
        lookup_error(self, "Bad response.");
        goto on_exit;
    }

    pthread_mutex_lock(&Winthread.lock);
    cmd_add_helper(self, t_data.m, t_data.id_bin, t_data.msg);
    pthread_mutex_unlock(&Winthread.lock);

on_exit:
    curl_slist_free_all(headers);
    curl_easy_cleanup(c_handle);
    kill_lookup_thread();

    return 0;
}

void name_lookup(ToxWindow *self, Tox *m, const char *id_bin, const char *addr, const char *message)
{
    if (t_data.disabled) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "name lookups are disabled.");
        return;
    }

    if (t_data.busy) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, "Please wait for previous name lookup to finish.");
        return;
    }

    snprintf(t_data.id_bin, sizeof(t_data.id_bin), "%s", id_bin);
    snprintf(t_data.addr, sizeof(t_data.addr), "%s", addr);
    snprintf(t_data.msg, sizeof(t_data.msg), "%s", message);
    t_data.self = self;
    t_data.m = m;
    t_data.busy = true;

    if (pthread_attr_init(&lookup_thread.attr) != 0) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, RED, "Error: lookup thread attr failed to init");
        memset(&t_data, 0, sizeof(struct thread_data));
        return;
    }

    if (pthread_attr_setdetachstate(&lookup_thread.attr, PTHREAD_CREATE_DETACHED) != 0) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, RED, "Error: lookup thread attr failed to set");
        pthread_attr_destroy(&lookup_thread.attr);
        memset(&t_data, 0, sizeof(struct thread_data));
        return;
    }

    if (pthread_create(&lookup_thread.tid, &lookup_thread.attr, lookup_thread_func, NULL) != 0) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, RED, "Error: lookup thread failed to init");
        pthread_attr_destroy(&lookup_thread.attr);
        memset(&t_data, 0, sizeof(struct thread_data));
        return;
    }
}

/* Initializes http based name lookups. Note: This function must be called only once before additional
 * threads are spawned.
 *
 * Returns 0 on success.
 * Returns -1 if curl failed to init.
 * Returns -2 if the nameserver list cannot be found.
 * Returns -3 if the nameserver list does not contain any valid entries.
 */
int name_lookup_init(void)
{
    if (curl_global_init(CURL_GLOBAL_ALL) != 0) {
        t_data.disabled = true;
        return -1;
    }

    const char *path = arg_opts.nameserver_path[0] ? arg_opts.nameserver_path : PACKAGE_DATADIR "/nameservers";
    int ret = load_nameserver_list(path);

    if (ret != 0) {
        t_data.disabled = true;
        return ret;
    }

    return 0;
}

void name_lookup_cleanup(void)
{
    curl_global_cleanup();
}

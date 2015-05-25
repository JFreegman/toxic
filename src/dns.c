/*  dns.c
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
#include <sys/types.h> /* for u_char */
#include <netinet/in.h>
#include <resolv.h>

#ifdef NO_GETTEXT
#define gettext(A) (A)
#else
#include <libintl.h>
#endif

#ifdef __APPLE__
    #include <arpa/nameser_compat.h>
#else
    #include <arpa/nameser.h>
#endif  /* ifdef __APPLE__ */

#include <tox/toxdns.h>

#include "toxic.h"
#include "windows.h"
#include "line_info.h"
#include "dns.h"
#include "global_commands.h"
#include "misc_tools.h"
#include "configdir.h"

#define DNS3_KEY_SIZE 32
#define MAX_DNS_REQST_SIZE 255
#define TOX_DNS3_TXT_PREFIX "v=tox3;id="

extern struct Winthread Winthread;
extern struct dns3_servers dns3_servers;
extern struct arg_opts arg_opts;

#define NUM_DNS3_BACKUP_SERVERS 2

/* Hardcoded backup in case domain list is not loaded */
static struct dns3_server_backup {
    const char *name;
    char key[DNS3_KEY_SIZE];
} dns3_servers_backup[] = {
    {
        "utox.org",
        {
          0xD3, 0x15, 0x4F, 0x65, 0xD2, 0x8A, 0x5B, 0x41, 0xA0, 0x5D, 0x4A, 0xC7, 0xE4, 0xB3, 0x9C, 0x6B,
          0x1C, 0x23, 0x3C, 0xC8, 0x57, 0xFB, 0x36, 0x5C, 0x56, 0xE8, 0x39, 0x27, 0x37, 0x46, 0x2A, 0x12
        }
    },
    {
        "toxme.se",
        {
          0x5D, 0x72, 0xC5, 0x17, 0xDF, 0x6A, 0xEC, 0x54, 0xF1, 0xE9, 0x77, 0xA6, 0xB6, 0xF2, 0x59, 0x14,
          0xEA, 0x4C, 0xF7, 0x27, 0x7A, 0x85, 0x02, 0x7C, 0xD9, 0xF5, 0x19, 0x6D, 0xF1, 0x7E, 0x0B, 0x13
        }
    },
};

static struct thread_data {
    ToxWindow *self;
    char id_bin[TOX_ADDRESS_SIZE];
    char addr[MAX_STR_SIZE];
    char msg[MAX_STR_SIZE];
    uint8_t busy;
    Tox *m;
} t_data;

static struct dns_thread {
    pthread_t tid;
    pthread_attr_t attr;
} dns_thread;


#define MAX_DNS_SERVERS 50
#define MAX_DOMAIN_SIZE 32
#define MAX_DNS_LINE MAX_DOMAIN_SIZE + (DNS3_KEY_SIZE * 2) + 3

struct dns3_servers {
    bool loaded;
    int lines;
    char names[MAX_DNS_SERVERS][MAX_DOMAIN_SIZE];
    char keys[MAX_DNS_SERVERS][DNS3_KEY_SIZE];
} dns3_servers;

static int load_dns_domainlist(const char *path)
{
    FILE *fp = fopen(path, "r");

    if (fp == NULL)
        return -1;

    char line[MAX_DNS_LINE];

    while (fgets(line, sizeof(line), fp) && dns3_servers.lines < MAX_DNS_SERVERS) {
        int linelen = strlen(line);

        if (linelen < DNS3_KEY_SIZE * 2 + 5)
            continue;

        if (line[linelen - 1] == '\n')
            line[--linelen] = '\0';

        const char *name = strtok(line, " ");
        const char *keystr = strtok(NULL, " ");

        if (name == NULL || keystr == NULL)
            continue;

        if (strlen(keystr) != DNS3_KEY_SIZE * 2)
            continue;

        snprintf(dns3_servers.names[dns3_servers.lines], sizeof(dns3_servers.names[dns3_servers.lines]), "%s", name);
        int res = hex_string_to_bytes(dns3_servers.keys[dns3_servers.lines], DNS3_KEY_SIZE, keystr);

        if (res == -1)
            continue;

        ++dns3_servers.lines;
    }

    fclose(fp);

    if (dns3_servers.lines < 1)
        return -2;

    return 0;
}

static int dns_error(ToxWindow *self, const char *errmsg)
{
    pthread_mutex_lock(&Winthread.lock);
    line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, gettext("User lookup failed: %s"), errmsg);
    pthread_mutex_unlock(&Winthread.lock);

    return -1;
}

static void killdns_thread(void *dns_obj)
{
    if (dns_obj)
        tox_dns3_kill(dns_obj);

    memset(&t_data, 0, sizeof(struct thread_data));
    pthread_attr_destroy(&dns_thread.attr);
    pthread_exit(NULL);
}

/* puts TXT from dns response in buf. Returns length of TXT on success, -1 on fail.*/
static int parse_dns_response(ToxWindow *self, u_char *answer, int ans_len, char *buf)
{
    uint8_t *ans_pt = answer + sizeof(HEADER);
    uint8_t *ans_end = answer + ans_len;
    char exp_ans[PACKETSZ];

    int len = dn_expand(answer, ans_end, ans_pt, exp_ans, sizeof(exp_ans));

    if (len == -1)
        return dns_error(self, gettext("dn_expand failed."));

    ans_pt += len;

    if (ans_pt > ans_end - 4)
         return dns_error(self, gettext("DNS reply was too short."));

    int type;
    GETSHORT(type, ans_pt);

    if (type != T_TXT)
        return dns_error(self, gettext("Broken DNS reply."));


    ans_pt += INT16SZ;    /* class */
    uint32_t size = 0;

    /* recurse through CNAME rr's */
    do {
        ans_pt += size;
        len = dn_expand(answer, ans_end, ans_pt, exp_ans, sizeof(exp_ans));

        if (len == -1)
            return dns_error(self, gettext("Second dn_expand failed."));

        ans_pt += len;

        if (ans_pt > ans_end - 10)
            return dns_error(self, gettext("DNS reply was too short."));

        GETSHORT(type, ans_pt);
        ans_pt += INT16SZ;
        ans_pt += 4;
        GETSHORT(size, ans_pt);

        if (ans_pt + size < answer || ans_pt + size > ans_end)
            return dns_error(self, gettext("RR overflow."));

    } while (type == T_CNAME);

    if (type != T_TXT)
        return dns_error(self, gettext("DNS response failed."));

    uint32_t txt_len = *ans_pt;

    if (!size || txt_len >= size || !txt_len)
        return dns_error(self, gettext("No record found."));

    if (txt_len > MAX_DNS_REQST_SIZE)
        return dns_error(self, gettext("Invalid DNS response."));

    ans_pt++;
    ans_pt[txt_len] = '\0';
    memcpy(buf, ans_pt, txt_len + 1);

    return txt_len;
}

/* Takes address addr in the form "username@domain", puts the username in namebuf,
   and the domain in dombuf.

   return length of username on success, -1 on failure */
static int parse_addr(const char *addr, char *namebuf, char *dombuf)
{
    char tmpaddr[MAX_STR_SIZE];
    char *tmpname, *tmpdom;

    strcpy(tmpaddr, addr);
    tmpname = strtok(tmpaddr, "@");
    tmpdom = strtok(NULL, "");

    if (tmpname == NULL || tmpdom == NULL)
        return -1;

    str_to_lower(tmpdom);
    strcpy(namebuf, tmpname);
    strcpy(dombuf, tmpdom);

    return strlen(namebuf);
}

/* matches input domain name with domains in list and obtains key. Return 0 on success, -1 on failure */
static int get_domain_match(char *pubkey, char *domain, const char *inputdomain)
{
    /* check server list first */
    int i;
    bool match = false;

    for (i = 0; i < dns3_servers.lines; ++i) {
        if (strcmp(dns3_servers.names[i], inputdomain) == 0) {
            memcpy(pubkey, dns3_servers.keys[i], DNS3_KEY_SIZE);
            snprintf(domain, MAX_DOMAIN_SIZE, "%s", dns3_servers.names[i]);
            match = true;
            break;
        }
    }

    /* fall back to hard-coded domains on server list failure */
    if (!match) {
        for (i = 0; i < NUM_DNS3_BACKUP_SERVERS; ++i) {
            if (strcmp(dns3_servers_backup[i].name, inputdomain) == 0) {
                memcpy(pubkey, dns3_servers_backup[i].key, DNS3_KEY_SIZE);
                snprintf(domain, MAX_DOMAIN_SIZE, "%s", dns3_servers_backup[i].name);
                match = true;
                break;
            }
        }

        if (!match)
            return -1;
    }

    return 0;
}

/* Does DNS lookup for addr and puts resulting tox id in id_bin. */
void *dns3_lookup_thread(void *data)
{
    ToxWindow *self = t_data.self;

    char inputdomain[MAX_STR_SIZE];
    char name[MAX_STR_SIZE];

    int namelen = parse_addr(t_data.addr, name, inputdomain);

    if (namelen == -1) {
        dns_error(self, gettext("Must be a Tox ID or an address in the form username@domain"));
        killdns_thread(NULL);
    }

    char DNS_pubkey[DNS3_KEY_SIZE];
    char domain[MAX_DOMAIN_SIZE];

    int match = get_domain_match(DNS_pubkey, domain, inputdomain);

    if (match == -1) {
        dns_error(self, gettext("Domain not found."));
        killdns_thread(NULL);
    }

    void *dns_obj = tox_dns3_new((uint8_t *) DNS_pubkey);

    if (dns_obj == NULL) {
        dns_error(self, gettext("Core failed to create DNS object."));
        killdns_thread(NULL);
    }

    char string[MAX_DNS_REQST_SIZE + 1];
    uint32_t request_id;

    int str_len = tox_generate_dns3_string(dns_obj, (uint8_t *) string, sizeof(string), &request_id,
                                           (uint8_t *) name, namelen);

    if (str_len == -1) {
        dns_error(self, gettext("Core failed to generate DNS3 string."));
        killdns_thread(dns_obj);
    }

    string[str_len] = '\0';

    u_char answer[PACKETSZ];
    char d_string[MAX_DOMAIN_SIZE + MAX_DNS_REQST_SIZE + 10];

    /* format string and create dns query */
    snprintf(d_string, sizeof(d_string), "_%s._tox.%s", string, domain);
    int ans_len = res_query(d_string, C_IN, T_TXT, answer, sizeof(answer));

    if (ans_len <= 0) {
        dns_error(self, gettext("DNS query failed."));
        killdns_thread(dns_obj);
    }

    char ans_id[MAX_DNS_REQST_SIZE + 1];

    /* extract TXT from DNS response */
    if (parse_dns_response(self, answer, ans_len, ans_id) == -1)
        killdns_thread(dns_obj);

    char encrypted_id[MAX_DNS_REQST_SIZE + 1];
    int prfx_len = strlen(TOX_DNS3_TXT_PREFIX);

    /* extract the encrypted ID from TXT response */
    if (strncmp(ans_id, TOX_DNS3_TXT_PREFIX, prfx_len) != 0) {
        dns_error(self, gettext("Bad DNS3 TXT response."));
        killdns_thread(dns_obj);
    }

    memcpy(encrypted_id, ans_id + prfx_len, ans_len - prfx_len);

    if (tox_decrypt_dns3_TXT(dns_obj, (uint8_t *) t_data.id_bin, (uint8_t *) encrypted_id,
                             strlen(encrypted_id), request_id) == -1) {
        dns_error(self, gettext("Core failed to decrypt DNS response."));
        killdns_thread(dns_obj);
    }

    pthread_mutex_lock(&Winthread.lock);
    cmd_add_helper(self, t_data.m, t_data.id_bin, t_data.msg);
    pthread_mutex_unlock(&Winthread.lock);

    killdns_thread(dns_obj);
    return 0;
}

/* creates new thread for dns3 lookup. Only allows one lookup at a time. */
void dns3_lookup(ToxWindow *self, Tox *m, const char *id_bin, const char *addr, const char *msg)
{
    if (arg_opts.proxy_type != TOX_PROXY_TYPE_NONE && arg_opts.force_tcp) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, gettext("DNS lookups are disabled."));
        return;
    }

    if (t_data.busy) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, gettext("Please wait for previous user lookup to finish."));
        return;
    }

    if (!dns3_servers.loaded) {
        const char *path = arg_opts.dns_path[0] ? arg_opts.dns_path : PACKAGE_DATADIR "/DNSservers";
        dns3_servers.loaded = true;
        int ret = load_dns_domainlist(path);

        if (ret < 0) {
            const char *errmsg = gettext("DNS server list failed to load with error code %d. Falling back to hard-coded list.");
            line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, 0, errmsg, ret);
        }
    }

    snprintf(t_data.id_bin, sizeof(t_data.id_bin), "%s", id_bin);
    snprintf(t_data.addr, sizeof(t_data.addr), "%s", addr);
    snprintf(t_data.msg, sizeof(t_data.msg), "%s", msg);
    t_data.self = self;
    t_data.m = m;
    t_data.busy = 1;

    if (pthread_attr_init(&dns_thread.attr) != 0) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, RED, gettext("Error: DNS thread attr failed to init"));
        memset(&t_data, 0, sizeof(struct thread_data));
        return;
    }

    if (pthread_attr_setdetachstate(&dns_thread.attr, PTHREAD_CREATE_DETACHED) != 0) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, RED, gettext("Error: DNS thread attr failed to set"));
        pthread_attr_destroy(&dns_thread.attr);
        memset(&t_data, 0, sizeof(struct thread_data));
        return;
    }

    if (pthread_create(&dns_thread.tid, &dns_thread.attr, dns3_lookup_thread, NULL) != 0) {
        line_info_add(self, NULL, NULL, NULL, SYS_MSG, 0, RED, gettext("Error: DNS thread failed to init"));
        pthread_attr_destroy(&dns_thread.attr);
        memset(&t_data, 0, sizeof(struct thread_data));
        return;
    }
}

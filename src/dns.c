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
#include <netinet/in.h>
#include <resolv.h>

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

#define MAX_DNS_REQST_SIZE 256
#define NUM_DNS3_SERVERS 2    /* must correspond to number of items in dns3_servers array */
#define TOX_DNS3_TXT_PREFIX "v=tox3;id="
#define DNS3_KEY_SZ 32

extern struct _Winthread Winthread;

/* TODO: process keys from key file instead of hard-coding like a noob */
static struct dns3_server {
    char *name;
    char key[DNS3_KEY_SZ];
} dns3_servers[] = {
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

static struct _thread_data {
    ToxWindow *self;
    char id_bin[TOX_FRIEND_ADDRESS_SIZE];
    char addr[MAX_STR_SIZE];
    char msg[MAX_STR_SIZE];
    uint8_t busy;
    Tox *m;
} t_data;

static struct _dns_thread {
    pthread_t tid;
} dns_thread;


static int dns_error(ToxWindow *self, char *errmsg)
{
    char msg[MAX_STR_SIZE];
    snprintf(msg, sizeof(msg), "User lookup failed: %s", errmsg);

    pthread_mutex_lock(&Winthread.lock);
    line_info_add(self, NULL, NULL, NULL, msg, SYS_MSG, 0, 0);
    pthread_mutex_unlock(&Winthread.lock);

    return -1;
}

static void kill_dns_thread(void *dns_obj)
{
    if (dns_obj)
        tox_dns3_kill(dns_obj);

    memset(&t_data, 0, sizeof(struct _thread_data));
    pthread_exit(0);
}

/* puts TXT from dns response in buf. Returns length of TXT on success, -1 on fail.*/
static int parse_dns_response(ToxWindow *self, u_char *answer, int ans_len, char *buf)
{
    uint8_t *ans_pt = answer + sizeof(HEADER);
    uint8_t *ans_end = answer + ans_len;
    char exp_ans[PACKETSZ];
    
    int len = dn_expand(answer, ans_end, ans_pt, exp_ans, sizeof(exp_ans));

    if (len == -1)
        return dns_error(self, "dn_expand failed."); 

    ans_pt += len;

    if (ans_pt > ans_end - 4)
         return dns_error(self, "DNS reply was too short."); 

    int type;
    GETSHORT(type, ans_pt);

    if (type != T_TXT)
        return dns_error(self, "Broken DNS reply."); 
 

    ans_pt += INT16SZ;    /* class */
    uint32_t size = 0;

    /* recurse through CNAME rr's */
    do { 
        ans_pt += size;
        len = dn_expand(answer, ans_end, ans_pt, exp_ans, sizeof(exp_ans));

        if (len == -1)
            return dns_error(self, "Second dn_expand failed."); 

        ans_pt += len;

        if (ans_pt > ans_end - 10)
            return dns_error(self, "DNS reply was too short."); 

        GETSHORT(type, ans_pt);
        ans_pt += INT16SZ;
        ans_pt += 4;
        GETSHORT(size, ans_pt);

        if (ans_pt + size < answer || ans_pt + size > ans_end)
            return dns_error(self, "RR overflow."); 

    } while (type == T_CNAME);

    if (type != T_TXT)
        return dns_error(self, "DNS response failed."); 

    uint32_t txt_len = *ans_pt;

    if (!size || txt_len >= size || !txt_len)
        return dns_error(self, "No record found.");

    ans_pt++;
    ans_pt[txt_len] = '\0';
    memcpy(buf, ans_pt, txt_len + 1);

    return txt_len;
}

/* Takes address addr in the form "username@domain", puts the username in namebuf, 
   and the domain in dombuf.

   return length of username on success, -1 on failure */
static int parse_addr(char *addr, char *namebuf, char *dombuf)
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

/* Does DNS lookup for addr and puts resulting tox id in id_bin. */
void *dns3_lookup_thread(void *data)
{
    ToxWindow *self = t_data.self;

    char domain[MAX_STR_SIZE];
    char name[MAX_STR_SIZE];

    int namelen = parse_addr(t_data.addr, name, domain);

    if (namelen == -1) {
        dns_error(self, "Must be a Tox ID or an address in the form username@domain");
        kill_dns_thread(NULL);
    }

    /* get domain name/pub key */
    char *DNS_pubkey = NULL;
    char *domname = NULL;
    int i;

    for (i = 0; i < NUM_DNS3_SERVERS; ++i) {
        if (strcmp(dns3_servers[i].name, domain) == 0) {
            DNS_pubkey = dns3_servers[i].key;
            domname = dns3_servers[i].name;
            break;
        }
    }

    if (domname == NULL) {
        dns_error(self, "Domain not found.");
        kill_dns_thread(NULL);
    }

    void *dns_obj = tox_dns3_new((uint8_t *) DNS_pubkey);

    if (dns_obj == NULL) {
        dns_error(self, "Core failed to create DNS object.");
        kill_dns_thread(NULL);
    }

    char string[MAX_DNS_REQST_SIZE];
    uint32_t request_id;

    int str_len = tox_generate_dns3_string(dns_obj, (uint8_t *) string, sizeof(string), &request_id, 
                                           (uint8_t *) name, namelen);

    if (str_len == -1) {
        dns_error(self, "Core failed to generate DNS3 string.");
        kill_dns_thread(dns_obj);
    }

    string[str_len] = '\0';

    u_char answer[PACKETSZ];
    char d_string[MAX_DNS_REQST_SIZE];

    /* format string and create dns query */
    snprintf(d_string, sizeof(d_string), "_%s._tox.%s", string, domname);
    int ans_len = res_query(d_string, C_IN, T_TXT, answer, sizeof(answer));

    if (ans_len <= 0) {
        dns_error(self, "DNS query failed.");
        kill_dns_thread(dns_obj);
    }

    char ans_id[MAX_DNS_REQST_SIZE];

    /* extract TXT from DNS response */
    if (parse_dns_response(self, answer, ans_len, ans_id) == -1)
        kill_dns_thread(dns_obj);

    char encrypted_id[MAX_DNS_REQST_SIZE];
    int prfx_len = strlen(TOX_DNS3_TXT_PREFIX);

    /* extract the encrypted ID from TXT response */
    if (strncmp(ans_id, TOX_DNS3_TXT_PREFIX, prfx_len) != 0) {
        dns_error(self, "Bad DNS3 TXT response.");
        kill_dns_thread(dns_obj);
    }

    memcpy(encrypted_id, ans_id + prfx_len, ans_len - prfx_len);

    if (tox_decrypt_dns3_TXT(dns_obj, (uint8_t *) t_data.id_bin, (uint8_t *) encrypted_id, 
                             strlen(encrypted_id), request_id) == -1) {
        dns_error(self, "Core failed to decrypt DNS response.");
        kill_dns_thread(dns_obj);
    }

    pthread_mutex_lock(&Winthread.lock);
    cmd_add_helper(self, t_data.m, t_data.id_bin, t_data.msg);
    pthread_mutex_unlock(&Winthread.lock);

    kill_dns_thread(dns_obj);
    return 0;
}

/* creates new thread for dns3 lookup. Only allows one lookup at a time. */
void dns3_lookup(ToxWindow *self, Tox *m, char *id_bin, char *addr, char *msg)
{
    if (t_data.busy) {
        char *err = "Please wait for previous user lookup to finish.";
        line_info_add(self, NULL, NULL, NULL, err, SYS_MSG, 0, 0);
        return;
    }

    snprintf(t_data.id_bin, sizeof(t_data.id_bin), "%s", id_bin);
    snprintf(t_data.addr, sizeof(t_data.addr), "%s", addr);
    snprintf(t_data.msg, sizeof(t_data.msg), "%s", msg);
    t_data.self = self;
    t_data.m = m;
    t_data.busy = 1;

    pthread_mutex_unlock(&Winthread.lock);

    if (pthread_create(&dns_thread.tid, NULL, dns3_lookup_thread, NULL) != 0)
        exit_toxic_err("failed in dns3_lookup", FATALERR_THREAD_CREATE);

    pthread_mutex_lock(&Winthread.lock);
}

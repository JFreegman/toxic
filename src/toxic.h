/*  toxic.h
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

#ifndef TOXIC_H
#define TOXIC_H

#ifndef TOXICVER
#define TOXICVER "NOVERS"    /* Use the -D flag to set this */
#endif

#ifndef SIGWINCH
#define SIGWINCH 28
#endif

#ifndef SIGINT
#define SIGINT 2
#endif

#include <stdbool.h>
#include <curses.h>

#include <tox/tox.h>

#define UNKNOWN_NAME "Anonymous"
#define DEFAULT_TOX_NAME "Tox User"   /* should always be the same as toxcore's default name */

#define MAX_STR_SIZE TOX_MAX_MESSAGE_LENGTH    /* must be >= TOX_MAX_MESSAGE_LENGTH */
#define MAX_CMDNAME_SIZE 64
#define TOXIC_MAX_NAME_LENGTH 32   /* Must be <= TOX_MAX_NAME_LENGTH */
#define KEY_IDENT_DIGITS 3    /* number of hex digits to display for the pub-key based identifier */
#define TIME_STR_SIZE 32

/* ASCII key codes */
#define T_KEY_ESC        0x1B     /* ESC key */
#define T_KEY_KILL       0x0B     /* ctrl-k */
#define T_KEY_DISCARD    0x15     /* ctrl-u */
#define T_KEY_NEXT       0x10     /* ctrl-p */
#define T_KEY_PREV       0x0F     /* ctrl-o */
#define T_KEY_C_E        0x05     /* ctrl-e */
#define T_KEY_C_A        0x01     /* ctrl-a */
#define T_KEY_C_RB       0x1D     /* ctrl-] */
#define T_KEY_C_LB       0x1B     /* ctrl-[ */
#define T_KEY_C_V        0x16     /* ctrl-v */
#define T_KEY_C_F        0x06     /* ctrl-f */
#define T_KEY_C_H        0x08     /* ctrl-h */
#define T_KEY_C_Y        0x19     /* ctrl-y */
#define T_KEY_C_L        0x0C     /* ctrl-l */
#define T_KEY_C_W        0x17     /* ctrl-w */
#define T_KEY_C_B        0x02     /* ctrl-b */
#define T_KEY_TAB        0x09     /* TAB key */

#define ONLINE_CHAR "*"
#define OFFLINE_CHAR "o"

typedef enum _FATAL_ERRS {
    FATALERR_MEMORY = -1,           /* heap memory allocation failed */
    FATALERR_FILEOP = -2,           /* critical file operation failed */
    FATALERR_THREAD_CREATE = -3,    /* thread creation failed for critical thread */
    FATALERR_MUTEX_INIT = -4,       /* mutex init for critical thread failed */
    FATALERR_THREAD_ATTR = -5,      /* thread attr object init failed */
    FATALERR_LOCALE_NOT_SET = -6,   /* system locale not set */
    FATALERR_STORE_DATA = -7,       /* store_data failed in critical section */
    FATALERR_INFLOOP = -8,          /* infinite loop detected */
    FATALERR_WININIT = -9,          /* window init failed */
    FATALERR_PROXY = -10,           /* Tox network failed to init using a proxy */
    FATALERR_ENCRYPT = -11,         /* Data file encryption failure */
    FATALERR_TOX_INIT = -12,        /* Tox instance failed to initialize */
} FATAL_ERRS;

/* Fixes text color problem on some terminals.
   Uncomment if necessary */
/* #define URXVT_FIX */

void lock_status ();
void unlock_status ();

void exit_toxic_success(Tox *m);
void exit_toxic_err(const char *errmsg, int errcode);

int store_data(Tox *m, const char *path);

/* callbacks */
void on_request(Tox *m, const uint8_t *public_key, const uint8_t *data, size_t length, void *userdata);
void on_connectionchange(Tox *m, uint32_t friendnumber, TOX_CONNECTION status, void *userdata);
void on_message(Tox *m, uint32_t friendnumber, TOX_MESSAGE_TYPE type, const uint8_t *string, size_t length, void *userdata);
void on_action(Tox *m, uint32_t friendnumber, const uint8_t *string, size_t length, void *userdata);
void on_nickchange(Tox *m, uint32_t friendnumber, const uint8_t *string, size_t length, void *userdata);
void on_statuschange(Tox *m, uint32_t friendnumber, TOX_USER_STATUS status, void *userdata);
void on_statusmessagechange(Tox *m, uint32_t friendnumber, const uint8_t *string, size_t length, void *userdata);
void on_friendadded(Tox *m, uint32_t friendnumber, bool sort);
void on_file_chunk_request(Tox *m, uint32_t friendnumber, uint32_t filenumber, uint64_t position, size_t length, void *userdata);
void on_file_recv_chunk(Tox *m, uint32_t friendnumber, uint32_t filenumber, uint64_t position, const uint8_t *data,
                        size_t length, void *userdata);
void on_file_control (Tox *m, uint32_t friendnumber, uint32_t filenumber, TOX_FILE_CONTROL control, void *userdata);
void on_file_recv(Tox *m, uint32_t friendnumber, uint32_t filenumber, uint32_t kind, uint64_t file_size,
                  const uint8_t *filename, size_t filename_length, void *userdata);
void on_typing_change(Tox *m, uint32_t friendnumber, bool is_typing, void *userdata);
void on_read_receipt(Tox *m, uint32_t friendnumber, uint32_t receipt, void *userdata);

void on_group_invite(Tox *m, int32_t friendnumber, const uint8_t *invite_data, uint16_t length, void *userdata);
void on_group_message(Tox *m, int groupnumber, uint32_t peernumber, const uint8_t *message, uint16_t length, void *userdata);
void on_group_action(Tox *m, int groupnumber, uint32_t peernumber, const uint8_t *action, uint16_t length, void *userdata);
void on_group_private_message(Tox *m, int groupnumber, uint32_t peernumber, const uint8_t *message, uint16_t length, void *userdata);
void on_group_namelistchange(Tox *m, int groupnumber, void *userdata);
void on_group_peer_join(Tox *m, int groupnumber, uint32_t peernumber, void *userdata);
void on_group_peer_exit(Tox *m, int groupnumber, uint32_t peernumber, const uint8_t *partmsg, uint16_t length, void *userdata);
void on_group_topic_change(Tox *m, int groupnumber, uint32_t peernumber, const uint8_t *topic, uint16_t length, void *userdata);
void on_group_nick_change(Tox *m, int groupnumber, uint32_t peernumber, const uint8_t *newname, uint16_t length, void *userdata);
void on_group_self_join(Tox *m, int groupnumber, void *userdata);
void on_group_rejected(Tox *m, int groupnumber, uint8_t type, void *userdata);
void on_group_moderation(Tox *m, int groupnumber, uint32_t source_peernum, uint32_t target_peernum,
                         TOX_GROUP_MOD_TYPE type, void *userdata);

#endif  /* #define TOXIC_H */

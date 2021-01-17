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
#define COLOR_STR_SIZE 10 /* should fit every color option */

#ifndef MAX_PORT_RANGE
#define MAX_PORT_RANGE 65535
#endif

/* ASCII key codes */
#define T_KEY_ESC        0x1B     /* ESC key */
#define T_KEY_KILL       0x0B     /* ctrl-k */
#define T_KEY_DISCARD    0x15     /* ctrl-u */
#define T_KEY_NEXT       0x10     /* ctrl-p */
#define T_KEY_PREV       0x0F     /* ctrl-o */
#define T_KEY_C_E        0x05     /* ctrl-e */
#define T_KEY_C_A        0x01     /* ctrl-a */
#define T_KEY_C_V        0x16     /* ctrl-v */
#define T_KEY_C_F        0x06     /* ctrl-f */
#define T_KEY_C_H        0x08     /* ctrl-h */
#define T_KEY_C_Y        0x19     /* ctrl-y */
#define T_KEY_C_L        0x0C     /* ctrl-l */
#define T_KEY_C_W        0x17     /* ctrl-w */
#define T_KEY_C_B        0x02     /* ctrl-b */
#define T_KEY_C_T        0x14     /* ctrl-t */
#define T_KEY_C_LEFT     0x221    /* ctrl-left arrow */
#define T_KEY_C_RIGHT    0x230    /* ctrl-right arrow */
#define T_KEY_C_UP       0x236    /* ctrl-up arrow */
#define T_KEY_C_DOWN     0x20D    /* ctrl-down arrow */
#define T_KEY_TAB        0x09     /* TAB key */

#define ONLINE_CHAR  "o"
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
    FATALERR_CURSES = -13,          /* Unrecoverable Ncurses error */
} FATAL_ERRS;

/* Fixes text color problem on some terminals.
   Uncomment if necessary */
/* #define URXVT_FIX */

void lock_status(void);
void unlock_status(void);

void exit_toxic_success(Tox *m);
void exit_toxic_err(const char *errmsg, int errcode);

int store_data(Tox *m, const char *path);

/* callbacks */
void on_friend_request(Tox *m, const uint8_t *public_key, const uint8_t *data, size_t length, void *userdata);
void on_friend_connection_status(Tox *m, uint32_t friendnumber, Tox_Connection status, void *userdata);
void on_friend_message(Tox *m, uint32_t friendnumber, Tox_Message_Type type, const uint8_t *string, size_t length,
                       void *userdata);
void on_friend_name(Tox *m, uint32_t friendnumber, const uint8_t *string, size_t length, void *userdata);
void on_friend_status(Tox *m, uint32_t friendnumber, Tox_User_Status status, void *userdata);
void on_friend_status_message(Tox *m, uint32_t friendnumber, const uint8_t *string, size_t length, void *userdata);
void on_friend_added(Tox *m, uint32_t friendnumber, bool sort);
void on_conference_message(Tox *m, uint32_t conferencenumber, uint32_t peernumber, Tox_Message_Type type,
                           const uint8_t *message, size_t length, void *userdata);
void on_conference_invite(Tox *m, uint32_t friendnumber, Tox_Conference_Type type, const uint8_t *conference_pub_key,
                          size_t length, void *userdata);
void on_conference_peer_list_changed(Tox *m, uint32_t conferencenumber, void *userdata);
void on_conference_peer_name(Tox *m, uint32_t conferencenumber, uint32_t peernumber, const uint8_t *name,
                             size_t length, void *userdata);
void on_conference_title(Tox *m, uint32_t conferencenumber, uint32_t peernumber, const uint8_t *title, size_t length,
                         void *userdata);
void on_file_chunk_request(Tox *m, uint32_t friendnumber, uint32_t filenumber, uint64_t position, size_t length,
                           void *userdata);
void on_file_recv_chunk(Tox *m, uint32_t friendnumber, uint32_t filenumber, uint64_t position, const uint8_t *data,
                        size_t length, void *userdata);
void on_file_recv_control(Tox *m, uint32_t friendnumber, uint32_t filenumber, Tox_File_Control control,
                          void *userdata);
void on_file_recv(Tox *m, uint32_t friendnumber, uint32_t filenumber, uint32_t kind, uint64_t file_size,
                  const uint8_t *filename, size_t filename_length, void *userdata);
void on_friend_typing(Tox *m, uint32_t friendnumber, bool is_typing, void *userdata);
void on_friend_read_receipt(Tox *m, uint32_t friendnumber, uint32_t receipt, void *userdata);
void on_lossless_custom_packet(Tox *m, uint32_t friendnumber, const uint8_t *data, size_t length, void *userdata);


#endif /* TOXIC_H */

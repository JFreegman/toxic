/*  windows.h
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

#ifndef WINDOWS_H
#define WINDOWS_H

#include <pthread.h>
#include <wctype.h>
#include <wchar.h>
#include <signal.h>

#include <tox/tox.h>

#ifdef AUDIO
#include <tox/toxav.h>
#endif /* AUDIO */

#include "toxic.h"

#define MAX_WINDOWS_NUM 32
#define MAX_WINDOW_NAME_LENGTH 16
#define CURS_Y_OFFSET 1    /* y-axis cursor offset for chat contexts */
#define CHATBOX_HEIGHT 2

/* Curses foreground colours (background is black) */
enum {
    WHITE,
    GREEN,
    CYAN,
    RED,
    BLUE,
    YELLOW,
    MAGENTA,
    BLACK,
} C_COLOURS;

/* tab alert types: lower types take priority (this relies on the order of C_COLOURS) */
typedef enum {
    WINDOW_ALERT_NONE = 0,
    WINDOW_ALERT_0 = GREEN,
    WINDOW_ALERT_1 = RED,
    WINDOW_ALERT_2 = MAGENTA,
} WINDOW_ALERTS;

/* Fixes text color problem on some terminals.
   Uncomment if necessary */
/* #define URXVT_FIX */

struct Winthread {
    pthread_t tid;
    pthread_mutex_t lock;
    volatile sig_atomic_t sig_exit_toxic;
    volatile sig_atomic_t flag_resize;
};

struct cqueue_thread {
    pthread_t tid;
};

struct arg_opts {
    int ignore_data_file;
    int use_ipv4;
    int force_tcp;
    int debug;
    int default_locale;
    int use_custom_data;
    int no_connect;
    int encrypt_data;
    int unencrypt_data;
    char dns_path[MAX_STR_SIZE];
    char config_path[MAX_STR_SIZE];
    char nodes_path[MAX_STR_SIZE];

    int use_proxy;
    char proxy_address[256];
    uint16_t proxy_port;
};

typedef struct ToxWindow ToxWindow;
typedef struct StatusBar StatusBar;
typedef struct PromptBuf PromptBuf;
typedef struct ChatContext ChatContext;
typedef struct Help Help;

struct ToxWindow {
    void(*onKey)(ToxWindow *, Tox *, wint_t, bool);
    void(*onDraw)(ToxWindow *, Tox *);
    void(*onInit)(ToxWindow *, Tox *);
    void(*onFriendRequest)(ToxWindow *, Tox *, const char *, const char *, uint16_t);
    void(*onFriendAdded)(ToxWindow *, Tox *, int32_t, bool);
    void(*onConnectionChange)(ToxWindow *, Tox *, int32_t, uint8_t);
    void(*onMessage)(ToxWindow *, Tox *, int32_t, const char *, uint16_t);
    void(*onNickChange)(ToxWindow *, Tox *, int32_t, const char *, uint16_t);
    void(*onStatusChange)(ToxWindow *, Tox *, int32_t, uint8_t);
    void(*onStatusMessageChange)(ToxWindow *, int32_t, const char *, uint16_t);
    void(*onAction)(ToxWindow *, Tox *, int32_t, const char *, uint16_t);
    void(*onGroupMessage)(ToxWindow *, Tox *, int, int, const char *, uint16_t);
    void(*onGroupAction)(ToxWindow *, Tox *, int, int, const char *, uint16_t);
    void(*onGroupInvite)(ToxWindow *, Tox *, int32_t, const char *, uint16_t);
    void(*onGroupNamelistChange)(ToxWindow *, Tox *, int, int, uint8_t);
    void(*onFileSendRequest)(ToxWindow *, Tox *, int32_t, uint8_t, uint64_t, const char *, uint16_t);
    void(*onFileControl)(ToxWindow *, Tox *, int32_t, uint8_t, uint8_t, uint8_t, const char *, uint16_t);
    void(*onFileData)(ToxWindow *, Tox *, int32_t, uint8_t, const char *, uint16_t);
    void(*onTypingChange)(ToxWindow *, Tox *, int32_t, uint8_t);
    void(*onReadReceipt)(ToxWindow *, Tox *, int32_t, uint32_t);

#ifdef AUDIO

    void(*onInvite)(ToxWindow *, ToxAv *, int);
    void(*onRinging)(ToxWindow *, ToxAv *, int);
    void(*onStarting)(ToxWindow *, ToxAv *, int);
    void(*onEnding)(ToxWindow *, ToxAv *, int);
    void(*onError)(ToxWindow *, ToxAv *, int);
    void(*onStart)(ToxWindow *, ToxAv *, int);
    void(*onCancel)(ToxWindow *, ToxAv *, int);
    void(*onReject)(ToxWindow *, ToxAv *, int);
    void(*onEnd)(ToxWindow *, ToxAv *, int);
    void(*onRequestTimeout)(ToxWindow *, ToxAv *, int);
    void(*onPeerTimeout)(ToxWindow *, ToxAv *, int);

    int call_idx; /* If in a call will have this index set, otherwise it's -1. 
                   * Don't modify outside av callbacks. */
    int device_selection[2]; /* -1 if not set, if set uses these selections instead of primary device */

    int ringing_sound;
#endif /* AUDIO */

    int active_box; /* For box notify */
    
    char name[TOXIC_MAX_NAME_LENGTH];
    int32_t num;    /* corresponds to friendnumber in chat windows */
    bool active;
    int x;

    bool is_chat;
    bool is_groupchat;
    bool is_prompt;
    bool is_friendlist;

    WINDOW_ALERTS alert;

    ChatContext *chatwin;
    StatusBar *stb;
    Help *help;

    WINDOW *window;
};

/* statusbar info holder */
struct StatusBar {
    WINDOW *topline;
    char statusmsg[TOX_MAX_STATUSMESSAGE_LENGTH];
    uint16_t statusmsg_len;
    char nick[TOXIC_MAX_NAME_LENGTH];
    int nick_len;
    uint8_t status;
    bool is_online;
};

#ifdef AUDIO


#define INFOBOX_HEIGHT 7
#define INFOBOX_WIDTH 21
/* holds display info for audio calls */
struct infobox {
    float vad_lvl;
    bool in_is_muted;
    bool out_is_muted;
    bool hide;
    bool active;

    uint64_t lastupdate;
    uint64_t starttime;
    char timestr[TIME_STR_SIZE];

    WINDOW *win;
};
#endif /* AUDIO */

#define MAX_LINE_HIST 128

/* chat and groupchat window/buffer holder */
struct ChatContext {
    wchar_t line[MAX_STR_SIZE];
    int pos;
    int len;
    int start;    /* the position to start printing line at */

    wchar_t ln_history[MAX_LINE_HIST][MAX_STR_SIZE];  /* history for input lines/commands */
    int hst_pos;
    int hst_tot;

    wchar_t yank[MAX_STR_SIZE];    /* contains last killed/discarded line */
    int yank_len;

    struct history *hst;
    struct chatlog *log;
    struct chat_queue *cqueue;

#ifdef AUDIO
    struct infobox infobox;
#endif

    uint8_t self_is_typing;

    WINDOW *history;
    WINDOW *linewin;
    WINDOW *sidebar;
};

struct Help {
    WINDOW *win;
    int type;
    bool active;
};

ToxWindow *init_windows(Tox *m);
void draw_active_window(Tox *m);
int add_window(Tox *m, ToxWindow w);
void del_window(ToxWindow *w);
void set_active_window(int ch);
int get_num_active_windows(void);
void kill_all_windows(Tox *m);    /* should only be called on shutdown */
void on_window_resize(void);
ToxWindow *get_window_ptr(int i);

/* refresh inactive windows to prevent scrolling bugs. 
   call at least once per second */
void refresh_inactive_windows(void);

#endif  /* #define WINDOWS_H */

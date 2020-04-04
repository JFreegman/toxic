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

#define MAX_WINDOWS_NUM 16
#define MAX_WINDOW_NAME_LENGTH 22
#define CURS_Y_OFFSET 1    /* y-axis cursor offset for chat contexts */
#define CHATBOX_HEIGHT 2

/* Curses foreground colours (background is black) */
typedef enum {
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

struct av_thread {
    pthread_t tid;
};

struct arg_opts {
    bool use_ipv4;
    bool force_tcp;
    bool debug;
    bool default_locale;
    bool use_custom_data;
    bool no_connect;
    bool encrypt_data;
    bool unencrypt_data;

    char nameserver_path[MAX_STR_SIZE];
    char config_path[MAX_STR_SIZE];
    char nodes_path[MAX_STR_SIZE];

    char proxy_address[256];
    uint8_t proxy_type;
    uint16_t proxy_port;

    uint16_t tcp_port;
};

typedef struct ToxWindow ToxWindow;
typedef struct StatusBar StatusBar;
typedef struct PromptBuf PromptBuf;
typedef struct ChatContext ChatContext;
typedef struct Help Help;

struct ToxWindow {
    /* ncurses */
    void(*onKey)(ToxWindow *, Tox *, wint_t, bool);
    void(*onDraw)(ToxWindow *, Tox *);
    void(*onInit)(ToxWindow *, Tox *);

    /* toxcore */
    void(*onFriendRequest)(ToxWindow *, Tox *, const char *, const char *, size_t);
    void(*onFriendAdded)(ToxWindow *, Tox *, uint32_t, bool);
    void(*onConnectionChange)(ToxWindow *, Tox *, uint32_t, Tox_Connection);
    void(*onMessage)(ToxWindow *, Tox *, uint32_t, Tox_Message_Type, const char *, size_t);
    void(*onNickChange)(ToxWindow *, Tox *, uint32_t, const char *, size_t);
    void(*onStatusChange)(ToxWindow *, Tox *, uint32_t, Tox_User_Status);
    void(*onStatusMessageChange)(ToxWindow *, uint32_t, const char *, size_t);
    void(*onGroupMessage)(ToxWindow *, Tox *, uint32_t, uint32_t, Tox_Message_Type, const char *, size_t);
    void(*onGroupInvite)(ToxWindow *, Tox *, int32_t, uint8_t, const char *, uint16_t);
    void(*onGroupNameListChange)(ToxWindow *, Tox *, uint32_t);
    void(*onGroupPeerNameChange)(ToxWindow *, Tox *, uint32_t, uint32_t, const char *, size_t);
    void(*onGroupTitleChange)(ToxWindow *, Tox *, uint32_t, uint32_t, const char *, size_t);
    void(*onFileChunkRequest)(ToxWindow *, Tox *, uint32_t, uint32_t, uint64_t, size_t);
    void(*onFileRecvChunk)(ToxWindow *, Tox *, uint32_t, uint32_t, uint64_t, const char *, size_t);
    void(*onFileControl)(ToxWindow *, Tox *, uint32_t, uint32_t, Tox_File_Control);
    void(*onFileRecv)(ToxWindow *, Tox *, uint32_t, uint32_t, uint64_t, const char *, size_t);
    void(*onTypingChange)(ToxWindow *, Tox *, uint32_t, bool);
    void(*onReadReceipt)(ToxWindow *, Tox *, uint32_t, uint32_t);

#ifdef AUDIO

    void(*onInvite)(ToxWindow *, ToxAV *, uint32_t, int);
    void(*onRinging)(ToxWindow *, ToxAV *, uint32_t, int);
    void(*onStarting)(ToxWindow *, ToxAV *, uint32_t, int);
    void(*onEnding)(ToxWindow *, ToxAV *, uint32_t, int);
    void(*onError)(ToxWindow *, ToxAV *, uint32_t, int);
    void(*onStart)(ToxWindow *, ToxAV *, uint32_t, int);
    void(*onCancel)(ToxWindow *, ToxAV *, uint32_t, int);
    void(*onReject)(ToxWindow *, ToxAV *, uint32_t, int);
    void(*onEnd)(ToxWindow *, ToxAV *, uint32_t, int);
    void(*onWriteDevice)(ToxWindow *, Tox *, uint32_t, int, const int16_t *, unsigned int, uint8_t, unsigned int);

    bool is_call;
    int ringing_sound;

#endif /* AUDIO */

    int active_box; /* For box notify */

    char name[TOXIC_MAX_NAME_LENGTH + 1];
    uint32_t num;    /* corresponds to friendnumber in chat windows */
    uint8_t index; /* This window's index in the windows array */
    int x;

    bool is_chat;
    bool is_prompt;
    bool is_friendlist;
    bool is_groupchat;
    int show_peerlist;    /* used to toggle groupchat peerlist */

    WINDOW_ALERTS alert;

    ChatContext *chatwin;
    StatusBar *stb;
    Help *help;

    WINDOW *window;
};

/* statusbar info holder */
struct StatusBar {
    WINDOW *topline;
    char statusmsg[TOX_MAX_STATUS_MESSAGE_LENGTH + 1];
    size_t statusmsg_len;
    char nick[TOXIC_MAX_NAME_LENGTH + 1];
    size_t nick_len;
    Tox_User_Status status;
    Tox_Connection connection;
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

    time_t lastupdate;
    time_t starttime;
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
    uint8_t pastemode; /* whether to translate \r to \n */

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
int add_window(Tox *m, ToxWindow *w);
void del_window(ToxWindow *w);
void set_active_window_index(uint8_t index);
int get_num_active_windows(void);
void kill_all_windows(Tox *m);    /* should only be called on shutdown */
void on_window_resize(void);
void force_refresh(WINDOW *w);
ToxWindow *get_window_ptr(size_t i);
ToxWindow *get_active_window(void);

/* refresh inactive windows to prevent scrolling bugs.
   call at least once per second */
void refresh_inactive_windows(void);

#endif /* WINDOWS_H */

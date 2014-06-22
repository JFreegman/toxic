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

#ifndef _windows_h
#define _windows_h

#include <pthread.h>
#include <wctype.h>
#include <wchar.h>

#include <tox/tox.h>

#ifdef _SUPPORT_AUDIO
#include <tox/toxav.h>
#endif /* _SUPPORT_AUDIO */

#include "toxic.h"

#define MAX_WINDOWS_NUM 32
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

/* tab alert types: lower types take priority */
enum {
    WINDOW_ALERT_0,
    WINDOW_ALERT_1,
    WINDOW_ALERT_2,
} WINDOW_ALERTS;

/* Fixes text color problem on some terminals.
   Uncomment if necessary */
/* #define URXVT_FIX */

struct _Winthread {
    pthread_t tid;
    pthread_mutex_t lock;
};

typedef struct ToxWindow ToxWindow;
typedef struct StatusBar StatusBar;
typedef struct PromptBuf PromptBuf;
typedef struct ChatContext ChatContext;

struct ToxWindow {
    void(*onKey)(ToxWindow *, Tox *, wint_t, bool);
    void(*onDraw)(ToxWindow *, Tox *);
    void(*onInit)(ToxWindow *, Tox *);
    void(*onFriendRequest)(ToxWindow *, Tox *, const uint8_t *, const uint8_t *, uint16_t);
    void(*onFriendAdded)(ToxWindow *, Tox *, int32_t, bool);
    void(*onConnectionChange)(ToxWindow *, Tox *, int32_t, uint8_t);
    void(*onMessage)(ToxWindow *, Tox *, int32_t, uint8_t *, uint16_t);
    void(*onNickChange)(ToxWindow *, Tox *, int32_t, uint8_t *, uint16_t);
    void(*onStatusChange)(ToxWindow *, Tox *, int32_t, uint8_t);
    void(*onStatusMessageChange)(ToxWindow *, int32_t, uint8_t *, uint16_t);
    void(*onAction)(ToxWindow *, Tox *, int32_t, uint8_t *, uint16_t);
    void(*onGroupMessage)(ToxWindow *, Tox *, int, int, uint8_t *, uint16_t);
    void(*onGroupAction)(ToxWindow *, Tox *, int, int, uint8_t *, uint16_t);
    void(*onGroupInvite)(ToxWindow *, Tox *, int32_t, uint8_t *);
    void(*onGroupNamelistChange)(ToxWindow *, Tox *, int, int, uint8_t);
    void(*onFileSendRequest)(ToxWindow *, Tox *, int32_t, uint8_t, uint64_t, uint8_t *, uint16_t);
    void(*onFileControl)(ToxWindow *, Tox *, int32_t, uint8_t, uint8_t, uint8_t, uint8_t *, uint16_t);
    void(*onFileData)(ToxWindow *, Tox *, int32_t, uint8_t, uint8_t *, uint16_t);
    void(*onTypingChange)(ToxWindow *, Tox *, int32_t, uint8_t);

#ifdef _SUPPORT_AUDIO

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
#endif /* _SUPPORT_AUDIO */

    char name[TOX_MAX_NAME_LENGTH];
    int32_t num;    /* corresponds to friendnumber in chat windows */
    bool active;
    int x;

    /* window type identifiers */
    bool is_chat;
    bool is_groupchat;
    bool is_prompt;

    bool alert0;
    bool alert1;
    bool alert2;

    ChatContext *chatwin;
    StatusBar *stb;

    WINDOW *popup;
    WINDOW *window;
};

/* statusbar info holder */
struct StatusBar {
    WINDOW *topline;
    uint8_t statusmsg[TOX_MAX_STATUSMESSAGE_LENGTH];
    uint16_t statusmsg_len;
    uint8_t nick[TOX_MAX_NAME_LENGTH];
    uint16_t nick_len;
    uint8_t status;
    bool is_online;
};

#define MAX_LINE_HIST 128

/* chat and groupchat window/buffer holder */
struct ChatContext {
    wchar_t line[MAX_STR_SIZE];
    size_t pos;
    size_t len;
    size_t start;    /* the position to start printing line at */

    wchar_t ln_history[MAX_LINE_HIST][MAX_STR_SIZE];  /* history for input lines/commands */
    int hst_pos;
    int hst_tot;

    struct history *hst;
    struct chatlog *log;

    uint8_t self_is_typing;

    WINDOW *history;
    WINDOW *linewin;
    WINDOW *sidebar;

    /* specific for prompt */
    bool at_bottom;    /* true if line end is at bottom of window */
    int orig_y;        /* y axis point of line origin */
};

ToxWindow *init_windows(Tox *m);
void draw_active_window(Tox *m);
int add_window(Tox *m, ToxWindow w);
void del_window(ToxWindow *w);
void set_active_window(int ch);
int get_num_active_windows(void);
void kill_all_windows(void);    /* should only be called on shutdown */
void on_window_resize(int sig);

#endif  /* #define _windows_h */

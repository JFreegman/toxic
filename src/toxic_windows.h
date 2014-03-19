/*  toxic_windows.h
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

#ifndef TOXICVER
#define TOXICVER "NOVER_"    /* Use the -D flag to set this */
#endif

#include <curses.h>
#include <wctype.h>
#include <wchar.h>

#include <tox/tox.h>

#ifdef _SUPPORT_AUDIO
    #include <tox/toxav.h>
#endif /* _SUPPORT_AUDIO */

#define UNKNOWN_NAME "Anonymous"

#define MAX_WINDOWS_NUM 32
#define MAX_FRIENDS_NUM 100
#define MAX_STR_SIZE 256
#define MAX_CMDNAME_SIZE 64
#define KEY_SIZE_BYTES 32
#define TOXIC_MAX_NAME_LENGTH 32   /* Must be <= TOX_MAX_NAME_LENGTH */
#define N_DEFAULT_WINS 2    /* number of permanent default windows */
#define CURS_Y_OFFSET 3    /* y-axis cursor offset for chat contexts */
#define CHATBOX_HEIGHT 4
#define KEY_IDENT_DIGITS 2    /* number of hex digits to display for the pub-key based identifier */

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1

/* ASCII key codes */
#define T_KEY_KILL       0xB      /* ctrl-k */
#define T_KEY_DISCARD    0x15     /* ctrl-u */
#define T_KEY_NEXT       0x10     /* ctrl-p */
#define T_KEY_PREV       0x0F     /* ctrl-o */
#define T_KEY_C_E        0x05     /* ctrl-e */
#define T_KEY_C_A        0x01     /* ctrl-a */

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
};

/* tab alert types: lower types take priority */
enum {
    WINDOW_ALERT_0,
    WINDOW_ALERT_1,
    WINDOW_ALERT_2,
};

/* Fixes text color problem on some terminals. 
   Uncomment if necessary */
/* #define URXVT_FIX */

typedef struct ToxWindow ToxWindow;
typedef struct StatusBar StatusBar;
typedef struct PromptBuf PromptBuf;
typedef struct ChatContext ChatContext;

struct ToxWindow {
    void(*onKey)(ToxWindow *, Tox *, wint_t);
    void(*onDraw)(ToxWindow *, Tox *);
    void(*onInit)(ToxWindow *, Tox *);
    void(*onFriendRequest)(ToxWindow *, Tox *, uint8_t *, uint8_t *, uint16_t);
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
    void(*onGroupNamelistChange)(ToxWindow *, Tox*, int, int, uint8_t);
    void(*onFileSendRequest)(ToxWindow *, Tox *, int32_t, uint8_t, uint64_t, uint8_t *, uint16_t);
    void(*onFileControl)(ToxWindow *, Tox *, int32_t, uint8_t, uint8_t, uint8_t, uint8_t *, uint16_t);
    void(*onFileData)(ToxWindow *, Tox *, int32_t, uint8_t, uint8_t *, uint16_t);
    void(*onTypingChange)(ToxWindow *, Tox *, int32_t, int);

#ifdef _SUPPORT_AUDIO

    void(*onInvite)(ToxWindow *, ToxAv *);
    void(*onRinging)(ToxWindow *, ToxAv *);
    void(*onStarting)(ToxWindow *, ToxAv *);
    void(*onEnding)(ToxWindow *, ToxAv *);
    void(*onError)(ToxWindow *, ToxAv *);
    void(*onStart)(ToxWindow *, ToxAv *);
    void(*onCancel)(ToxWindow *, ToxAv *);
    void(*onReject)(ToxWindow *, ToxAv *);
    void(*onEnd)(ToxWindow *, ToxAv *);
    void(*onRequestTimeout)(ToxWindow *, ToxAv *);
    void(*onPeerTimeout)(ToxWindow *, ToxAv *);
    
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
    PromptBuf *promptbuf;
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

#define LOG_FLUSH_LIMIT 2  /* limits calls to fflush(logfile) to a max of one per LOG_FLUSH_LIMIT seconds */

struct chatlog {
    FILE *file;
    uint64_t lastwrite;
    int pos;
    bool log_on;    /* specific to current chat window */
};

#define MAX_LINE_HIST 128

/* chat and groupchat window/buffer holder */
struct ChatContext {
    wchar_t line[MAX_STR_SIZE];
    size_t pos;
    size_t len;

    wchar_t ln_history[MAX_LINE_HIST][MAX_STR_SIZE];
    int hst_pos;
    int hst_tot;

    bool self_is_typing;

    struct chatlog *log;

    WINDOW *history;
    WINDOW *linewin;
    WINDOW *sidebar;
};

/* prompt window/buffer holder */
struct PromptBuf {
    wchar_t line[MAX_STR_SIZE];
    size_t pos;
    size_t len;

    bool at_bottom;    /* true if line end is at bottom of window */
    int orig_y;        /* y axis point of line origin */
    bool scroll;       /* used for prompt window hack to determine when to scroll down */

    wchar_t ln_history[MAX_LINE_HIST][MAX_STR_SIZE];
    int hst_pos;
    int hst_tot;

    struct chatlog *log;
    WINDOW *linewin;
};

/* Start file transfer code */

#define MAX_FILES 256
#define FILE_PIECE_SIZE 1024
#define TIMEOUT_FILESENDER 300

typedef struct {
    FILE *file;
    ToxWindow *toxwin;
    int32_t friendnum;
    bool active;
    int filenum;
    uint8_t nextpiece[FILE_PIECE_SIZE];
    uint16_t piecelen;
    uint8_t pathname[MAX_STR_SIZE];
    uint64_t timestamp;
} FileSender;

struct FileReceiver {
    uint8_t filenames[MAX_FILES][MAX_STR_SIZE];
    FILE *files[MAX_FILES];
    bool pending[MAX_FILES];
};

/* End file transfer code */

struct _Winthread {
    pthread_t tid;
    pthread_mutex_t lock;
};

void on_request(Tox *m, uint8_t *public_key, uint8_t *data, uint16_t length, void *userdata);
void on_connectionchange(Tox *m, int32_t friendnumber, uint8_t status, void *userdata);
void on_message(Tox *m, int32_t friendnumber, uint8_t *string, uint16_t length, void *userdata);
void on_action(Tox *m, int32_t friendnumber, uint8_t *string, uint16_t length, void *userdata);
void on_nickchange(Tox *m, int32_t friendnumber, uint8_t *string, uint16_t length, void *userdata);
void on_statuschange(Tox *m, int32_t friendnumber, uint8_t status, void *userdata);
void on_statusmessagechange(Tox *m, int32_t friendnumber, uint8_t *string, uint16_t length, void *userdata);
void on_friendadded(Tox *m, int32_t friendnumber, bool sort);
void on_groupmessage(Tox *m, int groupnumber, int peernumber, uint8_t *message, uint16_t length, void *userdata);
void on_groupaction(Tox *m, int groupnumber, int peernumber, uint8_t *action, uint16_t length, void *userdata);
void on_groupinvite(Tox *m, int32_t friendnumber, uint8_t *group_pub_key, void *userdata);
void on_group_namelistchange(Tox *m, int groupnumber, int peernumber, uint8_t change, void *userdata);
void on_file_sendrequest(Tox *m, int32_t friendnumber, uint8_t filenumber, uint64_t filesize, uint8_t *pathname, uint16_t pathname_length, void *userdata);
void on_file_control(Tox *m, int32_t friendnumber, uint8_t receive_send, uint8_t filenumber, uint8_t control_type, uint8_t *data, uint16_t length, void *userdata);
void on_file_data(Tox *m, int32_t friendnumber, uint8_t filenumber, uint8_t *data, uint16_t length, void *userdata);
void on_typing_change(Tox *m, int32_t friendnumber, int is_typing, void *userdata);

ToxWindow *init_windows(Tox *m);
void draw_active_window(Tox *m);
int add_window(Tox *m, ToxWindow w);
void del_window(ToxWindow *w);
void set_active_window(int ch);
int get_num_active_windows(void);

/* cleans up all chat and groupchat windows (should only be called on shutdown) */
void kill_all_windows(void);
#endif

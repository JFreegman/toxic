/*
 * Toxic -- Tox Curses Client
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

#define MAX_WINDOWS_NUM 32
#define MAX_FRIENDS_NUM 100
#define MAX_GROUPCHAT_NUM MAX_WINDOWS_NUM - N_DEFAULT_WINS
#define MAX_STR_SIZE 256
#define KEY_SIZE_BYTES 32
#define TOXIC_MAX_NAME_LENGTH 30   /* Must be <= TOX_MAX_NAME_LENGTH */
#define N_DEFAULT_WINS 2    /* number of permanent default windows */
#define UNKNOWN_NAME "Unknown"
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define CURS_Y_OFFSET 3    /* y-axis cursor offset for chat contexts */

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

/* Fixes text color problem on some terminals. 
   Uncomment if necessary */
//#define URXVT_FIX

typedef struct ToxWindow_ ToxWindow;

struct ToxWindow_ {
    void(*onKey)(ToxWindow *, Tox *, wint_t);
    void(*onDraw)(ToxWindow *, Tox *);
    void(*onInit)(ToxWindow *, Tox *);
    void(*onFriendRequest)(ToxWindow *, uint8_t *, uint8_t *, uint16_t);
    void(*onConnectionChange)(ToxWindow *, Tox *, int, uint8_t);
    void(*onMessage)(ToxWindow *, Tox *, int, uint8_t *, uint16_t);
    void(*onNickChange)(ToxWindow *, int, uint8_t *, uint16_t);
    void(*onStatusChange)(ToxWindow *, Tox *, int, TOX_USERSTATUS);
    void(*onStatusMessageChange)(ToxWindow *, int, uint8_t *, uint16_t);
    void(*onAction)(ToxWindow *, Tox *, int, uint8_t *, uint16_t);
    void(*onGroupMessage)(ToxWindow *, Tox *, int, int, uint8_t *, uint16_t);
    void(*onGroupInvite)(ToxWindow *, Tox *, int, uint8_t *);
    void(*onFileSendRequest)(ToxWindow *, Tox *, int, uint8_t, uint64_t, uint8_t *, uint16_t);
    void(*onFileControl)(ToxWindow *, Tox *, int, uint8_t, uint8_t, uint8_t, uint8_t *, uint16_t);
    void(*onFileData)(ToxWindow *, Tox *, int, uint8_t, uint8_t *, uint16_t);

    char name[TOX_MAX_NAME_LENGTH];
    int num;
    int x;

    void *chatwin;
    void *stb;
    void *prompt;

    bool blink;

    WINDOW *window;
};

typedef struct {
    WINDOW *topline;    
    uint8_t statusmsg[TOX_MAX_STATUSMESSAGE_LENGTH];
    uint16_t statusmsg_len;
    uint8_t nick[TOX_MAX_NAME_LENGTH];
    uint16_t nick_len;
    TOX_USERSTATUS status;
    bool is_online;
} StatusBar;

typedef struct {
    wchar_t line[MAX_STR_SIZE];
    size_t pos;
    WINDOW *history;
    WINDOW *linewin;
} ChatContext;

typedef struct {
    int chatwin;
    bool active;
} GroupChat;

/* Start file transfer code */

#define MAX_FILES 256
#define FILE_PIECE_SIZE 1024

typedef struct {
    FILE *file;
    WINDOW *chatwin;
    int friendnum;
    bool active;
    uint8_t filenum;
    uint8_t nextpiece[FILE_PIECE_SIZE];
    uint16_t piecelen;
    uint8_t pathname[MAX_STR_SIZE];
} FileSender;

FileSender file_senders[MAX_FILES];
uint8_t num_file_senders;

typedef struct {
  uint8_t filenames[MAX_FILES][MAX_STR_SIZE];
  bool pending[MAX_FILES];
} FileReceiver;

/* End file transfer code */

typedef struct {
    uint8_t name[TOX_MAX_NAME_LENGTH];
    uint16_t namelength;
    uint8_t statusmsg[TOX_MAX_STATUSMESSAGE_LENGTH];
    uint16_t statusmsg_len;
    int num;
    int chatwin;
    bool active;
    bool online;
    TOX_USERSTATUS status;
    FileReceiver file_receiver;
} friend_t;

friend_t friends[MAX_FRIENDS_NUM];

void on_request(uint8_t *public_key, uint8_t *data, uint16_t length, void *userdata);
void on_connectionchange(Tox *m, int friendnumber, uint8_t status, void *userdata);
void on_message(Tox *m, int friendnumber, uint8_t *string, uint16_t length, void *userdata);
void on_action(Tox *m, int friendnumber, uint8_t *string, uint16_t length, void *userdata);
void on_nickchange(Tox *m, int friendnumber, uint8_t *string, uint16_t length, void *userdata);
void on_statuschange(Tox *m, int friendnumber, TOX_USERSTATUS status, void *userdata);
void on_statusmessagechange(Tox *m, int friendnumber, uint8_t *string, uint16_t length, void *userdata);
void on_friendadded(Tox *m, int friendnumber);
void on_groupmessage(Tox *m, int groupnumber, int peernumber, uint8_t *message, uint16_t length, void *userdata);
void on_groupinvite(Tox *m, int friendnumber, uint8_t *group_pub_key, void *userdata);
void on_file_sendrequest(Tox *m, int friendnumber, uint8_t filenumber, uint64_t filesize, uint8_t *pathname, uint16_t pathname_length, void *userdata);
void on_file_control(Tox *m, int friendnumber, uint8_t receive_send, uint8_t filenumber, uint8_t control_type, uint8_t *data, uint16_t length, void *userdata);
void on_file_data(Tox *m, int friendnumber, uint8_t filenumber, uint8_t *data, uint16_t length, void *userdata);

ToxWindow *init_windows();
void draw_active_window(Tox *m);
int add_window(Tox *m, ToxWindow w);
void del_window(ToxWindow *w);
void set_active_window(int ch);
#endif

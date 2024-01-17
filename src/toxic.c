/*  toxic.c
 *
 *
 *  Copyright (C) 2024 Toxic All Rights Reserved.
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

#include <ctype.h>
#include <curses.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <locale.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include <curl/curl.h>
#include <tox/toxencryptsave.h>
#include <tox/tox.h>

#include "audio_device.h"
#include "bootstrap.h"
#include "conference.h"
#include "configdir.h"
#include "execute.h"
#include "file_transfers.h"
#include "friendlist.h"
#include "groupchats.h"
#include "line_info.h"
#include "log.h"
#include "message_queue.h"
#include "misc_tools.h"
#include "name_lookup.h"
#include "notify.h"
#include "prompt.h"
#include "settings.h"
#include "term_mplex.h"
#include "toxic.h"
#include "windows.h"

#ifdef X11
#include "x11focus.h"
#endif

#ifdef AUDIO
#include "audio_call.h"
#ifdef VIDEO
#include "video_call.h"
#endif /* VIDEO */
static ToxAV *av;
#endif /* AUDIO */

#ifdef PYTHON
#include "api.h"
#include "python_api.h"
#endif

#ifndef PACKAGE_DATADIR
#define PACKAGE_DATADIR "."
#endif

/* Export for use in Callbacks */
char *DATA_FILE = NULL;
char *BLOCK_FILE = NULL;
ToxWindow *prompt = NULL;

#define DATANAME  "toxic_profile.tox"
#define BLOCKNAME "toxic_blocklist"

#define MIN_PASSWORD_LEN 6
#define MAX_PASSWORD_LEN 64

struct Winthread Winthread;
static struct cqueue_thread cqueue_thread;
struct arg_opts arg_opts;

#ifdef AUDIO
static struct av_thread av_thread;
#endif

typedef struct Toxic {
    Tox *tox;
} Toxic;

// This struct is not thread safe. It should only ever be written to from the main thread
// before any other thread that uses it is initialized.
struct user_settings *user_settings = NULL;

static struct user_password {
    bool data_is_encrypted;
    char pass[MAX_PASSWORD_LEN + 1];
    int len;
} user_password;

static time_t last_signal_time;

static void catch_SIGINT(int sig)
{
    UNUSED_VAR(sig);

    time_t cur_time = get_unix_time();

    if (difftime(cur_time, last_signal_time) <= 1) {
        Winthread.sig_exit_toxic = 1;
    } else {
        last_signal_time = cur_time;
    }
}

static void catch_SIGSEGV(int sig)
{
    UNUSED_VAR(sig);

    if (freopen("/dev/tty", "w", stderr)) {    // make sure stderr is enabled since we may have disabled it
        fprintf(stderr, "Caught SIGSEGV: Aborting toxic session.\n");
    }

    endwin();

    exit(EXIT_FAILURE);
}

static void flag_window_resize(int sig)
{
    UNUSED_VAR(sig);

    Winthread.flag_resize = 1;
}

static void init_signal_catchers(void)
{
    signal(SIGWINCH, flag_window_resize);
    signal(SIGINT, catch_SIGINT);
    signal(SIGSEGV, catch_SIGSEGV);
}

void free_global_data(void)
{
    if (DATA_FILE) {
        free(DATA_FILE);
        DATA_FILE = NULL;
    }

    if (BLOCK_FILE) {
        free(BLOCK_FILE);
        BLOCK_FILE = NULL;
    }

    if (user_settings) {
        free(user_settings);
        user_settings = NULL;
    }
}

void exit_toxic_success(Tox *tox)
{
    store_data(tox, DATA_FILE);

    user_password = (struct user_password) {
        0
    };

    terminate_notify();

    kill_all_file_transfers(tox);
    kill_all_windows(tox);

#ifdef AUDIO
#ifdef VIDEO
    terminate_video();
#endif /* VIDEO */
    terminate_audio();
#endif /* AUDIO */

#ifdef PYTHON
    terminate_python();
#endif /* PYTHON */

    free_global_data();
    tox_kill(tox);

    if (arg_opts.log_fp != NULL) {
        fclose(arg_opts.log_fp);
        arg_opts.log_fp = NULL;
    }

    endwin();
    curl_global_cleanup();

#ifdef X11
    terminate_x11focus();
#endif /* X11 */

    exit(EXIT_SUCCESS);
}

void exit_toxic_err(const char *errmsg, int errcode)
{
    free_global_data();

    if (freopen("/dev/tty", "w", stderr)) {
        fprintf(stderr, "Toxic session aborted with error code %d (%s)\n", errcode, errmsg);
    }

    endwin();
    exit(EXIT_FAILURE);
}

static const char *tox_log_level_show(Tox_Log_Level level)
{
    switch (level) {
        case TOX_LOG_LEVEL_TRACE:
            return "TRACE";

        case TOX_LOG_LEVEL_DEBUG:
            return "DEBUG";

        case TOX_LOG_LEVEL_INFO:
            return "INFO";

        case TOX_LOG_LEVEL_WARNING:
            return "WARNING";

        case TOX_LOG_LEVEL_ERROR:
            return "ERROR";
    }

    return "<invalid>";
}

void cb_toxcore_logger(Tox *tox, TOX_LOG_LEVEL level, const char *file, uint32_t line, const char *func,
                       const char *message, void *user_data)
{
    UNUSED_VAR(tox);

    FILE *fp = (FILE *)user_data;

    if (!fp) {
        fp = stderr;
    }

    struct timeval tv;

    gettimeofday(&tv, NULL);

    struct tm tmp;

    gmtime_r(&tv.tv_sec, &tmp);

    char timestamp[200];

    strftime(timestamp, sizeof(timestamp), "%F %T", &tmp);

    fprintf(fp, "%c %s.%06ld %s:%u(%s) - %s\n", tox_log_level_show(level)[0], timestamp, tv.tv_usec, file, line, func,
            message);

    fflush(fp);
}

/* Sets ncurses refresh rate. Lower values make it refresh more often. */
void set_window_refresh_rate(size_t refresh_rate)
{
    timeout(refresh_rate);
}

static void init_term(void)
{
#if HAVE_WIDECHAR

    if (!arg_opts.default_locale) {
        if (setlocale(LC_ALL, "") == NULL)
            exit_toxic_err("Could not set your locale, please check your locale settings or "
                           "disable unicode support with the -d flag.", FATALERR_LOCALE_NOT_SET);
    }

#endif

    initscr();
    cbreak();
    keypad(stdscr, 1);
    noecho();
    nonl();
    set_window_refresh_rate(NCURSES_DEFAULT_REFRESH_RATE);

    if (has_colors()) {
        short bg_color = COLOR_BLACK;
        short bar_bg_color = COLOR_BLUE;
        short bar_fg_color = COLOR_WHITE;
        short bar_accent_color = COLOR_CYAN;
        short bar_notify_color = COLOR_YELLOW;
        start_color();

        if (user_settings->colour_theme == NATIVE_COLS) {
            if (assume_default_colors(-1, -1) == OK) {
                bg_color = -1;
            }
        }

        if (!string_is_empty(user_settings->color_bar_bg)) {
            if (strcmp(user_settings->color_bar_bg, "black") == 0) {
                bar_bg_color = COLOR_BLACK;
            } else if (strcmp(user_settings->color_bar_bg, "red") == 0) {
                bar_bg_color = COLOR_RED;
            } else if (strcmp(user_settings->color_bar_bg, "blue") == 0) {
                bar_bg_color = COLOR_BLUE;
            } else if (strcmp(user_settings->color_bar_bg, "cyan") == 0) {
                bar_bg_color = COLOR_CYAN;
            } else if (strcmp(user_settings->color_bar_bg, "green") == 0) {
                bar_bg_color = COLOR_GREEN;
            } else if (strcmp(user_settings->color_bar_bg, "yellow") == 0) {
                bar_bg_color = COLOR_YELLOW;
            } else if (strcmp(user_settings->color_bar_bg, "magenta") == 0) {
                bar_bg_color = COLOR_MAGENTA;
            } else if (strcmp(user_settings->color_bar_bg, "white") == 0) {
                bar_bg_color = COLOR_WHITE;
            } else {
                bar_bg_color = COLOR_BLUE;
            }
        } else {
            bar_bg_color = COLOR_BLUE;
        }

        if (!string_is_empty(user_settings->color_bar_fg)) {
            if (strcmp(user_settings->color_bar_fg, "black") == 0) {
                bar_fg_color = COLOR_BLACK;
            } else if (strcmp(user_settings->color_bar_fg, "red") == 0) {
                bar_fg_color = COLOR_RED;
            } else if (strcmp(user_settings->color_bar_fg, "blue") == 0) {
                bar_fg_color = COLOR_BLUE;
            } else if (strcmp(user_settings->color_bar_fg, "cyan") == 0) {
                bar_fg_color = COLOR_CYAN;
            } else if (strcmp(user_settings->color_bar_fg, "green") == 0) {
                bar_fg_color = COLOR_GREEN;
            } else if (strcmp(user_settings->color_bar_fg, "yellow") == 0) {
                bar_fg_color = COLOR_YELLOW;
            } else if (strcmp(user_settings->color_bar_fg, "magenta") == 0) {
                bar_fg_color = COLOR_MAGENTA;
            } else if (strcmp(user_settings->color_bar_fg, "white") == 0) {
                bar_fg_color = COLOR_WHITE;
            } else {
                bar_fg_color = COLOR_WHITE;
            }
        } else {
            bar_fg_color = COLOR_WHITE;
        }

        if (!string_is_empty(user_settings->color_bar_accent)) {
            if (strcmp(user_settings->color_bar_accent, "black") == 0) {
                bar_accent_color = COLOR_BLACK;
            } else if (strcmp(user_settings->color_bar_accent, "red") == 0) {
                bar_accent_color = COLOR_RED;
            } else if (strcmp(user_settings->color_bar_accent, "blue") == 0) {
                bar_accent_color = COLOR_BLUE;
            } else if (strcmp(user_settings->color_bar_accent, "cyan") == 0) {
                bar_accent_color = COLOR_CYAN;
            } else if (strcmp(user_settings->color_bar_accent, "green") == 0) {
                bar_accent_color = COLOR_GREEN;
            } else if (strcmp(user_settings->color_bar_accent, "yellow") == 0) {
                bar_accent_color = COLOR_YELLOW;
            } else if (strcmp(user_settings->color_bar_accent, "magenta") == 0) {
                bar_accent_color = COLOR_MAGENTA;
            } else if (strcmp(user_settings->color_bar_accent, "white") == 0) {
                bar_accent_color = COLOR_WHITE;
            } else {
                bar_accent_color = COLOR_CYAN;
            }
        } else {
            bar_accent_color = COLOR_CYAN;
        }

        if (!string_is_empty(user_settings->color_bar_notify)) {
            if (strcmp(user_settings->color_bar_notify, "black") == 0) {
                bar_notify_color = COLOR_BLACK;
            } else if (strcmp(user_settings->color_bar_notify, "red") == 0) {
                bar_notify_color = COLOR_RED;
            } else if (strcmp(user_settings->color_bar_notify, "blue") == 0) {
                bar_notify_color = COLOR_BLUE;
            } else if (strcmp(user_settings->color_bar_notify, "cyan") == 0) {
                bar_notify_color = COLOR_CYAN;
            } else if (strcmp(user_settings->color_bar_notify, "green") == 0) {
                bar_notify_color = COLOR_GREEN;
            } else if (strcmp(user_settings->color_bar_notify, "yellow") == 0) {
                bar_notify_color = COLOR_YELLOW;
            } else if (strcmp(user_settings->color_bar_notify, "magenta") == 0) {
                bar_notify_color = COLOR_MAGENTA;
            } else if (strcmp(user_settings->color_bar_notify, "white") == 0) {
                bar_notify_color = COLOR_WHITE;
            } else {
                bar_notify_color = COLOR_YELLOW;
            }
        } else {
            bar_notify_color = COLOR_YELLOW;
        }

        init_pair(WHITE, COLOR_WHITE, COLOR_BLACK);
        init_pair(GREEN, COLOR_GREEN, bg_color);
        init_pair(CYAN, COLOR_CYAN, bg_color);
        init_pair(RED, COLOR_RED, bg_color);
        init_pair(BLUE, COLOR_BLUE, bg_color);
        init_pair(YELLOW, COLOR_YELLOW, bg_color);
        init_pair(MAGENTA, COLOR_MAGENTA, bg_color);
        init_pair(BLACK, COLOR_BLACK, COLOR_BLACK);
        init_pair(WHITE_BLUE, COLOR_WHITE, COLOR_BLUE);
        init_pair(BLACK_WHITE, COLOR_BLACK, COLOR_WHITE);
        init_pair(WHITE_BLACK, COLOR_WHITE, COLOR_BLACK);
        init_pair(WHITE_GREEN, COLOR_WHITE, COLOR_GREEN);
        init_pair(BLACK_BG, COLOR_BLACK, bar_bg_color);
        init_pair(PURPLE_BG, COLOR_MAGENTA, bar_bg_color);
        init_pair(BAR_TEXT, bar_fg_color, bar_bg_color);
        init_pair(BAR_SOLID, bar_bg_color, bar_bg_color);
        init_pair(BAR_ACCENT, bar_accent_color, bar_bg_color);
        init_pair(BAR_NOTIFY, bar_notify_color, bar_bg_color);
        init_pair(STATUS_ONLINE, COLOR_GREEN, bar_bg_color);
        init_pair(STATUS_AWAY, COLOR_YELLOW, bar_bg_color);
        init_pair(STATUS_BUSY, COLOR_RED, bar_bg_color);
        init_pair(WHITE_BAR_FG, COLOR_WHITE, bar_bg_color);
        init_pair(RED_BAR_FG, COLOR_RED, bar_bg_color);
        init_pair(GREEN_BAR_FG, COLOR_GREEN, bar_bg_color);
        init_pair(CYAN_BAR_FG, COLOR_CYAN, bar_bg_color);
        init_pair(PURPLE_BAR_FG, COLOR_MAGENTA, bar_bg_color);
        init_pair(YELLOW_BAR_FG, COLOR_YELLOW, bar_bg_color);
        init_pair(BLACK_BAR_FG, COLOR_BLACK, bar_bg_color);
    }

    refresh();
}

static struct _init_messages {
    char **msgs;
    int num;
} init_messages;

/* One-time queue for messages created during init. Do not use after program init. */
static void queue_init_message(const char *msg, ...)
{
    char frmt_msg[MAX_STR_SIZE] = {0};

    va_list args;
    va_start(args, msg);
    vsnprintf(frmt_msg, sizeof(frmt_msg), msg, args);
    va_end(args);

    int i = init_messages.num;
    ++init_messages.num;

    char **new_msgs = realloc(init_messages.msgs, sizeof(char *) * init_messages.num);

    if (new_msgs == NULL) {
        exit_toxic_err("Failed in queue_init_message", FATALERR_MEMORY);
    }

    new_msgs[i] = malloc(MAX_STR_SIZE);

    if (new_msgs[i] == NULL) {
        exit_toxic_err("Failed in queue_init_message", FATALERR_MEMORY);
    }

    snprintf(new_msgs[i], MAX_STR_SIZE, "%s", frmt_msg);
    init_messages.msgs = new_msgs;
}

/* called after messages have been printed to prompt and are no longer needed */
static void cleanup_init_messages(void)
{
    if (init_messages.num <= 0) {
        return;
    }

    for (int i = 0; i < init_messages.num; ++i) {
        free(init_messages.msgs[i]);
    }

    free(init_messages.msgs);
}

static void print_init_messages(ToxWindow *toxwin)
{
    for (int i = 0; i < init_messages.num; ++i) {
        line_info_add(toxwin, NULL, NULL, NULL, SYS_MSG, 0, 0, init_messages.msgs[i]);
    }
}

static void load_friendlist(Tox *tox)
{
    size_t numfriends = tox_self_get_friend_list_size(tox);

    for (size_t i = 0; i < numfriends; ++i) {
        friendlist_onFriendAdded(NULL, tox, i, false);
    }

    sort_friendlist_index();
}

static void load_groups(Tox *tox)
{
    size_t numgroups = tox_group_get_number_groups(tox);

    for (size_t i = 0; i < numgroups; ++i) {
        if (init_groupchat_win(tox, i, NULL, 0, Group_Join_Type_Load) != 0) {
            tox_group_leave(tox, i, NULL, 0, NULL);
        }
    }
}

static void load_conferences(Tox *tox)
{
    size_t num_chats = tox_conference_get_chatlist_size(tox);

    if (num_chats == 0) {
        return;
    }

    uint32_t *chatlist = malloc(num_chats * sizeof(uint32_t));

    if (chatlist == NULL) {
        fprintf(stderr, "malloc() failed in load_conferences()\n");
        return;
    }

    tox_conference_get_chatlist(tox, chatlist);

    for (size_t i = 0; i < num_chats; ++i) {
        uint32_t conferencenum = chatlist[i];

        if (get_num_active_windows() >= MAX_WINDOWS_NUM) {
            tox_conference_delete(tox, conferencenum, NULL);
            continue;
        }

        Tox_Err_Conference_Get_Type err;
        Tox_Conference_Type type = tox_conference_get_type(tox, conferencenum, &err);

        if (err != TOX_ERR_CONFERENCE_GET_TYPE_OK) {
            tox_conference_delete(tox, conferencenum, NULL);
            continue;
        }

        Tox_Err_Conference_Title t_err;
        size_t length = tox_conference_get_title_size(tox, conferencenum, &t_err);
        uint8_t title[MAX_STR_SIZE];

        if (t_err != TOX_ERR_CONFERENCE_TITLE_OK || length >= sizeof(title)) {
            length = 0;
        } else {
            tox_conference_get_title(tox, conferencenum, title, &t_err);

            if (t_err != TOX_ERR_CONFERENCE_TITLE_OK) {
                length = 0;
            }
        }

        title[length] = 0;

        int win_idx = init_conference_win(tox, conferencenum, type, (const char *) title, length);

        if (win_idx == -1) {
            tox_conference_delete(tox, conferencenum, NULL);
            continue;
        }

        if (type == TOX_CONFERENCE_TYPE_AV) {
            line_info_add(get_window_ptr(win_idx), NULL, NULL, NULL, SYS_MSG, 0, 0,
#ifdef AUDIO
                          "Use \"/audio on\" to enable audio in this conference."
#else
                          "Audio support disabled by compile-time option."
#endif
                         );
        }
    }

    free(chatlist);
}

/* return length of password on success, 0 on failure */
static int password_prompt(char *buf, int size)
{
    buf[0] = '\0';

    /* disable terminal echo */
    struct termios oflags, nflags;
    tcgetattr(fileno(stdin), &oflags);
    nflags = oflags;
    nflags.c_lflag &= ~ECHO;
    nflags.c_lflag |= ECHONL;

    if (tcsetattr(fileno(stdin), TCSANOW, &nflags) != 0) {
        return 0;
    }

    const char *p = fgets(buf, size, stdin);

    /* re-enable terminal echo */
    tcsetattr(fileno(stdin), TCSANOW, &oflags);

    if (p == NULL) {
        return 0;
    }

    size_t len = strlen(buf);

    if (len <= 1) {
        return 0;
    }

    /* eat overflowed stdin and return error */
    if (buf[--len] != '\n') {
        int ch;

        while ((ch = getchar()) != '\n' && ch > 0) {
        }

        return 0;
    }

    buf[len] = '\0';
    return len;
}

/* Get the password from the eval command.
 * return length of password on success, 0 on failure
 */
static int password_eval(char *buf, int size)
{
    buf[0] = '\0';

    /* Run password_eval command */
    FILE *f = popen(user_settings->password_eval, "r");

    if (f == NULL) {
        fprintf(stderr, "Executing password_eval failed\n");
        return 0;
    }

    /* Get output from command */
    char *ret = fgets(buf, size, f);

    if (ret == NULL) {
        fprintf(stderr, "Reading password from password_eval command failed\n");
        pclose(f);
        return 0;
    }

    /* Get exit status */
    int status = pclose(f);

    if (status != 0) {
        fprintf(stderr, "password_eval command returned error %d\n", status);
        return 0;
    }

    /* Removez whitespace or \n at end */
    int i, len = strlen(buf);

    for (i = len - 1; i > 0 && isspace(buf[i]); i--) {
        buf[i] = 0;
        len--;
    }

    return len;
}

/* Ask user if they would like to encrypt the data file and set password */
static void first_time_encrypt(const char *msg)
{
    char ch[256] = {0};

    do {
        clear_screen();
        printf("%s ", msg);
        fflush(stdout);

        if (!strcasecmp(ch, "y\n") || !strcasecmp(ch, "n\n") || !strcasecmp(ch, "yes\n")
                || !strcasecmp(ch, "no\n") || !strcasecmp(ch, "q\n")) {
            break;
        }

    } while (fgets(ch, sizeof(ch), stdin));

    printf("\n");

    if (ch[0] == 'q' || ch[0] == 'Q') {
        exit(0);
    }

    if (ch[0] == 'y' || ch[0] == 'Y') {
        int len = 0;
        bool valid_password = false;
        char passconfirm[MAX_PASSWORD_LEN + 1] = {0};

        printf("Enter a new password (must be at least %d characters) ", MIN_PASSWORD_LEN);

        while (valid_password == false) {
            fflush(stdout); // Flush all before user input
            len = password_prompt(user_password.pass, sizeof(user_password.pass));
            user_password.len = len;

            if (strcasecmp(user_password.pass, "q") == 0) {
                exit(0);
            }

            if (string_is_empty(passconfirm) && (len < MIN_PASSWORD_LEN || len > MAX_PASSWORD_LEN)) {
                printf("Password must be between %d and %d characters long. ", MIN_PASSWORD_LEN, MAX_PASSWORD_LEN);
                continue;
            }

            if (string_is_empty(passconfirm)) {
                printf("Enter password again ");
                snprintf(passconfirm, sizeof(passconfirm), "%s", user_password.pass);
                continue;
            }

            if (strcmp(user_password.pass, passconfirm) != 0) {
                memset(passconfirm, 0, sizeof(passconfirm));
                memset(user_password.pass, 0, sizeof(user_password.pass));
                printf("Passwords don't match. Try again. ");
                continue;
            }

            valid_password = true;
        }

        queue_init_message("Data file '%s' is encrypted", DATA_FILE);
        memset(passconfirm, 0, sizeof(passconfirm));
        user_password.data_is_encrypted = true;
    }

    clear_screen();
}

/* Store Tox profile data to path.
 *
 * Return 0 if stored successfully.
 * Return -1 on error.
 */
#define TEMP_PROFILE_EXT ".tmp"
int store_data(Tox *tox, const char *path)
{
    if (path == NULL) {
        return -1;
    }

    size_t temp_buf_size = strlen(path) + strlen(TEMP_PROFILE_EXT) + 1;
    char *temp_path = malloc(temp_buf_size);

    if (temp_path == NULL) {
        return -1;
    }

    snprintf(temp_path, temp_buf_size, "%s%s", path, TEMP_PROFILE_EXT);

    FILE *fp = fopen(temp_path, "wb");

    if (fp == NULL) {
        free(temp_path);
        return -1;
    }

    size_t data_len = tox_get_savedata_size(tox);
    char *data = malloc(data_len * sizeof(char));

    if (data == NULL) {
        free(temp_path);
        fclose(fp);
        return -1;
    }

    tox_get_savedata(tox, (uint8_t *) data);

    if (user_password.data_is_encrypted && !arg_opts.unencrypt_data) {
        size_t enc_len = data_len + TOX_PASS_ENCRYPTION_EXTRA_LENGTH;
        char *enc_data = malloc(enc_len * sizeof(char));

        if (enc_data == NULL) {
            fclose(fp);
            free(temp_path);
            free(data);
            return -1;
        }

        Tox_Err_Encryption err;
        tox_pass_encrypt((uint8_t *) data, data_len, (uint8_t *) user_password.pass, user_password.len,
                         (uint8_t *) enc_data, &err);

        if (err != TOX_ERR_ENCRYPTION_OK) {
            fprintf(stderr, "tox_pass_encrypt() failed with error %d\n", err);
            fclose(fp);
            free(temp_path);
            free(data);
            free(enc_data);
            return -1;
        }

        if (fwrite(enc_data, enc_len, 1, fp) != 1) {
            fprintf(stderr, "Failed to write profile data.\n");
            fclose(fp);
            free(temp_path);
            free(data);
            free(enc_data);
            return -1;
        }

        free(enc_data);
    } else {  /* data will not be encrypted */
        if (fwrite(data, data_len, 1, fp) != 1) {
            fprintf(stderr, "Failed to write profile data.\n");
            fclose(fp);
            free(temp_path);
            free(data);
            return -1;
        }
    }

    fclose(fp);
    free(data);

    if (rename(temp_path, path) != 0) {
        free(temp_path);
        return -1;
    }

    free(temp_path);

    return 0;
}

static void init_tox_callbacks(Tox *tox)
{
    tox_callback_self_connection_status(tox, on_self_connection_status);
    tox_callback_friend_connection_status(tox, on_friend_connection_status);
    tox_callback_friend_typing(tox, on_friend_typing);
    tox_callback_friend_request(tox, on_friend_request);
    tox_callback_friend_message(tox, on_friend_message);
    tox_callback_friend_name(tox, on_friend_name);
    tox_callback_friend_status(tox, on_friend_status);
    tox_callback_friend_status_message(tox, on_friend_status_message);
    tox_callback_friend_read_receipt(tox, on_friend_read_receipt);
    tox_callback_conference_invite(tox, on_conference_invite);
    tox_callback_conference_message(tox, on_conference_message);
    tox_callback_conference_peer_list_changed(tox, on_conference_peer_list_changed);
    tox_callback_conference_peer_name(tox, on_conference_peer_name);
    tox_callback_conference_title(tox, on_conference_title);
    tox_callback_file_recv(tox, on_file_recv);
    tox_callback_file_chunk_request(tox, on_file_chunk_request);
    tox_callback_file_recv_control(tox, on_file_recv_control);
    tox_callback_file_recv_chunk(tox, on_file_recv_chunk);
    tox_callback_friend_lossless_packet(tox, on_lossless_custom_packet);
    tox_callback_group_invite(tox, on_group_invite);
    tox_callback_group_message(tox, on_group_message);
    tox_callback_group_private_message(tox, on_group_private_message);
    tox_callback_group_peer_status(tox, on_group_status_change);
    tox_callback_group_peer_join(tox, on_group_peer_join);
    tox_callback_group_peer_exit(tox, on_group_peer_exit);
    tox_callback_group_peer_name(tox, on_group_nick_change);
    tox_callback_group_topic(tox, on_group_topic_change);
    tox_callback_group_peer_limit(tox, on_group_peer_limit);
    tox_callback_group_privacy_state(tox, on_group_privacy_state);
    tox_callback_group_topic_lock(tox, on_group_topic_lock);
    tox_callback_group_password(tox, on_group_password);
    tox_callback_group_self_join(tox, on_group_self_join);
    tox_callback_group_join_fail(tox, on_group_rejected);
    tox_callback_group_moderation(tox, on_group_moderation);
    tox_callback_group_voice_state(tox, on_group_voice_state);
}

static void init_tox_options(struct Tox_Options *tox_opts)
{
    tox_options_default(tox_opts);

    tox_options_set_ipv6_enabled(tox_opts, !arg_opts.use_ipv4);
    tox_options_set_udp_enabled(tox_opts, !arg_opts.force_tcp);
    tox_options_set_proxy_type(tox_opts, arg_opts.proxy_type);
    tox_options_set_tcp_port(tox_opts, arg_opts.tcp_port);
    tox_options_set_local_discovery_enabled(tox_opts, !arg_opts.disable_local_discovery);

    if (arg_opts.logging) {
        tox_options_set_log_callback(tox_opts, cb_toxcore_logger);

        if (arg_opts.log_fp != NULL) {
            tox_options_set_log_user_data(tox_opts, arg_opts.log_fp);
        }
    }

    if (!tox_options_get_ipv6_enabled(tox_opts)) {
        queue_init_message("Forcing IPv4 connection");
    }

    if (tox_options_get_tcp_port(tox_opts)) {
        queue_init_message("TCP relaying enabled on port %d", tox_options_get_tcp_port(tox_opts));
    }

    if (tox_options_get_proxy_type(tox_opts) != TOX_PROXY_TYPE_NONE) {
        tox_options_set_proxy_port(tox_opts, arg_opts.proxy_port);
        tox_options_set_proxy_host(tox_opts, arg_opts.proxy_address);
        const char *ps = tox_options_get_proxy_type(tox_opts) == TOX_PROXY_TYPE_SOCKS5 ? "SOCKS5" : "HTTP";

        char tmp[sizeof(arg_opts.proxy_address) + MAX_STR_SIZE];
        snprintf(tmp, sizeof(tmp), "Using %s proxy %s : %d", ps, arg_opts.proxy_address, arg_opts.proxy_port);
        queue_init_message("%s", tmp);
    }

    if (!tox_options_get_udp_enabled(tox_opts)) {
        queue_init_message("UDP disabled");
    } else if (tox_options_get_proxy_type(tox_opts) != TOX_PROXY_TYPE_NONE) {
        const char *msg = "WARNING: Using a proxy without disabling UDP may leak your real IP address.";
        queue_init_message("%s", msg);
        msg = "Use the -t option to disable UDP.";
        queue_init_message("%s", msg);
    }
}

/* Returns a new Tox object on success.
 * If object fails to initialize the toxic process will terminate.
 */
static Tox *load_tox(char *data_path, struct Tox_Options *tox_opts, Tox_Err_New *new_err)
{
    Tox *tox = NULL;

    FILE *fp = fopen(data_path, "rb");

    if (fp != NULL) {   /* Data file exists */
        off_t len = file_size(data_path);

        if (len == 0) {
            fclose(fp);
            exit_toxic_err("failed in load_tox", FATALERR_FILEOP);
        }

        char *data = malloc(len);

        if (data == NULL) {
            fclose(fp);
            exit_toxic_err("failed in load_tox", FATALERR_MEMORY);
        }

        if (fread(data, len, 1, fp) != 1) {
            fclose(fp);
            free(data);
            exit_toxic_err("failed in load_tox", FATALERR_FILEOP);
        }

        bool is_encrypted = tox_is_data_encrypted((uint8_t *) data);

        /* attempt to encrypt an already encrypted data file */
        if (arg_opts.encrypt_data && is_encrypted) {
            fclose(fp);
            free(data);
            exit_toxic_err("failed in load_tox", FATALERR_ENCRYPT);
        }

        if (arg_opts.unencrypt_data && is_encrypted) {
            queue_init_message("Data file '%s' has been unencrypted", data_path);
        } else if (arg_opts.unencrypt_data) {
            queue_init_message("Warning: passed --unencrypt-data option with unencrypted data file '%s'", data_path);
        }

        if (is_encrypted) {
            if (!arg_opts.unencrypt_data) {
                user_password.data_is_encrypted = true;
            }

            size_t pwlen = 0;
            int pweval = user_settings->password_eval[0];

            if (!pweval) {
                clear_screen();
                printf("Enter password (q to quit) ");
            }

            size_t plain_len = len - TOX_PASS_ENCRYPTION_EXTRA_LENGTH;
            char *plain = malloc(plain_len);  // must be freed after tox_new()

            if (plain == NULL) {
                fclose(fp);
                free(data);
                exit_toxic_err("failed in load_tox", FATALERR_MEMORY);
            }

            while (true) {
                fflush(stdout); // Flush before prompts so the user sees the question/message

                if (pweval) {
                    pwlen = password_eval(user_password.pass, sizeof(user_password.pass));
                } else {
                    pwlen = password_prompt(user_password.pass, sizeof(user_password.pass));
                }

                user_password.len = pwlen;

                if (strcasecmp(user_password.pass, "q") == 0) {
                    fclose(fp);
                    free(plain);
                    free(data);
                    exit(0);
                }

                if (pwlen < MIN_PASSWORD_LEN) {
                    clear_screen();
                    sleep(1);
                    printf("Invalid password. Try again. ");
                    pweval = 0;
                    continue;
                }

                Tox_Err_Decryption pwerr;
                tox_pass_decrypt((uint8_t *) data, len, (uint8_t *) user_password.pass, pwlen,
                                 (uint8_t *) plain, &pwerr);

                if (pwerr == TOX_ERR_DECRYPTION_OK) {
                    tox_options_set_savedata_type(tox_opts, TOX_SAVEDATA_TYPE_TOX_SAVE);
                    tox_options_set_savedata_data(tox_opts, (uint8_t *) plain, plain_len);

                    tox = tox_new(tox_opts, new_err);

                    if (tox == NULL) {
                        fclose(fp);
                        free(data);
                        free(plain);
                        return NULL;
                    }

                    break;
                } else if (pwerr == TOX_ERR_DECRYPTION_FAILED) {
                    clear_screen();
                    sleep(1);
                    printf("Invalid password. Try again. ");
                    pweval = 0;
                } else {
                    fclose(fp);
                    free(data);
                    free(plain);
                    exit_toxic_err("tox_pass_decrypt() failed", pwerr);
                }
            }

            free(plain);
        } else {   /* data is not encrypted */
            tox_options_set_savedata_type(tox_opts, TOX_SAVEDATA_TYPE_TOX_SAVE);
            tox_options_set_savedata_data(tox_opts, (uint8_t *) data, len);

            tox = tox_new(tox_opts, new_err);

            if (tox == NULL) {
                fclose(fp);
                free(data);
                return NULL;
            }
        }

        fclose(fp);
        free(data);
    } else {   /* Data file does not/should not exist */
        if (file_exists(data_path)) {
            exit_toxic_err("failed in load_tox", FATALERR_FILEOP);
        }

        tox_options_set_savedata_type(tox_opts, TOX_SAVEDATA_TYPE_NONE);

        tox = tox_new(tox_opts, new_err);

        if (tox == NULL) {
            return NULL;
        }

        if (store_data(tox, data_path) == -1) {
            exit_toxic_err("failed in load_tox", FATALERR_FILEOP);
        }
    }

    return tox;
}

static Tox *load_toxic(char *data_path)
{
    Tox_Err_Options_New options_new_err;
    struct Tox_Options *tox_opts = tox_options_new(&options_new_err);

    if (tox_opts == NULL) {
        exit_toxic_err("tox_options_new returned fatal error", options_new_err);
    }

    init_tox_options(tox_opts);

    Tox_Err_New new_err;
    Tox *tox = load_tox(data_path, tox_opts, &new_err);

    if (new_err == TOX_ERR_NEW_PORT_ALLOC && tox_options_get_ipv6_enabled(tox_opts)) {
        queue_init_message("Falling back to ipv4");
        tox_options_set_ipv6_enabled(tox_opts, false);
        tox = load_tox(data_path, tox_opts, &new_err);
    }

    if (tox == NULL) {
        return NULL;
    }

    if (new_err != TOX_ERR_NEW_OK) {
        queue_init_message("tox_new returned non-fatal error %d", new_err);
    }

    init_tox_callbacks(tox);
    load_friendlist(tox);
    load_blocklist(BLOCK_FILE);

    if (tox_self_get_name_size(tox) == 0) {
        tox_self_set_name(tox, (uint8_t *) "Toxic User", strlen("Toxic User"), NULL);
    }

    tox_options_free(tox_opts);
    return tox;
}

static void do_toxic(Toxic *toxic)
{
    pthread_mutex_lock(&Winthread.lock);

    if (arg_opts.no_connect) {
        pthread_mutex_unlock(&Winthread.lock);
        return;
    }

    tox_iterate(toxic->tox, toxic);
    do_tox_connection(toxic->tox);

    pthread_mutex_unlock(&Winthread.lock);
}

/* Set interface refresh flag. This should be called whenever the interface changes.
 *
 * This function is not thread safe.
 */
void flag_interface_refresh(void)
{
    Winthread.flag_refresh = 1;
    Winthread.last_refresh_flag = get_unix_time();
}

/* How long we wait to idle interface refreshing after last flag set. Should be no less than 2. */
#define ACTIVE_WIN_REFRESH_TIMEOUT 2

static void poll_interface_refresh_flag(void)
{
    pthread_mutex_lock(&Winthread.lock);

    bool flag = Winthread.flag_refresh;
    time_t t = Winthread.last_refresh_flag;

    pthread_mutex_unlock(&Winthread.lock);

    if (flag == 1 && timed_out(t, ACTIVE_WIN_REFRESH_TIMEOUT)) {
        pthread_mutex_lock(&Winthread.lock);
        Winthread.flag_refresh = 0;
        pthread_mutex_unlock(&Winthread.lock);
    }
}


/* How often we refresh windows that aren't focused */
#define INACTIVE_WIN_REFRESH_RATE 10

void *thread_winref(void *data)
{
    Tox *tox = (Tox *) data;

    uint8_t draw_count = 0;

    init_signal_catchers();

    while (true) {
        draw_count++;
        draw_active_window(tox);

        if (Winthread.flag_resize) {
            on_window_resize();
            Winthread.flag_resize = 0;
        } else if (draw_count >= INACTIVE_WIN_REFRESH_RATE) {
            refresh_inactive_windows();
            draw_count = 0;
        }

        if (Winthread.sig_exit_toxic) {
            pthread_mutex_lock(&Winthread.lock);
            exit_toxic_success(tox);
        }

        poll_interface_refresh_flag();
    }
}

void *thread_cqueue(void *data)
{
    Tox *tox = (Tox *) data;

    while (true) {
        pthread_mutex_lock(&Winthread.lock);

        for (size_t i = 2; i < MAX_WINDOWS_NUM; ++i) {
            ToxWindow *toxwin = get_window_ptr(i);

            if ((toxwin != NULL) && (toxwin->type == WINDOW_TYPE_CHAT)) {
                cqueue_check_unread(toxwin);

                if (get_friend_connection_status(toxwin->num) != TOX_CONNECTION_NONE) {
                    cqueue_try_send(toxwin, tox);
                }
            }
        }

        pthread_mutex_unlock(&Winthread.lock);

        sleep_thread(750000L); // 0.75 seconds
    }
}

#ifdef AUDIO
void *thread_av(void *data)
{
    ToxAV *av = (ToxAV *) data;

    while (true) {
        pthread_mutex_lock(&Winthread.lock);
        toxav_iterate(av);
        pthread_mutex_unlock(&Winthread.lock);

        long int sleep_duration = toxav_iteration_interval(av) * 1000;
        sleep_thread(sleep_duration);
    }
}
#endif /* AUDIO */

static void print_usage(void)
{
    fprintf(stderr, "usage: toxic [OPTION] [FILE ...]\n");
    fprintf(stderr, "  -4, --ipv4               Force IPv4 connection\n");
    fprintf(stderr, "  -b, --debug              Enable stderr for debugging\n");
    fprintf(stderr, "  -c, --config             Use specified config file\n");
    fprintf(stderr, "  -d, --default-locale     Use default POSIX locale\n");
    fprintf(stderr, "  -e, --encrypt-data       Encrypt an unencrypted data file\n");
    fprintf(stderr, "  -f, --file               Use specified data file\n");
    fprintf(stderr, "  -h, --help               Show this message and exit\n");
    fprintf(stderr, "  -l, --logging            Enable toxcore logging: Requires [log_path | stderr]\n");
    fprintf(stderr, "  -L, --no-lan             Disable local discovery\n");
    fprintf(stderr, "  -n, --nodes              Use specified DHTnodes file\n");
    fprintf(stderr, "  -o, --noconnect          Do not connect to the DHT network\n");
    fprintf(stderr, "  -p, --SOCKS5-proxy       Use SOCKS5 proxy: Requires [IP] [port]\n");
    fprintf(stderr, "  -P, --HTTP-proxy         Use HTTP proxy: Requires [IP] [port]\n");
    fprintf(stderr, "  -r, --namelist           Use specified name lookup server list\n");
    fprintf(stderr, "  -t, --force-tcp          Force toxic to use a TCP connection (use with proxies)\n");
    fprintf(stderr, "  -T, --tcp-server         Act as a TCP relay server: Requires [port]\n");
    fprintf(stderr, "  -u, --unencrypt-data     Unencrypt an encrypted data file\n");
    fprintf(stderr, "  -v, --version            Print the version\n");
}

static void print_version(void)
{
    fprintf(stderr, "Toxic version %s\n", TOXICVER);
    fprintf(stderr, "Toxcore version %d.%d.%d\n", tox_version_major(), tox_version_minor(), tox_version_patch());
}

static void set_default_opts(void)
{
    arg_opts = (struct arg_opts) {
        0
    };

    /* set any non-zero defaults here*/
    arg_opts.proxy_type = TOX_PROXY_TYPE_NONE;
}

static void parse_args(int argc, char *argv[])
{
    set_default_opts();

    static struct option long_opts[] = {
        {"ipv4", no_argument, 0, '4'},
        {"debug", no_argument, 0, 'b'},
        {"default-locale", no_argument, 0, 'd'},
        {"config", required_argument, 0, 'c'},
        {"encrypt-data", no_argument, 0, 'e'},
        {"file", required_argument, 0, 'f'},
        {"logging", required_argument, 0, 'l'},
        {"no-lan", no_argument, 0, 'L'},
        {"nodes", required_argument, 0, 'n'},
        {"help", no_argument, 0, 'h'},
        {"noconnect", no_argument, 0, 'o'},
        {"namelist", required_argument, 0, 'r'},
        {"force-tcp", no_argument, 0, 't'},
        {"tcp-server", required_argument, 0, 'T'},
        {"SOCKS5-proxy", required_argument, 0, 'p'},
        {"HTTP-proxy", required_argument, 0, 'P'},
        {"unencrypt-data", no_argument, 0, 'u'},
        {"version", no_argument, 0, 'v'},
        {NULL, no_argument, NULL, 0},
    };

    const char *opts_str = "4bdehLotuxvc:f:l:n:r:p:P:T:";
    int opt = 0;
    int indexptr = 0;

    while ((opt = getopt_long(argc, argv, opts_str, long_opts, &indexptr)) != -1) {
        switch (opt) {
            case '4': {
                arg_opts.use_ipv4 = 1;
                break;
            }

            case 'b': {
                arg_opts.debug = 1;
                queue_init_message("stderr enabled");
                break;
            }

            case 'c': {
                if (optarg == NULL) {
                    queue_init_message("Invalid argument for option: %d", opt);
                    break;
                }

                snprintf(arg_opts.config_path, sizeof(arg_opts.config_path), "%s", optarg);

                if (!file_exists(arg_opts.config_path)) {
                    queue_init_message("Config file not found");
                }

                break;
            }

            case 'd': {
                arg_opts.default_locale = 1;
                queue_init_message("Using default POSIX locale");
                break;
            }

            case 'e': {
                arg_opts.encrypt_data = 1;
                break;
            }

            case 'f': {
                if (optarg == NULL) {
                    queue_init_message("Invalid argument for option: %d", opt);
                    break;
                }

                arg_opts.use_custom_data = 1;

                if (DATA_FILE) {
                    free(DATA_FILE);
                    DATA_FILE = NULL;
                }

                if (BLOCK_FILE) {
                    free(BLOCK_FILE);
                    BLOCK_FILE = NULL;
                }

                DATA_FILE = malloc(strlen(optarg) + 1);

                if (DATA_FILE == NULL) {
                    exit_toxic_err("failed in parse_args", FATALERR_MEMORY);
                }

                strcpy(DATA_FILE, optarg);

                BLOCK_FILE = malloc(strlen(optarg) + strlen("-blocklist") + 1);

                if (BLOCK_FILE == NULL) {
                    exit_toxic_err("failed in parse_args", FATALERR_MEMORY);
                }

                strcpy(BLOCK_FILE, optarg);
                strcat(BLOCK_FILE, "-blocklist");

                queue_init_message("Using '%s' data file", DATA_FILE);

                break;
            }

            case 'l': {
                if (optarg) {
                    arg_opts.logging = true;

                    if (strcmp(optarg, "stderr") != 0) {
                        arg_opts.log_fp = fopen(optarg, "w");

                        if (arg_opts.log_fp != NULL) {
                            queue_init_message("Toxcore logging enabled to %s", optarg);
                        } else {
                            arg_opts.debug = true;
                            queue_init_message("Failed to open log file %s. Falling back to stderr.", optarg);
                        }
                    } else {
                        arg_opts.debug = true;
                        queue_init_message("Toxcore logging enabled to stderr");
                    }
                }

                break;
            }

            case 'L': {
                arg_opts.disable_local_discovery = 1;
                queue_init_message("Local discovery disabled");
                break;
            }

            case 'n': {
                if (optarg == NULL) {
                    queue_init_message("Invalid argument for option: %d", opt);
                    break;
                }

                snprintf(arg_opts.nodes_path, sizeof(arg_opts.nodes_path), "%s", optarg);
                break;
            }

            case 'o': {
                arg_opts.no_connect = 1;
                queue_init_message("DHT disabled");
                break;
            }

            case 'p': {
                arg_opts.proxy_type = TOX_PROXY_TYPE_SOCKS5;
            }

            // Intentional fallthrough

            case 'P': {
                if (optarg == NULL) {
                    queue_init_message("Invalid argument for option: %d", opt);
                    arg_opts.proxy_type = TOX_PROXY_TYPE_NONE;
                    break;
                }

                if (arg_opts.proxy_type == TOX_PROXY_TYPE_NONE) {
                    arg_opts.proxy_type = TOX_PROXY_TYPE_HTTP;
                }

                snprintf(arg_opts.proxy_address, sizeof(arg_opts.proxy_address), "%s", optarg);

                if (++optind > argc || argv[optind - 1][0] == '-') {
                    exit_toxic_err("Proxy error", FATALERR_PROXY);
                }

                long int port = strtol(argv[optind - 1], NULL, 10);

                if (port <= 0 || port > MAX_PORT_RANGE) {
                    exit_toxic_err("Proxy error", FATALERR_PROXY);
                }

                arg_opts.proxy_port = port;
                break;
            }

            case 'r': {
                if (optarg == NULL) {
                    queue_init_message("Invalid argument for option: %d", opt);
                    break;
                }

                snprintf(arg_opts.nameserver_path, sizeof(arg_opts.nameserver_path), "%s", optarg);

                if (!file_exists(arg_opts.nameserver_path)) {
                    queue_init_message("nameserver list not found");
                }

                break;
            }

            case 't': {
                arg_opts.force_tcp = 1;
                break;
            }

            case 'T': {
                if (optarg == NULL) {
                    queue_init_message("Invalid argument for option: %d", opt);
                    break;
                }

                long int port = strtol(optarg, NULL, 10);

                if (port <= 0 || port > MAX_PORT_RANGE) {
                    port = MAX_PORT_RANGE;
                }

                arg_opts.tcp_port = port;
                break;
            }

            case 'u': {
                arg_opts.unencrypt_data = 1;
                break;
            }

            case 'v': {
                print_version();
                exit(EXIT_SUCCESS);
            }

            case 'h':

            // Intentional fallthrough
            default: {
                print_usage();
                exit(EXIT_SUCCESS);
            }
        }
    }
}

/* Initializes the default config directory and data files used by toxic.
 *
 * Exits the process with an error on failure.
 */
static void init_default_data_files(void)
{
    if (arg_opts.use_custom_data) {
        return;
    }

    char *user_config_dir = get_user_config_dir();

    if (user_config_dir == NULL) {
        exit_toxic_err("failed in init_default_data_files()", FATALERR_FILEOP);
    }

    int config_err = create_user_config_dirs(user_config_dir);

    if (config_err == -1) {
        DATA_FILE = strdup(DATANAME);
        BLOCK_FILE = strdup(BLOCKNAME);

        if (DATA_FILE == NULL || BLOCK_FILE == NULL) {
            exit_toxic_err("failed in init_default_data_files()", FATALERR_MEMORY);
        }
    } else {
        DATA_FILE = malloc(strlen(user_config_dir) + strlen(CONFIGDIR) + strlen(DATANAME) + 1);
        BLOCK_FILE = malloc(strlen(user_config_dir) + strlen(CONFIGDIR) + strlen(BLOCKNAME) + 1);

        if (DATA_FILE == NULL || BLOCK_FILE == NULL) {
            exit_toxic_err("failed in init_default_data_files()", FATALERR_MEMORY);
        }

        strcpy(DATA_FILE, user_config_dir);
        strcat(DATA_FILE, CONFIGDIR);
        strcat(DATA_FILE, DATANAME);

        strcpy(BLOCK_FILE, user_config_dir);
        strcat(BLOCK_FILE, CONFIGDIR);
        strcat(BLOCK_FILE, BLOCKNAME);
    }

    free(user_config_dir);
}

static Toxic *toxic_init(Tox *tox)
{
    Toxic *toxic = calloc(1, sizeof(Toxic));

    if (toxic == NULL) {
        return NULL;
    }

    toxic->tox = tox;

    return toxic;
}

int main(int argc, char **argv)
{
    /* Make sure all written files are read/writeable only by the current user. */
    umask(S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

    srand(time(NULL)); // We use rand() for trivial/non-security related things

    parse_args(argc, argv);

    /* Use the -b flag to enable stderr */
    if (!arg_opts.debug) {
        if (!freopen("/dev/null", "w", stderr)) {
            fprintf(stderr, "Warning: failed to enable stderr\n");
        }
    }

    if (arg_opts.encrypt_data && arg_opts.unencrypt_data) {
        arg_opts.encrypt_data = 0;
        arg_opts.unencrypt_data = 0;
        queue_init_message("Warning: Using --unencrypt-data and --encrypt-data simultaneously has no effect");
    }

    init_default_data_files();

    bool datafile_exists = file_exists(DATA_FILE);

    if (!datafile_exists && !arg_opts.unencrypt_data) {
        first_time_encrypt("Creating new data file. Would you like to encrypt it? Y/n (q to quit)");
    } else if (arg_opts.encrypt_data) {
        first_time_encrypt("Encrypt existing data file? Y/n (q to quit)");
    }

    /* init user_settings struct and load settings from conf file */
    user_settings = calloc(1, sizeof(struct user_settings));

    if (user_settings == NULL) {
        exit_toxic_err("failed in main", FATALERR_MEMORY);
    }

    const char *config_path = arg_opts.config_path[0] ? arg_opts.config_path : NULL;

    if (settings_load_main(user_settings, config_path) == -1) {
        queue_init_message("Failed to load user settings");
    }

    const int curl_init = curl_global_init(CURL_GLOBAL_ALL);
    const int nameserver_ret = name_lookup_init(curl_init);

    if (nameserver_ret == -1) {
        queue_init_message("curl failed to initialize; name lookup service is disabled.");
    } else if (nameserver_ret == -2) {
        queue_init_message("Name lookup server list could not be found.");
    } else if (nameserver_ret == -3) {
        fprintf(stderr, "Name lookup server list does not contain any valid entries\n");
    }

#ifdef X11

    if (init_x11focus() == -1) {
        queue_init_message("X failed to initialize");
    }

#endif /* X11 */

    Tox *tox = load_toxic(DATA_FILE);

    if (tox == NULL) {
        exit_toxic_err("Failed in main", FATALERR_TOX_INIT);
    }

    Toxic *toxic = toxic_init(tox);

    if (toxic == NULL) {
        exit_toxic_err("failed in main", FATALERR_TOXIC_INIT);
    }

    if (arg_opts.encrypt_data && !datafile_exists) {
        arg_opts.encrypt_data = 0;
    }

    const int fs_ret = settings_load_friends(config_path);

    if (fs_ret != 0) {
        queue_init_message("Failed to load friend config settings: error %d", fs_ret);
    }

    init_term();

    prompt = init_windows(toxic->tox);
    prompt_init_statusbar(prompt, toxic->tox, !datafile_exists);
    load_groups(toxic->tox);
    load_conferences(toxic->tox);
    set_active_window_index(0);

    if (pthread_mutex_init(&Winthread.lock, NULL) != 0) {
        exit_toxic_err("failed in main", FATALERR_MUTEX_INIT);
    }

#ifdef AUDIO

    av = init_audio(prompt, toxic->tox);

    if (av == NULL) {
        queue_init_message("Failed to init audio");
    }

#ifdef VIDEO
    init_video(prompt, toxic->tox);

    if (av == NULL) {
        queue_init_message("Failed to init video");
    }

#endif /* VIDEO */

    /* AV thread */
    if (pthread_create(&av_thread.tid, NULL, thread_av, (void *)av) != 0) {
        exit_toxic_err("failed in main", FATALERR_THREAD_CREATE);
    }

    set_al_device(input, user_settings->audio_in_dev);
    set_al_device(output, user_settings->audio_out_dev);

#elif SOUND_NOTIFY

    if (init_devices() == de_InternalError) {
        queue_init_message("Failed to init audio devices");
    }

#endif /* AUDIO */

    /* thread for ncurses UI */
    if (pthread_create(&Winthread.tid, NULL, thread_winref, (void *)toxic->tox) != 0) {
        exit_toxic_err("failed in main", FATALERR_THREAD_CREATE);
    }

    /* thread for message queue */
    if (pthread_create(&cqueue_thread.tid, NULL, thread_cqueue, (void *)toxic->tox) != 0) {
        exit_toxic_err("failed in main", FATALERR_THREAD_CREATE);
    }

#ifdef PYTHON

    init_python(toxic->tox);
    invoke_autoruns(prompt->chatwin->history, prompt);

#endif /* PYTHON */

    init_notify(60, user_settings->notification_timeout);

    /* screen/tmux auto-away timer */
    if (init_mplex_away_timer(toxic->tox) == -1) {
        queue_init_message("Failed to init mplex auto-away.");
    }

    const int nodeslist_ret = load_DHT_nodeslist();

    if (nodeslist_ret != 0) {
        queue_init_message("DHT nodeslist failed to load (error %d)", nodeslist_ret);
    }

    pthread_mutex_lock(&Winthread.lock);
    print_init_messages(prompt);
    flag_interface_refresh();
    pthread_mutex_unlock(&Winthread.lock);

    cleanup_init_messages();

    /* set user avatar from config file. if no path is supplied tox_unset_avatar is called */
    char avatarstr[PATH_MAX + 11];
    snprintf(avatarstr, sizeof(avatarstr), "/avatar %s", user_settings->avatar_path);
    execute(prompt->chatwin->history, prompt, toxic->tox, avatarstr, GLOBAL_COMMAND_MODE);

    time_t last_save = get_unix_time();

    while (true) {
        do_toxic(toxic);

        const time_t cur_time = get_unix_time();

        if (user_settings->autosave_freq > 0 && timed_out(last_save, user_settings->autosave_freq)) {
            pthread_mutex_lock(&Winthread.lock);

            if (store_data(toxic->tox, DATA_FILE) != 0) {
                line_info_add(prompt, false, NULL, NULL, SYS_MSG, 0, RED, "WARNING: Failed to save to data file");
            }

            pthread_mutex_unlock(&Winthread.lock);

            last_save = cur_time;
        }

        const long int sleep_duration = tox_iteration_interval(toxic->tox) * 1000;
        sleep_thread(sleep_duration);
    }

    return 0;
}

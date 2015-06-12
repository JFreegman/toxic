/*  toxic.c
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

#include <curses.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <signal.h>
#include <locale.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <getopt.h>
#include <netdb.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <limits.h>
#include <termios.h>

#include <tox/tox.h>
#include <tox/toxencryptsave.h>

#include "configdir.h"
#include "toxic.h"
#include "windows.h"
#include "friendlist.h"
#include "prompt.h"
#include "misc_tools.h"
#include "file_transfers.h"
#include "line_info.h"
#include "settings.h"
#include "log.h"
#include "notify.h"
#include "audio_device.h"
#include "video_device.h"
#include "message_queue.h"
#include "execute.h"
#include "term_mplex.h"

#ifdef X11
    #include "xtra.h"
#endif

#ifdef AUDIO
#include "audio_call.h"
#ifdef VIDEO
#include "video_call.h"
#endif /* VIDEO */
ToxAV *av;
#endif /* AUDIO */

#ifndef PACKAGE_DATADIR
    #define PACKAGE_DATADIR "."
#endif

/* Export for use in Callbacks */
char *DATA_FILE = NULL;
char *BLOCK_FILE = NULL;
ToxWindow *prompt = NULL;

#define AUTOSAVE_FREQ 60
#define MIN_PASSWORD_LEN 6
#define MAX_PASSWORD_LEN 64

struct Winthread Winthread;
struct cqueue_thread cqueue_thread;
struct audio_thread audio_thread;
struct arg_opts arg_opts;
struct user_settings *user_settings = NULL;


static struct user_password {
    bool data_is_encrypted;
    char pass[MAX_PASSWORD_LEN + 1];
    int len;
} user_password;

static void catch_SIGINT(int sig)
{
    Winthread.sig_exit_toxic = 1;
}

static void catch_SIGSEGV(int sig)
{
    freopen("/dev/tty", "w", stderr);    // make sure stderr is enabled since we may have disabled it
    endwin();
    fprintf(stderr, "Caught SIGSEGV: Aborting toxic session.\n");
    exit(EXIT_FAILURE);
}

static void flag_window_resize(int sig)
{
    Winthread.flag_resize = 1;
}

static void init_signal_catchers(void)
{
    signal(SIGWINCH, flag_window_resize);
    signal(SIGINT, catch_SIGINT);
    signal(SIGSEGV, catch_SIGSEGV);
}

void exit_toxic_success(Tox *m)
{
    store_data(m, DATA_FILE);
    memset(&user_password, 0, sizeof(struct user_password));
    kill_all_file_transfers(m);
    kill_all_windows(m);
    terminate_notify();

#ifdef AUDIO
    terminate_audio();
#endif /* AUDIO */

#ifdef VIDEO
    terminate_video();
#endif /* VIDEO */

    free(DATA_FILE);
    free(BLOCK_FILE);
    free(user_settings);

    tox_kill(m);
    endwin();

#ifdef X11
    /* We have to terminate xtra last coz reasons
     * Please don't call this anywhere else coz trust me
     */
    terminate_xtra();
#endif /* X11 */

    exit(EXIT_SUCCESS);
}

void exit_toxic_err(const char *errmsg, int errcode)
{
    freopen("/dev/tty", "w", stderr);
    endwin();
    fprintf(stderr, "Toxic session aborted with error code %d (%s)\n", errcode, errmsg);
    exit(EXIT_FAILURE);
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
    timeout(100);

    if (has_colors()) {
        short bg_color = COLOR_BLACK;
        start_color();

        if (user_settings->colour_theme == NATIVE_COLS) {
            if (assume_default_colors(-1, -1) == OK)
                bg_color = -1;
        }

        init_pair(0, COLOR_WHITE, COLOR_BLACK);
        init_pair(1, COLOR_GREEN, bg_color);
        init_pair(2, COLOR_CYAN, bg_color);
        init_pair(3, COLOR_RED, bg_color);
        init_pair(4, COLOR_BLUE, bg_color);
        init_pair(5, COLOR_YELLOW, bg_color);
        init_pair(6, COLOR_MAGENTA, bg_color);
        init_pair(7, COLOR_BLACK, COLOR_BLACK);
        init_pair(8, COLOR_BLACK, COLOR_WHITE);
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

    if (new_msgs == NULL)
        exit_toxic_err("Failed in queue_init_message", FATALERR_MEMORY);

    new_msgs[i] = malloc(MAX_STR_SIZE);

    if (new_msgs[i] == NULL)
        exit_toxic_err("Failed in queue_init_message", FATALERR_MEMORY);

    snprintf(new_msgs[i], MAX_STR_SIZE, "%s", frmt_msg);
    init_messages.msgs = new_msgs;
}

/* called after messages have been printed to prompt and are no longer needed */
static void cleanup_init_messages(void)
{
    if (init_messages.num <= 0)
        return;

    int i;

    for (i = 0; i < init_messages.num; ++i)
        free(init_messages.msgs[i]);

    free(init_messages.msgs);
}

static void print_init_messages(ToxWindow *toxwin)
{
    int i;

    for (i = 0; i < init_messages.num; ++i)
        line_info_add(toxwin, NULL, NULL, NULL, SYS_MSG, 0, 0, init_messages.msgs[i]);
}

#define MIN_NODE_LINE  50 /* IP: 7 + port: 5 + key: 38 + spaces: 2 = 70. ! (& e.g. tox.im = 6) */
#define MAX_NODE_LINE  256 /* Approx max number of chars in a sever line (name + port + key) */
#define MAXNODES 50
#define NODELEN (MAX_NODE_LINE - TOX_PUBLIC_KEY_SIZE - 7)

static struct toxNodes {
    int lines;
    char nodes[MAXNODES][NODELEN];
    uint16_t ports[MAXNODES];
    char keys[MAXNODES][TOX_PUBLIC_KEY_SIZE];
} toxNodes;

static int load_nodelist(const char *filename)
{
    if (!filename)
        return 1;

    FILE *fp = fopen(filename, "r");

    if (fp == NULL)
        return 1;

    char line[MAX_NODE_LINE];

    while (fgets(line, sizeof(line), fp) && toxNodes.lines < MAXNODES) {
        if (strlen(line) > MIN_NODE_LINE) {
            const char *name = strtok(line, " ");
            const char *port = strtok(NULL, " ");
            const char *key_ascii = strtok(NULL, " ");

            /* invalid line */
            if (name == NULL || port == NULL || key_ascii == NULL)
                continue;

            snprintf(toxNodes.nodes[toxNodes.lines], sizeof(toxNodes.nodes[toxNodes.lines]), "%s", name);
            toxNodes.nodes[toxNodes.lines][NODELEN - 1] = 0;
            toxNodes.ports[toxNodes.lines] = atoi(port);

            char *key_binary = hex_string_to_bin(key_ascii);
            memcpy(toxNodes.keys[toxNodes.lines], key_binary, TOX_PUBLIC_KEY_SIZE);
            free(key_binary);

            toxNodes.lines++;
        }
    }

    fclose(fp);

    if (toxNodes.lines < 1)
        return 1;

    return 0;
}

/* Bootstraps and adds as TCP relay.
 * Returns 0 if both actions are successful.
 * Returns -1 otherwise.
 */
int init_connection_helper(Tox *m, int line)
{
    TOX_ERR_BOOTSTRAP err;
    tox_bootstrap(m, toxNodes.nodes[line], toxNodes.ports[line], (uint8_t *) toxNodes.keys[line], &err);

    if (err != TOX_ERR_BOOTSTRAP_OK) {
        fprintf(stderr, "Failed to bootstrap %s:%d\n", toxNodes.nodes[line], toxNodes.ports[line]);
        return -1;
    }

    tox_add_tcp_relay(m, toxNodes.nodes[line], toxNodes.ports[line], (uint8_t *) toxNodes.keys[line], &err);

    if (err != TOX_ERR_BOOTSTRAP_OK) {
        fprintf(stderr, "Failed to add TCP relay %s:%d\n", toxNodes.nodes[line], toxNodes.ports[line]);
        return -1;
    }

    return 0;
}

/* Connects to a random DHT node listed in the DHTnodes file
 *
 * return codes:
 * 0: success
 * 1: failed to open node file
 * 2: no line of sufficient length in node file
 * 3: failed to resolve name to IP
 * 4: nodelist file contains no acceptable line
 */
static bool srvlist_loaded = false;

#define NUM_INIT_NODES 5

int init_connection(Tox *m)
{
    if (toxNodes.lines > 0) { /* already loaded nodelist */
        init_connection_helper(m, rand() % toxNodes.lines);
        return 0;
    }

    /* only once:
     * - load the nodelist
     * - connect to "everyone" inside
     */
    if (!srvlist_loaded) {
        srvlist_loaded = true;
        int res;

        if (!arg_opts.nodes_path[0])
            res = load_nodelist(PACKAGE_DATADIR "/DHTnodes");
        else
            res = load_nodelist(arg_opts.nodes_path);

        if (res != 0)
            return res;

        res = 3;
        int i;
        int n = MIN(NUM_INIT_NODES, toxNodes.lines);

        for (i = 0; i < n; ++i) {
            if (init_connection_helper(m, rand() % toxNodes.lines) == 0)
                res = 0;
        }

        return res;
    }

    /* empty nodelist file */
    return 4;
}

static void load_friendlist(Tox *m)
{
    size_t i;
    size_t numfriends = tox_self_get_friend_list_size(m);

    for (i = 0; i < numfriends; ++i)
        friendlist_onFriendAdded(NULL, m, i, false);

    sort_friendlist_index();
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

    if (tcsetattr(fileno(stdin), TCSANOW, &nflags) != 0)
        return 0;

    const char *p = fgets(buf, size, stdin);
    int len = strlen(buf);

    /* re-enable terminal echo */
    tcsetattr(fileno(stdin), TCSANOW, &oflags);

    if (p == NULL || len <= 1)
        return 0;

    /* eat overflowed stdin and return error */
    if (buf[--len] != '\n') {
        int ch;
        while ((ch = getchar()) != '\n' && ch > 0)
            ;
        return 0;
    }

    buf[len] = '\0';
    return len;
}

/* Ask user if they would like to encrypt the data file and set password */
static void first_time_encrypt(const char *msg)
{
    char ch[256] = {0};

    do {
        system("clear");
        printf("%s ", msg);

        if (!strcasecmp(ch, "y\n") || !strcasecmp(ch, "n\n") || !strcasecmp(ch, "yes\n")
            || !strcasecmp(ch, "no\n") || !strcasecmp(ch, "q\n"))
            break;

    } while (fgets(ch, sizeof(ch), stdin));

    printf("\n");

    if (ch[0] == 'q' || ch[0] == 'Q')
        exit(0);

    if (ch[0] == 'y' || ch[0] == 'Y') {
        int len = 0;
        bool valid_password = false;
        char passconfirm[MAX_PASSWORD_LEN + 1] = {0};

        printf("Enter a new password (must be at least %d characters) ", MIN_PASSWORD_LEN);

        while (valid_password == false) {
            len = password_prompt(user_password.pass, sizeof(user_password.pass));
            user_password.len = len;

            if (strcasecmp(user_password.pass, "q") == 0)
                exit(0);

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

    system("clear");
}

/* Store Tox data to given location
 *
 * Return 0 if stored successfully or ignoring data file.
 * Return -1 on error
 */
int store_data(Tox *m, const char *path)
{
    if (path == NULL)
        return -1;

    FILE *fp = fopen(path, "wb");

    if (fp == NULL)
        return -1;

    size_t data_len = tox_get_savedata_size(m);
    char data[data_len];

    tox_get_savedata(m, (uint8_t *) data);

    if (user_password.data_is_encrypted && !arg_opts.unencrypt_data) {
        size_t enc_len = data_len + TOX_PASS_ENCRYPTION_EXTRA_LENGTH;
        char enc_data[enc_len];

        TOX_ERR_ENCRYPTION err;
        tox_pass_encrypt((uint8_t *) data, data_len, (uint8_t *) user_password.pass, user_password.len,
                         (uint8_t *) enc_data, &err);

        if (err != TOX_ERR_ENCRYPTION_OK) {
            fprintf(stderr, "tox_pass_encrypt() failed with error %d\n", err);
            fclose(fp);
            return -1;
        }

        if (fwrite(enc_data, enc_len, 1, fp) != 1) {
            fclose(fp);
            return -1;
        }
    } else {  /* data will not be encrypted */
        if (fwrite(data, data_len, 1, fp) != 1) {
            fclose(fp);
            return -1;
        }
    }

    fclose(fp);
    return 0;
}

static void init_tox_callbacks(Tox *m)
{
    tox_callback_self_connection_status(m, prompt_onSelfConnectionChange, NULL);
    tox_callback_friend_connection_status(m, on_connectionchange, NULL);
    tox_callback_friend_typing(m, on_typing_change, NULL);
    tox_callback_friend_request(m, on_request, NULL);
    tox_callback_friend_message(m, on_message, NULL);
    tox_callback_friend_name(m, on_nickchange, NULL);
    tox_callback_friend_status(m, on_statuschange, NULL);
    tox_callback_friend_status_message(m, on_statusmessagechange, NULL);
    tox_callback_friend_read_receipt(m, on_read_receipt, NULL);
    tox_callback_group_invite(m, on_groupinvite, NULL);
    tox_callback_group_message(m, on_groupmessage, NULL);
    tox_callback_group_action(m, on_groupaction, NULL);
    tox_callback_group_namelist_change(m, on_group_namelistchange, NULL);
    tox_callback_group_title(m, on_group_titlechange, NULL);
    tox_callback_file_recv(m, on_file_recv, NULL);
    tox_callback_file_chunk_request(m, on_file_chunk_request, NULL);
    tox_callback_file_recv_control(m, on_file_control, NULL);
    tox_callback_file_recv_chunk(m, on_file_recv_chunk, NULL);
}

static void init_tox_options(struct Tox_Options *tox_opts)
{
    tox_options_default(tox_opts);

    tox_opts->ipv6_enabled = !arg_opts.use_ipv4;
    tox_opts->udp_enabled = !arg_opts.force_tcp;
    tox_opts->proxy_type = arg_opts.proxy_type;

    if (!tox_opts->ipv6_enabled)
        queue_init_message("Forcing IPv4 connection");

    if (tox_opts->proxy_type != TOX_PROXY_TYPE_NONE) {
        tox_opts->proxy_port = arg_opts.proxy_port;
        tox_opts->proxy_host = arg_opts.proxy_address;
        const char *ps = tox_opts->proxy_type == TOX_PROXY_TYPE_SOCKS5 ? "SOCKS5" : "HTTP";

        char tmp[48];
        snprintf(tmp, sizeof(tmp), "Using %s proxy %s : %d", ps, arg_opts.proxy_address, arg_opts.proxy_port);
        queue_init_message("%s", tmp);
    }

    if (!tox_opts->udp_enabled) {
        queue_init_message("UDP disabled");
    } else if (tox_opts->proxy_type != TOX_PROXY_TYPE_NONE) {
        const char *msg = "WARNING: Using a proxy without disabling UDP may leak your real IP address.";
        queue_init_message("%s", msg);
        msg = "Use the -t option to disable UDP.";
        queue_init_message("%s", msg);
    }
}

/* Returns a new Tox object on success.
 * If object fails to initialize the toxic process will terminate.
 */
static Tox *load_tox(char *data_path, struct Tox_Options *tox_opts, TOX_ERR_NEW *new_err)
{
    Tox *m = NULL;

    FILE *fp = fopen(data_path, "rb");

    if (fp != NULL) {   /* Data file exists */
        off_t len = file_size(data_path);

        if (len == 0) {
            fclose(fp);
            exit_toxic_err("failed in load_toxic", FATALERR_FILEOP);
        }

        char data[len];

        if (fread(data, sizeof(data), 1, fp) != 1) {
            fclose(fp);
            exit_toxic_err("failed in load_toxic", FATALERR_FILEOP);
        }

        bool is_encrypted = tox_is_data_encrypted((uint8_t *) data);

        /* attempt to encrypt an already encrypted data file */
        if (arg_opts.encrypt_data && is_encrypted) {
            fclose(fp);
            exit_toxic_err("failed in load_toxic", FATALERR_ENCRYPT);
        }

        if (arg_opts.unencrypt_data && is_encrypted)
            queue_init_message("Data file '%s' has been unencrypted", data_path);
        else if (arg_opts.unencrypt_data)
            queue_init_message("Warning: passed --unencrypt-data option with unencrypted data file '%s'", data_path);

        if (is_encrypted) {
            if (!arg_opts.unencrypt_data)
                user_password.data_is_encrypted = true;

            size_t pwlen = 0;
            system("clear");   // TODO: is this portable?
            printf("Enter password (q to quit) ");

            size_t plain_len = len - TOX_PASS_ENCRYPTION_EXTRA_LENGTH;
            char plain[plain_len];

            while (true) {
                pwlen = password_prompt(user_password.pass, sizeof(user_password.pass));
                user_password.len = pwlen;

                if (strcasecmp(user_password.pass, "q") == 0) {
                    fclose(fp);
                    exit(0);
                }

                if (pwlen < MIN_PASSWORD_LEN) {
                    system("clear");
                    sleep(1);
                    printf("Invalid password. Try again. ");
                    continue;
                }

                TOX_ERR_DECRYPTION pwerr;
                tox_pass_decrypt((uint8_t *) data, len, (uint8_t *) user_password.pass, pwlen,
                                 (uint8_t *) plain, &pwerr);

                if (pwerr == TOX_ERR_DECRYPTION_OK) {
                    tox_opts->savedata_type = TOX_SAVEDATA_TYPE_TOX_SAVE;
                    tox_opts->savedata_data = (uint8_t *) plain;
                    tox_opts->savedata_length = plain_len;

                    m = tox_new(tox_opts, new_err);

                    if (m == NULL) {
                        fclose(fp);
                        return NULL;
                    }

                    break;
                } else if (pwerr == TOX_ERR_DECRYPTION_FAILED) {
                    system("clear");
                    sleep(1);
                    printf("Invalid password. Try again. ");
                } else {
                    fclose(fp);
                    exit_toxic_err("tox_pass_decrypt() failed", pwerr);
                }
            }
        } else {   /* data is not encrypted */
            tox_opts->savedata_type = TOX_SAVEDATA_TYPE_TOX_SAVE;
            tox_opts->savedata_data = (uint8_t *) data;
            tox_opts->savedata_length = len;

            m = tox_new(tox_opts, new_err);

            if (m == NULL) {
                fclose(fp);
                return NULL;
            }
        }

        fclose(fp);
    } else {   /* Data file does not/should not exist */
        if (file_exists(data_path))
            exit_toxic_err("failed in load_toxic", FATALERR_FILEOP);

        tox_opts->savedata_type = TOX_SAVEDATA_TYPE_NONE;

        m = tox_new(tox_opts, new_err);

        if (m == NULL)
            return NULL;

        if (store_data(m, data_path) == -1)
            exit_toxic_err("failed in load_toxic", FATALERR_FILEOP);
    }

    return m;
}

static Tox *load_toxic(char *data_path)
{
    struct Tox_Options tox_opts;
    init_tox_options(&tox_opts);

    TOX_ERR_NEW new_err;
    Tox *m = load_tox(data_path, &tox_opts, &new_err);

    if (new_err == TOX_ERR_NEW_PORT_ALLOC && tox_opts.ipv6_enabled) {
        queue_init_message("Falling back to ipv4");
        tox_opts.ipv6_enabled = false;
        m = load_tox(data_path, &tox_opts, &new_err);
    }

    if (!m)
        exit_toxic_err("tox_new returned fatal error", new_err);

    if (new_err != TOX_ERR_NEW_OK)
        queue_init_message("tox_new returned non-fatal error %d", new_err);

    init_tox_callbacks(m);
    load_friendlist(m);
    load_blocklist(BLOCK_FILE);

    if (tox_self_get_name_size(m) == 0)
        tox_self_set_name(m, (uint8_t *) "Toxic User", strlen("Toxic User"), NULL);

    return m;
}

#define TRY_BOOTSTRAP_INTERVAL 5
static uint64_t last_bootstrap_time = 0;

static void do_bootstrap(Tox *m)
{
    static int conn_err = 0;
    uint64_t curtime = get_unix_time();

    if (!timed_out(last_bootstrap_time, curtime, TRY_BOOTSTRAP_INTERVAL))
        return;

    if (tox_self_get_connection_status(m) != TOX_CONNECTION_NONE)
        return;

    if (conn_err != 0)
        return;

    last_bootstrap_time = curtime;
    conn_err = init_connection(m);

    if (conn_err != 0)
        line_info_add(prompt, NULL, NULL, NULL, SYS_MSG, 0, 0, "Auto-connect failed with error code %d", conn_err);
}

static void do_toxic(Tox *m, ToxWindow *prompt)
{
    if (arg_opts.no_connect)
        return;

    pthread_mutex_lock(&Winthread.lock);
    tox_iterate(m);
    do_bootstrap(m);
    pthread_mutex_unlock(&Winthread.lock);
}

#define INACTIVE_WIN_REFRESH_RATE 10

void *thread_winref(void *data)
{
    Tox *m = (Tox *) data;
    uint8_t draw_count = 0;
    init_signal_catchers();

    while (true) {
        draw_active_window(m);
        draw_count++;

        if (Winthread.flag_resize) {
            on_window_resize();
            Winthread.flag_resize = 0;
        } else if (draw_count >= INACTIVE_WIN_REFRESH_RATE) {
            refresh_inactive_windows();
            draw_count = 0;
        }

        if (Winthread.sig_exit_toxic) {
            pthread_mutex_lock(&Winthread.lock);
            exit_toxic_success(m);
        }
    }
}

void *thread_cqueue(void *data)
{
    Tox *m = (Tox *) data;

    while (true) {
        pthread_mutex_lock(&Winthread.lock);

        size_t i;

        for (i = 2; i < MAX_WINDOWS_NUM; ++i) {
            ToxWindow *toxwin = get_window_ptr(i);

            if (toxwin != NULL && toxwin->is_chat
                && tox_friend_get_connection_status(m, toxwin->num, NULL) != TOX_CONNECTION_NONE)
                cqueue_try_send(toxwin, m);
        }

        pthread_mutex_unlock(&Winthread.lock);

        usleep(4000);
    }
}

#ifdef AUDIO
void *thread_audio(void *data)
{
    ToxAV *av = (ToxAV *) data;
    
    while (true) {
        pthread_mutex_lock(&Winthread.lock);
        toxav_iterate(av);
        pthread_mutex_unlock(&Winthread.lock);

        usleep(2 * 1000);
    }
}
#endif  /* AUDIO */

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
    fprintf(stderr, "  -n, --nodes              Use specified DHTnodes file\n");
    fprintf(stderr, "  -o, --noconnect          Do not connect to the DHT network\n");
    fprintf(stderr, "  -p, --SOCKS5-proxy       Use SOCKS5 proxy: Requires [IP] [port]\n");
    fprintf(stderr, "  -P, --HTTP-proxy         Use HTTP proxy: Requires [IP] [port]\n");
    fprintf(stderr, "  -r, --dnslist            Use specified DNSservers file\n");
    fprintf(stderr, "  -t, --force-tcp          Force TCP connection (use this with proxies)\n");
    fprintf(stderr, "  -u, --unencrypt-data     Unencrypt an encrypted data file\n");
}

static void set_default_opts(void)
{
    memset(&arg_opts, 0, sizeof(struct arg_opts));

    /* set any non-zero defaults here*/
    arg_opts.proxy_type = TOX_PROXY_TYPE_NONE;
}

static void parse_args(int argc, char *argv[])
{
    set_default_opts();

    static struct option long_opts[] = {
        {"file", required_argument, 0, 'f'},
        {"ipv4", no_argument, 0, '4'},
        {"debug", no_argument, 0, 'b'},
        {"default-locale", no_argument, 0, 'd'},
        {"config", required_argument, 0, 'c'},
        {"encrypt-data", no_argument, 0, 'e'},
        {"nodes", required_argument, 0, 'n'},
        {"help", no_argument, 0, 'h'},
        {"noconnect", no_argument, 0, 'o'},
        {"dnslist", required_argument, 0, 'r'},
        {"force-tcp", no_argument, 0, 't'},
        {"SOCKS5-proxy", required_argument, 0, 'p'},
        {"HTTP-proxy", required_argument, 0, 'P'},
        {"unencrypt-data", no_argument, 0, 'u'},
        {NULL, no_argument, NULL, 0},
    };

    const char *opts_str = "4bdehotuxc:f:n:r:p:P:";
    int opt, indexptr;

    while ((opt = getopt_long(argc, argv, opts_str, long_opts, &indexptr)) != -1) {
        switch (opt) {
            case '4':
                arg_opts.use_ipv4 = 1;
                break;

            case 'b':
                arg_opts.debug = 1;
                queue_init_message("stderr enabled");
                break;

            case 'c':
                snprintf(arg_opts.config_path, sizeof(arg_opts.config_path), "%s", optarg);

                if (!file_exists(arg_opts.config_path))
                    queue_init_message("Config file not found");

                break;

            case 'd':
                arg_opts.default_locale = 1;
                queue_init_message("Using default POSIX locale");
                break;

            case 'e':
                arg_opts.encrypt_data = 1;
                break;

            case 'f':
                arg_opts.use_custom_data = 1;
                DATA_FILE = strdup(optarg);
                BLOCK_FILE = malloc(strlen(optarg) + strlen("-blocklist") + 1);

                if (DATA_FILE == NULL || BLOCK_FILE == NULL)
                    exit_toxic_err("failed in parse_args", FATALERR_MEMORY);

                strcpy(BLOCK_FILE, optarg);
                strcat(BLOCK_FILE, "-blocklist");

                queue_init_message("Using '%s' data file", DATA_FILE);

                break;

            case 'n':
                snprintf(arg_opts.nodes_path, sizeof(arg_opts.nodes_path), "%s", optarg);

                if (!file_exists(arg_opts.nodes_path))
                    queue_init_message("DHTnodes file not found");

                break;

            case 'o':
                arg_opts.no_connect = 1;
                queue_init_message("DHT disabled");
                break;

            case 'p':
                arg_opts.proxy_type = TOX_PROXY_TYPE_SOCKS5;
                snprintf(arg_opts.proxy_address, sizeof(arg_opts.proxy_address), "%s", optarg);

                if (++optind > argc || argv[optind-1][0] == '-')
                    exit_toxic_err("Proxy error", FATALERR_PROXY);

                arg_opts.proxy_port = (uint16_t) atoi(argv[optind-1]);
                break;

            case 'P':
                arg_opts.proxy_type = TOX_PROXY_TYPE_HTTP;
                snprintf(arg_opts.proxy_address, sizeof(arg_opts.proxy_address), "%s", optarg);

                if (++optind > argc || argv[optind-1][0] == '-')
                    exit_toxic_err("Proxy error", FATALERR_PROXY);

                arg_opts.proxy_port = (uint16_t) atoi(argv[optind-1]);
                break;

            case 'r':
                snprintf(arg_opts.dns_path, sizeof(arg_opts.dns_path), "%s", optarg);

                if (!file_exists(arg_opts.dns_path))
                    queue_init_message("DNSservers file not found");

                break;

            case 't':
                arg_opts.force_tcp = 1;
                break;

            case 'u':
                arg_opts.unencrypt_data = 1;
                break;

            case 'h':
            default:
                print_usage();
                exit(EXIT_SUCCESS);
        }
    }
}

#define DATANAME "data"
#define BLOCKNAME "data-blocklist"
static int init_default_data_files(void)
{
    if (arg_opts.use_custom_data)
        return 0;

    char *user_config_dir = get_user_config_dir();
    int config_err = create_user_config_dirs(user_config_dir);

    if (config_err) {
        DATA_FILE = strdup(DATANAME);
        BLOCK_FILE = strdup(BLOCKNAME);

        if (DATA_FILE == NULL || BLOCK_FILE == NULL)
            exit_toxic_err("failed in load_data_structures", FATALERR_MEMORY);
    } else {
        DATA_FILE = malloc(strlen(user_config_dir) + strlen(CONFIGDIR) + strlen(DATANAME) + 1);
        BLOCK_FILE = malloc(strlen(user_config_dir) + strlen(CONFIGDIR) + strlen(BLOCKNAME) + 1);

        if (DATA_FILE == NULL || BLOCK_FILE == NULL)
            exit_toxic_err("failed in load_data_structures", FATALERR_MEMORY);

        strcpy(DATA_FILE, user_config_dir);
        strcat(DATA_FILE, CONFIGDIR);
        strcat(DATA_FILE, DATANAME);

        strcpy(BLOCK_FILE, user_config_dir);
        strcat(BLOCK_FILE, CONFIGDIR);
        strcat(BLOCK_FILE, BLOCKNAME);
    }

    free(user_config_dir);
    return config_err;
}

#define REC_TOX_DO_LOOPS_PER_SEC 25

/* Adjusts usleep value so that tox_do runs close to the recommended number of times per second */
static useconds_t optimal_msleepval(uint64_t *looptimer, uint64_t *loopcount, uint64_t cur_time, useconds_t msleepval)
{
    useconds_t new_sleep = msleepval;
    ++(*loopcount);

    if (*looptimer == cur_time)
        return new_sleep;

    if (*loopcount != REC_TOX_DO_LOOPS_PER_SEC)
        new_sleep *= (double) *loopcount / REC_TOX_DO_LOOPS_PER_SEC;

    *looptimer = cur_time;
    *loopcount = 0;
    return new_sleep;
}

#ifdef X11
// FIXME
void DnD_callback(const char* asdv, DropType dt)
{
    if (dt != DT_plain)
        return;

   line_info_add(prompt, NULL, NULL, NULL, SYS_MSG, 0, 0, asdv);
}
#endif /* X11 */

int main(int argc, char *argv[])
{
    parse_args(argc, argv);

    /* Use the -b flag to enable stderr */
    if (!arg_opts.debug)
        freopen("/dev/null", "w", stderr);

    if (arg_opts.encrypt_data && arg_opts.unencrypt_data) {
        arg_opts.encrypt_data = 0;
        arg_opts.unencrypt_data = 0;
        queue_init_message("Warning: Using --unencrypt-data and --encrypt-data simultaneously has no effect");
    }

    /* Make sure all written files are read/writeable only by the current user. */
    umask(S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

    int config_err = init_default_data_files();
    bool datafile_exists = file_exists(DATA_FILE);

    if (datafile_exists)
        last_bootstrap_time = get_unix_time();

    if (!datafile_exists && !arg_opts.unencrypt_data)
        first_time_encrypt("Creating new data file. Would you like to encrypt it? Y/n (q to quit)");
    else if (arg_opts.encrypt_data)
        first_time_encrypt("Encrypt existing data file? Y/n (q to quit)");


    /* init user_settings struct and load settings from conf file */
    user_settings = calloc(1, sizeof(struct user_settings));

    if (user_settings == NULL)
        exit_toxic_err("failed in main", FATALERR_MEMORY);

    const char *p = arg_opts.config_path[0] ? arg_opts.config_path : NULL;
    int settings_err = settings_load(user_settings, p);

#ifdef X11
    if (init_xtra(DnD_callback) == -1)
        queue_init_message("X failed to initialize");
#endif

    Tox *m = load_toxic(DATA_FILE);

    if (arg_opts.encrypt_data && !datafile_exists)
        arg_opts.encrypt_data = 0;


    init_term();

    prompt = init_windows(m);
    prompt_init_statusbar(prompt, m);

    /* thread for ncurses stuff */
    if (pthread_mutex_init(&Winthread.lock, NULL) != 0)
        exit_toxic_err("failed in main", FATALERR_MUTEX_INIT);

    if (pthread_create(&Winthread.tid, NULL, thread_winref, (void *) m) != 0)
        exit_toxic_err("failed in main", FATALERR_THREAD_CREATE);

    /* thread for message queue */
    if (pthread_create(&cqueue_thread.tid, NULL, thread_cqueue, (void *) m) != 0)
        exit_toxic_err("failed in main", FATALERR_THREAD_CREATE);

#ifdef AUDIO

    av = init_audio(prompt, m);
    
//#ifdef VIDEO
//   av = init_video(prompt, m, av);
//
//#endif /* VIDEO*/

    /* audio thread */
    if (pthread_create(&audio_thread.tid, NULL, thread_audio, (void *) av) != 0)
        exit_toxic_err("failed in main", FATALERR_THREAD_CREATE);

    set_primary_device(input, user_settings->audio_in_dev);
    set_primary_device(output, user_settings->audio_out_dev);

#elif SOUND_NOTIFY
    if ( init_devices() == de_InternalError )
        queue_init_message("Failed to init audio devices");

#endif /* AUDIO */

    init_notify(60, 3000);

    const char *msg;

    if (config_err) {
        msg = "Unable to determine configuration directory. Defaulting to 'data' for data file...";
        queue_init_message("%s", msg);
    }

    if (settings_err == -1)
        queue_init_message("Failed to load user settings");

    /* screen/tmux auto-away timer */
    if (init_mplex_away_timer(m) == -1)
        queue_init_message("Failed to init mplex auto-away.");

    print_init_messages(prompt);
    cleanup_init_messages();

    /* set user avatar from config file. if no path is supplied tox_unset_avatar is called */
    char avatarstr[MAX_STR_SIZE];
    snprintf(avatarstr, sizeof(avatarstr), "/avatar \"%s\"", user_settings->avatar_path);
    execute(prompt->chatwin->history, prompt, m, avatarstr, GLOBAL_COMMAND_MODE);

    uint64_t last_save = (uint64_t) time(NULL);
    uint64_t looptimer = last_save;
    useconds_t msleepval = 40000;
    uint64_t loopcount = 0;

    while (true) {
        update_unix_time();
        do_toxic(m, prompt);
        uint64_t cur_time = get_unix_time();

        if (timed_out(last_save, cur_time, AUTOSAVE_FREQ)) {
            pthread_mutex_lock(&Winthread.lock);
            if (store_data(m, DATA_FILE) != 0)
                line_info_add(prompt, NULL, NULL, NULL, SYS_MSG, 0, RED, "WARNING: Failed to save to data file");
            pthread_mutex_unlock(&Winthread.lock);

            last_save = cur_time;
        }

        msleepval = optimal_msleepval(&looptimer, &loopcount, cur_time, msleepval);
        usleep(msleepval);
    }

    return 0;
}

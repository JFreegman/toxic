/*  main.c
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

#include <tox/tox.h>

#include "configdir.h"
#include "toxic.h"
#include "windows.h"
#include "friendlist.h"
#include "prompt.h"
#include "misc_tools.h"
#include "file_senders.h"
#include "line_info.h"
#include "settings.h"
#include "log.h"
#include "notify.h"
#include "device.h"
#include "message_queue.h"

#ifdef _AUDIO
#include "audio_call.h"
#endif /* _AUDIO */

#ifndef PACKAGE_DATADIR
#define PACKAGE_DATADIR "."
#endif

#ifdef _AUDIO
ToxAv *av;
#endif /* _AUDIO */

/* Export for use in Callbacks */
char *DATA_FILE = NULL;
char *BLOCK_FILE = NULL;
ToxWindow *prompt = NULL;

#define AUTOSAVE_FREQ 60

struct _Winthread Winthread;
struct _cqueue_thread cqueue_thread;
struct arg_opts arg_opts;
struct user_settings *user_settings_ = NULL;

static void catch_SIGINT(int sig)
{
    Winthread.sig_exit_toxic = true;
}

static void catch_SIGSEGV(int sig)
{
    freopen("/dev/tty", "w", stderr);
    endwin();
    fprintf(stderr, "Caught SIGSEGV: Aborting toxic session.\n");
    exit(EXIT_FAILURE);
}

static void flag_window_resize(int sig)
{
    Winthread.flag_resize = true;
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
    close_all_file_senders(m);
    kill_all_windows(m);

    free(DATA_FILE);
    free(BLOCK_FILE);
    free(user_settings_);

#ifdef _SOUND_NOTIFY
//     sound_notify(NULL, self_log_out, NT_ALWAYS, NULL);
#endif /* _SOUND_NOTIFY */
    terminate_notify();
#ifdef _AUDIO
    terminate_audio();
#endif /* _AUDIO */
    tox_kill(m);
    endwin();
    exit(EXIT_SUCCESS);
}

void exit_toxic_err(const char *errmsg, int errcode)
{
    if (errmsg == NULL)
        errmsg = "No error message";

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
                           "disable unicode support with the -d flag.", FATALERR_LOCALE_SET);
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

        if (user_settings_->colour_theme == NATIVE_COLS) {
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
static void queue_init_message(const char *msg)
{
    int i = init_messages.num;
    ++init_messages.num;

    char **new_msgs = realloc(init_messages.msgs, sizeof(char *) * init_messages.num);

    if (new_msgs == NULL)
        exit_toxic_err("Failed in queue_init_message", FATALERR_MEMORY);

    new_msgs[i] = malloc(MAX_STR_SIZE);

    if (new_msgs[i] == NULL)
        exit_toxic_err("Failed in queue_init_message", FATALERR_MEMORY);

    snprintf(new_msgs[i], MAX_STR_SIZE, "%s", msg);
    init_messages.msgs = new_msgs;
}

/* called after messages have been printed to console and are no longer needed */
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

static Tox *init_tox(void)
{
    Tox_Options tox_opts;
    tox_opts.ipv6enabled = !arg_opts.use_ipv4;
    tox_opts.udp_disabled = arg_opts.force_tcp;
    tox_opts.proxy_enabled = arg_opts.use_proxy;

    if (tox_opts.proxy_enabled) {
        tox_opts.proxy_port = arg_opts.proxy_port;
        snprintf(tox_opts.proxy_address, sizeof(tox_opts.proxy_address), "%s", arg_opts.proxy_address);

        char tmp[48];
        snprintf(tmp, sizeof(tmp), "Using proxy %s : %d", 
                 arg_opts.proxy_address, arg_opts.proxy_port);
        queue_init_message(tmp);
    }

    if (tox_opts.udp_disabled) {
        queue_init_message("UDP disabled");
    } else if (tox_opts.proxy_enabled) {
        const char *msg = "WARNING: Using a proxy without disabling UDP may leak your real IP address.";
        queue_init_message(msg);
        msg = "Use the -t option to disable UDP.";
        queue_init_message(msg);
    }

    /* Init core */
    Tox *m = tox_new(&tox_opts);

    if (tox_opts.ipv6enabled && m == NULL) {
        queue_init_message("IPv6 failed to initialize");
        tox_opts.ipv6enabled = 0;
        m = tox_new(&tox_opts);
    }

    if (!tox_opts.ipv6enabled)
        queue_init_message("Forcing IPv4 connection");

    if (tox_opts.proxy_enabled && m == NULL)
        exit_toxic_err("Proxy error", FATALERR_PROXY);

    if (m == NULL)
        return NULL;

    /* Callbacks */
    tox_callback_connection_status(m, on_connectionchange, NULL);
    tox_callback_typing_change(m, on_typing_change, NULL);
    tox_callback_friend_request(m, on_request, NULL);
    tox_callback_friend_message(m, on_message, NULL);
    tox_callback_name_change(m, on_nickchange, NULL);
    tox_callback_user_status(m, on_statuschange, NULL);
    tox_callback_status_message(m, on_statusmessagechange, NULL);
    tox_callback_friend_action(m, on_action, NULL);
    tox_callback_group_invite(m, on_groupinvite, NULL);
    tox_callback_group_message(m, on_groupmessage, NULL);
    tox_callback_group_action(m, on_groupaction, NULL);
    tox_callback_group_namelist_change(m, on_group_namelistchange, NULL);
    tox_callback_file_send_request(m, on_file_sendrequest, NULL);
    tox_callback_file_control(m, on_file_control, NULL);
    tox_callback_file_data(m, on_file_data, NULL);
    tox_callback_read_receipt(m, on_read_receipt, NULL);

#ifdef __linux__
    tox_set_name(m, (uint8_t *) "Cool dude", strlen("Cool dude"));
#elif defined(__FreeBSD__)
    tox_set_name(m, (uint8_t *) "Nerd", strlen("Nerd"));
#elif defined(__APPLE__)
    tox_set_name(m, (uint8_t *) "Hipster", strlen("Hipster")); /* This used to users of other Unixes are hipsters */
#else
    tox_set_name(m, (uint8_t *) "Registered Minix user #4", strlen("Registered Minix user #4"));
#endif

    return m;
}

#define MIN_NODE_LINE  50 /* IP: 7 + port: 5 + key: 38 + spaces: 2 = 70. ! (& e.g. tox.im = 6) */
#define MAX_NODE_LINE  256 /* Approx max number of chars in a sever line (name + port + key) */
#define MAXNODES 50
#define NODELEN (MAX_NODE_LINE - TOX_CLIENT_ID_SIZE - 7)

static struct _toxNodes {
    int lines;
    char nodes[MAXNODES][NODELEN];
    uint16_t ports[MAXNODES];
    char keys[MAXNODES][TOX_CLIENT_ID_SIZE];
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
            memcpy(toxNodes.keys[toxNodes.lines], key_binary, TOX_CLIENT_ID_SIZE);
            free(key_binary);

            toxNodes.lines++;
        }
    }

    fclose(fp);

    if (toxNodes.lines < 1)
        return 1;

    return 0;
}

int init_connection_helper(Tox *m, int line)
{
    return tox_bootstrap_from_address(m, toxNodes.nodes[line], toxNodes.ports[line], (uint8_t *) toxNodes.keys[line]);
}

/* Connects to a random DHT node listed in the DHTnodes file
 *
 * return codes:
 * 1: failed to open node file
 * 2: no line of sufficient length in node file
 * 3: failed to resolve name to IP
 * 4: nodelist file contains no acceptable line
 */
static bool srvlist_loaded = false;

#define NUM_INIT_NODES 5

int init_connection(Tox *m)
{
    if (toxNodes.lines > 0) /* already loaded nodelist */
        return init_connection_helper(m, rand() % toxNodes.lines) ? 0 : 3;

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
            if (init_connection_helper(m, rand() % toxNodes.lines))
                res = 0;
        }

        return res;
    }

    /* empty nodelist file */
    return 4;
}

#define TRY_CONNECT 10   /* Seconds between connection attempts when DHT is not connected */

static void do_connection(Tox *m, ToxWindow *prompt)
{
    if (arg_opts.no_connect == 1)
        return;

    char msg[MAX_STR_SIZE] = {0};

    static int conn_err = 0;
    static bool was_connected = false;
    static uint64_t last_conn_try = 0;
    uint64_t curtime = get_unix_time();
    bool is_connected = tox_isconnected(m);

    if (was_connected && is_connected)
        return;

    if (!was_connected && is_connected) {
        was_connected = true;
        prompt_update_connectionstatus(prompt, was_connected);
        snprintf(msg, sizeof(msg), "DHT connected");
    } else if (was_connected && !is_connected) {
        was_connected = false;
        prompt_update_connectionstatus(prompt, was_connected);
        snprintf(msg, sizeof(msg), "DHT disconnected. Attempting to reconnect.");
    } else if (!was_connected && !is_connected && timed_out(last_conn_try, curtime, TRY_CONNECT)) {
        /* if autoconnect has already failed there's no point in trying again */
        if (conn_err == 0) {
            last_conn_try = curtime;

            if ((conn_err = init_connection(m)) != 0)
                snprintf(msg, sizeof(msg), "Auto-connect failed with error code %d", conn_err);
        }
    }

    if (msg[0])
        line_info_add(prompt, NULL, NULL, NULL, SYS_MSG, 0, 0, msg);
}

static void load_friendlist(Tox *m)
{
    uint32_t i;
    uint32_t numfriends = tox_count_friendlist(m);

    for (i = 0; i < numfriends; ++i)
        friendlist_onFriendAdded(NULL, m, i, false);

    sort_friendlist_index();
}

/*
 * Store Messenger to given location
 * Return 0 stored successfully or ignoring data file
 * Return -1 file path is NULL
 * Return -2 malloc failed
 * Return -3 opening path failed
 * Return -4 fwrite failed
 */
int store_data(Tox *m, char *path)
{
    if (arg_opts.ignore_data_file)
        return 0;

    if (path == NULL)
        return -1;

    int len = tox_size(m);
    char *buf = malloc(len);

    if (buf == NULL)
        return -2;

    tox_save(m, (uint8_t *) buf);

    FILE *fd = fopen(path, "wb");

    if (fd == NULL) {
        free(buf);
        return -3;
    }

    if (fwrite(buf, len, 1, fd) != 1) {
        free(buf);
        fclose(fd);
        return -4;
    }

    free(buf);
    fclose(fd);
    return 0;
}

static void load_data(Tox *m, char *path)
{
    if (arg_opts.ignore_data_file)
        return;

    FILE *fd;

    if ((fd = fopen(path, "rb")) != NULL) {
        fseek(fd, 0, SEEK_END);
        int len = ftell(fd);
        fseek(fd, 0, SEEK_SET);

        char *buf = malloc(len);

        if (buf == NULL) {
            fclose(fd);
            exit_toxic_err("failed in load_data", FATALERR_MEMORY);
        }

        if (fread(buf, len, 1, fd) != 1) {
            free(buf);
            fclose(fd);
            exit_toxic_err("failed in load_data", FATALERR_FREAD);
        }

        tox_load(m, (uint8_t *) buf, len);
        load_friendlist(m);
        load_blocklist(BLOCK_FILE);

        free(buf);
        fclose(fd);
    } else {
        if (store_data(m, path) != 0)
            exit_toxic_err("failed in load_data", FATALERR_STORE_DATA);
    }
}

static void do_toxic(Tox *m, ToxWindow *prompt)
{
    pthread_mutex_lock(&Winthread.lock);
    do_connection(m, prompt);
    do_file_senders(m);

    if (arg_opts.no_connect == 0)
        tox_do(m);    /* main tox-core loop */

    pthread_mutex_unlock(&Winthread.lock);
}

#define INACTIVE_WIN_REFRESH_RATE 10

void *thread_winref(void *data)
{
    Tox *m = (Tox *) data;
    uint8_t draw_count = 0;

    while (true) {
        draw_active_window(m);
        draw_count++;

        if (Winthread.flag_resize) {
            on_window_resize();
            Winthread.flag_resize = false;
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
        int i;

        for (i = 0; i < MAX_WINDOWS_NUM; ++i) {
            ToxWindow *toxwin = get_window_ptr(i);

            if (toxwin != NULL && toxwin->is_chat && tox_get_friend_connection_status(m, toxwin->num) == 1)
                cqueue_try_send(toxwin, m, toxwin->num);
        }

        pthread_mutex_unlock(&Winthread.lock);
        usleep(7000);
    }
}

static void print_usage(void)
{
    fprintf(stderr, "usage: toxic [OPTION] [FILE ...]\n");
    fprintf(stderr, "  -4, --ipv4               Force IPv4 connection\n");
    fprintf(stderr, "  -b  --debug              Enable stderr for debugging\n");
    fprintf(stderr, "  -c, --config             Use specified config file\n");
    fprintf(stderr, "  -d, --default-locale     Use default POSIX locale\n");
    fprintf(stderr, "  -f, --file               Use specified data file\n");
    fprintf(stderr, "  -h, --help               Show this message and exit\n");
    fprintf(stderr, "  -n, --nodes              Use specified DHTnodes file\n");
    fprintf(stderr, "  -o, --noconnect          Do not connect to the DHT network\n");
    fprintf(stderr, "  -p, --proxy              Use proxy: Requires [IP] [port]\n");
    fprintf(stderr, "  -r, --dnslist            Use specified DNSservers file\n");
    fprintf(stderr, "  -t, --force-tcp          Force TCP connection (use this with proxies)\n");
    fprintf(stderr, "  -x, --nodata             Ignore data file\n");
}

static void set_default_opts(void)
{
    arg_opts.use_ipv4 = 0;
    arg_opts.ignore_data_file = 0;
    arg_opts.debug = 0;
    arg_opts.default_locale = 0;
    arg_opts.use_custom_data = 0;
    arg_opts.no_connect = 0;
    arg_opts.force_tcp = 0;
    arg_opts.use_proxy = 0;
}

static void parse_args(int argc, char *argv[])
{
    set_default_opts();

    static struct option long_opts[] = {
        {"file", required_argument, 0, 'f'},
        {"nodata", no_argument, 0, 'x'},
        {"ipv4", no_argument, 0, '4'},
        {"debug", no_argument, 0, 'b'},
        {"default-locale", no_argument, 0, 'd'},
        {"config", required_argument, 0, 'c'},
        {"nodes", required_argument, 0, 'n'},
        {"help", no_argument, 0, 'h'},
        {"noconnect", no_argument, 0, 'o'},
        {"dnslist", required_argument, 0, 'r'},
        {"force-tcp", no_argument, 0, 't'},
        {"proxy", required_argument, 0, 'p'},
        {NULL, no_argument, NULL, 0},
    };

    const char *opts_str = "4bdhotxc:f:n:r:p:";
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

            case 'f':
                arg_opts.use_custom_data = 1;
                DATA_FILE = strdup(optarg);
                BLOCK_FILE = malloc(strlen(optarg) + strlen("-blocklist") + 1);

                if (DATA_FILE == NULL || BLOCK_FILE == NULL)
                    exit_toxic_err("failed in parse_args", FATALERR_MEMORY);

                strcpy(BLOCK_FILE, optarg);
                strcat(BLOCK_FILE, "-blocklist");

                char tmp[PATH_MAX];
                snprintf(tmp, sizeof(tmp), "Using '%s' data file", DATA_FILE);
                queue_init_message(tmp);
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
                arg_opts.use_proxy = 1;
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

            case 'x':
                arg_opts.ignore_data_file = 1;
                queue_init_message("Ignoring data file");
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
static int init_data_files(void)
{
    if (arg_opts.use_custom_data)
        return 0;

    char *user_config_dir = get_user_config_dir();
    int config_err = create_user_config_dirs(user_config_dir);

    if (DATA_FILE == NULL ) {
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

int main(int argc, char *argv[])
{
    init_signal_catchers();
    parse_args(argc, argv);

    /* Make sure all written files are read/writeable only by the current user. */
    umask(S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    int config_err = init_data_files();

    /* init user_settings struct and load settings from conf file */
    user_settings_ = calloc(1, sizeof(struct user_settings));

    if (user_settings_ == NULL)
        exit_toxic_err("failed in main", FATALERR_MEMORY);

    char *p = arg_opts.config_path[0] ? arg_opts.config_path : NULL;
    int settings_err = settings_load(user_settings_, p);

    Tox *m = init_tox();
    init_term();

    /* enable stderr for debugging */
    if (!arg_opts.debug)
        freopen("/dev/null", "w", stderr);

    if (m == NULL)
        exit_toxic_err("failed in main", FATALERR_NETWORKINIT);

    if (!arg_opts.ignore_data_file)
        load_data(m, DATA_FILE);

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

#ifdef _AUDIO

    av = init_audio(prompt, m);

    set_primary_device(input, user_settings_->audio_in_dev);
    set_primary_device(output, user_settings_->audio_out_dev);

#elif _SOUND_NOTIFY
    if ( init_devices() == de_InternalError )
        queue_init_message("Failed to init audio devices");

#endif /* _AUDIO */
    
    init_notify(60, 3000);

#ifdef _SOUND_NOTIFY
//     sound_notify(prompt, self_log_in, 0, NULL);
#endif /* _SOUND_NOTIFY */
    
    const char *msg;
    
    if (config_err) {
        msg = "Unable to determine configuration directory. Defaulting to 'data' for data file...";
        queue_init_message(msg);
    }

    if (settings_err == -1)
        queue_init_message("Failed to load user settings");

    print_init_messages(prompt);
    cleanup_init_messages();

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
            store_data(m, DATA_FILE);
            pthread_mutex_unlock(&Winthread.lock);

            last_save = cur_time;
        }

        msleepval = optimal_msleepval(&looptimer, &loopcount, cur_time, msleepval);
        usleep(msleepval);
    }

    return 0;
}

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

#ifndef SIGWINCH
#define SIGWINCH 28
#endif

#ifndef SIGINT
#define SIGINT 2
#endif

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

#ifdef _SUPPORT_AUDIO
#include "audio_call.h"
#endif /* _SUPPORT_AUDIO */

#ifndef PACKAGE_DATADIR
#define PACKAGE_DATADIR "."
#endif

#ifdef _SUPPORT_AUDIO
ToxAv *av;
#endif /* _SUPPORT_AUDIO */

/* Export for use in Callbacks */
char *DATA_FILE = NULL;
ToxWindow *prompt = NULL;

struct arg_opts {
    int ignore_data_file;
    int use_ipv4;
    char config_path[MAX_STR_SIZE];
    char nodes_path[MAX_STR_SIZE];
} arg_opts;

struct _Winthread Winthread;
struct _FSenderThread FSenderThread;

struct user_settings *user_settings = NULL;

static void ignore_SIGINT(int sig)
{
    return;
}

void exit_toxic_success(Tox *m)
{
    store_data(m, DATA_FILE);
    close_all_file_senders();
    kill_all_windows();
    log_disable(prompt->chatwin->log);
    line_info_cleanup(prompt->chatwin->hst);
    free(DATA_FILE);
    free(prompt->stb);
    free(prompt->chatwin->log);
    free(prompt->chatwin->hst);
    free(prompt->chatwin);
    free(user_settings);
#ifdef _SUPPORT_AUDIO
    terminate_audio();
#endif /* _SUPPORT_AUDIO */
    tox_kill(m);
    endwin();
    fprintf(stderr, "Toxic session ended gracefully.\n");
    exit(EXIT_SUCCESS);
}

void exit_toxic_err(const char *errmsg, int errcode)
{
    if (errmsg == NULL)
        errmsg = "No error message";

    endwin();
    fprintf(stderr, "Toxic session aborted with error code %d (%s)\n", errcode, errmsg);
    exit(EXIT_FAILURE);
}

static void init_term(void)
{
    signal(SIGWINCH, on_window_resize);

#if HAVE_WIDECHAR

    if (setlocale(LC_ALL, "") == NULL)
        exit_toxic_err("Could not set your locale, please check your locale settings or"
                       "disable wide char support", FATALERR_LOCALE_SET);
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

static Tox *init_tox(int ipv4)
{
    /* Init core */
    int ipv6 = !ipv4;
    Tox *m = tox_new(ipv6);

    /*
    * TOX_ENABLE_IPV6_DEFAULT is always 1.
    * Checking it is redundant, this *should* be doing ipv4 fallback
    */
    if (ipv6 && m == NULL) {
        fprintf(stderr, "IPv6 didn't initialize, trying IPv4\n");
        m = tox_new(0);
    }

    if (ipv4)
        fprintf(stderr, "Forcing IPv4 connection\n");

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

#define MINLINE  50 /* IP: 7 + port: 5 + key: 38 + spaces: 2 = 70. ! (& e.g. tox.im = 6) */
#define MAXLINE  256 /* Approx max number of chars in a sever line (name + port + key) */
#define MAXNODES 50
#define NODELEN (MAXLINE - TOX_CLIENT_ID_SIZE - 7)

static struct _toxNodes {
    int lines;
    char nodes[MAXNODES][NODELEN];
    uint16_t ports[MAXNODES];
    uint8_t keys[MAXNODES][TOX_CLIENT_ID_SIZE];
} toxNodes;

static int nodelist_load(char *filename)
{
    if (!filename)
        return 1;

    FILE *fp = fopen(filename, "r");

    if (fp == NULL)
        return 1;

    char line[MAXLINE];

    while (fgets(line, sizeof(line), fp) && toxNodes.lines < MAXNODES) {
        if (strlen(line) > MINLINE) {
            const char *name = strtok(line, " ");
            const char *port = strtok(NULL, " ");
            const char *key_ascii = strtok(NULL, " ");

            /* invalid line */
            if (name == NULL || port == NULL || key_ascii == NULL)
                continue;

            snprintf(toxNodes.nodes[toxNodes.lines], sizeof(toxNodes.nodes[toxNodes.lines]), "%s", name);
            toxNodes.nodes[toxNodes.lines][NODELEN - 1] = 0;
            toxNodes.ports[toxNodes.lines] = htons(atoi(port));

            uint8_t *key_binary = hex_string_to_bin(key_ascii);
            memcpy(toxNodes.keys[toxNodes.lines], key_binary, TOX_CLIENT_ID_SIZE);
            free(key_binary);

            toxNodes.lines++;
        }
    }

    if (toxNodes.lines < 1) {
        fclose(fp);
        return 2;
    }

    fclose(fp);
    return 0;
}

int init_connection_helper(Tox *m, int line)
{
    return tox_bootstrap_from_address(m, toxNodes.nodes[line], TOX_ENABLE_IPV6_DEFAULT,
                                      toxNodes.ports[line], toxNodes.keys[line]);
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
            res = nodelist_load(PACKAGE_DATADIR "/DHTnodes");
        else
            res = nodelist_load(arg_opts.nodes_path);

        if (toxNodes.lines < 1)
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

#define TRY_CONNECT 10

static void do_connection(Tox *m, ToxWindow *prompt)
{
    uint8_t msg[MAX_STR_SIZE] = {0};

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
        snprintf(msg, sizeof(msg), "DHT connected.");
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
        line_info_add(prompt, NULL, NULL, NULL, msg, SYS_MSG, 0, 0);
}

static void load_friendlist(Tox *m)
{
    uint32_t i;
    uint32_t numfriends = tox_count_friendlist(m);

    for (i = 0; i < numfriends; ++i)
        friendlist_onFriendAdded(NULL, m, i, false);
}

/*
 * Store Messenger to given location
 * Return 0 stored successfully
 * Return 1 file path is NULL
 * Return 2 malloc failed
 * Return 3 opening path failed
 * Return 4 fwrite failed
 */
int store_data(Tox *m, char *path)
{
    if (arg_opts.ignore_data_file)
        return 0;

    if (path == NULL)
        return 1;

    FILE *fd;
    size_t len;
    uint8_t *buf;

    len = tox_size(m);
    buf = malloc(len);

    if (buf == NULL)
        return 2;

    tox_save(m, buf);

    fd = fopen(path, "wb");

    if (fd == NULL) {
        free(buf);
        return 3;
    }

    if (fwrite(buf, len, 1, fd) != 1) {
        free(buf);
        fclose(fd);
        return 4;
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
    size_t len;
    uint8_t *buf;

    if ((fd = fopen(path, "rb")) != NULL) {
        fseek(fd, 0, SEEK_END);
        len = ftell(fd);
        fseek(fd, 0, SEEK_SET);

        buf = malloc(len);

        if (buf == NULL) {
            fclose(fd);
            exit_toxic_err("failed in load_data", FATALERR_MEMORY);
        }

        if (fread(buf, len, 1, fd) != 1) {
            free(buf);
            fclose(fd);
            exit_toxic_err("failed in load_data", FATALERR_FREAD);
        }

        tox_load(m, buf, len);
        load_friendlist(m);

        free(buf);
        fclose(fd);
    } else {
        int st;

        if ((st = store_data(m, path)) != 0)
            exit_toxic_err("failed in load_data", FATALERR_STORE_DATA);
    }
}

static void do_toxic(Tox *m, ToxWindow *prompt)
{
    pthread_mutex_lock(&Winthread.lock);
    do_connection(m, prompt);
    pthread_mutex_unlock(&Winthread.lock);

    tox_do(m);    /* main tox-core loop */
}

void *thread_winref(void *data)
{
    Tox *m = (Tox *) data;

    while (true) {
        draw_active_window(m);
        refresh_inactive_windows();
    }
}

void *thread_filesenders(void *data)
{
    Tox *m = (Tox *) data;

    while (true) {
        pthread_mutex_lock(&FSenderThread.lock);
        do_file_senders(m);
        pthread_mutex_unlock(&FSenderThread.lock);

        usleep(60000);
    }
}

static void print_usage(void)
{
    fprintf(stderr, "usage: toxic [OPTION] [FILE ...]\n");
    fprintf(stderr, "  -f, --file           Use specified data file\n");
    fprintf(stderr, "  -x, --nodata         Ignore data file\n");
    fprintf(stderr, "  -4, --ipv4           Force IPv4 connection\n");
    fprintf(stderr, "  -c, --config         Use specified config file\n");
    fprintf(stderr, "  -n, --nodes          Use specified DHTnodes file\n");
    fprintf(stderr, "  -h, --help           Show this message and exit\n");
}

static void set_default_opts(void)
{
    arg_opts.use_ipv4 = 0;
    arg_opts.ignore_data_file = 0;
}

static void parse_args(int argc, char *argv[])
{
    set_default_opts();

    static struct option long_opts[] = {
        {"file", required_argument, 0, 'f'},
        {"nodata", no_argument, 0, 'x'},
        {"ipv4", no_argument, 0, '4'},
        {"config", required_argument, 0, 'c'},
        {"nodes", required_argument, 0, 'n'},
        {"help", no_argument, 0, 'h'},
    };

    const char *opts_str = "4xf:c:n:h";
    int opt, indexptr;

    while ((opt = getopt_long(argc, argv, opts_str, long_opts, &indexptr)) != -1) {
        switch (opt) {
            case 'f':
                DATA_FILE = strdup(optarg);
                break;

            case 'x':
                arg_opts.ignore_data_file = 1;
                break;

            case '4':
                arg_opts.use_ipv4 = 1;
                break;

            case 'c':
                snprintf(arg_opts.config_path, sizeof(arg_opts.config_path), "%s", optarg);
                break;

            case 'n':
                snprintf(arg_opts.nodes_path, sizeof(arg_opts.nodes_path), "%s", optarg);
                break;

            case 'h':
            default:
                print_usage();
                exit(EXIT_SUCCESS);
        }
    }
}

int main(int argc, char *argv[])
{
    char *user_config_dir = get_user_config_dir();
    int config_err = 0;

    parse_args(argc, argv);

    /* Make sure all written files are read/writeable only by the current user. */
    umask(S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

    signal(SIGINT, ignore_SIGINT);

    config_err = create_user_config_dir(user_config_dir);

    if (DATA_FILE == NULL ) {
        if (config_err) {
            DATA_FILE = strdup("data");
        } else {
            DATA_FILE = malloc(strlen(user_config_dir) + strlen(CONFIGDIR) + strlen("data") + 1);

            if (DATA_FILE == NULL)
                exit_toxic_err("failed in main", FATALERR_MEMORY);

            strcpy(DATA_FILE, user_config_dir);
            strcat(DATA_FILE, CONFIGDIR);
            strcat(DATA_FILE, "data");
        }
    }

    free(user_config_dir);

    /* init user_settings struct and load settings from conf file */
    user_settings = malloc(sizeof(struct user_settings));

    if (user_settings == NULL)
        exit_toxic_err("failed in main", FATALERR_MEMORY);

    memset(user_settings, 0, sizeof(struct user_settings));

    char *p = arg_opts.config_path[0] ? arg_opts.config_path : NULL;
    int settings_err = settings_load(user_settings, p);

    Tox *m = init_tox(arg_opts.use_ipv4);
    init_term();

    if (m == NULL)
        exit_toxic_err("failed in main", FATALERR_NETWORKINIT);

    if (!arg_opts.ignore_data_file)
        load_data(m, DATA_FILE);

    prompt = init_windows(m);

    /* thread for ncurses stuff */
    if (pthread_mutex_init(&Winthread.lock, NULL) != 0)
        exit_toxic_err("failed in main", FATALERR_MUTEX_INIT);

    if (pthread_create(&Winthread.tid, NULL, thread_winref, (void *) m) != 0)
        exit_toxic_err("failed in main", FATALERR_THREAD_CREATE);
    
    /* thread for filesenders */
    if (pthread_mutex_init(&FSenderThread.lock, NULL) != 0)
        exit_toxic_err("failed in main", FATALERR_MUTEX_INIT);

    if (pthread_create(&FSenderThread.tid, NULL, thread_filesenders, (void *) m) != 0)
        exit_toxic_err("failed in main", FATALERR_THREAD_CREATE);

    uint8_t *msg;

#ifdef _SUPPORT_AUDIO

    av = init_audio(prompt, m);
    
    
    set_primary_device(input, user_settings->audio_in_dev);
    set_primary_device(output, user_settings->audio_out_dev);
    
#endif /* _SUPPORT_AUDIO */

    if (config_err) {
        msg = "Unable to determine configuration directory. Defaulting to 'data' for a keyfile...";
        line_info_add(prompt, NULL, NULL, NULL, msg, SYS_MSG, 0, 0);
    }

    if (settings_err == -1) {
        msg = "Failed to load user settings";
        line_info_add(prompt, NULL, NULL, NULL, msg, SYS_MSG, 0, 0);
    }


    sort_friendlist_index();
    prompt_init_statusbar(prompt, m);

    while (true) {
        update_unix_time();
        do_toxic(m, prompt);
        usleep(40000);
    }

    return 0;
}

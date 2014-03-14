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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef SIGWINCH
    #define SIGWINCH 28
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

#ifdef _WIN32
    #include <direct.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
#include <netdb.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <tox/tox.h>

#include "configdir.h"
#include "toxic_windows.h"
#include "friendlist.h"
#include "prompt.h"
#include "misc_tools.h"
#include "file_senders.h"

#ifdef _SUPPORT_AUDIO
    #include "audio_call.h"
#endif /* _SUPPORT_AUDIO */

#ifndef PACKAGE_DATADIR
#define PACKAGE_DATADIR "."
#endif

#ifdef _SUPPORT_AUDIO
    ToxAv* av;
#endif /* _SUPPORT_AUDIO */

/* Export for use in Callbacks */
char *DATA_FILE = NULL;
ToxWindow *prompt = NULL;

static int f_loadfromfile;    /* 1 if we want to load from/save the data file, 0 otherwise */

struct _Winthread Winthread;

void on_window_resize(int sig)
{
    endwin();
    refresh();
    clear();
}

static void init_term(void)
{
    /* Setup terminal */
    signal(SIGWINCH, on_window_resize);
#if HAVE_WIDECHAR
    if (setlocale(LC_ALL, "") == NULL) {
        fprintf(stderr, "Could not set your locale, plese check your locale settings or"
               "disable wide char support\n");
        exit(EXIT_FAILURE);
    }
#endif
    initscr();
    cbreak();
    keypad(stdscr, 1);
    noecho();
    timeout(100);

    if (has_colors()) {
        start_color();
        init_pair(0, COLOR_WHITE, COLOR_BLACK);
        init_pair(1, COLOR_GREEN, COLOR_BLACK);
        init_pair(2, COLOR_CYAN, COLOR_BLACK);
        init_pair(3, COLOR_RED, COLOR_BLACK);
        init_pair(4, COLOR_BLUE, COLOR_BLACK);
        init_pair(5, COLOR_YELLOW, COLOR_BLACK);
        init_pair(6, COLOR_MAGENTA, COLOR_BLACK);
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
    tox_set_name(m, (uint8_t *) "Cool guy", sizeof("Cool guy"));
#elif defined(_WIN32)
    tox_set_name(m, (uint8_t *) "I should install GNU/Linux", sizeof("I should install GNU/Linux"));
#elif defined(__APPLE__)
    tox_set_name(m, (uint8_t *) "Hipster", sizeof("Hipster")); /* This used to users of other Unixes are hipsters */
#else
    tox_set_name(m, (uint8_t *) "Registered Minix user #4", sizeof("Registered Minix user #4"));
#endif

    return m;
}

#define MINLINE    50 /* IP: 7 + port: 5 + key: 38 + spaces: 2 = 70. ! (& e.g. tox.im = 6) */
#define MAXLINE   256 /* Approx max number of chars in a sever line (name + port + key) */
#define MAXNODES 50
#define NODELEN (MAXLINE - TOX_CLIENT_ID_SIZE - 7)

static int  linecnt = 0;
static char nodes[MAXNODES][NODELEN];
static uint16_t ports[MAXNODES];
static uint8_t keys[MAXNODES][TOX_CLIENT_ID_SIZE];

static int nodelist_load(char *filename)
{
    if (!filename)
        return 1;

    FILE *fp = fopen(filename, "r");

    if (fp == NULL)
        return 1;

    char line[MAXLINE];
    while (fgets(line, sizeof(line), fp) && linecnt < MAXNODES) {
        if (strlen(line) > MINLINE) {
            char *name = strtok(line, " ");
            char *port = strtok(NULL, " ");
            char *key_ascii = strtok(NULL, " ");
            /* invalid line */
            if (name == NULL || port == NULL || key_ascii == NULL)
                continue;

            strncpy(nodes[linecnt], name, NODELEN);
            nodes[linecnt][NODELEN - 1] = 0;
            ports[linecnt] = htons(atoi(port));

            uint8_t *key_binary = hex_string_to_bin(key_ascii);
            memcpy(keys[linecnt], key_binary, TOX_CLIENT_ID_SIZE);
            free(key_binary);

            linecnt++;
        }
    }

    if (linecnt < 1) {
        fclose(fp);
        return 2;
    }

    fclose(fp);
    return 0;
}

int init_connection_helper(Tox *m, int line)
{
    return tox_bootstrap_from_address(m, nodes[line], TOX_ENABLE_IPV6_DEFAULT,
                                                ports[line], keys[line]);
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
    if (linecnt > 0) /* already loaded nodelist */
        return init_connection_helper(m, rand() % linecnt) ? 0 : 3;

    /* only once:
     * - load the nodelist
     * - connect to "everyone" inside
     */
    if (!srvlist_loaded) {
        srvlist_loaded = true;
        int res = nodelist_load(PACKAGE_DATADIR "/DHTnodes");

        if (linecnt < 1)
            return res;

        res = 3;
        int i;
        int n = MIN(NUM_INIT_NODES, linecnt);

        for(i = 0; i < n; ++i) {
            if (init_connection_helper(m, rand() % linecnt))
                res = 0;
        }

        return res;
    }

    /* empty nodelist file */
    return 4;
}

static void do_connection(Tox *m, ToxWindow *prompt)
{
    static int conn_try = 0;
    static int conn_err = 0;
    static bool dht_on = false;

    bool is_connected = tox_isconnected(m);

    if (!dht_on && !is_connected && !(conn_try++ % 100)) {
        if (!conn_err) {
            if ((conn_err = init_connection(m))) {
                prep_prompt_win();
                wprintw(prompt->window, "\nAuto-connect failed with error code %d\n", conn_err);
            }
        }
    } else if (!dht_on && is_connected) {
        dht_on = true;
        prompt_update_connectionstatus(prompt, dht_on);
        prep_prompt_win();
        wprintw(prompt->window, "DHT connected.\n");
    } else if (dht_on && !is_connected) {
        dht_on = false;
        prompt_update_connectionstatus(prompt, dht_on);
        prep_prompt_win();
        wprintw(prompt->window, "\nDHT disconnected. Attempting to reconnect.\n");
    }
}

static void load_friendlist(Tox *m)
{
    int i;
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
    if (f_loadfromfile == 0) /*If file loading/saving is disabled*/
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
    if (f_loadfromfile == 0) /*If file loading/saving is disabled*/
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
            endwin();
            fprintf(stderr, "malloc() failed. Aborting...\n");
            exit(EXIT_FAILURE);
        }

        if (fread(buf, len, 1, fd) != 1) {
            free(buf);
            fclose(fd);
            endwin();
            fprintf(stderr, "fread() failed. Aborting...\n");
            exit(EXIT_FAILURE);
        }

        tox_load(m, buf, len);
        load_friendlist(m);

        free(buf);
        fclose(fd);
    } else {
        int st;

        if ((st = store_data(m, path)) != 0) {
            endwin();
            fprintf(stderr, "Store messenger failed with return code: %d\n", st);
            exit(EXIT_FAILURE);
        }
    }
}

void exit_toxic(Tox *m)
{
    store_data(m, DATA_FILE);
    close_all_file_senders();
    kill_all_windows();
    log_disable(prompt->promptbuf->log);
    free(DATA_FILE);
    free(prompt->stb);
    free(prompt->promptbuf->log);
    free(prompt->promptbuf);
    tox_kill(m);
    #ifdef _SUPPORT_AUDIO
    terminate_audio();
    #endif /* _SUPPORT_AUDIO */
    endwin();
    exit(EXIT_SUCCESS);
}

static void do_toxic(Tox *m, ToxWindow *prompt)
{
    pthread_mutex_lock(&Winthread.lock);

    do_connection(m, prompt);
    do_file_senders(m);

    /* main tox-core loop */
    tox_do(m);

    pthread_mutex_unlock(&Winthread.lock);
}

void *thread_winref(void *data)
{
    Tox *m = (Tox *) data;

    while (true)
        draw_active_window(m);
}

int main(int argc, char *argv[])
{
    char *user_config_dir = get_user_config_dir();
    int config_err = 0;

    f_loadfromfile = 1;
    int f_flag = 0;
    int i = 0;
    int f_use_ipv4 = 0;

    /* Make sure all written files are read/writeable only by the current user. */
    umask(S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

    for (i = 0; i < argc; ++i) {
        if (argv[i] == NULL)
            break;
        else if (argv[i][0] == '-') {
            if (argv[i][1] == 'f') {
                if (argv[i + 1] != NULL)
                    DATA_FILE = strdup(argv[i + 1]);
                else
                    f_flag = -1;
            } else if (argv[i][1] == 'n') {
                f_loadfromfile = 0;
            } else if (argv[i][1] == '4') {
                f_use_ipv4 = 1;
            }
        }
    }

    config_err = create_user_config_dir(user_config_dir);
    if (DATA_FILE == NULL ) {
        if (config_err) {
            DATA_FILE = strdup("data");
        } else {
            DATA_FILE = malloc(strlen(user_config_dir) + strlen(CONFIGDIR) + strlen("data") + 1);
            if (DATA_FILE != NULL) {
                strcpy(DATA_FILE, user_config_dir);
                strcat(DATA_FILE, CONFIGDIR);
                strcat(DATA_FILE, "data");     
            } else {
                endwin();
                fprintf(stderr, "malloc() failed. Aborting...\n");
                exit(EXIT_FAILURE);
            }
        }
    }

    free(user_config_dir);

    init_term();
    Tox *m = init_tox(f_use_ipv4);

    if (m == NULL) {
        endwin();
        fprintf(stderr, "Failed to initialize network. Aborting...\n");
        exit(EXIT_FAILURE);
    }

    prompt = init_windows(m);

    /* create new thread for ncurses stuff */
    if (pthread_mutex_init(&Winthread.lock, NULL) != 0)
    {
        endwin();
        fprintf(stderr, "Mutex init failed. Aborting...\n");
        exit(EXIT_FAILURE);
    }

    if (pthread_create(&Winthread.tid, NULL, thread_winref, (void *) m) != 0) {
        endwin();
        fprintf(stderr, "Thread creation failed. Aborting...\n");
        exit(EXIT_FAILURE);
    }

#ifdef _SUPPORT_AUDIO 
    
    attron(COLOR_PAIR(RED) | A_BOLD);
    wprintw(prompt->window, "Starting audio...\n");
    attroff(COLOR_PAIR(RED) | A_BOLD);
    
    av = init_audio(prompt, m);
        
    if ( errors() == NoError )
        wprintw(prompt->window, "Audio started with no problems.\n");
    else /* Get error code and stuff */
        wprintw(prompt->window, "Error starting audio!\n");
    
    
#endif /* _SUPPORT_AUDIO */
    
    if (f_loadfromfile)
        load_data(m, DATA_FILE);

    if (f_flag == -1) {
        attron(COLOR_PAIR(RED) | A_BOLD);
        wprintw(prompt->window, "You passed '-f' without giving an argument.\n"
                "defaulting to 'data' for a keyfile...\n");
        attroff(COLOR_PAIR(RED) | A_BOLD);
    }

    if (config_err) {
        attron(COLOR_PAIR(RED) | A_BOLD);
        wprintw(prompt->window, "Unable to determine configuration directory.\n"
                "defaulting to 'data' for a keyfile...\n");
        attroff(COLOR_PAIR(RED) | A_BOLD);
    }

    sort_friendlist_index(m);
    prompt_init_statusbar(prompt, m);

    while (true) {
        do_toxic(m, prompt);
        usleep(10000);
    }
        
    return 0;
}

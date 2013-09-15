/*
 * Toxic -- Tox Curses Client
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

#ifdef _WIN32
    #include <direct.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
#include <netdb.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#endif

#include <tox/tox.h>

#include "configdir.h"
#include "toxic_windows.h"
#include "prompt.h"
#include "friendlist.h"

#ifndef PACKAGE_DATADIR
#define PACKAGE_DATADIR "."
#endif

/* Export for use in Callbacks */
char *DATA_FILE = NULL;
char *SRVLIST_FILE = NULL;
ToxWindow *prompt = NULL;

void on_window_resize(int sig)
{
    endwin();
    refresh();
    clear();
}

static void init_term()
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

static Tox *init_tox()
{
    /* Init core */
    Tox *m = tox_new(TOX_ENABLE_IPV6_DEFAULT);
    if (m == NULL)
        return NULL;

    /* Callbacks */
    tox_callback_connectionstatus(m, on_connectionchange, NULL);
    tox_callback_friendrequest(m, on_request, NULL);
    tox_callback_friendmessage(m, on_message, NULL);
    tox_callback_namechange(m, on_nickchange, NULL);
    tox_callback_userstatus(m, on_statuschange, NULL);
    tox_callback_statusmessage(m, on_statusmessagechange, NULL);
    tox_callback_action(m, on_action, NULL);
#ifdef __linux__
    tox_setname(m, (uint8_t *) "Cool guy", sizeof("Cool guy"));
#elif defined(_WIN32)
    tox_setname(m, (uint8_t *) "I should install GNU/Linux", sizeof("I should install GNU/Linux"));
#elif defined(__APPLE__)
    tox_setname(m, (uint8_t *) "Hipster", sizeof("Hipster")); //This used to users of other Unixes are hipsters
#else
    tox_setname(m, (uint8_t *) "Registered Minix user #4", sizeof("Registered Minix user #4"));
#endif
    return m;
}

#define MAXLINE 90    /* Approx max number of chars in a sever line (IP + port + key) */
#define MINLINE 70
#define MAXSERVERS 50

/* Connects to a random DHT server listed in the DHTservers file */
int init_connection(Tox *m)
{
    FILE *fp = NULL;

    fp = fopen(SRVLIST_FILE, "r");

    if (fp == NULL)
        return 1;

    char servers[MAXSERVERS][MAXLINE];
    char line[MAXLINE];
    int linecnt = 0;

    while (fgets(line, sizeof(line), fp) && linecnt < MAXSERVERS) {
        if (strlen(line) > MINLINE)
            strcpy(servers[linecnt++], line);
    }

    if (linecnt < 1) {
        fclose(fp);
        return 2;
    }

    fclose(fp);

    char *server = servers[rand() % linecnt];
    char *ip = strtok(server, " ");
    char *port = strtok(NULL, " ");
    char *key = strtok(NULL, " ");

    if (ip == NULL || port == NULL || key == NULL)
        return 3;

    uint8_t *binary_string = hex_string_to_bin(key);
    tox_bootstrap_from_address(m, ip, TOX_ENABLE_IPV6_DEFAULT,
                               htons(atoi(port)), binary_string);
    free(binary_string);
    return 0;
}

static void do_tox(Tox *m, ToxWindow *prompt)
{
    static int conn_try = 0;
    static int conn_err = 0;
    static bool dht_on = false;

    if (!dht_on && !tox_isconnected(m) && !(conn_try++ % 100)) {
        if (!conn_err) {
            conn_err = init_connection(m);
            wprintw(prompt->window, "\nEstablishing connection...\n");

            if (conn_err)
                wprintw(prompt->window, "\nAuto-connect failed with error code %d\n", conn_err);
        }
    } else if (!dht_on && tox_isconnected(m)) {
        dht_on = true;
        prompt_update_connectionstatus(prompt, dht_on);
        wprintw(prompt->window, "\nDHT connected.\n");
    } else if (dht_on && !tox_isconnected(m)) {
        dht_on = false;
        prompt_update_connectionstatus(prompt, dht_on);
        wprintw(prompt->window, "\nDHT disconnected. Attempting to reconnect.\n");
    }

    tox_do(m);
}

int f_loadfromfile;

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

    fd = fopen(path, "w");

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

    if ((fd = fopen(path, "r")) != NULL) {
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

        uint32_t i = 0;

        uint8_t name[TOX_MAX_NAME_LENGTH];
        while (tox_getname(m, i, name) != -1) {
            on_friendadded(m, i);
            i++;
        }

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
    free(DATA_FILE);
    free(SRVLIST_FILE);
    free(prompt->stb);
    tox_kill(m);
    endwin();
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
    char *user_config_dir = get_user_config_dir();
    int config_err = 0;

    f_loadfromfile = 1;
    int f_flag = 0;
    int i = 0;

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

    if (config_err) {
        SRVLIST_FILE = strdup(PACKAGE_DATADIR "/DHTservers");
    } else {
        SRVLIST_FILE = malloc(strlen(user_config_dir) + strlen(CONFIGDIR) + strlen("DHTservers") + 1);
        if (SRVLIST_FILE != NULL) {
            strcpy(SRVLIST_FILE, user_config_dir);
            strcat(SRVLIST_FILE, CONFIGDIR);
            strcat(SRVLIST_FILE, "DHTservers");
        } else {
            endwin();
            fprintf(stderr, "malloc() failed. Aborting...\n");
            exit(EXIT_FAILURE);
        }
    }

    free(user_config_dir);

    init_term();
    Tox *m = init_tox();

    if (m == NULL) {
        endwin();
        fprintf(stderr, "Failed to initialize network. Aborting...\n");
        exit(EXIT_FAILURE);
    }

    prompt = init_windows(m);

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

    prompt_init_statusbar(prompt, m);

    while (true) {
        /* Update tox */
        do_tox(m, prompt);

        /* Draw */
        draw_active_window(m);
    }

    exit_toxic(m);
    return 0;
}

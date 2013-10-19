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
#include "friendlist.h"
#include "prompt.h"
#include "misc_tools.h"

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
    tox_callback_group_invite(m, on_groupinvite, NULL);
    tox_callback_group_message(m, on_groupmessage, NULL);
    tox_callback_file_sendrequest(m, on_file_sendrequest, NULL);
    tox_callback_file_control(m, on_file_control, NULL);
    tox_callback_file_data(m, on_file_data, NULL);

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

#define MINLINE    50 /* IP: 7 + port: 5 + key: 38 + spaces: 2 = 70. ! (& e.g. tox.im = 6) */
#define MAXLINE   256 /* Approx max number of chars in a sever line (name + port + key) */
#define MAXSERVERS 50
#define SERVERLEN (MAXLINE - TOX_CLIENT_ID_SIZE - 7)

static int  linecnt = 0;
static char servers[MAXSERVERS][SERVERLEN];
static uint16_t ports[MAXSERVERS];
static uint8_t keys[MAXSERVERS][TOX_CLIENT_ID_SIZE];

int serverlist_load()
{
    FILE *fp = NULL;

    fp = fopen(SRVLIST_FILE, "r");

    if (fp == NULL)
        return 1;

    char line[MAXLINE];
    while (fgets(line, sizeof(line), fp) && linecnt < MAXSERVERS) {
        if (strlen(line) > MINLINE) {
            char *name = strtok(line, " ");
            char *port = strtok(NULL, " ");
            char *key_ascii = strtok(NULL, " ");
            /* invalid line */
            if (name == NULL || port == NULL || key_ascii == NULL)
                continue;

            strncpy(servers[linecnt], name, SERVERLEN);
            servers[linecnt][SERVERLEN - 1] = 0;
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

int init_connection_helper(Tox *m, int linenumber)
{
    return tox_bootstrap_from_address(m, servers[linenumber], TOX_ENABLE_IPV6_DEFAULT,
                                                ports[linenumber], keys[linenumber]);
}

/* Connects to a random DHT server listed in the DHTservers file
 *
 * return codes:
 * 1: failed to open server file
 * 2: no line of sufficient length in server file
 * 3: (old, removed) failed to split a selected line in the server file
 * 4: failed to resolve name to IP
 * 5: serverlist file contains no acceptable line
 */
static uint8_t init_connection_serverlist_loaded = 0;
int init_connection(Tox *m)
{
    if (linecnt > 0) /* already loaded serverlist */
        return init_connection_helper(m, rand() % linecnt) ? 0 : 4;

    /* only once:
     * - load the serverlist
     * - connect to "everyone" inside
     */
    if (!init_connection_serverlist_loaded) {
        init_connection_serverlist_loaded = 1;
        int res = serverlist_load();
        if (res)
            return res;

        if (!linecnt)
            return 4;

        res = 6;
        int linenumber;
        for(linenumber = 0; linenumber < linecnt; linenumber++)
            if (init_connection_helper(m, linenumber))
                res = 0;

        return res;
    }

    /* empty serverlist file */
    return 5;
}

static void do_tox(Tox *m, ToxWindow *prompt)
{
    static int conn_try = 0;
    static int conn_err = 0;
    static bool dht_on = false;

    if (!dht_on && !tox_isconnected(m) && !(conn_try++ % 100)) {
        if (!conn_err) {
            wprintw(prompt->window, "Establishing connection...\n");
            if (conn_err = init_connection(m))
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

void do_file_senders(Tox *m)
{
    int i;

    for (i = 0; i < MAX_FILES; ++i) {
        if (!file_senders[i].active)
            continue;

        while (true) {
            uint8_t filenum = file_senders[i].filenum;
            int friendnum = file_senders[i].friendnum;

            if (!tox_file_senddata(m, friendnum, filenum, file_senders[i].nextpiece,
                                   file_senders[i].piecelen))
                return;

            file_senders[i].piecelen = fread(file_senders[i].nextpiece, 1, tox_filedata_size(m, 
                                             friendnum), file_senders[i].file);

            if (file_senders[i].piecelen == 0) {
                fclose(file_senders[i].file);
                memset(&file_senders[i], 0, sizeof(FileSender));

                tox_file_sendcontrol(m, friendnum, 0, filenum, TOX_FILECONTROL_FINISHED, 0, 0);

                uint8_t *pathname = file_senders[i].pathname;
                wprintw(file_senders[i].chatwin, "File '%s' successfuly sent.\n", pathname);

                int i;

                for (i = num_file_senders; i > 0; --i) {
                    if (file_senders[i-1].active)
                        break;
                }

                num_file_senders = i;
                return;
            }
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
        do_tox(m, prompt);
        do_file_senders(m);
        draw_active_window(m);
    }

    exit_toxic(m);
    return 0;
}

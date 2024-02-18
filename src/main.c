/*  main.c
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
#include "run_options.h"
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
#endif /* AUDIO */

#ifdef PYTHON
#include "api.h"
#include "python_api.h"
#endif

#ifndef PACKAGE_DATADIR
#define PACKAGE_DATADIR "."
#endif

#define DATANAME  "toxic_profile.tox"
#define BLOCKNAME "toxic_blocklist"

static struct cqueue_thread cqueue_thread;

#ifdef AUDIO
static struct av_thread av_thread;
#endif

static void queue_init_message(const char *msg, ...);

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

static void cb_toxcore_logger(Tox *tox, TOX_LOG_LEVEL level, const char *file, uint32_t line, const char *func,
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

    format_time_str(timestamp, sizeof(timestamp), "%F %T", &tmp);

    fprintf(fp, "%c %s.%06ld %s:%u(%s) - %s\n", tox_log_level_show(level)[0], timestamp, tv.tv_usec, file, line, func,
            message);

    fflush(fp);
}

static struct _init_messages {
    char **msgs;
    int num;
} init_messages;

/* One-time queue for messages created during init. Do not use after program init. */
__attribute__((format(printf, 1, 2)))
static void queue_init_message(const char *msg, ...)
{
    char frmt_msg[MAX_STR_SIZE] = {0};

    va_list args;
    va_start(args, msg);
    vsnprintf(frmt_msg, sizeof(frmt_msg), msg, args);
    va_end(args);

    const int i = init_messages.num;
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

static void print_init_messages(ToxWindow *home_window, const Client_Config *c_config)
{
    for (int i = 0; i < init_messages.num; ++i) {
        line_info_add(home_window, c_config, NULL, NULL, NULL, SYS_MSG, 0, 0, "%s", init_messages.msgs[i]);
    }
}

static void load_friendlist(Toxic *toxic)
{
    const size_t numfriends = tox_self_get_friend_list_size(toxic->tox);

    for (size_t i = 0; i < numfriends; ++i) {
        friendlist_onFriendAdded(NULL, toxic, i, false);
    }

    sort_friendlist_index();
}

static void load_groups(Toxic *toxic)
{
    if (toxic == NULL) {
        return;
    }

    Tox *tox = toxic->tox;

    size_t numgroups = tox_group_get_number_groups(tox);

    for (size_t i = 0; i < numgroups; ++i) {
        if (init_groupchat_win(toxic, i, NULL, 0, Group_Join_Type_Load) != 0) {
            tox_group_leave(tox, i, NULL, 0, NULL);
        }
    }
}

static void load_conferences(Toxic *toxic)
{
    if (toxic == NULL) {
        return;
    }

    Tox *tox = toxic->tox;

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

        const int64_t win_id = init_conference_win(toxic, conferencenum, type, (const char *) title, length);

        if (win_id == -1) {
            tox_conference_delete(tox, conferencenum, NULL);
            continue;
        }

        if (type == TOX_CONFERENCE_TYPE_AV) {
            ToxWindow *win = get_window_pointer_by_id(toxic->windows, win_id);
            line_info_add(win, toxic->c_config, NULL, NULL, NULL, SYS_MSG, 0, 0,
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
static int password_eval(const Client_Config *c_config, char *buf, int size)
{
    buf[0] = '\0';

    /* Run password_eval command */
    FILE *f = popen(c_config->password_eval, "r");

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
static void first_time_encrypt(Client_Data *client_data, const char *msg)
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
            len = password_prompt(client_data->pass, sizeof(client_data->pass));
            client_data->pass_len = len;

            if (strcasecmp(client_data->pass, "q") == 0) {
                exit(0);
            }

            if (string_is_empty(passconfirm) && (len < MIN_PASSWORD_LEN || len > MAX_PASSWORD_LEN)) {
                printf("Password must be between %d and %d characters long. ", MIN_PASSWORD_LEN, MAX_PASSWORD_LEN);
                continue;
            }

            if (string_is_empty(passconfirm)) {
                printf("Enter password again ");
                snprintf(passconfirm, sizeof(passconfirm), "%s", client_data->pass);
                continue;
            }

            if (strcmp(client_data->pass, passconfirm) != 0) {
                memset(passconfirm, 0, sizeof(passconfirm));
                memset(client_data->pass, 0, sizeof(client_data->pass));
                printf("Passwords don't match. Try again. ");
                continue;
            }

            valid_password = true;
        }

        queue_init_message("Data file '%s' is encrypted", client_data->data_path);
        memset(passconfirm, 0, sizeof(passconfirm));
        client_data->is_encrypted = true;
    }

    clear_screen();
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

static void init_tox_options(const Run_Options *run_opts, struct Tox_Options *tox_opts)
{
    tox_options_default(tox_opts);

    tox_options_set_ipv6_enabled(tox_opts, !run_opts->use_ipv4);
    tox_options_set_udp_enabled(tox_opts, !run_opts->force_tcp);
    tox_options_set_proxy_type(tox_opts, run_opts->proxy_type);
    tox_options_set_tcp_port(tox_opts, run_opts->tcp_port);
    tox_options_set_local_discovery_enabled(tox_opts, !run_opts->disable_local_discovery);
    tox_options_set_experimental_groups_persistence(tox_opts, true);

    if (run_opts->logging) {
        tox_options_set_log_callback(tox_opts, cb_toxcore_logger);

        if (run_opts->log_fp != NULL) {
            tox_options_set_log_user_data(tox_opts, run_opts->log_fp);
        }
    }

    if (!tox_options_get_ipv6_enabled(tox_opts)) {
        queue_init_message("Forcing IPv4 connection");
    }

    if (tox_options_get_tcp_port(tox_opts)) {
        queue_init_message("TCP relaying enabled on port %d", tox_options_get_tcp_port(tox_opts));
    }

    if (tox_options_get_proxy_type(tox_opts) != TOX_PROXY_TYPE_NONE) {
        tox_options_set_proxy_port(tox_opts, run_opts->proxy_port);
        tox_options_set_proxy_host(tox_opts, run_opts->proxy_address);
        const char *ps = tox_options_get_proxy_type(tox_opts) == TOX_PROXY_TYPE_SOCKS5 ? "SOCKS5" : "HTTP";

        char tmp[sizeof(run_opts->proxy_address) + MAX_STR_SIZE];
        snprintf(tmp, sizeof(tmp), "Using %s proxy %s : %d", ps, run_opts->proxy_address, run_opts->proxy_port);
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

/*
 * Loads a Tox instance.
 *
 * Return true on success.
 */
static bool load_tox(Toxic *toxic, struct Tox_Options *tox_opts, Tox_Err_New *new_err)
{
    const Run_Options *run_opts = toxic->run_opts;

    FILE *fp = fopen(toxic->client_data.data_path, "rb");

    if (fp != NULL) {   /* Data file exists */
        off_t len = file_size(toxic->client_data.data_path);

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

        const bool is_encrypted = tox_is_data_encrypted((uint8_t *) data);

        /* attempt to encrypt an already encrypted data file */
        if (run_opts->encrypt_data && is_encrypted) {
            fclose(fp);
            free(data);
            exit_toxic_err("failed in load_tox", FATALERR_ENCRYPT);
        }

        Client_Data *client_data = &toxic->client_data;

        if (run_opts->unencrypt_data && is_encrypted) {
            queue_init_message("Data file '%s' has been unencrypted", client_data->data_path);
        } else if (run_opts->unencrypt_data) {
            queue_init_message("Warning: passed --unencrypt-data option with unencrypted data file '%s'",
                               client_data->data_path);
        }

        if (is_encrypted) {
            if (!run_opts->unencrypt_data) {
                client_data->is_encrypted = true;
            }

            size_t pwlen = 0;
            int pweval = toxic->c_config->password_eval[0];

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
                    pwlen = password_eval(toxic->c_config, client_data->pass, sizeof(client_data->pass));
                } else {
                    pwlen = password_prompt(client_data->pass, sizeof(client_data->pass));
                }

                client_data->pass_len = pwlen;

                if (strcasecmp(client_data->pass, "q") == 0) {
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
                tox_pass_decrypt((uint8_t *) data, len, (uint8_t *) client_data->pass, pwlen,
                                 (uint8_t *) plain, &pwerr);

                if (pwerr == TOX_ERR_DECRYPTION_OK) {
                    tox_options_set_savedata_type(tox_opts, TOX_SAVEDATA_TYPE_TOX_SAVE);
                    tox_options_set_savedata_data(tox_opts, (uint8_t *) plain, plain_len);

                    toxic->tox = tox_new(tox_opts, new_err);

                    if (toxic->tox == NULL) {
                        fclose(fp);
                        free(data);
                        free(plain);
                        return false;
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

            toxic->tox = tox_new(tox_opts, new_err);

            if (toxic->tox == NULL) {
                fclose(fp);
                free(data);
                return false;
            }
        }

        fclose(fp);
        free(data);
    } else {   /* Data file does not/should not exist */
        if (file_exists(toxic->client_data.data_path)) {
            exit_toxic_err("failed in load_tox", FATALERR_FILEOP);
        }

        tox_options_set_savedata_type(tox_opts, TOX_SAVEDATA_TYPE_NONE);

        toxic->tox = tox_new(tox_opts, new_err);

        if (toxic->tox == NULL) {
            return false;
        }

        if (store_data(toxic) == -1) {
            exit_toxic_err("failed in load_tox", FATALERR_FILEOP);
        }
    }

    return true;
}

static bool load_toxic(Toxic *toxic)
{
    Tox_Err_Options_New options_new_err;
    struct Tox_Options *tox_opts = tox_options_new(&options_new_err);

    if (tox_opts == NULL) {
        exit_toxic_err("tox_options_new returned fatal error", options_new_err);
    }

    init_tox_options(toxic->run_opts, tox_opts);

    Tox_Err_New new_err;

    if (!load_tox(toxic, tox_opts, &new_err)) {
        return false;
    }

    if (new_err == TOX_ERR_NEW_PORT_ALLOC && tox_options_get_ipv6_enabled(tox_opts)) {
        queue_init_message("Falling back to ipv4");
        tox_options_set_ipv6_enabled(tox_opts, false);

        if (!load_tox(toxic, tox_opts, &new_err)) {
            return false;
        }
    }

    if (toxic->tox == NULL) {
        return false;
    }

    if (new_err != TOX_ERR_NEW_OK) {
        queue_init_message("tox_new returned non-fatal error %d", new_err);
    }

    init_tox_callbacks(toxic->tox);
    load_friendlist(toxic);

    if (load_blocklist(toxic->client_data.block_path) == -1) {
        queue_init_message("Failed to load block list");
    }

    if (tox_self_get_name_size(toxic->tox) == 0) {
        tox_self_set_name(toxic->tox, (uint8_t *) "Toxic User", strlen("Toxic User"), NULL);
    }

    tox_options_free(tox_opts);

    return true;
}

static void do_toxic(Toxic *toxic)
{
    pthread_mutex_lock(&Winthread.lock);

    if (toxic->run_opts->no_connect) {
        pthread_mutex_unlock(&Winthread.lock);
        return;
    }

    tox_iterate(toxic->tox, (void *) toxic);
    do_tox_connection(toxic);

    pthread_mutex_unlock(&Winthread.lock);
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

static void *thread_winref(void *data)
{
    Toxic *toxic = (Toxic *) data;

    uint8_t draw_count = 0;

    init_signal_catchers();

    while (true) {
        draw_count++;
        draw_active_window(toxic);

        if (Winthread.flag_resize) {
            on_window_resize(toxic->windows);
            Winthread.flag_resize = 0;
        } else if (draw_count >= INACTIVE_WIN_REFRESH_RATE) {
            refresh_inactive_windows(toxic->windows, toxic->c_config);
            draw_count = 0;
        }

        if (Winthread.sig_exit_toxic) {
            pthread_mutex_lock(&Winthread.lock);
            exit_toxic_success(toxic);
        }

        poll_interface_refresh_flag();
    }
}

_Noreturn static void *thread_cqueue(void *data)
{
    Toxic *toxic = (Toxic *) data;
    Windows *windows = toxic->windows;

    while (true) {
        pthread_mutex_lock(&Winthread.lock);

        for (uint16_t i = 2; i < windows->count; ++i) {
            ToxWindow *w = windows->list[i];

            if (w->type == WINDOW_TYPE_CHAT) {
                cqueue_check_unread(w);

                if (get_friend_connection_status(w->num) != TOX_CONNECTION_NONE) {
                    cqueue_try_send(w, toxic->tox);
                }
            }
        }

        pthread_mutex_unlock(&Winthread.lock);

        sleep_thread(750000L); // 0.75 seconds
    }
}

#ifdef AUDIO
_Noreturn static void *thread_av(void *data)
{
    ToxAV *av = (ToxAV *) data;

    while (true) {
        pthread_mutex_lock(&Winthread.lock);
        toxav_iterate(av);
        pthread_mutex_unlock(&Winthread.lock);

        const long int sleep_duration = toxav_iteration_interval(av) * 1000;
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

static void set_default_run_options(Run_Options *run_opts)
{
    run_opts->proxy_type = (uint8_t) TOX_PROXY_TYPE_NONE;
}

static void parse_args(Toxic *toxic, int argc, char *argv[])
{
    Run_Options *run_opts = toxic->run_opts;

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
                run_opts->use_ipv4 = true;
                break;
            }

            case 'b': {
                run_opts->debug = true;
                queue_init_message("stderr enabled");
                break;
            }

            case 'c': {
                if (optarg == NULL) {
                    queue_init_message("Invalid argument for option: %d", opt);
                    break;
                }

                snprintf(run_opts->config_path, sizeof(run_opts->config_path), "%s", optarg);
                run_opts->use_custom_config_file = true;

                queue_init_message("Using '%s' custom config file", run_opts->config_path);
                break;
            }

            case 'd': {
                run_opts->default_locale = true;
                queue_init_message("Using default POSIX locale");
                break;
            }

            case 'e': {
                run_opts->encrypt_data = true;
                break;
            }

            case 'f': {
                if (optarg == NULL) {
                    queue_init_message("Invalid argument for option: %d", opt);
                    break;
                }

                run_opts->use_custom_data = true;

                Client_Data *client_data = &toxic->client_data;

                if (client_data->data_path) {
                    free(client_data->data_path);
                    client_data->data_path = NULL;
                }

                if (client_data->block_path) {
                    free(client_data->block_path);
                    client_data->block_path = NULL;
                }

                client_data->data_path = malloc(strlen(optarg) + 1);

                if (client_data->data_path == NULL) {
                    exit_toxic_err("failed in parse_args", FATALERR_MEMORY);
                }

                strcpy(client_data->data_path, optarg);

                client_data->block_path = malloc(strlen(optarg) + strlen("-blocklist") + 1);

                if (client_data->block_path == NULL) {
                    exit_toxic_err("failed in parse_args", FATALERR_MEMORY);
                }

                strcpy(client_data->block_path, optarg);
                strcat(client_data->block_path, "-blocklist");

                queue_init_message("Using '%s' tox profile", client_data->data_path);

                break;
            }

            case 'l': {
                if (optarg) {
                    run_opts->logging = true;

                    if (strcmp(optarg, "stderr") != 0) {
                        run_opts->log_fp = fopen(optarg, "w");

                        if (run_opts->log_fp != NULL) {
                            queue_init_message("Toxcore logging enabled to %s", optarg);
                        } else {
                            run_opts->debug = true;
                            queue_init_message("Failed to open log file %s. Falling back to stderr.", optarg);
                        }
                    } else {
                        run_opts->debug = true;
                        queue_init_message("Toxcore logging enabled to stderr");
                    }
                }

                break;
            }

            case 'L': {
                run_opts->disable_local_discovery = true;
                queue_init_message("Local discovery disabled");
                break;
            }

            case 'n': {
                if (optarg == NULL) {
                    queue_init_message("Invalid argument for option: %d", opt);
                    break;
                }

                snprintf(run_opts->nodes_path, sizeof(run_opts->nodes_path), "%s", optarg);
                break;
            }

            case 'o': {
                run_opts->no_connect = true;
                queue_init_message("DHT disabled");
                break;
            }

            case 'p': {
                run_opts->proxy_type = TOX_PROXY_TYPE_SOCKS5;
            }

            // Intentional fallthrough

            case 'P': {
                if (optarg == NULL) {
                    queue_init_message("Invalid argument for option: %d", opt);
                    run_opts->proxy_type = TOX_PROXY_TYPE_NONE;
                    break;
                }

                if (run_opts->proxy_type == TOX_PROXY_TYPE_NONE) {
                    run_opts->proxy_type = TOX_PROXY_TYPE_HTTP;
                }

                snprintf(run_opts->proxy_address, sizeof(run_opts->proxy_address), "%s", optarg);

                if (++optind > argc || argv[optind - 1][0] == '-') {
                    exit_toxic_err("Proxy error", FATALERR_PROXY);
                }

                long int port = strtol(argv[optind - 1], NULL, 10);

                if (port <= 0 || port > MAX_PORT_RANGE) {
                    exit_toxic_err("Proxy error", FATALERR_PROXY);
                }

                run_opts->proxy_port = port;
                break;
            }

            case 'r': {
                if (optarg == NULL) {
                    queue_init_message("Invalid argument for option: %d", opt);
                    break;
                }

                snprintf(run_opts->nameserver_path, sizeof(run_opts->nameserver_path), "%s", optarg);

                if (!file_exists(run_opts->nameserver_path)) {
                    queue_init_message("nameserver list not found");
                }

                break;
            }

            case 't': {
                run_opts->force_tcp = true;
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

                run_opts->tcp_port = port;
                break;
            }

            case 'u': {
                run_opts->unencrypt_data = true;
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
static void init_default_data_files(Client_Data *client_data)
{
    char *user_config_dir = get_user_config_dir();

    if (user_config_dir == NULL) {
        exit_toxic_err("failed in init_default_data_files()", FATALERR_FILEOP);
    }

    int config_err = create_user_config_dirs(user_config_dir);

    if (config_err == -1) {
        client_data->data_path = strdup(DATANAME);
        client_data->block_path = strdup(BLOCKNAME);

        if (client_data->data_path == NULL || client_data->block_path == NULL) {
            exit_toxic_err("failed in init_default_data_files()", FATALERR_MEMORY);
        }
    } else {
        client_data->data_path = malloc(strlen(user_config_dir) + strlen(CONFIGDIR) + strlen(DATANAME) + 1);
        client_data->block_path = malloc(strlen(user_config_dir) + strlen(CONFIGDIR) + strlen(BLOCKNAME) + 1);

        if (client_data->data_path == NULL || client_data->block_path == NULL) {
            exit_toxic_err("failed in init_default_data_files()", FATALERR_MEMORY);
        }

        strcpy(client_data->data_path, user_config_dir);
        strcat(client_data->data_path, CONFIGDIR);
        strcat(client_data->data_path, DATANAME);

        strcpy(client_data->block_path, user_config_dir);
        strcat(client_data->block_path, CONFIGDIR);
        strcat(client_data->block_path, BLOCKNAME);
    }

    free(user_config_dir);
}

static Toxic *toxic_init(void)
{
    Toxic *toxic = (Toxic *) calloc(1, sizeof(Toxic));

    if (toxic == NULL) {
        return NULL;
    }

    Client_Config *tmp_settings = (Client_Config *) calloc(1, sizeof(Client_Config));

    if (tmp_settings == NULL) {
        free(toxic);
        return NULL;
    }

    toxic->c_config = tmp_settings;

    Run_Options *tmp_opts = (Run_Options *) calloc(1, sizeof(Run_Options));

    if (tmp_opts == NULL) {
        free(toxic->c_config);
        free(toxic);
        return NULL;
    }

    toxic->run_opts = tmp_opts;

    set_default_run_options(toxic->run_opts);

    Windows *tmp_windows = (Windows *)calloc(1, sizeof(Windows));

    if (tmp_windows == NULL) {
        free(toxic->c_config);
        free(toxic->run_opts);
        free(toxic);
        return NULL;
    }

    toxic->windows = tmp_windows;

    return toxic;
}

int main(int argc, char **argv)
{
    /* Make sure all written files are read/writeable only by the current user. */
    umask(S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

    srand(time(NULL)); // We use rand() for trivial/non-security related things

    Toxic *toxic = toxic_init();

    if (toxic == NULL) {
        exit_toxic_err("failed in main", FATALERR_TOXIC_INIT);
    }

    parse_args(toxic, argc, argv);

    const Client_Config *c_config = toxic->c_config;
    Run_Options *run_opts = toxic->run_opts;
    Windows *windows = toxic->windows;

    /* Use the -b flag to enable stderr */
    if (!run_opts->debug) {
        if (!freopen("/dev/null", "w", stderr)) {
            fprintf(stderr, "Warning: failed to enable stderr\n");
        }
    }

    if (run_opts->encrypt_data && run_opts->unencrypt_data) {
        run_opts->encrypt_data = 0;
        run_opts->unencrypt_data = 0;
        queue_init_message("Warning: Using --unencrypt-data and --encrypt-data simultaneously has no effect");
    }

    if (!run_opts->use_custom_data) {
        init_default_data_files(&toxic->client_data);
    }

    const bool datafile_exists = file_exists(toxic->client_data.data_path);

    if (!datafile_exists && !run_opts->unencrypt_data) {
        first_time_encrypt(&toxic->client_data, "Creating new data file. Would you like to encrypt it? Y/n (q to quit)");
    } else if (run_opts->encrypt_data) {
        first_time_encrypt(&toxic->client_data, "Encrypt existing data file? Y/n (q to quit)");
    }

    if (!settings_load_config_file(run_opts, toxic->client_data.data_path)) {
        queue_init_message("Failed to load config file");
    }

    const int ms_ret = settings_load_main(toxic->c_config, run_opts);

    if (ms_ret < 0) {
        queue_init_message("Failed to load user settings: error %d", ms_ret);
    }

    if (!run_opts->use_custom_config_file && run_opts->use_custom_data) {
        queue_init_message("Using '%s' config file", run_opts->config_path);
    }

    const int curl_init = curl_global_init(CURL_GLOBAL_ALL);
    const int nameserver_ret = name_lookup_init(run_opts->nameserver_path, curl_init);

    if (nameserver_ret == -1) {
        queue_init_message("curl failed to initialize; name lookup service is disabled.");
    } else if (nameserver_ret == -2) {
        queue_init_message("Name lookup server list could not be found.");
    } else if (nameserver_ret == -3) {
        fprintf(stderr, "Name lookup server list does not contain any valid entries\n");
    }

#ifdef X11

    if (init_x11focus(&toxic->x11_focus) == -1) {
        queue_init_message("X failed to initialize");
    }

#endif /* X11 */

    if (!load_toxic(toxic)) {
        exit_toxic_err("Failed in main", FATALERR_TOX_INIT);
    }

    if (run_opts->encrypt_data && !datafile_exists) {
        run_opts->encrypt_data = 0;
    }

    init_term(c_config, run_opts->default_locale);

    init_windows(toxic);
    ToxWindow *home_window = toxic->home_window;

    prompt_init_statusbar(toxic, !datafile_exists);

    load_groups(toxic);
    load_conferences(toxic);

    const int fs_ret = settings_load_friends(run_opts);

    if (fs_ret != 0) {
        queue_init_message("Failed to load friend config settings: error %d", fs_ret);
    }

    const int gs_ret = settings_load_groups(windows, run_opts);

    if (gs_ret != 0) {
        queue_init_message("Failed to load groupchat config settings: error %d", gs_ret);
    }

    const int cs_ret = settings_load_conferences(windows, run_opts);

    if (cs_ret != 0) {
        queue_init_message("Failed to load conference config settings: error %d", cs_ret);
    }

    set_active_window_by_type(windows, WINDOW_TYPE_PROMPT);

    if (pthread_mutex_init(&Winthread.lock, NULL) != 0) {
        exit_toxic_err("failed in main", FATALERR_MUTEX_INIT);
    }

#ifdef AUDIO

    toxic->av = init_audio(toxic);

    if (toxic->av == NULL) {
        queue_init_message("Failed to init audio");
    }

#ifdef VIDEO
    init_video(toxic);

    if (toxic->av == NULL) {
        queue_init_message("Failed to init video");
    }

#endif /* VIDEO */

    /* AV thread */
    if (pthread_create(&av_thread.tid, NULL, thread_av, (void *) toxic->av) != 0) {
        exit_toxic_err("failed in main", FATALERR_THREAD_CREATE);
    }

    set_al_device(input, c_config->audio_in_dev);
    set_al_device(output, c_config->audio_out_dev);

#elif SOUND_NOTIFY

    if (init_devices() == de_InternalError) {
        queue_init_message("Failed to init audio devices");
    }

#endif /* AUDIO */

    /* thread for ncurses UI */
    if (pthread_create(&Winthread.tid, NULL, thread_winref, (void *) toxic) != 0) {
        exit_toxic_err("failed in main", FATALERR_THREAD_CREATE);
    }

    /* thread for message queue */
    if (pthread_create(&cqueue_thread.tid, NULL, thread_cqueue, (void *) toxic) != 0) {
        exit_toxic_err("failed in main", FATALERR_THREAD_CREATE);
    }

#ifdef PYTHON

    init_python(toxic->tox);
    invoke_autoruns(toxic->home_window, c_config->autorun_path);

#endif /* PYTHON */

    init_notify(60, c_config->notification_timeout);

    /* screen/tmux auto-away timer */
    if (init_mplex_away_timer(toxic) == -1) {
        queue_init_message("Failed to init mplex auto-away.");
    }

    const int nodeslist_ret = load_DHT_nodeslist(toxic);

    if (nodeslist_ret != 0) {
        queue_init_message("DHT nodeslist failed to load (error %d)", nodeslist_ret);
    }

    pthread_mutex_lock(&Winthread.lock);
    print_init_messages(toxic->home_window, c_config);
    flag_interface_refresh();
    pthread_mutex_unlock(&Winthread.lock);

    /* set user avatar from config file. if no path is supplied tox_unset_avatar is called */
    char avatarstr[PATH_MAX + 11];
    snprintf(avatarstr, sizeof(avatarstr), "/avatar %s", c_config->avatar_path);
    execute(home_window->chatwin->history, home_window, toxic, avatarstr, GLOBAL_COMMAND_MODE);

    time_t last_save = get_unix_time();

    while (true) {
        do_toxic(toxic);

        const time_t cur_time = get_unix_time();

        if (c_config->autosave_freq > 0 && timed_out(last_save, c_config->autosave_freq)) {
            pthread_mutex_lock(&Winthread.lock);

            if (store_data(toxic) != 0) {
                line_info_add(home_window, c_config, false, NULL, NULL, SYS_MSG, 0, RED,
                              "WARNING: Failed to save to data file");
            }

            pthread_mutex_unlock(&Winthread.lock);

            last_save = cur_time;
        }

        const long int sleep_duration = tox_iteration_interval(toxic->tox) * 1000;
        sleep_thread(sleep_duration);
    }
}

/*  term_mplex.c
 *
 *  Copyright (C) 2015-2024 Toxic All Rights Reserved.
 *
 *  This file is part of Toxic. Toxic is free software licensed
 *  under the GNU General Public License 3.0.
 */

#include <limits.h> /* PATH_MAX */
#include <stdio.h>  /* fgets, popen, pclose */
#include <stdlib.h> /* malloc, realloc, free, getenv */
#include <string.h> /* strlen, strcpy, strstr, strchr, strrchr, strcat, strncmp */

#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <tox/tox.h>

#include "execute.h"
#include "settings.h"
#include "term_mplex.h"
#include "toxic.h"
#include "windows.h"

extern struct Winthread Winthread;

#if defined(PATH_MAX) && PATH_MAX > 512
#define BUFFER_SIZE PATH_MAX
#else
#define BUFFER_SIZE 512
#endif

#define PATH_SEP_S "/"
#define PATH_SEP_C '/'

typedef enum {
    MPLEX_NONE,
    MPLEX_SCREEN,
    MPLEX_TMUX,
} mplex_status;

/* used for:
   - storing screen socket name
   - storing tmux session number in string form */
static char mplex_data [BUFFER_SIZE];

static char buffer [BUFFER_SIZE];

/* Differentiates between mplex auto-away and manual-away */
static bool auto_away_active = false;

static mplex_status mplex = MPLEX_NONE;
static Tox_User_Status prev_status = TOX_USER_STATUS_NONE;
static char prev_note [TOX_MAX_STATUS_MESSAGE_LENGTH] = "";

/* mutex for access to status data, for sync between:
   - user command /status from ncurses thread
   - auto-away POSIX timer, which runs from a separate thread
   after init, should be accessed only by cmd_status()
 */
static pthread_mutex_t status_lock;
static pthread_t mplex_tid;

void lock_status(void)
{
    pthread_mutex_lock(&status_lock);
}

void unlock_status(void)
{
    pthread_mutex_unlock(&status_lock);
}

static char *read_into_dyn_buffer(FILE *stream)
{
    const char *input_ptr = NULL;
    char *dyn_buffer = NULL;
    int dyn_buffer_size = 1; /* account for the \0 */

    while ((input_ptr = fgets(buffer, BUFFER_SIZE, stream)) != NULL) {
        int length = dyn_buffer_size + strlen(input_ptr);

        if (dyn_buffer) {
            char *tmp = realloc(dyn_buffer, length);

            if (tmp == NULL) {
                return NULL;
            }

            dyn_buffer = tmp;
        } else {
            dyn_buffer = malloc(length);

            if (dyn_buffer == NULL) {
                return NULL;
            }
        }

        strcpy(dyn_buffer + dyn_buffer_size - 1, input_ptr);
        dyn_buffer_size = length;
    }

    return dyn_buffer;
}

static char *extract_socket_path(const char *info)
{
    const char *search_str = " Socket";
    const char *pos = strstr(info, search_str);
    char *end = NULL;
    char *path = NULL;

    if (!pos) {
        return NULL;
    }

    pos += strlen(search_str);
    pos = strchr(pos, PATH_SEP_C);

    if (!pos) {
        return NULL;
    }

    end = strchr(pos, '\n');

    if (!end) {
        return NULL;
    }

    *end = '\0';
    end = strrchr(pos, '.');

    if (!end) {
        return NULL;
    }

    path = malloc(end - pos + 1);

    if (path == NULL) {
        return NULL;
    }

    *end = '\0';
    return strcpy(path, pos);
}

static int detect_gnu_screen(void)
{
    FILE *session_info_stream = NULL;
    char *socket_name = NULL, *socket_path = NULL;
    char *dyn_buffer = NULL;

    socket_name = getenv("STY");

    if (!socket_name) {
        goto nomplex;
    }

    session_info_stream = popen("env LC_ALL=C screen -ls", "r");

    if (!session_info_stream) {
        goto nomplex;
    }

    dyn_buffer = read_into_dyn_buffer(session_info_stream);

    if (!dyn_buffer) {
        goto nomplex;
    }

    pclose(session_info_stream);
    session_info_stream = NULL;

    socket_path = extract_socket_path(dyn_buffer);

    if (!socket_path) {
        goto nomplex;
    }

    free(dyn_buffer);
    dyn_buffer = NULL;

    if (strlen(socket_path) + strlen(PATH_SEP_S) + strlen(socket_name) >= sizeof(mplex_data)) {
        goto nomplex;
    }

    strcpy(mplex_data, socket_path);
    strcat(mplex_data, PATH_SEP_S);
    strcat(mplex_data, socket_name);
    free(socket_path);
    socket_path = NULL;

    mplex = MPLEX_SCREEN;
    return 1;

nomplex:

    if (session_info_stream) {
        pclose(session_info_stream);
    }

    if (dyn_buffer) {
        free(dyn_buffer);
    }

    if (socket_path) {
        free(socket_path);
    }

    return 0;
}

static int detect_tmux(void)
{
    char *tmux_env = getenv("TMUX"), *pos;

    if (!tmux_env) {
        return 0;
    }

    /* find second separator */
    pos = strrchr(tmux_env, ',');

    if (!pos) {
        return 0;
    }

    /* store the session id for later use */
    snprintf(mplex_data, sizeof(mplex_data), "$%s", pos + 1);
    mplex = MPLEX_TMUX;
    return 1;
}

/* Checks whether a terminal multiplexer (mplex) is present, and finds
   its unix socket.

   GNU screen and tmux are supported.

   Returns 1 if present, 0 otherwise. This value can be used to determine
   whether an auto-away detection timer is needed.
 */
static int detect_mplex(void)
{
    /* try screen, and if fails try tmux */
    return detect_gnu_screen() || detect_tmux();
}

/* Detects gnu screen session attached/detached by examining permissions of
   the session's unix socket.
 */
static int gnu_screen_is_detached(void)
{
    if (mplex != MPLEX_SCREEN) {
        return 0;
    }

    struct stat sb;

    if (stat(mplex_data, &sb) != 0) {
        return 0;
    }

    /* execution permission (x) means attached */
    return !(sb.st_mode & S_IXUSR);
}

/* Detects tmux attached/detached by getting session data and finding the
   current session's entry.
 */
static int tmux_is_detached(void)
{
    if (mplex != MPLEX_TMUX) {
        return 0;
    }

    FILE *session_info_stream = NULL;
    char *dyn_buffer = NULL, *search_str = NULL;
    char *entry_pos;
    int detached;
    const int numstr_len = strlen(mplex_data);

    /* get the number of attached clients for each session */
    session_info_stream = popen("tmux list-sessions -F \"#{session_id} #{session_attached}\"", "r");

    if (!session_info_stream) {
        goto fail;
    }

    dyn_buffer = read_into_dyn_buffer(session_info_stream);

    if (!dyn_buffer) {
        goto fail;
    }

    pclose(session_info_stream);
    session_info_stream = NULL;

    /* prepare search string, for finding the current session's entry */
    search_str = malloc(numstr_len + 2);

    if (search_str == NULL) {
        goto fail;
    }

    search_str[0] = '\n';
    strcpy(search_str + 1, mplex_data);

    /* do the search */
    if (strncmp(dyn_buffer, search_str + 1, numstr_len) == 0) {
        entry_pos = dyn_buffer;
    } else {
        entry_pos = strstr(dyn_buffer, search_str);
    }

    if (! entry_pos) {
        goto fail;
    }

    entry_pos = strchr(entry_pos, ' ') + 1;
    detached = strncmp(entry_pos, "0\n", 2) == 0;

    free(search_str);
    search_str = NULL;

    free(dyn_buffer);
    dyn_buffer = NULL;

    return detached;

fail:

    if (session_info_stream) {
        pclose(session_info_stream);
    }

    if (dyn_buffer) {
        free(dyn_buffer);
    }

    if (search_str) {
        free(search_str);
    }

    return 0;
}

/* Checks whether there is a terminal multiplexer present, but in detached
   state. Returns 1 if detached, 0 if attached or if there is no terminal
   multiplexer.

   If detect_mplex_socket() failed to find a mplex, there is no need to call
   this function. If it did find one, this function can be used to periodically
   sample its state and update away status according to attached/detached state
   of the mplex.
 */
static bool mplex_is_detached(void)
{
    return gnu_screen_is_detached() || tmux_is_detached();
}

static void mplex_timer_handler(Toxic *toxic)
{
    if (toxic == NULL) {
        return;
    }

    pthread_mutex_lock(&Winthread.lock);
    const bool auto_away_enabled = toxic->c_config->mplex_away;
    pthread_mutex_unlock(&Winthread.lock);

    // needed in case config is changed during a running session
    if (!auto_away_enabled) {
        return;
    }

    ToxWindow *home_window = toxic->home_window;

    Tox_User_Status current_status, new_status;
    const char *new_note;

    if (mplex == MPLEX_NONE) {
        return;
    }

    const bool detached = mplex_is_detached();

    pthread_mutex_lock(&Winthread.lock);
    current_status = tox_self_get_status(toxic->tox);
    pthread_mutex_unlock(&Winthread.lock);

    if (auto_away_active && current_status == TOX_USER_STATUS_AWAY && !detached) {
        auto_away_active = false;
        new_status = prev_status;
        new_note = prev_note;
    } else if (current_status == TOX_USER_STATUS_NONE && detached) {
        auto_away_active = true;
        prev_status = current_status;
        new_status = TOX_USER_STATUS_AWAY;
        pthread_mutex_lock(&Winthread.lock);
        const size_t slen = tox_self_get_status_message_size(toxic->tox);
        tox_self_get_status_message(toxic->tox, (uint8_t *) prev_note);
        prev_note[slen] = '\0';
        new_note = toxic->c_config->mplex_away_note;
        pthread_mutex_unlock(&Winthread.lock);
    } else {
        return;
    }

    char status_str[MAX_STR_SIZE];
    char note_str[MAX_STR_SIZE];
    const char *status = new_status == TOX_USER_STATUS_AWAY ? "away" :
                         new_status == TOX_USER_STATUS_BUSY ? "busy" : "online";
    snprintf(status_str, sizeof(status_str), "/status %s", status);
    snprintf(note_str, sizeof(status_str), "/note %s", new_note);

    pthread_mutex_lock(&Winthread.lock);
    execute(home_window->chatwin->history, home_window, toxic, status_str, GLOBAL_COMMAND_MODE);
    execute(home_window->chatwin->history, home_window, toxic, note_str, GLOBAL_COMMAND_MODE);
    pthread_mutex_unlock(&Winthread.lock);
}

/* Time in seconds between calls to mplex_timer_handler */
#define MPLEX_TIMER_INTERVAL 2

_Noreturn static void *mplex_timer_thread(void *data)
{
    Toxic *toxic = (Toxic *) data;

    while (true) {
        sleep(MPLEX_TIMER_INTERVAL);
        mplex_timer_handler(toxic);
    }
}

int init_mplex_away_timer(Toxic *toxic)
{
    if (!detect_mplex()) {
        return 0;
    }

    if (toxic == NULL) {
        return -1;
    }

    if (toxic->client_data.mplex_auto_away_initialized) {
        return 0;
    }

    if (!toxic->c_config->mplex_away) {
        return 0;
    }

    /* status access mutex */
    if (pthread_mutex_init(&status_lock, NULL) != 0) {
        return -1;
    }

    if (pthread_create(&mplex_tid, NULL, mplex_timer_thread, (void *)toxic) != 0) {
        return -1;
    }

    toxic->client_data.mplex_auto_away_initialized = true;

    return 0;
}

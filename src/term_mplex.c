/*  term_mplex.c
 *
 *
 *  Copyright (C) 2015 Toxic All Rights Reserved.
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

#include <limits.h> /* PATH_MAX */
#include <stdio.h>  /* fgets, popen, pclose */
#include <stdlib.h> /* malloc, realloc, free, getenv */
#include <string.h> /* strlen, strcpy, strstr, strchr, strrchr, strcat, strncmp */

#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <tox/tox.h>

#include "global_commands.h"
#include "windows.h"
#include "term_mplex.h"
#include "toxic.h"
#include "settings.h"

extern struct ToxWindow *prompt;
extern struct user_settings *user_settings;
extern struct Winthread Winthread;

#if defined(PATH_MAX) && PATH_MAX > 512
#define BUFFER_SIZE PATH_MAX
#else
#define BUFFER_SIZE 512
#endif

#define PATH_SEP_S "/"
#define PATH_SEP_C '/'

typedef enum
{
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
static TOX_USER_STATUS prev_status = TOX_USER_STATUS_NONE;
static char prev_note [TOX_MAX_STATUS_MESSAGE_LENGTH] = "";

/* mutex for access to status data, for sync between:
   - user command /status from ncurses thread
   - auto-away POSIX timer, which runs from a separate thread
   after init, should be accessed only by cmd_status()
 */
static pthread_mutex_t status_lock;
static pthread_t mplex_tid;

void lock_status ()
{
    pthread_mutex_lock (&status_lock);
}

void unlock_status ()
{
    pthread_mutex_unlock (&status_lock);
}

static char *read_into_dyn_buffer (FILE *stream)
{
    const char *input_ptr = NULL;
    char *dyn_buffer = NULL;
    int dyn_buffer_size = 1; /* account for the \0 */

    while ((input_ptr = fgets (buffer, BUFFER_SIZE, stream)) != NULL)
    {
        int length = dyn_buffer_size + strlen (input_ptr);
        if (dyn_buffer)
            dyn_buffer = (char*) realloc (dyn_buffer, length);
        else
            dyn_buffer = (char*) malloc (length);
        strcpy (dyn_buffer + dyn_buffer_size - 1, input_ptr);
        dyn_buffer_size = length;
    }

    return dyn_buffer;
}

static char *extract_socket_path (const char *info)
{
    const char *search_str = " Socket";
    const char *pos = strstr (info, search_str);
    char *end = NULL;
    char* path = NULL;

    if (!pos)
        return NULL;

    pos += strlen (search_str);
    pos = strchr (pos, PATH_SEP_C);
    if (!pos)
        return NULL;

    end = strchr (pos, '\n');
    if (!end)
        return NULL;

    *end = '\0';
    end = strrchr (pos, '.');
    if (!end)
        return NULL;

    path = (char*) malloc (end - pos + 1);
    *end = '\0';
    return strcpy (path, pos);
}

static int detect_gnu_screen ()
{
    FILE *session_info_stream = NULL;
    char *socket_name = NULL, *socket_path = NULL;
    char *dyn_buffer = NULL;

    socket_name = getenv ("STY");
    if (!socket_name)
        goto nomplex;

    session_info_stream = popen ("env LC_ALL=C screen -ls", "r");
    if (!session_info_stream)
        goto nomplex;

    dyn_buffer = read_into_dyn_buffer (session_info_stream);
    if (!dyn_buffer)
        goto nomplex;

    pclose (session_info_stream);
    session_info_stream = NULL;

    socket_path = extract_socket_path (dyn_buffer);
    if (!socket_path)
        goto nomplex;

    free (dyn_buffer);
    dyn_buffer = NULL;
    strcpy (mplex_data, socket_path);
    strcat (mplex_data, PATH_SEP_S);
    strcat (mplex_data, socket_name);
    free (socket_path);
    socket_path = NULL;

    mplex = MPLEX_SCREEN;
    return 1;

nomplex:
    if (session_info_stream)
        pclose (session_info_stream);
    if (dyn_buffer)
        free (dyn_buffer);
    return 0;
}

static int detect_tmux ()
{
    char *tmux_env = getenv ("TMUX"), *pos;
    if (!tmux_env)
        return 0;

    /* find second separator */
    pos = strrchr (tmux_env, ',');
    if (!pos)
        return 0;

    /* store the session number string for later use */
    strcpy (mplex_data, pos + 1);
    mplex = MPLEX_TMUX;
    return 1;
}

/* Checks whether a terminal multiplexer (mplex) is present, and finds
   its unix socket.

   GNU screen and tmux are supported.

   Returns 1 if present, 0 otherwise. This value can be used to determine
   whether an auto-away detection timer is needed.
 */
static int detect_mplex ()
{
    /* try screen, and if fails try tmux */
    return detect_gnu_screen () || detect_tmux ();
}

/* Detects gnu screen session attached/detached by examining permissions of
   the session's unix socket.
 */
static int gnu_screen_is_detached ()
{
    if (mplex != MPLEX_SCREEN)
        return 0;

    struct stat sb;
    if (stat (mplex_data, &sb) != 0)
        return 0;

    /* execution permission (x) means attached */
    return ! (sb.st_mode & S_IXUSR);
}

/* Detects tmux attached/detached by getting session data and finding the
   current session's entry. An attached entry ends with "(attached)". Example:

    $ tmux list-sessions
    0: 1 windows (created Mon Mar  2 21:48:29 2015) [80x23] (attached)
    1: 2 windows (created Mon Mar  2 21:48:43 2015) [80x23]

    In this example, session 0 is attached and session 1 is detached.
*/
static int tmux_is_detached ()
{
    if (mplex != MPLEX_TMUX)
        return 0;

    FILE *session_info_stream = NULL;
    char *dyn_buffer = NULL, *search_str = NULL;
    char *entry_pos, *nl_pos, *attached_pos;
    const int numstr_len = strlen (mplex_data);

    session_info_stream = popen ("env LC_ALL=C tmux list-sessions", "r");
    if (!session_info_stream)
        goto fail;

    dyn_buffer = read_into_dyn_buffer (session_info_stream);
    if (!dyn_buffer)
        goto fail;

    pclose (session_info_stream);
    session_info_stream = NULL;

    /* prepare search string, for finding the current session's entry */
    search_str = (char*) malloc (numstr_len + 4);
    search_str[0] = '\n';
    strcpy (search_str + 1, mplex_data);
    strcat (search_str, ": ");

    /* do the search */
    if (strncmp (dyn_buffer, search_str + 1, numstr_len + 2) == 0)
        entry_pos = dyn_buffer;
    else
        entry_pos = strstr (dyn_buffer, search_str);

    if (! entry_pos)
        goto fail;

    /* find the next \n and look for the "(attached)" before it */
    nl_pos = strchr (entry_pos + 1, '\n');
    attached_pos = strstr (entry_pos + 1, "(attached)\n");

    free (search_str);
    search_str = NULL;

    free (dyn_buffer);
    dyn_buffer = NULL;

    return attached_pos == NULL  ||  attached_pos > nl_pos;

fail:
    if (session_info_stream)
        pclose (session_info_stream);
    if (dyn_buffer)
        free (dyn_buffer);
    if (search_str)
        free (search_str);
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
static int mplex_is_detached ()
{
    return gnu_screen_is_detached ()  ||  tmux_is_detached ();
}

static void mplex_timer_handler (Tox *m)
{
    TOX_USER_STATUS current_status, new_status;
    const char *new_note;

    if (mplex == MPLEX_NONE)
        return;

    int detached = mplex_is_detached ();

    pthread_mutex_lock (&Winthread.lock);
    current_status = tox_self_get_status (m);
    pthread_mutex_unlock (&Winthread.lock);

    if (auto_away_active && current_status == TOX_USER_STATUS_AWAY && !detached)
    {
        auto_away_active = false;
        new_status = prev_status;
        new_note = prev_note;
    }
    else
    if (current_status == TOX_USER_STATUS_NONE && detached)
    {
        auto_away_active = true;
        prev_status = current_status;
        new_status = TOX_USER_STATUS_AWAY;
        pthread_mutex_lock (&Winthread.lock);
        tox_self_get_status_message (m, (uint8_t*) prev_note);
        pthread_mutex_unlock (&Winthread.lock);
        new_note = user_settings->mplex_away_note;
    }
    else
        return;

    char argv[3][MAX_STR_SIZE];
    strcpy (argv[0], "/status");
    strcpy (argv[1], (new_status == TOX_USER_STATUS_AWAY ? "away" :
                      new_status == TOX_USER_STATUS_BUSY ? "busy" : "online"));
    argv[2][0] = '\"';
    strcpy (argv[2] + 1, new_note);
    strcat (argv[2], "\"");
    pthread_mutex_lock (&Winthread.lock);
    cmd_status (prompt->chatwin->history, prompt, m, 2, argv);
    pthread_mutex_unlock (&Winthread.lock);
}

/* Time in seconds between calls to mplex_timer_handler */
#define MPLEX_TIMER_INTERVAL 5

void *mplex_timer_thread(void *data)
{
    Tox *m = (Tox *) data;

    while (true) {
        sleep(MPLEX_TIMER_INTERVAL);
        mplex_timer_handler(m);
    }
}

int init_mplex_away_timer (Tox *m)
{
    if (! detect_mplex ())
        return 0;

    if (! user_settings->mplex_away)
        return 0;

    /* status access mutex */
    if (pthread_mutex_init (&status_lock, NULL) != 0)
        return -1;

    if (pthread_create(&mplex_tid, NULL, mplex_timer_thread, (void *) m) != 0)
        return -1;

    return 0;
}

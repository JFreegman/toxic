/*  notify.c
 *
 *  Copyright (C) 2014-2024 Toxic All Rights Reserved.
 *
 *  This file is part of Toxic. Toxic is free software licensed
 *  under the GNU General Public License 3.0.
 */

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "audio_device.h"
#include "line_info.h"
#include "misc_tools.h"
#include "notify.h"
#include "settings.h"

#ifdef X11
#include "x11focus.h"
#endif  /* X11 */

#if defined(AUDIO) || defined(SOUND_NOTIFY)
#ifdef __APPLE__
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
/* compatibility with older versions of OpenAL */
#ifndef ALC_ALL_DEVICES_SPECIFIER
#include <AL/alext.h>
#endif /* ALC_ALL_DEVICES_SPECIFIER */
#endif /* __APPLE__ */
#ifdef SOUND_NOTIFY
#include <AL/alut.h> /* freealut packet */
#endif /* SOUND_NOTIFY */
#endif /* defined(AUDIO) || defined(SOUND_NOTIFY) */

#ifdef BOX_NOTIFY
#include <libnotify/notify.h>
#endif  /* BOX_NOTIFY */

#define MAX_BOX_MSG_LEN 127
#define ACTIVE_NOTIFS_MAX 10

#ifdef SOUND_NOTIFY
#define SOUNDS_SIZE 10
#endif  /* SOUND_NOTIFY */

#define CONTENT_HIDDEN_MESSAGE "[Content hidden]"

static_assert(sizeof(CONTENT_HIDDEN_MESSAGE) < MAX_BOX_MSG_LEN,
              "sizeof(CONTENT_HIDDEN_MESSAGE) >= MAX_BOX_MSG_LEN");

static struct Control {
    time_t cooldown;
    time_t notif_timeout;

#if defined(SOUND_NOTIFY) || defined(BOX_NOTIFY)
    pthread_mutex_t poll_mutex[1];
    bool poll_active;
#endif

#ifdef SOUND_NOTIFY
    uint32_t device_idx; /* index of output device */
    char *sounds[SOUNDS_SIZE];
#endif /* SOUND_NOTIFY */
} Control = {0};

static struct _ActiveNotifications {
#ifdef SOUND_NOTIFY
    uint32_t source;
    uint32_t buffer;
    bool looping;
#endif /* SOUND_NOTIFY */
    bool active;
    int *id_indicator;
#ifdef BOX_NOTIFY
    NotifyNotification *box;
    char messages[MAX_BOX_MSG_LEN + 1][MAX_BOX_MSG_LEN + 1];
    char title[64];
    size_t size;
    time_t n_timeout;
#endif /* BOX_NOTIFY */
} actives[ACTIVE_NOTIFS_MAX];

/**********************************************************************************/
/**********************************************************************************/
/**********************************************************************************/
/**********************************************************************************/
/**********************************************************************************/

static void clear_actives_index(size_t idx)
{
    if (actives[idx].id_indicator) {
        *actives[idx].id_indicator = -1;
    }

    actives[idx] = (struct _ActiveNotifications) {
        0
    };
}

/* coloured tab notifications: primary notification type */
static void tab_notify(ToxWindow *self, uint64_t flags)
{
    if (self == NULL) {
        return;
    }

    if (flags & NT_WNDALERT_0) {
        self->alert = WINDOW_ALERT_0;
    } else if ((flags & NT_WNDALERT_1) && (!self->alert || self->alert > WINDOW_ALERT_0)) {
        self->alert = WINDOW_ALERT_1;
    } else if ((flags & NT_WNDALERT_2) && (!self->alert || self->alert > WINDOW_ALERT_1)) {
        self->alert = WINDOW_ALERT_2;
    }

    ++self->pending_messages;
}

static bool notifications_are_disabled(const Toxic *toxic, uint64_t flags)
{
    const Client_Config *c_config = toxic->c_config;

    if (!c_config->alerts) {
        return true;
    }

    bool res = (flags & NT_RESTOL) && (Control.cooldown > get_unix_time());
#ifdef X11
    return res || ((flags & NT_NOFOCUS) && is_focused(&toxic->x11_focus));
#else
    return res;
#endif
}

static void control_lock(void)
{
#if defined(SOUND_NOTIFY) || defined(BOX_NOTIFY)
    pthread_mutex_lock(Control.poll_mutex);
#endif
}

static void control_unlock(void)
{
#if defined(SOUND_NOTIFY) || defined(BOX_NOTIFY)
    pthread_mutex_unlock(Control.poll_mutex);
#endif
}

#ifdef SOUND_NOTIFY
static bool is_playing(int source)
{
    int ready;
    alGetSourcei(source, AL_SOURCE_STATE, &ready);
    return ready == AL_PLAYING;
}

/* TODO maybe find better way to do this */
/* cooldown is in seconds */
#define DEVICE_COOLDOWN 5 /* TODO perhaps load this from config? */
static bool device_opened = false;
static time_t last_opened_update = 0;

/* Opens primary device. Returns true on succe*/
static void m_open_device(const Client_Config *c_config)
{
    last_opened_update = get_unix_time();

    if (device_opened) {
        return;
    }

#ifdef AUDIO
    const double VAD_threshold = c_config->VAD_threshold;
#else
    const double VAD_threshold = 0;
#endif  // AUDIO

    /* TODO: error check */
    open_output_device(&Control.device_idx, 48000, 20, 1, VAD_threshold);

    device_opened = true;
}

static void m_close_device(void)
{
    if (!device_opened) {
        return;
    }

    close_device(output, Control.device_idx);

    device_opened = false;
}

/* Terminate all sounds but wait for them to finish first */
static void graceful_clear(void)
{
    control_lock();

    while (1) {
        int i;

        for (i = 0; i < ACTIVE_NOTIFS_MAX; ++i) {
            if (actives[i].active) {
#ifdef BOX_NOTIFY

                if (actives[i].box) {
                    GError *ignore;
                    notify_notification_close(actives[i].box, &ignore);
                    actives[i].box = NULL;
                }

#endif /* BOX_NOTIFY */

                if (actives[i].id_indicator) {
                    *actives[i].id_indicator = -1;    /* reset indicator value */
                }

                if (actives[i].looping) {
                    stop_sound(i);
                } else {
                    if (!is_playing(actives[i].source)) {
                        clear_actives_index(i);
                    } else {
                        break;
                    }
                }
            }
        }

        if (i == ACTIVE_NOTIFS_MAX) {
            m_close_device(); /* In case it's opened */
            control_unlock();
            return;
        }

        sleep_thread(1000L);
    }

    control_unlock();
}

static void *do_playing(void *_p)
{
    UNUSED_VAR(_p);

    while (true) {
        control_lock();

        if (!Control.poll_active) {
            control_unlock();
            break;
        }

        bool has_looping = false;
        bool test_active_notify = false;
        int i;

        for (i = 0; i < ACTIVE_NOTIFS_MAX; ++i) {

            if (actives[i].looping) {
                has_looping = true;
            }

            test_active_notify = actives[i].active && !actives[i].looping;
#ifdef BOX_NOTIFY
            test_active_notify = test_active_notify && !actives[i].box;
#endif

            if (test_active_notify) {
                if (actives[i].id_indicator) {
                    *actives[i].id_indicator = -1;    /* reset indicator value */
                }

                if (!is_playing(actives[i].source)) {
                    /* Close */
                    alSourceStop(actives[i].source);
                    alDeleteSources(1, &actives[i].source);
                    alDeleteBuffers(1, &actives[i].buffer);
                    clear_actives_index(i);
                }
            }

#ifdef BOX_NOTIFY
            else if (actives[i].box && time(NULL) >= actives[i].n_timeout) {
                GError *ignore;
                notify_notification_close(actives[i].box, &ignore);
                actives[i].box = NULL;

                if (actives[i].id_indicator) {
                    *actives[i].id_indicator = -1;    /* reset indicator value */
                }

                if (!actives[i].looping && !is_playing(actives[i].source)) {
                    /* stop source if not looping or playing, just terminate box */
                    alSourceStop(actives[i].source);
                    alDeleteSources(1, &actives[i].source);
                    alDeleteBuffers(1, &actives[i].buffer);
                    clear_actives_index(i);
                }
            }

#endif /* BOX_NOTIFY */
        }

        /* device is opened and no activity in under DEVICE_COOLDOWN time, close device*/
        if (device_opened && !has_looping &&
                (time(NULL) - last_opened_update) > DEVICE_COOLDOWN) {
            m_close_device();
        }

        has_looping = false;

        control_unlock();
        sleep_thread(100000L);
    }

    pthread_exit(NULL);
}

static int play_source(uint32_t source, uint32_t buffer, bool looping)
{
    int i = 0;

    for (; i < ACTIVE_NOTIFS_MAX && actives[i].active; ++i);

    if (i == ACTIVE_NOTIFS_MAX) {
        return -1; /* Full */
    }

    alSourcePlay(source);

    actives[i].active = 1;
    actives[i].source = source;
    actives[i].buffer = buffer;
    actives[i].looping = looping;

    return i;
}

#elif BOX_NOTIFY
static void *do_playing(void *_p)
{
    UNUSED_VAR(_p);

    while (true) {
        control_lock();

        if (!Control.poll_active) {
            control_unlock();
            break;
        }

        for (size_t i = 0; i < ACTIVE_NOTIFS_MAX; ++i) {
            if (actives[i].box && time(NULL) >= actives[i].n_timeout) {
                GError *ignore;
                notify_notification_close(actives[i].box, &ignore);
                clear_actives_index(i);
            }
        }

        control_unlock();
        sleep_thread(10000L);
    }

    pthread_exit(NULL);
}

static void graceful_clear(void)
{
    control_lock();

    for (size_t i = 0; i < ACTIVE_NOTIFS_MAX; ++i) {
        if (actives[i].box) {
            GError *ignore;
            notify_notification_close(actives[i].box, &ignore);
        }

        clear_actives_index(i);
    }

    control_unlock();
}

#endif /* SOUND_NOTIFY */

/* Kills all notifications for `id`. This must be called before freeing a ToxWindow. */
void kill_notifs(int id)
{
    control_lock();

    for (size_t i = 0; i < ACTIVE_NOTIFS_MAX; ++i) {
        if (!actives[i].id_indicator) {
            continue;
        }

        if (*actives[i].id_indicator == id) {
#ifdef BOX_NOTIFY

            if (actives[i].box) {
                GError *ignore;
                notify_notification_close(actives[i].box, &ignore);
            }

#endif // BOX_NOTIFY
            clear_actives_index(i);
        }
    }

    control_unlock();
}

/**********************************************************************************/
/**********************************************************************************/
/**********************************************************************************/
/**********************************************************************************/
/**********************************************************************************/



/* Opens primary device */
int init_notify(int login_cooldown, int notification_timeout)
{
#ifdef SOUND_NOTIFY
    alutInitWithoutContext(NULL, NULL);
#endif /* SOUND_NOTIFY */

#if defined(SOUND_NOTIFY) || defined(BOX_NOTIFY)

    if (pthread_mutex_init(Control.poll_mutex, NULL) != 0) {
        return -1;
    }

    Control.poll_active = 1;
    pthread_t thread;

    if (pthread_create(&thread, NULL, do_playing, NULL) != 0 || pthread_detach(thread) != 0) {
        pthread_mutex_destroy(Control.poll_mutex);
        Control.poll_active = 0;
        return -1;
    }

#endif /* defined(SOUND_NOTIFY) || defined(BOX_NOTIFY) */
    Control.cooldown = time(NULL) + login_cooldown;


#ifdef BOX_NOTIFY
    notify_init("Toxic");
#endif
    Control.notif_timeout = notification_timeout;
    return 1;
}

void terminate_notify(void)
{
#if defined(SOUND_NOTIFY) || defined(BOX_NOTIFY)
    control_lock();

    if (!Control.poll_active) {
        control_unlock();
        return;
    }

    Control.poll_active = 0;
    control_unlock();

    graceful_clear();
#endif /* defined(SOUND_NOTIFY) || defined(BOX_NOTIFY) */

#ifdef SOUND_NOTIFY
    int i = 0;

    for (; i < SOUNDS_SIZE; ++i) {
        free(Control.sounds[i]);
    }

    alutExit();
#endif /* SOUND_NOTIFY */

#ifdef BOX_NOTIFY
    notify_uninit();
#endif
}

#ifdef SOUND_NOTIFY

/*
 * Sets notification sound designated by `sound` to file path `value`.
 *
 * Return true if the sound is successfully set.
 */
bool set_sound(Notification sound, const char *value)
{
    if (sound == silent) {
        return false;
    }

    free(Control.sounds[sound]);

    size_t len = strlen(value) + 1;
    Control.sounds[sound] = calloc(len, 1);

    if (Control.sounds[sound] == NULL) {
        return false;
    }

    memcpy(Control.sounds[sound], value, len);

    struct stat buf;
    return stat(value, &buf) == 0;
}

static int play_sound_internal(const Client_Config *c_config, Notification what, bool loop)
{
    uint32_t source;
    uint32_t buffer;

    m_open_device(c_config);

    alGenSources(1, &source);
    alGenBuffers(1, &buffer);
    buffer = alutCreateBufferFromFile(Control.sounds[what]);
    alSourcei(source, AL_BUFFER, buffer);
    alSourcei(source, AL_LOOPING, loop);

    int rc = play_source(source, buffer, loop);

    if (rc < 0) {
        alSourceStop(source);
        alDeleteSources(1, &source);
        alDeleteBuffers(1, &buffer);
        return -1;
    }

    return rc;
}

static int play_notify_sound(const Client_Config *c_config, Notification notif, uint64_t flags)
{
    int rc = -1;

    if (flags & NT_BEEP) {
        beep();
    }

    if (notif != silent) {
        if (!Control.poll_active || !Control.sounds[notif]) {
            return -1;
        }

        rc = play_sound_internal(c_config, notif, flags & NT_LOOP ? 1 : 0);
    }

    return rc;
}

void stop_sound(int id)
{
    if (id >= 0 && id < ACTIVE_NOTIFS_MAX && actives[id].looping && actives[id].active) {
#ifdef BOX_NOTIFY

        if (actives[id].box) {
            GError *ignore;
            notify_notification_close(actives[id].box, &ignore);
        }

#endif /* BOX_NOTIFY */

        // alSourcei(actives[id].source, AL_LOOPING, false);
        alSourceStop(actives[id].source);
        alDeleteSources(1, &actives[id].source);
        alDeleteBuffers(1, &actives[id].buffer);
        clear_actives_index(id);
    }
}

#endif /* SOUND_NOTIFY */

static int m_play_sound(const Client_Config *c_config, Notification notif, uint64_t flags)
{
#ifdef SOUND_NOTIFY
    return play_notify_sound(c_config, notif, flags);
#else

    if (notif != silent) {
        beep();
    }

    return -1;
#endif /* SOUND_NOTIFY */
}

int sound_notify(ToxWindow *self, const Toxic *toxic, Notification notif, uint64_t flags,
                 int *id_indicator)
{
    const Client_Config *c_config = toxic->c_config;

    tab_notify(self, flags);

    if (notifications_are_disabled(toxic, flags)) {
        return -1;
    }

    int id = -1;
    control_lock();

    if (self && (!self->stb || self->stb->status != TOX_USER_STATUS_BUSY)) {
        id = m_play_sound(c_config, notif, flags);
    } else if (flags & NT_ALWAYS) {
        id = m_play_sound(c_config, notif, flags);
    }

#if defined(BOX_NOTIFY) && !defined(SOUND_NOTIFY)

    if (id == -1) {
        for (id = 0; id < ACTIVE_NOTIFS_MAX && actives[id].box; id++);

        if (id == ACTIVE_NOTIFS_MAX) {
            control_unlock();
            return -1; /* Full */
        }
    }

#endif /* defined(BOX_NOTIFY) && !defined(SOUND_NOTIFY) */

    if (id_indicator && id != -1) {
        actives[id].id_indicator = id_indicator;
        *id_indicator = id;
    }

    control_unlock();

    return id;
}

int sound_notify2(ToxWindow *self, const Toxic *toxic, Notification notif, uint64_t flags, int id)
{
    tab_notify(self, flags);

    if (notifications_are_disabled(toxic, flags)) {
        return -1;
    }

    if (id < 0 || id >= ACTIVE_NOTIFS_MAX) {
        return -1;
    }

#ifdef SOUND_NOTIFY
    control_lock();

    if (!actives[id].active || !Control.sounds[notif]) {
        control_unlock();
        return -1;
    }

    m_open_device(toxic->c_config);

    alSourceStop(actives[id].source);
    alDeleteSources(1, &actives[id].source);
    alDeleteBuffers(1, &actives[id].buffer);

    alGenSources(1, &actives[id].source);
    alGenBuffers(1, &actives[id].buffer);
    actives[id].buffer = alutCreateBufferFromFile(Control.sounds[notif]);
    alSourcei(actives[id].source, AL_BUFFER, actives[id].buffer);
    alSourcei(actives[id].source, AL_LOOPING, flags & NT_LOOP);

    alSourcePlay(actives[id].source);

    control_unlock();

    return id;
#else

    if (notif != silent) {
        beep();
    }

    return 0;
#endif /* SOUND_NOTIFY */
}

int box_notify(ToxWindow *self, const Toxic *toxic, Notification notif, uint64_t flags,
               int *id_indicator, const char *title, const char *format, ...)
{
    if (notifications_are_disabled(toxic, flags)) {
        tab_notify(self, flags);
        return -1;
    }

#ifdef BOX_NOTIFY

    const Client_Config *c_config = toxic->c_config;

    int id = sound_notify(self, toxic, notif, flags, id_indicator);

    control_lock();

#ifdef SOUND_NOTIFY

    if (id == -1) { /* Could not play */

        for (id = 0; id < ACTIVE_NOTIFS_MAX && actives[id].active; id ++);

        if (id == ACTIVE_NOTIFS_MAX) {
            control_unlock();
            return -1; /* Full */
        }

        actives[id].active = 1;
        actives[id].id_indicator = id_indicator;

        if (id_indicator) {
            *id_indicator = id;
        }
    }

#else

    if (id == -1) {
        control_unlock();
        return -1;
    }

#endif /* SOUND_NOTIFY */

    snprintf(actives[id].title, sizeof(actives[id].title), "%s", title);

    if (strlen(title) > 23) {
        strcpy(actives[id].title + 20, "...");
    }

    if (c_config->show_notification_content) {
        va_list __ARGS__;
        va_start(__ARGS__, format);
        vsnprintf(actives[id].messages[0], MAX_BOX_MSG_LEN, format, __ARGS__);
        va_end(__ARGS__);
    } else {
        snprintf(actives[id].messages[0], MAX_BOX_MSG_LEN, "%s", CONTENT_HIDDEN_MESSAGE);
    }

    if (strlen(actives[id].messages[0]) > MAX_BOX_MSG_LEN - 3) {
        strcpy(actives[id].messages[0] + MAX_BOX_MSG_LEN - 3, "...");
    }

    actives[id].box = notify_notification_new(actives[id].title, actives[id].messages[0], NULL);
    actives[id].size++;
    actives[id].n_timeout = get_unix_time() + Control.notif_timeout / 1000;

    notify_notification_set_timeout(actives[id].box, Control.notif_timeout);
    notify_notification_set_app_name(actives[id].box, "toxic");
    notify_notification_show(actives[id].box, NULL);

    control_unlock();
    return id;
#else
    return sound_notify(self, toxic, notif, flags, id_indicator);
#endif /* BOX_NOTIFY */
}

int box_notify2(ToxWindow *self, const Toxic *toxic, Notification notif, uint64_t flags,
                int id, const char *format, ...)
{
    if (notifications_are_disabled(toxic, flags)) {
        tab_notify(self, flags);
        return -1;
    }

#ifdef BOX_NOTIFY

    const Client_Config *c_config = toxic->c_config;

    if (sound_notify2(self, toxic, notif, flags, id) == -1) {
        return -1;
    }

    control_lock();

    if (!actives[id].box || actives[id].size >= MAX_BOX_MSG_LEN + 1) {
        control_unlock();
        return -1;
    }

    if (c_config->show_notification_content) {
        va_list __ARGS__;
        va_start(__ARGS__, format);
        vsnprintf(actives[id].messages[actives[id].size], MAX_BOX_MSG_LEN, format, __ARGS__);
        va_end(__ARGS__);
    } else {
        snprintf(actives[id].messages[actives[id].size], MAX_BOX_MSG_LEN, "%s", CONTENT_HIDDEN_MESSAGE);
    }

    if (strlen(actives[id].messages[actives[id].size]) > MAX_BOX_MSG_LEN - 3) {
        strcpy(actives[id].messages[actives[id].size] + MAX_BOX_MSG_LEN - 3, "...");
    }

    actives[id].size++;
    actives[id].n_timeout = get_unix_time() + Control.notif_timeout / 1000;

    char *formatted = calloc(1, sizeof(char) * ((MAX_BOX_MSG_LEN + 1) * (MAX_BOX_MSG_LEN + 2)));

    for (size_t i = 0; i < actives[id].size; ++i) {
        strcat(formatted, actives[id].messages[i]);
        strcat(formatted, "\n");
    }

    notify_notification_update(actives[id].box, actives[id].title, formatted, NULL);
    notify_notification_show(actives[id].box, NULL);

    free(formatted);

    control_unlock();

    return id;
#else
    return sound_notify2(self, toxic, notif, flags, id);
#endif /* BOX_NOTIFY */
}

int box_silent_notify(ToxWindow *self, const Toxic *toxic, uint64_t flags, int *id_indicator,
                      const char *title, const char *format, ...)
{
    tab_notify(self, flags);

    if (notifications_are_disabled(toxic, flags)) {
        return -1;
    }

#ifdef BOX_NOTIFY

    const Client_Config *c_config = toxic->c_config;

    control_lock();

    int id;

    for (id = 0; id < ACTIVE_NOTIFS_MAX && actives[id].active; id ++);

    if (id == ACTIVE_NOTIFS_MAX) {
        control_unlock();
        return -1; /* Full */
    }

    if (id_indicator) {
        actives[id].id_indicator = id_indicator;
        *id_indicator = id;
    }

    snprintf(actives[id].title, sizeof(actives[id].title), "%s", title);

    if (strlen(title) > 23) {
        strcpy(actives[id].title + 20, "...");
    }

    if (c_config->show_notification_content) {
        va_list __ARGS__;
        va_start(__ARGS__, format);
        vsnprintf(actives[id].messages[0], MAX_BOX_MSG_LEN, format, __ARGS__);
        va_end(__ARGS__);
    } else {
        snprintf(actives[id].messages[0], MAX_BOX_MSG_LEN, "%s", CONTENT_HIDDEN_MESSAGE);
    }

    if (strlen(actives[id].messages[0]) > MAX_BOX_MSG_LEN - 3) {
        strcpy(actives[id].messages[0] + MAX_BOX_MSG_LEN - 3, "...");
    }

    actives[id].active = 1;
    actives[id].box = notify_notification_new(actives[id].title, actives[id].messages[0], NULL);
    actives[id].size ++;
    actives[id].n_timeout = get_unix_time() + Control.notif_timeout / 1000;

    notify_notification_set_timeout(actives[id].box, Control.notif_timeout);
    notify_notification_set_app_name(actives[id].box, "toxic");
    notify_notification_show(actives[id].box, NULL);

    control_unlock();
    return id;
#else
    return -1;
#endif /* BOX_NOTIFY */
}

int box_silent_notify2(ToxWindow *self, const Toxic *toxic, uint64_t flags, int id,
                       const char *format, ...)
{
    tab_notify(self, flags);

    if (notifications_are_disabled(toxic, flags)) {
        return -1;
    }

#ifdef BOX_NOTIFY

    const Client_Config *c_config = toxic->c_config;

    control_lock();

    if (id < 0 || id >= ACTIVE_NOTIFS_MAX || !actives[id].box || actives[id].size >= MAX_BOX_MSG_LEN + 1) {
        control_unlock();
        return -1;
    }

    if (c_config->show_notification_content) {
        va_list __ARGS__;
        va_start(__ARGS__, format);
        vsnprintf(actives[id].messages[actives[id].size], MAX_BOX_MSG_LEN, format, __ARGS__);
        va_end(__ARGS__);
    } else {
        snprintf(actives[id].messages[actives[id].size], MAX_BOX_MSG_LEN, "%s", CONTENT_HIDDEN_MESSAGE);
    }

    if (strlen(actives[id].messages[actives[id].size]) > MAX_BOX_MSG_LEN - 3) {
        strcpy(actives[id].messages[actives[id].size] + MAX_BOX_MSG_LEN - 3, "...");
    }

    actives[id].size ++;
    actives[id].n_timeout = get_unix_time() + Control.notif_timeout / 1000;

    char *formatted = calloc(1, sizeof(char) * ((MAX_BOX_MSG_LEN + 1) * (MAX_BOX_MSG_LEN + 2)));

    for (size_t i = 0; i < actives[id].size; ++i) {
        strcat(formatted, actives[id].messages[i]);
        strcat(formatted, "\n");
    }

    notify_notification_update(actives[id].box, actives[id].title, formatted, NULL);
    notify_notification_show(actives[id].box, NULL);

    free(formatted);

    control_unlock();

    return id;
#else
    return -1;
#endif /* BOX_NOTIFY */
}

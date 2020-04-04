/*  notify.c
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdarg.h>
#include <time.h>
#include <assert.h>
#include <sys/stat.h>

#include "notify.h"
#include "audio_device.h"
#include "settings.h"
#include "line_info.h"
#include "misc_tools.h"
#include "xtra.h"

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
#endif

#define MAX_BOX_MSG_LEN 127
#define SOUNDS_SIZE 10
#define ACTIVE_NOTIFS_MAX 50

extern struct user_settings *user_settings;

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
}

static bool notifications_are_disabled(uint64_t flags)
{
    if (user_settings->alerts != ALERTS_ENABLED) {
        return true;
    }

    bool res = (flags & NT_RESTOL) && (Control.cooldown > get_unix_time());
#ifdef X11
    return res || ((flags & NT_NOFOCUS) && is_focused());
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
bool is_playing(int source)
{
    int ready;
    alGetSourcei(source, AL_SOURCE_STATE, &ready);
    return ready == AL_PLAYING;
}

/* TODO maybe find better way to do this */
/* cooldown is in seconds */
#define DEVICE_COOLDOWN 5 /* TODO perhaps load this from config? */
static bool device_opened = false;
time_t last_opened_update = 0;

/* Opens primary device. Returns true on succe*/
void m_open_device(void)
{
    last_opened_update = get_unix_time();

    if (device_opened) {
        return;
    }

    /* Blah error check */
    open_output_device(&Control.device_idx, 48000, 20, 1);

    device_opened = true;
}

void m_close_device(void)
{
    if (!device_opened) {
        return;
    }

    close_device(output, Control.device_idx);

    device_opened = false;
}

/* Terminate all sounds but wait for them to finish first */
void graceful_clear(void)
{
    control_lock();

    while (1) {
        int i;

        for (i = 0; i < ACTIVE_NOTIFS_MAX; i ++) {
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
                        memset(&actives[i], 0, sizeof(struct _ActiveNotifications));
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

        usleep(1000);
    }

    control_unlock();
}

void *do_playing(void *_p)
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

        for (i = 0; i < ACTIVE_NOTIFS_MAX; i ++) {

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
                    memset(&actives[i], 0, sizeof(struct _ActiveNotifications));
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
                    memset(&actives[i], 0, sizeof(struct _ActiveNotifications));
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
        usleep(10000);
    }

    pthread_exit(NULL);
}

int play_source(uint32_t source, uint32_t buffer, bool looping)
{
    int i = 0;

    for (; i < ACTIVE_NOTIFS_MAX && actives[i].active; i ++);

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
void *do_playing(void *_p)
{
    UNUSED_VAR(_p);

    while (true) {
        control_lock();

        if (!Control.poll_active) {
            control_unlock();
            break;
        }

        int i;

        for (i = 0; i < ACTIVE_NOTIFS_MAX; i ++) {
            if (actives[i].box && time(NULL) >= actives[i].n_timeout) {
                GError *ignore;
                notify_notification_close(actives[i].box, &ignore);
                actives[i].box = NULL;

                if (actives[i].id_indicator) {
                    *actives[i].id_indicator = -1;    /* reset indicator value */
                }

                memset(&actives[i], 0, sizeof(struct _ActiveNotifications));
            }
        }

        control_unlock();
        usleep(10000);
    }

    pthread_exit(NULL);
}

void graceful_clear(void)
{
    int i;
    control_lock();

    for (i = 0; i < ACTIVE_NOTIFS_MAX; i ++) {
        if (actives[i].box) {
            GError *ignore;
            notify_notification_close(actives[i].box, &ignore);
            actives[i].box = NULL;
        }

        if (actives[i].id_indicator) {
            *actives[i].id_indicator = -1;    /* reset indicator value */
        }

        memset(&actives[i], 0, sizeof(struct _ActiveNotifications));
    }

    control_unlock();
}
#endif /* SOUND_NOTIFY */

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

    for (; i < SOUNDS_SIZE; i ++) {
        free(Control.sounds[i]);
    }

    alutExit();
#endif /* SOUND_NOTIFY */

#ifdef BOX_NOTIFY
    notify_uninit();
#endif
}

#ifdef SOUND_NOTIFY
int set_sound(Notification sound, const char *value)
{
    if (sound == silent) {
        return 0;
    }

    free(Control.sounds[sound]);

    size_t len = strlen(value) + 1;
    Control.sounds[sound] = calloc(len, 1);
    memcpy(Control.sounds[sound], value, len);

    struct stat buf;
    return stat(value, &buf) == 0;
}

int play_sound_internal(Notification what, bool loop)
{
    uint32_t source;
    uint32_t buffer;

    m_open_device();

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

int play_notify_sound(Notification notif, uint64_t flags)
{
    int rc = -1;

    if (flags & NT_BEEP) {
        beep();
    }

    if (notif != silent) {
        if (!Control.poll_active || !Control.sounds[notif]) {
            return -1;
        }

        rc = play_sound_internal(notif, flags & NT_LOOP ? 1 : 0);
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

        if (actives[id].id_indicator) {
            *actives[id].id_indicator = -1;
        }

        // alSourcei(actives[id].source, AL_LOOPING, false);
        alSourceStop(actives[id].source);
        alDeleteSources(1, &actives[id].source);
        alDeleteBuffers(1, &actives[id].buffer);
        memset(&actives[id], 0, sizeof(struct _ActiveNotifications));
    }
}
#endif /* SOUND_NOTIFY */

static int m_play_sound(Notification notif, uint64_t flags)
{
#ifdef SOUND_NOTIFY
    return play_notify_sound(notif, flags);
#else

    if (notif != silent) {
        beep();
    }

    return -1;
#endif /* SOUND_NOTIFY */
}

int sound_notify(ToxWindow *self, Notification notif, uint64_t flags, int *id_indicator)
{
    tab_notify(self, flags);

    if (notifications_are_disabled(flags)) {
        return -1;
    }

    int id = -1;
    control_lock();

    if (self && (!self->stb || self->stb->status != TOX_USER_STATUS_BUSY)) {
        id = m_play_sound(notif, flags);
    } else if (flags & NT_ALWAYS) {
        id = m_play_sound(notif, flags);
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

int sound_notify2(ToxWindow *self, Notification notif, uint64_t flags, int id)
{
    tab_notify(self, flags);

    if (notifications_are_disabled(flags)) {
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

    m_open_device();

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

int box_notify(ToxWindow *self, Notification notif, uint64_t flags, int *id_indicator, const char *title,
               const char *format, ...)
{
    if (notifications_are_disabled(flags)) {
        tab_notify(self, flags);
        return -1;
    }

#ifdef BOX_NOTIFY

    int id = sound_notify(self, notif, flags, id_indicator);

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
        return -1;
    }

#endif /* SOUND_NOTIFY */

    snprintf(actives[id].title, sizeof(actives[id].title), "%s", title);

    if (strlen(title) > 23) {
        strcpy(actives[id].title + 20, "...");
    }

    va_list __ARGS__;
    va_start(__ARGS__, format);
    vsnprintf(actives[id].messages[0], MAX_BOX_MSG_LEN, format, __ARGS__);
    va_end(__ARGS__);

    if (strlen(actives[id].messages[0]) > MAX_BOX_MSG_LEN - 3) {
        strcpy(actives[id].messages[0] + MAX_BOX_MSG_LEN - 3, "...");
    }

    actives[id].box = notify_notification_new(actives[id].title, actives[id].messages[0], NULL);
    actives[id].size++;
    actives[id].n_timeout = get_unix_time() + Control.notif_timeout / 1000;

    notify_notification_set_timeout(actives[id].box, Control.notif_timeout);
    notify_notification_set_app_name(actives[id].box, "toxic");
    /*notify_notification_add_action(actives[id].box, "lel", "default", m_notify_action, self, NULL);*/
    notify_notification_show(actives[id].box, NULL);

    control_unlock();
    return id;
#else
    return sound_notify(self, notif, flags, id_indicator);
#endif /* BOX_NOTIFY */
}

int box_notify2(ToxWindow *self, Notification notif, uint64_t flags, int id, const char *format, ...)
{
    if (notifications_are_disabled(flags)) {
        tab_notify(self, flags);
        return -1;
    }

#ifdef BOX_NOTIFY

    if (sound_notify2(self, notif, flags, id) == -1) {
        return -1;
    }

    control_lock();

    if (!actives[id].box || actives[id].size >= MAX_BOX_MSG_LEN + 1) {
        control_unlock();
        return -1;
    }

    va_list __ARGS__;
    va_start(__ARGS__, format);
    vsnprintf(actives[id].messages[actives[id].size], MAX_BOX_MSG_LEN, format, __ARGS__);
    va_end(__ARGS__);

    if (strlen(actives[id].messages[actives[id].size]) > MAX_BOX_MSG_LEN - 3) {
        strcpy(actives[id].messages[actives[id].size] + MAX_BOX_MSG_LEN - 3, "...");
    }

    actives[id].size++;
    actives[id].n_timeout = get_unix_time() + Control.notif_timeout / 1000;

    char formated[128 * 129] = {'\0'};

    int i = 0;

    for (; i < actives[id].size; i ++) {
        strcat(formated, actives[id].messages[i]);
        strcat(formated, "\n");
    }

    formated[strlen(formated) - 1] = '\0';

    notify_notification_update(actives[id].box, actives[id].title, formated, NULL);
    notify_notification_show(actives[id].box, NULL);

    control_unlock();

    return id;
#else
    return sound_notify2(self, notif, flags, id);
#endif /* BOX_NOTIFY */
}

int box_silent_notify(ToxWindow *self, uint64_t flags, int *id_indicator, const char *title, const char *format, ...)
{
    tab_notify(self, flags);

    if (notifications_are_disabled(flags)) {
        return -1;
    }

#ifdef BOX_NOTIFY

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

    va_list __ARGS__;
    va_start(__ARGS__, format);
    vsnprintf(actives[id].messages[0], MAX_BOX_MSG_LEN, format, __ARGS__);
    va_end(__ARGS__);

    if (strlen(actives[id].messages[0]) > MAX_BOX_MSG_LEN - 3) {
        strcpy(actives[id].messages[0] + MAX_BOX_MSG_LEN - 3, "...");
    }

    actives[id].active = 1;
    actives[id].box = notify_notification_new(actives[id].title, actives[id].messages[0], NULL);
    actives[id].size ++;
    actives[id].n_timeout = get_unix_time() + Control.notif_timeout / 1000;

    notify_notification_set_timeout(actives[id].box, Control.notif_timeout);
    notify_notification_set_app_name(actives[id].box, "toxic");
    /*notify_notification_add_action(actives[id].box, "lel", "default", m_notify_action, self, NULL);*/
    notify_notification_show(actives[id].box, NULL);

    control_unlock();
    return id;
#else
    return -1;
#endif /* BOX_NOTIFY */
}

int box_silent_notify2(ToxWindow *self, uint64_t flags, int id, const char *format, ...)
{
    tab_notify(self, flags);

    if (notifications_are_disabled(flags)) {
        return -1;
    }

#ifdef BOX_NOTIFY
    control_lock();

    if (id < 0 || id >= ACTIVE_NOTIFS_MAX || !actives[id].box || actives[id].size >= MAX_BOX_MSG_LEN + 1) {
        control_unlock();
        return -1;
    }


    va_list __ARGS__;
    va_start(__ARGS__, format);
    vsnprintf(actives[id].messages[actives[id].size], MAX_BOX_MSG_LEN, format, __ARGS__);
    va_end(__ARGS__);

    if (strlen(actives[id].messages[actives[id].size]) > MAX_BOX_MSG_LEN - 3) {
        strcpy(actives[id].messages[actives[id].size] + MAX_BOX_MSG_LEN - 3, "...");
    }

    actives[id].size ++;
    actives[id].n_timeout = get_unix_time() + Control.notif_timeout / 1000;

    char formated[128 * 129] = {'\0'};

    int i = 0;

    for (; i < actives[id].size; i ++) {
        strcat(formated, actives[id].messages[i]);
        strcat(formated, "\n");
    }

    formated[strlen(formated) - 1] = '\0';

    notify_notification_update(actives[id].box, actives[id].title, formated, NULL);
    notify_notification_show(actives[id].box, NULL);

    control_unlock();

    return id;
#else
    return -1;
#endif /* BOX_NOTIFY */
}

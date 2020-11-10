/*  notify.h
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

#ifndef NOTIFY_H
#define NOTIFY_H

#include <stdint.h>
#include "windows.h"

typedef enum _Notification {
    silent = -1,
    notif_error,
    self_log_in,
    self_log_out,
    user_log_in,
    user_log_out,
    call_incoming,
    call_outgoing,
    generic_message,
    transfer_pending,
    transfer_completed,
} Notification;

typedef enum _Flags {
    NT_NOFOCUS = 1 << 0,  /* Notify when focus is not on this terminal. NOTE: only works with x11,
                             * if no x11 present this flag is ignored
                             */
    NT_BEEP = 1 << 1,     /* Play native sound instead: \a */
    NT_LOOP = 1 << 2,       /* Loop sound. If this setting active, notify() will return id of the sound
                             * so it could be stopped. It will return 0 if error or NT_NATIVE flag is set and play \a instead
                             */
    NT_RESTOL = 1 << 3,     /* Respect tolerance. Usually used to stop flood at toxic startup
                             * Only works if login_cooldown is true when calling init_notify()
                             */
    NT_NOTIFWND = 1 << 4,   /* Pop notify window. NOTE: only works(/WILL WORK) if libnotify is present */
    NT_WNDALERT_0 = 1 << 5, /* Alert toxic */
    NT_WNDALERT_1 = 1 << 6, /* Alert toxic */
    NT_WNDALERT_2 = 1 << 7, /* Alert toxic */

    NT_ALWAYS = 1 << 8,     /* Force sound to play */
} Flags;

int init_notify(int login_cooldown, int notification_timeout);
void terminate_notify(void);

/* Kills all notifications for `id`. This must be called before freeing a ToxWindow. */
void kill_notifs(int id);

int sound_notify(ToxWindow *self, Notification notif, uint64_t flags, int *id_indicator);
int sound_notify2(ToxWindow *self, Notification notif, uint64_t flags, int id);

void stop_sound(int id);

int box_notify(ToxWindow *self, Notification notif, uint64_t flags, int *id_indicator, const char *title,
               const char *format, ...);
int box_notify2(ToxWindow *self, Notification notif, uint64_t flags, int id, const char *format, ...);
int box_silent_notify(ToxWindow *self, uint64_t flags, int *id_indicator, const char *title, const char *format, ...);
int box_silent_notify2(ToxWindow *self, uint64_t flags, int id, const char *format, ...);

#ifdef SOUND_NOTIFY
int set_sound(Notification sound, const char *value);
#endif /* SOUND_NOTIFY */

#endif /* NOTIFY_H */

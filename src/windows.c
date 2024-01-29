/*  windows.c
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
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "avatars.h"
#include "chat.h"
#include "conference.h"
#include "file_transfers.h"
#include "friendlist.h"
#include "groupchats.h"
#include "line_info.h"
#include "log.h"
#include "misc_tools.h"
#include "prompt.h"
#include "settings.h"
#include "toxic.h"
#include "windows.h"

#ifdef GAMES
#include "game_base.h"
#endif

ToxWindow *windows[MAX_WINDOWS_NUM];
static uint8_t active_window_index;
static int num_active_windows;

/* CALLBACKS START */
void on_friend_request(Tox *tox, const uint8_t *public_key, const uint8_t *data, size_t length, void *userdata)
{
    UNUSED_VAR(tox);
    Toxic *toxic = (Toxic *) userdata;

    char msg[MAX_STR_SIZE + 1];
    length = copy_tox_str(msg, sizeof(msg), (const char *) data, length);

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onFriendRequest != NULL) {
            windows[i]->onFriendRequest(windows[i], toxic, (const char *) public_key, msg, length);
        }
    }
}

void on_friend_connection_status(Tox *tox, uint32_t friendnumber, Tox_Connection connection_status, void *userdata)
{
    UNUSED_VAR(tox);
    Toxic *toxic = (Toxic *) userdata;

    on_avatar_friend_connection_status(toxic, friendnumber, connection_status);

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onConnectionChange != NULL) {
            windows[i]->onConnectionChange(windows[i], toxic, friendnumber, connection_status);
        }
    }

    flag_interface_refresh();
}

void on_friend_typing(Tox *tox, uint32_t friendnumber, bool is_typing, void *userdata)
{
    UNUSED_VAR(tox);
    Toxic *toxic = (Toxic *) userdata;

    if (toxic->c_config->show_typing_other == SHOW_TYPING_OFF) {
        return;
    }

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onTypingChange != NULL) {
            windows[i]->onTypingChange(windows[i], toxic, friendnumber, is_typing);
        }
    }

    flag_interface_refresh();
}

void on_friend_message(Tox *tox, uint32_t friendnumber, Tox_Message_Type type, const uint8_t *string, size_t length,
                       void *userdata)
{
    UNUSED_VAR(tox);
    Toxic *toxic = (Toxic *) userdata;

    char msg[MAX_STR_SIZE + 1];
    length = copy_tox_str(msg, sizeof(msg), (const char *) string, length);

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onMessage != NULL) {
            windows[i]->onMessage(windows[i], toxic, friendnumber, type, msg, length);
        }
    }
}

void on_friend_name(Tox *tox, uint32_t friendnumber, const uint8_t *string, size_t length, void *userdata)
{
    UNUSED_VAR(tox);

    Toxic *toxic = (Toxic *) userdata;

    char nick[TOXIC_MAX_NAME_LENGTH + 1];
    length = copy_tox_str(nick, sizeof(nick), (const char *) string, length);
    filter_str(nick, length);

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onNickChange != NULL) {
            windows[i]->onNickChange(windows[i], toxic, friendnumber, nick, length);
        }
    }

    flag_interface_refresh();

    store_data(toxic);
}

void on_friend_status_message(Tox *tox, uint32_t friendnumber, const uint8_t *string, size_t length, void *userdata)
{
    UNUSED_VAR(tox);
    UNUSED_VAR(userdata);

    char msg[TOX_MAX_STATUS_MESSAGE_LENGTH + 1];
    length = copy_tox_str(msg, sizeof(msg), (const char *) string, length);
    filter_str(msg, length);

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onStatusMessageChange != NULL) {
            windows[i]->onStatusMessageChange(windows[i], friendnumber, msg, length);
        }
    }

    flag_interface_refresh();
}

void on_friend_status(Tox *tox, uint32_t friendnumber, Tox_User_Status status, void *userdata)
{
    UNUSED_VAR(tox);
    Toxic *toxic = (Toxic *) userdata;

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onStatusChange != NULL) {
            windows[i]->onStatusChange(windows[i], toxic, friendnumber, status);
        }
    }

    flag_interface_refresh();
}

// TODO: This isn't a proper tox callback. Refactor with friendlist.
void on_friend_added(Toxic *toxic, uint32_t friendnumber, bool sort)
{
    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onFriendAdded != NULL) {
            windows[i]->onFriendAdded(windows[i], toxic, friendnumber, sort);
        }
    }

    store_data(toxic);
}

void on_conference_message(Tox *tox, uint32_t conferencenumber, uint32_t peernumber, Tox_Message_Type type,
                           const uint8_t *message, size_t length, void *userdata)
{
    UNUSED_VAR(tox);
    Toxic *toxic = (Toxic *) userdata;

    char msg[MAX_STR_SIZE + 1];
    length = copy_tox_str(msg, sizeof(msg), (const char *) message, length);

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onConferenceMessage != NULL) {
            windows[i]->onConferenceMessage(windows[i], toxic, conferencenumber, peernumber, type, msg, length);
        }
    }
}

void on_conference_invite(Tox *tox, uint32_t friendnumber, Tox_Conference_Type type, const uint8_t *conference_pub_key,
                          size_t length, void *userdata)
{
    UNUSED_VAR(tox);
    Toxic *toxic = (Toxic *) userdata;

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onConferenceInvite != NULL) {
            windows[i]->onConferenceInvite(windows[i], toxic, friendnumber, type, (const char *) conference_pub_key, length);
        }
    }
}

void on_conference_peer_list_changed(Tox *tox, uint32_t conferencenumber, void *userdata)
{
    UNUSED_VAR(tox);
    Toxic *toxic = (Toxic *) userdata;

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onConferenceNameListChange != NULL) {
            windows[i]->onConferenceNameListChange(windows[i], toxic, conferencenumber);
        }
    }

    flag_interface_refresh();
}

void on_conference_peer_name(Tox *tox, uint32_t conferencenumber, uint32_t peernumber, const uint8_t *name,
                             size_t length, void *userdata)
{
    UNUSED_VAR(tox);
    Toxic *toxic = (Toxic *) userdata;

    char nick[TOXIC_MAX_NAME_LENGTH + 1];
    length = copy_tox_str(nick, sizeof(nick), (const char *) name, length);
    filter_str(nick, length);

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onConferencePeerNameChange != NULL) {
            windows[i]->onConferencePeerNameChange(windows[i], toxic, conferencenumber, peernumber, nick, length);
        }
    }
}

void on_conference_title(Tox *tox, uint32_t conferencenumber, uint32_t peernumber, const uint8_t *title, size_t length,
                         void *userdata)
{
    UNUSED_VAR(tox);
    Toxic *toxic = (Toxic *) userdata;

    char data[MAX_STR_SIZE + 1];
    length = copy_tox_str(data, sizeof(data), (const char *) title, length);

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onConferenceTitleChange != NULL) {
            windows[i]->onConferenceTitleChange(windows[i], toxic, conferencenumber, peernumber, data, length);
        }
    }
}

void on_file_chunk_request(Tox *tox, uint32_t friendnumber, uint32_t filenumber, uint64_t position,
                           size_t length, void *userdata)
{
    Toxic *toxic = (Toxic *) userdata;

    FileTransfer *ft = get_file_transfer_struct(friendnumber, filenumber);

    if (ft == NULL) {
        return;
    }

    if (ft->file_type == TOX_FILE_KIND_AVATAR) {
        on_avatar_chunk_request(toxic, ft, position, length);
        return;
    }

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onFileChunkRequest != NULL) {
            windows[i]->onFileChunkRequest(windows[i], toxic, friendnumber, filenumber, position, length);
        }
    }
}

void on_file_recv_chunk(Tox *tox, uint32_t friendnumber, uint32_t filenumber, uint64_t position,
                        const uint8_t *data, size_t length, void *userdata)
{
    UNUSED_VAR(tox);
    Toxic *toxic = (Toxic *) userdata;

    const FileTransfer *ft = get_file_transfer_struct(friendnumber, filenumber);

    if (ft == NULL) {
        return;
    }

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onFileRecvChunk != NULL) {
            windows[i]->onFileRecvChunk(windows[i], toxic, friendnumber, filenumber, position, (const char *) data, length);
        }
    }
}

void on_file_recv_control(Tox *tox, uint32_t friendnumber, uint32_t filenumber, Tox_File_Control control,
                          void *userdata)
{
    Toxic *toxic = (Toxic *) userdata;

    FileTransfer *ft = get_file_transfer_struct(friendnumber, filenumber);

    if (ft == NULL) {
        return;
    }

    if (ft->file_type == TOX_FILE_KIND_AVATAR) {
        on_avatar_file_control(toxic, ft, control);
        return;
    }

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onFileControl != NULL) {
            windows[i]->onFileControl(windows[i], toxic, friendnumber, filenumber, control);
        }
    }
}

void on_file_recv(Tox *tox, uint32_t friendnumber, uint32_t filenumber, uint32_t kind, uint64_t file_size,
                  const uint8_t *filename, size_t filename_length, void *userdata)
{
    Toxic *toxic = (Toxic *) userdata;

    /* We don't care about receiving avatars */
    if (kind != TOX_FILE_KIND_DATA) {
        tox_file_control(tox, friendnumber, filenumber, TOX_FILE_CONTROL_CANCEL, NULL);
        return;
    }

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onFileRecv != NULL) {
            windows[i]->onFileRecv(windows[i], toxic, friendnumber, filenumber, file_size, (const char *) filename,
                                   filename_length);
        }
    }
}

void on_friend_read_receipt(Tox *tox, uint32_t friendnumber, uint32_t receipt, void *userdata)
{
    UNUSED_VAR(tox);
    Toxic *toxic = (Toxic *) userdata;

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onReadReceipt != NULL) {
            windows[i]->onReadReceipt(windows[i], toxic, friendnumber, receipt);
        }
    }
}

void on_lossless_custom_packet(Tox *tox, uint32_t friendnumber, const uint8_t *data, size_t length, void *userdata)
{
    if (length == 0 || data == NULL) {
        return;
    }

    Toxic *toxic = (Toxic *) userdata;

    const uint8_t type = data[0];

    switch (type) {
#ifdef GAMES

        case CUSTOM_PACKET_GAME_INVITE: {
            for (size_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
                ToxWindow *window = windows[i];

                if (window != NULL && window->onGameInvite != NULL) {
                    window->onGameInvite(window, toxic, friendnumber, data + 1, length - 1);
                }
            }

            break;
        }

        case CUSTOM_PACKET_GAME_DATA: {
            for (size_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
                ToxWindow *window = windows[i];

                if (window != NULL && window->onGameData != NULL) {
                    window->onGameData(window, toxic, friendnumber, data + 1, length - 1);
                }
            }

            break;
        }

#else
        UNUSED_VAR(toxic);
#endif // GAMES

        default: {
            fprintf(stderr, "Got unknown custom packet of type: %u\n", type);
            return;
        }
    }
}

void on_group_invite(Tox *tox, uint32_t friendnumber, const uint8_t *invite_data, size_t length,
                     const uint8_t *group_name,
                     size_t group_name_length, void *userdata)
{
    Toxic *toxic = (Toxic *) userdata;

    char gname[MAX_STR_SIZE + 1];
    group_name_length = copy_tox_str(gname, sizeof(gname), (const char *) group_name, group_name_length);

    for (size_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onGroupInvite != NULL) {
            windows[i]->onGroupInvite(windows[i], toxic, friendnumber, (char *) invite_data, length, gname, group_name_length);
        }
    }
}

void on_group_message(Tox *tox, uint32_t groupnumber, uint32_t peer_id, TOX_MESSAGE_TYPE type,
                      const uint8_t *message, size_t length, uint32_t message_id, void *userdata)
{
    UNUSED_VAR(message_id);
    UNUSED_VAR(tox);

    Toxic *toxic = (Toxic *) userdata;

    char msg[MAX_STR_SIZE + 1];
    length = copy_tox_str(msg, sizeof(msg), (const char *) message, length);

    for (size_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onGroupMessage != NULL) {
            windows[i]->onGroupMessage(windows[i], toxic, groupnumber, peer_id, type, msg, length);
        }
    }
}

void on_group_private_message(Tox *tox, uint32_t groupnumber, uint32_t peer_id, TOX_MESSAGE_TYPE type,
                              const uint8_t *message,
                              size_t length, void *userdata)
{
    UNUSED_VAR(tox);

    Toxic *toxic = (Toxic *) userdata;

    char msg[MAX_STR_SIZE + 1];
    length = copy_tox_str(msg, sizeof(msg), (const char *) message, length);

    for (size_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onGroupPrivateMessage != NULL) {
            windows[i]->onGroupPrivateMessage(windows[i], toxic, groupnumber, peer_id, msg, length);
        }
    }
}

void on_group_status_change(Tox *tox, uint32_t groupnumber, uint32_t peer_id, TOX_USER_STATUS status, void *userdata)
{
    UNUSED_VAR(tox);

    Toxic *toxic = (Toxic *) userdata;

    for (size_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onGroupStatusChange != NULL) {
            windows[i]->onGroupStatusChange(windows[i], toxic, groupnumber, peer_id, status);
        }
    }

    flag_interface_refresh();
}

void on_group_peer_join(Tox *tox, uint32_t groupnumber, uint32_t peer_id, void *userdata)
{
    UNUSED_VAR(tox);

    Toxic *toxic = (Toxic *) userdata;

    for (size_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onGroupPeerJoin != NULL) {
            windows[i]->onGroupPeerJoin(windows[i], toxic, groupnumber, peer_id);
        }
    }

    flag_interface_refresh();
}

void on_group_peer_exit(Tox *tox, uint32_t groupnumber, uint32_t peer_id, Tox_Group_Exit_Type exit_type,
                        const uint8_t *nick,
                        size_t nick_len, const uint8_t *part_message, size_t length, void *userdata)
{
    UNUSED_VAR(tox);

    Toxic *toxic = (Toxic *) userdata;

    char toxic_nick[TOXIC_MAX_NAME_LENGTH + 1];
    nick_len = copy_tox_str(toxic_nick, sizeof(toxic_nick), (const char *) nick, nick_len);

    char buf[MAX_STR_SIZE + 1] = {0};
    size_t buf_len = 0;

    if (part_message) {
        buf_len = copy_tox_str(buf, sizeof(buf), (const char *) part_message, length);
    }

    for (size_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onGroupPeerExit != NULL) {
            windows[i]->onGroupPeerExit(windows[i], toxic, groupnumber, peer_id, exit_type, toxic_nick, nick_len, buf, buf_len);
        }
    }
}

void on_group_topic_change(Tox *tox, uint32_t groupnumber, uint32_t peer_id, const uint8_t *topic, size_t length,
                           void *userdata)
{
    UNUSED_VAR(tox);

    Toxic *toxic = (Toxic *) userdata;

    char data[MAX_STR_SIZE + 1];
    length = copy_tox_str(data, sizeof(data), (const char *) topic, length);

    for (size_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onGroupTopicChange != NULL) {
            windows[i]->onGroupTopicChange(windows[i], toxic, groupnumber, peer_id, data, length);
        }
    }
}

void on_group_peer_limit(Tox *tox, uint32_t groupnumber, uint32_t peer_limit, void *userdata)
{
    UNUSED_VAR(tox);

    Toxic *toxic = (Toxic *) userdata;

    for (size_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onGroupPeerLimit != NULL) {
            windows[i]->onGroupPeerLimit(windows[i], toxic, groupnumber, peer_limit);
        }
    }
}

void on_group_privacy_state(Tox *tox, uint32_t groupnumber, Tox_Group_Privacy_State privacy_state, void *userdata)
{
    UNUSED_VAR(tox);

    Toxic *toxic = (Toxic *) userdata;

    for (size_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onGroupPrivacyState != NULL) {
            windows[i]->onGroupPrivacyState(windows[i], toxic, groupnumber, privacy_state);
        }
    }
}

void on_group_topic_lock(Tox *tox, uint32_t groupnumber, Tox_Group_Topic_Lock topic_lock, void *userdata)
{
    UNUSED_VAR(tox);

    Toxic *toxic = (Toxic *) userdata;

    for (size_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onGroupTopicLock != NULL) {
            windows[i]->onGroupTopicLock(windows[i], toxic, groupnumber, topic_lock);
        }
    }
}

void on_group_password(Tox *tox, uint32_t groupnumber, const uint8_t *password, size_t length, void *userdata)
{
    UNUSED_VAR(tox);

    Toxic *toxic = (Toxic *) userdata;

    for (size_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onGroupPassword != NULL) {
            windows[i]->onGroupPassword(windows[i], toxic, groupnumber, (char *) password, length);
        }
    }
}

void on_group_nick_change(Tox *tox, uint32_t groupnumber, uint32_t peer_id, const uint8_t *newname, size_t length,
                          void *userdata)
{
    UNUSED_VAR(tox);

    Toxic *toxic = (Toxic *) userdata;

    char name[TOXIC_MAX_NAME_LENGTH + 1];
    length = copy_tox_str(name, sizeof(name), (const char *) newname, length);
    filter_str(name, length);

    for (size_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onGroupNickChange != NULL) {
            windows[i]->onGroupNickChange(windows[i], toxic, groupnumber, peer_id, name, length);
        }
    }
}

void on_group_self_join(Tox *tox, uint32_t groupnumber, void *userdata)
{
    UNUSED_VAR(tox);

    Toxic *toxic = (Toxic *) userdata;

    for (size_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onGroupSelfJoin != NULL) {
            windows[i]->onGroupSelfJoin(windows[i], toxic, groupnumber);
        }
    }
}

void on_group_rejected(Tox *tox, uint32_t groupnumber, Tox_Group_Join_Fail type, void *userdata)
{
    UNUSED_VAR(tox);

    Toxic *toxic = (Toxic *) userdata;

    for (size_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onGroupRejected != NULL) {
            windows[i]->onGroupRejected(windows[i], toxic, groupnumber, type);
        }
    }
}

void on_group_moderation(Tox *tox, uint32_t groupnumber, uint32_t source_peer_id, uint32_t target_peer_id,
                         Tox_Group_Mod_Event type, void *userdata)
{
    UNUSED_VAR(tox);

    Toxic *toxic = (Toxic *) userdata;

    for (size_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onGroupModeration != NULL) {
            windows[i]->onGroupModeration(windows[i], toxic, groupnumber, source_peer_id, target_peer_id, type);
        }
    }
}

void on_group_voice_state(Tox *tox, uint32_t groupnumber, Tox_Group_Voice_State voice_state, void *userdata)
{
    UNUSED_VAR(tox);

    Toxic *toxic = (Toxic *) userdata;

    for (size_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] != NULL && windows[i]->onGroupVoiceState != NULL) {
            windows[i]->onGroupVoiceState(windows[i], toxic, groupnumber, voice_state);
        }
    }
}

/* CALLBACKS END */

int add_window(Toxic *toxic, ToxWindow *w)
{
    if (LINES < 2) {
        return -1;
    }

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; i++) {
        if (windows[i] != NULL) {
            continue;
        }

        w->index = i;
        w->window = newwin(LINES, COLS, 0, 0);
        w->colour = BAR_TEXT;

        if (w->window == NULL) {
            return -1;
        }

#ifdef URXVT_FIX
        /* Fixes text color problem on some terminals. */
        wbkgd(w->window, COLOR_PAIR(6));
#endif
        windows[i] = w;

        if (w->onInit) {
            w->onInit(w, toxic);
        }

        ++num_active_windows;

        return i;
    }

    return -1;
}

void set_active_window_index(uint8_t index)
{
    if (index < MAX_WINDOWS_NUM) {
        active_window_index = index;
    }
}

/* Displays the next window if `ch` is equal to the next window key binding.
 * Otherwise displays the previous window.
 */
static void set_next_window(const Client_Config *c_config, int ch)
{
    uint8_t index = 0;

    if (ch == c_config->key_next_tab) {
        for (uint8_t i = active_window_index + 1; i < MAX_WINDOWS_NUM; ++i) {
            if (windows[i] != NULL) {
                index = i;
                break;
            }
        }
    } else {
        uint8_t start = active_window_index == 0 ? MAX_WINDOWS_NUM - 1 : active_window_index - 1;

        for (uint8_t i = start; i > 0; --i) {
            if (windows[i] != NULL) {
                index = i;
                break;
            }
        }
    }

    set_active_window_index(index);
}

/* Deletes window w and cleans up */
void del_window(ToxWindow *w, const Client_Config *c_config)
{
    const uint8_t idx = w->index;
    delwin(w->window_bar);
    delwin(w->window);
    free(w);
    windows[idx] = NULL;

    clear();
    refresh();

    if (num_active_windows > 0) {
        if (active_window_index == 2) { // if closing current window would bring us to friend list
            set_next_window(c_config, -1);  // skip back to the home window instead. FIXME: magic numbers
        }

        set_next_window(c_config, -1);

        --num_active_windows;
    }
}

ToxWindow *init_windows(Toxic *toxic)
{
    if (COLS <= CHATBOX_HEIGHT + WINDOW_BAR_HEIGHT) {
        exit_toxic_err("add_window() for prompt failed in init_windows", FATALERR_WININIT);
    }

    prompt = new_prompt();

    int n_prompt = add_window(toxic, prompt);

    if (n_prompt < 0) {
        exit_toxic_err("add_window() for prompt failed in init_windows", FATALERR_WININIT);
    }

    if (add_window(toxic, new_friendlist()) == -1) {
        exit_toxic_err("add_window() for friendlist failed in init_windows", FATALERR_WININIT);
    }

    set_active_window_index(n_prompt);

    return prompt;
}

void on_window_resize(void)
{
    endwin();
    refresh();
    clear();

    int x2;
    int y2;

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        ToxWindow *w = windows[i];

        if (w == NULL) {
            continue;
        }

        if (w->type == WINDOW_TYPE_FRIEND_LIST)  {
            delwin(w->window_bar);
            delwin(w->window);
            w->window = newwin(LINES, COLS, 0, 0);
            w->window_bar = subwin(w->window, WINDOW_BAR_HEIGHT, COLS, LINES - 2, 0);
            continue;
        }

#ifdef GAMES

        if (w->type == WINDOW_TYPE_GAME) {
            delwin(w->window_bar);
            delwin(w->window);
            delwin(w->game->window);
            w->window = newwin(LINES, COLS, 0, 0);

            getmaxyx(w->window, y2, x2);

            if (y2 <= 0 || x2 <= 0) {
                fprintf(stderr, "Failed to resize game window: max_x: %d, max_y: %d\n", x2, y2);
                delwin(w->window);
                continue;
            }

            w->window_bar = subwin(w->window, WINDOW_BAR_HEIGHT, COLS, LINES - 2, 0);
            w->game->window = subwin(w->window, y2 - CHATBOX_HEIGHT - WINDOW_BAR_HEIGHT, x2, 0, 0);
            continue;
        }

#endif // GAMES

        if (w->help->active) {
            wclear(w->help->win);
        }

        if (w->type == WINDOW_TYPE_CONFERENCE || w->type == WINDOW_TYPE_GROUPCHAT) {
            delwin(w->chatwin->sidebar);
            w->chatwin->sidebar = NULL;
        } else {
            delwin(w->stb->topline);
        }

        delwin(w->chatwin->linewin);
        delwin(w->chatwin->history);
        delwin(w->window_bar);
        delwin(w->window);

        w->window = newwin(LINES, COLS, 0, 0);

        getmaxyx(w->window, y2, x2);

        if (y2 <= 0 || x2 <= 0) {
            fprintf(stderr, "Failed to resize window: max_x: %d, max_y: %d\n", x2, y2);
            delwin(w->window);
            continue;
        }

        if (w->show_peerlist) {
            w->chatwin->history = subwin(w->window, y2 - CHATBOX_HEIGHT - WINDOW_BAR_HEIGHT, x2 - SIDEBAR_WIDTH - 1, 0, 0);
            w->chatwin->sidebar = subwin(w->window, y2 - CHATBOX_HEIGHT - WINDOW_BAR_HEIGHT, SIDEBAR_WIDTH, 0, x2 - SIDEBAR_WIDTH);
        } else {
            w->chatwin->history =  subwin(w->window, y2 - CHATBOX_HEIGHT - WINDOW_BAR_HEIGHT, x2, 0, 0);

            if (!(w->type == WINDOW_TYPE_CONFERENCE || w->type == WINDOW_TYPE_GROUPCHAT)) {
                w->stb->topline = subwin(w->window, TOP_BAR_HEIGHT, x2, 0, 0);
            }
        }

        w->window_bar = subwin(w->window, WINDOW_BAR_HEIGHT, x2, y2 - (CHATBOX_HEIGHT + WINDOW_BAR_HEIGHT), 0);
        w->chatwin->linewin = subwin(w->window, CHATBOX_HEIGHT, x2, y2 - WINDOW_BAR_HEIGHT, 0);

#ifdef AUDIO

        if (w->chatwin->infobox.active) {
            delwin(w->chatwin->infobox.win);
            w->chatwin->infobox.win = newwin(INFOBOX_HEIGHT, INFOBOX_WIDTH + 1, 1, x2 - INFOBOX_WIDTH);
        }

#endif /* AUDIO */

        scrollok(w->chatwin->history, 0);
        wmove(w->window, y2 - CURS_Y_OFFSET, 0);
    }
}

static void draw_window_tab(WINDOW *win, ToxWindow *toxwin, bool active_window)
{
    pthread_mutex_lock(&Winthread.lock);

    bool has_alert = toxwin->alert != WINDOW_ALERT_NONE;
    unsigned int pending_messages = toxwin->pending_messages;

    pthread_mutex_unlock(&Winthread.lock);

    Window_Type type = toxwin->type;

    if (active_window) {
        wattron(win, A_BOLD | COLOR_PAIR(BAR_ACCENT));
        wprintw(win, " [");
        wattroff(win, COLOR_PAIR(BAR_ACCENT));
        wattron(win, COLOR_PAIR(BAR_TEXT));
    } else {
        if (has_alert) {
            wattron(win, COLOR_PAIR(BAR_ACCENT));
            wprintw(win, " [");
            wattroff(win, COLOR_PAIR(BAR_ACCENT));
            wattron(win, A_BOLD | COLOR_PAIR(toxwin->alert));
        } else {
            wattron(win, COLOR_PAIR(BAR_ACCENT));
            wprintw(win, " [");
            wattroff(win, COLOR_PAIR(BAR_ACCENT));
            wattron(win, COLOR_PAIR(BAR_TEXT));
        }
    }

    if (type == WINDOW_TYPE_PROMPT || type == WINDOW_TYPE_FRIEND_LIST) {
        if (!has_alert) {
            wattron(win, COLOR_PAIR(toxwin->colour));
            wprintw(win, "%s", toxwin->name);
            wattroff(win, COLOR_PAIR(toxwin->colour));
        } else {
            wprintw(win, "%s", toxwin->name);
        }
    } else if (active_window) {
        wattron(win, COLOR_PAIR(toxwin->colour));
        wprintw(win, "%s", toxwin->name);
        wattroff(win, COLOR_PAIR(toxwin->colour));
    } else {
        if (pending_messages > 0) {
            wprintw(win, "%u", pending_messages);
        } else {
            wprintw(win, "-");
        }
    }

    if (active_window) {
        wattroff(win, COLOR_PAIR(BAR_TEXT));
        wattron(win, COLOR_PAIR(BAR_ACCENT));
        wprintw(win, "]");
        wattroff(win, A_BOLD | COLOR_PAIR(BAR_ACCENT));
    } else {
        if (has_alert) {
            wattroff(win, A_BOLD | COLOR_PAIR(toxwin->alert));
            wattron(win, COLOR_PAIR(BAR_ACCENT));
            wprintw(win, "]");
            wattroff(win, COLOR_PAIR(BAR_ACCENT));
        } else {
            wattroff(win, COLOR_PAIR(BAR_TEXT));
            wattron(win, COLOR_PAIR(BAR_ACCENT));
            wprintw(win, "]");
            wattroff(win, COLOR_PAIR(BAR_ACCENT));
        }
    }
}

void draw_window_bar(ToxWindow *self)
{
    WINDOW *win = self->window_bar;
    wclear(win);

    if (self->scroll_pause) {
        wattron(win, A_BLINK | A_BOLD | COLOR_PAIR(BAR_NOTIFY));
        wprintw(win, "^");
        wattroff(win, A_BLINK | A_BOLD | COLOR_PAIR(BAR_NOTIFY));
    } else {
        wattron(win, COLOR_PAIR(BAR_TEXT));
        wprintw(win, " ");
        wattroff(win, COLOR_PAIR(BAR_TEXT));
    }

    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        if (windows[i] == NULL) {
            continue;
        }

        bool active_window = i == active_window_index;
        draw_window_tab(win, windows[i], active_window);
    }

    int cur_x;
    int cur_y;
    getyx(win, cur_y, cur_x);

    UNUSED_VAR(cur_y);

    wattron(win, COLOR_PAIR(BAR_TEXT));
    mvwhline(win, 0, cur_x, ' ', COLS - cur_x);
    wattroff(win, COLOR_PAIR(BAR_TEXT));
}

/*
 * Gets current char from stdscr and puts it in ch.
 *
 * Return 1 if char is printable.
 * Return 0 if char is not printable.
 * Return -1 on error.
 */
static int get_current_char(wint_t *ch)
{
    wint_t tmpchar = 0;
    bool is_printable = false;

#ifdef HAVE_WIDECHAR
    int status = wget_wch(stdscr, &tmpchar);

    if (status == ERR) {
        return -1;
    }

    if (status == OK) {
        is_printable = iswprint(tmpchar);
    }

#else
    tmpchar = getch();

    if (tmpchar == ERR) {
        return -1;
    }

    is_printable = isprint(tmpchar);
#endif /* HAVE_WIDECHAR */

    *ch = tmpchar;

    return (int) is_printable;
}

static struct key_sequence_codes {
    wchar_t *code;
    wint_t   key;
} Keys[] = {
    { L"[1;5A", T_KEY_C_UP    },
    { L"[1;5B", T_KEY_C_DOWN  },
    { L"[1;5C", T_KEY_C_RIGHT },
    { L"[1;5D", T_KEY_C_LEFT  },
    { NULL, 0 }
};

/*
 * Return key code corresponding to character sequence queued in stdscr.
 * Return -1 if sequence is unknown.
 */
#define MAX_SEQUENCE_SIZE 5
static wint_t get_input_sequence_code(void)
{
    wchar_t code[MAX_SEQUENCE_SIZE + 1];

    size_t length = 0;
    wint_t ch = 0;

    for (size_t i = 0; i < MAX_SEQUENCE_SIZE; ++i) {
        int res = get_current_char(&ch);

        if (res < 0) {
            break;
        }

        ++length;
        code[i] = (wchar_t) ch;
    }

    if (length == 0) {
        return -1;
    }

    code[length] = L'\0';

    for (size_t i = 0; Keys[i].key != 0; ++i) {
        if (wcscmp(code, Keys[i].code) == 0) {
            return Keys[i].key;
        }
    }

    return -1;
}

void draw_active_window(Toxic *toxic)
{
    if (toxic == NULL) {
        return;
    }

    const Client_Config *c_config = toxic->c_config;
    ToxWindow *a = windows[active_window_index];

    if (a == NULL) {
        return;
    }

    pthread_mutex_lock(&Winthread.lock);
    a->alert = WINDOW_ALERT_NONE;
    a->pending_messages = 0;
    const bool flag_refresh = Winthread.flag_refresh;
    pthread_mutex_unlock(&Winthread.lock);

    if (flag_refresh) {
        touchwin(a->window);
        a->onDraw(a, toxic);
        wrefresh(a->window);
    }

#ifdef AUDIO
    else if (a->is_call && timed_out(a->chatwin->infobox.lastupdate, 1)) {
        touchwin(a->window);
        a->onDraw(a, toxic);
        wrefresh(a->window);
    }

#endif  // AUDIO

#ifdef GAMES

    if (a->type == WINDOW_TYPE_GAME) {
        if (!flag_refresh) {  // we always want to be continously refreshing game windows
            touchwin(a->window);
            a->onDraw(a, toxic);
            wrefresh(a->window);
        }

        int ch = getch();

        if (ch == ERR) {
            return;
        }

        pthread_mutex_lock(&Winthread.lock);
        flag_interface_refresh();
        pthread_mutex_unlock(&Winthread.lock);

        if (ch == c_config->key_next_tab || ch == c_config->key_prev_tab) {
            set_next_window(c_config, ch);
        }

        a->onKey(a, toxic, ch, false);  // we lock only when necessary in the onKey callback

        return;
    }

#endif // GAMES

    wint_t ch = 0;
    int printable = get_current_char(&ch);

    if (printable < 0) {
        return;
    }

    pthread_mutex_lock(&Winthread.lock);
    flag_interface_refresh();
    pthread_mutex_unlock(&Winthread.lock);

    if (printable == 0 && (ch == c_config->key_next_tab || ch == c_config->key_prev_tab)) {
        set_next_window(c_config, (int) ch);
        return;
    } else if ((printable == 0) && (a->type != WINDOW_TYPE_FRIEND_LIST)) {
        pthread_mutex_lock(&Winthread.lock);
        bool input_ret = a->onKey(a, toxic, ch, (bool) printable);
        pthread_mutex_unlock(&Winthread.lock);

        if (input_ret) {
            return;
        }

        // if an unprintable key code is unrecognized by input handler we attempt to manually decode char sequence
        wint_t tmp = get_input_sequence_code();

        if (tmp != (wint_t) -1) {
            ch = tmp;
        }
    }

    pthread_mutex_lock(&Winthread.lock);
    a->onKey(a, toxic, ch, (bool) printable);
    pthread_mutex_unlock(&Winthread.lock);
}

/* Refresh inactive windows to prevent scrolling bugs.
 * Call at least once per second.
 */
void refresh_inactive_windows(const Client_Config *c_config)
{
    for (uint8_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        ToxWindow *toxwin = windows[i];

        if (toxwin == NULL) {
            continue;
        }

        if ((i != active_window_index) && (toxwin->type != WINDOW_TYPE_FRIEND_LIST)) {
            pthread_mutex_lock(&Winthread.lock);
            line_info_print(toxwin, c_config);
            pthread_mutex_unlock(&Winthread.lock);
        }
    }
}

/* Returns a pointer to the ToxWindow in the ith index.
 * Returns NULL if no ToxWindow exists.
 */
ToxWindow *get_window_ptr(size_t index)
{
    if (index >= MAX_WINDOWS_NUM) {
        return NULL;
    }

    return windows[index];
}

/* Returns a pointer to the currently active ToxWindow. */
ToxWindow *get_active_window(void)
{
    return windows[active_window_index];
}

void force_refresh(WINDOW *w)
{
    wclear(w);
    endwin();
    refresh();
}

int get_num_active_windows(void)
{
    return num_active_windows;
}

/* Returns the number of active windows of given type. */
size_t get_num_active_windows_type(Window_Type type)
{
    size_t count = 0;

    for (size_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        ToxWindow *w = windows[i];

        if (w == NULL) {
            continue;
        }

        if (w->type == type) {
            ++count;
        }
    }

    return count;
}

ToxWindow *get_window_by_number_type(uint32_t number, Window_Type type)
{
    for (size_t i = 0; i < MAX_WINDOWS_NUM; ++i) {
        ToxWindow *win = windows[i];

        if (win->type == type && win->num == number) {
            return win;
        }
    }

    return NULL;
}

bool disable_window_log_by_number_type(uint32_t number, Window_Type type)
{
    ToxWindow *win = get_window_by_number_type(number, type);

    if (win == NULL) {
        return false;
    }

    ChatContext *ctx = win->chatwin;

    if (ctx == NULL) {
        return false;
    }

    log_disable(ctx->log);

    return true;
}

bool enable_window_log_by_number_type(uint32_t number, Window_Type type)
{
    ToxWindow *win = get_window_by_number_type(number, type);

    if (win == NULL) {
        return false;
    }

    ChatContext *ctx = win->chatwin;

    if (ctx == NULL) {
        return false;
    }

    return log_enable(ctx->log) == 0;
}

/* destroys all chat and conference windows (should only be called on shutdown) */
void kill_all_windows(Toxic *toxic)
{
    const Client_Config *c_config = toxic->c_config;

    for (size_t i = 2; i < MAX_WINDOWS_NUM; ++i) {
        ToxWindow *w = windows[i];

        if (w == NULL) {
            continue;
        }

        switch (w->type) {
            case WINDOW_TYPE_CHAT: {
                kill_chat_window(w, toxic->tox);
                break;
            }

            case WINDOW_TYPE_CONFERENCE: {
                free_conference(w, c_config, w->num);
                break;
            }

#ifdef GAMES

            case WINDOW_TYPE_GAME: {
                game_kill(w, c_config);
                break;
            }

#endif // GAMES

            case WINDOW_TYPE_GROUPCHAT: {
                exit_groupchat(w, toxic, w->num, c_config->group_part_message,
                               strlen(c_config->group_part_message));
                break;
            }

            default: {
                break;
            }
        }
    }

    /* TODO: use enum instead of magic indices */
    kill_friendlist(windows[1], c_config);
    kill_prompt_window(windows[0], c_config);
}

/*  groupchat.h
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

#define SIDEBAR_WIDTH 16
#define SDBAR_OFST 2    /* Offset for the peer number box at the top of the statusbar */

typedef struct {
    int chatwin;
    bool active;
    int num_peers;
    int side_pos;    /* current position of the sidebar - used for scrolling up and down */
    uint8_t *peer_names;
    uint8_t *oldpeer_names;
} GroupChat;

int init_groupchat_win(ToxWindow *prompt, Tox *m, int groupnum);
ToxWindow new_group_chat(Tox *m, int groupnum);

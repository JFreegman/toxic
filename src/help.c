/*  help.c
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

#include <string.h>

#include "help.h"
#include "misc_tools.h"
#include "toxic.h"
#include "windows.h"

#ifdef PYTHON
#include "api.h"
#endif /* PYTHON */

#ifdef PYTHON
#define HELP_MENU_HEIGHT 10
#else
#define HELP_MENU_HEIGHT 9
#endif /* PYTHON */
#define HELP_MENU_WIDTH 26

void help_init_menu(ToxWindow *self)
{
    if (self->help->win) {
        delwin(self->help->win);
    }

    int y2, x2;
    getmaxyx(self->window, y2, x2);

    if (y2 < HELP_MENU_HEIGHT || x2 < HELP_MENU_WIDTH) {
        return;
    }

    self->help->win = newwin(HELP_MENU_HEIGHT, HELP_MENU_WIDTH, 3, 3);
    self->help->active = true;
    self->help->type = HELP_MENU;
}

static void help_exit(ToxWindow *self)
{
    delwin(self->help->win);

    *(self->help) = (struct Help) {
        0
    };
}

static void help_init_window(ToxWindow *self, int height, int width)
{
    if (self->help->win) {
        delwin(self->help->win);
    }

    int y2, x2;
    getmaxyx(stdscr, y2, x2);

    if (y2 <= 0 || x2 <= 0) {
        return;
    }

    height = MIN(height, y2);
    width = MIN(width, x2);

    self->help->win = newwin(height, width, 0, 0);
}

static void help_draw_menu(ToxWindow *self)
{
    WINDOW *win = self->help->win;

    wmove(win, 1, 1);

    wattron(win, A_BOLD | COLOR_PAIR(RED));
    wprintw(win, "       Help Menu\n");
    wattroff(win, A_BOLD | COLOR_PAIR(RED));

    wattron(win, A_BOLD | COLOR_PAIR(BLUE));
    wprintw(win, " g");
    wattroff(win, A_BOLD | COLOR_PAIR(BLUE));
    wprintw(win, "lobal commands\n");

    wattron(win, A_BOLD | COLOR_PAIR(BLUE));
    wprintw(win, " c");
    wattroff(win, A_BOLD | COLOR_PAIR(BLUE));
    wprintw(win, "hat commands\n");

    wprintw(win, " c");
    wattron(win, A_BOLD | COLOR_PAIR(BLUE));
    wprintw(win, "o");
    wattroff(win, A_BOLD | COLOR_PAIR(BLUE));
    wprintw(win, "nference commands\n");

#ifdef PYTHON
    wattron(win, A_BOLD | COLOR_PAIR(BLUE));
    wprintw(win, " p");
    wattroff(win, A_BOLD | COLOR_PAIR(BLUE));
    wprintw(win, "lugin commands\n");
#endif /* PYTHON */

    wattron(win, A_BOLD | COLOR_PAIR(BLUE));
    wprintw(win, " f");
    wattroff(win, A_BOLD | COLOR_PAIR(BLUE));
    wprintw(win, "riendlist controls\n");

    wattron(win, A_BOLD | COLOR_PAIR(BLUE));
    wprintw(win, " k");
    wattroff(win, A_BOLD | COLOR_PAIR(BLUE));
    wprintw(win, "ey bindings\n");

    wprintw(win, " e");
    wattron(win, A_BOLD | COLOR_PAIR(BLUE));
    wprintw(win, "x");
    wattroff(win, A_BOLD | COLOR_PAIR(BLUE));
    wprintw(win, "it menu\n");

    box(win, ACS_VLINE, ACS_HLINE);
    wnoutrefresh(win);
}

static void help_draw_bottom_menu(WINDOW *win)
{
    int y2, x2;
    getmaxyx(win, y2, x2);

    UNUSED_VAR(x2);

    wmove(win, y2 - 2, 1);

    wattron(win, A_BOLD | COLOR_PAIR(BLUE));
    wprintw(win, " m");
    wattroff(win, A_BOLD | COLOR_PAIR(BLUE));
    wprintw(win, "ain menu |");

    wprintw(win, " e");
    wattron(win, A_BOLD | COLOR_PAIR(BLUE));
    wprintw(win, "x");
    wattroff(win, A_BOLD | COLOR_PAIR(BLUE));
    wprintw(win, "it");
}

static void help_draw_global(ToxWindow *self)
{
    WINDOW *win = self->help->win;

    wmove(win, 1, 1);

    wattron(win, A_BOLD | COLOR_PAIR(RED));
    wprintw(win, "Global Commands:\n");
    wattroff(win, A_BOLD | COLOR_PAIR(RED));

    wprintw(win, "  /add <addr> <msg>          : Add contact with optional message\n");
    wprintw(win, "  /accept <id>               : Accept friend request\n");
    wprintw(win, "  /avatar <path>             : Set an avatar (leave path empty to unset)\n");
    wprintw(win, "  /decline <id>              : Decline friend request\n");
    wprintw(win, "  /requests                  : List pending friend requests\n");
    wprintw(win, "  /connect <ip> <port> <key> : Manually connect to a DHT node\n");
    wprintw(win, "  /status <type> <msg>       : Set status with optional note\n");
    wprintw(win, "  /note <msg>                : Set a personal note\n");
    wprintw(win, "  /nick <nick>               : Set your nickname\n");
    wprintw(win, "  /nospam <value>            : Change part of your Tox ID to stop spam\n");
    wprintw(win, "  /log <on> or <off>         : Enable/disable logging\n");
    wprintw(win, "  /conference <type>         : Create a conference where type: text | audio\n");
    wprintw(win, "  /myid                      : Print your Tox ID\n");
#ifdef GAMES
    wprintw(win, "  /game                      : Play a game\n");
#endif /* GAMES */

#ifdef QRCODE
#ifdef QRPNG
    wprintw(win, "  /myqr <txt> or <png>       : Print your Tox ID's QR code to a file.\n");
#else
    wprintw(win, "  /myqr                      : Print your Tox ID's QR code to a file.\n");
#endif /* QRPNG */
#endif /* QRCODE */
    wprintw(win, "  /clear                     : Clear window history\n");
    wprintw(win, "  /close                     : Close the current chat window\n");
    wprintw(win, "  /quit or /exit             : Exit Toxic\n");

#ifdef AUDIO
    wattron(win, A_BOLD);
    wprintw(win, "\n Audio:\n");
    wattroff(win, A_BOLD);

    wprintw(win, "  /lsdev <type>              : List devices where type: in|out\n");
    wprintw(win, "  /sdev <type> <id>          : Set active device\n");
#endif /* AUDIO */

#ifdef VIDEO
    wattron(win, A_BOLD);
    wprintw(win, "\n Video:\n");
    wattroff(win, A_BOLD);

    wprintw(win, "  /lsvdev <type>             : List video devices where type: in|out\n");
    wprintw(win, "  /svdev <type> <id>         : Set active video device\n");
#endif /* VIDEO */

#ifdef PYTHON
    wattron(win, A_BOLD);
    wprintw(win, "\n Scripting:\n");
    wattroff(win, A_BOLD);

    wprintw(win, "  /run <path>                : Load and run the script at path\n");
#endif /* PYTHON */

    help_draw_bottom_menu(win);

    box(win, ACS_VLINE, ACS_HLINE);
    wnoutrefresh(win);
}

static void help_draw_chat(ToxWindow *self)
{
    WINDOW *win = self->help->win;

    wmove(win, 1, 1);

    wattron(win, A_BOLD | COLOR_PAIR(RED));
    wprintw(win, "Chat Commands:\n");
    wattroff(win, A_BOLD | COLOR_PAIR(RED));

    wprintw(win, "  /invite <n>                : Invite contact to a conference \n");
    wprintw(win, "  /join                      : Join a pending conference\n");
    wprintw(win, "  /sendfile <path>           : Send a file\n");
    wprintw(win, "  /savefile <id>             : Receive a file\n");
    wprintw(win, "  /cancel <type> <id>        : Cancel file transfer where type: in|out\n");

#ifdef AUDIO
    wattron(win, A_BOLD);
    wprintw(win, "\n Audio:\n");
    wattroff(win, A_BOLD);

    wprintw(win, "  /call                      : Audio call\n");
    wprintw(win, "  /answer                    : Answer incoming call\n");
    wprintw(win, "  /reject                    : Reject incoming call\n");
    wprintw(win, "  /hangup                    : Hangup active call\n");
    wprintw(win, "  /sdev <type> <id>          : Change active device\n");
    wprintw(win, "  /mute <type>               : Mute active device if in call\n");
    wprintw(win, "  /sense <n>                 : VAD sensitivity threshold\n");
    wprintw(win, "  /bitrate <n>               : Set the audio encoding bitrate\n");
#endif /* AUDIO */

#ifdef VIDEO
    wattron(win, A_BOLD);
    wprintw(win, "\n Video:\n");
    wattroff(win, A_BOLD);
    wprintw(win, "  /res <width> <height>      : Set video resolution\n");
    wprintw(win, "  /vcall                     : Video call\n");
    wprintw(win, "  /video                     : Toggle video in call\n");
#endif /* VIDEO */

#ifdef GAMES
    wprintw(win, "  /game                      : Play a game with contact\n");
#endif /* GAMES */

    help_draw_bottom_menu(win);

    box(win, ACS_VLINE, ACS_HLINE);
    wnoutrefresh(win);
}

static void help_draw_keys(ToxWindow *self)
{
    WINDOW *win = self->help->win;

    wmove(win, 1, 1);

    wattron(win, A_BOLD | COLOR_PAIR(RED));
    wprintw(win, "Key bindings:\n");
    wattroff(win, A_BOLD | COLOR_PAIR(RED));

    wprintw(win, "  Ctrl+O and Ctrl+P         : Navigate through the tabs\n");
    wprintw(win, "  Page Up and Page Down     : Scroll window history one line\n");
    wprintw(win, "  Ctrl+F and Ctrl+V         : Scroll window history half a page\n");
    wprintw(win, "  Ctrl+H                    : Move to the bottom of window history\n");
    wprintw(win, "  Ctrl+up and Ctrl+down     : Scroll peer list in conference\n");
    wprintw(win, "  Ctrl+B                    : Toggle the conference peerlist\n");
    wprintw(win, "  Ctrl+J                    : Insert new line\n");
    wprintw(win, "  Ctrl+T                    : Toggle paste mode\n\n");
    wprintw(win, "  (Note: Custom keybindings override these defaults.)\n\n");

    help_draw_bottom_menu(win);

    box(win, ACS_VLINE, ACS_HLINE);
    wnoutrefresh(win);
}

static void help_draw_conference(ToxWindow *self)
{
    WINDOW *win = self->help->win;

    wmove(win, 1, 1);

    wattron(win, A_BOLD | COLOR_PAIR(RED));
    wprintw(win, "Conference commands:\n");
    wattroff(win, A_BOLD | COLOR_PAIR(RED));

    wprintw(win, "  /title <msg>               : Show/set conference title\n");
#ifdef AUDIO
    wattron(win, A_BOLD);
    wprintw(win, "\n Audio:\n");
    wattroff(win, A_BOLD);
    wprintw(win, "  /audio <on> or <off>       : Enable/disable audio in an audio conference\n");
    wprintw(win, "  /mute                      : Toggle self audio mute status\n");
    wprintw(win, "  /mute <nick> or <pubkey>   : Toggle peer audio mute status\n");
    wprintw(win, "  /ptt <on> or <off>         : Toggle audio input Push-To-Talk (F2 to activate)\n");
    wprintw(win, "  /sense <n>                 : VAD sensitivity threshold\n\n");
#endif

    help_draw_bottom_menu(win);

    box(win, ACS_VLINE, ACS_HLINE);
    wnoutrefresh(win);
}

#ifdef PYTHON
static void help_draw_plugin(ToxWindow *self)
{
    WINDOW *win = self->help->win;

    wmove(win, 1, 1);

    wattron(win, A_BOLD | COLOR_PAIR(RED));
    wprintw(win, "Plugin commands:\n");
    wattroff(win, A_BOLD | COLOR_PAIR(RED));

    draw_handler_help(win);

    help_draw_bottom_menu(win);

    box(win, ACS_VLINE, ACS_HLINE);
    wnoutrefresh(win);
}
#endif /* PYTHON */

static void help_draw_contacts(ToxWindow *self)
{
    WINDOW *win = self->help->win;

    wmove(win, 1, 1);

    wattron(win, A_BOLD | COLOR_PAIR(RED));
    wprintw(win, "Friendlist controls:\n");
    wattroff(win, A_BOLD | COLOR_PAIR(RED));

    wprintw(win, "  Up and Down arrows            : Scroll through list\n");
    wprintw(win, "  Right and Left arrows         : Switch between friendlist and blocked list\n");
    wprintw(win, "  Enter                         : Open a chat window with selected contact\n");
    wprintw(win, "  Delete                        : Permanently delete a contact\n");
    wprintw(win, "  B                             : Block or unblock a contact\n");

    help_draw_bottom_menu(win);

    box(win, ACS_VLINE, ACS_HLINE);
    wnoutrefresh(win);
}

void help_onKey(ToxWindow *self, wint_t key)
{
    int height;

    switch (key) {
        case L'x':
        case T_KEY_ESC:
            help_exit(self);
            break;

        case L'c':
#ifdef VIDEO
            help_init_window(self, 26, 80);
#elif AUDIO
            help_init_window(self, 21, 80);
#else
            help_init_window(self, 11, 80);
#endif
            self->help->type = HELP_CHAT;
            break;

        case L'g':
            height = 22;
#ifdef VIDEO
            height += 8;
#elif AUDIO
            height += 4;
#endif
#ifdef PYTHON
            height += 2;
#endif
#ifdef GAMES
            height += 1;
#endif
            help_init_window(self, height, 80);
            self->help->type = HELP_GLOBAL;
            break;

        case L'o':
            height = 6;
#ifdef AUDIO
            height += 7;
#endif
            help_init_window(self, height, 80);
            self->help->type = HELP_CONFERENCE;
            break;

#ifdef PYTHON

        case L'p':
            help_init_window(self, 4 + num_registered_handlers(), help_max_width());
            self->help->type = HELP_PLUGIN;
            break;
#endif /* PYTHON */

        case L'f':
            help_init_window(self, 10, 80);
            self->help->type = HELP_CONTACTS;
            break;

        case L'k':
            help_init_window(self, 15, 80);
            self->help->type = HELP_KEYS;
            break;

        case L'm':
            help_init_menu(self);
            self->help->type = HELP_MENU;
            break;
    }
}

void help_onDraw(ToxWindow *self)
{
    switch (self->help->type) {
        case HELP_MENU:
            help_draw_menu(self);
            return;

        case HELP_CHAT:
            help_draw_chat(self);
            break;

        case HELP_GLOBAL:
            help_draw_global(self);
            break;

        case HELP_KEYS:
            help_draw_keys(self);
            break;

        case HELP_CONTACTS:
            help_draw_contacts(self);
            break;

        case HELP_CONFERENCE:
            help_draw_conference(self);
            break;

#ifdef PYTHON

        case HELP_PLUGIN:
            help_draw_plugin(self);
            break;
#endif /* PYTHON */
    }
}

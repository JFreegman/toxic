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

#ifdef NO_GETTEXT
#define gettext(A) (A)
#else
#include <libintl.h>
#endif

#include "windows.h"
#include "toxic.h"
#include "help.h"
#include "misc_tools.h"

#define HELP_MENU_HEIGHT 9
#define HELP_MENU_WIDTH 26

void help_init_menu(ToxWindow *self)
{
    if (self->help->win)
        delwin(self->help->win);

    int y2, x2;
    getmaxyx(self->window, y2, x2);

    if (y2 < HELP_MENU_HEIGHT || x2 < HELP_MENU_WIDTH)
        return;

    self->help->win = newwin(HELP_MENU_HEIGHT, HELP_MENU_WIDTH, 3, 3);
    self->help->active = true;
    self->help->type = HELP_MENU;
}

static void help_exit(ToxWindow *self)
{
    delwin(self->help->win);
    memset(self->help, 0, sizeof(Help));
}

static void help_init_window(ToxWindow *self, int height, int width)
{
    if (self->help->win)
        delwin(self->help->win);

    int y2, x2;
    getmaxyx(stdscr, y2, x2);

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

    wprintw(win, " g");
    wattron(win, A_BOLD | COLOR_PAIR(BLUE));
    wprintw(win, "r");
    wattroff(win, A_BOLD | COLOR_PAIR(BLUE));
    wprintw(win, "oup commands\n");

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
    wrefresh(win);
}

static void help_draw_bottom_menu(WINDOW *win)
{
    int y2, x2;
    getmaxyx(win, y2, x2);
    (void) x2;

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
    wprintw(win, gettext("Global Commands:\n"));
    wattroff(win, A_BOLD | COLOR_PAIR(RED));

    wprintw(win, "  /add <ToxID> <msg>         : ");
    wprintw(win, gettext("Add contact with optional message\n"));
    wprintw(win, "  /accept <id>               : ");
    wprintw(win, gettext("Accept friend request\n"));
    wprintw(win, "  /avatar <path>             : ");
    wprintw(win, gettext("Set an avatar (leave path empty to unset)\n"));
    wprintw(win, "  /decline <id>              : ");
    wprintw(win, gettext("Decline friend request\n"));
    wprintw(win, "  /requests                  : ");
    wprintw(win, gettext("List pending friend requests\n"));
    wprintw(win, "  /connect <ip> <port> <key> : ");
    wprintw(win, gettext("Manually connect to a DHT node\n"));
    wprintw(win, "  /status <type> <msg>       : ");
    wprintw(win, gettext("Set status with optional note\n"));
    wprintw(win, "  /note <msg>                : ");
    wprintw(win, gettext("Set a personal note\n"));
    wprintw(win, "  /nick <nick>               : ");
    wprintw(win, gettext("Set your nickname\n"));
    wprintw(win, "  /log <on> or <off>         : ");
    wprintw(win, gettext("Enable/disable logging\n"));
    wprintw(win, "  /group <type>              : ");
    wprintw(win, gettext("Create a group chat where type: text | audio\n"));
    wprintw(win, "  /myid                      : ");
    wprintw(win, gettext("Print your Tox ID\n"));
    wprintw(win, "  /clear                     : ");
    wprintw(win, gettext("Clear window history\n"));
    wprintw(win, "  /close                     : ");
    wprintw(win, gettext("Close the current chat window\n"));
    wprintw(win, "  /quit or /exit             : ");
    wprintw(win, gettext("Exit Toxic\n"));

#ifdef AUDIO
    wattron(win, A_BOLD);
    wprintw(win, gettext("\n Audio:\n"));
    wattroff(win, A_BOLD);

    wprintw(win, "  /lsdev <type>              : ");
    wprintw(win, gettext("List devices where type:"));
    wprintw(win, " in|out\n");
    wprintw(win, "  /sdev <type> <id>          : ");
    wprintw(win, gettext("Set active device\n"));
#endif /* AUDIO */

    help_draw_bottom_menu(win);

    box(win, ACS_VLINE, ACS_HLINE);
    wrefresh(win);
}

static void help_draw_chat(ToxWindow *self)
{
    WINDOW *win = self->help->win;

    wmove(win, 1, 1);

    wattron(win, A_BOLD | COLOR_PAIR(RED));
    wprintw(win, gettext("Chat Commands:\n"));
    wattroff(win, A_BOLD | COLOR_PAIR(RED));

    wprintw(win, "  /invite <n>                : ");
    wprintw(win, gettext("Invite contact to a group chat\n"));
    wprintw(win, "  /join                      : ");
    wprintw(win, gettext("Join a pending group chat\n"));
    wprintw(win, "  /sendfile <path>           : ");
    wprintw(win, gettext("Send a file\n"));
    wprintw(win, "  /savefile <id>             : ");
    wprintw(win, gettext("Receive a file\n"));
    wprintw(win, "  /cancel <type> <id>        : ");
    wprintw(win, gettext("Cancel file transfer where type:"));
    wprintw(win, " in|out\n");

#ifdef AUDIO
    wattron(win, A_BOLD);
    wprintw(win, gettext("\n Audio:\n"));
    wattroff(win, A_BOLD);

    wprintw(win, "  /call                      : ");
    wprintw(win, gettext("Audio call\n"));
    wprintw(win, "  /answer                    : ");
    wprintw(win, gettext("Answer incoming call\n"));
    wprintw(win, "  /reject                    : ");
    wprintw(win, gettext("Reject incoming call\n"));
    wprintw(win, "  /hangup                    : ");
    wprintw(win, gettext("Hangup active call\n"));
    wprintw(win, "  /sdev <type> <id>          : ");
    wprintw(win, gettext("Change active device\n"));
    wprintw(win, "  /mute <type>               : ");
    wprintw(win, gettext("Mute active device if in call\n"));
    wprintw(win, "  /sense <n>                 : ");
    wprintw(win, gettext("VAD sensitivity threshold\n"));
#endif /* AUDIO */

    help_draw_bottom_menu(win);

    box(win, ACS_VLINE, ACS_HLINE);
    wrefresh(win);
}

static void help_draw_keys(ToxWindow *self)
{
    WINDOW *win = self->help->win;

    wmove(win, 1, 1);

    wattron(win, A_BOLD | COLOR_PAIR(RED));
    wprintw(win, gettext("Key bindings:\n"));
    wattroff(win, A_BOLD | COLOR_PAIR(RED));

    wprintw(win, "  Ctrl+O -- Ctrl+P          : ");
    wprintw(win, gettext("Navigate through the tabs\n"));
    wprintw(win, "  Page Up -- Page Down      : ");
    wprintw(win, gettext("Scroll window history one line\n"));
    wprintw(win, "  Ctrl+F -- Ctrl+V          : ");
    wprintw(win, gettext("Scroll window history half a page\n"));
    wprintw(win, "  Ctrl+H                    : ");
    wprintw(win, gettext("Move to the bottom of window history\n"));
    wprintw(win, "  Ctrl+[ -- Ctrl+]          : ");
    wprintw(win, gettext("Scroll peer list in groupchats\n"));
    wprintw(win, "  Ctrl+B                    : ");
    wprintw(win, gettext("Toggle the groupchat peerlist\n\n"));
    wprintw(win, gettext("  (Note: Custom keybindings override these defaults.)\n\n"));

    help_draw_bottom_menu(win);

    box(win, ACS_VLINE, ACS_HLINE);
    wrefresh(win);
}

static void help_draw_group(ToxWindow *self)
{
    WINDOW *win = self->help->win;

    wmove(win, 1, 1);

    wattron(win, A_BOLD | COLOR_PAIR(RED));
    wprintw(win, gettext("Group commands:\n"));
    wattroff(win, A_BOLD | COLOR_PAIR(RED));

    wprintw(win, "  /title <msg>               : ");
    wprintw(win, gettext("Set group title (show current title if no msg)\n\n"));

    help_draw_bottom_menu(win);

    box(win, ACS_VLINE, ACS_HLINE);
    wrefresh(win);
}

static void help_draw_contacts(ToxWindow *self)
{
    WINDOW *win = self->help->win;

    wmove(win, 1, 1);

    wattron(win, A_BOLD | COLOR_PAIR(RED));
    wprintw(win, gettext("Friendlist controls:\n"));
    wattroff(win, A_BOLD | COLOR_PAIR(RED));

    wprintw(win, gettext("  Up and Down arrows            : Scroll through list\n"));
    wprintw(win, gettext("  Right and Left arrows         : Switch between friendlist and blocked list\n"));
    wprintw(win, gettext("  Enter                         : Open a chat window with selected contact\n"));
    wprintw(win, gettext("  Delete                        : Permanently delete a contact\n"));
    wprintw(win, "  B                             : ");
    wprintw(win, gettext("Block or unblock a contact\n"));

    help_draw_bottom_menu(win);

    box(win, ACS_VLINE, ACS_HLINE);
    wrefresh(win);
}

void help_onKey(ToxWindow *self, wint_t key)
{
    switch(key) {
        case 'x':
        case T_KEY_ESC:
            help_exit(self);
            break;

        case 'c':
#ifdef AUDIO
            help_init_window(self, 19, 80);
#else
            help_init_window(self, 9, 80);
#endif
            self->help->type = HELP_CHAT;
            break;

        case 'g':
#ifdef AUDIO
            help_init_window(self, 24, 80);
#else
            help_init_window(self, 20, 80);
#endif
            self->help->type = HELP_GLOBAL;
            break;

        case 'r':
            help_init_window(self, 6, 80);
            self->help->type = HELP_GROUP;
            break;

        case 'f':
            help_init_window(self, 10, 80);
            self->help->type = HELP_CONTACTS;
            break;

        case 'k':
            help_init_window(self, 13, 80);
            self->help->type = HELP_KEYS;
            break;

        case 'm':
            help_init_menu(self);
            self->help->type = HELP_MENU;
            break;
    }
}

void help_onDraw(ToxWindow *self)
{
    curs_set(0);

    switch(self->help->type) {
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

        case HELP_GROUP:
            help_draw_group(self);
            break;
    }
}

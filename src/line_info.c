/*  line_info.c
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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE    /* needed for wcswidth() */
#endif

#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "conference.h"
#include "groupchats.h"
#include "line_info.h"
#include "message_queue.h"
#include "misc_tools.h"
#include "notify.h"
#include "settings.h"
#include "toxic.h"
#include "windows.h"

void line_info_init(struct history *hst)
{
    hst->line_root = calloc(1, sizeof(struct line_info));

    if (hst->line_root == NULL) {
        exit_toxic_err("failed in line_info_init", FATALERR_MEMORY);
    }

    hst->line_start = hst->line_root;
    hst->line_end = hst->line_start;
    hst->queue_size = 0;
}

/* resets line_start (moves to end of chat history) */
void line_info_reset_start(ToxWindow *self, struct history *hst)
{
    struct line_info *line = hst->line_end;

    if (line == NULL || line->prev == NULL) {
        return;
    }

    int y2;
    int x2;
    getmaxyx(self->window, y2, x2);
    UNUSED_VAR(x2);

    int top_offst = self->type != WINDOW_TYPE_CONFERENCE ? TOP_BAR_HEIGHT : 0;
    int max_y = y2 - CHATBOX_HEIGHT - WINDOW_BAR_HEIGHT - top_offst;

    uint16_t curlines = 0;

    do {
        curlines += line->format_lines;
        line = line->prev;
    } while (line->prev && curlines + line->format_lines <= max_y);

    hst->line_start = line;

    self->scroll_pause = false;
}

void line_info_cleanup(struct history *hst)
{
    if (hst == NULL) {
        return;
    }

    struct line_info *tmp1 = hst->line_root;

    while (tmp1) {
        struct line_info *tmp2 = tmp1->next;
        free(tmp1);
        tmp1 = tmp2;
    }

    for (size_t i = 0; i < hst->queue_size; ++i) {
        if (hst->queue[i]) {
            free(hst->queue[i]);
        }
    }

    free(hst);
}

/* moves root forward and frees previous root */
static void line_info_root_fwd(struct history *hst)
{
    struct line_info *tmp = hst->line_root->next;
    tmp->prev = NULL;

    if (hst->line_start->prev == NULL) {    /* if line_start is root move it forward as well */
        hst->line_start = hst->line_start->next;
        hst->line_start->prev = NULL;
        ++hst->start_id;
    }

    free(hst->line_root);
    hst->line_root = tmp;
}

/* returns ptr to queue item 0 and removes it from queue. Returns NULL if queue is empty. */
static struct line_info *line_info_ret_queue(struct history *hst)
{
    if (hst->queue_size == 0) {
        return NULL;
    }

    struct line_info *line = hst->queue[0];

    for (size_t i = 0; i < hst->queue_size; ++i) {
        hst->queue[i] = hst->queue[i + 1];
    }

    --hst->queue_size;

    return line;
}

/* Prints a maximum of `n` chars from `s` to `win`.
 *
 * Return 1 if the string contains a newline byte.
 * Return 0 if string does not contain a newline byte.
 * Return -1 if printing was aborted.
 */
static int print_n_chars(WINDOW *win, const wchar_t *s, size_t n, int max_y)
{
    // we use an array to represent a single wchar in order to get around an ncurses
    // bug with waddnwstr() that overreads the memory address by one byte when
    // supplied with a single wchar.
    wchar_t ch[2] = {0};
    bool newline = false;

    for (size_t i = 0; i < n && (ch[0] = s[i]); ++i) {
        if (ch[0] == L'\n') {
            newline = true;

            int x;
            int y;
            getyx(win, y, x);

            UNUSED_VAR(x);

            // make sure cursor will wrap correctly after newline to prevent display bugs
            if (y + 1 >= max_y) {
                return -1;
            }
        }

        if (win) {
#ifdef HAVE_WIDECHAR
            waddnwstr(win, ch, 1);
#else
            char b;

            if (wcstombs(&b, ch, sizeof(char)) != 1) {
                continue;
            }

            wprintw(win, "%c", b);
#endif  // HAVE_WIDECHAR
        }
    }

    return newline;
}

/* Returns the index of the last space character in `s` found before `limit`.
 * Returns -1 if no space is found.
 */
static int rspace_index(const wchar_t *s, int limit)
{
    for (int i = limit; i >= 0; --i) {
        if (s[i] == L' ') {
            return i;
        }
    }

    return -1;
}

/* Returns the first index in `s` containing a newline byte found before `limit`.
 * Returns -1 if no newline  is found.
 */
static int newline_index(const wchar_t *s, int limit)
{
    wchar_t ch;

    for (int i = 0; i < limit && (ch = s[i]); ++i) {
        if (ch == L'\n') {
            return i;
        }
    }

    return -1;
}

/* Returns the number of newline bytes in `s` */
static unsigned int newline_count(const wchar_t *s)
{
    wchar_t ch;
    unsigned int count = 0;

    for (size_t i = 0; (ch = s[i]); ++i) {
        if (ch == L'\n') {
            ++count;
        }
    }

    return count;
}

/* Prints `line` message to window, wrapping at the last word that fits on the current line.
 * This function updates the `format_lines` field of `line` according to current window dimensions.
 *
 * If `win` is null nothing will be printed to the window. This is useful to set the
 * `format_lines` field on initialization.
 *
 * Return 0 on success.
 * Return -1 if not all characters in line's message were printed to screen.
 */
static int print_wrap(WINDOW *win, struct line_info *line, int max_x, int max_y)
{
    const wchar_t *msg = line->msg;
    uint16_t length = line->msg_width;
    uint16_t lines = 0;
    const int x_start = line->len - line->msg_width - 1;  // manually keep track of x position because ncurses sucks
    int x_limit = max_x - x_start;

    if (x_limit <= 1) {
        fprintf(stderr, "Warning: x_limit <= 0 in print_wrap(): %d\n", x_limit);
        return -1;
    }

    int x;
    int y;
    UNUSED_VAR(y);

    while (msg) {
        getyx(win, y, x);

        // next line would print past window limit so we abort; we don't want to update format_lines
        if (x > x_start) {
            return -1;
        }

        if (length < x_limit) {
            const int p_ret = print_n_chars(win, msg, length, max_y);

            if (p_ret == 1) {
                lines += newline_count(msg);
            } else if (p_ret == -1) {
                return -1;
            }

            ++lines;
            break;
        }

        const int newline_idx = newline_index(msg, x_limit - 1);

        if (newline_idx >= 0) {
            if (print_n_chars(win, msg, newline_idx + 1, max_y) == -1) {
                return -1;
            }

            msg += (newline_idx + 1);
            length -= (newline_idx + 1);
            x_limit = max_x; // if we find a newline we stop adding column padding for rest of message
            ++lines;
            continue;
        }

        const int space_idx = rspace_index(msg, x_limit - 1);

        if (space_idx >= 1) {
            if (print_n_chars(win, msg, space_idx, max_y) == -1) {
                return -1;
            }

            msg += space_idx + 1;
            length -= (space_idx + 1);

            if (win) {
                waddch(win, '\n');
            }
        } else {
            if (print_n_chars(win, msg, x_limit, max_y) == -1) {
                return -1;
            }

            msg += x_limit;
            length -= x_limit;
        }

        // Add padding to the start of the next line
        if (win && x_limit < max_x) {
            for (size_t i = 0; i < x_start; ++i) {
                waddch(win, ' ');
            }
        }

        ++lines;
    }

    if (win && line->noread_flag) {
        getyx(win, y, x);

        if (x >= max_x - 1 || x == x_start) {
            ++lines;
        }

        wattron(win, COLOR_PAIR(RED));
        wprintw(win, " x");
        wattroff(win, COLOR_PAIR(RED));
    }

    line->format_lines = lines;

    return 0;
}

/* Converts the multibyte string `msg` into a wide character string and puts
 * the result in `buf`.
 *
 * `buf_size` is the number of wide characters that `buf` can hold.
 *
 * Returns the widechar width of the string.
 */
uint16_t line_info_add_msg(wchar_t *buf, size_t buf_size, const char *msg)
{
    if (msg == NULL || msg[0] == '\0') {
        return 0;
    }

    const int wc_msg_len = mbs_to_wcs_buf(buf, msg, buf_size);

    if (wc_msg_len > 0 && wc_msg_len < buf_size) {
        buf[wc_msg_len] = L'\0';
        int width = wcswidth(buf, wc_msg_len);

        if (width < 0 || width > UINT16_MAX) {  // the best we can do on failure is to fall back to strlen
            width = strlen(msg);
        }

        return (uint16_t)width;
    } else {
        fprintf(stderr, "Failed to convert string '%s' to widechar (len=%d, error=%s)\n",
                msg, wc_msg_len, strerror(errno));
        const wchar_t *err = L"Failed to parse message";
        uint16_t width = (uint16_t)wcslen(err);
        wmemcpy(buf, err, width);
        buf[width] = L'\0';
        return width;
    }
}

static void line_info_init_line(ToxWindow *self, struct line_info *line)
{
    int y2;
    int x2;
    getmaxyx(self->window, y2, x2);

    UNUSED_VAR(y2);

    const int max_y = y2 - CHATBOX_HEIGHT - WINDOW_BAR_HEIGHT;
    const int max_x = self->show_peerlist ? x2 - 1 - SIDEBAR_WIDTH : x2;

    print_wrap(NULL, line, max_x, max_y);
}

/* creates new line_info line and puts it in the queue.
 *
 * Returns the id of the new line.
 * Returns -1 on failure.
 */
int line_info_add(ToxWindow *self, const Client_Config *c_config, bool show_timestamp, const char *name1,
                  const char *name2, LINE_TYPE type, uint8_t bold, uint8_t colour, const char *msg, ...)
{
    if (self == NULL) {
        return -1;
    }

    struct history *hst = self->chatwin->hst;

    if (hst->queue_size >= MAX_LINE_INFO_QUEUE) {
        return -1;
    }

    struct line_info *new_line = calloc(1, sizeof(struct line_info));

    if (new_line == NULL) {
        exit_toxic_err("failed in line_info_add", FATALERR_MEMORY);
    }

    char frmt_msg[MAX_LINE_INFO_MSG_SIZE];
    frmt_msg[0] = 0;

    va_list args;
    va_start(args, msg);
    vsnprintf(frmt_msg, sizeof(frmt_msg), msg, args);
    va_end(args);

    int len = 1;     /* there will always be a newline */

    /* for type-specific formatting in print function */
    switch (type) {
        case IN_ACTION:

        /* fallthrough */
        case OUT_ACTION_READ:

        /* fallthrough */
        case OUT_ACTION:
            len += strlen(c_config->line_normal) + 2; // two spaces
            break;

        case IN_MSG:
        case OUT_MSG_READ:

        /* fallthrough */
        case OUT_MSG:
            len += strlen(c_config->line_normal) + 3; // two spaces and a ':' char
            break;

        case IN_PRVT_MSG:

        /* fallthrough */
        case OUT_PRVT_MSG:
            len += strlen(c_config->line_special) + 3;
            break;

        case CONNECTION:
            len += strlen(c_config->line_join) + 2;  // two spaces
            break;

        case DISCONNECTION:
            len += strlen(c_config->line_quit) + 2;  // two spaces
            break;

        case SYS_MSG:
            break;

        case NAME_CHANGE:
            len += strlen(c_config->line_alert) + 2;  // two spaces
            break;

        case PROMPT:
            len += 2;  // '$' char and a space
            break;

        default:
            len += 2;
            break;
    }

    const uint16_t msg_width = line_info_add_msg(new_line->msg, sizeof(new_line->msg) / sizeof(wchar_t), frmt_msg);
    len += msg_width;

    if (show_timestamp)  {
        if (c_config->timestamps == TIMESTAMPS_ON) {
            get_time_str(new_line->timestr, sizeof(new_line->timestr), c_config->timestamp_format);
        }

        len += strlen(new_line->timestr) + 1;  // need the +1 regardless of client setting
    }

    if (name1) {
        snprintf(new_line->name1, sizeof(new_line->name1), "%s", name1);
        len += strlen(new_line->name1);
    }

    if (name2) {
        snprintf(new_line->name2, sizeof(new_line->name2), "%s", name2);
        len += strlen(new_line->name2);
    }

    new_line->id = (hst->line_end->id + 1 + hst->queue_size) % INT_MAX;
    new_line->len = len;
    new_line->msg_width = msg_width;
    new_line->type = type;
    new_line->bold = bold;
    new_line->colour = colour;
    new_line->noread_flag = false;
    new_line->timestamp = get_unix_time();

    if (type == OUT_MSG || type == OUT_ACTION) {
        new_line->noread_flag = self->stb->connection == TOX_CONNECTION_NONE;
    }

    line_info_init_line(self, new_line);

    hst->queue[hst->queue_size] = new_line;
    ++hst->queue_size;

    return new_line->id;
}

int line_info_load_history(ToxWindow *self, const Client_Config *c_config, const char *timestamp,
                           const char *name, int colour, const char *message)
{
    if (self == NULL) {
        return -1;
    }

    struct history *hst = self->chatwin->hst;

    if (hst->queue_size >= MAX_LINE_INFO_QUEUE) {
        return -1;
    }

    struct line_info *new_line = calloc(1, sizeof(struct line_info));

    if (new_line == NULL) {
        return -1;
    }

    int len = 1 + strlen(c_config->line_normal) + 3;

    const uint16_t msg_width = line_info_add_msg(new_line->msg, sizeof(new_line->msg) / sizeof(wchar_t), message);
    len += msg_width;

    if (c_config->timestamps == TIMESTAMPS_ON) {
        snprintf(new_line->timestr, sizeof(new_line->timestr), "%s", timestamp);
    }

    len += strlen(new_line->timestr) + 1;

    snprintf(new_line->name1, sizeof(new_line->name1), "%s", name);
    len += strlen(new_line->name1);

    new_line->id = (hst->line_end->id + 1 + hst->queue_size) % INT_MAX;
    new_line->len = len;
    new_line->msg_width = msg_width;
    new_line->type = IN_MSG;
    new_line->bold = false;
    new_line->colour = colour;
    new_line->noread_flag = false;
    new_line->timestamp = get_unix_time();

    line_info_init_line(self, new_line);

    hst->queue[hst->queue_size] = new_line;
    ++hst->queue_size;

    return new_line->id;
}

/* adds a single queue item to hst if possible. only called once per call to line_info_print() */
static void line_info_check_queue(ToxWindow *self, const Client_Config *c_config)
{
    struct history *hst = self->chatwin->hst;
    struct line_info *line = line_info_ret_queue(hst);

    if (line == NULL) {
        return;
    }

    if (hst->start_id > c_config->history_size) {
        line_info_root_fwd(hst);
    }

    line->prev = hst->line_end;
    hst->line_end->next = line;
    hst->line_end = line;
    hst->line_end->id = line->id;

    if (!self->scroll_pause) {
        line_info_reset_start(self, hst);
    }
}

void line_info_print(ToxWindow *self, const Client_Config *c_config)
{
    ChatContext *ctx = self->chatwin;

    if (ctx == NULL) {
        return;
    }

    struct history *hst = ctx->hst;

    /* Only allow one new item to be added to chat window per call to this function */
    line_info_check_queue(self, c_config);

    WINDOW *win = ctx->history;

    wclear(win);

    int y2;

    int x2;

    getmaxyx(self->window, y2, x2);

    if (x2 - 1 <= SIDEBAR_WIDTH) {  // leave room on x axis for sidebar padding
        return;
    }

    if (self->type == WINDOW_TYPE_CONFERENCE) {
        wmove(win, 0, 0);
    } else {
        wmove(win, TOP_BAR_HEIGHT, 0);
    }

    struct line_info *line = hst->line_start->next;

    if (line == NULL) {
        return;
    }

    const int max_y = y2 - CHATBOX_HEIGHT - WINDOW_BAR_HEIGHT;
    const int max_x = self->show_peerlist ? x2 - 1 - SIDEBAR_WIDTH : x2;
    uint16_t numlines = line->format_lines;

    while (line && numlines++ <= max_y) {
        int y;
        int x;
        getyx(win, y, x);

        UNUSED_VAR(y);

        if (x > 0) { // Prevents us from printing off the screen
            break;
        }

        uint8_t type = line->type;

        switch (type) {
            case OUT_MSG:

            /* fallthrough */
            case OUT_MSG_READ:

            /* fallthrough */
            case IN_MSG: {
                wattron(win, COLOR_PAIR(BLUE));
                wprintw(win, "%s ", line->timestr);
                wattroff(win, COLOR_PAIR(BLUE));

                int nameclr = GREEN;

                if (line->colour) {
                    nameclr = line->colour;
                } else if (type == IN_MSG) {
                    nameclr = CYAN;
                }

                wattron(win, COLOR_PAIR(nameclr));
                wprintw(win, "%s %s: ", c_config->line_normal, line->name1);
                wattroff(win, COLOR_PAIR(nameclr));

                if (line->msg[0] == L'\0') {
                    waddch(win, '\n');
                    break;
                }

                if (line->msg[0] == L'>') {
                    wattron(win, COLOR_PAIR(GREEN));
                } else if (line->msg[0] == L'<') {
                    wattron(win, COLOR_PAIR(RED));
                }

                print_wrap(win, line, max_x, max_y);

                if (line->msg[0] == L'>') {
                    wattroff(win, COLOR_PAIR(GREEN));
                } else if (line->msg[0] == L'<') {
                    wattroff(win, COLOR_PAIR(RED));
                }

                waddch(win, '\n');
                break;
            }

            case IN_PRVT_MSG:

            /* fallthrough */

            case OUT_PRVT_MSG: {
                wattron(win, COLOR_PAIR(BLUE));
                wprintw(win, "%s ", line->timestr);
                wattroff(win, COLOR_PAIR(BLUE));

                const int nameclr = line->colour ? line->colour : GREEN;

                wattron(win, COLOR_PAIR(nameclr));
                wprintw(win, "%s %s: ", c_config->line_special, line->name1);
                wattroff(win, COLOR_PAIR(nameclr));

                if (line->msg[0] == '>') {
                    wattron(win, COLOR_PAIR(GREEN));
                } else if (line->msg[0] == '<') {
                    wattron(win, COLOR_PAIR(RED));
                }

                print_wrap(win, line, max_x, max_y);

                if (line->msg[0] == '>') {
                    wattroff(win, COLOR_PAIR(GREEN));
                } else if (line->msg[0] == '<') {
                    wattroff(win, COLOR_PAIR(RED));
                }

                waddch(win, '\n');
                break;
            }

            case OUT_ACTION_READ:

            /* fallthrough */
            case OUT_ACTION:

            /* fallthrough */
            case IN_ACTION: {
                wattron(win, COLOR_PAIR(BLUE));
                wprintw(win, "%s ", line->timestr);
                wattroff(win, COLOR_PAIR(BLUE));

                wattron(win, COLOR_PAIR(YELLOW));
                wprintw(win, "%s %s ", c_config->line_normal, line->name1);
                print_wrap(win, line, max_x, max_y);
                wattroff(win, COLOR_PAIR(YELLOW));

                waddch(win, '\n');
                break;
            }

            case SYS_MSG: {
                if (line->timestr[0]) {
                    wattron(win, COLOR_PAIR(BLUE));
                    wprintw(win, "%s ", line->timestr);
                    wattroff(win, COLOR_PAIR(BLUE));
                }

                if (line->bold) {
                    wattron(win, A_BOLD);
                }

                if (line->colour) {
                    wattron(win, COLOR_PAIR(line->colour));
                }

                print_wrap(win, line, max_x, max_y);
                waddch(win, '\n');

                if (line->bold) {
                    wattroff(win, A_BOLD);
                }

                if (line->colour) {
                    wattroff(win, COLOR_PAIR(line->colour));
                }

                break;
            }

            case PROMPT: {
                wattron(win, COLOR_PAIR(GREEN));
                wprintw(win, "$ ");
                wattroff(win, COLOR_PAIR(GREEN));

                if (line->msg[0] != L'\0') {
                    print_wrap(win, line, max_x, max_y);
                }

                waddch(win, '\n');
                break;
            }

            case CONNECTION: {
                wattron(win, COLOR_PAIR(BLUE));
                wprintw(win, "%s ", line->timestr);
                wattroff(win, COLOR_PAIR(BLUE));

                wattron(win, COLOR_PAIR(line->colour));
                wprintw(win, "%s ", c_config->line_join);

                wattron(win, A_BOLD);
                wprintw(win, "%s ", line->name1);
                wattroff(win, A_BOLD);

                print_wrap(win, line, max_x, max_y);
                waddch(win, '\n');

                wattroff(win, COLOR_PAIR(line->colour));

                break;
            }

            case DISCONNECTION: {
                wattron(win, COLOR_PAIR(BLUE));
                wprintw(win, "%s ", line->timestr);
                wattroff(win, COLOR_PAIR(BLUE));

                wattron(win, COLOR_PAIR(line->colour));
                wprintw(win, "%s ", c_config->line_quit);

                wattron(win, A_BOLD);
                wprintw(win, "%s ", line->name1);
                wattroff(win, A_BOLD);

                print_wrap(win, line, max_x, max_y);
                waddch(win, '\n');

                wattroff(win, COLOR_PAIR(line->colour));

                break;
            }

            case NAME_CHANGE: {
                wattron(win, COLOR_PAIR(BLUE));
                wprintw(win, "%s ", line->timestr);
                wattroff(win, COLOR_PAIR(BLUE));

                wattron(win, COLOR_PAIR(MAGENTA));
                wprintw(win, "%s ", c_config->line_alert);
                wattron(win, A_BOLD);
                wprintw(win, "%s", line->name1);
                wattroff(win, A_BOLD);

                print_wrap(win, line, max_x, max_y);

                wattron(win, A_BOLD);
                wprintw(win, "%s\n", line->name2);
                wattroff(win, A_BOLD);
                wattroff(win, COLOR_PAIR(MAGENTA));

                break;
            }
        }

        line = line->next;
    }

    flag_interface_refresh();

    /* keep calling until queue is empty */
    if (hst->queue_size > 0) {
        line_info_print(self, c_config);
    }
}

/*
 * Return true if all lines starting from `line` can fit on the screen.
 */
static bool line_info_screen_fit(ToxWindow *self, struct line_info *line)
{
    if (!line) {
        return true;
    }

    int x2;
    int y2;
    getmaxyx(self->chatwin->history, y2, x2);

    UNUSED_VAR(x2);

    const int top_offset = (self->type == WINDOW_TYPE_CHAT) || (self->type == WINDOW_TYPE_PROMPT) ? TOP_BAR_HEIGHT : 0;
    const int max_y = y2 - top_offset;

    uint16_t lines = line->format_lines;

    while (line) {
        if (lines > max_y) {
            return false;
        }

        lines += line->format_lines;
        line = line->next;
    }

    return true;
}

/* puts msg in specified line_info msg buffer */
void line_info_set(ToxWindow *self, uint32_t id, char *msg)
{
    flag_interface_refresh();

    struct line_info *line = self->chatwin->hst->line_end;

    while (line) {
        if (line->id == id) {
            const uint16_t new_width = line_info_add_msg(line->msg, sizeof(line->msg) / sizeof(wchar_t), msg);
            line->len = line->len - line->msg_width + new_width;
            line->msg_width = new_width;
            return;
        }

        line = line->prev;
    }
}

/* Return the line_info object associated with `id`.
 * Return NULL if id cannot be found
 */
struct line_info *line_info_get(ToxWindow *self, uint32_t id)
{
    struct line_info *line = self->chatwin->hst->line_end;

    while (line) {
        if (line->id == id) {
            return line;
        }

        line = line->prev;
    }

    return NULL;
}

static void line_info_scroll_up(ToxWindow *self, struct history *hst)
{
    if (hst->line_start->prev) {
        hst->line_start = hst->line_start->prev;
        self->scroll_pause = true;
    }
}

static void line_info_scroll_down(ToxWindow *self, struct history *hst)
{
    struct line_info *next = hst->line_start->next;

    if (next && self->scroll_pause) {
        if (line_info_screen_fit(self, next->next)) {
            line_info_reset_start(self, hst);
        } else {
            hst->line_start = next;
        }
    } else {
        line_info_reset_start(self, hst);
    }
}

static void line_info_page_up(ToxWindow *self, struct history *hst)
{
    int x2;
    int y2;
    getmaxyx(self->window, y2, x2);

    UNUSED_VAR(x2);

    const int top_offset = (self->type == WINDOW_TYPE_CHAT) || (self->type == WINDOW_TYPE_PROMPT) ? TOP_BAR_HEIGHT : 0;
    const int max_y = y2 - top_offset;
    size_t jump_dist = max_y / 2;

    for (size_t i = 0; i < jump_dist && hst->line_start->prev; ++i) {
        hst->line_start = hst->line_start->prev;
    }

    self->scroll_pause = true;
}

static void line_info_page_down(ToxWindow *self, struct history *hst)
{
    if (!self->scroll_pause) {
        return;
    }

    int x2;
    int y2;
    getmaxyx(self->chatwin->history, y2, x2);

    UNUSED_VAR(x2);

    const int top_offset = (self->type == WINDOW_TYPE_CHAT) || (self->type == WINDOW_TYPE_PROMPT) ? TOP_BAR_HEIGHT : 0;
    const int max_y = y2 - top_offset;
    size_t jump_dist = max_y / 2;

    struct line_info *next = hst->line_start->next;

    for (size_t i = 0; i < jump_dist && next; ++i) {
        if (line_info_screen_fit(self, next->next)) {
            line_info_reset_start(self, hst);
            break;
        }

        hst->line_start = next;
        next = hst->line_start->next;
    }
}

bool line_info_onKey(ToxWindow *self, const Client_Config *c_config, wint_t key)
{
    struct history *hst = self->chatwin->hst;
    bool match = true;

    if (key == c_config->key_half_page_up) {
        line_info_page_up(self, hst);
    } else if (key == c_config->key_half_page_down) {
        line_info_page_down(self, hst);
    } else if (key == c_config->key_scroll_line_up) {
        line_info_scroll_up(self, hst);
    } else if (key == c_config->key_scroll_line_down) {
        line_info_scroll_down(self, hst);
    } else if (key == c_config->key_page_bottom) {
        line_info_reset_start(self, hst);
    } else {
        match = false;
    }

    if (match) {
        flag_interface_refresh();
    }

    return match;
}

void line_info_clear(struct history *hst)
{
    hst->line_start = hst->line_end;
    hst->start_id = hst->line_start->id;
}

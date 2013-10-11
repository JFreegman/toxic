/*
 * Toxic -- Tox Curses Client
 */

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>

#include "toxic_windows.h"

// XXX: FIX
unsigned char *hex_string_to_bin(char hex_string[])
{
    size_t len = strlen(hex_string);
    unsigned char *val = malloc(len);

    if (val == NULL) {
        endwin();
        fprintf(stderr, "malloc() failed. Aborting...\n");
        exit(EXIT_FAILURE);
    }

    char *pos = hex_string;
    size_t i;

    for (i = 0; i < len; ++i, pos += 2)
        sscanf(pos, "%2hhx", &val[i]);

    return val;
}

/* Get the current local time */
struct tm *get_time(void)
{
    struct tm *timeinfo;
    time_t now;
    time(&now);
    timeinfo = localtime(&now);
    return timeinfo;
}

/* check that the string has one non-space character */
int string_is_empty(char *string)
{
    int rc = 0;
    char *copy = strdup(string);
    rc = ((strtok(copy, " ") == NULL) ? 1 : 0);
    free(copy);
    return rc;
}

/* convert wide characters to null terminated string */
uint8_t *wcs_to_char(wchar_t *string)
{
    size_t len = 0;
    char *ret = NULL;

    len = wcstombs(NULL, string, 0);
    if (len != (size_t) -1) {
        len++;
        ret = malloc(len);
        if (ret != NULL)
            wcstombs(ret, string, len);
    } else {
        ret = malloc(2);
        if (ret != NULL) {
            ret[0] = ' ';
            ret[1] = '\0';
        }
    }

    if (ret == NULL) {
        endwin();
        fprintf(stderr, "malloc() failed. Aborting...\n");
        exit(EXIT_FAILURE);
    }

    return ret;
}

/* convert a wide char to null terminated string */
char *wc_to_char(wchar_t ch)
{
    int len = 0;
    static char ret[MB_LEN_MAX + 1];

    len = wctomb(ret, ch);
    if (len == -1) {
        ret[0] = ' ';
        ret[1] = '\0';
    } else {
        ret[len] = '\0';
    }

    return ret;
}

/* Prints the time to given window */
void print_time(WINDOW *window)
{
    struct tm *timeinfo = get_time();

    wattron(window, COLOR_PAIR(CYAN));
    wprintw(window, "[%02d:%02d:%02d] ", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    wattroff(window, COLOR_PAIR(CYAN));
}
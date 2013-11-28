/*
 * Toxic -- Tox Curses Client
 */

#define MIN(x, y) (((x) < (y)) ? (x) : (y))

/* convert a hex string to binary */
unsigned char *hex_string_to_bin(char hex_string[]);

/* get the current local time */
struct tm *get_time(void);

/* Prints the time to given window */
void print_time(WINDOW *window);

/* Returns 1 if the string is empty, 0 otherwise */
int string_is_empty(char *string);

/* convert wide characters to null terminated string */
uint8_t *wcs_to_char(wchar_t *string);

/* convert a wide char to null terminated string */
char *wc_to_char(wchar_t ch);

/* Returns true if connection has timed out, false otherwise */
bool timed_out(uint64_t timestamp, uint64_t timeout, uint64_t curtime);

/* Beeps and makes window tab blink */
void alert_window(ToxWindow *self);

/* case-insensitive string compare function for use with qsort - same return logic as strcmp */
int name_compare(const void *nick1, const void *nick2);

/* Returns true if nick is valid. A valid toxic nick:
      - cannot be empty
      - cannot start with a space
      - must not contain contiguous spaces */
bool valid_nick(uint8_t *nick);

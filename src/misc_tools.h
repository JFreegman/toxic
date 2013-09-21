/*
 * Toxic -- Tox Curses Client
 */

/* convert a hex string to binary */
unsigned char *hex_string_to_bin(char hex_string[]);

/* get the current local time */
struct tm *get_time(void);

/* check that the string has one non-space character */
int string_is_empty(char *string);

/* convert wide characters to null terminated string */
uint8_t *wcs_to_char(wchar_t *string);

/* convert a wide char to null terminated string */
char *wc_to_char(wchar_t ch);

/*
 * Toxic -- Tox Curses Client
 */

// #define MIN(x, y) (((x) < (y)) ? (x) : (y))

/* convert a hex string to binary */
unsigned char *hex_string_to_bin(char hex_string[]);

/* get the current local time */
struct tm *get_time(void);

/* Prints the time to given window */
void print_time(WINDOW *window);

/* Returns 1 if the string is empty, 0 otherwise */
int string_is_empty(char *string);

/* convert a multibyte string to a wide character string (must provide buffer) */
int char_to_wcs_buf(wchar_t *buf, const uint8_t *string, size_t n);

/* converts wide character string into a multibyte string.
   Same thing as wcs_to_mbs() but caller must provide its own buffer */
int wcs_to_mbs_buf(uint8_t *buf, const wchar_t *string, size_t n);

/* convert wide characters to multibyte string: string returned must be free'd */
uint8_t *wcs_to_mbs(wchar_t *string);

/* convert a wide char to multibyte char */
char *wc_to_char(wchar_t ch);

/* Returns true if connection has timed out, false otherwise */
bool timed_out(uint64_t timestamp, uint64_t timeout, uint64_t curtime);

/* Colours the window tab according to type. Beeps if is_beep is true */
void alert_window(ToxWindow *self, int type, bool is_beep);

/* case-insensitive string compare function for use with qsort */
int qsort_strcasecmp_hlpr(const void *nick1, const void *nick2);

/* Returns true if nick is valid. A valid toxic nick:
      - cannot be empty
      - cannot start with a space
      - must not contain contiguous spaces */
bool valid_nick(uint8_t *nick);

/*
 * Buffer helper tools.
 */

/* Adds char to buffer at pos */
void add_char_to_buf(wchar_t *buf, size_t *pos, size_t *len, wint_t ch);

/* Deletes the character before pos */
void del_char_buf_bck(wchar_t *buf, size_t *pos, size_t *len);

/* Deletes the character at pos */
void del_char_buf_frnt(wchar_t *buf, size_t *pos, size_t *len);

/* Deletes the line from beginning to pos */
void discard_buf(wchar_t *buf, size_t *pos, size_t *len);

/* Deletes the line from pos to len */
void kill_buf(wchar_t *buf, size_t *pos, size_t *len);

/* nulls buf and sets pos and len to 0 */
void reset_buf(wchar_t *buf, size_t *pos, size_t *len);

/* looks for the first instance in list that begins with the last entered word in buf according to pos, 
   then fills buf with the complete word. e.g. "Hello jo" would complete the buffer 
   with "Hello john".

   list is a pointer to the list of strings being compared, n_items is the number of items
   in the list, and size is the size of each item in the list. 

   Returns the difference between the old len and new len of buf on success, -1 if error */
int complete_line(wchar_t *buf, size_t *pos, size_t *len, const uint8_t *list, int n_items, int size);
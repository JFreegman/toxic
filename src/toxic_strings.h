/*
 * Toxic -- Tox Curses Client
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
int complete_line(wchar_t *buf, size_t *pos, size_t *len, const void *list, int n_items, int size);

/*
 * General charset conversion library header (using gconv)
 * Convert Palm  <-> Unix:
 * Palm : Any - according to the "other-pda-charset" setup option.
 * Unix : UTF-8
 */

/* otherconv_init: Call this before any conversion 
 * (also use whenever other-pda-charset option changed) 
 * 
 * Returns 0 if OK, -1 if iconv could not be initialized
 *  (probably because of bad charset string)
 */
int otherconv_init(void);
/* otherconv_free: Call this when done */ 
void otherconv_free(void); 

char *other_to_UTF(const char *buf, int buf_len);
void UTF_to_other(char *const buf, int buf_len);

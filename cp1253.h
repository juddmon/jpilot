/*
 * Modern Greek library header
 * Convert Palm  <-> Unix:
 * Palm : CP 1253
 * Unix : UTF-8
 */

int WinGR2UTFsz(const unsigned char *buf, int buf_len);
unsigned char *win1253_to_UTF(const unsigned char *buf, int buf_len);
void UTF_to_win1253(unsigned char *const buf, int buf_len);

/*
 * Czech, Polish (and other CP 1250 languages) library header
 * Convert Palm  <-> Unix:
 * Palm : CP 1250
 * Unix : ISO-8859-2
 *   and
 * Palm : CP 1250
 * Unix : UTF-8
 *   and also (JPA)
 * Palm : CP 1252
 * Unix : UTF-8
 */

void Win2Lat(unsigned char *const buf, int buf_len);
void Lat2Win(unsigned char *const buf, int buf_len);

int Win2UTFsz(const unsigned char *buf, int buf_len); /* JPA */
/* void Win2UTF(unsigned char *const buf, int buf_len); JPA */
unsigned char *Win2UTF(const unsigned char *buf, int buf_len); /* JPA */
void UTF2Win(unsigned char *const buf, int buf_len);

unsigned char *Lat2UTF(const unsigned char *buf, int buf_len);
void UTF2Lat(char *const buf, int buf_len);


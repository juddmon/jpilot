/*
	Header for Russian library
	Convert Palm  <-> Unix:
		Palm : koi8
		Unix : Win1251
*/

void koi8_to_win1251(unsigned char *const buf, int buf_len);
void win1251_to_koi8(unsigned char *const buf, int buf_len);

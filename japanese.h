/*
	Header for Japanization library
	Convert Palm <-> Unix Japanese Code, ie:
		Palm : SJIS
		Unix : EUC
*/

void Sjis2Euc(unsigned char *buf, int max_len);
void Euc2Sjis(unsigned char *buf, int max_len);
void Sjis2Euc(unsigned char *buf, int max_len);

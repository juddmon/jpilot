/*
	Header for Japanization library
	Convert Palm <-> Unix Japanese Code, ie:
		Palm : SJIS
		Unix : EUC
*/

void Sjis2Euc(char *buf, int max_len);
void Euc2Sjis(char *buf, int max_len);
void jp_Sjis2Euc(char *buf, int max_len);

/*
	Japanization library
	Convert Palm <-> Unix Japanese Code, ie:
		Palm : SJIS
		Unix : EUC
*/

#include "config.h"

#if defined(WITH_JAPANESE)

#include <stdio.h>

#define isSjis1stByte(c) \
    (((c) >= 0x81 && (c) <= 0x9f) || ((c) >= 0xe0))

static unsigned int CharSjis2Jis(unsigned int code)
{
    unsigned char sh,sl,jh,jl;

    sh = (code >> 8) & 0xff;
    sl = code & 0xff;

    if (sl <= 0x9e){
	if (sh <= 0x9f){
	    jh = (sh - 0x71) * 2 + 1;
	} else {
	    jh = (sh - 0xb1) * 2 + 1;
	}
	jl = sl - 0x1f;
	if (sl >= 0x80)
	    jl--;
    } else {
	if (sh <= 0x9f){
	    jh = (sh - 0x70) * 2;
	} else {
	    jh = (sh - 0xB0) * 2;
	}
	jl = sl - 0x7e;
    }
    return (jh << 8) + jl;
}

void Sjis2Euc(unsigned char *buf, int max_len)
{
    unsigned char *p, *q;
    int half_byte = 0;
    int word16, first_byte = 0;
    int n;

    if (buf == NULL)
	return;
    for (p = buf, q = buf, n = 0; *p && n < max_len; p++, n++) {
	if (half_byte) {
	    word16 = CharSjis2Jis((first_byte << 8) | *p) | 0x8080; /* to EUC */
	    half_byte = 0;
	    *q++ = word16 >> 8;
	    *q++ = word16 & 0xff;
	} else {
	    if (isSjis1stByte(*p)) {
		/* shift JIS kanji first byte */
		half_byte = 1;
		first_byte = *p;
	    } else {
		*q++ = *p;
	    }
	}
    }
    *q++ = '\0';
}

static unsigned int CharEuc2Sjis(unsigned int code)
{
    register unsigned char jh, jl, sh, sl;

    jh = (code >> 8) & 0x7f;
    jl = code & 0x7f;
    if (jh <= 0x5e) {
	sh = (jh - 1) / 2 + 0x71;
    } else {
	sh = (jh - 1) / 2 + 0xb1;
    }
    if (jh & 1) {
	if ((sl = jl + 0x1f) >= 0x7f)
	    sl++;
    } else {
	sl = jl + 0x7e;
    }
    return (sh << 8) + sl;
}

void Euc2Sjis(unsigned char *buf, int max_len)
{
    unsigned char *p = buf, *q = buf;
    unsigned int code, cl, ch;
    int n;

    if (buf == NULL)
	return;
    n = 0;
    while (*p && n < max_len) {
	if ((ch = *p++) & 0x80) {
	    cl = *p++;
	    code = (ch << 8) + cl;
	    code = CharEuc2Sjis(code);
	    *q++ = (code >> 8) & 0xff;
	    *q++ = code & 0xff;
	    n += 2;
	} else {
	    *q++ = ch;
	    n++;
	}
    }
}
#endif

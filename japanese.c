/*
	Japanization library
	Convert Palm <-> Unix Japanese Code, ie:
		Palm : SJIS
		Unix : EUC
*/

#include "config.h"
#include "japanese.h"

#include <stdlib.h>
#include <stdio.h>
 
/* In utils.c, also a prototype in utils.h */
void multibyte_safe_strncpy(char *dst, char *src, size_t max_len);

#define isSjis1stByte(c) \
    (((c) >= 0x81 && (c) <= 0x9f) || ((c) >= 0xe0))
#define isSjisKana(c) \
    (0xa0 <= (c) && (c) <= 0xdf)
#define euc_kana (0x8e)
#define isEucKana(c) ((c) == euc_kana)
#define isEuc(c) \
    (0xa0 < ((unsigned char) (c)) && ((unsigned char) (c)) < 0xff)

/* convert SJIS char to EUC char 

   this does not support Machine dependent codes.
   args: hi: first byte of sjis char.
         lo: second byte of sjis char.
   return:   euc char in 16bit value. 

 */
static unsigned int SjisToEuc(unsigned char hi, unsigned char lo)
{
  if (lo >= 0x9f)
    return ((hi * 2 - (hi >= 0xe0 ? 0xe0 : 0x60)) << 8) | (lo + 2);
  else
    return ((hi * 2 - (hi >= 0xe0 ? 0xe1 : 0x61)) << 8) |
            (lo + (lo >= 0x7f ? 0x60 : 0x61));
}

/* 
   args: source char pointer, destination source pointer, a length of srting
   Length include null termination.  
   return the pointer of nul termination code.
 */
unsigned char *Sjis2EucCpy(unsigned char *dest, unsigned char *src, int max_len)
{
    unsigned char *p, *q;
    unsigned char hi, lo;
    unsigned int w;
    int n = 0;

    p = src;
    q = dest;
    while ((*p) && (n < max_len-2)) {
        if (isSjis1stByte(*p)) {
  	    hi = *p++;
	    lo = *p++;
	    w = SjisToEuc(hi, lo);
	    *q++ = (w >> 8) & 0xff;
	    *q++ = w & 0xff;
	    n += 2;
	} else if (isSjisKana(*p)) {                /* sjis(1byte) -> euc(2byte) */
	    *q++ = (unsigned char)euc_kana;
	    *q++ = *p++;
	    n += 2;
	} else if ((*p) & 0x80) {	            /* irregular japanese char */
	    p++;                                    /* ??abort and return NULL?? */
	    /* discard it */ 
	} else {
	    *q++ = *p++;
	    n++;
	}
    }
    if ((*p) && !(*p & 0x80) && (n < max_len-1)) {
	    *q++ = *p++;
	    *q = '\0';
    } else {
	    *q = '\0';
    }
    return q;
}

/* 
   convert strings from Sjis to EUC.
   max_len includes null termiantion.
   size of buf must be more than max_len.

*/

void Sjis2Euc(unsigned char *buf, int max_len)
{
	unsigned char *dst;

	if (buf == NULL) return;
	if ((dst = (unsigned char *)malloc(max_len)) != NULL) { 
                            /* assign buffer for destination. */
		if (Sjis2EucCpy(dst, buf, max_len) != NULL) { 
			multibyte_safe_strncpy(buf, dst, max_len);
			buf[max_len-1] = '\0';  /* i am a paranoire B-) */
		}
		free(dst);
	}
}

/*
   Convert one char from euc to sjis.
   args:  hi:  first byte of euc code.
          lo:  second byte of euc code.
   return:  16bit value of sjis char code. 
 */
static unsigned int EucToSjis(unsigned char hi, unsigned char lo)
{
  if (hi & 1)
    return ((hi / 2 + (hi < 0xdf ? 0x31 : 0x71)) << 8) |
            (lo - (lo >= 0xe0 ? 0x60 : 0x61));
  else
    return ((hi / 2 + (hi < 0xdf ? 0x30 : 0x70)) << 8) | (lo - 2);
}

/* 
   Convert string from euc to sjis with coping to another buffer. 
   Theoritically, strlen(EUC) >= strlen(SJIS), 
    then it is ok that dest == src.
 */
unsigned char *Euc2SjisCpy(unsigned char *dest, unsigned char *src, int max_len)
{
    unsigned char *p, *q; 
    unsigned char hi, lo;
    unsigned int w;
    int n = 0;

    p = src;   
    q = dest;
    while ((*p) && (n < max_len-2)) {
	if (isEucKana(*p)) {      /* euc kana(2byte) -> sjis(1byte) */
	    p++;
	    *q++ = *p++;
	    n++;
	} else if (isEuc(*p) && isEuc(*(p+1))) {
   	    hi = *p++;
	    lo = *p++;
	    w = EucToSjis(hi, lo);
	    *q++ = (w >> 8) & 0xff;
	    *q++ = w & 0xff;
	    n += 2;
	} else {                  /* ascii or irregular japanese char */
	    *q++ = *p++;
	    n++;
	}
    }
    if ((*p) && !(*p & 0x80) && n < max_len-1) {
	    *q++ = *p++;
	    *q = '\0';
    } else {
	    *q = '\0';
    }
    return dest;
}
/*
  convert euc to sjis.
  size of buf must be more than man_len.
  it simply call Euc2SjisCpy().
  this function exists for symmetry.
 */

void Euc2Sjis(unsigned char *buf, int max_len)
{
	if (buf == NULL) return;
	if (max_len <= 0) return;
	(void *)Euc2SjisCpy(buf, buf, max_len);
}

/*
 * Modern Greek library
 * Convert Palm  <-> Unix:
 * Palm : CP 1253
 * Unix : UTF-8
 */

#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "cp1253.h"
#include "log.h"


/********** Unix: UTF-8 **************************************************/

const unsigned char* WinGR2UTFMap[128] = { 
        /* 80 */
    "\xE2\x82\xAC", "\xCA\x81", "\xE2\x80\x9A", "\xC6\x92",
    "\xE2\x80\x9E", "\xE2\x80\xA6", "\xE2\x80\xA0", "\xE2\x80\xA1",
    "\xCA\x88", "\xE2\x80\xB0", "\xCA\x8A", "\xE2\x80\xB9",
    "\xCA\x8C", "\xCA\x8D", "\xCA\x8E", "\xCA\x8F",
        /* 90 */
    "\xCA\x90", "\xE2\x80\x98", "\xE2\x80\x99", "\xE2\x80\x9C",
    "\xE2\x80\x9D", "\xE2\x80\xA2", "\xE2\x80\x93", "\xE2\x80\x94",
    "\xCA\x98", "\xE2\x84\xA2", "\xCA\x9A", "\xE2\x80\xBA",
    "\xCA\x9C", "\xCA\x9D", "\xCA\x9E", "\xCA\x9F",
        /* A0 */
    "\xEF\xBF\xBD", "\xCE\x85", "\xCE\x86", "\xEF\xBF\xBD",
    "\xEF\xBF\xBD", "\xEF\xBF\xBD", "\xEF\xBF\xBD", "\xEF\xBF\xBD",
    "\xEF\xBF\xBD", "\xEF\xBF\xBD", "\xCA\xAA", "\xEF\xBF\xBD",
    "\xEF\xBF\xBD", "\xEF\xBF\xBD", "\xEF\xBF\xBD", "\xE2\x80\x95",
        /* B0 */
    "\xEF\xBF\xBD", "\xEF\xBF\xBD", "\xEF\xBF\xBD", "\xEF\xBF\xBD",
    "\xCE\x84", "\xEF\xBF\xBD", "\xEF\xBF\xBD", "\xEF\xBF\xBD",
    "\xCE\x88", "\xCE\x89", "\xCE\x8A", "\xEF\xBF\xBD",
    "\xCE\x8C", "\xEF\xBF\xBD", "\xCE\x8E", "\xCE\x8F",
        /* C0 */
    "\xCE\x90", "\xCE\x91", "\xCE\x92", "\xCE\x93",
    "\xCE\x94", "\xCE\x95", "\xCE\x96", "\xCE\x97",
    "\xCE\x98", "\xCE\x99", "\xCE\x9A", "\xCE\x9B",
    "\xCE\x9C", "\xCE\x9D", "\xCE\x9E", "\xCE\x9F",
        /* D0 */
    "\xCE\xA0", "\xCE\xA1", "\xCB\x92", "\xCE\xA3",
    "\xCE\xA4", "\xCE\xA5", "\xCE\xA6", "\xCE\xA7",
    "\xCE\xA8", "\xCE\xA9", "\xCE\xAA", "\xCE\xAB",
    "\xCE\xAC", "\xCE\xAD", "\xCE\xAE", "\xCE\xAF",
        /* E0 */
    "\xCE\xB0", "\xCE\xB1", "\xCE\xB2", "\xCE\xB3",
    "\xCE\xB4", "\xCE\xB5", "\xCE\xB6", "\xCE\xB7",
    "\xCE\xB8", "\xCE\xB9", "\xCE\xBA", "\xCE\xBB",
    "\xCE\xBC", "\xCE\xBD", "\xCE\xBE", "\xCE\xBF",
        /* F0 */
    "\xCF\x80", "\xCF\x81", "\xCF\x82", "\xCF\x83",
    "\xCF\x84", "\xCF\x85", "\xCF\x86", "\xCF\x87",
    "\xCF\x88", "\xCF\x89", "\xCF\x8A", "\xCF\x8B",
    "\xCF\x8C", "\xCF\x8D", "\xCF\x8E", "\xCB\xBF"
};


/*
 * compute size of buffer needed to store UTF8 text
 * when converted from Win CP 1253
 */

int WinGR2UTFsz(const unsigned char *buf, int buf_len)
   {
   int i;
   const char *p;
   int sz;

   sz = 0;
   for (i=0, p=buf; (i<buf_len) && *p; i++, p++)
      {
      if (*p & 0x80) sz += strlen(WinGR2UTFMap[*p & 0x7f]);
      else sz++;
      }
   return (sz + 1); /* do not forget ending */
   }

/*
 *           Conversion from CP1253 to UTF
 *     A new buffer is now allocated and the old one remains unchanged
 */

unsigned char *win1253_to_UTF(const unsigned char *buf, int buf_len)
{
   unsigned char *newbuf; /* new buffer allocated */
   int newlen; /* new buffer size */
   const unsigned char *p; /* pointer to source location */
   unsigned char *q; /* pointer to destination */
  const unsigned char* u; /* pointer to UTF8 character to be put in destination */
  int i, l; /* conter of remaining bytes available in destination buffer, UTF8 character's length */
  int stop = 0; /* stop flag (no space for UTF8 multibyte character in destination) */

   if (buf && (buf_len > 0)) {
      newlen = WinGR2UTFsz(buf, buf_len);
      newbuf = (unsigned char*)malloc(newlen);
      if (newbuf) {

         jp_logf(JP_LOG_DEBUG, "win1253_to_UTF: converting   [%s]\n", buf);

         for (i = newlen - 1, p = buf, q = newbuf; *p && i > 0 && !stop; p++) 
           {
              if (!(*p & 0x80))        /* this charaster is ascii */
                {
                   *q = *p; 
                   q++;
                   i--;
                }
              else
                {
                   u = WinGR2UTFMap[*p & 0x7f]; /* Get the lower 7 bits */
                   l = strlen(u);
                   stop = l > i;
                   if (!stop)
                     {
                        strcpy(q, u);
                        q += l;
                        i -= l;
                     }
                }
           }
         *q = '\0';

         jp_logf(JP_LOG_DEBUG, "win1253_to_UTF: converted to [%s]\n", newbuf);
      }
   }
   else newbuf = (unsigned char*)NULL;
   return (newbuf);
}

void UTF_to_win1253(unsigned char *const buf, int buf_len)
{
  unsigned char *p, *q; /* pointers to actual position in source and destination buffers */
  unsigned char bufU[8]; /* buffer for UTF8 character read from source */
  int i, l; /* conter of remaining bytes available in destination buffer, UTF8 character's length */

  if (buf == NULL) return;
  if (buf_len <= 0) return;

  jp_logf(JP_LOG_DEBUG, "UTF_to_win1253: converting   [%s]\n", buf);

  for (i = buf_len - 1, p = q = buf; *p && i > 0;) 
    {
      if (!(*p & 0x80))
        {
          *q = *p; 
          p++;
          q++;
          i--;
        }
      else
        { 
          l = 0;
          bufU[l++] = *p++;
            while (*p >> 6 == 2) /* 2nd-6th bytes in UTF8 character have the most significant bits set to "10" */
            {
              if (l < 7)
                bufU[l] = *p;
              l++;
              p++;
            }
          bufU[l > 7 ? 7 : l] = '\0';

          if (l > 1 && l < 7 && bufU[0] >> 6 == 3) /* the 1st byte in UTF8 character has the most significant bits set to "11" */
            {
              *q = '\0'; /* mark that character has not been converted yet */

              switch (strlen(bufU)) {
              case 2: 
                switch (bufU[0]) {
                case 0xC6 : 
                  switch (bufU[1]) { 
                  case 0x92 : (*q) = 0x83; break;
                  }
                  break;
                case 0xCA : 
                  switch (bufU[1]) { 
                  case 0x81 : (*q) = 0x81; break;
                  case 0x88 : (*q) = 0x88; break;
                  case 0x8A : (*q) = 0x8A; break;
                  case 0x8C : (*q) = 0x8C; break;
                  case 0x8D : (*q) = 0x8D; break;
                  case 0x8E : (*q) = 0x8E; break;
                  case 0x8F : (*q) = 0x8F; break;
                  case 0x90 : (*q) = 0x90; break;
                  case 0x98 : (*q) = 0x98; break;
                  case 0x9A : (*q) = 0x9A; break;
                  case 0x9C : (*q) = 0x9C; break;
                  case 0x9D : (*q) = 0x9D; break;
                  case 0x9E : (*q) = 0x9E; break;
                  case 0x9F : (*q) = 0x9F; break;
                  case 0xAA : (*q) = 0xAA; break;
                  }
                  break;
                case 0xCB : 
                  switch (bufU[1]) { 
                  case 0x92 : (*q) = 0xD2; break;
                  case 0xBF : (*q) = 0xFF; break;
                  }
                  break;
                case 0xCE : 
                  switch (bufU[1]) { 
                  case 0x84 : (*q) = 0xB4; break;
                  case 0x85 : (*q) = 0xA1; break;
                  case 0x86 : (*q) = 0xA2; break;
                  case 0x88 : (*q) = 0xB8; break;
                  case 0x89 : (*q) = 0xB9; break;
                  case 0x8A : (*q) = 0xBA; break;
                  case 0x8C : (*q) = 0xBC; break;
                  case 0x8E : (*q) = 0xBE; break;
                  case 0x8F : (*q) = 0xBF; break;
                  case 0x90 : (*q) = 0xC0; break;
                  case 0x91 : (*q) = 0xC1; break;
                  case 0x92 : (*q) = 0xC2; break;
                  case 0x93 : (*q) = 0xC3; break;
                  case 0x94 : (*q) = 0xC4; break;
                  case 0x95 : (*q) = 0xC5; break;
                  case 0x96 : (*q) = 0xC6; break;
                  case 0x97 : (*q) = 0xC7; break;
                  case 0x98 : (*q) = 0xC8; break;
                  case 0x99 : (*q) = 0xC9; break;
                  case 0x9A : (*q) = 0xCA; break;
                  case 0x9B : (*q) = 0xCB; break;
                  case 0x9C : (*q) = 0xCC; break;
                  case 0x9D : (*q) = 0xCD; break;
                  case 0x9E : (*q) = 0xCE; break;
                  case 0x9F : (*q) = 0xCF; break;
                  case 0xA0 : (*q) = 0xD0; break;
                  case 0xA1 : (*q) = 0xD1; break;
                  case 0xA3 : (*q) = 0xD3; break;
                  case 0xA4 : (*q) = 0xD4; break;
                  case 0xA5 : (*q) = 0xD5; break;
                  case 0xA6 : (*q) = 0xD6; break;
                  case 0xA7 : (*q) = 0xD7; break;
                  case 0xA8 : (*q) = 0xD8; break;
                  case 0xA9 : (*q) = 0xD9; break;
                  case 0xAA : (*q) = 0xDA; break;
                  case 0xAB : (*q) = 0xDB; break;
                  case 0xAC : (*q) = 0xDC; break;
                  case 0xAD : (*q) = 0xDD; break;
                  case 0xAE : (*q) = 0xDE; break;
                  case 0xAF : (*q) = 0xDF; break;
                  case 0xB0 : (*q) = 0xE0; break;
                  case 0xB1 : (*q) = 0xE1; break;
                  case 0xB2 : (*q) = 0xE2; break;
                  case 0xB3 : (*q) = 0xE3; break;
                  case 0xB4 : (*q) = 0xE4; break;
                  case 0xB5 : (*q) = 0xE5; break;
                  case 0xB6 : (*q) = 0xE6; break;
                  case 0xB7 : (*q) = 0xE7; break;
                  case 0xB8 : (*q) = 0xE8; break;
                  case 0xB9 : (*q) = 0xE9; break;
                  case 0xBA : (*q) = 0xEA; break;
                  case 0xBB : (*q) = 0xEB; break;
                  case 0xBC : (*q) = 0xEC; break;
                  case 0xBD : (*q) = 0xED; break;
                  case 0xBE : (*q) = 0xEE; break;
                  case 0xBF : (*q) = 0xEF; break;
                  }
                case 0xCF : 
                  switch (bufU[1]) { 
                  case 0x80 : (*q) = 0xF0; break;
                  case 0x81 : (*q) = 0xF1; break;
                  case 0x82 : (*q) = 0xF2; break;
                  case 0x83 : (*q) = 0xF3; break;
                  case 0x84 : (*q) = 0xF4; break;
                  case 0x85 : (*q) = 0xF5; break;
                  case 0x86 : (*q) = 0xF6; break;
                  case 0x87 : (*q) = 0xF7; break;
                  case 0x88 : (*q) = 0xF8; break;
                  case 0x89 : (*q) = 0xF9; break;
                  case 0x8A : (*q) = 0xFA; break;
                  case 0x8B : (*q) = 0xFB; break;
                  case 0x8C : (*q) = 0xFC; break;
                  case 0x8D : (*q) = 0xFD; break;
                  case 0x8E : (*q) = 0xFE; break;
                  }
                  break;
                }
                break;
              case 3:
                switch (bufU[0]) {
                case 0xE2 : 
                  switch (bufU[1]) { 
                  case 0x80 : 
                    switch (bufU[2]) { 
                    case 0x93 : (*q) = 0x96; break;
                    case 0x94 : (*q) = 0x97; break;
                    case 0x95 : (*q) = 0xAF; break;
                    case 0x98 : (*q) = 0x91; break;
                    case 0x99 : (*q) = 0x92; break;
                    case 0x9A : (*q) = 0x82; break;
                    case 0x9C : (*q) = 0x93; break;
                    case 0x9D : (*q) = 0x94; break;
                    case 0x9E : (*q) = 0x84; break;
                    case 0xA0 : (*q) = 0x86; break;
                    case 0xA1 : (*q) = 0x87; break;
                    case 0xA2 : (*q) = 0x95; break;
                    case 0xA6 : (*q) = 0x85; break;
                    case 0xB0 : (*q) = 0x89; break;
                    case 0xB9 : (*q) = 0x8B; break;
                    case 0xBA : (*q) = 0x9B; break;
                    }
                    break;
                  case 0x82 : 
                    switch (bufU[2]) { 
                    case 0xAC : (*q) = 0x80; break;
                    }
                    break;
                  case 0x84 : 
                    switch (bufU[2]) { 
                    case 0xA2 : (*q) = 0x99; break;
                    }
                    break;
                  }
                case 0xEF :
                  switch (bufU[1]) {
                  case 0xBF :
                    switch (bufU[2]) {
                    case 0xBD : (*q) = 0xA0; break;
                    }
                    break;
                  }
                  break;
                }
                break;
              }
        
              if (*q)
                {
                  q++;
                  i--;
                }
              else
                {
                  jp_logf(JP_LOG_WARN, "UTF_to_win1253: UTF character [%s] absent in CP1250 - dropped\n", bufU);
                }
            }
          else
            {
              jp_logf(JP_LOG_WARN, "UTF_to_win1253: non-UTF character [%s] - dropped\n", bufU);
            }
        }
    }
  *q = '\0';

  if (*p)
    {
      jp_logf(JP_LOG_WARN, "UTF_to_win1253: buffer too small - string had to be truncated to [%s]\n", buf);
    }

  jp_logf(JP_LOG_DEBUG, "UTF_to_win1253: converted to [%s]\n", buf);
}

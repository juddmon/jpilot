/*
 * Czech, Polish (and other CP 1250 languages) library
 * Convert Palm  <-> Unix:
 * Palm : CP 1250
 * Unix : ISO-8859-2
 *   and
 * Palm : CP 1250
 * Unix : UTF-8
 */

#include "config.h"
#include <stdio.h>
#include <string.h>
#include "cp1250.h"
#include "log.h"

/********** Unix: ISO **************************************************/

const unsigned char w2l[128] = {
   0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0xa9, 0x8b,
   0xa6, 0xab, 0xae, 0xac, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
   0x98, 0x99, 0xb9, 0x9b, 0xb6, 0xbb, 0xbe, 0xbc, 0xa0, 0xb7, 0xa2, 0xa3,
   0xa4, 0xa1, 0x8c, 0xa7, 0xa8, 0x8a, 0xaa, 0x8d, 0x8f, 0xad, 0x8e, 0xaf,
   0xb0, 0x9a, 0xb2, 0xb3, 0xb4, 0x9e, 0x9c, 0x9f, 0xb8, 0xb1, 0xba, 0x9d,
   0xa5, 0xbd, 0xb5, 0xbf, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
   0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0, 0xd1, 0xd2, 0xd3,
   0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
   0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb,
   0xec, 0xed, 0xee, 0xef, 0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
   0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

const unsigned char l2w[128] = {
   0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0xa9, 0x8b,
   0xa6, 0xab, 0xae, 0xac, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
   0x98, 0x99, 0xb1, 0x9b, 0xb6, 0xbb, 0xb5, 0xb7, 0xa0, 0xa5, 0xa2, 0xa3,
   0xa4, 0xbc, 0x8c, 0xa7, 0xa8, 0x8a, 0xaa, 0x8d, 0x8f, 0xad, 0x8e, 0xaf,
   0xb0, 0xb9, 0xb2, 0xb3, 0xb4, 0xbe, 0x9c, 0xa1, 0xb8, 0x9a, 0xba, 0x9d,
   0x9f, 0xbd, 0x9e, 0xbf, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
   0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0, 0xd1, 0xd2, 0xd3,
   0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf,
   0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb,
   0xec, 0xed, 0xee, 0xef, 0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7,
   0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff
};

#define isCZ(c) ((c) & 0x80)

void Win2Lat(unsigned char *const buf, int buf_len)
{
   unsigned char *p;
   int i;

   if (buf == NULL) return;

   for (i=0, p = buf; *p && i < buf_len; p++, i++) {
      if (isCZ(*p)) {
	 *p = w2l[(*p) & 0x7f];
      }
   }
}

void Lat2Win(unsigned char *const buf, int buf_len)
{
   unsigned char *p;
   int i;

   if (buf == NULL) return;

   for (i=0, p = buf; *p && i < buf_len; p++, i++) {
      if (isCZ(*p)) {
	 *p = l2w[(*p) & 0x7f];
      }
   }
}

/********** Unix: UTF-8 **************************************************/

const unsigned char* Win2UtfMap[128] = {
  "\xE2\x82\xAC",   "\xC2\x81",       "\xE2\x80\x9A",   "\xC2\x83",       
  "\xE2\x80\x9E",   "\xE2\x80\xA6",   "\xE2\x80\xA0",   "\xE2\x80\xA1",   
  "\xC2\x88",       "\xE2\x80\xB0",   "\xC5\xA0",       "\xE2\x80\xB9",   
  "\xC5\x9A",       "\xC5\xA4",       "\xC5\xBD",       "\xC5\xB9",       
  "\xC2\x90",       "\xE2\x80\x98",   "\xE2\x80\x99",   "\xE2\x80\x9C",   
  "\xE2\x80\x9D",   "\xE2\x80\xA2",   "\xE2\x80\x93",   "\xE2\x80\x94",   
  "\xC2\x98",       "\xE2\x84\xA2",   "\xC5\xA1",       "\xE2\x80\xBA",   
  "\xC5\x9B",       "\xC5\xA5",       "\xC5\xBE",       "\xC5\xBA",       
  "\xC2\xA0",       "\xCB\x87",       "\xCB\x98",       "\xC5\x81",       
  "\xC2\xA4",       "\xC4\x84",       "\xC2\xA6",       "\xC2\xA7",       
  "\xC2\xA8",       "\xC2\xA9",       "\xC5\x9E",       "\xC2\xAB",       
  "\xC2\xAC",       "\xC2\xAD",       "\xC2\xAE",       "\xC5\xBB",       
  "\xC2\xB0",       "\xC2\xB1",       "\xCB\x9B",       "\xC5\x82",       
  "\xC2\xB4",       "\xC2\xB5",       "\xC2\xB6",       "\xC2\xB7",       
  "\xC2\xB8",       "\xC4\x85",       "\xC5\x9F",       "\xC2\xBB",       
  "\xC4\xBD",       "\xCB\x9D",       "\xC4\xBE",       "\xC5\xBC",       
  "\xC5\x94",       "\xC3\x81",       "\xC3\x82",       "\xC4\x82",       
  "\xC3\x84",       "\xC4\xB9",       "\xC4\x86",       "\xC3\x87",       
  "\xC4\x8C",       "\xC3\x89",       "\xC4\x98",       "\xC3\x8B",       
  "\xC4\x9A",       "\xC3\x8D",       "\xC3\x8E",       "\xC4\x8E",       
  "\xC4\x90",       "\xC5\x83",       "\xC5\x87",       "\xC3\x93",       
  "\xC3\x94",       "\xC5\x90",       "\xC3\x96",       "\xC3\x97",       
  "\xC5\x98",       "\xC5\xAE",       "\xC3\x9A",       "\xC5\xB0",       
  "\xC3\x9C",       "\xC3\x9D",       "\xC5\xA2",       "\xC3\x9F",       
  "\xC5\x95",       "\xC3\xA1",       "\xC3\xA2",       "\xC4\x83",       
  "\xC3\xA4",       "\xC4\xBA",       "\xC4\x87",       "\xC3\xA7",       
  "\xC4\x8D",       "\xC3\xA9",       "\xC4\x99",       "\xC3\xAB",       
  "\xC4\x9B",       "\xC3\xAD",       "\xC3\xAE",       "\xC4\x8F",       
  "\xC4\x91",       "\xC5\x84",       "\xC5\x88",       "\xC3\xB3",       
  "\xC3\xB4",       "\xC5\x91",       "\xC3\xB6",       "\xC3\xB7",       
  "\xC5\x99",       "\xC5\xAF",       "\xC3\xBA",       "\xC5\xB1",       
  "\xC3\xBC",       "\xC3\xBD",       "\xC5\xA3",       "\xCB\x99"
};


void Win2UTF(unsigned char *const buf, int buf_len)
{
  unsigned char bufTMP[0xFFFF]; /* temporary destination buffer */
  unsigned char *p, *q; /* pointers to actual position in source and destination buffers */
  const unsigned char* u; /* pointer to UTF8 character to be put in destination */
  int i, l; /* conter of remaining bytes avaliable in destination buffer, UTF8 character's length */
  int stop = 0; /* stop flag (no space for UTF8 multibyte character in destination) */

  if (buf == NULL) return;
  if (buf_len <= 0) return;
  if (buf_len > 0xFFFF) buf_len = 0xFFFF;

  jp_logf(JP_LOG_DEBUG, "Win2UTF: converting   [%s]\n", buf);

  for (i = buf_len - 1, p = buf, q = bufTMP; *p && i > 0 && !stop; p++) 
    {
      if (!(*p & 0x80))
	{
	  *q = *p; 
	  q++;
	  i--;
	}
      else
	{
	  u = Win2UtfMap[*p & 0x7f];
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

  if (*p || stop)
    {
      jp_logf(JP_LOG_WARN, "Win2UTF: buffer too small - string had to be truncated to [%s]\n", bufTMP);
    }

  jp_logf(JP_LOG_DEBUG, "Win2UTF: converted to [%s]\n", bufTMP);

  strcpy(buf, bufTMP);
}


void UTF2Win(unsigned char *const buf, int buf_len)
{
  unsigned char bufTMP[0xFFFF]; /* temporary destination buffer */
  unsigned char *p, *q; /* pointers to actual position in source and destination buffers */
  unsigned char bufU[8]; /* buffer for UTF8 character read from source */
  int i, l; /* conter of remaining bytes avaliable in destination buffer, UTF8 character's length */

  if (buf == NULL) return;
  if (buf_len <= 0) return;
  if (buf_len > 0xFFFF) buf_len = 0xFFFF;

  jp_logf(JP_LOG_DEBUG, "UTF2Win: converting   [%s]\n", buf);

  for (i = buf_len - 1, p = buf, q = bufTMP; *p && i > 0;) 
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
		case 0xC2 : 
		  switch (bufU[1]) { 
		  case 0x81 : (*q) = 129; break;
		  case 0x83 : (*q) = 131; break;
		  case 0x88 : (*q) = 136; break;
		  case 0x90 : (*q) = 144; break;
		  case 0x98 : (*q) = 152; break;
		  case 0xA0 : (*q) = 160; break;
		  case 0xA4 : (*q) = 164; break;
		  case 0xA6 : (*q) = 166; break;
		  case 0xA7 : (*q) = 167; break;
		  case 0xA8 : (*q) = 168; break;
		  case 0xA9 : (*q) = 169; break;
		  case 0xAB : (*q) = 171; break;
		  case 0xAC : (*q) = 172; break;
		  case 0xAD : (*q) = 173; break;
		  case 0xAE : (*q) = 174; break;
		  case 0xB0 : (*q) = 176; break;
		  case 0xB1 : (*q) = 177; break;
		  case 0xB4 : (*q) = 180; break;
		  case 0xB5 : (*q) = 181; break;
		  case 0xB6 : (*q) = 182; break;
		  case 0xB7 : (*q) = 183; break;
		  case 0xB8 : (*q) = 184; break;
		  case 0xBB : (*q) = 187; break;
		  }
		  break;
		case 0xC3 : 
		  switch (bufU[1]) { 
		  case 0x81 : (*q) = 193; break;
		  case 0x82 : (*q) = 194; break;
		  case 0x84 : (*q) = 196; break;
		  case 0x87 : (*q) = 199; break;
		  case 0x89 : (*q) = 201; break;
		  case 0x8B : (*q) = 203; break;
		  case 0x8D : (*q) = 205; break;
		  case 0x8E : (*q) = 206; break;
		  case 0x93 : (*q) = 211; break;
		  case 0x94 : (*q) = 212; break;
		  case 0x96 : (*q) = 214; break;
		  case 0x97 : (*q) = 215; break;
		  case 0x9A : (*q) = 218; break;
		  case 0x9C : (*q) = 220; break;
		  case 0x9D : (*q) = 221; break;
		  case 0x9F : (*q) = 223; break;
		  case 0xA1 : (*q) = 225; break;
		  case 0xA2 : (*q) = 226; break;
		  case 0xA4 : (*q) = 228; break;
		  case 0xA7 : (*q) = 231; break;
		  case 0xA9 : (*q) = 233; break;
		  case 0xAB : (*q) = 235; break;
		  case 0xAD : (*q) = 237; break;
		  case 0xAE : (*q) = 238; break;
		  case 0xB3 : (*q) = 243; break;
		  case 0xB4 : (*q) = 244; break;
		  case 0xB6 : (*q) = 246; break;
		  case 0xB7 : (*q) = 247; break;
		  case 0xBA : (*q) = 250; break;
		  case 0xBC : (*q) = 252; break;
		  case 0xBD : (*q) = 253; break;
		  }
		  break;
		case 0xC4 : 
		  switch (bufU[1]) { 
		  case 0x82 : (*q) = 195; break;
		  case 0x83 : (*q) = 227; break;
		  case 0x84 : (*q) = 165; break;
		  case 0x85 : (*q) = 185; break;
		  case 0x86 : (*q) = 198; break;
		  case 0x87 : (*q) = 230; break;
		  case 0x8C : (*q) = 200; break;
		  case 0x8D : (*q) = 232; break;
		  case 0x8E : (*q) = 207; break;
		  case 0x8F : (*q) = 239; break;
		  case 0x90 : (*q) = 208; break;
		  case 0x91 : (*q) = 240; break;
		  case 0x98 : (*q) = 202; break;
		  case 0x99 : (*q) = 234; break;
		  case 0x9A : (*q) = 204; break;
		  case 0x9B : (*q) = 236; break;
		  case 0xB9 : (*q) = 197; break;
		  case 0xBA : (*q) = 229; break;
		  case 0xBD : (*q) = 188; break;
		  case 0xBE : (*q) = 190; break;
		  }
		  break;
		case 0xC5 : 
		  switch (bufU[1]) { 
		  case 0x81 : (*q) = 163; break;
		  case 0x82 : (*q) = 179; break;
		  case 0x83 : (*q) = 209; break;
		  case 0x84 : (*q) = 241; break;
		  case 0x87 : (*q) = 210; break;
		  case 0x88 : (*q) = 242; break;
		  case 0x90 : (*q) = 213; break;
		  case 0x91 : (*q) = 245; break;
		  case 0x94 : (*q) = 192; break;
		  case 0x95 : (*q) = 224; break;
		  case 0x98 : (*q) = 216; break;
		  case 0x99 : (*q) = 248; break;
		  case 0x9A : (*q) = 140; break;
		  case 0x9B : (*q) = 156; break;
		  case 0x9E : (*q) = 170; break;
		  case 0x9F : (*q) = 186; break;
		  case 0xA0 : (*q) = 138; break;
		  case 0xA1 : (*q) = 154; break;
		  case 0xA2 : (*q) = 222; break;
		  case 0xA3 : (*q) = 254; break;
		  case 0xA4 : (*q) = 141; break;
		  case 0xA5 : (*q) = 157; break;
		  case 0xAE : (*q) = 217; break;
		  case 0xAF : (*q) = 249; break;
		  case 0xB0 : (*q) = 219; break;
		  case 0xB1 : (*q) = 251; break;
		  case 0xB9 : (*q) = 143; break;
		  case 0xBA : (*q) = 159; break;
		  case 0xBB : (*q) = 175; break;
		  case 0xBC : (*q) = 191; break;
		  case 0xBD : (*q) = 142; break;
		  case 0xBE : (*q) = 158; break;
		  }
		  break;
		case 0xCB : 
		  switch (bufU[1]) { 
		  case 0x87 : (*q) = 161; break;
		  case 0x98 : (*q) = 162; break;
		  case 0x99 : (*q) = 255; break;
		  case 0x9B : (*q) = 178; break;
		  case 0x9D : (*q) = 189; break;
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
		    case 0x93 : (*q) = 150; break;
		    case 0x94 : (*q) = 151; break;
		    case 0x98 : (*q) = 145; break;
		    case 0x99 : (*q) = 146; break;
		    case 0x9A : (*q) = 130; break;
		    case 0x9C : (*q) = 147; break;
		    case 0x9D : (*q) = 148; break;
		    case 0x9E : (*q) = 132; break;
		    case 0xA0 : (*q) = 134; break;
		    case 0xA1 : (*q) = 135; break;
		    case 0xA2 : (*q) = 149; break;
		    case 0xA6 : (*q) = 133; break;
		    case 0xB0 : (*q) = 137; break;
		    case 0xB9 : (*q) = 139; break;
		    case 0xBA : (*q) = 155; break;
		    }
		    break;
		  case 0x82 : 
		    switch (bufU[2]) { 
		    case 0xAC : (*q) = 128; break;
		    }
		    break;
		  case 0x84 : 
		    switch (bufU[2]) { 
		    case 0xA2 : (*q) = 153; break;
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
		  jp_logf(JP_LOG_WARN, "UTF2Win: UTF character [%s] absent in CP1250 - dropped\n", bufU);
		}
	    }
	  else
	    {
	      jp_logf(JP_LOG_WARN, "UTF2Win: non-UTF character [%s] - dropped\n", bufU);
	    }
	}
    }
  *q = '\0';

  if (*p)
    {
      jp_logf(JP_LOG_WARN, "UTF2Win: buffer too small - string had to be truncated to [%s]\n", bufTMP);
    }

  jp_logf(JP_LOG_DEBUG, "UTF2Win: converted to [%s]\n", bufTMP);

  strcpy(buf, bufTMP);
}

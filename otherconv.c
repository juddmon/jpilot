/* $Id: otherconv.c,v 1.46 2010/10/15 16:42:13 rikster5 Exp $ */

/*******************************************************************************
 * otherconv.c
 * A module of J-Pilot http://jpilot.org
 *
 * Copyright (C) 2004 by Amit Aronovitch 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 ******************************************************************************/

/*
 * General charset conversion library (using gconv)
 * Convert Palm  <-> Unix:
 * Palm : Any - according to the "other-pda-charset" setup option.
 * Unix : UTF-8
 */

/********************************* Includes ***********************************/
#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <glib.h>

#include "otherconv.h"
#include "i18n.h"
#include "prefs.h"
#include "log.h"

/********************************* Constants **********************************/
#define HOST_CS "UTF-8"

#define min(a,b) (((a) < (b)) ? (a) : (b))

/* You can't do #ifndef __FUNCTION__ */
#if !defined(__GNUC__) && !defined(__IBMC__)
#define __FUNCTION__ ""
#endif

#define OC_FREE_ICONV(conv) oc_free_iconv(__FUNCTION__, conv,#conv)

/* #define OTHERCONV_DEBUG */

/******************************* Global vars **********************************/
static GIConv glob_topda = NULL;
static GIConv glob_frompda = NULL;

/****************************** Main Code *************************************/
/*
 * strnlen is not ANSI.
 * To avoid conflicting declarations, it is reimplemented as a thin
 * inline function over the library function strlen
 */
static inline size_t oc_strnlen(const char *s, size_t maxlen) 
{
   return min(strlen(s), maxlen); 
}

static void oc_free_iconv(const char *funcname, GIConv conv, char *convname) 
{
   if (conv != NULL) {
      if (g_iconv_close(conv) != 0) {
         jp_logf(JP_LOG_WARN, _("%s: error exit from g_iconv_close(%s)\n"),
            funcname,convname);
      }
   }
}

/*
 * Convert char_set integer code to iconv charset text string
 */
static char *char_set_to_text(int char_set)
{
   switch (char_set) {
    case CHAR_SET_1250_UTF:
      return "CP1250";

    case CHAR_SET_1253_UTF:
      return "CP1253";

    case CHAR_SET_ISO8859_2_UTF:
      return "ISO8859-2";

    case CHAR_SET_KOI8_R_UTF:
      return "KOI8-R";

    case CHAR_SET_1251_UTF:
      return "CP1251";

    case CHAR_SET_GBK_UTF:
      return "GBK";

    case CHAR_SET_BIG5_UTF:
      return "BIG-5";

    case CHAR_SET_SJIS_UTF:
      return "SJIS";

    case CHAR_SET_1255_UTF:
      return "CP1255";

    case CHAR_SET_949_UTF:
      return "CP949";

    case CHAR_SET_1252_UTF:
    default:
      return "CP1252";
   }
}

/*
 * Module initialization function
 *  Call this before any conversion routine.
 *  Can also be used if you want to reread the 'charset' option
 *
 * Returns 0 if OK, -1 if iconv could not be initialized
 *  (probably because of bad charset string)
 */
int otherconv_init(void) 
{
   long char_set;
 
   get_pref(PREF_CHAR_SET, &char_set, NULL);
 
   /* (re)open the "to" iconv */
   OC_FREE_ICONV(glob_topda);
   glob_topda = g_iconv_open(char_set_to_text(char_set), HOST_CS);
   if (glob_topda == (GIConv)(-1))
      return EXIT_FAILURE;
 
   /* (re)open the "from" iconv */
   OC_FREE_ICONV(glob_frompda);
   glob_frompda = g_iconv_open(HOST_CS, char_set_to_text(char_set));
   if (glob_frompda == (GIConv)(-1)) {
      OC_FREE_ICONV(glob_topda);
      return EXIT_FAILURE;
   }
   return EXIT_SUCCESS;
}

/*
 * Module finalization function
 * closes the iconvs
 */
void otherconv_free(void) 
{
   OC_FREE_ICONV(glob_topda);
   OC_FREE_ICONV(glob_frompda);
}

/*
 *           Conversion to UTF using g_convert_with_iconv
 *     A new buffer is now allocated and the old one remains unchanged
 */
char *other_to_UTF(const char *buf, int buf_len)
{
   size_t rc;
   char *outbuf;
   gsize bytes_read;
   GError *err = NULL;
   size_t str_len;
 
#ifdef OTHERCONV_DEBUG
   jp_logf(JP_LOG_DEBUG, "%s:%s reset iconv state...\n", __FILE__, __FUNCTION__);
#endif
   rc = g_iconv(glob_frompda, NULL, NULL, NULL, NULL);
#ifdef OTHERCONV_DEBUG
   jp_logf(JP_LOG_DEBUG, "%s:%s converting   [%s]\n", __FILE__, __FUNCTION__, buf);
#endif
 
   if (buf_len == -1) {
      str_len = -1;
   } else {
      str_len = oc_strnlen(buf, buf_len-1);  // -1 leaves room for \0 terminator
   }
 
   outbuf = (char *)g_convert_with_iconv((gchar *)buf,
            str_len, glob_frompda, &bytes_read, NULL, &err);
 
   if (err != NULL) {
      char c;
      char *head, *tail;
      int  outbuf_len;
      char *tmp_buf = (char *)buf;
      static int call_depth = 0;
      printf("ERROR HAPPENED\n");
      if (0 == call_depth)
         jp_logf(JP_LOG_WARN, _("%s:%s g_convert_with_iconv error: %s, buff: %s\n"),
                              __FILE__, __FUNCTION__, 
                              err ? err->message : _("last char truncated"),
                              buf);
      if (err != NULL)
         g_error_free(err);
      else
         g_free(outbuf);
 
      if (buf_len == -1) {
         buf_len = strlen(buf); 
      }
 
      /* convert the head, skip the problematic char, convert the tail */
      c = tmp_buf[bytes_read];
      tmp_buf[bytes_read] = '\0';
      head = g_convert_with_iconv(tmp_buf, 
                                  oc_strnlen(tmp_buf, buf_len),
                                  glob_frompda, 
                                  &bytes_read, 
                                  NULL, NULL);
      tmp_buf[bytes_read] = c;
 
      call_depth++;
      tail = other_to_UTF(tmp_buf + bytes_read +1, buf_len - bytes_read - 1);
      call_depth--;
 
      outbuf_len = strlen(head) +4 + strlen(tail)+1;
      outbuf = g_malloc(outbuf_len);
      g_snprintf(outbuf, outbuf_len, "%s\\%02X%s", head, (unsigned char)c, tail);
 
      g_free(head);
      g_free(tail);
   }
 
#ifdef OTHERCONV_DEBUG
   jp_logf(JP_LOG_DEBUG, "%s:%s converted to [%s]\n", __FILE__, __FUNCTION__, outbuf);
#endif
   /*
    * Note: outbuf was allocated by glib, so should be freed with g_free
    * To be 100% safe, I should have done strncpy to a new malloc-allocated 
    * string. (at least under an 'if (!g_mem_is_system_malloc())' test)
    *
    * However, unless you replace the default GMemVTable, freeing with C free
    * should be fine so I decided this is not worth the overhead.
    * -- Amit Aronovitch
    */
   return outbuf;
}

/*
 *           Conversion to pda encoding using g_iconv
 */
void UTF_to_other(char *const buf, int buf_len)
{
   gsize inleft,outleft;
   gchar *inptr, *outptr;
   size_t rc;
   char *errstr = NULL;
   char buf_out[1000];
   char *buf_out_ptr = NULL;
   int failed = FALSE;

#ifdef OTHERCONV_DEBUG
   jp_logf(JP_LOG_DEBUG, "%s:%s reset iconv state...\n", __FILE__, __FUNCTION__);
#endif
   rc = g_iconv(glob_topda, NULL, NULL, NULL, NULL);
#ifdef OTHERCONV_DEBUG
   jp_logf(JP_LOG_DEBUG, "%s:%s converting   [%s]\n", __FILE__, __FUNCTION__, buf);
#endif

   inleft = oc_strnlen(buf, buf_len);
   outleft = buf_len-1;
   inptr = buf;

  /* Most strings can be converted without recourse to malloc */
   if (buf_len > sizeof(buf_out)) {
      buf_out_ptr = malloc(buf_len);
      if (NULL == buf_out_ptr) {
         jp_logf(JP_LOG_WARN, _("UTF_to_other: %s\n"), _("Out of memory"));
         return;
      }
      outptr = buf_out_ptr;
   } else {
      outptr = buf_out;
   }

   rc = g_iconv(glob_topda, &inptr, &inleft, &outptr, &outleft);
   *outptr = 0; /* NULL terminate whatever fraction was converted */
 
   if ((size_t)(-1) == rc) {
      switch (errno) {
       case EILSEQ:
         errstr = _("iconv: unconvertible sequence at place %d in \'%s\'\n");
         failed = TRUE;
         break;
       case EINVAL:
         errstr = _("iconv: incomplete UTF-8 sequence at place %d in \'%s\'\n");
         break;
       case E2BIG:
         errstr = _("iconv: buffer filled. stopped at place %d in \'%s\'\n");
        break;
       default:
         errstr = _("iconv: unexpected error at place %d in \'%s\'\n");
      }
   }

   if (buf_out_ptr) {
      g_strlcpy(buf, buf_out_ptr, buf_len);
      free(buf_out_ptr);
   } else {
      g_strlcpy(buf, buf_out, buf_len);
   }

   if ((size_t)(-1) == rc)
      jp_logf(JP_LOG_WARN, errstr, inptr - buf, buf);

   if (failed)
   {
      /* convert the end of the string */
      int l = inptr - buf;

      buf[l] = '?';
      UTF_to_other(inptr+1, buf_len-l-1);
      memmove(buf+l+1, inptr+1, buf_len-l-1);
   }

#ifdef OTHERCONV_DEBUG
   jp_logf(JP_LOG_DEBUG, "%s:%s converted to [%s]\n", __FILE__, __FUNCTION__, buf);
#endif
}


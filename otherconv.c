/*
 * General charset conversion library (using gconv)
 * Convert Palm  <-> Unix:
 * Palm : Any - according to the "other-pda-charset" setup option.
 * Unix : UTF-8
 */


#include "config.h"

#ifdef ENABLE_GTK2
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <glib.h>
/* To speed up compilation use these instead of glib.h
#include <glib/gmacros.h>
#include <glib/gconvert.h>
*/
#include "prefs.h"
#include "otherconv.h"
#include "log.h"

static GIConv glob_topda = NULL;
static GIConv glob_frompda = NULL;

#define HOST_CS "UTF-8//IGNORE"

/* You can't do #ifndef __FUNCTION__ */
#if !defined(__GNUC__) && !defined(__IBMC__)
#define __FUNCTION__ ""
#endif


/*
 * strnlen is not ANSI.
 * To avoid messing with conflicting declarations, I just implement my own version.
 * (this is easy & portable might not be very efficient) -- Amit Aronovitch
 */
G_INLINE_FUNC size_t oc_strnlen(const unsigned char *s, size_t maxlen) {
  const unsigned char *p,*endp;

  endp = s+maxlen;
  for (p=s;p<endp;++p) if (! *p) break;
  return p-s;
}

void oc_free_iconv(char *funcname, GIConv conv, char *convname) {
  if (conv != NULL) {
    if (g_iconv_close(conv) != 0) {
      jp_logf(JP_LOG_WARN, "%s: error exit from g_iconv_close(%s)\n",
	      funcname,convname);
    }
  }
}

#define OC_FREE_ICONV(conv) oc_free_iconv(__FUNCTION__, conv,#conv)

/*
 * Convert char_set integer code to iconv charset text string
 */
char *char_set_to_text(int char_set)
{
   static char text_char_set[100];

   switch (char_set)
   {
      case CHAR_SET_1250_UTF:
	 sprintf(text_char_set, "CP1250");
	 break;

      case CHAR_SET_1253_UTF:
	 sprintf(text_char_set, "CP1253");
	 break;

      case CHAR_SET_ISO8859_2_UTF:
	 sprintf(text_char_set, "ISO8859-2");
	 break;

      case CHAR_SET_KOI8_R_UTF:
	 sprintf(text_char_set, "KOI8-R");
	 break;

      case CHAR_SET_1251_UTF:
	 sprintf(text_char_set, "CP1251");
	 break;

      case CHAR_SET_GB2312_UTF:
	 sprintf(text_char_set, "GB2312");
	 break;

      case CHAR_SET_SJIS_UTF:
	 sprintf(text_char_set, "SJIS");
	 break;

      case CHAR_SET_1255_UTF:
	 sprintf(text_char_set, "CP1255");
	 break;

      case CHAR_SET_1252_UTF:
      default:
	 sprintf(text_char_set, "CP1252");
   }

   return text_char_set;
}

/*
 * Module initialization function
 *  Call this before any conversion routine.
 *  Can also be used if you want to reread the 'charset' option
 *
 * Returns 0 if OK, -1 if iconv could not be initialized
 *  (probably because of bad charset string)
 */
int otherconv_init() {
  long char_set;

  get_pref(PREF_CHAR_SET, &char_set, NULL);

  /* (re)open the "to" iconv */
  OC_FREE_ICONV(glob_topda);
  glob_topda = g_iconv_open(char_set_to_text(char_set), HOST_CS);
  if (glob_topda == (GIConv)(-1))
     return -1;

  /* (re)open the "from" iconv */
  OC_FREE_ICONV(glob_frompda);
  glob_frompda = g_iconv_open(HOST_CS, char_set_to_text(char_set));
  if (glob_frompda == (GIConv)(-1)) {
    OC_FREE_ICONV(glob_topda);
    return -1;
  }
  return 0;
}

/*
 * Module finalization function
 * closes the iconvs
 */
void otherconv_free() {
  OC_FREE_ICONV(glob_topda);
  OC_FREE_ICONV(glob_frompda);
}

/*
 *           Conversion to UTF using g_convert_with_iconv
 *     A new buffer is now allocated and the old one remains unchanged
 */
unsigned char *other_to_UTF(const unsigned char *buf, int buf_len)
{
  size_t rc;
  char *outbuf;
  gsize bytes_read;
  GError *err = NULL;

  jp_logf(JP_LOG_DEBUG, "%s:%s reset iconv state...\n", __FILE__, __FUNCTION__);
  rc = g_iconv(glob_frompda, NULL, NULL, NULL, NULL);
  jp_logf(JP_LOG_DEBUG, "%s:%s converting   [%s]\n", __FILE__, __FUNCTION__,
     buf);

  outbuf = (char *)g_convert_with_iconv((gchar *)buf, oc_strnlen(buf, buf_len),
     glob_frompda, &bytes_read, NULL, &err);
  if (err != NULL) {
      jp_logf(JP_LOG_WARN, "%s:%s g_convert_with_iconv error: %s, buff: %s\n",
	      __FILE__, __FUNCTION__, err->message, buf);
      g_error_free(err);

      /* return the unconverted text */
      outbuf = buf;
  }

  jp_logf(JP_LOG_DEBUG, "%s:%s converted to [%s]\n", __FILE__, __FUNCTION__,
	outbuf);

  /*
   * Note: outbuf was allocated by glib, so should be freed with g_free
   * To be 100% safe, I should have done strncpy to a new malloc-allocated string.
   * (at least under an 'if (!g_mem_is_system_malloc())' test)
   *
   * However, unless you replace the default GMemVTable, freeing with C free should be fine
   *  so I decided this is not worth the overhead  -- Amit Aronovitch
   */
  return outbuf;
}

/*
 *           Conversion to pda encoding using g_iconv
 *     The conversion is performed inplace
 *
 *  Note: this should work only as long as output is guarenteed to be shorter
 *  than input - otherwise iconv might do unexpected stuff.
 */
void UTF_to_other(unsigned char *const buf, int buf_len)
{
  gsize inleft,outleft;
  gchar *inptr, *outptr;
  size_t rc;
  char *errstr;

  jp_logf(JP_LOG_DEBUG, "%s:%s reset iconv state...\n", __FILE__, __FUNCTION__);
  rc = g_iconv(glob_topda, NULL, NULL, NULL, NULL);
  jp_logf(JP_LOG_DEBUG, "%s:%s converting   [%s]\n", __FILE__, __FUNCTION__,
     buf);

  inleft = oc_strnlen(buf,buf_len);
  outleft = buf_len-1;
  inptr = outptr = buf;

  rc = g_iconv(glob_topda, &inptr, &inleft, &outptr, &outleft);
  *outptr = 0;
  if (rc<0) {
    switch (errno) {
    case EILSEQ:
      errstr = "iconv: unconvertable sequence at place %d\n";
      break;
    case EINVAL:
      errstr = "iconv: incomplete UTF-8 sequence at place %d\n";
      break;
    case E2BIG:
      errstr = "iconv: buffer filled. stopped at place %d\n";
      break;
    default:
      errstr = "iconv: unexpected error at place %d\n";
    }
    jp_logf(JP_LOG_WARN, errstr, ((unsigned char *)inptr)-buf);
  }

  jp_logf(JP_LOG_DEBUG, "%s:%s converted to [%s]\n", __FILE__, __FUNCTION__,
     buf);
}

#else

unsigned char *other_to_UTF(const unsigned char *buf, int buf_len)
{
	return (unsigned char*)strdup(buf);
}

void UTF_to_other(unsigned char *const buf, int buf_len)
{
}

#endif


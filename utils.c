/* $Id: utils.c,v 1.105 2005/11/30 01:44:30 rikster5 Exp $ */

/*******************************************************************************
 * utils.c
 * A module of J-Pilot http://jpilot.org
 *
 * Copyright (C) 1999-2002 by Judd Montgomery
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

#include "config.h"
#include "i18n.h"
#include "utils.h"
#include "log.h"
#include "prefs.h"
#include "sync.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pi-datebook.h>
#include <pi-address.h>
#include <sys/types.h>
#include <sys/wait.h>
/*#include <unistd.h> */
#include <utime.h>
#include <time.h>
#include <errno.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdk.h>
#include "plugins.h"
#include "libplugin.h"
#include "otherconv.h"

#include <pi-source.h>
#include <pi-socket.h>
#include <pi-dlp.h>
#include <pi-file.h>

/*
 * For versioning of files
 */
#define FILE_VERSION     "version"
#define FILE_VERSION2    "version2"
#define FILE_VERSION2_CR "version2\n"

/*Stuff for the dialog window */
extern GtkWidget *glob_dialog;
int dialog_result;

unsigned int glob_find_id;

/*Stuff for the calendar window */
static int glob_cal_return_code;
static int glob_cal_mon, glob_cal_day, glob_cal_year;

/*
 * Returns usage string that needs to be freed by the caller
 */
void fprint_usage_string(FILE *out)
{
   fprintf(out, "%s [-v] || [-h] || [-d] || [-a] || [-A] || [-i]\n", EPN);
   fprintf(out, "%s", _(" -v displays version and compile options and exits.\n"));
   fprintf(out, "%s", _(" -h displays help and exits.\n"));
   fprintf(out, "%s", _(" -d displays debug info to stdout.\n"));
   fprintf(out, "%s", _(" -p skips loading plugins.\n"));
   fprintf(out, "%s", _(" -a ignores missed alarms since the last time program was run.\n"));
   fprintf(out, "%s", _(" -A ignores all alarms past and future.\n"));
   fprintf(out, "%s", _(" -i makes program iconify itself upon launch.\n"));
   fprintf(out, "%s", _(" The PILOTPORT, and PILOTRATE env variables are used to specify\n"));
   fprintf(out, "%s", _(" which port to sync on, and at what speed.\n"));
   fprintf(out, "%s", _(" If PILOTPORT is not set then it defaults to /dev/pilot.\n"));
}

void get_compile_options(char *string, int len)
{
   g_snprintf(string, len,
	      PN" version "VERSION"\n"
	      "  Copyright (C) 1999-2005 by Judd Montgomery\n"
	      "  judd@jpilot.org, http://jpilot.org\n"
	      "\n"
	      PN" comes with ABSOLUTELY NO WARRANTY; for details see the file\n"
	      "COPYING included with the source code, or in /usr/share/docs/jpilot/.\n\n"
	      "This program is free software; you can redistribute it and/or modify\n"
	      "it under the terms of the GNU General Public License as published by\n"
	      "the Free Software Foundation; version 2 of the License.\n\n"
	      "%s %s %s\n"
	      "%s\n"
	      "  %s - %s\n"
	      "  %s - %d.%d.%d\n"
	      "  %s - %s\n"
	      "  %s - %s\n"
	      "  %s - %s\n"
	      "  %s - %s\n"
	      "  %s - %s\n"
	      "  %s - %s\n"
	      "  %s - %s",
	      _("Date compiled"), __DATE__, __TIME__,
	      _("Compiled with these options:"),

	      _("Installed Path"),
	      BASE_DIR,
	      _("pilot-link version"),
	      PILOT_LINK_VERSION,
	      PILOT_LINK_MAJOR,
	      PILOT_LINK_MINOR,
	      _("USB support"),
#ifdef USB_PILOT_LINK
	      _("yes"),
#else
	      _("no"),
#endif
	      _("Private record support"),
#ifdef ENABLE_PRIVATE
	      _("yes"),
#else
	      _("no"),
#endif
	      _("Datebk support"),
#ifdef ENABLE_DATEBK
	      _("yes"),
#else
	      _("no"),
#endif
	      _("Plugin support"),
#ifdef ENABLE_PLUGINS
	      _("yes"),
#else
	      _("no"),
#endif
	      _("Manana support"),
#ifdef ENABLE_MANANA
	      _("yes"),
#else
	      _("no"),
#endif
	      _("NLS support (foreign languages)"),
#ifdef ENABLE_NLS
	      _("yes"),
#else
	      _("no"),
#endif
	      _("GTK2 support"),
#ifdef ENABLE_GTK2
	      _("yes")
#else
	      _("no")
#endif
	      );
}

int cat_compare(const void *v1, const void *v2)
{
   struct sorted_cats *s1, *s2;
   s1=(struct sorted_cats *)v1; s2=(struct sorted_cats *)v2;
   if ((s1)->Pcat[0]=='\0') {
      return 1;
   }
   if ((s2)->Pcat[0]=='\0') {
      return -1;
   }
   return strcmp((s1)->Pcat, (s2)->Pcat);
}

gint timeout_date(gpointer data)
{
   extern GtkWidget *glob_date_label;
   char str[102];
   char datef[102];
   const char *svalue1, *svalue2;
   time_t ltime;
   struct tm *now;

   if (glob_date_label==NULL) {
      return FALSE;
   }
   time(&ltime);
   now = localtime(&ltime);


   /* Build a long date string */
   get_pref(PREF_LONGDATE, NULL, &svalue1);
   get_pref(PREF_TIME, NULL, &svalue2);

   if ((svalue1==NULL)||(svalue2==NULL)) {
      strcpy(datef, _("Today is %A, %x %X"));
   } else {
      sprintf(datef, _("Today is %%A, %s %s"), svalue1, svalue2);
   }
   jp_strftime(str, 100, datef, now);
   str[100]='\0';

   gtk_label_set_text(GTK_LABEL(glob_date_label), str);
   return TRUE;
}
/*
 * This is a slow algorithm, but its not used much
 */
int add_days_to_date(struct tm *date, int n)
{
   int ndim;
   int fdom;
   int flag;
   int i;

   get_month_info(date->tm_mon, 1, date->tm_year, &fdom, &ndim);
   for (i=0; i<n; i++) {
      flag = 0;
      if (++(date->tm_mday) > ndim) {
	 date->tm_mday=1;
	 flag = 1;
	 if (++(date->tm_mon) > 11) {
	    date->tm_mon=0;
	    flag = 1;
	    if (++(date->tm_year)>137) {
	       date->tm_year = 137;
	    }
	 }
      }
      if (flag) {
	 get_month_info(date->tm_mon, 1, date->tm_year, &fdom, &ndim);
      }
   }
   date->tm_isdst=-1;
   mktime(date);

   return EXIT_SUCCESS;
}

/*
 * This is a slow algorithm, but its not used much
 */
int sub_days_from_date(struct tm *date, int n)
{
   int ndim;
   int fdom;
   int flag;
   int reset_days;
   int i;

   get_month_info(date->tm_mon, 1, date->tm_year, &fdom, &ndim);
   for (i=0; i<n; i++) {
      flag = reset_days = 0;
      if (--(date->tm_mday) < 1) {
	 date->tm_mday=28;
	 reset_days = 1;
	 flag = 1;
	 if (--(date->tm_mon) < 0) {
	    date->tm_mon=11;
	    flag = 1;
	    if (--(date->tm_year)<3) {
	       date->tm_year = 3;
	    }
	 }
      }
      if (flag) {
	 get_month_info(date->tm_mon, 1, date->tm_year, &fdom, &ndim);
      }
      /* this assumes that flag is always set when reset_days is set */
      if (reset_days) {
	 date->tm_mday=ndim;
      }
   }
   date->tm_isdst=-1;
   mktime(date);
   return EXIT_SUCCESS;
}

/*
 * This function will increment the date by n number of months and
 * adjust the day to the last day of the month if it exceeds the number
 * of days in the new month
 */
int add_months_to_date(struct tm *date, int n)
{
   int i;
   int days_in_month[]={31,28,31,30,31,30,31,31,30,31,30,31
   };

   for (i=0; i<n; i++) {
      if (++(date->tm_mon) > 11) {
	 date->tm_mon=0;
	 if (++(date->tm_year)>137) {
	    date->tm_year = 137;
	 }
      }
   }

   if ((date->tm_year%4 == 0) &&
       !(((date->tm_year+1900)%100==0) && ((date->tm_year+1900)%400!=0))) {
      days_in_month[1]++;
   }

   if (date->tm_mday > days_in_month[date->tm_mon]) {
      date->tm_mday = days_in_month[date->tm_mon];
   }

   date->tm_isdst=-1;
   mktime(date);
   return EXIT_SUCCESS;
}

/*
 * This function will decrement the date by n number of months and
 * adjust the day to the last day of the month if it exceeds the number
 * of days in the new month
 */
int sub_months_from_date(struct tm *date, int n)
{
   int i;
   int days_in_month[]={31,28,31,30,31,30,31,31,30,31,30,31
   };

   for (i=0; i<n; i++) {
      if (--(date->tm_mon) < 0) {
	 date->tm_mon=11;
	 if (--(date->tm_year)<3) {
	    date->tm_year = 3;
	 }
      }
   }

   if ((date->tm_year%4 == 0) &&
       !(((date->tm_year+1900)%100==0) && ((date->tm_year+1900)%400!=0))) {
      days_in_month[1]++;
   }

   if (date->tm_mday > days_in_month[date->tm_mon]) {
      date->tm_mday = days_in_month[date->tm_mon];
   }

   date->tm_isdst=-1;
   mktime(date);
   return EXIT_SUCCESS;
}

/*
 * This function will increment the date by n number of years and
 * adjust feb 29th to feb 28th if its not a leap year
 */
static int add_or_sub_years_to_date(struct tm *date, int n)
{
   date->tm_year += n;

   if (date->tm_year>137) {
      date->tm_year = 137;
   }
   if (date->tm_year<3) {
      date->tm_year = 3;
   }
   /*Leap day/year */
   if ((date->tm_mon==1) && (date->tm_mday==29)) {
      if (!((date->tm_year%4 == 0) &&
	    !(((date->tm_year+1900)%100==0) && ((date->tm_year+1900)%400!=0)))) {
	 /* Move it back one day */
	 date->tm_mday=28;
      }
   }
   return EXIT_SUCCESS;
}

int add_years_to_date(struct tm *date, int n)
{
   return add_or_sub_years_to_date(date, n);
}

int sub_years_from_date(struct tm *date, int n)
{
   return add_or_sub_years_to_date(date, -n);
}

const char base64chars[] =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";

/* RFC 1341 et seq. */
void base64_out(FILE *f, char *str)
{
   unsigned char *p;
   int n;
   unsigned int val;
   int pad;
   int mask, shift;

   n = 0;
   val = 0;
   pad = 0;
   for (p = str; *p || n; *p ? p++ : 0) {
      if (*p == '\0' && pad == 0) {
	 pad = n;
      }
      val = (val << 8) + *p;
      n++;
      if (n == 3) {
	 mask = 0xfc0000;
	 shift = 18;
	 for (n = 0; n < 4; n++) {
	    if (pad && n > pad) {
	       fputc('=', f);
	    } else {
	       fputc(base64chars[(val & mask) >> shift], f);
	    }
	    mask >>= 6;
	    shift -= 6;
	 }
	 n = 0;
	 val = 0;
      }
   }
}

/* RFC 2849 */
void ldif_out(FILE *f, char *name, char *fmt, ...)
{
   va_list ap;
   unsigned char buf[8192];
   unsigned char buf2[2 * sizeof(buf)];
   char *p;
   int printable = 1;

   va_start(ap, fmt);
   vsnprintf(buf, sizeof(buf), fmt, ap);
   if (buf[0] == ' ' || buf[0] == ':' || buf[0] == '<')	/* SAFE-INIT-CHAR */ {
      printable = 0;
   }
   for (p = buf; *p && printable; p++) {
      if (*p < 32 || *p > 126) { /* SAFE-CHAR, excluding all control chars */
	 printable = 0;
      }
      if (*p == ' ' && *(p + 1) == '\0') { /* note 8 */
	 printable = 0;
      }
   }
   if (printable) {
      fprintf(f, "%s: %s\n", name, buf);
   } else {
      /*
       * Convert to UTF-8.
       * Assume the data on this end is in ISO-8859-1 for now, which
       * maps directly to a UCS character.  More complete character
       * set support on the j-pilot side will require a way to
       * translate to UCS and a more complete UCS->UTF8 converter.
       * TO DO: iconv() can do anything -> UTF-8
       * iconv_t foo = iconv_open("UTF-8", "ISO-8859-1");
       * iconv(foo, buf, &sizeof(buf), buf2, &sizeof(buf2));
       */
      unsigned char *p, *q;
      for (p = buf, q = buf2; *p; p++, q++) {
	 if (*p <= 127) {
	    *q = *p;
	 } else {
	    *q++ = 0xc0 | ((*p >> 6) & 0x03);
	    *q = 0x80 | (*p & 0x3f);
	 }
      }
      *q = '\0';
      fprintf(f, "%s:: ", name);
      base64_out(f, buf2);
      fprintf(f, "\n");
   }
}

/*
 * Quote for iCalendar (RFC 2445) or vCard (RFC 2426).
 * The only difference is that iCalendar also quotes semicolons.
 * Wrap at 60-ish characters.
 */
static int str_to_iv_str(char *dest, int destsz, char *src, int isical)
{
   int c;
   char *destend, *odest;

   if ((!src) || (!dest)) {
      return EXIT_SUCCESS;
   }
   odest = dest;
   destend = dest + destsz - 4;	/* max 4 chars into dest per loop iteration */
   c=0;
   while (*src) {
      if (dest >= destend) {
	 break;
      }
      if (c>60) {
	 *dest++='\n';
	 *dest++=' ';
	 c=0;
      }
      if (*src=='\n') {
	 *dest++='\\';
	 *dest++='n';
	 c+=2;
	 src++;
	 continue;
      }
      if (*src=='\\' || (isical && *src == ';') || *src == ',') {
	 *dest++='\\';
	 c++;
      }
      *dest++=*src++;
      c++;
   }
   *dest++='\0';
   return dest - odest;
}

/*
 * Quote a TEXT format string as specified by RFC 2445.
 * Wrap it at 60-ish characters.
 */
int str_to_ical_str(char *dest, int destsz, char *src)
{
   return str_to_iv_str(dest, destsz, src, 1);
}

/*
 * Quote a *TEXT-LIST-CHAR format string as specified by RFC 2426.
 * Wrap it at 60-ish characters.
 */
int str_to_vcard_str(char *dest, int destsz, char *src)
{
   return str_to_iv_str(dest, destsz, src, 0);
}

/*
 * Copy src string into dest while escaping quotes with double quotes.
 * dest could be as long as strlen(src)*2.
 * Return value is the number of chars written to dest.
 */
int str_to_csv_str(char *dest, char *src)
{
   int s, d;

   if (dest) dest[0]='\0';
   if ((!src) || (!dest)) {
      return EXIT_SUCCESS;
   }
   s=d=0;
   while (src[s]) {
      if (src[s]=='\"') {
	 dest[d++]='\"';
      }
      dest[d++]=src[s++];
   }
   dest[d++]='\0';
   return d;
}

/*
 * Parse the string and replace CR and LFs with spaces
 */
void remove_cr_lfs(char *str)
{
   int i;

   if (!str) {
      return;
   }
   for (i=0; str[i]; i++) {
      if ((str[i]=='\r') || (str[i]=='\n')) {
	 str[i]=' ';
      }
   }
}

/*
 * Parse the string and replace CR and LFs with spaces
 * a null is written if len is reached
 */
void lstrncpy_remove_cr_lfs(char *dest, char *src, int len)
{
   int i;
#ifdef ENABLE_GTK2
   gchar* end;
#endif

   if ((!src) || (!dest)) {
      return;
   }
   dest[0]='\0';
   for (i=0; src[i] && (i<len); i++) {
      if ((src[i]=='\r') || (src[i]=='\n')) {
	 dest[i]=' ';
      } else {
	 dest[i]=src[i];
      }
   }
   if (i==len) {
      dest[i-1]='\0';
   } else {
      dest[i]='\0';
   }

#ifdef ENABLE_GTK2
   /* truncate the string on an UTF-8 character boundary */
   if (!g_utf8_validate(dest, -1, (const gchar **)&end))
     *end = 0;
#endif
}

/*
 * This function just removes extra slashes from a string
 */
void cleanup_path(char *path)
{
   register int s, d; /* source and destination */

   if (!path) return;
   for (s=d=0; path[s]!='\0'; s++,d++) {
      if ((path[s]=='/') && (path[s+1]=='/')) {
	 d--;
	 continue;
      }
      if (d!=s) {
	 path[d]=path[s];
      }
   }
   path[d]='\0';
}

void free_search_record_list(struct search_record **sr)
{
   struct search_record *temp_sr, *temp_sr_next;

   for (temp_sr = *sr; temp_sr; temp_sr=temp_sr_next) {
      temp_sr_next = temp_sr->next;
      free(temp_sr);
   }
   *sr = NULL;
}

void set_bg_rgb_clist_row(GtkWidget *clist, int row, int r, int g, int b)
{
   GtkStyle *old_style, *new_style;
   GdkColor color;

   if ((old_style = gtk_widget_get_style(clist))) {
      new_style = gtk_style_copy(old_style);
   }
   else {
      new_style = gtk_style_new();
   }

   color.red=r;
   color.green=g;
   color.blue=b;

   new_style->base[GTK_STATE_NORMAL] = color;
   gtk_clist_set_row_style(GTK_CLIST(clist), row, new_style);
}

void set_fg_rgb_clist_cell(GtkWidget *clist, int row, int col, int r, int g, int b)
{
   GtkStyle *old_style, *new_style;
   GdkColor fg_color;

   if ((old_style = gtk_clist_get_row_style(GTK_CLIST(clist), row)) ||
       (old_style = gtk_widget_get_style(clist))) {
      new_style = gtk_style_copy(old_style);
   }
   else {
      new_style = gtk_style_new();
   }

   fg_color.red=r;
   fg_color.green=g;
   fg_color.blue=b;
   new_style->fg[GTK_STATE_NORMAL]   = fg_color;
   new_style->fg[GTK_STATE_SELECTED] = fg_color;
   gtk_clist_set_cell_style(GTK_CLIST(clist), row, col, new_style);
}

/*returns 0 if not found, 1 if found */
int clist_find_id(GtkWidget *clist,
		  unsigned int unique_id,
		  int *found_at)
{
   int i, found;
   MyAddress *maddr;

   *found_at = 0;

   for (found = i = 0; i<GTK_CLIST(clist)->rows; i++) {
      maddr = gtk_clist_get_row_data(GTK_CLIST(clist), i);
      if (maddr < (MyAddress *)CLIST_MIN_DATA) {
	 break;
      }
      if (maddr->unique_id==unique_id) {
	 found = TRUE;
	 *found_at = i;
         break;
      }
   }

   return found;
}

/* Encapsulate broken GTK function to make it work as documented */
void clist_select_row(GtkCList *clist, 
                      int       row,
		      int       column)
{
  clist->focus_row = row;
  gtk_clist_select_row(clist, row, column);
}


int get_pixmaps(GtkWidget *widget,
		int which_one,
		GdkPixmap **out_pixmap,
		GdkBitmap **out_mask)
{
/*Note pixmap */
char * xpm_note[] = {
   "11 16 3 1",
   "       c None",
   ".      c #000000000000",
   "X      c #cccccccccccc",
   "           ",
   " ......    ",
   " .XXX.X.   ",
   " .XXX.XX.  ",
   " .XXX.XXX. ",
   " .XXX..... ",
   " .XXXXXXX. ",
   " .XXXXXXX. ",
   " .XXXXXXX. ",
   " .XXXXXXX. ",
   " .XXXXXXX. ",
   " .XXXXXXX. ",
   " .XXXXXXX. ",
   " ......... ",
   "           ",
   "           "
};

/*Alarm pixmap */
char * xpm_alarm[] = {
   "16 16 3 1",
   "       c None",
   ".      c #000000000000",
   "X      c #cccccccccccc",
   "                ",
   "   .       .    ",
   "  ...     ...   ",
   "  ...........   ",
   "   .XXXXXXXX.   ",
   "  .XXXX.XXXXX.  ",
   " .XXXXX.XXXXXX. ",
   " .X.....XXXXXX. ",
   " .XXXXXXXXXXX.  ",
   "  .XXXXXXXXX.   ",
   "   .XXXXXXX.    ",
   "   .........    ",
   "   .       .    ",
   " ....     ....  ",
   "                ",
   "                "
};

char * xpm_check[] = {
   "12 16 3 1",
   "       c None",
   ".      c #000000000000",
   "X      c #cccccccccccc",
   "                ",
   " .........  ",
   " .XXXXXXX.  ",
   " .X     X.  ",
   " .X     X.  ",
   " .X     X.  ",
   " .X     X.  ",
   " .X     X.  ",
   " .X     X.  ",
   " .X     X.  ",
   " .X     X.  ",
   " .X     X.  ",
   " .XXXXXXX.  ",
   " .........  ",
   "            ",
   "            "
};

char * xpm_checked[] = {
   "12 16 4 1",
   "       c None",
   ".      c #000000000000",
   "X      c #cccccccccccc",
   "R      c #FFFF00000000",
   "            ",
   " .........  ",
   " .XXXXXXX.RR",
   " .X     XRR ",
   " .X     RR  ",
   " .X    RR.  ",
   " .X    RR.  ",
   " .X   RRX.  ",
   " RR  RR X.  ",
   " .RR RR X.  ",
   " .X RR  X.  ",
   " .X  R  X.  ",
   " .XXXXXXX.  ",
   " .........  ",
   "            ",
   "            "
};

char * xpm_float_check[] = {
   "14 16 4 1",
   "       c None",
   ".      c #000000000000",
   "X      c #CCCCCCCCCCCC",
   "W      c #FFFFFFFFFFFF",
   "              ",
   "     ....     ",
   "    ......    ",
   "   ..XXXX..   ",
   "  ..XWWWWX..  ",
   " ..XWWWWWWX.. ",
   " ..XWWWWWWX.. ",
   " ..XWWWWWWX.. ",
   " ..XWWWWWWX.. ",
   " ..XWWWWWWX.. ",
   "  ..XWWWWX..  ",
   "   ..XXXX..   ",
   "    ......    ",
   "     ....     ",
   "              ",
   "              "
};

char * xpm_float_checked[] = {
   "14 16 5 1",
   "       c None",
   ".      c #000000000000",
   "X      c #cccccccccccc",
   "R      c #FFFF00000000",
   "W      c #FFFFFFFFFFFF",
   "              ",
   "     ....     ",
   "    ...... RR ",
   "   ..XXXX.RR  ",
   "  ..XWWWWRR.  ",
   " ..XWWWWRRX.. ",
   " ..XWWWWRRX.. ",
   " ..XWWWRRWX.. ",
   " .RRWWRRWWX.. ",
   " ..RRWRRWWX.. ",
   "  ..XRRWWX..  ",
   "   ..XRXX..   ",
   "    ......    ",
   "     ....     ",
   "              ",
   "              "
};

   static int inited=0;
   static GdkPixmap *pixmap_note;
   static GdkPixmap *pixmap_alarm;
   static GdkPixmap *pixmap_check;
   static GdkPixmap *pixmap_checked;
   static GdkPixmap *pixmap_float_check;
   static GdkPixmap *pixmap_float_checked;
   static GdkBitmap *mask_note;
   static GdkBitmap *mask_alarm;
   static GdkBitmap *mask_check;
   static GdkBitmap *mask_checked;
   static GdkBitmap *mask_float_check;
   static GdkBitmap *mask_float_checked;
   GtkStyle *style;

   if (inited) {
      goto assign;
   }

   inited=1;

   /*Make the note pixmap */
   /*style = gtk_widget_get_style(window); */
   style = gtk_widget_get_style(widget);
   pixmap_note = gdk_pixmap_create_from_xpm_d(widget->window,  &mask_note,
					      &style->bg[GTK_STATE_NORMAL],
					      (gchar **)xpm_note);

   /*Make the alarm pixmap */
   pixmap_alarm = gdk_pixmap_create_from_xpm_d(widget->window,  &mask_alarm,
					       &style->bg[GTK_STATE_NORMAL],
					       (gchar **)xpm_alarm);

   /*Make the check pixmap */
   pixmap_check = gdk_pixmap_create_from_xpm_d(widget->window,  &mask_check,
					       &style->bg[GTK_STATE_NORMAL],
					       (gchar **)xpm_check);

   /*Make the checked pixmap */
   pixmap_checked = gdk_pixmap_create_from_xpm_d(widget->window,  &mask_checked,
					       &style->bg[GTK_STATE_NORMAL],
					       (gchar **)xpm_checked);

   /*Make the float_checked pixmap */
   pixmap_float_check = gdk_pixmap_create_from_xpm_d
     (widget->window,  &mask_float_check,
      &style->bg[GTK_STATE_NORMAL],
      (gchar **)xpm_float_check);

   /*Make the float_checked pixmap */
   pixmap_float_checked = gdk_pixmap_create_from_xpm_d
     (widget->window,  &mask_float_checked,
      &style->bg[GTK_STATE_NORMAL],
      (gchar **)xpm_float_checked);

   assign:
   switch (which_one) {
    case PIXMAP_NOTE:
      *out_pixmap = pixmap_note;
      *out_mask = mask_note;
      break;
    case PIXMAP_ALARM:
      *out_pixmap = pixmap_alarm;
      *out_mask = mask_alarm;
      break;
    case PIXMAP_BOX_CHECK:
      *out_pixmap = pixmap_check;
      *out_mask = mask_check;
      break;
    case PIXMAP_BOX_CHECKED:
      *out_pixmap = pixmap_checked;
      *out_mask = mask_checked;
      break;
    case PIXMAP_FLOAT_CHECK:
      *out_pixmap = pixmap_float_check;
      *out_mask = mask_float_check;
      break;
    case PIXMAP_FLOAT_CHECKED:
      *out_pixmap = pixmap_float_checked;
      *out_mask = mask_float_checked;
      break;
    default:
      *out_pixmap = NULL;
      *out_mask = NULL;
   }

   return EXIT_SUCCESS;
}

/*
 * This is a hack to add pixmaps in column titles.
 * Its a hack because it assumes things about GTK that are not exposed.
 */
int hack_clist_set_column_title_pixmap(GtkWidget *clist,
				       int column,
				       GtkWidget *pixmapwid)
{
   GtkWidget *old_widget;

   old_widget = GTK_BIN(GTK_CLIST(clist)->column[column].button)->child;
   if (old_widget) {
      gtk_container_remove(GTK_CONTAINER(GTK_CLIST(clist)->column[column].button), old_widget);
   }

   gtk_widget_show(pixmapwid);
   gtk_container_add(GTK_CONTAINER(GTK_CLIST(clist)->column[column].button), pixmapwid);

   return EXIT_SUCCESS;
}


/*
 * Start of GTK calendar code
 */
#define PRESSED_P            100
#define PRESSED_A            101
#define PRESSED_TAB_OR_MINUS 102

static GtkWidget *util_cal;
static GtkWidget *cal_window;

static void
cb_today(GtkWidget *widget,
	 gpointer   data)
{
   time_t ltime;
   struct tm *now;
   GtkWidget *cal;

   cal = data;
   time(&ltime);
   now = localtime(&ltime);

   gtk_calendar_select_month(GTK_CALENDAR(cal), now->tm_mon, now->tm_year+1900);
   gtk_calendar_select_day(GTK_CALENDAR(cal), now->tm_mday);
}

static gboolean cb_destroy(GtkWidget *widget)
{
   gtk_main_quit();
   return FALSE;
}

static void
cb_quit(GtkWidget *widget,
	gpointer   data)
{
   unsigned int y,m,d;

   glob_cal_return_code = GPOINTER_TO_INT(data);

   if (glob_cal_return_code==CAL_DONE) {
      gtk_calendar_get_date(GTK_CALENDAR(util_cal),&y,&m,&d);
      glob_cal_mon=m;
      glob_cal_day=d;
      glob_cal_year=y;
   }

   gtk_widget_destroy(cal_window);
}

/* mon 0-11
 * day 1-31
 * year (year - 1900)
 * This function will bring up the cal at mon, day, year
 * After a new date is selected it will return mon, day, year
 */
int cal_dialog(GtkWindow *main_window,
	       const char *title, int monday_is_fdow,
	       int *mon, int *day, int *year)
{
   GtkWidget *button;
   GtkWidget *vbox;
   GtkWidget *hbox;
   glob_cal_mon = *mon;
   glob_cal_day = *day;
   glob_cal_year = (*year) + 1900;

   cal_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
   gtk_window_set_title(GTK_WINDOW(cal_window), title);

   gtk_window_set_position(GTK_WINDOW(cal_window), GTK_WIN_POS_MOUSE);

   gtk_window_set_modal(GTK_WINDOW(cal_window), TRUE);

   gtk_window_set_transient_for(GTK_WINDOW(cal_window), GTK_WINDOW(main_window));

   gtk_signal_connect(GTK_OBJECT(cal_window), "destroy",
		      GTK_SIGNAL_FUNC(cb_destroy), cal_window);

   vbox = gtk_vbox_new(FALSE, 0);
   gtk_container_add(GTK_CONTAINER(cal_window), vbox);

   util_cal = gtk_calendar_new();
   gtk_box_pack_start(GTK_BOX(vbox), util_cal, TRUE, TRUE, 0);

   hbox = gtk_hbutton_box_new();
   gtk_container_set_border_width(GTK_CONTAINER(hbox), 12);
   gtk_button_box_set_layout(GTK_BUTTON_BOX (hbox), GTK_BUTTONBOX_END);
   gtk_button_box_set_spacing(GTK_BUTTON_BOX(hbox), 6);
   gtk_container_add(GTK_CONTAINER(vbox), hbox);

   gtk_calendar_display_options(GTK_CALENDAR(util_cal),
				GTK_CALENDAR_SHOW_HEADING |
				GTK_CALENDAR_SHOW_DAY_NAMES |
				/*GTK_CALENDAR_NO_MONTH_CHANGE |*/
				GTK_CALENDAR_SHOW_WEEK_NUMBERS |
				(monday_is_fdow ? GTK_CALENDAR_WEEK_START_MONDAY : 0));

   /* gtk_signal_connect(GTK_OBJECT(util_cal), "day_selected", cb_cal_sel, NULL); */
   gtk_signal_connect(GTK_OBJECT(util_cal), "day_selected_double_click", GTK_SIGNAL_FUNC(cb_quit),
		      GINT_TO_POINTER(CAL_DONE));

   /* gtk_calendar_mark_day(GTK_CALENDAR(util_cal), 23); */

   gtk_calendar_select_month(GTK_CALENDAR(util_cal), *mon, (*year)+1900);
   gtk_calendar_select_day(GTK_CALENDAR(util_cal), *day);


   /* Bottom Buttons */
#ifdef ENABLE_GTK2
   button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
#else
   button = gtk_button_new_with_label(_("Cancel"));
#endif
   gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
   gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(cb_quit),
		      GINT_TO_POINTER(CAL_CANCEL));

   button = gtk_button_new_with_label(_("Today"));
   gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_today), util_cal);

#ifdef ENABLE_GTK2
   button = gtk_button_new_from_stock(GTK_STOCK_OK);
#else
   button = gtk_button_new_with_label(_("OK"));
#endif
   gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
   gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(cb_quit),
		      GINT_TO_POINTER(CAL_DONE));

   gtk_widget_show_all(cal_window);

   gtk_main();

   if (glob_cal_return_code==CAL_DONE) {
      *mon = glob_cal_mon;
      *day = glob_cal_day;
      *year = glob_cal_year - 1900;
   }

   return glob_cal_return_code;
}
/*
 * End of GTK calendar code
 */

/* */
/*Start of Dialog window code */
/* */
void cb_dialog_button(GtkWidget *widget,
			gpointer   data)
{
   dialog_result=GPOINTER_TO_INT(data);

   gtk_widget_destroy(glob_dialog);
}

static gboolean cb_destroy_dialog(GtkWidget *widget)
{
   glob_dialog = NULL;
   gtk_main_quit();

   return FALSE;
}

/* nob = number of buttons */
int dialog_generic(GtkWindow *main_window,
			     char *title, int type,
			     char *text, int nob, char *button_text[])
{
   GtkWidget *button, *label1;
   GtkWidget *hbox1, *vbox1, *vbox2, *image;
   int i;
   char *markup;

   /* This gdk function call is required in order to avoid a GTK
    * error which causes X and the mouse pointer to lock up.
    * The lockup is generated whenever a modal dialog is created
    * from the callback routine of a clist. */
   gdk_pointer_ungrab(GDK_CURRENT_TIME);

   dialog_result=0;
   glob_dialog = gtk_widget_new(GTK_TYPE_WINDOW,
				"type", GTK_WINDOW_TOPLEVEL,
				"window_position", GTK_WIN_POS_MOUSE,
				NULL);

   gtk_window_set_title(GTK_WINDOW(glob_dialog), title);

   gtk_signal_connect(GTK_OBJECT(glob_dialog), "destroy",
                      GTK_SIGNAL_FUNC(cb_destroy_dialog), glob_dialog);

   gtk_window_set_modal(GTK_WINDOW(glob_dialog), TRUE);

   if (main_window) {
      gtk_window_set_transient_for(GTK_WINDOW(glob_dialog), GTK_WINDOW(main_window));
   }

   vbox1 = gtk_vbox_new(FALSE, 5);
   gtk_container_add(GTK_CONTAINER(glob_dialog), vbox1);

   hbox1 = gtk_hbox_new(FALSE, 2);
   gtk_container_add(GTK_CONTAINER(vbox1), hbox1);
   gtk_container_set_border_width(GTK_CONTAINER(hbox1), 12);
#ifdef ENABLE_GTK2
   switch(type)
   {
      case DIALOG_INFO:
	 image = gtk_image_new_from_stock(GTK_STOCK_DIALOG_INFO, GTK_ICON_SIZE_DIALOG);
	 break;
      case DIALOG_QUESTION:
	 image = gtk_image_new_from_stock(GTK_STOCK_DIALOG_QUESTION, GTK_ICON_SIZE_DIALOG);
	 break;
      case DIALOG_ERROR:
	 image = gtk_image_new_from_stock(GTK_STOCK_DIALOG_ERROR, GTK_ICON_SIZE_DIALOG);
	 break;
      case DIALOG_WARNING:
	 image = gtk_image_new_from_stock(GTK_STOCK_DIALOG_WARNING, GTK_ICON_SIZE_DIALOG);
	 break;
      default:
	 image = NULL;
   }
   if (image)
      gtk_box_pack_start(GTK_BOX(hbox1), image, FALSE, FALSE, 2);
#endif

   vbox2 = gtk_vbox_new(FALSE, 5);
   gtk_container_set_border_width(GTK_CONTAINER(vbox2), 5);
   gtk_box_pack_start(GTK_BOX(hbox1), vbox2, FALSE, FALSE, 2);

   label1 = gtk_label_new(NULL);
#ifdef ENABLE_GTK2
   markup = g_markup_printf_escaped("<b><big>%s</big></b>\n\n%s", title, text);
   gtk_label_set_markup(GTK_LABEL(label1), markup);
   g_free(markup);
#else
   gtk_window_set_title(GTK_WINDOW(glob_dialog), title);
   gtk_label_set_text(GTK_LABEL(label1), text);
   gtk_label_set_justify(GTK_LABEL(label1), GTK_JUSTIFY_LEFT);
#endif
   gtk_box_pack_start(GTK_BOX(vbox2), label1, FALSE, FALSE, 2);

   hbox1 = gtk_hbutton_box_new();
   gtk_container_set_border_width(GTK_CONTAINER(hbox1), 12);
   gtk_button_box_set_layout(GTK_BUTTON_BOX (hbox1), GTK_BUTTONBOX_END);
   gtk_button_box_set_spacing(GTK_BUTTON_BOX(hbox1), 6);

   gtk_box_pack_start(GTK_BOX(vbox1), hbox1, FALSE, FALSE, 2);

   for (i=0; i < nob; i++) {
#ifdef ENABLE_GTK2
      if (0 == strcmp("OK", button_text[i]))
	 button = gtk_button_new_from_stock(GTK_STOCK_OK);
      else
	 if (0 == strcmp("Cancel", button_text[i]))
	    button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
	 else
	    if (0 == strcmp("Yes", button_text[i]))
	       button = gtk_button_new_from_stock(GTK_STOCK_YES);
	    else
	       if (0 == strcmp("No", button_text[i]))
		  button = gtk_button_new_from_stock(GTK_STOCK_NO);
	       else
#endif
      button = gtk_button_new_with_label(gettext(button_text[i]));
      gtk_signal_connect(GTK_OBJECT(button), "clicked",
			 GTK_SIGNAL_FUNC(cb_dialog_button),
			 GINT_TO_POINTER(DIALOG_SAID_1 + i));
      gtk_box_pack_start(GTK_BOX(hbox1), button, TRUE, TRUE, 1);

      /* default button is the last one */
      if (i == nob-1)
      {
	 GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
	 gtk_widget_grab_default(button);
	 gtk_widget_grab_focus(button);
      }
   }

   gtk_widget_show_all(glob_dialog);

   gtk_main();

   return dialog_result;
}

int dialog_generic_ok(GtkWidget *widget,
		      char *title, int type, char *text)
{
   char *button_text[] = {N_("OK")};

   if (widget) {
      return dialog_generic(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(widget))),
			    title, type, text, 1, button_text);
   }
   return dialog_generic(NULL, title, type, text, 1, button_text);
}


/*
 * Widget must be some widget used to get the main window from.
 * The main window passed in would be fastest.
 * changed is MODIFY_FLAG, or NEW_FLAG
 */
int dialog_save_changed_record(GtkWidget *widget, int changed)
{
   int b;
   char *button_text[] = {N_("No"), N_("Yes")};

   b=0;

   if ((changed!=MODIFY_FLAG) && (changed!=NEW_FLAG)) {
      return EXIT_SUCCESS;
   }
   if (changed==MODIFY_FLAG) {
      b=dialog_generic(GTK_WINDOW(gtk_widget_get_toplevel(widget)),
		       _("Save Changed Record?"), DIALOG_QUESTION,
		       _("Do you want to save the changes to this record?"),
		       2, button_text);
   }
   if (changed==NEW_FLAG) {
      b=dialog_generic(GTK_WINDOW(gtk_widget_get_toplevel(widget)),
		       _("Save New Record?"), DIALOG_QUESTION,
		       _("Do you want to save this new record?"),
		       2, button_text);
   }

   return b;
}

/* creates the full path name of a file in the ~/.jpilot dir */
int get_home_file_name(char *file, char *full_name, int max_size)
{
   char *home, default_path[]=".";

#ifdef ENABLE_PROMETHEON
   home = getenv("COPILOT_HOME");
#else
   home = getenv("JPILOT_HOME");
#endif
   if (!home) {/*Not home; */
      home = getenv("HOME");
      if (!home) {/*Not home; */
	 jp_logf(JP_LOG_WARN, _("Can't get HOME environment variable\n"));
      }
   }
   if (!home) {
      home = default_path;
   }
   if (strlen(home)>(max_size-strlen(file)-strlen("/."EPN"/")-2)) {
      jp_logf(JP_LOG_WARN, _("Your HOME environment variable is too long for me\n"));
      home=default_path;
   }
   sprintf(full_name, "%s/."EPN"/%s", home, file);
   return EXIT_SUCCESS;
}


/*
 * Returns 0 if ok
 */
int check_hidden_dir()
{
   struct stat statb;
   char hidden_dir[FILENAME_MAX];
   char test_file[FILENAME_MAX];
   FILE *out;

   get_home_file_name("", hidden_dir, sizeof(hidden_dir));
   hidden_dir[strlen(hidden_dir)-1]='\0';

   if (stat(hidden_dir, &statb)) {
      /*directory isn't there, create it */
      if (mkdir(hidden_dir, 0700)) {
	 /*Can't create directory */
	 jp_logf(JP_LOG_WARN, _("Can't create directory %s\n"), hidden_dir);
	 return EXIT_FAILURE;
      }
      if (stat(hidden_dir, &statb)) {
	 jp_logf(JP_LOG_WARN, _("Can't create directory %s\n"), hidden_dir);
	 return EXIT_FAILURE;
      }
   }
   /*Is it a directory? */
   if (!S_ISDIR(statb.st_mode)) {
      jp_logf(JP_LOG_WARN, _("%s doesn't appear to be a directory.\n"
		  "I need it to be.\n"), hidden_dir);
      return EXIT_FAILURE;
   }
   /*Can we write in it? */
   get_home_file_name("test", test_file, sizeof(test_file));
   out = fopen(test_file, "w+");
   if (!out) {
      jp_logf(JP_LOG_WARN, _("I can't write files in directory %s\n"), hidden_dir);
   } else {
      fclose(out);
      unlink(test_file);
   }

   return EXIT_SUCCESS;
}

/*
 * month = 0-11
 * day = day of month 1-31
 * dow = day of week for this date
 * ndim = number of days in month 28-31
 */
void get_month_info(int month, int day, int year, int *dow, int *ndim)
{
   time_t ltime;
   struct tm *now;
   struct tm new_time;
   int days_in_month[]={31,28,31,30,31,30,31,31,30,31,30,31
   };

   time(&ltime);
   now = localtime(&ltime);

   new_time.tm_sec=0;
   new_time.tm_min=0;
   new_time.tm_hour=11;
   new_time.tm_mday=day; /*day of month 1-31 */
   new_time.tm_mon=month;
   new_time.tm_year=year;
   new_time.tm_isdst=now->tm_isdst;

   mktime(&new_time);
   *dow = new_time.tm_wday;

   /* leap year */
   if (month == 1) {
      if ((year%4 == 0) &&
	  !(((year+1900)%100==0) && ((year+1900)%400!=0))
	  ) {
	 days_in_month[1]++;
      }
   }
   *ndim = days_in_month[month];
}

int write_to_next_id_open(FILE *pc_out, unsigned int unique_id)
{
   char id_str[50];

   if (fseek(pc_out, 0, SEEK_SET)) {
      jp_logf(JP_LOG_WARN, "fseek failed\n");
      return EXIT_FAILURE;
   }

   if (fwrite(FILE_VERSION2_CR, strlen(FILE_VERSION2_CR), 1, pc_out) != 1) {
      jp_logf(JP_LOG_WARN, _("Error writing PC header to file: next_id\n"));
      return EXIT_FAILURE;
   }
   sprintf(id_str, "%d", unique_id);
   if (fwrite(id_str, strlen(id_str), 1, pc_out) != 1) {
      jp_logf(JP_LOG_WARN, _("Error writing next id to file: next_id\n"));
      return EXIT_FAILURE;
   }
   if (fwrite("\n", strlen("\n"), 1, pc_out) != 1) {
      jp_logf(JP_LOG_WARN, _("Error writing PC header to file: next_id\n"));
      return EXIT_FAILURE;
   }
   return EXIT_SUCCESS;
}

int write_to_next_id(unsigned int unique_id)
{
   FILE *pc_out;
   int ret;

   pc_out = jp_open_home_file("next_id", "r+");
   if (pc_out==NULL) {
      jp_logf(JP_LOG_WARN, _("Error opening file: next_id\n"));
      return EXIT_FAILURE;
   }

   ret = write_to_next_id_open(pc_out, unique_id);

   fclose(pc_out);

   return ret;
}

int get_next_unique_pc_id(unsigned int *next_unique_id)
{
   FILE *pc_in_out;
   char file_name[FILENAME_MAX];
   char str[256];

   pc_in_out = jp_open_home_file("next_id", "a+");
   if (pc_in_out==NULL) {
      jp_logf(JP_LOG_WARN, _("Error opening file: %s\n"),file_name);
      return EXIT_FAILURE;
   }

   if (ftell(pc_in_out)==0) {
      /*We have to write out the file header */
      *next_unique_id=1;
      write_to_next_id_open(pc_in_out, *next_unique_id);
   }
   fclose(pc_in_out);
   pc_in_out = jp_open_home_file("next_id", "r+");
   if (pc_in_out==NULL) {
      jp_logf(JP_LOG_WARN, _("Error opening file: %s\n"),file_name);
      return EXIT_FAILURE;
   }
   memset(str, '\0', sizeof(FILE_VERSION)+4);
   fread(str, 1, strlen(FILE_VERSION), pc_in_out);
   if (!strcmp(str, FILE_VERSION)) {
      /* Must be a versioned file */
      fseek(pc_in_out, 0, SEEK_SET);
      fgets(str, 200, pc_in_out);
      fgets(str, 200, pc_in_out);
      str[200]='\0';
      *next_unique_id = atoi(str);
   } else {
      fseek(pc_in_out, 0, SEEK_SET);
      fread(next_unique_id, sizeof(*next_unique_id), 1, pc_in_out);
   }
   (*next_unique_id)++;
   if (fseek(pc_in_out, 0, SEEK_SET)) {
      jp_logf(JP_LOG_WARN, "fseek failed\n");
   }
   /*rewind(pc_in_out); */
   /*todo - if > 16777216 then cleanup */

   write_to_next_id_open(pc_in_out, *next_unique_id);
   fclose(pc_in_out);

   return EXIT_SUCCESS;
}

int read_gtkrc_file()
{
   char filename[FILENAME_MAX];
   char fullname[FILENAME_MAX];
   struct stat buf;
   const char *svalue;

   get_pref(PREF_RCFILE, NULL, &svalue);
   if (svalue) {
     jp_logf(JP_LOG_DEBUG, "rc file from prefs is %s\n", svalue);
   } else {
     jp_logf(JP_LOG_DEBUG, "rc file from prefs is NULL\n");
   }

   strncpy(filename, svalue, sizeof(filename));
   filename[sizeof(filename)-1]='\0';

   /*Try to read the file out of the home directory first */
   get_home_file_name(filename, fullname, sizeof(fullname));

   if (stat(fullname, &buf)==0) {
      jp_logf(JP_LOG_DEBUG, "parsing %s\n", fullname);
      gtk_rc_parse(fullname);
      return EXIT_SUCCESS;
   }

   g_snprintf(fullname, sizeof(fullname), "%s/%s/%s/%s", BASE_DIR, "share", EPN, filename);
   if (stat(fullname, &buf)==0) {
      jp_logf(JP_LOG_DEBUG, "parsing %s\n", fullname);
      gtk_rc_parse(fullname);
      return EXIT_SUCCESS;
   }
   return EXIT_FAILURE;
}

FILE *jp_open_home_file(char *filename, char *mode)
{
   char fullname[FILENAME_MAX];
   FILE *pc_in;

   get_home_file_name(filename, fullname, sizeof(fullname));

   pc_in = fopen(fullname, mode);
   if (pc_in == NULL) {
      pc_in = fopen(fullname, "w+");
      if (pc_in) {
	 fclose(pc_in);
	 pc_in = fopen(fullname, mode);
      }
   }
   return pc_in;
}

int rename_file(char *old_filename, char *new_filename)
{
   char old_fullname[FILENAME_MAX];
   char new_fullname[FILENAME_MAX];

   get_home_file_name(old_filename, old_fullname, sizeof(old_fullname));
   get_home_file_name(new_filename, new_fullname, sizeof(new_fullname));

   return rename(old_fullname, new_fullname);
}


int unlink_file(char *filename)
{
   char fullname[FILENAME_MAX];

   get_home_file_name(filename, fullname, sizeof(fullname));

   return unlink(fullname);
}

/* This function will copy an empty DB file */
/* from the share directory to the users JPILOT_HOME or HOME directory */
/* if it doesn't exist already and its length is > 0 */
int check_copy_DBs_to_home()
{
   FILE *in, *out;
   struct stat sbuf;
   int i, c, r;
   char destname[FILENAME_MAX];
   char srcname[FILENAME_MAX];
   struct utimbuf times;
   char *dbname[]={
      "DatebookDB.pdb",
	"AddressDB.pdb",
	"ToDoDB.pdb",
	"MemoDB.pdb",
	"Memo32DB.pdb",
	"ExpenseDB.pdb",
	NULL
   };

   for (i=0; dbname[i]!=NULL; i++) {
      get_home_file_name(dbname[i], destname, sizeof(destname));
      r = stat(destname, &sbuf);
      if (((r)&&(errno==ENOENT)) || (sbuf.st_size==0)) {
	 /*The file doesn't exist or is zero in size, copy an empty DB file */
	 if ((strlen(BASE_DIR) + strlen(EPN) + strlen(dbname[i])) > sizeof(srcname)) {
	    jp_logf(JP_LOG_DEBUG, "copy_DB_to_home filename too long\n");
	    return EXIT_FAILURE;
	 }
	 g_snprintf(srcname, sizeof(srcname), "%s/%s/%s/%s", BASE_DIR, "share", EPN, dbname[i]);
	 in = fopen(srcname, "r");
	 out = fopen(destname, "w");
	 if (!in) {
	    jp_logf(JP_LOG_WARN, _("Couldn't find empty DB file %s: %s\n"),
		    srcname, strerror(errno));
	    jp_logf(JP_LOG_WARN, EPN); /* EPN is jpilot, or copilot depending on configure */
	    jp_logf(JP_LOG_WARN, _(" may not be installed.\n"));
	    return EXIT_FAILURE;
	 }
	 if (!out) {
	    fclose(in);
	    return EXIT_FAILURE;
	 }
	 while (!feof(in)) {
	    c = fgetc(in);
	    fputc(c, out);
	 }
	 fclose(in);
	 fclose(out);
	 /* Set the dates on the file to be old (not up to date) */
	 times.actime = 1;
	 times.modtime = 1;
	 utime(destname, &times);
      }
   }
   return EXIT_SUCCESS;
}

int jp_copy_file(char *src, char *dest)
{
   FILE *in, *out;
   int r;
   struct stat statb;
   struct utimbuf times;
   unsigned char buf[10002];

   if (!strcmp(src, dest)) {
      return EXIT_SUCCESS;
   }

   in = fopen(src, "r");
   out = fopen(dest, "w");
   if (!in) {
      return EXIT_FAILURE;
   }
   if (!out) {
      fclose(in);
      return EXIT_FAILURE;
   }
   while ((r = fread(buf, 1, sizeof(buf)-2, in))) {
      fwrite(buf, 1, r, out);
   }
   fclose(in);
   fclose(out);

   /*Set the create and modify times of new file to the same as the old */
   stat(src, &statb);
   times.actime = statb.st_atime;
   times.modtime = statb.st_mtime;
   utime(dest, &times);

   return EXIT_SUCCESS;
}



/*These next 2 functions were copied from pi-file.c in the pilot-link app */
/* Exact value of "Jan 1, 1970 0:00:00 GMT" - "Jan 1, 1904 0:00:00 GMT" */
#define PILOT_TIME_DELTA (unsigned)(2082844800)

time_t
pilot_time_to_unix_time (unsigned long raw_time)
{
   return (time_t)(raw_time - PILOT_TIME_DELTA);
}

unsigned long
unix_time_to_pilot_time (time_t t)
{
   return (unsigned long)((unsigned long)t + PILOT_TIME_DELTA);
}

unsigned int bytes_to_bin(unsigned char *bytes, unsigned int num_bytes)
{
   unsigned int i, n;
   n=0;
   for (i=0;i<num_bytes;i++) {
      n = n*256+bytes[i];
   }
   return n;
}

int raw_header_to_header(RawDBHeader *rdbh, DBHeader *dbh)
{
   unsigned long temp;

   strncpy(dbh->db_name, rdbh->db_name, 31);
   dbh->db_name[sizeof(dbh->db_name)-1] = '\0';
   dbh->flags = bytes_to_bin(rdbh->flags, 2);
   dbh->version = bytes_to_bin(rdbh->version, 2);
   temp = bytes_to_bin(rdbh->creation_time, 4);
   dbh->creation_time = pilot_time_to_unix_time(temp);
   temp = bytes_to_bin(rdbh->modification_time, 4);
   dbh->modification_time = pilot_time_to_unix_time(temp);
   temp = bytes_to_bin(rdbh->backup_time, 4);
   dbh->backup_time = pilot_time_to_unix_time(temp);
   dbh->modification_number = bytes_to_bin(rdbh->modification_number, 4);
   dbh->app_info_offset = bytes_to_bin(rdbh->app_info_offset, 4);
   dbh->sort_info_offset = bytes_to_bin(rdbh->sort_info_offset, 4);
   strncpy(dbh->type, rdbh->type, sizeof(dbh->type));
   dbh->type[sizeof(dbh->type)-1] = '\0';
   strncpy(dbh->creator_id, rdbh->creator_id, sizeof(dbh->creator_id));
   dbh->creator_id[sizeof(dbh->creator_id)-1] = '\0';
   strncpy(dbh->unique_id_seed, rdbh->unique_id_seed, sizeof(dbh->unique_id_seed));
   dbh->unique_id_seed[sizeof(dbh->unique_id_seed)-1] = '\0';
   dbh->next_record_list_id = bytes_to_bin(rdbh->next_record_list_id, 4);
   dbh->number_of_records = bytes_to_bin(rdbh->number_of_records, 2);

   return EXIT_SUCCESS;
}

/*returns 1 if found */
/*        0 if eof */
int find_next_offset(mem_rec_header *mem_rh, long fpos,
		     unsigned int *next_offset,
		     unsigned char *attrib, unsigned int *unique_id)
{
   mem_rec_header *temp_mem_rh;
   unsigned char found = 0;
   unsigned long found_at;

   found_at=0xFFFFFF;
   for (temp_mem_rh=mem_rh; temp_mem_rh; temp_mem_rh = temp_mem_rh->next) {
      if ((temp_mem_rh->offset > fpos) && (temp_mem_rh->offset < found_at)) {
	 found_at = temp_mem_rh->offset;
	 /* *attrib = temp_mem_rh->attrib; */
	 /* *unique_id = temp_mem_rh->unique_id; */
      }
      if ((temp_mem_rh->offset == fpos)) {
	 found = 1;
	 *attrib = temp_mem_rh->attrib;
	 *unique_id = temp_mem_rh->unique_id;
      }
   }
   *next_offset = found_at;
   return found;
}

void free_mem_rec_header(mem_rec_header **mem_rh)
{
   mem_rec_header *h, *next_h;

   for (h=*mem_rh; h; h=next_h) {
      next_h=h->next;
      free(h);
   }
   *mem_rh=NULL;
}


void print_string(char *str, int len)
{
   unsigned char c;
   int i;

   for (i=0;i<len;i++) {
      c=str[i];
      if (c < ' ' || c >= 0x7f)
	jp_logf(JP_LOG_STDOUT, "%x", c);
      else
	jp_logf(JP_LOG_STDOUT, "%c", c);
   }
   jp_logf(JP_LOG_STDOUT, "\n");
}

/* */
/* Warning, this function will move the file pointer */
/* */
int get_app_info_size(FILE *in, int *size)
{
   RawDBHeader rdbh;
   DBHeader dbh;
   unsigned int offset;
   record_header rh;

   fseek(in, 0, SEEK_SET);

   fread(&rdbh, sizeof(RawDBHeader), 1, in);
   if (feof(in)) {
      jp_logf(JP_LOG_WARN, "get_app_info_size(): %s\n", _("Error reading file"));
      return EXIT_FAILURE;
   }

   raw_header_to_header(&rdbh, &dbh);

   if (dbh.app_info_offset==0) {
      *size=0;
      return EXIT_SUCCESS;
   }
   if (dbh.sort_info_offset!=0) {
      *size = dbh.sort_info_offset - dbh.app_info_offset;
      return EXIT_SUCCESS;
   }
   if (dbh.number_of_records==0) {
      fseek(in, 0, SEEK_END);
      *size=ftell(in) - dbh.app_info_offset;
      return EXIT_SUCCESS;
   }

   fread(&rh, sizeof(record_header), 1, in);
   offset = ((rh.Offset[0]*256+rh.Offset[1])*256+rh.Offset[2])*256+rh.Offset[3];
   *size=offset - dbh.app_info_offset;

   return EXIT_SUCCESS;
}

/*
 * This deletes a record from the appropriate Datafile
 */
int delete_pc_record(AppType app_type, void *VP, int flag)
{
   FILE *pc_in;
   PC3RecordHeader header;
   struct Appointment *appt;
   MyAppointment *mappt;
   struct Address *addr;
   MyAddress *maddr;
   struct ToDo *todo;
   MyToDo *mtodo;
   struct Memo *memo;
   MyMemo *mmemo;
   char filename[FILENAME_MAX];
#ifndef PILOT_LINK_0_12
   unsigned char record[65536];
#else /* PILOT_LINK_0_12 */
   pi_buffer_t *RecordBuffer;
#endif /* PILOT_LINK_0_12 */
   PCRecType record_type;
   unsigned int unique_id;
   long ivalue;

   if (VP==NULL) {
      return EXIT_FAILURE;
   }

   /* to keep the compiler happy with -Wall*/
   mappt=NULL;
   maddr=NULL;
   mtodo=NULL;
   mmemo=NULL;
   switch (app_type) {
    case DATEBOOK:
      mappt = (MyAppointment *) VP;
      record_type = mappt->rt;
      unique_id = mappt->unique_id;
      strcpy(filename, "DatebookDB.pc3");
      break;
    case ADDRESS:
      maddr = (MyAddress *) VP;
      record_type = maddr->rt;
      unique_id = maddr->unique_id;
      strcpy(filename, "AddressDB.pc3");
      break;
    case TODO:
      mtodo = (MyToDo *) VP;
      record_type = mtodo->rt;
      unique_id = mtodo->unique_id;
#ifdef ENABLE_MANANA
      get_pref(PREF_MANANA_MODE, &ivalue, NULL);
      if (ivalue) {
	 strcpy(filename, "MañanaDB.pc3");
      } else {
	 strcpy(filename, "ToDoDB.pc3");
      }
#else
      strcpy(filename, "ToDoDB.pc3");
#endif
      break;
    case MEMO:
      mmemo = (MyMemo *) VP;
      record_type = mmemo->rt;
      unique_id = mmemo->unique_id;
      get_pref(PREF_MEMO32_MODE, &ivalue, NULL);
      if (ivalue) {
	 strcpy(filename, "Memo32DB.pc3");
      } else {
	 strcpy(filename, "MemoDB.pc3");
      }
      break;
    default:
      return EXIT_SUCCESS;
   }

   if ((record_type==DELETED_PALM_REC) || (record_type==MODIFIED_PALM_REC)) {
      jp_logf(JP_LOG_INFO|JP_LOG_GUI, _("This record is already deleted.\n"
	   "It is scheduled to be deleted from the Palm on the next sync.\n"));
      return EXIT_SUCCESS;
   }
   switch (record_type) {
    case NEW_PC_REC:
    case REPLACEMENT_PALM_REC:
      pc_in=jp_open_home_file(filename, "r+");
      if (pc_in==NULL) {
	 jp_logf(JP_LOG_WARN, _("Unable to open PC records file\n"));
	 return EXIT_FAILURE;
      }
      while(!feof(pc_in)) {
	 read_header(pc_in, &header);
	 if (feof(pc_in)) {
	    jp_logf(JP_LOG_WARN, _("Couldn't find record to delete\n"));
	    fclose(pc_in);
	    return EXIT_FAILURE;
	 }
	 /* Keep unique ID intact */
	 if (header.header_version==2) {
	    if ((header.unique_id==unique_id) &&
		((header.rt==NEW_PC_REC)||(header.rt==REPLACEMENT_PALM_REC))) {
	       if (fseek(pc_in, -header.header_len, SEEK_CUR)) {
		  jp_logf(JP_LOG_WARN, "fseek failed\n");
	       }
	       header.rt=DELETED_PC_REC;
	       write_header(pc_in, &header);
	       jp_logf(JP_LOG_DEBUG, "record deleted\n");
	       fclose(pc_in);
	       return EXIT_SUCCESS;
	    }
	 } else {
	    jp_logf(JP_LOG_WARN, _("Unknown header version %d\n"), header.header_version);
	 }
	 if (fseek(pc_in, header.rec_len, SEEK_CUR)) {
	    jp_logf(JP_LOG_WARN, "fseek failed\n");
	 }
      }
      fclose(pc_in);
      return EXIT_FAILURE;

    case PALM_REC:
      jp_logf(JP_LOG_DEBUG, "Deleting Palm ID %d\n",unique_id);
      pc_in=jp_open_home_file(filename, "a");
      if (pc_in==NULL) {
	 jp_logf(JP_LOG_WARN, _("Unable to open PC records file\n"));
	 return EXIT_FAILURE;
      }
      header.unique_id=unique_id;
      if (flag==MODIFY_FLAG) {
	 header.rt=MODIFIED_PALM_REC;
      } else {
	 header.rt=DELETED_PALM_REC;
      }
#ifdef PILOT_LINK_0_12
      RecordBuffer = pi_buffer_new(0);
#endif /* PILOT_LINK_0_12 */
      switch (app_type) {
       case DATEBOOK:
	 appt=&mappt->appt;
	 /*memset(&appt, 0, sizeof(appt)); */
#ifndef PILOT_LINK_0_12
	 header.rec_len = pack_Appointment(appt, record, sizeof(record)-1);
	 if (!header.rec_len) {
	    PRINT_FILE_LINE;
	    jp_logf(JP_LOG_WARN, "pack_Appointment %s\n", _("error"));
	 }
#else /* PILOT_LINK_0_12 */
	 if (pack_Appointment(appt, RecordBuffer, datebook_v1) == -1) {
	    PRINT_FILE_LINE;
	    jp_logf(JP_LOG_WARN, "pack_Appointment %s\n", _("error"));
	 }
#endif /* PILOT_LINK_0_12 */
	 break;
       case ADDRESS:
	 addr=&maddr->addr;
	 /* memset(&addr, 0, sizeof(addr)); */
#ifndef PILOT_LINK_0_12
	 header.rec_len = pack_Address(addr, record, sizeof(record)-1);
	 if (!header.rec_len) {
	    PRINT_FILE_LINE;
	    jp_logf(JP_LOG_WARN, "pack_Address %s\n", _("error"));
	 }
#else /* PILOT_LINK_0_12 */
	 if (pack_Address(addr, RecordBuffer, address_v1) == -1) {
	    PRINT_FILE_LINE;
	    jp_logf(JP_LOG_WARN, "pack_Address %s\n", _("error"));
	 }
#endif /* PILOT_LINK_0_12 */
	 break;
       case TODO:
	 todo=&mtodo->todo;
	 /* memset(&todo, 0, sizeof(todo)); */
#ifndef PILOT_LINK_0_12
	 header.rec_len = pack_ToDo(todo, record, sizeof(record)-1);
	 if (!header.rec_len) {
	    PRINT_FILE_LINE;
	    jp_logf(JP_LOG_WARN, "pack_ToDo %s\n", _("error"));
	 }
#else /* PILOT_LINK_0_12 */
	 if (pack_ToDo(todo, RecordBuffer, todo_v1) == -1) {
	    PRINT_FILE_LINE;
	    jp_logf(JP_LOG_WARN, "pack_ToDo %s\n", _("error"));
	 }
#endif /* PILOT_LINK_0_12 */
	 break;
       case MEMO:
	 memo=&mmemo->memo;
	 /* memset(&memo, 0, sizeof(memo)); */
#ifndef PILOT_LINK_0_12
	 header.rec_len = pack_Memo(memo, record, sizeof(record)-1);

	 if (!header.rec_len) {
	    PRINT_FILE_LINE;
	    jp_logf(JP_LOG_WARN, "pack_Memo %s\n", _("error"));
	 }
#else /* PILOT_LINK_0_12 */
	 if (pack_Memo(memo, RecordBuffer, memo_v1) == -1) {
	    PRINT_FILE_LINE;
	    jp_logf(JP_LOG_WARN, "pack_Memo %s\n", _("error"));
	 }
#endif /* PILOT_LINK_0_12 */
	 break;
       default:
	 fclose(pc_in);
#ifdef PILOT_LINK_0_12
	 pi_buffer_free(RecordBuffer);
#endif
	 return EXIT_SUCCESS;
      }
#ifdef PILOT_LINK_0_12
      header.rec_len = RecordBuffer->used;

#endif /* PILOT_LINK_0_12 */
      jp_logf(JP_LOG_DEBUG, "writing header to pc file\n");
      write_header(pc_in, &header);
      /* This record be used for making sure that the palm record
       * hasn't changed before we delete it */
      jp_logf(JP_LOG_DEBUG, "writing record to pc file, %d bytes\n", header.rec_len);
#ifndef PILOT_LINK_0_12
      fwrite(record, header.rec_len, 1, pc_in);
#else /* PILOT_LINK_0_12 */
      fwrite(RecordBuffer->data, header.rec_len, 1, pc_in);
#endif /* PILOT_LINK_0_12 */
      jp_logf(JP_LOG_DEBUG, "record deleted\n");
      fclose(pc_in);
#ifdef PILOT_LINK_0_12
      pi_buffer_free(RecordBuffer);
#endif
      return EXIT_SUCCESS;
      break;
    default:
      break;
   }
#ifdef PILOT_LINK_0_12
   pi_buffer_free(RecordBuffer);
#endif
   return EXIT_SUCCESS;
}

/*
 * This undeletes a record from the appropriate Datafile
 */
int undelete_pc_record(AppType app_type, void *VP, int flag)
{
   PC3RecordHeader header;
   MyAppointment *mappt;
   MyAddress *maddr;
   MyToDo *mtodo;
   MyMemo *mmemo;
   unsigned int unique_id;
   long ivalue;
   char filename[FILENAME_MAX];
   char filename2[FILENAME_MAX];
   FILE *pc_file  = NULL;
   FILE *pc_file2 = NULL;
   char *record;
   int found;
   int ret = -1;
   int num;

   if (VP==NULL) {
      return EXIT_FAILURE;
   }

   /* to keep the compiler happy with -Wall*/
   mappt = NULL;
   maddr = NULL;
   mtodo = NULL;
   mmemo = NULL;
   switch (app_type) {
    case DATEBOOK:
      mappt = (MyAppointment *) VP;
      unique_id = mappt->unique_id;
      strcpy(filename, "DatebookDB.pc3");
      break;
    case ADDRESS:
      maddr = (MyAddress *) VP;
      unique_id = maddr->unique_id;
      strcpy(filename, "AddressDB.pc3");
      break;
    case TODO:
      mtodo = (MyToDo *) VP;
      unique_id = mtodo->unique_id;
#ifdef ENABLE_MANANA
      get_pref(PREF_MANANA_MODE, &ivalue, NULL);
      if (ivalue) {
	 strcpy(filename, "MañanaDB.pc3");
      } else {
	 strcpy(filename, "ToDoDB.pc3");
      }
#else
      strcpy(filename, "ToDoDB.pc3");
#endif
      break;
    case MEMO:
      mmemo = (MyMemo *) VP;
      unique_id = mmemo->unique_id;
      get_pref(PREF_MEMO32_MODE, &ivalue, NULL);
      if (ivalue) {
	 strcpy(filename, "Memo32DB.pc3");
      } else {
	 strcpy(filename, "MemoDB.pc3");
      }
      break;
    default:
      return EXIT_SUCCESS;
   }

   found  = FALSE;
   record = NULL;

   g_snprintf(filename2, sizeof(filename2), "%s.pct", filename);

   pc_file = jp_open_home_file(filename , "r");
   if (!pc_file) {
      return EXIT_FAILURE;
   }

   pc_file2=jp_open_home_file(filename2, "w");
   if (!pc_file2) {
      fclose(pc_file);
      return EXIT_FAILURE;
   }

   while(!feof(pc_file)) {
      read_header(pc_file, &header);
      if (feof(pc_file)) {
	 break;
      }
      /* Skip copying DELETED_PALM_REC entry which undeletes it */
      if (header.unique_id == unique_id &&
	  header.rt == DELETED_PALM_REC) {
	 found = TRUE;
	 if (fseek(pc_file, header.rec_len, SEEK_CUR)) {
	    jp_logf(JP_LOG_WARN, "fseek failed\n");
	    ret = -1;
	    break;
	 }
	 continue;
      }
      /* Change header on DELETED_PC_REC to undelete this type */
      if (header.unique_id == unique_id &&
          header.rt == DELETED_PC_REC) {
	  found = TRUE;
          header.rt = NEW_PC_REC;
      }

      /* Otherwise, keep whatever is there by copying it to the new pc3 file */
      record = malloc(header.rec_len);
      if (!record) {
	 jp_logf(JP_LOG_WARN, "cleanup_pc_file(): Out of memory\n");
	 ret = -1;
	 break;
      }
      num = fread(record, header.rec_len, 1, pc_file);
      if (num != 1) {
	 if (ferror(pc_file)) {
	    ret = -1;
	    break;
	 }
      }
      ret = write_header(pc_file2, &header);
      ret = fwrite(record, header.rec_len, 1, pc_file2);
      if (ret != 1) {
	 ret = -1;
	 break;
      }
      free(record);
      record = NULL;
   }

   if (record) {
      free(record);
   }
   if (pc_file) {
      fclose(pc_file);
   }
   if (pc_file2) {
      fclose(pc_file2);
   }

   if (found) {
      rename_file(filename2, filename);
   } else {
      unlink_file(filename2);
   }

   return ret;
}


int cleanup_pc_file(char *DB_name, unsigned int *max_id)
{
   PC3RecordHeader header;
   char pc_filename[FILENAME_MAX];
   char pc_filename2[FILENAME_MAX];
   FILE *pc_file;
   FILE *pc_file2;
   char *record;
   int r;
   int ret;
   int num;
   int compact_it;
   int next_id;

   r=0;
   *max_id = 0;
   next_id = 1;
   record = NULL;
   pc_file = pc_file2 = NULL;

   g_snprintf(pc_filename, sizeof(pc_filename), "%s.pc3", DB_name);
   g_snprintf(pc_filename2, sizeof(pc_filename2), "%s.pct", DB_name);

   pc_file = jp_open_home_file(pc_filename , "r");
   if (!pc_file) {
      return EXIT_FAILURE;
   }

   compact_it = 0;
   /* Scan through the file and see if it needs to be compacted */
   while(!feof(pc_file)) {
      read_header(pc_file, &header);
      if (feof(pc_file)) {
	 break;
      }
      if (header.rt & SPENT_PC_RECORD_BIT) {
	 compact_it=1;
	 break;
      }
      if ((header.unique_id > *max_id)
	  && (header.rt != PALM_REC)
	  && (header.rt != MODIFIED_PALM_REC)
	  && (header.rt != DELETED_PALM_REC)
	  && (header.rt != REPLACEMENT_PALM_REC) ){
	 *max_id = header.unique_id;
      }
      if (fseek(pc_file, header.rec_len, SEEK_CUR)) {
	 jp_logf(JP_LOG_WARN, "fseek failed\n");
      }
   }

   if (!compact_it) {
      jp_logf(JP_LOG_DEBUG, "No compacting needed\n");
      fclose(pc_file);
      return EXIT_SUCCESS;
   }

   fseek(pc_file, 0, SEEK_SET);

   pc_file2=jp_open_home_file(pc_filename2, "w");
   if (!pc_file2) {
      fclose(pc_file);
      return EXIT_FAILURE;
   }

   while(!feof(pc_file)) {
      read_header(pc_file, &header);
      if (feof(pc_file)) {
	 break;
      }
      if (header.rt & SPENT_PC_RECORD_BIT) {
	 r++;
	 if (fseek(pc_file, header.rec_len, SEEK_CUR)) {
	    jp_logf(JP_LOG_WARN, "fseek failed\n");
	    r = -1;
	    break;
	 }
	 continue;
      } else {
	 if (header.rt == NEW_PC_REC) {
	    header.unique_id = next_id++;
	 }
	 if ((header.unique_id > *max_id)
	     && (header.rt != PALM_REC)
	     && (header.rt != MODIFIED_PALM_REC)
	     && (header.rt != DELETED_PALM_REC)
	     && (header.rt != REPLACEMENT_PALM_REC)
	     ){
	    *max_id = header.unique_id;
	 }
	 record = malloc(header.rec_len);
	 if (!record) {
	    jp_logf(JP_LOG_WARN, "cleanup_pc_file(): %s\n", _("Out of memory"));
	    r = -1;
	    break;
	 }
	 num = fread(record, header.rec_len, 1, pc_file);
	 if (num != 1) {
	    if (ferror(pc_file)) {
	       r = -1;
	       break;
	    }
	 }
	 ret = write_header(pc_file2, &header);
	 /*if (ret != 1) {
	    r = -1;
	    break;
	 }*/
	 ret = fwrite(record, header.rec_len, 1, pc_file2);
	 if (ret != 1) {
	    r = -1;
	    break;
	 }
	 free(record);
	 record = NULL;
      }
   }

   if (record) {
      free(record);
   }
   if (pc_file) {
      fclose(pc_file);
   }
   if (pc_file2) {
      fclose(pc_file2);
   }

   if (r>=0) {
      rename_file(pc_filename2, pc_filename);
   } else {
      unlink_file(pc_filename2);
   }

   return r;
}

int cleanup_pc_files()
{
   int ret;
   int fail_flag;
   unsigned int max_id, max_max_id;
#ifdef ENABLE_PLUGINS
   GList *plugin_list, *temp_list;
   struct plugin_s *plugin;
#endif

   fail_flag = 0;
   max_id = max_max_id = 0;
   jp_logf(JP_LOG_DEBUG, "cleanup_pc_file for DatebookDB\n");
   ret = cleanup_pc_file("DatebookDB", &max_id);
   jp_logf(JP_LOG_DEBUG, "max_id was %d\n", max_id);
   if (ret<0) {
      fail_flag=1;
   } else if (max_id > max_max_id) {
      max_max_id = max_id;
   }
   jp_logf(JP_LOG_DEBUG, "cleanup_pc_file for AddressDB\n");
   ret = cleanup_pc_file("AddressDB", &max_id);
   jp_logf(JP_LOG_DEBUG, "max_id was %d\n", max_id);
   if (ret<0) {
      fail_flag=1;
   } else if (max_id > max_max_id) {
      max_max_id = max_id;
   }
   jp_logf(JP_LOG_DEBUG, "cleanup_pc_file for ToDoDB\n");
   ret = cleanup_pc_file("ToDoDB", &max_id);
   jp_logf(JP_LOG_DEBUG, "max_id was %d\n", max_id);
   if (ret<0) {
      fail_flag=1;
   } else if (max_id > max_max_id) {
      max_max_id = max_id;
   }
   jp_logf(JP_LOG_DEBUG, "cleanup_pc_file for MemoDB\n");
   ret += cleanup_pc_file("MemoDB", &max_id);
   jp_logf(JP_LOG_DEBUG, "max_id was %d\n", max_id);
   if (ret<0) {
      fail_flag=1;
   } else if (max_id > max_max_id) {
      max_max_id = max_id;
   }
   jp_logf(JP_LOG_DEBUG, "cleanup_pc_file for Memo32DB\n");
   ret += cleanup_pc_file("Memo32DB", &max_id);
   jp_logf(JP_LOG_DEBUG, "max_id was %d\n", max_id);
   if (ret<0) {
      fail_flag=1;
   } else if (max_id > max_max_id) {
      max_max_id = max_id;
   }
#ifdef ENABLE_PLUGINS
   plugin_list = get_plugin_list();

   for (temp_list = plugin_list; temp_list; temp_list = temp_list->next) {
      plugin = (struct plugin_s *)temp_list->data;
      if ((plugin->db_name==NULL) || (plugin->db_name[0]=='\0')) {
	 jp_logf(JP_LOG_DEBUG, "not calling cleanup_pc_file for: [%s]\n", plugin->db_name);
	 continue;
      }
      jp_logf(JP_LOG_DEBUG, "cleanup_pc_file for [%s]\n", plugin->db_name);
      ret = cleanup_pc_file(plugin->db_name, &max_id);
      jp_logf(JP_LOG_DEBUG, "max_id was %d\n", max_id);
      if (ret<0) {
	 fail_flag=1;
      } else if (max_id > max_max_id) {
	 max_max_id = max_id;
      }
   }
#endif
   if (!fail_flag) {
      write_to_next_id(max_max_id);
   }

   return EXIT_SUCCESS;
}

int setup_sync(unsigned int flags)
{
   long num_backups;
   const char *svalue;
   const char *port;
   int r;
#ifndef HAVE_SETENV
   char str[80];
#endif
   struct my_sync_info sync_info;

   /* look in env for PILOTRATE first */
   if (!(getenv("PILOTRATE"))) {
      get_pref(PREF_RATE, NULL, &svalue);
      jp_logf(JP_LOG_DEBUG, "setting PILOTRATE=[%s]\n", svalue);
      if (svalue) {
#ifdef HAVE_SETENV
	 setenv("PILOTRATE", svalue, TRUE);
#else
	 sprintf(str, "PILOTRATE=%s", svalue);
	 putenv(str);
#endif
      }
   }

   get_pref(PREF_PORT, NULL, &port);
   get_pref(PREF_NUM_BACKUPS, &num_backups, NULL);
   jp_logf(JP_LOG_DEBUG, "pref port=[%s]\n", port);
   jp_logf(JP_LOG_DEBUG, "num_backups=%d\n", num_backups);
   get_pref(PREF_USER, NULL, &svalue);
   strncpy(sync_info.username, svalue, sizeof(sync_info.username));
   sync_info.username[sizeof(sync_info.username)-1]='\0';
   get_pref(PREF_USER_ID, &(sync_info.userID), NULL);

   get_pref(PREF_PC_ID, &(sync_info.PC_ID), NULL);
   if (sync_info.PC_ID == 0) {
      srandom(time(NULL));
      /* RAND_MAX is 32768 on Solaris machines for some reason.
       * If someone knows how to fix this, let me know.
       */
      if (RAND_MAX==32768) {
	 sync_info.PC_ID = 1+(2000000000.0*random()/(2147483647+1.0));
      } else {
	 sync_info.PC_ID = 1+(2000000000.0*random()/(RAND_MAX+1.0));
      }
      jp_logf(JP_LOG_WARN, _("PC ID is 0.\n"));
      jp_logf(JP_LOG_WARN, _("I generated a new PC ID.  It is %lu\n"), sync_info.PC_ID);
      set_pref(PREF_PC_ID, sync_info.PC_ID, NULL, TRUE);
   }

   sync_info.sync_over_ride = 0;
   strncpy(sync_info.port, port, sizeof(sync_info.port));
   sync_info.port[sizeof(sync_info.port)-1]='\0';
   sync_info.flags=flags;
   sync_info.num_backups=num_backups;

   r = sync_once(&sync_info);

   return r;
}

void multibyte_safe_strncpy(char *dst, char *src, size_t len)
{
   long char_set;

   get_pref(PREF_CHAR_SET, &char_set, NULL);

   if (char_set == CHAR_SET_JAPANESE ||
       char_set == CHAR_SET_TRADITIONAL_CHINESE ||
       char_set == CHAR_SET_KOREAN
       ) {
      char *p, *q;
      int n = 0;
      p = src; q = dst;
      while ((*p) && n < (len-2)) {
	 if ((*p) & 0x80) {
	    *q++ = *p++;
	    n++;
	    if (*p) {
	       *q++ = *p++;
	       n++;
	    }
	 } else {
	    *q++ = *p++;
	    n++;
	 }
      }
      if (!(*p & 0x80 ) && (n < len-1))
	*q++ = *p++;

      *q = '\0';
   } else {
      strncpy(dst, src, len);
   }
}

char *multibyte_safe_memccpy(char *dst, const char *src, int c, size_t len)
{
   long char_set;

   if (len == 0) return NULL;
   if (dst == NULL) return NULL;
   if (src == NULL) return NULL;

   get_pref(PREF_CHAR_SET, &char_set, NULL);

   if (char_set == CHAR_SET_JAPANESE ||
       char_set == CHAR_SET_TRADITIONAL_CHINESE ||
       char_set == CHAR_SET_KOREAN
       ) {  /* Multibyte Charactors */
      char *p, *q;
      int n = 0;

      p = (char *)src;
      q = dst;
      while ((*p) && (n < (len -2))) {
	 if ((*p) & 0x80) {
	    *q++ = *p++;
	    n++;
	    if (*p) {
	       *q++ = *p++;
	       n++;
	    }
	 } else {
	    *q++ = *p++;
	    n++;
	 }
	 if (*(p-1) == (char)(c & 0xff))
	    return q;
      }
      if (!(*p & 0x80) && (n < len-1))
	*q++ = *p++;

      *q = '\0';
      return NULL;
   } else
     return memccpy(dst, src, c, len);
}

void charset_j2p(char *buf, int max_len, long char_set)
{
   switch (char_set) {
    case CHAR_SET_JAPANESE: Euc2Sjis(buf, max_len); break;
    case CHAR_SET_1250: Lat2Win(buf,max_len); break;
    case CHAR_SET_1251: koi8_to_win1251(buf, max_len); break;
    case CHAR_SET_1251_B: win1251_to_koi8(buf, max_len); break;
    default:
      UTF_to_other(buf, max_len);
      break;
   }
}

void jp_charset_j2p(char *const buf, int max_len)
{
   long char_set;

   get_pref(PREF_CHAR_SET, &char_set, NULL);
   charset_j2p(buf, max_len, char_set);
}

void jp_charset_p2j(char *const buf, int max_len)
{
   long char_set;

   get_pref(PREF_CHAR_SET, &char_set, NULL);
   if (char_set == CHAR_SET_JAPANESE) jp_Sjis2Euc(buf, max_len);
   else charset_p2j(buf, max_len, char_set); /* JPA */
/* JPA */
/*   if (char_set == CHAR_SET_1250) Win2Lat(buf,max_len);   */
/*   if (char_set == CHAR_SET_1251) win1251_to_koi8(buf, max_len); */
/*   if (char_set == CHAR_SET_1251_B) koi8_to_win1251(buf, max_len); */
/*   if (char_set == CHAR_SET_1250UTF) Win2UTF(buf, max_len); */

}

/*
 *         JPA overwrite a Palm Pilot character string by its
 *             conversion to host character set
 */

void charset_p2j(char *const buf, int max_len, int char_set)
{
   char *newbuf;

   newbuf = charset_p2newj(buf, max_len, char_set);

   strncpy(buf, newbuf, max_len);
   buf[max_len - 1] = '\0';
   /* note : string may get truncated within a multibyte char */
   if (strlen(newbuf) >= max_len)
     jp_logf(JP_LOG_WARN, "charset_p2j: buffer too small - string had to be truncated to [%s]\n", buf);
   free(newbuf);
}

/*
 *         JPA convert a Palm Pilot character string to host
 *             equivalent without overwriting
 */

char *charset_p2newj(const char *buf, int max_len, int char_set)
{
   char *newbuf = NULL;

   /* allocate a longer buffer if not done in conversion routine */
   if (char_set < CHAR_SET_UTF) {
      newbuf = (char*)malloc(2*max_len - 1);
      if (newbuf) {
	 /* be safe, though string should fit into buf */
	 strncpy(newbuf, buf, max_len);
	 newbuf[max_len - 1] = '\0';
      }
   } else {
      newbuf = (char*)NULL; /* keep compiler happy */
   }
   switch (char_set) {
    case CHAR_SET_JAPANESE : Sjis2Euc(newbuf, max_len); break;
    case CHAR_SET_1250 : Win2Lat(newbuf,max_len); break;
    case CHAR_SET_1251 : win1251_to_koi8(newbuf, max_len); break;
    case CHAR_SET_1251_B : koi8_to_win1251(newbuf, max_len); break;
    default:
      newbuf = other_to_UTF(buf, max_len);
      break;
   }
   return (newbuf);
}

#define NUM_CAT_ITEMS 16

int make_category_menu(GtkWidget **category_menu,
		       GtkWidget **cat_menu_item,
		       struct sorted_cats *sort_l,
		       void (*selection_callback)
		       (GtkWidget *item, int selection),
		       int add_an_all_item)
{
   GtkWidget *menu;
   GSList    *group;
   int i;
   int offset;

   *category_menu = gtk_option_menu_new();

   menu = gtk_menu_new();
   group = NULL;

   offset=0;
   if (add_an_all_item) {
      cat_menu_item[0] = gtk_radio_menu_item_new_with_label(group, _("All"));
      if (selection_callback) {
	 gtk_signal_connect(GTK_OBJECT(cat_menu_item[0]), "activate",
			    GTK_SIGNAL_FUNC(selection_callback), GINT_TO_POINTER(CATEGORY_ALL));
      }
      group = gtk_radio_menu_item_group(GTK_RADIO_MENU_ITEM(cat_menu_item[0]));
      gtk_menu_append(GTK_MENU(menu), cat_menu_item[0]);
      gtk_widget_show(cat_menu_item[0]);
      offset=1;
   }
   for (i=0; i<NUM_CAT_ITEMS; i++) {
      if (sort_l[i].Pcat[0]) {
	 cat_menu_item[i+offset] = gtk_radio_menu_item_new_with_label(
	    group, sort_l[i].Pcat);
	 if (selection_callback) {
	    gtk_signal_connect(GTK_OBJECT(cat_menu_item[i+offset]), "activate",
			       GTK_SIGNAL_FUNC(selection_callback), GINT_TO_POINTER(sort_l[i].cat_num));
	 }
	 group = gtk_radio_menu_item_group(GTK_RADIO_MENU_ITEM(cat_menu_item[i+offset]));
	 gtk_menu_append(GTK_MENU(menu), cat_menu_item[i+offset]);
	 gtk_widget_show(cat_menu_item[i+offset]);
      }
   }

   gtk_option_menu_set_menu(GTK_OPTION_MENU(*category_menu), menu);

   return EXIT_SUCCESS;
}

int pdb_file_count_recs(char *DB_name, int *num)
{
   char local_pdb_file[FILENAME_MAX];
   char full_local_pdb_file[FILENAME_MAX];
   struct pi_file *pf;

   jp_logf(JP_LOG_DEBUG, "pdb_file_count_recs\n");

   *num = 0;

   g_snprintf(local_pdb_file, sizeof(local_pdb_file), "%s.pdb", DB_name);
   get_home_file_name(local_pdb_file, full_local_pdb_file, sizeof(full_local_pdb_file));

   pf = pi_file_open(full_local_pdb_file);
   if (!pf) {
      jp_logf(JP_LOG_WARN, _("Unable to open file: %s\n"), full_local_pdb_file);
      return EXIT_FAILURE;
   }

   pi_file_get_entries(pf, num);

   pi_file_close(pf);

   return EXIT_SUCCESS;
}

int pdb_file_delete_record_by_id(char *DB_name, pi_uid_t uid_in)
{
   char local_pdb_file[FILENAME_MAX];
   char full_local_pdb_file[FILENAME_MAX];
   char full_local_pdb_file2[FILENAME_MAX];
   struct pi_file *pf1, *pf2;
   struct DBInfo infop;
   void *app_info;
   void *sort_info;
   void *record;
   int r;
   int idx;
#ifdef PILOT_LINK_0_12
   size_t size;
#else
   int size;
#endif
   int attr;
   int cat;
   pi_uid_t uid;
   struct stat statb;
   struct utimbuf times;

   jp_logf(JP_LOG_DEBUG, "pdb_file_delete_record_by_id\n");

   g_snprintf(local_pdb_file, sizeof(local_pdb_file), "%s.pdb", DB_name);
   get_home_file_name(local_pdb_file, full_local_pdb_file, sizeof(full_local_pdb_file));
   strcpy(full_local_pdb_file2, full_local_pdb_file);
   strcat(full_local_pdb_file2, "2");

   /* After we are finished, set the create and modify times of new file
      to the same as the old */
   stat(full_local_pdb_file, &statb);
   times.actime = statb.st_atime;
   times.modtime = statb.st_mtime;

   pf1 = pi_file_open(full_local_pdb_file);
   if (!pf1) {
      jp_logf(JP_LOG_WARN, _("Unable to open file: %s\n"), full_local_pdb_file);
      return EXIT_FAILURE;
   }
   pi_file_get_info(pf1, &infop);
   pf2 = pi_file_create(full_local_pdb_file2, &infop);
   if (!pf2) {
      jp_logf(JP_LOG_WARN, _("Unable to open file: %s\n"), full_local_pdb_file2);
      return EXIT_FAILURE;
   }

   pi_file_get_app_info(pf1, &app_info, &size);
   pi_file_set_app_info(pf2, app_info, size);

   pi_file_get_sort_info(pf1, &sort_info, &size);
   pi_file_set_sort_info(pf2, sort_info, size);

   for(idx=0;;idx++) {
      r = pi_file_read_record(pf1, idx, &record, &size, &attr, &cat, &uid);
      if (r<0) break;
      if (uid==uid_in) continue;
      pi_file_append_record(pf2, record, size, attr, cat, uid);
   }

   pi_file_close(pf1);
   pi_file_close(pf2);

   if (rename(full_local_pdb_file2, full_local_pdb_file) < 0) {
      jp_logf(JP_LOG_WARN, "pdb_file_delete_record_by_id(): %s\n", _("rename failed"));
   }

   utime(full_local_pdb_file, &times);

   return EXIT_SUCCESS;
}

/*
 * Original ID is in the case of a modification
 * new ID is used in the case of an add record
 */
int pdb_file_modify_record(char *DB_name, void *record_in, int size_in,
			   int attr_in, int cat_in, pi_uid_t uid_in)
{
   char local_pdb_file[FILENAME_MAX];
   char full_local_pdb_file[FILENAME_MAX];
   char full_local_pdb_file2[FILENAME_MAX];
   struct pi_file *pf1, *pf2;
   struct DBInfo infop;
   void *app_info;
   void *sort_info;
   void *record;
   int r;
   int idx;
#ifdef PILOT_LINK_0_12
   size_t size;
#else
   int size;
#endif
   int attr;
   int cat;
   int found;
   pi_uid_t uid;
   struct stat statb;
   struct utimbuf times;

   jp_logf(JP_LOG_DEBUG, "pi_file_modify_record\n");

   g_snprintf(local_pdb_file, sizeof(local_pdb_file), "%s.pdb", DB_name);
   get_home_file_name(local_pdb_file, full_local_pdb_file, sizeof(full_local_pdb_file));
   strcpy(full_local_pdb_file2, full_local_pdb_file);
   strcat(full_local_pdb_file2, "2");

   /* After we are finished, set the create and modify times of new file
      to the same as the old */
   stat(full_local_pdb_file, &statb);
   times.actime = statb.st_atime;
   times.modtime = statb.st_mtime;

   pf1 = pi_file_open(full_local_pdb_file);
   if (!pf1) {
      jp_logf(JP_LOG_WARN, _("Unable to open file: %s\n"), full_local_pdb_file);
      return EXIT_FAILURE;
   }
   pi_file_get_info(pf1, &infop);
   pf2 = pi_file_create(full_local_pdb_file2, &infop);
   if (!pf2) {
      jp_logf(JP_LOG_WARN, _("Unable to open file: %s\n"), full_local_pdb_file2);
      return EXIT_FAILURE;
   }

   pi_file_get_app_info(pf1, &app_info, &size);
   pi_file_set_app_info(pf2, app_info, size);

   pi_file_get_sort_info(pf1, &sort_info, &size);
   pi_file_set_sort_info(pf2, sort_info, size);

   found = 0;

   for(idx=0;;idx++) {
      r = pi_file_read_record(pf1, idx, &record, &size, &attr, &cat, &uid);
      if (r<0) break;
      if (uid==uid_in) {
	 pi_file_append_record(pf2, record_in, size_in, attr_in, cat_in, uid_in);
	 found=1;
      } else {
	 pi_file_append_record(pf2, record, size, attr, cat, uid);
      }
   }
   if (!found) {
      pi_file_append_record(pf2, record_in, size_in, attr_in, cat_in, uid_in);
   }

   pi_file_close(pf1);
   pi_file_close(pf2);

   if (rename(full_local_pdb_file2, full_local_pdb_file) < 0) {
      jp_logf(JP_LOG_WARN, "pdb_file_modify_record(): %s\n", _("rename failed"));
   }

   utime(full_local_pdb_file, &times);

   return EXIT_SUCCESS;
}

#ifdef PILOT_LINK_0_12
int pdb_file_read_record_by_id(char *DB_name,
			       pi_uid_t uid,
			       void **bufp, size_t *sizep, int *idxp,
			       int *attrp, int *catp)
#else
int pdb_file_read_record_by_id(char *DB_name,
			       pi_uid_t uid,
			       void **bufp, int *sizep, int *idxp,
			       int *attrp, int *catp)
#endif
{
   char local_pdb_file[FILENAME_MAX];
   char full_local_pdb_file[FILENAME_MAX];
   struct pi_file *pf1;
   void *temp_buf;
   int r;

   jp_logf(JP_LOG_DEBUG, "pdb_file_read_record_by_id\n");

   g_snprintf(local_pdb_file, sizeof(local_pdb_file), "%s.pdb", DB_name);
   get_home_file_name(local_pdb_file, full_local_pdb_file, sizeof(full_local_pdb_file));

   pf1 = pi_file_open(full_local_pdb_file);
   if (!pf1) {
      jp_logf(JP_LOG_WARN, _("Unable to open file: %s\n"), full_local_pdb_file);
      return EXIT_FAILURE;
   }

   r = pi_file_read_record_by_id(pf1, uid, &temp_buf, sizep, idxp, attrp, catp);
   /* during the close bufp will be freed, so we copy it */
   /* if ( (r>0) && (*sizep>0) ) { JPA */
   if ( (r>=0) && (*sizep>0) ) { /* JPA */
      *bufp=malloc(*sizep);
      if (*bufp) {
	 memcpy(*bufp, temp_buf, *sizep);
      }
   } else {
      *bufp=NULL;
   }

   pi_file_close(pf1);

   return r;
}

#ifdef PILOT_LINK_0_12
int pdb_file_write_app_block(char *DB_name, void *bufp, size_t size_in)
#else
int pdb_file_write_app_block(char *DB_name, void *bufp, int size_in)
#endif
{
   char local_pdb_file[FILENAME_MAX];
   char full_local_pdb_file[FILENAME_MAX];
   char full_local_pdb_file2[FILENAME_MAX];
   struct pi_file *pf1, *pf2;
   struct DBInfo infop;
   void *app_info;
   void *sort_info;
   void *record;
   int r;
   int idx;
#ifdef PILOT_LINK_0_12
   size_t size;
#else
   int size;
#endif
   int attr;
   int cat;
   pi_uid_t uid;
   struct stat statb;
   struct utimbuf times;

   jp_logf(JP_LOG_DEBUG, "pdb_file_write_app_block\n");

   g_snprintf(local_pdb_file, sizeof(local_pdb_file), "%s.pdb", DB_name);
   get_home_file_name(local_pdb_file, full_local_pdb_file, sizeof(full_local_pdb_file));
   strcpy(full_local_pdb_file2, full_local_pdb_file);
   strcat(full_local_pdb_file2, "2");

   /* After we are finished, set the create and modify times of new file
      to the same as the old */
   stat(full_local_pdb_file, &statb);
   times.actime = statb.st_atime;
   times.modtime = statb.st_mtime;

   pf1 = pi_file_open(full_local_pdb_file);
   if (!pf1) {
      jp_logf(JP_LOG_WARN, _("Unable to open file: %s\n"), full_local_pdb_file);
      return EXIT_FAILURE;
   }
   pi_file_get_info(pf1, &infop);
   pf2 = pi_file_create(full_local_pdb_file2, &infop);
   if (!pf2) {
      jp_logf(JP_LOG_WARN, _("Unable to open file: %s\n"), full_local_pdb_file2);
      return EXIT_FAILURE;
   }

   pi_file_get_app_info(pf1, &app_info, &size);
   pi_file_set_app_info(pf2, bufp, size_in);

   pi_file_get_sort_info(pf1, &sort_info, &size);
   pi_file_set_sort_info(pf2, sort_info, size);

   for(idx=0;;idx++) {
      r = pi_file_read_record(pf1, idx, &record, &size, &attr, &cat, &uid);
      if (r<0) break;
      pi_file_append_record(pf2, record, size, attr, cat, uid);
   }

   pi_file_close(pf1);
   pi_file_close(pf2);

   if (rename(full_local_pdb_file2, full_local_pdb_file) < 0) {
      jp_logf(JP_LOG_WARN, "pdb_file_write_app_block(): %s\n", _("rename failed"));
   }

   utime(full_local_pdb_file, &times);

   return EXIT_SUCCESS;
}

/* DB_name is filename with extention and path, i.e: "/tmp/Net Prefs.prc"
 */
int pdb_file_write_dbinfo(char *full_DB_name, struct DBInfo *Pinfo_in)
{
   char full_local_pdb_file2[FILENAME_MAX];
   struct pi_file *pf1, *pf2;
   struct DBInfo infop;
   void *app_info;
   void *sort_info;
   void *record;
   int r;
   int idx;
#ifdef PILOT_LINK_0_12
   size_t size;
#else
   int size;
#endif
   int attr;
   int cat;
   pi_uid_t uid;
   struct stat statb;
   struct utimbuf times;

   jp_logf(JP_LOG_DEBUG, "pdb_file_write_dbinfo\n");

   g_snprintf(full_local_pdb_file2, sizeof(full_local_pdb_file2), "%s2", full_DB_name);

   /* After we are finished, set the create and modify times of new file
      to the same as the old */
   stat(full_DB_name, &statb);
   times.actime = statb.st_atime;
   times.modtime = statb.st_mtime;

   pf1 = pi_file_open(full_DB_name);
   if (!pf1) {
      jp_logf(JP_LOG_WARN, _("Unable to open file: %s\n"), full_DB_name);
      return EXIT_FAILURE;
   }
   pi_file_get_info(pf1, &infop);
   /* Set the DBInfo to the one coming into the function */
   pf2 = pi_file_create(full_local_pdb_file2, Pinfo_in);
   if (!pf2) {
      jp_logf(JP_LOG_WARN, _("Unable to open file: %s\n"), full_local_pdb_file2);
      return EXIT_FAILURE;
   }

   pi_file_get_app_info(pf1, &app_info, &size);
   pi_file_set_app_info(pf2, app_info, size);

   pi_file_get_sort_info(pf1, &sort_info, &size);
   pi_file_set_sort_info(pf2, sort_info, size);

   for(idx=0;;idx++) {
      r = pi_file_read_record(pf1, idx, &record, &size, &attr, &cat, &uid);
      if (r<0) break;
      pi_file_append_record(pf2, record, size, attr, cat, uid);
   }

   pi_file_close(pf1);
   pi_file_close(pf2);

   if (rename(full_local_pdb_file2, full_DB_name) < 0) {
      jp_logf(JP_LOG_WARN, "pdb_file_write_dbinfo(): %s\n", _("rename failed"));
   }

   utime(full_DB_name, &times);

   return EXIT_SUCCESS;
}

size_t jp_strftime(char *s, size_t max, const char *format, const struct tm *tm)
{
   size_t ret;

#ifdef ENABLE_GTK2
   gchar *utf8_text;
   gchar *local_format;

   /* the format string is UTF-8 encoded since it comes from a .po file */
   local_format = g_locale_from_utf8(format, -1, NULL, NULL, NULL);

   ret = strftime(s, max, local_format, tm);
   g_free(local_format);

   utf8_text = g_locale_to_utf8 (s, -1, NULL, NULL, NULL);
   g_strlcpy(s, utf8_text, max);
   g_free(utf8_text);
#else
   ret = strftime(s, max, format, tm);
#endif
   return ret;
}

/* This function is passed a bounded event description before it appears
 * on the gui (read-only) views. It checks if the event is a yearly repeat
 * (i.e. an anniversary) and then if the last 4 characters look like a
 * year. If so then it appends a "number of years" to the description.
 * This is handy for viewing ages on birthdays etc.
 */
void append_anni_years(char *desc, int max, struct tm *date,
		       struct Appointment *appt)
{
   int len;
   int year;
   /* Only append the years if this is a yearly repeating type (i.e. an
    * anniversary) */
   if (appt->repeatType != repeatYearly)
      return;

   /* Only display this is the user option is enabled */
   if (!get_pref_int_default(PREF_DATEBOOK_ANNI_YEARS, FALSE))
      return;

   len = strlen(desc);

   /* Make sure we have room to insert what we want */
   if (len < 4 || len > (max - 7))
      return;

   /* Get and check for a year */
   year = strtoul(&desc[len - 4], NULL, 10);

   /* Only allow up to 3 digits to be added */
   if (year < 1100 || year > 3000)
      return;

   /* Append the number of years */
   sprintf(&desc[len], " (%d)", 1900 + date->tm_year - year);
}

/* Get today's date and work out day in month. This is used to highlight
 * today in the gui (read-only) views. Returns the day of month if today
 * is in the passed month else returns -1.
 */
int get_highlighted_today(struct tm *date)
{
   time_t now;
   struct tm* now_tm;

   /* Quit immediately if the user option is not enabled */
   if (!get_pref_int_default(PREF_DATEBOOK_HI_TODAY, FALSE))
      return -1;

   /* Get now time */
   now = time(NULL);
   now_tm = localtime(&now);

   /* Check if option is on and return today's day of month if the month
    * and year match was was passed in */
   if (now_tm->tm_mon != date->tm_mon || now_tm->tm_year != date->tm_year)
      return -1;

   /* Today is within the passed month, return the day of month */
   return now_tm->tm_mday;
}

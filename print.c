/* $Id: print.c,v 1.53 2010/10/19 00:33:39 rikster5 Exp $ */

/*******************************************************************************
 * print.c
 * A module of J-Pilot http://jpilot.org
 *
 * Copyright (C) 2000-2002 by Judd Montgomery
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

/********************************* Includes ***********************************/
#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#include "print.h"
#include "print_headers.h"
#include "print_logo.h"
#include "datebook.h"
#include "calendar.h"
#include "address.h"
#include "todo.h"
#include "sync.h"
#include "prefs.h"
#include "log.h"
#include "i18n.h"
#ifdef HAVE_LOCALE_H
#  include <locale.h>
#endif

/********************************* Constants **********************************/
#ifdef JPILOT_PRINTABLE
#  define FLAG_CHAR 'A'
#  define Q_FLAG_CHAR "A"
#else
#  define FLAG_CHAR 010
#  define Q_FLAG_CHAR "\\010"
#endif

/******************************* Global vars **********************************/
static FILE *out;
static int first_hour, first_min, last_hour, last_min;
extern int datebk_category;

static char *PaperSizes[] = { 
   "Letter", "Legal", "Statement", "Tabloid", "Ledger", "Folio", "Quarto",
   "7x9", "9x11", "9x12", "10x13", "10x14", "Executive", 
   "A0", "A1", "A2", "A3", "A4", "A5", "A6", "A7", "A8", "A9", "A10",
   "B0", "B1", "B2", "B3", "B4", "B5", "B6", "B7", "B8", "B9", "B10", 
   "ISOB0", "ISOB1", "ISOB2", "ISOB3", "ISOB4", "ISOB5", "ISOB6", "ISOB7", 
   "ISOB8", "ISOB9", "ISOB10", 
   "C0", "C1", "C2", "C3", "C4", "C5", "C6", "C7", "DL", "Filo"
};

/****************************** Prototypes ************************************/
static int fill_in(struct tm *date, CalendarEventList *a_list);
static void ps_strncat(char *dest, char *src, int n);

/****************************** Main Code *************************************/
static FILE *print_open(void)
{
   const char *command;

   get_pref(PREF_PRINT_COMMAND, NULL, &command);
   if (command) {
      return popen(command, "w");
   } else {
      return NULL;
   }
}

static void print_close(FILE *f)
{
   pclose(f);
}

static int courier_12(void)
{
   /* fprintf(out, "/Courier 12 selectfont\n"); */
   fprintf(out, "%cC12\n", FLAG_CHAR);
   return EXIT_SUCCESS;
}

static int courier_bold_12(void)
{
   /* fprintf(out, "/Courier-Bold 12 selectfont\n"); */
   fprintf(out, "%cCB12\n", FLAG_CHAR);
   return EXIT_SUCCESS;
}

static int clip_to_box(float x1, float y1, float x2, float y2)
{
   fprintf(out, "%g inch %g inch %g inch %g inch rectclip\n",
                 x1,     y1,     x2-x1,  y2-y1);
   return EXIT_SUCCESS;
}

static int puttext(float x, float y, char *text)
{
   int len;
   char *buf;

   len = strlen(text);
   buf = malloc(2 * len + 1);
   memset(buf, 0, 2 * len + 1);
   ps_strncat(buf, text, 2 * len);
   fprintf(out, "%g inch %g inch moveto (%s) show\n", x, y, buf);
   free(buf);
   return EXIT_SUCCESS;
}

static int header(void)
{
   time_t ltime;

   time(&ltime);
   fprintf(out,
           "%%!PS-Adobe-2.0\n"
           "%%%%Creator: J-Pilot\n"
           "%%%%CreationDate: %s"
           "%%%%DocumentData: Clean7Bit\n"
           "%%%%Orientation: Portrait\n"
           "%%DocumentFonts: Times-Roman Times-Bold Courier Courier-Bold\n"
           "%%%%Magnification: 1.0000\n"
           "%%%%Pages: 1\n"
           "%%%%EndComments\n"
           "%%%%BeginProlog\n"
           ,ctime(&ltime));
   fprintf(out, "/PageSize (%s) def\n\n", PaperSizes[PAPER_Letter]);
   print_common_prolog(out);
   fprintf(out,
           "%%%%EndProlog\n"
           "%%%%BeginSetup\n");
   print_common_setup(out);
   fprintf(out, "595 612 div 842 792 div Min dup scale %% HACK!!!! (CMB)\n");
           /* This hack pre-scales to compensate for the standard scaling
              mechanism below, to avoid me having to redo the layout of
              the dayview for the A4 standard size page. */
   fprintf(out,
           "%%%%EndSetup\n"
           "%%%%Page: 1 1\n\n");

   return EXIT_SUCCESS;
}

static int print_dayview(struct tm *date, CalendarEventList *ce_list)
{
   char str[80];
   char datef[80];
   time_t ltime;
   struct tm *now;
   const char *svalue;

#ifdef HAVE_LOCALE_H
   char *current_locale;
   current_locale = setlocale(LC_NUMERIC,"C");
#endif

   header();
   /* Draw the 2 gray columns and header block */
   print_day_header(out);

   /* Put the month name up */
   fprintf(out, "/Times-Bold-ISOLatin1 findfont 20 scalefont setfont\n"
                "newpath 0 setgray\n");
   get_pref(PREF_LONGDATE, NULL, &svalue);
   strftime(str, sizeof(str), _(svalue), date);
   puttext(0.5, 10.25, str);

   /* Put the weekday name up */
   fprintf(out, "/Times-Roman-ISOLatin1 findfont 15 scalefont setfont\n");
   strftime(str, sizeof(str), "%A", date);
   puttext(0.5, 10, str);

   /* Put the time of printing up */
   fprintf(out, "newpath\n"
                "/Times-Roman-ISOLatin1 findfont 10 scalefont setfont\n");

   time(&ltime);
   now = localtime(&ltime);
   get_pref(PREF_SHORTDATE, NULL, &svalue);
   g_snprintf(datef, sizeof(datef), "%s %s", "Printed on: ", svalue);
   strftime(str, sizeof(str), datef, now);
   puttext(0.5, 0.9, str);
   puttext(7.5, 0.9, "J-Pilot");
   fprintf(out, "stroke\n");

   print_logo(out, 40, 90, 0.35);

   /* Put the appointments on the dayview calendar */
   fill_in(date, ce_list);

   fprintf(out, "showpage\n");
   fprintf(out, "%%%%EOF\n");

#ifdef HAVE_LOCALE_H
   setlocale(LC_ALL, current_locale);
#endif

   return EXIT_SUCCESS;
}

static int fill_in(struct tm *date, CalendarEventList *ce_list)
{
   CalendarEventList *temp_cel;
   int i;
   int hours[24];
   int defaults1=0, defaults2=0;
   int hour24;
   int am;
   float top_y=9.40;
   float default_y=3.40;
   float indent1=1.25;
   float indent2=5.00;
   float step=0.12; /* This is the space between lines */
   float x,y;
   int max_per_line=4;
   char str[256];
   char datef[32];

   for (i=0; i<24; i++) {
      hours[i]=0;
   }

   /* We have to go through them twice, once for AM, and once for PM
    * This is because of the clipping */
   for (i=0; i<2; i++) {
      am=i%2;
      fprintf(out, "gsave\n");
      if (am) {
         clip_to_box(1.25, 0.5, 4.25, 9.5);
      } else {
         clip_to_box(5.0, 0.5, 8.0, 9.5);
      }
      for (temp_cel = ce_list; temp_cel; temp_cel=temp_cel->next) {
         if (temp_cel->mcale.cale.description == NULL) {
            continue;
         }
         if (temp_cel->mcale.cale.event) {
            strcpy(str, " ");
            if (!am) {
               continue;
            }
            x=indent1;
            y=default_y - defaults1 * step;
            defaults1++;
         } else {
            hour24 = temp_cel->mcale.cale.begin.tm_hour;
            if ((hour24 > 11) && (am)) {
               continue;
            }
            if ((hour24 < 12) && (!am)) {
               continue;
            }

            get_pref_time_no_secs(datef);
            strftime(str, sizeof(str), datef, &temp_cel->mcale.cale.begin);

            if (hour24 > 11) {
               x=indent2;
               y=top_y - (hour24 - 12) * 0.5 - (hours[hour24]) * step;
               hours[hour24]++;
               if (hours[hour24] > max_per_line) {
                  y=default_y - defaults2 * step;
                  defaults2++;
               }
            } else {
               x=indent1;
               y=top_y - (hour24) * 0.5 - (hours[hour24]) * step;
               hours[hour24]++;
               if (hours[hour24] > max_per_line) {
                  y=default_y - defaults1 * step;
                  defaults1++;
               }
            }
         }
         if (temp_cel->mcale.cale.description) {
            strcat(str, " ");
            strncat(str, temp_cel->mcale.cale.description, sizeof(str)-strlen(str)-2);
            str[128]='\0';
            /* FIXME: Add location in parentheses (loc) as the Palm does.
             * We would need to check strlen, etc., before adding */
         }
         if (y > 1.0) {
            puttext(x, y, str);
         } else {
            jp_logf(JP_LOG_WARN, "Too many appointments, dropping one\n");
         }
      }
      fprintf(out, "grestore\n");
   }

   return EXIT_SUCCESS;
}

int print_days_appts(struct tm *date)
{
   CalendarEventList *ce_list;

   out = print_open();
   if (!out) {
      return EXIT_FAILURE;
   }

   ce_list = NULL;

   get_days_calendar_events2(&ce_list, date, 2, 2, 2, CATEGORY_ALL, NULL);

   print_dayview(date, ce_list);

   print_close(out);

   free_CalendarEventList(&ce_list);

   return EXIT_SUCCESS;
}

static int f_indent_print(FILE *f, int indent, char *str) {
   char *P;
   int i, col;

   col=indent;
   for (P=str; *P; P++) {
      col++;
      if ((*P==10) || (*P==13)) {
         fprintf(f, "%c", *P);
         for (i=indent; i; i--) {
            fprintf(f, " ");
         }
         col=indent;
         continue;
      }
      if (col>75) {
         fprintf(f, "\n");
         for (i=indent; i; i--) {
            fprintf(f, " ");
         }
         col=indent+1;
      }
      fprintf(f, "%c", *P);
   }
   return EXIT_SUCCESS;
}

/*----------------------------------------------------------------------
 * ps_strncat   Escapes brackets for printing in PostScript strings
 *----------------------------------------------------------------------*/

void ps_strncat(char *dest, char *src, int n)
{
   int i = 0, j = 0;
   char *dest2;
   dest2 = strchr(dest, '\0');
   while (j < n) {
      if (src[i] == '\0') {
         dest2[j]='\0';
         break;
      }
      if (strchr("()", src[i]) != NULL) {
         if(j<n-1) dest2[j] = '\\'; else dest2[j]=' ';
         j++;
      }
      dest2[j] = src[i];
      i++;
      j++;
   }
}

/*----------------------------------------------------------------------
 * days_in_mon  Returns the number of days in the month containing the
 *              date passed in.
 *----------------------------------------------------------------------*/

static int days_in_mon(struct tm *date)
{
   int days_in_month[]={ 31,28,31,30,31,30,31,31,30,31,30,31 };

   if ((date->tm_year%4 == 0) &&
       !(((date->tm_year+1900)%100==0) && ((date->tm_year+1900)%400!=0))) {
       days_in_month[1]++;
   }
   return(days_in_month[date->tm_mon]);
}

/*----------------------------------------------------------------------
 * print_months_appts   Function to print the current month's
 *                      appointments.
 *----------------------------------------------------------------------*/

static char *MonthNames[] = {
   "January", "February", "March", "April", "May", "June", "July",
   "August", "September", "October", "November", "December"
};

int print_months_appts(struct tm *date_in, PaperSize paper_size)
{
   CalendarEventList *ce_list;
   CalendarEventList *temp_cel;
   struct tm date;
   char desc[100];
   time_t ltime;
   int dow;
   int ndim;
   int n;
   long fdow;
   int mask;
#ifdef ENABLE_DATEBK
   int ret;
   int cat_bit;
   int db3_type;
   long use_db3_tags;
   struct db4_struct db4;
#endif
#ifdef HAVE_LOCALE_H
   char *current_locale;
#endif

   /*------------------------------------------------------------------
    * Set up the PostScript output file, and print the header to it.
    *------------------------------------------------------------------*/
   mask=0;

   time(&ltime);

#ifdef ENABLE_DATEBK
   get_pref(PREF_USE_DB3, &use_db3_tags, NULL);
#endif

#ifdef HAVE_LOCALE_H
   current_locale = setlocale(LC_NUMERIC,"C");
#endif
   if (! (out = print_open())) return(EXIT_FAILURE);

   fprintf(out,
           "%%!PS-Adobe-2.0\n"
           "%%%%Creator: J-Pilot\n"
           "%%%%CreationDate: %s"
           "%%%%DocumentData: Clean7Bit\n"
           "%%%%Orientation: Landscape\n\n"
           "%%DocumentFonts: Times-Roman Times-Bold Courier Courier-Bold\n"
           "%%%%Magnification: 1.0000\n"
           "%%%%Pages: 1\n"
           "%%%%EndComments\n"
           "%%%%BeginProlog\n"
           ,ctime(&ltime));
   fprintf(out, "/PageSize (%s) def\n\n", PaperSizes[paper_size]);
   print_common_prolog(out);
   fprintf(out,
           "%%%%EndProlog\n"
           "%%%%BeginSetup\n");
   print_common_setup(out);
   print_month_header(out);
   fprintf(out,
           "%%%%EndSetup\n"
           "%%%%Page: 1 1\n\n");

   /*------------------------------------------------------------------
    * Extract the appointments
    *------------------------------------------------------------------*/
   ce_list = NULL;
   memcpy(&date, date_in, sizeof(struct tm));
   /* Get all of the appointments */

   get_days_calendar_events2(&ce_list, NULL, 2, 2, 2, CATEGORY_ALL, NULL);
   get_month_info(date.tm_mon, 1, date.tm_year, &dow, &ndim);
   weed_calendar_event_list(&ce_list, date.tm_mon, date.tm_year, 0, &mask);

   /*------------------------------------------------------------------
    * Loop through the days in the month, printing appointments
    *------------------------------------------------------------------*/
   date.tm_mday=1;
   date.tm_sec=0;
   date.tm_min=0;
   date.tm_hour=11;
   date.tm_isdst=-1;
   mktime(&date);

   get_pref(PREF_FDOW, &fdow, NULL);

   fprintf(out,
           "(%s %d) %d (%s) (%s version %s) %ld InitialisePage\n\n",
           MonthNames[date_in->tm_mon], date_in->tm_year + 1900,
           date.tm_wday,
           ctime(&ltime),
           PN, VERSION, fdow);

   for (n=0, date.tm_mday=1; date.tm_mday<=ndim; date.tm_mday++, n++) {
      date.tm_sec=0;
      date.tm_min=0;
      date.tm_hour=11;
      date.tm_isdst=-1;
      date.tm_wday=0;
      date.tm_yday=1;
      mktime(&date);

      fprintf(out, "%%--------------------------------------------------\n"
              "%%Stuff for day %2d being printed\n", date.tm_mday);
      fprintf(out, "NextDay\n");

      for (temp_cel = ce_list; temp_cel; temp_cel=temp_cel->next) {
#ifdef ENABLE_DATEBK
         if (use_db3_tags) {
            ret = db3_parse_tag(temp_cel->mcale.cale.note, &db3_type, &db4);
            /* jp_logf(JP_LOG_DEBUG, "category = 0x%x\n", db4.category); */
            cat_bit=1<<db4.category;
            if (!(cat_bit & datebk_category)) {
               jp_logf(JP_LOG_DEBUG, "skipping rec not in this category\n");
               continue;
            }
         }
#endif
         if (calendar_isApptOnDate(&(temp_cel->mcale.cale), &date)) {
            char tmp[20];
            char datef1[20];
            char datef2[20];
            tmp[0]='\0';
            if ( ! temp_cel->mcale.cale.event) {
               get_pref_time_no_secs(datef1);
               g_snprintf(datef2, sizeof(datef2), "(%s )", datef1);
               strftime(tmp, sizeof(tmp), datef2, &(temp_cel->mcale.cale.begin));
               tmp[19]='\0';
            }
            desc[0]='\0';
            if (temp_cel->mcale.cale.description) {
               ps_strncat(desc, temp_cel->mcale.cale.description, 100);
               desc[sizeof(desc)-1]='\0';
               /* FIXME: Add location in parentheses (loc) as the Palm does.
                * We would need to check strlen, etc., before adding */
            }
            remove_cr_lfs(desc);
            fprintf(out, "%s (%s) %simedItem\n", tmp, desc,
                    (strlen(tmp) == 0) ? "Unt" : "T" );
         }
      }
   }

   /*------------------------------------------------------------------*/
   memcpy(&date, date_in, sizeof(struct tm));
   date.tm_mday = 1;    /* Go to the first of the month */
   mktime(&date);
   sub_months_from_date(&date, 1);
   strftime(desc, sizeof(desc), "(%B %Y) %w ", &date);
   fprintf(out, "\n\n%%----------------------------------------\n"
           "%% Now generate the small months\n\n"
           "%s %d ", desc, days_in_mon(&date));

   add_months_to_date(&date, 2);
   strftime(desc, sizeof(desc), "(%B %Y) %w ", &date);
   fprintf(out, "%s %d SmallMonths\n", desc, days_in_mon(&date));

   /*------------------------------------------------------------------*/

   free_CalendarEventList(&ce_list);

   fprintf(out, "grestore\n");
   print_logo(out, 20, 30, 0.35);
   fprintf(out, "\nshowpage\n");
   fprintf(out, "%%%%EOF\n");

   print_close(out);

#ifdef HAVE_LOCALE_H
   setlocale(LC_NUMERIC, current_locale);
#endif
   return EXIT_SUCCESS;
}

/*----------------------------------------------------------------------
 * reset_first_last     Routine to reset max/min appointment times
 *----------------------------------------------------------------------*/

static void reset_first_last(void)
{
   first_hour = 25;
   first_min  = 61;
   last_hour  = -1;
   last_min   = -1;
}

/*----------------------------------------------------------------------
 * check_first_last     Routine to track max/min appointment times
 *----------------------------------------------------------------------*/

static void check_first_last(CalendarEventList *cel)
{
   struct tm *ApptTime;
   ApptTime = &(cel->mcale.cale.begin);
   if (ApptTime->tm_hour == first_hour) {
      if (ApptTime->tm_min < first_min) first_min = ApptTime->tm_min;
   }
   else if (ApptTime->tm_hour < first_hour) {
      first_hour = ApptTime->tm_hour;
      first_min  = ApptTime->tm_min;
   }

   ApptTime = &(cel->mcale.cale.end);
   if (ApptTime->tm_hour == last_hour) {
      if (ApptTime->tm_min > last_min) last_min = ApptTime->tm_min;
   } else if (ApptTime->tm_hour > last_hour) {
      last_hour = ApptTime->tm_hour;
      last_min  = ApptTime->tm_min;
   }
}

/*----------------------------------------------------------------------
 * print_weeks_appts    Function to print a weeks appointments onto a
 *                      weekly plan. We assume that date_in is the chosen
 *                      first day of the week.
 *----------------------------------------------------------------------*/

int print_weeks_appts(struct tm *date_in, PaperSize paper_size)
{
   CalendarEventList *ce_list, *temp_cel;
   struct tm date;
   struct tm *today_date;
   char desc[256], short_date[32];
   int n;
   time_t ltime;
#ifdef ENABLE_DATEBK
   int ret;
   int cat_bit;
   int db3_type;
   long use_db3_tags;
   struct db4_struct db4;
#endif
#ifdef HAVE_LOCALE_H
   char *current_locale;
#endif

#ifdef ENABLE_DATEBK
   get_pref(PREF_USE_DB3, &use_db3_tags, NULL);
#endif
#ifdef HAVE_LOCALE_H
   current_locale = setlocale(LC_NUMERIC,"C");
#endif

   /*------------------------------------------------------------------
    * Set up the PostScript output file, and print the header to it.
    *------------------------------------------------------------------*/
   if (! (out = print_open())) return(EXIT_FAILURE);

   time(&ltime);
   fprintf(out,
           "%%!PS-Adobe-2.0\n"
           "%%%%Creator: J-Pilot\n"
           "%%%%CreationDate: %s"
           "%%%%DocumentData: Clean7Bit\n"
           "%%%%Orientation: Landscape\n"
           "%%DocumentFonts: Times-Roman Times-Bold Courier Courier-Bold\n"
           "%%%%Magnification: 1.0000\n"
           "%%%%Pages: 1\n"
           "%%%%EndComments\n"
           "%%%%BeginProlog\n"
           ,ctime(&ltime));
   /*------------------------------------------------------------------
    * These are preferences for page size (passed in), first and last
    * hours on the plan (default; scales if earlier or later are present),
    * and whether to print dashes across the page.
    *------------------------------------------------------------------*/
   fprintf(out, "/PageSize (%s) def\n\n", PaperSizes[paper_size]);
   fprintf(out, "/FirstHour  9 def\n"
                "/LastHour  22 def\n");
   fprintf(out, "/Dashes true def\n");
   print_common_prolog(out);
   fprintf(out,
           "%%%%EndProlog\n"
           "%%%%BeginSetup\n");
   print_common_setup(out);
   print_week_header(out);
   fprintf(out,
           "%%%%EndSetup\n"
           "%%%%Page: 1 1\n\n");

   /*------------------------------------------------------------------
    * Run through the appointments, looking for earliest and latest
    *------------------------------------------------------------------*/
   ce_list = NULL;
   get_days_calendar_events2(&ce_list, NULL, 2, 2, 2, CATEGORY_ALL, NULL);
   reset_first_last();

   memcpy(&date, date_in, sizeof(struct tm));
   for (n = 0; n < 7; n++, add_days_to_date(&date, 1)) {
      for (temp_cel = ce_list; temp_cel; temp_cel=temp_cel->next) {
#ifdef ENABLE_DATEBK
         if (use_db3_tags) {
            ret = db3_parse_tag(temp_cel->mcale.cale.note, &db3_type, &db4);
            cat_bit=1<<db4.category;
            if (!(cat_bit & datebk_category)) continue;
         }
#endif
         if (calendar_isApptOnDate(&(temp_cel->mcale.cale), &date))
            if (! temp_cel->mcale.cale.event)
               check_first_last(temp_cel);
      }
   }
   if (last_min > 0) last_hour++;

   /*------------------------------------------------------------------
    * Now put in the finishing touches to the header, and kick-start
    * the printing process
    *------------------------------------------------------------------*/

   today_date = localtime(&ltime);
   fprintf(out,
           "%%------------------------------------------------------------\n"
           "%% This is today's date, the date of printing, plus the hour\n"
           "%% before & after the first and last appointments, respectively\n"
           "%d %d %d %d %d startprinting\n\n",
           today_date->tm_mday, today_date->tm_mon + 1,
           today_date->tm_year + 1900, first_hour, last_hour);
   fprintf(out, "( by %s version %s) show\n", PN, VERSION);

   print_logo(out, 20, 30, 0.35);

   /*------------------------------------------------------------------
    * Run through the appointments, printing them out
    *------------------------------------------------------------------*/
   free_CalendarEventList(&ce_list);
   ce_list = NULL;

   /* Get all of the appointments */
   get_days_calendar_events2(&ce_list, NULL, 2, 2, 2, CATEGORY_ALL, NULL);

   /* iterate through seven days */
   memcpy(&date, date_in, sizeof(struct tm));

   for (n = 0; n < 7; n++, add_days_to_date(&date, 1)) {
      strftime(short_date, sizeof(short_date), "%a, %d %b, %Y", &date);
      fprintf(out, "%d startday\n(%s) dateline\n", n, short_date);

      for (temp_cel = ce_list; temp_cel; temp_cel=temp_cel->next) {
#ifdef ENABLE_DATEBK
         if (use_db3_tags) {
            ret = db3_parse_tag(temp_cel->mcale.cale.note, &db3_type, &db4);
            jp_logf(JP_LOG_DEBUG, "category = 0x%x\n", db4.category);
            cat_bit=1<<db4.category;
            if (!(cat_bit & datebk_category)) {
               jp_logf(JP_LOG_DEBUG, "skip rec not in this category\n");
               continue;
            }
         }
#endif
         if (calendar_isApptOnDate(&(temp_cel->mcale.cale), &date)) {
            memset(desc, 0, sizeof(desc));
            memset(short_date, 0, sizeof(short_date));

            if ( ! temp_cel->mcale.cale.event)
              {
                 char t1[6], t2[6], ht[3], mt[3];
                 int j, m;

                 strftime(ht, sizeof(ht), "%H", &(temp_cel->mcale.cale.begin));
                 strftime(mt, sizeof(mt), "%M", &(temp_cel->mcale.cale.begin));
                 m = atoi(mt);
                 snprintf(t1, sizeof(t1), "%s.%02d", ht, (int)((m * 100.)/60));

                 strftime(ht, sizeof(ht), "%H", &(temp_cel->mcale.cale.end));
                 strftime(mt, sizeof(mt), "%M", &(temp_cel->mcale.cale.end));
                 m = atoi(mt);
                 snprintf(t2, sizeof(t2), "%s.%02d", ht, (int)((m * 100.)/60));
                 sprintf(short_date, "%s %s ", t1, t2);
                 for (j=0; j<30;j++) short_date[j] =tolower(short_date[j]);
              }
            if (temp_cel->mcale.cale.description) {
               ps_strncat(desc, temp_cel->mcale.cale.description, 250);
               /* FIXME: Add location in parentheses (loc) as the Palm does.
                * We would need to check strlen, etc., before adding */
               remove_cr_lfs(desc);
            }
            fprintf(out, "%s (%s) itemline\n", short_date, desc);
         }
      }
   }
   free_CalendarEventList(&ce_list);
   fprintf(out, "\nfinishprinting\n");
   fprintf(out, "%%%%EOF\n");
   print_close(out);

#ifdef HAVE_LOCALE_H
   setlocale(LC_ALL, current_locale);
#endif
   return EXIT_SUCCESS;
}

/*
 * Address code
 */

static int print_address_header(void)
{
   time_t ltime;
   struct tm *date;
   const char *svalue;
   char str[256];

   time(&ltime);
   date = localtime(&ltime);

   get_pref(PREF_SHORTDATE, NULL, &svalue);
   strftime(str, sizeof(str), svalue, date);

   fprintf(out,
           "%%!PS-Adobe-2.0\n"
           "%%%%Creator: J-Pilot\n"
           "%%%%CreationDate: %s"
           "%%%%DocumentData: Clean7Bit\n"
           /* XXX Title */
           "%%%%Orientation: Portrait\n"
           /* XXX BoundingBox */
           "%%DocumentFonts: Times-Roman Times-Bold "
           "Courier Courier-Bold ZapfDingbats\n"
           "%%%%Magnification: 1.0000\n"
           "%%%%BoundingBox: 36 36 576 756\n"
           "%%%%EndComments\n",
           ctime(&ltime));
   fprintf(out,
           "%%%%BeginProlog\n"
           "%%%%BeginResource: procset\n"
           "/inch {72 mul} def\n"
           "/left {0.5 inch} def\n"
           "/bottom {1.0 inch} def\n"
           "/bottom_hline {2.0 inch} def\n"
           "/footer {0.9 inch} def\n"
           "/top {10.5 inch 14 sub} def\n"
           "/buffer 1024 string def\n"
           "/scratch 128 string def\n"
           "/printobject {\n"
           "dup 128 string cvs dup (--nostringval--) eq {\n"
           "pop type24 string cvs\n"
           "}{\n"
           "exch pop\n"
           "} ifelse\n"
           "} bind def\n");
   /* Checkbox stuff */
   fprintf(out,
           "/checkboxcheck {\n"
           "%%currentpoint 6 add moveto\n"
           "%%4 -5 rlineto\n"
           "%%6 12 rlineto\n"
           "/ZapfDingbats 14 selectfont (4) show\n" /* or 3 if you prefer */
           "} bind def\n"
           "/checkboxbox {\n"
           "8 0 rlineto\n"
           "0 8 rlineto\n"
           "-8 0 rlineto\n"
           "0 -8 rlineto\n"
           "} bind def\n"
           "/checkbox {\n"
           "currentpoint\n"
           "gsave\n"
           "newpath\n"
           "moveto\n"
           "1 setlinewidth\n"
           "checkboxbox\n"
           "stroke\n"
           "grestore\n"
           "} bind def\n"
           "/checkedbox {\n"
           "currentpoint\n"
           "gsave\n"
           "newpath\n"
           "moveto\n"
           "1 setlinewidth\n"
           "checkboxbox\n"
           "checkboxcheck\n"
           "stroke\n"
           "grestore\n"
           "} bind def\n"
           );

   /* Recode font function */
   fprintf(out,
           "/Recode {\n"
           "exch\n"
           "findfont\n"
           "dup length dict\n"
           "begin\n"
           "{ def\n"
           "} forall\n"
           "/Encoding ISOLatin1Encoding def\n"
           "currentdict\n"
           "end\n"
           "definefont pop\n"
           "} bind def\n");
   fprintf(out,
           "/Times-Roman  /Times-Roman-ISOLatin1 Recode\n"
           "/Courier      /Courier-ISOLatin1 Recode\n"
           "/Courier-Bold /Courier-Bold-ISOLatin1 Recode\n");
   fprintf(out,
          "/hline {\n"
           "currentpoint 1 add currentpoint 1 add\n"
           "currentpoint 4 add currentpoint 4 add\n"
           "gsave\n"
           "newpath\n"
           "moveto\n"
           "exch\n"
           "1.0 inch add\n"
           "exch\n"
           "7 setlinewidth\n"
           "lineto\n"
           "stroke\n"
           "%%\n"
           "newpath\n"
           "moveto\n"
           "exch\n"
           "7.5 inch add\n"
           "exch\n"
           "1 setlinewidth\n"
           "lineto\n"
           "stroke\n"
           "grestore\n"
           "} bind def\n"
           "%%\n"
           "%%\n");
   fprintf(out,
           "/setup\n"
           "{\n"
           "/Times-Roman-ISOLatin1 10 selectfont\n"
           "left footer moveto\n"
           "(%s) show\n"
           "7.5 inch footer moveto\n"
           "(J-Pilot) show\n"
           "%% This assumes that the prev page number is on the stack\n"
           "4.25 inch footer moveto\n"
           "1 add dup printobject show\n"
           "/Courier-ISOLatin1 12 selectfont\n"
           "left top moveto\n"
           "} bind def\n"
           "/printit\n"
           "{\n"
           "{ %%loop\n"
           "currentfile buffer readline { %%ifelse\n"
           "("Q_FLAG_CHAR"LINEFEED) search { %%if\n"
           "pop pop pop showpage setup ( )\n"
           "currentpoint 14 add moveto\n"
           "} if\n"
           "("Q_FLAG_CHAR"HLINE) search { %%if\n"
           "currentpoint exch pop bottom_hline le { %%if\n"
           "pop pop pop\n"
           "showpage setup\n"
           "0 0 0\n"
           "} if\n"
           "hline\n"
           "pop pop pop ( )\n"
           "} if\n"
           "("Q_FLAG_CHAR"END) search { %%if\n"
           "   showpage stop\n"
           "} if\n"
           "("Q_FLAG_CHAR"C12) search {\n"
           "/Courier-ISOLatin1 12 selectfont\n"
           "currentpoint 14 add moveto\n"
           "pop pop pop ( )\n"
           "} if\n"
           "("Q_FLAG_CHAR"CB12) search {\n"
           "/Courier-Bold-ISOLatin1 12 selectfont\n"
           "currentpoint 14 add moveto\n"
           "pop pop pop ( )\n"
           "} if\n",
           str
           );
   /* Check box */
   fprintf(out,
           "("Q_FLAG_CHAR"CHECKBOX) search {\n"
           "currentpoint exch pop bottom_hline le {\n"
           "pop pop pop\n"
           "showpage setup\n"
           "0 0 0\n"
           "} if\n"
           "checkbox\n"
           "currentpoint 14 add moveto\n"
           "pop pop pop ( )\n"
           "} if\n"
           );
   /* Check box */
   fprintf(out,
           "("Q_FLAG_CHAR"CHECKEDBOX) search {\n"
           "currentpoint exch pop bottom_hline le {\n"
           "pop pop pop\n"
           "showpage setup\n"
           "0 0 0\n"
           "} if\n"
           "checkedbox\n"
           "currentpoint 14 add moveto\n"
           "pop pop pop ( )\n"
           "} if\n"
           );
   fprintf(out,
           "%%%%EndResource\n"
           "%%%%EndProlog\n"); /* XXX not exactly sure about position */

   fprintf(out,
           "gsave show grestore\n"
           "currentpoint 14 sub moveto\n"
           "currentpoint exch pop bottom le { %%if\n"
           "showpage setup\n"
           "} if\n"
           "}{ %%else\n"
           "showpage exit\n"
           "} ifelse\n"
           "} loop\n"
           "} bind def\n"
           "0 %%The page number minus 1\n"
           "setup printit\n"
           );
   return EXIT_SUCCESS;
}


int print_contacts(ContactList *contact_list, 
                   struct ContactAppInfo *contact_app_info,
                   address_schema_entry *schema, int schema_size)
{
   long one_rec_per_page;
   long lines_between_recs;
   ContactList *temp_cl;
   MyContact *mcont;
   int show1, show2, show3;
   int i;
   int address_i, phone_i, IM_i;
   char str[100];
   char spaces[24];
   char birthday_str[255];
   const char *pref_date;
   char *utf;
   long char_set;
#ifdef HAVE_LOCALE_H
   char *current_locale;
#endif

   out = print_open();
   if (!out) {
      return EXIT_FAILURE;
   }

#ifdef HAVE_LOCALE_H
   current_locale = setlocale(LC_NUMERIC,"C");
#endif

   memset(spaces, ' ', sizeof(spaces));

   get_pref(PREF_CHAR_SET, &char_set, NULL);

   print_address_header();

   switch (addr_sort_order) {
    case SORT_BY_LNAME:
    default:
      show1=contLastname;
      show2=contFirstname;
      show3=contCompany;
      break;
    case SORT_BY_FNAME:
      show1=contFirstname;
      show2=contLastname;
      show3=contCompany;
      break;
    case SORT_BY_COMPANY:
      show1=contCompany;
      show2=contLastname;
      show3=contFirstname;
      break;
   }

   get_pref(PREF_PRINT_ONE_PER_PAGE, &one_rec_per_page, NULL);
   get_pref(PREF_NUM_BLANK_LINES, &lines_between_recs, NULL);

   for (temp_cl = contact_list; temp_cl; temp_cl=temp_cl->next) {

      fprintf(out, "%cHLINE\n", FLAG_CHAR);

      str[0]='\0';
      if (temp_cl->mcont.cont.entry[show1] || temp_cl->mcont.cont.entry[show2]) {
         if (temp_cl->mcont.cont.entry[show1] && temp_cl->mcont.cont.entry[show2]) {
            g_snprintf(str, sizeof(str), "%s, %s", temp_cl->mcont.cont.entry[show1], temp_cl->mcont.cont.entry[show2]);
         }
         if (temp_cl->mcont.cont.entry[show1] && ! temp_cl->mcont.cont.entry[show2]) {
            strncpy(str, temp_cl->mcont.cont.entry[show1], 48);
         }
         if (! temp_cl->mcont.cont.entry[show1] && temp_cl->mcont.cont.entry[show2]) {
            strncpy(str, temp_cl->mcont.cont.entry[show2], 48);
         }
      } else if (temp_cl->mcont.cont.entry[show3]) {
            strncpy(str, temp_cl->mcont.cont.entry[show3], 48);
      } else {
            strcpy(str, "-Unnamed-");
      }

      courier_bold_12();
      fprintf(out, "%s\n", str);
      courier_12();

      mcont = &(temp_cl->mcont);
      address_i=phone_i=IM_i=0;
      for (i=0; i<schema_size; i++) {
         /* Get the entry texts */
         if (mcont->cont.entry[schema[i].record_field]) {
            switch (schema[i].type) {
             case ADDRESS_GUI_IM_MENU_TEXT:
               g_snprintf(str, 18, "%s:%s", contact_app_info->IMLabels[mcont->cont.IMLabel[IM_i]], spaces);
               fprintf(out, "%s", str);
               IM_i++;
               break;
             case ADDRESS_GUI_DIAL_SHOW_PHONE_MENU_TEXT:
               g_snprintf(str, 18, "%s:%s", contact_app_info->phoneLabels[mcont->cont.phoneLabel[phone_i]], spaces);
               fprintf(out, "%s", str);
               phone_i++;
               break;
             case ADDRESS_GUI_ADDR_MENU_TEXT:
               g_snprintf(str, 18, "%s:%s", contact_app_info->addrLabels[mcont->cont.addressLabel[address_i]], spaces);
               fprintf(out, "%s", str);
               address_i++;
               break;
             default:
               if (contact_app_info->labels[schema[i].record_field]) {
                  utf = charset_p2newj(contact_app_info->labels[schema[i].record_field], 16, char_set);
                  g_snprintf(str, 18, "%s:%s", utf, spaces);
                  fprintf(out, "%s", str);
                  g_free(utf);
               }
               else {
                  g_snprintf(str, 18, ":%s", spaces);
                  fprintf(out, "%s", str);
               }
            }
            switch (schema[i].type) {
             case ADDRESS_GUI_LABEL_TEXT:
             case ADDRESS_GUI_DIAL_SHOW_PHONE_MENU_TEXT:
             case ADDRESS_GUI_IM_MENU_TEXT:
             case ADDRESS_GUI_ADDR_MENU_TEXT:
             case ADDRESS_GUI_WEBSITE_TEXT:
               f_indent_print(out, 17, mcont->cont.entry[schema[i].record_field]);
               fprintf(out, "\n");
               break;
             case ADDRESS_GUI_BIRTHDAY:
               if (mcont->cont.birthdayFlag) {
                  birthday_str[0]='\0';
                  get_pref(PREF_SHORTDATE, NULL, &pref_date);
                  strftime(birthday_str, sizeof(birthday_str), pref_date, &(mcont->cont.birthday));
                  g_snprintf(str, 18, "%s:%s", contact_app_info->labels[schema[i].record_field] ? contact_app_info->labels[schema[i].record_field] : "",
                             spaces);
                  fprintf(out, "%s", str);
                  f_indent_print(out, 17, birthday_str);
                  fprintf(out, "\n");
               }
               break;
            }
         }
      }

      if (one_rec_per_page) {
         fprintf(out, "%cLINEFEED\n", FLAG_CHAR);
      } else {
         for (i=lines_between_recs; i>0; i--) {
            fprintf(out, "\n");
         }
      }
   }

   print_close(out);

#ifdef HAVE_LOCALE_H
   setlocale(LC_ALL, current_locale);
#endif

   return EXIT_SUCCESS;
}

/*
 * ToDo code
 */
int print_todos(ToDoList *todo_list, char *category_name)
{
   long one_rec_per_page;
   long lines_between_recs;
   ToDoList *temp_l;
   struct ToDo *todo;
   int indent;
   const char *datef;
   char str[100];
   time_t ltime;
   struct tm *now;
#ifdef HAVE_LOCALE_H
   char *current_locale;
#endif

   out = print_open();
   if (!out) {
      return EXIT_FAILURE;
   }

#ifdef HAVE_LOCALE_H
   current_locale = setlocale(LC_NUMERIC,"C");
#endif

   fprintf(out, "%%!PS-Adobe-2.0\n\n"
           "/PageSize (%s) def\n\n", PaperSizes[PAPER_Letter]);
   print_common_prolog(out);
   print_common_setup(out);
   fprintf(out, "/CategoryName (%s) def\n", category_name);
   print_todo_header(out);

   get_pref(PREF_PRINT_ONE_PER_PAGE, &one_rec_per_page, NULL);
   get_pref(PREF_NUM_BLANK_LINES, &lines_between_recs, NULL);

   get_pref(PREF_SHORTDATE, NULL, &datef);
   time(&ltime);
   now = localtime(&ltime);
   strftime(str, sizeof(str), datef, now);
   indent=strlen(str) + 8;

   for (temp_l = todo_list; temp_l; temp_l=temp_l->next) {
      todo = &(temp_l->mtodo.todo);

      fprintf(out, todo->complete ? "true  " : "false ");

      fprintf(out, "%d ", todo->priority);

      if (todo->indefinite) {
         sprintf(str, "%s           ", "No Due");
         str[indent-8]='\0';
      } else {
         strftime(str, sizeof(str), datef, &(todo->due));
      }
      fprintf(out, "(%s) ", str);

      if (todo->description) {
         int len;
         char *buf;

         len = strlen(todo->description);
         buf = malloc(2 * len + 1);
         memset(buf, 0, 2 * len + 1);
         ps_strncat(buf, todo->description, 2 * len);

         fprintf(out, "(%s) ", buf);
         free(buf);
      } else {
         fprintf(out, "() ");
      }

      if ((todo->note) && todo->note[0]) {
         int len;
         char *buf;

         len = strlen(todo->note);
         buf = malloc(2 * len + 1);
         memset(buf, 0, 2 * len + 1);
         ps_strncat(buf, todo->note, 2 * len);

         fprintf(out, "(%s) ", buf);
         free(buf);
      } else {
         fprintf(out, "()");
      }
      fprintf(out, " Todo\n");

      if (one_rec_per_page) {
         fprintf(out, "NewPage\n");
      }
   }
   fprintf(out, "showpage\n");

   print_close(out);

#ifdef HAVE_LOCALE_H
   setlocale(LC_ALL, current_locale);
#endif

   return EXIT_SUCCESS;
}
/*
 * Memo code
 */
int print_memos(MemoList *memo_list)
{
   long one_rec_per_page;
   long lines_between_recs;
   MemoList *temp_l;
   struct Memo *memo;
   int i;
#ifdef HAVE_LOCALE_H
   char *current_locale;
#endif

   out = print_open();
   if (!out) {
      return EXIT_FAILURE;
   }

#ifdef HAVE_LOCALE_H
   current_locale = setlocale(LC_NUMERIC,"C");
#endif

   print_address_header();

   get_pref(PREF_PRINT_ONE_PER_PAGE, &one_rec_per_page, NULL);
   get_pref(PREF_NUM_BLANK_LINES, &lines_between_recs, NULL);

   courier_12();

   for (temp_l = memo_list; temp_l; temp_l=temp_l->next) {
      memo = &(temp_l->mmemo.memo);

      if (memo->text) {
         fprintf(out, "%cHLINE\n", FLAG_CHAR);
         f_indent_print(out, 0, memo->text);
         fprintf(out, "\n");
      }

      if (one_rec_per_page) {
         fprintf(out, "%cLINEFEED\n", FLAG_CHAR);
      } else {
         for (i=lines_between_recs; i>0; i--) {
            fprintf(out, "\n");
         }
      }
   }

   fprintf(out, "%cEND\n", FLAG_CHAR);

#ifdef HAVE_LOCALE_H
   setlocale(LC_ALL, current_locale);
#endif

   print_close(out);

   return EXIT_SUCCESS;
}


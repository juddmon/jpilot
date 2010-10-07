/* $Id: datebook.c,v 1.69 2010/10/07 21:04:31 rikster5 Exp $ */

/*******************************************************************************
 * datebook.c
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

/********************************* Includes ***********************************/
#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>

#include <pi-source.h>
#include <pi-socket.h>
#include <pi-calendar.h>
#include <pi-dlp.h>
#include <pi-file.h>

#include "i18n.h"
#include "utils.h"
#include "datebook.h"
#include "calendar.h"
#include "log.h"
#include "prefs.h"
#include "libplugin.h"
#include "password.h"

/****************************** Main Code *************************************/
int appointment_on_day_list(int mon, int year, int *mask, 
                            int category, int datebook_version)
{
   struct tm tm_dom;
   CalendarEventList *tcel, *cel;
   int dow, ndim, num;
   int bit;
   int show_priv;
   int skip_privates;

   memset(&tm_dom, 0, sizeof(tm_dom));
   tm_dom.tm_hour=11;
   tm_dom.tm_mday=1;
   tm_dom.tm_mon=mon;
   tm_dom.tm_year=year;

   cel = NULL;
   /* Get private records back
    * We want to highlight a day with a private record if we are showing or
    * masking private records */
   if (datebook_version) {
      /* Calendar supports category option */
      num = get_days_calendar_events2(&cel, NULL, 2, 2, 1, category, NULL);
   } else {
      num = get_days_calendar_events2(&cel, NULL, 2, 2, 1, CATEGORY_ALL, NULL);
   }

   show_priv = show_privates(GET_PRIVATES);
   skip_privates = (show_priv==HIDE_PRIVATES);

   get_month_info(mon, 1, year, &dow, &ndim);

   *mask = 0;

   weed_calendar_event_list(&cel, mon, year, skip_privates, mask);

   for (tm_dom.tm_mday=1, bit=1; tm_dom.tm_mday<=ndim; tm_dom.tm_mday++, bit=bit<<1) {
      if (*mask & bit) {
         continue;
      }
      
      mktime(&tm_dom);
      for (tcel=cel; tcel; tcel = tcel->next) {
         if (calendar_isApptOnDate(&(tcel->mcale.cale), &tm_dom)) {
            *mask = *mask | bit;
            break;
         }
      }
   }
   free_CalendarEventList(&cel);

   return EXIT_SUCCESS;
}

/* returns 0 if times equal */
/* returns 1 if time1 is greater (later) */
/* returns 2 if time2 is greater (later) */
int compareTimesToDay(struct tm *tm1, struct tm *tm2)
{
   unsigned int t1, t2;

   t1 = tm1->tm_year*366+tm1->tm_yday;
   t2 = tm2->tm_year*366+tm2->tm_yday;
   if (t1 > t2 ) return 1;
   if (t1 < t2 ) return 2;
   return 0;
}

/* Year is years since 1900 */
/* Mon is 0-11 */
/* Day is 1-31 */
/* */
int datebook_add_exception(struct CalendarEvent *cale, int year, int mon, int day)
{
   struct tm *new_exception, *Ptm;

   if (cale->exceptions==0) {
      cale->exception=NULL;
   }

   new_exception = malloc((cale->exceptions + 1) * sizeof(struct tm));
   if (!new_exception) {
      jp_logf(JP_LOG_WARN, "datebook_add_exception(): %s\n", _("Out of memory"));
      return EXIT_FAILURE;
   }
   memcpy(new_exception, cale->exception, (cale->exceptions) * sizeof(struct tm));
   free(cale->exception);
   cale->exceptions++;
   cale->exception = new_exception;
   Ptm = &(cale->exception[cale->exceptions - 1]);
   Ptm->tm_year = year;
   Ptm->tm_mon = mon;
   Ptm->tm_mday = day;
   Ptm->tm_hour = 0;
   Ptm->tm_min = 0;
   Ptm->tm_sec = 0;
   Ptm->tm_isdst = -1;
   mktime(Ptm);
   return EXIT_SUCCESS;
}

/*
 * If a copy is made, then it should be freed through free_Appointment
 */
int datebook_copy_appointment(struct Appointment *a1,
                              struct Appointment **a2)
{
   long datebook_version;

   get_pref(PREF_DATEBOOK_VERSION, &datebook_version, NULL);

   *a2=malloc(sizeof(struct Appointment));
   if (!(*a2)) {
      jp_logf(JP_LOG_WARN, "datebook_copy_appointment(): %s\n", _("Out of memory"));
      return EXIT_FAILURE;
   }
   memcpy(*a2, a1, sizeof(struct Appointment));

   (*a2)->exception = malloc(a1->exceptions * sizeof(struct tm));
   if (!(*a2)->exception) {
      jp_logf(JP_LOG_WARN, "datebook_copy_appointment(): %s 2\n", _("Out of memory"));
      return EXIT_FAILURE;
   }
   memcpy((*a2)->exception, a1->exception, a1->exceptions * sizeof(struct tm));

   if (a1->description) {
      (*a2)->description=strdup(a1->description);
   }
   if (a1->note) {
      (*a2)->note=strdup(a1->note);
   }

   return EXIT_SUCCESS;
}

/*
 * If a copy is made, then it should be freed through free_CalendarEvent
 */
int copy_calendar_event(const struct CalendarEvent *source,
                        struct CalendarEvent **dest)
{
   int r;

   *dest = malloc(sizeof(struct CalendarEvent));
   r = copy_CalendarEvent(source, *dest);

   if (!r) {
      return EXIT_SUCCESS;
   }
   return EXIT_FAILURE;
}

int datebook_sort(AppointmentList **al,
                  int (*compare_func)(const void*, const void*))
{
   AppointmentList *temp_al;
   AppointmentList **sort_al;
   int count, i;

   /* Count the entries in the list */
   for (count=0, temp_al=*al; temp_al; temp_al=temp_al->next, count++) {}

   if (count<2) {
      /* No need to sort 0 or 1 items */
      return EXIT_SUCCESS;
   }

   /* Allocate an array to be qsorted */
   sort_al = calloc(count, sizeof(AppointmentList *));
   if (!sort_al) {
      jp_logf(JP_LOG_WARN, "datebook_sort(): %s\n", _("Out of memory"));
      return EXIT_FAILURE;
   }

   /* Set our array to be a list of pointers to the nodes in the linked list */
   for (i=0, temp_al=*al; temp_al; temp_al=temp_al->next, i++) {
      sort_al[i] = temp_al;
   }

   /* qsort them */
   qsort(sort_al, count, sizeof(AppointmentList *), compare_func);

   /* Put the linked list in the order of the array */
   sort_al[count-1]->next = NULL;
   for (i=count-1; i; i--) {
      sort_al[i-1]->next=sort_al[i];
   }

   *al = sort_al[0];

   free(sort_al);

   return EXIT_SUCCESS;
}

#ifdef ENABLE_DATEBK
static void db3_fill_struct(char *note, int type, struct db4_struct *db4)
{
   /* jp_logf(JP_LOG_WARN, "db3_fill_struct()\n"); */
   switch (note[2]) {
    case 'c':
    case 'C':
      db4->floating_event=DB3_FLOAT_COMPLETE;
      break;
    case 'd':
      db4->floating_event=DB3_FLOAT_DONE;
      break;
    case 'f':
    case 'F':
      db4->floating_event=DB3_FLOAT;
      break;
   }
   if (type==DB3_TAG_TYPE_DBplus) {
      return;
   }
   switch (note[3]) {
    case 'b':
      db4->custom_font=DB3_FONT_BOLD;
      break;
    case 'l':
      db4->custom_font=DB3_FONT_LARGE;
      break;
    case 'L':
      db4->custom_font=DB3_FONT_LARGE_BOLD;
      break;
   }
   db4->category = (note[4] - '@') & 0x0F;
   db4->icon = (note[5] - '@');
   if (note[6] == 's') {
      db4->spans_midnight = DB3_SPAN_MID_YES;
   }
   /* bytes 8,9 I don't understand yet */
   if (type==DB3_TAG_TYPE_DB3) {
      return;
   }
   if (note[9] == 'l') {
      db4->link = DB3_LINK_YES;
   }
   /* bytes 11-14 I don't understand yet */
   /* bytes 15-18 lower 6 bits make 24 bit number (not sure of byte order) */
   db4->custom_alarm_sound = ((note[14] & 0x3F) << 18) +
     ((note[15] & 0x3F) << 12) +
     ((note[16] & 0x3F) << 6) +
     ((note[17] & 0x3F) << 0);
   db4->color = (note[18] - '@') & 0x0F;
   /* Byte 19 is a carriage return */
}

// FIXME: verify that new routine calendar_db3_hack_date in calendar.c
// works for datebook databases as well and then remove.
#if 0
static int db3_hack_date(struct Appointment *appt, struct tm *today)
{
   int t1, t2;

   if (today==NULL) {
      return EXIT_SUCCESS;
   }
   if (!appt->note) {
      return EXIT_SUCCESS;
   }
   if (strlen(appt->note) > 8) {
      if ((appt->note[0]=='#') && (appt->note[1]=='#')) {
         if (appt->note[2]=='f' || appt->note[2]=='F') {
            /* Check to see if its in the future */
            t1 = appt->begin.tm_mday + appt->begin.tm_mon*31 + appt->begin.tm_year*372;
            t2 = today->tm_mday + today->tm_mon*31 + today->tm_year*372;
            if (t1 > t2) return EXIT_SUCCESS;
            /* We found some silly hack, so we lie about the date */
            /* memcpy(&(appt->begin), today, sizeof(struct tm));*/
            /* memcpy(&(appt->end), today, sizeof(struct tm));*/
            appt->begin.tm_mday = today->tm_mday;
            appt->begin.tm_mon = today->tm_mon;
            appt->begin.tm_year = today->tm_year;
            appt->begin.tm_wday = today->tm_wday;
            appt->begin.tm_yday = today->tm_yday;
            appt->begin.tm_isdst = today->tm_isdst;
            appt->end.tm_mday = today->tm_mday;
            appt->end.tm_mon = today->tm_mon;
            appt->end.tm_year = today->tm_year;
            appt->end.tm_wday = today->tm_wday;
            appt->end.tm_yday = today->tm_yday;
            appt->end.tm_isdst = today->tm_isdst;
            /* If the appointment has an end date, and today is past the end
             * date, because of this hack we would never be able to view
             * it anymore (or delete it).  */
            if (!(appt->repeatForever)) {
               if (compareTimesToDay(today, &(appt->repeatEnd))==1) {
                  /* end date is before start date, illegal appointment */
                  /* make it legal, by only floating up to the end date */
                  memcpy(&(appt->begin), &(appt->repeatEnd), sizeof(struct tm));
                  memcpy(&(appt->end), &(appt->repeatEnd), sizeof(struct tm));
               }
            }
         }
      }
   }
   return EXIT_SUCCESS;
}
#endif

/*
 * Parses the note tag and looks for db3 or db4 tags.
 * returns -1: error, 0: false, 1: true
 * for true: will in db4
 *
 * db4 can be passed in NULL for just checking for datebk tags.
 */
int db3_parse_tag(char *note, int *type, struct db4_struct *db4)
{
   /* Characters allowed to exist in each character of a db3/4 tag */
   /* NULL means any character is allowed */
   char *allowed[]={
      "#", "#", /* First 2 characters are # */
        "@FfCcd", /* F or f floating, C or c completed, d done */
        "@blL", /* b bold, l large, L large bold */
        "@ABCDEFGHIJKLMNO", /* Category */
        "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_abcdefghijklmnopqrst", /* icon */
        "@s", /* Spans midnight */
        NULL, /* Lower 2 digits of next 2 chars are time zone */
        NULL,
        "@l\n", /* l link, EOL for datebk3 tags */
        NULL, /* Lower 6 bits of next 4 chars make a 24 bit number (advance) */
        NULL, /* I don't understand this yet and have not coded it */
        NULL,
        NULL,
        NULL, /* Lower 6 bits of next 4 chars make a 24 bit number */
        NULL, /*  which is the custom sound */
        NULL,
        NULL,
        "@ABCDEFGHIJKLMNO", /* Color */
        "\n", /* EOL for datebk4 tags */
   };
   int len, i;

   /* We need to guarantee these to be set upon return */
   if (db4) {
      memset(db4, 0, sizeof(*db4));
   }
   *type=DB3_TAG_TYPE_NONE;

   if (!note) {
      return 0;
   }

   len = strlen(note);

   for (i=0; i<20; i++) {
      if (i+1>len) {
         return 0;
      }
      if ((i==3) && (note[i]=='\n')) {
         /* If we made it this far and CR then its a db+ tag */
         *type=DB3_TAG_TYPE_DBplus;
         if (note[i+1]) {
            if (db4) {
               db4->note=&(note[i+1]);
            }
         }
         if (db4) {
            db3_fill_struct(note, DB3_TAG_TYPE_DBplus, db4);
         }
         return 1;
      }
      if ((i==9) && (note[i]=='\n')) {
         /* If we made it this far and CR then its a db3 tag */
         *type=DB3_TAG_TYPE_DB3;
         if (note[i+1]) {
            if (db4) {
               db4->note=&(note[i+1]);
            }
         }
         if (db4) {
            db3_fill_struct(note, DB3_TAG_TYPE_DB3, db4);
         }
         return 1;
      }
      if (allowed[i]==NULL) continue;
      if (!strchr(allowed[i], note[i])) {
         return 0;
      }
   }

   /* If we made it this far then its a db4 tag */
   *type=DB3_TAG_TYPE_DB4;
   if (note[i]) {
      if (db4) {
         db4->note=&(note[i]);
      }
   }
   if (db4) {
      db3_fill_struct(note, DB3_TAG_TYPE_DB4, db4);
   }
   return 1;
}
#endif

void free_AppointmentList(AppointmentList **al)
{
   AppointmentList *temp_al, *temp_al_next;
   for (temp_al = *al; temp_al; temp_al=temp_al_next) {
      free_Appointment(&(temp_al->mappt.appt));
      temp_al_next = temp_al->next;
      free(temp_al);
   }
   *al = NULL;
}

int get_calendar_or_datebook_app_info(struct CalendarAppInfo *cai, 
                                      long datebook_version)
{
   int num, r;
   int rec_size;
   unsigned char *buf;
   char DBname[32];
   struct AppointmentAppInfo aai;
   pi_buffer_t pi_buf;

   memset(&aai, 0, sizeof(aai));
   memset(cai, 0, sizeof(*cai));
   /* Put at least one entry in there */
   strcpy(aai.category.name[0], "Unfiled");
   strcpy(cai->category.name[0], "Unfiled");

   if (datebook_version) {
      strcpy(DBname, "CalendarDB-PDat");
   } else {
      strcpy(DBname, "DatebookDB");
   }
      
   r = jp_get_app_info(DBname, &buf, &rec_size);
   if ((r != EXIT_SUCCESS) || (rec_size<=0)) {
      jp_logf(JP_LOG_WARN, _("%s:%d Error reading application info %s\n"), __FILE__, __LINE__, DBname);
      if (buf) {
         free(buf);
      }
      return EXIT_FAILURE;
   }

   pi_buf.data = buf;
   pi_buf.used = rec_size;
   pi_buf.allocated = rec_size;

   if (datebook_version) {
      num = unpack_CalendarAppInfo(cai, &pi_buf);
   } else {
      num = unpack_AppointmentAppInfo(&aai, buf, rec_size);
   }
   if (buf) {
      free(buf);
   }
   if ((num<0) || (rec_size<=0)) {
      jp_logf(JP_LOG_WARN, _("Error reading file: %s\n"), DBname);
      return EXIT_FAILURE;
   }

   if (datebook_version==0) {
      copy_appointment_ai_to_calendar_ai(&aai, cai);
   }

   return EXIT_SUCCESS;
}

/* The following 3 routines are slow because they copy everything from 
   CalenderEvents over to Appointments.  This is not troublesome because
   only jpilot-dump still uses these routines.  Everything else uses the
   updated calendar versions */
int get_days_appointments(AppointmentList **appointment_list, struct tm *now,
                          int *total_records)
{
   return get_days_appointments2(appointment_list, now, 1, 1, 1, total_records);
}

/*
 * If Null is passed in for date, then all appointments will be returned
 * modified, deleted and private, 0 for no, 1 for yes, 2 for use prefs
 */
int get_days_appointments2(AppointmentList **appointment_list, struct tm *now,
                           int modified, int deleted, int privates,
                           int *total_records)
{
   int r;
   CalendarEventList *cel;

   r = get_days_calendar_events2(&cel, now, modified, deleted, privates, CATEGORY_ALL, total_records);
   copy_calendarEvents_to_appointments(cel, appointment_list);

   free_CalendarEventList(&cel); 

   return r;
}

unsigned int isApptOnDate(struct Appointment *appt, struct tm *date)
{
   struct CalendarEvent cale;
   int r;

   copy_appointment_to_calendarEvent(appt, &cale);
   r = calendar_isApptOnDate(&cale, date);
   free_CalendarEvent(&cale);

   return r;
}

unsigned int calendar_isApptOnDate(struct CalendarEvent *cale, struct tm *date)
{
   unsigned int ret;
   unsigned int r;
   int week1, week2;
   int dow, ndim;
   int i;
   /* days_in_month is adjusted for leap year with the date structure */
   int days_in_month[]={31,28,31,30,31,30,31,31,30,31,30,31};
   int exception_days;
   static int days, begin_days;

   jp_logf(JP_LOG_DEBUG, "calendar_isApptOnDate\n");

   ret = FALSE;

   if (!date) {
      return FALSE;
   }

   /* Leap year */
   if ((date->tm_year%4 == 0) &&
       !(((date->tm_year+1900)%100==0) && ((date->tm_year+1900)%400!=0))
       ) {
      days_in_month[1]++;
   }

   /* See if the appointment starts after date */
   r = compareTimesToDay(&(cale->begin), date);
   if (r == 1) {
      return FALSE;
   }
   if (r == 0) {
      ret = TRUE;
   }
   /* If the appointment has an end date, see that we are not past it */
   if (!(cale->repeatForever)) {
      r = compareTimesToDay(&(cale->repeatEnd), date);
      if (r == 2) {
         return FALSE;
      }
   }

   switch (cale->repeatType) {
    case calendarRepeatNone:
      break;
    case calendarRepeatDaily:
      /* See if this appt repeats on this day */
      begin_days = dateToDays(&(cale->begin));
      days = dateToDays(date);

      ret = (((days - begin_days)%(cale->repeatFrequency))==0);
      break;
    case calendarRepeatWeekly:
      get_month_info(date->tm_mon, date->tm_mday, date->tm_year, &dow, &ndim);
      /* See if the appointment repeats on this day */
      if (!(cale->repeatDays[dow])) {
         ret = FALSE;
         break;
      }
      /* See if we are in a week that is repeated in */
      begin_days = dateToDays(&(cale->begin));
      days = dateToDays(date);

      /* In repeatWeekly Palm treats the week as running Monday-Sunday [0-6]
       * This contrasts with the C time structures which run Sunday-Sat[0-6]
       * The date calculation requires switching between the two.
       * Palm tm structure = C tm structure -1 with wraparound for Sunday */
      if (cale->begin.tm_wday == 0)
      {
         ret = (days - begin_days) + 6;
      } else {
         ret = (days - begin_days) + (cale->begin.tm_wday - 1);
      }
      ret = ((((int)ret/7) % cale->repeatFrequency) == 0);
      break;
    case calendarRepeatMonthlyByDay:
      /* See if we are in a month that is repeated in */
      ret = (((date->tm_year - cale->begin.tm_year)*12 +
       (date->tm_mon - cale->begin.tm_mon))%(cale->repeatFrequency)==0);
      if (!ret) {
         break;
      }
      /* If the days of the week match - good */
      /* e.g. Monday or Thur, etc. */
      if (cale->repeatDay%7 != date->tm_wday) {
         ret = FALSE;
         break;
      }
      /* Are they both in the same week in the month */
      /* e.g. The 3rd Mon, or the 2nd Fri, etc. */
      week1 = cale->repeatDay/7;
      week2 = (date->tm_mday - 1)/7;
      if (week1 != week2) {
         ret = FALSE;
      }
      /* See if the appointment repeats on the last week of the month */
      /* and this is the 4th, and last. */
      if (week1 > 3) {
         if ((date->tm_mday + 7) > days_in_month[date->tm_mon]) {
            ret = TRUE;
         }
      }
      break;
    case calendarRepeatMonthlyByDate:
      /* See if we are in a repeating month */
      ret = (((date->tm_year - cale->begin.tm_year)*12 +
       (date->tm_mon - cale->begin.tm_mon))%(cale->repeatFrequency) == 0);
      if (!ret) {
         break;
      }
      /* See if this is the date that the appt repeats on */
      if (date->tm_mday == cale->begin.tm_mday) {
         ret = TRUE;
         break;
      }
      /* If appt occurs after the last day of the month and this date */
      /* is the last day of the month then it occurs today */
      ret = ((cale->begin.tm_mday > days_in_month[date->tm_mon]) &&
             (date->tm_mday == days_in_month[date->tm_mon]));
      break;
    case calendarRepeatYearly:
      if ((date->tm_year - cale->begin.tm_year)%(cale->repeatFrequency) != 0) {
         ret = FALSE;
         break;
      }
      if ((date->tm_mday == cale->begin.tm_mday) &&
          (date->tm_mon == cale->begin.tm_mon)) {
         ret = TRUE;
         break;
      }
      /* Take care of Feb 29th (Leap Day) */
      if ((cale->begin.tm_mon == 1) && (cale->begin.tm_mday == 29) &&
        (date->tm_mon == 1) && (date->tm_mday == 28)) {
         ret = TRUE;
         break;
      }
      break;
    default:
      jp_logf(JP_LOG_WARN, _("Unknown repeatType (%d) found in DatebookDB\n"),
           cale->repeatType);
      ret = FALSE;
   }/* switch */

   /* Check for exceptions */
   if (ret && cale->exceptions) {
      days = dateToDays(date);
      for (i=0; i<cale->exceptions; i++) {
#ifdef JPILOT_DEBUG
         jp_logf(JP_LOG_DEBUG, "exception %d mon %d\n", i, cale->exception[i].tm_mon);
         jp_logf(JP_LOG_DEBUG, "exception %d day %d\n", i, cale->exception[i].tm_mday);
         jp_logf(JP_LOG_DEBUG, "exception %d year %d\n", i, cale->exception[i].tm_year);
         jp_logf(JP_LOG_DEBUG, "exception %d yday %d\n", i, cale->exception[i].tm_yday);
         jp_logf(JP_LOG_DEBUG, "today is yday %d\n", date->tm_yday);
#endif
         exception_days = dateToDays(&(cale->exception[i]));
         if (exception_days == days) {
            ret = FALSE;
            break;
         }
      }
   }

   return ret;
}

int weed_calendar_event_list(CalendarEventList **cel, int mon, int year,
                             int skip_privates, int *mask)
{
   struct tm tm_fdom;
   struct tm tm_ldom;
   struct tm tm_test;
   CalendarEventList *prev_cel, *next_cel, *tcel;
   int r, fdow, ndim;
   int days, begin_days;
   int ret;
   int trash_it;

   memset(&tm_fdom, 0, sizeof(tm_fdom));
   tm_fdom.tm_hour=11;
   tm_fdom.tm_mday=1;
   tm_fdom.tm_mon=mon;
   tm_fdom.tm_year=year;

   get_month_info(mon, 1, year, &fdow, &ndim);

   memcpy(&tm_ldom, &tm_fdom, sizeof(tm_fdom));

   tm_ldom.tm_mday=ndim;

   mktime(&tm_fdom);
   mktime(&tm_ldom);

   memcpy(&tm_test, &tm_fdom, sizeof(tm_fdom));

   days = dateToDays(&tm_fdom);

   next_cel=NULL;
   /* We are going to try to shrink the linked list since we are about to
    * search though it ~30 times.  */
   for (prev_cel=NULL, tcel=*cel; tcel; tcel = next_cel) {
      if (skip_privates && (tcel->mcale.attrib & dlpRecAttrSecret)) {
         trash_it=1;
         goto trash;
      }
      trash_it=0;
      /* See if the appointment starts after the last day of the month */
      r = compareTimesToDay(&(tcel->mcale.cale.begin), &tm_ldom);
      if (r == 1) {
         trash_it=1;
         goto trash;
      }
      /* If the appointment has an end date, see if it ended before the 1st */
      if (!(tcel->mcale.cale.repeatForever)) {
         r = compareTimesToDay(&(tcel->mcale.cale.repeatEnd), &tm_fdom);
         if (r == 2) {
            trash_it=1;
            goto trash;
         }
      }
      /* No repeat */
      /* See if its a non-repeating appointment that is this month/year */
      if (tcel->mcale.cale.repeatType==calendarRepeatNone) {
         if ((tcel->mcale.cale.begin.tm_year==year) &&
             (tcel->mcale.cale.begin.tm_mon==mon)) {
            tm_test.tm_mday=tcel->mcale.cale.begin.tm_mday;
            mktime(&tm_test);
            /* Go ahead and mark this day (highlight it) */
            if (calendar_isApptOnDate(&(tcel->mcale.cale), &tm_test)) {
               *mask = *mask | (1 << ((tcel->mcale.cale.begin.tm_mday)-1));
            }
         } else {
            trash_it=1;
            goto trash;
         }
      }
      /* Daily */
      /* See if this daily appt repeats this month or not */
      if (tcel->mcale.cale.repeatType==calendarRepeatDaily) {
         begin_days = dateToDays(&(tcel->mcale.cale.begin));
         ret = ((days - begin_days)%(tcel->mcale.cale.repeatFrequency));
         if ((ret>31) || (ret<-31)) {
            trash_it=1;
            goto trash;
         }
      }
      /* Weekly */
      if (tcel->mcale.cale.repeatType==calendarRepeatWeekly) {
         begin_days = dateToDays(&(tcel->mcale.cale.begin));
         /* Note: Palm Bug?  I think the palm does this wrong.
          * I prefer this way of doing it so that you can have appts repeating
          * from Wed->Tue, for example.  The palms way prevents this */
         /* ret = (((int)((days - begin_days - fdow)/7))%(appt->repeatFrequency)==0);*/
         /* But, here is the palm way */
         ret = (((int)((days-begin_days+tcel->mcale.cale.begin.tm_wday-fdow)/7))
                %(tcel->mcale.cale.repeatFrequency));
         if ((ret>6) || (ret<-6)) {
            trash_it=1;
            goto trash;
         }
      }
      /* Monthly */
      if ((tcel->mcale.cale.repeatType==calendarRepeatMonthlyByDay) ||
          (tcel->mcale.cale.repeatType==calendarRepeatMonthlyByDate)) {
         /* See if we are in a month that is repeated in */
         ret = (((year - tcel->mcale.cale.begin.tm_year)*12 +
                 (mon - tcel->mcale.cale.begin.tm_mon))%(tcel->mcale.cale.repeatFrequency)==0);
         if (!ret) {
            trash_it=1;
            goto trash;
         }
         if (tcel->mcale.cale.repeatType==calendarRepeatMonthlyByDay) {
            tm_test.tm_mday=tcel->mcale.cale.begin.tm_mday;
            mktime(&tm_test);
            if (calendar_isApptOnDate(&(tcel->mcale.cale), &tm_test)) {
               *mask = *mask | (1 << ((tcel->mcale.cale.begin.tm_mday)-1));
            }
         }
      }
      /* Yearly */
      /* See if its a yearly appointment that does reoccur on this month */
      if (tcel->mcale.cale.repeatType==calendarRepeatYearly) {
         if ((tcel->mcale.cale.begin.tm_mon==mon)) {
            tm_test.tm_mday=tcel->mcale.cale.begin.tm_mday;
            mktime(&tm_test);
            if (calendar_isApptOnDate(&(tcel->mcale.cale), &tm_test)) {
               *mask = *mask | (1 << ((tcel->mcale.cale.begin.tm_mday)-1));
            }
         } else {
            trash_it=1;
            goto trash;
         }
      }
      /* Remove it from this list if it can't help us */
      trash:
      if (trash_it) {
         if (prev_cel) {
            prev_cel->next=tcel->next;
         } else {
            *cel=tcel->next;
         }
         next_cel=tcel->next;
         free_CalendarEvent(&(tcel->mcale.cale));
         free(tcel);
      } else {
         prev_cel=tcel;
         next_cel=tcel->next;
      }
   }
   return EXIT_SUCCESS;
}


/* datebook.c
 *
 * Copyright (C) 1999 by Judd Montgomery
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
 */
#include "config.h"
#include "i18n.h"
#include <stdio.h>
#include <pi-source.h>
#include <pi-socket.h>
#include <pi-datebook.h>
#include <pi-dlp.h>
#include <pi-file.h>
#include <time.h>
/*#include <sys/stat.h> */
/*#include <sys/types.h> */
#include <unistd.h>
#include <utime.h>

#include "datebook.h"
#include "utils.h"
#include "log.h"
#include "prefs.h"
#include "libplugin.h"
#include "password.h"

#include "japanese.h"
#include "cp1250.h"
#include "russian.h"

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

#define DATEBOOK_EOF 7

int datebook_compare(const void *v1, const void *v2)
{
   AppointmentList **al1, **al2;
   struct Appointment *a1, *a2;
   
   al1=(AppointmentList **)v1;
   al2=(AppointmentList **)v2;
   
   a1=&((*al1)->ma.a);
   a2=&((*al2)->ma.a);
   
   /* Jim Rees pointed out my sorting error */
   /* return ((a1->begin.tm_hour*60 + a1->begin.tm_min) > */
   return ((a1->begin.tm_hour*60 + a1->begin.tm_min) -
	   (a2->begin.tm_hour*60 + a2->begin.tm_min));
}

static int datebook_sort(AppointmentList **al)
{
   AppointmentList *temp_al;
   AppointmentList **sort_al;
   int count, i;

   /* Count the entries in the list */
   for (count=0, temp_al=*al; temp_al; temp_al=temp_al->next, count++) {
      ;
   }

   if (count<2) {
      /* We don't have to sort less than 2 items */
      return 0;
   }
   
   /* Allocate an array to be qsorted */
   sort_al = calloc(count, sizeof(AppointmentList *));
   if (!sort_al) {
      jpilot_logf(LOG_WARN, "datebook_sort(): Out of Memory\n");
      return 0;
   }
   
   /* Set our array to be a list of pointers to the nodes in the linked list */
   for (i=0, temp_al=*al; temp_al; temp_al=temp_al->next, i++) {
      sort_al[i] = temp_al;
   }

   /* qsort them */
   qsort(sort_al, count, sizeof(AppointmentList *), datebook_compare);

   /* Put the linked list in the order of the array */
   sort_al[count-1]->next = NULL;
   for (i=count-1; i; i--) {
      sort_al[i-1]->next=sort_al[i];
   }

   *al = sort_al[0];

   free(sort_al);

   return 0;
}

#ifdef USE_DB3
int db3_hack_date(struct Appointment *a, struct tm *today)
{
   int t1, t2;

   if (today==NULL) {
      return 0;
   }
   if (!a->note) {
      return 0;
   }
   if (strlen(a->note) > 8) {
      if ((a->note[0]=='#') && (a->note[1]=='#')) {
	 if (a->note[2]=='f') {
	    /* Check to see if its in the future */
	    t1 = a->begin.tm_mday + a->begin.tm_mon*31 + a->begin.tm_year*372;
	    t2 = today->tm_mday + today->tm_mon*31 + today->tm_year*372;
	    if (t1 > t2) return 0;
	    /* We found some silly hack, so we lie about the date */
	    /*memcpy(&(a->begin), today, sizeof(struct tm));*/
	    /*memcpy(&(a->end), today, sizeof(struct tm));*/
	    a->begin.tm_mday = today->tm_mday;
	    a->begin.tm_mon = today->tm_mon;
	    a->begin.tm_year = today->tm_year;
	    a->begin.tm_wday = today->tm_wday;
	    a->begin.tm_yday = today->tm_yday;
	    a->begin.tm_isdst = today->tm_isdst;
	    a->end.tm_mday = today->tm_mday;
	    a->end.tm_mon = today->tm_mon;
	    a->end.tm_year = today->tm_year;
	    a->end.tm_wday = today->tm_wday;
	    a->end.tm_yday = today->tm_yday;
	    a->end.tm_isdst = today->tm_isdst;
	    /* If the appointment has an end date, and today is past the end
	     * date, because of this hack we would never be able to view
	     * it anymore (or delete it).
	     */
	    if (!(a->repeatForever)) {
	       if (compareTimesToDay(today, &(a->repeatEnd))==1) {
		  /* end date is before start date, illegal appointment */
		  /* make it legal, by only floating upto the end date */
		  memcpy(&(a->begin), &(a->repeatEnd), sizeof(struct tm));
		  memcpy(&(a->end), &(a->repeatEnd), sizeof(struct tm));
	       }
	    }
	 }
      }
   }
   return 0;
}

/* Returns a bitmask
 * 0 if not a floating OR
 * bitmask:
 *  1 if float,
 *  2 if completed float
 *  16 if float has a note
 */
int db3_is_float(struct Appointment *a, int *category)
{
   int len, mask=0;
   
   *category=0;
   if (!a->note) {
      return 0;
   }
   len = strlen(a->note);
   if (len > 8) {
      if ((a->note[0]=='#') && (a->note[1]=='#')) {
	 *category = a->note[4] - '@';
	 if (len > 10) {
	    mask=mask | DB3_FLOAT_HAS_NOTE;
	 }
	 if (a->note[2]=='f') {
	    mask=mask | DB3_FLOAT;
	    return mask;
	 }
	 if (a->note[2]=='c') {
	    mask=mask | DB3_FLOAT_COMPLETE;
	    return mask;
	 }
      } else if (a->note[0] != '\0') {
	 mask=mask | DB3_FLOAT_HAS_NOTE;
      }
   }
   return mask;
}
#endif

int pc_datebook_write(struct Appointment *a, PCRecType rt, unsigned char attrib)
{
   char record[65536];
   int rec_len;
   buf_rec br;

   rec_len = pack_Appointment(a, record, 65535);
   if (!rec_len) {
      PRINT_FILE_LINE;
      jpilot_logf(LOG_WARN, "pack_Appointment %s\n", _("error"));
      return -1;
   }
   br.rt=rt;
   br.attrib = attrib;
   br.buf = record;
   br.size = rec_len;
   
   jp_pc_write("DatebookDB", &br);
   /* *unique_id = br.unique_id;*/
   
   return 0;
}

void free_AppointmentList(AppointmentList **al)
{
   AppointmentList *temp_al, *temp_al_next;
   
   for (temp_al = *al; temp_al; temp_al=temp_al_next) {
      free_Appointment(&(temp_al->ma.a));
      temp_al_next = temp_al->next;
      free(temp_al);
   }
   *al = NULL;
}

/*
 * If a copy is made, then it should be freed through free_Appointment
 */
int datebook_copy_appointment(struct Appointment *a1,
			     struct Appointment **a2)
{
   *a2=malloc(sizeof(struct Appointment));
   if (!(*a2)) {
      jpilot_logf(LOG_WARN, "datebook_copy_appointment(): Out of memory\n");
      return -1;
   }
   memcpy(*a2, a1, sizeof(struct Appointment));
   
   (*a2)->exception = (struct tm *)malloc(a1->exceptions * sizeof(struct tm));
   if (!(*a2)->exception) {
      jpilot_logf(LOG_WARN, "datebook_copy_appointment(): Out of memory 2\n");
      return -1;
   }
   memcpy((*a2)->exception, a1->exception, a1->exceptions * sizeof(struct tm));

   if (a1->description) {
      (*a2)->description=strdup(a1->description);
   }
   if (a1->note) {
      (*a2)->note=strdup(a1->note);
   }

   return 0;
}


/* Year is years since 1900 */
/* Mon is 0-11 */
/* Day is 1-31 */
/* */
int datebook_add_exception(struct Appointment *a, int year, int mon, int day)
{
   struct tm *new_exception, *Ptm;
   
   if (a->exceptions==0) {
      a->exception=NULL;
   }

   new_exception = malloc((a->exceptions + 1) * sizeof(struct tm));
   if (!new_exception) {
      jpilot_logf(LOG_WARN, "datebook_add_exception(): Out of memory\n");
      return -1;
   }
   memcpy(new_exception, a->exception, (a->exceptions) * sizeof(struct tm));
   free(a->exception);
   a->exceptions++;
   a->exception = new_exception;
   Ptm = &(a->exception[a->exceptions - 1]);
   Ptm->tm_year = year;
   Ptm->tm_mon = mon;
   Ptm->tm_mday = day;
   Ptm->tm_hour = 0;
   Ptm->tm_min = 0;
   Ptm->tm_sec = 0;
   Ptm->tm_isdst = -1;
   mktime(Ptm);
   return 0;
}

int dateToSecs(struct tm *tm1)
{
   time_t t1;
   struct tm *gmt;
   struct tm tm2;
   static time_t adj = -1;
   
   memcpy(&tm2, tm1, sizeof(struct tm));
   tm2.tm_isdst = 0;
   tm2.tm_hour=0;
   t1 = mktime(&tm2);
   if (-1 == adj) {
      gmt = gmtime(&t1);
      adj = t1 - mktime(gmt);
   }
   return (t1+adj);
}

int dateToDays(struct tm *tm1)
{
   time_t t1;
   struct tm *gmt;
   struct tm tm2;
   static time_t adj = -1;
   
   memcpy(&tm2, tm1, sizeof(struct tm));
   tm2.tm_isdst = 0;
   tm2.tm_hour=12;
   t1 = mktime(&tm2);
   if (-1 == adj) {
      gmt = gmtime(&t1);
      adj = t1 - mktime(gmt);
   }
   return (t1+adj)/86400; /*There are 86400 secs in a day */
}

/*returns 0 if times equal */
/*returns 1 if time1 is greater (later) */
/*returns 2 if time2 is greater (later) */
/*
int compareTimesToSec(struct tm *tm1, struct tm *tm2)
{
   time_t t1, t2;

   t1 = mktime(tm1);
   t2 = mktime(tm2);
   if (t1 > t2) return 1;
   if (t1 < t2) return 2;
   return 0;
}
*/
/*returns 0 if times equal */
/*returns 1 if time1 is greater (later) */
/*returns 2 if time2 is greater (later) */
int compareTimesToDay(struct tm *tm1, struct tm *tm2)
{
   unsigned int t1, t2;
   
   t1 = tm1->tm_year*366+tm1->tm_yday;
   t2 = tm2->tm_year*366+tm2->tm_yday;
   if (t1 > t2 ) return 1;
   if (t1 < t2 ) return 2;
   return 0;
}

unsigned int isApptOnDate(struct Appointment *a, struct tm *date)
{
   long fdow;
   unsigned int ret;
   unsigned int r;
   int begin_days, days, week1, week2;
   int dow, ndim;
   int i;
   /*days_in_month is adjusted for leap year with the date */
   /*structure */
   int days_in_month[]={31,28,31,30,31,30,31,31,30,31,30,31
   };

   ret = FALSE;
   
   if (!date) {
      return FALSE;
   }

   /*Leap year */
   if ((date->tm_year%4 == 0) &&
       !(((date->tm_year+1900)%100==0) && ((date->tm_year+1900)%400!=0))
       ) {
      days_in_month[1]++;
   }
   
   /*See if the appointment starts after date */
   r = compareTimesToDay(&(a->begin), date);
   if (r == 1) {
      return FALSE;
   }
   if (r == 0) {
      ret = TRUE;
   }
   /*If the appointment has an end date, see that we are not past it */
   if (!(a->repeatForever)) {
      r = compareTimesToDay(&(a->repeatEnd), date);
      if (r == 2) {
	 return FALSE;
      }
   }

   switch (a->repeatType) {
    case repeatNone:
      break;
    case repeatDaily:
      /*See if this appt repeats on this day */
      begin_days = dateToDays(&(a->begin));
      days = dateToDays(date);
      ret = (((days - begin_days)%(a->repeatFrequency))==0);
      break;
    case repeatWeekly:
      get_month_info(date->tm_mon, date->tm_mday, date->tm_year, &dow, &ndim);
      /*See if the appointment repeats on this day */
      /*
      if (a->repeatWeekstart > 1) {
	 a->repeatWeekstart = 1;
      }
      if (a->repeatWeekstart < 0) {
	 a->repeatWeekstart = 0;
      }
      */
      /*if (!(a->repeatDays[dow + a->repeatWeekstart])) {*/
      if (!(a->repeatDays[dow])) {
	 ret = FALSE;
	 break;
      }
      /*See if we are in a week that is repeated in */
      begin_days = dateToDays(&(a->begin));
      days = dateToDays(date);
      get_pref(PREF_FDOW, &fdow, NULL);
      /* Note: Palm Bug?  I think the palm does this wrong.
       * I prefer this way of doing it so that you can have appts repeating
       * from Wed->Tue, for example.  The palms way prevents this */
      /* ret = (((int)((days - begin_days - fdow)/7))%(a->repeatFrequency)==0);*/
      /* But, here is the palm way */
      ret = (((int)((days-begin_days+a->begin.tm_wday-fdow)/7))
	     %(a->repeatFrequency)==0);
      break;
    case repeatMonthlyByDay:
      /*See if we are in a month that is repeated in */
      ret = (((date->tm_year - a->begin.tm_year)*12 +
       (date->tm_mon - a->begin.tm_mon))%(a->repeatFrequency)==0);
      if (!ret) {
	 break;
      }
      /*If the days of the week match - good */
      /*e.g. Monday or Thur, etc. */
      if (a->repeatDay%7 != date->tm_wday) {
	 ret = FALSE;
	 break;
      }
      /*Are they both in the same week in the month */
      /*e.g. The 3rd Mon, or the 2nd Fri, etc. */
      week1 = a->repeatDay/7;
      week2 = (date->tm_mday - 1)/7;
      if (week1 != week2) {
	 ret = FALSE;
      }
      /*See if the appointment repeats on the last week of the month */
      /*and this is the 4th, and last. */
      if (week1 > 3) {
	 if ((date->tm_mday + 7) > days_in_month[date->tm_mon]) {
	    ret = TRUE;
	 }
      }
      break;
    case repeatMonthlyByDate:
      /*See if we are in a repeating month */
      ret = (((date->tm_year - a->begin.tm_year)*12 +
       (date->tm_mon - a->begin.tm_mon))%(a->repeatFrequency) == 0);
      if (!ret) {
	 break;
      }
      /*See if this is the date that the appt repeats on */
      if (date->tm_mday == a->begin.tm_mday) {
	 ret = TRUE;
	 break;
      }
      /*If appt occurs after the last day of the month and this date */
      /*is the last day of the month then it occurs today */
      ret = ((a->begin.tm_mday > days_in_month[date->tm_mon]) &&
	     (date->tm_mday == days_in_month[date->tm_mon]));
      break;
    case repeatYearly:
      if ((date->tm_year - a->begin.tm_year)%(a->repeatFrequency) != 0) {
	 ret = FALSE;
	 break;
      }
      if ((date->tm_mday == a->begin.tm_mday) &&
	  (date->tm_mon == a->begin.tm_mon)) {
	 ret = TRUE;
	 break;
      }
      /*Take care of Feb 29th (Leap Day) */
      if ((a->begin.tm_mon == 1) && (a->begin.tm_mday == 29) &&
	(date->tm_mon == 1) && (date->tm_mday == 28)) {
	 ret = TRUE;
	 break;
      }   
      break;
    default:
      jpilot_logf(LOG_WARN, "unknown repeatType (%d) found in DatebookDB\n",
	   a->repeatType);
      ret = FALSE;
   }/*switch */

   if (ret) {
      /*Check for exceptions */
      for (i=0; i<a->exceptions; i++) {
#ifdef JPILOT_DEBUG
	 jpilot_logf(LOG_DEBUG, "exception %d mon %d\n", i, a->exception[i].tm_mon);
	 jpilot_logf(LOG_DEBUG, "exception %d day %d\n", i, a->exception[i].tm_mday);
	 jpilot_logf(LOG_DEBUG, "exception %d year %d\n", i, a->exception[i].tm_year);
	 jpilot_logf(LOG_DEBUG, "exception %d yday %d\n", i, a->exception[i].tm_yday);
	 jpilot_logf(LOG_DEBUG, "today is yday %d\n", date->tm_yday);
#endif
	 begin_days = dateToDays(&(a->exception[i]));
	 days = dateToDays(date);
	 if (begin_days == days) {
	    ret = FALSE;
	    break;
	 }
      }
   }
   
   return ret;
}

int get_datebook_app_info(struct AppointmentAppInfo *ai)
{
   int num;
   unsigned int rec_size;
   unsigned char *buf;

   bzero(ai, sizeof(*ai));

   jp_get_app_info("DatebookDB", &buf, &rec_size);
   num = unpack_AppointmentAppInfo(ai, buf, rec_size);
   if (buf) {
      free(buf);
   }
   if (num <= 0) {
      jpilot_logf(LOG_WARN, _("Error reading"), "DatebookDB.pdb");
      return -1;
   }
	 
   return 0;
}

int weed_datebook_list(AppointmentList **al, int mon, int year)
{
   struct tm tm_fdom;
   struct tm tm_ldom;
   AppointmentList *prev_al, *next_al, *tal;
   int r, dow, ndim;
   int trash_it;

   bzero(&tm_fdom, sizeof(tm_fdom));
   tm_fdom.tm_hour=11;
   tm_fdom.tm_mday=1;
   tm_fdom.tm_mon=mon;
   tm_fdom.tm_year=year;
   
   get_month_info(mon, 1, year, &dow, &ndim);

   memcpy(&tm_ldom, &tm_fdom, sizeof(tm_fdom));

   tm_ldom.tm_mday=ndim;

   mktime(&tm_fdom);
   mktime(&tm_ldom);

   /*
    * We are going to try to shrink the linked list since we are about to
    * search though it ~30 times.
    */
   for (prev_al=NULL, tal=*al; tal; tal = next_al) {
      trash_it=0;
      /* See if its a non-repeating appointment that isn't this month/year */
      if (tal->ma.a.repeatType==repeatNone) {
	 if ((tal->ma.a.begin.tm_year!=year) ||
	     (tal->ma.a.begin.tm_mon!=mon)) {
	    trash_it=1;
	    goto trash;
	 }
      }
      /* See if its a yearly appointment that doesn't reoccur on this month */
      if (tal->ma.a.repeatType==repeatYearly) {
	 if ((tal->ma.a.begin.tm_mon!=mon)) {
	    trash_it=1;
	    goto trash;
	 }
      }
      /*See if the appointment starts after the last day of the month */
      r = compareTimesToDay(&(tal->ma.a.begin), &tm_ldom);
      if (r == 1) {
	 trash_it=1;
	 goto trash;
      }
      /*If the appointment has an end date, see if it ended before the 1st */
      if (!(tal->ma.a.repeatForever)) {
	 r = compareTimesToDay(&(tal->ma.a.repeatEnd), &tm_fdom);
	 if (r == 2) {
	    trash_it=1;
	    goto trash;
	 }
      }
      /* Remove it from this list if it can't help us */
      trash:
      if (trash_it) {
	 if (prev_al) {
	    prev_al->next=tal->next;
	 } else {
	    *al=tal->next;
	 }
	 next_al=tal->next;
	 free_Appointment(&(tal->ma.a));
	 free(tal);
      } else {
	 prev_al=tal;
	 next_al=tal->next;
      }
   }
   return 0;
}

int appointment_on_day_list(int mon, int year, int *mask)
{
   struct tm tm_dom;
   AppointmentList *tal, *al;
   int i, dow, ndim, num;
   int bit;

   bzero(&tm_dom, sizeof(tm_dom));
   tm_dom.tm_hour=11;
   tm_dom.tm_mday=1;
   tm_dom.tm_mon=mon;
   tm_dom.tm_year=year;

   al = NULL;
   num = get_days_appointments2(&al, NULL, 2, 2, 2);

   get_month_info(mon, 1, year, &dow, &ndim);

   weed_datebook_list(&al, mon, year);

   *mask = 0;
   
   for (i=0, bit=1; i<ndim; i++, bit=bit<<1) {
      tm_dom.tm_mday = i+1;
      mktime(&tm_dom);

      for (tal=al; tal; tal = tal->next) {
	 if (isApptOnDate(&(tal->ma.a), &tm_dom)) {
	    *mask = *mask | bit;
	    break;
	 }
      }
   }

   free_AppointmentList(&al);
   
   return 0;
}

int get_days_appointments(AppointmentList **appointment_list, struct tm *now)
{
   return get_days_appointments2(appointment_list, now, 1, 1, 1);
}
/*
 * If Null is passed in for date, then all appointments will be returned
 * modified, deleted and private, 0 for no, 1 for yes, 2 for use prefs
 */
int get_days_appointments2(AppointmentList **appointment_list, struct tm *now,
			   int modified, int deleted, int privates)
{
   GList *records;
   GList *temp_list;
   int recs_returned, i, num;
   struct Appointment a;
   AppointmentList *temp_a_list;
   long keep_modified, keep_deleted;
   int keep_priv;
   long char_set;
   buf_rec *br;
#ifdef USE_DB3
   long use_db3_tags;
   time_t ltime;
   struct tm *today;
#endif
   
#ifdef USE_DB3
   time(&ltime);
   today = localtime(&ltime);
   get_pref(PREF_USE_DB3, &use_db3_tags, NULL);
#endif
  
   jpilot_logf(LOG_DEBUG, "get_days_appointments()\n");

   if (modified==2) {
      get_pref(PREF_SHOW_MODIFIED, &keep_modified, NULL);
   } else {
      keep_modified = modified;
   }
   if (deleted==2) {
      get_pref(PREF_SHOW_DELETED, &keep_deleted, NULL);
   } else {
      keep_deleted = deleted;
   }
   if (privates==2) {
      keep_priv = show_privates(GET_PRIVATES, NULL);
   } else {
      keep_priv = privates;
   }

   *appointment_list=NULL;
   recs_returned = 0;

   num = jp_read_DB_files("DatebookDB", &records);
   /* Go to first entry in the list */
   for (temp_list = records; temp_list; temp_list = temp_list->prev) {
      records = temp_list;
   }
   for (i=0, temp_list = records; temp_list; temp_list = temp_list->next, i++) {
      if (temp_list->data) {
	 br=temp_list->data;
      } else {
	 continue;
      }
      if (!br->buf) {
	 continue;
      }

      if ( ((br->rt==DELETED_PALM_REC) && (!keep_deleted)) ||
	  ((br->rt==MODIFIED_PALM_REC) && (!keep_modified)) ) {
	 continue;
      }
      if ((keep_priv != SHOW_PRIVATES) && 
	  (br->attrib & dlpRecAttrSecret)) {
	 continue;
      }

      num = unpack_Appointment(&a, br->buf, br->size);

      if (num <= 0) {
	 continue;
      }

#ifdef USE_DB3
      if (use_db3_tags) {
	 db3_hack_date(&a, today);
      }
#endif
      if ( ! ((now==NULL) || isApptOnDate(&a, now)) ) {
	 continue;
      }

      get_pref(PREF_CHAR_SET, &char_set, NULL);
      if (char_set==CHAR_SET_JAPANESE) {
	 Sjis2Euc(a.description, 65536);
	 Sjis2Euc(a.note, 65536);
      }
      if (char_set==CHAR_SET_1250) {
	 Win2Lat(a.description, 65536);
	 Win2Lat(a.note, 65536);
      }
      if (char_set==CHAR_SET_1251) {
	 win1251_to_koi8(a.description, 65536);
	 win1251_to_koi8(a.note, 65536);
      }
      if (char_set==CHAR_SET_1251_B) {
	 koi8_to_win1251(a.description, 65536);
	 koi8_to_win1251(a.note, 65536);
      }

      temp_a_list = malloc(sizeof(AppointmentList));
      if (!temp_a_list) {
	 jpilot_logf(LOG_WARN, "get_days_appointments(): Out of memory\n");
	 break;
      }
      memcpy(&(temp_a_list->ma.a), &a, sizeof(struct Appointment));
      temp_a_list->app_type = DATEBOOK;
      temp_a_list->ma.rt = br->rt;
      temp_a_list->ma.attrib = br->attrib;
      temp_a_list->ma.unique_id = br->unique_id;
      temp_a_list->next = *appointment_list;
      *appointment_list = temp_a_list;
      recs_returned++;
   }

   jp_free_DB_records(&records);

   datebook_sort(appointment_list);

   jpilot_logf(LOG_DEBUG, "Leaving get_days_appointments()\n");

   return recs_returned;
}

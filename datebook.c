/* $Id: datebook.c,v 1.41 2004/11/22 06:58:09 rikster5 Exp $ */

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

#include "config.h"
#include "i18n.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
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
   struct Appointment *appt1, *appt2;

   al1=(AppointmentList **)v1;
   al2=(AppointmentList **)v2;

   appt1=&((*al1)->mappt.appt);
   appt2=&((*al2)->mappt.appt);

   if ((appt1->event) || (appt2->event)) {
      return appt2->event-appt1->event;
   }

   /* Jim Rees pointed out my sorting error */
   /* return ((appt1->begin.tm_hour*60 + appt1->begin.tm_min) > */
   return ((appt1->begin.tm_hour*60 + appt1->begin.tm_min) -
	   (appt2->begin.tm_hour*60 + appt2->begin.tm_min));
}

int datebook_sort(AppointmentList **al, 
                  int (*compare_func)(const void*, const void*))
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
      jp_logf(JP_LOG_WARN, "datebook_sort(): %s\n", _("Out of memory"));
      return 0;
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

   return 0;
}

#ifdef ENABLE_DATEBK
int db3_hack_date(struct Appointment *appt, struct tm *today)
{
   int t1, t2;

   if (today==NULL) {
      return 0;
   }
   if (!appt->note) {
      return 0;
   }
   if (strlen(appt->note) > 8) {
      if ((appt->note[0]=='#') && (appt->note[1]=='#')) {
	 if (appt->note[2]=='f' || appt->note[2]=='F') {
	    /* Check to see if its in the future */
	    t1 = appt->begin.tm_mday + appt->begin.tm_mon*31 + appt->begin.tm_year*372;
	    t2 = today->tm_mday + today->tm_mon*31 + today->tm_year*372;
	    if (t1 > t2) return 0;
	    /* We found some silly hack, so we lie about the date */
	    /*memcpy(&(appt->begin), today, sizeof(struct tm));*/
	    /*memcpy(&(appt->end), today, sizeof(struct tm));*/
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
	     * it anymore (or delete it).
	     */
	    if (!(appt->repeatForever)) {
	       if (compareTimesToDay(today, &(appt->repeatEnd))==1) {
		  /* end date is before start date, illegal appointment */
		  /* make it legal, by only floating upto the end date */
		  memcpy(&(appt->begin), &(appt->repeatEnd), sizeof(struct tm));
		  memcpy(&(appt->end), &(appt->repeatEnd), sizeof(struct tm));
	       }
	    }
	 }
      }
   }
   return 0;
}

/* Note should be pretty much validated by now */
void db3_fill_struct(char *note, int type, struct db4_struct *db4)
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
   /* bytes 15-18 lower 6 bits make 24 bit numer (not sure of byte order) */
   db4->custom_alarm_sound = ((note[14] & 0x3F) << 18) +
     ((note[15] & 0x3F) << 12) +
     ((note[16] & 0x3F) << 6) +
     ((note[17] & 0x3F) << 0);
   db4->color = (note[18] - '@') & 0x0F;
   /* Byte 19 is a carriage return */
}

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

int pc_datebook_write(struct Appointment *appt, PCRecType rt,
		      unsigned char attrib, unsigned int *unique_id)
{
   unsigned char record[65536];
   int rec_len;
   buf_rec br;
   long char_set;

   get_pref(PREF_CHAR_SET, &char_set, NULL);
   if (char_set != CHAR_SET_LATIN1) {
      if (appt->description) charset_j2p(appt->description, strlen(appt->description)+1, char_set);
      if (appt->note) charset_j2p(appt->note, strlen(appt->note)+1, char_set);
   }

   rec_len = pack_Appointment(appt, record, 65535);
   if (!rec_len) {
      PRINT_FILE_LINE;
      jp_logf(JP_LOG_WARN, "pack_Appointment %s\n", _("error"));
      return -1;
   }
   br.rt=rt;
   br.attrib = attrib;
   br.buf = record;
   br.size = rec_len;
   /* Keep unique ID intact */
   if ((unique_id) && (*unique_id!=0)) {
      br.unique_id = *unique_id;
   } else {
      br.unique_id = 0;
   }

   jp_pc_write("DatebookDB", &br);
   if (unique_id) {
      *unique_id = br.unique_id;
   }

   return 0;
}

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

/*
 * If a copy is made, then it should be freed through free_Appointment
 */
int datebook_copy_appointment(struct Appointment *a1,
			     struct Appointment **a2)
{
   *a2=malloc(sizeof(struct Appointment));
   if (!(*a2)) {
      jp_logf(JP_LOG_WARN, "datebook_copy_appointment(): %s\n", _("Out of memory"));
      return -1;
   }
   memcpy(*a2, a1, sizeof(struct Appointment));

   (*a2)->exception = (struct tm *)malloc(a1->exceptions * sizeof(struct tm));
   if (!(*a2)->exception) {
      jp_logf(JP_LOG_WARN, "datebook_copy_appointment(): %s 2\n", _("Out of memory"));
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
int datebook_add_exception(struct Appointment *appt, int year, int mon, int day)
{
   struct tm *new_exception, *Ptm;

   if (appt->exceptions==0) {
      appt->exception=NULL;
   }

   new_exception = malloc((appt->exceptions + 1) * sizeof(struct tm));
   if (!new_exception) {
      jp_logf(JP_LOG_WARN, "datebook_add_exception(): %s\n", _("Out of memory"));
      return -1;
   }
   memcpy(new_exception, appt->exception, (appt->exceptions) * sizeof(struct tm));
   free(appt->exception);
   appt->exceptions++;
   appt->exception = new_exception;
   Ptm = &(appt->exception[appt->exceptions - 1]);
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

unsigned int isApptOnDate(struct Appointment *appt, struct tm *date)
{
/*   long fdow; */
   unsigned int ret;
   unsigned int r;
   int week1, week2;
   int dow, ndim;
   int i;
   /* days_in_month is adjusted for leap year with the date structure */
   int days_in_month[]={31,28,31,30,31,30,31,31,30,31,30,31
   };
   int exception_days;
   static int days, begin_days;

   /* jp_logf(JP_LOG_DEBUG, "isApptOnDate\n"); */

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
   r = compareTimesToDay(&(appt->begin), date);
   if (r == 1) {
      return FALSE;
   }
   if (r == 0) {
      ret = TRUE;
   }
   /* If the appointment has an end date, see that we are not past it */
   if (!(appt->repeatForever)) {
      r = compareTimesToDay(&(appt->repeatEnd), date);
      if (r == 2) {
	 return FALSE;
      }
   }

   switch (appt->repeatType) {
    case repeatNone:
      break;
    case repeatDaily:
      /* See if this appt repeats on this day */
      begin_days = dateToDays(&(appt->begin));
      days = dateToDays(date);

      ret = (((days - begin_days)%(appt->repeatFrequency))==0);
      break;
    case repeatWeekly:
      get_month_info(date->tm_mon, date->tm_mday, date->tm_year, &dow, &ndim);
      /* See if the appointment repeats on this day */
      /*
      if (appt->repeatWeekstart > 1) {
	 appt->repeatWeekstart = 1;
      }
      if (appt->repeatWeekstart < 0) {
	 appt->repeatWeekstart = 0;
      }
      */
      /*if (!(appt->repeatDays[dow + appt->repeatWeekstart])) {*/
      if (!(appt->repeatDays[dow])) {
	 ret = FALSE;
	 break;
      }
      /*See if we are in a week that is repeated in */
      begin_days = dateToDays(&(appt->begin));
      days = dateToDays(date);

      /* In repeatWeekly Palm treats the week as running Monday-Sunday [0-6]
       * This contrasts with the C time structures which run Sunday-Sat[0-6]
       * The date calculation requires switching between the two.
       * Palm tm structure = C tm structure -1 with wraparound for Sunday */
      if (appt->begin.tm_wday == 0)
      {
	 ret = (days - begin_days) + 6;
      } else {
	 ret = (days - begin_days) + (appt->begin.tm_wday - 1);
      }
      ret = ((((int)ret/7) % appt->repeatFrequency) == 0);
      break;
    case repeatMonthlyByDay:
      /* See if we are in a month that is repeated in */
      ret = (((date->tm_year - appt->begin.tm_year)*12 +
       (date->tm_mon - appt->begin.tm_mon))%(appt->repeatFrequency)==0);
      if (!ret) {
	 break;
      }
      /* If the days of the week match - good */
      /* e.g. Monday or Thur, etc. */
      if (appt->repeatDay%7 != date->tm_wday) {
	 ret = FALSE;
	 break;
      }
      /* Are they both in the same week in the month */
      /* e.g. The 3rd Mon, or the 2nd Fri, etc. */
      week1 = appt->repeatDay/7;
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
    case repeatMonthlyByDate:
      /* See if we are in a repeating month */
      ret = (((date->tm_year - appt->begin.tm_year)*12 +
       (date->tm_mon - appt->begin.tm_mon))%(appt->repeatFrequency) == 0);
      if (!ret) {
	 break;
      }
      /* See if this is the date that the appt repeats on */
      if (date->tm_mday == appt->begin.tm_mday) {
	 ret = TRUE;
	 break;
      }
      /* If appt occurs after the last day of the month and this date */
      /* is the last day of the month then it occurs today */
      ret = ((appt->begin.tm_mday > days_in_month[date->tm_mon]) &&
	     (date->tm_mday == days_in_month[date->tm_mon]));
      break;
    case repeatYearly:
      if ((date->tm_year - appt->begin.tm_year)%(appt->repeatFrequency) != 0) {
	 ret = FALSE;
	 break;
      }
      if ((date->tm_mday == appt->begin.tm_mday) &&
	  (date->tm_mon == appt->begin.tm_mon)) {
	 ret = TRUE;
	 break;
      }
      /* Take care of Feb 29th (Leap Day) */
      if ((appt->begin.tm_mon == 1) && (appt->begin.tm_mday == 29) &&
	(date->tm_mon == 1) && (date->tm_mday == 28)) {
	 ret = TRUE;
	 break;
      }   
      break;
    default:
      jp_logf(JP_LOG_WARN, _("Unknown repeatType (%d) found in DatebookDB\n"),
	   appt->repeatType);
      ret = FALSE;
   }/*switch */

   /* Check for exceptions */
   if (ret && appt->exceptions) {
      days = dateToDays(date);
      for (i=0; i<appt->exceptions; i++) {
#ifdef JPILOT_DEBUG
	 jp_logf(JP_LOG_DEBUG, "exception %d mon %d\n", i, appt->exception[i].tm_mon);
	 jp_logf(JP_LOG_DEBUG, "exception %d day %d\n", i, appt->exception[i].tm_mday);
	 jp_logf(JP_LOG_DEBUG, "exception %d year %d\n", i, appt->exception[i].tm_year);
	 jp_logf(JP_LOG_DEBUG, "exception %d yday %d\n", i, appt->exception[i].tm_yday);
	 jp_logf(JP_LOG_DEBUG, "today is yday %d\n", date->tm_yday);
#endif
	 exception_days = dateToDays(&(appt->exception[i]));
	 if (exception_days == days) {
	    ret = FALSE;
	    break;
	 }
      }
   }

   return ret;
}

int get_datebook_app_info(struct AppointmentAppInfo *ai)
{
   int num,i;
   int rec_size;
   unsigned char *buf;
   long char_set;

   memset(ai, 0, sizeof(*ai));
   /* Put at least one entry in there */
   strcpy(ai->category.name[0], "Unfiled");

   jp_get_app_info("DatebookDB", &buf, &rec_size);
   num = unpack_AppointmentAppInfo(ai, buf, rec_size);
   if (buf) {
      free(buf);
   }
   if ((num<0) || (rec_size<=0)) {
      jp_logf(JP_LOG_WARN, _("Error reading file: %s\n"), "DatebookDB.pdb");
      return -1;
   }
   get_pref(PREF_CHAR_SET, &char_set, NULL);
   if (char_set != CHAR_SET_LATIN1) {
      for (i = 0; i < 16; i++) {
	 if (ai->category.name[i][0] != '\0') {
	    charset_p2j(ai->category.name[i], 16, char_set);
	 }
      }
   }

   return 0;
}

int weed_datebook_list(AppointmentList **al, int mon, int year,
		       int skip_privates, int *mask)
{
   struct tm tm_fdom;
   struct tm tm_ldom;
   struct tm tm_test;
   AppointmentList *prev_al, *next_al, *tal;
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

   next_al=NULL;
   /*
    * We are going to try to shrink the linked list since we are about to
    * search though it ~30 times.
    */
   for (prev_al=NULL, tal=*al; tal; tal = next_al) {
      if (skip_privates && (tal->mappt.attrib & dlpRecAttrSecret)) {
	 next_al=tal->next;
	 continue;
      }
      trash_it=0;
      /* See if the appointment starts after the last day of the month */
      r = compareTimesToDay(&(tal->mappt.appt.begin), &tm_ldom);
      if (r == 1) {
	 trash_it=1;
	 goto trash;
      }
      /* If the appointment has an end date, see if it ended before the 1st */
      if (!(tal->mappt.appt.repeatForever)) {
	 r = compareTimesToDay(&(tal->mappt.appt.repeatEnd), &tm_fdom);
	 if (r == 2) {
	    trash_it=1;
	    goto trash;
	 }
      }
      /* No repeat */
      /* See if its a non-repeating appointment that is this month/year */
      if (tal->mappt.appt.repeatType==repeatNone) {
	 if ((tal->mappt.appt.begin.tm_year==year) &&
	     (tal->mappt.appt.begin.tm_mon==mon)) {
	    tm_test.tm_mday=tal->mappt.appt.begin.tm_mday;
	    mktime(&tm_test);
	    /* Go ahead and mark this day (highlight it) */
	    if (isApptOnDate(&(tal->mappt.appt), &tm_test)) {
	       *mask = *mask | (1 << ((tal->mappt.appt.begin.tm_mday)-1));
	    }
	 } else {
	    trash_it=1;
	    goto trash;
	 }
      }
      /* Daily */
      /* See if this daily appt repeats this month or not */
      if (tal->mappt.appt.repeatType==repeatDaily) {
	 begin_days = dateToDays(&(tal->mappt.appt.begin));
	 ret = ((days - begin_days)%(tal->mappt.appt.repeatFrequency));
	 if ((ret>31) || (ret<-31)) {
	    trash_it=1;
	    goto trash;
	 }
      }
      /* Weekly */
      if (tal->mappt.appt.repeatType==repeatWeekly) {
	 begin_days = dateToDays(&(tal->mappt.appt.begin));
	 /* Note: Palm Bug?  I think the palm does this wrong.
	  * I prefer this way of doing it so that you can have appts repeating
	  * from Wed->Tue, for example.  The palms way prevents this */
	 /* ret = (((int)((days - begin_days - fdow)/7))%(appt->repeatFrequency)==0);*/
	 /* But, here is the palm way */
	 ret = (((int)((days-begin_days+tal->mappt.appt.begin.tm_wday-fdow)/7))
		%(tal->mappt.appt.repeatFrequency));
	 if ((ret>6) || (ret<-6)) {
	    trash_it=1;
	    goto trash;
	 }
      }
      /* Monthly */
      if ((tal->mappt.appt.repeatType==repeatMonthlyByDay) ||
	  (tal->mappt.appt.repeatType==repeatMonthlyByDate)) {
	 /* See if we are in a month that is repeated in */
	 ret = (((year - tal->mappt.appt.begin.tm_year)*12 +
		 (mon - tal->mappt.appt.begin.tm_mon))%(tal->mappt.appt.repeatFrequency)==0);
	 if (!ret) {
	    trash_it=1;
	    goto trash;
	 }
	 if (tal->mappt.appt.repeatType==repeatMonthlyByDay) {
	    tm_test.tm_mday=tal->mappt.appt.begin.tm_mday;
	    mktime(&tm_test);
	    if (isApptOnDate(&(tal->mappt.appt), &tm_test)) {
	       *mask = *mask | (1 << ((tal->mappt.appt.begin.tm_mday)-1));
	    }
	 }
      }
      /* Yearly */
      /* See if its a yearly appointment that does reoccur on this month */
      if (tal->mappt.appt.repeatType==repeatYearly) {
	 if ((tal->mappt.appt.begin.tm_mon==mon)) {
	    tm_test.tm_mday=tal->mappt.appt.begin.tm_mday;
	    mktime(&tm_test);
	    if (isApptOnDate(&(tal->mappt.appt), &tm_test)) {
	       *mask = *mask | (1 << ((tal->mappt.appt.begin.tm_mday)-1));
	    }
	 } else {
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
	 free_Appointment(&(tal->mappt.appt));
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
   int dow, ndim, num;
   int bit;
   int show_priv;
   int skip_privates;

   memset(&tm_dom, 0, sizeof(tm_dom));
   tm_dom.tm_hour=11;
   tm_dom.tm_mday=1;
   tm_dom.tm_mon=mon;
   tm_dom.tm_year=year;

   al = NULL;
   /* Get private records back
    * We want to highlight a day with a private record if we are showing or
    * masking private records */
   num = get_days_appointments2(&al, NULL, 2, 2, 1, NULL);

   show_priv = show_privates(GET_PRIVATES);
   skip_privates = (show_priv==HIDE_PRIVATES);

   get_month_info(mon, 1, year, &dow, &ndim);

   *mask = 0;

   weed_datebook_list(&al, mon, year, skip_privates, mask);

   for (tm_dom.tm_mday=1, bit=1; tm_dom.tm_mday<=ndim; tm_dom.tm_mday++, bit=bit<<1) {
      if (*mask & bit) {
	 continue;
      }
      mktime(&tm_dom);

      for (tal=al; tal; tal = tal->next) {
	 if (skip_privates && (tal->mappt.attrib & dlpRecAttrSecret)) continue;
	 if (isApptOnDate(&(tal->mappt.appt), &tm_dom)) {
	    *mask = *mask | bit;
	    break;
	 }
      }
   }
   free_AppointmentList(&al);

   return 0;
}

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
   GList *records;
   GList *temp_list;
   int recs_returned, i, num;
   struct Appointment appt;
   AppointmentList *temp_a_list;
   long keep_modified, keep_deleted;
   int keep_priv;
   buf_rec *br;
   long char_set;
   char *buf;
   size_t len;
#ifdef ENABLE_DATEBK
   long use_db3_tags;
   time_t ltime;
   struct tm *Ptoday, today;
#endif

#ifdef ENABLE_DATEBK
   time(&ltime);
   Ptoday = localtime(&ltime);
   /* Copy into stable memory */
   memcpy(&today, Ptoday, sizeof(struct tm));
   get_pref(PREF_USE_DB3, &use_db3_tags, NULL);
#endif

   jp_logf(JP_LOG_DEBUG, "get_days_appointments()\n");

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
      keep_priv = show_privates(GET_PRIVATES);
   } else {
      keep_priv = privates;
   }

   *appointment_list=NULL;
   recs_returned = 0;

   num = jp_read_DB_files("DatebookDB", &records);
   if (total_records) *total_records = num;
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

      if ( ((br->rt==DELETED_PALM_REC)  && (!keep_deleted)) ||
	   ((br->rt==DELETED_PC_REC)    && (!keep_deleted)) ||
	   ((br->rt==MODIFIED_PALM_REC) && (!keep_modified)) ) {
	 continue;
      }
      if ((keep_priv != SHOW_PRIVATES) && 
	  (br->attrib & dlpRecAttrSecret)) {
	 continue;
      }

      appt.exception=NULL;
      appt.description=NULL;
      appt.note=NULL;
      num = unpack_Appointment(&appt, br->buf, br->size);

      if (num <= 0) {
	 continue;
      }

#ifdef ENABLE_DATEBK
      if (use_db3_tags) {
	 db3_hack_date(&appt, &today);
      }
#endif
      if (now!=NULL) {
	 if (!isApptOnDate(&appt, now)) {
	    free_Appointment(&appt);
	    continue;
	 }
      }

      get_pref(PREF_CHAR_SET, &char_set, NULL);

      if (appt.description) {
	 if ((buf = (char *)malloc((len = strlen(appt.description)*2+1))) != NULL) {
	    multibyte_safe_strncpy(buf, appt.description, len);
	    charset_p2j(buf, len, char_set);
	    if (strlen(buf) > strlen(appt.description)) {
	       free(appt.description);
	       appt.description = strdup(buf);
	    } else {
	       multibyte_safe_strncpy(appt.description, buf, strlen(appt.description)+1);
	    }
	    free(buf);
	 }
      }
      if (appt.note) {
	 if ((buf = (char *) malloc((len = strlen(appt.note)*2+1))) != NULL) {
	    multibyte_safe_strncpy(buf, appt.note, len);
	    charset_p2j(buf, len, char_set);
	    if (strlen(buf) > strlen(appt.note)) {
	       free(appt.note);
	       appt.note = strdup(buf);
	    } else {
	       multibyte_safe_strncpy(appt.note, buf, strlen(appt.note)+1);
	    }
	    free(buf);
	 }
      }

      temp_a_list = malloc(sizeof(AppointmentList));
      if (!temp_a_list) {
	 jp_logf(JP_LOG_WARN, "get_days_appointments(): %s\n", _("Out of memory"));
	 free_Appointment(&appt);
	 break;
      }
      memcpy(&(temp_a_list->mappt.appt), &appt, sizeof(struct Appointment));
      temp_a_list->app_type = DATEBOOK;
      temp_a_list->mappt.rt = br->rt;
      temp_a_list->mappt.attrib = br->attrib;
      temp_a_list->mappt.unique_id = br->unique_id;
      temp_a_list->next = *appointment_list;
      *appointment_list = temp_a_list;
      recs_returned++;
   }

   jp_free_DB_records(&records);

   datebook_sort(appointment_list, datebook_compare);

   jp_logf(JP_LOG_DEBUG, "Leaving get_days_appointments()\n");

   return recs_returned;
}

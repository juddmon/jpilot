/* $Id: calendar.c,v 1.12 2010/04/21 11:44:54 rousseau Exp $ */

/*******************************************************************************
 * calendar.c
 * A module of J-Pilot http://jpilot.org
 *
 * Copyright (C) 1999-2009 by Judd Montgomery
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
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <pi-source.h>
#include <pi-socket.h>
#include <pi-calendar.h>
#include <pi-dlp.h>

#include "datebook.h"
#include "i18n.h"
#include "utils.h"
#include "log.h"
#include "prefs.h"
#include "libplugin.h"
#include "password.h"
#include "calendar.h"

/* Copy AppInfo data structures */
int copy_appointment_ai_to_calendar_ai(const struct AppointmentAppInfo *aai, struct CalendarAppInfo *cai)
{
   cai->type = calendar_v1;
   memcpy(&cai->category, &aai->category, sizeof(struct CategoryAppInfo));
   cai->startOfWeek = aai->startOfWeek;
   memset(&cai->internal, '\0', sizeof(cai->internal));

   return EXIT_SUCCESS;
}

/* Copy AppInfo data structures */
int copy_calendar_ai_to_appointment_ai(const struct CalendarAppInfo *cai, struct AppointmentAppInfo *aai)
{
   memcpy(&aai->category, &cai->category, sizeof(struct CategoryAppInfo));
   aai->startOfWeek = cai->startOfWeek;

   return EXIT_SUCCESS;
}

int copy_appointment_to_calendarEvent(const struct Appointment *appt, 
                                      struct CalendarEvent *cale)
{
   int i;

   cale->event = appt->event;
   cale->begin = appt->begin;
   cale->end = appt->end;
   cale->alarm = appt->alarm;
   cale->advance = appt->advance;
   cale->advanceUnits = appt->advanceUnits;
   cale->repeatType = appt->repeatType;
   cale->repeatForever = appt->repeatForever;
   cale->repeatEnd = appt->repeatEnd;
   cale->repeatFrequency = appt->repeatFrequency;
   cale->repeatDay = appt->repeatDay;
   for (i=0; i<7; i++) {
      cale->repeatDays[i] = appt->repeatDays[i];
   }
   cale->repeatWeekstart = appt->repeatWeekstart;
   cale->exceptions = appt->exceptions;
   if (appt->exceptions > 0) {
      cale->exception = (struct tm *) malloc(appt->exceptions * sizeof(struct tm));
      memcpy(cale->exception, appt->exception, appt->exceptions * sizeof(struct tm));
   } else {
      cale->exception = NULL;
   }
   if (appt->description) {
      cale->description = strdup(appt->description);
   } else {
      cale->description = NULL;
   }
   if (appt->note) {
      cale->note = strdup(appt->note);
   } else {
      cale->note = NULL;
   }
   cale->location = NULL;
   /* No blobs */
   for (i=0; i<MAX_BLOBS; i++) {
      cale->blob[i]=NULL;
   }
   cale->tz = NULL;

   return EXIT_SUCCESS;
}

int copy_calendarEvent_to_appointment(const struct CalendarEvent *cale, 
                                      struct Appointment *appt)
{
   int i;

   appt->event = cale->event;
   appt->begin = cale->begin;
   appt->end = cale->end;
   appt->alarm = cale->alarm;
   appt->advance = cale->advance;
   appt->advanceUnits = cale->advanceUnits;
   appt->repeatType = cale->repeatType;
   appt->repeatForever = cale->repeatForever;
   appt->repeatEnd = cale->repeatEnd;
   appt->repeatFrequency = cale->repeatFrequency;
   appt->repeatDay = cale->repeatDay;
   for (i=0; i<7; i++) {
      appt->repeatDays[i] = cale->repeatDays[i];
   }
   appt->repeatWeekstart = cale->repeatWeekstart;
   appt->exceptions = cale->exceptions;
   if (cale->exceptions > 0) {
      appt->exception = (struct tm *) malloc(cale->exceptions * sizeof(struct tm));
      memcpy(appt->exception, cale->exception, cale->exceptions * sizeof(struct tm));
   } else {
      appt->exception = NULL;
   }
   if (cale->description) {
      appt->description = strdup(cale->description);
   } else {
      appt->description = NULL;
   }
   if (cale->note) {
      appt->note = strdup(cale->note);
   } else {
      appt->note = NULL;
   }

   return EXIT_SUCCESS;
}

int copy_appointments_to_calendarEvents(AppointmentList *al, CalendarEventList **cel)
{
   CalendarEventList *temp_cel, *last_cel;
   AppointmentList *temp_al;

   *cel = last_cel = NULL;

   for (temp_al = al; temp_al; temp_al=temp_al->next) {
      temp_cel = malloc(sizeof(CalendarEventList));
      if (!temp_cel) return -1;
      temp_cel->mcale.rt = temp_al->mappt.rt;
      temp_cel->mcale.unique_id = temp_al->mappt.unique_id;
      temp_cel->mcale.attrib = temp_al->mappt.attrib;
      copy_appointment_to_calendarEvent(&(temp_al->mappt.appt), &(temp_cel->mcale.cale));
      temp_cel->app_type = CALENDAR;
      temp_cel->next=NULL;
      if (!last_cel) {
         *cel = last_cel = temp_cel;
      } else {
         last_cel->next = temp_cel;
         last_cel = temp_cel;
      }
   }

   return EXIT_SUCCESS;
}

int copy_calendarEvents_to_appointments(CalendarEventList *cel, AppointmentList **al)
{
   AppointmentList *temp_al, *last_al;
   CalendarEventList *temp_cel;

   *al = last_al = NULL;

   for (temp_cel = cel; temp_cel; temp_cel=temp_cel->next) {
      temp_al = malloc(sizeof(AppointmentList));
      if (!temp_al) return -1;
      temp_al->mappt.rt = temp_cel->mcale.rt;
      temp_al->mappt.unique_id = temp_cel->mcale.unique_id;
      temp_al->mappt.attrib = temp_cel->mcale.attrib;
      copy_calendarEvent_to_appointment(&(temp_cel->mcale.cale), &(temp_al->mappt.appt));
      temp_al->app_type = DATEBOOK;
      temp_al->next=NULL;
      if (!last_al) {
         *al = last_al = temp_al;
      } else {
         last_al->next = temp_al;
         last_al = temp_al;
      }
   }

   return EXIT_SUCCESS;
}

void free_CalendarEventList(CalendarEventList **cel)
{
   CalendarEventList *temp_cel, *temp_cel_next;

   for (temp_cel = *cel; temp_cel; temp_cel=temp_cel_next) {
      free_CalendarEvent(&(temp_cel->mcale.cale));
      temp_cel_next = temp_cel->next;
      free(temp_cel);
   }
   *cel = NULL;
}

int get_calendar_app_info(struct CalendarAppInfo *cai)
{
   int num, r;
   int rec_size;
   unsigned char *buf;
   pi_buffer_t pi_buf;

   memset(cai, 0, sizeof(*cai));
   /* Put at least one entry in there */
   strcpy(cai->category.name[0], "Unfiled");

   r = jp_get_app_info("CalendarDB-PDat", &buf, &rec_size);
   if ((r != EXIT_SUCCESS) || (rec_size<=0)) {
      jp_logf(JP_LOG_WARN, _("%s:%d Error reading application info %s\n"), __FILE__, __LINE__, "CalendarDB-PDat");
      if (buf) {
         free(buf);
      }
      return EXIT_FAILURE;
   }

   pi_buf.data = buf;
   pi_buf.used = rec_size;
   pi_buf.allocated = rec_size;

   num = unpack_CalendarAppInfo(cai, &pi_buf);
   if (buf) {
      free(buf);
   }
   if ((num<0) || (rec_size<=0)) {
      jp_logf(JP_LOG_WARN, _("Error reading file: %s\n"), "CalendarDB-PDat.pdb");
      return EXIT_FAILURE;
   }


   return EXIT_SUCCESS;
}

static int calendar_compare(const void *v1, const void *v2)
{
   CalendarEventList **cel1, **cel2;
   struct CalendarEvent *ce1, *ce2;

   cel1=(CalendarEventList **)v1;
   cel2=(CalendarEventList **)v2;

   ce1=&((*cel1)->mcale.cale);
   ce2=&((*cel2)->mcale.cale);

   if ((ce1->event) || (ce2->event)) {
      return ce2->event - ce1->event;
   }

   /* Jim Rees pointed out my sorting error */
   return ((ce1->begin.tm_hour*60 + ce1->begin.tm_min) -
           (ce2->begin.tm_hour*60 + ce2->begin.tm_min));
}

int calendar_sort(CalendarEventList **cel,
                  int (*compare_func)(const void*, const void*))
{
   CalendarEventList *temp_cel;
   CalendarEventList **sort_cel;
   int count, i;

   /* Count the entries in the list */
   for (count=0, temp_cel=*cel; temp_cel; temp_cel=temp_cel->next, count++) {}

   if (count<2) {
      /* No need to sort 0 or 1 items */
      return EXIT_SUCCESS;
   }

   /* Allocate an array to be qsorted */
   sort_cel = calloc(count, sizeof(CalendarEventList *));
   if (!sort_cel) {
      jp_logf(JP_LOG_WARN, "calendar_sort(): %s\n", _("Out of memory"));
      return EXIT_FAILURE;
   }

   /* Set our array to be a list of pointers to the nodes in the linked list */
   for (i=0, temp_cel=*cel; temp_cel; temp_cel=temp_cel->next, i++) {
      sort_cel[i] = temp_cel;
   }

   /* qsort them */
   qsort(sort_cel, count, sizeof(CalendarEventList *), compare_func);

   /* Put the linked list in the order of the array */
   sort_cel[count-1]->next = NULL;
   for (i=count-1; i; i--) {
      sort_cel[i-1]->next=sort_cel[i];
   }

   *cel = sort_cel[0];

   free(sort_cel);

   return EXIT_SUCCESS;
}

int get_days_calendar_events(CalendarEventList **calendar_event_list, struct tm *now, int category, int *total_records)
{
   return get_days_calendar_events2(calendar_event_list, now, 1, 1, 1, category, total_records);
}



static int calendar_db3_hack_date(struct CalendarEvent *cale, struct tm *today)
{
   int t1, t2;

   if (today==NULL) {
      return EXIT_SUCCESS;
   }
   if (!cale->note) {
      return EXIT_SUCCESS;
   }
   if (strlen(cale->note) > 8) {
      if ((cale->note[0]=='#') && (cale->note[1]=='#')) {
         if (cale->note[2]=='f' || cale->note[2]=='F') {
            /* Check to see if its in the future */
            t1 = cale->begin.tm_mday + cale->begin.tm_mon*31 + cale->begin.tm_year*372;
            t2 = today->tm_mday + today->tm_mon*31 + today->tm_year*372;
            if (t1 > t2) return EXIT_SUCCESS;
            /* We found some silly hack, so we lie about the date */
            /* memcpy(&(cale->begin), today, sizeof(struct tm));*/
            /* memcpy(&(cale->end), today, sizeof(struct tm));*/
            cale->begin.tm_mday = today->tm_mday;
            cale->begin.tm_mon = today->tm_mon;
            cale->begin.tm_year = today->tm_year;
            cale->begin.tm_wday = today->tm_wday;
            cale->begin.tm_yday = today->tm_yday;
            cale->begin.tm_isdst = today->tm_isdst;
            cale->end.tm_mday = today->tm_mday;
            cale->end.tm_mon = today->tm_mon;
            cale->end.tm_year = today->tm_year;
            cale->end.tm_wday = today->tm_wday;
            cale->end.tm_yday = today->tm_yday;
            cale->end.tm_isdst = today->tm_isdst;
            /* If the appointment has an end date, and today is past the end
             * date, because of this hack we would never be able to view
             * it anymore (or delete it).  */
            if (!(cale->repeatForever)) {
               if (compareTimesToDay(today, &(cale->repeatEnd))==1) {
                  /* end date is before start date, illegal appointment */
                  /* make it legal, by only floating up to the end date */
                  memcpy(&(cale->begin), &(cale->repeatEnd), sizeof(struct tm));
                  memcpy(&(cale->end), &(cale->repeatEnd), sizeof(struct tm));
               }
            }
         }
      }
   }
   return EXIT_SUCCESS;
}

/*
 * If Null is passed in for date, then all appointments will be returned
 * modified, deleted and private, 0 for no, 1 for yes, 2 for use prefs
 */
int get_days_calendar_events2(CalendarEventList **calendar_event_list, 
                              struct tm *now,
                              int modified, int deleted, int privates,
                              int category, int *total_records)
{
   GList *records;
   GList *temp_list;
   int recs_returned, num;
   struct CalendarEvent cale;
   CalendarEventList *temp_ce_list;
   long keep_modified, keep_deleted;
   int keep_priv;
   buf_rec *br;
   long char_set;
   long datebook_version;
   char *buf;
   pi_buffer_t RecordBuffer;
   int i;
#ifdef ENABLE_DATEBK
   long use_db3_tags;
   time_t ltime;
   struct tm today;
#endif
   struct Appointment appt;

#ifdef ENABLE_DATEBK
   time(&ltime);
   /* Copy into stable memory */
   memcpy(&today, localtime(&ltime), sizeof(struct tm));
   get_pref(PREF_USE_DB3, &use_db3_tags, NULL);
#endif

   jp_logf(JP_LOG_DEBUG, "get_days_calendar_events()\n");

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
   get_pref(PREF_CHAR_SET, &char_set, NULL);

   *calendar_event_list=NULL;
   recs_returned = 0;

   get_pref(PREF_DATEBOOK_VERSION, &datebook_version, NULL);

   if (datebook_version) {
      num = jp_read_DB_files("CalendarDB-PDat", &records);
   } else {
      num = jp_read_DB_files("DatebookDB", &records);
   }
   if (-1 == num)
     return 0;

   if (total_records) *total_records = num;

   for (temp_list = records; temp_list; temp_list = temp_list->next) {
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

      if ( ((br->attrib & 0x0F) != category) && category != CATEGORY_ALL) {
         continue;
      }

      cale.exception=NULL;
      cale.description=NULL;
      cale.note=NULL;
      cale.location=NULL;
      for (i=0; i< MAX_BLOBS; i++) {
         cale.blob[i]=NULL;
      }
      cale.tz=NULL;

      /* This is kind of a hack to set the pi_buf directly, but its faster */
      RecordBuffer.data = br->buf;
      RecordBuffer.used = br->size;
      RecordBuffer.allocated = br->size;

      if (datebook_version) {
         if (unpack_CalendarEvent(&cale, &RecordBuffer, calendar_v1) == -1) {
            continue;
         }
      } else {
         if (unpack_Appointment(&appt, &RecordBuffer, calendar_v1) == -1) {
            continue;
         }
         copy_appointment_to_calendarEvent(&appt, &cale);
         free_Appointment(&appt);
      }

      //FIXME: verify db3 hack works with new calendar code
#ifdef ENABLE_DATEBK
      if (use_db3_tags) {
         calendar_db3_hack_date(&cale, &today);
      }
#endif
      if (now!=NULL) {
         if (! calendar_isApptOnDate(&cale, now)) {
            free_CalendarEvent(&cale);
            continue;
         }
      }

      if (cale.description) {
         buf = charset_p2newj(cale.description, -1, char_set);
         if (buf) {
            free(cale.description);
            cale.description = buf;
         }
      }
      if (cale.note) {
         buf = charset_p2newj(cale.note, -1, char_set);
         if (buf) {
            free(cale.note);
            cale.note = buf;
         }
      }
      if (cale.location) {
         buf = charset_p2newj(cale.location, -1, char_set);
         if (buf) {
            free(cale.location);
            cale.location = buf;
         }
      }

      temp_ce_list = malloc(sizeof(CalendarEventList));
      if (!temp_ce_list) {
         jp_logf(JP_LOG_WARN, "get_days_calendar_events2(): %s\n", _("Out of memory"));
         free_CalendarEvent(&cale);
         break;
      }
      memcpy(&(temp_ce_list->mcale.cale), &cale, sizeof(struct CalendarEvent));
      temp_ce_list->app_type = CALENDAR;
      temp_ce_list->mcale.rt = br->rt;
      temp_ce_list->mcale.attrib = br->attrib;
      temp_ce_list->mcale.unique_id = br->unique_id;
      temp_ce_list->next = *calendar_event_list;
      *calendar_event_list = temp_ce_list;
      recs_returned++;
   }

   jp_free_DB_records(&records);

   calendar_sort(calendar_event_list, calendar_compare);

   jp_logf(JP_LOG_DEBUG, "Leaving get_days_calendar_events()\n");

   return recs_returned;
}

int pc_calendar_write(struct CalendarEvent *cale, 
                      PCRecType rt, 
                      unsigned char attrib, 
                      unsigned int *unique_id)
{
   Appointment_t appt;
   pi_buffer_t *RecordBuffer;
   buf_rec br;
   long char_set;
   long datebook_version;
   int r;

   get_pref(PREF_DATEBOOK_VERSION, &datebook_version, NULL);
   get_pref(PREF_CHAR_SET, &char_set, NULL);

   if (char_set != CHAR_SET_LATIN1) {
      if (cale->description) charset_j2p(cale->description, strlen(cale->description)+1, char_set);
      if (cale->note) charset_j2p(cale->note, strlen(cale->note)+1, char_set);
      if (datebook_version) {
         if (cale->location) 
            charset_j2p(cale->location, strlen(cale->location)+1, char_set);
      }
   }

   RecordBuffer = pi_buffer_new(0);
   if (datebook_version) {
      if (pack_CalendarEvent(cale, RecordBuffer, calendar_v1) == -1) {
         PRINT_FILE_LINE;
         jp_logf(JP_LOG_WARN, "pack_CalendarEvent %s\n", _("error"));
         return EXIT_FAILURE;
      }
   } else {
      copy_calendarEvent_to_appointment(cale, &appt);
      r = pack_Appointment(&appt, RecordBuffer, datebook_v1);
      free_Appointment(&appt);
      if (r == -1) {
         PRINT_FILE_LINE;
         jp_logf(JP_LOG_WARN, "pack_Appointment %s\n", _("error"));
         return EXIT_FAILURE;
      }
   }
   br.rt=rt;
   br.attrib = attrib;
   br.buf = RecordBuffer->data;
   br.size = RecordBuffer->used;
   /* Keep unique ID intact */
   if ((unique_id) && (*unique_id!=0)) {
      br.unique_id = *unique_id;
   } else {
      br.unique_id = 0;
   }

   if (datebook_version) {
      jp_pc_write("CalendarDB-PDat", &br);
   } else {
      jp_pc_write("DatebookDB", &br);
   }

   if (unique_id) {
      *unique_id = br.unique_id;
   }

   pi_buffer_free(RecordBuffer);

   return EXIT_SUCCESS;
}

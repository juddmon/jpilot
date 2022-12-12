/*******************************************************************************
 * jpilot-dump.c
 *
 * Copyright (C) 2000 by hvrietsc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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
#include <sys/stat.h>
#ifdef HAVE_LOCALE_H
#  include <locale.h>
#endif

/* Pilot-link header files */
#include <pi-todo.h>
#include <pi-memo.h>
#include <pi-address.h>
#include <pi-calendar.h>

/* Jpilot header files */
#include "datebook.h"
#include "address.h"
#include "todo.h"
#include "memo.h"
#include "utils.h"
#include "i18n.h"
#include "otherconv.h"
#include "prefs.h"
#include "sync.h"

/********************************* Constants **********************************/
/* RFCs use CRLF for Internet newline */
#define CRLF "\x0D\x0A"

#define LIMIT(a,b,c) if (a < b) {a=b;} if (a > c) {a=c;}

/* Uncomment for more debug output */
/* #define JDUMP_DEBUG 1 */

/******************************* Global vars **********************************/
/* dump switches */
int  dumpD;
int  dumpI;
int  dumpN;
int  Nyear;
int  Nmonth;
int  Nday;
int  dumpA;
int  dumpC;
int  dumpC_type;
int  dumpM;
int  dumpT;
const char *formatD;
const char *formatM;
const char *formatA;
const char *formatT;

/* Start Hack */
/* FIXME: The following is a hack.
 * The variables below are global variables in jpilot.c which are unused in
 * this code but must be instantiated for the code to compile.  
 * The same is true of the functions which are only used in GUI mode. */
pid_t jpilot_master_pid = -1;
int pipe_to_parent;
GtkWidget *glob_dialog;
GtkWidget *glob_date_label;
gint glob_date_timer_tag;

void output_to_pane(const char *str) { return; }
int sync_once(struct my_sync_info *sync_info) { return EXIT_SUCCESS; }
/* End Hack */

/* Structs needed for ContactsDB export */

static address_schema_entry contact_schema[NUM_CONTACT_FIELDS]={
   {contLastname,  0, ADDRESS_GUI_LABEL_TEXT},
   {contFirstname, 0, ADDRESS_GUI_LABEL_TEXT},
   {contCompany,   0, ADDRESS_GUI_LABEL_TEXT},
   {contTitle,     0, ADDRESS_GUI_LABEL_TEXT},
   {contPhone1,    0, ADDRESS_GUI_DIAL_SHOW_PHONE_MENU_TEXT},
   {contPhone2,    0, ADDRESS_GUI_DIAL_SHOW_PHONE_MENU_TEXT},
   {contPhone3,    0, ADDRESS_GUI_DIAL_SHOW_PHONE_MENU_TEXT},
   {contPhone4,    0, ADDRESS_GUI_DIAL_SHOW_PHONE_MENU_TEXT},
   {contPhone5,    0, ADDRESS_GUI_DIAL_SHOW_PHONE_MENU_TEXT},
   {contPhone6,    0, ADDRESS_GUI_DIAL_SHOW_PHONE_MENU_TEXT},
   {contPhone7,    0, ADDRESS_GUI_DIAL_SHOW_PHONE_MENU_TEXT},
   {contIM1,       0, ADDRESS_GUI_IM_MENU_TEXT},
   {contIM2,       0, ADDRESS_GUI_IM_MENU_TEXT},
   {contWebsite,   0, ADDRESS_GUI_WEBSITE_TEXT},
   {contAddress1,  1, ADDRESS_GUI_ADDR_MENU_TEXT},
   {contCity1,     1, ADDRESS_GUI_LABEL_TEXT},
   {contState1,    1, ADDRESS_GUI_LABEL_TEXT},
   {contZip1,      1, ADDRESS_GUI_LABEL_TEXT},
   {contCountry1,  1, ADDRESS_GUI_LABEL_TEXT},
   {contAddress2,  2, ADDRESS_GUI_ADDR_MENU_TEXT},
   {contCity2,     2, ADDRESS_GUI_LABEL_TEXT},
   {contState2,    2, ADDRESS_GUI_LABEL_TEXT},
   {contZip2,      2, ADDRESS_GUI_LABEL_TEXT},
   {contCountry2,  2, ADDRESS_GUI_LABEL_TEXT},
   {contAddress3,  3, ADDRESS_GUI_ADDR_MENU_TEXT},
   {contCity3,     3, ADDRESS_GUI_LABEL_TEXT},
   {contState3,    3, ADDRESS_GUI_LABEL_TEXT},
   {contZip3,      3, ADDRESS_GUI_LABEL_TEXT},
   {contCountry3,  3, ADDRESS_GUI_LABEL_TEXT},
   {contBirthday,  4, ADDRESS_GUI_BIRTHDAY},
   {contCustom1,   4, ADDRESS_GUI_LABEL_TEXT},
   {contCustom2,   4, ADDRESS_GUI_LABEL_TEXT},
   {contCustom3,   4, ADDRESS_GUI_LABEL_TEXT},
   {contCustom4,   4, ADDRESS_GUI_LABEL_TEXT},
   {contCustom5,   4, ADDRESS_GUI_LABEL_TEXT},
   {contCustom6,   4, ADDRESS_GUI_LABEL_TEXT},
   {contCustom7,   4, ADDRESS_GUI_LABEL_TEXT},
   {contCustom8,   4, ADDRESS_GUI_LABEL_TEXT},
   {contCustom9,   4, ADDRESS_GUI_LABEL_TEXT},
   {contNote,      5, ADDRESS_GUI_LABEL_TEXT}
};

static char *ldifMapType(int label)
{
   switch (label) {
    case 0:
      return "telephoneNumber";
    case 1:
      return "homePhone";
    case 2:
      return "facsimileTelephoneNumber";
    case 3:
      return "xotherTelephoneNumber";
    case 4:
      return "mail";
    case 5:
      return "xmainTelephoneNumber";
    case 6:
      return "pager";
    case 7:
      return "mobile";
    default:
      return "xunknownTelephoneNumber";
   }
}

static char *vCardMapType(int label)
{
   switch (label) {
    case 0:
      return "work";
    case 1:
      return "home";
    case 2:
      return "fax";
    case 3:
      return "x-other";
    case 4:
      return "email";
    case 5:
      return "x-main";
    case 6:
      return "pager";
    case 7:
      return "cell";
    default:
      return "x-unknown";
   }
}

/****************************** Main Code *************************************/
static void fprint_jpd_usage_string(FILE *out)
{
   fprintf(out, "%s-dump [ +format [-v] || [-h] || [-f] || [-D] || [-i] || [-A] || [-C] || [-T] || [-M] || [-N] ]\n", EPN);
   fprintf(out, _(" +D +A +T +M format like date +format.\n"));
   fprintf(out, _(" -v displays version and exits.\n"));
   fprintf(out, _(" -h displays help and exits.\n"));
   fprintf(out, _(" -f displays help for format codes.\n"));
   fprintf(out, _(" -D dump DateBook.\n"));
   fprintf(out, _(" -i dump DateBook in iCalendar format.\n"));
   fprintf(out, _(" -N dump appts for today in DateBook.\n"));
   fprintf(out, _(" -NYYYY/MM/DD dump appts on YYYY/MM/DD in DateBook.\n"));
   fprintf(out, _(" -A dump Address book.\n"));
   fprintf(out, _(" -C dumps Contacts database:-\n"));
   fprintf(out, _("    -Ct dumps as text (default).\n"));
   fprintf(out, _("    -Cc dumps as csv.\n"));
   fprintf(out, _("    -Cv dumps as vcard.\n"));
   fprintf(out, _("    -Cl dumps as ldif.\n"));
   fprintf(out, _(" -T dump ToDo list as CSV.\n"));
   fprintf(out, _(" -M dump Memos.\n"));
}

/* convert from UTF8 to local encoding */
static void utf8_to_local(char *str)
{
   char *local_buf;
   
   if (str == NULL)
      return;

   local_buf = g_locale_from_utf8(str, -1, NULL, NULL, NULL);
   if (local_buf)
   {
      g_strlcpy(str, local_buf, strlen(str)+1);
      free(local_buf);
   }
}

/* Parse the string and replace dangerous characters with spaces */
static void takeoutfunnies(char *str)
{
   int i;

   if (!str) {
      return;
   }
   for (i=0; str[i]; i++) {
      if ((str[i]=='\r') ||
          (str[i]=='\n') ||
          (str[i]=='\\') ||
          (str[i]=='\'') ||
          (str[i]=='"' ) ||
          (str[i]=='`' )
         ) {
         str[i]=' ';
      }
   }
}

static int dumpical(void)
{
   MyAppointment *mappt;
   AppointmentList *al, *temp_list;
   int i;
   char text[1024];
   char csv_text[65550];
   char *p;
   gchar *end;
   time_t ltime;
   struct tm *now;
   struct tm ical_time;
   long char_set;
   char username[256];
   char hostname[256];
   const char *svalue;
   long userid;

   /* ICAL setup */
   get_pref(PREF_CHAR_SET, &char_set, NULL);
   if (char_set < CHAR_SET_UTF) {
      fprintf(stderr, _("Warning: "
                        "Host character encoding is not UTF-8 based.\n"
                        "Exported ical file may not be standards-compliant\n"));
   }

   get_pref(PREF_USER, NULL, &svalue);
   g_strlcpy(text, svalue, 128);
   text[127] = '\0';
   charset_p2j(text, 128, char_set);
   str_to_ical_str(username, sizeof(username), text);
   get_pref(PREF_USER_ID, &userid, NULL);
   gethostname(text, sizeof(hostname));
   text[sizeof(hostname)-1]='\0';
   str_to_ical_str(hostname, sizeof(hostname), text);
   time(&ltime);
   now = gmtime(&ltime);

   al=NULL;
   get_days_appointments2(&al, NULL, 2, 2, 2, NULL);

   mappt=NULL;
   for (i=0, temp_list=al; temp_list; temp_list = temp_list->next, i++) {
      mappt = &(temp_list->mappt);
      /* RFC 2445: Internet Calendaring and Scheduling Core
       *           Object Specification */
      if (i == 0) {
         printf("BEGIN:VCALENDAR"CRLF);
         printf("VERSION:2.0"CRLF);
         printf("PRODID:%s"CRLF, FPI_STRING);
      }
      printf("BEGIN:VEVENT"CRLF);
      /* XXX maybe if it's secret export a VFREEBUSY busy instead? */
      if (mappt->attrib & dlpRecAttrSecret) {
         printf("CLASS:PRIVATE"CRLF);
      }
      printf("UID:palm-datebook-%08x-%08lx-%s@%s"CRLF,
              mappt->unique_id, userid, username, hostname);
      printf("DTSTAMP:%04d%02d%02dT%02d%02d%02dZ"CRLF,
              now->tm_year+1900,
              now->tm_mon+1,
              now->tm_mday,
              now->tm_hour,
              now->tm_min,
              now->tm_sec);
      if (mappt->appt.description) {
         g_strlcpy(text, mappt->appt.description, 51);
         /* truncate the string on a UTF-8 character boundary */
         if (char_set > CHAR_SET_UTF) {
            if (!g_utf8_validate(text, -1, (const gchar **)&end))
               *end = 0;
         }
      } else {
         /* Handle pathological case with null description. */
         text[0] = '\0';
      }
      if ((p = strchr(text, '\n'))) {
         *p = '\0';
      }
      str_to_ical_str(csv_text, sizeof(csv_text), text);
      printf("SUMMARY:%s%s"CRLF, csv_text,
              strlen(text) > 49 ? "..." : "");
      str_to_ical_str(csv_text, sizeof(csv_text), mappt->appt.description);
      printf("DESCRIPTION:%s", csv_text);
      if (mappt->appt.note && mappt->appt.note[0]) {
         str_to_ical_str(csv_text, sizeof(csv_text), mappt->appt.note);
         printf("\\n"CRLF" %s"CRLF, csv_text);
      } else {
         printf(CRLF);
      }
      if (mappt->appt.event) {
         printf("DTSTART;VALUE=DATE:%04d%02d%02d"CRLF,
                 mappt->appt.begin.tm_year+1900,
                 mappt->appt.begin.tm_mon+1,
                 mappt->appt.begin.tm_mday);
         /* XXX unclear: can "event" span multiple days? */
         /* since DTEND is "noninclusive", should this be the next day? */
         if (mappt->appt.end.tm_year != mappt->appt.begin.tm_year ||
             mappt->appt.end.tm_mon != mappt->appt.begin.tm_mon ||
             mappt->appt.end.tm_mday != mappt->appt.begin.tm_mday) {
            printf("DTEND;VALUE=DATE:%04d%02d%02d"CRLF,
                    mappt->appt.end.tm_year+1900,
                    mappt->appt.end.tm_mon+1,
                    mappt->appt.end.tm_mday);
         }
      } else {
         /*
          * These are "local" times, so will be treated as being in
          * the other person's timezone when they are imported.  This
          * may or may not be what is desired.  (DateBk calls this
          * "all time zones").
          *
          * DateBk timezones could help us decide what to do here.
          *
          * When using DateBk timezones, we could write them out
          * as iCalendar timezones.
          *
          * Maybe the default should be to write an absolute (UTC) time,
          * and only write a "local" time when using DateBk and it says to.
          * It'd be interesting to see if repeated events get translated
          * properly when doing this, or if they become not eligible for
          * daylight savings.  This probably depends on the importing
          * application.
          */
         printf("DTSTART:%04d%02d%02dT%02d%02d00"CRLF,
                 mappt->appt.begin.tm_year+1900,
                 mappt->appt.begin.tm_mon+1,
                 mappt->appt.begin.tm_mday,
                 mappt->appt.begin.tm_hour,
                 mappt->appt.begin.tm_min);
         printf("DTEND:%04d%02d%02dT%02d%02d00"CRLF,
                 mappt->appt.end.tm_year+1900,
                 mappt->appt.end.tm_mon+1,
                 mappt->appt.end.tm_mday,
                 mappt->appt.end.tm_hour,
                 mappt->appt.end.tm_min);
      }
      if (mappt->appt.repeatType != repeatNone) {
         int wcomma, rptday;
         const char *wday[] = {"SU","MO","TU","WE","TH","FR","SA"};
         printf("RRULE:FREQ=");
         switch (mappt->appt.repeatType) {
          case repeatNone:
            /* can't happen, just here to silence compiler warning */
            break;
          case repeatDaily:
            printf("DAILY");
            break;
          case repeatWeekly:
            printf("WEEKLY;BYDAY=");
            wcomma=0;
            for (i=0; i<7; i++) {
               if (mappt->appt.repeatDays[i]) {
                  if (wcomma) {
                     printf(",");
                  }
                  wcomma = 1;
                  printf("%s", wday[i]);
               }
            }
            break;
          case repeatMonthlyByDay:
            rptday = (mappt->appt.repeatDay / 7) + 1;
            printf("MONTHLY;BYDAY=%d%s", rptday == 5 ? -1 : rptday,
                    wday[mappt->appt.repeatDay % 7]);
            break;
          case repeatMonthlyByDate:
            printf("MONTHLY;BYMONTHDAY=%d", mappt->appt.begin.tm_mday);
            break;
          case repeatYearly:
            printf("YEARLY");
            break;
         }
         if (mappt->appt.repeatFrequency != 1) {
            if (mappt->appt.repeatType == repeatWeekly &&
                mappt->appt.repeatWeekstart >= 0 && mappt->appt.repeatWeekstart < 7) {
               printf(CRLF" ");  // Weekly repeats can exceed RFC line length
               printf(";WKST=%s", wday[mappt->appt.repeatWeekstart]);
            }
            printf(";INTERVAL=%d", mappt->appt.repeatFrequency);
         }
         if (!mappt->appt.repeatForever) {
            /* RFC 2445 is unclear on how to handle inclusivity for 
             * dates, rather than datestamps. Because most other
             * ical parsers assume non-inclusivity Jpilot needs to
             * add one day to the end date of repeating events. */
            memset(&ical_time, 0, sizeof(ical_time));
            ical_time.tm_year = mappt->appt.repeatEnd.tm_year;
            ical_time.tm_mon  = mappt->appt.repeatEnd.tm_mon;
            ical_time.tm_mday = mappt->appt.repeatEnd.tm_mday;
            ical_time.tm_isdst= -1;
            mktime(&ical_time);
            printf(";UNTIL=%04d%02d%02d",
                    ical_time.tm_year+1900,
                    ical_time.tm_mon+1,
                    ical_time.tm_mday);
         }
         printf(CRLF);
         if (mappt->appt.exceptions > 0) {
            for (i=0; i<mappt->appt.exceptions; i++) {
               printf("EXDATE;VALUE=DATE:%04d%02d%02d"CRLF,
                          mappt->appt.exception[i].tm_year+1900,
                          mappt->appt.exception[i].tm_mon+1,
                          mappt->appt.exception[i].tm_mday);
            }
         }
      }
      if (mappt->appt.alarm) {
         const char *units;
         printf("BEGIN:VALARM"CRLF);
         printf("ACTION:DISPLAY"CRLF);
         str_to_ical_str(csv_text, sizeof(csv_text), mappt->appt.description);
         printf("DESCRIPTION:%s"CRLF, csv_text);
         switch (mappt->appt.advanceUnits) {
          case advMinutes:
            units = "M";
            break;
          case advHours:
            units = "H";
            break;
          case advDays:
            units = "D";
            break;
          default: /* XXX */
            units = "?";
            break;
         }
         printf("TRIGGER:-PT%d%s"CRLF, mappt->appt.advance, units);
         printf("END:VALARM"CRLF);
      }
      printf("END:VEVENT"CRLF);
      if (temp_list->next == NULL) {
         printf("END:VCALENDAR"CRLF);
      }
   }
   free_AppointmentList(&al);
   return EXIT_SUCCESS;
}

static int dumpbook(void)
{
   AppointmentList *tal, *al;
   int num, i;
   int year, month, day, hour, minute;
   struct tm tm_dom;

   al = NULL;
   num = get_days_appointments(&al, NULL, NULL);
   if (num == 0) 
      return (0);

   /* get date */
   LIMIT(Nday,1,31);
   LIMIT(Nyear,1900,3000);
   LIMIT(Nmonth,1,12);
   tm_dom.tm_sec  = 0;
   tm_dom.tm_min  = 0;
   tm_dom.tm_hour = 0;
   tm_dom.tm_mday = Nday;
   tm_dom.tm_year = Nyear-1900;
   tm_dom.tm_mon  = Nmonth-1;
   tm_dom.tm_isdst = 0;
   mktime(&tm_dom);

#ifdef JDUMP_DEBUG
   printf("Dumpbook:dump year=%d,month=%d,day=%d\n", Nyear, Nmonth, Nday);
   printf("Dumpbook:date is %s", asctime(&tm_dom));
#endif

   for (tal=al; tal; tal = tal->next) {
      if (((dumpN == FALSE) || (isApptOnDate(&(tal->mappt.appt), &tm_dom) == TRUE))
         && (tal->mappt.rt != DELETED_PALM_REC)
         && (tal->mappt.rt != MODIFIED_PALM_REC)) {
       
         utf8_to_local(tal->mappt.appt.description);
         utf8_to_local(tal->mappt.appt.note);

         /* sort through format codes */
         for (i=2; formatD[i] != '\0'; i++) {
            if ( formatD[i] != '%') {
               printf("%c", formatD[i]);
            } else {
               switch (formatD[i+1]) {
                case '\0':
                     break;
                case 'n' :
                     printf("\n");
                     i++;
                     break;
                case 't' :
                     printf("\t");
                     i++;
                     break;
                case 'q' :
                     printf("'");
                     i++;
                     break;
                case 'Q' :
                     printf("\"");
                     i++;
                     break;
                case 'w' :
                     printf("%d", tal->mappt.appt.alarm);
                     i++;
                     break;
                case 'v' :
                     printf("%d", tal->mappt.appt.advance);
                     i++;
                     break;
                case 'u' :
                     switch (tal->mappt.appt.advanceUnits) {
                      case advMinutes : printf("m"); break;
                      case advHours   : printf("h"); break;
                      case advDays    : printf("d"); break;
                      default         : printf("x"); break;
                     }
                     i++;
                     break;
                case 'X' :
                     takeoutfunnies(tal->mappt.appt.note);
                     /* fall thru */
                case 'x' :
                     if (tal->mappt.appt.note != NULL) {
                        printf("%s", tal->mappt.appt.note);
                     }
                     i++;
                     break;
                case 'A' :
                     takeoutfunnies(tal->mappt.appt.description);
                     /* fall thru */
                case 'a' :
                     printf("%s", tal->mappt.appt.description);
                     i++;
                     break;
                case 'N' :      /* normal output */
                     /* start date+time, end date+time, "description" */
                     takeoutfunnies(tal->mappt.appt.description);
                     printf("%.4d/%.2d/%.2d,%.2d:%.2d,%.4d/%.2d/%.2d,%.2d:%.2d,\"%s\"",
                            tal->mappt.appt.begin.tm_year+1900,
                            tal->mappt.appt.begin.tm_mon+1,
                            tal->mappt.appt.begin.tm_mday,
                            tal->mappt.appt.begin.tm_hour,
                            tal->mappt.appt.begin.tm_min,
                            tal->mappt.appt.end.tm_year+1900,
                            tal->mappt.appt.end.tm_mon+1,
                            tal->mappt.appt.end.tm_mday,
                            tal->mappt.appt.end.tm_hour,
                            tal->mappt.appt.end.tm_min,
                            tal->mappt.appt.description
                     );
                     i++;
                     break;
                /* now process the double character format codes */
                case 'b' :
                case 'e' :
                     if (formatD[i+1] == 'b') {
                        year   = tal->mappt.appt.begin.tm_year+1900;
                        month  = tal->mappt.appt.begin.tm_mon+1;
                        day    = tal->mappt.appt.begin.tm_mday;
                        hour   = tal->mappt.appt.begin.tm_hour;
                        minute = tal->mappt.appt.begin.tm_min;
                     } else {
                        year   = tal->mappt.appt.end.tm_year+1900;
                        month  = tal->mappt.appt.end.tm_mon+1;
                        day    = tal->mappt.appt.end.tm_mday;
                        hour   = tal->mappt.appt.end.tm_hour;
                        minute = tal->mappt.appt.end.tm_min;
                     }
                     /* do %bx and %ex format codes */
                     switch (formatD[i+2]) {
                      case '\0':
                        printf("%c", formatD[i+1]);
                        break;
                      case 'm' :
                        printf("%.2d", month);
                        i++;
                        break;
                      case 'd' :
                        printf("%.2d", day);
                        i++;
                        break;
                      case 'y' :
                        printf("%.2d", year%100);
                        i++;
                        break;
                      case 'Y' :
                        printf("%.4d", year);
                        i++;
                        break;
                      case 'X' :
                        printf("%.2d/%.2d/%.2d", year-1900, month, day);
                        i++;
                        break;
                      case 'D' :
                        printf("%.2d/%.2d/%.2d", month, day, year%100);
                        i++;
                        break;
                      case 'H' :
                        printf("%.2d", hour);
                        i++;
                        break;
                      case 'k' :
                        printf("%d", hour);
                        i++;
                        break;
                      case 'I' :
                        if (hour < 13) {
                           printf("%.2d", hour);
                        } else {
                           printf("%.2d", hour-12);
                        }
                        i++;
                        break;
                      case 'l' :
                        if (hour < 13) {
                           printf("%d", hour);
                        } else {
                           printf("%d", hour-12);
                        }
                        i++;
                        break;
                      case 'M' :
                        printf("%.2d", minute);
                        i++;
                        break;
                      case 'p' :
                        if (hour < 13) {
                           printf("AM");
                        } else {
                           printf("PM");
                        }
                        i++;
                        break;
                      case 'T' :
                        printf("%.2d:%.2d", hour, minute);
                        i++;
                        break;
                      case 'r' :
                        if (hour < 13) {
                           printf("%.2d:%.2d AM", hour, minute);
                        } else {
                           printf("%.2d:%.2d PM", hour-12, minute);
                        }
                        i++;
                        break;
                      case 'h' :
                      case 'b' :
                        switch (month-1) {
                         case 0: printf("Jan"); break;
                         case 1: printf("Feb"); break;
                         case 2: printf("Mar"); break;
                         case 3: printf("Apr"); break;
                         case 4: printf("May"); break;
                         case 5: printf("Jun"); break;
                         case 6: printf("Jul"); break;
                         case 7: printf("Aug"); break;
                         case 8: printf("Sep"); break;
                         case 9: printf("Oct"); break;
                         case 10:printf("Nov"); break;
                         case 11:printf("Dec"); break;
                         default:printf("???"); break;
                        }
                        i++;
                        break;
                      case 'B' :
                        switch (month-1) {
                         case 0: printf("January");   break;
                         case 1: printf("February");  break;
                         case 2: printf("March");     break;
                         case 3: printf("April");     break;
                         case 4: printf("May");       break;
                         case 5: printf("June");      break;
                         case 6: printf("July");      break;
                         case 7: printf("August");    break;
                         case 8: printf("September"); break;
                         case 9: printf("October");   break;
                         case 10:printf("November");  break;
                         case 11:printf("December");  break;
                         default:printf("???");       break;
                        }
                        i++;
                        break;
                      default: /* 2 letter format codes */
                        printf("%c%c", formatD[i+1], formatD[i+2]);
                        i++;
                        break;
                     } /* end switch 2 letters format codes */
                     i++;
                     break;

                default: /* one letter format codes */
                  printf("%c", formatD[i+1]);
                  i++;
                  break;
               } /* end switch one letter format codes */
            } /* end if % */
         } /* for loop over formatD */
         printf("\n");
      } /* end if excluding deleted records */
   } /* end for loop on tal= */

   free_AppointmentList(&al);
   return EXIT_SUCCESS;
}

static int dumpaddress(void)
{
   AddressList *tal, *al;
   int num, i;
   struct AddressAppInfo ai;

   get_address_app_info(&ai);

   al = NULL;
   i = 0;
   num = get_addresses(&al, i);

   for (tal=al; tal; tal = tal->next) {
      if ((tal->maddr.rt != DELETED_PALM_REC) && 
          (tal->maddr.rt != MODIFIED_PALM_REC)) {
         for (num=0; num < 19; num++)
            utf8_to_local(tal->maddr.addr.entry[num]);

         for ( i=2 ; formatA[i] != '\0' ; i++) {
            if ( formatA[i] != '%') {
               printf("%c", formatA[i]);
            } else {
               switch (formatA[i+1]) {
                case '\0':
                     break;
                case 'n' :
                     printf("\n");
                     i++;
                     break;
                case 't' :
                     printf("\t");
                     i++;
                     break;
                case 'q' :
                     printf("'");
                     i++;
                     break;
                case 'Q' :
                     printf("\"");
                     i++;
                     break;
                case 'C' :
                     printf("%s", ai.category.name[tal->maddr.attrib & 0x0F]);
                     i++;
                     break;
                case 'N' :   /* normal output */
                     for (num=0; num < 19 ; num++) {
                        if (tal->maddr.addr.entry[num] == NULL) {
                           printf("\n");
                        } else {
                           printf("%s\n", tal->maddr.addr.entry[num]);
                        }
                     }
                     i++;
                     break;

#define PRIT if (tal->maddr.addr.entry[num] != NULL) { printf("%s", tal->maddr.addr.entry[num]); }

#define PRITE if (tal->maddr.addr.entry[num + 3] != NULL) { printf("%d", tal->maddr.addr.phoneLabel[num]); }

                case 'l' : num=0; PRIT; i++; break;
                case 'f' : num=1; PRIT; i++; break;
                case 'c' : num=2; PRIT; i++; break;
                case 'p' : num=3;
                     switch  (formatA[i+2]) {
                     case '1' : num=3; PRIT; i++; break;
                     case '2' : num=4; PRIT; i++; break;
                     case '3' : num=5; PRIT; i++; break;
                     case '4' : num=6; PRIT; i++; break;
                     case '5' : num=7; PRIT; i++; break;
                     }
                     i++;
                     break;
                case 'e' : num = 0;
                     switch (formatA[i+2]) {
                     case '1' : num=0; PRITE; i++; break;
                     case '2' : num=1; PRITE; i++; break;
                     case '3' : num=2; PRITE; i++; break;
                     case '4' : num=3; PRITE; i++; break;
                     case '5' : num=4; PRITE; i++; break;
                     }
                     i++;
                     break;
                case 'a' : num=8; PRIT; i++; break;
                case 'T' : num=9; PRIT; i++; break;
                case 's' : num=10; PRIT; i++; break;
                case 'z' : num=11; PRIT; i++; break;
                case 'u' : num=12; PRIT; i++; break;
                case 'm' : num=13; PRIT; i++; break;
                case 'U' :
                     switch (formatA[i+2]) {
                     case '1' : num=14; PRIT; i++; break;
                     case '2' : num=15; PRIT; i++; break;
                     case '3' : num=16; PRIT; i++; break;
                     case '4' : num=17; PRIT; i++; break;
                     }
                     i++;
                     break;
                case 'X' :
                     takeoutfunnies(tal->maddr.addr.entry[18]);
                     /* fall thru */
                case 'x' :
                     if (tal->maddr.addr.entry[18] != NULL) printf("%s", tal->maddr.addr.entry[18]);
                     i++;
                     break;
                default:        /* one letter ones */
                     printf("%c", formatA[i+1]);
                     i++;
                     break;
               } /* switch one letter ones */
            } /* fi */
         } /* for */
         printf("\n");
      }/* end if deleted*/
   }/* end for tal=*/

   free_AddressList(&al);
   return EXIT_SUCCESS;
}

static void dumpcontact()
{
   MyContact *mcont;
   const char *short_date;
   time_t ltime;
   struct tm *now;
   char str1[256], str2[256];
   char pref_time[40];
   int i, n;
   int record_num;
   char text[1024];
   char date_string[1024];
   char csv_text[65550];
   long char_set;
   char username[256];
   char hostname[256];
   const char *svalue;
   long userid;
   char birthday_str[255];
   const char *pref_date;
   int address_i, IM_i, phone_i;
   char *utf;

   static struct ContactAppInfo contact_app_info;

   ContactList *tcl, *cl;
   get_contact_app_info(&contact_app_info);
   cl = NULL;
   record_num = get_contacts(&cl, SORT_ASCENDING);

   /* Write a header for TEXT file */
   if (dumpC_type == EXPORT_TYPE_TEXT) {
      get_pref(PREF_SHORTDATE, NULL, &short_date);
      get_pref_time_no_secs(pref_time);
      time(&ltime);
      now = localtime(&ltime);
      strftime(str1, sizeof(str1), short_date, now);
      strftime(str2, sizeof(str2), pref_time, now);
      g_snprintf(date_string, sizeof(date_string), "%s %s", str1, str2);
      printf("Contact exported from %s %s on %s\n\n", 
                                               PN,VERSION,date_string);
   }

   /* Write a header to the CSV file */
   if (dumpC_type == EXPORT_TYPE_CSV) {
      printf("CSV contacts version "VERSION": Category, Private, ");

      address_i=phone_i=IM_i=0;
      for (i=0; i<NUM_CONTACT_FIELDS; i++) {
          switch (contact_schema[i].type) {
           case ADDRESS_GUI_IM_MENU_TEXT:
             printf("IM %d label, ", IM_i);
             printf("IM %d, ", IM_i);
             IM_i++;
             break;
           case ADDRESS_GUI_DIAL_SHOW_PHONE_MENU_TEXT:
             printf("Phone %d label, ", phone_i);
             printf("Phone %d, ", phone_i);
             phone_i++;
             break;
           case ADDRESS_GUI_ADDR_MENU_TEXT:
             printf("Address %d label, ", address_i);
             printf("Address %d, ", address_i);
             address_i++;
             break;
           case ADDRESS_GUI_BIRTHDAY:
             printf("%s, ", contact_app_info.labels[contact_schema[i].record_field]);
             printf("Reminder Advance, ");
                  break;
           case ADDRESS_GUI_LABEL_TEXT:
           case ADDRESS_GUI_WEBSITE_TEXT:
             printf("%s, ", contact_app_info.labels[contact_schema[i].record_field]);
             break;
          }
      }

      printf("Show in List\n");
   }  /* end writing CSV header */

   /* Special setup for VCARD export */
   if (dumpC_type == EXPORT_TYPE_VCARD) {
      get_pref(PREF_USER, NULL, &svalue);
      g_strlcpy(text, svalue, sizeof(text));
      str_to_ical_str(username, sizeof(username), text);
      get_pref(PREF_USER_ID, &userid, NULL);
      gethostname(text, sizeof(text));
      text[sizeof(text)-1]='\0';
      str_to_ical_str(hostname, sizeof(hostname), text);
   }

   /* Check encoding for LDIF output */
   if (dumpC_type == EXPORT_TYPE_LDIF) {
      get_pref(PREF_CHAR_SET, &char_set, NULL);
      if (char_set < CHAR_SET_UTF) {
         jp_logf(JP_LOG_WARN, _("Host character encoding is not UTF-8 based.\n Exported ldif file may not be standards-compliant\n"));
      }
   }

   get_pref(PREF_CHAR_SET, &char_set, NULL);

   for (record_num=0, tcl=cl; tcl; tcl = tcl->next, record_num++) {
      mcont = &tcl->mcont;
      switch (dumpC_type) {
         case EXPORT_TYPE_TEXT:
            utf = charset_p2newj(contact_app_info.category.name[mcont->attrib & 0x0F], 16, char_set);
            printf(("Category: %s\n"), utf);
            g_free(utf);
            printf(("Private: %s\n"),
            (mcont->attrib & dlpRecAttrSecret) ? _("Yes"):_("No"));

            address_i=phone_i=IM_i=0;
            for (i=0; i<NUM_CONTACT_FIELDS; i++) {
               /* Special handling for birthday which doesn't have an entry
                * field but instead has a flag and a tm struct field */
               if ((contact_schema[i].type == ADDRESS_GUI_BIRTHDAY) &&
                   mcont->cont.birthdayFlag)
               {
                  printf(("%s: "), contact_app_info.labels[contact_schema[i].record_field] ? contact_app_info.labels[contact_schema[i].record_field] : "");
                  birthday_str[0]='\0';
                  get_pref(PREF_SHORTDATE, NULL, &pref_date);
                  strftime(birthday_str, sizeof(birthday_str), pref_date, &(mcont->cont.birthday));
                  printf(("%s\n"), birthday_str);
               }

               if (mcont->cont.entry[contact_schema[i].record_field]) {
                  /* Print labels for menu selectable fields (Work, Fax, etc.) */
                  switch (contact_schema[i].type) {
                     case ADDRESS_GUI_IM_MENU_TEXT:
                        printf(("%s: "), contact_app_info.IMLabels[mcont->cont.IMLabel[IM_i]]);
                        IM_i++;
                        break;
                     case ADDRESS_GUI_DIAL_SHOW_PHONE_MENU_TEXT:
                        printf(("%s: "), contact_app_info.phoneLabels[mcont->cont.phoneLabel[phone_i]]);
                        phone_i++;
                        break;
                     case ADDRESS_GUI_ADDR_MENU_TEXT:
                        printf(("%s: "), contact_app_info.addrLabels[mcont->cont.addressLabel[address_i]]);
                        address_i++;
                        break;
                     default:
                        printf(("%s: "), contact_app_info.labels[contact_schema[i].record_field] ? contact_app_info.labels[contact_schema[i].record_field] : "");
                  }
                  /* Next print the entry field */
                  switch (contact_schema[i].type) {
                     case ADDRESS_GUI_LABEL_TEXT:
                     case ADDRESS_GUI_DIAL_SHOW_PHONE_MENU_TEXT:
                     case ADDRESS_GUI_IM_MENU_TEXT:
                     case ADDRESS_GUI_ADDR_MENU_TEXT:
                     case ADDRESS_GUI_WEBSITE_TEXT:
                        printf("%s\n", mcont->cont.entry[contact_schema[i].record_field]);
                        break;
                  }
               }
            }
            printf("\n");

            break;

         case EXPORT_TYPE_CSV:
            /* Category name */
            utf = charset_p2newj(contact_app_info.category.name[mcont->attrib & 0x0F], 16, char_set);
            printf("\"%s\",", utf);
            g_free(utf);

            /* Private */
            printf("\"%s\",", (mcont->attrib & dlpRecAttrSecret) ? "1":"0");

            address_i=phone_i=IM_i=0;
            /* The Contact entry values */
            for (i=0; i<NUM_CONTACT_FIELDS; i++) {
               switch (contact_schema[i].type) {
                  /* For labels that are menu selectable ("Work", Fax", etc)
                   * we list what they are set to in the record */
                  case ADDRESS_GUI_IM_MENU_TEXT:
                     str_to_csv_str(csv_text, contact_app_info.IMLabels[mcont->cont.IMLabel[IM_i]]);
                     printf("\"%s\",", csv_text);
                     str_to_csv_str(csv_text, mcont->cont.entry[contact_schema[i].record_field] ? mcont->cont.entry[contact_schema[i].record_field] : "");
                     printf("\"%s\",", csv_text);
                     IM_i++;
                     break;
                  case ADDRESS_GUI_DIAL_SHOW_PHONE_MENU_TEXT:
                     str_to_csv_str(csv_text, contact_app_info.phoneLabels[mcont->cont.phoneLabel[phone_i]]);
                     printf("\"%s\",", csv_text);
                     str_to_csv_str(csv_text, mcont->cont.entry[contact_schema[i].record_field] ? mcont->cont.entry[contact_schema[i].record_field] : "");
                     printf("\"%s\",", csv_text);
                     phone_i++;
                     break;
                  case ADDRESS_GUI_ADDR_MENU_TEXT:
                     str_to_csv_str(csv_text, contact_app_info.addrLabels[mcont->cont.addressLabel[address_i]]);
                     printf("\"%s\",", csv_text);
                     str_to_csv_str(csv_text, mcont->cont.entry[contact_schema[i].record_field] ? mcont->cont.entry[contact_schema[i].record_field] : "");
                     printf("\"%s\",", csv_text);
                     address_i++;
                     break;
                  case ADDRESS_GUI_LABEL_TEXT:
                  case ADDRESS_GUI_WEBSITE_TEXT:
                     printf("\"%s\",", mcont->cont.entry[contact_schema[i].record_field] ? mcont->cont.entry[contact_schema[i].record_field] : "");
                     break;
                  case ADDRESS_GUI_BIRTHDAY:
                     if (mcont->cont.birthdayFlag) {
                        birthday_str[0]='\0'; 
                        strftime(birthday_str, sizeof(birthday_str), "%Y/%02m/%02d", &(mcont->cont.birthday));
                        printf("\"%s\",", birthday_str);

                           if (mcont->cont.reminder) {
                               printf("\"%d\",", mcont->cont.advance);
                           } else {
                               printf("\"\",");
                           }

                     } else {
                        printf("\"\",");  /* for null Birthday field */
                        printf("\"\",");  /* for null Birthday Reminder field */
                     }
                     break;
               }
            }

            printf("\"%d\"\n", mcont->cont.showPhone);
            break;

         case EXPORT_TYPE_VCARD:
            /* RFC 2426: vCard MIME Directory Profile */
            printf("BEGIN:VCARD"CRLF);
            printf("VERSION:3.0"CRLF);
            printf("PRODID:%s"CRLF, FPI_STRING);
            if (mcont->attrib & dlpRecAttrSecret) {
               printf("CLASS:PRIVATE"CRLF);
            }
            printf("UID:palm-addressbook-%08x-%08lx-%s@%s"CRLF, mcont->unique_id, userid, username, hostname);
            utf = charset_p2newj(contact_app_info.category.name[mcont->attrib & 0x0F], 16, char_set);
            str_to_vcard_str(csv_text, sizeof(csv_text), utf);
            printf("CATEGORIES:%s"CRLF, csv_text);
            printf("\"%s\",", utf);
            g_free(utf);
            if (mcont->cont.entry[contLastname] || mcont->cont.entry[contFirstname]) {
               char *last = mcont->cont.entry[contLastname];
               char *first = mcont->cont.entry[contFirstname];
               printf("FN:");
               if (first) {
                  str_to_vcard_str(csv_text, sizeof(csv_text), first);
                  printf("%s", csv_text);
               }
               if (first && last) {
                  printf(" ");
               }
               if (last) {
                  str_to_vcard_str(csv_text, sizeof(csv_text), last);
                  printf("%s", csv_text);
               }
               printf(CRLF);
               printf("N:");
               if (last) {
                  str_to_vcard_str(csv_text, sizeof(csv_text), last);
                  printf("%s", csv_text);
               }
               printf(";");
               /* split up first into first + middle and do first;middle,middle*/
               if (first) {
                  str_to_vcard_str(csv_text, sizeof(csv_text), first);
                  printf("%s", csv_text);
               }
               printf(CRLF);
            } else if (mcont->cont.entry[contCompany]) {
               str_to_vcard_str(csv_text, sizeof(csv_text), mcont->cont.entry[contCompany]);
               printf("FN:%s"CRLF"N:%s"CRLF, csv_text, csv_text);
            } else {
               printf("FN:-Unknown-"CRLF"N:known-;-Un"CRLF);
            }
            if (mcont->cont.entry[contTitle]) {
               str_to_vcard_str(csv_text, sizeof(csv_text), mcont->cont.entry[contTitle]);
               printf("TITLE:%s"CRLF, csv_text);
            }
            if (mcont->cont.entry[contCompany]) {
               str_to_vcard_str(csv_text, sizeof(csv_text), mcont->cont.entry[contCompany]);
               printf("ORG:%s"CRLF, csv_text);
            }
            for (n = contPhone1; n < contPhone7 + 1; n++) {
               if (mcont->cont.entry[n]) {
                  str_to_vcard_str(csv_text, sizeof(csv_text), mcont->cont.entry[n]);
                  if (!strcmp(contact_app_info.phoneLabels[mcont->cont.phoneLabel[n-contPhone1]], _("E-mail"))) {
                     printf("EMAIL:%s"CRLF, csv_text);
                  } else {
                     printf("TEL;TYPE=%s", vCardMapType(mcont->cont.phoneLabel[n - contPhone1]));
                     if (mcont->cont.showPhone == n - contPhone1) {
                        printf(",pref");
                     }
                     printf(":%s"CRLF, csv_text);
                  }
               }
            }
            for (i=0; i<NUM_ADDRESSES; i++) {
               int address_i = 0, city_i = 0, state_i = 0, zip_i = 0, country_i = 0;
               switch (i) {
                  case 0:
                     address_i = contAddress1;
                     city_i = contCity1;
                     state_i = contState1;
                     zip_i = contZip1;
                     country_i = contCountry1;
                     break;
                  case 1:
                     address_i = contAddress2;
                     city_i = contCity2;
                     state_i = contState2;
                     zip_i = contZip2;
                     country_i = contCountry2;
                     break;
                  case 2:
                     address_i = contAddress3;
                     city_i = contCity3;
                     state_i = contState3;
                     zip_i = contZip3;
                     country_i = contCountry3;
                     break;
               }
               if (mcont->cont.entry[address_i] ||
                     mcont->cont.entry[city_i] ||
                     mcont->cont.entry[state_i] ||
                     mcont->cont.entry[zip_i] ||
                     mcont->cont.entry[country_i]) {
                  printf("ADR:;;");
                  for (n = address_i; n < country_i + 1; n++) {
                     if (mcont->cont.entry[n]) {
                        str_to_vcard_str(csv_text, sizeof(csv_text), mcont->cont.entry[n]);
                        printf("%s", csv_text);
                     }
                     if (n < country_i) {
                        printf(";");
                     }
                  }
               }
               printf(CRLF);
            }
            if (mcont->cont.entry[contCustom1] ||
                  mcont->cont.entry[contCustom2] ||
                  mcont->cont.entry[contCustom3] ||
                  mcont->cont.entry[contCustom4] ||
                  mcont->cont.entry[contCustom5] ||
                  mcont->cont.entry[contCustom6] ||
                  mcont->cont.entry[contCustom7] ||
                  mcont->cont.entry[contCustom8] ||
                  mcont->cont.entry[contCustom9] ||
                  mcont->cont.entry[contNote]) {
               int firstnote=1;
               printf("NOTE:");
               for (n=contCustom1; n<=contNote; n++) {
                  if (mcont->cont.entry[n]) {
                     str_to_vcard_str(csv_text, sizeof(csv_text), mcont->cont.entry[n]);
                     if (firstnote == 0) {
                        printf(" ");
                     }
                     if (n == contNote && firstnote) {
                        printf("%s\\n"CRLF, csv_text);
                     } else {
                        printf("%s:\\n"CRLF" %s\\n"CRLF, contact_app_info.labels[n], csv_text);
                     }
                     firstnote=0;
                  }
                  if (n == contCustom9) n = contNote - 1;
               }
            }
            printf("END:VCARD"CRLF);
            break;

         case EXPORT_TYPE_LDIF:
            /* RFC 2256 - organizationalPerson */
            /* RFC 2798 - inetOrgPerson */
            /* RFC 2849 - LDIF file format */
            if (record_num == 0) {
               printf("version: 1\n");
            }
            {
               char *cn;
               char *email = NULL;
               char *last = mcont->cont.entry[contLastname];
               char *first = mcont->cont.entry[contFirstname];
               for (n = contPhone1; n <= contPhone7; n++) {
                  if (mcont->cont.entry[n] && mcont->cont.phoneLabel[n - contPhone1] == 4) {
                     email = mcont->cont.entry[n];
                     break;
                  }
               }
               if (first || last) {
                  cn = csv_text;
                  snprintf(csv_text, sizeof(csv_text), "%s%s%s", first ? first : "",
                  first && last ? " " : "", last ? last : "");
                  if (!last) {
                     last = first;
                     first = NULL;
                  }
               } else if (mcont->cont.entry[contCompany]) {
                  last = mcont->cont.entry[contCompany];
                  cn = last;
               } else {
                  last = "Unknown";
                  cn = last;
               }
               /* maybe add dc=%s for each part of the email address? */
               /* Mozilla just does mail=%s */
               printf("dn: cn=%s%s%s", cn, email ? ",mail=" : "", email ? email : "");
               printf("dnQualifier: %s\n", PN);
               printf("objectClass: top\nobjectClass: person\n");
               printf("objectClass: organizationalPerson\n");
               printf("objectClass: inetOrgPerson\n");
               printf("cn: %s", cn);
               printf("sn: %s", last);
               if (first)
                  printf("givenName: %s", first);
               if (mcont->cont.entry[contCompany])
                  printf("o: %s", mcont->cont.entry[contCompany]);
               for (n = contPhone1; n <= contPhone7; n++) {
                  if (mcont->cont.entry[n]) {
                     printf("%s: %s", ldifMapType(mcont->cont.phoneLabel[n - contPhone1]), mcont->cont.entry[n]);
                  }
               }
               if (mcont->cont.entry[contAddress1])
                  printf("postalAddress: %s", mcont->cont.entry[contAddress1]);
               if (mcont->cont.entry[contCity1])
                  printf("l: %s", mcont->cont.entry[contCity1]);
               if (mcont->cont.entry[contState1])
                  printf("st: %s", mcont->cont.entry[contState1]);
               if (mcont->cont.entry[contZip1])
                  printf("postalCode: %s", mcont->cont.entry[contZip1]);
               if (mcont->cont.entry[contCountry1])
                  printf("c: %s", mcont->cont.entry[contCountry1]);

               if (mcont->cont.entry[contAddress2])
                  printf("postalAddress: %s", mcont->cont.entry[contAddress2]);
               if (mcont->cont.entry[contCity2])
                  printf("l: %s", mcont->cont.entry[contCity2]);
               if (mcont->cont.entry[contState2])
                  printf("st: %s", mcont->cont.entry[contState2]);
               if (mcont->cont.entry[contZip2])
                  printf("postalCode: %s", mcont->cont.entry[contZip2]);
               if (mcont->cont.entry[contCountry2])
                  printf("c: %s", mcont->cont.entry[contCountry2]);

               if (mcont->cont.entry[contAddress3])
                  printf("postalAddress: %s", mcont->cont.entry[contAddress3]);
               if (mcont->cont.entry[contCity3])
                  printf("l: %s", mcont->cont.entry[contCity3]);
               if (mcont->cont.entry[contState3])
                  printf("st: %s", mcont->cont.entry[contState3]);
               if (mcont->cont.entry[contZip3])
                  printf("postalCode: %s", mcont->cont.entry[contZip3]);
               if (mcont->cont.entry[contCountry3])
                  printf("c: %s", mcont->cont.entry[contCountry3]);

               if (mcont->cont.entry[contIM1]) {
                  strncpy(text, contact_app_info.IMLabels[mcont->cont.IMLabel[0]], 100);
                  printf("%s: %s", text, mcont->cont.entry[contIM1]);
               }
               if (mcont->cont.entry[contIM2]) {
                  strncpy(text, contact_app_info.IMLabels[mcont->cont.IMLabel[1]], 100);
                  printf("%s: %s", text, mcont->cont.entry[contIM2]);
               }

               if (mcont->cont.entry[contWebsite])
                  printf("website: %s", mcont->cont.entry[contWebsite]);
               if (mcont->cont.entry[contTitle])
                  printf("title: %s", mcont->cont.entry[contTitle]);
               if (mcont->cont.entry[contCustom1])
                  printf("custom1: %s", mcont->cont.entry[contCustom1]);
               if (mcont->cont.entry[contCustom2])
                  printf("custom2: %s", mcont->cont.entry[contCustom2]);
               if (mcont->cont.entry[contCustom3])
                  printf("custom3: %s", mcont->cont.entry[contCustom3]);
               if (mcont->cont.entry[contCustom4])
                  printf("custom4: %s", mcont->cont.entry[contCustom4]);
               if (mcont->cont.entry[contCustom5])
                  printf("custom5: %s", mcont->cont.entry[contCustom5]);
               if (mcont->cont.entry[contCustom6])
                  printf("custom6: %s", mcont->cont.entry[contCustom6]);
               if (mcont->cont.entry[contCustom7])
                  printf("custom7: %s", mcont->cont.entry[contCustom7]);
               if (mcont->cont.entry[contCustom8])
                  printf("custom8: %s", mcont->cont.entry[contCustom8]);
               if (mcont->cont.entry[contCustom9])
                  printf("custom9: %s", mcont->cont.entry[contCustom9]);
               if (mcont->cont.entry[contNote])
                  printf("description: %s", mcont->cont.entry[contNote]);
               printf("\n");
               break;
            }
         default:
            jp_logf(JP_LOG_WARN, _("Unknown export type\n"));
      }
   }
}

static int dumptodo(void)
{
   ToDoList *tal, *al;
   int num, i;
   int year, month, day, hour, minute;
   struct ToDoAppInfo ai;

   get_todo_app_info(&ai);

   al = NULL;
   num = get_todos(&al, SORT_ASCENDING);
   if (num == 0)
      return (0);

   for (tal=al; tal; tal = tal->next) {
      if ((tal->mtodo.rt != DELETED_PALM_REC) && 
          (tal->mtodo.rt != MODIFIED_PALM_REC)) {

         utf8_to_local(tal->mtodo.todo.description);
         utf8_to_local(tal->mtodo.todo.note);

         for ( i=2; formatT[i] != '\0'; i++) {
            if ( formatT[i] != '%') {
               printf("%c", formatT[i]);
            } else {
               switch (formatT[i+1]) {
                case '\0':
                     break;
                case 'n' :
                     printf("\n");
                     i++;
                     break;
                case 't' :
                     printf("\t");
                     i++;
                     break;
                case 'p' :
                     printf("%d", tal->mtodo.todo.priority);
                     i++;
                     break;
                case 'q' :
                     printf("'");
                     i++;
                     break;
                case 'Q' :
                     printf("\"");
                     i++;
                     break;
                case 'X' :
                     takeoutfunnies(tal->mtodo.todo.note);
                     /* fall thru */
                case 'x' :
                     if (tal->mtodo.todo.note != NULL) 
                        printf("%s", tal->mtodo.todo.note);
                     i++;
                     break;
                case 'C' :
                     printf("%s", ai.category.name[tal->mtodo.attrib & 0x0F]);
                     i++;
                     break;
                case 'A' :
                     takeoutfunnies(tal->mtodo.todo.description);
                     /* fall thru */
                case 'a' :
                     printf("%s", tal->mtodo.todo.description);
                     i++;
                     break;
                case 'c' :
                     printf("%d", tal->mtodo.todo.complete);
                     i++;
                     break;
                case 'i' :
                     printf("%d", tal->mtodo.todo.indefinite);
                     i++;
                     break;
                case 'N' :   /* normal output */
                     takeoutfunnies(tal->mtodo.todo.description);
                     if(tal->mtodo.todo.indefinite && 
                       !tal->mtodo.todo.complete) {
                        year   = 9999;
                        month  = 12;
                        day    = 31;
                        hour   = 23;
                        minute = 59;
                     } else {
                        year   = tal->mtodo.todo.due.tm_year+1900;
                        month  = tal->mtodo.todo.due.tm_mon+1;
                        day    = tal->mtodo.todo.due.tm_mday;
                        hour   = tal->mtodo.todo.due.tm_hour;
                        minute = tal->mtodo.todo.due.tm_min;
                     }
                     /* check garbage */
                     LIMIT(year,1900,9999);
                     LIMIT(month,1,12);
                     LIMIT(day,1,31);
                     LIMIT(hour,0,23);
                     LIMIT(minute,0,59);
                     printf("%d,%d,%d,%.4d/%.2d/%.2d,\"%s\"",
                            tal->mtodo.todo.complete,
                            tal->mtodo.todo.priority,
                            tal->mtodo.todo.indefinite,
                            year, month, day,
                            tal->mtodo.todo.description
                     );
                     i++;
                     break;
                /* now the double letter format codes */
                case 'd' :
                  if(tal->mtodo.todo.indefinite && !tal->mtodo.todo.complete) {
                     year   = 9999;
                     month  = 12;
                     day    = 31;
                     hour   = 23;
                     minute = 59;
                  } else {
                     year   = tal->mtodo.todo.due.tm_year+1900;
                     month  = tal->mtodo.todo.due.tm_mon+1;
                     day    = tal->mtodo.todo.due.tm_mday;
                     hour   = tal->mtodo.todo.due.tm_hour;
                     minute = tal->mtodo.todo.due.tm_min;
                  }
                  /* check garbage */
                  LIMIT(year,1900,9999);
                  LIMIT(month,1,12);
                  LIMIT(day,1,31);
                  LIMIT(hour,0,23);
                  LIMIT(minute,0,59);
                  /* do %dx formats */
                  switch (formatT[i+2]) {
                   case '\0':
                     printf("%c", formatT[i+1]);
                     break;
                   case 'm' :
                     printf("%.2d", month);
                     i++;
                     break;
                   case 'd' :
                     printf("%.2d", day);
                     i++;
                     break;
                   case 'y' :
                     printf("%.2d", year%100);
                     i++;
                     break;
                   case 'Y' :
                     printf("%.4d", year);
                     i++;
                     break;
                   case 'X' :
                     printf("%.2d/%.2d/%.2d", year-1900, month, day);
                     i++;
                     break;
                   case 'D' :
                     printf("%.2d/%.2d/%.2d", month, day, year%100);
                     i++;
                     break;
                   case 'H' :
                     printf("%.2d", hour);
                     i++;
                     break;
                   case 'k' :
                     printf("%d", hour);
                     i++;
                     break;
                   case 'I' :
                     if (hour < 13) {
                        printf("%.2d", hour);
                     } else {
                        printf("%.2d", hour-12);
                     }
                     i++;
                     break;
                   case 'l' :
                     if (hour < 13) {
                        printf("%d", hour);
                     } else {
                        printf("%d", hour-12);
                     }
                     i++;
                     break;
                   case 'M' :
                     printf("%.2d", minute);
                     i++;
                     break;
                   case 'p' :
                     if (hour < 13) {
                        printf("AM");
                     } else {
                        printf("PM");
                     }
                     i++;
                     break;
                   case 'T' :
                     printf("%.2d:%.2d", hour, minute);
                     i++;
                     break;
                   case 'r' :
                     if (hour < 13) {
                        printf("%.2d:%.2d AM", hour, minute);
                     } else {
                        printf("%.2d:%.2d PM", hour-12, minute);
                     }
                     i++;
                     break;
                   case 'h' :
                   case 'b' :
                     switch (month-1) {
                      case 0: printf("Jan"); break;
                      case 1: printf("Feb"); break;
                      case 2: printf("Mar"); break;
                      case 3: printf("Apr"); break;
                      case 4: printf("May"); break;
                      case 5: printf("Jun"); break;
                      case 6: printf("Jul"); break;
                      case 7: printf("Aug"); break;
                      case 8: printf("Sep"); break;
                      case 9: printf("Oct"); break;
                      case 10:printf("Nov"); break;
                      case 11:printf("Dec"); break;
                      default:printf("???"); break;
                     }
                     i++;
                     break;
                   case 'B' :
                     switch (month-1) {
                      case 0: printf("January");   break;
                      case 1: printf("February");  break;
                      case 2: printf("March");     break;
                      case 3: printf("April");     break;
                      case 4: printf("May");       break;
                      case 5: printf("June");      break;
                      case 6: printf("July");      break;
                      case 7: printf("August");    break;
                      case 8: printf("September"); break;
                      case 9: printf("October");   break;
                      case 10:printf("November");  break;
                      case 11:printf("December");  break;
                      default:printf("???");       break;
                     }
                     i++;
                     break;
                   default: /* 2 letter format codes */
                     printf("%c%c", formatT[i+1], formatT[i+2]);
                     i++;
                     break;
                  } /* switch 2 letter format codes*/
                     i++;
                     break;
                default:   /* one letter format codes */
                  printf("%c", formatT[i+1]);
                  i++;
                  break;
               } /* switch one letter format codes */
            } /* fi */
         } /* for */
         printf("\n");
      } /* end if deleted*/
   } /* end for tal=*/

   free_ToDoList(&al);
   return EXIT_SUCCESS;
}

static int dumpmemo(void)
{
   MemoList *tal, *al;
   int num,i;
   struct MemoAppInfo ai;

   get_memo_app_info(&ai);

   al = NULL;
   i = 0;
   num = get_memos(&al, i);
   if (num == 0)
      return (0);

   for (tal=al; tal; tal = tal->next) {
      if ((tal->mmemo.rt != DELETED_PALM_REC) && 
          (tal->mmemo.rt != MODIFIED_PALM_REC)) {

         utf8_to_local(tal->mmemo.memo.text);

         for ( i=2 ; formatM[i] != '\0' ; i++) {
            if ( formatM[i] != '%') {
               printf("%c", formatM[i]);
            } else {
               switch (formatM[i+1]) {
                case '\0':
                     break;
                case 'n' :
                     printf("\n");
                     i++;
                     break;
                case 't' :
                     printf("\t");
                     i++;
                     break;
                case 'q' :
                     printf("'");
                     i++;
                     break;
                case 'Q' :
                     printf("\"");
                     i++;
                     break;
                case 'X' :
                     takeoutfunnies(tal->mmemo.memo.text);
                     /* fall thru */
                case 'x' :
                     if (tal->mmemo.memo.text != NULL) 
                        printf("%s", tal->mmemo.memo.text);
                     i++;
                     break;
                case 'C' :
                     printf("%s", ai.category.name[tal->mmemo.attrib & 0x0F]);
                     i++;
                     break;
                case 'N' :   /* normal output */
                     printf("%s\n", tal->mmemo.memo.text);
                     i++;
                     break;
                default:    /* one letter ones */
                     printf("%c", formatM[i+1]);
                     i++;
                     break;
               } /* switch one letter ones */
            } /* fi */
         } /* for */
         printf("\n");
      } /* end if deleted */
   } /* end for tal= */

   free_MemoList(&al);
   return EXIT_SUCCESS;
}

int main(int argc, char *argv[])
{
   int i;
   time_t ltime;
   struct tm *now;

   /* fill dump format with default */
   formatD="+D%N";
   formatM="+M%N";
   formatA="+A%N";
   formatT="+T%N";
   dumpD  = FALSE;
   dumpI  = FALSE;
   dumpN  = FALSE;
   Nyear  = 1997;
   Nmonth = 12;
   Nday   = 31;
   dumpA  = FALSE;
   dumpM  = FALSE;
   dumpT  = FALSE;

   /* enable internationalization(i18n) before printing any output */
#if defined(ENABLE_NLS)
#  ifdef HAVE_LOCALE_H
   setlocale(LC_ALL, "");
#  endif
   bindtextdomain(EPN, LOCALEDIR);
   textdomain(EPN);
#endif

   /* If called with no arguments then print usage information */
   if (argc == 1)
   {
      fprint_jpd_usage_string(stderr);
      exit(0);
   }

   /* process command line options */
   for (i=1; i<argc; i++) {
      if (!strncasecmp(argv[i], "+D", 2)) {
         formatD=argv[i];
      }
      if (!strncasecmp(argv[i], "+M", 2)) {
         formatM=argv[i];
      }
      if (!strncasecmp(argv[i], "+A", 2)) {
         formatA=argv[i];
      }
      if (!strncasecmp(argv[i], "+T", 2)) {
         formatT=argv[i];
      }
      if (!strncasecmp(argv[i], "-v", 2)) {
         printf("jpilot-dump %s Copyright (C) hvrietsc@yahoo.com\n", VERSION);
         exit(0);
      }
      if (!strncasecmp(argv[i], "-h", 2)) {
         fprint_jpd_usage_string(stderr);
         exit(0);
      }
      if (!strncasecmp(argv[i], "-D", 2)) {
         dumpD = TRUE;
      }
      if (!strncasecmp(argv[i], "-I", 2)) {
         dumpI = TRUE;
      }
      if (!strncasecmp(argv[i], "-N", 2)) {
         dumpN = TRUE;
         dumpD = TRUE;
         if ( strlen(argv[i]) < 12) { 
            /* illegal format, use today's date */
            time(&ltime);
            now = localtime(&ltime);
            Nyear = 1900+now->tm_year;
            Nmonth= 1+now->tm_mon;
            Nday  = now->tm_mday;
         } else {
            Nyear = (argv[i][2]-'0')*1000+(argv[i][3]-'0')*100+(argv[i][4]-'0')*10 +argv[i][5]-'0';
            Nmonth= (argv[i][7]-'0')*10+(argv[i][8]-'0');
            Nday  = (argv[i][10]-'0')*10+(argv[i][11]-'0');
         }
#ifdef JDUMP_DEBUG
         printf("-N option: year=%d,month=%d,day=%d\n", Nyear, Nmonth, Nday);
#endif
      }
      if (!strncasecmp(argv[i], "-A", 2)) {
         dumpA = TRUE;
      }
      if (!strncasecmp(argv[i], "-C", 2)) {
         dumpC = TRUE;
         dumpC_type = EXPORT_TYPE_TEXT;
         if ( strlen(argv[i]) == 3 ) {
            switch (argv[i][2]) {
               case 't':
                  dumpC_type = EXPORT_TYPE_TEXT;
                  break;
               case 'c':
                  dumpC_type = EXPORT_TYPE_CSV;
                  break;
               case 'v':
                  dumpC_type = EXPORT_TYPE_VCARD;
                  break;
               case 'l':
                  dumpC_type = EXPORT_TYPE_LDIF;
                  break;
            }
         }
      }
      if (!strncasecmp(argv[i], "-T", 2)) {
         dumpT = TRUE;
      }
      if (!strncasecmp(argv[i], "-M", 2)) {
         dumpM = TRUE;
      }
      if (!strncasecmp(argv[i], "-f", 2)) {
         puts("+format GENERAL string:");
         puts("%% prints a %");
         puts("%n prints a newline");
         puts("%t prints a tab");
         puts("%q prints a \'");
         puts("%Q prints a \"");
         puts("%a prints appointment/todo description");
         puts("%A prints appointment/todo description CR,LF,\',\",//,` removed");
         puts("%x prints attached note");
         puts("%X prints attached note CR,LF,etc removed");
         puts("GENERAL date&time fields for value of %b see below:");
         puts("%bm prints month 00-12");
         puts("%bd prints day 01-31");
         puts("%by prints year 00-99");
         puts("%bY prints year 1970-....");
         puts("%bX prints years since 1900 00-999");
         puts("%bD prints mm/dd/yy");
         puts("%bH prints hour 00-23");
         puts("%bk prints hour 0-23");
         puts("%bI prints hour 01-12");
         puts("%bl prints hour 1-12");
         puts("%bM prints minute 00-59");
         puts("%bp prints AM or PM");
         puts("%bT prints HH:MM HH=00-23");
         puts("%br prints hh:mm [A|P]M hh=01-12");
         puts("%bh prints month Jan-Dec");
         puts("%bb prints month Jan-Dec");
         puts("%bB prints month January-December");
         printf("+D Datebook SPECIFIC strings (Default is %s):\n", formatD);
         puts("if %b=%b then begin date/time, if %b=%e then end date/time");
         puts("%N prints start-date,start-time,end-date,end-time,\"description\"");
         puts("%w prints 1 if alarm on else prints 0");
         puts("%v prints nr of advance alarm units");
         puts("%u prints unit of advance m(inute), h(our), d(ay)");
         printf("+A Address SPECIFIC strings (Default is %s):\n", formatA);
         puts("%N prints every field from last name to note on a separate line");
         puts("%C prints category of address as text");
         puts("%l last name");
         puts("%f first name");
         puts("%c company");
         puts("%p phone1, %p1 phone1, %p2 phone2, %p3 phone3, %p4 phone4 %p5 phone5");
         puts("%e phone label1, %e1 phone label1, %e2 phone label2, %e3 phone label3, %e4 phone label4 %e5 phone label5");
         puts("%a address");
         puts("%T town/city");
         puts("%s state");
         puts("%z zip");
         puts("%u country");
         puts("%m title");
         puts("%U1 user defined 1, %U2-%U4");
         puts("%x prints memo");
         puts("%X prints memo CR,LF,etc removed");
         printf("+T Todo SPECIFIC strings (Default is %s):\n", formatT);
         puts("if %b=%d then due date/time");
         puts("%N prints completed,priority,indefinite,due-date,\"description\"");
         puts("%c prints 1 if completed else 0");
         puts("%i prints 1 if indefinite else 0");
         puts("%p prints priority of todo item");
         printf("+M memo SPECIFIC strings (Default is %s):\n", formatM);
         puts("%N prints each memo separated by a blank line");
         puts("%x prints memo");
         puts("%X prints memo CR,LF,etc removed");
         puts("%C prints category of memo as text");
         exit(0);
      }  /* end printing format usage */
   }  /* end for over argc */

   pref_init();
   pref_read_rc_file();

   if (otherconv_init()) {
      printf("Error: could not set encoding\n");
      return EXIT_FAILURE;
   }

   /* dump selected database */
   if (dumpD) {
      dumpbook();
   }

   if (dumpI) {
      dumpical();
   }

   if (dumpA) {
      dumpaddress();
   }

   if (dumpC) {
      dumpcontact();
   }

   if (dumpT) {
      dumptodo();
   }

   if (dumpM) {
      dumpmemo();
   }

   /* clean up */
   otherconv_free();

   return EXIT_SUCCESS;
}


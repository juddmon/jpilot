/* jpilot-dump.c
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
 */
#include "config.h"
#include <stdio.h>
#include <pi-source.h>
#include <pi-socket.h>
#include <pi-datebook.h>
#include <pi-dlp.h>
#include <pi-file.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>

#include "utils.h"
#include "log.h"
#include "prefs.h"

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

#define DUMP_USAGE_STRING "\njpilot_dump [ +format [-v] || [-h] || [-B] || [-M] || [-A] || [-T] ]\n"\
" +B +M +A +T format like date +format (use -? for more info).\n"\
" -v displays version and exits.\n"\
" -h displays help and exits.\n"\
" -p do not load plugins.\n"\
" -B dump dateBook as CSV.\n"\
" -NYYY/MM/DD dump apps on YYYY/MM/DD in dateBook as CSV.\n"\
" -N dump apps on today in dateBook as CSV.\n"\
" -M dump Memos as CSV.\n"\
" -A dump Address book as CSV.\n"\
" -T dump Todo list as CSV.\n"\

#define LIMIT(a,b,c) if (a < b) {a=b;} if (a > c) {a=c;}

int dumpbook();
int dumptodo();
int dumpmemo();
int dumpaddress();

/* hack */
GtkWidget *glob_dialog;
GtkWidget *glob_date_label;
int pipe_out;
//int sync_once(struct my_sync_info *sync_info)
int sync_once(void *sync_info)
{
   ;
}

/* dump switches */
int dumpB;
int dumpN;
int Nyear;
int Nmonth;
int Nday;
int dumpA;
int dumpM;
int dumpT;
char *formatB;
char *formatM;
char *formatA;
char *formatT;

int dumpbook()

{
   AppointmentList *tal, *al;
   int num,i;
   int year,month,day,hour,minute;
   struct tm tm_dom;

   al = NULL;
   num = get_days_appointments(&al, NULL);
   if (num != 0) return (num);

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
   mktime(&tm_dom);
/*
printf("in dump year=%d,month=%d,day=%d\n",Nyear,Nmonth,Nday);
printf("date is %s",asctime(&tm_dom));
*/
   for (tal=al; tal; tal = tal->next) {
    if ( 
        ((dumpN == FALSE) ||  (isApptOnDate(&(tal->ma.a), &tm_dom) == TRUE))
	&& (tal->ma.rt != DELETED_PALM_REC) 
	&& (tal->ma.rt != MODIFIED_PALM_REC)
       ) {
 for ( i=2 ; formatB[i] != '\0' ; i++) {
  if ( formatB[i] != '%') {
   printf("%c",formatB[i]);
  } else {
   switch (formatB[i+1]) {
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
         printf("%d",tal->ma.a.alarm);
	 i++;
	 break;
    case 'v' :
    	 printf("%d",tal->ma.a.advance);
	 i++;
	 break;
    case 'u' :
    	 switch (tal->ma.a.advanceUnits) {
	 case advMinutes : printf("m"); break;
	 case advHours   : printf("h"); break;
	 case advDays    : printf("d"); break;
	 default         : printf("x"); break;
	 }/*switch*/
	 i++;
	 break;
    case 'X' :
    	 remove_cr_lfs(tal->ma.a.note);
	 /* fall thru */
    case 'x' :
    	 if (tal->ma.a.note != NULL) printf("%s",tal->ma.a.note);
	 i++;
	 break;
    case 'A' :
    	 remove_cr_lfs(tal->ma.a.description);
	 /* fall thru */
    case 'a' :
    	 printf("%s",tal->ma.a.description);
	 i++;
	 break;
    case 'N' :	/* normal output */
    	 /* start date+time, end date+time, "description" */
    	 remove_cr_lfs(tal->ma.a.description);
    	 printf("%.4d/%.2d/%.2d,%.2d:%.2d,%.4d/%.2d/%.2d,%.2d:%.2d,\"%s\"",
		tal->ma.a.begin.tm_year+1900,
		tal->ma.a.begin.tm_mon+1,
		tal->ma.a.begin.tm_mday,
		tal->ma.a.begin.tm_hour,
		tal->ma.a.begin.tm_min,
		tal->ma.a.end.tm_year+1900,
		tal->ma.a.end.tm_mon+1,
		tal->ma.a.end.tm_mday,
		tal->ma.a.end.tm_hour,
		tal->ma.a.end.tm_min,
		tal->ma.a.description
	 );
	 i++;
	 break;
/* now the double ones */
    case 'b' :
    case 'e' :
	 if (formatB[i+1] == 'b') {
	 	year   = tal->ma.a.begin.tm_year+1900;
	 	month  = tal->ma.a.begin.tm_mon+1;
	 	day    = tal->ma.a.begin.tm_mday;
	 	hour   = tal->ma.a.begin.tm_hour;
	 	minute = tal->ma.a.begin.tm_min;
	 } else {
	 	year   = tal->ma.a.end.tm_year+1900;
	 	month  = tal->ma.a.end.tm_mon+1;
	 	day    = tal->ma.a.end.tm_mday;
	 	hour   = tal->ma.a.end.tm_hour;
	 	minute = tal->ma.a.end.tm_min;
	 }
/*	 i++; /* move to next one */
/* do %bx and %ex ones */
	 switch (formatB[i+2]) {
    case '\0':
    	 printf("%c",formatB[i+1]);
    	 break;
    case 'm' :
         printf("%.2d",month);
	 i++;
	 break;
    case 'd' :
         printf("%.2d",day);
	 i++;
	 break;
    case 'y' :
         printf("%.2d",year%100);
	 i++;
	 break;
    case 'Y' :
    	 printf("%.4d",year);
	 i++;
	 break;
    case 'X' :
         printf("%.2d/%.2d/%.2d",year-1900,month,day);
	 i++;
	 break;
    case 'D' :
         printf("%.2d/%.2d/%.2d",month,day,year%100);
	 i++;
	 break;
    case 'H' :
         printf("%.2d",hour);
	 i++;
	 break;
    case 'k' :
         printf("%d",hour);
	 i++;
	 break;
    case 'I' :
    	 if (hour < 13) {
	         printf("%.2d",hour);
	 } else {
	         printf("%.2d",hour-12);
	 }	 
	 i++;
	 break;
    case 'l' :
    	 if (hour < 13) {
	         printf("%d",hour);
	 } else {
	         printf("%d",hour-12);
	 }	 
	 i++;
	 break;
    case 'M' :
         printf("%.2d",minute);
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
         printf("%.2d:%.2d",hour,minute);
	 i++;
	 break;
    case 'r' :
         if (hour < 13) {
	         printf("%.2d:%.2d AM",hour,minute);
         } else {
	         printf("%.2d:%.2d PM",hour-12,minute);
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
	 }/*switch*/
         i++;
	 break;
    case 'B' :
         switch (month-1) {
          case 0: printf("January"); break;
	  case 1: printf("February"); break;
	  case 2: printf("March"); break;
	  case 3: printf("April"); break;
	  case 4: printf("May"); break;
	  case 5: printf("June"); break;
	  case 6: printf("July"); break;
	  case 7: printf("August"); break;
	  case 8: printf("September"); break;
	  case 9: printf("October"); break;
	  case 10:printf("November"); break;
	  case 11:printf("December"); break;
	  default:printf("???"); break;
	 }/*switch*/
         i++;
	 break;
	 default: /* 2 letter ones*/
	  printf("%c%c",formatB[i+1],formatB[i+2]);
	  i++;
	  break;
	 }/*switch 2 letters*/
	 i++;
         break;
    default:	/* one letter ones */
         printf("%c",formatB[i+1]);
         i++;
	 break;
   }/*switch one letter ones*/

  }/*fi*/
 }/*for*/
 printf("\n");
 }/*end if deleted*/
 }/*end for tal=*/

   free_AppointmentList(&al);
   return 0;
}

int dumptodo()
{
   ToDoList *tal, *al;
   int num,i;
   int year,month,day,hour,minute;

   al = NULL;
   num = get_todos(&al);

   for (tal=al; tal; tal = tal->next) {
    if ( (tal->mtodo.rt != DELETED_PALM_REC) && (tal->mtodo.rt != MODIFIED_PALM_REC)) {

 for ( i=2 ; formatT[i] != '\0' ; i++) {
  if ( formatT[i] != '%') {
   printf("%c",formatT[i]);
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
    case 'q' :
	 printf("'");
	 i++;
	 break;
    case 'Q' :
	 printf("\"");
	 i++;
	 break;
    case 'X' :
    	 remove_cr_lfs(tal->mtodo.todo.note);
	 /* fall thru */
    case 'x' :
    	 if (tal->mtodo.todo.note != NULL) printf("%s",tal->mtodo.todo.note);
	 i++;
	 break;
    case 'A' :
    	 remove_cr_lfs(tal->mtodo.todo.description);
	 /* fall thru */
    case 'a' :
    	 printf("%s",tal->mtodo.todo.description);
	 i++;
	 break;
    case 'c' :
    	 printf("%d",tal->mtodo.todo.complete);
	 i++;
	 break;
    case 'i' :
    	 printf("%d",tal->mtodo.todo.indefinite);
	 i++;
	 break;
    case 'N' :	/* normal output */
    	 remove_cr_lfs(tal->mtodo.todo.description);
         if(tal->mtodo.todo.indefinite && !tal->mtodo.todo.complete) {
           year = 9999;
	   month = 12;
	   day= 31;
	   hour=23;
	   minute=59;
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
/* now the double ones */
    case 'd' :
      if(tal->mtodo.todo.indefinite && !tal->mtodo.todo.complete) {
        year = 9999;
	month = 12;
	day= 31;
	hour=23;
	minute=59;
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
/* do %dx ones */
	 switch (formatT[i+2]) {
    case '\0':
    	 printf("%c",formatT[i+1]);
    	 break;
    case 'm' :
         printf("%.2d",month);
	 i++;
	 break;
    case 'd' :
         printf("%.2d",day);
	 i++;
	 break;
    case 'y' :
         printf("%.2d",year%100);
	 i++;
	 break;
    case 'Y' :
    	 printf("%.4d",year);
	 i++;
	 break;
    case 'X' :
         printf("%.2d/%.2d/%.2d",year-1900,month,day);
	 i++;
	 break;
    case 'D' :
         printf("%.2d/%.2d/%.2d",month,day,year%100);
	 i++;
	 break;
    case 'H' :
         printf("%.2d",hour);
	 i++;
	 break;
    case 'k' :
         printf("%d",hour);
	 i++;
	 break;
    case 'I' :
    	 if (hour < 13) {
	         printf("%.2d",hour);
	 } else {
	         printf("%.2d",hour-12);
	 }	 
	 i++;
	 break;
    case 'l' :
    	 if (hour < 13) {
	         printf("%d",hour);
	 } else {
	         printf("%d",hour-12);
	 }	 
	 i++;
	 break;
    case 'M' :
         printf("%.2d",minute);
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
         printf("%.2d:%.2d",hour,minute);
	 i++;
	 break;
    case 'r' :
         if (hour < 13) {
	         printf("%.2d:%.2d AM",hour,minute);
         } else {
	         printf("%.2d:%.2d PM",hour-12,minute);
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
	 }/*switch*/
         i++;
	 break;
    case 'B' :
         switch (month-1) {
          case 0: printf("January"); break;
	  case 1: printf("February"); break;
	  case 2: printf("March"); break;
	  case 3: printf("April"); break;
	  case 4: printf("May"); break;
	  case 5: printf("June"); break;
	  case 6: printf("July"); break;
	  case 7: printf("August"); break;
	  case 8: printf("September"); break;
	  case 9: printf("October"); break;
	  case 10:printf("November"); break;
	  case 11:printf("December"); break;
	  default:printf("???"); break;
	 }/*switch*/
         i++;
	 break;
	 default: /* 2 letter ones*/
	  printf("%c%c",formatT[i+1],formatT[i+2]);
	  i++;
	  break;
	 }/*switch 2 letters*/
	 i++;
         break;
    default:	/* one letter ones */
         printf("%c",formatT[i+1]);
         i++;
	 break;
   }/*switch one letter ones*/

  }/*fi*/
 }/*for*/
 printf("\n");
 }/*end if deleted*/
 }/*end for tal=*/

   free_ToDoList(&al);
   return 0;
}

int dumpmemo()
{
}

int dumpaddress()
{
}

int main(int   argc,
	 char *argv[])
{
   int filedesc[2];
   long ivalue;
   const char *svalue;
   int sync_only;
   int i;
   char title[MAX_PREF_VALUE+40];
   long pref_width, pref_height;
   time_t ltime;
   struct tm *now;

/* fill dump format with default */
   formatB="+B%N";
   formatM="+M%N";
   formatA="+A%N";
   formatT="+T%N";
   dumpB = FALSE;
   dumpN = FALSE;
   Nyear = 1997;
   Nmonth = 12;
   Nday = 31;
   dumpA = FALSE;
   dumpM = FALSE;
   dumpT = FALSE;

   for (i=1; i<argc; i++) {
      if (!strncasecmp(argv[i], "+B", 2)) {
	 formatB=argv[i];
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
	 printf("%s\n", VERSION_STRING);
	 exit(0);
      }
      if (!strncasecmp(argv[i], "-h", 2)) {
	 printf("%s\n", DUMP_USAGE_STRING);
	 exit(0);
      }
      if (!strncasecmp(argv[i], "-B", 2)) {
	dumpB = TRUE;
      }
      if (!strncasecmp(argv[i], "-N", 2)) {
	dumpN = TRUE;
	dumpB = TRUE;
	if ( strlen(argv[i]) < 12) { /* illegal format use today */
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
/*
printf("year=%d,month=%d,day=%d\n",Nyear,Nmonth,Nday);
*/
      }
      if (!strncasecmp(argv[i], "-A", 2)) {
	dumpA = TRUE;
      }
      if (!strncasecmp(argv[i], "-M", 2)) {
	dumpM = TRUE;
      }
      if (!strncasecmp(argv[i], "-T", 2)) {
	dumpT = TRUE;
      }
      if (!strncasecmp(argv[i], "-?", 2)) {
	puts("+format GENERAL string:");
	puts("%% a %");
	puts("%n a newline");
	puts("%t a tab");
	puts("%q a \'");
	puts("%Q a \"");
	puts("%a appointment/todo description");
	puts("%A appointment/todo description CR,LF removed");
	puts("%x attached note");
	puts("%X attached note CR,LF removed");
	puts("GENERAL date&time fields for value of %b see below:");
	puts("%bm month 00-12");
	puts("%bd day 01-31");
	puts("%by year 00-99");
	puts("%bY year 1970-....");
	puts("%bX years since 1900 00-999");
	puts("%bD mm/dd/yy");
	puts("%bH hour 00-23");
	puts("%bk hour 0-23");
	puts("%bI hour 01-12");
	puts("%bl hour 1-12");
	puts("%bM minute 00-59");
	puts("%bp AM or PM");
	puts("%bT HH:MM HH=00-23");
	puts("%br hh:mm [A|P]M hh=01-12");
	puts("%bh month Jan-Dec");
	puts("%bb month Jan-Dec");
	puts("%bB month January-December");
	printf("+B Datebook SPECIFIC strings (Default is %s):\n",formatB);
	puts("if %b=%b then begin date/time, if %b=%e then end date/time");
	puts("%N start-date,start-time,end-date,end-time,\"description\"");
	puts("%w alarm on 1/0");
	puts("%v nr of advance alarm units");
	puts("%u unit of advance m(inute), h(our), d(ay)");
	printf("+M Memo SPECIFIC strings (Default is %s):\n",formatM);
	printf("+A Address SPECIFIC strings (Default is %s):\n",formatA);
	printf("+T Todo SPECIFIC strings (Default is %s):\n",formatT);
	puts("if %b=%d then due date/time");
	puts("%N completed,priority,indefinite,due-date,\"description\"");
	puts("%c complete 1/0");
	puts("%i indefinite 1/0");
	exit(0);
      }/*fi*/
   }/*for*/

   if (dumpB) {
	dumpbook();
   }/*end dumpB*/

   if (dumpA) {
	dumpaddress();
   }/*end dumpA*/

   if (dumpM) {
	dumpmemo();
   }/*end dumpM*/

   if (dumpT) {
	dumptodo();
   }/*end dumpT*/


 /* clean up */
 /*  rmlock(); */

   return 0;
}

/* jpilot-dump.c
 * hvrietsc: 10/19/2000 added memo dump
 * hvrietsc: 10/17/2000 added %p for priority of todo
 * hvrietsc: 7/18/2000 added %C in todo to print categories
 * hvrietsc: 3/11/00 own version of remove_cr_lfs to take out ` ' " \ stuff
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
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>
#include <pi-source.h>
#include <pi-socket.h>
#include <pi-datebook.h>
#include <pi-dlp.h>
#include <pi-file.h>
#include "utils.h"
#include "log.h"
#include "prefs.h"
#include "datebook.h"
#include "todo.h"

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
" -B dump dateBook.\n"\
" -NYYY/MM/DD dump apps on YYYY/MM/DD in dateBook.\n"\
" -N dump apps on today in dateBook.\n"\
" -M dump Memos.\n"\
" -A dump Address book.\n"\
" -T dump Todo list as CSV.\n"\

#define LIMIT(a,b,c) if (a < b) {a=b;} if (a > c) {a=c;}

/*
 * Parse the string and replace CR and LFs with spaces
 */
void takeoutfunnies(char *str)
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

int dumpbook();
int dumptodo();
int dumpmemo();
int dumpaddress();

/* hack */
GtkWidget *glob_dialog;
GtkWidget *glob_date_label;
int pipe_to_parent;

int sync_once(void *sync_info)
{
   return 0;
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
   num = get_days_appointments(&al, NULL, NULL);
   if (num == 0) return (num);

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
    	 takeoutfunnies(tal->ma.a.note);
	 /* fall thru */
    case 'x' :
    	 if (tal->ma.a.note != NULL) printf("%s",tal->ma.a.note);
	 i++;
	 break;
    case 'A' :
    	 takeoutfunnies(tal->ma.a.description);
	 /* fall thru */
    case 'a' :
    	 printf("%s",tal->ma.a.description);
	 i++;
	 break;
    case 'N' :	/* normal output */
    	 /* start date+time, end date+time, "description" */
    	 takeoutfunnies(tal->ma.a.description);
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
/*       i++;  move to next one */
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
   struct ToDoAppInfo ai;
   static struct ToDoAppInfo *glob_Ptodo_app_info;

   get_todo_app_info(&ai);
   glob_Ptodo_app_info = &ai;

   al = NULL;
   num = get_todos(&al, SORT_ASCENDING);

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
    case 'p' :
         printf("%d",tal->mtodo.todo.priority);
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
    	 if (tal->mtodo.todo.note != NULL) printf("%s",tal->mtodo.todo.note);
	 i++;
	 break;
    case 'C' :
    	 printf("%s",glob_Ptodo_app_info->category.name[tal->mtodo.attrib & 0x0F]);
	 i++;
	 break;
    case 'A' :
    	 takeoutfunnies(tal->mtodo.todo.description);
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
    	 takeoutfunnies(tal->mtodo.todo.description);
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

extern void free_MemoList(MemoList **memo);
extern int get_memos(MemoList **memo_list, int sort_order);
extern int get_memo_app_info(struct MemoAppInfo *ai);

int dumpmemo()
{
   MemoList *tal, *al;
   int num,i;
   struct MemoAppInfo ai;
   static struct MemoAppInfo *glob_PMemo_app_info;

   get_memo_app_info(&ai);
   glob_PMemo_app_info = &ai;

   al = NULL;
   i = 0;
   num = get_memos(&al,i);

   for (tal=al; tal; tal = tal->next) {
    if ( (tal->mmemo.rt != DELETED_PALM_REC) && (tal->mmemo.rt != MODIFIED_PALM_REC)) {

 for ( i=2 ; formatM[i] != '\0' ; i++) {
  if ( formatM[i] != '%') {
   printf("%c",formatM[i]);
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
    	 if (tal->mmemo.memo.text != NULL) printf("%s",tal->mmemo.memo.text);
	 i++;
	 break;
    case 'C' :
    	 printf("%s",glob_PMemo_app_info->category.name[tal->mmemo.attrib & 0x0F]);
	 i++;
	 break;
    case 'N' :	/* normal output */
	 printf("%s\n",tal->mmemo.memo.text);
	 i++;
         break;
    default:	/* one letter ones */
         printf("%c",formatM[i+1]);
         i++;
	 break;
   }/*switch one letter ones*/

  }/*fi*/
 }/*for*/
 printf("\n");
 }/*end if deleted*/
 }/*end for tal=*/

   free_MemoList(&al);
   return 0;
}

extern int get_address_app_info(struct AddressAppInfo *aai);
extern int get_addresses(AddressList **address_list, int sort_order);
extern void free_AddressList(AddressList **al);

int dumpaddress()
{
   AddressList *tal, *al;
   int num,i;
   struct AddressAppInfo ai;
   static struct AddressAppInfo *glob_PAddress_app_info;

   get_address_app_info(&ai);
   glob_PAddress_app_info = &ai;

   al = NULL;
   i = 0;
   num = get_addresses(&al,i);

   for (tal=al; tal; tal = tal->next) {
    if ( (tal->ma.rt != DELETED_PALM_REC) && (tal->ma.rt != MODIFIED_PALM_REC)) {

 for ( i=2 ; formatA[i] != '\0' ; i++) {
  if ( formatA[i] != '%') {
   printf("%c",formatA[i]);
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
    	 printf("%s",glob_PAddress_app_info->category.name[tal->ma.attrib & 0x0F]);
	 i++;
	 break;
    case 'N' :	/* normal output */
	 for (num=0; num < 19 ; num++) {
	 	if (tal->ma.a.entry[num] == NULL) {
			printf("\n");
		} else {
			printf("%s\n",tal->ma.a.entry[num]);
		}
	 }
	 i++;
         break;

#define PRIT if (tal->ma.a.entry[num] != NULL) { printf("%s",tal->ma.a.entry[num]); }

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
    	 takeoutfunnies(tal->ma.a.entry[18]);
	 /* fall thru */
    case 'x' :
    	 if (tal->ma.a.entry[18] != NULL) printf("%s",tal->ma.a.entry[18]);
	 i++;
	 break;
    default:	/* one letter ones */
         printf("%c",formatA[i+1]);
         i++;
	 break;
   }/*switch one letter ones*/

  }/*fi*/
 }/*for*/
 printf("\n");
 }/*end if deleted*/
 }/*end for tal=*/

   free_AddressList(&al);
   return 0;
}

int main(int   argc,
	 char *argv[])
{
   int i;
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
	 printf("jpilot-dump 0.98-1 Copyright (C) hvrietsc@yahoo.com\n");
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
	printf("+B Datebook SPECIFIC strings (Default is %s):\n",formatB);
	puts("if %b=%b then begin date/time, if %b=%e then end date/time");
	puts("%N prints start-date,start-time,end-date,end-time,\"description\"");
	puts("%w prints 1 if alarm on else prints 0");
	puts("%v prints nr of advance alarm units");
	puts("%u prints unit of advance m(inute), h(our), d(ay)");
	printf("+M memo SPECIFIC strings (Default is %s):\n",formatM);
	puts("%x prints memo");
	puts("%X prints memo CR,LF,etc removed");
	puts("%C prints category of memo as text");
	printf("+A Address SPECIFIC strings (Default is %s):\n",formatA);
	puts("%C prints category of address as text");
	puts("%l last name");
	puts("%f first name");
	puts("%c company");
	puts("%p phone1, %p1 phone1, %p2 phone2, %p3 phone3, %p4 phone4 %p5 phone5");
	puts("%a address");
	puts("%T town/city");
	puts("%s state");
	puts("%z zip");
	puts("%u country");
	puts("%m title");
	puts("%U1 user defined 1, %U2-%U4");
	puts("%x prints memo");
	puts("%X prints memo CR,LF,etc removed");
	printf("+T Todo SPECIFIC strings (Default is %s):\n",formatT);
	puts("if %b=%d then due date/time");
	puts("%N prints completed,priority,indefinite,due-date,\"description\"");
	puts("%c prints 1 if completed else 0");
	puts("%i prints 1 if indefinite else 0");
	puts("%p prints priority of todo item");
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

   return 0;
}

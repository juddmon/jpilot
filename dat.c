/* dat.c
 * A module of J-Pilot http://jpilot.org
 * 
 * Copyright (C) 2001-2002 by Judd Montgomery
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
/*
 * Thanks to nseessle@mail.hh.provi.de
 * http://ourworld.compuserve.com/homepages/nseessle/frames/pilot/dat_e.htm
 * http://www.geocities.com/Heartland/Acres/3216/todo_dat.htm
 * Scott Leighton helphand@pacbell.net
 * 
 * For their descriptions of the dat formats.
 */

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#include "log.h"
#include "utils.h"
#include "i18n.h"

#define JPILOT_DEBUGno

#define DAT_STATUS_ADD     0x01
#define DAT_STATUS_UPDATE  0x02
#define DAT_STATUS_DELETE  0x04
#define DAT_STATUS_PENDING 0x08
#define DAT_STATUS_ARCHIVE 0x80
/* I made this one up for a bit to store the private flag */
#define DAT_STATUS_PRIVATE 0x10

#define DAILY           1
#define WEEKLY          2
#define MONTHLY_BY_DAY  3
#define MONTHLY_BY_DATE 4
#define YEARLY_BY_DATE  5
#define YEARLY_BY_DAY   6


#ifdef JPILOT_DEBUG
static int print_date(int palm_date);
#endif

struct field {
   int type;
   int i;
   long date;
   char *str;
};

static int x86_short(unsigned char *str)
{
   return str[1] * 0x100 + str[0];
}

static long x86_long(unsigned char *str)
{
   return str[3]*0x1000000 + str[2]*0x0010000 + str[1]*0x0000100 + str[0];
}


/* Returns the length of the CString read */
static int get_CString(FILE *in, char **PStr)
{
   unsigned char size1;
   unsigned char size2[2];
   int size;

   fread(&size1, 1, 1, in);
   if (size1==0) {
      *PStr = NULL;
      return 0;
   }
   if (size1==0xFF) {
      fread(size2, 2, 1, in);
      size = x86_short(size2);
#ifdef JPILOT_DEBUG
      printf("BIG STRING size=%d\n", size);
#endif
   } else {
      size=size1;
   }
   /* malloc an extra byte just to be safe */
   *PStr=malloc(size+2);
   fread(*PStr, size, 1, in);
   (*PStr)[size]='\0';

   return size;
}

static int get_categories(FILE *in, struct CategoryAppInfo *ai)
{
   unsigned char str_long[4];
   char *PStr;
   long count;
   int i;

   /* Get the category count */
   fread(str_long, 4, 1, in);
   count = x86_long(str_long);

   for (i=0; i<16; i++) {
      ai->renamed[i]=0;
      ai->name[i][0]=0;
      ai->ID[i]=0;
   }
   ai->lastUniqueID=0;

   for (i=0; i<count; i++) {
      /* This is the category index */
      fread(str_long, 4, 1, in);

      /* This is the category ID */
      fread(str_long, 4, 1, in);
      ai->ID[i] = x86_long(str_long);

      /* This is the category dirty flag */
      fread(str_long, 4, 1, in);

      /* Long Category Name */
      get_CString(in, &PStr);
      strncpy(ai->name[i], PStr, 16);
      ai->name[i][15]='\0';
      free(PStr);

      /* Short Category Name */
      get_CString(in, &PStr);
      free(PStr);
   }
   return count;
}

int get_repeat(FILE *in, struct Appointment *a)
{
   time_t t;
   struct tm *now;
   unsigned char str_long[4];
   unsigned char str_short[2];
   int l, s, i, bit;
   char *PStr;
   int repeat_type;

   fread(str_short, 2, 1, in);
   s = x86_short(str_short);
#ifdef JPILOT_DEBUG
   printf("  repeat entry follows:\n");
   printf("%d exceptions\n", s);
#endif
   if (a) {
      a->exception=NULL;
      memset(&(a->repeatEnd), 0, sizeof(a->repeatEnd));
   }

   if (a) {
      a->exceptions=s;
      if (s>0) {
	 a->exception=malloc(sizeof(struct tm) * s);
	 if (!(a->exceptions)) {
	    jp_logf(JP_LOG_WARN, "get_repeat(): Out of Memory\n");
	 }
      }
   }

   l=0;
   for (i=0; i<s; i++) {
      fread(str_long, 4, 1, in);
      l = x86_long(str_long);
      if (a) {
	 t = l;
	 now = localtime(&t);
	 memcpy(&(a->exception[i]), now, sizeof(struct tm));
      }
#ifdef JPILOT_DEBUG
      printf("date_exception_entry: ");
      print_date(l);
#endif
   }

   fread(str_short, 2, 1, in);
   s = x86_short(str_short);
#ifdef JPILOT_DEBUG
   printf("0x%x repeat event flag\n", s);
#endif

   if (s==0x0000) {
      if (a) {
	 a->repeatType=repeatNone;
      }
      return 0;
   }

   if (s==0xFFFF) {
      /* We have a Class entry here */
      fread(str_short, 2, 1, in);
      s = x86_short(str_short);
#ifdef JPILOT_DEBUG
      printf("constant of 1 = %d\n", s);
#endif
      fread(str_short, 2, 1, in);
      s = x86_short(str_short);
#ifdef JPILOT_DEBUG
      printf("class name length = %d\n", s);
#endif

      PStr = malloc(s+1);
      fread(PStr, s, 1, in);
      PStr[s]='\0';
#ifdef JPILOT_DEBUG
      printf("class = [%s]\n", PStr);
#endif
      free(PStr);
   }

   fread(str_long, 4, 1, in);
   repeat_type = x86_long(str_long);
   if (a) {
      a->repeatType=repeat_type;
   }
#ifdef JPILOT_DEBUG
   printf("repeatType=%d ", repeat_type);
   switch(repeat_type) {
    case DAILY:
      printf("Daily\n");
      break;
    case WEEKLY:
      printf("Weekly\n");
      break;
    case MONTHLY_BY_DAY:
      printf("MonthlyByDay\n");
      break;
    case MONTHLY_BY_DATE:
      printf("Monthly By Date\n");
      break;
    case YEARLY_BY_DATE:
      printf("Yearly By Date\n");
      break;
    case YEARLY_BY_DAY:
      printf("Yearly By Day\n");
      break;
    default:
      printf("unknown repeat type %d\n", l);
   }
#endif

   fread(str_long, 4, 1, in);
   l = x86_long(str_long);
#ifdef JPILOT_DEBUG
   printf("Interval = %d\n", l);
#endif
   if (a) {
      a->repeatFrequency=l;
   }

   fread(str_long, 4, 1, in);
   l = x86_long(str_long);
   if (a) {
      t = l;
      now = localtime(&t);
      memcpy(&(a->repeatEnd), now, sizeof(struct tm));
   }
   if (t==0x749e77bf) {
      a->repeatForever=TRUE;
   } else {
      a->repeatForever=FALSE;
   }
#ifdef JPILOT_DEBUG
   printf("repeatEnd: 0x%x -> ", l); print_date(l);
#endif
   fread(str_long, 4, 1, in);
   l = x86_long(str_long);
   if (a) {
      a->repeatWeekstart=l;
   }
#ifdef JPILOT_DEBUG
   printf("First Day of Week = %d\n", l);
#endif

   switch(repeat_type) {
    case DAILY:
      fread(str_long, 4, 1, in);
      l = x86_long(str_long);
#ifdef JPILOT_DEBUG
      printf("Day Index = %d\n", l);
#endif
      break;
    case WEEKLY:
      fread(str_long, 4, 1, in);
      l = x86_long(str_long);
#ifdef JPILOT_DEBUG
      printf("Day Index = %d\n", l);
#endif

      fread(str_long, 1, 1, in);
      for (i=0, bit=1; i<7; i++, bit=bit<<1) {
	 a->repeatDays[i]=( str_long[0] & bit );
      }
#ifdef JPILOT_DEBUG
      printf("Days Mask = %x\n", str_long[0]);
#endif
      break;
    case MONTHLY_BY_DAY:
      fread(str_long, 4, 1, in);
      l = x86_long(str_long);
#ifdef JPILOT_DEBUG
      printf("Day Index = %d\n", l);
#endif

      fread(str_long, 4, 1, in);
      l = x86_long(str_long);
#ifdef JPILOT_DEBUG
      printf("Week Index = %d\n", l);
#endif
      break;
    case MONTHLY_BY_DATE:
      fread(str_long, 4, 1, in);
      l = x86_long(str_long);
#ifdef JPILOT_DEBUG
      printf("Day Number = %d\n", l);
#endif
      break;
    case YEARLY_BY_DATE:
      fread(str_long, 4, 1, in);
      l = x86_long(str_long);
#ifdef JPILOT_DEBUG
      printf("Day Number = %d\n", l);
#endif
      fread(str_long, 4, 1, in);
      l = x86_long(str_long);
#ifdef JPILOT_DEBUG
      printf("Month Index = %d\n", l);
#endif
      break;
    case YEARLY_BY_DAY:
      break;
    default:
#ifdef JPILOT_DEBUG
      printf("unknown repeat type2 %d\n", l);
#endif
      break;
   }

   return 0;
}


#ifdef JPILOT_DEBUG
static int print_date(int palm_date)
{
   time_t t;
   struct tm *now;
   char text[256];

   t = palm_date;/* - 20828448800; */
   now = localtime(&t);
   strftime(text, sizeof(text), "%02m/%02d/%Y %02H:%02M:%02S", now);
   printf("%s\n", text);

   return 0;
}
#endif

#ifdef JPILOT_DEBUG
int print_field(struct field *f)
{

   switch(f->type) {
    case DAT_TYPE_INTEGER:
      printf("%d\n", f->i);
      break;
    case DAT_TYPE_CSTRING:
      printf("%s\n", f->str);
      break;
    case DAT_TYPE_BOOLEAN:
      if (f->i) {
	 printf("True\n");
      } else {
	 printf("False\n");
      }
      break;
    case DAT_TYPE_DATE:
      print_date(f->date);
      break;
    case DAT_TYPE_REPEAT:
      printf("Repeat Type\n");
      break;
    default:
      printf("print_field: unknown type = %d\n", f->type);
      break;
   }

   return 0;
}
#endif

int get_field(FILE *in, struct field *f)
{
   unsigned char str_long[4];
   long type;
   char *PStr;

   fread(str_long, 4, 1, in);
   type = x86_long(str_long);
   f->type=0;
   f->str=NULL;

   switch(type) {
    case DAT_TYPE_INTEGER:
      f->type=type;
      fread(str_long, 4, 1, in);
      f->i = x86_long(str_long);
      break;
    case DAT_TYPE_CSTRING:
      f->type=type;
      /* padding */
      fseek(in, 4, SEEK_CUR);
      get_CString(in, &PStr);
      f->str = PStr;
      break;
    case DAT_TYPE_BOOLEAN:
      f->type=type;
      fread(str_long, 4, 1, in);
      f->i = x86_long(str_long);
      break;
    case DAT_TYPE_DATE:
      f->type=type;
      fread(str_long, 4, 1, in);
      f->date = x86_long(str_long);
      break;
    case DAT_TYPE_REPEAT:
      f->type=type;
      /* The calling function needs to call this */
      /* get_repeat(in, NULL); */
      break;
    default:
      jp_logf(JP_LOG_WARN, "get_field(): unknown type = %ld\n", type);
      break;
   }

   return 0;
}

int dat_check_if_dat_file(FILE *in)
{
   char version[6];

   memset(version, 0, sizeof(version));
   fseek(in, 0, SEEK_SET);
   /* Version */
   fread(version, 4, 1, in);
   fseek(in, 0, SEEK_SET);
   jp_logf(JP_LOG_DEBUG, "dat_check_if_dat_file(): version = [%c%c%d%d]\n", version[3],version[2],version[1],version[0]);
   if ((version[3]=='D') && (version[2]=='B') &&
       (version[1]==1) && (version[0]==0)) {
      return DAT_DATEBOOK_FILE;
   }
   if ((version[3]=='A') && (version[2]=='B') &&
       (version[1]==1) && (version[0]==0)) {
      return DAT_ADDRESS_FILE;
   }
   if ((version[3]=='T') && (version[2]=='D') &&
       (version[1]==1) && (version[0]==0)) {
      return DAT_TODO_FILE;
   }
   if ((version[3]=='M') && (version[2]=='P') &&
       (version[1]==1) && (version[0]==0)) {
      return DAT_MEMO_FILE;
   }
   return 0;
}

int dat_read_header(FILE *in,
		    int expected_field_count,
		    char *schema,
		    struct CategoryAppInfo *ai,
		    int *schema_count, int *field_count, long *rec_count)
{
   int i;
   unsigned char filler[100];
   char version[4];
   char *PStr;
   unsigned char str_long[4];

   fseek(in, 0, SEEK_SET);

   /* Version */
   fread(version, 4, 1, in);
   jp_logf(JP_LOG_DEBUG, "version = [%c%c%d%d]\n", version[3],version[2],version[1],version[0]);

   /* Full file path name */
   get_CString(in, &PStr);
   jp_logf(JP_LOG_DEBUG, "path:[%s]\n",PStr);
   free(PStr);

   /* Show Header */
   get_CString(in, &PStr);
   jp_logf(JP_LOG_DEBUG, "show header:[%s]\n",PStr);
   free(PStr);

   /* Next free category ID */
   fread(filler, 4, 1, in);

   /* Categories */
   get_categories(in, ai);
#ifdef JPILOT_DEBUG
   for (i=0; i<16; i++) {
      printf("%d [%s]\n", ai->ID[i], ai->name[i]);
   }
#endif

   /* Schema resource ID */
   fread(filler, 4, 1, in);
   /* Schema fields per row */
   fread(filler, 4, 1, in);
   *field_count=x86_long(filler);
   if (*field_count != expected_field_count) {
      jp_logf(JP_LOG_WARN, "fields per row count != %d, unknown format\n",
		  expected_field_count);
      return -1;
   }
   /* Schema record ID position */
   fread(filler, 4, 1, in);
   /* Schema record status position */
   fread(filler, 4, 1, in);
   /* Schema placement position */
   fread(filler, 4, 1, in);
   /* Schema fields count */
   fread(filler, 2, 1, in);
   *field_count = x86_short(filler);
   if (*field_count != expected_field_count) {
      jp_logf(JP_LOG_WARN, "field count != %d, unknown format\n",
		  expected_field_count);
      return -1;
   }

   /* Schema fields */
   fread(filler, (*field_count)*2, 1, in);
   if (memcmp(filler, schema, (*field_count)*2)) {
      jp_logf(JP_LOG_WARN, "unknown format, file has wrong schema\n");
      jp_logf(JP_LOG_WARN, "File schema is:");
      for (i=0; i<(*field_count)*2; i++) {
	 jp_logf(JP_LOG_WARN, " %02d", (char)filler[i]);
      }
      jp_logf(JP_LOG_WARN, "\n");
      jp_logf(JP_LOG_WARN, "It should be:  ");
      for (i=0; i<(*field_count)*2; i++) {
	 jp_logf(JP_LOG_WARN, " %02d", (char)schema[i]);
      }
      jp_logf(JP_LOG_WARN, "\n");
      return -1;
   }

   /* Get record count */
   fread(str_long, 4, 1, in);
   if ((*field_count)) {
      *rec_count = x86_long(str_long) / (*field_count);
   }
#ifdef JPILOT_DEBUG
   printf("Record Count = %ld\n", *rec_count);
#endif
   return 0;
}


int dat_get_appointments(FILE *in, AppointmentList **alist, struct CategoryAppInfo *ai)
{
#ifdef JPILOT_DEBUG
   struct field hack_f;
#endif
   int ret, i, j;
   struct field fa[28];
   int schema_count, field_count;
   long rec_count;
   AppointmentList *temp_alist;
   AppointmentList *last_alist;
   time_t t;
   struct tm *now;
   /* Should be 1, 1, 1, 3, 3, 5, 1, 5, 6, 6, 1, 6, 1, 1, 8 */
   char schema[30]={
      DAT_TYPE_INTEGER,0,
	DAT_TYPE_INTEGER,0,
	DAT_TYPE_INTEGER,0,
	DAT_TYPE_DATE,0,
	DAT_TYPE_INTEGER,0,
	DAT_TYPE_CSTRING,0,
	DAT_TYPE_INTEGER,0,
	DAT_TYPE_CSTRING,0,
	DAT_TYPE_BOOLEAN,0,
	DAT_TYPE_BOOLEAN,0,
	DAT_TYPE_INTEGER,0,
	DAT_TYPE_BOOLEAN,0,
	DAT_TYPE_INTEGER,0,
	DAT_TYPE_INTEGER,0,
	DAT_TYPE_REPEAT,0
   };
#ifdef JPILOT_DEBUG
   char *rec_fields[]={
      "Record ID",
	"Status field",
	"Position"
   };
   char *field_names[]={
      "Start Time",
	"End Time",
	"Description",
	"Duration",
	"Note",
	"Untimed",
	"Private",
	"Category",
	"Alarm Set",
	"Alarm Advance Units",
	"Alarm Advance Type",
	"Repeat Event"
   };
#endif

   jp_logf(JP_LOG_DEBUG, "dat_get_appointments\n");

   if (!alist) return 0;
   *alist=NULL;

   ret = dat_read_header(in, 15, schema, ai,
			 &schema_count, &field_count, &rec_count);

   if (ret<0) return ret;

   /* Get records */
   last_alist=*alist;
#ifdef JPILOT_DEBUG
   printf("---------- Records ----------\n");
#endif
   for (i=0; i<rec_count; i++) {
      temp_alist = malloc(sizeof(AppointmentList));
      if (!temp_alist) {
	 jp_logf(JP_LOG_WARN, "dat_get_appointments(): Out of memory\n");
	 return i;
      }
#ifdef JPILOT_DEBUG
      printf("----- record %d -----\n", i+1);
#endif
      memset(&(temp_alist->ma.a), 0, sizeof(temp_alist->ma.a));
      temp_alist->next=NULL;
      temp_alist->app_type=DATEBOOK;

      /* Record ID */
      /* Status Field */
      /* Position */
      for (j=0; j<3; j++) {
	 get_field(in, &(fa[j]));
#ifdef JPILOT_DEBUG
	 printf("rec field %d %s: ", j, rec_fields[j]); print_field(&(fa[j]));
#endif
	 if (fa[j].type!=schema[j*2]) {
	    jp_logf(JP_LOG_WARN, "%s:%d Invalid schema ID (%d) record %d, field %d\n", __FILE__, __LINE__, fa[j].type, i+1, j+3);
	    jp_logf(JP_LOG_WARN, _("read of file terminated\n"));
	    free(temp_alist);
	    return 0;
	 }
      }
      /* Get Fields */
      for (j=0; j<12; j++) {
	 get_field(in, &(fa[j]));
#ifdef JPILOT_DEBUG
	 printf("field %d %s: ", j, field_names[j]); print_field(&(fa[j]));
	 if (j==1) {
	    hack_f.type=DAT_TYPE_DATE;
	    hack_f.date=fa[j].i;
	    printf("        "); print_field(&hack_f);
	 }
#endif
	 if (fa[j].type!=schema[j*2+6]) {
	    jp_logf(JP_LOG_WARN, "%s:%d Invalid schema ID (%d) record %d, field %d\n", __FILE__, __LINE__, fa[j].type, i+1, j+3);
	    jp_logf(JP_LOG_WARN, _("read of file terminated\n"));
	    free(temp_alist);
	    return 0;
	 }
	 if (fa[j].type==DAT_TYPE_REPEAT) {
	    get_repeat(in, &(temp_alist->ma.a));
	 }
      }
      /* Start Time */
      t = fa[0].date;
      now = localtime(&t);
      memcpy(&(temp_alist->ma.a.begin), now, sizeof(struct tm));
      /* End Time */
      t = fa[1].i;
      now = localtime(&t);
      memcpy(&(temp_alist->ma.a.end), now, sizeof(struct tm));
      /* Description */
      if (fa[2].str) {
	 temp_alist->ma.a.description=fa[2].str;
      } else {
	 temp_alist->ma.a.description=strdup("");
      }
      /* Duration */
      /* what is duration? (repeatForever?) */
      /* Note */
      if (fa[4].str) {
	 temp_alist->ma.a.note=fa[4].str;
      } else {
	 temp_alist->ma.a.note=strdup("");
      }
      /* Untimed */
      temp_alist->ma.a.event=fa[5].i;
      /* Private */
      temp_alist->ma.attrib = 0;
      if (fa[6].i) {
	 temp_alist->ma.attrib |= DAT_STATUS_PRIVATE;
      }
      /* Category */
      temp_alist->ma.unique_id = fa[7].i;
      if (temp_alist->ma.unique_id > 15) {
	 temp_alist->ma.unique_id = 15;
      }
      if (temp_alist->ma.unique_id < 0) {
	 temp_alist->ma.unique_id = 0;
      }	 
      /* Alarm Set */
      temp_alist->ma.a.alarm=fa[8].i;
      /* Alarm Advance Units */
      temp_alist->ma.a.advance=fa[9].i;
      /* Alarm Advance Type */
      temp_alist->ma.a.advanceUnits=fa[10].i;

      /* Append onto the end of the list */
      if (last_alist) {
	 last_alist->next=temp_alist;
	 last_alist=temp_alist;
      } else {
	 last_alist=temp_alist;
	 *alist=last_alist;
      }
   }
   return 0;
}

int dat_get_addresses(FILE *in, AddressList **addrlist, struct CategoryAppInfo *ai)
{
   int ret, i, j, k;
   struct field fa[28];
   int schema_count, field_count;
   long rec_count;
   AddressList *temp_addrlist;
   AddressList *last_addrlist;
   int dat_order[19]={
      0,1,3,5,7,9,11,13,14,15,16,17,18,2,22,23,24,25,19
   };
   /* Should be 1, 1, 1, 5, 5, 5, 5, 1, 5, 1, 5, 1, 5, 1, 5, 1, 5,
      5, 5, 5, 5, 5, 5, 6, 1, 5, 5, 5, 5, 1 */
   char schema[60]={
      DAT_TYPE_INTEGER,0,
      DAT_TYPE_INTEGER,0,
      DAT_TYPE_INTEGER,0,
      DAT_TYPE_CSTRING,0,
      DAT_TYPE_CSTRING,0,
      DAT_TYPE_CSTRING,0,
      DAT_TYPE_CSTRING,0,
      DAT_TYPE_INTEGER,0,
      DAT_TYPE_CSTRING,0,
      DAT_TYPE_INTEGER,0,
      DAT_TYPE_CSTRING,0,
      DAT_TYPE_INTEGER,0,
      DAT_TYPE_CSTRING,0,
      DAT_TYPE_INTEGER,0,
      DAT_TYPE_CSTRING,0,
      DAT_TYPE_INTEGER,0,
      DAT_TYPE_CSTRING,0,
      DAT_TYPE_CSTRING,0,
      DAT_TYPE_CSTRING,0,
      DAT_TYPE_CSTRING,0,
      DAT_TYPE_CSTRING,0,
      DAT_TYPE_CSTRING,0,
      DAT_TYPE_CSTRING,0,
      DAT_TYPE_BOOLEAN,0,
      DAT_TYPE_INTEGER,0,
      DAT_TYPE_CSTRING,0,
      DAT_TYPE_CSTRING,0,
      DAT_TYPE_CSTRING,0,
      DAT_TYPE_CSTRING,0,
      DAT_TYPE_INTEGER,0
   };
#ifdef JPILOT_DEBUG
   char *rec_fields[]={
      "Record ID",
	"Status field",
	"Position"
   };
   char *field_names[]={
      "Last Name",
	"First Name",
	"Title",
	"Company",
	"Phone1 Label",
	"Phone1",
	"Phone2 Label",
	"Phone2",
	"Phone3 Label",
	"Phone3",
	"Phone4 Label",
	"Phone4",
	"Phone5 Label",
	"Phone5",
	"Address",
	"City",
	"State",
	"Zip",
	"Country",
	"Note",
	"Private",
	"Category",
	"Custom1",
	"Custom2",
	"Custom3",
	"Custom4",
	"Display Phone"
   };
#endif

   jp_logf(JP_LOG_DEBUG, "dat_get_addresses\n");

   if (!addrlist) return 0;
   *addrlist=NULL;

   ret = dat_read_header(in, 30, schema, ai,
			 &schema_count, &field_count, &rec_count);

   if (ret<0) return ret;

   /* Get records */
   last_addrlist=*addrlist;
#ifdef JPILOT_DEBUG
   printf("---------- Records ----------\n");
#endif
   for (i=0; i<rec_count; i++) {
      temp_addrlist = malloc(sizeof(AddressList));
      if (!temp_addrlist) {
	 jp_logf(JP_LOG_WARN, "dat_get_addresses(): Out of memory\n");
	 return i;
      }
      temp_addrlist->next=NULL;
      temp_addrlist->app_type=ADDRESS;
#ifdef JPILOT_DEBUG
      printf("----- record %d -----\n", i+1);
#endif
      /* Record ID */
      /* Status Field */
      /* Position */
      for (j=0; j<3; j++) {
	 get_field(in, &(fa[j]));
#ifdef JPILOT_DEBUG
	 printf("rec field %d %s: ", j, rec_fields[j]); print_field(&(fa[j]));
#endif
	 if (fa[j].type!=schema[j*2]) {
	    jp_logf(JP_LOG_WARN, "%s:%d Invalid schema ID (%d) record %d, field %d\n", __FILE__, __LINE__, fa[j].type, i+1, j+3);
	    jp_logf(JP_LOG_WARN, _("read of file terminated\n"));
	    free(temp_addrlist);
	    return 0;
	 }
      }
      /* Get Fields */
      for (j=0; j<27; j++) {
	 get_field(in, &(fa[j]));
#ifdef JPILOT_DEBUG
	 printf("field %d %s: ", j, field_names[j]); print_field(&(fa[j]));
#endif
	 if (fa[j].type!=schema[j*2+6]) {
	    jp_logf(JP_LOG_WARN, "%s:%d Invalid schema ID (%d) record %d, field %d\n", __FILE__, __LINE__, fa[j].type, i+1, j+3);
	    jp_logf(JP_LOG_WARN, _("read of file terminated\n"));
	    free(temp_addrlist);
	    return 0;
	 }
      }
      for (k=0; k<19; k++) {
	 temp_addrlist->ma.a.entry[k]=fa[dat_order[k]].str;
      }
      temp_addrlist->ma.a.phoneLabel[0] = fa[4].i;
      temp_addrlist->ma.a.phoneLabel[1] = fa[6].i;
      temp_addrlist->ma.a.phoneLabel[2] = fa[8].i;
      temp_addrlist->ma.a.phoneLabel[3] = fa[10].i;
      temp_addrlist->ma.a.phoneLabel[4] = fa[12].i;
      /* Private */
      temp_addrlist->ma.attrib = 0;
      if (fa[20].i) {
	 temp_addrlist->ma.attrib |= DAT_STATUS_PRIVATE;
      }
      /* Category */
      temp_addrlist->ma.unique_id = fa[21].i;
      if (temp_addrlist->ma.unique_id > 15) {
	 temp_addrlist->ma.unique_id = 15;
      }
      if (temp_addrlist->ma.unique_id < 0) {
	 temp_addrlist->ma.unique_id = 0;
      }	 
      /* Show phone in list */
      temp_addrlist->ma.a.showPhone = fa[26].i - 1;
      for (k=0; k<19; k++) {
	 if (temp_addrlist->ma.a.entry[k]==NULL) {
	    temp_addrlist->ma.a.entry[k]=strdup("");
	 }
      }
      /* Append onto the end of the list */
      if (temp_addrlist) {
	 last_addrlist->next=temp_addrlist;
	 last_addrlist=temp_addrlist;
      } else {
	 last_addrlist=temp_addrlist;
	 *addrlist=last_addrlist;
      }
   }
   return 0;
}

int dat_get_todos(FILE *in, ToDoList **todolist, struct CategoryAppInfo *ai)
{
   int ret, i, j;
   struct field fa[10];
   int schema_count, field_count;
   long rec_count;
   time_t t;
   struct tm *now;
   ToDoList *temp_todolist;
   ToDoList *last_todolist;
   /* Should be 1, 1, 1, 5, 3, 6, 1, 6, 1, 5 */
   char schema[20]={
      DAT_TYPE_INTEGER,0,
      DAT_TYPE_INTEGER,0,
      DAT_TYPE_INTEGER,0,
      DAT_TYPE_CSTRING,0,
      DAT_TYPE_DATE,0,
      DAT_TYPE_BOOLEAN,0,
      DAT_TYPE_INTEGER,0,
      DAT_TYPE_BOOLEAN,0,
      DAT_TYPE_INTEGER,0,
      DAT_TYPE_CSTRING,0
   };
#ifdef JPILOT_DEBUG
   char *rec_fields[]={
      "Record ID",
	"Status field",
	"Position"
   };
   char *field_names[]={
      "Description",
	"Due Date",
	"Completed",
	"Priority",
	"Private",
	"Category",
	"Note"
   };
#endif

   jp_logf(JP_LOG_DEBUG, "dat_get_todos\n");

   if (!todolist) return 0;
   *todolist=NULL;

   ret = dat_read_header(in, 10, schema, ai,
			 &schema_count, &field_count, &rec_count);

   if (ret<0) return ret;

   /* Get records */
   last_todolist=*todolist;
#ifdef JPILOT_DEBUG
   printf("---------- Records ----------\n");
#endif
   for (i=0; i<rec_count; i++) {
      temp_todolist = malloc(sizeof(ToDoList));
      if (!temp_todolist) {
	 jp_logf(JP_LOG_WARN, "dat_get_todos(): Out of memory\n");
	 jp_logf(JP_LOG_WARN, _("read of file terminated\n"));
	 free(temp_todolist);
	 return i;
      }
      temp_todolist->next=NULL;
      temp_todolist->app_type=TODO;
#ifdef JPILOT_DEBUG
      printf("----- record %d -----\n", i+1);
#endif
      /* Record ID */
      /* Status Field */
      /* Position */
      for (j=0; j<3; j++) {
	 get_field(in, &(fa[j]));
#ifdef JPILOT_DEBUG
	 printf("rec field %d %s: ", j, rec_fields[j]); print_field(&(fa[j]));
#endif
	 if (fa[j].type!=schema[j*2]) {
	    jp_logf(JP_LOG_WARN, "%s:%d Invalid schema ID (%d) record %d, field %d\n", __FILE__, __LINE__, fa[j].type, i+1, j+3);
	    jp_logf(JP_LOG_WARN, _("read of file terminated\n"));
	    free(temp_todolist);
	    return 0;
	 }
      }
      /* Get Fields */
      for (j=0; j<7; j++) {
	 get_field(in, &(fa[j]));
#ifdef JPILOT_DEBUG
	 printf("field %d %s: ", j, field_names[j]); print_field(&(fa[j]));
#endif
	 if (fa[j].type!=schema[j*2+6]) {
	    jp_logf(JP_LOG_WARN, "%s:%d Invalid schema ID (%d) record %d, field %d\n", __FILE__, __LINE__, fa[j].type, i+1, j+3);
	    jp_logf(JP_LOG_WARN, _("read of file terminated\n"));
	    free(temp_todolist);
	    return 0;
	 }
      }
      /* Description */
      if (fa[0].str) {
	 temp_todolist->mtodo.todo.description=fa[0].str;
      } else {
	 temp_todolist->mtodo.todo.description=strdup("");
      }
      /* Due Date */
      if (fa[1].date==0x749E77BF) {
	 temp_todolist->mtodo.todo.indefinite=1;
	 memset(&(temp_todolist->mtodo.todo.due), 0, sizeof(temp_todolist->mtodo.todo.due));
      } else {
	 t = fa[1].date;
	 now = localtime(&t);
	 memcpy(&(temp_todolist->mtodo.todo.due), now, sizeof(struct tm));
	 temp_todolist->mtodo.todo.indefinite=0;
      }
      /* Completed */
      temp_todolist->mtodo.todo.complete = (fa[2].i==0) ? 0 : 1;
      /* Priority */
      if (fa[3].i < 0) fa[3].i=0;
      if (fa[3].i > 5) fa[3].i=5;
      temp_todolist->mtodo.todo.priority = fa[3].i;
      /* Private */
      temp_todolist->mtodo.attrib = 0;
      if (fa[4].i) {
	 temp_todolist->mtodo.attrib |= DAT_STATUS_PRIVATE;
      }
      /* Category */
      /* Normally the category would go into 4 bits of the attrib.
       * They jumbled the attrib bits, so to make things easy I'll put it
       * here.
       */
      if (fa[5].i < 0) fa[5].i=0;
      if (fa[5].i > 15) fa[5].i=15;
      temp_todolist->mtodo.unique_id = fa[5].i;
      /* Note */
      if (fa[6].str) {
	 temp_todolist->mtodo.todo.note=fa[6].str;
      } else {
	 temp_todolist->mtodo.todo.note=strdup("");
      }
      /* Append onto the end of the list */
      if (last_todolist) {
	 last_todolist->next=temp_todolist;
	 last_todolist=temp_todolist;
      } else {
	 last_todolist=temp_todolist;
	 *todolist=last_todolist;
      }
   }

   return 0;
}

int dat_get_memos(FILE *in, MemoList **memolist, struct CategoryAppInfo *ai)
{
   int ret, i, j;
   struct field fa[10];
   int schema_count, field_count;
   long rec_count;
   MemoList *temp_memolist;
   MemoList *last_memolist;
   char schema[12]={
      DAT_TYPE_INTEGER,0,
      DAT_TYPE_INTEGER,0,
      DAT_TYPE_INTEGER,0,
      DAT_TYPE_CSTRING,0,
      DAT_TYPE_BOOLEAN,0,
      DAT_TYPE_INTEGER,0
   };
#ifdef JPILOT_DEBUG
   char *rec_fields[]={
      "Record ID",
	"Status field",
	"Position"
   };
   char *field_names[]={
      "Memo",
	"Private",
	"Category"
   };
#endif

   jp_logf(JP_LOG_DEBUG, "dat_get_memos\n");

   if (!memolist) return 0;
   *memolist=NULL;

   ret = dat_read_header(in, 6, schema, ai,
			 &schema_count, &field_count, &rec_count);

   if (ret<0) return ret;

   /* Get records */
   last_memolist=*memolist;
#ifdef JPILOT_DEBUG
   printf("---------- Records ----------\n");
#endif
   for (i=0; i<rec_count; i++) {
      temp_memolist = malloc(sizeof(MemoList));
      if (!temp_memolist) {
	 jp_logf(JP_LOG_WARN, "dat_get_memos(): Out of memory\n");
	 return i;
      }
      temp_memolist->next=NULL;
      temp_memolist->app_type=MEMO;
#ifdef JPILOT_DEBUG
      printf("----- record %d -----\n", i+1);
#endif
      /* Record ID */
      /* Status Field */
      /* Position */
      for (j=0; j<3; j++) {
	 get_field(in, &(fa[j]));
#ifdef JPILOT_DEBUG
	 printf("rec field %d %s: ", j, rec_fields[j]); print_field(&(fa[j]));
#endif
	 if (fa[j].type!=schema[j*2]) {
	    jp_logf(JP_LOG_WARN, "%s:%d Invalid schema ID (%d) record %d, field %d\n", __FILE__, __LINE__, fa[j].type, i+1, j+3);
	    jp_logf(JP_LOG_WARN, _("read of file terminated\n"));
	    free(temp_memolist);
	    return 0;
	 }
      }
      /* Get Fields */
      for (j=0; j<3; j++) {
	 get_field(in, &(fa[j]));
#ifdef JPILOT_DEBUG
	 printf("field %d %s: ", j, field_names[j]); print_field(&(fa[j]));
#endif
	 if (fa[j].type!=schema[j*2+6]) {
	    jp_logf(JP_LOG_WARN, "%s:%d Invalid schema ID (%d) record %d, field %d\n", __FILE__, __LINE__, fa[j].type, i+1, j+3);
	    jp_logf(JP_LOG_WARN, _("read of file terminated\n"));
	    free(temp_memolist);
	    return 0;
	 }
      }
      /* Memo */
      temp_memolist->mmemo.memo.text=fa[0].str;
      /* Private */
      temp_memolist->mmemo.attrib = 0;
      if (fa[1].i) {
	 temp_memolist->mmemo.attrib |= DAT_STATUS_PRIVATE;
      }
      /* Category */
      /* Normally the category would go into 4 bits of the attrib.
       * They jumbled the attrib bits, so to make things easy I'll put it
       * here.
       */
      if (fa[2].i < 0) fa[2].i=0;
      if (fa[2].i > 15) fa[2].i=15;
      temp_memolist->mmemo.unique_id = fa[2].i;
      /* Append onto the end of the list */

      if (last_memolist) {
	 last_memolist->next=temp_memolist;
	 last_memolist=temp_memolist;
      } else {
	 last_memolist=temp_memolist;
	 *memolist=last_memolist;
      }
   }

   return 0;
}

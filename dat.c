/* dat.c
 * A module of J-Pilot http://jpilot.org
 * 
 * Copyright (C) 2001 by Judd Montgomery
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
   char str_long[4];
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
   
static int get_custom_field(FILE *in, char *field)
{
   int i;
   int c;
   
   for (i=0; i<16; i++) {
      c=fgetc(in);
      if (c==0x0A) {
	 field[i]='\0';
	 break;
      } else {
	 field[i]=c;
      }
   }
   field[15]='\0';

   return 0;
}

int get_custom_names(FILE *in)
{
   char custom[256];
   
   /* I'm not sure what this character is for */
   fread(custom, 1, 1, in);

   get_custom_field(in, custom);
#ifdef JPILOT_DEBUG
   printf("custom: [%s]\n", custom);
#endif
   get_custom_field(in, custom);
#ifdef JPILOT_DEBUG
   printf("custom: [%s]\n", custom);
#endif
   get_custom_field(in, custom);
#ifdef JPILOT_DEBUG
   printf("custom: [%s]\n", custom);
#endif
   get_custom_field(in, custom);
#ifdef JPILOT_DEBUG
   printf("custom: [%s]\n", custom);
#endif
   return 0;
}

int get_repeat(FILE *in)
{
   unsigned char str_long[4];
   unsigned char str_short[2];
   int l, s, i;
   char *PStr;
   int repeat_type;
   
   fread(str_short, 2, 1, in);
   s = x86_short(str_short);
#ifdef JPILOT_DEBUG
   printf("  repeat entry follows:\n");
   printf("%d exceptions\n", s);
#endif

   l=0;
   for (i=0; i<s; i++) {
      fread(str_long, 4, 1, in);
      l = x86_long(str_long);
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
#ifdef JPILOT_DEBUG
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

   fread(str_long, 4, 1, in);
   l = x86_long(str_long);
#ifdef JPILOT_DEBUG
   print_date(l);
#endif
   fread(str_long, 4, 1, in);
   l = x86_long(str_long);
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
#ifdef JPILOT_DEBUG
      printf("Days Mask = %d\n", str_long[0]);
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
   strftime(text, 255, "%02m/%02d/%Y %02H:%02M:%02S", now);
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
   char str_long[4];
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
      get_repeat(in);
      break;
    default:
      jpilot_logf(LOG_WARN, "get_field(): unknown type = %ld\n", type);
      break;
   }
   
   return 0;
}

int get_records(FILE *in, int field_count)
{
   long count;
   char str_long[4];
   int i, j;
   struct field f;
   
   /* We skiped some steps like reading in the field count, which should be 30 */
   /* Get the record count, which is field count / fields/rec */
   fread(str_long, 4, 1, in);
   count = x86_long(str_long) / field_count;
#ifdef JPILOT_DEBUG
   printf("Record Count = %ld\n", count);   
   printf("---------- Records ----------\n");
#endif
   for (i=0; i<count; i++) {
#ifdef JPILOT_DEBUG
      printf("----- record %d -----\n", i+1);
#endif
      for (j=0; j<field_count; j++) {
	 get_field(in, &f);
#ifdef JPILOT_DEBUG
	 print_field(&f);
#endif
	 if (f.type==DAT_TYPE_CSTRING) {
	    if (f.str) free(f.str);
	 }
#ifdef JPILOT_DEBUG
	 printf("\n");
#endif
      }
   }

   return 0;
}
   
int dump_datebook()
{
   FILE *in;
   char filler[100];
   char version[4];
   char *PStr;
   short num_fields;
   struct CategoryAppInfo ai;
   
   in=fopen("datebook.dat", "r");

   fread(version, 4, 1, in);
#ifdef JPILOT_DEBUG
   printf("version = [%c%c%d%d]\n", version[3],version[2],version[1],version[0]);
#endif

   /* Get the full file path name */
   get_CString(in, &PStr);
#ifdef JPILOT_DEBUG
   printf("PStr = [%s]\n", PStr);
#endif
   free(PStr);
   
   /* Show Header */
   get_CString(in, &PStr);
   free(PStr);

   /* Next free category ID */
   fread(filler, 4, 1, in);

   get_categories(in, &ai);

   /* Schema resource ID */
   fread(filler, 4, 1, in);

   /* Schema fields per row (15) */
   fread(filler, 4, 1, in);

   /* Schema record ID position */
   fread(filler, 4, 1, in);

   /* Schema record status position */
   fread(filler, 4, 1, in);

   /* Schema placement position */
   fread(filler, 4, 1, in);

   /* Schema field count */
   fread(filler, 2, 1, in);
   num_fields = x86_short(filler);
#ifdef JPILOT_DEBUG
   printf("num_fields=%d\n", num_fields);
#endif
   if (num_fields>30) num_fields=15;
   fread(filler, num_fields*2, 1, in);

   get_records(in, num_fields);
   
   fclose(in);

   return 0;
}

int dump_address()
{
   FILE *in;
   char filler[100];
   char version[4];
   char *PStr;
   long num_fields;
   struct CategoryAppInfo ai;
   
   in=fopen("address.dat", "r");
   
   /* Version */
   fread(version, 4, 1, in);
#ifdef JPILOT_DEBUG
   printf("version = [%c%c%d%d]\n", version[3],version[2],version[1],version[0]);
#endif
   /* Get the full file path name */
   get_CString(in, &PStr);
#ifdef JPILOT_DEBUG
   printf("PStr = [%s]\n", PStr);
#endif
   free(PStr);
   
   /* Custom Names and Show Header*/
   get_CString(in, &PStr);
#ifdef JPILOT_DEBUG
   printf("Custom Names and Show header\n");
   printf("PStr = [%s]\n", PStr);
#endif
   free(PStr);

   /* Next free category ID */
   fread(filler, 4, 1, in);

   get_categories(in, &ai);

   /* Schema resource ID */
   fread(filler, 4, 1, in);

   /* Schema fields per row (30) */
   fread(filler, 4, 1, in);

   /* Schema record ID position */
   fread(filler, 4, 1, in);

   /* Schema record status position */
   fread(filler, 4, 1, in);

   /* Schema placement position */
   fread(filler, 4, 1, in);

   /* Schema fields count (30) */
   fread(filler, 2, 1, in);
   num_fields = x86_short(filler);
   if (num_fields>40) num_fields=30;
   fread(filler, num_fields*2, 1, in);

#ifdef JPILOT_DEBUG
   printf("get_records\n");
#endif
   get_records(in, num_fields);

   fclose(in);

   return 0;
}

int dump_memo()
{
   FILE *in;
   char filler[100];
   char version[4];
   char *PStr;
   long num_fields;
   struct CategoryAppInfo ai;
   
   in=fopen("memopad.dat", "r");
   
   /* Version */
   fread(version, 4, 1, in);
#ifdef JPILOT_DEBUG
   printf("version = [%c%c%d%d]\n", version[3],version[2],version[1],version[0]);
#endif
   /* Get the full file path name */
   get_CString(in, &PStr);
#ifdef JPILOT_DEBUG
   printf("PStr = [%s]\n", PStr);
#endif
   free(PStr);
   
   /* Show Header */
   get_CString(in, &PStr);
   free(PStr);
   
   /* Next free category ID */
   fread(filler, 4, 1, in);

   get_categories(in, &ai);

   /* Schema resource ID */
   fread(filler, 4, 1, in);
   /* Schema fields per row (6) */
   fread(filler, 4, 1, in);
   /* Schema record ID position */
   fread(filler, 4, 1, in);
   /* Schema record status position */
   fread(filler, 4, 1, in);
   /* Schema placement position */
   fread(filler, 4, 1, in);
   /* Schema fields count (6) */
   fread(filler, 2, 1, in);
   num_fields = x86_short(filler);

   if (num_fields>20) num_fields=6;
   /* Schema fields */
   fread(filler, num_fields*2, 1, in);

   get_records(in, num_fields);

   fclose(in);

   return 0;
}

int dump_todo()
{
   FILE *in;
   char filler[100];
   char version[4];
   char *PStr;
   int num_fields;
   struct CategoryAppInfo ai;
   
   in=fopen("todo.dat", "r");
   
   /* Version */
   fread(version, 4, 1, in);
#ifdef JPILOT_DEBUG
   printf("version = [%c%c%d%d]\n", version[3],version[2],version[1],version[0]);
#endif
   /* Get the full file path name */
   get_CString(in, &PStr);
#ifdef JPILOT_DEBUG
   printf("PStr = [%s]\n", PStr);
#endif
   free(PStr);
   
   /* Show Header */
   get_CString(in, &PStr);
#ifdef JPILOT_DEBUG
   printf("Show Header\n");
   printf("PStr = [%s]\n", PStr);
#endif
   free(PStr);
   
   /* Next free category ID */
   fread(filler, 4, 1, in);

   get_categories(in, &ai);

   /* Schema resource ID */
   fread(filler, 4, 1, in);
   /* Schema fields per row (6) */
   fread(filler, 4, 1, in);
   /* Schema record ID position */
   fread(filler, 4, 1, in);
   /* Schema record status position */
   fread(filler, 4, 1, in);
   /* Schema placement position */
   fread(filler, 4, 1, in);
   /* Schema fields count (6) */
   fread(filler, 2, 1, in);
   num_fields = x86_short(filler);
#ifdef JPILOT_DEBUG
   printf("schema count=%d\n", num_fields);
#endif
   if (num_fields>20) num_fields=6;
   /* Schema fields */
   fread(filler, num_fields*2, 1, in);

   get_records(in, num_fields);

   fclose(in);

   return 0;
}

int dat_check_if_dat_file(FILE *in)
{
   char version[6];

   bzero(version, 4);
   fseek(in, 0, SEEK_SET);
   /* Version */
   fread(version, 4, 1, in);
   fseek(in, 0, SEEK_SET);
   jpilot_logf(LOG_DEBUG, "dat_check_if_dat_file(): version = [%c%c%d%d]\n", version[3],version[2],version[1],version[0]);
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

int dat_get_memos(FILE *in, MemoList **memolist, struct CategoryAppInfo *ai)
{
   char filler[100];
   char version[4];
   char *PStr;
   long num_fields;
   long count;
   char str_long[4];
   int i;
   struct field f;
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
   
   jpilot_logf(LOG_DEBUG, "dat_get_memos\n");

   if (!memolist) return 0;
   *memolist=NULL;

   fseek(in, 0, SEEK_SET);
   
   /* Version */
   fread(version, 4, 1, in);
   jpilot_logf(LOG_DEBUG, "version = [%c%c%d%d]\n", version[3],version[2],version[1],version[0]);

   /* Full file path name */
   get_CString(in, &PStr);
   jpilot_logf(LOG_DEBUG, "path:[%s]\n",PStr);
   free(PStr);
   
   /* Show Header */
   get_CString(in, &PStr);
   jpilot_logf(LOG_DEBUG, "show header:[%s]\n",PStr);
   free(PStr);
   
   /* Next free category ID */
   fread(filler, 4, 1, in);

   get_categories(in, ai);
#ifdef JPILOT_DEBUG
   for (i=0; i<16; i++) {
      printf("%d [%s]\n", ai->ID[i], ai->name[i]);
   }
#endif

   /* Schema resource ID */
   fread(filler, 4, 1, in);
   /* Schema fields per row (6) */
   fread(filler, 4, 1, in);
   if (x86_long(filler) != 6) {
      jpilot_logf(LOG_WARN, "fields per row count != 6, unknown format\n");
      return 0;
   }
   /* Schema record ID position */
   fread(filler, 4, 1, in);
   /* Schema record status position */
   fread(filler, 4, 1, in);
   /* Schema placement position */
   fread(filler, 4, 1, in);
   /* Schema fields count (6) */
   fread(filler, 2, 1, in);
   num_fields = x86_short(filler);
   if (num_fields != 6) {
      jpilot_logf(LOG_WARN, "unknown format, field count != 6\n");
      return 0;
   }

   /* Schema fields */
   /* Should be 1, 1, 1, 5, 6, 1 */
   fread(filler, num_fields*2, 1, in);
   if (memcmp(filler, schema, 12)) {
      jpilot_logf(LOG_WARN, "unknown format, file has wrong schema\n");
      jpilot_logf(LOG_WARN, "File schema is:");
      for (i=0; i<12; i++) {
	 jpilot_logf(LOG_WARN, " %02d", (char)filler[i]);
      }
      jpilot_logf(LOG_WARN, "\n");
      jpilot_logf(LOG_WARN, "It should be:  ");
      for (i=0; i<12; i++) {
	 jpilot_logf(LOG_WARN, " %02d", (char)filler[i]);
      }
      jpilot_logf(LOG_WARN, "\n");
      return 0;
   }

   /* Get record count */
   fread(str_long, 4, 1, in);
   count = x86_long(str_long) / num_fields;
#ifdef JPILOT_DEBUG
   printf("Record Count = %ld\n", count);
#endif   
   /* Get records */
   last_memolist=*memolist;
#ifdef JPILOT_DEBUG
   printf("---------- Records ----------\n");
#endif
   for (i=0; i<count; i++) {
      temp_memolist = malloc(sizeof(MemoList));
      if (!temp_memolist) {
	 jpilot_logf(LOG_WARN, "dat_get_memos(): Out of memory\n");
	 return count;
      }
      if (last_memolist) {
	 last_memolist->next=temp_memolist;
	 last_memolist=temp_memolist;
      } else {
	 last_memolist=temp_memolist;
	 *memolist=last_memolist;
      }
      last_memolist->next=NULL;
      last_memolist->app_type=MEMO;
#ifdef JPILOT_DEBUG
      printf("----- record %d -----\n", i+1);
#endif
      /* Record ID */
      get_field(in, &f);
#ifdef JPILOT_DEBUG
      print_field(&f);
#endif
      if (f.type!=schema[0]) {
	 jpilot_logf(LOG_WARN, "Invalid schema ID record %d, field 1\n", i+1);
	 return 0;
      }
      /* status field */
      get_field(in, &f);
#ifdef JPILOT_DEBUG
      print_field(&f);
#endif
      if (f.type!=schema[2]) {
	 jpilot_logf(LOG_WARN, "Invalid schema ID record %d, field 2\n", i+1);
	 return 0;
      }
      last_memolist->mmemo.attrib=f.i;
      /* position */
      get_field(in, &f);
#ifdef JPILOT_DEBUG
      print_field(&f);
#endif
      if (f.type!=schema[4]) {
	 jpilot_logf(LOG_WARN, "Invalid schema ID record %d, field 3\n", i+1);
	 return 0;
      }
      /* memo */
      get_field(in, &f);
#ifdef JPILOT_DEBUG
      print_field(&f);
#endif
      if (f.type!=schema[6]) {
	 jpilot_logf(LOG_WARN, "Invalid schema ID record %d, field 4\n", i+1);
	 return 0;
      }
      last_memolist->mmemo.memo.text=f.str;
      /* private */
      get_field(in, &f);
#ifdef JPILOT_DEBUG
      print_field(&f);
#endif
      if (f.type!=schema[8]) {
	 jpilot_logf(LOG_WARN, "Invalid schema ID record %d, field 5\n", i+1);
	 return 0;
      }
      if (f.i) {
	 last_memolist->mmemo.attrib |= DAT_STATUS_PRIVATE;
      }
      /* category */
      get_field(in, &f);
#ifdef JPILOT_DEBUG
      print_field(&f);
#endif
      /* Normally the category would go into 4 bits of the attrib.
       * They jumbled the attrib bits, so to make things easy I'll put it
       * here.
       */
      last_memolist->mmemo.unique_id = f.i;
      if (f.type!=schema[10]) {
	 jpilot_logf(LOG_WARN, "Invalid schema ID record %d, field 6\n", i+1);
	 return 0;
      }
   }
   
   return 0;
}

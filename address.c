/* address.c
 *
 * Copyright (C) 1999 by Judd Montgomery
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
#include "i18n.h"
#include <stdio.h>
#include <pi-source.h>
#include <pi-socket.h>
#include <pi-address.h>
#include <pi-dlp.h>
#include "address.h"
#include "utils.h"
#include "log.h"
#include "prefs.h"

#if defined(WITH_JAPANESE)
#include "japanese.h"
#endif

#define ADDRESS_EOF 7

#ifdef JPILOT_DEBUG
int print_address_list(AddressList **al)
{
   AddressList *temp_al, *prev_al, *next;

   for (prev_al=NULL, temp_al=*al; temp_al;
	prev_al=temp_al, temp_al=temp_al->next) {
      jpilot_logf(LOG_FILE | LOG_STDOUT, "entry[0]=[%s]\n", temp_al->ma.a.entry[0]);
   }
}
#endif

static int glob_sort_by_company;

int address_compare(const void *v1, const void *v2)
{
   char str1[100], str2[100];
   int sort1, sort2, sort3;
   AddressList **al1, **al2;
   struct Address *a1, *a2;

   al1=(AddressList **)v1;
   al2=(AddressList **)v2;
   
   a1=&((*al1)->ma.a);
   a2=&((*al2)->ma.a);

   if (glob_sort_by_company) {
      sort1=2; /*company */
      sort2=0; /*last name */
      sort3=1; /*first name */
   } else {
      sort1=0; /*last name */
      sort2=1; /*first name */
      sort3=2; /*company */
   }
   /*sort_by_company: */
   /*0 last, first or */
   /*1 company, last */
   
   
   str1[0]='\0';
   str2[0]='\0';

   if (a1->entry[sort1] || a1->entry[sort2]) {
      if (a1->entry[sort1] && a1->entry[sort2]) {
	 strncpy(str1, a1->entry[sort1], 99);
	 str1[99]='\0';
	 strncat(str1, a1->entry[sort2], 99-strlen(str1));
      }
      if (a1->entry[sort1] && (!a1->entry[sort2])) {
	 strncpy(str1, a1->entry[sort1], 99);
      }
      if ((!a1->entry[sort1]) && a1->entry[sort2]) {
	 strncpy(str1, a1->entry[sort2], 99);
      }
   } else if (a1->entry[sort3]) {
      strncpy(str1, a1->entry[sort3], 99);
   }

   if (a2->entry[sort1] || a2->entry[sort2]) {
      if (a2->entry[sort1] && a2->entry[sort2]) {
	 strncpy(str2, a2->entry[sort1], 99);
	 str2[99]='\0';
	 strncat(str2, a2->entry[sort2], 99-strlen(str2));
      }
      if (a2->entry[sort1] && (!a2->entry[sort2])) {
	 strncpy(str2, a2->entry[sort1], 99);
      }
      if ((!a2->entry[sort1]) && a2->entry[sort2]) {
	 strncpy(str2, a2->entry[sort2], 99);
      }
   } else if (a2->entry[sort3]) {
      strncpy(str2, a2->entry[sort3], 99);
   }

   return strncasecmp(str2, str1, 99);
}

/* 
 * sort_order: 0=descending,  1=ascending
 */
int address_sort(AddressList **al, int sort_order)
{
   AddressList *temp_al;
   AddressList **sort_al;
   struct AddressAppInfo ai;
   int count, i;

   /* Count the entries in the list */
   for (count=0, temp_al=*al; temp_al; temp_al=temp_al->next, count++) {
      ;
   }

   if (count<2) {
      /* We don't have to sort less than 2 items */
      return 0;
   }
   
   get_address_app_info(&ai);

   glob_sort_by_company = ai.sortByCompany;

   /* Allocate an array to be qsorted */
   sort_al = calloc(count, sizeof(AddressList *));
   if (!sort_al) {
      jpilot_logf(LOG_WARN, _("Out of Memory\n"));
      return 0;
   }
   
   /* Set our array to be a list of pointers to the nodes in the linked list */
   for (i=0, temp_al=*al; temp_al; temp_al=temp_al->next, i++) {
      sort_al[i] = temp_al;
   }

   /* qsort them */
   qsort(sort_al, count, sizeof(AddressList *), address_compare);

   /* Put the linked list in the order of the array */
   if (sort_order==SORT_ASCENDING) {
      for (i=count-1; i>0; i--) {
	 sort_al[i]->next=sort_al[i-1];
      }
      sort_al[0]->next = NULL;
      *al = sort_al[count-1];
   } else {
      /* Descending order */
      sort_al[count-1]->next = NULL;
      for (i=count-1; i; i--) {
	 sort_al[i-1]->next=sort_al[i];
      }
      *al = sort_al[0];
   }

   free(sort_al);

   return 0;
}

static int pc_address_read_next_rec(FILE *in, MyAddress *ma)
{
   PCRecordHeader header;
   int rec_len, num;
   char *record;
   
   if (feof(in)) {
      return ADDRESS_EOF;
   }
   num = fread(&header, sizeof(header), 1, in);
   if (num != 1) {
      if (ferror(in)) {
	 jpilot_logf(LOG_WARN, _("Error reading %s\n"),"AddressDB.pc");
	 return ADDRESS_EOF;
      }
      if (feof(in)) {
	 return ADDRESS_EOF;
      }
   }
   rec_len = header.rec_len;
   ma->rt = header.rt;
   ma->attrib = header.attrib;
   ma->unique_id = header.unique_id;
   record = malloc(rec_len);
   if (!record) {
      if (rec_len > 0) {
	 jpilot_logf(LOG_WARN, _("Out of memory 1\n"));
      }
      return ADDRESS_EOF;
   }
   num = fread(record, rec_len, 1, in);
   if (num != 1) {
      if (ferror(in)) {
	 jpilot_logf(LOG_WARN, _("Error reading %s\n"),"AddressDB.pc");
	 free(record);
	 return ADDRESS_EOF;
      }
   }
   num = unpack_Address(&(ma->a), record, rec_len);
   free(record);
   if (num<=0) {
      return -1;
   }
   return 0;
}

int pc_address_write(struct Address *a, PCRecType rt, unsigned char attrib,
		     unsigned int *unique_id)
{
   PCRecordHeader header;
   /*PCFileHeader   file_header; */
   FILE *out;
   char record[65536];
   int rec_len;
   unsigned int next_unique_id;

   get_next_unique_pc_id(&next_unique_id);
   *unique_id = next_unique_id;
#ifdef JPILOT_DEBUG
   jpilot_logf(LOG_DEBUG, "next unique id = %d\n",next_unique_id);
#endif
   
   out = open_file("AddressDB.pc", "a");
   if (!out) {
      jpilot_logf(LOG_WARN, _("Error opening %s\n"), "AddressDB.pc");
      return -1;
   }
   rec_len = pack_Address(a, record, 65535);
   if (!rec_len) {
      PRINT_FILE_LINE;
      jpilot_logf(LOG_WARN, "pack_Address %s\n",_("error"));
      return -1;
   }
   header.rec_len=rec_len;
   header.rt=rt;
   header.attrib=attrib;
   header.unique_id=next_unique_id;
   fwrite(&header, sizeof(header), 1, out);
   fwrite(record, rec_len, 1, out);
   fclose(out);
   
   return 0;
}

void free_AddressList(AddressList **al)
{
   AddressList *temp_al, *temp_al_next;

   for (temp_al = *al; temp_al; temp_al=temp_al_next) {
      free_Address(&(temp_al->ma.a));
      temp_al_next = temp_al->next;
      free(temp_al);
   }
   *al = NULL;
}

int get_address_app_info(struct AddressAppInfo *ai)
{
   FILE *in;
   int num;
   unsigned int rec_size;
   char *buf;
   RawDBHeader rdbh;
   DBHeader dbh;

   bzero(ai, sizeof(*ai));
   in = open_file("AddressDB.pdb", "r");
   if (!in) {
      jpilot_logf(LOG_WARN, _("Error opening %s\n"), "AddressDB.pdb");
      return -1;
   }
   num = fread(&rdbh, sizeof(RawDBHeader), 1, in);
   if (num != 1) {
      if (ferror(in)) {
	 jpilot_logf(LOG_WARN, _("Error reading %s\n"), "AddressDB.pdb");
	 fclose(in);
	 return -1;
      }
   }

   raw_header_to_header(&rdbh, &dbh);

   num = get_app_info_size(in, &rec_size);
   if (num) {
      fclose(in);
      return -1;
   }

   fseek(in, dbh.app_info_offset, SEEK_SET);
   buf=malloc(rec_size);
   if (!buf) {
      if (rec_size > 0) {
	 jpilot_logf(LOG_WARN, _("Out of memory 2\n"));
      }
      fclose(in);
      return -1;
   }
   num = fread(buf, rec_size, 1, in);
   if (num != 1) {
      if (ferror(in)) {
	 fclose(in);
	 free(buf);
	 jpilot_logf(LOG_WARN, _("Error reading %s\n"), "AddressDB.pdb");
	 return -1;
      }
   }
   num = unpack_AddressAppInfo(ai, buf, rec_size);
   if (num <= 0) {
      if (ferror(in)) {
	 fclose(in);
	 free(buf);
	 jpilot_logf(LOG_WARN, _("Error in %s\n"), "unpack_AddressAppInfo");
	 return -1;
      }
   }
#if defined(WITH_JAPANESE)
   /* Converto to EUC Japanese Kanji code */
   {
      int i;
      for (i = 0; i < 16; i++)
	 if (ai->category.name[i][0] != '\0')
            Sjis2Euc(ai->category.name[i], 16);
      for (i = 0; i < 19 + 3; i++)
         if (ai->labels[i][0] != '\0')
            Sjis2Euc(ai->labels[i], 16);
      for (i = 0; i < 8; i++)
         if (ai->phoneLabels[i][0] != '\0')
            Sjis2Euc(ai->phoneLabels[i], 16);
   }
#endif
   free(buf);

   fclose(in);

   return 0;
}


int get_addresses(AddressList **address_list, int sort_order)
{
   return get_addresses2(address_list, sort_order, 1, 1, CATEGORY_ALL);
}
/* 
 * sort_order: 0=descending,  1=ascending
 * modified and deleted, 0 for no, 1 for yes, 2 for use prefs
 */
int get_addresses2(AddressList **address_list, int sort_order,
		  int modified, int deleted, int category)
{
   FILE *in, *pc_in;
   char *buf;
   int num_records, recs_returned, i, num, r;
   int out_of_order;
   unsigned int offset, prev_offset, next_offset, rec_size;
   long fpos;  /*file position indicator */
   unsigned char attrib;
   unsigned int unique_id;
   mem_rec_header *mem_rh, *temp_mem_rh, *last_mem_rh;
   record_header rh;
   RawDBHeader rdbh;
   DBHeader dbh;
   struct Address a;
   AddressList *temp_address_list;
   AddressList *tal, *next_al, *prev_al;
   MyAddress ma;
   long keep_modified, keep_deleted;
   
   keep_modified = modified;
   keep_deleted = deleted;

   if (modified==2) {
      get_pref(PREF_SHOW_MODIFIED, &keep_modified, NULL);
   }
   if (deleted==2) {
      get_pref(PREF_SHOW_DELETED, &keep_deleted, NULL);
   }

   mem_rh = last_mem_rh = NULL;
   recs_returned = 0;
   out_of_order = 0;
   prev_offset = 0;

   in = open_file("AddressDB.pdb", "r");
   if (!in) {
      jpilot_logf(LOG_WARN, _("Error opening %s\n"), "AddressDB.pdb");
      return -1;
   }
   /*Read the database header */
   num = fread(&rdbh, sizeof(RawDBHeader), 1, in);
   if (num != 1) {
      if (ferror(in)) {
	 fclose(in);
	 jpilot_logf(LOG_WARN, _("Error reading %s\n"), "AddressDB.pdb");
	 return -1;
      }
      if (feof(in)) {
	 return ADDRESS_EOF;
      }
   }

   raw_header_to_header(&rdbh, &dbh);
   
   jpilot_logf(LOG_DEBUG, "db_name = %s\n", dbh.db_name);
   jpilot_logf(LOG_DEBUG, "num records = %d\n", dbh.number_of_records);
   jpilot_logf(LOG_DEBUG, "app info offset = %d\n", dbh.app_info_offset);

   /*fread(filler, 2, 1, in); */

   /*Read each record entry header */
   num_records = dbh.number_of_records;
   /*jpilot_logf(LOG_DEBUG, "sizeof(record_header)=%d\n",sizeof(record_header)); */
   for (i=1; i<num_records+1; i++) {
      num = fread(&rh, sizeof(record_header), 1, in);
      if (num != 1) {
	 if (ferror(in)) {
	    fclose(in);
	    jpilot_logf(LOG_WARN, _("Error reading %s\n"), "AddressDB.pdb");
	    return -1;
	 }
	 if (feof(in)) {
	    return ADDRESS_EOF;
	 }      
      }
      offset = ((rh.Offset[0]*256+rh.Offset[1])*256+rh.Offset[2])*256+rh.Offset[3];

      if (offset < prev_offset) {
	 out_of_order = 1;
      }
      prev_offset = offset;

#ifdef JPILOT_DEBUG
      jpilot_logf(LOG_DEBUG, "record header %u offset = %u\n",i, offset);
      jpilot_logf(LOG_DEBUG, "       attrib 0x%x\n",rh.attrib);
      jpilot_logf(LOG_DEBUG, "    unique_ID %d %d %d = ",rh.unique_ID[0],rh.unique_ID[1],rh.unique_ID[2]);
      jpilot_logf(LOG_DEBUG, "%d\n",(rh.unique_ID[0]*256+rh.unique_ID[1])*256+rh.unique_ID[2]);
#endif
      temp_mem_rh = (mem_rec_header *)malloc(sizeof(mem_rec_header));
      temp_mem_rh->next = NULL;
      temp_mem_rh->rec_num = i;
      temp_mem_rh->offset = offset;
      temp_mem_rh->attrib = rh.attrib;
      temp_mem_rh->unique_id = (rh.unique_ID[0]*256+rh.unique_ID[1])*256+rh.unique_ID[2];
      if (mem_rh == NULL) {
	 mem_rh = temp_mem_rh;
	 last_mem_rh = temp_mem_rh;
      } else {
	 last_mem_rh->next = temp_mem_rh;
	 last_mem_rh = temp_mem_rh;
      }
   }

   temp_mem_rh = mem_rh;

   if (num_records) {
      if (out_of_order) {
	 find_next_offset(mem_rh, 0, &next_offset, &attrib, &unique_id);
      } else {
	 if (mem_rh) {
	    next_offset = mem_rh->offset;
	    attrib = mem_rh->attrib;
	    unique_id = mem_rh->unique_id;
	 }
      }
      fseek(in, next_offset, SEEK_SET);
      while(!feof(in)) {
	 fpos = ftell(in);
	 if (out_of_order) {
	    find_next_offset(mem_rh, fpos, &next_offset, &attrib, &unique_id);
	 } else {
	    next_offset = 0xFFFFFF;
	    if (temp_mem_rh) {
	       attrib = temp_mem_rh->attrib;
	       unique_id = temp_mem_rh->unique_id;
	       if (temp_mem_rh->next) {
		  temp_mem_rh = temp_mem_rh->next;
		  next_offset = temp_mem_rh->offset;
	       }
	    }
	 }
	 rec_size = next_offset - fpos;
#ifdef JPILOT_DEBUG
	 jpilot_logf(LOG_DEBUG, "rec_size = %u\n",rec_size);
	 jpilot_logf(LOG_DEBUG, "fpos,next_offset = %u %u\n",fpos,next_offset);
	 jpilot_logf(LOG_DEBUG, "----------\n");
#endif
	 buf = malloc(rec_size);
	 if (!buf) {
	    if (rec_size > 0) {
	       jpilot_logf(LOG_WARN, _("Out of memory 3\n"));
	    }
	    break;
	 }
	 num = fread(buf, rec_size, 1, in);
	 if (num != 1) {
	    if (ferror(in)) {
	       free(buf);
	       break;
	    }
	 }

	 /* check category */
	 if ( ((attrib & 0x0F) != category) &&
	     category != CATEGORY_ALL) {
	    free(buf);
	    continue;
	 }

	 num = unpack_Address(&a, buf, rec_size);
	 free(buf);
	 if (num<=0) {
	    continue;
	 }
#if defined(WITH_JAPANESE)
	/* Convert to EUC Japanese Kanji code */
	{
	    int i;
	    for (i = 0; i < 19; i++)
		if (a.entry[i] != NULL)
		    Sjis2Euc(a.entry[i], 65536);
	}
#endif
	 temp_address_list = malloc(sizeof(AddressList));
	 memcpy(&(temp_address_list->ma.a), &a, sizeof(struct Address));
	 /*temp_address_list->ma.a = temp_a; */
	 temp_address_list->app_type = ADDRESS;
	 temp_address_list->ma.rt = PALM_REC;
	 temp_address_list->ma.attrib = attrib;
	 temp_address_list->ma.unique_id = unique_id;
	 temp_address_list->next = *address_list;
	 *address_list = temp_address_list;
	 recs_returned++;
      }
   }
   fclose(in);
   free_mem_rec_header(&mem_rh);

   /* */
   /*Get the appointments out of the PC database */
   /* */
   pc_in = open_file("AddressDB.pc", "r");
   if (pc_in==NULL) {
      jpilot_logf(LOG_WARN, _("Error opening %s\n"), "AddressDB.pc\n");
      return -1;
   }
   /*r = pc_datebook_read_file_header(pc_in); */
   while(!feof(pc_in)) {
      r = pc_address_read_next_rec(pc_in, &ma);
      if (r==ADDRESS_EOF) break;
      if (r<0) break;
      if ((ma.rt!=DELETED_PC_REC)
	  &&(ma.rt!=DELETED_PALM_REC)
	  &&(ma.rt!=MODIFIED_PALM_REC)
	  &&(ma.rt!=DELETED_DELETED_PALM_REC)) {
	 /* check category */
	 if ( ((ma.attrib & 0x0F) != category) &&
	     category != CATEGORY_ALL) {
	    continue;
	 }
	 temp_address_list = malloc(sizeof(AddressList));
	 if (!temp_address_list) {
	    jpilot_logf(LOG_WARN, _("Out of memory 4\n"));
	    break;
	 }
	 memcpy(&(temp_address_list->ma), &ma, sizeof(MyAddress));
	 temp_address_list->app_type = ADDRESS;
	 temp_address_list->next = *address_list;
	 *address_list = temp_address_list;
	 recs_returned++;
	 /*temp_address_list->ma.attrib=0; */
      } else {
	 /*this doesnt really free it, just the string pointers */
	 free_Address(&(ma.a));
      }

      if ( ((ma.rt==DELETED_PALM_REC) && (keep_deleted)) || 
	  ((ma.rt==MODIFIED_PALM_REC) && (keep_modified)) ) {
	 for (temp_address_list = *address_list; temp_address_list;
	      temp_address_list=temp_address_list->next) {
	    if (temp_address_list->ma.unique_id == ma.unique_id) {
	       temp_address_list->ma.rt = ma.rt;
	    }
	 }
      } else if ( ((ma.rt==DELETED_PALM_REC) && (!keep_deleted)) || 
		 ((ma.rt==MODIFIED_PALM_REC) && (!keep_modified)) ) {
	 for (prev_al=NULL, tal=*address_list; tal; tal = next_al) {
	    if (tal->ma.unique_id == ma.unique_id) {
	       /* Remove it from this list */
	       if (prev_al) {
		  prev_al->next=tal->next;
	       } else {
		  *address_list=tal->next;
	       }
	       next_al=tal->next;
	       free_Address(&(tal->ma.a));
	       free(tal);
	    } else {
	       prev_al=tal;
	       next_al=tal->next;
	    }
	 }
      }
   }

   fclose(pc_in);

#ifdef JPILOT_DEBUG
   print_address_list(address_list);
#endif
   address_sort(address_list, sort_order);
   
   jpilot_logf(LOG_DEBUG, "Leaving get_addresses\n");
   
   return recs_returned;
}

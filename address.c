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
#include <stdio.h>
#include <pi-source.h>
#include <pi-socket.h>
#include <pi-address.h>
#include <pi-dlp.h>
#include "address.h"
#include "utils.h"
#include "log.h"

#if defined(Japanese)
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

int address_compare(struct Address *a1, struct Address *a2, int sort_by_company)
{
   char str1[100], str2[100];
   int sort1, sort2, sort3;
   
   if (sort_by_company) {
      sort1=2; //company
      sort2=0; //last name
      sort3=1; //first name
   } else {
      sort1=0; //last name
      sort2=1; //first name
      sort3=2; //company
   }
   //sort_by_company:
   //0 last, first or
   //1 company, last
   
   
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

   return strncasecmp(str1, str2, 99);
}

int address_sort(AddressList **al)
{
   AddressList *temp_al, *prev_al, *next;
   struct AddressAppInfo ai;
   int found_one;

   get_address_app_info(&ai);

   found_one=1;
   while (found_one) {
      found_one=0;
      for (prev_al=NULL, temp_al=*al; temp_al;
	   prev_al=temp_al, temp_al=temp_al->next) {
	 if (temp_al->next) {
	    if (address_compare(&(temp_al->ma.a),
	        &(temp_al->next->ma.a), ai.sortByCompany) < 0) {
	       found_one=1;
	       next=temp_al->next;
	       if (prev_al) {
		  prev_al->next = next;
	       }
	       temp_al->next=next->next;
	       next->next = temp_al;
	       if (temp_al==*al) {
		  *al=next;
	       }
	       temp_al=next;
	    }
	 }
      }
   }
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
   if (feof(in)) {
      return ADDRESS_EOF;
   }
   if (num != 1) {
      jpilot_logf(LOG_WARN, "error on fread\n");
      return ADDRESS_EOF;
   }
   rec_len = header.rec_len;
   ma->rt = header.rt;
   ma->attrib = header.attrib;
   ma->unique_id = header.unique_id;
   record = malloc(rec_len);
   if (!record) {
      return ADDRESS_EOF;
   }
   num = fread(record, rec_len, 1, in);
   if (feof(in)) {
      free(record);
      return ADDRESS_EOF;
   }
   if (num != 1) {
      jpilot_logf(LOG_WARN, "error on fread\n");
      free(record);
      return ADDRESS_EOF;
   }
   num = unpack_Address(&(ma->a), record, rec_len);
   free(record);
   if (num<=0) {
      return -1;
   }
   return 0;
}

int pc_address_write(struct Address *a, PCRecType rt, unsigned char attrib)
{
   PCRecordHeader header;
   //PCFileHeader   file_header;
   FILE *out;
   char record[65536];
   int rec_len;
   unsigned int next_unique_id;

   get_next_unique_pc_id(&next_unique_id);
#ifdef JPILOT_DEBUG
   jpilot_logf(LOG_DEBUG, "next unique id = %d\n",next_unique_id);
#endif
   
   out = open_file("AddressDB.pc", "a");
   if (!out) {
      jpilot_logf(LOG_WARN, "Error opening AddressDB.pc\n");
      return -1;
   }
   rec_len = pack_Address(a, record, 65535);
   if (!rec_len) {
      PRINT_FILE_LINE;
      jpilot_logf(LOG_WARN, "pack_Address error\n");
   }
   header.rec_len=rec_len;
   header.rt=rt;
   header.attrib=attrib;
   header.unique_id=next_unique_id;
   fwrite(&header, sizeof(header), 1, out);
   fwrite(record, rec_len, 1, out);
   fflush(out);
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

   in = open_file("AddressDB.pdb", "r");
   if (!in) {
      jpilot_logf(LOG_WARN, "Error opening AddressDB.pdb\n");
      return -1;
   }
   fread(&rdbh, sizeof(RawDBHeader), 1, in);
   if (feof(in)) {
      jpilot_logf(LOG_WARN, "Error reading AddressDB.pdb\n");
      fclose(in);
      return -1;
   }
   raw_header_to_header(&rdbh, &dbh);

   get_app_info_size(in, &rec_size);

   fseek(in, dbh.app_info_offset, SEEK_SET);
   buf=malloc(rec_size);
   if (!buf) {
      fclose(in);
      return -1;
   }
   num = fread(buf, 1, rec_size, in);
   if (feof(in)) {
      fclose(in);
      free(buf);
      jpilot_logf(LOG_WARN, "Error reading AddressDB.pdb\n");
      return -1;
   }
   unpack_AddressAppInfo(ai, buf, rec_size);
#if defined(Japanese)
   // Converto to EUC Japanese Kanji code
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

int get_addresses(AddressList **address_list)
{
   FILE *in, *pc_in;
   char *buf;
   int num_records, recs_returned, i, num, r;
   unsigned int offset, next_offset, rec_size;
   long fpos;  //file position indicator
   unsigned char attrib;
   unsigned int unique_id;
   mem_rec_header *mem_rh, *temp_mem_rh;
   record_header rh;
   RawDBHeader rdbh;
   DBHeader dbh;
   struct Address a;
   AddressList *temp_address_list;
   MyAddress ma;

   mem_rh = NULL;
   recs_returned = 0;

   in = open_file("AddressDB.pdb", "r");
   if (!in) {
      jpilot_logf(LOG_WARN, "Error opening AddressDB.pdb\n");
      return -1;
   }
   //Read the database header
   fread(&rdbh, sizeof(RawDBHeader), 1, in);
   if (feof(in)) {
      jpilot_logf(LOG_WARN, "Error opening AddressDB.pdb\n");
      return -1;
   }
   raw_header_to_header(&rdbh, &dbh);
   
   jpilot_logf(LOG_DEBUG, "db_name = %s\n", dbh.db_name);
   jpilot_logf(LOG_DEBUG, "num records = %d\n", dbh.number_of_records);
   jpilot_logf(LOG_DEBUG, "app info offset = %d\n", dbh.app_info_offset);

   //fread(filler, 2, 1, in);

   //Read each record entry header
   num_records = dbh.number_of_records;
   //jpilot_logf(LOG_DEBUG, "sizeof(record_header)=%d\n",sizeof(record_header));
   for (i=1; i<num_records+1; i++) {
      fread(&rh, sizeof(record_header), 1, in);
      offset = ((rh.Offset[0]*256+rh.Offset[1])*256+rh.Offset[2])*256+rh.Offset[3];
#ifdef JPILOT_DEBUG
      jpilot_logf(LOG_DEBUG, "record header %u offset = %u\n",i, offset);
      jpilot_logf(LOG_DEBUG, "       attrib 0x%x\n",rh.attrib);
      jpilot_logf(LOG_DEBUG, "    unique_ID %d %d %d = ",rh.unique_ID[0],rh.unique_ID[1],rh.unique_ID[2]);
      jpilot_logf(LOG_DEBUG, "%d\n",(rh.unique_ID[0]*256+rh.unique_ID[1])*256+rh.unique_ID[2]);
#endif
      temp_mem_rh = (mem_rec_header *)malloc(sizeof(mem_rec_header));
      temp_mem_rh->next = mem_rh;
      mem_rh = temp_mem_rh;
      mem_rh->rec_num = i;
      mem_rh->offset = offset;
      mem_rh->attrib = rh.attrib;
      mem_rh->unique_id = (rh.unique_ID[0]*256+rh.unique_ID[1])*256+rh.unique_ID[2];
   }

   if (num_records) {
      find_next_offset(mem_rh, 0, &next_offset, &attrib, &unique_id);
      fseek(in, next_offset, SEEK_SET);
      while(!feof(in)) {
	 fpos = ftell(in);
	 find_next_offset(mem_rh, fpos, &next_offset, &attrib, &unique_id);
	 rec_size = next_offset - fpos;
#ifdef JPILOT_DEBUG
	 jpilot_logf(LOG_DEBUG, "rec_size = %u\n",rec_size);
	 jpilot_logf(LOG_DEBUG, "fpos,next_offset = %u %u\n",fpos,next_offset);
	 jpilot_logf(LOG_DEBUG, "----------\n");
#endif
	 if (feof(in)) break;
	 buf = malloc(rec_size);
	 if (!buf) break;
	 num = fread(buf, 1, rec_size, in);
	 if (num<=0) {
	    free(buf);
	    break;
	 }

	 num = unpack_Address(&a, buf, rec_size);
	 if (num<=0) {
	    free(buf);
	    continue;
	 }
#if defined(Japanese)
	// Convert to EUC Japanese Kanji code
	{
	    int i;
	    for (i = 0; i < 19; i++)
		if (a.entry[i] != NULL)
		    Sjis2Euc(a.entry[i], 65536);
	}
#endif
	 temp_address_list = malloc(sizeof(AddressList));
	 memcpy(&(temp_address_list->ma.a), &a, sizeof(struct Address));
	 //temp_address_list->ma.a = temp_a;
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

   //
   //Get the appointments out of the PC database
   //
   pc_in = open_file("AddressDB.pc", "r");
   if (pc_in==NULL) {
      return -1;
   }
   //r = pc_datebook_read_file_header(pc_in);
   while(!feof(pc_in)) {
      r = pc_address_read_next_rec(pc_in, &ma);
      if (r==ADDRESS_EOF) break;
      if ((ma.rt!=DELETED_PC_REC)
	  &&(ma.rt!=DELETED_PALM_REC)
	  &&(ma.rt!=MODIFIED_PALM_REC)
	  &&(ma.rt!=DELETED_DELETED_PALM_REC)) {
	 temp_address_list = malloc(sizeof(AddressList));
	 memcpy(&(temp_address_list->ma), &ma, sizeof(MyAddress));
	 temp_address_list->app_type = ADDRESS;
	 temp_address_list->next = *address_list;
	 *address_list = temp_address_list;
	 recs_returned++;

	 //temp_address_list->ma.attrib=0;
      } else {
	 //this doesnt really free it, just the string pointers
	 free_Address(&(ma.a));
      }
      if ((ma.rt==DELETED_PALM_REC) || (ma.rt==MODIFIED_PALM_REC)) {
	 for (temp_address_list = *address_list; temp_address_list;
	      temp_address_list=temp_address_list->next) {
	    if (temp_address_list->ma.unique_id == ma.unique_id) {
	       temp_address_list->ma.rt = ma.rt;
	    }
	 }
      }
   }
   fclose(pc_in);

#ifdef JPILOT_DEBUG
   print_address_list(address_list);
#endif
   address_sort(address_list);
   
   jpilot_logf(LOG_DEBUG, "Leaving get_addresses\n");
   
   return recs_returned;
}

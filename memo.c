/* memo.c
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
#include <pi-memo.h>
#include <pi-dlp.h>
#include "memo.h"
#include "utils.h"
#include "log.h"
#include "prefs.h"

#if defined(WITH_JAPANESE)
#include "japanese.h"
#endif

#define MEMO_EOF 7

/* Memos aren't really sorted, this just reverses them if needed */
int memo_sort(MemoList **memol, int sort_order)
{
   MemoList *temp_memol;
   MemoList **sort_memol;
   int count, i;

   if (sort_order==SORT_DESCENDING) {
      return 0;
   }
   
   /* Count the entries in the list */
   for (count=0, temp_memol=*memol; temp_memol; temp_memol=temp_memol->next, count++) {
      ;
   }

   if (count<2) {
      /* We don't have to sort less than 2 items */
      return 0;
   }
   
   /* Allocate an array to be qsorted */
   sort_memol = calloc(count, sizeof(MemoList *));
   if (!sort_memol) {
      jpilot_logf(LOG_WARN, _("Out of Memory\n"));
      return 0;
   }
   
   /* Set our array to be a list of pointers to the nodes in the linked list */
   for (i=0, temp_memol=*memol; temp_memol; temp_memol=temp_memol->next, i++) {
      sort_memol[i] = temp_memol;
   }

   /* Put the linked list in the order of the array */
   if (sort_order==SORT_ASCENDING) {
      for (i=count-1; i>0; i--) {
	 sort_memol[i]->next=sort_memol[i-1];
      }
      sort_memol[0]->next = NULL;
      *memol = sort_memol[count-1];
   }
   
   free(sort_memol);

   return 0;
}

int pc_memo_write(struct Memo *memo, PCRecType rt, unsigned char attrib,
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
   
   out = open_file("MemoDB.pc", "a");
   if (!out) {
      jpilot_logf(LOG_WARN, _("Error opening %s\n"), "MemoDB.pc");
      return -1;
   }
   rec_len = pack_Memo(memo, record, 65535);
   if (!rec_len) {
      PRINT_FILE_LINE;
      jpilot_logf(LOG_WARN, "pack_Memo %s\n", _("error"));
      return -1;
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


static int pc_memo_read_next_rec(FILE *in, MyMemo *mmemo)
{
   PCRecordHeader header;
   int rec_len;
   char *record;
   int num;
   
   if (feof(in)) {
      return MEMO_EOF;
   }
   num = fread(&header, sizeof(header), 1, in);
   if (num != 1) {
      if (ferror(in)) {
	 jpilot_logf(LOG_WARN, _("Error reading %s\n"), "MemoDB.pc");
	 return MEMO_EOF;
      }
      if (feof(in)) {
	 return MEMO_EOF;
      }      
   }
   rec_len = header.rec_len;
   mmemo->rt = header.rt;
   mmemo->attrib = header.attrib;
   mmemo->unique_id = header.unique_id;
   record = malloc(rec_len);
   if (!record) {
      jpilot_logf(LOG_DEBUG, _("Out of memory\n"));
      return MEMO_EOF;
   }
   num = fread(record, rec_len, 1, in);
   if (num != 1) {
      if (ferror(in)) {
	 free(record);
	 jpilot_logf(LOG_WARN, _("Error reading %s\n"), "MemoDB.pc");
	 return MEMO_EOF;
      }
   }

   num = unpack_Memo(&(mmemo->memo), record, rec_len);
   free(record);
   if (num <= 0) {
      jpilot_logf(LOG_WARN, "unpack_Memo %s\n", _("failed"));
      return -1;
   }
   return 0;
}

void free_MemoList(MemoList **memo)
{
   MemoList *temp_memo, *temp_memo_next;
   
   for (temp_memo = *memo; temp_memo; temp_memo=temp_memo_next) {
      free_Memo(&(temp_memo->mmemo.memo));
      temp_memo_next = temp_memo->next;
      free(temp_memo);
   }
   *memo = NULL;
}

int get_memo_app_info(struct MemoAppInfo *ai)
{
   FILE *in;
   int num;
   unsigned int rec_size;
   char *buf;
   RawDBHeader rdbh;
   DBHeader dbh;

   bzero(ai, sizeof(*ai));
   in = open_file("MemoDB.pdb", "r");
   if (!in) {
      jpilot_logf(LOG_WARN, _("Error opening %s\n"), "MemoDB.pdb");
      return -1;
   }
   num = fread(&rdbh, sizeof(RawDBHeader), 1, in);
   if (num != 1) {
      if (ferror(in)) {
	 jpilot_logf(LOG_WARN, _("Error reading %s\n"), "MemoDB.pdb");
	 fclose(in);
	 return -1;
      }
      if (feof(in)) {
	 fclose(in);
	 return MEMO_EOF;
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
      jpilot_logf(LOG_WARN, _("Out of memory\n"));
      fclose(in);
      return -1;
   }
   num = fread(buf, rec_size, 1, in);
   if (num != 1) {
      if (ferror(in)) {
	 fclose(in);
	 free(buf);
	 jpilot_logf(LOG_WARN, _("Error reading %s\n"), "MemoDB.pdb");
	 return -1;
      }
   }
   num = unpack_MemoAppInfo(ai, buf, rec_size);
   if (num <= 0) {
      jpilot_logf(LOG_WARN, _("Error reading"), "MemoDB.pdb");
      free(buf);
      fclose(in);
      return -1;
   }
#if defined(WITH_JAPANESE)
   {
      int i;
      for (i = 0; i < 16; i++)
	 if (ai->category.name[i][0] != '\0')
	    Sjis2Euc(ai->category.name[i], 16);
   }
#endif
   free(buf);
   
   fclose(in);
   
   return 0;
}

int get_memos(MemoList **memo_list, int sort_order)
{
   return get_memos2(memo_list, sort_order, 1, 1, CATEGORY_ALL);
}
/* 
 * sort_order: 0=descending,  1=ascending (memos are not sorted)
 * modified and deleted, 0 for no, 1 for yes, 2 for use prefs
 */
int get_memos2(MemoList **memo_list, int sort_order,
 	       int modified, int deleted, int category)
{
   FILE *in, *pc_in;
   char *buf;
   int num_records, recs_returned, i, num, r;
   unsigned int offset, prev_offset, next_offset, rec_size;
   int out_of_order;
   long fpos;  /*file position indicator */
   unsigned char attrib;
   unsigned int unique_id;
   mem_rec_header *mem_rh, *temp_mem_rh, *last_mem_rh;
   record_header rh;
   RawDBHeader rdbh;
   DBHeader dbh;
   struct Memo memo;
   MemoList *temp_memo_list;
   MemoList *tl, *next_l, *prev_l;
   MyMemo mmemo;
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
   *memo_list=NULL;
   recs_returned = 0;

   in = open_file("MemoDB.pdb", "r");
   if (!in) {
      jpilot_logf(LOG_WARN, _("Error opening %s\n"), "MemoDB.pdb");
      return -1;
   }
   /*Read the database header */
   num = fread(&rdbh, sizeof(RawDBHeader), 1, in);
   if (num != 1) {	
      if (ferror(in)) {
	 jpilot_logf(LOG_WARN, _("Error reading %s\n"), "MemoDB.pdb");
	 fclose(in);
	 return -1;
      }
      if (feof(in)) {
	 return MEMO_EOF;
      }      
   }

   raw_header_to_header(&rdbh, &dbh);
   
   jpilot_logf(LOG_DEBUG, "db_name = %s\n", dbh.db_name);
   jpilot_logf(LOG_DEBUG, "num records = %d\n", dbh.number_of_records);
   jpilot_logf(LOG_DEBUG, "app info offset = %d\n", dbh.app_info_offset);

   out_of_order = 0;
   prev_offset = 0;
   
   /*Read each record entry header */
   num_records = dbh.number_of_records;
   for (i=1; i<num_records+1; i++) {
      num = fread(&rh, sizeof(record_header), 1, in);
      if (num != 1) {	
	 if (ferror(in)) {
	    jpilot_logf(LOG_DEBUG, _("Error reading %s\n"), "MemoDB.pdb");
	    fclose(in);
	    break;
	 }
	 if (feof(in)) {
	    return MEMO_EOF;
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
      if (!temp_mem_rh) {
	 jpilot_logf(LOG_WARN, _("Out of memory\n"));
	 break;
      }
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
	    jpilot_logf(LOG_WARN, _("Out of memory\n"));
	    break;
	 }
	 num = fread(buf, rec_size, 1, in);
	 if (num != 1) {	
	    if (ferror(in)) {
	       jpilot_logf(LOG_DEBUG, _("Error reading %s\n"), "MemoDB.pdb");
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
 
	 num = unpack_Memo(&memo, buf, rec_size);
	 free(buf);
	 if (num <= 0) {
	    continue;
	 }
#if defined(WITH_JAPANESE)
	 Sjis2Euc(memo.text, 65536);
#endif
	 temp_memo_list = malloc(sizeof(MemoList));
	 if (!temp_memo_list) {
	    jpilot_logf(LOG_WARN, _("Out of memory\n"));
	    break;
	 }
	 memcpy(&(temp_memo_list->mmemo.memo), &memo, sizeof(struct Memo));
	 temp_memo_list->app_type = MEMO;
	 temp_memo_list->mmemo.rt = PALM_REC;
	 temp_memo_list->mmemo.attrib = attrib;
	 temp_memo_list->mmemo.unique_id = unique_id;
	 temp_memo_list->next = *memo_list;
	 *memo_list = temp_memo_list;
	 recs_returned++;
      }
   }
   fclose(in);
   free_mem_rec_header(&mem_rh);

   /* */
   /*Get the appointments out of the PC database */
   /* */
   pc_in = open_file("MemoDB.pc", "r");
   if (pc_in==NULL) {
      return 0;
   }
   /*r = pc_datebook_read_file_header(pc_in); */
   while(!feof(pc_in)) {
      r = pc_memo_read_next_rec(pc_in, &mmemo);
      if (r==MEMO_EOF) break;
      if (r<0) break;
      if ((mmemo.rt!=DELETED_PC_REC)
	  &&(mmemo.rt!=DELETED_PALM_REC)
	  &&(mmemo.rt!=MODIFIED_PALM_REC)
	  &&(mmemo.rt!=DELETED_DELETED_PALM_REC)) {
	 temp_memo_list = malloc(sizeof(MemoList));
	 if (!temp_memo_list) {
	    jpilot_logf(LOG_WARN, _("Out of memory\n"));
	    break;
	 }
	 memcpy(&(temp_memo_list->mmemo), &mmemo, sizeof(MyMemo));
	 temp_memo_list->app_type = MEMO;
	 temp_memo_list->next = *memo_list;
	 *memo_list = temp_memo_list;
	 recs_returned++;
	 /*temp_address_list->ma.attrib=0; */
      } else {
	 /*this doesnt really free it, just the string pointers */
	 free_Memo(&(mmemo.memo));
      }
      if ( ((mmemo.rt==DELETED_PALM_REC) && (keep_deleted)) || 
  	  ((mmemo.rt==MODIFIED_PALM_REC) && (keep_modified)) ) {
	 for (temp_memo_list = *memo_list; temp_memo_list;
	      temp_memo_list=temp_memo_list->next) {
	    if (temp_memo_list->mmemo.unique_id == mmemo.unique_id) {
	       temp_memo_list->mmemo.rt = mmemo.rt;
	    }
	 }
      } else if ( ((mmemo.rt==DELETED_PALM_REC) && (!keep_deleted)) || 
  		 ((mmemo.rt==MODIFIED_PALM_REC) && (!keep_modified)) ) {
  	 for (prev_l=NULL, tl=*memo_list; tl; tl = next_l) {
  	    if (tl->mmemo.unique_id == mmemo.unique_id) {
  	       /* Remove it from this list */
  	       if (prev_l) {
  		  prev_l->next=tl->next;
  	       } else {
  		  *memo_list=tl->next;
  	       }
  	       next_l=tl->next;
  	       free_Memo(&(tl->mmemo.memo));
  	       free(tl);
  	    } else {
  	       prev_l=tl;
  	       next_l=tl->next;
  	    }
  	 }
      }
   }
   fclose(pc_in);

   memo_sort(memo_list, sort_order);

   jpilot_logf(LOG_DEBUG, "Leaving get_memos\n");

   return recs_returned;
}

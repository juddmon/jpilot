/*
 * memo.c
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
#include <pi-memo.h>
#include <pi-dlp.h>
//#include "memo.h"
#include "utils.h"
#include "log.h"

#define MEMO_EOF 7



int pc_memo_write(struct Memo *memo, PCRecType rt, unsigned char attrib)
{
   PCRecordHeader header;
   //PCFileHeader   file_header;
   FILE *out;
   char record[65536];
   int rec_len;
   unsigned int next_unique_id;

   get_next_unique_pc_id(&next_unique_id);
#ifdef JPILOT_DEBUG
   logf(LOG_DEBUG, "next unique id = %d\n",next_unique_id);
#endif
   
   out = open_file("MemoDB.pc", "a");
   if (!out) {
      logf(LOG_WARN, "Error opening MemoDB.pc\n");
      return -1;
   }
   rec_len = pack_Memo(memo, record, 65535);
   if (!rec_len) {
      PRINT_FILE_LINE;
      logf(LOG_WARN, "pack_Memo error\n");
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
}


static int pc_memo_read_next_rec(FILE *in, MyMemo *mmemo)
{
   PCRecordHeader header;
   int rec_len;
   char *record;
   
   if (feof(in)) {
      return MEMO_EOF;
   }
   fread(&header, sizeof(header), 1, in);
   if (feof(in)) {
      return MEMO_EOF;
   }
   rec_len = header.rec_len;
   mmemo->rt = header.rt;
   mmemo->attrib = header.attrib;
   mmemo->unique_id = header.unique_id;
   record = malloc(rec_len);
   if (!record) {
      return MEMO_EOF;
   }
   fread(record, rec_len, 1, in);
   if (feof(in)) {
      free(record);
      return MEMO_EOF;
   }
   unpack_Memo(&(mmemo->memo), record, rec_len);
   free(record);
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

   in = open_file("MemoDB.pdb", "r");
   if (!in) {
      logf(LOG_WARN, "Error opening MemoDB.pdb\n");
      return -1;
   }
   fread(&rdbh, sizeof(RawDBHeader), 1, in);
   if (feof(in)) {
      logf(LOG_WARN, "Error reading MemoDB.pdb\n");
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
      logf(LOG_WARN, "Error reading MemoDB.pdb\n");
      return -1;
   }
   unpack_MemoAppInfo(ai, buf, rec_size);
   free(buf);
   
   return 0;
}

int get_memos(MemoList **memo_list)
{
   FILE *in, *pc_in;
//   char db_name[34];
//   char filler[100];
   char *buf;
//   unsigned char char_num_records[4];
//   unsigned char char_ai_offset[4];//app info offset
   int num_records, recs_returned, i, num, r;
   unsigned int offset, next_offset, rec_size;
//   unsigned char c;
   long fpos;  //file position indicator
   unsigned char attrib;
   unsigned int unique_id;
   mem_rec_header *mem_rh, *temp_mem_rh;
   record_header rh;
   RawDBHeader rdbh;
   DBHeader dbh;
   struct Memo memo;
   //struct AddressAppInfo ai;
   MemoList *temp_memo_list;
   MyMemo mmemo;

   mem_rh = NULL;
   *memo_list=NULL;
   recs_returned = 0;

   in = open_file("MemoDB.pdb", "r");
   if (!in) {
      logf(LOG_WARN, "Error opening MemoDB.pdb\n");
      return -1;
   }
   //Read the database header
   fread(&rdbh, sizeof(RawDBHeader), 1, in);
   if (feof(in)) {
      logf(LOG_WARN, "Error opening MemoDB.pdb\n");
      return -1;
   }
   raw_header_to_header(&rdbh, &dbh);
   
   logf(LOG_DEBUG, "db_name = %s\n", dbh.db_name);
   logf(LOG_DEBUG, "num records = %d\n", dbh.number_of_records);
   logf(LOG_DEBUG, "app info offset = %d\n", dbh.app_info_offset);

   //fread(filler, 2, 1, in);

   //Read each record entry header
   num_records = dbh.number_of_records;
   for (i=1; i<num_records+1; i++) {
      fread(&rh, sizeof(record_header), 1, in);
      offset = ((rh.Offset[0]*256+rh.Offset[1])*256+rh.Offset[2])*256+rh.Offset[3];
#ifdef JPILOT_DEBUG
      logf(LOG_DEBUG, "record header %u offset = %u\n",i, offset);
      logf(LOG_DEBUG, "       attrib 0x%x\n",rh.attrib);
      logf(LOG_DEBUG, "    unique_ID %d %d %d = ",rh.unique_ID[0],rh.unique_ID[1],rh.unique_ID[2]);
      logf(LOG_DEBUG, "%d\n",(rh.unique_ID[0]*256+rh.unique_ID[1])*256+rh.unique_ID[2]);
#endif
      temp_mem_rh = (mem_rec_header *)malloc(sizeof(mem_rec_header));
      temp_mem_rh->next = mem_rh;
      mem_rh = temp_mem_rh;
      mem_rh->rec_num = i;
      mem_rh->offset = offset;
      mem_rh->attrib = rh.attrib;
      mem_rh->unique_id = (rh.unique_ID[0]*256+rh.unique_ID[1])*256+rh.unique_ID[2];
   }

   find_next_offset(mem_rh, 0, &next_offset, &attrib, &unique_id);
   fseek(in, next_offset, SEEK_SET);

   if (num_records) {
      while(!feof(in)) {
	 fpos = ftell(in);
	 find_next_offset(mem_rh, fpos, &next_offset, &attrib, &unique_id);
	 //next_offset += 223;
	 rec_size = next_offset - fpos;
#ifdef JPILOT_DEBUG
	 logf(LOG_DEBUG, "rec_size = %u\n",rec_size);
	 logf(LOG_DEBUG, "fpos,next_offset = %u %u\n",fpos,next_offset);
	 logf(LOG_DEBUG, "----------\n");
#endif
	 if (feof(in)) break;
	 buf = malloc(rec_size);
	 if (!buf) break;
	 num = fread(buf, 1, rec_size, in);
	 
	 unpack_Memo(&memo, buf, rec_size);
	 free(buf);
	 temp_memo_list = malloc(sizeof(MemoList));
	 memcpy(&(temp_memo_list->mmemo.memo), &memo, sizeof(struct Memo));
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

   //
   //Get the appointments out of the PC database
   //
   pc_in = open_file("MemoDB.pc", "r");
   if (pc_in==NULL) {
      return 0;
   }
   //r = pc_datebook_read_file_header(pc_in);
   while(!feof(pc_in)) {
      r = pc_memo_read_next_rec(pc_in, &mmemo);
      if (r==MEMO_EOF) break;
      if ((mmemo.rt!=DELETED_PC_REC)
	  &&(mmemo.rt!=DELETED_PALM_REC)
	  &&(mmemo.rt!=MODIFIED_PALM_REC)
	  &&(mmemo.rt!=DELETED_DELETED_PALM_REC)) {
	 temp_memo_list = malloc(sizeof(MemoList));
	 memcpy(&(temp_memo_list->mmemo), &mmemo, sizeof(MyMemo));
	 temp_memo_list->next = *memo_list;
	 *memo_list = temp_memo_list;
	 recs_returned++;

	 //temp_address_list->ma.attrib=0;
      } else {
	 //this doesnt really free it, just the string pointers
	 free_Memo(&(mmemo.memo));
      }
      if ((mmemo.rt==DELETED_PALM_REC) || (mmemo.rt==MODIFIED_PALM_REC)) {
	 for (temp_memo_list = *memo_list; temp_memo_list;
	      temp_memo_list=temp_memo_list->next) {
	    if (temp_memo_list->mmemo.unique_id == mmemo.unique_id) {
	       temp_memo_list->mmemo.rt = mmemo.rt;
	    }
	 }
      }
   }
   fclose(pc_in);

   return recs_returned;
}

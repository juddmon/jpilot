/*
 * todo.c
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
#include <pi-todo.h>
#include <pi-dlp.h>
#include "address.h"
#include "utils.h"

#define TODO_EOF 7

/*
 * sort by:
 * priority, due date
 * due date, priority
 * category, priority
 * category, due date
 */
int todo_compare(struct ToDo *todo1, struct ToDo *todo2,
		 struct ToDoAppInfo *ai,
		 int cat1, int cat2)
{
   time_t t1, t2;
   int r;
   int sort_by_priority;
   
   sort_by_priority = ai->sortByPriority;
   
   if (sort_by_priority == 0) {
      // priority, due date
      if ( (todo1->priority) < (todo2->priority) ) {
	 return -1;
      }
      if ( (todo1->priority) > (todo2->priority) ) {
	 return 1;
      }
      if ( (todo1->indefinite) && (todo2->indefinite) ) {
	 return 0;
      }
      if ( !(todo1->indefinite) && (todo2->indefinite) ) {
	 return -1;
      }
      if ( (todo1->indefinite) && !(todo2->indefinite) ) {
	 return 1;
      }
      t1 = mktime(&(todo1->due));
      t2 = mktime(&(todo1->due));
      if ( t1 < t2 ) {
	 return -1;
      }
      if ( t1 > t2 ) {
	 return 1;
      }
   }
      
   if (sort_by_priority == 1) {
      // due date, priority
      if ( !(todo1->indefinite) && (todo2->indefinite) ) {
	 return -1;
      }
      if ( (todo1->indefinite) && !(todo2->indefinite) ) {
	 return 1;
      }
      if ( !(todo1->indefinite) && !(todo2->indefinite) ) {
	 t1 = mktime(&(todo1->due));
	 t2 = mktime(&(todo1->due));
	 if ( t1 < t2 ) {
	    return -1;
	 }
	 if ( t1 > t2 ) {
	    return 1;
	 }
      }
      if ( (todo1->priority) < (todo2->priority) ) {
	 return -1;
      }
      if ( (todo1->priority) > (todo2->priority) ) {
	 return 1;
      }
   }

   if (sort_by_priority == 2) {
      // category, priority
      r = strcmp(ai->category.name[cat1],
		 ai->category.name[cat2]);
      if (r) {
	 return r;
      }
      if ( (todo1->priority) < (todo2->priority) ) {
	 return -1;
      }
      if ( (todo1->priority) > (todo2->priority) ) {
	 return 1;
      }
   }

   if (sort_by_priority == 3) {
      // category, due date
      r = strcmp(ai->category.name[cat1],
		 ai->category.name[cat2]);
      if (r) {
	 return r;
      }
      if ( (todo1->indefinite) && (todo2->indefinite) ) {
	 return 0;
      }
      if ( !(todo1->indefinite) && (todo2->indefinite) ) {
	 return -1;
      }
      if ( (todo1->indefinite) && !(todo2->indefinite) ) {
	 return 1;
      }
      t1 = mktime(&(todo1->due));
      t2 = mktime(&(todo1->due));
      if ( t1 < t2 ) {
	 return -1;
      }
      if ( t1 > t2 ) {
	 return 1;
      }
   }
      
   return 0;
}

int todo_sort(ToDoList **al)
{
   ToDoList *temp_al, *prev_al, *next;
   struct ToDoAppInfo ai;
   int found_one;

   get_todo_app_info(&ai);

   found_one=1;
   while (found_one) {
      found_one=0;
      for (prev_al=NULL, temp_al=*al; temp_al;
	   prev_al=temp_al, temp_al=temp_al->next) {
	 if (temp_al->next) {
	    if (todo_compare(&(temp_al->mtodo.todo),
	        &(temp_al->next->mtodo.todo),
		&ai,
		temp_al->mtodo.attrib & 0x0F,
		temp_al->next->mtodo.attrib & 0x0F) < 0) {
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
}

static int pc_todo_read_next_rec(FILE *in, MyToDo *mtodo)
{
   PCRecordHeader header;
   int rec_len;
   char *record;
   
   if (feof(in)) {
      return TODO_EOF;
   }
//  if (ftell(in)==0) {
//     printf("Error: File header not read\n");
//      return TODO_EOF;
//   }
   fread(&header, sizeof(header), 1, in);
   if (feof(in)) {
      return TODO_EOF;
   }
   rec_len = header.rec_len;
   mtodo->rt = header.rt;
   mtodo->attrib = header.attrib;
   //printf("read attrib = %d\n", mtodo->attrib);
   mtodo->unique_id = header.unique_id;
   record = malloc(rec_len);
   if (!record) {
      return TODO_EOF;
   }
   fread(record, rec_len, 1, in);
   if (feof(in)) {
      free(record);
      return TODO_EOF;
   }
   unpack_ToDo(&(mtodo->todo), record, rec_len);
   free(record);
   return 0;
}

int pc_todo_write(struct ToDo *todo, PCRecType rt, unsigned char attrib)
{
   PCRecordHeader header;
   //PCFileHeader   file_header;
   FILE *out;
   char record[65536];
   int rec_len;
   unsigned int next_unique_id;

   get_next_unique_pc_id(&next_unique_id);
#ifdef JPILOT_DEBUG
   printf("next unique id = %d\n",next_unique_id);
#endif
   
   out = open_file("ToDoDB.pc", "a");
   if (!out) {
      printf("Error opening ToDoDB.pc\n");
      return -1;
   }
   //todo check return code - if 0 then buffer was too small
   rec_len = pack_ToDo(todo, record, 65535);
   if (!rec_len) {
      printf("pack_ToDo failed\n");
      PRINT_FILE_LINE;
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

void free_ToDoList(ToDoList **todo)
{
   ToDoList *temp_todo, *temp_todo_next;
   
   for (temp_todo = *todo; temp_todo; temp_todo=temp_todo_next) {
      free_ToDo(&(temp_todo->mtodo.todo));
      temp_todo_next = temp_todo->next;
      free(temp_todo);
   }
   *todo = NULL;
}

int get_todo_app_info(struct ToDoAppInfo *ai)
{
   FILE *in;
   int num;
   unsigned int rec_size;
   char *buf;
   RawDBHeader rdbh;
   DBHeader dbh;

   in = open_file("ToDoDB.pdb", "r");
   if (!in) {
      printf("Error opening ToDoDB.pdb\n");
      return -1;
   }
   fread(&rdbh, sizeof(RawDBHeader), 1, in);
   if (feof(in)) {
      fclose(in);
      printf("Error reading ToDoDB.pdb\n");
      return -1;
   }
   raw_header_to_header(&rdbh, &dbh);

   fseek(in, dbh.app_info_offset, SEEK_SET);
   rec_size = 300;
   buf=malloc(rec_size);
   if (!buf) {
      fclose(in);
      return -1;
   }
   num = fread(buf, 1, rec_size, in);
   if (feof(in)) {
      fclose(in);
      printf("sd Error reading ToDoDB.pdb\n");
      return -1;
   }
   unpack_ToDoAppInfo(ai, buf, rec_size);
   free(buf);
   
   return 0;
}

int get_todos(ToDoList **todo_list)
{
   FILE *in, *pc_in;
//   char db_name[34];
//   char filler[100];
   char *buf;
//   unsigned char char_num_records[4];
//   unsigned char char_ai_offset[4];//app info offset
   int num_records, i, num, r;
   unsigned int offset, next_offset, rec_size;
//   unsigned char c;
   long fpos;  //file position indicator
   unsigned char attrib;
   unsigned int unique_id;
   mem_rec_header *mem_rh, *temp_mem_rh;
   record_header rh;
   RawDBHeader rdbh;
   DBHeader dbh;
   struct ToDo todo;
   //struct AddressAppInfo ai;
   ToDoList *temp_todo_list;
   MyToDo mtodo;

   mem_rh = NULL;
   *todo_list=NULL;

   in = open_file("ToDoDB.pdb", "r");
   if (!in) {
      printf("Error opening ToDoDB.pdb\n");
      return -1;
   }
   //Read the database header
   fread(&rdbh, sizeof(RawDBHeader), 1, in);
   if (feof(in)) {
      printf("Error opening ToDoDB.pdb\n");
      return -1;
   }
   raw_header_to_header(&rdbh, &dbh);
   
   //printf("db_name = %s\n", dbh.db_name);
   //printf("num records = %d\n", dbh.number_of_records);
   //printf("app info offset = %d\n", dbh.app_info_offset);

   //fread(filler, 2, 1, in);

   //Read each record entry header
   num_records = dbh.number_of_records;
   //printf("sizeof(record_header)=%d\n",sizeof(record_header));
   for (i=1; i<num_records+1; i++) {
      fread(&rh, sizeof(record_header), 1, in);
      offset = ((rh.Offset[0]*256+rh.Offset[1])*256+rh.Offset[2])*256+rh.Offset[3];
      //printf("record header %u offset = %u\n",i, offset);
      //printf("       attrib %d\n",rh.attrib);
      //printf("    unique_ID %d %d %d = ",rh.unique_ID[0],rh.unique_ID[1],rh.unique_ID[2]);
      //printf("%d\n",(rh.unique_ID[0]*256+rh.unique_ID[1])*256+rh.unique_ID[2]);
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
   
   while(!feof(in)) {
      fpos = ftell(in);
      find_next_offset(mem_rh, fpos, &next_offset, &attrib, &unique_id);
      //next_offset += 223;
      rec_size = next_offset - fpos;
      //printf("rec_size = %u\n",rec_size);
      //printf("fpos,next_offset = %u %u\n",fpos,next_offset);
      //printf("----------\n");
      if (feof(in)) break;
      buf = malloc(rec_size);
      if (!buf) break;
      num = fread(buf, 1, rec_size, in);

      unpack_ToDo(&todo, buf, rec_size);
      free(buf);
      temp_todo_list = malloc(sizeof(ToDoList));
      memcpy(&(temp_todo_list->mtodo.todo), &todo, sizeof(struct ToDo));
      //temp_address_list->ma.a = temp_a;
      temp_todo_list->mtodo.rt = PALM_REC;
      temp_todo_list->mtodo.attrib = attrib;
      temp_todo_list->mtodo.unique_id = unique_id;
      temp_todo_list->next = *todo_list;
      *todo_list = temp_todo_list;
   }
   fclose(in);
   free_mem_rec_header(&mem_rh);

   //
   //Get the appointments out of the PC database
   //
   pc_in = open_file("ToDoDB.pc", "r");
   if (pc_in==NULL) {
      return 0;
   }
   //r = pc_datebook_read_file_header(pc_in);
   while(!feof(pc_in)) {
      r = pc_todo_read_next_rec(pc_in, &mtodo);
      if (r==TODO_EOF) break;
      if ((mtodo.rt!=DELETED_PC_REC)
	  &&(mtodo.rt!=DELETED_PALM_REC)
	  &&(mtodo.rt!=DELETED_DELETED_PALM_REC)) {
	 temp_todo_list = malloc(sizeof(ToDoList));
	 memcpy(&(temp_todo_list->mtodo), &mtodo, sizeof(MyToDo));
	 temp_todo_list->next = *todo_list;
	 *todo_list = temp_todo_list;

	 //temp_address_list->ma.attrib=0;
      } else {
	 //this doesnt really free it, just the string pointers
	 free_ToDo(&(mtodo.todo));
      }
      if (mtodo.rt==DELETED_PALM_REC) {
	 for (temp_todo_list = *todo_list; temp_todo_list;
	      temp_todo_list=temp_todo_list->next) {
	    if (temp_todo_list->mtodo.unique_id == mtodo.unique_id) {
	       temp_todo_list->mtodo.rt = mtodo.rt;
	    }
	 }
      }
   }
   fclose(pc_in);

   todo_sort(todo_list);
}

/* todo.c
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
#include <pi-todo.h>
#include <pi-dlp.h>
#include "utils.h"
#include "log.h"
#include "todo.h"
#include "prefs.h"

#if defined(WITH_JAPANESE)
#include "japanese.h"
#endif

#define TODO_EOF 7

static struct ToDoAppInfo *glob_Ptodo_app_info;
/*
 * sort by:
 * priority, due date
 * due date, priority
 * category, priority
 * category, due date
 */
int todo_compare(const void *v1, const void *v2)
{
   time_t t1, t2;
   int r;
   int cat1, cat2;
   ToDoList **todol1, **todol2;
   struct ToDo *todo1, *todo2;
   int sort_by_priority;

   todol1=(ToDoList **)v1;
   todol2=(ToDoList **)v2;

   todo1=&((*todol1)->mtodo.todo);
   todo2=&((*todol2)->mtodo.todo);

   sort_by_priority = glob_Ptodo_app_info->sortByPriority;

   cat1 = (*todol1)->mtodo.attrib & 0x0F;
   cat2 = (*todol2)->mtodo.attrib & 0x0F;
   
   if (sort_by_priority == 0) {
      /* due date, priority */
      if ( !(todo1->indefinite) && (todo2->indefinite) ) {
	 return 1;
      }
      if ( (todo1->indefinite) && !(todo2->indefinite) ) {
	 return -1;
      }
      if ( !(todo1->indefinite) && !(todo2->indefinite) ) {
	 t1 = mktime(&(todo1->due));
	 t2 = mktime(&(todo2->due));
	 if ( t1 < t2 ) {
	    return 1;
	 }
	 if ( t1 > t2 ) {
	    return -1;
	 }
      }
      if ( (todo1->priority) < (todo2->priority) ) {
	 return 1;
      }
      if ( (todo1->priority) > (todo2->priority) ) {
	 return -1;
      }
   }

   if (sort_by_priority == 1) {
      /* priority, due date */
      if ( (todo1->priority) < (todo2->priority) ) {
	 return 1;
      }
      if ( (todo1->priority) > (todo2->priority) ) {
	 return -1;
      }
      if ( (todo1->indefinite) && (todo2->indefinite) ) {
	 return 0;
      }
      if ( !(todo1->indefinite) && (todo2->indefinite) ) {
	 return 1;
      }
      if ( (todo1->indefinite) && !(todo2->indefinite) ) {
	 return -1;
      }
      t1 = mktime(&(todo1->due));
      t2 = mktime(&(todo2->due));
      if ( t1 < t2 ) {
	 return 1;
      }
      if ( t1 > t2 ) {
	 return -1;
      }
   }

   if (sort_by_priority == 2) {
      /* category, priority */
      r = strcmp(glob_Ptodo_app_info->category.name[cat1],
		 glob_Ptodo_app_info->category.name[cat2]);
      if (r) {
	 return -r;
      }
      if ( (todo1->priority) < (todo2->priority) ) {
	 return 1;
      }
      if ( (todo1->priority) > (todo2->priority) ) {
	 return -1;
      }
   }

   if (sort_by_priority == 3) {
      /* category, due date */
      r = strcmp(glob_Ptodo_app_info->category.name[cat1],
		 glob_Ptodo_app_info->category.name[cat2]);
      if (r) {
	 return -r;
      }
      if ( (todo1->indefinite) && (todo2->indefinite) ) {
	 return 0;
      }
      if ( !(todo1->indefinite) && (todo2->indefinite) ) {
	 return 1;
      }
      if ( (todo1->indefinite) && !(todo2->indefinite) ) {
	 return -1;
      }
      t1 = mktime(&(todo1->due));
      t2 = mktime(&(todo2->due));
      if ( t1 < t2 ) {
	 return 1;
      }
      if ( t1 > t2 ) {
	 return -1;
      }
   }

   return 0;
}

int todo_sort(ToDoList **todol, int sort_order)
{
   ToDoList *temp_todol;
   ToDoList **sort_todol;
   struct ToDoAppInfo ai;
   int count, i;

   /* Count the entries in the list */
   for (count=0, temp_todol=*todol; temp_todol; temp_todol=temp_todol->next, count++) {
      ;
   }

   if (count<2) {
      /* We don't have to sort less than 2 items */
      return 0;
   }
   
   get_todo_app_info(&ai);

   glob_Ptodo_app_info = &ai;

   /* Allocate an array to be qsorted */
   sort_todol = calloc(count, sizeof(ToDoList *));
   if (!sort_todol) {
      jpilot_logf(LOG_WARN, _("Out of Memory\n"));
      return 0;
   }
   
   /* Set our array to be a list of pointers to the nodes in the linked list */
   for (i=0, temp_todol=*todol; temp_todol; temp_todol=temp_todol->next, i++) {
      sort_todol[i] = temp_todol;
   }

   /* qsort them */
   qsort(sort_todol, count, sizeof(ToDoList *), todo_compare);
   
   /* Put the linked list in the order of the array */
   if (sort_order==SORT_ASCENDING) {
      for (i=count-1; i>0; i--) {
	 sort_todol[i]->next=sort_todol[i-1];
      }
      sort_todol[0]->next = NULL;
      *todol = sort_todol[count-1];
   } else {
      /* Descending order */
      sort_todol[count-1]->next = NULL;
      for (i=count-1; i; i--) {
	 sort_todol[i-1]->next=sort_todol[i];
      }
      *todol = sort_todol[0];
   }
   
   free(sort_todol);

   return 0;
}

static int pc_todo_read_next_rec(FILE *in, MyToDo *mtodo)
{
   PCRecordHeader header;
   int rec_len, num;
   char *record;
   
   if (feof(in)) {
      return TODO_EOF;
   }
   num = fread(&header, sizeof(header), 1, in);
   if (num != 1) {
      if (ferror(in)) {
	 jpilot_logf(LOG_WARN, _("Error reading %s\n"), "ToDoDB.pc 1");
	 return TODO_EOF;
      }
      if (feof(in)) {
	 return TODO_EOF;
      }      
   }
   rec_len = header.rec_len;
   mtodo->rt = header.rt;
   mtodo->attrib = header.attrib;
   mtodo->unique_id = header.unique_id;
   record = malloc(rec_len);
   if (!record) {
      jpilot_logf(LOG_WARN, _("Out of memory\n"));
      return TODO_EOF;
   }
   num = fread(record, rec_len, 1, in);
   if (num != 1) {
      if (ferror(in)) {
	 jpilot_logf(LOG_WARN, _("Error reading %s\n"), "ToDoDB.pc 2");
	 free(record);
	 return TODO_EOF;
      }
   }
   num = unpack_ToDo(&(mtodo->todo), record, rec_len);
   free(record);
   if (num<=0) {
      jpilot_logf(LOG_DEBUG, "unpack_ToDo %s\n", _("failed"));
      return TODO_EOF;
   }
   return 0;
}

int pc_todo_write(struct ToDo *todo, PCRecType rt, unsigned char attrib,
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
   
   out = open_file("ToDoDB.pc", "a");
   if (!out) {
      jpilot_logf(LOG_WARN, _("Error opening %s\n"), "ToDoDB.pc");
      return -1;
   }
   rec_len = pack_ToDo(todo, record, 65535);
   if (rec_len<=0) {
      jpilot_logf(LOG_WARN, "pack_ToDo %s\n", _("failed"));
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

   bzero(ai, sizeof(*ai));
   in = open_file("ToDoDB.pdb", "r");
   if (!in) {
      jpilot_logf(LOG_WARN, _("Error opening %s\n"), "ToDoDB.pdb");
      return -1;
   }
   num = fread(&rdbh, sizeof(RawDBHeader), 1, in);
   if (num != 1) {
      if (ferror(in)) {
	 fclose(in);
	 jpilot_logf(LOG_WARN, _("Error reading %s\n"), "ToDoDB.pdb 2");
	 return -1;
      }
      if (feof(in)) {
	 fclose(in);
	 return TODO_EOF;
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
	 jpilot_logf(LOG_WARN, _("Error reading %s\n"), "ToDoDB.pdb 3");
	 return -1;
      }
   }
   num = unpack_ToDoAppInfo(ai, buf, rec_size);
   if (num <= 0) {
      jpilot_logf(LOG_WARN, "unpack_ToDoAppInfo %s\n", _("failed"));
      free(buf);
      fclose(in);
      return -1;
   }
#if defined(WITH_JAPANESE)
   /* Convert 'Category name' to EUC Japanese Kanji code */
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


int get_todos(ToDoList **todo_list, int sort_order)
{
   return get_todos2(todo_list, sort_order, 1, 1, CATEGORY_ALL);
}
/* 
 * sort_order: 0=descending,  1=ascending
 * modified and deleted, 0 for no, 1 for yes, 2 for use prefs
 */
int get_todos2(ToDoList **todo_list, int sort_order,
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
   struct ToDo todo;
   ToDoList *temp_todo_list;
   ToDoList *tal, *next_al, *prev_al;
   MyToDo mtodo;
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
   *todo_list=NULL;
   recs_returned = 0;

   in = open_file("ToDoDB.pdb", "r");
   if (!in) {
      jpilot_logf(LOG_WARN, _("Error opening %s\n"), "ToDoDB.pdb");
      return -1;
   }
   /*Read the database header */
   num = fread(&rdbh, sizeof(RawDBHeader), 1, in);
   if (num != 1) {
      if (ferror(in)) {
	 jpilot_logf(LOG_WARN, _("Error reading %s\n"), "ToDoDB.pdb");
	 fclose(in);
	 return -1;
      }
      if (feof(in)) {
	 return TODO_EOF;
      }      
   }
   raw_header_to_header(&rdbh, &dbh);
#ifdef JPILOT_DEBUG
   jpilot_logf(LOG_DEBUG, "db_name = %s\n", dbh.db_name);
   jpilot_logf(LOG_DEBUG, "num records = %d\n", dbh.number_of_records);
   jpilot_logf(LOG_DEBUG, "app info offset = %d\n", dbh.app_info_offset);
#endif

   out_of_order = 0;
   prev_offset = 0;

   /*Read each record entry header */
   num_records = dbh.number_of_records;
   for (i=1; i<num_records+1; i++) {
      num = fread(&rh, sizeof(record_header), 1, in);
      if (num != 1) {
	 if (ferror(in)) {
	    jpilot_logf(LOG_WARN, _("Error reading %s\n"), "ToDoDB.pdb 4");
	    break;
	 }
	 if (feof(in)) {
	    return TODO_EOF;
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
	 if (!buf) break;
	 num = fread(buf, rec_size, 1, in);
	 if ((num != 1)) {
	    if (ferror(in)) {
	       jpilot_logf(LOG_WARN, _("Error reading %s\n"), "ToDoDB.pdb 5");
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

	 num = unpack_ToDo(&todo, buf, rec_size);
	 free(buf);
	 if (num<=0) {
	    jpilot_logf(LOG_DEBUG, "unpack_ToDo %s\n", _("failed"));
	    continue;
	 }
#if defined(WITH_JAPANESE)
      /* Convert to EUC Japanese Kanji code */
      if (todo.description != NULL)
         Sjis2Euc(todo.description, 65536);
      if (todo.note != NULL)
         Sjis2Euc(todo.note, 65536);
#endif
	 temp_todo_list = malloc(sizeof(ToDoList));
	 if (!temp_todo_list) {
	    jpilot_logf(LOG_WARN, _("Out of memory\n"));
	    break;
	 }
	 memcpy(&(temp_todo_list->mtodo.todo), &todo, sizeof(struct ToDo));
	 /*temp_address_list->ma.a = temp_a; */
	 temp_todo_list->app_type = TODO;
	 temp_todo_list->mtodo.rt = PALM_REC;
	 temp_todo_list->mtodo.attrib = attrib;
	 temp_todo_list->mtodo.unique_id = unique_id;
	 temp_todo_list->next = *todo_list;
	 *todo_list = temp_todo_list;
	 recs_returned++;
      }
   }
   fclose(in);
   free_mem_rec_header(&mem_rh);

   /* */
   /*Get the appointments out of the PC database */
   /* */
   pc_in = open_file("ToDoDB.pc", "r");
   if (pc_in==NULL) {
      jpilot_logf(LOG_DEBUG, "open_file failed\n");
      return 0;
   }
   /*r = pc_datebook_read_file_header(pc_in); */
   while(!feof(pc_in)) {
      r = pc_todo_read_next_rec(pc_in, &mtodo);
      if (r==TODO_EOF) break;
      if (r<0) break;
      if ((mtodo.rt!=DELETED_PC_REC)
	  &&(mtodo.rt!=DELETED_PALM_REC)
	  &&(mtodo.rt!=MODIFIED_PALM_REC)
	  &&(mtodo.rt!=DELETED_DELETED_PALM_REC)) {
	 /* check category */
	 if ( ((mtodo.attrib & 0x0F) != category) &&
	     category != CATEGORY_ALL) {
	    continue;
	 }
	 temp_todo_list = malloc(sizeof(ToDoList));
	 if (!temp_todo_list) {
	    jpilot_logf(LOG_WARN, _("Out of memory\n"));
	    break;
	 }
	 memcpy(&(temp_todo_list->mtodo), &mtodo, sizeof(MyToDo));
	 temp_todo_list->app_type = TODO;
	 temp_todo_list->next = *todo_list;
	 *todo_list = temp_todo_list;
	 recs_returned++;

	 /*temp_address_list->ma.attrib=0; */
      } else {
	 /*this doesnt really free it, just the string pointers */
	 free_ToDo(&(mtodo.todo));
      }
      if ( ((mtodo.rt==DELETED_PALM_REC) && (keep_deleted)) || 
 	  ((mtodo.rt==MODIFIED_PALM_REC) && (keep_modified)) ) {
	 for (temp_todo_list = *todo_list; temp_todo_list;
	      temp_todo_list=temp_todo_list->next) {
	    if (temp_todo_list->mtodo.unique_id == mtodo.unique_id) {
	       temp_todo_list->mtodo.rt = mtodo.rt;
	    }
	 }
      } else if ( ((mtodo.rt==DELETED_PALM_REC) && (!keep_deleted)) || 
 		 ((mtodo.rt==MODIFIED_PALM_REC) && (!keep_modified)) ) {
 	 for (prev_al=NULL, tal=*todo_list; tal; tal = next_al) {
 	    if (tal->mtodo.unique_id == mtodo.unique_id) {
 	       /* Remove it from this list */
 	       if (prev_al) {
 		  prev_al->next=tal->next;
 	       } else {
 		  *todo_list=tal->next;
 	       }
 	       next_al=tal->next;
 	       free_ToDo(&(tal->mtodo.todo));
 	       free(tal);
 	    } else {
 	       prev_al=tal;
 	       next_al=tal->next;
 	    }
 	 }
      }
   }

   fclose(pc_in);

   todo_sort(todo_list, sort_order);

   jpilot_logf(LOG_DEBUG, "Leaving get_todos\n");

   return recs_returned;
}

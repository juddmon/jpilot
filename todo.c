/* todo.c
 * A module of J-Pilot http://jpilot.org
 * 
 * Copyright (C) 1999-2001 by Judd Montgomery
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

#include "config.h"
#include "i18n.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pi-source.h>
#include <pi-socket.h>
#include <pi-todo.h>
#include <pi-dlp.h>
#include "utils.h"
#include "log.h"
#include "todo.h"
#include "prefs.h"
#include "libplugin.h"
#include "password.h"

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
      /* If all else fails sort alphabetically */
      if ((todo1->description) && (todo2->description)) {
	 return -(strcoll(todo1->description,todo2->description));
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
      /* If all else fails sort alphabetically */
      if ((todo1->description) && (todo2->description)) {
	 return -(strcoll(todo1->description,todo2->description));
      }
   }

   if (sort_by_priority == 2) {
      /* category, priority */
      r = strcoll(glob_Ptodo_app_info->category.name[cat1],
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
      /* If all else fails sort alphabetically */
      if ((todo1->description) && (todo2->description)) {
	 return -(strcoll(todo1->description,todo2->description));
      }
   }

   if (sort_by_priority == 3) {
      /* category, due date */
      r = strcoll(glob_Ptodo_app_info->category.name[cat1],
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
      /* If all else fails sort alphabetically */
      if ((todo1->description) && (todo2->description)) {
	 return -(strcoll(todo1->description,todo2->description));
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
      jpilot_logf(LOG_WARN, "todo_sort(): Out of Memory\n");
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

/*
 * This function just checks some todo fields to make sure they are valid.
 * It truncates the description and note fields if necessary.
 */
void pc_todo_validate_correct(struct ToDo *todo)
{
   if (todo->description) {
      if ((strlen(todo->description)+1 > MAX_TODO_DESC_LEN)) {
	 jpilot_logf(LOG_WARN, "%s\n", todo->description);
	 jpilot_logf(LOG_WARN, _("Warning ToDo description too long, truncating to %d\n"), MAX_TODO_DESC_LEN-1);
	 todo->description[MAX_TODO_DESC_LEN-1]='\0';
      }
   }
   if (todo->note) {
      if ((strlen(todo->note)+1 > MAX_TODO_NOTE_LEN)) {
	 jpilot_logf(LOG_WARN, "%s\n", todo->note);
	 jpilot_logf(LOG_WARN, _("Warning ToDo note too long, truncating to %d\n"), MAX_TODO_NOTE_LEN-1);
	 todo->note[MAX_TODO_NOTE_LEN-1]='\0';
      }
   }
}

int pc_todo_write(struct ToDo *todo, PCRecType rt, unsigned char attrib,
		  unsigned int *unique_id)
{
   char record[65536];
   int rec_len;
   buf_rec br;
   long char_set;
#ifdef ENABLE_MANANA
   long ivalue;
#endif

   get_pref(PREF_CHAR_SET, &char_set, NULL);
   if (char_set != CHAR_SET_LATIN1) {
      if (todo->description) charset_j2p(todo->description, strlen(todo->description)+1, char_set);
      if (todo->note) charset_j2p(todo->note, strlen(todo->note)+1, char_set);
   }

   pc_todo_validate_correct(todo);
   rec_len = pack_ToDo(todo, record, 65535);
   if (!rec_len) {
      PRINT_FILE_LINE;
      jpilot_logf(LOG_WARN, "pack_ToDo %s\n", _("error"));
      return -1;
   }
   br.rt=rt;
   br.attrib = attrib;
   br.buf = record;
   br.size = rec_len;
   /* Keep unique ID intact */
   if (unique_id) {
      br.unique_id = *unique_id;
   } else {
      br.unique_id = 0;
   }

   *unique_id = br.unique_id;
#ifdef ENABLE_MANANA
   get_pref(PREF_MANANA_MODE, &ivalue, NULL);
   if (ivalue) {
      jp_pc_write("MañanaDB", &br);
   } else {
      jp_pc_write("ToDoDB", &br);
   }
#else
   jp_pc_write("ToDoDB", &br);
#endif
   if (unique_id) {
      *unique_id = br.unique_id;
   }

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
   int num, i, r;
   unsigned int rec_size;
   unsigned char *buf;
   long char_set;
#ifdef ENABLE_MANANA
   long ivalue;
#endif

   bzero(ai, sizeof(*ai));
   buf=NULL;
   /* Put at least one entry in there */
   strcpy(ai->category.name[0], "Unfiled");

#ifdef ENABLE_MANANA
   get_pref(PREF_MANANA_MODE, &ivalue, NULL);                                  
   if (ivalue) {
      r = jp_get_app_info("MañanaDB", &buf, &rec_size);
   } else {
      r = jp_get_app_info("ToDoDB", &buf, &rec_size);
   }
#else
   r = jp_get_app_info("ToDoDB", &buf, &rec_size);
#endif
   if (r<0) {
      if (buf) {
	 free(buf);
      }
      return -1;
   }
   num = unpack_ToDoAppInfo(ai, buf, rec_size);
   if (buf) {
      free(buf);
   }
   if (num <= 0) {
#ifdef ENABLE_MANANA
      if (ivalue) {
	 jpilot_logf(LOG_WARN, _("Error reading %s\n"), "MañanaDB.pdb");
      } else {
	 jpilot_logf(LOG_WARN, _("Error reading %s\n"), "ToDoDB.pdb");
      }
#else
      jpilot_logf(LOG_WARN, _("Error reading %s\n"), "ToDoDB.pdb");
#endif
      return -1;
   }

   get_pref(PREF_CHAR_SET, &char_set, NULL);
   if (char_set != CHAR_SET_LATIN1) {
      for (i = 0; i < 16; i++) {
	 if (ai->category.name[i][0] != '\0') {
	    charset_p2j(ai->category.name[i], 16, char_set);
	 }
      }
   }

   return 0;
}

int get_todos(ToDoList **todo_list, int sort_order)
{
   return get_todos2(todo_list, sort_order, 1, 1, 1, 1, CATEGORY_ALL);
}
/* 
 * sort_order: 0=descending,  1=ascending
 * modified, deleted and private, completed:
 *  0 for no, 1 for yes, 2 for use prefs
 */
int get_todos2(ToDoList **todo_list, int sort_order,
	       int modified, int deleted, int privates, int completed,
	       int category)
{
   GList *records;
   GList *temp_list;
   int recs_returned, i, num;
   struct ToDo todo;
   ToDoList *temp_todo_list;
   long keep_modified, keep_deleted, hide_completed;
   int keep_priv;
   buf_rec *br;
   long char_set;
#ifdef ENABLE_MANANA
   long ivalue;
#endif

   jpilot_logf(LOG_DEBUG, "get_todos2()\n");
   if (modified==2) {
      get_pref(PREF_SHOW_MODIFIED, &keep_modified, NULL);
   } else {
      keep_modified = modified;
   }
   if (deleted==2) {
      get_pref(PREF_SHOW_DELETED, &keep_deleted, NULL);
   } else {
      keep_deleted = deleted;
   }
   if (privates==2) {
      keep_priv = show_privates(GET_PRIVATES, NULL);
   } else {
      keep_priv = privates;
   }
   if (completed==2) {
      get_pref(PREF_HIDE_COMPLETED, &hide_completed, NULL);
   } else {
      hide_completed = !completed;
   }

   *todo_list=NULL;
   recs_returned = 0;

#ifdef ENABLE_MANANA
   get_pref(PREF_MANANA_MODE, &ivalue, NULL);
   if (ivalue) {
      num = jp_read_DB_files("MañanaDB", &records);
   } else {
      num = jp_read_DB_files("ToDoDB", &records);
   }
#else
   num = jp_read_DB_files("ToDoDB", &records);
#endif
   /* Go to first entry in the list */
   for (temp_list = records; temp_list; temp_list = temp_list->prev) {
      records = temp_list;
   }
   for (i=0, temp_list = records; temp_list; temp_list = temp_list->next, i++) {
      if (temp_list->data) {
	 br=temp_list->data;
      } else {
	 continue;
      }
      if (!br->buf) {
	 continue;
      }

      if ( ((br->rt==DELETED_PALM_REC) && (!keep_deleted)) ||
	  ((br->rt==MODIFIED_PALM_REC) && (!keep_modified)) ) {
	 continue;
      }
      if ((keep_priv != SHOW_PRIVATES) && 
	  (br->attrib & dlpRecAttrSecret)) {
	 continue;
      }

      num = unpack_ToDo(&todo, br->buf, br->size);

      if (num <= 0) {
	 continue;
      }

      if ( ((br->attrib & 0x0F) != category) && category != CATEGORY_ALL) {
	 continue;
      }

      if (hide_completed && todo.complete) {
	 continue;
      }

      get_pref(PREF_CHAR_SET, &char_set, NULL);
      if (todo.description) charset_p2j(todo.description, strlen(todo.description)+1, char_set);
      if (todo.note) charset_p2j(todo.note, strlen(todo.note)+1, char_set);

      temp_todo_list = malloc(sizeof(ToDoList));
      if (!temp_todo_list) {
	 jpilot_logf(LOG_WARN, "get_todos2(): Out of memory\n");
	 break;
      }
      memcpy(&(temp_todo_list->mtodo.todo), &todo, sizeof(struct ToDo));
      temp_todo_list->app_type = TODO;
      temp_todo_list->mtodo.rt = br->rt;
      temp_todo_list->mtodo.attrib = br->attrib;
      temp_todo_list->mtodo.unique_id = br->unique_id;
      temp_todo_list->next = *todo_list;
      *todo_list = temp_todo_list;
      recs_returned++;
   }

   jp_free_DB_records(&records);

   todo_sort(todo_list, sort_order);

   jpilot_logf(LOG_DEBUG, "Leaving get_todos2()\n");

   return recs_returned;
}

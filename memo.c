/* memo.c
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
#include <stdio.h>
#include <ctype.h>
#include <pi-source.h>
#include <pi-socket.h>
#include <pi-memo.h>
#include <pi-dlp.h>
#include "memo.h"
#include "utils.h"
#include "log.h"
#include "prefs.h"
#include "libplugin.h"
#include "password.h"

#define MEMO_EOF 7

int memo_compare(const void *v1, const void *v2)
{
   MemoList **memol1, **memol2;
   char str1[52], str2[52];
   struct Memo *m1, *m2;
   int i;

   memol1=(MemoList **)v1;
   memol2=(MemoList **)v2;
   
   m1=&((*memol1)->mmemo.memo);
   m2=&((*memol2)->mmemo.memo);

   multibyte_safe_strncpy(str1, m1->text, 50);
   multibyte_safe_strncpy(str2, m2->text, 50);
   str1[50]='\0';
   str2[50]='\0';

   /* lower case the strings for a better compare */
   for (i=strlen(str1)-1; i >= 0; i--) {
      str1[i] = tolower(str1[i]);
   }
   for (i=strlen(str2)-1; i >= 0; i--) {
      str2[i] = tolower(str2[i]);
   }

   return strcoll(str2, str1);
}

int memo_sort(MemoList **memol, int sort_order)
{
   struct MemoAppInfo memo_ai;
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
      jpilot_logf(LOG_WARN, "memo_sort(): Out of Memory\n");
      return 0;
   }
   
   /* Set our array to be a list of pointers to the nodes in the linked list */
   for (i=0, temp_memol=*memol; temp_memol; temp_memol=temp_memol->next, i++) {
      sort_memol[i] = temp_memol;
   }

   get_memo_app_info(&memo_ai);
   if (memo_ai.sortByAlpha==1) {
      /* qsort them */
      qsort(sort_memol, count, sizeof(MemoList *), memo_compare);
   }

   /* Put the linked list in the order of the array */
   if (sort_order==SORT_ASCENDING) {
      for (i=count-1; i>0; i--) {
	 sort_memol[i]->next=sort_memol[i-1];
      }
      sort_memol[0]->next = NULL;
      *memol = sort_memol[count-1];
   } else {
      /* Descending order */
      sort_memol[count-1]->next = NULL;
      for (i=count-1; i; i--) {
	 sort_memol[i-1]->next=sort_memol[i];
      }
      *memol = sort_memol[0];
   }
   
   free(sort_memol);

   return 0;
}

int pc_memo_write(struct Memo *memo, PCRecType rt, unsigned char attrib,
		  unsigned int *unique_id)
{
   char record[65536];
   int rec_len;
   buf_rec br;
   long ivalue;
   long char_set;

   get_pref(PREF_CHAR_SET, &char_set, NULL);
   if (char_set != CHAR_SET_ENGLISH) {
      if (memo->text) charset_j2p(memo->text, strlen(memo->text)+1, char_set);
   }

   rec_len = pack_Memo(memo, record, 65535);
   if (!rec_len) {
      PRINT_FILE_LINE;
      jpilot_logf(LOG_WARN, "pack_Memo %s\n", _("error"));
      return -1;
   }
   br.rt=rt;
   br.attrib = attrib;
   br.buf = record;
   br.size = rec_len;
   
   get_pref(PREF_MEMO32_MODE, &ivalue, NULL);
   if (ivalue) {
      jp_pc_write("Memo32DB", &br);
   } else {
      jp_pc_write("MemoDB", &br);
   }
   *unique_id = br.unique_id;
   
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
   int num,i;
   unsigned int rec_size;
   unsigned char *buf;
   long char_set;
   long ivalue;

   bzero(ai, sizeof(*ai));

   get_pref(PREF_MEMO32_MODE, &ivalue, NULL);
   if (ivalue) {
      jp_get_app_info("Memo32DB", &buf, &rec_size);
   } else {
      jp_get_app_info("MemoDB", &buf, &rec_size);
   }
   num = unpack_MemoAppInfo(ai, buf, rec_size);
   if (buf) {
      free(buf);
   }
   if (num <= 0) {
      jpilot_logf(LOG_WARN, _("Error reading"), "MemoDB.pdb");
      return -1;
   }
	 
   get_pref(PREF_CHAR_SET, &char_set, NULL);
   if (char_set != CHAR_SET_ENGLISH) {
      for (i = 0; i < 16; i++) {
	 if (ai->category.name[i][0] != '\0') {
	    charset_p2j(ai->category.name[i], 16, char_set);
	 }
      }
   }

   return 0;
}

int get_memos(MemoList **memo_list, int sort_order)
{
   return get_memos2(memo_list, sort_order, 1, 1, 1, CATEGORY_ALL);
}
/* 
 * sort_order: 0=descending, 1=ascending (memos are sorted if set in pdb file)
 * modified, deleted and private, 0 for no, 1 for yes, 2 for use prefs
 */
int get_memos2(MemoList **memo_list, int sort_order,
 	       int modified, int deleted, int privates, int category)
{
   GList *records;
   GList *temp_list;
   int recs_returned, i, num;
   struct Memo memo;
   MemoList *temp_memo_list;
   long keep_modified, keep_deleted;
   int keep_priv;
   long char_set;
   long ivalue;
   buf_rec *br;
  
   jpilot_logf(LOG_DEBUG, "get_memos2()\n");
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

   *memo_list=NULL;
   recs_returned = 0;

   get_pref(PREF_MEMO32_MODE, &ivalue, NULL);
   if (ivalue) {
      num = jp_read_DB_files("Memo32DB", &records);
   } else {
      num = jp_read_DB_files("MemoDB", &records);
   }
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

      num = unpack_Memo(&memo, br->buf, br->size);

      if (num <= 0) {
	 continue;
      }

      if ( ((br->attrib & 0x0F) != category) && category != CATEGORY_ALL) {
	 continue;
      }
      get_pref(PREF_CHAR_SET, &char_set, NULL);
      if (memo.text) charset_p2j(memo.text, strlen(memo.text)+1, char_set);

      temp_memo_list = malloc(sizeof(MemoList));
      if (!temp_memo_list) {
	 jpilot_logf(LOG_WARN, "get_memos2(): Out of memory\n");
	 break;
      }
      memcpy(&(temp_memo_list->mmemo.memo), &memo, sizeof(struct Memo));
      temp_memo_list->app_type = MEMO;
      temp_memo_list->mmemo.rt = br->rt;
      temp_memo_list->mmemo.attrib = br->attrib;
      temp_memo_list->mmemo.unique_id = br->unique_id;
      temp_memo_list->next = *memo_list;
      *memo_list = temp_memo_list;
      recs_returned++;
   }

   jp_free_DB_records(&records);

   memo_sort(memo_list, sort_order);

   jpilot_logf(LOG_DEBUG, "Leaving get_memos2()\n");

   return recs_returned;
}

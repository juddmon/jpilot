/*******************************************************************************
 * memo.c
 * A module of J-Pilot http://jpilot.org
 *
 * Copyright (C) 1999-2002 by Judd Montgomery
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
 ******************************************************************************/

/********************************* Includes ***********************************/
#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <pi-memo.h>
#include <pi-dlp.h>

#include "memo.h"
#include "i18n.h"
#include "utils.h"
#include "log.h"
#include "prefs.h"
#include "libplugin.h"
#include "password.h"

/********************************* Constants **********************************/

/****************************** Prototypes ************************************/
static int memo_sort(MemoList **memol, int sort_order);

/****************************** Main Code *************************************/
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
   int num, r;
   unsigned int rec_size;
   unsigned char *buf;
   char DBname[32];
   long memo_version;

   memset(ai, 0, sizeof(*ai));
   /* Put at least one entry in there */
   strcpy(ai->category.name[0], "Unfiled");

   get_pref(PREF_MEMO_VERSION, &memo_version, NULL);

   switch (memo_version) {
    case 0:
    default:
      strcpy(DBname, "MemoDB");
      break;
    case 1:
      strcpy(DBname, "MemosDB-PMem");
      break;
    case 2:
      strcpy(DBname, "Memo32DB");
      break;
   }

   r = jp_get_app_info(DBname, &buf, (int*)&rec_size);
   if ((r != EXIT_SUCCESS) || (rec_size<=0)) {
      jp_logf(JP_LOG_WARN, _("%s:%d Error reading application info %s\n"), __FILE__, __LINE__, DBname);
      if (buf) {
         free(buf);
      }
      return EXIT_FAILURE;
   }

   num = unpack_MemoAppInfo(ai, buf, rec_size);
   if (buf) {
      free(buf);
   }
   if ((num<0) || (rec_size<=0)) {
      jp_logf(JP_LOG_WARN, _("Error reading file: %s\n"), DBname);
      return EXIT_FAILURE;
   }

   return EXIT_SUCCESS;
}

int get_memos(MemoList **memo_list, int sort_order)
{
   return get_memos2(memo_list, sort_order, 1, 1, 1, CATEGORY_ALL);
}

/*
 * sort_order: SORT_ASCENDING | SORT_DESCENDING (used to keep pdb sort order
 *                                               but not yet implemented)
 * modified, deleted, private: 0 for no, 1 for yes, 2 for use prefs
 */
int get_memos2(MemoList **memo_list, int sort_order,
               int modified, int deleted, int privates, int category)
{
   GList *records;
   GList *temp_list;
   int recs_returned, num;
   struct Memo memo;
   MemoList *temp_memo_list;
   long keep_modified, keep_deleted;
   int keep_priv;
   long char_set;
   char *newtext;
   long memo_version;
   buf_rec *br;
   pi_buffer_t *RecordBuffer;

   jp_logf(JP_LOG_DEBUG, "get_memos2()\n");
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
      keep_priv = show_privates(GET_PRIVATES);
   } else {
      keep_priv = privates;
   }
   get_pref(PREF_CHAR_SET, &char_set, NULL);

   *memo_list=NULL;
   recs_returned = 0;

   get_pref(PREF_MEMO_VERSION, &memo_version, NULL);

   switch (memo_version) {
    case 0:
    default:
      num = jp_read_DB_files("MemoDB", &records);
      break;
    case 1:
      num = jp_read_DB_files("MemosDB-PMem", &records);
      break;
    case 2:
      num = jp_read_DB_files("Memo32DB", &records);
      break;
   }
   if (-1 == num)
      return 0;

   for (temp_list = records; temp_list; temp_list = temp_list->next) {
      if (temp_list->data) {
         br=temp_list->data;
      } else {
         continue;
      }
      if (!br->buf) {
         continue;
      }

      if ( ((br->rt==DELETED_PALM_REC)  && (!keep_deleted)) ||
           ((br->rt==DELETED_PC_REC)    && (!keep_deleted)) ||
           ((br->rt==MODIFIED_PALM_REC) && (!keep_modified)) ) {
         continue;
      }
      if ((keep_priv != SHOW_PRIVATES) &&
          (br->attrib & dlpRecAttrSecret)) {
         continue;
      }

      RecordBuffer = pi_buffer_new(br->size);
      memcpy(RecordBuffer->data, br->buf, br->size);
      RecordBuffer->used = br->size;

      if (unpack_Memo(&memo, RecordBuffer, memo_v1) == -1) {
         pi_buffer_free(RecordBuffer);
         continue;
      }
      pi_buffer_free(RecordBuffer);

      if ( ((br->attrib & 0x0F) != category) && category != CATEGORY_ALL) {
         free_Memo(&memo);
         continue;
      }
      if (memo.text) {
         newtext = charset_p2newj(memo.text, -1, char_set);
         if (newtext) {
            free(memo.text);
            memo.text = newtext;
         }
      }

      temp_memo_list = malloc(sizeof(MemoList));
      if (!temp_memo_list) {
         jp_logf(JP_LOG_WARN, "get_memos2(): %s\n", _("Out of memory"));
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

   jp_logf(JP_LOG_DEBUG, "Leaving get_memos2()\n");

   return recs_returned;
}

static int memo_compare(const void *v1, const void *v2)
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

static int memo_sort(MemoList **memol, int sort_order)
{
   /* struct MemoAppInfo memo_ai; */
   MemoList *temp_memol;
   MemoList **sort_memol;
   int count, i;

   if (sort_order==SORT_DESCENDING) {
      return EXIT_SUCCESS;
   }

   /* Count the entries in the list */
   for (count=0, temp_memol=*memol; temp_memol; temp_memol=temp_memol->next, count++) {}

   if (count<2) { 
      /* No need to sort 0 or 1 items */
      return EXIT_SUCCESS;
   }

   /* Allocate an array to be qsorted */
   sort_memol = calloc(count, sizeof(MemoList *));
   if (!sort_memol) {
      jp_logf(JP_LOG_WARN, "memo_sort(): %s\n", _("Out of memory"));
      return EXIT_FAILURE;
   }

   /* Set our array to be a list of pointers to the nodes in the linked list */
   for (i=0, temp_memol=*memol; temp_memol; temp_memol=temp_memol->next, i++) {
      sort_memol[i] = temp_memol;
   }

   /* TODO: Restore code when syncing of AppInfo blocks is implemented
   get_memo_app_info(&memo_ai);
   if (memo_ai.sortByAlpha==1) {
      qsort(sort_memol, count, sizeof(MemoList *), memo_compare);
   }
   */
   qsort(sort_memol, count, sizeof(MemoList *), memo_compare);

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

   return EXIT_SUCCESS;
}

/*
 * This function writes to the MemosDB-PMem.pc3 file
 *
 * memo - input - a memo to be written
 * rt - input - type of record to be written
 * attrib - input - attributes of record
 * unique_id - input/output - If unique_id==0 then the palm assigns an ID,
 *  else the unique_id passed in is used.
 */
int pc_memo_write(struct Memo *memo, PCRecType rt, unsigned char attrib,
                  unsigned int *unique_id)
{
   pi_buffer_t *RecordBuffer;
   buf_rec br;
   long char_set;
   long memo_version;

   get_pref(PREF_MEMO_VERSION, &memo_version, NULL);
   
   get_pref(PREF_CHAR_SET, &char_set, NULL);
   if (char_set != CHAR_SET_LATIN1) {
      if (memo->text) {
         charset_j2p(memo->text, strlen(memo->text)+1, char_set);
      }
   }
   RecordBuffer = pi_buffer_new(0);
   if (pack_Memo(memo, RecordBuffer, memo_v1) == -1) {
      PRINT_FILE_LINE;
      jp_logf(JP_LOG_WARN, "pack_Memo %s\n", _("error"));
      return EXIT_FAILURE;
   }
   br.rt=rt;
   br.attrib = attrib;
   br.buf = RecordBuffer->data;
   br.size = RecordBuffer->used;
   /* Keep unique ID intact */
   if (unique_id) {
      br.unique_id = *unique_id;
   } else {
      br.unique_id = 0;
   }

   switch (memo_version) {
    case 0:
    default:
      jp_pc_write("MemoDB", &br);
      break;
    case 1:
      jp_pc_write("MemosDB-PMem", &br);
      break;
    case 2:
      jp_pc_write("Memo32DB", &br);
      break;
   }

   if (unique_id) {
      *unique_id = br.unique_id;
   }

   pi_buffer_free(RecordBuffer);

   return EXIT_SUCCESS;
}


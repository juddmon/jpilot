/* address.c
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
 */
#include "config.h"
#include "i18n.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <pi-source.h>
#include <pi-socket.h>
#include <pi-address.h>
#include <pi-dlp.h>
#include "address.h"
#include "utils.h"
#include "log.h"
#include "prefs.h"
#include "libplugin.h"
#include "password.h"

#define ADDRESS_EOF 7

static int glob_sort_rule;
#define SORT_BY_COMPANY 1
#define SORT_JAPANESE 2
#define SORT_JOS 4
int sort_override=0;

#ifdef JPILOT_DEBUG
int print_address_list(AddressList **al)
{
   AddressList *temp_al, *prev_al, *next;

   for (prev_al=NULL, temp_al=*al; temp_al;
	prev_al=temp_al, temp_al=temp_al->next) {
      jp_logf(JP_LOG_FILE | JP_LOG_STDOUT, "entry[0]=[%s]\n", temp_al->ma.a.entry[0]);
   }
}
#endif

int address_compare(const void *v1, const void *v2)
{
   char *str1, *str2;
   int sort1, sort2, sort3;
   AddressList **al1, **al2;
   struct Address *a1, *a2;
   int i;

   al1=(AddressList **)v1;
   al2=(AddressList **)v2;

   a1=&((*al1)->ma.a);
   a2=&((*al2)->ma.a);

   if (glob_sort_rule & SORT_BY_COMPANY) {
      sort1=2; /*company */
      sort2=0; /*last name */
      sort3=1; /*first name */
   } else {
      sort1=0; /*last name */
      sort2=1; /*first name */
      sort3=2; /*company */
   }
   /* sort_rule: */
     /* SORT_BY_COMPANY */
   /*0 last, first or */
   /*1 company, last */
     /* SORT_JAPANESE */
        /* 0 no use */
        /* 2 use japanese */
     /* SORT_JOS */
        /* 0 no use */
        /* 4 using J-OS */

   str1=str2=NULL;


   if (!(glob_sort_rule & SORT_JAPANESE) | (glob_sort_rule & SORT_JOS)) { /* normal */
      if (a1->entry[sort1] || a1->entry[sort2]) {
	 if (a1->entry[sort1] && a1->entry[sort2]) {
	    if ((str1 = (char *)malloc(strlen(a1->entry[sort1])+strlen(a1->entry[sort2])+1)) == NULL) {
	       return 0;
	    }	      
	    strcpy(str1, a1->entry[sort1]);
	    strcat(str1, a1->entry[sort2]);
	 }
	 if (a1->entry[sort1] && (!a1->entry[sort2])) {
	    if ((str1 = (char *)malloc(strlen(a1->entry[sort1])+1)) == NULL) {
	       return 0;
	    }
	    strcpy(str1, a1->entry[sort1]);
	 }
	 if ((!a1->entry[sort1]) && a1->entry[sort2]) {
	    if ((str1 = (char *)malloc(strlen(a1->entry[sort2])+1)) == NULL) {
	       return 0;
	    }
	    strcpy(str1, a1->entry[sort2]);
	 }
      } else if (a1->entry[sort3]) {
	 if ((str1 = (char *)malloc(strlen(a1->entry[sort3])+1)) == NULL) {
	    return 0;
	 }
	 strcpy(str1, a1->entry[sort3]);
      } else {
	 return -1;
      }

      if (a2->entry[sort1] || a2->entry[sort2]) {
	 if (a2->entry[sort1] && a2->entry[sort2]) {
	    if ((str2 = (char *)malloc(strlen(a2->entry[sort1])+strlen(a2->entry[sort2])+1)) == NULL) {
	       return 0;
	    }
	    strcpy(str2, a2->entry[sort1]);
	    strcat(str2, a2->entry[sort2]);
	 }
	 if (a2->entry[sort1] && (!a2->entry[sort2])) {
	    if ((str2 = (char *)malloc(strlen(a2->entry[sort1])+1)) == NULL) {
	       return 0;
	    }
	    strcpy(str2, a2->entry[sort1]);
	 }
	 if ((!a2->entry[sort1]) && a2->entry[sort2]) {
	    if ((str2 = (char *)malloc(strlen(a2->entry[sort2])+1)) == NULL) {
	       return 0;
	    }	      
	    strcpy(str2, a2->entry[sort2]);
	 }
      } else if (a2->entry[sort3]) {
	 if ((str2 = (char *)malloc(strlen(a2->entry[sort3])+1)) == NULL) {
	    return 0;
	 }
	 strcpy(str2, a2->entry[sort3]);
      } else {
	 free(str1);
	 return 1;
      }
   } else if ((glob_sort_rule & SORT_JAPANESE) && !(glob_sort_rule & SORT_JOS)){
      char *tmp_p1, *tmp_p2, *tmp_p3;
      if (a1->entry[sort1] || a1->entry[sort2]) {
	 if (a1->entry[sort1] && a1->entry[sort2]) {
	    if (!(tmp_p1 = strchr(a1->entry[sort1],'\1'))) tmp_p1=a1->entry[sort1]+1;
	    if (!(tmp_p2 = strchr(a1->entry[sort2],'\1'))) tmp_p2=a1->entry[sort2]+1;
	    if ((str1 = (char *)malloc(strlen(tmp_p1)+strlen(tmp_p2)+1)) == NULL) {
	       return 0;
	    }
	    strcpy(str1, tmp_p1);
	    strcat(str1, tmp_p2);
	 }
	 if (a1->entry[sort1] && (!a1->entry[sort2])) {
	    if (!(tmp_p1 = strchr(a1->entry[sort1],'\1'))) tmp_p1=a1->entry[sort1]+1;
	    if ((str1 = (char *)malloc(strlen(tmp_p1)+1)) == NULL) {
	       return 0;
	    }
	    strcpy(str1, tmp_p1);
	 }
	 if ((!a1->entry[sort1]) && a1->entry[sort2]) {
	    if (!(tmp_p2 = strchr(a1->entry[sort2],'\1'))) tmp_p2=a1->entry[sort2]+1;
	    if ((str1 = (char *)malloc(strlen(tmp_p2)+1)) == NULL) {
	       return 0;
	    }
	    strcpy(str1, tmp_p2);
	 }
      } else if (a1->entry[sort3]) {
	 if (!(tmp_p3 = strchr(a1->entry[sort3],'\1'))) tmp_p3=a1->entry[sort3]+1;
	 if ((str1 = (char *)malloc(strlen(tmp_p3)+1)) == NULL) {
	    return 0;
	 }
	 strcpy(str1, tmp_p3);
      } else {
	 return -1;
      }

      if (a2->entry[sort1] || a2->entry[sort2]) {
	 if (a2->entry[sort1] && a2->entry[sort2]) {
	    if (!(tmp_p1 = strchr(a2->entry[sort1],'\1'))) tmp_p1=a2->entry[sort1]+1;
	    if (!(tmp_p2 = strchr(a2->entry[sort2],'\1'))) tmp_p2=a2->entry[sort2]+1;
	    if ((str2 = (char *)malloc(strlen(tmp_p1)+strlen(tmp_p2)+1)) == NULL) {
	       return 0;
	    }
	    strcpy(str2, tmp_p1);
	    strcat(str2, tmp_p2);
	 }
	 if (a2->entry[sort1] && (!a2->entry[sort2])) {
	    if (!(tmp_p1 = strchr(a2->entry[sort1],'\1'))) tmp_p1=a2->entry[sort1]+1;
	    if ((str2 = (char *)malloc(strlen(tmp_p1)+1)) == NULL) {
	       return 0;
	    }
	    strcpy(str2, tmp_p1);
	 }
	 if ((!a2->entry[sort1]) && a2->entry[sort2]) {
	    if (!(tmp_p2 = strchr(a2->entry[sort2],'\1'))) tmp_p2=a2->entry[sort2]+1;
	    if ((str2 = (char *)malloc(strlen(tmp_p2)+1)) == NULL) {
	       return 0;
	    }
	    strcpy(str2, tmp_p2);
	 }
      } else if (a2->entry[sort3]) {
	 if (!(tmp_p3 = strchr(a2->entry[sort3],'\1'))) tmp_p3=a2->entry[sort3]+1;
	 if ((str2 = (char *)malloc(strlen(tmp_p3)+1)) == NULL) {
	    return 0;
	 }
	 strcpy(str2, tmp_p3);
      } else {
	 free(str1);
	 return 1;
      }
   }

   /* lower case the strings for a better compare */
   for (i=strlen(str1)-1; i >= 0; i--) {
      str1[i] = tolower(str1[i]);
   }
   for (i=strlen(str2)-1; i >= 0; i--) {
      str2[i] = tolower(str2[i]);
   }

   i = strcoll(str2, str1);
   if (str1) {
      free(str1);
   }
   if (str2) {
      free(str2);
   }
   return i;
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
   long use_jos, char_set;

   /* Count the entries in the list */
   for (count=0, temp_al=*al; temp_al; temp_al=temp_al->next, count++) {
      ;
   }

   if (count<2) {
      /* We don't have to sort less than 2 items */
      return 0;
   }

   get_address_app_info(&ai);
   glob_sort_rule = ai.sortByCompany;
   if (sort_override) {
      glob_sort_rule = !(ai.sortByCompany & SORT_BY_COMPANY);
   }
   get_pref(PREF_CHAR_SET, &char_set, NULL);
   if (char_set == CHAR_SET_JAPANESE) {
      glob_sort_rule = glob_sort_rule | SORT_JAPANESE;
   } else {
      glob_sort_rule = glob_sort_rule & (SORT_JAPANESE-1);
   }
   get_pref(PREF_USE_JOS, &use_jos, NULL);
   if (use_jos) {
      glob_sort_rule = glob_sort_rule | SORT_JOS;
   } else {
      glob_sort_rule = glob_sort_rule & (SORT_JOS-1);
   }

   /* Allocate an array to be qsorted */
   sort_al = calloc(count, sizeof(AddressList *));
   if (!sort_al) {
      jp_logf(JP_LOG_WARN, "address_sort(): Out of Memory\n");
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

int pc_address_write(struct Address *a, PCRecType rt, unsigned char attrib,
		     unsigned int *unique_id)
{
   char record[65536];
   int rec_len,i;
   buf_rec br;
   long char_set;

   get_pref(PREF_CHAR_SET, &char_set, NULL);
   if (char_set != CHAR_SET_LATIN1) {
      for (i = 0; i < 19; i++) {
	 if (a->entry[i]) charset_j2p((unsigned char *)a->entry[i], strlen(a->entry[i])+1, char_set);
      }
   }

   rec_len = pack_Address(a, (unsigned char *)record, 65535);
   if (!rec_len) {
      PRINT_FILE_LINE;
      jp_logf(JP_LOG_WARN, "pack_Address %s\n", _("error"));
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

   jp_pc_write("AddressDB", &br);
   if (unique_id) {
      *unique_id = br.unique_id;
   }

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
   int num;
   int rec_size;
   unsigned char *buf;
   long char_set;

   memset(ai, 0, sizeof(*ai));
   /* Put at least one entry in there */
   strcpy(ai->category.name[0], "Unfiled");

   jp_get_app_info("AddressDB", &buf, &rec_size);
   num = unpack_AddressAppInfo(ai, buf, rec_size);
   if (buf) {
      free(buf);
   }
   if ((num<0) || (rec_size<=0)) {
      jp_logf(JP_LOG_WARN, _("Error reading %s\n"), "AddressDB.pdb");
      return -1;
   }

   get_pref(PREF_CHAR_SET, &char_set, NULL); 
   if (char_set != CHAR_SET_LATIN1) {
      /* Convert to host character set */
      int i;
      for (i = 0; i < CATCOUNT; i++)
	if (ai->category.name[i][0] != '\0') {
	   charset_p2j((unsigned char *)ai->category.name[i], HOSTCATLTH, char_set);
	}
      for (i = 0; i < 19 + 3; i++)
	if (ai->labels[i][0] != '\0') {
	   charset_p2j((unsigned char *)ai->labels[i],16, char_set);
	}
      for (i = 0; i < 8; i++)
	if (ai->phoneLabels[i][0] != '\0') {
	   charset_p2j((unsigned char *)ai->phoneLabels[i],16, char_set);
	}
   }

   return 0;
}

int get_addresses(AddressList **address_list, int sort_order)
{
   return get_addresses2(address_list, sort_order, 1, 1, 1, CATEGORY_ALL);
}
/* 
 * sort_order: 0=descending,  1=ascending
 * modified, deleted and private, 0 for no, 1 for yes, 2 for use prefs
 */
int get_addresses2(AddressList **address_list, int sort_order,
		  int modified, int deleted, int privates, int category)
{
   GList *records;
   GList *temp_list;
   int recs_returned, i, num;
   struct Address a;
   AddressList *temp_a_list;
   long keep_modified, keep_deleted;
   int keep_priv;
   long char_set;
   buf_rec *br;
   char *buf;

   jp_logf(JP_LOG_DEBUG, "get_addresses2()\n");
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

   *address_list=NULL;
   recs_returned = 0;

   num = jp_read_DB_files("AddressDB", &records);
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

      num = unpack_Address(&a, br->buf, br->size);

      if (num <= 0) {
	 continue;
      }

      if ( ((br->attrib & 0x0F) != category) && category != CATEGORY_ALL) {
	 continue;
      }
      buf = NULL;
      get_pref(PREF_CHAR_SET, &char_set, NULL);
      if (char_set != CHAR_SET_LATIN1) {
	 for (i = 0; i < 19; i++) {
	    if ((a.entry[i] != NULL) && (a.entry[i][0] != '\0')) {
/* JPA use new conversion routines
	       if ((buf = (char *)realloc(buf, strlen(a.entry[i])*2+1)) != NULL) {
		  strcpy(buf, a.entry[i]);
		  charset_p2j((unsigned char *)buf, strlen(a.entry[i])*2+1, char_set);
*/
               buf = charset_p2newj((unsigned char *)a.entry[i], strlen(a.entry[i])+1, char_set);
               if (buf) {
		  if (strlen(buf) > strlen(a.entry[i])) {
		     free(a.entry[i]);
		     a.entry[i] = strdup(buf);
		  } else {
		     multibyte_safe_strncpy(a.entry[i], buf, strlen(a.entry[i])+1);
		  }
	       }
	    }
	 }
	 free(buf);
      }

      temp_a_list = malloc(sizeof(AddressList));
      if (!temp_a_list) {
	 jp_logf(JP_LOG_WARN, "get_addresses2(): Out of memory\n");
	 break;
      }
      memcpy(&(temp_a_list->ma.a), &a, sizeof(struct Address));
      temp_a_list->app_type = ADDRESS;
      temp_a_list->ma.rt = br->rt;
      temp_a_list->ma.attrib = br->attrib;
      temp_a_list->ma.unique_id = br->unique_id;
      temp_a_list->next = *address_list;
      *address_list = temp_a_list;
      recs_returned++;
   }

   jp_free_DB_records(&records);

#ifdef JPILOT_DEBUG
   print_address_list(address_list);
#endif
   address_sort(address_list, sort_order);

   jp_logf(JP_LOG_DEBUG, "Leaving get_addresses2()\n");

   return recs_returned;
}

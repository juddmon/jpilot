/* $Id: contact.c,v 1.3 2008/04/23 22:14:38 rikster5 Exp $ */

/*******************************************************************************
 * contact.c
 * A module of J-Pilot http://jpilot.org
 *
 * Copyright (C) 1999-2006 by Judd Montgomery
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

#include "config.h"
#include "i18n.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <pi-source.h>
#include <pi-socket.h>
#include <pi-address.h>
#include "jp-pi-contact.h"
#include <pi-dlp.h>
#include "utils.h"
#include "log.h"
#include "prefs.h"
#include "libplugin.h"
#include "password.h"
#include "address.h"

static int glob_sort_rule;
#define SORT_BY_COMPANY 1
#define SORT_JAPANESE 2
#define SORT_JOS 4
static int sort_override=0;

#define CONT_ADDR_MAP_SIZE (NUM_CONTACT_ENTRIES * 2)

int contact_compare(const void *v1, const void *v2)
{
   char *str1, *str2;
   int sort1, sort2, sort3;
   ContactList **cl1, **cl2;
   struct Contact *c1, *c2;
   int i;

   cl1=(ContactList **)v1;
   cl2=(ContactList **)v2;

   c1=&((*cl1)->mcont.cont);
   c2=&((*cl2)->mcont.cont);

   if (glob_sort_rule & SORT_BY_COMPANY) {
      sort1=contCompany;
      sort2=contLastname;
      sort3=contFirstname;
   } else {
      sort1=contLastname;
      sort2=contFirstname;
      sort3=contCompany;
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
      if (c1->entry[sort1] || c1->entry[sort2]) {
	 if (c1->entry[sort1] && c1->entry[sort2]) {
	    if ((str1 = malloc(strlen(c1->entry[sort1])+strlen(c1->entry[sort2])+1)) == NULL) {
	       return 0;
	    }
	    strcpy(str1, c1->entry[sort1]);
	    strcat(str1, c1->entry[sort2]);
	 }
	 if (c1->entry[sort1] && (!c1->entry[sort2])) {
	    if ((str1 = malloc(strlen(c1->entry[sort1])+1)) == NULL) {
	       return 0;
	    }
	    strcpy(str1, c1->entry[sort1]);
	 }
	 if ((!c1->entry[sort1]) && c1->entry[sort2]) {
	    if ((str1 = malloc(strlen(c1->entry[sort2])+1)) == NULL) {
	       return 0;
	    }
	    strcpy(str1, c1->entry[sort2]);
	 }
      } else if (c1->entry[sort3]) {
	 if ((str1 = malloc(strlen(c1->entry[sort3])+1)) == NULL) {
	    return 0;
	 }
	 strcpy(str1, c1->entry[sort3]);
      } else {
	 return -1;
      }

      if (c2->entry[sort1] || c2->entry[sort2]) {
	 if (c2->entry[sort1] && c2->entry[sort2]) {
	    if ((str2 = malloc(strlen(c2->entry[sort1])+strlen(c2->entry[sort2])+1)) == NULL) {
	       return 0;
	    }
	    strcpy(str2, c2->entry[sort1]);
	    strcat(str2, c2->entry[sort2]);
	 }
	 if (c2->entry[sort1] && (!c2->entry[sort2])) {
	    if ((str2 = malloc(strlen(c2->entry[sort1])+1)) == NULL) {
	       return 0;
	    }
	    strcpy(str2, c2->entry[sort1]);
	 }
	 if ((!c2->entry[sort1]) && c2->entry[sort2]) {
	    if ((str2 = malloc(strlen(c2->entry[sort2])+1)) == NULL) {
	       return 0;
	    }
	    strcpy(str2, c2->entry[sort2]);
	 }
      } else if (c2->entry[sort3]) {
	 if ((str2 = malloc(strlen(c2->entry[sort3])+1)) == NULL) {
	    return 0;
	 }
	 strcpy(str2, c2->entry[sort3]);
      } else {
	 free(str1);
	 return 1;
      }
   } else if ((glob_sort_rule & SORT_JAPANESE) && !(glob_sort_rule & SORT_JOS)){
      char *tmp_p1, *tmp_p2, *tmp_p3;
      if (c1->entry[sort1] || c1->entry[sort2]) {
	 if (c1->entry[sort1] && c1->entry[sort2]) {
	    if (!(tmp_p1 = strchr(c1->entry[sort1],'\1'))) tmp_p1=c1->entry[sort1]+1;
	    if (!(tmp_p2 = strchr(c1->entry[sort2],'\1'))) tmp_p2=c1->entry[sort2]+1;
	    if ((str1 = malloc(strlen(tmp_p1)+strlen(tmp_p2)+1)) == NULL) {
	       return 0;
	    }
	    strcpy(str1, tmp_p1);
	    strcat(str1, tmp_p2);
	 }
	 if (c1->entry[sort1] && (!c1->entry[sort2])) {
	    if (!(tmp_p1 = strchr(c1->entry[sort1],'\1'))) tmp_p1=c1->entry[sort1]+1;
	    if ((str1 = malloc(strlen(tmp_p1)+1)) == NULL) {
	       return 0;
	    }
	    strcpy(str1, tmp_p1);
	 }
	 if ((!c1->entry[sort1]) && c1->entry[sort2]) {
	    if (!(tmp_p2 = strchr(c1->entry[sort2],'\1'))) tmp_p2=c1->entry[sort2]+1;
	    if ((str1 = malloc(strlen(tmp_p2)+1)) == NULL) {
	       return 0;
	    }
	    strcpy(str1, tmp_p2);
	 }
      } else if (c1->entry[sort3]) {
	 if (!(tmp_p3 = strchr(c1->entry[sort3],'\1'))) tmp_p3=c1->entry[sort3]+1;
	 if ((str1 = malloc(strlen(tmp_p3)+1)) == NULL) {
	    return 0;
	 }
	 strcpy(str1, tmp_p3);
      } else {
	 return -1;
      }

      if (c2->entry[sort1] || c2->entry[sort2]) {
	 if (c2->entry[sort1] && c2->entry[sort2]) {
	    if (!(tmp_p1 = strchr(c2->entry[sort1],'\1'))) tmp_p1=c2->entry[sort1]+1;
	    if (!(tmp_p2 = strchr(c2->entry[sort2],'\1'))) tmp_p2=c2->entry[sort2]+1;
	    if ((str2 = malloc(strlen(tmp_p1)+strlen(tmp_p2)+1)) == NULL) {
	       return 0;
	    }
	    strcpy(str2, tmp_p1);
	    strcat(str2, tmp_p2);
	 }
	 if (c2->entry[sort1] && (!c2->entry[sort2])) {
	    if (!(tmp_p1 = strchr(c2->entry[sort1],'\1'))) tmp_p1=c2->entry[sort1]+1;
	    if ((str2 = malloc(strlen(tmp_p1)+1)) == NULL) {
	       return 0;
	    }
	    strcpy(str2, tmp_p1);
	 }
	 if ((!c2->entry[sort1]) && c2->entry[sort2]) {
	    if (!(tmp_p2 = strchr(c2->entry[sort2],'\1'))) tmp_p2=c2->entry[sort2]+1;
	    if ((str2 = malloc(strlen(tmp_p2)+1)) == NULL) {
	       return 0;
	    }
	    strcpy(str2, tmp_p2);
	 }
      } else if (c2->entry[sort3]) {
	 if (!(tmp_p3 = strchr(c2->entry[sort3],'\1'))) tmp_p3=c2->entry[sort3]+1;
	 if ((str2 = malloc(strlen(tmp_p3)+1)) == NULL) {
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
int contacts_sort(ContactList **cl, int sort_order)
{
   ContactList *temp_cl;
   ContactList **sort_cl;
   struct ContactAppInfo ai;
   int count, i;
   long use_jos, char_set;

   /* Count the entries in the list */
   for (count=0, temp_cl=*cl; temp_cl; temp_cl=temp_cl->next, count++) {
      ;
   }

   if (count<2) {
      /* We don't have to sort less than 2 items */
      return EXIT_SUCCESS;
   }

   get_contact_app_info(&ai);
   glob_sort_rule = ai.sortByCompany;
   if (sort_override) {
      glob_sort_rule = !(ai.sortByCompany & SORT_BY_COMPANY);
   }
   get_pref(PREF_CHAR_SET, &char_set, NULL);
   if (char_set == CHAR_SET_JAPANESE || char_set == CHAR_SET_SJIS_UTF) {
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
   sort_cl = calloc(count, sizeof(ContactList *));
   if (!sort_cl) {
      jp_logf(JP_LOG_WARN, "contact_sort(): %s\n", _("Out of memory"));
      return EXIT_FAILURE;
   }

   /* Set our array to be a list of pointers to the nodes in the linked list */
   for (i=0, temp_cl=*cl; temp_cl; temp_cl=temp_cl->next, i++) {
      sort_cl[i] = temp_cl;
   }

   /* qsort them */
   qsort(sort_cl, count, sizeof(ContactList *), contact_compare);

   /* Put the linked list in the order of the array */
   if (sort_order==SORT_ASCENDING) {
      for (i=count-1; i>0; i--) {
	 sort_cl[i]->next=sort_cl[i-1];
      }
      sort_cl[0]->next = NULL;
      *cl = sort_cl[count-1];
   } else {
      /* Descending order */
      sort_cl[count-1]->next = NULL;
      for (i=count-1; i; i--) {
	 sort_cl[i-1]->next=sort_cl[i];
      }
      *cl = sort_cl[0];
   }

   free(sort_cl);

   return EXIT_SUCCESS;
}

int pc_contact_write(struct Contact *cont, PCRecType rt, unsigned char attrib,
		     unsigned int *unique_id)
{
//#ifndef PILOT_LINK_0_12
//   char *record;
//   int rec_len;
//#else /* PILOT_LINK_0_12 */
   pi_buffer_t *RecordBuffer;
//#endif /* PILOT_LINK_0_12 */
   int i;
   buf_rec br;
   long char_set;

   get_pref(PREF_CHAR_SET, &char_set, NULL);
   if (char_set != CHAR_SET_LATIN1) {
      for (i = 0; i < 39; i++) {
	 if (cont->entry[i]) charset_j2p(cont->entry[i], strlen(cont->entry[i])+1, char_set);
      }
   }

   RecordBuffer = pi_buffer_new(0);
   jp_pack_Contact(cont, RecordBuffer);
   if (!RecordBuffer->used) {
      PRINT_FILE_LINE;
      jp_logf(JP_LOG_WARN, "jp_pack_Contact %s\n", _("error"));
      pi_buffer_free(RecordBuffer);
      return EXIT_FAILURE;
   }
   br.rt=rt;
   br.attrib = attrib;

//#ifndef PILOT_LINK_0_12
//   br.buf = record;
//   br.size = rec_len;
//#else /* PILOT_LINK_0_12 */
   br.buf = RecordBuffer->data;
   br.size = RecordBuffer->used;
//#endif /* PILOT_LINK_0_12 */
   /* Keep unique ID intact */
   if (unique_id) {
      br.unique_id = *unique_id;
   } else {
      br.unique_id = 0;
   }

   jp_pc_write("ContactsDB-PAdd", &br);
   if (unique_id) {
      *unique_id = br.unique_id;
   }

#ifdef PILOT_LINK_0_12
   pi_buffer_free(RecordBuffer);
#endif
   return EXIT_SUCCESS;
}

void free_ContactList(ContactList **cl)
{
   ContactList *temp_cl, *temp_cl_next;

   for (temp_cl = *cl; temp_cl; temp_cl=temp_cl_next) {
      jp_free_Contact(&(temp_cl->mcont.cont));
      temp_cl_next = temp_cl->next;
      free(temp_cl);
   }
   *cl = NULL;
}

int get_contact_app_info(struct ContactAppInfo *ai)
{
   int num;
   int rec_size;
   unsigned char *buf;
   pi_buffer_t pi_buf;
   long char_set;

   memset(ai, 0, sizeof(*ai));
   /* Put at least one entry in there */
   strcpy(ai->category.name[0], "Unfiled");

   jp_get_app_info("ContactsDB-PAdd", &buf, &rec_size);
   //num = jp_unpack_ContactAppInfo(ai, buf, rec_size);
   pi_buf.data = buf;
   pi_buf.used = rec_size;
   pi_buf.allocated = rec_size;

   num = jp_unpack_ContactAppInfo(ai, &pi_buf);

   if (buf) {
      free(buf);
   }

   if ((num<0) || (rec_size<=0)) {
      jp_logf(JP_LOG_WARN, _("Error reading file: %s\n"), "ContactsDB-PAdd.pdb");
      return EXIT_FAILURE;
   }

   get_pref(PREF_CHAR_SET, &char_set, NULL);
   if (char_set != CHAR_SET_LATIN1) {
      /* Convert to host character set */
      int i;

      for (i = 0; i < 49 + 3; i++)
	if (ai->labels[i][0] != '\0') {
	   charset_p2j(ai->labels[i],16, char_set);
	}
      for (i = 0; i < 8; i++)
	if (ai->phoneLabels[i][0] != '\0') {
	   charset_p2j(ai->phoneLabels[i],16, char_set);
	}
      for (i = 0; i < 3; i++)
	if (ai->addrLabels[i][0] != '\0') {
	   charset_p2j(ai->addrLabels[i],16, char_set);
	}
      for (i = 0; i < 5; i++)
	if (ai->IMLabels[i][0] != '\0') {
	   charset_p2j(ai->IMLabels[i],16, char_set);
	}
   }

   return EXIT_SUCCESS;
}

int copy_addresses_to_contacts(AddressList *al, ContactList **cl)
{
   ContactList *temp_cl, *last_cl;
   AddressList *temp_al;

   *cl = last_cl = NULL;

   for (temp_al = al; temp_al; temp_al=temp_al->next) {
      temp_cl = malloc(sizeof(ContactList));
      if (!temp_cl) return -1;
      temp_cl->mcont.rt = temp_al->maddr.rt;
      temp_cl->mcont.unique_id = temp_al->maddr.unique_id;
      temp_cl->mcont.attrib = temp_al->maddr.attrib;
      copy_address_to_contact(&(temp_al->maddr.addr), &(temp_cl->mcont.cont));
      temp_cl->app_type = CONTACTS;
      temp_cl->next=NULL;
      if (!last_cl) {
	 *cl = last_cl = temp_cl;
      } else {
	 last_cl->next = temp_cl;
	 last_cl = temp_cl;
      }
   }
   return 0;
}

int get_contacts(ContactList **contact_list, int sort_order)
{
   return get_contacts2(contact_list, sort_order, 1, 1, 1, CATEGORY_ALL);
}
/*
 * sort_order: 0=descending,  1=ascending
 * modified, deleted and private, 0 for no, 1 for yes, 2 for use prefs
 */
int get_contacts2(ContactList **contact_list, int sort_order,
		  int modified, int deleted, int privates, int category)
{
   GList *records;
   GList *temp_list;
   int recs_returned, i, num;
   struct Contact cont;
   ContactList *temp_c_list;
   long keep_modified, keep_deleted;
   int keep_priv;
   long char_set;
   buf_rec *br;
   char *buf;
#ifdef PILOT_LINK_0_12
   pi_buffer_t pi_buf;
#endif /* PILOT_LINK_0_12 */

   jp_logf(JP_LOG_DEBUG, "get_contacts2()\n");
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

   *contact_list=NULL;
   recs_returned = 0;

   num = jp_read_DB_files("ContactsDB-PAdd", &records);
   if (-1 == num)
     return 0;

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

      if ( ((br->rt==DELETED_PALM_REC)  && (!keep_deleted)) ||
	   ((br->rt==DELETED_PC_REC)    && (!keep_deleted)) ||
	   ((br->rt==MODIFIED_PALM_REC) && (!keep_modified)) ) {
	 continue;
      }
      if ((keep_priv != SHOW_PRIVATES) &&
	  (br->attrib & dlpRecAttrSecret)) {
	 continue;
      }

//#ifndef PILOT_LINK_0_12
      /* This is kind of a hack to set the pi_buf directly, but its faster */
      pi_buf.data = br->buf;
      pi_buf.used = br->size;
      pi_buf.allocated = br->size;
      //num = jp_unpack_Contact(&cont, br->buf, br->size);
      num = jp_unpack_Contact(&cont, &pi_buf);
//#else /* PILOT_LINK_0_12 */
//      RecordBuffer = pi_buffer_new(br->size);
//      memcpy(RecordBuffer->data, br->buf, br->size);
//      RecordBuffer->used = br->size;
//#endif /* PILOT_LINK_0_12 */

//#ifndef PILOT_LINK_0_12
      if (num <= 0) {
	 continue;
      }
//#else /* PILOT_LINK_0_12 */
//      if (jp_unpack_Contact(&cont, RecordBuffer, contact_v1) == -1) {
//	 pi_buffer_free(RecordBuffer);
//	 continue;
//      }
//      pi_buffer_free(RecordBuffer);
//#endif /* PILOT_LINK_0_12 */

      if ( ((br->attrib & 0x0F) != category) && category != CATEGORY_ALL) {
	 continue;
      }
      buf = NULL;
      get_pref(PREF_CHAR_SET, &char_set, NULL);
      if (char_set != CHAR_SET_LATIN1) {
	 for (i = 0; i < 39; i++) {
	    if ((cont.entry[i] != NULL) && (cont.entry[i][0] != '\0')) {
               buf = charset_p2newj(cont.entry[i], strlen(cont.entry[i])+1, char_set);
               if (buf) {
		  if (strlen(buf) > strlen(cont.entry[i])) {
		     free(cont.entry[i]);
		     cont.entry[i] = strdup(buf);
		     free(buf);
		  } else {
		     multibyte_safe_strncpy(cont.entry[i], buf, strlen(cont.entry[i])+1);
		     free(buf);
		  }
	       }
	    }
	 }
      }

      temp_c_list = malloc(sizeof(ContactList));
      if (!temp_c_list) {
	 jp_logf(JP_LOG_WARN, "get_contacts2(): %s\n", _("Out of memory"));
	 break;
      }
      memcpy(&(temp_c_list->mcont.cont), &cont, sizeof(struct Contact));
      temp_c_list->app_type = CONTACTS;
      temp_c_list->mcont.rt = br->rt;
      temp_c_list->mcont.attrib = br->attrib;
      temp_c_list->mcont.unique_id = br->unique_id;
      temp_c_list->next = *contact_list;
      *contact_list = temp_c_list;
      recs_returned++;
   }

   jp_free_DB_records(&records);

#ifdef JPILOT_DEBUG
   print_contact_list(contact_list);
#endif
   contacts_sort(contact_list, sort_order);

   jp_logf(JP_LOG_DEBUG, "Leaving get_contacts2()\n");

   return recs_returned;
}

/**********************************************************************
 * These are compatibility functions to copy address records into contact
 * records, and vice-versa.
 **********************************************************************/


long cont_addr_map[CONT_ADDR_MAP_SIZE]={
   contLastname, entryLastname,
     contFirstname, entryFirstname,
     contCompany, entryCompany,
     contTitle, entryTitle,
     contPhone1, entryPhone1,
     contPhone2, entryPhone2,
     contPhone3, entryPhone3,
     contPhone4, entryPhone4,
     contPhone5, entryPhone5,
     contPhone6, -1,
     contPhone7, -1,
     contIM1, -1,
     contIM2, -1,
     contWebsite, -1,
     contCustom1, entryCustom1,
     contCustom2, entryCustom2,
     contCustom3, entryCustom3,
     contCustom4, entryCustom4,
     contCustom5, -1,
     contCustom6, -1,
     contCustom7, -1,
     contCustom8, -1,
     contCustom9, -1,
     contAddress1, entryAddress,
     contCity1, entryCity,
     contState1, entryState,
     contZip1, entryZip,
     contCountry1, entryCountry,
     contAddress2, -1,
     contCity2, -1,
     contState2, -1,
     contZip2, -1,
     contCountry2, -1,
     contAddress3, -1,
     contCity3, -1,
     contState3, -1,
     contZip3, -1,
     contCountry3, -1,
     contNote, entryNote
};
      
int copy_contact_to_address(const struct Contact *c, struct Address *a)
{
   int i, a_entry, c_entry;

   for (i=0; i<NUM_CONTACT_ENTRIES; i++) {
      c_entry = cont_addr_map[i*2];
      a_entry = cont_addr_map[i*2+1];
      
      a->entry[a_entry]=NULL;
      if ((c_entry>=0) && (a_entry>=0)) {
	 if (c->entry[c_entry]) {
	    a->entry[a_entry]=strdup(c->entry[c_entry]);
	 }
      }
   }

   for (i=0; i<5; i++) {
      a->phoneLabel[i] = c->phoneLabel[i];
   }
   
   a->showPhone = c->showPhone;

   if (a->showPhone > 4) {
      for (i=0; i<5; i++) {
	 if (a->entry[entryPhone1 + i]) {
	    a->showPhone = i;
	    break;
	 }
      }
   }
   return 0;
}

int copy_address_to_contact(const struct Address *a, struct Contact *c)
{
   int i, a_entry, c_entry;

   for (i=0; i<NUM_CONTACT_ENTRIES; i++) {
      c_entry = cont_addr_map[i*2];
      a_entry = cont_addr_map[i*2+1];
      
      c->entry[c_entry]=NULL;
      if ((c_entry>=0) && (a_entry>=0)) {
	 if (a->entry[a_entry]) {
	    c->entry[c_entry]=strdup(a->entry[a_entry]);
	 }
      }
   }

   for (i=0; i<5; i++) {
      c->phoneLabel[i] = a->phoneLabel[i];
   }
   /* Set these 2 to their default values */
   c->phoneLabel[5] = 6;
   c->phoneLabel[6] = 3;
   
   c->showPhone = a->showPhone;

   /* Set remaining fields to default values */
   c->addressLabel[0] = 0;
   c->addressLabel[1] = 1;
   c->addressLabel[2] = 2;

   c->IMLabel[0] = 0;
   c->IMLabel[1] = 1;

   c->birthdayFlag = 0;

   c->reminder = 0;
   c->advance = 0;
   c->advanceUnits = 0;	   
   memset(&(c->birthday), 0, sizeof(struct tm));
   for (i=0; i<MAX_CONTACT_BLOBS; i++) {
      c->blob[i] = NULL;
   }
   c->picture = NULL;

   return 0;
}

/* Copy App info data structures */
int copy_address_ai_to_contact_ai(const struct AddressAppInfo *aai, struct ContactAppInfo *cai)
{
   int i, a_entry, c_entry;

   memcpy(&cai->category, &aai->category, sizeof(struct CategoryAppInfo));

   memset(&cai->unknown1, '\0', 26);

   for (i=0; i<NUM_CONTACT_ENTRIES; i++) {
      c_entry = cont_addr_map[i*2];
      a_entry = cont_addr_map[i*2+1];
      if ((c_entry>=0) && (a_entry>=0)) {
	 strncpy(cai->labels[c_entry], aai->labels[a_entry], 16);
	 cai->labels[c_entry][15]='\0';
      } else {
	 if (c_entry>=0) {
	    cai->labels[c_entry][0]='\0';
	 }
      }
   }

   /* The rest of the labels do not exist in address */
   if (cai->version==10) {
      for (i=NUM_CONTACT_ENTRIES; i<NUM_CONTACT_V10_LABELS; i++) {
	 cai->labels[i][0]='\0';
      }
   }
   if (cai->version==11) {
      for (i=NUM_CONTACT_ENTRIES; i<NUM_CONTACT_V11_LABELS; i++) {
	 cai->labels[i][0]='\0';
      }
   }

   cai->country=aai->country;
   cai->sortByCompany=aai->sortByCompany;

   for (i=0; i<8; i++) {
      strncpy(cai->phoneLabels[i], aai->phoneLabels[i], 16);
      cai->phoneLabels[i][15]='\0';
   }

   for (i=0; i<3; i++) {
      cai->addrLabels[i][0] = '\0';
   }
   for (i=0; i<5; i++) {
      cai->IMLabels[i][0] = '\0';
   }
   return 0;
}

/* Copy App info data structures */
int copy_contact_ai_to_address_ai(const struct ContactAppInfo *cai, struct AddressAppInfo *aai)
{
   int i, a_entry, c_entry;

   memcpy(&aai->category, &cai->category, sizeof(struct CategoryAppInfo));

   for (i=0; i<NUM_CONTACT_ENTRIES; i++) {
      c_entry = cont_addr_map[i*2];
      a_entry = cont_addr_map[i*2+1];
      if ((c_entry>=0) && (a_entry>=0)) {
	 strncpy(aai->labels[a_entry], cai->labels[c_entry], 16);
	 aai->labels[a_entry][15]='\0';
      } else {
	 if (a_entry>=0) {
	    aai->labels[a_entry][0]='\0';
	 }
      }
   }

   aai->country=cai->country;
   aai->sortByCompany=cai->sortByCompany;

   for (i=0; i<8; i++) {
      strncpy(aai->phoneLabels[i], cai->phoneLabels[i], 16);
      aai->phoneLabels[i][15]='\0';
   }

   for (i=0; i<19+3; i++) {
      aai->labelRenamed[i]=0;
   }

   return 0;
}

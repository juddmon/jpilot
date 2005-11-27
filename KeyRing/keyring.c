/* $Id: keyring.c,v 1.49 2005/11/27 19:14:34 rikster5 Exp $ */

/*******************************************************************************
 * keyring.c
 *
 * This is a plugin for J-Pilot for the KeyRing Palm program.
 * It keeps records and uses DES3 encryption.
 *
 * Copyright (C) 2001 by Judd Montgomery
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <gtk/gtk.h>

/* OpenSSL header files */
#include <openssl/md5.h>
#include <openssl/des.h>

/* Pilot-link header files */
#include <pi-appinfo.h>
#include <pi-dlp.h>
#include <pi-file.h>

/* Jpilot header files */
#include "libplugin.h"
#include "utils.h"
#include "../i18n.h"
#include "prefs.h"
#include "stock_buttons.h"

/********************************* Constants **********************************/
#define KEYRING_CAT1 1
#define KEYRING_CAT2 2

#define PASSWD_ENTER	   0
#define PASSWD_ENTER_RETRY 1
#define PASSWD_ENTER_NEW   2

#define PASSWD_LEN 100

#define CONNECT_SIGNALS    400
#define DISCONNECT_SIGNALS 401

#define NUM_KEYRING_CAT_ITEMS 16

/* re-ask password PLUGIN_MAX_INACTIVE_TIME seconds after 
 * deselecting the plugin */
#define PLUGIN_MAX_INACTIVE_TIME 1

/* for password hashes */
#define SALT_SIZE 4
#define MESSAGE_BUF_SIZE 64
#define MD5_HASH_SIZE 16

#define MIN_KR_PASS (20)   /* Minimum auto-generated passwd length */
#define MAX_KR_PASS (25)   /* Maximum auto-generated passwd length */

struct KeyRing {
   char *name;	   /* Unencrypted */
   char *account;  /* Encrypted */
   char *password; /* Encrypted */
   char *note;	   /* Encrypted */
   unsigned long last_changed; /* Encrypted */
};
/* 
 * This is my wrapper to the KeyRing structure so that I can put
 * a few more fields in with it.
 */
struct MyKeyRing {
   PCRecType rt;
   unsigned int unique_id;
   unsigned char attrib;
   struct KeyRing kr;
   struct MyKeyRing *next;
};

/******************************* Global vars **********************************/
/* This is the category that is currently being displayed */
static int keyr_category = CATEGORY_ALL;
static int glob_category_number_from_menu_item[NUM_KEYRING_CAT_ITEMS];

static GtkWidget *clist;
static GtkWidget *entry_name;
static GtkWidget *entry_account;
static GtkWidget *entry_password;
static GtkWidget *keyr_note;
#ifdef ENABLE_GTK2
static GObject	 *keyr_note_buffer;
#endif
static GtkWidget *menu_category1;
static GtkWidget *menu_category2;
static int old_category;
static GtkWidget *menu_item_category1[NUM_KEYRING_CAT_ITEMS+1];
static GtkWidget *menu_item_category2[NUM_KEYRING_CAT_ITEMS];
static GtkWidget *scrolled_window;
static GtkWidget *new_record_button;
static GtkWidget *apply_record_button;
static GtkWidget *add_record_button;
static GtkWidget *delete_record_button;
static GtkWidget *undelete_record_button;
static GtkWidget *copy_record_button;
static GtkWidget *cancel_record_button;
static GtkWidget *pane = NULL;
#ifndef ENABLE_STOCK_BUTTONS
static GtkAccelGroup *accel_group;
#endif
static int record_changed;
static int clist_hack;
static int clist_row_selected;

#ifdef HEADER_NEW_DES_H
DES_cblock current_key1;
DES_cblock current_key2;
DES_cblock new_key1;
DES_cblock new_key2;
static DES_key_schedule s1, s2;
#else
des_cblock current_key1;
des_cblock current_key2;
des_cblock new_key1;
des_cblock new_key2;
static des_key_schedule s1, s2;
#endif
  
time_t plugin_last_time = 0;
gboolean plugin_active = FALSE;

struct MyKeyRing *glob_keyring_list=NULL;

/****************************** Prototypes ************************************/
static void keyr_update_clist();

static void connect_changed_signals(int con_or_dis);

static void cb_clist_selection(GtkWidget      *clist,
			       gint	      row,
			       gint	      column,
			       GdkEventButton *event,
			       gpointer       data);

static int keyring_find(int unique_id);

/****************************** Main Code *************************************/
static int pack_KeyRing(struct KeyRing *kr, unsigned char *buf, int buf_size,
			int *wrote_size)
{
   int n;
   int i;
   char empty[]="";

   jp_logf(JP_LOG_DEBUG, "KeyRing: pack_KeyRing()\n");

   *wrote_size=0;

   if (!(kr->name))	kr->name=empty;
   if (!(kr->account))	kr->account=empty;
   if (!(kr->password)) kr->password=empty;
   if (!(kr->note))	kr->note=empty;

   /* 3 chars accounts for NULL string terminators */
   n=strlen(kr->account) + strlen(kr->password) + strlen(kr->note) + 3;
   /* The encrypted portion must be a multiple of 8 */
   if ((n%8)) {
      n=n+(8-(n%8));
   }
   /* Now we can add in the unencrypted part */
   n=n+strlen(kr->name)+1;
   jp_logf(JP_LOG_DEBUG, "pack n=%d\n", n);

   if (n+2>buf_size) {
      jp_logf(JP_LOG_WARN, _("KeyRing: pack_KeyRing(): buf_size too small\n"));
      return EXIT_FAILURE;
   }

   memset(buf, 0, n+1);
   *wrote_size = n;
   strcpy(buf, kr->name);
   i = strlen(kr->name)+1;
   strcpy(&buf[i], kr->account);
   i += strlen(kr->account)+1;
   strcpy(&buf[i], kr->password);
   i += strlen(kr->password)+1;
   strcpy(&buf[i], kr->note);
   for (i=strlen(kr->name)+1; i<n; i=i+8) {
      /* des_encrypt3((DES_LONG *)&buf[i], s1, s2, s1); */
#ifdef HEADER_NEW_DES_H
      DES_ecb3_encrypt(&buf[i], &buf[i], 
		       &s1, &s2, &s1, DES_ENCRYPT);
#else
      des_ecb3_encrypt((const_des_cblock *)&buf[i], (des_cblock *)(&buf[i]),
		       s1, s2, s1, DES_ENCRYPT);
#endif
   }
#ifdef JPILOT_DEBUG
   for (i=0;i<n; i++) {
      printf("%02x ", (unsigned char)buf[i]);	
   }
   printf("\n");
#endif
  
   return n;
}

static int unpack_KeyRing(struct KeyRing *kr, unsigned char *buf, int buf_size)
{
   int i, j;
   int n;
   int rem;
   unsigned char *clear_text;
   unsigned char *P;
   unsigned char *Pstr[3];
   unsigned char *safety[]={"","",""};

   jp_logf(JP_LOG_DEBUG, "KeyRing: unpack_KeyRing\n");
   if (!memchr(buf, '\0', buf_size)) {
      jp_logf(JP_LOG_DEBUG, "KeyRing: unpack_KeyRing(): No null terminater found in buf\n");
      return 0;
   }
   n=strlen(buf)+1;

   rem=buf_size-n;
   if (rem>0xFFFF) {
      /* This can be cause by a bug in libplugin.c from jpilot 0.99.1 
       * and before.  It occurs on the last record */
      jp_logf(JP_LOG_DEBUG, "KeyRing: unpack_KeyRing(): buffer too big n=%d, buf_size=%d\n", n, buf_size);
      jp_logf(JP_LOG_DEBUG, "KeyRing: unpack_KeyRing(): truncating\n");
      rem=0xFFFF-n;
      rem=rem-(rem%8);
   }
   clear_text=malloc(rem+8); /* Allow for some safety NULLs */
   bzero(clear_text, rem+8);

   jp_logf(JP_LOG_DEBUG, "KeyRing: unpack_KeyRing(): rem (should be multiple of 8)=%d\n", rem);
   jp_logf(JP_LOG_DEBUG, "KeyRing: unpack_KeyRing(): rem%%8=%d\n", rem%8);

   P=&buf[n];
   for (i=0; i<rem; i+=8) {
#ifdef HEADER_NEW_DES_H
      DES_ecb3_encrypt(&P[i], clear_text+i,
		       &s1, &s2, &s1, DES_DECRYPT);
#else
      des_ecb3_encrypt((const_des_cblock *)&P[i], (des_cblock *)(clear_text+i),
		       s1, s2, s1, DES_DECRYPT);
#endif
		       
   }

   Pstr[0]=clear_text;
   Pstr[1]=safety[1];
   Pstr[2]=safety[2];

   for (i=0, j=1; (i<rem) && (j<3); i++) {
      if (!clear_text[i]) {
	 Pstr[j]=&clear_text[i+1];
	 j++;
      }
   }

#ifdef DEBUG
   printf("name  [%s]\n", buf);
   printf("Pstr0 [%s]\n", Pstr[0]);
   printf("Pstr1 [%s]\n", Pstr[1]);
   printf("Pstr2 [%s]\n", Pstr[2]);
#endif
   kr->name=strdup(buf);
   kr->account=strdup(Pstr[0]);
   kr->password=strdup(Pstr[1]);
   kr->note=strdup(Pstr[2]);

   free(clear_text);

   return 1;
}

/*
 * Return EXIT_FAILURE if password isn't good.
 * Return EXIT_SUCCESS if good and global and also sets s1, and s2 set
 */
static int set_password_hash(unsigned char *buf, int buf_size, char *passwd)
{
   unsigned char buffer[MESSAGE_BUF_SIZE];
   unsigned char md[MD5_HASH_SIZE];
   
   if (buf_size < MD5_HASH_SIZE) {
      return EXIT_FAILURE;
   }
   /* Must wipe passwd out of memory after using it */
   memset(buffer, 0, MESSAGE_BUF_SIZE);
   memcpy(buffer, buf, SALT_SIZE);
   strncpy(buffer+SALT_SIZE, passwd, MESSAGE_BUF_SIZE - SALT_SIZE - 1);
   MD5(buffer, MESSAGE_BUF_SIZE, md);

   /* wipe out password traces */
   memset(buffer, 0, MESSAGE_BUF_SIZE);

   if (memcmp(md, buf+SALT_SIZE, MD5_HASH_SIZE)) {
      return EXIT_FAILURE;
   }

   MD5(passwd, strlen(passwd), md);
   memcpy(current_key1, md, 8);
   memcpy(current_key2, md+8, 8);
#ifdef HEADER_NEW_DES_H
   DES_set_key(&current_key1, &s1);
   DES_set_key(&current_key2, &s2);
#else
   des_set_key(&current_key1, s1);
   des_set_key(&current_key2, s2);
#endif

   return EXIT_SUCCESS;
}

/* Start password change code */

/* 
 * Code for this is written, just need to add another jpilot API for
 * cancelling a sync if the passwords don't match.
 */

/* End password change code   */

/* utility function to read keyring data file and filter out unwanted records */
static int get_keyring(struct MyKeyRing **mkr_list, int category)
{
   GList *records=NULL;
   GList *temp_list;
   buf_rec *br;
   struct MyKeyRing *mkr;
   int rec_count;
   long keep_modified, keep_deleted;

   jp_logf(JP_LOG_DEBUG, "get_keyring()\n");

   *mkr_list = NULL;
   rec_count = 0;

   /* Read raw database of records */
   if (jp_read_DB_files("Keys-Gtkr", &records) == -1)
     return 0;

   /* Go to first entry in the list */
   for (temp_list = records; temp_list; temp_list = temp_list->prev) {
      records = temp_list;
   }

   /* Get preferences used for filtering */
   get_pref(PREF_SHOW_MODIFIED, &keep_modified, NULL);
   get_pref(PREF_SHOW_DELETED, &keep_deleted, NULL);

   /* Sort through list of records masking out unwanted ones */
   for (temp_list = records; temp_list; temp_list = temp_list->next) {
      if (temp_list->data) {
	 br=temp_list->data;
      } else {
	 continue;
      }
      if (!br->buf) {
	 continue;
      }
      /* record 0 is the hash-key record */
      if (br->attrib & dlpRecAttrSecret) {
	 continue;
      }

      /* Filter out deleted or deleted/modified records */
      if ( ((br->rt==DELETED_PALM_REC)	&& (!keep_deleted)) ||
	   ((br->rt==DELETED_PC_REC)	&& (!keep_deleted)) ||
	   ((br->rt==MODIFIED_PALM_REC) && (!keep_modified)) ) {
	 continue;
      }

      /* Filter by category */
      if ( ((br->attrib & 0x0F) != category) && category != CATEGORY_ALL) {
	 continue;
      }

      mkr = malloc(sizeof(struct MyKeyRing));
      mkr->next=NULL;
      mkr->attrib = br->attrib;
      mkr->unique_id = br->unique_id;
      mkr->rt = br->rt;

      if (unpack_KeyRing(&(mkr->kr), br->buf, br->size) <=0) {
	 free(mkr);
	 continue;
      }

      /* prepend to list */
      mkr->next=*mkr_list;
      *mkr_list=mkr;

      rec_count++;
   }

   jp_free_DB_records(&records);

   jp_logf(JP_LOG_DEBUG, "Leaving get_keyring()\n");

   return rec_count;
}

static void set_new_button_to(int new_state)
{
   jp_logf(JP_LOG_DEBUG, "set_new_button_to new %d old %d\n", new_state, record_changed);

   if (record_changed==new_state) {
      return;
   }

   switch (new_state) {
    case MODIFY_FLAG:
      gtk_clist_set_selection_mode(GTK_CLIST(clist), GTK_SELECTION_SINGLE);
      clist_hack=TRUE;
      /* The line selected on the clist becomes unhighlighted, so we do this */
      gtk_clist_select_row(GTK_CLIST(clist), clist_row_selected, 0);
      gtk_widget_show(apply_record_button);
      gtk_widget_show(cancel_record_button);
      gtk_widget_hide(delete_record_button);
      break;
    case NEW_FLAG:
      gtk_clist_set_selection_mode(GTK_CLIST(clist), GTK_SELECTION_SINGLE);
      clist_hack=TRUE;
      /* The line selected on the clist becomes unhighlighted, so we do this */
      gtk_clist_select_row(GTK_CLIST(clist), clist_row_selected, 0);
      gtk_widget_show(add_record_button);
      gtk_widget_show(cancel_record_button);
      gtk_widget_hide(copy_record_button);
      gtk_widget_hide(delete_record_button);
      break;
    case CLEAR_FLAG:
      gtk_clist_set_selection_mode(GTK_CLIST(clist), GTK_SELECTION_BROWSE);
      clist_hack=FALSE;
      gtk_widget_show(new_record_button);
      gtk_widget_show(copy_record_button);
      gtk_widget_show(delete_record_button);
      break;
    case UNDELETE_FLAG:
      gtk_clist_set_selection_mode(GTK_CLIST(clist), GTK_SELECTION_BROWSE);
      clist_hack=FALSE;
      gtk_widget_show(undelete_record_button);
      gtk_widget_hide(delete_record_button);
      break;

    default:
      return;
   }
   switch (record_changed) {
    case MODIFY_FLAG:
      gtk_widget_hide(apply_record_button);
      gtk_widget_hide(cancel_record_button);
      gtk_widget_show(copy_record_button);
      gtk_widget_show(delete_record_button);
      break;
    case NEW_FLAG:
      gtk_widget_hide(add_record_button);
      gtk_widget_hide(cancel_record_button);
      gtk_widget_show(copy_record_button);
      gtk_widget_show(delete_record_button);
      break;
    case CLEAR_FLAG:
      if (new_state != UNDELETE_FLAG)
      {
	 gtk_widget_hide(new_record_button);
	 gtk_widget_hide(delete_record_button);
      }
      break;
    case UNDELETE_FLAG:
      gtk_widget_hide(undelete_record_button);
      gtk_widget_show(delete_record_button);
      break;
   }

   record_changed=new_state;
}

static void cb_record_changed(GtkWidget *widget,
			      gpointer	 data)
{
   jp_logf(JP_LOG_DEBUG, "cb_record_changed\n");

   if (record_changed==CLEAR_FLAG) {
      connect_changed_signals(DISCONNECT_SIGNALS);
      if ((GTK_CLIST(clist)->rows > 0)) {
	 set_new_button_to(MODIFY_FLAG);
      } else {
	 set_new_button_to(NEW_FLAG);
      }
   }
   else if (record_changed==UNDELETE_FLAG)
   {
      jp_logf(JP_LOG_INFO|JP_LOG_GUI,
	      _("This record is deleted.\n"
		"Undelete it or copy it to make changes.\n"));
   }
}

static void connect_changed_signals(int con_or_dis)
{
   int i;
   static int connected=0;

   /* CONNECT */
   if ((con_or_dis==CONNECT_SIGNALS) && (!connected)) {
      jp_logf(JP_LOG_DEBUG, "KeyRing: connect_changed_signals\n");
      connected=1;

      for (i=0; i<NUM_KEYRING_CAT_ITEMS; i++) {
	 if (menu_item_category2[i]) {
	    gtk_signal_connect(GTK_OBJECT(menu_item_category2[i]), "toggled",
			       GTK_SIGNAL_FUNC(cb_record_changed), NULL);
	 }
      }

      gtk_signal_connect(GTK_OBJECT(entry_name), "changed",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_connect(GTK_OBJECT(entry_account), "changed",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_connect(GTK_OBJECT(entry_password), "changed",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);
#ifdef ENABLE_GTK2
      g_signal_connect(keyr_note_buffer, "changed",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);
#else
      gtk_signal_connect(GTK_OBJECT(keyr_note), "changed",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);
#endif
   }
   
   /* DISCONNECT */
   if ((con_or_dis==DISCONNECT_SIGNALS) && (connected)) {
      jp_logf(JP_LOG_DEBUG, "KeyRing: disconnect_changed_signals\n");
      connected=0;

      for (i=0; i<NUM_KEYRING_CAT_ITEMS; i++) {
	 if (menu_item_category2[i]) {
	    gtk_signal_disconnect_by_func(GTK_OBJECT(menu_item_category2[i]),
					  GTK_SIGNAL_FUNC(cb_record_changed), 
					  NULL);
	 }
      }

      gtk_signal_disconnect_by_func(GTK_OBJECT(entry_name),
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_disconnect_by_func(GTK_OBJECT(entry_account),
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_disconnect_by_func(GTK_OBJECT(entry_password),
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
#ifdef ENABLE_GTK2
      g_signal_handlers_disconnect_by_func(keyr_note_buffer,
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
#else
      gtk_signal_disconnect_by_func(GTK_OBJECT(keyr_note),
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
#endif
   }
}

static void free_mykeyring_list(struct MyKeyRing **PPmkr)
{
   struct MyKeyRing *mkr, *next_mkr;

   jp_logf(JP_LOG_DEBUG, "KeyRing: free_mykeyring_list\n");
   for (mkr = *PPmkr; mkr; mkr=next_mkr) {
      if (mkr->kr.name)     free(mkr->kr.name);
      if (mkr->kr.account)  free(mkr->kr.account);
      if (mkr->kr.password) free(mkr->kr.password);
      if (mkr->kr.note)     free(mkr->kr.note);
      next_mkr = mkr->next;
      free(mkr);
   }
   *PPmkr=NULL;
}

/* This function gets called when the "delete" button is pressed */
static void cb_delete_keyring(GtkWidget *widget, gpointer data)
{
   struct MyKeyRing *mkr;
   int new_size;
   char buf[0xFFFF];
   buf_rec br;
   int flag;

   jp_logf(JP_LOG_DEBUG, "KeyRing: cb_delete_keyring\n");

   mkr = gtk_clist_get_row_data(GTK_CLIST(clist), clist_row_selected);
   if (!mkr) {
      return;
   }

   /* The record that we want to delete should be written to the pc file
    * so that it can be deleted at sync time.  We need the original record
    * so that if it has changed on the pilot we can warn the user that
    * the record has changed on the pilot. */
   pack_KeyRing(&(mkr->kr), (unsigned char *)buf, 0xFFFF, &new_size);
   
   br.rt = mkr->rt;
   br.unique_id = mkr->unique_id;
   br.attrib = mkr->attrib;
   br.buf = buf;
   br.size = new_size;

   flag = GPOINTER_TO_INT(data);
   if ((flag==MODIFY_FLAG) || (flag==DELETE_FLAG)) {
      jp_delete_record("Keys-Gtkr", &br, flag);
      if (flag==DELETE_FLAG) {
	 /* when we redraw we want to go to the line above the deleted one */
	 if (clist_row_selected>0) {
	    clist_row_selected--;
	 }
      }
   }

   if (flag == DELETE_FLAG) {
      keyr_update_clist();
   }
}

static void cb_undelete_keyring(GtkWidget *widget,
				gpointer   data)
{
   struct MyKeyRing *mkr;
   buf_rec br;
   char buf[0xFFFF];
   int new_size;
   int flag;

   mkr = gtk_clist_get_row_data(GTK_CLIST(clist), clist_row_selected);
   if (mkr==NULL) {
      return;
   }

   jp_logf(JP_LOG_DEBUG, "mkr->unique_id = %d\n",mkr->unique_id);
   jp_logf(JP_LOG_DEBUG, "mkr->rt = %d\n",mkr->rt);

   pack_KeyRing(&(mkr->kr), (unsigned char *)buf, 0xFFFF, &new_size);
   
   br.rt = mkr->rt;
   br.unique_id = mkr->unique_id;
   br.attrib = mkr->attrib;
   br.buf = buf;
   br.size = new_size;

   flag = GPOINTER_TO_INT(data);

   if (flag==UNDELETE_FLAG) {
      if (mkr->rt == DELETED_PALM_REC ||
	  mkr->rt == DELETED_PC_REC)
      {
	 jp_undelete_record("Keys-Gtkr", &br, flag);
      }
      /* Possible later addition of undelete for modified records
      else if (mmemo->rt == MODIFIED_PALM_REC)
      {
	 cb_add_new_record(widget, GINT_TO_POINTER(COPY_FLAG));
      }
      */
   }

   keyr_update_clist();
}

static void cb_cancel(GtkWidget *widget, gpointer data)
{
   set_new_button_to(CLEAR_FLAG);
   keyr_update_clist();
}

/*
 * This is called when the "Clear" button is pressed.
 * It just clears out all the detail fields on the right-hand side.
 */
static int keyr_clear_details()
{
   int new_cat;

   jp_logf(JP_LOG_DEBUG, "KeyRing: cb_clear\n");

   connect_changed_signals(DISCONNECT_SIGNALS);

   gtk_entry_set_text(GTK_ENTRY(entry_name), "");
   gtk_entry_set_text(GTK_ENTRY(entry_account), "");
   gtk_entry_set_text(GTK_ENTRY(entry_password), "");
#ifdef ENABLE_GTK2
   gtk_text_buffer_set_text(GTK_TEXT_BUFFER(keyr_note_buffer), "", -1);
#else
   gtk_text_freeze(GTK_TEXT(keyr_note));
   gtk_text_backward_delete(GTK_TEXT(keyr_note),
			    gtk_text_get_length(GTK_TEXT(keyr_note)));
   gtk_text_thaw(GTK_TEXT(keyr_note));
#endif
   if (keyr_category==CATEGORY_ALL) 
      new_cat = 0;
   else 
      new_cat = keyr_category;

   gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM
				 (menu_item_category2[new_cat]), TRUE);
   gtk_option_menu_set_history(GTK_OPTION_MENU(menu_category2), new_cat);
   
   set_new_button_to(CLEAR_FLAG);

   return EXIT_SUCCESS;
}

/*
 * This function is called when the user presses the "Add" button.
 * We collect all of the data from the GUI and pack it into a keyring
 * record and then write it out.
 */
static void cb_add_new_record(GtkWidget *widget, gpointer data)
{
   struct KeyRing kr;
   buf_rec br;
   unsigned char buf[0x10000];
   int new_size;
   int flag;
   struct MyKeyRing *mkr;
#ifdef ENABLE_GTK2
   GtkTextIter start_iter;
   GtkTextIter end_iter;
#endif
   int i;
   unsigned int unique_id;

   mkr = NULL;
   unique_id=0;

   jp_logf(JP_LOG_DEBUG, "KeyRing: cb_add_new_record\n");

   flag=GPOINTER_TO_INT(data);

   if (flag==CLEAR_FLAG) {
      keyr_clear_details();
      connect_changed_signals(DISCONNECT_SIGNALS);
      set_new_button_to(NEW_FLAG);
      gtk_widget_grab_focus(GTK_WIDGET(entry_name));
      return;
   }
   if ((flag!=NEW_FLAG) && (flag!=MODIFY_FLAG) && (flag!=COPY_FLAG)) {
      return;
   }

   kr.name     = (char *)gtk_entry_get_text(GTK_ENTRY(entry_name));
   kr.account  = (char *)gtk_entry_get_text(GTK_ENTRY(entry_account));
   kr.password = (char *)gtk_entry_get_text(GTK_ENTRY(entry_password));
#ifdef ENABLE_GTK2
   gtk_text_buffer_get_bounds(GTK_TEXT_BUFFER(keyr_note_buffer),&start_iter,&end_iter);
   kr.note = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(keyr_note_buffer),&start_iter,&end_iter,TRUE);
#else
   kr.note = (char *)gtk_editable_get_chars(GTK_EDITABLE(keyr_note), 0, -1);
#endif

   /* TODO: Fixed memory leak here with strdup and not freeing kr
    *       by adding calls to free after pack_KeyRing
    *	    Bigger question is why we even run jp_charset_j2p here */
   kr.name = strdup(kr.name);
   jp_charset_j2p(kr.name, strlen(kr.name)+1);

   kr.account = strdup(kr.account);
   jp_charset_j2p(kr.account, strlen(kr.account)+1);
   
   kr.password = strdup(kr.password);
   jp_charset_j2p(kr.password, strlen(kr.password)+1);
   
   jp_charset_j2p(kr.note, strlen(kr.note)+1);

   pack_KeyRing(&kr, buf, 0xFFFF, &new_size);

   /* free allocated memory now that kr structure is packed into buf */
   if (kr.name)     free(kr.name);
   if (kr.account)  free(kr.account);
   if (kr.password) free(kr.password);
   if (kr.note)     free(kr.note);

   /* Any attributes go here.  Usually just the category */
   /* grab category from menu */
   for (i=0; i<NUM_KEYRING_CAT_ITEMS; i++) {
      if (GTK_IS_WIDGET(menu_item_category2[i])) {
	 if (GTK_CHECK_MENU_ITEM(menu_item_category2[i])->active) {
	    br.attrib = glob_category_number_from_menu_item[i];
	    break;
	 }
      }
   }
   jp_logf(JP_LOG_DEBUG, "category is %d\n", br.attrib);

   br.buf = buf;
   br.size = new_size;

   set_new_button_to(CLEAR_FLAG);

   /* Keep unique ID intact */
   if (flag==MODIFY_FLAG) {
      mkr = gtk_clist_get_row_data(GTK_CLIST(clist), clist_row_selected);
      if (!mkr) {
	 return;
      }
      unique_id = mkr->unique_id;

      if ((mkr->rt==DELETED_PALM_REC) ||
	  (mkr->rt==DELETED_PC_REC)   ||
	  (mkr->rt==MODIFIED_PALM_REC)) {
	 jp_logf(JP_LOG_INFO, _("You can't modify a record that is deleted\n"));
	 return;
      }
   }

   /* Keep unique ID intact */
   if (flag==MODIFY_FLAG) {
      cb_delete_keyring(NULL, data);
      if ((mkr->rt==PALM_REC) || (mkr->rt==REPLACEMENT_PALM_REC)) {
	 br.rt = unique_id;
	 br.rt = REPLACEMENT_PALM_REC;
      } else {
	 br.unique_id = 0;
	 br.rt = NEW_PC_REC;
      }
   } else {
      br.unique_id = 0;
      br.rt = NEW_PC_REC;
   }

   /* Write out the record.  It goes to the .pc3 file until it gets synced */
   jp_pc_write("Keys-Gtkr", &br);

   keyr_update_clist();

   keyring_find(br.unique_id);

   return;
}

/* First pass at password generating code */
static void cb_gen_password(GtkWidget *widget, gpointer data)
{
   GtkWidget *entry;
   int	i,
	length,
	alpha_size,
	numer_size;
   char alpha[] = "abcdfghjklmnpqrstvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
   char numer[] = "1234567890";
   char passwd[MAX_KR_PASS + 1];

   jp_logf(JP_LOG_DEBUG, "KeyRing: cb_gen_password\n");

   entry=data;

   srand(time(NULL) * getpid());
   alpha_size = strlen(alpha);
   numer_size = strlen(numer);

   length = rand() % (MAX_KR_PASS - MIN_KR_PASS) + MIN_KR_PASS;

   for (i = 0; i < length; i++) {
      if ((i % 2) == 0) {
	 passwd[i] = alpha[rand() % alpha_size];
      } else {
	 passwd[i] = numer[rand() % numer_size];
      }
   }

   passwd[length] = '\0';

   gtk_entry_set_text(GTK_ENTRY(entry), passwd);

   return;
}

/*
 * This function just adds the record to the clist on the left side of
 * the screen.
 */
static int display_record(struct MyKeyRing *mkr, int row)
{
   char temp[8];
   char *temp_str;
   int	len;

   jp_logf(JP_LOG_DEBUG, "KeyRing: display_record\n");

   /* Highlight row background depending on status */
   switch (mkr->rt) {
    case NEW_PC_REC:
    case REPLACEMENT_PALM_REC:
      set_bg_rgb_clist_row(clist, row,
		       CLIST_NEW_RED, CLIST_NEW_GREEN, CLIST_NEW_BLUE);

      break;
    case DELETED_PALM_REC:
    case DELETED_PC_REC:
      set_bg_rgb_clist_row(clist, row,
		       CLIST_DEL_RED, CLIST_DEL_GREEN, CLIST_DEL_BLUE);
      break;
    case MODIFIED_PALM_REC:
      set_bg_rgb_clist_row(clist, row,
		       CLIST_MOD_RED, CLIST_MOD_GREEN, CLIST_MOD_BLUE);
      break;
    default:
      gtk_clist_set_row_style(GTK_CLIST(clist), row, NULL);
   }

   gtk_clist_set_row_data(GTK_CLIST(clist), row, mkr);

   if ( (!(mkr->kr.name)) || (mkr->kr.name[0]=='\0') ) {
      sprintf(temp, "#%03d", row);
      gtk_clist_set_text(GTK_CLIST(clist), row, 0, temp);
   } else {
      temp_str = malloc((len = strlen(mkr->kr.name)*2+1));
      multibyte_safe_strncpy(temp_str, mkr->kr.name, len);
      jp_charset_p2j(temp_str, len);
      gtk_clist_set_text(GTK_CLIST(clist), row, 0, temp_str);
      free(temp_str);
   }

   if ( (!(mkr->kr.account)) || (mkr->kr.account[0]=='\0') ) {
      gtk_clist_set_text(GTK_CLIST(clist), row, 1, "");
   } else {
      temp_str = malloc((len = strlen(mkr->kr.account)*2+1));
      multibyte_safe_strncpy(temp_str, mkr->kr.account, len);
      jp_charset_p2j(temp_str, len);
      gtk_clist_set_text(GTK_CLIST(clist), row, 1, temp_str);
      free(temp_str);
   }

   return EXIT_SUCCESS;
}

/*
 * This function lists the records in the clist on the left side of
 * the screen.
 */
static void keyr_update_clist()
{
   int num;
   int entries_shown;
   struct MyKeyRing *temp_list;
   gchar *empty_line[] = { "", "" };
   
   jp_logf(JP_LOG_DEBUG, "KeyRing: keyr_update_clist\n");

   free_mykeyring_list(&glob_keyring_list);

   /* This function takes care of reading the database for us */
   num = get_keyring(&glob_keyring_list, keyr_category);

   keyr_clear_details();

   /* Freeze clist to prevent flicker during updating */
   gtk_clist_freeze(GTK_CLIST(clist));
   connect_changed_signals(DISCONNECT_SIGNALS);
   gtk_signal_disconnect_by_func(GTK_OBJECT(clist),
				 GTK_SIGNAL_FUNC(cb_clist_selection), NULL);
   gtk_clist_clear(GTK_CLIST(clist));

   entries_shown=0;

   for (temp_list = glob_keyring_list; temp_list; temp_list = temp_list->next) {
      gtk_clist_append(GTK_CLIST(clist), empty_line);
      display_record(temp_list, entries_shown);
      entries_shown++;
   }

   /* Sort the clist */
   gtk_clist_sort(GTK_CLIST(clist));

   gtk_signal_connect(GTK_OBJECT(clist), "select_row",
		      GTK_SIGNAL_FUNC(cb_clist_selection), NULL);
   
   /* If there are items in the list, highlight the selected row */
   if (entries_shown>0) {
      /* Select the existing requested row, or row 0 if that is impossible */
      if (clist_row_selected <= entries_shown) {
	 clist_select_row(GTK_CLIST(clist), clist_row_selected, 0);
	 if (!gtk_clist_row_is_visible(GTK_CLIST(clist), clist_row_selected)) {
	    gtk_clist_moveto(GTK_CLIST(clist), clist_row_selected, 0, 0.5, 0.0);
	 }
      }
      else
      {
	 clist_select_row(GTK_CLIST(clist), 0, 0);
      }
   }

   /* Unfreeze clist after all changes */
   gtk_clist_thaw(GTK_CLIST(clist));

   connect_changed_signals(CONNECT_SIGNALS);

   /* return focus to clist after any big operation which requires a redraw */
   gtk_widget_grab_focus(GTK_WIDGET(clist));

   jp_logf(JP_LOG_DEBUG, "KeyRing: leave keyr_update_clist\n");
}

/*
 * This function just displays a record on the right hand side of the screen
 * (the details)
 */
static void cb_clist_selection(GtkWidget      *clist,
			       gint	      row,
			       gint	      column,
			       GdkEventButton *event,
			       gpointer       data)
{
   struct MyKeyRing *mkr;
   int i, item_num, category;
   int keep, b;
   char *temp_str;
   int len;
  
   jp_logf(JP_LOG_DEBUG, "KeyRing: cb_clist_selection\n");

   if ((!event) && (clist_hack)) return;

   /* HACK, see clist hack explanation in memo_gui.c */
   if (clist_hack) {
      keep=record_changed;
      gtk_clist_select_row(GTK_CLIST(clist), clist_row_selected, column);
      b=dialog_save_changed_record(clist, record_changed);
      if (b==DIALOG_SAID_2) {
	 cb_add_new_record(NULL, GINT_TO_POINTER(record_changed));
      }
      set_new_button_to(CLEAR_FLAG);
      clist_select_row(GTK_CLIST(clist), row, column);
      return;
   }

   clist_row_selected = row;

   mkr = gtk_clist_get_row_data(GTK_CLIST(clist), row);
   if (mkr==NULL) {
      return;
   }

   if (mkr->rt == DELETED_PALM_REC ||
      (mkr->rt == DELETED_PC_REC))
      /* Possible later addition of undelete code for modified deleted records
	 || mkr->rt == MODIFIED_PALM_REC
      */
   {
      set_new_button_to(UNDELETE_FLAG);
   }
   else
   {
      set_new_button_to(CLEAR_FLAG);
   }
   
   connect_changed_signals(DISCONNECT_SIGNALS);
   
   category = mkr->attrib & 0x0F;
   item_num=0;
   for (i=0; i<NUM_KEYRING_CAT_ITEMS; i++) {
      if (glob_category_number_from_menu_item[i]==category) {
	 item_num=i;
	 break;
      }
   }

   gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM
				 (menu_item_category2[item_num]), TRUE);
   gtk_option_menu_set_history(GTK_OPTION_MENU(menu_category2), item_num);

   if (mkr->kr.name) {
      temp_str = malloc((len = strlen(mkr->kr.name)*2+1));
      multibyte_safe_strncpy(temp_str, mkr->kr.name, len);
      jp_charset_p2j(temp_str, len);
      gtk_entry_set_text(GTK_ENTRY(entry_name), temp_str);
      free(temp_str);
   } else {
      gtk_entry_set_text(GTK_ENTRY(entry_name), "");
   }

   if (mkr->kr.account) {
      temp_str = malloc((len = strlen(mkr->kr.account)*2+1));
      multibyte_safe_strncpy(temp_str, mkr->kr.account, len);
      jp_charset_p2j(temp_str, len);
      gtk_entry_set_text(GTK_ENTRY(entry_account), temp_str); 
      free(temp_str);
   } else {
      gtk_entry_set_text(GTK_ENTRY(entry_account), "");
   }

   if (mkr->kr.password) {
      temp_str = malloc((len = strlen(mkr->kr.password)*2+1));
      multibyte_safe_strncpy(temp_str, mkr->kr.password, len);
      jp_charset_p2j(temp_str, len);
      gtk_entry_set_text(GTK_ENTRY(entry_password), temp_str); 
      free(temp_str);
   } else {
      gtk_entry_set_text(GTK_ENTRY(entry_password), "");
   }
   
#ifdef ENABLE_GTK2
   gtk_text_buffer_set_text(GTK_TEXT_BUFFER(keyr_note_buffer), "", -1);
#else
   gtk_text_set_point(GTK_TEXT(keyr_note), 0);
   gtk_text_forward_delete(GTK_TEXT(keyr_note),
			   gtk_text_get_length(GTK_TEXT(keyr_note)));
#endif

   if (mkr->kr.note) {
      temp_str = malloc((len = strlen(mkr->kr.note)*2+1));
      multibyte_safe_strncpy(temp_str, mkr->kr.note, len);
      jp_charset_p2j(temp_str, len);
#ifdef ENABLE_GTK2
      gtk_text_buffer_set_text(GTK_TEXT_BUFFER(keyr_note_buffer), temp_str, -1);
#else
      gtk_text_insert(GTK_TEXT(keyr_note), NULL,NULL,NULL, temp_str, -1);
#endif
      free(temp_str);
   }

   connect_changed_signals(CONNECT_SIGNALS);

   jp_logf(JP_LOG_DEBUG, "KeyRing: leaving cb_clist_selection\n");
}

static void cb_category(GtkWidget *item, unsigned int selection)
{
   int b;
   
   jp_logf(JP_LOG_DEBUG, "KeyRing: cb_category\n");

   if ((GTK_CHECK_MENU_ITEM(item))->active) {
      b=dialog_save_changed_record(clist, record_changed);
      if (b==DIALOG_SAID_2) {
	 cb_add_new_record(NULL, GINT_TO_POINTER(record_changed));
      }   

      /* remember the previously used category */
      for (old_category=0; old_category<NUM_KEYRING_CAT_ITEMS; old_category++) {
#ifdef ENABLE_GTK2
	 if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menu_item_category1[old_category])))
	   break;
#else
	 if (GTK_CHECK_MENU_ITEM(menu_item_category1[old_category])->active)
	   break;
#endif
      }

      keyr_category = selection;
      clist_row_selected = 0;
      keyr_update_clist();
   }
}

/*
 * Just a convenience function for passing in an array of strings and getting
 * them all stuffed into a menu.
 */
static int make_menu(char *items[], int menu_index, GtkWidget **Poption_menu,
		     GtkWidget *menu_items[])
{
   int i, item_num;
   GSList *group;
   GtkWidget *option_menu;
   GtkWidget *menu_item;
   GtkWidget *menu;
   
   jp_logf(JP_LOG_DEBUG, "KeyRing: make_menu\n");

   *Poption_menu = option_menu = gtk_option_menu_new();
   
   menu = gtk_menu_new();
   group = NULL;
   
   for (i=0; items[i]; i++) {
      menu_item = gtk_radio_menu_item_new_with_label(group, items[i]);
      menu_items[i] = menu_item;
      if (menu_index==KEYRING_CAT1) {
	 if (i==0) {
	    item_num=CATEGORY_ALL;
	 } else {
	    item_num = i - 1;
	 }
      } else {
	 item_num = i;
      }
      if (menu_index==KEYRING_CAT1) {
	 gtk_signal_connect(GTK_OBJECT(menu_item), "activate",
			    GTK_SIGNAL_FUNC(cb_category), 
			    GINT_TO_POINTER(item_num));
      }
      group = gtk_radio_menu_item_group(GTK_RADIO_MENU_ITEM(menu_item));
      gtk_menu_append(GTK_MENU(menu), menu_item);
      gtk_widget_show(menu_item);
   }
   gtk_option_menu_set_menu(GTK_OPTION_MENU(option_menu), menu);
   /* Make this one show up by default */
   gtk_option_menu_set_history(GTK_OPTION_MENU(option_menu), 0);

   return EXIT_SUCCESS;
}

/* 
 * This function makes all of the menus on the screen.
 */
static void make_menus()
{
   GList *records;
   struct CategoryAppInfo ai;
   unsigned char *buf;
   int buf_size;
   int i, count;
   char all[]=N_("All");
   char *categories[18];
     
   jp_logf(JP_LOG_DEBUG, "KeyRing: make_menus\n");

   /* This gets the application specific data out of the database for us.
    * We still need to write a function to unpack it from its blob form. */
   
   jp_get_app_info("Keys-Gtkr", &buf, &buf_size);

   /* This call should work, but the appinfo is too small, so we do it */
   /* Keyring is not using a legal category app info structure */
   /* unpack_CategoryAppInfo(&ai, buf, buf_size+4); */
   
   /* I'm going to be lazy and only get the names, since that's all I use */
   for (i=0; i<NUM_KEYRING_CAT_ITEMS; i++) {
      memcpy(&ai.name[i][0], buf+i*16+2, 16);
   }

   free(buf);
   
   categories[0]=all;
   for (i=0, count=0; i<NUM_KEYRING_CAT_ITEMS; i++) {
      glob_category_number_from_menu_item[i]=0;
      if (ai.name[i][0]=='\0') {
	 continue;
      }
      jp_charset_p2j(ai.name[i], 16);
      categories[count+1]=ai.name[i];
      glob_category_number_from_menu_item[count++]=i;
   }
   categories[count+1]=NULL;
   
   records=NULL;
   
   /* TODO: We should probably use the standard menu making function
    * make_category_menu in utils.c for this rather than code a new version */
   make_menu(categories, KEYRING_CAT1, &menu_category1, menu_item_category1);
   /* Skip the ALL for this menu */
   make_menu(&categories[1], KEYRING_CAT2, &menu_category2, menu_item_category2);   
}

/***** PASSWORD GUI *****/

/*
 * Start of Dialog window code
 */
struct dialog_data {
   GtkWidget *entry;
   int button_hit;
   char text[PASSWD_LEN+2];
};

static void cb_dialog_button(GtkWidget *widget,
			     gpointer   data)
{
   struct dialog_data *Pdata;
   GtkWidget *w;

   /* Find the main window from some widget */
   w = GTK_WIDGET(gtk_widget_get_toplevel(widget));
   
   if (GTK_IS_WINDOW(w)) {
      Pdata = gtk_object_get_data(GTK_OBJECT(w), "dialog_data");
      if (Pdata) {
	 Pdata->button_hit = GPOINTER_TO_INT(data);
      }
      gtk_widget_destroy(GTK_WIDGET(w));
   }
}

static gboolean cb_destroy_dialog(GtkWidget *widget)
{
   struct dialog_data *Pdata;
   const char *entry;

   Pdata = gtk_object_get_data(GTK_OBJECT(widget), "dialog_data");
   if (!Pdata) {
      return TRUE;
   }
   entry = gtk_entry_get_text(GTK_ENTRY(Pdata->entry));
   
   if (entry) {
      strncpy(Pdata->text, entry, PASSWD_LEN);
      Pdata->text[PASSWD_LEN]='\0';
      /* Wipe out password data */
      gtk_entry_set_text(GTK_ENTRY(Pdata->entry), "	     ");
   }

   gtk_main_quit();

   return TRUE;
}

/*
 * returns 2 if OK was pressed, 1 if cancel was hit
 */
static int dialog_password(GtkWindow *main_window, 
			   char *ascii_password, 
			   int reason)
{
   GtkWidget *button, *label;
   GtkWidget *hbox1, *vbox1;
   GtkWidget *dialog;
   GtkWidget *entry;
   struct dialog_data *Pdata;
   int ret;

   if (!ascii_password) {
      return EXIT_FAILURE;
   }
   ascii_password[0]='\0';
   ret = 2; 
  
   dialog = gtk_widget_new(GTK_TYPE_WINDOW,
			   "type", GTK_WINDOW_TOPLEVEL,
			   "title", "KeyRing",
			   NULL);
   
   gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_MOUSE);

   gtk_signal_connect(GTK_OBJECT(dialog), "destroy",
		      GTK_SIGNAL_FUNC(cb_destroy_dialog), dialog);

   gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
      
   if (main_window) {
      if (GTK_IS_WINDOW(main_window)) {
	 gtk_window_set_transient_for(GTK_WINDOW(dialog), 
				      GTK_WINDOW(main_window));
      }
   }

   hbox1 = gtk_hbox_new(FALSE, 2);
   gtk_container_add(GTK_CONTAINER(dialog), hbox1);
#ifdef ENABLE_GTK2
   gtk_box_pack_start(GTK_BOX(hbox1), gtk_image_new_from_stock(GTK_STOCK_DIALOG_AUTHENTICATION, GTK_ICON_SIZE_DIALOG), FALSE, FALSE, 2);
#endif

   vbox1 = gtk_vbox_new(FALSE, 2);

   gtk_container_set_border_width(GTK_CONTAINER(vbox1), 5);
   
   gtk_container_add(GTK_CONTAINER(hbox1), vbox1);

   hbox1 = gtk_hbox_new(TRUE, 2);
   gtk_container_set_border_width(GTK_CONTAINER(hbox1), 5);
   gtk_box_pack_start(GTK_BOX(vbox1), hbox1, FALSE, FALSE, 2);

   /* Label */
   if (reason==PASSWD_ENTER_RETRY) {
      label = gtk_label_new(_("Incorrect, Reenter KeyRing Password"));
   } else if (reason==PASSWD_ENTER_NEW) {
      label = gtk_label_new(_("Enter a NEW KeyRing Password"));
   } else {
      label = gtk_label_new(_("Enter KeyRing Password"));
   }
   gtk_box_pack_start(GTK_BOX(hbox1), label, FALSE, FALSE, 2);

   entry = gtk_entry_new_with_max_length(32);
   gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
   gtk_signal_connect(GTK_OBJECT(entry), "activate",
		      GTK_SIGNAL_FUNC(cb_dialog_button),
		      GINT_TO_POINTER(DIALOG_SAID_2));
   gtk_box_pack_start(GTK_BOX(hbox1), entry, TRUE, TRUE, 1);

   /* Button Box */
   hbox1 = gtk_hbutton_box_new();
   gtk_button_box_set_layout(GTK_BUTTON_BOX (hbox1), GTK_BUTTONBOX_END);
   gtk_button_box_set_spacing(GTK_BUTTON_BOX(hbox1), 6);
   gtk_container_set_border_width(GTK_CONTAINER(hbox1), 5);
   gtk_box_pack_start(GTK_BOX(vbox1), hbox1, FALSE, FALSE, 2);

   /* Buttons */
#ifdef ENABLE_STOCK_BUTTONS
   button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
#else
   button = gtk_button_new_with_label(_("Cancel"));
#endif
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_dialog_button),
		      GINT_TO_POINTER(DIALOG_SAID_1));
   gtk_box_pack_start(GTK_BOX(hbox1), button, FALSE, FALSE, 1);

#ifdef ENABLE_STOCK_BUTTONS
   button = gtk_button_new_from_stock(GTK_STOCK_OK);
#else
   button = gtk_button_new_with_label(_("OK"));
#endif
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_dialog_button),
		      GINT_TO_POINTER(DIALOG_SAID_2));
   gtk_box_pack_start(GTK_BOX(hbox1), button, FALSE, FALSE, 1);

   Pdata = malloc(sizeof(struct dialog_data));
   if (Pdata) {
      /* Set the default button pressed to CANCEL */
      Pdata->button_hit = DIALOG_SAID_1;
      Pdata->entry=entry;
      Pdata->text[0]='\0';
   }
   gtk_object_set_data(GTK_OBJECT(dialog), "dialog_data", Pdata);		     
   gtk_widget_grab_focus(GTK_WIDGET(entry));
     
   gtk_widget_show_all(dialog);

   gtk_main();
   
   if (Pdata->button_hit==DIALOG_SAID_1) {
      ret = 1;
   }
   if (Pdata->button_hit==DIALOG_SAID_2) {
      ret = 2;
   }
   strncpy(ascii_password, Pdata->text, PASSWD_LEN);
   memset(Pdata->text, 0, PASSWD_LEN);

   free(Pdata);

   return ret;
}

/***** End Password GUI *****/

static int check_for_db()
{
   char full_name[1024];
   int max_size=1024;
   char *home;
   char file[]="Keys-Gtkr.pdb";
   struct stat buf;

   home = getenv("JPILOT_HOME");
   if (!home) { /* Not home; */
      home = getenv("HOME");
      if (!home) {
	 jp_logf(JP_LOG_WARN, _("Can't get HOME environment variable\n"));
	 return EXIT_FAILURE;
      }
   }
   if (strlen(home)>(max_size-strlen(file)-strlen("/.jpilot/")-2)) {
      jp_logf(JP_LOG_WARN, _("Your HOME environment variable is too long(>1024)\n"));
      return EXIT_FAILURE;
   }
   sprintf(full_name, "%s/.jpilot/%s", home, file);
   if (stat(full_name, &buf)) {
      jp_logf(JP_LOG_FATAL, _("KeyRing: file %s not found.\n"), full_name);
      jp_logf(JP_LOG_FATAL, _("KeyRing: Try Syncing.\n"), full_name);
      return EXIT_FAILURE;
   }
		 
   return EXIT_SUCCESS;
}

/*
 * returns EXIT_SUCCESS on password correct, 
 *	   EXIT_FAILURE on password incorrect, 
 *	   <0		on error
 */
static int verify_pasword(char *ascii_password)
{
   GList *records;
   GList *temp_list;
   buf_rec *br;
   int password_not_correct;

   jp_logf(JP_LOG_DEBUG, "KeyRing: verify_pasword\n");

   if (check_for_db()) {
      return EXIT_FAILURE;
   }

   /* TODO: Maybe keep records in memory for performance */
   /* This function takes care of reading the Database for us */
   records=NULL;
   if (jp_read_DB_files("Keys-Gtkr", &records) == -1)
     return EXIT_SUCCESS;

   password_not_correct = 1;
   /* Find special record marked as password */
   for (temp_list = records; temp_list; temp_list = temp_list->prev) {
      if (temp_list->data) {
	 br=temp_list->data;
      } else {
	 continue;
      }
      if (!br->buf) {
	 continue;
      }

      if ((br->rt == DELETED_PALM_REC) || (br->rt == MODIFIED_PALM_REC)) {
	 continue;
      }

      /* This record should be record 0 and is the hash-key record */
      if (br->attrib & dlpRecAttrSecret) {
	 password_not_correct = 
	   set_password_hash(br->buf, br->size, ascii_password);
	 break;
      }
   }

   jp_free_DB_records(&records);

   if (password_not_correct) 
      return EXIT_FAILURE;
   else
      return EXIT_SUCCESS;
}

/* This is a mandatory plugin function. */
void plugin_version(int *major_version, int *minor_version)
{
   *major_version=0;
   *minor_version=99;
}

/* This is a mandatory plugin function. */
int plugin_get_name(char *name, int len)
{
   jp_logf(JP_LOG_DEBUG, "KeyRing: plugin_get_name\n");
   strncpy(name, "KeyRing 0.01", len);
   return EXIT_SUCCESS;
}

/*
 * This is an optional plugin function.
 * This is the name that will show up in the plugins menu in J-Pilot.
 */
int plugin_get_menu_name(char *name, int len)
{
   strncpy(name, _("KeyRing"), len);
   return EXIT_SUCCESS;
}

/*
 * This is an optional plugin function.
 * This is the name that will show up in the plugins help menu in J-Pilot.
 * If this function is used then plugin_help must be also.
 */
int plugin_get_help_name(char *name, int len)
{
   g_snprintf(name, len, _("About %s"), _("KeyRing"));
   return EXIT_SUCCESS;
}

/*
 * This is an optional plugin function.
 * This is the palm database that will automatically be synced.
 */
int plugin_get_db_name(char *name, int len)
{
   strncpy(name, "Keys-Gtkr", len);
   return EXIT_SUCCESS;
}

/*
 * This is a plugin callback function which provides information
 * to the user about the plugin.
 */
int plugin_help(char **text, int *width, int *height)
{
   /* We could also pass back *text=NULL
    * and implement whatever we wanted to here.
    */
   *text = strdup(
	   /*-------------------------------------------*/
	   "KeyRing plugin for J-Pilot was written by\n"
	   "Judd Montgomery (c) 2001.\n"
	   "judd@jpilot.org\n"
	   "http://jpilot.org\n"
	   "KeyRing is a free PalmOS program for storing\n"
	   "passwords and other information encrypted\n"
	   "http://gnukeyring.sourceforge.net\n"
	   );
   *height = 0;
   *width  = 0;
   
   return EXIT_SUCCESS;
}
	 
/*
 * This is a plugin callback function that is executed when J-Pilot starts up.
 * base_dir is where J-Pilot is compiled to be installed at (e.g. /usr/local/)
 */
int plugin_startup(jp_startup_info *info)
{
   jp_init();

   jp_logf(JP_LOG_DEBUG, "KeyRing: plugin_startup\n");
   if (info) {
      if (info->base_dir) {
	 jp_logf(JP_LOG_DEBUG, "KeyRing: base_dir = [%s]\n", info->base_dir);
      }
   }
   return EXIT_SUCCESS;
}

/*
 * This is a plugin callback function that is executed before a sync occurs.
 * Any sync preperation can be done here.
 */
int plugin_pre_sync(void)
{
   jp_logf(JP_LOG_DEBUG, "KeyRing: plugin_pre_sync\n");
   return EXIT_SUCCESS;
}

/*
 * This is a plugin callback function that is executed during a sync.
 * Notice that I don't need to sync the KeyRing application.  Since I used
 * the plugin_get_db_name call to tell J-Pilot what to sync for me.  It will
 * be done automatically.
 */
int plugin_sync(int sd)
{
   jp_logf(JP_LOG_DEBUG, "KeyRing: plugin_sync\n");
   return EXIT_SUCCESS;
}

/*
 * This is a plugin callback function called after a sync.
 */
int plugin_post_sync(void)
{
   jp_logf(JP_LOG_DEBUG, "KeyRing: plugin_post_sync\n");
   return EXIT_SUCCESS;
}

static int add_search_result(const char *line, 
			     int unique_id, 
			     struct search_result **sr)
{
   struct search_result *new_sr;

   jp_logf(JP_LOG_DEBUG, "KeyRing: add_search_result for [%s]\n", line);

   new_sr=malloc(sizeof(struct search_result));
   if (!new_sr) {
      return EXIT_FAILURE;
   }
   new_sr->unique_id=unique_id;
   new_sr->line=strdup(line);
   new_sr->next = *sr;
   *sr = new_sr;

   return EXIT_SUCCESS;
}

/*
 * This function is called when the user does a search.  It should return
 * records which match the search string.
 */
int plugin_search(const char *search_string, int case_sense,
		  struct search_result **sr)
{
   struct MyKeyRing *mkr_list;
   struct MyKeyRing *temp_list;
   struct MyKeyRing mkr;
   int num, count;
   char *line;

   jp_logf(JP_LOG_DEBUG, "KeyRing: plugin_search\n");

   *sr=NULL;
   mkr_list=NULL;

   if (!plugin_active) {
      return 0;
   }

   /* This function takes care of reading the Database for us */
   num = get_keyring(&mkr_list, CATEGORY_ALL);
   if (-1 == num)
     return 0;

   count = 0;
   
   /* Search through returned records */
   for (temp_list = mkr_list; temp_list; temp_list = temp_list->next) {
      mkr = *temp_list;
      line = NULL;

      /* find in record name */
      if (jp_strstr(mkr.kr.name, search_string, case_sense))
	 line = mkr.kr.name;

      /* find in record account */
      if (jp_strstr(mkr.kr.account, search_string, case_sense))
	 line = mkr.kr.account;

      /* find in record password */
      if (jp_strstr(mkr.kr.password, search_string, case_sense))
	 line = mkr.kr.password;

      /* find in record note */
      if (jp_strstr(mkr.kr.note, search_string, case_sense))
	 line = mkr.kr.note;

      if (line) {
	 /* Add it to our result list */
	 jp_logf(JP_LOG_DEBUG, "KeyRing: calling add_search_result\n");
	 add_search_result(line, mkr.unique_id, sr);
	 jp_logf(JP_LOG_DEBUG, "KeyRing: back from add_search_result\n");
	 count++;
      }
   }

   free_mykeyring_list(&mkr_list);

   return count;
}

static int keyring_find(int unique_id)
{
   int r, found_at;
   
   jp_logf(JP_LOG_DEBUG, "KeyRing: keyring_find\n");

   r = clist_find_id(clist,
		     unique_id,
		     &found_at);
   if (r) {
      clist_select_row(GTK_CLIST(clist), found_at, 0);
      if (!gtk_clist_row_is_visible(GTK_CLIST(clist), found_at)) {
	 gtk_clist_moveto(GTK_CLIST(clist), found_at, 0, 0.5, 0.0);
      }
   }

   return EXIT_SUCCESS;
}

/*
 * This is a plugin callback function called during Jpilot program exit.
 */
int plugin_exit_cleanup(void)
{
   jp_logf(JP_LOG_DEBUG, "KeyRing: plugin_exit_cleanup\n");
   return EXIT_SUCCESS;
}

/*
 * This is a plugin callback function called when the plugin is terminated
 * such as by switching to another application(ToDo, Memo, etc.)
 */
int plugin_gui_cleanup() {
   int b;
   
   jp_logf(JP_LOG_DEBUG, "KeyRing: plugin_gui_cleanup\n");

   b=dialog_save_changed_record(clist, record_changed);
   if (b==DIALOG_SAID_2) {
      cb_add_new_record(NULL, GINT_TO_POINTER(record_changed));
   }

   free_mykeyring_list(&glob_keyring_list);

   /* if the password was correct */
   if (plugin_last_time) {
      plugin_last_time = time(NULL);
   }
   plugin_active = FALSE;

   connect_changed_signals(DISCONNECT_SIGNALS);

   /* the pane may not exist if the wrong password is entered and
    * the GUI is not built */
   if (pane)
   {
      /* Remove the accelerators */
#ifndef ENABLE_STOCK_BUTTONS
#ifdef ENABLE_GTK2
      gtk_window_remove_accel_group(GTK_WINDOW(gtk_widget_get_toplevel(pane)), accel_group);
#else
      gtk_accel_group_detach(accel_group, GTK_OBJECT(gtk_widget_get_toplevel(pane)));
#endif
#endif

      /* Record the position of the window pane to restore later */
#ifdef ENABLE_GTK2
      set_pref(PREF_KEYRING_PANE, gtk_paned_get_position(GTK_PANED(pane)), NULL, TRUE);
#else
      set_pref(PREF_KEYRING_PANE, GTK_PANED(pane)->handle_xpos, NULL, TRUE);
#endif

      pane = NULL;
   }

   return EXIT_SUCCESS;
}

/*
 * This function is called by J-Pilot when the user selects this plugin
 * from the plugin menu, or from the search window when a search result
 * record is chosen.  In the latter case, unique ID will be set.  This
 * application should go directly to that record in the case.
 */
int plugin_gui(GtkWidget *vbox, GtkWidget *hbox, unsigned int unique_id)
{
   GtkWidget *vbox1, *vbox2;
   GtkWidget *hbox_temp;
   GtkWidget *button;
   GtkWidget *label;
#ifndef ENABLE_GTK2
   GtkWidget *vscrollbar;
#endif
   GtkWidget *table;
   GtkWindow *w;
   GtkWidget *separator;
   long ivalue;
   char ascii_password[PASSWD_LEN];
   int r;
   int password_not_correct;
   char *titles[] = { N_("Name"), N_("Account") };
   int retry;
   int cycle_category = FALSE;

   jp_logf(JP_LOG_DEBUG, "KeyRing: plugin gui started, unique_id=%d\n", unique_id);

   if (check_for_db()) {
      return EXIT_FAILURE;
   }
   
   /* Find the main window from some widget */
   w = GTK_WINDOW(gtk_widget_get_toplevel(hbox));

#if 0
   /* Change Password button */
   button = gtk_button_new_with_label(_("Change\nKeyRing\nPassword"));
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_change_password), NULL);
   gtk_box_pack_start(GTK_BOX(vbox), button, TRUE, TRUE, 0);
#endif

   if (difftime(time(NULL), plugin_last_time) > PLUGIN_MAX_INACTIVE_TIME) {
      /* reset last time we entered */
      plugin_last_time = 0;

      password_not_correct = 1;
      retry = PASSWD_ENTER;
      while (password_not_correct) {
	 r = dialog_password(w, ascii_password, retry);
	 retry = PASSWD_ENTER_RETRY;
	 if (r != 2) {
	    memset(ascii_password, 0, PASSWD_LEN-1);
	    return 0;
	 }
	 password_not_correct = (verify_pasword(ascii_password) > 0);
      }
      memset(ascii_password, 0, PASSWD_LEN-1);
   }
   else
      cycle_category = TRUE;

   /* called to display the result of a search */
   if (unique_id) {
      cycle_category = FALSE;
   }

   /* plugin entered with correct password */
   plugin_last_time = time(NULL);
   plugin_active = TRUE;

   record_changed=CLEAR_FLAG;
   clist_row_selected = 0;

   pane = gtk_hpaned_new();
   get_pref(PREF_KEYRING_PANE, &ivalue, NULL);
   gtk_paned_set_position(GTK_PANED(pane), ivalue + PANE_CREEP);

   gtk_box_pack_start(GTK_BOX(hbox), pane, TRUE, TRUE, 5);

   /* left and right main boxes */
   vbox1 = gtk_vbox_new(FALSE, 0);
   vbox2 = gtk_vbox_new(FALSE, 0);
   gtk_paned_pack1(GTK_PANED(pane), vbox1, TRUE, FALSE);
   gtk_paned_pack2(GTK_PANED(pane), vbox2, TRUE, FALSE);

   gtk_widget_set_usize(GTK_WIDGET(vbox1), 0, 230);
   gtk_widget_set_usize(GTK_WIDGET(vbox2), 0, 230);

   /* Make accelerators for some buttons window */
#ifndef ENABLE_STOCK_BUTTONS
   accel_group = gtk_accel_group_new();
#ifdef ENABLE_GTK2
   gtk_window_add_accel_group(GTK_WINDOW(gtk_widget_get_toplevel(vbox)), accel_group);
#else
   gtk_accel_group_attach(accel_group, GTK_OBJECT(gtk_widget_get_toplevel(vbox)));
#endif
#endif

   /* Make the menus */
   make_menus();

   /**********************************************************************/
   /* Left half of screen */
   /**********************************************************************/
   /* Make a temporary hbox */
   hbox_temp = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox1), hbox_temp, FALSE, FALSE, 0);
   
   label = gtk_label_new(_("Category: "));
   gtk_box_pack_start(GTK_BOX(hbox_temp), label, FALSE, FALSE, 0);
   gtk_box_pack_start(GTK_BOX(hbox_temp), menu_category1, TRUE, TRUE, 0);

   /* Scrolled Window */
   scrolled_window = gtk_scrolled_window_new(NULL, NULL);
   gtk_container_set_border_width(GTK_CONTAINER(scrolled_window), 0);
   gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
   gtk_box_pack_start(GTK_BOX(vbox1), scrolled_window, TRUE, TRUE, 0);
   
   /* Clist */
   clist = gtk_clist_new_with_titles(2, titles);
   clist_hack=FALSE;
   gtk_signal_connect(GTK_OBJECT(clist), "select_row",
		      GTK_SIGNAL_FUNC(cb_clist_selection),
		      NULL);
   gtk_clist_set_selection_mode(GTK_CLIST(clist), GTK_SELECTION_BROWSE);
   gtk_clist_set_column_width(GTK_CLIST(clist), 0, 150);
   gtk_clist_set_column_width(GTK_CLIST(clist), 1, 60);
   gtk_container_add(GTK_CONTAINER(scrolled_window), GTK_WIDGET(clist));
   
   gtk_clist_set_sort_column(GTK_CLIST(clist), 0);
   gtk_clist_set_sort_type(GTK_CLIST(clist), GTK_SORT_ASCENDING);
   
   /**********************************************************************/
   /* Right half of screen */
   /**********************************************************************/
   hbox_temp = gtk_hbox_new(FALSE, 3);
   gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, FALSE, FALSE, 0);

   /* Cancel button */
   CREATE_BUTTON(cancel_record_button, _("Cancel"), CANCEL, _("Cancel the modifications"), GDK_Escape, 0, "ESC")
   gtk_signal_connect(GTK_OBJECT(cancel_record_button), "clicked",
		      GTK_SIGNAL_FUNC(cb_cancel), NULL);

   /* Delete Button */
   CREATE_BUTTON(delete_record_button, _("Delete"), DELETE, _("Delete the selected record"), GDK_d, GDK_CONTROL_MASK, "Ctrl+D");
   gtk_signal_connect(GTK_OBJECT(delete_record_button), "clicked",
		      GTK_SIGNAL_FUNC(cb_delete_keyring),
		      GINT_TO_POINTER(DELETE_FLAG));

   /* Undelete Button */
   CREATE_BUTTON(undelete_record_button, _("Undelete"), UNDELETE, _("Undelete the selected record"), 0, 0, "")
   gtk_signal_connect(GTK_OBJECT(undelete_record_button), "clicked",
		      GTK_SIGNAL_FUNC(cb_undelete_keyring),
		      GINT_TO_POINTER(UNDELETE_FLAG));

   /* Copy button */
   CREATE_BUTTON(copy_record_button, _("Copy"), COPY, _("Copy the selected record"), GDK_o, GDK_CONTROL_MASK, "Ctrl+O");
   gtk_signal_connect(GTK_OBJECT(copy_record_button), "clicked",
		      GTK_SIGNAL_FUNC(cb_add_new_record),
		      GINT_TO_POINTER(COPY_FLAG));

   /* New Record button */
   CREATE_BUTTON(new_record_button, _("New Record"), NEW, _("Add a new record"), GDK_n, GDK_CONTROL_MASK, "Ctrl+N")
   gtk_signal_connect(GTK_OBJECT(new_record_button), "clicked",
		      GTK_SIGNAL_FUNC(cb_add_new_record),
		      GINT_TO_POINTER(CLEAR_FLAG));

   /* Add Record button */
   CREATE_BUTTON(add_record_button, _("Add Record"), ADD, _("Add the new record"), GDK_Return, GDK_CONTROL_MASK, "Ctrl+Enter")
   gtk_signal_connect(GTK_OBJECT(add_record_button), "clicked",
		      GTK_SIGNAL_FUNC(cb_add_new_record),
		      GINT_TO_POINTER(NEW_FLAG));
#ifndef ENABLE_STOCK_BUTTONS
   gtk_widget_set_name(GTK_WIDGET(GTK_LABEL(GTK_BIN(add_record_button)->child)),
		       "label_high");
#endif

   /* Apply Changes button */
   CREATE_BUTTON(apply_record_button, _("Apply Changes"), APPLY, _("Commit the modifications"), GDK_Return, GDK_CONTROL_MASK, "Ctrl+Enter")
   gtk_signal_connect(GTK_OBJECT(apply_record_button), "clicked",
		      GTK_SIGNAL_FUNC(cb_add_new_record),
		      GINT_TO_POINTER(MODIFY_FLAG));
#ifndef ENABLE_STOCK_BUTTONS
   gtk_widget_set_name(GTK_WIDGET(GTK_LABEL(GTK_BIN(apply_record_button)->child)),
		       "label_high");
#endif
   
   /*Separator */
   separator = gtk_hseparator_new();
   gtk_box_pack_start(GTK_BOX(vbox2), separator, FALSE, FALSE, 5);

   /* Table */
   table = gtk_table_new(4, 10, FALSE);
   gtk_table_set_row_spacings(GTK_TABLE(table),0);
   gtk_table_set_col_spacings(GTK_TABLE(table),0);
   gtk_box_pack_start(GTK_BOX(vbox2), table, FALSE, FALSE, 0);
   
   /* Category Menu */
   label = gtk_label_new(_("Category: "));
   gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(label), 0, 1, 0, 1);
   gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(menu_category2), 1, 10, 0, 1);
   gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);

   /* name Entry */
   label = gtk_label_new(_("name: "));
   entry_name = gtk_entry_new();
   gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(label), 0, 1, 1, 2);
   gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(entry_name), 1, 10, 1, 2);
   gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);

   /* account Entry */
   label = gtk_label_new(_("account: "));
   entry_account = gtk_entry_new();
   gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(label), 0, 1, 2, 3);
   gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(entry_account), 1, 10, 2, 3);
   gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);

   /* password */
   label = gtk_label_new(_("password: "));
   entry_password = gtk_entry_new();
   gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(label), 0, 1, 3, 4);
   gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(entry_password), 1, 9, 3, 4);
   gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);

   /* Button for random password */
   button = gtk_button_new_with_label(_("Generate Password"));
   gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(button), 9, 10, 3, 4);
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_gen_password), entry_password);

   /* Note textbox */
   label = gtk_label_new(_("Note"));
   gtk_box_pack_start(GTK_BOX(vbox2), label, FALSE, FALSE, 0);

   hbox_temp = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, TRUE, TRUE, 0);

#ifdef ENABLE_GTK2
   keyr_note = gtk_text_view_new();
   keyr_note_buffer = G_OBJECT(gtk_text_view_get_buffer(GTK_TEXT_VIEW(keyr_note)));
   gtk_text_view_set_editable(GTK_TEXT_VIEW(keyr_note), TRUE);
   gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(keyr_note), GTK_WRAP_WORD);

   scrolled_window = gtk_scrolled_window_new(NULL, NULL);
   gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
				   GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
   gtk_container_set_border_width(GTK_CONTAINER(scrolled_window), 1);
   gtk_container_add(GTK_CONTAINER(scrolled_window), keyr_note);
   gtk_box_pack_start_defaults(GTK_BOX(hbox_temp), scrolled_window);
#else
   keyr_note = gtk_text_new(NULL, NULL);
   gtk_text_set_editable(GTK_TEXT(keyr_note), TRUE);
   gtk_text_set_word_wrap(GTK_TEXT(keyr_note), TRUE);
   vscrollbar = gtk_vscrollbar_new(GTK_TEXT(keyr_note)->vadj);
   gtk_box_pack_start(GTK_BOX(hbox_temp), keyr_note, TRUE, TRUE, 0);
   gtk_box_pack_start(GTK_BOX(hbox_temp), vscrollbar, FALSE, FALSE, 0);
#endif

   /**********************************************************************/

   gtk_widget_show_all(hbox);
   gtk_widget_show_all(vbox);
   
   gtk_widget_hide(add_record_button);
   gtk_widget_hide(apply_record_button);
   gtk_widget_hide(undelete_record_button);
   gtk_widget_hide(cancel_record_button);

   if (cycle_category) {
      old_category++;

      if (menu_item_category1[old_category] == NULL) {
	 old_category = 0;
	 keyr_category = CATEGORY_ALL;
      }

      gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_item_category1[old_category]), TRUE);
      gtk_option_menu_set_history(GTK_OPTION_MENU(menu_category1), old_category);
   }
   else
   {
      keyr_category = CATEGORY_ALL;
   }

   keyr_update_clist();

   if (unique_id) {
      keyring_find(unique_id);
   }

   return EXIT_SUCCESS;
}


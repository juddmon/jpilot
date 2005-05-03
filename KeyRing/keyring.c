/* $Id: keyring.c,v 1.37 2005/05/03 02:25:40 judd Exp $ */

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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <gtk/gtk.h>
#include <sys/stat.h>
#include <unistd.h>

/* Opensll header files */
#include <openssl/md5.h>
#include <openssl/des.h>

#include "libplugin.h"
#include "utils.h"

#include <pi-appinfo.h>
#include <pi-dlp.h>
#include <pi-file.h>

#include "../i18n.h"

#define KEYRING_CAT1 1
#define KEYRING_CAT2 2

#define PASSWD_ENTER       0
#define PASSWD_ENTER_RETRY 1
#define PASSWD_ENTER_NEW   2

#define PASSWD_LEN 100

#define CONNECT_SIGNALS 400
#define DISCONNECT_SIGNALS 401

#define NUM_KEYRING_CAT_ITEMS 16

/* Global vars */
/* This is the category that is currently being displayed */
static int show_category = CATEGORY_ALL;
static int glob_category_number_from_menu_item[NUM_KEYRING_CAT_ITEMS];

static GtkWidget *clist;
static GtkWidget *entry_name;
static GtkWidget *entry_account;
static GtkWidget *entry_password;
static GtkWidget *text_note;
#ifdef ENABLE_GTK2
static GObject   *text_note_buffer;
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
static GtkWidget *copy_record_button;

static int record_changed;
static int clist_hack;

static int glob_detail_category;
static int clist_row_selected;

static void display_records();

static void connect_changed_signals(int con_or_dis);

static void cb_clist_selection(GtkWidget      *clist,
			       gint           row,
			       gint           column,
			       GdkEventButton *event,
			       gpointer       data);

static void cb_add_new_record(GtkWidget *widget, gpointer data);

static int check_for_db();
static int verify_pasword(char *ascii_password);
static int dialog_password(GtkWindow *main_window, char *ascii_password, int reason);
static int keyring_find(int unique_id);

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
  
struct KeyRing {
   char *name; /* Unencrypted */
   char *account; /* Encrypted */
   char *password; /* Encrypted */
   char *note; /* Encrypted */
   unsigned long last_changed; /* Encrypted */
 };
/* 
 * This is my wrapper to the  keyringstructure so that I can put
 * a few more fields in with it.
 */
struct MyKeyRing {
   PCRecType rt;
   unsigned int unique_id;
   unsigned char attrib;
   struct KeyRing kr;
   struct MyKeyRing *next;
};

time_t plugin_last_time = 0;
/* re-ask password PLUGIN_MAX_INACTIVE_TIME seconds after deselecting the plugin */
#define PLUGIN_MAX_INACTIVE_TIME 1
gboolean plugin_active = FALSE;

struct MyKeyRing *glob_keyring_list=NULL;

static int pack_KeyRing(struct KeyRing *kr, unsigned char *buf, int buf_size,
			int *wrote_size)
{
   int n;
   int i;
   char empty[]="";

   jp_logf(JP_LOG_DEBUG, "KeyRing: pack_KeyRing()\n");

   *wrote_size=0;

   if (!(kr->name)) kr->name=empty;
   if (!(kr->account)) kr->account=empty;
   if (!(kr->password)) kr->password=empty;
   if (!(kr->note)) kr->note=empty;

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
#define SALT_SIZE 4
#define MESSAGE_BUF_SIZE 64
#define MD5_HASH_SIZE 16

/*
 * Return -1 if password isn't good.
 * Return 0 if good and global and also sets s1, and s2 set
 */
static int set_password_hash(unsigned char *buf, int buf_size, char *passwd)
{
   unsigned char buffer[MESSAGE_BUF_SIZE];
   unsigned char md[MD5_HASH_SIZE];
   
   /* Must wipe passwd out of memory after using it */
   
   if (buf_size < MD5_HASH_SIZE) {
      return EXIT_FAILURE;
   }
   memset(buffer, 0, MESSAGE_BUF_SIZE);
   memcpy(buffer, buf, SALT_SIZE);
   strncpy(buffer+SALT_SIZE, passwd, MESSAGE_BUF_SIZE - SALT_SIZE - 1);
   MD5(buffer, MESSAGE_BUF_SIZE, md);

   /* wipe password traces out */
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

static int unpack_KeyRing(struct KeyRing *kr, unsigned char *buf, int buf_size)
{
   int i, j;
   int n;
   int rem;
   unsigned char *clear_text;
   unsigned char *P;
   unsigned char *Pstr[3];
   unsigned char *safety[]={"","",""
   };

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
      /* memcpy(chunk, &P[i], 8); */
      /* des_decrypt3((DES_LONG *)chunk, s1, s2, s1); */
      /* memcpy(clear_text+i, chunk, 8); */
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
 * Start password change code
 */
/* Code for this is written, just need to add another jpilot API for
 * cancelling a sync if the passwords don't match.
 */
/*
 * End password change code
 */

static void
set_new_button_to(int new_state)
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
      //gtk_widget_hide(copy_record_button);
      gtk_widget_hide(delete_record_button);
      break;
    case NEW_FLAG:
      gtk_clist_set_selection_mode(GTK_CLIST(clist), GTK_SELECTION_SINGLE);
      clist_hack=TRUE;
      /* The line selected on the clist becomes unhighlighted, so we do this */
      gtk_clist_select_row(GTK_CLIST(clist), clist_row_selected, 0);
      gtk_widget_show(add_record_button);
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
    default:
      return;
   }
   switch (record_changed) {
    case MODIFY_FLAG:
      gtk_widget_hide(apply_record_button);
      gtk_widget_show(copy_record_button);
      gtk_widget_show(delete_record_button);
      break;
    case NEW_FLAG:
      gtk_widget_hide(add_record_button);
      gtk_widget_show(copy_record_button);
      gtk_widget_show(delete_record_button);
      break;
    case CLEAR_FLAG:
      gtk_widget_hide(new_record_button);
      //gtk_widget_hide(copy_record_button);
      gtk_widget_hide(delete_record_button);
      break;
   }
   record_changed=new_state;
}

static void
cb_record_changed(GtkWidget *widget,
		  gpointer   data)
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
}

static void connect_changed_signals(int con_or_dis)
{
   static int connected=0;

   /* CONNECT */
   if ((con_or_dis==CONNECT_SIGNALS) && (!connected)) {
      jp_logf(JP_LOG_DEBUG, "KeyRing: connect_changed_signals\n");
      connected=1;

#ifdef ENABLE_GTK2
      g_signal_connect(text_note_buffer, "changed",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);
#else
      gtk_signal_connect(GTK_OBJECT(text_note), "changed",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);
#endif
      gtk_signal_connect(GTK_OBJECT(entry_name), "changed",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_connect(GTK_OBJECT(entry_account), "changed",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_connect(GTK_OBJECT(entry_password), "changed",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);
   }
   
   /* DISCONNECT */
   if ((con_or_dis==DISCONNECT_SIGNALS) && (connected)) {
      jp_logf(JP_LOG_DEBUG, "KeyRing: disconnect_changed_signals\n");
      connected=0;

#ifdef ENABLE_GTK2
      g_signal_handlers_disconnect_by_func(text_note_buffer,
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
#else
      gtk_signal_disconnect_by_func(GTK_OBJECT(text_note),
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
#endif
      gtk_signal_disconnect_by_func(GTK_OBJECT(entry_name),
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_disconnect_by_func(GTK_OBJECT(entry_account),
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_disconnect_by_func(GTK_OBJECT(entry_password),
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
   }
}

static void free_mykeyring_list(struct MyKeyRing **PPkr)
{
   struct MyKeyRing *kr, *next_kr;

   jp_logf(JP_LOG_DEBUG, "KeyRing: free_mykeyring_list\n");
   for (kr = *PPkr; kr; kr=next_kr) {
      next_kr = kr->next;
      if (kr->kr.name) free(kr->kr.name);
      if (kr->kr.account) free(kr->kr.account);
      if (kr->kr.password) free(kr->kr.password);
      if (kr->kr.note) free(kr->kr.note);
      free(kr);
   }
   *PPkr=NULL;
}

/*
 * This is a mandatory plugin function.
 */
void plugin_version(int *major_version, int *minor_version)
{
   *major_version=0;
   *minor_version=99;
}

/*
 * This is a mandatory plugin function.
 */
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

/* This function gets called when the "delete" button is pressed */
static void cb_delete(GtkWidget *widget, gpointer data)
{
   struct MyKeyRing *mkr;
   int new_size;
   char buf[0xFFFF];
   buf_rec br;

   jp_logf(JP_LOG_DEBUG, "KeyRing: cb_delete\n");

   mkr = gtk_clist_get_row_data(GTK_CLIST(clist), clist_row_selected);
   if (!mkr) {
      return;
   }

   connect_changed_signals(DISCONNECT_SIGNALS);
   set_new_button_to(CLEAR_FLAG);

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

   jp_delete_record("Keys-Gtkr", &br, DELETE_FLAG);

   display_records();

   connect_changed_signals(CONNECT_SIGNALS);
}

/*
 * This is called when the "Clear" button is pressed.
 * It just clears out all the detail fields.
 */
static void clear_details()
{
   time_t ltime;
   struct tm *now;

   time(&ltime);
   now = localtime(&ltime);
   
   jp_logf(JP_LOG_DEBUG, "KeyRing: cb_clear\n");

   connect_changed_signals(DISCONNECT_SIGNALS);
   set_new_button_to(NEW_FLAG);

   gtk_entry_set_text(GTK_ENTRY(entry_name), "");
   gtk_entry_set_text(GTK_ENTRY(entry_account), "");
   gtk_entry_set_text(GTK_ENTRY(entry_password), "");
#ifdef ENABLE_GTK2
   gtk_text_buffer_set_text(GTK_TEXT_BUFFER(text_note_buffer), "", -1);
#else
   gtk_text_backward_delete(GTK_TEXT(text_note),
			    gtk_text_get_length(GTK_TEXT(text_note)));
#endif

   connect_changed_signals(CONNECT_SIGNALS);
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

   jp_logf(JP_LOG_DEBUG, "KeyRing: cb_add_new_record\n");

   flag=GPOINTER_TO_INT(data);

   if (flag==CLEAR_FLAG) {
      connect_changed_signals(DISCONNECT_SIGNALS);
      clear_details();
      set_new_button_to(NEW_FLAG);
      return;
   }
   if ((flag!=NEW_FLAG) && (flag!=MODIFY_FLAG) && (flag!=COPY_FLAG)) {
      return;
   }

   kr.name = (char *)gtk_entry_get_text(GTK_ENTRY(entry_name));
   kr.account = (char *)gtk_entry_get_text(GTK_ENTRY(entry_account));
   kr.password = (char *)gtk_entry_get_text(GTK_ENTRY(entry_password));
#ifdef ENABLE_GTK2
   gtk_text_buffer_get_bounds(GTK_TEXT_BUFFER(text_note_buffer),&start_iter,&end_iter);
   kr.note = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(text_note_buffer),&start_iter,&end_iter,TRUE);
#else
   kr.note = (char *)gtk_editable_get_chars(GTK_EDITABLE(text_note), 0, -1);
#endif

   kr.name = strdup(kr.name);
   jp_charset_j2p((unsigned char *)kr.name, strlen(kr.name)+1);

   kr.account = strdup(kr.account);
   jp_charset_j2p((unsigned char *)kr.account, strlen(kr.account)+1);
   
   kr.password = strdup(kr.password);
   jp_charset_j2p((unsigned char *)kr.password, strlen(kr.password)+1);
   
   jp_charset_j2p((unsigned char *)kr.note, strlen(kr.note)+1);

   pack_KeyRing(&kr, buf, 0xFFFF, &new_size);

   /* This is a new record from the PC, and not yet on the palm */
   br.rt = NEW_PC_REC;
   
   /* Any attributes go here.  Usually just the category */

   br.attrib = glob_category_number_from_menu_item[glob_detail_category];
   jp_logf(JP_LOG_DEBUG, "category is %d\n", br.attrib);
   br.buf = buf;
   br.size = new_size;
   br.unique_id = 0;

   connect_changed_signals(CONNECT_SIGNALS);
   set_new_button_to(CLEAR_FLAG);
   
   if (flag==MODIFY_FLAG) {
      mkr = gtk_clist_get_row_data(GTK_CLIST(clist), clist_row_selected);
      if (!mkr) {
	 return;
      }
      /* printf("mkr->rt=%d\n", mex->rt); */
      /* printf("mex->unique_id=%d\n", mex->unique_id); */
      if ((mkr->rt==PALM_REC) || (mkr->rt==REPLACEMENT_PALM_REC)) {
	 /* This code is to keep the unique ID intact */
	 br.unique_id = mkr->unique_id;
	 br.rt = REPLACEMENT_PALM_REC;
	 /* printf("setting br.rt\n"); */
      }
      cb_delete(NULL, GINT_TO_POINTER(MODIFY_FLAG));
   }

   /* mkr will no longer point to valid memory after this cb_delete */
   mkr=NULL;

   /* Write out the record.  It goes to the .pc3 file until it gets synced */
   jp_pc_write("Keys-Gtkr", &br);

   display_records();

   return;
}

/* First pass at password generating code */
static void cb_gen_password(GtkWidget *widget, gpointer data)
{
#define MIN_KR_PASS (20)	/* Minimum length */
#define MAX_KR_PASS (25)	/* Maximum length */
   GtkWidget *entry;
   int 	i,
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
static int display_record(struct MyKeyRing *mkr, int at_row)
{
   GdkColor color;
   GdkColormap *colormap;
   char temp[8];
   char *temp_str;

   /* jp_logf(JP_LOG_DEBUG, "KeyRing: display_record\n");*/

   switch (mkr->rt) {
    case NEW_PC_REC:
    case REPLACEMENT_PALM_REC:
      colormap = gtk_widget_get_colormap(GTK_WIDGET(clist));
      color.red=CLIST_NEW_RED;
      color.green=CLIST_NEW_GREEN;
      color.blue=CLIST_NEW_BLUE;
      gdk_color_alloc(colormap, &color);
      gtk_clist_set_background(GTK_CLIST(clist), at_row, &color);
      break;
    case DELETED_PALM_REC:
      colormap = gtk_widget_get_colormap(GTK_WIDGET(clist));
      color.red=CLIST_DEL_RED;
      color.green=CLIST_DEL_GREEN;
      color.blue=CLIST_DEL_BLUE;
      gdk_color_alloc(colormap, &color);
      gtk_clist_set_background(GTK_CLIST(clist), at_row, &color);
      break;
    case MODIFIED_PALM_REC:
      colormap = gtk_widget_get_colormap(GTK_WIDGET(clist));
      color.red=CLIST_MOD_RED;
      color.green=CLIST_MOD_GREEN;
      color.blue=CLIST_MOD_BLUE;
      gdk_color_alloc(colormap, &color);
      gtk_clist_set_background(GTK_CLIST(clist), at_row, &color);
      break;
    default:
      gtk_clist_set_background(GTK_CLIST(clist), at_row, NULL);
   }

   gtk_clist_set_row_data(GTK_CLIST(clist), at_row, mkr);

   if ( (!(mkr->kr.name)) || (mkr->kr.name[0]=='\0') ) {
      sprintf(temp, "#%03d", at_row);
      gtk_clist_set_text(GTK_CLIST(clist), at_row, 0, temp);
   } else {
	  int len;

	  temp_str = malloc((len = strlen(mkr->kr.name)*2+1));
	  multibyte_safe_strncpy(temp_str, mkr->kr.name, len);
	  jp_charset_p2j(temp_str, len);
      gtk_clist_set_text(GTK_CLIST(clist), at_row, 0, temp_str);
      free(temp_str);
   }

   if ( (!(mkr->kr.account)) || (mkr->kr.account[0]=='\0') ) {
      gtk_clist_set_text(GTK_CLIST(clist), at_row, 1, "");
   } else {
	  int len;

	  temp_str = malloc((len = strlen(mkr->kr.account)*2+1));
	  multibyte_safe_strncpy(temp_str, mkr->kr.account, len);
	  jp_charset_p2j(temp_str, len);
      gtk_clist_set_text(GTK_CLIST(clist), at_row, 1, temp_str);
      free(temp_str);
   }

   return EXIT_SUCCESS;
}

/*
 * This function lists the records in the clist on the left side of
 * the screen.
 */
static void display_records()
{
   int num, i;
   int row_count, entries_shown;
   struct MyKeyRing *mkr;
   GList *records;
   GList *temp_list;
   buf_rec *br;
   gchar *empty_line[] = { "", "" };
   
   records=NULL;
   
   jp_logf(JP_LOG_DEBUG, "KeyRing: display_records\n");

   row_count=GTK_CLIST(clist)->rows;

   /* Need to remove pointers to data we are about to delete */
   for (i=0; i<row_count; i++) {
      gtk_clist_set_row_data(GTK_CLIST(clist), i, NULL);
   }

   connect_changed_signals(DISCONNECT_SIGNALS);
   set_new_button_to(CLEAR_FLAG);
/*
   jp_charset_j2p(mkr->kr.note, strlen(mkr->kr.note)+1);
   jp_charset_j2p(mkr->kr.account, strlen(mkr->kr.account)+1);
   jp_charset_j2p(mkr->kr.password, strlen(mkr->kr.password)+1); 
   jp_charset_j2p(mkr->kr.note, strlen(mkr->kr.note)+1);
*/

   if (glob_keyring_list!=NULL) {
      free_mykeyring_list(&glob_keyring_list);
   }

   gtk_clist_freeze(GTK_CLIST(clist));

   /* This function takes care of reading the Database for us */
   num = jp_read_DB_files("Keys-Gtkr", &records);
   if (-1 == num)
     return;

   /* Go to first entry in the list */
   for (temp_list = records; temp_list; temp_list = temp_list->prev) {
      records = temp_list;
   }
   entries_shown = 0;

   for (i=0, temp_list = records; temp_list; temp_list = temp_list->next, i++) {
      if (temp_list->data) {
	 br=temp_list->data;
      } else {
	 continue;
      }
      if (!br->buf) {
	 continue;
      }

      /* Since deleted and modified records are also returned and we don't
       * want to see those we skip over them. */
      if ((br->rt == DELETED_PALM_REC) || (br->rt == MODIFIED_PALM_REC)) {
	 continue;
      }
      if (show_category < 16) {
	 if ( ((br->attrib & 0x0F) != 
	       glob_category_number_from_menu_item[show_category]) &&
	     show_category != CATEGORY_ALL) {
	    continue;
	 }
      }

      /* This record should be record 0 and is the hash-key record */
      if (br->attrib & dlpRecAttrSecret) {
	 continue;
      }

      mkr = malloc(sizeof(struct MyKeyRing));
      mkr->next=NULL;
      mkr->attrib = br->attrib;
      mkr->unique_id = br->unique_id;
      mkr->rt = br->rt;

      if (unpack_KeyRing(&(mkr->kr), br->buf, br->size)!=0) {
         entries_shown++;
	 if (entries_shown>row_count) {
	    gtk_clist_append(GTK_CLIST(clist), empty_line);
	 }
	 display_record(mkr, entries_shown-1);
      }
      
      if (glob_keyring_list==NULL) {
	 glob_keyring_list=mkr;
      } else {
	 glob_keyring_list->next=mkr;
      }
   }

   /* Delete any extra rows, the data is already freed by freeing the list */
   for (i=row_count-1; i>=entries_shown; i--) {
      gtk_clist_set_row_data(GTK_CLIST(clist), i, NULL);
      gtk_clist_remove(GTK_CLIST(clist), i);
   }

   /* Sort the clist */
   gtk_clist_sort(GTK_CLIST(clist));
   
   gtk_clist_thaw(GTK_CLIST(clist));

   if (entries_shown) {
      gtk_clist_select_row(GTK_CLIST(clist), clist_row_selected, 0);
      cb_clist_selection(clist, clist_row_selected, 0, (gpointer)455, NULL);
   }

   jp_free_DB_records(&records);

   connect_changed_signals(CONNECT_SIGNALS);

   jp_logf(JP_LOG_DEBUG, "KeyRing: leave display_records\n");
}

/*
 * This function just displays a record on the right hand side of the screen
 * (the details)
 */
static void cb_clist_selection(GtkWidget      *clist,
			       gint           row,
			       gint           column,
			       GdkEventButton *event,
			       gpointer       data)
{
   struct MyKeyRing *mkr;
   int i, item_num, category;
   int keep, b;
   char *temp_str;
   
   jp_logf(JP_LOG_DEBUG, "KeyRing: cb_clist_selection\n");

   if ((!event) && (clist_hack)) return;

   if (row<0) {
      return;
   }
   /* HACK, see clist hack explanation in memo_gui.c */
   if (clist_hack) {
      keep=record_changed;
      gtk_clist_select_row(GTK_CLIST(clist), clist_row_selected, column);
      b=dialog_save_changed_record(clist, record_changed);
      if (b==DIALOG_SAID_1) {
	 cb_add_new_record(NULL, GINT_TO_POINTER(record_changed));
      }
      set_new_button_to(CLEAR_FLAG);
      /* This doesn't cause an event to occur, it does highlight
       * the line, so we do the next call also */
      gtk_clist_select_row(GTK_CLIST(clist), row, column);
      cb_clist_selection(clist, row, column, GINT_TO_POINTER(1), NULL);
      return;
   }

   clist_row_selected = row;

   mkr = gtk_clist_get_row_data(GTK_CLIST(clist), row);
   if (mkr==NULL) {
      return;
   }

   /* Need to disconnect these signals first */
   connect_changed_signals(DISCONNECT_SIGNALS);
   set_new_button_to(NEW_FLAG);
   
   category = mkr->attrib & 0x0F;
   item_num=0;
   for (i=0; i<16; i++) {
      if (glob_category_number_from_menu_item[i]==category) {
	 item_num=i;
	 break;
      }
   }

   gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM
       (menu_item_category2[item_num]), TRUE);
   gtk_option_menu_set_history(GTK_OPTION_MENU(menu_category2), item_num);

   if (mkr->kr.name) {
	  int len;

	  temp_str = malloc((len = strlen(mkr->kr.name)*2+1));
	  multibyte_safe_strncpy(temp_str, mkr->kr.name, len);
      jp_charset_p2j((unsigned char *)temp_str, len);
      gtk_entry_set_text(GTK_ENTRY(entry_name), temp_str);
      free(temp_str);
   } else {
      gtk_entry_set_text(GTK_ENTRY(entry_name), "");
   }

   if (mkr->kr.account) {
	  int len;

	  temp_str = malloc((len = strlen(mkr->kr.account)*2+1));
	  multibyte_safe_strncpy(temp_str, mkr->kr.account, len);
      jp_charset_p2j((unsigned char *)temp_str, len);
      gtk_entry_set_text(GTK_ENTRY(entry_account), temp_str); 
      free(temp_str);
   } else {
      gtk_entry_set_text(GTK_ENTRY(entry_account), "");
   }

   if (mkr->kr.password) {
	  int len;

	  temp_str = malloc((len = strlen(mkr->kr.password)*2+1));
	  multibyte_safe_strncpy(temp_str, mkr->kr.password, len);
      jp_charset_p2j((unsigned char *)temp_str, len);
      gtk_entry_set_text(GTK_ENTRY(entry_password), temp_str); 
      free(temp_str);
   } else {
      gtk_entry_set_text(GTK_ENTRY(entry_password), "");
   }
   
#ifdef ENABLE_GTK2
   gtk_text_buffer_set_text(GTK_TEXT_BUFFER(text_note_buffer), "", -1);
#else
   gtk_text_set_point(GTK_TEXT(text_note), 0);
   gtk_text_forward_delete(GTK_TEXT(text_note),
			   gtk_text_get_length(GTK_TEXT(text_note)));
#endif
   if (mkr->kr.note) {
	  int len;

	  temp_str = malloc((len = strlen(mkr->kr.note)*2+1));
	  multibyte_safe_strncpy(temp_str, mkr->kr.note, len);
      jp_charset_p2j((unsigned char *)temp_str, len);
#ifdef ENABLE_GTK2
      gtk_text_buffer_set_text(GTK_TEXT_BUFFER(text_note_buffer), temp_str, -1);
#else
      gtk_text_insert(GTK_TEXT(text_note), NULL,NULL,NULL, temp_str, -1);
#endif
      free(temp_str);
   }
   set_new_button_to(CLEAR_FLAG);
   connect_changed_signals(CONNECT_SIGNALS);

   jp_logf(JP_LOG_DEBUG, "KeyRing: leaving cb_clist_selection\n");
}

/*
 * All menus use this same callback function.  I use the value parameter
 * to determine which menu was changed and which item was selected from it.
 */
static void cb_category(GtkWidget *item, unsigned int value)
{
   int menu, sel;
   int b;
   
   jp_logf(JP_LOG_DEBUG, "KeyRing: cb_category\n");
   if (!item) {
      return;
   }
   if (!(GTK_CHECK_MENU_ITEM(item))->active) {
      return;
   }
   
   menu=(value & 0xFF00) >> 8;
   sel=value & 0x00FF;

   switch (menu) {
    case KEYRING_CAT1:
      // remember the previously used category
      for (old_category=0; old_category<NUM_KEYRING_CAT_ITEMS; old_category++) {
#ifdef ENABLE_GTK2
	 if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menu_item_category1[old_category])))
	   break;
#else
	 if (GTK_CHECK_MENU_ITEM(menu_item_category1[old_category])->active)
	   break;
#endif
      }

      b=dialog_save_changed_record(clist, record_changed);
      if (b==DIALOG_SAID_1) {
	 cb_add_new_record(NULL, GINT_TO_POINTER(record_changed));
      }   
      show_category=sel;
      display_records();
      break;
    case KEYRING_CAT2:
      cb_record_changed(NULL, NULL);
      glob_detail_category=sel;
      break;
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
      gtk_signal_connect(GTK_OBJECT(menu_item), "activate",
			 GTK_SIGNAL_FUNC(cb_category), GINT_TO_POINTER(menu_index<<8 | item_num));
      group = gtk_radio_menu_item_group(GTK_RADIO_MENU_ITEM(menu_item));
      gtk_menu_append(GTK_MENU(menu), menu_item);
      gtk_widget_show(menu_item);
   }
   gtk_option_menu_set_menu(GTK_OPTION_MENU(option_menu), menu);
   /* Make this one show up by default */
   gtk_option_menu_set_history(GTK_OPTION_MENU(option_menu), 0);

   gtk_widget_show(option_menu);

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
   char all[]="All";
   char *categories[18];
     
   jp_logf(JP_LOG_DEBUG, "KeyRing: make_menus\n");

   /* This gets the application specific data out of the database for us.
    * We still need to write a function to unpack it from its blob form. */
   
   jp_get_app_info("Keys-Gtkr", &buf, &buf_size);

   /* This call should work, but the appinfo is too small, so we do it */
   /* Keyring is not using a legal category app info structure */
   /* unpack_CategoryAppInfo(&ai, buf, buf_size+4); */
   
   /* I'm going to be lazy and only get the names, since thats all I use */
   for (i=0; i<16; i++) {
      memcpy(&ai.name[i][0], buf+i*16+2, 16);
   }
   
   categories[0]=all;
   for (i=0, count=0; i<16; i++) {
      glob_category_number_from_menu_item[i]=0;
      if (ai.name[i][0]=='\0') {
	 continue;
      }
      jp_charset_p2j((unsigned char *)ai.name[i], 16);
      categories[count+1]=ai.name[i];
      glob_category_number_from_menu_item[count++]=i;
   }
   categories[count+1]=NULL;
   
   records=NULL;
   
   make_menu(categories, KEYRING_CAT1, &menu_category1, menu_item_category1);
   /* Skip the ALL for this menu */
   make_menu(&categories[1], KEYRING_CAT2, &menu_category2, menu_item_category2);   
}

/*
 * PASSWORD GUI
 */
/*
 * Start of Dialog window code
 */
struct dialog_data {
   GtkWidget *entry;
   int button_hit;
   char text[PASSWD_LEN+2];
};

#define DIALOG_SAID_1  454
#define DIALOG_SAID_2  455

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
      gtk_entry_set_text(GTK_ENTRY(Pdata->entry), "          ");
   }

   gtk_main_quit();

   return TRUE;
}

/*
 * returns 1 if OK was pressed, 2 if cancel was hit
 */
static int dialog_password(GtkWindow *main_window, char *ascii_password, int reason)
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
	 gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(main_window));
      }
   }

   vbox1 = gtk_vbox_new(FALSE, 2);

   gtk_container_set_border_width(GTK_CONTAINER(vbox1), 5);
   
   gtk_container_add(GTK_CONTAINER(dialog), vbox1);

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
		      GINT_TO_POINTER(DIALOG_SAID_1));
   gtk_box_pack_start(GTK_BOX(hbox1), entry, TRUE, TRUE, 1);


   /* Button Box */
   hbox1 = gtk_hbox_new(TRUE, 2);
   gtk_container_set_border_width(GTK_CONTAINER(hbox1), 5);
   gtk_box_pack_start(GTK_BOX(vbox1), hbox1, FALSE, FALSE, 2);

   /* Buttons */
   button = gtk_button_new_with_label(_("OK"));
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_dialog_button),
		      GINT_TO_POINTER(DIALOG_SAID_1));
   gtk_box_pack_start(GTK_BOX(hbox1), button, TRUE, TRUE, 1);

   button = gtk_button_new_with_label(_("Cancel"));
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_dialog_button),
		      GINT_TO_POINTER(DIALOG_SAID_2));
   gtk_box_pack_start(GTK_BOX(hbox1), button, TRUE, TRUE, 1);


   Pdata = malloc(sizeof(struct dialog_data));
   if (Pdata) {
      /* Set the default button pressed to CANCEL */
      Pdata->button_hit = DIALOG_SAID_2;
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
/*
 * End Password GUI
 */


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
      jp_logf(JP_LOG_WARN, _("Your HOME environment variable is too long for me\n"));
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
 * returns 0 on password correct, >1 not correct, <0 error
 */
int verify_pasword(char *ascii_password)
{
   int num;
   GList *records;
   GList *temp_list;
   buf_rec *br;
   int password_not_correct;

   jp_logf(JP_LOG_DEBUG, "KeyRing: verify_pasword\n");

   if (check_for_db()) {
      return EXIT_FAILURE;
   }

   password_not_correct = 1;
   /* TODO - Maybe keep records in memory for performance */
   /* This function takes care of reading the Database for us */
   records=NULL;
   num = jp_read_DB_files("Keys-Gtkr", &records);
   if (-1 == num)
     return 0;

   /* Go to first entry in the list */
   for (temp_list = records; temp_list; temp_list = temp_list->prev) {
      records = temp_list;
   }
   for (temp_list = records; temp_list; temp_list = temp_list->next) {
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
   if (password_not_correct) return 1;
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
   GtkWidget *temp_hbox;
   GtkWidget *button;
   GtkWidget *label;
#ifndef ENABLE_GTK2
   GtkWidget *vscrollbar;
#endif
   GtkWidget *table;
   GtkWindow *w;
   time_t ltime;
   struct tm *now;
   char ascii_password[PASSWD_LEN];
   int r;
   int password_not_correct;
   char *titles[2];
   int retry;
   int cycle_category = FALSE;

   titles[0] = _("Name"); titles[1] = _("Account");
   
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

   if (glob_keyring_list!=NULL) {
      free_mykeyring_list(&glob_keyring_list);
   }

   if (difftime(time(NULL), plugin_last_time) > PLUGIN_MAX_INACTIVE_TIME) {
      /* reset last time we entered */
      plugin_last_time = 0;

      password_not_correct=1;
      retry=PASSWD_ENTER;
      while (password_not_correct) {
         r = dialog_password(w, ascii_password, retry);
         retry=PASSWD_ENTER_RETRY;
         if (r!=1) {
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
      show_category = CATEGORY_ALL;
      cycle_category = FALSE;
   }

   /* plug entered with correct password */
   plugin_last_time = time(NULL);
   plugin_active = TRUE;

   record_changed=CLEAR_FLAG;
   clist_row_selected = 0;

   time(&ltime);
   now = localtime(&ltime);

   /* Make the menus */
   jp_logf(JP_LOG_DEBUG, "KeyRing: calling make_menus\n");
   make_menus();
   
   /* left and right main boxes */
   vbox1 = gtk_vbox_new(FALSE, 0);
   vbox2 = gtk_vbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(hbox), vbox1, TRUE, TRUE, 5);
   gtk_box_pack_start(GTK_BOX(hbox), vbox2, TRUE, TRUE, 5);

   gtk_widget_set_usize(GTK_WIDGET(vbox1), 0, 230);
   gtk_widget_set_usize(GTK_WIDGET(vbox2), 0, 230);

   /* Make a temporary hbox */
   temp_hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox1), temp_hbox, FALSE, FALSE, 0);
   
   label = gtk_label_new(_("Category: "));
   gtk_box_pack_start(GTK_BOX(temp_hbox), label, FALSE, FALSE, 0);
   gtk_box_pack_start(GTK_BOX(temp_hbox), menu_category1, TRUE, TRUE, 0);

   
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
   /* gtk_clist_set_shadow_type(GTK_CLIST(clist), SHADOW);*/
   gtk_clist_set_selection_mode(GTK_CLIST(clist), GTK_SELECTION_BROWSE);
   gtk_clist_set_column_width(GTK_CLIST(clist), 0, 150);
   gtk_clist_set_column_width(GTK_CLIST(clist), 1, 60);
   gtk_container_add(GTK_CONTAINER(scrolled_window), GTK_WIDGET(clist));
   
   gtk_clist_set_sort_column(GTK_CLIST(clist), 0);
   gtk_clist_set_sort_type(GTK_CLIST(clist), GTK_SORT_ASCENDING);
   
   /* -------------------- */
   /* Right half of screen */
   /* -------------------- */
   
   temp_hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox2), temp_hbox, FALSE, FALSE, 0);

   /* Add record button */
   delete_record_button = gtk_button_new_with_label(_("Delete"));
   gtk_signal_connect(GTK_OBJECT(delete_record_button), "clicked",
		      GTK_SIGNAL_FUNC(cb_delete),
		      GINT_TO_POINTER(DELETE_FLAG));
   gtk_box_pack_start(GTK_BOX(temp_hbox), delete_record_button, TRUE, TRUE, 0);

   copy_record_button = gtk_button_new_with_label(_("Copy"));
   gtk_box_pack_start(GTK_BOX(temp_hbox), copy_record_button, TRUE, TRUE, 0);
   gtk_signal_connect(GTK_OBJECT(copy_record_button), "clicked",
		      GTK_SIGNAL_FUNC(cb_add_new_record),
		      GINT_TO_POINTER(COPY_FLAG));

   new_record_button = gtk_button_new_with_label(_("New Record"));
   gtk_box_pack_start(GTK_BOX(temp_hbox), new_record_button, TRUE, TRUE, 0);
   gtk_signal_connect(GTK_OBJECT(new_record_button), "clicked",
		      GTK_SIGNAL_FUNC(cb_add_new_record),
		      GINT_TO_POINTER(CLEAR_FLAG));

   add_record_button = gtk_button_new_with_label(_("Add Record"));
   gtk_box_pack_start(GTK_BOX(temp_hbox), add_record_button, TRUE, TRUE, 0);
   gtk_signal_connect(GTK_OBJECT(add_record_button), "clicked",
		      GTK_SIGNAL_FUNC(cb_add_new_record),
		      GINT_TO_POINTER(NEW_FLAG));
   gtk_widget_set_name(GTK_WIDGET(GTK_LABEL(GTK_BIN(add_record_button)->child)),
		       "label_high");

   apply_record_button = gtk_button_new_with_label(_("Apply Changes"));
   gtk_box_pack_start(GTK_BOX(temp_hbox), apply_record_button, TRUE, TRUE, 0);
   gtk_signal_connect(GTK_OBJECT(apply_record_button), "clicked",
		      GTK_SIGNAL_FUNC(cb_add_new_record),
		      GINT_TO_POINTER(MODIFY_FLAG));
   gtk_widget_set_name(GTK_WIDGET(GTK_LABEL(GTK_BIN(apply_record_button)->child)),
		       "label_high");
   
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

   temp_hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox2), temp_hbox, TRUE, TRUE, 0);

#ifdef ENABLE_GTK2
   text_note = gtk_text_view_new();
   text_note_buffer = G_OBJECT(gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_note)));
   gtk_text_view_set_editable(GTK_TEXT_VIEW(text_note), TRUE);
   gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_note), GTK_WRAP_WORD);

   scrolled_window = gtk_scrolled_window_new(NULL, NULL);
   gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
				   GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
   gtk_container_set_border_width(GTK_CONTAINER(scrolled_window), 1);
   gtk_container_add(GTK_CONTAINER(scrolled_window), text_note);
   gtk_box_pack_start_defaults(GTK_BOX(temp_hbox), scrolled_window);
#else
   text_note = gtk_text_new(NULL, NULL);
   gtk_text_set_editable(GTK_TEXT(text_note), TRUE);
   gtk_text_set_word_wrap(GTK_TEXT(text_note), TRUE);
   vscrollbar = gtk_vscrollbar_new(GTK_TEXT(text_note)->vadj);
   gtk_box_pack_start(GTK_BOX(temp_hbox), text_note, TRUE, TRUE, 0);
   gtk_box_pack_start(GTK_BOX(temp_hbox), vscrollbar, FALSE, FALSE, 0);
#endif

   gtk_widget_show_all(hbox);
   gtk_widget_show_all(vbox);
   
   gtk_widget_hide(add_record_button);
   gtk_widget_hide(apply_record_button);

   if (cycle_category) {
      old_category++;

      if (NULL == menu_item_category1[old_category]) {
	 old_category = 0;
	 show_category = CATEGORY_ALL;
      }

      gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_item_category1[old_category]), TRUE);
      gtk_option_menu_set_history(GTK_OPTION_MENU(menu_category1), old_category);
   }

   jp_logf(JP_LOG_DEBUG, "KeyRing: calling display_records\n");

   display_records();

   jp_logf(JP_LOG_DEBUG, "KeyRing: after display_records\n");

   if (unique_id) {
      keyring_find(unique_id);
   }

   return EXIT_SUCCESS;
}

/*
 * This function is called by J-Pilot when the user selects this plugin
 * from the plugin menu, or from the search window when a search result
 * record is chosen.  In the latter case, unique ID will be set.  This
 * application should go directly to that record in the case.
 */
int plugin_gui_cleanup() {
   int b;
   
   b=dialog_save_changed_record(clist, record_changed);
   if (b==DIALOG_SAID_1) {
      cb_add_new_record(NULL, GINT_TO_POINTER(record_changed));
   }

   connect_changed_signals(DISCONNECT_SIGNALS);

   jp_logf(JP_LOG_DEBUG, "KeyRing: plugin_gui_cleanup\n");
   if (glob_keyring_list!=NULL) {
      free_mykeyring_list(&glob_keyring_list);
   }

   /* if the password was correct */
   if (plugin_last_time) {
      plugin_last_time = time(NULL);
   }
   plugin_active = FALSE;

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
   *width = 0;
   
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

/*
 * This is a plugin callback function called during program exit.
 */
int plugin_exit_cleanup(void)
{
   jp_logf(JP_LOG_DEBUG, "KeyRing: plugin_exit_cleanup\n");
   return EXIT_SUCCESS;
}

static int add_search_result(const char *line, int unique_id, struct search_result **sr)
{
   struct search_result *temp_sr;

   jp_logf(JP_LOG_DEBUG, "KeyRing: add_search_result for [%s]\n", line);
   temp_sr=malloc(sizeof(struct search_result));
   if (!temp_sr) {
      return EXIT_FAILURE;
   }
   temp_sr->next=NULL;
   temp_sr->unique_id=unique_id;
   temp_sr->line=strdup(line);
   if (!(*sr)) {
      (*sr)=temp_sr;
   } else {
      (*sr)->next=temp_sr;
   }

   return EXIT_SUCCESS;
}

/*
 * This function is called when the user does a search.  It should return
 * records which match the search string.
 */
int plugin_search(const char *search_string, int case_sense,
		  struct search_result **sr)
{
   GList *records;
   GList *temp_list;
   buf_rec *br;
   struct MyKeyRing mkr;
   int num, count;
   char *line;

   records=NULL;

   *sr=NULL;

   jp_logf(JP_LOG_DEBUG, "KeyRing: plugin_search\n");

   if (! plugin_active) {
      return 0;
   }

   /* This function takes care of reading the Database for us */
   num = jp_read_DB_files("Keys-Gtkr", &records);
   if (-1 == num)
     return 0;

   /* Go to first entry in the list */
   for (temp_list = records; temp_list; temp_list = temp_list->prev) {
      records = temp_list;
   }

   count = 0;

   for (temp_list = records; temp_list; temp_list = temp_list->next) {
      if (temp_list->data) {
	 br=temp_list->data;
      } else {
	 continue;
      }

      if (!br->buf) {
	 continue;
      }

      /* Since deleted and modified records are also returned and we don't
       * want to see those we skip over them. */
      if ((br->rt == DELETED_PALM_REC) || (br->rt == MODIFIED_PALM_REC)) {
	 continue;
      }

      memset(&mkr, 0, sizeof(mkr));
      mkr.attrib = br->attrib;
      mkr.unique_id = br->unique_id;
      mkr.rt = br->rt;

      /* We need to unpack the record blobs from the database.
       * unpack_Expense is already written in pilot-link, but normally
       * an unpack must be written for each type of application */
      if (unpack_KeyRing(&mkr.kr, br->buf, br->size)!=0) {
	 line = NULL;

	 /* find in record name */
	 if (jp_strstr(mkr.kr.name, search_string, case_sense))
	    /* Add it to our result list */
	    line = strdup(mkr.kr.name);

	 /* find in record account */
	 if (jp_strstr(mkr.kr.account, search_string, case_sense))
	    /* Add it to our result list */
	    line = strdup(mkr.kr.account);

	 /* find in record password */
	 if (jp_strstr(mkr.kr.password, search_string, case_sense))
	    /* Add it to our result list */
	    line = strdup(mkr.kr.password);

	 /* find in record note */
	 if (jp_strstr(mkr.kr.note, search_string, case_sense))
	    line = strdup(mkr.kr.note);

	 if (line) {
	    /* Add it to our result list */
	    jp_logf(JP_LOG_DEBUG, "KeyRing: calling add_search_result\n");
	    add_search_result(line, br->unique_id, sr);
	    jp_logf(JP_LOG_DEBUG, "KeyRing: back from add_search_result\n");
	    count++;
	 }
      }
   }

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
      cb_clist_selection(clist, found_at, 0, (gpointer)455, NULL);
      gtk_clist_select_row(GTK_CLIST(clist), found_at, 0);
      gtk_clist_moveto(GTK_CLIST(clist), found_at, 0, 0.5, 0.0);
   }

   return EXIT_SUCCESS;
}

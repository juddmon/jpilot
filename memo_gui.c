/* memo_gui.c
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
#include <sys/stat.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <pi-dlp.h>
#include "utils.h"
#include "log.h"
#include "prefs.h"
#include "password.h"
#include "print.h"
#include "memo.h"
#include "export.h"


#define NUM_MEMO_CAT_ITEMS 16

#define CONNECT_SIGNALS 400
#define DISCONNECT_SIGNALS 401

extern GtkTooltips *glob_tooltips;

struct MemoAppInfo memo_app_info;
static int memo_category = CATEGORY_ALL;
static int clist_row_selected;
static GtkWidget *clist;
static GtkWidget *memo_text;
static GtkWidget *private_checkbox;
static GtkWidget *memo32_checkbox;
/*Need one extra for the ALL category */
static GtkWidget *memo_cat_menu_item1[NUM_MEMO_CAT_ITEMS+1];
static GtkWidget *memo_cat_menu_item2[NUM_MEMO_CAT_ITEMS];
static GtkWidget *category_menu1;
static GtkWidget *category_menu2;
static GtkWidget *scrolled_window;
static GtkWidget *pane;
static struct sorted_cats sort_l[NUM_MEMO_CAT_ITEMS];
static GtkWidget *new_record_button;
static GtkWidget *apply_record_button;
static GtkWidget *add_record_button;
static int record_changed;
static int clist_hack;

static MemoList *glob_memo_list=NULL;
static MemoList *export_memo_list=NULL;

static int memo_clear_details();
int memo_clist_redraw();
static void connect_changed_signals(int con_or_dis);
static int memo_find();
int memo_get_details(struct Memo *new_memo, unsigned char *attrib);
static void memo_update_clist(GtkWidget *clist, GtkWidget *tooltip_widget,
			      MemoList *memo_list, int category, int main);
static void cb_add_new_record(GtkWidget *widget, gpointer data);

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
      break;
    case NEW_FLAG:
      gtk_clist_set_selection_mode(GTK_CLIST(clist), GTK_SELECTION_SINGLE);
      clist_hack=TRUE;
      /* The line selected on the clist becomes unhighlighted, so we do this */
      gtk_clist_select_row(GTK_CLIST(clist), clist_row_selected, 0);
      gtk_widget_show(add_record_button);
      break;
    case CLEAR_FLAG:
      gtk_clist_set_selection_mode(GTK_CLIST(clist), GTK_SELECTION_BROWSE);
      clist_hack=FALSE;
      gtk_widget_show(new_record_button);
      break;
    default:
      return;
   }
   switch (record_changed) {
    case MODIFY_FLAG:
      gtk_widget_hide(apply_record_button);
      break;
    case NEW_FLAG:
      gtk_widget_hide(add_record_button);
      break;
    case CLEAR_FLAG:
      gtk_widget_hide(new_record_button);
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
      if (((GtkCList *)clist)->rows > 0) {
	 set_new_button_to(MODIFY_FLAG);
      } else {
	 set_new_button_to(NEW_FLAG);
      }
   }
}

static void connect_changed_signals(int con_or_dis)
{
   int i;
   static int connected=0;
   /* CONNECT */
   if ((con_or_dis==CONNECT_SIGNALS) && (!connected)) {
      connected=1;

      for (i=0; i<NUM_MEMO_CAT_ITEMS; i++) {
	 if (memo_cat_menu_item2[i]) {
	    gtk_signal_connect(GTK_OBJECT(memo_cat_menu_item2[i]), "toggled",
			       GTK_SIGNAL_FUNC(cb_record_changed), NULL);
	 }
      }
      gtk_signal_connect(GTK_OBJECT(memo_text), "changed",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_connect(GTK_OBJECT(private_checkbox), "toggled",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);
   }

   /* DISCONNECT */
   if ((con_or_dis==DISCONNECT_SIGNALS) && (connected)) {
      connected=0;

      for (i=0; i<NUM_MEMO_CAT_ITEMS; i++) {
	 if (memo_cat_menu_item2[i]) {
	    gtk_signal_disconnect_by_func(GTK_OBJECT(memo_cat_menu_item2[i]),
					  GTK_SIGNAL_FUNC(cb_record_changed), NULL);     
	 }
      }
      gtk_signal_disconnect_by_func(GTK_OBJECT(memo_text),
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_disconnect_by_func(GTK_OBJECT(private_checkbox),
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
   }
}

int memo_print()
{
   long this_many;
   MyMemo *mmemo;
   MemoList *memo_list;
   MemoList memo_list1;

   get_pref(PREF_PRINT_THIS_MANY, &this_many, NULL);

   memo_list=NULL;
   if (this_many==1) {
      mmemo = gtk_clist_get_row_data(GTK_CLIST(clist), clist_row_selected);
      if (mmemo < (MyMemo *)CLIST_MIN_DATA) {
	 return -1;
      }
      memcpy(&(memo_list1.mmemo), mmemo, sizeof(MyMemo));
      memo_list1.next=NULL;
      memo_list = &memo_list1;
   }
   if (this_many==2) {
      get_memos2(&memo_list, SORT_ASCENDING, 2, 2, 2, memo_category);
   }
   if (this_many==3) {
      get_memos2(&memo_list, SORT_ASCENDING, 2, 2, 2, CATEGORY_ALL);
   }

   print_memos(memo_list);

   if ((this_many==2) || (this_many==3)) {
      free_MemoList(&memo_list);
   }

   return 0;
}

/*
 * Start Import Code
 */
int memo_import_callback(GtkWidget *parent_window, char *file_path, int type)
{
   FILE *in;
   char line[256];
   char text[65536];
   struct Memo new_memo;
   unsigned char attrib;
   unsigned int len;
   unsigned int text_len;
   int i, ret, index;
   int import_all;
   MemoList *memolist;
   MemoList *temp_memolist;
   struct CategoryAppInfo cai;
   char old_cat_name[32];
   int suggested_cat_num;
   int new_cat_num;
   int priv;

   memo_get_details(&new_memo, &attrib);
   if (new_memo.text) {
      free_Memo(&new_memo);
   }

   in=fopen(file_path, "r");
   if (!in) {
      jp_logf(JP_LOG_WARN, _("Could not open file %s\n"), file_path);
      return -1;
   }

   /* TEXT */
   if (type==IMPORT_TYPE_TEXT) {
      jp_logf(JP_LOG_DEBUG, "Memo import text [%s]\n", file_path);
      text_len=0;
      text[0]='\0';
      while (!feof(in)) {
	 line[0]='\0';
	 fgets(line, 255, in);
	 line[255] = '\0';
	 len=strlen(line);
	 if (text_len+len>65534) {
	    len=65534-text_len;
	    line[len]='\0';
	    jp_logf(JP_LOG_WARN, "Memo text > 65535, truncating\n");
	    strcat(text, line);
	    break;
	 }
	 text_len+=len;
	 strcat(text, line);
      }
#ifdef JPILOT_DEBUG
      printf("text=[%s]\n", text);
      printf("text_len=%d\n", text_len);
      printf("strlen(text)=%d\n", strlen(text));
#endif

      new_memo.text=text;
      ret=import_record_ask(parent_window, pane,
			    new_memo.text,
			    &(memo_app_info.category),
			    "Unfiled",
			    0,
			    attrib & 0x0F,
			    &new_cat_num);
      if ((ret==DIALOG_SAID_IMPORT_ALL) || (ret==DIALOG_SAID_IMPORT_YES)) {
	 pc_memo_write(&new_memo, NEW_PC_REC, attrib, NULL);
	 jp_logf(JP_LOG_WARN, "Imported Memo %s\n", file_path);
      }
   }

   /* CSV */
   if (type==IMPORT_TYPE_CSV) {
      jp_logf(JP_LOG_DEBUG, "Memo import CSV [%s]\n", file_path);
      /* The first line is format, so we don't need it */
      fgets(text, 1000, in);
      import_all=FALSE;
      while (1) {
	 /* Read the category field */
	 ret = read_csv_field(in, text, 65535);
	 if (feof(in)) break;
#ifdef JPILOT_DEBUG
	 printf("category is [%s]\n", text);
#endif
	 strncpy(old_cat_name, text, 16);
	 old_cat_name[16]='\0';
	 attrib=0;
	 /* Figure out what the best category number is */
	 index=temp_memolist->mmemo.unique_id-1;
	 suggested_cat_num=0;
	 for (i=0; i<NUM_MEMO_CAT_ITEMS; i++) {
	    if (memo_app_info.category.name[i][0]=='\0') continue;
	    if (!strcmp(memo_app_info.category.name[i], old_cat_name)) {
	       suggested_cat_num=i;
	       i=1000;
	       break;
	    }
	 }

	 /* Read the private field */
	 ret = read_csv_field(in, text, 65535);
#ifdef JPILOT_DEBUG
	 printf("private is [%s]\n", text);
#endif
	 sscanf(text, "%d", &priv);

	 ret = read_csv_field(in, text, 65535);
#ifdef JPILOT_DEBUG
	 printf("memo text is [%s]\n", text);
#endif
	 new_memo.text=text;
	 if (!import_all) {
	    ret=import_record_ask(parent_window, pane,
				  new_memo.text,
				  &(memo_app_info.category),
				  old_cat_name,
				  priv,
				  suggested_cat_num,
				  &new_cat_num);
	 } else {
	    new_cat_num=suggested_cat_num;
	 }
	 if (ret==DIALOG_SAID_IMPORT_QUIT) break;
	 if (ret==DIALOG_SAID_IMPORT_SKIP) continue;
	 if (ret==DIALOG_SAID_IMPORT_ALL) {
	    import_all=TRUE;
	 }
	 attrib = (new_cat_num & 0x0F) |
	   (priv ? dlpRecAttrSecret : 0);
	 if ((ret==DIALOG_SAID_IMPORT_YES) || (import_all)) {
	    pc_memo_write(&new_memo, NEW_PC_REC, attrib, NULL);
	 }
      }
   }

   /* Palm Desktop DAT format */
   if (type==IMPORT_TYPE_DAT) {
      jp_logf(JP_LOG_DEBUG, "Memo import DAT [%s]\n", file_path);
      if (dat_check_if_dat_file(in)!=DAT_MEMO_FILE) {
	 jp_logf(JP_LOG_WARN, _("File doesn't appear to be memopad.dat format\n"));
	 fclose(in);
	 return 1;
      }
      memolist=NULL;
      dat_get_memos(in, &memolist, &cai);
      import_all=FALSE;
      for (temp_memolist=memolist; temp_memolist; temp_memolist=temp_memolist->next) {
#ifdef JPILOT_DEBUG
	 printf("category=%d\n", temp_memolist->mmemo.unique_id);
	 printf("attrib=%d\n", temp_memolist->mmemo.attrib);
	 printf("private=%d\n", temp_memolist->mmemo.attrib & 0x10);
	 printf("memo=[%s]\n", temp_memolist->mmemo.memo.text);
#endif
	 new_memo.text=temp_memolist->mmemo.memo.text;
	 index=temp_memolist->mmemo.unique_id-1;
	 if (index<0) {
	    strncpy(old_cat_name, _("Unfiled"), 16);
	    old_cat_name[16]='\0';
	    index=0;
	 } else {
	    strncpy(old_cat_name, cai.name[index], 16);
	    old_cat_name[16]='\0';
	 }
	 attrib=0;
	 /* Figure out what category it was in the dat file */
	 index=temp_memolist->mmemo.unique_id-1;
	 suggested_cat_num=0;
	 if (index>-1) {
	    for (i=0; i<NUM_MEMO_CAT_ITEMS; i++) {
	       if (memo_app_info.category.name[i][0]=='\0') continue;
	       if (!strcmp(memo_app_info.category.name[i], old_cat_name)) {
		  suggested_cat_num=i;
		  i=1000;
		  break;
	       }
	    }
	 }

	 ret=0;
	 if (!import_all) {
	    ret=import_record_ask(parent_window, pane,
				  new_memo.text,
				  &(memo_app_info.category),
				  old_cat_name,
				  (temp_memolist->mmemo.attrib & 0x10),
				  suggested_cat_num,
				  &new_cat_num);
	 } else {
	    new_cat_num=suggested_cat_num;
	 }
	 if (ret==DIALOG_SAID_IMPORT_QUIT) break;
	 if (ret==DIALOG_SAID_IMPORT_SKIP) continue;
	 if (ret==DIALOG_SAID_IMPORT_ALL) {
	    import_all=TRUE;
	 }
	 attrib = (new_cat_num & 0x0F) |
	   ((temp_memolist->mmemo.attrib & 0x10) ? dlpRecAttrSecret : 0);
	 if ((ret==DIALOG_SAID_IMPORT_YES) || (import_all)) {
	    pc_memo_write(&new_memo, NEW_PC_REC, attrib, NULL);
	 }
      }
      free_MemoList(&memolist);
   }

   memo_refresh();
   fclose(in);
   return 0;
}

int memo_import(GtkWidget *window)
{
   char *type_desc[] = {
      "Text",
      "CSV (Comma Separated Values)",
      "DAT/MPA (Palm Archive Formats)",
      NULL
   };
   int type_int[] = {
      IMPORT_TYPE_TEXT,
      IMPORT_TYPE_CSV,
      IMPORT_TYPE_DAT,
      0
   };

   import_gui(window, pane, type_desc, type_int, memo_import_callback);
   return 0;
}
/*
 * End Import Code
 */

/*
 * Start Export code
 */

void cb_memo_export_ok(GtkWidget *export_window, GtkWidget *clist,
		       int type, const char *filename)
{
   MyMemo *mmemo;
   GList *list, *temp_list;
   FILE *out;
   struct stat statb;
   int i, r, num, len;
   const char *short_date;
   time_t ltime;
   struct tm *now;
   char *button_text[]={gettext_noop("OK")};
   char *button_overwrite_text[]={gettext_noop("Yes"), gettext_noop("No")};
   char text[1024];
   char str1[256], str2[256];
   char pref_time[40];
   char *csv_text;

   list=GTK_CLIST(clist)->selection;

   if (!stat(filename, &statb)) {
      if (S_ISDIR(statb.st_mode)) {
	 g_snprintf(text, 1024, _("%s is a directory"), filename);
	 dialog_generic(GTK_WINDOW(export_window),
			0, 0, _("Error Opening File"),
			"Directory", text, 1, button_text);
	 return;
      }
      g_snprintf(text, 1024, _("Do you want to overwrite file %s?"), filename);
      r = dialog_generic(GTK_WINDOW(export_window),
			 0, 0, _("Overwrite File?"),
			 _("Overwrite File"), text, 2, button_overwrite_text);
      if (r!=DIALOG_SAID_1) {
	 return;
      }
   }

   out = fopen(filename, "w");
   if (!out) {
      g_snprintf(text, 1024, "Error Opening File: %s", filename);
      dialog_generic(GTK_WINDOW(export_window),
		     0, 0, _("Error Opening File"),
		     "Filename", text, 1, button_text);
      return;
   }

   /* Count how many are to be exported */
   for (num=0, temp_list=list; temp_list; temp_list = temp_list->next, num++);

   for (i=0, temp_list=list; temp_list; temp_list = temp_list->next, i++) {
      mmemo = gtk_clist_get_row_data(GTK_CLIST(clist), (int) temp_list->data);
      if (!mmemo) {
	 continue;
	 jp_logf(JP_LOG_WARN, "Can't export memo %d\n", (long) temp_list->data + 1);
      }
      switch (type) {
       case EXPORT_TYPE_CSV:
	 if (i==0) {
	    fprintf(out, "CSV memo: Category, Private, Memo Text\n");
	 }
	 len=0;
	 if (mmemo->memo.text) {
	    len=strlen(mmemo->memo.text) * 2 + 4;
	 }
	 if (len<256) len=256;
	 csv_text=malloc(len);
	 if (!csv_text) {
	    continue;
	    jp_logf(JP_LOG_WARN, "Can't export memo %d\n", (long) temp_list->data + 1);
	 }
	 str_to_csv_str(csv_text, memo_app_info.category.name[mmemo->attrib & 0x0F]);
	 fprintf(out, "\"%s\",", csv_text);
	 fprintf(out, "\"%s\",", (mmemo->attrib & dlpRecAttrSecret) ? "1":"0");
	 str_to_csv_str(csv_text, mmemo->memo.text);
	 fprintf(out, "\"%s\"\n", csv_text);
	 free(csv_text);
	 break;
       case EXPORT_TYPE_TEXT:
	 get_pref(PREF_SHORTDATE, NULL, &short_date);
	 get_pref_time_no_secs(pref_time);
	 time(&ltime);
	 now = localtime(&ltime);
	 strftime(str1, 50, short_date, now);
	 strftime(str2, 50, pref_time, now);
	 g_snprintf(text, 100, "%s %s", str1, str2);
	 text[100]='\0';

	 /* Todo Should I translate these? */
	 fprintf(out, "Memo: %ld, exported from %s on %s\n",
		 (long) temp_list->data + 1, PN, text);
	 fprintf(out, "Category: %s\n", memo_app_info.category.name[mmemo->attrib & 0x0F]);
	 fprintf(out, "This memo was marked as: %s\n",
		 (mmemo->attrib & dlpRecAttrSecret) ? "Private":"Not Private");
	 fprintf(out, "----- Start of Memo -----\n");
	 fprintf(out, "%s", mmemo->memo.text);
	 fprintf(out, "\n----- End of Memo -----\n\n");
	 break;
       default:
	 jp_logf(JP_LOG_WARN, "Unknown export type\n");
      }
   }

   if (out) {
      fclose(out);
   }
}


static void cb_memo_update_clist(GtkWidget *clist, int category)
{
   memo_update_clist(clist, NULL, export_memo_list, category, FALSE);
}


static void cb_memo_export_done(GtkWidget *widget, const char *filename)
{
   free_MemoList(&export_memo_list);

   set_pref(PREF_MEMO_EXPORT_FILENAME, 0, filename, TRUE);
}

int memo_export(GtkWidget *window)
{
   int w, h, x, y;
   char *type_text[]={"Text", "CSV", NULL};
   int type_int[]={EXPORT_TYPE_TEXT, EXPORT_TYPE_CSV};

   gdk_window_get_size(window->window, &w, &h);
   gdk_window_get_root_origin(window->window, &x, &y);

   w = GTK_PANED(pane)->handle_xpos;
   x+=40;

   export_gui(w, h, x, y, 1, sort_l,
	      PREF_MEMO_EXPORT_FILENAME,
	      type_text,
	      type_int,
	      cb_memo_update_clist,
	      cb_memo_export_done,
	      cb_memo_export_ok
	      );

   return 0;
}

/*
 * End Export Code
 */


static int find_sorted_cat(int cat)
{
   int i;
   for (i=0; i< NUM_MEMO_CAT_ITEMS; i++) {
      if (sort_l[i].cat_num==cat) {
 	 return i;
      }
   }
   return -1;
}

void cb_delete_memo(GtkWidget *widget,
		    gpointer   data)
{
   MyMemo *mmemo;
   int flag;
   int show_priv;

   mmemo = gtk_clist_get_row_data(GTK_CLIST(clist), clist_row_selected);
   if (mmemo < (MyMemo *)CLIST_MIN_DATA) {
      return;
   }
   /* Do masking like Palm OS 3.5 */
   show_priv = show_privates(GET_PRIVATES);
   if ((show_priv != SHOW_PRIVATES) &&
       (mmemo->attrib & dlpRecAttrSecret)) {
      return;
   }
   /* End Masking */
   jp_logf(JP_LOG_DEBUG, "mmemo->unique_id = %d\n",mmemo->unique_id);
   jp_logf(JP_LOG_DEBUG, "mmemo->rt = %d\n",mmemo->rt);
   flag = GPOINTER_TO_INT(data);
   if ((flag==MODIFY_FLAG) || (flag==DELETE_FLAG)) {
      delete_pc_record(MEMO, mmemo, flag);
      if (flag==DELETE_FLAG) {
	 /* when we redraw we want to go to the line above the deleted one */
	 if (clist_row_selected>0) {
	    clist_row_selected--;
	 }
      }
   }

   memo_clist_redraw();
}


static void cb_category(GtkWidget *item, int selection)
{
   int b;

   if ((GTK_CHECK_MENU_ITEM(item))->active) {
      b=dialog_save_changed_record(pane, record_changed);
      if (b==DIALOG_SAID_1) {
	 cb_add_new_record(NULL, GINT_TO_POINTER(record_changed));
      }
      memo_category = selection;
      jp_logf(JP_LOG_DEBUG, "cb_category() cat=%d\n", memo_category);
      memo_clear_details();
      memo_update_clist(clist, category_menu1, glob_memo_list, memo_category, TRUE);
      jp_logf(JP_LOG_DEBUG, "Leaving cb_category()\n");
   }
}

static int memo_clear_details()
{
   int new_cat;
   int sorted_position;

   jp_logf(JP_LOG_DEBUG, "memo_clear_details()\n");
   /* Need to disconnect these signals first */
   set_new_button_to(NEW_FLAG);
   connect_changed_signals(DISCONNECT_SIGNALS);

   gtk_text_freeze(GTK_TEXT(memo_text));

   gtk_text_set_point(GTK_TEXT(memo_text), 0);
   gtk_text_forward_delete(GTK_TEXT(memo_text),
			   gtk_text_get_length(GTK_TEXT(memo_text)));

   gtk_text_thaw(GTK_TEXT(memo_text));

   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(private_checkbox), FALSE);

   if (memo_category==CATEGORY_ALL) {
      new_cat = 0;
   } else {
      new_cat = memo_category;
   }
   sorted_position = find_sorted_cat(new_cat);
   if (sorted_position<0) {
      jp_logf(JP_LOG_WARN, "Category is not legal\n");
   } else {
      gtk_check_menu_item_set_active
	(GTK_CHECK_MENU_ITEM(memo_cat_menu_item2[sorted_position]), TRUE);
      gtk_option_menu_set_history(GTK_OPTION_MENU(category_menu2), sorted_position);
   }

   set_new_button_to(CLEAR_FLAG);
   jp_logf(JP_LOG_DEBUG, "Leaving memo_clear_details()\n");
   return 0;
}

int memo_get_details(struct Memo *new_memo, unsigned char *attrib)
{
   int i;

   new_memo->text = gtk_editable_get_chars
     (GTK_EDITABLE(memo_text), 0, -1);
   if (new_memo->text[0]=='\0') {
      free(new_memo->text);
      new_memo->text=NULL;
   }

   /*Get the category that is set from the menu */
   for (i=0; i<NUM_MEMO_CAT_ITEMS; i++) {
      if (GTK_IS_WIDGET(memo_cat_menu_item2[i])) {
	 if (GTK_CHECK_MENU_ITEM(memo_cat_menu_item2[i])->active) {
	    *attrib = sort_l[i].cat_num;
	    break;
	 }
      }
   }
   if (GTK_TOGGLE_BUTTON(private_checkbox)->active) {
      *attrib |= dlpRecAttrSecret;
   }
   return 0;
}

static void cb_use_memo32(GtkWidget *widget,
			  gpointer   data)
{
   set_pref(PREF_MEMO32_MODE, GTK_TOGGLE_BUTTON(memo32_checkbox)->active, NULL, TRUE);
   cb_app_button(NULL, GINT_TO_POINTER(REDRAW));
}

static void cb_add_new_record(GtkWidget *widget, gpointer data)
{
   MyMemo *mmemo;
   struct Memo new_memo;
   unsigned char attrib;
   int flag;
   unsigned int unique_id;
   int show_priv;

   flag=GPOINTER_TO_INT(data);

   mmemo=NULL;
   unique_id=0;

   /* Do masking like Palm OS 3.5 */
   if ((GPOINTER_TO_INT(data)==COPY_FLAG) || 
       (GPOINTER_TO_INT(data)==MODIFY_FLAG)) {
      show_priv = show_privates(GET_PRIVATES);
      mmemo = gtk_clist_get_row_data(GTK_CLIST(clist), clist_row_selected);
      if (mmemo < (MyMemo *)CLIST_MIN_DATA) {
	 return;
      }
      if ((show_priv != SHOW_PRIVATES) &&
	  (mmemo->attrib & dlpRecAttrSecret)) {
	 return;
      }
   }
   /* End Masking */
   if (flag==CLEAR_FLAG) {
      /*Clear button was hit */
      memo_clear_details();
      connect_changed_signals(DISCONNECT_SIGNALS);
      set_new_button_to(NEW_FLAG);
      gtk_widget_grab_focus(GTK_WIDGET(memo_text));
      return;
   }
   if ((flag!=NEW_FLAG) && (flag!=MODIFY_FLAG) && (flag!=COPY_FLAG)) {
      return;
   }
   if (flag==MODIFY_FLAG) {
      mmemo = gtk_clist_get_row_data(GTK_CLIST(clist), clist_row_selected);
      unique_id = mmemo->unique_id;
      if (mmemo < (MyMemo *)CLIST_MIN_DATA) {
	 return;
      }
      if ((mmemo->rt==DELETED_PALM_REC) || (mmemo->rt==MODIFIED_PALM_REC)) {
	 jp_logf(JP_LOG_INFO, "You can't modify a record that is deleted\n");
	 return;
      }
   }
   memo_get_details(&new_memo, &attrib);

   set_new_button_to(CLEAR_FLAG);

   /* Keep unique ID intact */
   if (flag==MODIFY_FLAG) {
      cb_delete_memo(NULL, data);
      if ((mmemo->rt==PALM_REC) || (mmemo->rt==REPLACEMENT_PALM_REC)) {
	 pc_memo_write(&new_memo, REPLACEMENT_PALM_REC, attrib, &unique_id);
      } else {
	 unique_id=0;
	 pc_memo_write(&new_memo, NEW_PC_REC, attrib, &unique_id);
      }
   } else {
      unique_id=0;
      pc_memo_write(&new_memo, NEW_PC_REC, attrib, &unique_id);
   }

   free_Memo(&new_memo);
   memo_clist_redraw();

   glob_find_id = unique_id;
   memo_find();

   return;
}

/* Do masking like Palm OS 3.5 */
static void clear_mymemo(MyMemo *mmemo)
{
   mmemo->unique_id=0;
   mmemo->attrib=mmemo->attrib & 0xF8;
   if (mmemo->memo.text) {
      free(mmemo->memo.text);
      mmemo->memo.text=strdup("");
   }

   return;
}
/* End Masking */


static void cb_edit_cats(GtkWidget *widget, gpointer data)
{
   struct MemoAppInfo ai;
   char db_name[256];
   char pdb_name[256];
   char full_name[256];
   char buffer[65536];
   int num, r;
   int size;
   void *buf;
   long ivalue;
   struct pi_file *pf;

   jp_logf(JP_LOG_DEBUG, "cb_edit_cats\n");

   get_pref(PREF_MEMO32_MODE, &ivalue, NULL);
   if (ivalue) {
      strcpy(pdb_name, "Memo32DB.pdb");
      strcpy(db_name, "Memo32DB");
   } else {
      strcpy(pdb_name, "MemoDB.pdb");
      strcpy(db_name, "MemoDB");
   }
   get_home_file_name(pdb_name, full_name, 250);

   buf=NULL;
   bzero(&ai, sizeof(ai));

   pf = pi_file_open(full_name);
   r = pi_file_get_app_info(pf, &buf, &size);

   num = unpack_MemoAppInfo(&ai, buf, size);
   if (num <= 0) {
      jp_logf(JP_LOG_WARN, _("Error reading %s\n"), pdb_name);
      return;
   }

   pi_file_close(pf);

   edit_cats(widget, db_name, &(ai.category));

   size = pack_MemoAppInfo(&ai, buffer, 65535);

   pdb_file_write_app_block("MemoDB", buffer, size);

   cb_app_button(NULL, GINT_TO_POINTER(REDRAW));
}


static void cb_clist_selection(GtkWidget      *clist,
			       gint           row,
			       gint           column,
			       GdkEventButton *event,
			       gpointer       data)
{
   struct Memo *memo;
   MyMemo *mmemo;
   int i, index, count, b;
   int sorted_position;
   int keep;

   if ((!event) && (clist_hack)) return;

   /* HACK */
   if (clist_hack) {
      keep=record_changed;
      gtk_clist_select_row(GTK_CLIST(clist), clist_row_selected, column);
      b=dialog_save_changed_record(pane, record_changed);
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

   clist_row_selected=row;

   mmemo = gtk_clist_get_row_data(GTK_CLIST(clist), row);
   if (mmemo==NULL) {
      return;
   }

   set_new_button_to(CLEAR_FLAG);

   connect_changed_signals(DISCONNECT_SIGNALS);

   memo=&(mmemo->memo);

   index = mmemo->attrib & 0x0F;
   sorted_position = find_sorted_cat(index);
   if (memo_cat_menu_item2[sorted_position]==NULL) {
      /* Illegal category */
      jp_logf(JP_LOG_DEBUG, "Category is not legal\n");
      index = sorted_position = 0;
   }
   /* We need to count how many items down in the list this is */
   for (i=sorted_position, count=0; i>=0; i--) {
      if (memo_cat_menu_item2[i]) {
	 count++;
      }
   }
   count--;

   if (sorted_position<0) {
      jp_logf(JP_LOG_WARN, "Category is not legal\n");
   } else {
      gtk_check_menu_item_set_active
	(GTK_CHECK_MENU_ITEM(memo_cat_menu_item2[sorted_position]), TRUE);
   }
   gtk_option_menu_set_history(GTK_OPTION_MENU(category_menu2), count);

   gtk_text_freeze(GTK_TEXT(memo_text));

   gtk_text_set_point(GTK_TEXT(memo_text), 0);
   gtk_text_forward_delete(GTK_TEXT(memo_text),
			   gtk_text_get_length(GTK_TEXT(memo_text)));

   gtk_text_insert(GTK_TEXT(memo_text), NULL,NULL,NULL, memo->text, -1);

   gtk_text_thaw(GTK_TEXT(memo_text));

   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(private_checkbox),
				mmemo->attrib & dlpRecAttrSecret);

   connect_changed_signals(CONNECT_SIGNALS);
}

static void memo_update_clist(GtkWidget *clist, GtkWidget *tooltip_widget,
			      MemoList *memo_list, int category, int main)
{
#define MEMO_CLIST_CHAR_WIDTH 50
   int num_entries, entries_shown, i;
   int row_count;
   size_t copy_max_length;
   char *last;
   gchar *empty_line[] = { "" };
   GdkColor color;
   GdkColormap *colormap;
   MemoList *temp_memo;
   char str[MEMO_CLIST_CHAR_WIDTH+10];
   int len, len1;
   int show_priv;

   jp_logf(JP_LOG_DEBUG, "memo_update_clist()\n");

   row_count=((GtkCList *)clist)->rows;

   free_MemoList(&memo_list);

   /* Need to get the private ones back for the hints calculation */
   num_entries = get_memos2(&memo_list, SORT_ASCENDING, 2, 2, 1, CATEGORY_ALL);

   if (memo_list==NULL) {
      if (tooltip_widget) {
	 gtk_tooltips_set_tip(glob_tooltips, tooltip_widget, _("0 records"), NULL);
      }
      return;
   }

   gtk_clist_freeze(GTK_CLIST(clist));

   show_priv = show_privates(GET_PRIVATES);

   for (temp_memo = memo_list; temp_memo; temp_memo=temp_memo->next) {
      if ( ((temp_memo->mmemo.attrib & 0x0F) != category) &&
	  category != CATEGORY_ALL) {
	 continue;
      }

      if ((show_priv != SHOW_PRIVATES) && 
	  (temp_memo->mmemo.attrib & dlpRecAttrSecret)) {
	 continue;
      }
   }

   entries_shown=0;
   for (temp_memo = memo_list; temp_memo; temp_memo=temp_memo->next) {
      if ( ((temp_memo->mmemo.attrib & 0x0F) != category) &&
	  category != CATEGORY_ALL) {
	 continue;
      }

      /* Do masking like Palm OS 3.5 */
      if ((show_priv == MASK_PRIVATES) && 
	  (temp_memo->mmemo.attrib & dlpRecAttrSecret)) {
	 if (entries_shown+1>row_count) {
	    gtk_clist_append(GTK_CLIST(clist), empty_line);
	 }
	 gtk_clist_set_text(GTK_CLIST(clist), entries_shown, 0, "----------------------------------------");
	 clear_mymemo(&temp_memo->mmemo);
	 gtk_clist_set_row_data(GTK_CLIST(clist), entries_shown, &(temp_memo->mmemo));
	 gtk_clist_set_background(GTK_CLIST(clist), entries_shown, NULL);
	 entries_shown++;
	 continue;
      }
      /* End Masking */
      if ((show_priv != SHOW_PRIVATES) && 
	  (temp_memo->mmemo.attrib & dlpRecAttrSecret)) {
	 continue;
      }

      if (entries_shown+1>row_count) {
	 gtk_clist_append(GTK_CLIST(clist), empty_line);
      }
      sprintf(str, "%d. ", entries_shown + 1);

      len1 = strlen(str);
      len = strlen(temp_memo->mmemo.memo.text)+1;
      /* ..memo clist does not display '/n' */
      if ((copy_max_length = len) > MEMO_CLIST_CHAR_WIDTH) {
	 copy_max_length = MEMO_CLIST_CHAR_WIDTH;
      }
      last = (char *)multibyte_safe_memccpy(str+len1, temp_memo->mmemo.memo.text,'\n', copy_max_length);
      if (last) {
	 *(last-1)='\0';
      } else {
	 str[copy_max_length + len1]='\0';
      }
      gtk_clist_set_text(GTK_CLIST(clist), entries_shown, 0, str);
      gtk_clist_set_row_data(GTK_CLIST(clist), entries_shown, &(temp_memo->mmemo));

      switch (temp_memo->mmemo.rt) {
       case NEW_PC_REC:
       case REPLACEMENT_PALM_REC:
	 colormap = gtk_widget_get_colormap(clist);
	 color.red=CLIST_NEW_RED;
	 color.green=CLIST_NEW_GREEN;
	 color.blue=CLIST_NEW_BLUE;
	 gdk_color_alloc(colormap, &color);
	 gtk_clist_set_background(GTK_CLIST(clist), entries_shown, &color);
	 break;
       case DELETED_PALM_REC:
	 colormap = gtk_widget_get_colormap(clist);
	 color.red=CLIST_DEL_RED;
	 color.green=CLIST_DEL_GREEN;
	 color.blue=CLIST_DEL_BLUE;
	 gdk_color_alloc(colormap, &color);
	 gtk_clist_set_background(GTK_CLIST(clist), entries_shown, &color);
	 break;
       case MODIFIED_PALM_REC:
	 colormap = gtk_widget_get_colormap(clist);
	 color.red=CLIST_MOD_RED;
	 color.green=CLIST_MOD_GREEN;
	 color.blue=CLIST_MOD_BLUE;
	 gdk_color_alloc(colormap, &color);
	 gtk_clist_set_background(GTK_CLIST(clist), entries_shown, &color);
	 break;
       default:
	 if (temp_memo->mmemo.attrib & dlpRecAttrSecret) {
	    colormap = gtk_widget_get_colormap(clist);
	    color.red=CLIST_PRIVATE_RED;
	    color.green=CLIST_PRIVATE_GREEN;
	    color.blue=CLIST_PRIVATE_BLUE;
	    gdk_color_alloc(colormap, &color);
	    gtk_clist_set_background(GTK_CLIST(clist), entries_shown, &color);
	 } else {
	    gtk_clist_set_background(GTK_CLIST(clist), entries_shown, NULL);
	 }
      }
      entries_shown++;
   }

   jp_logf(JP_LOG_DEBUG, "entries_shown=%d\n", entries_shown);

   for (i=row_count-1; i>=entries_shown; i--) {
      gtk_clist_set_row_data(GTK_CLIST(clist), i, NULL);
      gtk_clist_remove(GTK_CLIST(clist), i);
   }

   /*If there is an item in the list, select the first one */
   if ((main) && (entries_shown>0)) {
      gtk_clist_select_row(GTK_CLIST(clist), 0, 0);
      cb_clist_selection(clist, 0, 0, (gpointer)455, NULL);
   }

   gtk_clist_thaw(GTK_CLIST(clist));

   sprintf(str, _("%d of %d records"), entries_shown, num_entries);
   if (tooltip_widget) {
      gtk_tooltips_set_tip(glob_tooltips, tooltip_widget, str, NULL);
   }

   if (main) {
      connect_changed_signals(CONNECT_SIGNALS);
   }

   jp_logf(JP_LOG_DEBUG, "Leaving memo_update_clist()\n");
}

static int memo_find()
{
   int r, found_at, total_count;

   if (glob_find_id) {
      r = clist_find_id(clist,
			glob_find_id,
			&found_at,
			&total_count);
      if (r) {
	 if (total_count == 0) {
	    total_count = 1;
	 }
	 gtk_clist_select_row(GTK_CLIST(clist), found_at, 0);
	 cb_clist_selection(clist, found_at, 0, (gpointer)455, NULL);
	 if (!gtk_clist_row_is_visible(GTK_CLIST(clist), found_at)) {
	    gtk_clist_moveto(GTK_CLIST(clist), found_at, 0, 0.5, 0.0);
	 }
      }
      glob_find_id = 0;
   }
   return 0;
}

/* This redraws the clist and goes back to the same line number */
int memo_clist_redraw()
{
   int line_num;

   line_num = clist_row_selected;

   memo_update_clist(clist, category_menu1, glob_memo_list, memo_category, TRUE);

   gtk_clist_select_row(GTK_CLIST(clist), line_num, 0);
   cb_clist_selection(clist, line_num, 0, (gpointer)455, NULL);

   return 0;
}

int memo_refresh()
{
   int index;

   if (glob_find_id) {
      memo_category = CATEGORY_ALL;
   }
   if (memo_category==CATEGORY_ALL) {
      index=0;
   } else {
      index=find_sorted_cat(memo_category)+1;
   }
   memo_update_clist(clist, category_menu1, glob_memo_list, memo_category, TRUE);
   if (index<0) {
      jp_logf(JP_LOG_WARN, "Category not legal\n");
   } else {	 
      gtk_option_menu_set_history(GTK_OPTION_MENU(category_menu1), index);
      gtk_check_menu_item_set_active
	(GTK_CHECK_MENU_ITEM(memo_cat_menu_item1[index]), TRUE);
   }
   memo_find();
   return 0;
}

int memo_gui_cleanup()
{
   int b;

   b=dialog_save_changed_record(pane, record_changed);
   if (b==DIALOG_SAID_1) {
      cb_add_new_record(NULL, GINT_TO_POINTER(record_changed));
   }
   free_MemoList(&glob_memo_list);
   connect_changed_signals(DISCONNECT_SIGNALS);
   set_pref(PREF_MEMO_PANE, GTK_PANED(pane)->handle_xpos, NULL, TRUE);
   return 0;
}

/*
 * Main function
 */
int memo_gui(GtkWidget *vbox, GtkWidget *hbox)
{
   extern GtkWidget *glob_date_label;
   extern int glob_date_timer_tag;
   int i;
   GtkWidget *vbox1, *vbox2, *hbox_temp;
   GtkWidget *separator;
   GtkWidget *button;
   GtkWidget *vscrollbar;
   long ivalue;
   const char *svalue;

   clist_row_selected=0;
   record_changed=CLEAR_FLAG;

   /* Do some initialization */
   for (i=0; i<NUM_MEMO_CAT_ITEMS; i++) {
      memo_cat_menu_item2[i] = NULL;
   }

   get_memo_app_info(&memo_app_info);

   for (i=0; i<NUM_MEMO_CAT_ITEMS; i++) {
      sort_l[i].Pcat = memo_app_info.category.name[i];
      sort_l[i].cat_num = i;
   }
   qsort(sort_l, NUM_MEMO_CAT_ITEMS, sizeof(struct sorted_cats), cat_compare);

#ifdef JPILOT_DEBUG
   for (i=0; i<NUM_MEMO_CAT_ITEMS; i++) {
      printf("cat %d %s\n", sort_l[i].cat_num, sort_l[i].Pcat);
   }
#endif

   if ((memo_category != CATEGORY_ALL) && (memo_app_info.category.name[memo_category][0]=='\0')) {
      memo_category=CATEGORY_ALL;
   }

   pane = gtk_hpaned_new();
   get_pref(PREF_MEMO_PANE, &ivalue, &svalue);
   gtk_paned_set_position(GTK_PANED(pane), ivalue + 2);

   gtk_box_pack_start(GTK_BOX(hbox), pane, TRUE, TRUE, 5);

   vbox1 = gtk_vbox_new(FALSE, 0);
   vbox2 = gtk_vbox_new(FALSE, 0);

   gtk_paned_pack1(GTK_PANED(pane), vbox1, TRUE, FALSE);
   gtk_paned_pack2(GTK_PANED(pane), vbox2, TRUE, FALSE);

   /* gtk_widget_set_usize(GTK_WIDGET(vbox1), 210, 0); */

   /* Add buttons in left vbox */
   /* Separator */
   separator = gtk_hseparator_new();
   gtk_box_pack_start(GTK_BOX(vbox1), separator, FALSE, FALSE, 5);

   /* Make the Today is: label */
   glob_date_label = gtk_label_new(" ");
   gtk_box_pack_start(GTK_BOX(vbox1), glob_date_label, FALSE, FALSE, 0);
   timeout_date(NULL);
   glob_date_timer_tag = gtk_timeout_add(CLOCK_TICK, timeout_date, NULL);


   /* Separator */
   separator = gtk_hseparator_new();
   gtk_box_pack_start(GTK_BOX(vbox1), separator, FALSE, FALSE, 5);


   /* Category Box */
   hbox_temp = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox1), hbox_temp, FALSE, FALSE, 0);

   /* Put the left-hand category menu up */
   make_category_menu(&category_menu1, memo_cat_menu_item1,
		      sort_l, cb_category, TRUE);
   gtk_box_pack_start(GTK_BOX(hbox_temp), category_menu1, TRUE, TRUE, 0);


   /* Edit category button */
   button = gtk_button_new_with_label(_("Edit Categories"));
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_edit_cats), NULL);
   gtk_box_pack_start(GTK_BOX(hbox_temp), button, FALSE, FALSE, 0);


   /* Memo32 check box */
   memo32_checkbox = gtk_check_button_new_with_label(_("Use Memo32 (pedit32)"));
   gtk_box_pack_start(GTK_BOX(vbox1), memo32_checkbox, FALSE, FALSE, 0);
   get_pref(PREF_MEMO32_MODE, &ivalue, NULL);
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(memo32_checkbox), ivalue);
   gtk_signal_connect(GTK_OBJECT(memo32_checkbox), "clicked",
		      GTK_SIGNAL_FUNC(cb_use_memo32), NULL);


   /* Put the memo list window up */
   scrolled_window = gtk_scrolled_window_new(NULL, NULL);
   /*gtk_widget_set_usize(GTK_WIDGET(scrolled_window), 330, 100); */
   gtk_container_set_border_width(GTK_CONTAINER(scrolled_window), 0);
   gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
   gtk_box_pack_start(GTK_BOX(vbox1), scrolled_window, TRUE, TRUE, 0);

   clist = gtk_clist_new(1);
   clist_hack=FALSE;

   gtk_signal_connect(GTK_OBJECT(clist), "select_row",
		      GTK_SIGNAL_FUNC(cb_clist_selection), NULL);
   gtk_clist_set_shadow_type(GTK_CLIST(clist), SHADOW);
   /*
    * There is a strange hack here.
    * If a clist is in browse mode and a callback is called on it and then
    * a dialog window is opened (which we do if a record has changed, and we
    * ask to save it) then XFree86 locks up.
    * This was tested with XFree86 4.1.0 / GTK 1.2.10
    *                  and XFree86 3.3.6 / GTK 1.2.8
    *  So, we switch the modes of the clist before opening a dialog window.
    */
   gtk_clist_set_selection_mode(GTK_CLIST(clist), GTK_SELECTION_BROWSE);
   gtk_clist_set_column_width(GTK_CLIST(clist), 0, 50);

   gtk_container_add(GTK_CONTAINER(scrolled_window), GTK_WIDGET(clist));

   /*
    * The right hand part of the main window follows:
    */
   hbox_temp = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, FALSE, FALSE, 0);


   /* Add record modification buttons on right side */
   button = gtk_button_new_with_label(_("Delete"));
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_delete_memo),
		      GINT_TO_POINTER(DELETE_FLAG));
   gtk_box_pack_start(GTK_BOX(hbox_temp), button, TRUE, TRUE, 0);

   button = gtk_button_new_with_label(_("Copy"));
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_add_new_record), 
		      GINT_TO_POINTER(COPY_FLAG));
   gtk_box_pack_start(GTK_BOX(hbox_temp), button, TRUE, TRUE, 0);

   new_record_button = gtk_button_new_with_label(_("New Record"));
   gtk_signal_connect(GTK_OBJECT(new_record_button), "clicked",
		      GTK_SIGNAL_FUNC(cb_add_new_record), 
		      GINT_TO_POINTER(CLEAR_FLAG));
   gtk_box_pack_start(GTK_BOX(hbox_temp), new_record_button, TRUE, TRUE, 0);

   add_record_button = gtk_button_new_with_label(_("Add Record"));
   gtk_signal_connect(GTK_OBJECT(add_record_button), "clicked",
		      GTK_SIGNAL_FUNC(cb_add_new_record),
		      GINT_TO_POINTER(NEW_FLAG));
   gtk_box_pack_start(GTK_BOX(hbox_temp), add_record_button, TRUE, TRUE, 0);
   gtk_widget_set_name(GTK_WIDGET(GTK_LABEL(GTK_BIN(add_record_button)->child)),
		       "label_high");

   apply_record_button = gtk_button_new_with_label(_("Apply Changes"));
   gtk_signal_connect(GTK_OBJECT(apply_record_button), "clicked",
		      GTK_SIGNAL_FUNC(cb_add_new_record), 
		      GINT_TO_POINTER(MODIFY_FLAG));
   gtk_box_pack_start(GTK_BOX(hbox_temp), apply_record_button, TRUE, TRUE, 0);
   gtk_widget_set_name(GTK_WIDGET(GTK_LABEL(GTK_BIN(apply_record_button)->child)),
		       "label_high");


   /*Separator */
   separator = gtk_hseparator_new();
   gtk_box_pack_start(GTK_BOX(vbox2), separator, FALSE, FALSE, 5);


   /*Private check box */
   hbox_temp = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, FALSE, FALSE, 0);
   private_checkbox = gtk_check_button_new_with_label(_("Private"));
   gtk_box_pack_end(GTK_BOX(hbox_temp), private_checkbox, FALSE, FALSE, 0);


   /*Put the right-hand category menu up */
   make_category_menu(&category_menu2, memo_cat_menu_item2,
		      sort_l, NULL, FALSE);
   gtk_box_pack_start(GTK_BOX(hbox_temp), category_menu2, TRUE, TRUE, 0);


   /*The Description text box on the right side */
   hbox_temp = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, FALSE, FALSE, 0);


   hbox_temp = gtk_hbox_new (FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, TRUE, TRUE, 0);

   memo_text = gtk_text_new(NULL, NULL);
   gtk_text_set_editable(GTK_TEXT(memo_text), TRUE);
   gtk_text_set_word_wrap(GTK_TEXT(memo_text), TRUE);
   vscrollbar = gtk_vscrollbar_new(GTK_TEXT(memo_text)->vadj);
   gtk_box_pack_start(GTK_BOX(hbox_temp), memo_text, TRUE, TRUE, 0);
   gtk_box_pack_start(GTK_BOX(hbox_temp), vscrollbar, FALSE, FALSE, 0);

   gtk_widget_show_all(vbox);
   gtk_widget_show_all(hbox);

   gtk_widget_hide(add_record_button);
   gtk_widget_hide(apply_record_button);

   memo_refresh();
   memo_find();

   return 0;
}

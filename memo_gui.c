/* memo_gui.c
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
#include <gtk/gtk.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include "utils.h"
#include "log.h"
#include "prefs.h"
#include "memo.h"

#define NUM_MEMO_CAT_ITEMS 16

extern GtkTooltips *glob_tooltips;

struct MemoAppInfo memo_app_info;
int memo_category;
int clist_row_selected;
GtkWidget *clist;
GtkWidget *memo_text;
GtkWidget *memo_cat_menu2;
//Need one extra for the ALL category
GtkWidget *memo_cat_menu_item1[NUM_MEMO_CAT_ITEMS+1];
GtkWidget *memo_cat_menu_item2[NUM_MEMO_CAT_ITEMS];
GtkWidget *category_menu1;
GtkWidget *scrolled_window;

static void update_memo_screen();
static int memo_clear_details();


void cb_delete_memo(GtkWidget *widget,
		    gpointer   data)
{
   MyMemo *mmemo;
   int flag;
   
   mmemo = gtk_clist_get_row_data(GTK_CLIST(clist), clist_row_selected);
   if (mmemo < (MyMemo *)CLIST_MIN_DATA) {
      return;
   }
   jpilot_logf(LOG_DEBUG, "mmemo->unique_id = %d\n",mmemo->unique_id);
   jpilot_logf(LOG_DEBUG, "mmemo->rt = %d\n",mmemo->rt);
   flag = GPOINTER_TO_INT(data);
   if ((flag==MODIFY_FLAG) || (flag==DELETE_FLAG)) {
      delete_pc_record(MEMO, mmemo, flag);
   }

   update_memo_screen();
}


void cb_memo_category(GtkWidget *item, int selection)
{
   if ((GTK_CHECK_MENU_ITEM(item))->active) {
      memo_category = selection;
      jpilot_logf(LOG_DEBUG, "memo_category = %d\n", memo_category);
      memo_clear_details();
      update_memo_screen();
   }
}

static int memo_clear_details()
{
   gtk_text_freeze(GTK_TEXT(memo_text));

   gtk_text_set_point(GTK_TEXT(memo_text), 0);
   gtk_text_forward_delete(GTK_TEXT(memo_text),
			   gtk_text_get_length(GTK_TEXT(memo_text)));

   gtk_text_thaw(GTK_TEXT(memo_text));
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

   //Get the category that is set from the menu
   for (i=0; i<NUM_MEMO_CAT_ITEMS; i++) {
      if (GTK_IS_WIDGET(memo_cat_menu_item2[i])) {
	 if (GTK_CHECK_MENU_ITEM(memo_cat_menu_item2[i])->active) {
	    *attrib = i;
	    break;
	 }
      }
   }
   return 0;
}

static void cb_add_new_record(GtkWidget *widget,
		       gpointer   data)
{
   MyMemo *mmemo;
   struct Memo new_memo;
   unsigned char attrib;
   int flag;
   
   flag=GPOINTER_TO_INT(data);
   
   if (flag==CLEAR_FLAG) {
      //Clear button was hit
      memo_clear_details();
      return;
   }
   if ((flag!=NEW_FLAG) && (flag!=MODIFY_FLAG)) {
      return;
   }
   if (flag==MODIFY_FLAG) {
      mmemo = gtk_clist_get_row_data(GTK_CLIST(clist), clist_row_selected);
      if (mmemo < (MyMemo *)CLIST_MIN_DATA) {
	 return;
      }
      if ((mmemo->rt==DELETED_PALM_REC) || (mmemo->rt==MODIFIED_PALM_REC)) {
	 jpilot_logf(LOG_INFO, "You can't modify a record that is deleted\n");
	 return;
      }
   }
   memo_get_details(&new_memo, &attrib);
   pc_memo_write(&new_memo, NEW_PC_REC, attrib);
   free_Memo(&new_memo);
   if (flag==MODIFY_FLAG) {
      cb_delete_memo(NULL, data);
   } else {
      update_memo_screen();
   }
   return;
}

static void cb_clist_selection(GtkWidget      *clist,
			       gint           row,
			       gint           column,
			       GdkEventButton *event,
			       gpointer       data)
{
   struct Memo *memo;//, new_a;
   MyMemo *mmemo;
#ifdef OLD_ENTRY
   struct Memo new_memo;
   unsigned char attrib;
#endif
   clist_row_selected=row;

   mmemo = gtk_clist_get_row_data(GTK_CLIST(clist), row);

#ifdef OLD_ENTRY
   if (mmemo == GINT_TO_POINTER(CLIST_NEW_ENTRY_DATA)) {
      gtk_clist_set_text(GTK_CLIST(clist), row, 0,
			 "Fill in details, then click here again");
      gtk_clist_set_row_data(GTK_CLIST(clist), row,
			     GINT_TO_POINTER(CLIST_ADDING_ENTRY_DATA));
      memo_clear_details();
      return;
   }
   
   if (mmemo == GINT_TO_POINTER(CLIST_ADDING_ENTRY_DATA)) {
      memo_get_details(&new_memo, &attrib);
      pc_memo_write(&new_memo, NEW_PC_REC, attrib);
      free_Memo(&new_memo);
      update_memo_screen();
      return;
   }
#endif

   if (mmemo==NULL) {
      return;
   }
   memo=&(mmemo->memo);
   
   gtk_check_menu_item_set_active
     (GTK_CHECK_MENU_ITEM(memo_cat_menu_item2[mmemo->attrib & 0x0F]), TRUE);
   gtk_option_menu_set_history
     (GTK_OPTION_MENU(memo_cat_menu2), mmemo->attrib & 0x0F);

   gtk_text_freeze(GTK_TEXT(memo_text));

   gtk_text_set_point(GTK_TEXT(memo_text), 0);
   gtk_text_forward_delete(GTK_TEXT(memo_text),
			   gtk_text_get_length(GTK_TEXT(memo_text)));

   gtk_text_insert(GTK_TEXT(memo_text), NULL,NULL,NULL, memo->text, -1);

   gtk_text_thaw(GTK_TEXT(memo_text));
}

static void update_memo_screen()
{
#define MEMO_CLIST_CHAR_WIDTH 50
   int num_entries, entries_shown, precount;
   char *last;
   int ivalue;
   const char *svalue;
   gchar *empty_line[] = { "" };
   GdkColor color;
   GdkColormap *colormap;
   MemoList *temp_memo;
   static MemoList *memo_list=NULL;
   char str[MEMO_CLIST_CHAR_WIDTH+2];

   free_MemoList(&memo_list);

   num_entries = get_memos(&memo_list);
   gtk_clist_clear(GTK_CLIST(clist));
   //gtk_text_backward_delete(GTK_TEXT(memo_text1),
	//		    gtk_text_get_length(GTK_TEXT(memo_text1)));
   
   //if (memo_list==NULL) {
   //   return;
   //}

   //Clear the text box to make things look nice
   //gtk_text_set_point(GTK_TEXT(memo_text), 0);
   //gtk_text_forward_delete(GTK_TEXT(memo_text),
   //gtk_text_get_length(GTK_TEXT(memo_text)));

   if (memo_list==NULL) {
      gtk_tooltips_set_tip(glob_tooltips, category_menu1, "0 records", NULL);   
      return;
   }

   gtk_clist_freeze(GTK_CLIST(clist));

   precount=0;
   for (temp_memo = memo_list; temp_memo; temp_memo=temp_memo->next) {
      if ( ((temp_memo->mmemo.attrib & 0x0F) != memo_category) &&
	  memo_category != CATEGORY_ALL) {
	 continue;
      }
      if (temp_memo->mmemo.rt == MODIFIED_PALM_REC) {
	 get_pref(PREF_SHOW_MODIFIED, &ivalue, &svalue);
	 //this will be in preferences as to whether you want to
	 //see deleted records, or not.
	 if (!ivalue) {
	    num_entries--;
	    continue;
	 }
      }
      if (temp_memo->mmemo.rt == DELETED_PALM_REC) {
	 get_pref(PREF_SHOW_DELETED, &ivalue, &svalue);
	 //this will be in preferences as to whether you want to
	 //see deleted records, or not.
	 if (!ivalue) {
	    num_entries--;
	    continue;
	 }
      }
      precount++;
   }

   entries_shown=0;
   for (temp_memo = memo_list; temp_memo; temp_memo=temp_memo->next) {
      if ( ((temp_memo->mmemo.attrib & 0x0F) != memo_category) &&
	  memo_category != CATEGORY_ALL) {
	 continue;
      }
      if (temp_memo->mmemo.rt == MODIFIED_PALM_REC) {
	 get_pref(PREF_SHOW_MODIFIED, &ivalue, &svalue);
	 //this will be in preferences as to whether you want to
	 //see deleted records, or not.
	 if (!ivalue) {
	    num_entries--;
	    continue;
	 }
      }
      if (temp_memo->mmemo.rt == DELETED_PALM_REC) {
	 get_pref(PREF_SHOW_DELETED, &ivalue, &svalue);
	 //this will be in preferences as to whether you want to
	 //see deleted records, or not.
	 if (!ivalue) {
	    num_entries--;
	    continue;
	 }
      }

      entries_shown++;
      gtk_clist_prepend(GTK_CLIST(clist), empty_line);
      sprintf(str, "%d. ", precount - entries_shown + 1);
      last = (char *)memccpy(str+strlen(str), temp_memo->mmemo.memo.text,
			     '\n', MEMO_CLIST_CHAR_WIDTH-strlen(str));
      if (last) {
	 *(last-1)='\0';
      } else {
	 str[MEMO_CLIST_CHAR_WIDTH]='\0';
      }
      gtk_clist_set_text(GTK_CLIST(clist), 0, 0, str);
      gtk_clist_set_row_data(GTK_CLIST(clist), 0, &(temp_memo->mmemo));

      if (temp_memo->mmemo.rt == NEW_PC_REC) {
	 colormap = gtk_widget_get_colormap(clist);
	 color.red=CLIST_NEW_RED;
	 color.green=CLIST_NEW_GREEN;
	 color.blue=CLIST_NEW_BLUE;
	 gdk_color_alloc(colormap, &color);
	 gtk_clist_set_background(GTK_CLIST(clist), 0, &color);
      }
      if (temp_memo->mmemo.rt == DELETED_PALM_REC) {
	 colormap = gtk_widget_get_colormap(clist);
	 color.red=CLIST_DEL_RED;
	 color.green=CLIST_DEL_GREEN;
	 color.blue=CLIST_DEL_BLUE;
	 gdk_color_alloc(colormap, &color);
	 gtk_clist_set_background(GTK_CLIST(clist), 0, &color);
      }  
      if (temp_memo->mmemo.rt == MODIFIED_PALM_REC) {
	 colormap = gtk_widget_get_colormap(clist);
	 color.red=CLIST_MOD_RED;
	 color.green=CLIST_MOD_GREEN;
	 color.blue=CLIST_MOD_BLUE;
	 gdk_color_alloc(colormap, &color);
	 gtk_clist_set_background(GTK_CLIST(clist), 0, &color);
      }  
   }

   jpilot_logf(LOG_DEBUG, "entries_shown=%d\n",entries_shown);
#ifdef OLD_ENTRY
   gtk_clist_append(GTK_CLIST(clist), empty_line);
   gtk_clist_set_text(GTK_CLIST(clist), entries_shown, 0,
		      "Select here to add an entry");
   gtk_clist_set_row_data(GTK_CLIST(clist), entries_shown,
			  GINT_TO_POINTER(CLIST_NEW_ENTRY_DATA));
#endif
   //If there is an item in the list, select the first one
   if (entries_shown>0) {
      gtk_clist_select_row(GTK_CLIST(clist), 0, 0);
      //cb_clist_selection(clist, 0, 0, (GdkEventButton *)455, "");
   }
   
   gtk_clist_thaw(GTK_CLIST(clist));

   sprintf(str, "%d of %d records", entries_shown, num_entries);
   gtk_tooltips_set_tip(glob_tooltips, category_menu1, str, NULL);   
}

static int make_category_menu1(GtkWidget **category_menu)
{
   GtkWidget *menu;
   GSList    *group;
   int i;

   *category_menu = gtk_option_menu_new();
   
   menu = gtk_menu_new();
   group = NULL;
   
   memo_cat_menu_item1[0] = gtk_radio_menu_item_new_with_label(group, "All");
   gtk_signal_connect(GTK_OBJECT(memo_cat_menu_item1[0]), "activate",
		      cb_memo_category, GINT_TO_POINTER(CATEGORY_ALL));
   group = gtk_radio_menu_item_group(GTK_RADIO_MENU_ITEM(memo_cat_menu_item1[0]));
   gtk_menu_append(GTK_MENU(menu), memo_cat_menu_item1[0]);
   gtk_widget_show(memo_cat_menu_item1[0]);
   for (i=0; i<NUM_MEMO_CAT_ITEMS; i++) {
      if (memo_app_info.category.name[i][0]) {
	 memo_cat_menu_item1[i+1] = gtk_radio_menu_item_new_with_label(
		     group, memo_app_info.category.name[i]);
	 gtk_signal_connect(GTK_OBJECT(memo_cat_menu_item1[i+1]), "activate",
			    cb_memo_category, GINT_TO_POINTER(i));
	 group = gtk_radio_menu_item_group(GTK_RADIO_MENU_ITEM(memo_cat_menu_item1[i+1]));
	 gtk_menu_append(GTK_MENU(menu), memo_cat_menu_item1[i+1]);
	 gtk_widget_show(memo_cat_menu_item1[i+1]);
      }
   }

   gtk_option_menu_set_menu(GTK_OPTION_MENU(*category_menu), menu);
   
   return 0;
}

static int make_category_menu2()
{
   int i;
   
   GtkWidget *menu;
   GSList    *group;

   memo_cat_menu2 = gtk_option_menu_new();
   
   menu = gtk_menu_new();
   group = NULL;
   
   for (i=0; i<NUM_MEMO_CAT_ITEMS; i++) {
      if (memo_app_info.category.name[i][0]) {
	 memo_cat_menu_item2[i] = gtk_radio_menu_item_new_with_label
	   (group, memo_app_info.category.name[i]);
	 group = gtk_radio_menu_item_group
	   (GTK_RADIO_MENU_ITEM(memo_cat_menu_item2[i]));
	 gtk_menu_append(GTK_MENU(menu), memo_cat_menu_item2[i]);
	 gtk_widget_show(memo_cat_menu_item2[i]);
      }
   }
   gtk_option_menu_set_menu(GTK_OPTION_MENU(memo_cat_menu2), menu);
   return 0;
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
	 gtk_clist_select_row(GTK_CLIST(clist), found_at, 1);
	 move_scrolled_window_hack(scrolled_window,
				   (float)found_at/(float)total_count);
      }
      glob_find_id = 0;
   }
   return 0;
}

int memo_refresh()
{
   memo_category = CATEGORY_ALL;
   update_memo_screen();
   gtk_option_menu_set_history(GTK_OPTION_MENU(category_menu1), 0);
   gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
       memo_cat_menu_item1[0]), TRUE);
   memo_find();   
   return 0;
}

//
//Main function
//
int memo_gui(GtkWidget *vbox, GtkWidget *hbox)
{
   extern GtkWidget *glob_date_label;
   extern glob_date_timer_tag;
   GtkWidget *vbox1, *vbox2, *hbox_temp;
   GtkWidget *separator;
   GtkWidget *button;
   GtkWidget *vscrollbar;
   
   clist_row_selected=0;

   get_memo_app_info(&memo_app_info);

   vbox1 = gtk_vbox_new (FALSE, 0);
   vbox2 = gtk_vbox_new (FALSE, 0);
   gtk_box_pack_start(GTK_BOX(hbox), vbox1, TRUE, TRUE, 5);
   gtk_box_pack_start(GTK_BOX(hbox), vbox2, TRUE, TRUE, 5);

   gtk_widget_set_usize(GTK_WIDGET(vbox1), 210, 0);

   //Add buttons in left vbox
   button = gtk_button_new_with_label("Delete");
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_delete_memo),
		      GINT_TO_POINTER(DELETE_FLAG));
   gtk_box_pack_start(GTK_BOX(vbox), button, TRUE, TRUE, 0);
   gtk_widget_show(button);
   
   //Separator
   separator = gtk_hseparator_new();
   gtk_box_pack_start(GTK_BOX(vbox1), separator, FALSE, FALSE, 5);
   gtk_widget_show(separator);

   //Make the Today is: label
   glob_date_label = gtk_label_new(" ");
   gtk_box_pack_start(GTK_BOX(vbox1), glob_date_label, FALSE, FALSE, 0);
   gtk_widget_show(glob_date_label);
   timeout_date(NULL);
   glob_date_timer_tag = gtk_timeout_add(CLOCK_TICK, timeout_date, NULL);


   //Separator
   separator = gtk_hseparator_new();
   gtk_box_pack_start(GTK_BOX(vbox1), separator, FALSE, FALSE, 5);
   gtk_widget_show(separator);


   //Put the left-hand category menu up
   make_category_menu1(&category_menu1);   
   gtk_box_pack_start(GTK_BOX(vbox1), category_menu1, FALSE, FALSE, 0);
   gtk_widget_show(category_menu1);


   //Put the address list window up
   scrolled_window = gtk_scrolled_window_new(NULL, NULL);
   //gtk_widget_set_usize(GTK_WIDGET(scrolled_window), 330, 100);
   gtk_container_set_border_width(GTK_CONTAINER(scrolled_window), 0);
   gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
   gtk_box_pack_start(GTK_BOX(vbox1), scrolled_window, TRUE, TRUE, 0);
   gtk_widget_show(scrolled_window);

   clist = gtk_clist_new(1);
   gtk_signal_connect(GTK_OBJECT(clist), "select_row",
		      GTK_SIGNAL_FUNC(cb_clist_selection),
		      memo_text);
   gtk_clist_set_shadow_type(GTK_CLIST(clist), SHADOW);
   gtk_clist_set_selection_mode(GTK_CLIST(clist), GTK_SELECTION_BROWSE);
   gtk_clist_set_column_width(GTK_CLIST(clist), 0, 14);
   gtk_clist_set_column_width(GTK_CLIST(clist), 1, 8);
   gtk_clist_set_column_width(GTK_CLIST(clist), 2, 242);
   gtk_clist_set_column_width(GTK_CLIST(clist), 3, 14);
   gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW
					 (scrolled_window), clist);
   gtk_widget_show(clist);
   
   
   
   //
   // The right hand part of the main window follows:
   //
   hbox_temp = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, FALSE, FALSE, 0);
   gtk_widget_show(hbox_temp);

   
   //Add record modification buttons on right side
   button = gtk_button_new_with_label("Add It");
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_add_new_record), 
		      GINT_TO_POINTER(NEW_FLAG));
   gtk_box_pack_start(GTK_BOX(hbox_temp), button, TRUE, TRUE, 0);
   gtk_widget_show(button);
   
   button = gtk_button_new_with_label("Apply Changes");
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_add_new_record), 
		      GINT_TO_POINTER(MODIFY_FLAG));
   gtk_box_pack_start(GTK_BOX(hbox_temp), button, TRUE, TRUE, 0);
   gtk_widget_show(button);
   
   button = gtk_button_new_with_label("Clear");
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_add_new_record), 
		      GINT_TO_POINTER(CLEAR_FLAG));
   gtk_box_pack_start(GTK_BOX(hbox_temp), button, TRUE, TRUE, 0);
   gtk_widget_show(button);


   //Separator
   separator = gtk_hseparator_new();
   gtk_box_pack_start(GTK_BOX(vbox2), separator, FALSE, FALSE, 5);
   gtk_widget_show(separator);


   //Put the right-hand category menu up
   make_category_menu2();   
   gtk_box_pack_start(GTK_BOX (vbox2), memo_cat_menu2, FALSE, FALSE, 0);
   gtk_widget_show(memo_cat_menu2);
   
   
   //The Description text box on the right side
   hbox_temp = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, FALSE, FALSE, 0);
   gtk_widget_show(hbox_temp);


   hbox_temp = gtk_hbox_new (FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, TRUE, TRUE, 0);

   memo_text = gtk_text_new(NULL, NULL);
   gtk_text_set_editable(GTK_TEXT(memo_text), TRUE);
   gtk_text_set_word_wrap(GTK_TEXT(memo_text), TRUE);
   vscrollbar = gtk_vscrollbar_new(GTK_TEXT(memo_text)->vadj);
   gtk_box_pack_start(GTK_BOX(hbox_temp), memo_text, TRUE, TRUE, 0);
   gtk_box_pack_start(GTK_BOX(hbox_temp), vscrollbar, FALSE, FALSE, 0);
   gtk_widget_show(memo_text);
   gtk_widget_show(vscrollbar);   
   gtk_widget_show(hbox_temp);

   gtk_widget_show(vbox1);
   gtk_widget_show(vbox2);
   gtk_widget_show(vbox);
   gtk_widget_show(hbox);

   memo_refresh();
   memo_find();

   return 0;
}

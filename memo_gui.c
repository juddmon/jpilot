/* memo_gui.c
 * 
 * Copyright (C) 1999 by Judd Montgomery
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

static void update_memo_screen();
static int memo_clear_details();
int memo_clist_redraw();
static void connect_changed_signals(int con_or_dis);
static int memo_find();


static void
set_new_button_to(int new_state)
{
   jpilot_logf(LOG_DEBUG, "set_new_button_to new %d old %d\n", new_state, record_changed);
   if (record_changed==new_state) {
      return;
   }
   
   switch (new_state) {
    case MODIFY_FLAG:
      gtk_widget_show(apply_record_button);
      break;
    case NEW_FLAG:
      gtk_widget_show(add_record_button);
      break;
    case CLEAR_FLAG:
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
   jpilot_logf(LOG_DEBUG, "cb_record_changed\n");
   if (record_changed==CLEAR_FLAG) {
      connect_changed_signals(DISCONNECT_SIGNALS);
      set_new_button_to(MODIFY_FLAG);
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
   
   mmemo = gtk_clist_get_row_data(GTK_CLIST(clist), clist_row_selected);
   if (mmemo < (MyMemo *)CLIST_MIN_DATA) {
      return;
   }
   jpilot_logf(LOG_DEBUG, "mmemo->unique_id = %d\n",mmemo->unique_id);
   jpilot_logf(LOG_DEBUG, "mmemo->rt = %d\n",mmemo->rt);
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


void cb_memo_category(GtkWidget *item, int selection)
{
   if ((GTK_CHECK_MENU_ITEM(item))->active) {
      memo_category = selection;
      jpilot_logf(LOG_DEBUG, "cb_memo_category() cat=%d\n", memo_category);
      memo_clear_details();
      update_memo_screen();
      jpilot_logf(LOG_DEBUG, "Leaving cb_memo_category()\n");
   }
}

static int memo_clear_details()
{
   int new_cat;
   int sorted_position;

   jpilot_logf(LOG_DEBUG, "memo_clear_details()\n");
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
      jpilot_logf(LOG_WARN, "Category is not legal\n");
   } else {
      gtk_check_menu_item_set_active
	(GTK_CHECK_MENU_ITEM(memo_cat_menu_item2[sorted_position]), TRUE);
      gtk_option_menu_set_history(GTK_OPTION_MENU(category_menu2), sorted_position);
   }

   set_new_button_to(CLEAR_FLAG);
   jpilot_logf(LOG_DEBUG, "Leaving memo_clear_details()\n");
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

static void cb_add_new_record(GtkWidget *widget,
		       gpointer   data)
{
   MyMemo *mmemo;
   struct Memo new_memo;
   unsigned char attrib;
   int flag;
   unsigned int unique_id;
   
   flag=GPOINTER_TO_INT(data);
   
   if (flag==CLEAR_FLAG) {
      /*Clear button was hit */
      memo_clear_details();
      connect_changed_signals(DISCONNECT_SIGNALS);
      set_new_button_to(NEW_FLAG);
      gtk_widget_grab_focus(GTK_WIDGET(memo_text));
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

   set_new_button_to(CLEAR_FLAG);
   
   pc_memo_write(&new_memo, NEW_PC_REC, attrib, &unique_id);
   free_Memo(&new_memo);
   if (flag==MODIFY_FLAG) {
      cb_delete_memo(NULL, data);
   } else {
      memo_clist_redraw();
   }
   glob_find_id = unique_id;
   memo_find();
   
   return;
}

static void cb_clist_selection(GtkWidget      *clist,
			       gint           row,
			       gint           column,
			       GdkEventButton *event,
			       gpointer       data)
{
   struct Memo *memo;/*, new_a; */
   MyMemo *mmemo;
   int i, index, count;
   int sorted_position;
   clist_row_selected=row;

   mmemo = gtk_clist_get_row_data(GTK_CLIST(clist), row);

   set_new_button_to(CLEAR_FLAG);
   
   connect_changed_signals(DISCONNECT_SIGNALS);
   
   if (mmemo==NULL) {
      return;
   }
   memo=&(mmemo->memo);
   
   index = mmemo->attrib & 0x0F;
   sorted_position = find_sorted_cat(index);
   if (memo_cat_menu_item2[sorted_position]==NULL) {
      /* Illegal category */
      jpilot_logf(LOG_DEBUG, "Category is not legal\n");
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
      jpilot_logf(LOG_WARN, "Category is not legal\n");
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

static void update_memo_screen()
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
   static MemoList *memo_list=NULL;
   char str[MEMO_CLIST_CHAR_WIDTH+2];
   int len;
   int show_priv;

   jpilot_logf(LOG_DEBUG, "update_memo_screen()\n");

   row_count=((GtkCList *)clist)->rows;
   
   free_MemoList(&memo_list);

   num_entries = get_memos2(&memo_list, SORT_ASCENDING, 2, 2, 1, CATEGORY_ALL);

   if (memo_list==NULL) {
      gtk_tooltips_set_tip(glob_tooltips, category_menu1, _("0 records"), NULL);   
      return;
   }

   gtk_clist_freeze(GTK_CLIST(clist));

   show_priv = show_privates(GET_PRIVATES, NULL);

   for (temp_memo = memo_list; temp_memo; temp_memo=temp_memo->next) {
      if ( ((temp_memo->mmemo.attrib & 0x0F) != memo_category) &&
	  memo_category != CATEGORY_ALL) {
	 continue;
      }

      if ((show_priv != SHOW_PRIVATES) && 
	  (temp_memo->mmemo.attrib & dlpRecAttrSecret)) {
	 continue;
      }
   }

   entries_shown=0;
   for (temp_memo = memo_list; temp_memo; temp_memo=temp_memo->next) {
      if ( ((temp_memo->mmemo.attrib & 0x0F) != memo_category) &&
	  memo_category != CATEGORY_ALL) {
	 continue;
      }

      if ((show_priv != SHOW_PRIVATES) && 
	  (temp_memo->mmemo.attrib & dlpRecAttrSecret)) {
	 continue;
      }

      if (entries_shown+1>row_count) {
	 gtk_clist_append(GTK_CLIST(clist), empty_line);
      }
      sprintf(str, "%d. ", entries_shown + 1);

      len=strlen(temp_memo->mmemo.memo.text);
      /* ..memo clist does not display '/n' */
      if ((copy_max_length = len) > MEMO_CLIST_CHAR_WIDTH) {
	 copy_max_length = MEMO_CLIST_CHAR_WIDTH;
      }
      last = (char *)multibyte_safe_memccpy(str, temp_memo->mmemo.memo.text,'\n', copy_max_length);
      if (last) {
	 *(last-1)='\0';
      } else {
	 str[copy_max_length]='\0';
      }
      gtk_clist_set_text(GTK_CLIST(clist), entries_shown, 0, str);
      gtk_clist_set_row_data(GTK_CLIST(clist), entries_shown, &(temp_memo->mmemo));

      switch (temp_memo->mmemo.rt) {
       case NEW_PC_REC:
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
	 gtk_clist_set_background(GTK_CLIST(clist), entries_shown, NULL);
      }
      entries_shown++;
   }

   jpilot_logf(LOG_DEBUG, "entries_shown=%d\n", entries_shown);

   /*If there is an item in the list, select the first one */
   if (entries_shown>0) {
      gtk_clist_select_row(GTK_CLIST(clist), 0, 0);
      /*cb_clist_selection(clist, 0, 0, (GdkEventButton *)455, ""); */
   }
   
   for (i=row_count-1; i>=entries_shown; i--) {
      gtk_clist_remove(GTK_CLIST(clist), i);
   }

   gtk_clist_thaw(GTK_CLIST(clist));

   sprintf(str, _("%d of %d records"), entries_shown, num_entries);
   gtk_tooltips_set_tip(glob_tooltips, category_menu1, str, NULL);   

   connect_changed_signals(CONNECT_SIGNALS);

   jpilot_logf(LOG_DEBUG, "Leaving update_memo_screen()\n");
}

/*todo combine these 2 functions */
static int make_category_menu(GtkWidget **category_menu,
			      int include_all)
{
   GtkWidget *menu;
   GSList    *group;
   int i;
   int offset;
   GtkWidget **memo_cat_menu_item;

   if (include_all) {
      memo_cat_menu_item = memo_cat_menu_item1;
   } else {
      memo_cat_menu_item = memo_cat_menu_item2;
   }
   
   *category_menu = gtk_option_menu_new();
   
   menu = gtk_menu_new();
   group = NULL;
   
   offset=0;
   if (include_all) {
      memo_cat_menu_item[0] = gtk_radio_menu_item_new_with_label(group, _("All"));
      gtk_signal_connect(GTK_OBJECT(memo_cat_menu_item[0]), "activate",
			 cb_memo_category, GINT_TO_POINTER(CATEGORY_ALL));
      group = gtk_radio_menu_item_group(GTK_RADIO_MENU_ITEM(memo_cat_menu_item[0]));
      gtk_menu_append(GTK_MENU(menu), memo_cat_menu_item[0]);
      gtk_widget_show(memo_cat_menu_item[0]);
      offset=1;
   }
   for (i=0; i<NUM_MEMO_CAT_ITEMS; i++) {
      if (sort_l[i].Pcat[0]) {
	 memo_cat_menu_item[i+offset] = gtk_radio_menu_item_new_with_label(
	    group, sort_l[i].Pcat);
	 if (include_all) {
	    gtk_signal_connect(GTK_OBJECT(memo_cat_menu_item[i+offset]), "activate",
	       cb_memo_category, GINT_TO_POINTER(sort_l[i].cat_num));
	 }
	 group = gtk_radio_menu_item_group(GTK_RADIO_MENU_ITEM(memo_cat_menu_item[i+offset]));
	 gtk_menu_append(GTK_MENU(menu), memo_cat_menu_item[i+offset]);
	 gtk_widget_show(memo_cat_menu_item[i+offset]);
      }
   }

   gtk_option_menu_set_menu(GTK_OPTION_MENU(*category_menu), menu);
   
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
	 gtk_clist_select_row(GTK_CLIST(clist), found_at, 0);
	 if (!gtk_clist_row_is_visible(GTK_CLIST(clist), found_at)) {
	    move_scrolled_window_hack(scrolled_window,
				      (float)found_at/(float)total_count);
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

   update_memo_screen();

   gtk_clist_select_row(GTK_CLIST(clist), line_num, 0);

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
   update_memo_screen();
   if (index<0) {
      jpilot_logf(LOG_WARN, "Category not legal\n");
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
   connect_changed_signals(DISCONNECT_SIGNALS);
   set_pref(PREF_MEMO_PANE, GTK_PANED(pane)->handle_xpos);
   return 0;
}

/* */
/*Main function */
/* */
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
   /*Separator */
   separator = gtk_hseparator_new();
   gtk_box_pack_start(GTK_BOX(vbox1), separator, FALSE, FALSE, 5);

   /*Make the Today is: label */
   glob_date_label = gtk_label_new(" ");
   gtk_box_pack_start(GTK_BOX(vbox1), glob_date_label, FALSE, FALSE, 0);
   timeout_date(NULL);
   glob_date_timer_tag = gtk_timeout_add(CLOCK_TICK, timeout_date, NULL);


   /*Separator */
   separator = gtk_hseparator_new();
   gtk_box_pack_start(GTK_BOX(vbox1), separator, FALSE, FALSE, 5);


   /*Put the left-hand category menu up */
   make_category_menu(&category_menu1, TRUE);   
   gtk_box_pack_start(GTK_BOX(vbox1), category_menu1, FALSE, FALSE, 0);


   /*Put the memo list window up */
   scrolled_window = gtk_scrolled_window_new(NULL, NULL);
   /*gtk_widget_set_usize(GTK_WIDGET(scrolled_window), 330, 100); */
   gtk_container_set_border_width(GTK_CONTAINER(scrolled_window), 0);
   gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
   gtk_box_pack_start(GTK_BOX(vbox1), scrolled_window, TRUE, TRUE, 0);

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
   /*   gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW */
   /*					 (scrolled_window), clist); */
   gtk_container_add(GTK_CONTAINER(scrolled_window), GTK_WIDGET(clist));
   
   /* */
   /* The right hand part of the main window follows: */
   /* */
   hbox_temp = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, FALSE, FALSE, 0);

   
   /*Add record modification buttons on right side */
   button = gtk_button_new_with_label(_("Delete"));
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_delete_memo),
		      GINT_TO_POINTER(DELETE_FLAG));
   gtk_box_pack_start(GTK_BOX(hbox_temp), button, TRUE, TRUE, 0);
   
   button = gtk_button_new_with_label(_("Copy"));
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_add_new_record), 
		      GINT_TO_POINTER(NEW_FLAG));
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
   make_category_menu(&category_menu2, FALSE);
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

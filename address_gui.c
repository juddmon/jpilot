/* address_gui.c
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
#include "config.h"
#include <gtk/gtk.h>
#include <time.h>
#include <string.h>
#include "utils.h"
#include "address.h"
#include "log.h"
#include "prefs.h"

//#define SHADOW GTK_SHADOW_IN
//#define SHADOW GTK_SHADOW_OUT
//#define SHADOW GTK_SHADOW_ETCHED_IN
#define SHADOW GTK_SHADOW_ETCHED_OUT

#define NUM_MENU_ITEM1 8
#define NUM_MENU_ITEM2 8
#define NUM_ADDRESS_CAT_ITEMS 16
#define NUM_ADDRESS_ENTRIES 19

GtkWidget *clist;
GtkWidget *address_text[22];
GtkWidget *vbox2_1, *vbox2_2;
GtkWidget *text;
GtkWidget *vscrollbar;
GtkWidget *phone_list_menu[5];
GtkWidget *menu;
GtkWidget *menu_item[NUM_MENU_ITEM1][NUM_MENU_ITEM2];
GtkWidget *address_cat_menu2;
//We need an extra one for the ALL category
GtkWidget *address_cat_menu_item1[NUM_ADDRESS_CAT_ITEMS+1];
GtkWidget *address_cat_menu_item2[NUM_ADDRESS_CAT_ITEMS];
GtkWidget *category_menu1;
GtkWidget *scrolled_window;
GtkWidget *address_quickfind_entry;

struct AddressAppInfo address_app_info;
int address_category;
int address_phone_label_selected[5];
int clist_row_selected;
extern GtkTooltips *glob_tooltips;

void update_address_screen();

static void init()
{
   int i, j;
   for (i=0; i<NUM_MENU_ITEM1; i++) {
      for (j=0; j<NUM_MENU_ITEM2; j++) {
	 menu_item[i][j] = NULL;
      }
   }
   for (i=0; i<NUM_ADDRESS_CAT_ITEMS; i++) {
      address_cat_menu_item2[NUM_ADDRESS_CAT_ITEMS] = NULL;
   }
}
   
void cb_delete_address(GtkWidget *widget,
		       gpointer   data)
{
   MyAddress *ma;
   int flag;

   ma = gtk_clist_get_row_data(GTK_CLIST(clist), clist_row_selected);
   if (ma < (MyAddress *)CLIST_MIN_DATA) {
      return;
   }
   flag = GPOINTER_TO_INT(data);
   if ((flag==MODIFY_FLAG) || (flag==DELETE_FLAG)) {
      delete_pc_record(ADDRESS, ma, flag);
   }

   update_address_screen();
}

void cb_phone_menu(GtkWidget *item, unsigned int value)
{
   if (!item)
     return;
   if ((GTK_CHECK_MENU_ITEM(item))->active) {
      jpilot_logf(LOG_DEBUG, "phone_menu = %d\n", (value & 0xF0) >> 4);
      jpilot_logf(LOG_DEBUG, "selection = %d\n", value & 0x0F);
      address_phone_label_selected[(value & 0xF0) >> 4] = value & 0x0F;
   }
}

void cb_new_address_done(GtkWidget *widget,
			 gpointer   data)
{
   int i, found;
   struct Address a;
   MyAddress *ma;
   unsigned char attrib;
   
   gtk_widget_hide(vbox2_2);
   gtk_widget_show(vbox2_1);
   if (GPOINTER_TO_INT(data)==CANCEL_FLAG) {
      //Cancel button was hit
      return;
   }
   if ((GPOINTER_TO_INT(data)==NEW_FLAG) || 
       (GPOINTER_TO_INT(data)==MODIFY_FLAG)) {
      //These rec_types are both the same for now
      if (GPOINTER_TO_INT(data)==MODIFY_FLAG) {
	 ma = gtk_clist_get_row_data(GTK_CLIST(clist), clist_row_selected);
	 if (ma < (MyAddress *)CLIST_MIN_DATA) {
	    return;
	 }
	 if ((ma->rt==DELETED_PALM_REC) || (ma->rt==MODIFIED_PALM_REC)) {
	    jpilot_logf(LOG_INFO, "You can't modify a record that is deleted\n");
	    return;
	 }
      }
      found=0;
      a.showPhone=0;
      for (i=0; i<NUM_ADDRESS_ENTRIES; i++) {
	 a.entry[i] = 
	   gtk_editable_get_chars(GTK_EDITABLE(address_text[i]), 0, -1);
	 if (i>2 && i<8 && !found) {
	    if (a.entry[i][0]!='\0') {
	       found=1;
	       a.showPhone=i-3;
	    }
	 }
      }
      for (i=0; i<5; i++) {
	 a.phoneLabel[i]=address_phone_label_selected[i];
      }

      //Get the category that is set from the menu
      attrib = 0;
      for (i=0; i<NUM_ADDRESS_CAT_ITEMS; i++) {
	 if (GTK_IS_WIDGET(address_cat_menu_item2[i])) {
	    if (GTK_CHECK_MENU_ITEM(address_cat_menu_item2[i])->active) {
	       attrib = i;
	       break;
	    }
	 }
      }

      pc_address_write(&a, NEW_PC_REC, attrib);
      free_Address(&a);
      if (GPOINTER_TO_INT(data) == MODIFY_FLAG) {
	 cb_delete_address(NULL, data);
      } else {
	 update_address_screen();
      }
   }
}

void clear_address_entries()
{
   int i;
   
   //Clear all the address entry texts
   for (i=0; i<NUM_ADDRESS_ENTRIES; i++) {
      gtk_text_set_point(GTK_TEXT(address_text[i]), 0);
      gtk_text_forward_delete(GTK_TEXT(address_text[i]),
			      gtk_text_get_length(GTK_TEXT(address_text[i])));
   }
   for (i=0; i<5; i++) {
      if (GTK_IS_WIDGET(menu_item[i][i])) {
	 gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM
					(menu_item[i][i]), TRUE);
	 gtk_option_menu_set_history(GTK_OPTION_MENU(phone_list_menu[i]), i);
      }
   }   
}

void cb_address_clear(GtkWidget *widget,
		      gpointer   data)
{
   clear_address_entries();
}

void cb_new_address_mode(GtkWidget *widget,
			    gpointer   data)
{
   //clear_address_entries();
   gtk_widget_hide(vbox2_1);
   gtk_widget_show(vbox2_2);
}

void cb_address_quickfind(GtkWidget *widget,
			  gpointer   data)
{
   char *entry_text;
   int i, r, found, found_at, line_count;
   char *clist_text;
   
   found_at = 0;
   entry_text = gtk_entry_get_text(GTK_ENTRY(widget));
   if (!strlen(entry_text)) {
      return;
   }
   //100000 is just to prevent ininite looping during a solar flare
   for (found = i = 0; i<100000; i++) {
      r = gtk_clist_get_text(GTK_CLIST(clist), i, 0, &clist_text);
      if (!r) {
	 break;
      }
      if (found) {
	 continue;
      }
      if (!strncasecmp(clist_text, entry_text, strlen(entry_text))) {
	 found = 1;
	 found_at = i;
	 gtk_clist_select_row(GTK_CLIST(clist), i, 0);
      }
   }
   line_count = i;
	 
   if (found) {
      move_scrolled_window(scrolled_window,
			   ((float)found_at)/((float)line_count));
   }
}

void cb_address_category(GtkWidget *item, int selection)
{
   if (!item)
     return;
   if ((GTK_CHECK_MENU_ITEM(item))->active) {
      address_category = selection;
      jpilot_logf(LOG_DEBUG, "address_category = %d\n",address_category);
      update_address_screen();
   }
}


void cb_address_clist_sel(GtkWidget      *clist,
			  gint           row,
			  gint           column,
			  GdkEventButton *event,
			  gpointer       data)
{
   //The rename-able phone entries are indexes 3,4,5,6,7
   struct Address *a;
   MyAddress *ma;
   int i, i2;
   //This is because the palm doesn\'t show the address entries in order
   int order[22]={0,1,13,2,3,4,5,6,7,8,9,10,11,12,14,15,16,17,18,19,20,21
   };
   char *clist_text, *entry_text;

   clist_row_selected=row;

   //text = data;
   ma = gtk_clist_get_row_data(GTK_CLIST(clist), row);
   if (ma==NULL) {
      return;
   }
   a=&(ma->a);
   clist_text = NULL;
   gtk_clist_get_text(GTK_CLIST(clist), row, 0, &clist_text);
   entry_text = gtk_entry_get_text(GTK_ENTRY(address_quickfind_entry));
   if (strncasecmp(clist_text, entry_text, strlen(entry_text))) {
      gtk_entry_set_text(GTK_ENTRY(address_quickfind_entry), "");
   }

   gtk_text_freeze(GTK_TEXT(text));

   gtk_text_set_point(GTK_TEXT(text), 0);
   gtk_text_forward_delete(GTK_TEXT(text),
			   gtk_text_get_length(GTK_TEXT(text)));

   gtk_text_insert(GTK_TEXT(text), NULL,NULL,NULL, "Category: ", -1);
   gtk_text_insert(GTK_TEXT(text), NULL,NULL,NULL,
		   address_app_info.category.name[ma->attrib & 0x0F], -1);
   gtk_text_insert(GTK_TEXT(text), NULL,NULL,NULL, "\n", -1);
   for (i=0; i<NUM_ADDRESS_ENTRIES; i++) {
      i2=order[i];
      if (a->entry[i2]) {
	 if (i2>2 && i2<8) {
	    gtk_text_insert(GTK_TEXT(text), NULL,NULL,NULL,
			    address_app_info.phoneLabels[a->phoneLabel[i2-3]],
			    -1);
	 } else {
	    gtk_text_insert(GTK_TEXT(text), NULL,NULL,NULL, address_app_info.labels[i2], -1);
	 }
	 gtk_text_insert(GTK_TEXT(text), NULL,NULL,NULL, ": ", -1);
	 gtk_text_insert(GTK_TEXT(text), NULL,NULL,NULL, a->entry[i2], -1);
	 gtk_text_insert(GTK_TEXT(text), NULL,NULL,NULL, "\n", -1);
      }
   }
   gtk_text_thaw(GTK_TEXT(text));

   //Update all the new entry text boxes with info
   if (GTK_IS_WIDGET(address_cat_menu_item2[ma->attrib & 0x0F])) {
      gtk_check_menu_item_set_active
	(GTK_CHECK_MENU_ITEM(address_cat_menu_item2[ma->attrib & 0x0F]), TRUE);
      gtk_option_menu_set_history
	(GTK_OPTION_MENU(address_cat_menu2), ma->attrib & 0x0F);
   }

   for (i=0; i<NUM_ADDRESS_ENTRIES; i++) {
      gtk_text_set_point(GTK_TEXT(address_text[i]), 0);
      gtk_text_forward_delete(GTK_TEXT(address_text[i]),
			      gtk_text_get_length(GTK_TEXT(address_text[i])));
      if (a->entry[i]) {
	 gtk_text_insert(GTK_TEXT(address_text[i]), NULL,NULL,NULL, a->entry[i], -1);
      }
   }
   for (i=0; i<5; i++) {
      if (GTK_IS_WIDGET(menu_item[i][a->phoneLabel[i]])) {
	 gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM
					(menu_item[i][a->phoneLabel[i]]), TRUE);
	 gtk_option_menu_set_history(GTK_OPTION_MENU(phone_list_menu[i]),
				     a->phoneLabel[i]);
      }
   }
}

void update_address_screen()
{
   int num_entries, entries_shown, i;
   int show1, show2, show3;
   gchar *empty_line[] = { "","","" };
   GdkPixmap *pixmap_note;
   GdkPixmap *pixmap_alarm;
   GdkPixmap *pixmap_check;
   GdkPixmap *pixmap_checked;
   GdkBitmap *mask_note;
   GdkBitmap *mask_alarm;
   GdkBitmap *mask_check;
   GdkBitmap *mask_checked;
   GdkColor color;
   GdkColormap *colormap;
   AddressList *temp_al;
   static AddressList *address_list=NULL;
   char str[50];
   int ivalue;
   const char *svalue;

   free_AddressList(&address_list);

#ifdef JPILOT_DEBUG
    for (i=0;i<NUM_ADDRESS_CAT_ITEMS;i++) {
      jpilot_logf(LOG_DEBUG, "renamed:[%02d]:\n",address_app_info.category.renamed[i]);
      jpilot_logf(LOG_DEBUG, "category name:[%02d]:",i);
      print_string(address_app_info.category.name[i],16);
      jpilot_logf(LOG_DEBUG, "category ID:%d\n", address_app_info.category.ID[i]);
   }

   for (i=0;i<22;i++) {
      jpilot_logf(LOG_DEBUG, "labels[%02d]:",i);
      print_string(address_app_info.labels[i],16);
   }
   for (i=0;i<8;i++) {
      jpilot_logf(LOG_DEBUG, "phoneLabels[%d]:",i);
      print_string(address_app_info.phoneLabels[i],16);
   }
   jpilot_logf(LOG_DEBUG, "country %d\n",address_app_info.country);
   jpilot_logf(LOG_DEBUG, "sortByCompany %d\n",address_app_info.sortByCompany);
#endif

   num_entries = get_addresses(&address_list);
   gtk_clist_clear(GTK_CLIST(clist));
   //gtk_text_backward_delete(GTK_TEXT(text1),
	//		    gtk_text_get_length(GTK_TEXT(text1)));
   
   if (address_list==NULL) {
      gtk_tooltips_set_tip(glob_tooltips, category_menu1, "0 records", NULL);   
      return;
   }
   
   //Clear the text box to make things look nice
   gtk_text_set_point(GTK_TEXT(text), 0);
   gtk_text_forward_delete(GTK_TEXT(text),
			   gtk_text_get_length(GTK_TEXT(text)));

   gtk_clist_freeze(GTK_CLIST(clist));

   if (address_app_info.sortByCompany) {
      show1=2; //company
      show2=0; //last name
      show3=1; //first name
   } else {
      show1=0; //last name
      show2=1; //first name
      show3=2; //company
   }
   

   entries_shown=0;
   for (temp_al = address_list, i=0; temp_al; temp_al=temp_al->next, i++) {
      if (temp_al->ma.rt == MODIFIED_PALM_REC) {
	 get_pref(PREF_SHOW_MODIFIED, &ivalue, &svalue);
	 //this will be in preferences as to whether you want to
	 //see deleted records, or not.
	 if (!ivalue) {
	    num_entries--;
	    i--;
	    continue;
	 }
      }
      if (temp_al->ma.rt == DELETED_PALM_REC) {
	 get_pref(PREF_SHOW_DELETED, &ivalue, &svalue);
	 //this will be in preferences as to whether you want to
	 //see deleted records, or not.
	 if (!ivalue) {
	    num_entries--;
	    i--;
	    continue;
	 }
      }
      if ( ((temp_al->ma.attrib & 0x0F) != address_category) &&
	  address_category != CATEGORY_ALL) {
	 continue;
      }
      
      entries_shown++;
      str[0]='\0';
      if (temp_al->ma.a.entry[show1] || temp_al->ma.a.entry[show2]) {
	 if (temp_al->ma.a.entry[show1] && temp_al->ma.a.entry[show2]) {
	    g_snprintf(str, 48, "%s, %s", temp_al->ma.a.entry[show1], temp_al->ma.a.entry[show2]);
	 }
	 if (temp_al->ma.a.entry[show1] && ! temp_al->ma.a.entry[show2]) {
	    strncpy(str, temp_al->ma.a.entry[show1], 48);
	 }
	 if (! temp_al->ma.a.entry[show1] && temp_al->ma.a.entry[show2]) {
	    strncpy(str, temp_al->ma.a.entry[show2], 48);
	 }
      } else if (temp_al->ma.a.entry[show3]) {
	    strncpy(str, temp_al->ma.a.entry[show3], 48);
      } else {
	    strcpy(str, "-Unnamed-");
      }
      gtk_clist_prepend(GTK_CLIST(clist), empty_line);
      gtk_clist_set_text(GTK_CLIST(clist), 0, 0, str);
      gtk_clist_set_text(GTK_CLIST(clist), 0, 1, temp_al->ma.a.entry[temp_al->ma.a.showPhone+3]);
      gtk_clist_set_row_data(GTK_CLIST(clist), 0, &(temp_al->ma));

      if (temp_al->ma.rt == NEW_PC_REC) {
	 colormap = gtk_widget_get_colormap(clist);
	 color.red=CLIST_NEW_RED;
	 color.green=CLIST_NEW_GREEN;
	 color.blue=CLIST_NEW_BLUE;
	 gdk_color_alloc(colormap, &color);
	 gtk_clist_set_background(GTK_CLIST(clist), 0, &color);
      }
      if (temp_al->ma.rt == DELETED_PALM_REC) {
	 colormap = gtk_widget_get_colormap(clist);
	 color.red=CLIST_DEL_RED;
	 color.green=CLIST_DEL_GREEN;
	 color.blue=CLIST_DEL_BLUE;
	 gdk_color_alloc(colormap, &color);
	 gtk_clist_set_background(GTK_CLIST(clist), 0, &color);
      }
      if (temp_al->ma.rt == MODIFIED_PALM_REC) {
	 colormap = gtk_widget_get_colormap(clist);
	 color.red=CLIST_MOD_RED;
	 color.green=CLIST_MOD_GREEN;
	 color.blue=CLIST_MOD_BLUE;
	 gdk_color_alloc(colormap, &color);
	 gtk_clist_set_background(GTK_CLIST(clist), 0, &color);
      }
      
      if (temp_al->ma.a.entry[18]) {
	 //Put a note pixmap up
	 get_pixmaps(clist,
		     &pixmap_note, &pixmap_alarm, &pixmap_check, &pixmap_checked,
		     &mask_note, &mask_alarm, &mask_check, &mask_checked
		     );
	 gtk_clist_set_pixmap(GTK_CLIST(clist), 0, 2, pixmap_note, mask_note);
      }
   }
   //gtk_clist_append(GTK_CLIST(clist), empty_line);
   //gtk_clist_set_text(GTK_CLIST(clist), entries_shown, 0, "New");
   //gtk_clist_set_text(GTK_CLIST(clist), entries_shown, 1, "Select to add an entry");
   
   //If there is an item in the list, select the first one
   if (entries_shown>0) {
      gtk_clist_select_row(GTK_CLIST(clist), 0, 1);
      //cb_add_clist_sel(clist, 0, 0, (GdkEventButton *)455, "");
   }

   gtk_clist_thaw(GTK_CLIST(clist));
   
   sprintf(str, "%d of %d records", entries_shown, num_entries);
   gtk_tooltips_set_tip(glob_tooltips, category_menu1, str, NULL);   
}

//default set is which menu item is to be set on by default
//set is which set in the menu_item array to use
static int make_phone_menu(int default_set, unsigned int callback_id, int set)
{
   int i;
   GSList *group;
   
   phone_list_menu[set] = gtk_option_menu_new();
   
   menu = gtk_menu_new();
   group = NULL;
   
   for (i=0; i<8; i++) {
      if (address_app_info.phoneLabels[i][0]) {
	 menu_item[set][i] = gtk_radio_menu_item_new_with_label(
			group, address_app_info.phoneLabels[i]);
	 gtk_signal_connect(GTK_OBJECT(menu_item[set][i]), "activate",
			    cb_phone_menu, GINT_TO_POINTER(callback_id + i));
	 group = gtk_radio_menu_item_group(GTK_RADIO_MENU_ITEM(menu_item[set][i]));
	 gtk_menu_append(GTK_MENU(menu), menu_item[set][i]);
	 gtk_widget_show(menu_item[set][i]);
      }
   }
   //Set this one to active
   if (GTK_IS_WIDGET(menu_item[set][default_set])) {
      gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
				     menu_item[set][default_set]), TRUE);
   }
   
   gtk_option_menu_set_menu(GTK_OPTION_MENU(phone_list_menu[set]), menu);
   //Make this one show up by default
   gtk_option_menu_set_history(GTK_OPTION_MENU(phone_list_menu[set]),
			       default_set);
   
   gtk_widget_show(phone_list_menu[set]);
   
   return 0;
}

//todo - I should combine these next 2 functions into 1
static int make_category_menu1(GtkWidget **category_menu)
{
   GtkWidget *menu;
   GSList    *group;
   int i;

   *category_menu = gtk_option_menu_new();
   
   menu = gtk_menu_new();
   group = NULL;
   
   address_cat_menu_item1[0] = gtk_radio_menu_item_new_with_label(group, "All");
   gtk_signal_connect(GTK_OBJECT(address_cat_menu_item1[0]), "activate",
		      cb_address_category, GINT_TO_POINTER(CATEGORY_ALL));
   group = gtk_radio_menu_item_group(GTK_RADIO_MENU_ITEM(address_cat_menu_item1[0]));
   gtk_menu_append(GTK_MENU(menu), address_cat_menu_item1[0]);
   gtk_widget_show(address_cat_menu_item1[0]);
   for (i=0; i<NUM_ADDRESS_CAT_ITEMS; i++) {
      if (address_app_info.category.name[i][0]) {
	 address_cat_menu_item1[i+1] = gtk_radio_menu_item_new_with_label(
		     group, address_app_info.category.name[i]);
	 gtk_signal_connect(GTK_OBJECT(address_cat_menu_item1[i+1]), "activate",
			    cb_address_category, GINT_TO_POINTER(i));
	 group = gtk_radio_menu_item_group(GTK_RADIO_MENU_ITEM(address_cat_menu_item1[i+1]));
	 gtk_menu_append(GTK_MENU(menu), address_cat_menu_item1[i+1]);
	 gtk_widget_show(address_cat_menu_item1[i+1]);
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

   address_cat_menu2 = gtk_option_menu_new();
   
   menu = gtk_menu_new();
   group = NULL;
   
   for (i=0; i<NUM_ADDRESS_CAT_ITEMS; i++) {
      if (address_app_info.category.name[i][0]) {
	 address_cat_menu_item2[i] = gtk_radio_menu_item_new_with_label
	   (group, address_app_info.category.name[i]);
	 group = gtk_radio_menu_item_group
	   (GTK_RADIO_MENU_ITEM(address_cat_menu_item2[i]));
	 gtk_menu_append(GTK_MENU(menu), address_cat_menu_item2[i]);
	 gtk_widget_show(address_cat_menu_item2[i]);
      }
   }
   gtk_option_menu_set_menu(GTK_OPTION_MENU(address_cat_menu2), menu);
   
   return 0;
}

static int address_find()
{
   int r, found_at, total_count;
   
   if (glob_find_id) {
      r = clist_find_id(clist,
			glob_find_id,
			&found_at,
			&total_count);
      if (r) {
	 //avoid dividing by zero
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

int address_refresh()
{
   address_category = CATEGORY_ALL;
   update_address_screen();
   gtk_option_menu_set_history(GTK_OPTION_MENU(category_menu1), 0);
   gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
       address_cat_menu_item1[0]), TRUE);
   address_find();
   return 0;
}

//
//Main function
//
int address_gui(GtkWidget *vbox, GtkWidget *hbox)
{
   extern GtkWidget *glob_date_label;
   extern glob_date_timer_tag;
   GtkWidget *vbox1, *hbox_temp;
   GtkWidget *vbox_temp1, *vbox_temp2, *vbox_temp3;
   GtkWidget *vbox2_1_sub1;
   GtkWidget *separator;
   GtkWidget *label;
   GtkWidget *button;
   GtkWidget *frame;
//   GtkWidget *clist;
   GtkWidget *notebook;
   GtkWidget *table1, *table2, *table3;
//   GtkWidget *calendar;
   GtkWidget *notebook_tab;
   

   int i, i2;
   int order[22]={0,1,13,2,3,4,5,6,7,8,9,10,11,12,14,15,16,17,18,19,20,21
   };

   clist_row_selected=0;
   
   init();

   if (get_address_app_info(&address_app_info)) {
      for (i=0; i<NUM_ADDRESS_CAT_ITEMS; i++) {
	 address_app_info.category.name[i][0]='\0';
      }
   }

   vbox1 = gtk_vbox_new (FALSE, 0);
   vbox2_1 = gtk_vbox_new (FALSE, 0);
   vbox2_2 = gtk_vbox_new (FALSE, 0);
   gtk_box_pack_start(GTK_BOX(hbox), vbox1, TRUE, TRUE, 5);
   gtk_box_pack_start(GTK_BOX(hbox), vbox2_1, TRUE, TRUE, 5);
   gtk_box_pack_start(GTK_BOX(hbox), vbox2_2, TRUE, TRUE, 5);

   //Add buttons in left vbox
   button = gtk_button_new_with_label("Delete");
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_delete_address),
		      GINT_TO_POINTER(DELETE_FLAG));
   gtk_box_pack_start(GTK_BOX(vbox), button, TRUE, TRUE, 0);
   gtk_widget_show(button);
   
   gtk_widget_set_usize(GTK_WIDGET(vbox1), 210, 0);
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

   //Put the category menu up
   make_category_menu1(&category_menu1);
   gtk_box_pack_start(GTK_BOX(vbox1), category_menu1, FALSE, FALSE, 0);
   gtk_widget_show(category_menu1);
   
   //Put the address list window up
   scrolled_window = gtk_scrolled_window_new(NULL, NULL);
   //gtk_widget_set_usize(GTK_WIDGET(scrolled_window), 150, 0);
   gtk_container_set_border_width(GTK_CONTAINER(scrolled_window), 0);
   gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
   gtk_box_pack_start(GTK_BOX(vbox1), scrolled_window, TRUE, TRUE, 0);
   gtk_widget_show(scrolled_window);

   clist = gtk_clist_new(3);
   gtk_signal_connect(GTK_OBJECT(clist), "select_row",
		      GTK_SIGNAL_FUNC(cb_address_clist_sel),
		      text);
   gtk_clist_set_shadow_type(GTK_CLIST(clist), SHADOW);
   gtk_clist_set_selection_mode(GTK_CLIST(clist), GTK_SELECTION_BROWSE);
   gtk_clist_set_column_width(GTK_CLIST(clist), 0, 140);
   gtk_clist_set_column_width(GTK_CLIST(clist), 1, 140);
   gtk_clist_set_column_width(GTK_CLIST(clist), 2, 16);
   // gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW
   //					   (scrolled_window), clist);
   gtk_container_add(GTK_CONTAINER(scrolled_window), GTK_WIDGET(clist));
   //gtk_viewport_set_shadow_type(GTK_VIEWPORT(scrolled_window), GTK_SHADOW_NONE);
   //gtk_clist_set_sort_column (GTK_CLIST(clist1), 0);
   //gtk_clist_set_auto_sort(GTK_CLIST(clist1), TRUE);
   
   hbox_temp = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox1), hbox_temp, FALSE, FALSE, 0);
   gtk_widget_show(hbox_temp);

   label = gtk_label_new("Quick Find");
   gtk_box_pack_start(GTK_BOX(hbox_temp), label, FALSE, FALSE, 0);
   gtk_widget_show(label);

   address_quickfind_entry = gtk_entry_new();
   gtk_signal_connect(GTK_OBJECT(address_quickfind_entry), "changed",
		      GTK_SIGNAL_FUNC(cb_address_quickfind),
		      NULL);
   gtk_box_pack_start(GTK_BOX(hbox_temp), address_quickfind_entry, TRUE, TRUE, 0);
   gtk_widget_show(address_quickfind_entry);
   
   //calendar = gtk_calendar_new();
   //gtk_box_pack_start(GTK_BOX(vbox2), calendar, TRUE, TRUE, 0);
   //gtk_widget_show(calendar);
   
   //gtk_box_pack_start (GTK_BOX (vbox_detail), hbox_text1, TRUE, TRUE, 0);

   //The Frame on the right side
   frame = gtk_frame_new("Quick View");
   gtk_frame_set_label_align(GTK_FRAME(frame), 0.5, 0.0);
   gtk_box_pack_start(GTK_BOX(vbox2_1), frame, TRUE, TRUE, 0);
   vbox2_1_sub1 = gtk_vbox_new(FALSE, 0);
   gtk_container_set_border_width(GTK_CONTAINER(vbox2_1_sub1), 5);
   gtk_container_add(GTK_CONTAINER(frame), vbox2_1_sub1);
   gtk_widget_show(frame);
   gtk_widget_show(vbox2_1_sub1);


   //The Right hand side of the screen
   button = gtk_button_new_with_label("Add or Modify a Record");
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_new_address_mode), NULL);
   gtk_box_pack_start(GTK_BOX(vbox2_1_sub1), button, FALSE, FALSE, 0);
   gtk_widget_show(button);

   //Separator
   separator = gtk_hseparator_new();
   gtk_box_pack_start(GTK_BOX(vbox2_1_sub1), separator, FALSE, FALSE, 5);
   gtk_widget_show(separator);


   //The text box on the right side
   hbox_temp = gtk_hbox_new (FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox2_1_sub1), hbox_temp, TRUE, TRUE, 0);

   text = gtk_text_new(NULL, NULL);
   gtk_text_set_editable(GTK_TEXT(text), FALSE);
   gtk_text_set_word_wrap(GTK_TEXT(text), TRUE);
   vscrollbar = gtk_vscrollbar_new(GTK_TEXT(text)->vadj);
   gtk_box_pack_start(GTK_BOX(hbox_temp), text, TRUE, TRUE, 0);
   gtk_box_pack_start(GTK_BOX(hbox_temp), vscrollbar, FALSE, FALSE, 0);
   //gtk_widget_set_usize (GTK_WIDGET(text), 100, 50);
   gtk_widget_show(text);
   gtk_widget_show(vscrollbar);   
   gtk_widget_show(hbox_temp);

   
   //The new entry gui
   hbox_temp = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox2_2), hbox_temp, FALSE, FALSE, 0);
   gtk_widget_show(hbox_temp);

   button = gtk_button_new_with_label("Add It");
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_new_address_done),
		      GINT_TO_POINTER(NEW_FLAG));
   gtk_box_pack_start(GTK_BOX(hbox_temp), button, TRUE, TRUE, 0);
   gtk_widget_show(button);

   button = gtk_button_new_with_label("Apply Changes");
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_new_address_done),
		      GINT_TO_POINTER(MODIFY_FLAG));
   gtk_box_pack_start(GTK_BOX(hbox_temp), button, TRUE, TRUE, 0);
   gtk_widget_show(button);

   button = gtk_button_new_with_label("Cancel");
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_new_address_done), 
		      GINT_TO_POINTER(CANCEL_FLAG));
   gtk_box_pack_end(GTK_BOX(hbox_temp), button, TRUE, TRUE, 0);
   gtk_widget_show(button);


   button = gtk_button_new_with_label("Clear");
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_address_clear), NULL);
   gtk_box_pack_end(GTK_BOX(hbox_temp), button, TRUE, TRUE, 0);
   gtk_widget_show(button);
   

   //Separator
   separator = gtk_hseparator_new();
   gtk_box_pack_start(GTK_BOX(vbox2_2), separator, FALSE, FALSE, 5);
   gtk_widget_show(separator);

   
   //Add the new category menu
   make_category_menu2();
   gtk_box_pack_start(GTK_BOX(vbox2_2), address_cat_menu2, FALSE, FALSE, 0);
   gtk_widget_show(address_cat_menu2);
   
   
   //Add the notebook for new entries
   notebook = gtk_notebook_new();
   gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_TOP);
   gtk_box_pack_start(GTK_BOX(vbox2_2), notebook, TRUE, TRUE, 0);
   gtk_widget_show(notebook);

   //Page 1
   notebook_tab = gtk_label_new("Name");
   vbox_temp1 = gtk_vbox_new(FALSE, 0);
   gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox_temp1, notebook_tab);
   gtk_widget_show(vbox_temp1);
   gtk_widget_show(notebook_tab);
   
   //Page 2
   notebook_tab = gtk_label_new("Address");
   vbox_temp2 = gtk_vbox_new(FALSE, 0);
   gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox_temp2, notebook_tab);
   gtk_widget_show(vbox_temp2);
   gtk_widget_show(notebook_tab);

   
   //Page 3
   notebook_tab = gtk_label_new("Other");
   vbox_temp3 = gtk_vbox_new(FALSE, 0);
   gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox_temp3, notebook_tab);
   gtk_widget_show(vbox_temp3);
   gtk_widget_show(notebook_tab);

   //Put a table on every page
   table1 = gtk_table_new(9, 2, FALSE);
   gtk_box_pack_start(GTK_BOX(vbox_temp1), table1, TRUE, TRUE, 0);
   table2 = gtk_table_new(9, 2, FALSE);
   gtk_box_pack_start(GTK_BOX(vbox_temp2), table2, TRUE, TRUE, 0);
   table3 = gtk_table_new(9, 2, FALSE);
   gtk_box_pack_start(GTK_BOX(vbox_temp3), table3, TRUE, TRUE, 0);

   label = NULL;
   for (i=0; i<NUM_ADDRESS_ENTRIES; i++) {
      i2=order[i];
      if (i2>2 && i2<8) {
	 make_phone_menu(i2-3, (i2-3)<<4, i2-3);
      } else {
 	 label = gtk_label_new(address_app_info.labels[i2]);
	 //gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
	 gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
      }
      address_text[i2] = gtk_text_new(NULL, NULL);
      gtk_text_set_editable(GTK_TEXT(address_text[i2]), TRUE);
      gtk_text_set_word_wrap(GTK_TEXT(address_text[i2]), TRUE);
      gtk_widget_set_usize(GTK_WIDGET(address_text[i2]), 0, 25);
      //gtk_box_pack_start(GTK_BOX(hbox_temp), address_text[i2], TRUE, TRUE, 0);
      //hbox_temp = gtk_hbox_new(FALSE, 0);
      if (i<9) {
	 if (i2>2 && i2<8) {
	    gtk_table_attach_defaults(GTK_TABLE(table1), GTK_WIDGET(phone_list_menu[i2-3]),
				      0, 1, i, i+1);
	 } else {
	    gtk_table_attach_defaults(GTK_TABLE(table1), GTK_WIDGET(label),
				      0, 1, i, i+1);
	 }
	 gtk_table_attach_defaults(GTK_TABLE(table1), GTK_WIDGET(address_text[i2]),
				   1, 2, i, i+1);
	 //gtk_box_pack_start(GTK_BOX(vbox_temp1), hbox_temp, TRUE, TRUE, 0);
      }
      if (i>8 && i<14) {
	 gtk_table_attach_defaults(GTK_TABLE(table2), GTK_WIDGET(label),
				   0, 1, i-9, i-8);
	 gtk_table_attach_defaults(GTK_TABLE(table2), GTK_WIDGET(address_text[i2]),
				   1, 2, i-9, i-8);
	 //gtk_box_pack_start(GTK_BOX(vbox_temp2), hbox_temp, TRUE, TRUE, 0);
      }
      if (i>13 && i<100) {
	 gtk_table_attach_defaults(GTK_TABLE(table3), GTK_WIDGET(label),
				   0, 1, i-14, i-13);
	 gtk_table_attach_defaults(GTK_TABLE(table3), GTK_WIDGET(address_text[i2]),
				   1, 2, i-14, i-13);
	 //gtk_box_pack_start(GTK_BOX(vbox_temp3), hbox_temp, TRUE, TRUE, 0);
      }
      //gtk_widget_show(hbox_temp);
      //gtk_box_pack_start(GTK_BOX(hbox_temp), label, FALSE, FALSE, 0);
      gtk_widget_show(label);

      gtk_widget_show(address_text[i2]);
   }
   gtk_widget_set_usize(GTK_WIDGET(table1), 200, 0);
   gtk_widget_set_usize(GTK_WIDGET(table2), 200, 0);
   gtk_widget_set_usize(GTK_WIDGET(table3), 200, 0);
   gtk_widget_show(table1);
   gtk_widget_show(table2);
   gtk_widget_show(table3); 
  
   
   gtk_widget_show(clist);

   gtk_widget_show(vbox1);
   gtk_widget_show(vbox2_1);
   gtk_widget_show(vbox2_1_sub1);
   gtk_widget_show(vbox);
   gtk_widget_show(hbox);

//   address_category = CATEGORY_ALL;
//   update_address_screen();

//   address_find();
   
   address_refresh();

   return 0;
}

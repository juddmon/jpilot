/*
 * address_gui.c
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
#include "utils.h"
#include "address.h"

//#define SHADOW GTK_SHADOW_IN
//#define SHADOW GTK_SHADOW_OUT
//#define SHADOW GTK_SHADOW_ETCHED_IN
#define SHADOW GTK_SHADOW_ETCHED_OUT

GtkWidget *clist;
GtkWidget *address_text[22];
GtkWidget *vbox2_1, *vbox2_2;
GtkWidget *text;
GtkWidget *vscrollbar;
GtkWidget *menu_item[8][8];
struct AddressAppInfo address_app_info;
int address_category;
int address_phone_label_selected[5];
int clist_row_selected;

void update_address_screen();

void cb_delete_address(GtkWidget *widget,
		       gpointer   data)
{
   MyAddress *ma;
   
   ma = gtk_clist_get_row_data(GTK_CLIST(clist), clist_row_selected);
   if (ma < (MyAddress *)CLIST_MIN_DATA) {
      return;
   }
   //printf("ma->unique_id = %d\n",ma->unique_id);
   //printf("ma->rt = %d\n",ma->rt);
   delete_pc_record(ADDRESS, ma);

   update_address_screen();
}

void cb_phone_menu(GtkWidget *item, unsigned int value)
{
   if ((GTK_CHECK_MENU_ITEM(item))->active) {
      //printf("phone_menu = %d\n", (value & 0xF0) >> 4);
      //printf("selection = %d\n", value & 0x0F);
      address_phone_label_selected[(value & 0xF0) >> 4] = value & 0x0F;
   }
}

void cb_new_address_done(GtkWidget *widget,
			 gpointer   data)
{
   int i, found;
   struct Address a;
   unsigned char attrib;
   
   gtk_widget_hide(vbox2_2);
   gtk_widget_show(vbox2_1);
   if (data==0) {
      //Cancel button was hit
      return;
   }
   found=0;
   a.showPhone=0;
   for (i=0; i<19; i++) {
      a.entry[i] = 
	gtk_editable_get_chars(GTK_EDITABLE(address_text[i]), 0, -1);
      //printf("a.entry[%d] = [%s]\n", i, a.entry[i]);
      if (i>2 && i<8 && !found) {
	 if (a.entry[i][0]!='\0') {
	    found=1;
	    a.showPhone=i-3;
	 }
      }
   }
   //printf("showPhone = %d\n", a.showPhone);
   for (i=0; i<5; i++) {
      //printf("phone label selected[%d]=%d\n",i,address_phone_label_selected[i]);
      a.phoneLabel[i]=address_phone_label_selected[i];
   }

   if (address_category == CATEGORY_ALL) {
      attrib = 0;
   } else {
      attrib = address_category;
   }
   //printf("attrib = %d\n", attrib);

   pc_address_write(&a, NEW_PC_REC, attrib);
   free_Address(&a);
   update_address_screen();
}

void cb_new_address(GtkWidget *widget,
		    gpointer   data)
{
   int i;
   
   gtk_widget_hide(vbox2_1);
   gtk_widget_show(vbox2_2);

   //Clear all the address entry texts
   for (i=0; i<19; i++) {
      gtk_text_set_point(GTK_TEXT(address_text[i]), 0);
      gtk_text_forward_delete(GTK_TEXT(address_text[i]),
			      gtk_text_get_length(GTK_TEXT(address_text[i])));
   }

}


void cb_add_category(GtkWidget *item, int selection)
{
   if ((GTK_CHECK_MENU_ITEM(item))->active) {
      address_category = selection;
      //printf("address_category = %d\n",address_category);
      update_address_screen();
   }
}


void cb_add_clist_sel(GtkWidget      *clist,
			 gint           row,
			 gint           column,
			 GdkEventButton *event,
			 gpointer       data)
{
   //The rename-able phone entries are indexes 3,4,5,6,7
   struct Address *a, new_a;
   MyAddress *ma;
   int i, i2;
   //This is because the palm doesn\'t show the address entries in order
   int order[22]={0,1,13,2,3,4,5,6,7,8,9,10,11,12,14,15,16,17,18,19,20,21
   };
   char str[12];
   //GtkWidget *text;

   clist_row_selected=row;

   //text = data;
   ma = gtk_clist_get_row_data(GTK_CLIST(clist), row);
   if (ma==NULL) {
      return;
   }
   a=&(ma->a);
   
   gtk_text_freeze(GTK_TEXT(text));

   gtk_text_set_point(GTK_TEXT(text), 0);
   gtk_text_forward_delete(GTK_TEXT(text),
			   gtk_text_get_length(GTK_TEXT(text)));

   gtk_text_insert(GTK_TEXT(text), NULL,NULL,NULL, "Category: ", -1);
   gtk_text_insert(GTK_TEXT(text), NULL,NULL,NULL,
		   address_app_info.category.name[ma->attrib & 0x0F], -1);
   gtk_text_insert(GTK_TEXT(text), NULL,NULL,NULL, "\n", -1);
   for (i=0; i<19; i++) {
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

   //todo - maybe only make this happen in new mode?
   //Update all the new entry text boxes with info
   for (i=0; i<19; i++) {
      gtk_text_set_point(GTK_TEXT(address_text[i]), 0);
      gtk_text_forward_delete(GTK_TEXT(address_text[i]),
			      gtk_text_get_length(GTK_TEXT(address_text[i])));
      if (a->entry[i]) {
	 gtk_text_insert(GTK_TEXT(address_text[i]), NULL,NULL,NULL, a->entry[i], -1);
      }
   }
   for (i=0; i<5; i++) {
      gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
				     menu_item[i][a->phoneLabel[i]+1]), TRUE);
   }
}

void update_address_screen()
{
   int num_entries, entries_shown, i;
   int show1, show2, show3;
   gchar *empty_line[] = { "","","" };
   char a_time[16];
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

   free_AddressList(&address_list);

#ifdef JPILOT_DEBUG
    for (i=0;i<16;i++) {
      printf("renamed:[%02d]:\n",address_app_info.category.renamed[i]);
      printf("category name:[%02d]:",i);
      print_string(address_app_info.category.name[i],16);
      printf("category ID:%d\n", address_app_info.category.ID[i]);
   }

   for (i=0;i<22;i++) {
      printf("labels[%02d]:",i);
      print_string(address_app_info.labels[i],16);
   }
   for (i=0;i<8;i++) {
      printf("phoneLabels[%d]:",i);
      print_string(address_app_info.phoneLabels[i],16);
   }
   printf("country %d\n",address_app_info.country);
   printf("sortByCompany %d\n",address_app_info.sortByCompany);
#endif

   num_entries = get_addresses(&address_list);
   gtk_clist_clear(GTK_CLIST(clist));
   //gtk_text_backward_delete(GTK_TEXT(text1),
	//		    gtk_text_get_length(GTK_TEXT(text1)));
   
   if (address_list==NULL) {
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
      if ( ((temp_al->ma.attrib & 0x0F) != address_category) &&
	  address_category != CATEGORY_ALL) {
	 continue;
      }

      entries_shown++;
      str[0]='\0';
      if (temp_al->ma.a.entry[show1] || temp_al->ma.a.entry[show2]) {
	 if (temp_al->ma.a.entry[show1] && temp_al->ma.a.entry[show2]) {
	    snprintf(str, 48, "%s, %s", temp_al->ma.a.entry[show1], temp_al->ma.a.entry[show2]);
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
}

//default set is which one is to be set on by default
//set is which set in the array to use
int make_phone_menu(GtkWidget **phone_list_menu, int default_set,
		    unsigned int callback_id, int set)
{
   int i, i2;
   GtkWidget *menu;
   GSList *group;
   
   *phone_list_menu = gtk_option_menu_new();
   
   menu = gtk_menu_new();
   group = NULL;
   
   //printf("default_set=%d\n",default_set);
   //printf("callback_id=%d\n",callback_id);
   //gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_item), TRUE);
   //gtk_check_menu_item_select(GTK_CHECK_MENU_ITEM(menu_item));
   //gtk_check_menu_item_activate(GTK_CHECK_MENU_ITEM(menu_item));
   for (i=0; i<8; i++) {
      if (i==0) {
	 i2=default_set;
      } else {
	 i2=i-1;
      }
      if (address_app_info.phoneLabels[i2][0]) {
	 menu_item[set][i] = gtk_radio_menu_item_new_with_label(
			group, address_app_info.phoneLabels[i2]);
	 gtk_signal_connect(GTK_OBJECT(menu_item[set][i]), "activate",
			    cb_phone_menu, GINT_TO_POINTER(callback_id + i2));
	 if (i2==default_set) {
	    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
					   menu_item[set][i]), TRUE);
	 }
	 group = gtk_radio_menu_item_group(GTK_RADIO_MENU_ITEM(menu_item[set][i]));
	 gtk_menu_append(GTK_MENU(menu), menu_item[set][i]);
	 gtk_widget_show(menu_item[set][i]);
      }
   }
   
   gtk_option_menu_set_menu(GTK_OPTION_MENU(*phone_list_menu), menu);
   //gtk_option_menu_set_history (GTK_OPTION_MENU (category_menu), history);
   //gtk_widget_set_usize(GTK_WIDGET(*phone_list_menu), 20, 0);
   
   gtk_widget_show(*phone_list_menu);
}
   
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
   GtkWidget *scrolled_window;
   GtkWidget *category_menu;
   GtkWidget *menu_item;
   GtkWidget *menu;
   GSList    *group;
   GtkWidget *phone_list_menu;
   GtkWidget *notebook_tab;
   
   time_t ltime;
   struct tm *now;
#define MAX_STR 100
   char str[MAX_STR];
   int i, i2;
   int order[22]={0,1,13,2,3,4,5,6,7,8,9,10,11,12,14,15,16,17,18,19,20,21
   };

   clist_row_selected=0;

   get_address_app_info(&address_app_info);

   vbox1 = gtk_vbox_new (FALSE, 0);
   vbox2_1 = gtk_vbox_new (FALSE, 0);
   vbox2_2 = gtk_vbox_new (FALSE, 0);
   gtk_box_pack_start(GTK_BOX(hbox), vbox1, TRUE, TRUE, 5);
   gtk_box_pack_start(GTK_BOX(hbox), vbox2_1, TRUE, TRUE, 5);
   gtk_box_pack_start(GTK_BOX(hbox), vbox2_2, TRUE, TRUE, 5);

   //Add buttons in left vbox
   button = gtk_button_new_with_label("Delete");
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_delete_address), NULL);
   gtk_box_pack_start(GTK_BOX(vbox), button, TRUE, TRUE, 0);
   gtk_widget_show(button);
   
   //Add buttons in left vbox
   button = gtk_button_new_with_label("New");
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_new_address), NULL);
   gtk_box_pack_start(GTK_BOX(vbox), button, TRUE, TRUE, 0);
   gtk_widget_show(button);
   
   //Separator
   separator = gtk_hseparator_new();
   gtk_box_pack_start(GTK_BOX(vbox1), separator, FALSE, FALSE, 5);
   gtk_widget_show(separator);

   //Make the Today is: label
   time(&ltime);
   now = localtime(&ltime);
   strftime(str, MAX_STR, "%A, %x %X", now);
   glob_date_label = gtk_label_new(str);
   gtk_box_pack_start(GTK_BOX(vbox1), glob_date_label, FALSE, FALSE, 0);
   gtk_widget_show(glob_date_label);
   glob_date_timer_tag = gtk_timeout_add(1000, timeout_date, NULL);

   //Separator
   separator = gtk_hseparator_new();
   gtk_box_pack_start(GTK_BOX(vbox1), separator, FALSE, FALSE, 5);
   gtk_widget_show(separator);

   //Put the category menu up
   category_menu = gtk_option_menu_new();
   
   menu = gtk_menu_new();
   group = NULL;
   
   //gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_item), TRUE);
   //gtk_check_menu_item_select(GTK_CHECK_MENU_ITEM(menu_item));
   //gtk_check_menu_item_activate(GTK_CHECK_MENU_ITEM(menu_item));
   menu_item = gtk_radio_menu_item_new_with_label(group, "All");
   gtk_signal_connect(GTK_OBJECT(menu_item), "activate",
		      cb_add_category, GINT_TO_POINTER(CATEGORY_ALL));
   group = gtk_radio_menu_item_group(GTK_RADIO_MENU_ITEM(menu_item));
   gtk_menu_append(GTK_MENU(menu), menu_item);
   gtk_widget_show(menu_item);
   for (i=0; i<16; i++) {
      if (address_app_info.category.name[i][0]) {
	 menu_item = gtk_radio_menu_item_new_with_label(
		     group, address_app_info.category.name[i]);
	 gtk_signal_connect(GTK_OBJECT(menu_item), "activate",
			    cb_add_category, GINT_TO_POINTER(i));
	 group = gtk_radio_menu_item_group(GTK_RADIO_MENU_ITEM(menu_item));
	 gtk_menu_append(GTK_MENU(menu), menu_item);
	 gtk_widget_show(menu_item);
      }
   }

   gtk_option_menu_set_menu(GTK_OPTION_MENU(category_menu), menu);
   //gtk_option_menu_set_history (GTK_OPTION_MENU (category_menu), history);
   
   gtk_box_pack_start (GTK_BOX (vbox1), category_menu, FALSE, FALSE, 0);
   gtk_widget_show (category_menu);
   
   
   //Put the address list window up
   scrolled_window = gtk_scrolled_window_new(NULL, NULL);
   gtk_widget_set_usize(GTK_WIDGET(scrolled_window), 234, 0);
   gtk_container_set_border_width(GTK_CONTAINER(scrolled_window), 0);
   gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
   gtk_box_pack_start(GTK_BOX(vbox1), scrolled_window, TRUE, TRUE, 0);
   gtk_widget_show(scrolled_window);

   clist = gtk_clist_new(3);
   gtk_signal_connect(GTK_OBJECT(clist), "select_row",
		      GTK_SIGNAL_FUNC(cb_add_clist_sel),
		      text);
   gtk_clist_set_shadow_type(GTK_CLIST(clist), SHADOW);
   gtk_clist_set_selection_mode(GTK_CLIST(clist), GTK_SELECTION_BROWSE);
   gtk_clist_set_column_width(GTK_CLIST(clist), 0, 140);
   gtk_clist_set_column_width(GTK_CLIST(clist), 1, 140);
   gtk_clist_set_column_width(GTK_CLIST(clist), 2, 16);
   gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW
					 (scrolled_window), clist);
   //gtk_clist_set_sort_column (GTK_CLIST(clist1), 0);
   //gtk_clist_set_auto_sort(GTK_CLIST(clist1), TRUE);
   
   
   
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

   button = gtk_button_new_with_label("Add it");
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_new_address_done), GINT_TO_POINTER(1));
   gtk_box_pack_start(GTK_BOX(hbox_temp), button, TRUE, TRUE, 0);
   gtk_widget_show(button);

   button = gtk_button_new_with_label("Cancel");
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_new_address_done), GINT_TO_POINTER(0));
   gtk_box_pack_end(GTK_BOX(hbox_temp), button, TRUE, TRUE, 0);
   gtk_widget_show(button);

   
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

   for (i=0; i<19; i++) {
      i2=order[i];
      if (i2>2 && i2<8) {
	 make_phone_menu(&phone_list_menu, i2-3, (i2-3)<<4, i2-3);
      } else {
	 label = gtk_label_new(address_app_info.labels[i2]);
	 //gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
      }
      address_text[i2] = gtk_text_new(NULL, NULL);
      gtk_text_set_editable(GTK_TEXT(address_text[i2]), TRUE);
      gtk_text_set_word_wrap(GTK_TEXT(address_text[i2]), TRUE);
      gtk_widget_set_usize(GTK_WIDGET(address_text[i2]), 0, 25);
      //gtk_box_pack_start(GTK_BOX(hbox_temp), address_text[i2], TRUE, TRUE, 0);
      //hbox_temp = gtk_hbox_new(FALSE, 0);
      if (i<9) {
	 if (i2>2 && i2<8) {
	    gtk_table_attach_defaults(GTK_TABLE(table1), GTK_WIDGET(phone_list_menu),
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

   address_category = CATEGORY_ALL;
   update_address_screen();

}

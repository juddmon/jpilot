/*
 * todo_gui.c
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
#include "todo.h"

//todo - put this in utils.h
#define SHADOW GTK_SHADOW_ETCHED_OUT

GtkWidget *clist;
GtkWidget *todo_text, *todo_text_note;
GtkWidget *todo_cat_label;
GtkWidget *todo_completed_checkbox;
GtkWidget *todo_no_due_date_checkbox;
GtkWidget *radio_button_todo[5];
GtkWidget *todo_spinner_due_mon;
GtkWidget *todo_spinner_due_day;
GtkWidget *todo_spinner_due_year;
GtkAdjustment *todo_adj_due_mon, *todo_adj_due_day, *todo_adj_due_year;

struct ToDoAppInfo todo_app_info;
int todo_category;
int clist_row_selected;

void update_todo_screen();
int todo_clear_details();

void cb_delete_todo(GtkWidget *widget,
		       gpointer   data)
{
   MyToDo *mtodo;
   
   mtodo = gtk_clist_get_row_data(GTK_CLIST(clist), clist_row_selected);
   if (mtodo < (MyToDo *)CLIST_MIN_DATA) {
      return;
   }
   //printf("ma->unique_id = %d\n",ma->unique_id);
   //printf("ma->rt = %d\n",ma->rt);
   delete_pc_record(TODO, mtodo);

   update_todo_screen();
}

void cb_todo_category(GtkWidget *item, int selection)
{
   if ((GTK_CHECK_MENU_ITEM(item))->active) {
      todo_category = selection;
      //printf("address_category = %d\n",address_category);
      todo_clear_details();
      update_todo_screen();
   }
}

int todo_clear_details()
{
   time_t ltime;
   struct tm *now;
   
   time( &ltime );
   now = localtime( &ltime );

   gtk_text_freeze(GTK_TEXT(todo_text));
   gtk_text_freeze(GTK_TEXT(todo_text_note));

   gtk_text_set_point(GTK_TEXT(todo_text), 0);
   gtk_text_forward_delete(GTK_TEXT(todo_text),
			   gtk_text_get_length(GTK_TEXT(todo_text)));

   gtk_text_set_point(GTK_TEXT(todo_text_note), 0);
   gtk_text_forward_delete(GTK_TEXT(todo_text_note),
			   gtk_text_get_length(GTK_TEXT(todo_text_note)));

   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_button_todo[0]), TRUE);

   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(todo_completed_checkbox), FALSE);

   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(todo_no_due_date_checkbox),
				TRUE);

   gtk_adjustment_set_value(GTK_ADJUSTMENT(todo_adj_due_mon),
			    now->tm_mon+1);
   gtk_adjustment_set_value(GTK_ADJUSTMENT(todo_adj_due_day),
			    now->tm_mday);
   gtk_adjustment_set_value(GTK_ADJUSTMENT(todo_adj_due_year),
			    now->tm_year+1900);

   gtk_text_thaw(GTK_TEXT(todo_text));
   gtk_text_thaw(GTK_TEXT(todo_text_note));
}

int todo_get_details(struct ToDo *new_todo, unsigned char *attrib)
{
   int i;
   
   new_todo->indefinite = (GTK_TOGGLE_BUTTON(todo_no_due_date_checkbox)->active);
   if (!new_todo->indefinite) {
      new_todo->due.tm_mon = gtk_spin_button_get_value_as_int
	(GTK_SPIN_BUTTON(todo_spinner_due_mon)) - 1;
      new_todo->due.tm_mday = gtk_spin_button_get_value_as_int
	(GTK_SPIN_BUTTON(todo_spinner_due_day));
      new_todo->due.tm_year = gtk_spin_button_get_value_as_int
	(GTK_SPIN_BUTTON(todo_spinner_due_year)) - 1900;
   }
   new_todo->priority=1;
   for (i=0; i<5; i++) {
      if (GTK_TOGGLE_BUTTON(radio_button_todo[i])->active) {
	 new_todo->priority=i+1;
	 break;
      }
   }
   new_todo->complete = (GTK_TOGGLE_BUTTON(todo_completed_checkbox)->active);
   //todo - can there be an entry with no description?
   new_todo->description = gtk_editable_get_chars
     (GTK_EDITABLE(todo_text), 0, -1);
   new_todo->note = gtk_editable_get_chars
     (GTK_EDITABLE(todo_text_note), 0, -1);
   if (new_todo->note[0]=='\0') {
      free(new_todo->note);
      new_todo->note=NULL;
   }

   if (todo_category == CATEGORY_ALL) {
      *attrib = 0;
   } else {
      *attrib = todo_category;
   }

#ifdef JPILOT_DEBUG
   printf("attrib = %d\n", *attrib);
   printf("indefinite=%d\n",new_todo->indefinite);
   if (!new_todo->indefinite)
     printf("due: %d/%d/%d\n",new_todo->due.tm_mon,
	    new_todo->due.tm_mday, 
	    new_todo->due.tm_year);
   printf("priority=%d\n",new_todo->priority);
   printf("complete=%d\n",new_todo->complete);
   printf("description=[%s]\n",new_todo->description);
   printf("note=[%s]\n",new_todo->note);
#endif
}

static void cb_clist_selection(GtkWidget      *clist,
			 gint           row,
			 gint           column,
			 GdkEventButton *event,
			 gpointer       data)
{
   struct ToDo *todo;//, new_a;
   MyToDo *mtodo;
   struct ToDo new_todo;
   unsigned char attrib;
   int i;

   clist_row_selected=row;

   mtodo = gtk_clist_get_row_data(GTK_CLIST(clist), row);

   if (mtodo == GINT_TO_POINTER(CLIST_NEW_ENTRY_DATA)) {
      gtk_clist_set_text(GTK_CLIST(clist), row, 2,
			 "Fill in details, then click here again");
      gtk_clist_set_row_data(GTK_CLIST(clist), row,
			     GINT_TO_POINTER(CLIST_ADDING_ENTRY_DATA));
      todo_clear_details();
      return;
   }
   
   if (mtodo == GINT_TO_POINTER(CLIST_ADDING_ENTRY_DATA)) {
      todo_get_details(&new_todo, &attrib);
      pc_todo_write(&new_todo, NEW_PC_REC, attrib);
      free_ToDo(&new_todo);
      update_todo_screen();
      return;
   }

   if (mtodo==NULL) {
      return;
   }
   todo=&(mtodo->todo);
   
   gtk_text_freeze(GTK_TEXT(todo_text));
   gtk_text_freeze(GTK_TEXT(todo_text_note));

   gtk_text_set_point(GTK_TEXT(todo_text), 0);
   gtk_text_forward_delete(GTK_TEXT(todo_text),
			   gtk_text_get_length(GTK_TEXT(todo_text)));

   gtk_text_set_point(GTK_TEXT(todo_text_note), 0);
   gtk_text_forward_delete(GTK_TEXT(todo_text_note),
			   gtk_text_get_length(GTK_TEXT(todo_text_note)));

   //gtk_text_insert(GTK_TEXT(todo_text), NULL,NULL,NULL, "Category: ", -1);
   //gtk_text_insert(GTK_TEXT(todo_text), NULL,NULL,NULL,
	//	   todo_app_info.category.name[mtodo->attrib & 0x0F], -1);
   //gtk_text_insert(GTK_TEXT(todo_text), NULL,NULL,NULL, "\n", -1);

   gtk_text_insert(GTK_TEXT(todo_text), NULL,NULL,NULL, todo->description, -1);

   if (todo->note[0]) {
      gtk_text_insert(GTK_TEXT(todo_text_note), NULL,NULL,NULL, todo->note, -1);
   }

   if ( (todo->priority<1) || (todo->priority>5) ) {
      printf("Priority out of range\n");
   } else {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_button_todo[todo->priority-1]), TRUE);
   }

   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(todo_completed_checkbox), todo->complete);

   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(todo_no_due_date_checkbox),
				todo->indefinite);
   if (!todo->indefinite) {
      gtk_adjustment_set_value(GTK_ADJUSTMENT(todo_adj_due_mon),
			       todo->due.tm_mon+1);
      gtk_adjustment_set_value(GTK_ADJUSTMENT(todo_adj_due_day),
			       todo->due.tm_mday);
      gtk_adjustment_set_value(GTK_ADJUSTMENT(todo_adj_due_year),
			       todo->due.tm_year+1900);
   } else {
      gtk_adjustment_set_value(GTK_ADJUSTMENT(todo_adj_due_mon), 1);
      gtk_adjustment_set_value(GTK_ADJUSTMENT(todo_adj_due_day), 1);
      gtk_adjustment_set_value(GTK_ADJUSTMENT(todo_adj_due_year), 1900);
   }

   gtk_text_thaw(GTK_TEXT(todo_text));
   gtk_text_thaw(GTK_TEXT(todo_text_note));
}


void update_todo_screen()
{
   int num_entries, entries_shown, i;
   gchar *empty_line[] = { "","","","" };
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
   ToDoList *temp_todo;
   static ToDoList *todo_list=NULL;
   char str[50];

   free_ToDoList(&todo_list);

#ifdef JPILOT_DEBUG
    for (i=0;i<16;i++) {
      printf("renamed:[%02d]:\n",todo_app_info.category.renamed[i]);
      printf("category name:[%02d]:",i);
      print_string(todo_app_info.category.name[i],16);
      printf("category ID:%d\n", todo_app_info.category.ID[i]);
   }
   printf("dirty %d\n",todo_app_info.dirty);
   printf("sortByCompany %d\n",todo_app_info.sortByPriority);
#endif

   num_entries = get_todos(&todo_list);
   gtk_clist_clear(GTK_CLIST(clist));
   //gtk_text_backward_delete(GTK_TEXT(todo_text1),
	//		    gtk_text_get_length(GTK_TEXT(todo_text1)));
   
   //if (todo_list==NULL) {
   //   return;
   //}

   if (todo_category==CATEGORY_ALL) {
      sprintf(str, "Category: %s", "All");
   } else {
      sprintf(str, "Category: %s", todo_app_info.category.name[todo_category]);
   }
   gtk_label_set_text(GTK_LABEL(todo_cat_label), str);
   
   //Clear the text box to make things look nice
   gtk_text_set_point(GTK_TEXT(todo_text), 0);
   gtk_text_forward_delete(GTK_TEXT(todo_text),
			   gtk_text_get_length(GTK_TEXT(todo_text)));

   gtk_clist_freeze(GTK_CLIST(clist));

   entries_shown=0;
   for (temp_todo = todo_list, i=0; temp_todo; temp_todo=temp_todo->next, i++) {
      if ( ((temp_todo->mtodo.attrib & 0x0F) != todo_category) &&
	  todo_category != CATEGORY_ALL) {
	 continue;
      }

      entries_shown++;
      gtk_clist_prepend(GTK_CLIST(clist), empty_line);
      gtk_clist_set_text(GTK_CLIST(clist), 0, 2, temp_todo->mtodo.todo.description);
      sprintf(str, "%d", temp_todo->mtodo.todo.priority);
      gtk_clist_set_text(GTK_CLIST(clist), 0, 1, str);
      gtk_clist_set_row_data(GTK_CLIST(clist), 0, &(temp_todo->mtodo));

      if (temp_todo->mtodo.rt == NEW_PC_REC) {
	 colormap = gtk_widget_get_colormap(clist);
	 color.red=CLIST_NEW_RED;
	 color.green=CLIST_NEW_GREEN;
	 color.blue=CLIST_NEW_BLUE;
	 gdk_color_alloc(colormap, &color);
	 gtk_clist_set_background(GTK_CLIST(clist), 0, &color);
      }
      if (temp_todo->mtodo.rt == DELETED_PALM_REC) {
	 colormap = gtk_widget_get_colormap(clist);
	 color.red=CLIST_DEL_RED;
	 color.green=CLIST_DEL_GREEN;
	 color.blue=CLIST_DEL_BLUE;
	 gdk_color_alloc(colormap, &color);
	 gtk_clist_set_background(GTK_CLIST(clist), 0, &color);
      }
      
      get_pixmaps(clist,
		  &pixmap_note, &pixmap_alarm, &pixmap_check, &pixmap_checked,
		  &mask_note, &mask_alarm, &mask_check, &mask_checked
		  );
      
      if (temp_todo->mtodo.todo.note[0]) {
	 //Put a note pixmap up
	 gtk_clist_set_pixmap(GTK_CLIST(clist), 0, 3, pixmap_note, mask_note);
      }

      if (temp_todo->mtodo.todo.complete) {
	 //Put a check or checked pixmap up
	 gtk_clist_set_pixmap(GTK_CLIST(clist), 0, 0, pixmap_checked, mask_checked);
      } else {
	 gtk_clist_set_pixmap(GTK_CLIST(clist), 0, 0, pixmap_check, mask_check);
      }
   }

   //printf("entries_shown=%d\n",entries_shown);
   gtk_clist_append(GTK_CLIST(clist), empty_line);
   gtk_clist_set_text(GTK_CLIST(clist), entries_shown, 2,
		      "Select here to add an entry");
   gtk_clist_set_row_data(GTK_CLIST(clist), entries_shown,
			  GINT_TO_POINTER(CLIST_NEW_ENTRY_DATA));

   //If there is an item in the list, select the first one
   if (entries_shown>0) {
      gtk_clist_select_row(GTK_CLIST(clist), 0, 1);
      //cb_clist_selection(clist, 0, 0, (GdkEventButton *)455, "");
   }
   
   gtk_clist_thaw(GTK_CLIST(clist));
}

int todo_gui(GtkWidget *vbox, GtkWidget *hbox)
{
   extern GtkWidget *glob_date_label;
   extern glob_date_timer_tag;
   GtkWidget *vbox1, *vbox2;
   GtkWidget *hbox_temp, *vbox_temp1, *vbox_temp2, *vbox_temp3, *vbox_temp4;
   GtkWidget *separator;
   GtkWidget *label;
   GtkWidget *vscrollbar;
   GtkWidget *button;
   time_t ltime;
   struct tm *now;
#define MAX_STR 100
   char str[MAX_STR];
   int i;
   GtkWidget *scrolled_window;
   GtkWidget *category_menu;
   GtkWidget *menu_item;
   GtkWidget *menu;
   GSList    *group;
   GtkWidget *phone_list_menu;
   
   clist_row_selected=0;

   get_todo_app_info(&todo_app_info);

   vbox1 = gtk_vbox_new (FALSE, 0);
   vbox2 = gtk_vbox_new (FALSE, 0);
   gtk_box_pack_start(GTK_BOX(hbox), vbox1, TRUE, TRUE, 5);
   gtk_box_pack_start(GTK_BOX(hbox), vbox2, TRUE, TRUE, 5);

   //Add buttons in left vbox
   button = gtk_button_new_with_label("Delete");
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_delete_todo), NULL);
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
		      cb_todo_category, GINT_TO_POINTER(CATEGORY_ALL));
   group = gtk_radio_menu_item_group(GTK_RADIO_MENU_ITEM(menu_item));
   gtk_menu_append(GTK_MENU(menu), menu_item);
   gtk_widget_show(menu_item);
   for (i=0; i<16; i++) {
      if (todo_app_info.category.name[i][0]) {
	 menu_item = gtk_radio_menu_item_new_with_label(
		     group, todo_app_info.category.name[i]);
	 gtk_signal_connect(GTK_OBJECT(menu_item), "activate",
			    cb_todo_category, GINT_TO_POINTER(i));
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
   gtk_widget_set_usize(GTK_WIDGET(scrolled_window), 200, 0);
   gtk_container_set_border_width(GTK_CONTAINER(scrolled_window), 0);
   gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
   gtk_box_pack_start(GTK_BOX(vbox1), scrolled_window, TRUE, TRUE, 0);
   gtk_widget_show(scrolled_window);

   clist = gtk_clist_new(4);
   gtk_signal_connect(GTK_OBJECT(clist), "select_row",
		      GTK_SIGNAL_FUNC(cb_clist_selection),
		      todo_text);
   gtk_clist_set_shadow_type(GTK_CLIST(clist), SHADOW);
   gtk_clist_set_selection_mode(GTK_CLIST(clist), GTK_SELECTION_BROWSE);
   gtk_clist_set_column_width(GTK_CLIST(clist), 0, 14);
   gtk_clist_set_column_width(GTK_CLIST(clist), 1, 8);
   gtk_clist_set_column_width(GTK_CLIST(clist), 2, 240);
   gtk_clist_set_column_width(GTK_CLIST(clist), 3, 14);
   gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW
					 (scrolled_window), clist);
   
   //The completed check box
   todo_completed_checkbox = gtk_check_button_new_with_label("Completed");
   gtk_box_pack_start(GTK_BOX(vbox2), todo_completed_checkbox, FALSE, FALSE, 0);
   gtk_widget_show(todo_completed_checkbox);


   //Priority Radio Buttons
   hbox_temp = gtk_hbox_new (FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, FALSE, FALSE, 0);

   label = gtk_label_new("Priority: ");
   gtk_box_pack_start(GTK_BOX(hbox_temp), label, FALSE, FALSE, 0);
   gtk_widget_show(label);

   group = NULL;
   for (i=0; i<5; i++) {
      sprintf(str,"%d",i+1);
      radio_button_todo[i] = gtk_radio_button_new_with_label(group, str);
      group = gtk_radio_button_group(GTK_RADIO_BUTTON(radio_button_todo[i]));
      //gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (radio_button_todo), TRUE);
      gtk_box_pack_start(GTK_BOX(hbox_temp),
			 radio_button_todo[i], FALSE, FALSE, 0);
      gtk_widget_show(radio_button_todo[i]);
   }
   gtk_widget_set_usize (hbox_temp, 10, 0);
   gtk_widget_show(hbox_temp);

   
   //Begin spinners for due date
   //Boxes
   hbox_temp = gtk_hbox_new(FALSE, 0);
   gtk_widget_set_usize(hbox_temp, 100, 0);
   vbox_temp1 = gtk_vbox_new(FALSE, 0);
   vbox_temp2 = gtk_vbox_new(FALSE, 0);
   vbox_temp3 = gtk_vbox_new(FALSE, 0);
   vbox_temp4 = gtk_vbox_new(FALSE, 0);
   label = gtk_label_new("\nDate Due:");
   gtk_box_pack_start(GTK_BOX(hbox_temp), label, FALSE, FALSE, 0);
   gtk_widget_show(label);
   gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, FALSE, FALSE, 0);
   gtk_box_pack_start(GTK_BOX(hbox_temp), vbox_temp1, FALSE, FALSE, 0);
   gtk_box_pack_start(GTK_BOX(hbox_temp), vbox_temp2, FALSE, FALSE, 0);
   gtk_box_pack_start(GTK_BOX(hbox_temp), vbox_temp3, FALSE, FALSE, 0);
   gtk_box_pack_start(GTK_BOX(hbox_temp), vbox_temp4, FALSE, FALSE, 5);

   //Labels
   label = gtk_label_new("Month:");
   gtk_box_pack_start(GTK_BOX(vbox_temp1), label, FALSE, FALSE, 0);
   gtk_widget_show(label);
   label = gtk_label_new("Day:");
   gtk_box_pack_start(GTK_BOX(vbox_temp2), label, FALSE, FALSE, 0);
   gtk_widget_show(label);
   label = gtk_label_new("Year:");
   gtk_box_pack_start(GTK_BOX(vbox_temp3), label, FALSE, FALSE, 0);
   gtk_widget_show(label);

   //month
   todo_adj_due_mon = (GtkAdjustment *)gtk_adjustment_new
     (now->tm_mon+1, 1.0, 12.0, 1.0, 5.0, 0.0);
   todo_spinner_due_mon = gtk_spin_button_new(todo_adj_due_mon, 0, 0);
   gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(todo_spinner_due_mon), FALSE);
   gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(todo_spinner_due_mon), TRUE);
   gtk_spin_button_set_shadow_type(GTK_SPIN_BUTTON(todo_spinner_due_mon),
				   SHADOW);
   gtk_box_pack_start(GTK_BOX(vbox_temp1),
		      todo_spinner_due_mon, FALSE, TRUE, 0);
   //Day
   todo_adj_due_day = (GtkAdjustment *)gtk_adjustment_new
     (now->tm_mday, 1.0, 31.0, 1.0, 5.0, 0.0);
   todo_spinner_due_day = gtk_spin_button_new(todo_adj_due_day, 0, 0);
   gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(todo_spinner_due_day), FALSE);
   gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(todo_spinner_due_day), TRUE);
   gtk_spin_button_set_shadow_type(GTK_SPIN_BUTTON(todo_spinner_due_day),
				   SHADOW);
   gtk_box_pack_start(GTK_BOX(vbox_temp2),
		      todo_spinner_due_day, FALSE, TRUE, 0);
   //Year
   todo_adj_due_year = (GtkAdjustment *)gtk_adjustment_new
     (now->tm_year+1900, 0.0, 2037.0, 1.0, 100.0, 0.0);
   todo_spinner_due_year = gtk_spin_button_new(todo_adj_due_year, 0, 0);
   gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(todo_spinner_due_year), FALSE);
   gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(todo_spinner_due_year), TRUE);
   gtk_spin_button_set_shadow_type(GTK_SPIN_BUTTON(todo_spinner_due_year),
				   SHADOW);
   gtk_widget_set_usize(todo_spinner_due_year, 55, 0);
   gtk_box_pack_start(GTK_BOX(vbox_temp3),
		      todo_spinner_due_year, FALSE, TRUE, 0);

   //The No due date check box
   todo_no_due_date_checkbox = gtk_check_button_new_with_label("No Due Date");
   gtk_box_pack_end(GTK_BOX(vbox_temp4), todo_no_due_date_checkbox, FALSE, FALSE, 0);
   gtk_widget_show(todo_no_due_date_checkbox);

   gtk_widget_show(todo_spinner_due_mon);
   gtk_widget_show(todo_spinner_due_day);
   gtk_widget_show(todo_spinner_due_year);
   gtk_widget_show(vbox_temp1);
   gtk_widget_show(vbox_temp2);
   gtk_widget_show(vbox_temp3);
   gtk_widget_show(vbox_temp4);
   gtk_widget_show(hbox_temp);
   //end spinners

   
   

   //The Description text box on the right side
   hbox_temp = gtk_hbox_new (FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, FALSE, FALSE, 0);
   gtk_widget_show(hbox_temp);

   todo_cat_label = gtk_label_new("category:");
   gtk_box_pack_start(GTK_BOX(hbox_temp), todo_cat_label, FALSE, FALSE, 0);
   gtk_widget_show(todo_cat_label);

   hbox_temp = gtk_hbox_new (FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, TRUE, TRUE, 0);

   todo_text = gtk_text_new(NULL, NULL);
   gtk_text_set_editable(GTK_TEXT(todo_text), TRUE);
   gtk_text_set_word_wrap(GTK_TEXT(todo_text), TRUE);
   vscrollbar = gtk_vscrollbar_new(GTK_TEXT(todo_text)->vadj);
   gtk_box_pack_start(GTK_BOX(hbox_temp), todo_text, TRUE, TRUE, 0);
   gtk_box_pack_start(GTK_BOX(hbox_temp), vscrollbar, FALSE, FALSE, 0);
   gtk_widget_show(todo_text);
   gtk_widget_show(vscrollbar);   
   gtk_widget_show(hbox_temp);

   
   //The Note text box on the right side
   hbox_temp = gtk_hbox_new (FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, FALSE, FALSE, 0);
   gtk_widget_show(hbox_temp);

   label = gtk_label_new("Note");
   gtk_box_pack_start(GTK_BOX(hbox_temp), label, FALSE, FALSE, 0);
   gtk_widget_show(label);

   hbox_temp = gtk_hbox_new (FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, TRUE, TRUE, 0);

   todo_text_note = gtk_text_new(NULL, NULL);
   gtk_text_set_editable(GTK_TEXT(todo_text_note), TRUE);
   gtk_text_set_word_wrap(GTK_TEXT(todo_text_note), TRUE);
   vscrollbar = gtk_vscrollbar_new(GTK_TEXT(todo_text_note)->vadj);
   gtk_box_pack_start(GTK_BOX(hbox_temp), todo_text_note, TRUE, TRUE, 0);
   gtk_box_pack_start(GTK_BOX(hbox_temp), vscrollbar, FALSE, FALSE, 0);
   gtk_widget_show(todo_text_note);
   gtk_widget_show(vscrollbar);   
   gtk_widget_show(hbox_temp);

   
   
   
   
   gtk_widget_show(clist);
   gtk_widget_show(vbox1);
   gtk_widget_show(vbox2);
   gtk_widget_show(vbox);
   gtk_widget_show(hbox);
   
   todo_category = CATEGORY_ALL;
   update_todo_screen();
}

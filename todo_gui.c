/* todo_gui.c
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
#include "i18n.h"
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdk.h>
#include <time.h>
#include <stdlib.h>
#include "utils.h"
#include "todo.h"
#include "log.h"
#include "prefs.h"
#include "print.h"


#define NUM_TODO_CAT_ITEMS 16

extern GtkTooltips *glob_tooltips;

GtkWidget *clist;
GtkWidget *todo_text, *todo_text_note;
GtkWidget *todo_completed_checkbox;
GtkWidget *todo_no_due_date_checkbox;
GtkWidget *radio_button_todo[5];
GtkWidget *todo_spinner_due_mon;
GtkWidget *todo_spinner_due_day;
GtkWidget *todo_spinner_due_year;
GtkAdjustment *todo_adj_due_mon, *todo_adj_due_day, *todo_adj_due_year;
GtkWidget *todo_cat_menu2;
/*We need an extra one forthe ALL category */
GtkWidget *todo_cat_menu_item1[NUM_TODO_CAT_ITEMS+1];
GtkWidget *todo_cat_menu_item2[NUM_TODO_CAT_ITEMS];
GtkWidget *todo_hide_completed_checkbox;
GtkWidget *date_due_hbox;
GtkWidget *todo_modify_button;
GtkWidget *category_menu1;
GtkWidget *scrolled_window;
GtkWidget *pane;

struct ToDoAppInfo todo_app_info;
int todo_category=CATEGORY_ALL;
int clist_row_selected;

void update_todo_screen();
int todo_clear_details();
int todo_clist_redraw();



int todo_print()
{
   long this_many;
   MyToDo *mtodo;
   ToDoList *todo_list;
   ToDoList todo_list1;

   get_pref(PREF_PRINT_THIS_MANY, &this_many, NULL);

   todo_list=NULL;
   if (this_many==1) {
      mtodo = gtk_clist_get_row_data(GTK_CLIST(clist), clist_row_selected);
      if (mtodo < (MyToDo *)CLIST_MIN_DATA) {
	 return -1;
      }
      memcpy(&(todo_list1.mtodo), mtodo, sizeof(MyToDo));
      todo_list1.next=NULL;
      todo_list = &todo_list1;
   }
   if (this_many==2) {
      get_todos2(&todo_list, SORT_ASCENDING, 2, 2, todo_category);
   }
   if (this_many==3) {
      get_todos2(&todo_list, SORT_ASCENDING, 2, 2, CATEGORY_ALL);
   }

   print_todos(todo_list);

   if ((this_many==2) || (this_many==3)) {
      free_ToDoList(&todo_list);
   }

   return 0;
}

void cb_delete_todo(GtkWidget *widget,
		    gpointer   data)
{
   MyToDo *mtodo;
   int flag;
   
   mtodo = gtk_clist_get_row_data(GTK_CLIST(clist), clist_row_selected);
   if (mtodo < (MyToDo *)CLIST_MIN_DATA) {
      return;
   }
   flag = GPOINTER_TO_INT(data);
   if ((flag==MODIFY_FLAG) || (flag==DELETE_FLAG)) {
      jpilot_logf(LOG_DEBUG, "calling delete_pc_record\n");
      delete_pc_record(TODO, mtodo, flag);
      if (flag==DELETE_FLAG) {
	 /* when we redraw we want to go to the line above the deleted one */
	 if (clist_row_selected>0) {
	    clist_row_selected--;
	 }
      }
   }
   todo_clist_redraw();
}

void cb_todo_category(GtkWidget *item, int selection)
{
   if ((GTK_CHECK_MENU_ITEM(item))->active) {
      todo_category = selection;
      jpilot_logf(LOG_DEBUG, "todo_category = %d\n",todo_category);
      todo_clear_details();
      update_todo_screen();
   }
}

void cb_check_button_no_due_date(GtkWidget *widget, gpointer data)
{
   if (GTK_TOGGLE_BUTTON(widget)->active) {
      gtk_widget_hide(date_due_hbox);
   } else {
      gtk_widget_show(date_due_hbox);
   }
}

void cb_hide_completed(GtkWidget *widget,
		       gpointer   data)
{
   set_pref(PREF_HIDE_COMPLETED,
	    GTK_TOGGLE_BUTTON(todo_hide_completed_checkbox)->active);
   todo_clear_details();
   update_todo_screen();
}

int todo_clear_details()
{
   time_t ltime;
   struct tm *now;
   int new_cat;
   
   time(&ltime);
   now = localtime(&ltime);

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

   if (todo_category==CATEGORY_ALL) {
      new_cat = 0;
   } else {
      new_cat = todo_category;
   }
   gtk_check_menu_item_set_active
     (GTK_CHECK_MENU_ITEM(todo_cat_menu_item2[new_cat]), TRUE);
   gtk_option_menu_set_history(GTK_OPTION_MENU(todo_cat_menu2), new_cat);
   
   return 0;
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
   /*Can there be an entry with no description? */
   /*Yes, but the Palm Pilot gui doesn't allow it to be entered on the Palm, */
   /*it will show it though.  I allow it. */
   new_todo->description = gtk_editable_get_chars
     (GTK_EDITABLE(todo_text), 0, -1);
   new_todo->note = gtk_editable_get_chars
     (GTK_EDITABLE(todo_text_note), 0, -1);
   if (new_todo->note[0]=='\0') {
      free(new_todo->note);
      new_todo->note=NULL;
   }

   for (i=0; i<NUM_TODO_CAT_ITEMS; i++) {
      if (GTK_IS_WIDGET(todo_cat_menu_item2[i])) {
	 if (GTK_CHECK_MENU_ITEM(todo_cat_menu_item2[i])->active) {
	    *attrib = i;
	    break;
	 }
      }
   }

#ifdef JPILOT_DEBUG
   jpilot_logf(LOG_DEBUG, "attrib = %d\n", *attrib);
   jpilot_logf(LOG_DEBUG, "indefinite=%d\n",new_todo->indefinite);
   if (!new_todo->indefinite)
     jpilot_logf(LOG_DEBUG, "due: %d/%d/%d\n",new_todo->due.tm_mon,
	    new_todo->due.tm_mday, 
	    new_todo->due.tm_year);
   jpilot_logf(LOG_DEBUG, "priority=%d\n",new_todo->priority);
   jpilot_logf(LOG_DEBUG, "complete=%d\n",new_todo->complete);
   jpilot_logf(LOG_DEBUG, "description=[%s]\n",new_todo->description);
   jpilot_logf(LOG_DEBUG, "note=[%s]\n",new_todo->note);
#endif
   
   return 0;
}

static void cb_add_new_record(GtkWidget *widget,
			      gpointer   data)
{
   MyToDo *mtodo;
   struct ToDo new_todo;
   unsigned char attrib;
   int flag;
   unsigned int unique_id;
   
   flag=GPOINTER_TO_INT(data);
   
   if (flag==CLEAR_FLAG) {
      /*Clear button was hit */
      todo_clear_details();
      return;
   }
   if ((flag!=NEW_FLAG) && (flag!=MODIFY_FLAG)) {
      return;
   }
   if (flag==MODIFY_FLAG) {
      mtodo = gtk_clist_get_row_data(GTK_CLIST(clist), clist_row_selected);
      if (mtodo < (MyToDo *)CLIST_MIN_DATA) {
	 return;
      }
      if ((mtodo->rt==DELETED_PALM_REC) || (mtodo->rt==MODIFIED_PALM_REC)) {
	 jpilot_logf(LOG_INFO, "You can't modify a record that is deleted\n");
	 return;
      }
   }
   todo_get_details(&new_todo, &attrib);
   pc_todo_write(&new_todo, NEW_PC_REC, attrib, &unique_id);
   free_ToDo(&new_todo);
   if (flag==MODIFY_FLAG) {
      cb_delete_todo(NULL, data);
   } else {
      /* glob_find_id = unique_id;*/
      todo_clist_redraw();
   }

   return;
}


static void cb_clist_selection(GtkWidget      *clist,
			       gint           row,
			       gint           column,
			       GdkEventButton *event,
			       gpointer       data)
{
   struct ToDo *todo;/*, new_a; */
   MyToDo *mtodo;
   int i, index, count;
#ifdef OLD_ENTRY
   struct ToDo new_todo;
   unsigned char attrib;
#endif

   clist_row_selected=row;

   mtodo = gtk_clist_get_row_data(GTK_CLIST(clist), row);

#ifdef OLD_ENTRY
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
#endif
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

   /*gtk_text_insert(GTK_TEXT(todo_text), NULL,NULL,NULL, "Category: ", -1); */
   /*gtk_text_insert(GTK_TEXT(todo_text), NULL,NULL,NULL, */
	/*	   todo_app_info.category.name[mtodo->attrib & 0x0F], -1); */
   /*gtk_text_insert(GTK_TEXT(todo_text), NULL,NULL,NULL, "\n", -1); */

   index = mtodo->attrib & 0x0F;
   if (todo_cat_menu_item2[index]==NULL) {
      /* Illegal category */
      jpilot_logf(LOG_DEBUG, "Category is not legal\n");
      index = count = 0;
   } else {
      /* We need to count how many items down in the list this is */
      for (i=index, count=0; i>=0; i--) {
	 if (todo_cat_menu_item2[i]) {
	    count++;
	 }
      }
      count--;
   }
   
   gtk_check_menu_item_set_active
     (GTK_CHECK_MENU_ITEM(todo_cat_menu_item2[index]), TRUE);
   gtk_option_menu_set_history(GTK_OPTION_MENU(todo_cat_menu2), count);

   
   if (todo->description) {
      if (todo->description[0]) {
	 gtk_text_insert(GTK_TEXT(todo_text), NULL,NULL,NULL, todo->description, -1);
      }
   }

   if (todo->note) {
      if (todo->note[0]) {
	 gtk_text_insert(GTK_TEXT(todo_text_note), NULL,NULL,NULL, todo->note, -1);
      }
   }

   if ( (todo->priority<1) || (todo->priority>5) ) {
      jpilot_logf(LOG_WARN, "Priority out of range\n");
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

   /*If they have clicked on the checkmark box then do a modify */
   if (column==0) {
      todo->complete = !(todo->complete);
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(todo_completed_checkbox), todo->complete);
      gtk_signal_emit_by_name(GTK_OBJECT(todo_modify_button), "clicked");
   }
}


void update_todo_screen()
{
   int num_entries, entries_shown, i;
   gchar *empty_line[] = { "","","","","" };
   GdkPixmap *pixmap_note;
   GdkPixmap *pixmap_check;
   GdkPixmap *pixmap_checked;
   GdkBitmap *mask_note;
   GdkBitmap *mask_check;
   GdkBitmap *mask_checked;
   GdkColor color;
   GdkColormap *colormap;
   ToDoList *temp_todo;
   static ToDoList *todo_list=NULL;
   char str[50];
   long ivalue, modified, deleted;
   const char *svalue;
   long hide_completed;

   free_ToDoList(&todo_list);

   get_pref(PREF_HIDE_COMPLETED, &hide_completed, NULL);

#ifdef JPILOT_DEBUG
    for (i=0;i<NUM_TODO_CAT_ITEMS;i++) {
      jpilot_logf(LOG_DEBUG, "renamed:[%02d]:\n",todo_app_info.category.renamed[i]);
      jpilot_logf(LOG_DEBUG, "category name:[%02d]:",i);
      print_string(todo_app_info.category.name[i],16);
      jpilot_logf(LOG_DEBUG, "category ID:%d\n", todo_app_info.category.ID[i]);
   }
   jpilot_logf(LOG_DEBUG, "dirty %d\n",todo_app_info.dirty);
   jpilot_logf(LOG_DEBUG, "sortByCompany %d\n",todo_app_info.sortByPriority);
#endif

   num_entries = get_todos(&todo_list, SORT_DESCENDING);
   gtk_clist_clear(GTK_CLIST(clist));

   /*Clear the text box to make things look nice */
   gtk_text_set_point(GTK_TEXT(todo_text), 0);
   gtk_text_forward_delete(GTK_TEXT(todo_text),
			   gtk_text_get_length(GTK_TEXT(todo_text)));

   if (todo_list==NULL) {
      gtk_tooltips_set_tip(glob_tooltips, category_menu1, _("0 records"), NULL);   
      return;
   }

   gtk_clist_freeze(GTK_CLIST(clist));

   entries_shown=0;
   for (temp_todo = todo_list, i=0; temp_todo; temp_todo=temp_todo->next, i++) {
      if ( ((temp_todo->mtodo.attrib & 0x0F) != todo_category) &&
	  todo_category != CATEGORY_ALL) {
	 continue;
      }
      /*Hide the completed records if need be */
      if (hide_completed && temp_todo->mtodo.todo.complete) {
	 continue;
      }

      get_pref(PREF_SHOW_MODIFIED, &modified, NULL);
      get_pref(PREF_SHOW_DELETED, &deleted, NULL);

      if (temp_todo->mtodo.rt == MODIFIED_PALM_REC) {
	 if (!modified) {
	    num_entries--;
	    i--;
	    continue;
	 }
      }
      if (temp_todo->mtodo.rt == DELETED_PALM_REC) {
	 if (!deleted) {
	    num_entries--;
	    i--;
	    continue;
	 }
      }

      entries_shown++;
      gtk_clist_prepend(GTK_CLIST(clist), empty_line);
      gtk_clist_set_text(GTK_CLIST(clist), 0, 2, temp_todo->mtodo.todo.description);
      sprintf(str, "%d", temp_todo->mtodo.todo.priority);
      gtk_clist_set_text(GTK_CLIST(clist), 0, 1, str);

      if (!temp_todo->mtodo.todo.indefinite) {
	  get_pref(PREF_SHORTDATE, &ivalue, &svalue);
	  get_pref_possibility(PREF_SHORTDATE,ivalue,str);
	  strftime(str, 50, svalue, &(temp_todo->mtodo.todo.due));
      }
      else {
	  sprintf(str, "No date");
      }
      gtk_clist_set_text(GTK_CLIST(clist), 0, 4, str);

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
      if (temp_todo->mtodo.rt == MODIFIED_PALM_REC) {
	 colormap = gtk_widget_get_colormap(clist);
	 color.red=CLIST_MOD_RED;
	 color.green=CLIST_MOD_GREEN;
	 color.blue=CLIST_MOD_BLUE;
	 gdk_color_alloc(colormap, &color);
	 gtk_clist_set_background(GTK_CLIST(clist), 0, &color);
      }
      
      get_pixmaps(clist, PIXMAP_NOTE, &pixmap_note, &mask_note);
      get_pixmaps(clist, PIXMAP_BOX_CHECK, &pixmap_check, &mask_check);
      get_pixmaps(clist, PIXMAP_BOX_CHECKED, &pixmap_checked,&mask_checked);
      
      if (temp_todo->mtodo.todo.note[0]) {
	 /*Put a note pixmap up */
	 gtk_clist_set_pixmap(GTK_CLIST(clist), 0, 3, pixmap_note, mask_note);
      }

      if (temp_todo->mtodo.todo.complete) {
	 /*Put a check or checked pixmap up */
	 gtk_clist_set_pixmap(GTK_CLIST(clist), 0, 0, pixmap_checked, mask_checked);
      } else {
	 gtk_clist_set_pixmap(GTK_CLIST(clist), 0, 0, pixmap_check, mask_check);
      }
   }

   jpilot_logf(LOG_DEBUG, "entries_shown=%d\n",entries_shown);
#ifdef OLD_ENTRY
   gtk_clist_append(GTK_CLIST(clist), empty_line);
   gtk_clist_set_text(GTK_CLIST(clist), entries_shown, 2,
		      "Select here to add an entry");
   gtk_clist_set_row_data(GTK_CLIST(clist), entries_shown,
			  GINT_TO_POINTER(CLIST_NEW_ENTRY_DATA));
#endif
   /*If there is an item in the list, select the first one */
   if (entries_shown>0) {
      gtk_clist_select_row(GTK_CLIST(clist), 0, 1);
      /*cb_clist_selection(clist, 0, 0, (GdkEventButton *)455, ""); */
   }
   
   gtk_clist_thaw(GTK_CLIST(clist));

   sprintf(str, _("%d of %d records"), entries_shown, num_entries);
   gtk_tooltips_set_tip(glob_tooltips, category_menu1, str, NULL);   
}

/*todo combine the next 2 functions */
static int make_category_menu1(GtkWidget **category_menu)
{
   int i;
   
   GtkWidget *menu;
   GSList    *group;

   *category_menu = gtk_option_menu_new();
   
   menu = gtk_menu_new();
   group = NULL;
   
   todo_cat_menu_item1[0] = gtk_radio_menu_item_new_with_label(group, _("All"));
   gtk_signal_connect(GTK_OBJECT(todo_cat_menu_item1[0]), "activate",
		      cb_todo_category, GINT_TO_POINTER(CATEGORY_ALL));
   group = gtk_radio_menu_item_group(GTK_RADIO_MENU_ITEM(todo_cat_menu_item1[0]));
   gtk_menu_append(GTK_MENU(menu), todo_cat_menu_item1[0]);
   gtk_widget_show(todo_cat_menu_item1[0]);
   for (i=0; i<NUM_TODO_CAT_ITEMS; i++) {
      if (todo_app_info.category.name[i][0]) {
	 todo_cat_menu_item1[i+1] = gtk_radio_menu_item_new_with_label(
		     group, todo_app_info.category.name[i]);
	 gtk_signal_connect(GTK_OBJECT(todo_cat_menu_item1[i+1]), "activate",
			    cb_todo_category, GINT_TO_POINTER(i));
	 group = gtk_radio_menu_item_group(GTK_RADIO_MENU_ITEM(todo_cat_menu_item1[i+1]));
	 gtk_menu_append(GTK_MENU(menu), todo_cat_menu_item1[i+1]);
	 gtk_widget_show(todo_cat_menu_item1[i+1]);
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

   todo_cat_menu2 = gtk_option_menu_new();
   
   menu = gtk_menu_new();
   group = NULL;
   
   for (i=0; i<NUM_TODO_CAT_ITEMS; i++) {
      if (todo_app_info.category.name[i][0]) {
	 todo_cat_menu_item2[i] = gtk_radio_menu_item_new_with_label
	   (group, todo_app_info.category.name[i]);
	 group = gtk_radio_menu_item_group
	   (GTK_RADIO_MENU_ITEM(todo_cat_menu_item2[i]));
	 gtk_menu_append(GTK_MENU(menu), todo_cat_menu_item2[i]);
	 gtk_widget_show(todo_cat_menu_item2[i]);
      } else {
	 todo_cat_menu_item2[i] = NULL;
      }
   }
   gtk_option_menu_set_menu(GTK_OPTION_MENU(todo_cat_menu2), menu);
   
   return 0;
}

static int todo_find()
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

static gboolean
  cb_key_pressed(GtkWidget *widget, GdkEventKey *event,
		 gpointer next_widget) 
{
   if (event->keyval == GDK_Tab) {
      /* See if they are at the end of the text */
      if (gtk_text_get_point(GTK_TEXT(widget)) ==
	  gtk_text_get_length(GTK_TEXT(widget))) {
	 gtk_signal_emit_stop_by_name(GTK_OBJECT(widget), "key_press_event"); 
	 gtk_widget_grab_focus(GTK_WIDGET(next_widget));
	 return TRUE;
      }
   }
   return FALSE; 
}

static int todo_goto_line(int line_num, gfloat percentage)
{
   int total_count;

   clist_count(clist, &total_count);

   /*avoid dividing by zero */
   if (total_count == 0) {
      total_count = 1;
   }
   gtk_clist_select_row(GTK_CLIST(clist), line_num, 1);
   move_scrolled_window_hack(scrolled_window, percentage);

   return 0;
}

/* This redraws the clist and goes back to the same line number */
int todo_clist_redraw()
{
   int line_num;
   GtkScrollbar *sb;
   gfloat upper, lower, value, step, percentage;
   
   line_num = clist_row_selected;

   sb = GTK_SCROLLBAR(GTK_SCROLLED_WINDOW(scrolled_window)->vscrollbar);
   upper = GTK_ADJUSTMENT(sb->range.adjustment)->upper;
   lower = GTK_ADJUSTMENT(sb->range.adjustment)->lower;
   value = GTK_ADJUSTMENT(sb->range.adjustment)->value;
   step = GTK_ADJUSTMENT(sb->range.adjustment)->step_increment;
   if (upper - lower + step) {
      percentage = value/(upper - lower + step);
   } else {
      percentage = 0;
   }

   update_todo_screen();
   
   todo_goto_line(line_num, percentage);
   
   return 0;
}

int todo_refresh()
{
   int index;
 
   if (glob_find_id) {
      todo_category = CATEGORY_ALL;
   }
   if (todo_category==CATEGORY_ALL) {
      index=0;
   } else {
      index=todo_category+1;
   }
   update_todo_screen();
   gtk_option_menu_set_history(GTK_OPTION_MENU(category_menu1), index);
   gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(
       todo_cat_menu_item1[index]), TRUE);
   todo_find();
   return 0;
}

int todo_gui_cleanup()
{
   set_pref(PREF_TODO_PANE, GTK_PANED(pane)->handle_xpos);
   return 0;
}

int todo_gui(GtkWidget *vbox, GtkWidget *hbox)
{
   extern GtkWidget *glob_date_label;
   extern int glob_date_timer_tag;
   GtkWidget *vbox1, *vbox2;
   GtkWidget *hbox_temp;
   GtkWidget *vbox_temp1, *vbox_temp2, *vbox_temp3, *vbox_temp4;
   GtkWidget *align;
   GtkWidget *separator;
   GtkWidget *label;
   GtkWidget *vscrollbar;
   GtkWidget *button;
   time_t ltime;
   struct tm *now;
#define MAX_STR 100
   char str[MAX_STR];
   int i;
   GSList    *group;
   int dmy_order;
   long hide_completed;
   long ivalue;
   const char *svalue;

   
   clist_row_selected=0;

   /* Do some initialization */
   for (i=0; i<NUM_TODO_CAT_ITEMS; i++) {
      todo_cat_menu_item2[i] = NULL;
   }
   
   get_todo_app_info(&todo_app_info);

   if (todo_app_info.category.name[todo_category][0]=='\0') {
      todo_category=CATEGORY_ALL;
   }


   pane = gtk_hpaned_new();
   get_pref(PREF_TODO_PANE, &ivalue, &svalue);
   gtk_paned_set_position(GTK_PANED(pane), ivalue + 2);

   gtk_box_pack_start(GTK_BOX(hbox), pane, TRUE, TRUE, 5);

   vbox1 = gtk_vbox_new(FALSE, 0);
   vbox2 = gtk_vbox_new(FALSE, 0);

   gtk_paned_pack1(GTK_PANED(pane), vbox1, TRUE, FALSE);
   gtk_paned_pack2(GTK_PANED(pane), vbox2, TRUE, FALSE);

   /* gtk_widget_set_usize(GTK_WIDGET(vbox1), 260, 0); */

   /*Add buttons in left vbox */
   button = gtk_button_new_with_label(_("Delete"));
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_delete_todo),
		      GINT_TO_POINTER(DELETE_FLAG));
   gtk_box_pack_start(GTK_BOX(vbox), button, TRUE, TRUE, 0);
   
   /*Separator */
   separator = gtk_hseparator_new();
   gtk_box_pack_start(GTK_BOX(vbox1), separator, FALSE, FALSE, 5);

   time(&ltime);
   now = localtime(&ltime);

   /*Make the Today is: label */
   glob_date_label = gtk_label_new(" ");
   gtk_box_pack_start(GTK_BOX(vbox1), glob_date_label, FALSE, FALSE, 0);
   timeout_date(NULL);
   glob_date_timer_tag = gtk_timeout_add(CLOCK_TICK, timeout_date, NULL);

   /*Separator */
   separator = gtk_hseparator_new();
   gtk_box_pack_start(GTK_BOX(vbox1), separator, FALSE, FALSE, 5);

   
   /*Put the left-hand category menu up */
   make_category_menu1(&category_menu1);

   gtk_box_pack_start(GTK_BOX(vbox1), category_menu1, FALSE, FALSE, 0);
   
   
   get_pref(PREF_HIDE_COMPLETED, &hide_completed, &svalue);

   /*The hide completed check box */
   todo_hide_completed_checkbox = gtk_check_button_new_with_label(_("Hide Completed ToDos"));
   gtk_box_pack_start(GTK_BOX(vbox1), todo_hide_completed_checkbox, FALSE, FALSE, 0);
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(todo_hide_completed_checkbox),
				hide_completed);
   gtk_signal_connect_object(GTK_OBJECT(todo_hide_completed_checkbox), 
			     "clicked", GTK_SIGNAL_FUNC(cb_hide_completed),
			     NULL);

   
   /*Put the todo list window up */
   scrolled_window = gtk_scrolled_window_new(NULL, NULL);
   /*gtk_widget_set_usize(GTK_WIDGET(scrolled_window), 200, 0); */
   gtk_container_set_border_width(GTK_CONTAINER(scrolled_window), 0);
   gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
   gtk_box_pack_start(GTK_BOX(vbox1), scrolled_window, TRUE, TRUE, 0);

   clist = gtk_clist_new(5);
   gtk_signal_connect(GTK_OBJECT(clist), "select_row",
		      GTK_SIGNAL_FUNC(cb_clist_selection),
		      todo_text);
   gtk_clist_set_shadow_type(GTK_CLIST(clist), SHADOW);
   gtk_clist_set_selection_mode(GTK_CLIST(clist), GTK_SELECTION_BROWSE);
   gtk_clist_set_column_width(GTK_CLIST(clist), 0, 12);
   gtk_clist_set_column_width(GTK_CLIST(clist), 1, 8);
   gtk_clist_set_column_width(GTK_CLIST(clist), 2, 220);
   gtk_clist_set_column_width(GTK_CLIST(clist), 3, 11);
   gtk_clist_set_column_width(GTK_CLIST(clist), 4, 20);
   /*   gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW */
   /*					 (scrolled_window), clist); */
   gtk_container_add(GTK_CONTAINER(scrolled_window), GTK_WIDGET(clist));
   
   /* */
   /* The right hand part of the main window follows: */
   /* */
   hbox_temp = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, FALSE, FALSE, 0);

   
   /*Add record modification buttons on right side */
   button = gtk_button_new_with_label(_("Add It"));
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_add_new_record), 
		      GINT_TO_POINTER(NEW_FLAG));
   gtk_box_pack_start(GTK_BOX(hbox_temp), button, TRUE, TRUE, 0);
   
   todo_modify_button = gtk_button_new_with_label(_("Apply Changes"));
   gtk_signal_connect(GTK_OBJECT(todo_modify_button), "clicked",
		      GTK_SIGNAL_FUNC(cb_add_new_record),
		      GINT_TO_POINTER(MODIFY_FLAG));
   gtk_box_pack_start(GTK_BOX(hbox_temp), todo_modify_button, TRUE, TRUE, 0);
   
   button = gtk_button_new_with_label(_("New"));
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_add_new_record), 
		      GINT_TO_POINTER(CLEAR_FLAG));
   gtk_box_pack_start(GTK_BOX(hbox_temp), button, TRUE, TRUE, 0);


   /*Separator */
   separator = gtk_hseparator_new();
   gtk_box_pack_start(GTK_BOX(vbox2), separator, FALSE, FALSE, 5);


   /*The completed check box */
   todo_completed_checkbox = gtk_check_button_new_with_label(_("Completed"));
   gtk_box_pack_start(GTK_BOX(vbox2), todo_completed_checkbox, FALSE, FALSE, 0);


   /*Priority Radio Buttons */
   hbox_temp = gtk_hbox_new (FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, FALSE, FALSE, 0);

   label = gtk_label_new(_("Priority: "));
   gtk_box_pack_start(GTK_BOX(hbox_temp), label, FALSE, FALSE, 0);

   group = NULL;
   for (i=0; i<5; i++) {
      sprintf(str,"%d",i+1);
      radio_button_todo[i] = gtk_radio_button_new_with_label(group, str);
      group = gtk_radio_button_group(GTK_RADIO_BUTTON(radio_button_todo[i]));
      /*gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (radio_button_todo), TRUE); */
      gtk_box_pack_start(GTK_BOX(hbox_temp),
			 radio_button_todo[i], FALSE, FALSE, 0);
      /* gtk_widget_show(radio_button_todo[i]);*/
   }
   gtk_widget_set_usize(hbox_temp, 10, 0);

   
   /*Begin spinners for due date */
   /*Boxes */
   hbox_temp = gtk_hbox_new(FALSE, 0);
   date_due_hbox = gtk_hbox_new(FALSE, 0);
   /* gtk_widget_set_usize(date_due_hbox, 100, 0); */
   vbox_temp1 = gtk_vbox_new(FALSE, 0);
   vbox_temp2 = gtk_vbox_new(FALSE, 0);
   vbox_temp3 = gtk_vbox_new(FALSE, 0);
   vbox_temp4 = gtk_vbox_new(FALSE, 0);

   gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, FALSE, FALSE, 0);

   /* Due date stuff */
   /*The No due date check box */
   todo_no_due_date_checkbox = gtk_check_button_new_with_label(_("No Due Date"));
   gtk_signal_connect(GTK_OBJECT(todo_no_due_date_checkbox), "clicked",
		      GTK_SIGNAL_FUNC(cb_check_button_no_due_date), NULL);

   align = gtk_alignment_new(0.9, 0.9, 0, 0);
   gtk_box_pack_start(GTK_BOX(hbox_temp), align, FALSE, FALSE, 0);
   gtk_container_add(GTK_CONTAINER(align), todo_no_due_date_checkbox);
   
   
   gtk_box_pack_start(GTK_BOX(hbox_temp), date_due_hbox, FALSE, FALSE, 0);

   gtk_box_pack_start(GTK_BOX(date_due_hbox), vbox_temp4, FALSE, FALSE, 0);

   label = gtk_label_new(_("Date Due:"));
   gtk_misc_set_alignment(GTK_MISC(label), 0.5, 0.8);
   gtk_box_pack_start(GTK_BOX(vbox_temp4), label, TRUE, TRUE, 0);
   /*Put the date in the order of user preference */
   dmy_order = get_pref_dmy_order();
   switch (dmy_order) {
    case PREF_DMY:
      gtk_box_pack_start(GTK_BOX(date_due_hbox), vbox_temp2, FALSE, FALSE, 0);/*day */
      gtk_box_pack_start(GTK_BOX(date_due_hbox), vbox_temp1, FALSE, FALSE, 0);/*mon */
      gtk_box_pack_start(GTK_BOX(date_due_hbox), vbox_temp3, FALSE, FALSE, 0);/*year */
      break;
    case PREF_YMD:
      gtk_box_pack_start(GTK_BOX(date_due_hbox), vbox_temp3, FALSE, FALSE, 0);/*year */
      gtk_box_pack_start(GTK_BOX(date_due_hbox), vbox_temp1, FALSE, FALSE, 0);/*mon */
      gtk_box_pack_start(GTK_BOX(date_due_hbox), vbox_temp2, FALSE, FALSE, 0);/*day */
      break;
    default:
      gtk_box_pack_start(GTK_BOX(date_due_hbox), vbox_temp1, FALSE, FALSE, 0);/*mon */
      gtk_box_pack_start(GTK_BOX(date_due_hbox), vbox_temp2, FALSE, FALSE, 0);/*day */
      gtk_box_pack_start(GTK_BOX(date_due_hbox), vbox_temp3, FALSE, FALSE, 0);/*year */
   }

   /*Labels */
   label = gtk_label_new(_("Month:"));
   gtk_box_pack_start(GTK_BOX(vbox_temp1), label, FALSE, FALSE, 0);
   label = gtk_label_new(_("Day:"));
   gtk_box_pack_start(GTK_BOX(vbox_temp2), label, FALSE, FALSE, 0);
   label = gtk_label_new(_("Year:"));
   gtk_box_pack_start(GTK_BOX(vbox_temp3), label, FALSE, FALSE, 0);

   /*month */
   todo_adj_due_mon = (GtkAdjustment *)gtk_adjustment_new
     (now->tm_mon+1, 1.0, 12.0, 1.0, 5.0, 0.0);
   todo_spinner_due_mon = gtk_spin_button_new(todo_adj_due_mon, 0, 0);
   gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(todo_spinner_due_mon), FALSE);
   gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(todo_spinner_due_mon), TRUE);
   gtk_spin_button_set_shadow_type(GTK_SPIN_BUTTON(todo_spinner_due_mon),
				   SHADOW);
   gtk_box_pack_start(GTK_BOX(vbox_temp1),
		      todo_spinner_due_mon, FALSE, TRUE, 0);
   /*Day */
   todo_adj_due_day = (GtkAdjustment *)gtk_adjustment_new
     (now->tm_mday, 1.0, 31.0, 1.0, 5.0, 0.0);
   todo_spinner_due_day = gtk_spin_button_new(todo_adj_due_day, 0, 0);
   gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(todo_spinner_due_day), FALSE);
   gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(todo_spinner_due_day), TRUE);
   gtk_spin_button_set_shadow_type(GTK_SPIN_BUTTON(todo_spinner_due_day),
				   SHADOW);
   gtk_box_pack_start(GTK_BOX(vbox_temp2),
		      todo_spinner_due_day, FALSE, TRUE, 0);
   /*Year */
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

   /*end spinners */

   
   /*Put the right-hand category menu up */
   make_category_menu2();
   gtk_box_pack_start(GTK_BOX(vbox2), todo_cat_menu2, FALSE, FALSE, 5);
   

   /*The Description text box on the right side */
   hbox_temp = gtk_hbox_new (FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, FALSE, FALSE, 0);

   hbox_temp = gtk_hbox_new (FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, TRUE, TRUE, 0);

   todo_text = gtk_text_new(NULL, NULL);
   gtk_text_set_editable(GTK_TEXT(todo_text), TRUE);
   gtk_text_set_word_wrap(GTK_TEXT(todo_text), TRUE);
   vscrollbar = gtk_vscrollbar_new(GTK_TEXT(todo_text)->vadj);
   gtk_box_pack_start(GTK_BOX(hbox_temp), todo_text, TRUE, TRUE, 0);
   gtk_box_pack_start(GTK_BOX(hbox_temp), vscrollbar, FALSE, FALSE, 0);

   
   /*The Note text box on the right side */
   hbox_temp = gtk_hbox_new (FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, FALSE, FALSE, 0);

   label = gtk_label_new("Note");
   gtk_box_pack_start(GTK_BOX(hbox_temp), label, FALSE, FALSE, 0);

   hbox_temp = gtk_hbox_new (FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, TRUE, TRUE, 0);

   todo_text_note = gtk_text_new(NULL, NULL);
   gtk_text_set_editable(GTK_TEXT(todo_text_note), TRUE);
   gtk_text_set_word_wrap(GTK_TEXT(todo_text_note), TRUE);
   vscrollbar = gtk_vscrollbar_new(GTK_TEXT(todo_text_note)->vadj);
   gtk_box_pack_start(GTK_BOX(hbox_temp), todo_text_note, TRUE, TRUE, 0);
   gtk_box_pack_start(GTK_BOX(hbox_temp), vscrollbar, FALSE, FALSE, 0);

   /* Capture the TAB key to change focus with it */
   gtk_signal_connect(GTK_OBJECT(todo_text), "key_press_event",
		      GTK_SIGNAL_FUNC(cb_key_pressed), todo_text_note);
   
   gtk_widget_set_usize(GTK_WIDGET(todo_text), 10, 10);
   gtk_widget_set_usize(GTK_WIDGET(todo_text_note), 10, 10);
   
   gtk_widget_show_all(vbox);
   gtk_widget_show_all(hbox);
   
   todo_refresh();
   todo_find();

   return 0;
}

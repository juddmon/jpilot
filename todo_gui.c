/* todo_gui.c
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
#include <gdk/gdkkeysyms.h>
#include <gdk/gdk.h>
#include <time.h>
#include <stdlib.h>
#include <pi-dlp.h>
#include "utils.h"
#include "todo.h"
#include "log.h"
#include "prefs.h"
#include "password.h"
#include "print.h"


#define NUM_TODO_PRIORITIES 5
#define NUM_TODO_CAT_ITEMS 16
#define CONNECT_SIGNALS 400
#define DISCONNECT_SIGNALS 401

extern GtkTooltips *glob_tooltips;

static GtkWidget *clist;
static GtkWidget *todo_text, *todo_text_note;
static GtkWidget *todo_completed_checkbox;
static GtkWidget *private_checkbox;
static struct tm due_date;
static GtkWidget *due_date_button;
static GtkWidget *todo_no_due_date_checkbox;
static GtkWidget *radio_button_todo[NUM_TODO_PRIORITIES];
/* We need an extra one for the ALL category */
static GtkWidget *todo_cat_menu_item1[NUM_TODO_CAT_ITEMS+1];
static GtkWidget *todo_cat_menu_item2[NUM_TODO_CAT_ITEMS];
static GtkWidget *new_record_button;
static GtkWidget *apply_record_button;
static GtkWidget *add_record_button;
static GtkWidget *category_menu1;
static GtkWidget *category_menu2;
static GtkWidget *scrolled_window;
static GtkWidget *pane;

static struct sorted_cats sort_l[NUM_TODO_CAT_ITEMS];

struct ToDoAppInfo todo_app_info;
int todo_category=CATEGORY_ALL;
static int clist_row_selected;
static int record_changed;

void update_todo_screen();
int todo_clear_details();
int todo_clist_redraw();
static void connect_changed_signals(int con_or_dis);
static int todo_find();


static void init()
{
   time_t ltime;
   struct tm *now;

   clist_row_selected=0;

   record_changed=CLEAR_FLAG;
   time(&ltime);
   now = localtime(&ltime);

   memcpy(&due_date, now, sizeof(struct tm));
}

static void update_due_button(GtkWidget *button, struct tm *t)
{
   long ivalue;
   const char *short_date;
   char str[255];

   if (t) {
      get_pref(PREF_SHORTDATE, &ivalue, &short_date);
      strftime(str, 250, short_date, t);

      gtk_label_set_text(GTK_LABEL(GTK_BIN(button)->child), str);
   } else {
      gtk_label_set_text(GTK_LABEL(GTK_BIN(button)->child), _("No Date"));
   }
}

static void cb_cal_dialog(GtkWidget *widget,
			  gpointer   data)
{
   long fdow;
   int r = 0;
   struct tm *Pt;
   GtkWidget *Pcheck_button;
   GtkWidget *Pbutton;

   Pcheck_button = todo_no_due_date_checkbox;
   Pt = &due_date;
   Pbutton = due_date_button;

   get_pref(PREF_FDOW, &fdow, NULL);

   r = cal_dialog(_("Due Date"), fdow,
		  &(Pt->tm_mon),
		  &(Pt->tm_mday),
		  &(Pt->tm_year));

   if (r==CAL_DONE) {
      if (Pcheck_button) {
	 gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(Pcheck_button), FALSE);
      }
      if (Pbutton) {
	 update_due_button(Pbutton, Pt);
      }
   }
}


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
      get_todos2(&todo_list, SORT_ASCENDING, 2, 2, 2, 2, todo_category);
   }
   if (this_many==3) {
      get_todos2(&todo_list, SORT_ASCENDING, 2, 2, 2, 2, CATEGORY_ALL);
   }

   print_todos(todo_list);

   if ((this_many==2) || (this_many==3)) {
      free_ToDoList(&todo_list);
   }

   return 0;
}

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

      for (i=0; i<NUM_TODO_CAT_ITEMS; i++) {
	 if (todo_cat_menu_item2[i]) {
	    gtk_signal_connect(GTK_OBJECT(todo_cat_menu_item2[i]), "toggled",
			       GTK_SIGNAL_FUNC(cb_record_changed), NULL);
	 }
      }
      for (i=0; i<NUM_TODO_PRIORITIES; i++) {
	 if (radio_button_todo[i]) {
	    gtk_signal_connect(GTK_OBJECT(radio_button_todo[i]), "toggled",
			       GTK_SIGNAL_FUNC(cb_record_changed), NULL);
	 }
      }
      gtk_signal_connect(GTK_OBJECT(todo_text), "changed",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_connect(GTK_OBJECT(todo_text_note), "changed",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_connect(GTK_OBJECT(todo_completed_checkbox), "toggled",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_connect(GTK_OBJECT(private_checkbox), "toggled",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_connect(GTK_OBJECT(todo_no_due_date_checkbox), "toggled",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_connect(GTK_OBJECT(due_date_button), "pressed",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);
   }
   
   /* DISCONNECT */
   if ((con_or_dis==DISCONNECT_SIGNALS) && (connected)) {
      connected=0;

      for (i=0; i<NUM_TODO_CAT_ITEMS; i++) {
	 if (todo_cat_menu_item2[i]) {
	    gtk_signal_disconnect_by_func(GTK_OBJECT(todo_cat_menu_item2[i]),
					  GTK_SIGNAL_FUNC(cb_record_changed), NULL);     
	 }
      }
      for (i=0; i<NUM_TODO_PRIORITIES; i++) {
	 gtk_signal_disconnect_by_func(GTK_OBJECT(radio_button_todo[i]),
				       GTK_SIGNAL_FUNC(cb_record_changed), NULL);     
      }
      gtk_signal_disconnect_by_func(GTK_OBJECT(todo_text),
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_disconnect_by_func(GTK_OBJECT(todo_text_note),
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_disconnect_by_func(GTK_OBJECT(todo_completed_checkbox),
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_disconnect_by_func(GTK_OBJECT(private_checkbox),
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_disconnect_by_func(GTK_OBJECT(todo_no_due_date_checkbox),
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_disconnect_by_func(GTK_OBJECT(due_date_button),
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
   }
}


static int find_sorted_cat(int cat)
{
   int i;
   for (i=0; i< NUM_TODO_CAT_ITEMS; i++) {
      if (sort_l[i].cat_num==cat) {
 	 return i;
      }
   }
   return -1;
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
      update_due_button(due_date_button, NULL);
   } else {
      update_due_button(due_date_button, &due_date);
   }   
}

void cb_hide_completed(GtkWidget *widget,
		       gpointer   data)
{
   set_pref(PREF_HIDE_COMPLETED,
	    GTK_TOGGLE_BUTTON(widget)->active);
   todo_clear_details();
   todo_clist_redraw();
}

int todo_clear_details()
{
   time_t ltime;
   struct tm *now;
   int new_cat;
   int sorted_position;
   
   time(&ltime);
   now = localtime(&ltime);

   /* Need to disconnect these signals first */
   set_new_button_to(NEW_FLAG);
   connect_changed_signals(DISCONNECT_SIGNALS);
   
   update_due_button(due_date_button, NULL);

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

   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(private_checkbox), FALSE);

   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(todo_no_due_date_checkbox),
				TRUE);

   memcpy(&due_date, now, sizeof(now));

   gtk_text_thaw(GTK_TEXT(todo_text));
   gtk_text_thaw(GTK_TEXT(todo_text_note));

   if (todo_category==CATEGORY_ALL) {
      new_cat = 0;
   } else {
      new_cat = todo_category;
   }
   sorted_position = find_sorted_cat(new_cat);
   if (sorted_position<0) {
      jpilot_logf(LOG_WARN, "Category is not legal\n");
   } else {
      gtk_check_menu_item_set_active
	(GTK_CHECK_MENU_ITEM(todo_cat_menu_item2[sorted_position]), TRUE);
      gtk_option_menu_set_history(GTK_OPTION_MENU(category_menu2), sorted_position);
   }
   
   set_new_button_to(CLEAR_FLAG);
   return 0;
}

int todo_get_details(struct ToDo *new_todo, unsigned char *attrib)
{
   int i;
   
   new_todo->indefinite = (GTK_TOGGLE_BUTTON(todo_no_due_date_checkbox)->active);
   if (!new_todo->indefinite) {
      new_todo->due.tm_mon = due_date.tm_mon;
      new_todo->due.tm_mday = due_date.tm_mday;
      new_todo->due.tm_year = due_date.tm_year;
   }
   new_todo->priority=1;
   for (i=0; i<NUM_TODO_PRIORITIES; i++) {
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
	    *attrib = sort_l[i].cat_num;
	    break;
	 }
      }
   }
   if (GTK_TOGGLE_BUTTON(private_checkbox)->active) {
      *attrib |= dlpRecAttrSecret;
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
      connect_changed_signals(DISCONNECT_SIGNALS);
      set_new_button_to(NEW_FLAG);
      gtk_widget_grab_focus(GTK_WIDGET(todo_text));
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

   set_new_button_to(CLEAR_FLAG);

   pc_todo_write(&new_todo, NEW_PC_REC, attrib, &unique_id);
   free_ToDo(&new_todo);
   if (flag==MODIFY_FLAG) {
      cb_delete_todo(NULL, data);
   } else {
      todo_clist_redraw();
   }
   glob_find_id = unique_id;
   todo_find();

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
   int sorted_position;

   time_t ltime;
   struct tm *now;
   
   time(&ltime);
   now = localtime(&ltime);

   clist_row_selected=row;

   mtodo = gtk_clist_get_row_data(GTK_CLIST(clist), row);

   set_new_button_to(CLEAR_FLAG);
   
   connect_changed_signals(DISCONNECT_SIGNALS);
   
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

   index = mtodo->attrib & 0x0F;
   sorted_position = find_sorted_cat(index);
   if (todo_cat_menu_item2[sorted_position]==NULL) {
      /* Illegal category */
      jpilot_logf(LOG_DEBUG, "Category is not legal\n");
      index = sorted_position = 0;
      sorted_position = find_sorted_cat(index);
   }
   /* We need to count how many items down in the list this is */
   for (i=sorted_position, count=0; i>=0; i--) {
      if (todo_cat_menu_item2[i]) {
	 count++;
      }
   }
   count--;
   
   if (sorted_position<0) {
      jpilot_logf(LOG_WARN, "Category is not legal\n");
   } else {
      gtk_check_menu_item_set_active
	(GTK_CHECK_MENU_ITEM(todo_cat_menu_item2[sorted_position]), TRUE);
   }
   gtk_option_menu_set_history(GTK_OPTION_MENU(category_menu2), count);
   
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

   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(private_checkbox),
				mtodo->attrib & dlpRecAttrSecret);
     
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(todo_no_due_date_checkbox),
				todo->indefinite);
   if (!todo->indefinite) {
      update_due_button(due_date_button, &(todo->due));
      due_date.tm_mon=todo->due.tm_mon;
      due_date.tm_mday=todo->due.tm_mday;
      due_date.tm_year=todo->due.tm_year;
   } else {
      update_due_button(due_date_button, NULL);
      due_date.tm_mon=now->tm_mon;
      due_date.tm_mday=now->tm_mday;
      due_date.tm_year=now->tm_year;
   }

   gtk_text_thaw(GTK_TEXT(todo_text));
   gtk_text_thaw(GTK_TEXT(todo_text_note));

   /*If they have clicked on the checkmark box then do a modify */
   if (column==0) {
      todo->complete = !(todo->complete);
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(todo_completed_checkbox), todo->complete);
      gtk_signal_emit_by_name(GTK_OBJECT(apply_record_button), "clicked");
   }
   connect_changed_signals(CONNECT_SIGNALS);
}


void update_todo_screen()
{
   int num_entries, entries_shown, i;
   int row_count;
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
   long ivalue;
   const char *svalue;
   long hide_completed;
   int show_priv;
   
   row_count=((GtkCList *)clist)->rows;
   
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

   /* Need to get the private ones back for the hints calculation */
   num_entries = get_todos2(&todo_list, SORT_ASCENDING, 2, 2, 1, 1, CATEGORY_ALL);

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
   show_priv = show_privates(GET_PRIVATES, NULL);
   for (temp_todo = todo_list, i=0; temp_todo; temp_todo=temp_todo->next) {
      if ( ((temp_todo->mtodo.attrib & 0x0F) != todo_category) &&
	  todo_category != CATEGORY_ALL) {
	 continue;
      }
      /*Hide the completed records if need be */
      if (hide_completed && temp_todo->mtodo.todo.complete) {
	 continue;
      }

      if ((show_priv != SHOW_PRIVATES) && 
	  (temp_todo->mtodo.attrib & dlpRecAttrSecret)) {
	 continue;
      }

      if (entries_shown+1>row_count) {
	 gtk_clist_append(GTK_CLIST(clist), empty_line);
      }
      gtk_clist_set_text(GTK_CLIST(clist), entries_shown, 2, temp_todo->mtodo.todo.description);
      sprintf(str, "%d", temp_todo->mtodo.todo.priority);
      gtk_clist_set_text(GTK_CLIST(clist), entries_shown, 1, str);

      if (!temp_todo->mtodo.todo.indefinite) {
	  get_pref(PREF_SHORTDATE, &ivalue, &svalue);
	  get_pref_possibility(PREF_SHORTDATE,ivalue,str);
	  strftime(str, 50, svalue, &(temp_todo->mtodo.todo.due));
      }
      else {
	  sprintf(str, _("No date"));
      }
      gtk_clist_set_text(GTK_CLIST(clist), entries_shown, 4, str);

      gtk_clist_set_row_data(GTK_CLIST(clist), entries_shown, &(temp_todo->mtodo));

      switch (temp_todo->mtodo.rt) {
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
      
      get_pixmaps(clist, PIXMAP_NOTE, &pixmap_note, &mask_note);
      get_pixmaps(clist, PIXMAP_BOX_CHECK, &pixmap_check, &mask_check);
      get_pixmaps(clist, PIXMAP_BOX_CHECKED, &pixmap_checked,&mask_checked);
      
      if (temp_todo->mtodo.todo.note[0]) {
	 /*Put a note pixmap up */
	 gtk_clist_set_pixmap(GTK_CLIST(clist), entries_shown, 3, pixmap_note, mask_note);
      } else {
	 gtk_clist_set_text(GTK_CLIST(clist), entries_shown, 3, "");
      }
      if (temp_todo->mtodo.todo.complete) {
	 /*Put a check or checked pixmap up */
	 gtk_clist_set_pixmap(GTK_CLIST(clist), entries_shown, 0, pixmap_checked, mask_checked);
      } else {
	 gtk_clist_set_pixmap(GTK_CLIST(clist), entries_shown, 0, pixmap_check, mask_check);
      }
      entries_shown++;
   }

   jpilot_logf(LOG_DEBUG, "entries_shown=%d\n",entries_shown);

   /*If there is an item in the list, select the first one */
   if (entries_shown>0) {
      gtk_clist_select_row(GTK_CLIST(clist), 0, 1);
      /*cb_clist_selection(clist, 0, 0, (GdkEventButton *)455, ""); */
   }

   for (i=row_count-1; i>=entries_shown; i--) {
      gtk_clist_remove(GTK_CLIST(clist), i);
   }

   gtk_clist_thaw(GTK_CLIST(clist));

   sprintf(str, _("%d of %d records"), entries_shown, num_entries);
   gtk_tooltips_set_tip(glob_tooltips, category_menu1, str, NULL);   

   set_new_button_to(CLEAR_FLAG);
}

/*todo combine the next 2 functions */
static int make_category_menu(GtkWidget **category_menu,
			      int include_all)
{
   GtkWidget *menu;
   GSList    *group;
   int i;
   int offset;
   GtkWidget **todo_cat_menu_item;

   if (include_all) {
      todo_cat_menu_item = todo_cat_menu_item1;
   } else {
      todo_cat_menu_item = todo_cat_menu_item2;
   }
   
   *category_menu = gtk_option_menu_new();
   
   menu = gtk_menu_new();
   group = NULL;
   
   offset=0;
   if (include_all) {
      todo_cat_menu_item[0] = gtk_radio_menu_item_new_with_label(group, _("All"));
      gtk_signal_connect(GTK_OBJECT(todo_cat_menu_item[0]), "activate",
			 cb_todo_category, GINT_TO_POINTER(CATEGORY_ALL));
      group = gtk_radio_menu_item_group(GTK_RADIO_MENU_ITEM(todo_cat_menu_item[0]));
      gtk_menu_append(GTK_MENU(menu), todo_cat_menu_item[0]);
      gtk_widget_show(todo_cat_menu_item[0]);
      offset=1;
   }
   for (i=0; i<NUM_TODO_CAT_ITEMS; i++) {
      if (sort_l[i].Pcat[0]) {
	 todo_cat_menu_item[i+offset] = gtk_radio_menu_item_new_with_label(
	    group, sort_l[i].Pcat);
	 if (include_all) {
	    gtk_signal_connect(GTK_OBJECT(todo_cat_menu_item[i+offset]), "activate",
	       cb_todo_category, GINT_TO_POINTER(sort_l[i].cat_num));
	 }
	 group = gtk_radio_menu_item_group(GTK_RADIO_MENU_ITEM(todo_cat_menu_item[i+offset]));
	 gtk_menu_append(GTK_MENU(menu), todo_cat_menu_item[i+offset]);
	 gtk_widget_show(todo_cat_menu_item[i+offset]);
      }
   }
   gtk_option_menu_set_menu(GTK_OPTION_MENU(*category_menu), menu);
   
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
	 if (!gtk_clist_row_is_visible(GTK_CLIST(clist), found_at)) {
	    move_scrolled_window_hack(scrolled_window,
				      (float)found_at/(float)total_count);
	 }
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

/* This redraws the clist and goes back to the same line number */
int todo_clist_redraw()
{
   int line_num;

   line_num = clist_row_selected;

   update_todo_screen();
   
   /* Don't select column 0 as this is the checkbox */
   gtk_clist_select_row(GTK_CLIST(clist), line_num, 1);
   
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
      index=find_sorted_cat(todo_category)+1;
   }
   update_todo_screen();
   if (index<0) {   
      jpilot_logf(LOG_WARN, "Category not legal\n");
   } else {
      gtk_option_menu_set_history(GTK_OPTION_MENU(category_menu1), index);
      gtk_check_menu_item_set_active
	(GTK_CHECK_MENU_ITEM(todo_cat_menu_item1[index]), TRUE);
   }
   todo_find();
   return 0;
}

int todo_gui_cleanup()
{
   connect_changed_signals(DISCONNECT_SIGNALS);
   set_pref(PREF_TODO_PANE, GTK_PANED(pane)->handle_xpos);
   return 0;
}

int todo_gui(GtkWidget *vbox, GtkWidget *hbox)
{
   extern GtkWidget *glob_date_label;
   extern int glob_date_timer_tag;
   GtkWidget *vbox1, *vbox2;
   GtkWidget *hbox_temp;
   GtkWidget *separator;
   GtkWidget *label;
   GtkWidget *vscrollbar;
   GtkWidget *button;
   GtkWidget *checkbox;
   time_t ltime;
   struct tm *now;
#define MAX_STR 100
   char str[MAX_STR];
   int i;
   GSList    *group;
   long hide_completed;
   long ivalue;
   const char *svalue;
   char *titles[]={"","","","",""};

   init();

   /* Do some initialization */
   for (i=0; i<NUM_TODO_CAT_ITEMS; i++) {
      todo_cat_menu_item2[i] = NULL;
   }
   
   get_todo_app_info(&todo_app_info);

   for (i=0; i<NUM_TODO_CAT_ITEMS; i++) {
      sort_l[i].Pcat = todo_app_info.category.name[i];
      sort_l[i].cat_num = i;
   }
   qsort(sort_l, NUM_TODO_CAT_ITEMS, sizeof(struct sorted_cats), cat_compare);
#ifdef JPILOT_DEBUG
   for (i=0; i<NUM_TODO_CAT_ITEMS; i++) {
      printf("cat %d %s\n", sort_l[i].cat_num, sort_l[i].Pcat);
   }
#endif

   if ((todo_category != CATEGORY_ALL) && (todo_app_info.category.name[todo_category][0]=='\0')) {
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
   make_category_menu(&category_menu1, TRUE);

   gtk_box_pack_start(GTK_BOX(vbox1), category_menu1, FALSE, FALSE, 0);
   
   
   get_pref(PREF_HIDE_COMPLETED, &hide_completed, &svalue);

   /*The hide completed check box */
   checkbox = gtk_check_button_new_with_label(_("Hide Completed ToDos"));
   gtk_box_pack_start(GTK_BOX(vbox1), checkbox, FALSE, FALSE, 0);
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox), hide_completed);
   gtk_signal_connect(GTK_OBJECT(checkbox), "clicked",
		      GTK_SIGNAL_FUNC(cb_hide_completed), NULL);

   
   /*Put the todo list window up */
   scrolled_window = gtk_scrolled_window_new(NULL, NULL);
   /*gtk_widget_set_usize(GTK_WIDGET(scrolled_window), 200, 0); */
   gtk_container_set_border_width(GTK_CONTAINER(scrolled_window), 0);
   gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
   gtk_box_pack_start(GTK_BOX(vbox1), scrolled_window, TRUE, TRUE, 0);

   clist = gtk_clist_new_with_titles(5, titles);
   gtk_clist_set_column_title(GTK_CLIST(clist), 2, _("Task"));
   gtk_clist_set_column_title(GTK_CLIST(clist), 4, _("Due"));
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

   
   /* Add record modification buttons on right side */
   /* Delete button */
   button = gtk_button_new_with_label(_("Delete"));
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_delete_todo),
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


   /*The completed check box */
   todo_completed_checkbox = gtk_check_button_new_with_label(_("Completed"));
   gtk_box_pack_start(GTK_BOX(vbox2), todo_completed_checkbox, FALSE, FALSE, 0);


   /*Priority Radio Buttons */
   hbox_temp = gtk_hbox_new (FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, FALSE, FALSE, 0);

   label = gtk_label_new(_("Priority: "));
   gtk_box_pack_start(GTK_BOX(hbox_temp), label, FALSE, FALSE, 0);

   group = NULL;
   for (i=0; i<NUM_TODO_PRIORITIES; i++) {
      sprintf(str,"%d",i+1);
      radio_button_todo[i] = gtk_radio_button_new_with_label(group, str);
      group = gtk_radio_button_group(GTK_RADIO_BUTTON(radio_button_todo[i]));
      /*gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (radio_button_todo), TRUE); */
      gtk_box_pack_start(GTK_BOX(hbox_temp),
			 radio_button_todo[i], FALSE, FALSE, 0);
      /* gtk_widget_show(radio_button_todo[i]);*/
   }
   gtk_widget_set_usize(hbox_temp, 10, 0);

   
   /* Due date stuff */
   hbox_temp = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, FALSE, FALSE, 0);

   label = gtk_label_new(_("Date Due:"));
   gtk_box_pack_start(GTK_BOX(hbox_temp), label, FALSE, FALSE, 0);

   /* Due date button */
   due_date_button = gtk_button_new_with_label("");
   gtk_box_pack_start(GTK_BOX(hbox_temp), due_date_button, FALSE, FALSE, 5);
   gtk_signal_connect(GTK_OBJECT(due_date_button), "clicked",
		      GTK_SIGNAL_FUNC(cb_cal_dialog), NULL);


   /*The No due date check box */
   todo_no_due_date_checkbox = gtk_check_button_new_with_label(_("No Date"));
   gtk_signal_connect(GTK_OBJECT(todo_no_due_date_checkbox), "clicked",
		      GTK_SIGNAL_FUNC(cb_check_button_no_due_date), NULL);
   gtk_box_pack_start(GTK_BOX(hbox_temp), todo_no_due_date_checkbox, FALSE, FALSE, 0);
   
   
   /*Private check box */
   hbox_temp = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, FALSE, FALSE, 0);
   private_checkbox = gtk_check_button_new_with_label(_("Private"));
   gtk_box_pack_end(GTK_BOX(hbox_temp), private_checkbox, FALSE, FALSE, 0);

   /*Put the right-hand category menu up */
   make_category_menu(&category_menu2, FALSE);

   gtk_box_pack_start(GTK_BOX(hbox_temp), category_menu2, TRUE, TRUE, 0);
   

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
   
   gtk_widget_hide(add_record_button);
   gtk_widget_hide(apply_record_button);

   todo_clear_details();

   todo_refresh();
   todo_find();

   return 0;
}

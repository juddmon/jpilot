/* todo_gui.c
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
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdk.h>
#include <time.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>
#include <pi-dlp.h>
#include "utils.h"
#include "todo.h"
#include "log.h"
#include "prefs.h"
#include "password.h"
#include "print.h"
#include "export.h"

#define TODO_MAX_COLUMN_LEN 80
#define MAX_RADIO_BUTTON_LEN 100

#define NUM_TODO_PRIORITIES 5
#define NUM_TODO_CAT_ITEMS 16
#define CONNECT_SIGNALS 400
#define DISCONNECT_SIGNALS 401

extern GtkTooltips *glob_tooltips;

static GtkWidget *clist;
static GtkWidget *todo_text, *todo_text_note;
#ifdef ENABLE_GTK2
static GObject   *todo_text_buffer, *todo_text_note_buffer;
#endif
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
static GtkWidget *delete_record_button;
static GtkWidget *undelete_record_button;
static GtkWidget *copy_record_button;
static GtkWidget *category_menu1;
static GtkWidget *category_menu2;
static GtkWidget *pane;

static ToDoList *glob_todo_list=NULL;
static ToDoList *export_todo_list=NULL;

static struct sorted_cats sort_l[NUM_TODO_CAT_ITEMS];

struct ToDoAppInfo todo_app_info;
int todo_category=CATEGORY_ALL;
static int clist_col_selected;
static int clist_row_selected;
static int record_changed;
static int clist_hack;

void todo_update_clist(GtkWidget *clist, GtkWidget *tooltip_widget,
		       ToDoList **todo_list, int category, int main);
int todo_clear_details();
int todo_clist_redraw();
static void connect_changed_signals(int con_or_dis);
static int todo_find();
static void cb_add_new_record(GtkWidget *widget, gpointer data);
int todo_get_details(struct ToDo *new_todo, unsigned char *attrib);


static void init()
{
   time_t ltime;
   struct tm *now;
   long ivalue;

   clist_col_selected=1;
   clist_row_selected=0;

   record_changed=CLEAR_FLAG;
   time(&ltime);
   now = localtime(&ltime);

   memcpy(&due_date, now, sizeof(struct tm));

   get_pref(PREF_TODO_DAYS_TILL_DUE, &ivalue, NULL);
   add_days_to_date(&due_date, ivalue);
}

static void update_due_button(GtkWidget *button, struct tm *t)
{
   long ivalue;
   const char *short_date;
   char str[255];

   if (t) {
      get_pref(PREF_SHORTDATE, &ivalue, &short_date);
      strftime(str, sizeof(str), short_date, t);

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
   struct tm t;
   GtkWidget *Pcheck_button;
   GtkWidget *Pbutton;

   Pcheck_button = todo_no_due_date_checkbox;
   memcpy(&t, &due_date, sizeof(t));
   Pbutton = due_date_button;

   get_pref(PREF_FDOW, &fdow, NULL);

   r = cal_dialog(GTK_WINDOW(gtk_widget_get_toplevel(widget)), _("Due Date"), fdow,
		  &(t.tm_mon),
		  &(t.tm_mday),
		  &(t.tm_year));

   if (r==CAL_DONE) {
      mktime(&t);
      memcpy(&due_date, &t, sizeof(due_date));
      if (Pcheck_button) {
	 gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(Pcheck_button), FALSE);
	 /* The above call sets due_date forward by n days, so we correct it */
	 memcpy(&due_date, &t, sizeof(due_date));
	 update_due_button(Pbutton, &t);
      }
      if (Pbutton) {
	 update_due_button(Pbutton, &t);
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

   print_todos(todo_list, PN);

   if ((this_many==2) || (this_many==3)) {
      free_ToDoList(&todo_list);
   }

   return 0;
}

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
    case UNDELETE_FLAG:
      gtk_clist_set_selection_mode(GTK_CLIST(clist), GTK_SELECTION_BROWSE);
      clist_hack=FALSE;
      gtk_widget_hide(delete_record_button);
      gtk_widget_show(undelete_record_button);
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
      if (new_state != UNDELETE_FLAG)
      {
         gtk_widget_hide(new_record_button);
         gtk_widget_hide(delete_record_button);
      }
      else
      {
	 gtk_widget_hide(delete_record_button);
      }
      break;
    case UNDELETE_FLAG:
      if (new_state != UNDELETE_FLAG)
      {
	 gtk_widget_hide(undelete_record_button);
	 gtk_widget_show(delete_record_button);
      }
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
   else if (record_changed==UNDELETE_FLAG)
   {
      jp_logf(JP_LOG_INFO|JP_LOG_GUI, _("This record is deleted.\n"
	   "Undelete it or copy it to make changes.\n"));
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
#ifdef ENABLE_GTK2
      g_signal_connect(todo_text_buffer, "changed",
		       GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      g_signal_connect(todo_text_note_buffer, "changed",
		       GTK_SIGNAL_FUNC(cb_record_changed), NULL);
#else
      gtk_signal_connect(GTK_OBJECT(todo_text), "changed",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_connect(GTK_OBJECT(todo_text_note), "changed",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);
#endif

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
#ifdef ENABLE_GTK2
      g_signal_handlers_disconnect_by_func(todo_text_buffer,
					   GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      g_signal_handlers_disconnect_by_func(todo_text_note_buffer,
					   GTK_SIGNAL_FUNC(cb_record_changed), NULL);
#else
      gtk_signal_disconnect_by_func(GTK_OBJECT(todo_text),
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_disconnect_by_func(GTK_OBJECT(todo_text_note),
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
#endif

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


static int todo_to_text(struct ToDo *todo, char *text, int len)
{
   char yes[]="Yes";
   char no[]="No";
   char empty[]="";
   char *complete;
   char *description;
   char *note;
   char due[20];
   const char *short_date;

   if (todo->indefinite) {
      strcpy(due, "Never");
   } else {
      get_pref(PREF_SHORTDATE, NULL, &short_date);
      strftime(due, sizeof(due), short_date, &(todo->due));
   }
   complete=todo->complete ? yes : no;
   description=todo->description ? todo->description : empty;
   note=todo->note ? todo->note : empty;

   g_snprintf(text, len, "Due: %s\nPriority: %d\nComplete: %s\n\
Description: %s\nNote: %s\n", due, todo->priority, complete,
	      description, note);
   return 0;
}

/*
 * Start Import Code
 */
int todo_import_callback(GtkWidget *parent_window, const char *file_path, int type)
{
   FILE *in;
   char text[65536];
   char description[65536];
   char note[65536];
   struct ToDo new_todo;
   unsigned char attrib;
   int i, ret, index;
   int import_all;
   ToDoList *todolist;
   ToDoList *temp_todolist;
   struct CategoryAppInfo cai;
   char old_cat_name[32];
   int suggested_cat_num;
   int new_cat_num;
   int priv, indefinite, priority, completed;
   int year, month, day;

   todo_get_details(&new_todo, &attrib);
   free_ToDo(&new_todo);

   in=fopen(file_path, "r");
   if (!in) {
      jp_logf(JP_LOG_WARN, _("Unable to open file: %s\n"), file_path);
      return -1;
   }

   /* CSV */
   if (type==IMPORT_TYPE_CSV) {
      jp_logf(JP_LOG_DEBUG, "Todo import CSV [%s]\n", file_path);
      /* The first line is format, so we don't need it */
      fgets(text, sizeof(text), in);
      import_all=FALSE;
      while (1) {
	 /* Read the category field */
	 ret = read_csv_field(in, text, sizeof(text));
	 if (feof(in)) break;
#ifdef JPILOT_DEBUG
	 printf("category is [%s]\n", text);
#endif
	 strncpy(old_cat_name, text, 16);
	 old_cat_name[16]='\0';
	 attrib=0;
	 /* Figure out what the best category number is */
	 suggested_cat_num=0;
	 for (i=0; i<NUM_TODO_CAT_ITEMS; i++) {
	    if (todo_app_info.category.name[i][0]=='\0') continue;
	    if (!strcmp(todo_app_info.category.name[i], old_cat_name)) {
	       suggested_cat_num=i;
	       i=1000;
	       break;
	    }
	 }

	 /* Read the private field */
	 ret = read_csv_field(in, text, sizeof(text));
#ifdef JPILOT_DEBUG
	 printf("private is [%s]\n", text);
#endif
	 sscanf(text, "%d", &priv);

	 /* Read the indefinite field */
	 ret = read_csv_field(in, text, sizeof(text));
#ifdef JPILOT_DEBUG
	 printf("indefinite is [%s]\n", text);
#endif
	 sscanf(text, "%d", &indefinite);

	 /* Read the Due Date field */
	 ret = read_csv_field(in, text, sizeof(text));
#ifdef JPILOT_DEBUG
	 printf("due date is [%s]\n", text);
#endif
	 sscanf(text, "%d %d %d", &year, &month, &day);

	 /* Read the Priority field */
	 ret = read_csv_field(in, text, sizeof(text));
#ifdef JPILOT_DEBUG
	 printf("priority is [%s]\n", text);
#endif
	 sscanf(text, "%d", &priority);

	 /* Read the Completed field */
	 ret = read_csv_field(in, text, sizeof(text));
#ifdef JPILOT_DEBUG
	 printf("completed is [%s]\n", text);
#endif
	 sscanf(text, "%d", &completed);

	 /* Read the Description field */
	 ret = read_csv_field(in, description, sizeof(text));
#ifdef JPILOT_DEBUG
	 printf("todo description [%s]\n", description);
#endif

	 /* Read the Note field */
	 ret = read_csv_field(in, note, sizeof(text));
#ifdef JPILOT_DEBUG
	 printf("todo note [%s]\n", note);
#endif

	 new_todo.indefinite=indefinite;
	 memset(&(new_todo.due), 0, sizeof(new_todo.due));
	 new_todo.due.tm_year=year-1900;
	 new_todo.due.tm_mon=month-1;
	 new_todo.due.tm_mday=day;
	 new_todo.priority=priority;
	 new_todo.description=description;
	 new_todo.note=note;

	 todo_to_text(&new_todo, text, sizeof(text));
	 if (!import_all) {
	    ret=import_record_ask(parent_window, pane,
				  text,
				  &(todo_app_info.category),
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
	    pc_todo_write(&new_todo, NEW_PC_REC, attrib, NULL);
	 }
      }
   }

   /* Palm Desktop DAT format */
   if (type==IMPORT_TYPE_DAT) {
      jp_logf(JP_LOG_DEBUG, "Todo import DAT [%s]\n", file_path);
      if (dat_check_if_dat_file(in)!=DAT_TODO_FILE) {
	 jp_logf(JP_LOG_WARN, _("File doesn't appear to be todo.dat format\n"));
	 fclose(in);
	 return 1;
      }
      todolist=NULL;
      dat_get_todos(in, &todolist, &cai);
      import_all=FALSE;
      for (temp_todolist=todolist; temp_todolist; temp_todolist=temp_todolist->next) {
	 index=temp_todolist->mtodo.unique_id-1;
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
	 index=temp_todolist->mtodo.unique_id-1;
	 suggested_cat_num=0;
	 if (index>-1) {
	    for (i=0; i<NUM_TODO_CAT_ITEMS; i++) {
	       if (todo_app_info.category.name[i][0]=='\0') continue;
	       if (!strcmp(todo_app_info.category.name[i], old_cat_name)) {
		  suggested_cat_num=i;
		  i=1000;
		  break;
	       }
	    }
	 }

	 ret=0;
	 todo_to_text(&(temp_todolist->mtodo.todo), text, sizeof(text));
	 if (!import_all) {
	    ret=import_record_ask(parent_window, pane,
				  text,
				  &(todo_app_info.category),
				  old_cat_name,
				  (temp_todolist->mtodo.attrib & 0x10),
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
	   ((temp_todolist->mtodo.attrib & 0x10) ? dlpRecAttrSecret : 0);
	 if ((ret==DIALOG_SAID_IMPORT_YES) || (import_all)) {
	    pc_todo_write(&(temp_todolist->mtodo.todo), NEW_PC_REC,
			  attrib, NULL);
	 }
      }
      free_ToDoList(&todolist);
   }

   todo_refresh();
   fclose(in);
   return 0;
}

int todo_import(GtkWidget *window)
{
   char *type_desc[] = {
      "CSV (Comma Separated Values)",
      "DAT/TDA (Palm Archive Formats)",
      NULL
   };
   int type_int[] = {
      IMPORT_TYPE_CSV,
      IMPORT_TYPE_DAT,
      0
   };

   import_gui(window, pane, type_desc, type_int, todo_import_callback);
   return 0;
}
/*
 * End Import Code
 */

/*
 * Start Export code
 */

void cb_todo_export_ok(GtkWidget *export_window, GtkWidget *clist,
		       int type, const char *filename)
{
   MyToDo *mtodo;
   GList *list, *temp_list;
   FILE *out;
   struct stat statb;
   int i, r, len;
   const char *short_date;
   time_t ltime;
   struct tm *now;
   char *button_text[]={gettext_noop("OK")};
   char *button_overwrite_text[]={gettext_noop("Yes"), gettext_noop("No")};
   char text[1024];
   char str1[256], str2[256];
   char pref_time[40];
   char *csv_text;
   char *p;
   char username[256];
   char hostname[256];
   const char *svalue;
   long userid;

   /* this stuff is for ical only. */
   /* todo: create a pre-export switch */
   get_pref(PREF_USER, &userid, &svalue);
   strncpy(text, svalue, sizeof(text));
   text[sizeof(text)-1]='\0';
   str_to_ical_str(username, sizeof(username), text);
   get_pref(PREF_USER_ID, &userid, &svalue);
   gethostname(text, sizeof(text));
   text[sizeof(text)-1]='\0';
   str_to_ical_str(hostname, sizeof(hostname), text);

   list=GTK_CLIST(clist)->selection;

   if (!stat(filename, &statb)) {
      if (S_ISDIR(statb.st_mode)) {
	 g_snprintf(text, sizeof(text), _("%s is a directory"), filename);
	 dialog_generic(GTK_WINDOW(export_window),
			0, 0, _("Error Opening File"),
			_("Directory"), text, 1, button_text);
	 return;
      }
      g_snprintf(text, sizeof(text), _("Do you want to overwrite file %s?"), filename);
      r = dialog_generic(GTK_WINDOW(export_window),
			 0, 0, _("Overwrite File?"),
			 _("Overwrite File"), text, 2, button_overwrite_text);
      if (r!=DIALOG_SAID_1) {
	 return;
      }
   }

   out = fopen(filename, "w");
   if (!out) {
      g_snprintf(text, sizeof(text), _("Error opening file: %s"), filename);
      dialog_generic(GTK_WINDOW(export_window),
		     0, 0, _("Error Opening File"),
		     _("Filename"), text, 1, button_text);
      return;
   }

   time(&ltime);
   now = gmtime(&ltime);
   /* XXX ical wants gmtime, text wants localtime */

   for (i=0, temp_list=list; temp_list; temp_list = temp_list->next, i++) {
      mtodo = gtk_clist_get_row_data(GTK_CLIST(clist), (int) temp_list->data);
      if (!mtodo) {
	 continue;
	 jp_logf(JP_LOG_WARN, _("Can't export todo %d\n"), (long) temp_list->data + 1);
      }
      switch (type) {
       case EXPORT_TYPE_CSV:
	 if (i==0) {
	    fprintf(out, "CSV todo: Category, Private, Indefinite, Due Date, Priority, Completed, ToDo Text, Note\n");
	 }
	 len=0;
	 if (mtodo->todo.description) {
	    len=strlen(mtodo->todo.description) * 2 + 4;
	 }
	 if (len<256) len=256;
	 csv_text=malloc(len);
	 if (!csv_text) {
	    continue;
	    jp_logf(JP_LOG_WARN, _("Can't export todo %d\n"), (long) temp_list->data + 1);
	 }
	 str_to_csv_str(csv_text, todo_app_info.category.name[mtodo->attrib & 0x0F]);
	 fprintf(out, "\"%s\",", csv_text);
	 fprintf(out, "\"%s\",", (mtodo->attrib & dlpRecAttrSecret) ? "1":"0");
	 fprintf(out, "\"%s\",", mtodo->todo.indefinite ? "1":"0");
	 if (mtodo->todo.indefinite) {
	    csv_text[0]='\0';
	 } else {
	    strftime(csv_text, len, "%Y %02m %02d", &(mtodo->todo.due));
	 }
	 fprintf(out, "\"%s\",", csv_text);
	 fprintf(out, "\"%d\",", mtodo->todo.priority);
	 fprintf(out, "\"%s\",", mtodo->todo.complete ? "1":"0");
	 if (mtodo->todo.description) {
	    str_to_csv_str(csv_text, mtodo->todo.description);
	    fprintf(out, "\"%s\",", csv_text);
	 } else {
	    fprintf(out, "\"\",");
	 }
	 if (mtodo->todo.note) {
	    str_to_csv_str(csv_text, mtodo->todo.note);
	    fprintf(out, "\"%s\"\n", csv_text);
	 } else {
	    fprintf(out, "\"\",");
	 }
	 free(csv_text);
	 break;
       case EXPORT_TYPE_TEXT:
	 get_pref(PREF_SHORTDATE, NULL, &short_date);
	 get_pref_time_no_secs(pref_time);
	 time(&ltime);
	 now = localtime(&ltime);
	 strftime(str1, sizeof(str1), short_date, now);
	 strftime(str2, sizeof(str2), pref_time, now);
	 g_snprintf(text, sizeof(text), "%s %s", str1, str2);

	 /* Todo Should I translate these? */
	 fprintf(out, "ToDo: exported from %s on %s\n", PN, text);
	 fprintf(out, "Category: %s\n", todo_app_info.category.name[mtodo->attrib & 0x0F]);
	 fprintf(out, "Private: %s\n",
		 (mtodo->attrib & dlpRecAttrSecret) ? "Yes":"No");
	 if (mtodo->todo.indefinite) {
	    fprintf(out, "Due Date: Indefinite\n");
	 } else {
	    strftime(text, sizeof(text), "%Y %02m %02d", &(mtodo->todo.due));
	    fprintf(out, "Due Date: %s\n", text);
	 }
	 fprintf(out, "Priority: %d\n", mtodo->todo.priority);
	 fprintf(out, "Completed: %s\n", mtodo->todo.complete ? "Yes":"No");
	 if (mtodo->todo.description) {
	    fprintf(out, "Description: %s\n", mtodo->todo.description);
	 }
	 if (mtodo->todo.note) {
	    fprintf(out, "Note: %s\n", mtodo->todo.note);
	 }
	 break;
       case EXPORT_TYPE_ICALENDAR:
	 /* RFC 2445: Internet Calendaring and Scheduling Core
	  *           Object Specification */
	 if (i == 0) {
	    fprintf(out, "BEGIN:VCALENDAR\nVERSION:2.0\n");
	    fprintf(out, "PRODID:%s\n", FPI_STRING);
	 }
	 fprintf(out, "BEGIN:VTODO\n");
	 if (mtodo->attrib & dlpRecAttrSecret) {
	    fprintf(out, "CLASS:PRIVATE\n");
	 }
	 fprintf(out, "UID:palm-todo-%08x-%08lx-%s@%s\n",
		 mtodo->unique_id, userid, username, hostname);
	 fprintf(out, "DTSTAMP:%04d%02d%02dT%02d%02d%02dZ\n",
		 now->tm_year+1900,
		 now->tm_mon+1,
		 now->tm_mday,
		 now->tm_hour,
		 now->tm_min,
		 now->tm_sec);
	 str_to_ical_str(text, sizeof(text), 
			 todo_app_info.category.name[mtodo->attrib & 0x0F]);
	 fprintf(out, "CATEGORIES:%s\n", text);
	 g_snprintf(str1, sizeof(str1), "%s", mtodo->todo.description);
	 if ((p = strchr(str1, '\n'))) {
	    *p = '\0';
	 }
	 str_to_ical_str(text, sizeof(text), str1);
	 fprintf(out, "SUMMARY:%s%s\n", text,
		 strlen(str1) > 49 ? "..." : "");
	 str_to_ical_str(text, sizeof(text), mtodo->todo.description);
	 fprintf(out, "DESCRIPTION:%s", text);
	 if (mtodo->todo.note && mtodo->todo.note[0]) {
	    str_to_ical_str(text, sizeof(text), mtodo->todo.note);
	    fprintf(out, "\\n\n %s\n", text);
	 } else {
	    fprintf(out, "\n");
	 }
	 fprintf(out, "STATUS:%s\n", mtodo->todo.complete ? "COMPLETED" : "NEEDS-ACTION");
	 fprintf(out, "PRIORITY:%d\n", mtodo->todo.priority);
	 if (!mtodo->todo.indefinite) {
	    fprintf(out, "DUE;VALUE=DATE:%04d%02d%02d\n",
		    mtodo->todo.due.tm_year+1900,
		    mtodo->todo.due.tm_mon+1,
		    mtodo->todo.due.tm_mday);
	 }
	 fprintf(out, "END:VTODO\n");
	 if (temp_list->next == NULL) {
	    fprintf(out, "END:VCALENDAR\n");
	 }
	 break;
       default:
	 jp_logf(JP_LOG_WARN, _("Unknown export type\n"));
      }
   }

   if (out) {
      fclose(out);
   }
}


static void cb_todo_update_clist(GtkWidget *clist, int category)
{
   todo_update_clist(clist, NULL, &export_todo_list, category, FALSE);
}


static void cb_todo_export_done(GtkWidget *widget, const char *filename)
{
   free_ToDoList(&export_todo_list);

   set_pref(PREF_TODO_EXPORT_FILENAME, 0, filename, TRUE);
}

int todo_export(GtkWidget *window)
{
   int w, h, x, y;
   char *type_text[]={"Text", "CSV", "iCalendar", NULL};
   int type_int[]={EXPORT_TYPE_TEXT, EXPORT_TYPE_CSV, EXPORT_TYPE_ICALENDAR};

   gdk_window_get_size(window->window, &w, &h);
   gdk_window_get_root_origin(window->window, &x, &y);

#ifdef ENABLE_GTK2
   w = gtk_paned_get_position(GTK_PANED(pane));
#else
   w = GTK_PANED(pane)->handle_xpos;
#endif
   x+=40;

   export_gui(window,
              w, h, x, y, 5, sort_l,
	      PREF_TODO_EXPORT_FILENAME,
	      type_text,
	      type_int,
	      cb_todo_update_clist,
	      cb_todo_export_done,
	      cb_todo_export_ok
	      );

   return 0;
}

/*
 * End Export Code
 */


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
   int show_priv;
   long char_set; /* JPA */

   mtodo = gtk_clist_get_row_data(GTK_CLIST(clist), clist_row_selected);
   if (mtodo < (MyToDo *)CLIST_MIN_DATA) {
      return;
   }

   /* JPA convert to Palm character set */
   get_pref(PREF_CHAR_SET, &char_set, NULL);
   if (char_set != CHAR_SET_LATIN1) {
      if (mtodo->todo.description)
	charset_j2p(mtodo->todo.description, strlen(mtodo->todo.description)+1, char_set);
      if (mtodo->todo.note)
	charset_j2p(mtodo->todo.note, strlen(mtodo->todo.note)+1, char_set);
   }

   /* Do masking like Palm OS 3.5 */
   show_priv = show_privates(GET_PRIVATES);
   if ((show_priv != SHOW_PRIVATES) &&
       (mtodo->attrib & dlpRecAttrSecret)) {
      return;
   }
   /* End Masking */
   flag = GPOINTER_TO_INT(data);
   if ((flag==MODIFY_FLAG) || (flag==DELETE_FLAG)) {
      jp_logf(JP_LOG_DEBUG, "calling delete_pc_record\n");
      delete_pc_record(TODO, mtodo, flag);
      if (flag==DELETE_FLAG) {
	 /* when we redraw we want to go to the line above the deleted one */
	 if (clist_row_selected>0) {
	    clist_row_selected--;
	 }
      }
   }

   if (flag == DELETE_FLAG) {
      todo_clist_redraw();
   }
}

void cb_undelete_todo(GtkWidget *widget,
		      gpointer   data)
{
   MyToDo *mtodo;
   int flag;
   int show_priv;

   mtodo = gtk_clist_get_row_data(GTK_CLIST(clist), clist_row_selected);
   if (mtodo < (MyToDo *)CLIST_MIN_DATA) {
      return;
   }

   /* Do masking like Palm OS 3.5 */
   show_priv = show_privates(GET_PRIVATES);
   if ((show_priv != SHOW_PRIVATES) &&
       (mtodo->attrib & dlpRecAttrSecret)) {
      return;
   }
   /* End Masking */

   jp_logf(JP_LOG_DEBUG, "mtodo->unique_id = %d\n",mtodo->unique_id);
   jp_logf(JP_LOG_DEBUG, "mtodo->rt = %d\n",mtodo->rt);

   flag = GPOINTER_TO_INT(data);
   if (flag==UNDELETE_FLAG) {
      if (mtodo->rt == DELETED_PALM_REC ||
          mtodo->rt == DELETED_PC_REC)
      {
	 undelete_pc_record(TODO, mtodo, flag);
      }
      /* Possible later addition of undelete for modified records 
      else if (mtodo->rt == MODIFIED_PALM_REC)
      {
	 cb_add_new_record(widget, GINT_TO_POINTER(COPY_FLAG));
      }
      */
   }

   todo_clist_redraw();
}

static void cb_category(GtkWidget *item, int selection)
{
   int b;

   if ((GTK_CHECK_MENU_ITEM(item))->active) {
      b=dialog_save_changed_record(pane, record_changed);
      if (b==DIALOG_SAID_1) {
	 cb_add_new_record(NULL, GINT_TO_POINTER(record_changed));
      }

      todo_category = selection;
      clist_row_selected = 0;
      jp_logf(JP_LOG_DEBUG, "todo_category = %d\n",todo_category);
      todo_clear_details();
      todo_update_clist(clist, category_menu1, &glob_todo_list, todo_category, TRUE);
   }
}

void cb_check_button_no_due_date(GtkWidget *widget, gpointer data)
{
   long till_due;
   struct tm *now;
   time_t ltime;

   if (GTK_TOGGLE_BUTTON(widget)->active) {
      update_due_button(due_date_button, NULL);
   } else {
      time(&ltime);
      now = localtime(&ltime);
      memcpy(&due_date, now, sizeof(struct tm));

      get_pref(PREF_TODO_DAYS_TILL_DUE, &till_due, NULL);
      add_days_to_date(&due_date, till_due);

      update_due_button(due_date_button, &due_date);
   }   
}

int todo_clear_details()
{
   time_t ltime;
   struct tm *now;
   int new_cat;
   int sorted_position;
   long default_due, till_due;

   time(&ltime);
   now = localtime(&ltime);

   /* Need to disconnect these signals first */
   connect_changed_signals(DISCONNECT_SIGNALS);

#ifdef ENABLE_GTK2
   gtk_widget_freeze_child_notify(todo_text);
   gtk_widget_freeze_child_notify(todo_text_note);

   gtk_text_buffer_set_text(GTK_TEXT_BUFFER(todo_text_buffer), "", -1);
   gtk_text_buffer_set_text(GTK_TEXT_BUFFER(todo_text_note_buffer), "", -1);
#else
   gtk_text_freeze(GTK_TEXT(todo_text));
   gtk_text_freeze(GTK_TEXT(todo_text_note));

   gtk_text_set_point(GTK_TEXT(todo_text), 0);
   gtk_text_forward_delete(GTK_TEXT(todo_text),
			   gtk_text_get_length(GTK_TEXT(todo_text)));

   gtk_text_set_point(GTK_TEXT(todo_text_note), 0);
   gtk_text_forward_delete(GTK_TEXT(todo_text_note),
			   gtk_text_get_length(GTK_TEXT(todo_text_note)));
#endif

   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_button_todo[0]), TRUE);

   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(todo_completed_checkbox), FALSE);

   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(private_checkbox), FALSE);

   get_pref(PREF_TODO_DAYS_DUE, &default_due, NULL);
   get_pref(PREF_TODO_DAYS_TILL_DUE, &till_due, NULL);
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(todo_no_due_date_checkbox),
				! default_due);

   memcpy(&due_date, now, sizeof(struct tm));

   if (default_due) {
      add_days_to_date(&due_date, till_due);
      update_due_button(due_date_button, &due_date);
   } else {
      update_due_button(due_date_button, NULL);
   }
   

#ifdef ENABLE_GTK2
   gtk_widget_thaw_child_notify(todo_text);
   gtk_widget_thaw_child_notify(todo_text_note);
#else
   gtk_text_thaw(GTK_TEXT(todo_text));
   gtk_text_thaw(GTK_TEXT(todo_text_note));
#endif

   if (todo_category==CATEGORY_ALL) {
      new_cat = 0;
   } else {
      new_cat = todo_category;
   }
   sorted_position = find_sorted_cat(new_cat);
   if (sorted_position<0) {
      jp_logf(JP_LOG_WARN, _("Category is not legal\n"));
   } else {
      gtk_check_menu_item_set_active
	(GTK_CHECK_MENU_ITEM(todo_cat_menu_item2[sorted_position]), TRUE);
      gtk_option_menu_set_history(GTK_OPTION_MENU(category_menu2), sorted_position);
   }

   set_new_button_to(CLEAR_FLAG);
   connect_changed_signals(CONNECT_SIGNALS);

   return 0;
}

int todo_get_details(struct ToDo *new_todo, unsigned char *attrib)
{
   int i;
#ifdef ENABLE_GTK2
   GtkTextIter start_iter;
   GtkTextIter end_iter;
#endif

   new_todo->indefinite = (GTK_TOGGLE_BUTTON(todo_no_due_date_checkbox)->active);
   if (!(new_todo->indefinite)) {
      new_todo->due.tm_mon = due_date.tm_mon;
      new_todo->due.tm_mday = due_date.tm_mday;
      new_todo->due.tm_year = due_date.tm_year;
      jp_logf(JP_LOG_DEBUG, "todo_get_details: setting due date=%d/%d/%d\n", new_todo->due.tm_mon,
	      new_todo->due.tm_mday, new_todo->due.tm_year);
   } else {
      bzero(&(new_todo->due), sizeof(new_todo->due));
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
#ifdef ENABLE_GTK2
   gtk_text_buffer_get_bounds(GTK_TEXT_BUFFER(todo_text_buffer),
			      &start_iter,&end_iter);
   new_todo->description = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(todo_text_buffer),
						    &start_iter,&end_iter,TRUE);
   gtk_text_buffer_get_bounds(GTK_TEXT_BUFFER(todo_text_note_buffer),
			      &start_iter,&end_iter);
   new_todo->note = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(todo_text_note_buffer),
					     &start_iter,&end_iter,TRUE);
#else
   new_todo->description = gtk_editable_get_chars(GTK_EDITABLE(todo_text), 0, -1);
   new_todo->note = gtk_editable_get_chars(GTK_EDITABLE(todo_text_note), 0, -1);
#endif
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
   jp_logf(JP_LOG_DEBUG, "attrib = %d\n", *attrib);
   jp_logf(JP_LOG_DEBUG, "indefinite=%d\n",new_todo->indefinite);
   if (!new_todo->indefinite)
     jp_logf(JP_LOG_DEBUG, "due: %d/%d/%d\n",new_todo->due.tm_mon,
	    new_todo->due.tm_mday, 
	    new_todo->due.tm_year);
   jp_logf(JP_LOG_DEBUG, "priority=%d\n",new_todo->priority);
   jp_logf(JP_LOG_DEBUG, "complete=%d\n",new_todo->complete);
   jp_logf(JP_LOG_DEBUG, "description=[%s]\n",new_todo->description);
   jp_logf(JP_LOG_DEBUG, "note=[%s]\n",new_todo->note);
#endif

   return 0;
}

static void cb_add_new_record(GtkWidget *widget, gpointer data)
{
   MyToDo *mtodo;
   struct ToDo new_todo;
   unsigned char attrib;
   int flag;
   int show_priv;
   unsigned int unique_id;

   flag=GPOINTER_TO_INT(data);
   unique_id = 0;
   mtodo=NULL;

   /* Do masking like Palm OS 3.5 */
   if ((flag==COPY_FLAG) || (flag==MODIFY_FLAG)) {
      show_priv = show_privates(GET_PRIVATES);
      mtodo = gtk_clist_get_row_data(GTK_CLIST(clist), clist_row_selected);
      if (mtodo < (MyToDo *)CLIST_MIN_DATA) {
	 return;
      }
      if ((show_priv != SHOW_PRIVATES) &&
	  (mtodo->attrib & dlpRecAttrSecret)) {
	 return;
      }
   }
   /* End Masking */
   if (flag==CLEAR_FLAG) {
      /*Clear button was hit */
      todo_clear_details();
      connect_changed_signals(DISCONNECT_SIGNALS);
      set_new_button_to(NEW_FLAG);
      gtk_widget_grab_focus(GTK_WIDGET(todo_text));
      return;
   }
   if ((flag!=NEW_FLAG) && (flag!=MODIFY_FLAG) && (flag!=COPY_FLAG)) {
      return;
   }
   if (flag==MODIFY_FLAG) {
      mtodo = gtk_clist_get_row_data(GTK_CLIST(clist), clist_row_selected);
      unique_id=mtodo->unique_id;
      if (mtodo < (MyToDo *)CLIST_MIN_DATA) {
	 return;
      }
      if ((mtodo->rt==DELETED_PALM_REC) || 
	  (mtodo->rt==DELETED_PC_REC)   ||
          (mtodo->rt==MODIFIED_PALM_REC)) {
	 jp_logf(JP_LOG_INFO, _("You can't modify a record that is deleted\n"));
	 return;
      }
   }
   todo_get_details(&new_todo, &attrib);

   set_new_button_to(CLEAR_FLAG);

   if (flag==MODIFY_FLAG) {
      cb_delete_todo(NULL, data);
      if ((mtodo->rt==PALM_REC) || (mtodo->rt==REPLACEMENT_PALM_REC)) {
	 pc_todo_write(&new_todo, REPLACEMENT_PALM_REC, attrib, &unique_id);
      } else {
	 unique_id=0;
	 pc_todo_write(&new_todo, NEW_PC_REC, attrib, &unique_id);
      }
   } else {
      unique_id=0;
      pc_todo_write(&new_todo, NEW_PC_REC, attrib, &unique_id);
   }
   todo_clist_redraw();
   free_ToDo(&new_todo);
   glob_find_id = unique_id;
   todo_find();

   return;
}

/* Do masking like Palm OS 3.5 */
static void clear_mytodos(MyToDo *mtodo)
{
   mtodo->unique_id=0;
   mtodo->attrib=mtodo->attrib & 0xF8;
   if (mtodo->todo.description) {
      free(mtodo->todo.description);
      mtodo->todo.description=strdup("");
   }
   if (mtodo->todo.note) {
      free(mtodo->todo.note);
      mtodo->todo.note=strdup("");
   }

   return;
}
/* End Masking */

static void cb_edit_cats(GtkWidget *widget, gpointer data)
{
   struct ToDoAppInfo ai;
   char full_name[FILENAME_MAX];
   unsigned char buffer[65536];
   int num;
   int size;
   void *buf;
   struct pi_file *pf;

   jp_logf(JP_LOG_DEBUG, "cb_edit_cats\n");

   get_home_file_name("ToDoDB.pdb", full_name, sizeof(full_name));

   buf=NULL;
   memset(&ai, 0, sizeof(ai));

   pf = pi_file_open(full_name);
   pi_file_get_app_info(pf, &buf, &size);

   num = unpack_ToDoAppInfo(&ai, buf, size);
   if (num <= 0) {
      jp_logf(JP_LOG_WARN, _("Error reading file: %s\n"), "ToDoDB.pdb");
      return;
   }

   pi_file_close(pf);

   edit_cats(widget, "ToDoDB", &(ai.category));

   size = pack_ToDoAppInfo(&ai, buffer, sizeof(buffer));

   pdb_file_write_app_block("ToDoDB", buffer, size);

   cb_app_button(NULL, GINT_TO_POINTER(REDRAW));
}

/* Function is used to sort clist based on the completed checkbox */
gint GtkClistCompareCheckbox(GtkCList *clist,
                             gconstpointer ptr1,
                             gconstpointer ptr2)
{
   GtkCListRow *row1;
   GtkCListRow *row2;
   MyToDo      *mtodo1;
   MyToDo      *mtodo2;
   struct ToDo *todo1;
   struct ToDo *todo2;

   row1 = (GtkCListRow *) ptr1;
   row2 = (GtkCListRow *) ptr2;
   
   mtodo1 = row1->data; 
   mtodo2 = row2->data; 

   todo1 = &(mtodo1->todo); 
   todo2 = &(mtodo2->todo); 

   if (todo1->complete && !todo2->complete) {
      return -1;
   } else if (todo2->complete && !todo1->complete) {
      return 1;
   } else {
      return 0;
   }

}

static void cb_clist_click_column(GtkWidget *clist, int column)
{
   /* Clicking on same column toggles ascending/descending sort */
   if (clist_col_selected == column)
   {
      if (GTK_CLIST(clist)->sort_type == GTK_SORT_ASCENDING) {
	 gtk_clist_set_sort_type(GTK_CLIST (clist), GTK_SORT_DESCENDING);
      }
      else {
	 gtk_clist_set_sort_type(GTK_CLIST (clist), GTK_SORT_ASCENDING);
      }
   }
   else /* Always sort in ascending order when changing sort column */
   {
      gtk_clist_set_sort_type(GTK_CLIST (clist), GTK_SORT_ASCENDING);
   }

   clist_col_selected = column;

   gtk_clist_set_sort_column(GTK_CLIST(clist), column);
   if (column == 0) {
      gtk_clist_set_compare_func(GTK_CLIST(clist),GtkClistCompareCheckbox);
   } else {
      gtk_clist_set_compare_func(GTK_CLIST(clist),NULL);
   }
   gtk_clist_sort (GTK_CLIST (clist));

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
   int keep, b;

   time_t ltime;
   struct tm *now;

   if ((!event) && (clist_hack)) return;

   /* HACK, see clist hack explanation in memo_gui.c */
   if (clist_hack) {
      keep=record_changed;
      gtk_clist_select_row(GTK_CLIST(clist), clist_row_selected, column);
      b=dialog_save_changed_record(pane, record_changed);
      if (b==DIALOG_SAID_1) {
	 cb_add_new_record(NULL, GINT_TO_POINTER(record_changed));
      }
      set_new_button_to(CLEAR_FLAG);
      gtk_clist_select_row(GTK_CLIST(clist), row, column);
      return;
   }

   time(&ltime);
   now = localtime(&ltime);

   clist_row_selected=row;

   mtodo = gtk_clist_get_row_data(GTK_CLIST(clist), row);
   if (mtodo==NULL) {
      return;
   }

   if (mtodo->rt == DELETED_PALM_REC ||
      (mtodo->rt == DELETED_PC_REC))
      /* Possible later addition of undelete code for modified deleted records
         || mtodo->rt == MODIFIED_PALM_REC
      */
   {
      set_new_button_to(UNDELETE_FLAG);
   }
   else
   {
      set_new_button_to(CLEAR_FLAG);
   }

   connect_changed_signals(DISCONNECT_SIGNALS);

   if (mtodo==NULL) {
      return;
   }
   todo=&(mtodo->todo);

#ifdef ENABLE_GTK2
   gtk_widget_freeze_child_notify(todo_text);
   gtk_widget_freeze_child_notify(todo_text_note);

   gtk_text_buffer_set_text(GTK_TEXT_BUFFER(todo_text_buffer), "", -1);
   gtk_text_buffer_set_text(GTK_TEXT_BUFFER(todo_text_note_buffer), "", -1);
#else
   gtk_text_freeze(GTK_TEXT(todo_text));
   gtk_text_freeze(GTK_TEXT(todo_text_note));

   gtk_text_set_point(GTK_TEXT(todo_text), 0);
   gtk_text_forward_delete(GTK_TEXT(todo_text),
			   gtk_text_get_length(GTK_TEXT(todo_text)));

   gtk_text_set_point(GTK_TEXT(todo_text_note), 0);
   gtk_text_forward_delete(GTK_TEXT(todo_text_note),
			   gtk_text_get_length(GTK_TEXT(todo_text_note)));
#endif

   index = mtodo->attrib & 0x0F;
   sorted_position = find_sorted_cat(index);
   if (todo_cat_menu_item2[sorted_position]==NULL) {
      /* Illegal category */
      jp_logf(JP_LOG_DEBUG, _("Category is not legal\n"));
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
      jp_logf(JP_LOG_WARN, _("Category is not legal\n"));
   } else {
      gtk_check_menu_item_set_active
	(GTK_CHECK_MENU_ITEM(todo_cat_menu_item2[sorted_position]), TRUE);
   }
   gtk_option_menu_set_history(GTK_OPTION_MENU(category_menu2), count);

   if (todo->description) {
      if (todo->description[0]) {
#ifdef ENABLE_GTK2
	 gtk_text_buffer_set_text(GTK_TEXT_BUFFER(todo_text_buffer), todo->description, -1);
#else
	 gtk_text_insert(GTK_TEXT(todo_text), NULL,NULL,NULL, todo->description, -1);
#endif
      }
   }

   if (todo->note) {
      if (todo->note[0]) {
#ifdef ENABLE_GTK2
	 gtk_text_buffer_set_text(GTK_TEXT_BUFFER(todo_text_note_buffer), todo->note, -1);
#else
	 gtk_text_insert(GTK_TEXT(todo_text_note), NULL,NULL,NULL, todo->note, -1);
#endif
      }
   }

   if ( (todo->priority<1) || (todo->priority>5) ) {
      jp_logf(JP_LOG_WARN, _("Priority out of range\n"));
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

#ifdef ENABLE_GTK2
   gtk_widget_thaw_child_notify(todo_text);
   gtk_widget_thaw_child_notify(todo_text_note);
#else
   gtk_text_thaw(GTK_TEXT(todo_text));
   gtk_text_thaw(GTK_TEXT(todo_text_note));
#endif

   /*If they have clicked on the checkmark box then do a modify */
   if (column==0) {
      todo->complete = !(todo->complete);
      /* See if we need to mark the due date as the date completed */
      if (todo->complete) {
	 long todo_completion_date;
	 get_pref(PREF_TODO_COMPLETION_DATE, &todo_completion_date, NULL);
	 if (todo_completion_date) {
	    time_t ltime;
	    struct tm *now;
	    time(&ltime);
	    now = localtime(&ltime);
	    memcpy(&due_date, now, sizeof(struct tm));
	    jp_logf(JP_LOG_DEBUG, "cb_clist_selection: setting due date=%d/%d/%d\n", due_date.tm_mon,
		    due_date.tm_mday, due_date.tm_year);
	    update_due_button(due_date_button, NULL);
	    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(todo_no_due_date_checkbox), FALSE);
	 }
      }
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(todo_completed_checkbox), todo->complete);
      gtk_signal_emit_by_name(GTK_OBJECT(apply_record_button), "clicked");
   }
   connect_changed_signals(CONNECT_SIGNALS);
}


void todo_update_clist(GtkWidget *clist, GtkWidget *tooltip_widget,
		       ToDoList **todo_list, int category, int main)
{
   int num_entries, entries_shown, i;
   gchar *empty_line[] = { "","","","","" };
   GdkPixmap *pixmap_note;
   GdkPixmap *pixmap_check;
   GdkPixmap *pixmap_checked;
   GdkBitmap *mask_note;
   GdkBitmap *mask_check;
   GdkBitmap *mask_checked;
   ToDoList *temp_todo;
   char str[50];
   char str2[TODO_MAX_COLUMN_LEN+2];
   long ivalue;
   const char *svalue;
   long hide_completed;
   long hide_not_due;
   int show_priv;
   time_t ltime;
   struct tm *now, *due;
   int comp_now, comp_due;

   free_ToDoList(todo_list);

   /* Need to get all records including private ones for the tooltips calculation */
   num_entries = get_todos2(todo_list, SORT_ASCENDING, 2, 2, 1, 1, CATEGORY_ALL);

   /* Start by clearing existing entry if in main window */
   if (main) {
      todo_clear_details();
   }

   /* Freeze clist to prevent flicker during updating */
   gtk_clist_freeze(GTK_CLIST(clist));
   if (main) {
      gtk_signal_disconnect_by_func(GTK_OBJECT(clist),
				    GTK_SIGNAL_FUNC(cb_clist_selection), NULL);
   }
   gtk_clist_clear(GTK_CLIST(clist));

   /* Collect preferences and constant pixmaps for loop */
   get_pref(PREF_HIDE_COMPLETED, &hide_completed, NULL);
   get_pref(PREF_HIDE_NOT_DUE, &hide_not_due, NULL);
   show_priv = show_privates(GET_PRIVATES);
   get_pixmaps(clist, PIXMAP_NOTE, &pixmap_note, &mask_note);
   get_pixmaps(clist, PIXMAP_BOX_CHECK, &pixmap_check, &mask_check);
   get_pixmaps(clist, PIXMAP_BOX_CHECKED, &pixmap_checked,&mask_checked);

   entries_shown=0;
   show_priv = show_privates(GET_PRIVATES);
   for (temp_todo = *todo_list, i=0; temp_todo; temp_todo=temp_todo->next) {
      if ( ((temp_todo->mtodo.attrib & 0x0F) != category) &&
	  category != CATEGORY_ALL) {
	 continue;
      }

      /* Do masking like Palm OS 3.5 */
      if ((show_priv == MASK_PRIVATES) && 
	  (temp_todo->mtodo.attrib & dlpRecAttrSecret)) {
	 gtk_clist_append(GTK_CLIST(clist), empty_line);
	 gtk_clist_set_text(GTK_CLIST(clist), entries_shown, TODO_CHECK_COLUMN, "---");
	 gtk_clist_set_text(GTK_CLIST(clist), entries_shown, TODO_PRIORITY_COLUMN, "---");
	 gtk_clist_set_text(GTK_CLIST(clist), entries_shown, TODO_TEXT_COLUMN, "--------------------");
	 gtk_clist_set_text(GTK_CLIST(clist), entries_shown, TODO_DATE_COLUMN, "----------");
	 clear_mytodos(&temp_todo->mtodo);
	 gtk_clist_set_row_data(GTK_CLIST(clist), entries_shown, &(temp_todo->mtodo));
	 gtk_clist_set_row_style(GTK_CLIST(clist), entries_shown, NULL);
	 entries_shown++;
	 continue;
      }
      /* End Masking */
      
      /* Allow a record found through search window to temporarily be 
         displayed even if it would normally be hidden by option settings */
      if (!glob_find_id || (glob_find_id != temp_todo->mtodo.unique_id))
      {
	 /*Hide the completed records if need be */
	 if (hide_completed && temp_todo->mtodo.todo.complete) {
	    continue;
	 }

	 /*Hide the not due yet records if need be */
	 if ((hide_not_due) && (!(temp_todo->mtodo.todo.indefinite))) {
	    time_t ltime;
	    struct tm *now, *due;
	    int comp_now, comp_due;
	    time(&ltime);
	    now = localtime(&ltime);
	    comp_now=now->tm_year*380+now->tm_mon*31+now->tm_mday-1;
	    due = &(temp_todo->mtodo.todo.due);
	    comp_due=due->tm_year*380+due->tm_mon*31+due->tm_mday-1;
	    if (comp_due > comp_now) {
	       continue;
	    }
	 }
      }

      /* Hide the private records if need be */
      if ((show_priv != SHOW_PRIVATES) && 
	  (temp_todo->mtodo.attrib & dlpRecAttrSecret)) {
	 continue;
      }

      /* Add entry to clist */
      gtk_clist_append(GTK_CLIST(clist), empty_line);

      /* Put a checkbox or checked checkbox pixmap up */
      if (temp_todo->mtodo.todo.complete) {
	 gtk_clist_set_pixmap(GTK_CLIST(clist), entries_shown, TODO_CHECK_COLUMN, pixmap_checked, mask_checked);
      } else {
	 gtk_clist_set_pixmap(GTK_CLIST(clist), entries_shown, TODO_CHECK_COLUMN, pixmap_check, mask_check);
      }

      /* Print the priority number */
      sprintf(str, "%d", temp_todo->mtodo.todo.priority);
      gtk_clist_set_text(GTK_CLIST(clist), entries_shown, TODO_PRIORITY_COLUMN, str);

      /* Put a note pixmap up */
      if (temp_todo->mtodo.todo.note[0]) {
	 gtk_clist_set_pixmap(GTK_CLIST(clist), entries_shown, TODO_NOTE_COLUMN, pixmap_note, mask_note);
      } else {
	 gtk_clist_set_text(GTK_CLIST(clist), entries_shown, TODO_NOTE_COLUMN, "");
      }

      /* Print the due date */
      if (!temp_todo->mtodo.todo.indefinite) {
	  get_pref(PREF_SHORTDATE, &ivalue, &svalue);
	  get_pref_possibility(PREF_SHORTDATE,ivalue,str);
	  strftime(str, sizeof(str), svalue, &(temp_todo->mtodo.todo.due));
      }
      else {
	  sprintf(str, _("No date"));
      }
      gtk_clist_set_text(GTK_CLIST(clist), entries_shown, TODO_DATE_COLUMN, str);
      /* Print the todo text */
      lstrncpy_remove_cr_lfs(str2, temp_todo->mtodo.todo.description, TODO_MAX_COLUMN_LEN);
      gtk_clist_set_text(GTK_CLIST(clist), entries_shown, TODO_TEXT_COLUMN, str2);

      gtk_clist_set_row_data(GTK_CLIST(clist), entries_shown, &(temp_todo->mtodo));

      /* Highlight row background depending on status */
      switch (temp_todo->mtodo.rt) {
       case NEW_PC_REC:
       case REPLACEMENT_PALM_REC:
	 set_bg_rgb_clist_row(clist, entries_shown,
			  CLIST_NEW_RED, CLIST_NEW_GREEN, CLIST_NEW_BLUE);
	 break;
       case DELETED_PALM_REC:
       case DELETED_PC_REC:
	 set_bg_rgb_clist_row(clist, entries_shown,
			  CLIST_DEL_RED, CLIST_DEL_GREEN, CLIST_DEL_BLUE);
	 break;
       case MODIFIED_PALM_REC:
	 set_bg_rgb_clist_row(clist, entries_shown,
			  CLIST_MOD_RED, CLIST_MOD_GREEN, CLIST_MOD_BLUE);
	 break;
       default:
	 if (temp_todo->mtodo.attrib & dlpRecAttrSecret) {
	    set_bg_rgb_clist_row(clist, entries_shown, 
			     CLIST_PRIVATE_RED, CLIST_PRIVATE_GREEN, CLIST_PRIVATE_BLUE);
	 } else {
	    gtk_clist_set_row_style(GTK_CLIST(clist), entries_shown, NULL);
	 }
      }

      /* Highlight dates of overdue todo items */
      if (!(temp_todo->mtodo.todo.indefinite)) {
	 time(&ltime);
	 now = localtime(&ltime);
	 comp_now=now->tm_year*380+now->tm_mon*31+now->tm_mday-1;
	 due = &(temp_todo->mtodo.todo.due);
	 comp_due=due->tm_year*380+due->tm_mon*31+due->tm_mday-1;

	 if (comp_due < comp_now) {
	    set_fg_rgb_clist_cell(clist, entries_shown, TODO_DATE_COLUMN, CLIST_OVERDUE_RED, CLIST_OVERDUE_GREEN, CLIST_OVERDUE_BLUE);
	 }
      }

      entries_shown++;
   }

   jp_logf(JP_LOG_DEBUG, "entries_shown=%d\n",entries_shown);

   if (main) {
      gtk_signal_connect(GTK_OBJECT(clist), "select_row",
			 GTK_SIGNAL_FUNC(cb_clist_selection), NULL);
   }

   /* If there are items in the list, highlight the selected row */
   if ((main) && (entries_shown>0)) {
      /* Select the existing requested row, or row 0 if that is impossible */
      if (clist_row_selected <= entries_shown)
      {
	 gtk_clist_select_row(GTK_CLIST(clist), clist_row_selected, TODO_PRIORITY_COLUMN);
	 if (!gtk_clist_row_is_visible(GTK_CLIST(clist), clist_row_selected)) {
	    gtk_clist_moveto(GTK_CLIST(clist), clist_row_selected, 0, 0.5, 0.0);
	 }
      }
      else
      {
	 gtk_clist_select_row(GTK_CLIST(clist), 0, TODO_PRIORITY_COLUMN);
      }
   }

   /* Unfreeze clist after all changes */
   gtk_clist_thaw(GTK_CLIST(clist));

   if (tooltip_widget) {
      if (todo_list==NULL) {
	 gtk_tooltips_set_tip(glob_tooltips, tooltip_widget, _("0 records"), NULL);
      }
      else {
	 sprintf(str, _("%d of %d records"), entries_shown, num_entries);
	 gtk_tooltips_set_tip(glob_tooltips, tooltip_widget, str, NULL);
      }
   }

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
	 gtk_clist_select_row(GTK_CLIST(clist), found_at, TODO_PRIORITY_COLUMN);
	 if (!gtk_clist_row_is_visible(GTK_CLIST(clist), found_at)) {
	    gtk_clist_moveto(GTK_CLIST(clist), found_at, 0, 0.5, 0.0);
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
#ifdef ENABLE_GTK2
      GtkTextIter   cursor_pos_iter;
      GtkTextBuffer *text_buffer;
#endif
   if (event->keyval == GDK_Tab) {
      /* See if they are at the end of the text */
#ifdef ENABLE_GTK2
      text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(widget));
      gtk_text_buffer_get_iter_at_mark(text_buffer,&cursor_pos_iter,gtk_text_buffer_get_insert(text_buffer));
      if (gtk_text_iter_is_end(&cursor_pos_iter)) {
#else
      if (gtk_text_get_point(GTK_TEXT(widget)) ==
	  gtk_text_get_length(GTK_TEXT(widget))) {
#endif
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
   todo_update_clist(clist, category_menu1, &glob_todo_list, todo_category, TRUE);

   return 0;
}

int todo_cycle_cat()
{
   int b;
   int i, new_cat;

   b=dialog_save_changed_record(pane, record_changed);
   if (b==DIALOG_SAID_1) {
      cb_add_new_record(NULL, GINT_TO_POINTER(record_changed));
   }

   if (todo_category == CATEGORY_ALL) {
      new_cat = -1;
   } else {
      new_cat = find_sorted_cat(todo_category);
   }
   for (i=0; i<NUM_TODO_CAT_ITEMS; i++) {
      new_cat++;
      if (new_cat >= NUM_TODO_CAT_ITEMS) {
	 todo_category = CATEGORY_ALL;
	 break;
      }
      if ((sort_l[new_cat].Pcat) && (sort_l[new_cat].Pcat[0])) {
	 todo_category = sort_l[new_cat].cat_num;
	 break;
      }
   }
   clist_row_selected = 0;

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
   todo_update_clist(clist, category_menu1, &glob_todo_list, todo_category, TRUE);
   if (index<0) {   
      jp_logf(JP_LOG_WARN, _("Category is not legal\n"));
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
   int b;

   b=dialog_save_changed_record(pane, record_changed);
   if (b==DIALOG_SAID_1) {
      cb_add_new_record(NULL, GINT_TO_POINTER(record_changed));
   }
   free_ToDoList(&glob_todo_list);
   connect_changed_signals(DISCONNECT_SIGNALS);
#ifdef ENABLE_GTK2
   set_pref(PREF_TODO_PANE, gtk_paned_get_position(GTK_PANED(pane)), NULL, TRUE);
#else
   set_pref(PREF_TODO_PANE, GTK_PANED(pane)->handle_xpos, NULL, TRUE);
#endif
   set_pref(PREF_LAST_TODO_CATEGORY, todo_category, NULL, TRUE);

   return 0;
}

int todo_gui(GtkWidget *vbox, GtkWidget *hbox)
{
   extern GtkWidget *glob_date_label;
   extern int glob_date_timer_tag;
   GtkWidget *scrolled_window;
   GtkWidget *pixmapwid;
   GdkPixmap *pixmap;
   GdkBitmap *mask;
   GtkWidget *vbox1, *vbox2;
   GtkWidget *hbox_temp;
   GtkWidget *separator;
   GtkWidget *label;
#ifndef ENABLE_GTK2
   GtkWidget *vscrollbar;
#endif
   GtkWidget *button;
   time_t ltime;
   struct tm *now;
   char str[MAX_RADIO_BUTTON_LEN];
   int i;
   GSList    *group;
   long ivalue;
   const char *svalue;
   char *titles[]={"","","","",""};
   GtkAccelGroup *accel_group;

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
   
   get_pref(PREF_LAST_TODO_CATEGORY, &ivalue, NULL);
   todo_category = ivalue;

   if ((todo_category != CATEGORY_ALL) && (todo_app_info.category.name[todo_category][0]=='\0')) {
      todo_category=CATEGORY_ALL;
   }

   accel_group = gtk_accel_group_new();
   gtk_window_add_accel_group(GTK_WINDOW(gtk_widget_get_toplevel(vbox)),
      accel_group);

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

   /* Category Box */
   hbox_temp = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox1), hbox_temp, FALSE, FALSE, 0);

   /*Put the left-hand category menu up */
   make_category_menu(&category_menu1, todo_cat_menu_item1,
		      sort_l, cb_category, TRUE);
   gtk_box_pack_start(GTK_BOX(hbox_temp), category_menu1, TRUE, TRUE, 0);

   /* Edit category button */
   button = gtk_button_new_with_label(_("Edit Categories"));
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_edit_cats), NULL);
   gtk_box_pack_start(GTK_BOX(hbox_temp), button, FALSE, FALSE, 0);

   /*Put the todo list window up */
   scrolled_window = gtk_scrolled_window_new(NULL, NULL);
   /*gtk_widget_set_usize(GTK_WIDGET(scrolled_window), 200, 0); */
   gtk_container_set_border_width(GTK_CONTAINER(scrolled_window), 0);
   gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
   gtk_box_pack_start(GTK_BOX(vbox1), scrolled_window, TRUE, TRUE, 0);

   clist = gtk_clist_new_with_titles(5, titles);
   clist_hack=FALSE;
   gtk_clist_set_column_title(GTK_CLIST(clist), TODO_TEXT_COLUMN, _("Task"));
   gtk_clist_set_column_title(GTK_CLIST(clist), TODO_DATE_COLUMN, _("Due"));
   /* Put pretty pictures in the clist column headings */
   get_pixmaps(vbox, PIXMAP_NOTE, &pixmap, &mask);
   pixmapwid = gtk_pixmap_new(pixmap, mask);
   hack_clist_set_column_title_pixmap(clist, TODO_NOTE_COLUMN, pixmapwid);

   get_pixmaps(vbox, PIXMAP_BOX_CHECKED, &pixmap, &mask);
   pixmapwid = gtk_pixmap_new(pixmap, mask);
   hack_clist_set_column_title_pixmap(clist, TODO_CHECK_COLUMN, pixmapwid);

   gtk_clist_column_titles_active(GTK_CLIST(clist));
   gtk_signal_connect(GTK_OBJECT(clist), "click_column",
                      GTK_SIGNAL_FUNC (cb_clist_click_column), NULL);

   gtk_signal_connect(GTK_OBJECT(clist), "select_row",
		      GTK_SIGNAL_FUNC(cb_clist_selection), NULL);
   gtk_clist_set_shadow_type(GTK_CLIST(clist), SHADOW);
   gtk_clist_set_selection_mode(GTK_CLIST(clist), GTK_SELECTION_BROWSE);

   gtk_clist_set_column_auto_resize(GTK_CLIST(clist), TODO_CHECK_COLUMN, TRUE);
   gtk_clist_set_column_auto_resize(GTK_CLIST(clist), TODO_PRIORITY_COLUMN, TRUE);
   gtk_clist_set_column_auto_resize(GTK_CLIST(clist), TODO_NOTE_COLUMN, TRUE);
   gtk_clist_set_column_auto_resize(GTK_CLIST(clist), TODO_DATE_COLUMN, TRUE);
   gtk_clist_set_column_auto_resize(GTK_CLIST(clist), TODO_TEXT_COLUMN, FALSE);

   gtk_container_add(GTK_CONTAINER(scrolled_window), GTK_WIDGET(clist));

   /* The focus doesn't do any good on the application button */
   gtk_widget_grab_focus(GTK_WIDGET(clist));

   /* */
   /* The right hand part of the main window follows: */
   /* */
   hbox_temp = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, FALSE, FALSE, 0);


   /* Add record modification buttons on right side */
   /* Delete button */
   delete_record_button = gtk_button_new_with_label(_("Delete"));
   gtk_signal_connect(GTK_OBJECT(delete_record_button), "clicked",
		      GTK_SIGNAL_FUNC(cb_delete_todo),
		      GINT_TO_POINTER(DELETE_FLAG));
   gtk_box_pack_start(GTK_BOX(hbox_temp), delete_record_button, TRUE, TRUE, 0);
   gtk_widget_add_accelerator(delete_record_button, "clicked", accel_group,
	 GDK_d, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
   gtk_tooltips_set_tip(glob_tooltips, delete_record_button, _("Delete the selected record   Ctrl+D"), NULL);

   undelete_record_button = gtk_button_new_with_label(_("Undelete"));
   gtk_signal_connect(GTK_OBJECT(undelete_record_button), "clicked",
		      GTK_SIGNAL_FUNC(cb_undelete_todo),
		      GINT_TO_POINTER(UNDELETE_FLAG));
   gtk_box_pack_start(GTK_BOX(hbox_temp), undelete_record_button, TRUE, TRUE, 0);

   copy_record_button = gtk_button_new_with_label(_("Copy"));
   gtk_signal_connect(GTK_OBJECT(copy_record_button), "clicked",
		      GTK_SIGNAL_FUNC(cb_add_new_record), 
		      GINT_TO_POINTER(COPY_FLAG));
   gtk_box_pack_start(GTK_BOX(hbox_temp), copy_record_button, TRUE, TRUE, 0);
   gtk_widget_add_accelerator(copy_record_button, "clicked", accel_group, GDK_o,
      GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
   gtk_tooltips_set_tip(glob_tooltips, copy_record_button, _("Copy the record   Ctrl+O"), NULL);

   new_record_button = gtk_button_new_with_label(_("New Record"));
   gtk_signal_connect(GTK_OBJECT(new_record_button), "clicked",
		      GTK_SIGNAL_FUNC(cb_add_new_record), 
		      GINT_TO_POINTER(CLEAR_FLAG));
   gtk_box_pack_start(GTK_BOX(hbox_temp), new_record_button, TRUE, TRUE, 0);
   gtk_widget_add_accelerator(new_record_button, "clicked", accel_group, GDK_n,
      GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
   gtk_tooltips_set_tip(glob_tooltips, new_record_button, _("Add a new record   Ctrl+N"), NULL);

   add_record_button = gtk_button_new_with_label(_("Add Record"));
   gtk_signal_connect(GTK_OBJECT(add_record_button), "clicked",
		      GTK_SIGNAL_FUNC(cb_add_new_record), 
		      GINT_TO_POINTER(NEW_FLAG));
   gtk_box_pack_start(GTK_BOX(hbox_temp), add_record_button, TRUE, TRUE, 0);
   gtk_widget_set_name(GTK_WIDGET(GTK_LABEL(GTK_BIN(add_record_button)->child)),
		       "label_high");
   gtk_widget_add_accelerator(add_record_button, "clicked", accel_group,
      GDK_r, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
   gtk_tooltips_set_tip(glob_tooltips, add_record_button, _("Add the new record   Ctrl+R"), NULL);

   apply_record_button = gtk_button_new_with_label(_("Apply Changes"));
   gtk_signal_connect(GTK_OBJECT(apply_record_button), "clicked",
		      GTK_SIGNAL_FUNC(cb_add_new_record),
		      GINT_TO_POINTER(MODIFY_FLAG));
   gtk_box_pack_start(GTK_BOX(hbox_temp), apply_record_button, TRUE, TRUE, 0);
   gtk_widget_set_name(GTK_WIDGET(GTK_LABEL(GTK_BIN(apply_record_button)->child)),
		       "label_high");
   gtk_widget_add_accelerator(apply_record_button, "clicked", accel_group,
      GDK_Return, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
   gtk_tooltips_set_tip(glob_tooltips, apply_record_button, _("Commit the modifications   Ctrl+Enter"), NULL);


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
   make_category_menu(&category_menu2, todo_cat_menu_item2,
		      sort_l, NULL, FALSE);

   gtk_box_pack_start(GTK_BOX(hbox_temp), category_menu2, TRUE, TRUE, 0);


   /*The Description text box on the right side */
   hbox_temp = gtk_hbox_new (FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, FALSE, FALSE, 0);

   hbox_temp = gtk_hbox_new (FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, TRUE, TRUE, 0);

#ifdef ENABLE_GTK2
   todo_text = gtk_text_view_new();
   todo_text_buffer = G_OBJECT(gtk_text_view_get_buffer(GTK_TEXT_VIEW(todo_text)));
   gtk_text_view_set_editable(GTK_TEXT_VIEW(todo_text), TRUE);
   gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(todo_text), GTK_WRAP_WORD);

   scrolled_window = gtk_scrolled_window_new(NULL, NULL);
   gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
				  GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
   gtk_container_set_border_width(GTK_CONTAINER(scrolled_window), 1);
   gtk_container_add(GTK_CONTAINER(scrolled_window), todo_text);
   gtk_box_pack_start_defaults(GTK_BOX(hbox_temp), scrolled_window);
#else
   todo_text = gtk_text_new(NULL, NULL);
   gtk_text_set_editable(GTK_TEXT(todo_text), TRUE);
   gtk_text_set_word_wrap(GTK_TEXT(todo_text), TRUE);
   vscrollbar = gtk_vscrollbar_new(GTK_TEXT(todo_text)->vadj);
   gtk_box_pack_start(GTK_BOX(hbox_temp), todo_text, TRUE, TRUE, 0);
   gtk_box_pack_start(GTK_BOX(hbox_temp), vscrollbar, FALSE, FALSE, 0);
#endif

   /*The Note text box on the right side */
   hbox_temp = gtk_hbox_new (FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, FALSE, FALSE, 0);

   label = gtk_label_new(_("Note"));
   gtk_box_pack_start(GTK_BOX(hbox_temp), label, FALSE, FALSE, 0);

   hbox_temp = gtk_hbox_new (FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, TRUE, TRUE, 0);

#ifdef ENABLE_GTK2
   todo_text_note = gtk_text_view_new();
   todo_text_note_buffer = G_OBJECT(gtk_text_view_get_buffer(GTK_TEXT_VIEW(todo_text_note)));
   gtk_text_view_set_editable(GTK_TEXT_VIEW(todo_text_note), TRUE);
   gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(todo_text_note), GTK_WRAP_WORD);

   scrolled_window = gtk_scrolled_window_new(NULL, NULL);
   gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
				  GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
   gtk_container_set_border_width(GTK_CONTAINER(scrolled_window), 1);
   gtk_container_add(GTK_CONTAINER(scrolled_window), todo_text_note);
   gtk_box_pack_start_defaults(GTK_BOX(hbox_temp), scrolled_window);
#else
   todo_text_note = gtk_text_new(NULL, NULL);
   gtk_text_set_editable(GTK_TEXT(todo_text_note), TRUE);
   gtk_text_set_word_wrap(GTK_TEXT(todo_text_note), TRUE);
   vscrollbar = gtk_vscrollbar_new(GTK_TEXT(todo_text_note)->vadj);
   gtk_box_pack_start(GTK_BOX(hbox_temp), todo_text_note, TRUE, TRUE, 0);
   gtk_box_pack_start(GTK_BOX(hbox_temp), vscrollbar, FALSE, FALSE, 0);
#endif

   /* Capture the TAB key to change focus with it */
   gtk_signal_connect(GTK_OBJECT(todo_text), "key_press_event",
		      GTK_SIGNAL_FUNC(cb_key_pressed), todo_text_note);

   gtk_widget_set_usize(GTK_WIDGET(todo_text), 10, 10);
   gtk_widget_set_usize(GTK_WIDGET(todo_text_note), 10, 10);

   gtk_widget_show_all(vbox);
   gtk_widget_show_all(hbox);

   gtk_widget_hide(add_record_button);
   gtk_widget_hide(apply_record_button);
   gtk_widget_hide(undelete_record_button);

   todo_refresh();

   return 0;
}

/* search_gui.c
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
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "utils.h"
#include "prefs.h"
#include "log.h"
#include "datebook.h"
#include "address.h"
#include "todo.h"
#include "memo.h"
#include <pi-datebook.h>
#include <pi-address.h>
#include <pi-todo.h>
#include <pi-memo.h>
#ifdef ENABLE_PLUGINS
#include "plugins.h"
#endif


#define SEARCH_MAX_COLUMN_LEN 80

static struct search_record *search_rl = NULL;
static GtkWidget *case_sense_checkbox;
static GtkWidget *window = NULL;
static GtkWidget *entry = NULL;
static GtkAccelGroup *accel_group = NULL;

int datebook_search_sort_compare(const void *v1, const void *v2)
{
   AppointmentList **al1, **al2;
   struct Appointment *appt1, *appt2;
   time_t time1, time2;

   al1=(AppointmentList **)v1;
   al2=(AppointmentList **)v2;

   appt1=&((*al1)->mappt.appt);
   appt2=&((*al2)->mappt.appt);

   time1 = mktime(&(appt1->begin));
   time2 = mktime(&(appt2->begin));

   return(time2 - time1); 
}

static int search_datebook(const char *needle, GtkWidget *clist)
{
   gchar *empty_line[] = { "","" };
   AppointmentList *a_list;
   AppointmentList *temp_al;
   int found, count;
   char str[202];
   char str2[SEARCH_MAX_COLUMN_LEN+2];
   char date_str[52];
   char datef[52];
   const char *svalue1;
   long ivalue;
   struct search_record *new_sr;

   /*Search Appointments */
   a_list = NULL;

   get_days_appointments2(&a_list, NULL, 2, 2, 2, NULL);

   if (a_list==NULL) {
      return 0;
   }

   /* Sort returned results according to date rather than just HH:MM */
   datebook_sort(&a_list, datebook_search_sort_compare);

   count = 0;

   for (temp_al = a_list; temp_al; temp_al=temp_al->next) {
      found = 0;
      if ( (temp_al->mappt.appt.description) &&
	  (temp_al->mappt.appt.description[0]) ) {
	 if ( jp_strstr(temp_al->mappt.appt.description, needle,
	      GTK_TOGGLE_BUTTON(case_sense_checkbox)->active) ) {
	    found = 1;
	 }
      }
      if ( (temp_al->mappt.appt.note) &&
	  (temp_al->mappt.appt.note[0]) ) {
	 if ( jp_strstr(temp_al->mappt.appt.note, needle,
	      GTK_TOGGLE_BUTTON(case_sense_checkbox)->active) ) {
	    found = 2;
	 }
      }
      if (found) {
	 gtk_clist_prepend(GTK_CLIST(clist), empty_line);
	 gtk_clist_set_text(GTK_CLIST(clist), 0, 0, _("datebook"));

	 /*Add to the search list */
	 new_sr = malloc(sizeof(struct search_record));
	 new_sr->app_type = DATEBOOK;
	 new_sr->plugin_flag = 0;
	 new_sr->unique_id = temp_al->mappt.unique_id;
	 new_sr->next = search_rl;
	 search_rl = new_sr;

	 gtk_clist_set_row_data(GTK_CLIST(clist), 0, new_sr);

	 /*get the date */
	 get_pref(PREF_SHORTDATE, &ivalue, &svalue1);
	 if (svalue1 == NULL) {
	    strcpy(datef, "%x");
	 } else {
	    strncpy(datef, svalue1, sizeof(datef));
	 }
	 strftime(date_str, sizeof(date_str), datef, &temp_al->mappt.appt.begin);
	 date_str[sizeof(date_str)-1]='\0';

	 if (found == 1) {
#ifdef ENABLE_GTK2    
	    g_snprintf(str, sizeof(str), "%s\t%s",
#else
	    g_snprintf(str, sizeof(str), "%s  %s",
#endif
		       date_str,
		       temp_al->mappt.appt.description);
	    lstrncpy_remove_cr_lfs(str2, str, SEARCH_MAX_COLUMN_LEN);
	    gtk_clist_set_text(GTK_CLIST(clist), 0, 1, str2);
	 }
	 if (found == 2) {
#ifdef ENABLE_GTK2    
	    g_snprintf(str, sizeof(str), "%s\t%s",
#else
	    g_snprintf(str, sizeof(str), "%s  %s",
#endif
		       date_str,
		       temp_al->mappt.appt.note);
	    lstrncpy_remove_cr_lfs(str2, str, SEARCH_MAX_COLUMN_LEN);
	    gtk_clist_set_text(GTK_CLIST(clist), 0, 1, str2);
	 }

	 count++;
      }
   }

   jp_logf(JP_LOG_DEBUG, "calling free_AppointmentList\n");
   free_AppointmentList(&a_list);
   a_list = NULL;
   return count;
}

static int
  search_address(const char *needle, GtkWidget *clist)
{
   gchar *empty_line[] = { "","" };
   char str2[SEARCH_MAX_COLUMN_LEN+2];
   AddressList *a_list;
   AddressList *temp_al;
   struct search_record *new_sr;
   int i, count;

   /*Search Addresses */
   a_list = NULL;

   get_addresses2(&a_list, SORT_DESCENDING, 2, 2, 2, CATEGORY_ALL);

   if (a_list==NULL) {
      return 0;
   }

   count = 0;

   for (temp_al = a_list; temp_al; temp_al=temp_al->next) {
      for (i=0; i<19; i++) {
	 if (temp_al->maddr.addr.entry[i]) {
	    if ( jp_strstr(temp_al->maddr.addr.entry[i], needle,
			       GTK_TOGGLE_BUTTON(case_sense_checkbox)->active) ) {
	       gtk_clist_prepend(GTK_CLIST(clist), empty_line);
	       gtk_clist_set_text(GTK_CLIST(clist), 0, 0, _("address"));
	       lstrncpy_remove_cr_lfs(str2, temp_al->maddr.addr.entry[i], SEARCH_MAX_COLUMN_LEN);
	       gtk_clist_set_text(GTK_CLIST(clist), 0, 1, str2);

	       /*Add to the search list */
	       new_sr = malloc(sizeof(struct search_record));
	       new_sr->app_type = ADDRESS;
	       new_sr->plugin_flag = 0;
	       new_sr->unique_id = temp_al->maddr.unique_id;
	       new_sr->next = search_rl;
	       search_rl = new_sr;

	       gtk_clist_set_row_data(GTK_CLIST(clist), 0, new_sr);
	       count++;

	       break;
	    }
	 }
      }
   }
   jp_logf(JP_LOG_DEBUG, "calling free_AddressList\n");
   free_AddressList(&a_list);
   a_list = NULL;
   return count;
}

static int
  search_todo(const char *needle, GtkWidget *clist)
{
   gchar *empty_line[] = { "","" };
   char str2[SEARCH_MAX_COLUMN_LEN+2];
   ToDoList *todo_list;
   ToDoList *temp_todo;
   struct search_record *new_sr;
   int found, count;

   /*Search Appointments */
   todo_list = NULL;

   get_todos2(&todo_list, SORT_DESCENDING, 2, 2, 2, 1, CATEGORY_ALL);

   if (todo_list==NULL) {
      return 0;
   }

   count = 0;

   for (temp_todo = todo_list; temp_todo; temp_todo=temp_todo->next) {
      found = 0;
      if ( (temp_todo->mtodo.todo.description) &&
	  (temp_todo->mtodo.todo.description[0]) ) {
	 if ( jp_strstr(temp_todo->mtodo.todo.description, needle,
			    GTK_TOGGLE_BUTTON(case_sense_checkbox)->active) ) {
	    found = 1;
	 }
      }
      if ( (temp_todo->mtodo.todo.note) &&
	  (temp_todo->mtodo.todo.note[0]) ) {
	 if ( jp_strstr(temp_todo->mtodo.todo.note, needle,
			    GTK_TOGGLE_BUTTON(case_sense_checkbox)->active) ) {
	    found = 2;
	 }
      }
      if (found) {
	 gtk_clist_prepend(GTK_CLIST(clist), empty_line);
	 gtk_clist_set_text(GTK_CLIST(clist), 0, 0, _("todo"));

	 /*Add to the search list */
	 new_sr = malloc(sizeof(struct search_record));
	 new_sr->app_type = TODO;
	 new_sr->plugin_flag = 0;
	 new_sr->unique_id = temp_todo->mtodo.unique_id;
	 new_sr->next = search_rl;
	 search_rl = new_sr;

	 gtk_clist_set_row_data(GTK_CLIST(clist), 0, new_sr);
	 count++;

	 if (found == 1) {
	    lstrncpy_remove_cr_lfs(str2, temp_todo->mtodo.todo.description, SEARCH_MAX_COLUMN_LEN);
	    gtk_clist_set_text(GTK_CLIST(clist), 0, 1, str2);
	 }
	 if (found == 2) {
	    lstrncpy_remove_cr_lfs(str2, temp_todo->mtodo.todo.note, SEARCH_MAX_COLUMN_LEN);
	    gtk_clist_set_text(GTK_CLIST(clist), 0, 1, str2);
	 }
      }
   }
   jp_logf(JP_LOG_DEBUG, "calling free_ToDoList\n");
   free_ToDoList(&todo_list);
   todo_list = NULL;
   return count;
}

static int
  search_memo(const char *needle, GtkWidget *clist)
{
   gchar *empty_line[] = { "","" };
   char str2[SEARCH_MAX_COLUMN_LEN+2];
   MemoList *memo_list;
   MemoList *temp_memo;
   struct search_record *new_sr;
   int count;

   /*Search Memos */
   memo_list = NULL;

   get_memos2(&memo_list, SORT_DESCENDING, 2, 2, 2, CATEGORY_ALL);

   if (memo_list==NULL) {
      return 0;
   }

   count = 0;

   for (temp_memo = memo_list; temp_memo; temp_memo=temp_memo->next) {
      if (jp_strstr(temp_memo->mmemo.memo.text, needle,
		 GTK_TOGGLE_BUTTON(case_sense_checkbox)->active) ) {
	 gtk_clist_prepend(GTK_CLIST(clist), empty_line);
	 gtk_clist_set_text(GTK_CLIST(clist), 0, 0, _("memo"));
	 if (temp_memo->mmemo.memo.text) {
	    lstrncpy_remove_cr_lfs(str2, temp_memo->mmemo.memo.text, SEARCH_MAX_COLUMN_LEN);
	    gtk_clist_set_text(GTK_CLIST(clist), 0, 1, str2);
	 }

	 /*Add to the search list */
	 new_sr = malloc(sizeof(struct search_record));
	 new_sr->app_type = MEMO;
	 new_sr->plugin_flag = 0;
	 new_sr->unique_id = temp_memo->mmemo.unique_id;
	 new_sr->next = search_rl;
	 search_rl = new_sr;

	 gtk_clist_set_row_data(GTK_CLIST(clist), 0, new_sr);
	 count++;
      }
   }
   jp_logf(JP_LOG_DEBUG, "calling free_MemoList\n");
   free_MemoList(&memo_list);
   memo_list = NULL;
   return count;
}

#ifdef ENABLE_PLUGINS
static int
  search_plugins(const char *needle, const GtkWidget *clist)
{
   GList *plugin_list, *temp_list;
   gchar *empty_line[] = { "","" };
   char str2[SEARCH_MAX_COLUMN_LEN+2];
   int found;
   int count;
   int case_sense;
   struct search_result *sr, *temp_sr;
   struct plugin_s *plugin;
   struct search_record *new_sr;

   plugin_list = NULL;
   plugin_list = get_plugin_list();

   found = 0;
   case_sense = GTK_TOGGLE_BUTTON(case_sense_checkbox)->active;

   count = 0;
   for (temp_list = plugin_list; temp_list; temp_list = temp_list->next) {
      plugin = (struct plugin_s *)temp_list->data;
      if (plugin) {
	 sr = NULL;
	 if (plugin->plugin_search) {
	    if (plugin->plugin_search(needle, case_sense, &sr) > 0) {
	       for (temp_sr=sr; temp_sr; temp_sr=temp_sr->next) {
		  gtk_clist_prepend(GTK_CLIST(clist), empty_line);
		  if (plugin->menu_name) {
		     gtk_clist_set_text(GTK_CLIST(clist), 0, 0, plugin->menu_name);
		  } else {
		     gtk_clist_set_text(GTK_CLIST(clist), 0, 0, _("plugin ?"));
		  }
		  if (temp_sr->line) {
		     lstrncpy_remove_cr_lfs(str2, temp_sr->line, SEARCH_MAX_COLUMN_LEN);
		     gtk_clist_set_text(GTK_CLIST(clist), 0, 1, str2);
		  }

		  /*Add to the search list */
		  new_sr = malloc(sizeof(struct search_record));
		  new_sr->app_type = plugin->number;
		  new_sr->plugin_flag = 1;
		  new_sr->unique_id = temp_sr->unique_id;
		  new_sr->next = search_rl;
		  search_rl = new_sr;

		  gtk_clist_set_row_data(GTK_CLIST(clist), 0, new_sr);
		  count++;
	       }
	       free_search_result(&sr);
	    }
	 }
      }
   }

   return count;
}
#endif


static gboolean cb_destroy(GtkWidget *widget)
{
   if (search_rl) {
      free_search_record_list(&search_rl);
      search_rl = NULL;
   }
   window = NULL;
   return FALSE;
}

static void
  cb_quit(GtkWidget *widget,
	   gpointer   data)
{
   gtk_widget_destroy(data);
}

static void
  cb_entry(GtkWidget *widget,
	   gpointer   data)
{
   gchar *empty_line[] = { "","" };
   GtkWidget *clist;
   const char *entry_text;
   int count;

   jp_logf(JP_LOG_DEBUG, "enter cb_entry\n");

   clist = data;
   entry_text = gtk_entry_get_text(GTK_ENTRY(widget));
   if (!entry_text || !strlen(entry_text)) {
      return;
   }

   jp_logf(JP_LOG_DEBUG, "entry text = %s\n", entry_text);

   gtk_clist_clear(GTK_CLIST(clist));

   count = search_datebook(entry_text, clist);
   count += search_address(entry_text, clist);
   count += search_todo(entry_text, clist);
   count += search_memo(entry_text, clist);
#ifdef ENABLE_PLUGINS
   count += search_plugins(entry_text, clist);
#endif

   if (count == 0) {
      gtk_clist_prepend(GTK_CLIST(clist), empty_line);
      gtk_clist_set_text(GTK_CLIST(clist), 0, 1, _("No records found"));
   }

   return;
}

static void
  cb_search(GtkWidget *widget,
	   gpointer   data)
{
	cb_entry(entry, data);
}

static void
  cb_clist_select(GtkWidget      *clist,
		  gint           row,
		  gint           column,
		  GdkEventButton *event,
		  gpointer       data)
{

   struct search_record *sr;

   if (!event) return;

   sr = gtk_clist_get_row_data(GTK_CLIST(clist), row);
   if (sr == NULL) {
      return;
   }
   switch (sr->app_type) {
    case DATEBOOK:
      glob_find_id = sr->unique_id;
      cb_app_button(NULL, GINT_TO_POINTER(DATEBOOK));
      break;
    case ADDRESS:
      glob_find_id = sr->unique_id;
      cb_app_button(NULL, GINT_TO_POINTER(ADDRESS));
      break;
    case TODO:
      glob_find_id = sr->unique_id;
      cb_app_button(NULL, GINT_TO_POINTER(TODO));
      break;
    case MEMO:
      glob_find_id = sr->unique_id;
      cb_app_button(NULL, GINT_TO_POINTER(MEMO));
      break;
    default:
#ifdef ENABLE_PLUGINS
      /* We didn't find it so it must be a plugin */
      jp_logf(JP_LOG_DEBUG, "choosing search result from plugin %d\n", sr->app_type);
      call_plugin_gui(sr->app_type, sr->unique_id);
#endif
      break;
   }
}

void cb_search_gui(GtkWidget *widget, gpointer data)
{
   GtkWidget *scrolled_window;
   GtkWidget *clist;
   GtkWidget *label;
   GtkWidget *button;
   GtkWidget *vbox, *hbox;
   char temp[256];

   if (GTK_IS_WIDGET(window)) {
#ifdef ENABLE_GTK2
      /* Shift focus to existing window if called again 
         and window is still alive. */
      gtk_window_present(GTK_WINDOW(window));
#endif
      return;
   }

   if (search_rl) {
      free_search_record_list(&search_rl);
      search_rl = NULL;
   }

   window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
   gtk_window_set_default_size(GTK_WINDOW(window), 500, 300);
   gtk_container_set_border_width(GTK_CONTAINER(window), 10);
   g_snprintf(temp, sizeof(temp), "%s %s", PN, _("Search"));
   gtk_window_set_title(GTK_WINDOW(window), temp);

   gtk_signal_connect(GTK_OBJECT(window), "destroy",
                      GTK_SIGNAL_FUNC(cb_destroy), window);

   accel_group = gtk_accel_group_new();
   gtk_window_add_accel_group(GTK_WINDOW(window), accel_group);

   vbox = gtk_vbox_new(FALSE, 0);
   gtk_container_add(GTK_CONTAINER(window), vbox);

   hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

   label = gtk_label_new(_("Search for: "));
   gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

   entry = gtk_entry_new();
   gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);
   gtk_widget_grab_focus(GTK_WIDGET(entry));

   case_sense_checkbox = gtk_check_button_new_with_label(_("Case Sensitive"));
   gtk_box_pack_start(GTK_BOX(hbox), case_sense_checkbox, FALSE, FALSE, 0);
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(case_sense_checkbox),
				FALSE);

   /*Put the scrolled window up */
   scrolled_window = gtk_scrolled_window_new(NULL, NULL);
   gtk_container_set_border_width(GTK_CONTAINER(scrolled_window), 0);
   gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
   gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 0);

   clist = gtk_clist_new(2);
   gtk_signal_connect(GTK_OBJECT(clist), "select_row",
		      GTK_SIGNAL_FUNC(cb_clist_select),
		      NULL);
   gtk_clist_set_shadow_type(GTK_CLIST(clist), SHADOW);
   gtk_clist_set_selection_mode(GTK_CLIST(clist), GTK_SELECTION_BROWSE);
   gtk_clist_set_column_auto_resize(GTK_CLIST(clist), 0, TRUE);
   gtk_clist_set_column_auto_resize(GTK_CLIST(clist), 1, TRUE);

   gtk_container_add(GTK_CONTAINER(scrolled_window), GTK_WIDGET(clist));

   gtk_signal_connect(GTK_OBJECT(entry), "activate",
		      GTK_SIGNAL_FUNC(cb_entry),
		      clist);

   hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

   /* Create a "Search" button */
   button = gtk_button_new_with_label(_("Search"));
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_search), clist);
   gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);

   /* Create a "Done" button */
   button = gtk_button_new_with_label(_("Close"));
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_quit), window);
   gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
   gtk_widget_add_accelerator(button, "clicked", accel_group, GDK_Escape, 0, 0);

   gtk_widget_show_all(window);
}


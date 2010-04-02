/* $Id: search_gui.c,v 1.53 2010/04/02 04:26:27 rikster5 Exp $ */

/*******************************************************************************
 * search_gui.c
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
 ******************************************************************************/

/********************************* Includes ***********************************/
#include "config.h"
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <pi-calendar.h>
#include <pi-address.h>
#include <pi-todo.h>
#include <pi-memo.h>

#include "i18n.h"
#include "utils.h"
#include "prefs.h"
#include "log.h"
#include "datebook.h"
#include "calendar.h"
#include "address.h"
#include "todo.h"
#include "memo.h"
#ifdef ENABLE_PLUGINS
#  include "plugins.h"
#endif

/********************************* Constants **********************************/
#define SEARCH_MAX_COLUMN_LEN 80

/******************************* Global vars **********************************/
static struct search_record *search_rl = NULL;
static GtkWidget *case_sense_checkbox;
static GtkWidget *window = NULL;
static GtkWidget *entry = NULL;
static GtkAccelGroup *accel_group = NULL;

static int clist_row_selected;

/****************************** Prototypes ************************************/
static void cb_clist_selection(GtkWidget *clist, gint row, gint column,
                               GdkEventButton *event, gpointer data);

/****************************** Main Code *************************************/
static int datebook_search_sort_compare(const void *v1, const void *v2)
{
   CalendarEventList **cel1, **cel2;
   struct CalendarEvent *ce1, *ce2;
   time_t time1, time2;

   cel1=(CalendarEventList **)v1;
   cel2=(CalendarEventList **)v2;

   ce1=&((*cel1)->mcale.cale);
   ce2=&((*cel2)->mcale.cale);

   time1 = mktime(&(ce1->begin));
   time2 = mktime(&(ce2->begin));

   return(time2 - time1);
}

static int search_datebook(const char *needle, GtkWidget *clist)
{
   gchar *empty_line[] = { "","" };
   CalendarEventList *ce_list;
   CalendarEventList *temp_cel;
   int found, count;
   int case_sense;
   char str[202];
   char str2[SEARCH_MAX_COLUMN_LEN+2];
   char date_str[52];
   char datef[52];
   const char *svalue1;
   struct search_record *new_sr;
   long datebook_version=0;

   get_pref(PREF_DATEBOOK_VERSION, &datebook_version, NULL);
  
   /* Search Appointments */
   ce_list = NULL;

   get_days_calendar_events2(&ce_list, NULL, 2, 2, 2, CATEGORY_ALL, NULL);

   if (ce_list==NULL) {
      return 0;
   }

   /* Sort returned results according to date rather than just HH:MM */
   calendar_sort(&ce_list, datebook_search_sort_compare);

   count = 0;
   case_sense = GTK_TOGGLE_BUTTON(case_sense_checkbox)->active;

   for (temp_cel = ce_list; temp_cel; temp_cel=temp_cel->next) {
      found = 0;
      if ( (temp_cel->mcale.cale.description) &&
           (temp_cel->mcale.cale.description[0]) ) {
         if (jp_strstr(temp_cel->mcale.cale.description, needle, case_sense)) {
            found = 1;
         }
      }
      if ( !found &&
           (temp_cel->mcale.cale.note) &&
           (temp_cel->mcale.cale.note[0]) ) {
         if (jp_strstr(temp_cel->mcale.cale.note, needle, case_sense )) {
            found = 2;
         }
      }
      if (datebook_version) {
         if ( !found &&
              (temp_cel->mcale.cale.location) &&
              (temp_cel->mcale.cale.location[0]) ) {
            if (jp_strstr(temp_cel->mcale.cale.location, needle, case_sense )) {
               found = 3;
            }
         }
      }

      if (found) {
         gtk_clist_prepend(GTK_CLIST(clist), empty_line);
         if (datebook_version==0) {
            gtk_clist_set_text(GTK_CLIST(clist), 0, 0, _("datebook"));
         } else {
            gtk_clist_set_text(GTK_CLIST(clist), 0, 0, _("calendar"));
         }

         /* get the date */
         get_pref(PREF_SHORTDATE, NULL, &svalue1);
         if (svalue1 == NULL) {
            strcpy(datef, "%x");
         } else {
            strncpy(datef, svalue1, sizeof(datef));
         }
         strftime(date_str, sizeof(date_str), datef, &temp_cel->mcale.cale.begin);
         date_str[sizeof(date_str)-1]='\0';

         if (found == 1) {
            g_snprintf(str, sizeof(str), "%s\t%s",
                       date_str,
                       temp_cel->mcale.cale.description);
         } else if (found == 2) {
            g_snprintf(str, sizeof(str), "%s\t%s",
                       date_str,
                       temp_cel->mcale.cale.note);
         } else {
            g_snprintf(str, sizeof(str), "%s\t%s",
                       date_str,
                       temp_cel->mcale.cale.location);
         }
         lstrncpy_remove_cr_lfs(str2, str, SEARCH_MAX_COLUMN_LEN);
         gtk_clist_set_text(GTK_CLIST(clist), 0, 1, str2);

         /* Add to the search list */
         new_sr = malloc(sizeof(struct search_record));
         new_sr->app_type = DATEBOOK;
         new_sr->plugin_flag = 0;
         new_sr->unique_id = temp_cel->mcale.unique_id;
         new_sr->next = search_rl;
         search_rl = new_sr;

         gtk_clist_set_row_data(GTK_CLIST(clist), 0, new_sr);

         count++;
      }
   }

   jp_logf(JP_LOG_DEBUG, "calling free_CalendarEventList\n");
   free_CalendarEventList(&ce_list);

   return count;
}

static int search_address_or_contacts(const char *needle, GtkWidget *clist)
{
   gchar *empty_line[] = { "","" };
   char str2[SEARCH_MAX_COLUMN_LEN+2];
   AddressList *addr_list;
   ContactList *cont_list;
   ContactList *temp_cl;
   struct search_record *new_sr;
   int i, count;
   int case_sense;
   long address_version=0;

   get_pref(PREF_ADDRESS_VERSION, &address_version, NULL);

   /* Get addresses and move to a contacts structure, or get contacts directly */
   if (address_version==0) {
      addr_list = NULL;
      get_addresses2(&addr_list, SORT_ASCENDING, 2, 2, 2, CATEGORY_ALL);
      copy_addresses_to_contacts(addr_list, &cont_list);
      free_AddressList(&addr_list);
   } else {
      cont_list = NULL;
      get_contacts2(&cont_list, SORT_ASCENDING, 2, 2, 2, CATEGORY_ALL);
   }

   if (cont_list==NULL) {
      return 0;
   }

   count = 0;
   case_sense = GTK_TOGGLE_BUTTON(case_sense_checkbox)->active;

   for (temp_cl = cont_list; temp_cl; temp_cl=temp_cl->next) {
      for (i=0; i<NUM_CONTACT_ENTRIES; i++) {
         if (temp_cl->mcont.cont.entry[i]) {
            if ( jp_strstr(temp_cl->mcont.cont.entry[i], needle, case_sense) ) {
               gtk_clist_prepend(GTK_CLIST(clist), empty_line);
               if (address_version==0) {
                  gtk_clist_set_text(GTK_CLIST(clist), 0, 0, _("address"));
               } else {
                  gtk_clist_set_text(GTK_CLIST(clist), 0, 0, _("contact"));
               }
               lstrncpy_remove_cr_lfs(str2, temp_cl->mcont.cont.entry[i], SEARCH_MAX_COLUMN_LEN);
               gtk_clist_set_text(GTK_CLIST(clist), 0, 1, str2);

               /* Add to the search list */
               new_sr = malloc(sizeof(struct search_record));
               new_sr->app_type = ADDRESS;
               new_sr->plugin_flag = 0;
               new_sr->unique_id = temp_cl->mcont.unique_id;
               new_sr->next = search_rl;
               search_rl = new_sr;

               gtk_clist_set_row_data(GTK_CLIST(clist), 0, new_sr);

               count++;

               break;
            }
         }
      }
   }

   jp_logf(JP_LOG_DEBUG, "calling free_ContactList\n");
   free_ContactList(&cont_list);

   return count;
}

static int search_todo(const char *needle, GtkWidget *clist)
{
   gchar *empty_line[] = { "","" };
   char str2[SEARCH_MAX_COLUMN_LEN+2];
   ToDoList *todo_list;
   ToDoList *temp_todo;
   struct search_record *new_sr;
   int found, count;
   int case_sense;

   /* Search Appointments */
   todo_list = NULL;

   get_todos2(&todo_list, SORT_DESCENDING, 2, 2, 2, 1, CATEGORY_ALL);

   if (todo_list==NULL) {
      return 0;
   }

   count = 0;
   case_sense = GTK_TOGGLE_BUTTON(case_sense_checkbox)->active;

   for (temp_todo = todo_list; temp_todo; temp_todo=temp_todo->next) {
      found = 0;
      if ( (temp_todo->mtodo.todo.description) &&
           (temp_todo->mtodo.todo.description[0]) ) {
         if ( jp_strstr(temp_todo->mtodo.todo.description, needle, case_sense) ) {
            found = 1;
         }
      }
      if ( !found &&
           (temp_todo->mtodo.todo.note) &&
           (temp_todo->mtodo.todo.note[0]) ) {
         if ( jp_strstr(temp_todo->mtodo.todo.note, needle, case_sense) ) {
            found = 2;
         }
      }

      if (found) {
         gtk_clist_prepend(GTK_CLIST(clist), empty_line);
         gtk_clist_set_text(GTK_CLIST(clist), 0, 0, _("todo"));
         if (found == 1) {
            lstrncpy_remove_cr_lfs(str2, temp_todo->mtodo.todo.description, SEARCH_MAX_COLUMN_LEN);
         } else {
            lstrncpy_remove_cr_lfs(str2, temp_todo->mtodo.todo.note, SEARCH_MAX_COLUMN_LEN);
         }
         gtk_clist_set_text(GTK_CLIST(clist), 0, 1, str2);

         /* Add to the search list */
         new_sr = malloc(sizeof(struct search_record));
         new_sr->app_type = TODO;
         new_sr->plugin_flag = 0;
         new_sr->unique_id = temp_todo->mtodo.unique_id;
         new_sr->next = search_rl;
         search_rl = new_sr;

         gtk_clist_set_row_data(GTK_CLIST(clist), 0, new_sr);

         count++;
      }
   }

   jp_logf(JP_LOG_DEBUG, "calling free_ToDoList\n");
   free_ToDoList(&todo_list);

   return count;
}

static int search_memo(const char *needle, GtkWidget *clist)
{
   gchar *empty_line[] = { "","" };
   char str2[SEARCH_MAX_COLUMN_LEN+2];
   MemoList *memo_list;
   MemoList *temp_memo;
   struct search_record *new_sr;
   int count;
   int case_sense;

   /* Search Memos */
   memo_list = NULL;

   get_memos2(&memo_list, SORT_DESCENDING, 2, 2, 2, CATEGORY_ALL);

   if (memo_list==NULL) {
      return 0;
   }

   count = 0;
   case_sense = GTK_TOGGLE_BUTTON(case_sense_checkbox)->active;

   for (temp_memo = memo_list; temp_memo; temp_memo=temp_memo->next) {
      if (jp_strstr(temp_memo->mmemo.memo.text, needle, case_sense) ) {
         gtk_clist_prepend(GTK_CLIST(clist), empty_line);
         gtk_clist_set_text(GTK_CLIST(clist), 0, 0, _("memo"));
         if (temp_memo->mmemo.memo.text) {
            lstrncpy_remove_cr_lfs(str2, temp_memo->mmemo.memo.text, SEARCH_MAX_COLUMN_LEN);
            gtk_clist_set_text(GTK_CLIST(clist), 0, 1, str2);
         }

         /* Add to the search list */
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

   return count;
}

#ifdef ENABLE_PLUGINS
static int search_plugins(const char *needle, const GtkWidget *clist)
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

                  /* Add to the search list */
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

static void cb_quit(GtkWidget *widget, gpointer data)
{
   gtk_widget_destroy(data);
}

static void cb_entry(GtkWidget *widget, gpointer data)
{
   gchar *empty_line[] = { "","" };
   GtkWidget *clist;
   const char *entry_text;
   int count = 0;

   jp_logf(JP_LOG_DEBUG, "enter cb_entry\n");

   entry_text = gtk_entry_get_text(GTK_ENTRY(widget));
   if (!entry_text || !strlen(entry_text)) {
      return;
   }

   jp_logf(JP_LOG_DEBUG, "entry text = %s\n", entry_text);

   clist = data;
   gtk_clist_clear(GTK_CLIST(clist));

   count += search_address_or_contacts(entry_text, clist);
   count += search_todo(entry_text, clist);
   count += search_memo(entry_text, clist);
#ifdef ENABLE_PLUGINS
   count += search_plugins(entry_text, clist);
#endif

   /* sort the results */
   gtk_clist_set_sort_column(GTK_CLIST(clist), 1);
   gtk_clist_sort(GTK_CLIST(clist));

   /* the datebook events are already sorted by date */
   count += search_datebook(entry_text, clist);

   if (count == 0) {
      gtk_clist_prepend(GTK_CLIST(clist), empty_line);
      gtk_clist_set_text(GTK_CLIST(clist), 0, 1, _("No records found"));
   }

   /* Highlight the first row in the list of returned items.
    * This does NOT cause the main window to jump to the selected record. */
   clist_select_row(GTK_CLIST(clist), 0, 0);

   /* select the first record found */
   cb_clist_selection(clist, 0, 0, (GdkEventButton *)1, NULL);

   return;
}

static void cb_search(GtkWidget *widget, gpointer data)
{
   cb_entry(entry, data);
}

static void cb_clist_selection(GtkWidget      *clist,
                               gint           row,
                               gint           column,
                               GdkEventButton *event,
                               gpointer       data)
{
   struct search_record *sr;

   clist_row_selected = row;

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
      /* Not one of the main 4 apps so it must be a plugin */
      jp_logf(JP_LOG_DEBUG, "choosing search result from plugin %d\n", sr->app_type);
      call_plugin_gui(sr->app_type, sr->unique_id);
#endif
      break;
   }
}

static gboolean cb_key_pressed_in_clist(GtkWidget   *widget, 
                                        GdkEventKey *event,
                                        gpointer     data)
{
   if (event->keyval == GDK_Return) {
      gtk_signal_emit_stop_by_name(GTK_OBJECT(widget), "key_press_event");
      cb_clist_selection(widget, clist_row_selected, 0, (GdkEventButton *)1, NULL);

      return TRUE;
   }

   return FALSE;
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
      /* Shift focus to existing window if called again
         and window is still alive. */
      gtk_window_present(GTK_WINDOW(window));
      gtk_widget_grab_focus(GTK_WIDGET(entry));
      return;
   }

   if (search_rl) {
      free_search_record_list(&search_rl);
      search_rl = NULL;
   }

   window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
   gtk_window_set_default_size(GTK_WINDOW(window), 500, 300);
   g_snprintf(temp, sizeof(temp), "%s %s", PN, _("Search"));
   gtk_window_set_title(GTK_WINDOW(window), temp);

   gtk_signal_connect(GTK_OBJECT(window), "destroy",
                      GTK_SIGNAL_FUNC(cb_destroy), window);

   accel_group = gtk_accel_group_new();
   gtk_window_add_accel_group(GTK_WINDOW(window), accel_group);

   vbox = gtk_vbox_new(FALSE, 0);
   gtk_container_add(GTK_CONTAINER(window), vbox);

   hbox = gtk_hbox_new(FALSE, 0);
   gtk_container_set_border_width(GTK_CONTAINER(hbox), 6);
   gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

   /* Search label */
   label = gtk_label_new(_("Search for: "));
   gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

   /* Search entry */
   entry = gtk_entry_new();
   gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);
   gtk_widget_grab_focus(GTK_WIDGET(entry));

   /* Case Sensitive checkbox */
   case_sense_checkbox = gtk_check_button_new_with_label(_("Case Sensitive"));
   gtk_box_pack_start(GTK_BOX(hbox), case_sense_checkbox, FALSE, FALSE, 0);
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(case_sense_checkbox),
                                FALSE);

   /* Scrolled window for search results */
   scrolled_window = gtk_scrolled_window_new(NULL, NULL);
   gtk_container_set_border_width(GTK_CONTAINER(scrolled_window), 3);
   gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
   gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 0);

   clist = gtk_clist_new(2);
   gtk_signal_connect(GTK_OBJECT(clist), "select_row",
                      GTK_SIGNAL_FUNC(cb_clist_selection),
                      NULL);
   gtk_signal_connect(GTK_OBJECT(clist), "key_press_event",
                      GTK_SIGNAL_FUNC(cb_key_pressed_in_clist), 
                      NULL);
   gtk_clist_set_shadow_type(GTK_CLIST(clist), SHADOW);
   gtk_clist_set_selection_mode(GTK_CLIST(clist), GTK_SELECTION_BROWSE);
   gtk_clist_set_column_auto_resize(GTK_CLIST(clist), 0, TRUE);
   gtk_clist_set_column_auto_resize(GTK_CLIST(clist), 1, TRUE);

   gtk_container_add(GTK_CONTAINER(scrolled_window), GTK_WIDGET(clist));

   gtk_signal_connect(GTK_OBJECT(entry), "activate",
                      GTK_SIGNAL_FUNC(cb_entry),
                      clist);

   hbox = gtk_hbutton_box_new();
   gtk_container_set_border_width(GTK_CONTAINER(hbox), 6);
   gtk_button_box_set_layout(GTK_BUTTON_BOX (hbox), GTK_BUTTONBOX_END);
   gtk_button_box_set_spacing(GTK_BUTTON_BOX(hbox), 6);
   gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

   /* Search button */
   button = gtk_button_new_from_stock(GTK_STOCK_FIND);
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
                      GTK_SIGNAL_FUNC(cb_search), clist);
   gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);

   /* clicking on "Case Sensitive" also starts a search */
   gtk_signal_connect(GTK_OBJECT(case_sense_checkbox), "clicked",
                      GTK_SIGNAL_FUNC(cb_search), clist);

   /* Done button */
   button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
                      GTK_SIGNAL_FUNC(cb_quit), window);
   gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
   gtk_widget_add_accelerator(button, "clicked", accel_group, GDK_Escape, 0, 0);

   gtk_widget_show_all(window);
}


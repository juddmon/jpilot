/* prefs_gui.c
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
#include <string.h>
#include "utils.h"
#include "prefs.h"
#include "prefs_gui.h"
#include "log.h"

GtkWidget *show_deleted_checkbutton;
GtkWidget *show_modified_checkbutton;
static GtkWidget *window;
static GtkWidget *port_entry;


static void cb_pref_menu(GtkWidget *widget,
			 gpointer   data)
{
   int pref;
   int value;
   
   if (!widget)
     return;
   if (!(GTK_CHECK_MENU_ITEM(widget))->active) {
      return;
   }

   pref = GPOINTER_TO_INT(data);
   value = pref & 0xFF;
   pref = pref >> 8;
   set_pref(pref, value);
   jpilot_logf(LOG_DEBUG, "pref %d, value %d\n", pref, value);

   return;
}
   

static int make_pref_menu(GtkWidget **pref_menu, int pref_num)
{
   GtkWidget *menu_item;
   GtkWidget *menu;
   GSList    *group;
   int i, r, ivalue;
   const char *svalue;
   char format_text[MAX_PREF_VALUE];
   char human_text[MAX_PREF_VALUE];
   time_t ltime;
   struct tm *now;
      
   time(&ltime);
   now = localtime(&ltime);

   *pref_menu = gtk_option_menu_new();
   
   menu = gtk_menu_new();
   group = NULL;
   
   get_pref(pref_num, &ivalue, &svalue);
	    
   for (i=0; i<1000; i++) {
      r = get_pref_possibility(pref_num, i, format_text);
      if (r) {
	 break;
      }
      switch (pref_num) {
       case PREF_SHORTDATE:
       case PREF_LONGDATE:
       case PREF_TIME:
	 strftime(human_text, MAX_PREF_VALUE, format_text, now);
	 break;
       default:
	 strncpy(human_text, format_text, MAX_PREF_VALUE);
	 break;
      }
      menu_item = gtk_radio_menu_item_new_with_label(
		     group, human_text);
      gtk_signal_connect(GTK_OBJECT(menu_item), "activate", cb_pref_menu,
			 GINT_TO_POINTER(((pref_num*0x100) + (i & 0xFF))));
      group = gtk_radio_menu_item_group(GTK_RADIO_MENU_ITEM(menu_item));
      gtk_menu_append(GTK_MENU(menu), menu_item);
      
      if (ivalue == i) {
	 gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_item), ivalue);
      }

      gtk_widget_show(menu_item);
   }
   gtk_option_menu_set_menu(GTK_OPTION_MENU(*pref_menu), menu);
   
   return 0;
}

void cb_show_deleted(GtkWidget *widget,
		     gpointer data)
{
   set_pref(PREF_SHOW_DELETED, GTK_TOGGLE_BUTTON(show_deleted_checkbutton)->active);
}
void cb_show_modified(GtkWidget *widget,
		      gpointer data)
{
   set_pref(PREF_SHOW_MODIFIED, GTK_TOGGLE_BUTTON(show_modified_checkbutton)->active);
}


static gboolean cb_destroy(GtkWidget *widget)
{
   char *entry_text;
   
   entry_text = gtk_entry_get_text(GTK_ENTRY(port_entry));
   jpilot_logf(LOG_DEBUG, "port_entry = [%s]\n", entry_text);
   set_pref_char(PREF_PORT, entry_text);
   
   jpilot_logf(LOG_DEBUG, "Cleanup\n");
   free_prefs();
   window = NULL;
   return FALSE;
}

static void
  cb_quit(GtkWidget *widget,
	   gpointer   data)
{
   jpilot_logf(LOG_DEBUG, "cb_quit\n");
   if (GTK_IS_WIDGET(data)) {
      gtk_widget_destroy(data);
   }
}

void cb_prefs_gui(GtkWidget *widget, gpointer data)
{
   GtkWidget *pref_menu;
   GtkWidget *label;
   GtkWidget *button;
   GtkWidget *table;
   GtkWidget *vbox;
   int ivalue;
   const char *cstr;
   
   jpilot_logf(LOG_DEBUG, "cb_prefs_gui\n");
   if (GTK_IS_WINDOW(window)) {
      jpilot_logf(LOG_DEBUG, "pref_window is already up\n");
      return;
   }

   window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
   //gtk_window_set_default_size(GTK_WINDOW(window), 500, 300);

   gtk_container_set_border_width(GTK_CONTAINER(window), 10);
   gtk_window_set_title(GTK_WINDOW(window), PN" Preferences");

   gtk_signal_connect(GTK_OBJECT(window), "destroy",
                      GTK_SIGNAL_FUNC(cb_destroy), window);

   vbox = gtk_vbox_new(FALSE, 0);
   gtk_container_add(GTK_CONTAINER(window), vbox);

   // Table
   table = gtk_table_new(7, 2, FALSE);
   gtk_table_set_row_spacings(GTK_TABLE(table),0);
   gtk_table_set_col_spacings(GTK_TABLE(table),0);
   gtk_box_pack_start (GTK_BOX (vbox), table, FALSE, FALSE, 0);

   // Shortdate
   label = gtk_label_new("Short date format ");
   gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(label),
			     0, 1, 0, 1);
   gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);

   make_pref_menu(&pref_menu, PREF_SHORTDATE);
   gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(pref_menu),
			     1, 2, 0, 1);

   get_pref(PREF_SHORTDATE, &ivalue, &cstr);
   gtk_option_menu_set_history(GTK_OPTION_MENU(pref_menu), ivalue);

   // Longdate
   label = gtk_label_new("Long date format ");
   gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(label),
			     0, 1, 1, 2);
   gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);

   make_pref_menu(&pref_menu, PREF_LONGDATE);
   gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(pref_menu),
			     1, 2, 1, 2);

   get_pref(PREF_LONGDATE, &ivalue, &cstr);
   gtk_option_menu_set_history(GTK_OPTION_MENU(pref_menu), ivalue);


   // Time
   label = gtk_label_new("Time format ");
   gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(label),
			     0, 1, 2, 3);
   gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);

   make_pref_menu(&pref_menu, PREF_TIME);
   gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(pref_menu),
			     1, 2, 2, 3);

   get_pref(PREF_TIME, &ivalue, &cstr);
   gtk_option_menu_set_history(GTK_OPTION_MENU(pref_menu), ivalue);


   // FDOW
   label = gtk_label_new("The first day of the week is ");
   gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(label),
			     0, 1, 3, 4);
   gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);

   make_pref_menu(&pref_menu, PREF_FDOW);
   gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(pref_menu),
			     1, 2, 3, 4);

   get_pref(PREF_FDOW, &ivalue, &cstr);
   gtk_option_menu_set_history(GTK_OPTION_MENU(pref_menu), ivalue);


   // GTK colors file
   label = gtk_label_new("My GTK colors file is ");
   gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(label),
			     0, 1, 4, 5);
   gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);

   make_pref_menu(&pref_menu, PREF_RCFILE);
   gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(pref_menu),
			     1, 2, 4, 5);
   
   get_pref(PREF_RCFILE, &ivalue, &cstr);
   gtk_option_menu_set_history(GTK_OPTION_MENU(pref_menu), ivalue);


   // Rate
   label = gtk_label_new("Serial Rate ");
   gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(label),
			     0, 1, 6, 7);
   gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);

   make_pref_menu(&pref_menu, PREF_RATE);
   gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(pref_menu),
			     1, 2, 6, 7);

   get_pref(PREF_RATE, &ivalue, &cstr);
   gtk_option_menu_set_history(GTK_OPTION_MENU(pref_menu), ivalue);

   // Port
   label = gtk_label_new("Serial Port ");
   gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(label),
			     0, 1, 5, 6);
   gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);

   port_entry = gtk_entry_new_with_max_length(MAX_PREF_VALUE - 2);
   gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(port_entry),
			     1, 2, 5, 6);
   get_pref(PREF_PORT, &ivalue, &cstr);
   if (cstr) {
      gtk_entry_set_text(GTK_ENTRY(port_entry), cstr);
   }


   //Show deleted files check box
   show_deleted_checkbutton = gtk_check_button_new_with_label
     ("Show deleted records (default NO)");
   gtk_box_pack_start(GTK_BOX(vbox), show_deleted_checkbutton, FALSE, FALSE, 0);
   get_pref(PREF_SHOW_DELETED, &ivalue, &cstr);
   gtk_widget_show(show_deleted_checkbutton);
   if (ivalue) {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(show_deleted_checkbutton), TRUE);
   }
   gtk_signal_connect_object(GTK_OBJECT(show_deleted_checkbutton), 
			     "clicked", GTK_SIGNAL_FUNC(cb_show_deleted),
			     GINT_TO_POINTER(PREF_SHOW_DELETED));

   //Show modified files check box
   show_modified_checkbutton = gtk_check_button_new_with_label
     ("Show modified deleted records (default NO)");
   gtk_box_pack_start(GTK_BOX(vbox), show_modified_checkbutton, FALSE, FALSE, 0);
   get_pref(PREF_SHOW_MODIFIED, &ivalue, &cstr);
   gtk_widget_show(show_modified_checkbutton);
   if (ivalue) {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(show_modified_checkbutton), TRUE);
   }
   gtk_signal_connect_object(GTK_OBJECT(show_modified_checkbutton), 
			     "clicked", GTK_SIGNAL_FUNC(cb_show_modified),
			     GINT_TO_POINTER(PREF_SHOW_MODIFIED));


// Create a "Quit" button
   button = gtk_button_new_with_label("Done");
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_quit), window);
   gtk_box_pack_end(GTK_BOX(vbox), button, FALSE, FALSE, 0);

   gtk_widget_show_all(window);
}

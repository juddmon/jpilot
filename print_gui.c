/* print_gui.c
 *
 * Copyright (C) 2000 by Judd Montgomery
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
#include <stdlib.h>
#include <string.h>
#include "utils.h"
#include "prefs.h"
#include "prefs_gui.h"
#include "log.h"


static GtkWidget *lines_entry;
static GtkWidget *print_command_entry;
static GtkWidget *one_record_checkbutton;
static GtkWidget *window;
static GtkWidget *radio_button_one;
static GtkWidget *radio_button_shown;
static GtkWidget *radio_button_all;

static int print_dialog;

static gboolean cb_destroy(GtkWidget *widget)
{
   char *entry_text;
   char *lines_text;
   int num_lines;
   
   jpilot_logf(LOG_DEBUG, "Cleanup print_gui\n");

   /* Get radio button prefs */
   if (GTK_TOGGLE_BUTTON(radio_button_one)->active) {
      jpilot_logf(LOG_DEBUG, "print one");
      set_pref(PREF_PRINT_THIS_MANY, 1);
   }
   if (GTK_TOGGLE_BUTTON(radio_button_shown)->active) {
      jpilot_logf(LOG_DEBUG, "print shown");
      set_pref(PREF_PRINT_THIS_MANY, 2);
   }
   if (GTK_TOGGLE_BUTTON(radio_button_all)->active) {
      jpilot_logf(LOG_DEBUG, "print all");
      set_pref(PREF_PRINT_THIS_MANY, 3);
   }

   /* Get one record per page pref */
   if (GTK_TOGGLE_BUTTON(one_record_checkbutton)->active) {
      jpilot_logf(LOG_DEBUG, "one record per page");
      set_pref(PREF_PRINT_ONE_PER_PAGE, 1);
   } else {
      set_pref(PREF_PRINT_ONE_PER_PAGE, 0);
   }

   /* Get number of blank lines */
   lines_text = gtk_entry_get_text(GTK_ENTRY(lines_entry));
   jpilot_logf(LOG_DEBUG, "lines_entry = [%s]\n", lines_text);
   num_lines = atoi(lines_text);
   if (num_lines < 0) {
      num_lines = 0;
   }
   if (num_lines > 99) {
      num_lines = 99;
   }

   set_pref(PREF_NUM_BLANK_LINES, num_lines);
   
   /* Get print command */
   entry_text = gtk_entry_get_text(GTK_ENTRY(print_command_entry));
   jpilot_logf(LOG_DEBUG, "print_command_entry = [%s]\n", entry_text);
   set_pref_char(PREF_PRINT_COMMAND, entry_text);
   
   window = NULL;
   gtk_main_quit();
   return FALSE;
}

static void cb_print(GtkWidget *widget, gpointer data)
{
   jpilot_logf(LOG_DEBUG, "cb_print\n");
   if (GTK_IS_WIDGET(data)) {
      gtk_widget_destroy(data);
   }
   print_dialog=DIALOG_SAID_PRINT;
}

static void cb_cancel(GtkWidget *widget, gpointer data)
{
   jpilot_logf(LOG_DEBUG, "cb_cancel\n");
   if (GTK_IS_WIDGET(data)) {
      gtk_widget_destroy(data);
   }
   print_dialog=DIALOG_SAID_CANCEL;
}

int print_gui(GtkWidget *main_window)
{
   GtkWidget *label;
   GtkWidget *button;
   GtkWidget *vbox;
   GtkWidget *hbox;
   long ivalue;
   char temp_str[10];
   char temp[256];
   const char *svalue;
   GSList *group;   
   
   jpilot_logf(LOG_DEBUG, "print_gui\n");
   if (GTK_IS_WINDOW(window)) {
      jpilot_logf(LOG_DEBUG, "print_gui window is already up\n");
      gdk_window_raise(window->window);
      return 0;
   }
   print_dialog=0;

   window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

   gtk_container_set_border_width(GTK_CONTAINER(window), 10);
   g_snprintf(temp, 255, "%s %s", PN, _("Print Options"));
   temp[255]='\0';
   gtk_window_set_title(GTK_WINDOW(window), temp);

   gtk_signal_connect(GTK_OBJECT(window), "destroy",
                      GTK_SIGNAL_FUNC(cb_destroy), window);

   vbox = gtk_vbox_new(FALSE, 0);
   gtk_container_add(GTK_CONTAINER(window), vbox);


   /* Radio buttons for number of records to print */
   group = NULL;
   radio_button_one = gtk_radio_button_new_with_label
     (group, _("One record"));

   group = gtk_radio_button_group(GTK_RADIO_BUTTON(radio_button_one));
   radio_button_shown = gtk_radio_button_new_with_label
     (group, _("All records in this category"));

   group = gtk_radio_button_group(GTK_RADIO_BUTTON(radio_button_shown));
   radio_button_all = gtk_radio_button_new_with_label
     (group, _("Print all records"));
   
   get_pref(PREF_PRINT_THIS_MANY, &ivalue, NULL);
   if (ivalue==1) {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_button_one), TRUE);
   }
   if (ivalue==2) {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_button_shown), TRUE);
   }
   if (ivalue==3) {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_button_all), TRUE);
   }

   gtk_box_pack_start(GTK_BOX(vbox), radio_button_one, FALSE, FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox), radio_button_shown, FALSE, FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox), radio_button_all, FALSE, FALSE, 0);


   /*One record per page check box */
   one_record_checkbutton = gtk_check_button_new_with_label
     (_("One record per page"));
   gtk_box_pack_start(GTK_BOX(vbox), one_record_checkbutton, FALSE, FALSE, 0);
   get_pref(PREF_PRINT_ONE_PER_PAGE, &ivalue, NULL);
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(one_record_checkbutton), ivalue);


   /* Number of blank lines */
   hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

   lines_entry = gtk_entry_new_with_max_length(2);
   gtk_widget_set_usize(lines_entry, 30, 0);
   gtk_box_pack_start(GTK_BOX(hbox), lines_entry, FALSE, FALSE, 0);

   label = gtk_label_new(_(" Blank lines between each record"));
   gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

   get_pref(PREF_NUM_BLANK_LINES, &ivalue, NULL);
   sprintf(temp_str, "%ld", ivalue);
   gtk_entry_set_text(GTK_ENTRY(lines_entry), temp_str);


   /* Print Command */
   label = gtk_label_new(_("Print Command (e.g. lpr, or cat > file.ps)"));
   gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

   print_command_entry = gtk_entry_new_with_max_length(250);
   gtk_box_pack_start(GTK_BOX(vbox), print_command_entry, FALSE, FALSE, 0);

   get_pref(PREF_PRINT_COMMAND, &ivalue, &svalue);
   gtk_entry_set_text(GTK_ENTRY(print_command_entry), svalue);


   /* Create a "Print" button */
   hbox = gtk_hbox_new(TRUE, 0);
   gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

   button = gtk_button_new_with_label(_("Print"));
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_print), window);
   gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);

   /* Create a "Quit" button */
   button = gtk_button_new_with_label(_("Cancel"));
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_cancel), window);
   gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);

   gtk_widget_show_all(window);

   gtk_window_set_modal(GTK_WINDOW(window), TRUE);
   
   gtk_main();
   
   return print_dialog;
}

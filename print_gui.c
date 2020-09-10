/*******************************************************************************
 * print_gui.c
 * A module of J-Pilot http://jpilot.org
 *
 * Copyright (C) 2000-2014 by Judd Montgomery
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
#include <string.h>
#include <gtk/gtk.h>

#include "i18n.h"
#include "utils.h"
#include "prefs.h"
#include "prefs_gui.h"
#include "log.h"
#include "print.h"

/******************************* Global vars **********************************/
static GtkWidget *lines_entry;
static GtkWidget *print_command_entry;
static GtkWidget *one_record_checkbutton;
static GtkWidget *window;
static GtkWidget *radio_button_one;
static GtkWidget *radio_button_shown;
static GtkWidget *radio_button_all;
static GtkWidget *radio_button_daily;
static GtkWidget *radio_button_weekly;
static GtkWidget *radio_button_monthly;

static int print_dialog;
/* This is a temporary hack */
int print_day_week_month;

/****************************** Main Code *************************************/
static gboolean cb_destroy(GtkWidget *widget)
{
   const char *entry_text;
   const char *lines_text;
   int num_lines;

   jp_logf(JP_LOG_DEBUG, "Cleanup print_gui\n");

   /* Get radio button prefs */
   if (radio_button_one) {
      if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio_button_one))) {
         jp_logf(JP_LOG_DEBUG, "print one");
         set_pref(PREF_PRINT_THIS_MANY, 1, NULL, FALSE);
      }
      if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio_button_shown))) {
         jp_logf(JP_LOG_DEBUG, "print shown");
         set_pref(PREF_PRINT_THIS_MANY, 2, NULL, FALSE);
      }
      if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio_button_all))) {
         jp_logf(JP_LOG_DEBUG, "print all");
         set_pref(PREF_PRINT_THIS_MANY, 3, NULL, FALSE);
      }
   }

   /* Get radio button prefs */
   if (radio_button_daily) {
      if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio_button_daily))) {
         jp_logf(JP_LOG_DEBUG, "print daily");
         print_day_week_month=1;
      }
      if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio_button_weekly))) {
         jp_logf(JP_LOG_DEBUG, "print weekly");
         print_day_week_month=2;
      }
      if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio_button_monthly))) {
         jp_logf(JP_LOG_DEBUG, "print monthly");
         print_day_week_month=3;
      }
   }

   /* Get one record per page pref */
   if (one_record_checkbutton) {
      if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(one_record_checkbutton))) {
         jp_logf(JP_LOG_DEBUG, "one record per page");
         set_pref(PREF_PRINT_ONE_PER_PAGE, 1, NULL, FALSE);
      } else {
         set_pref(PREF_PRINT_ONE_PER_PAGE, 0, NULL, FALSE);
      }
   }

   /* Get number of blank lines */
   if (lines_entry) {
      lines_text = gtk_entry_get_text(GTK_ENTRY(lines_entry));
      jp_logf(JP_LOG_DEBUG, "lines_entry = [%s]\n", lines_text);
      num_lines = atoi(lines_text);
      if (num_lines < 0) {
         num_lines = 0;
      }
      if (num_lines > 99) {
         num_lines = 99;
      }

      set_pref(PREF_NUM_BLANK_LINES, num_lines, NULL, FALSE);
   }

   /* Get print command */
   entry_text = gtk_entry_get_text(GTK_ENTRY(print_command_entry));
   jp_logf(JP_LOG_DEBUG, "print_command_entry = [%s]\n", entry_text);
   set_pref(PREF_PRINT_COMMAND, 0, entry_text, TRUE);

   window = NULL;

   gtk_main_quit();

   return FALSE;
}

static void cb_print(GtkWidget *widget, gpointer data)
{
   jp_logf(JP_LOG_DEBUG, "cb_print\n");
   if (GTK_IS_WIDGET(data)) {
      gtk_widget_destroy(data);
   }
   print_dialog=DIALOG_SAID_PRINT;
}

static void cb_cancel(GtkWidget *widget, gpointer data)
{
   jp_logf(JP_LOG_DEBUG, "cb_cancel\n");
   if (GTK_IS_WIDGET(data)) {
      gtk_widget_destroy(data);
   }
   print_dialog=DIALOG_SAID_CANCEL;
}

/* mon_week_day is a binary flag to choose which radio buttons appear for
 * datebook printing.
 * 1 = daily
 * 2 = weekly
 * 4 = monthly
 */
int print_gui(GtkWidget *main_window, int app, int date_button, int mon_week_day)
{
   GtkWidget *label;
   GtkWidget *button;
   GtkWidget *vbox;
   GtkWidget *hbox;
   GtkWidget *pref_menu;
   long ivalue;
   char temp_str[10];
   char temp[256];
   const char *svalue;
   GSList *group;

   jp_logf(JP_LOG_DEBUG, "print_gui\n");
   if (GTK_IS_WINDOW(window)) {
      jp_logf(JP_LOG_DEBUG, "print_gui window is already up\n");
      gdk_window_raise(gtk_widget_get_window(window));
      return EXIT_SUCCESS;
   }
   print_dialog=0;
   radio_button_one=NULL;
   radio_button_daily=NULL;
   one_record_checkbutton=NULL;
   lines_entry=NULL;

   window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
   gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_MOUSE);
   gtk_window_set_modal(GTK_WINDOW(window), TRUE);
   gtk_window_set_transient_for(GTK_WINDOW(window), GTK_WINDOW(main_window));

   gtk_container_set_border_width(GTK_CONTAINER(window), 10);
   g_snprintf(temp, sizeof(temp), "%s %s", PN, _("Print Options"));
   gtk_window_set_title(GTK_WINDOW(window), temp);

   g_signal_connect(GTK_OBJECT(window), "destroy",
                      G_CALLBACK(cb_destroy), window);

   vbox = gtk_vbox_new(FALSE, 0);
   gtk_container_add(GTK_CONTAINER(window), vbox);

   /* Paper Size */
   hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

   label = gtk_label_new(_("Paper Size"));
   gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

   make_pref_menu(&pref_menu, PREF_PAPER_SIZE);
   gtk_box_pack_start(GTK_BOX(hbox), pref_menu, FALSE, FALSE, 0);

   get_pref(PREF_PAPER_SIZE, &ivalue, NULL);
   gtk_combo_box_set_active(GTK_COMBO_BOX(pref_menu), ivalue);

   /* Radio buttons for Datebook */
   radio_button_daily=radio_button_weekly=radio_button_monthly=NULL;
   if (app == DATEBOOK) {
      group = NULL;

      if (mon_week_day & 0x01) {
         radio_button_daily = gtk_radio_button_new_with_label
           (group, _("Daily Printout"));
         group = gtk_radio_button_group(GTK_RADIO_BUTTON(radio_button_daily));
      }

      if (mon_week_day & 0x02) {
         radio_button_weekly = gtk_radio_button_new_with_label
           (group, _("Weekly Printout"));
         group = gtk_radio_button_group(GTK_RADIO_BUTTON(radio_button_weekly));
      }

      if (mon_week_day & 0x04) {
         radio_button_monthly = gtk_radio_button_new_with_label
           (group, _("Monthly Printout"));
      }

      switch (date_button) {
       case 1:
         if (mon_week_day & 0x01) {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_button_daily), TRUE);
         }
         break;
       case 2:
         if (mon_week_day & 0x02) {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_button_weekly), TRUE);
         }
         break;
       case 3:
         if (mon_week_day & 0x04) {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_button_monthly), TRUE);
         }
         break;
       default:
         if (mon_week_day & 0x01) {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_button_daily), TRUE);
         }
      }
      if (mon_week_day & 0x01) {
         gtk_box_pack_start(GTK_BOX(vbox), radio_button_daily, FALSE, FALSE, 0);
      }
      if (mon_week_day & 0x02) {
         gtk_box_pack_start(GTK_BOX(vbox), radio_button_weekly, FALSE, FALSE, 0);
      }
      if (mon_week_day & 0x04) {
         gtk_box_pack_start(GTK_BOX(vbox), radio_button_monthly, FALSE, FALSE, 0);
      }
   }

   if (app != DATEBOOK) {
      /* Radio buttons for number of records to print */
      group = NULL;

      radio_button_one = gtk_radio_button_new_with_label
        (group, _("Selected record"));

      group = gtk_radio_button_group(GTK_RADIO_BUTTON(radio_button_one));
      radio_button_shown = gtk_radio_button_new_with_label
        (group, _("All records in this category"));

      group = gtk_radio_button_group(GTK_RADIO_BUTTON(radio_button_shown));
      radio_button_all = gtk_radio_button_new_with_label
        (group, _("Print all records"));

      get_pref(PREF_PRINT_THIS_MANY, &ivalue, NULL);
      switch (ivalue) {
       case 1:
         gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_button_one), TRUE);
         break;
       case 2:
         gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_button_shown), TRUE);
         break;
       case 3:
         gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_button_all), TRUE);
      }

      gtk_box_pack_start(GTK_BOX(vbox), radio_button_one, FALSE, FALSE, 0);
      gtk_box_pack_start(GTK_BOX(vbox), radio_button_shown, FALSE, FALSE, 0);
      gtk_box_pack_start(GTK_BOX(vbox), radio_button_all, FALSE, FALSE, 0);
   }

   if (app != DATEBOOK) {
      /* One record per page check box */
      one_record_checkbutton = gtk_check_button_new_with_label
        (_("One record per page"));
      gtk_box_pack_start(GTK_BOX(vbox), one_record_checkbutton, FALSE, FALSE, 0);
      get_pref(PREF_PRINT_ONE_PER_PAGE, &ivalue, NULL);
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(one_record_checkbutton), ivalue);
   }

   if (app != DATEBOOK) {
      /* Number of blank lines */
      hbox = gtk_hbox_new(FALSE, 0);
      gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

      lines_entry = gtk_entry_new_with_max_length(2);
      entry_set_multiline_truncate(GTK_ENTRY(lines_entry), TRUE);
      gtk_widget_set_usize(lines_entry, 30, 0);
      gtk_box_pack_start(GTK_BOX(hbox), lines_entry, FALSE, FALSE, 0);

      label = gtk_label_new(_("Blank lines between each record"));
      gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);

      get_pref(PREF_NUM_BLANK_LINES, &ivalue, NULL);
      sprintf(temp_str, "%ld", ivalue);
      gtk_entry_set_text(GTK_ENTRY(lines_entry), temp_str);
   }

   /* Print Command */
   label = gtk_label_new(_("Print Command (e.g. lpr, or cat > file.ps)"));
   gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

   print_command_entry = gtk_entry_new_with_max_length(250);
   gtk_box_pack_start(GTK_BOX(vbox), print_command_entry, FALSE, FALSE, 0);

   get_pref(PREF_PRINT_COMMAND, NULL, &svalue);
   gtk_entry_set_text(GTK_ENTRY(print_command_entry), svalue);

   /* Dialog button box */
   hbox = gtk_hbutton_box_new();
   gtk_container_set_border_width(GTK_CONTAINER(hbox), 6);
   gtk_button_box_set_layout(GTK_BUTTON_BOX (hbox), GTK_BUTTONBOX_END);
   gtk_button_box_set_spacing(GTK_BUTTON_BOX(hbox), 6);
   gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

   /* Cancel button */
   button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
   g_signal_connect(GTK_OBJECT(button), "clicked",
                      G_CALLBACK(cb_cancel), window);
   gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);

   /* Print button */
   button = gtk_button_new_from_stock(GTK_STOCK_PRINT);
   g_signal_connect(GTK_OBJECT(button), "clicked",
                      G_CALLBACK(cb_print), window);
   gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);


   gtk_widget_show_all(window);

   gtk_main();

   return print_dialog;
}


/*******************************************************************************
 * export_gui.c
 * A module of J-Pilot http://jpilot.org
 *
 * Copyright (C) 1999-2014 by Judd Montgomery
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
#include <sys/stat.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <pi-appinfo.h>

#include "i18n.h"
#include "utils.h"
#include "log.h"
#include "prefs.h"
#include "export.h"

/********************************* Constants **********************************/
#define NUM_CAT_ITEMS 16

#define BROWSE_OK     1
#define BROWSE_CANCEL 2

/******************************* Global vars **********************************/
static GtkWidget *export_clist;
static int export_category;

static int glob_export_browse_pressed;
static int glob_pref_export;

static GtkWidget *export_radio_type[10];
static int glob_export_type;
static GtkWidget *save_as_entry;

/****************************** Prototypes ************************************/
static void (*glob_cb_export_menu)(GtkWidget *clist, int category);
static void (*glob_cb_export_done)(GtkWidget *widget, const char *filename);
static void (*glob_cb_export_ok)(GtkWidget *export_window,
                                 GtkWidget *clist,
                                 int type,
                                 const char *filename);

/****************************** Main Code *************************************/
/* 
 * Browse GUI
 */
static gboolean cb_export_browse_destroy(GtkWidget *widget)
{
   gtk_main_quit();
   return FALSE;
}

static void cb_export_browse_cancel(GtkWidget *widget, gpointer data)
{
   glob_export_browse_pressed=BROWSE_CANCEL;
   gtk_widget_destroy(data);
}

static void cb_export_browse_ok(GtkWidget *widget, gpointer data)
{
   const char *sel;

   glob_export_browse_pressed=BROWSE_OK;
   if (glob_pref_export) {
      sel = gtk_file_selection_get_filename(GTK_FILE_SELECTION(data));
      set_pref(glob_pref_export, 0, sel, TRUE);
   }
   gtk_widget_destroy(data);
}

int export_browse(GtkWidget *main_window, int pref_export)
{
   GtkWidget *filesel;
   const char *svalue;
   char dir[MAX_PREF_LEN+2];
   int i;

   glob_export_browse_pressed = 0;

   if (pref_export) {
      glob_pref_export = pref_export;
   } else {
      glob_pref_export = 0;
   }

   if (pref_export) {
      get_pref(pref_export, NULL, &svalue);
      g_strlcpy(dir, svalue, sizeof(dir));
      i=strlen(dir)-1;
      if (i<0) i=0;
      if (dir[i]!='/') {
         for (i=strlen(dir); i>=0; i--) {
            if (dir[i]=='/') {
               dir[i+1]='\0';
               break;
            }
         }
      }

      if (chdir(dir)<0) {
         jp_logf(JP_LOG_WARN, "chdir failed %s %d\n", __FILE__, __LINE__);
      }
   }
   filesel = gtk_file_selection_new(_("File Browser"));

   gtk_window_set_modal(GTK_WINDOW(filesel), TRUE);
   gtk_window_set_transient_for(GTK_WINDOW(filesel), GTK_WINDOW(main_window));

   gtk_signal_connect(GTK_OBJECT(filesel), "destroy",
                      GTK_SIGNAL_FUNC(cb_export_browse_destroy), filesel);

   gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(filesel)->ok_button),
                      "clicked", GTK_SIGNAL_FUNC(cb_export_browse_ok), filesel);
   gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(filesel)->cancel_button),
                      "clicked", GTK_SIGNAL_FUNC(cb_export_browse_cancel), filesel);

   gtk_widget_show(filesel);

   gtk_main();

   return glob_export_browse_pressed;
}

/* End Export Browse */

/*
 * Start Export code
 */
static gboolean cb_export_destroy(GtkWidget *widget)
{
   const char *filename;

   filename = gtk_entry_get_text(GTK_ENTRY(save_as_entry));
   if (glob_cb_export_done) {
      glob_cb_export_done(widget, filename);
   }
   clist_clear(GTK_CLIST(export_clist));
   gtk_main_quit();

   return FALSE;
}

static void cb_ok(GtkWidget *widget, gpointer data)
{
   const char *filename;

   filename = gtk_entry_get_text(GTK_ENTRY(save_as_entry));

   if (glob_cb_export_ok) {
      glob_cb_export_ok(data, export_clist, glob_export_type, filename);
   }

   gtk_widget_destroy(data);
}

static void cb_export_browse(GtkWidget *widget, gpointer data)
{
   int r;
   const char *svalue;

   r = export_browse(GTK_WIDGET(data), glob_pref_export);
   if (r==BROWSE_OK) {
      if (glob_pref_export) {
         get_pref(glob_pref_export, NULL, &svalue);
         gtk_entry_set_text(GTK_ENTRY(save_as_entry), svalue);
      }
   }
}

static void cb_export_quit(GtkWidget *widget, gpointer data)
{
   gtk_widget_destroy(data);
}

static void cb_export_type(GtkWidget *widget, gpointer data)
{
   glob_export_type=GPOINTER_TO_INT(data);
}

static void cb_export_category(GtkWidget *item, int selection)
{
   if ((GTK_CHECK_MENU_ITEM(item))->active) {
      export_category = selection;
      jp_logf(JP_LOG_DEBUG, "cb_export_category() cat=%d\n", export_category);
      if (glob_cb_export_menu) {
         glob_cb_export_menu(export_clist, export_category);
      }
      gtk_clist_select_all(GTK_CLIST(export_clist));
      jp_logf(JP_LOG_DEBUG, "Leaving cb_export_category()\n");
   }
}

int export_gui(GtkWidget *main_window,
               int w, int h, int x, int y,
               int columns,
               struct sorted_cats *sort_l,
               int pref_export,
               char *type_text[],
               int type_int[],
               void (*cb_export_menu)(GtkWidget *clist, int category),
               void (*cb_export_done)(GtkWidget *widget,
                                      const char *filename),
               void (*cb_export_ok)(GtkWidget *export_window,
                                    GtkWidget *clist,
                                    int type,
                                    const char *filename)
               )
{
   GtkWidget *export_window;
   GtkWidget *button;
   GtkWidget *vbox;
   GtkWidget *hbox;
   GtkWidget *category_menu;
   GtkWidget *cat_menu_item[NUM_CAT_ITEMS+1];
   GtkWidget *scrolled_window;
   GtkWidget *label;
   char title[256];
   GSList *group;
   int i;
   const char *svalue;

   jp_logf(JP_LOG_DEBUG, "export_gui()\n");

   export_category = CATEGORY_ALL;

   /* Set the export type to the first type available */
   glob_export_type = type_int[0];
   glob_cb_export_menu = cb_export_menu;
   glob_cb_export_done = cb_export_done;
   glob_cb_export_ok = cb_export_ok;

   glob_pref_export=pref_export;

   g_snprintf(title, sizeof(title), "%s %s", PN, _("Export"));

   export_window = gtk_widget_new(GTK_TYPE_WINDOW,
                                  "type", GTK_WINDOW_TOPLEVEL,
                                  "title", title,
                                  NULL);

   gtk_window_set_default_size(GTK_WINDOW(export_window), w, h);
   gtk_widget_set_uposition(GTK_WIDGET(export_window), x, y);

   gtk_window_set_modal(GTK_WINDOW(export_window), TRUE);
   gtk_window_set_transient_for(GTK_WINDOW(export_window), GTK_WINDOW(main_window));

   gtk_container_set_border_width(GTK_CONTAINER(export_window), 5);

   gtk_signal_connect(GTK_OBJECT(export_window), "destroy",
                      GTK_SIGNAL_FUNC(cb_export_destroy), export_window);

   vbox = gtk_vbox_new(FALSE, 0);
   gtk_container_add(GTK_CONTAINER(export_window), vbox);

   /* Label for instructions */
   label = gtk_label_new(_("Select records to be exported"));
   gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
   label = gtk_label_new(_("Use Ctrl and Shift Keys"));
   gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

   /* Put the export category menu up */
   make_category_menu(&category_menu, cat_menu_item, sort_l,
                      cb_export_category, TRUE, FALSE);
   gtk_box_pack_start(GTK_BOX(vbox), category_menu, FALSE, FALSE, 0);

   /* Put the record list window up */
   scrolled_window = gtk_scrolled_window_new(NULL, NULL);
   gtk_container_set_border_width(GTK_CONTAINER(scrolled_window), 0);
   gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
   gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 0);

   export_clist = gtk_clist_new(columns);

   gtk_clist_set_shadow_type(GTK_CLIST(export_clist), SHADOW);
   gtk_clist_set_selection_mode(GTK_CLIST(export_clist), GTK_SELECTION_EXTENDED);
   for (i=0; i<columns; i++) {
      gtk_clist_set_column_auto_resize(GTK_CLIST(export_clist), i, TRUE);
   }

   gtk_container_add(GTK_CONTAINER(scrolled_window), GTK_WIDGET(export_clist));

   /* Export Type buttons */
   group = NULL;
   for (i=0; i<100; i++) {
      if (type_text[i]==NULL) break;
      export_radio_type[i] = gtk_radio_button_new_with_label(group, _(type_text[i]));
      group = gtk_radio_button_group(GTK_RADIO_BUTTON(export_radio_type[i]));
      gtk_box_pack_start(GTK_BOX(vbox), export_radio_type[i], FALSE, FALSE, 0);
      gtk_signal_connect(GTK_OBJECT(export_radio_type[i]), "pressed",
                         GTK_SIGNAL_FUNC(cb_export_type),
                         GINT_TO_POINTER(type_int[i]));
   }
   export_radio_type[i] = NULL;

   /* Save As entry */
   hbox = gtk_hbox_new(FALSE, 5);
   gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
   label = gtk_label_new(_("Save as"));
   gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
   save_as_entry = gtk_entry_new_with_max_length(250);
   svalue=NULL;
   if (glob_pref_export) {
      get_pref(glob_pref_export, NULL, &svalue);
   }
   if (svalue) {
      gtk_entry_set_text(GTK_ENTRY(save_as_entry), svalue);
   }
   gtk_box_pack_start(GTK_BOX(hbox), save_as_entry, TRUE, TRUE, 0);

   /* Browse button */
   button = gtk_button_new_with_label(_("Browse"));
   gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
                      GTK_SIGNAL_FUNC(cb_export_browse), export_window);

   /* Cancel/OK buttons */
   hbox = gtk_hbutton_box_new();
   gtk_container_set_border_width(GTK_CONTAINER(hbox), 12);
   gtk_button_box_set_layout(GTK_BUTTON_BOX (hbox), GTK_BUTTONBOX_END);
   gtk_button_box_set_spacing(GTK_BUTTON_BOX(hbox), 6);
   gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

   button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
   gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
                      GTK_SIGNAL_FUNC(cb_export_quit), export_window);

   button = gtk_button_new_from_stock(GTK_STOCK_OK);
   gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
                      GTK_SIGNAL_FUNC(cb_ok), export_window);

   if (glob_cb_export_menu) {
      glob_cb_export_menu(export_clist, export_category);
   }

   gtk_widget_show_all(export_window);

   gtk_clist_select_all(GTK_CLIST(export_clist));

   gtk_main();

   return EXIT_SUCCESS;
}


/*
 * jpilot.c
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
#include <stdio.h>

#include <pi-datebook.h>

//#include "datebook.h"
#include "utils.h"

#include "datebook.xpm"
#include "address.xpm"
#include "todo.xpm"
#include "memo.xpm"

//#define SHADOW GTK_SHADOW_IN
//#define SHADOW GTK_SHADOW_OUT
//#define SHADOW GTK_SHADOW_ETCHED_IN
#define SHADOW GTK_SHADOW_ETCHED_OUT

GtkWidget *g_hbox, *g_vbox0;
GtkWidget *g_hbox2, *g_vbox0_1;

GtkWidget *glob_date_label;
gint glob_date_timer_tag;

void delete_event(GtkWidget *widget, GdkEvent *event, gpointer data);

void cb_sync(GtkWidget *widget, gpointer data);

int create_main_boxes()
{
   GtkWidget *button;
   
   g_hbox2 = gtk_hbox_new(FALSE, 0);
   g_vbox0_1 = gtk_vbox_new(FALSE, 0);

   gtk_box_pack_start(GTK_BOX(g_hbox), g_hbox2, TRUE, TRUE, 3);
   gtk_box_pack_start(GTK_BOX(g_vbox0), g_vbox0_1, FALSE, FALSE, 0);

   // Create "Quit" button
   button = gtk_button_new_with_label ("Quit!");
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC (delete_event), NULL);
   gtk_box_pack_start(GTK_BOX(g_vbox0_1), button, FALSE, FALSE, 0);
   gtk_widget_show(button);
   
   // Create "Sync" button
   button = gtk_button_new_with_label ("Sync");
   gtk_signal_connect (GTK_OBJECT(button), "clicked",
		       GTK_SIGNAL_FUNC(cb_sync), NULL);
   gtk_box_pack_start (GTK_BOX (g_vbox0_1), button, FALSE, FALSE, 0);
   gtk_widget_show (button);
}

void cb_datebook(GtkWidget *widget, gpointer data)
{
   gtk_widget_destroy(g_vbox0_1);
   gtk_widget_destroy(g_hbox2);
   create_main_boxes();
   if (glob_date_timer_tag) {
      gtk_timeout_remove(glob_date_timer_tag);
   }
   datebook_gui(g_vbox0_1, g_hbox2);
}

void cb_address(GtkWidget *widget, gpointer data)
{
   gtk_widget_destroy(g_vbox0_1);
   gtk_widget_destroy(g_hbox2);
   create_main_boxes();
   if (glob_date_timer_tag) {
      gtk_timeout_remove(glob_date_timer_tag);
   }
   address_gui(g_vbox0_1, g_hbox2);
}

void cb_todo(GtkWidget *widget, gpointer data)
{
   gtk_widget_destroy(g_vbox0_1);
   gtk_widget_destroy(g_hbox2);
   create_main_boxes();
   if (glob_date_timer_tag) {
      gtk_timeout_remove(glob_date_timer_tag);
   }
   todo_gui(g_vbox0_1, g_hbox2);
}

void cb_memo(GtkWidget *widget, gpointer data)
{
   gtk_widget_destroy(g_vbox0_1);
   gtk_widget_destroy(g_hbox2);
   create_main_boxes();
   if (glob_date_timer_tag) {
      gtk_timeout_remove(glob_date_timer_tag);
   }
   memo_gui(g_vbox0_1, g_hbox2);
}

void delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
   gtk_main_quit();
}

int main(int   argc,
	 char *argv[])
{
   GtkWidget *window;
   GtkWidget *button_datebook,*button_address,*button_todo,*button_memo;
   GtkWidget *separator;
   GtkStyle *style;
   GdkBitmap *mask;
   GtkWidget *pixmapwid;
   GdkPixmap *pixmap;

   if (argc > 1) {
      if (!strncasecmp(argv[1], "-v", 2)) {
	 printf("%s\n", VERSION_STRING);
	 exit(0);
      }
   }

   //Check to see if ~/.jpilot is there, or create it
   if (check_hidden_dir()) {
      exit(1);
   }

   gtk_set_locale();

   gtk_init(&argc, &argv);

   read_rc_file();

   glob_date_timer_tag=0;

   window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

   gtk_window_set_default_size(GTK_WINDOW(window), 770, 440);

   gtk_window_set_title(GTK_WINDOW(window), "J-Pilot");

   // Set a handler for delete_event that immediately
   // exits GTK.
   gtk_signal_connect(GTK_OBJECT(window), "delete_event",
		      GTK_SIGNAL_FUNC(delete_event), NULL);

   gtk_container_set_border_width(GTK_CONTAINER(window), 20);

   g_hbox = gtk_hbox_new(FALSE, 0);
   g_vbox0 = gtk_vbox_new(FALSE, 0);

   gtk_container_add(GTK_CONTAINER(window), g_hbox);

   gtk_box_pack_start(GTK_BOX(g_hbox), g_vbox0, FALSE, FALSE, 3);
   
   // Create "Datebook" button
   button_datebook = gtk_button_new();
   gtk_signal_connect(GTK_OBJECT(button_datebook), "clicked",
		      GTK_SIGNAL_FUNC(cb_datebook), NULL);
   gtk_box_pack_start(GTK_BOX(g_vbox0), button_datebook, FALSE, FALSE, 0);
   gtk_widget_set_usize(button_datebook, 46, 46);

   // Create "Address" button
   button_address = gtk_button_new();
   gtk_signal_connect(GTK_OBJECT(button_address), "clicked",
		      GTK_SIGNAL_FUNC(cb_address), NULL);
   gtk_box_pack_start(GTK_BOX(g_vbox0), button_address, FALSE, FALSE, 0);
   gtk_widget_set_usize(button_address, 46, 46);

   // Create "Todo" button
   button_todo = gtk_button_new();
   gtk_signal_connect(GTK_OBJECT(button_todo), "clicked",
		      GTK_SIGNAL_FUNC(cb_todo), NULL);
   gtk_box_pack_start(GTK_BOX(g_vbox0), button_todo, FALSE, FALSE, 0);
   gtk_widget_set_usize(button_todo, 46, 46);

   // Create "memo" button
   button_memo = gtk_button_new();
   gtk_signal_connect(GTK_OBJECT(button_memo), "clicked",
		      GTK_SIGNAL_FUNC(cb_memo), NULL);
   gtk_box_pack_start(GTK_BOX(g_vbox0), button_memo, FALSE, FALSE, 0);
   gtk_widget_set_usize(button_memo, 46, 46);

   gtk_widget_set_name(button_datebook, "button_app");
   gtk_widget_set_name(button_address, "button_app");
   gtk_widget_set_name(button_todo, "button_app");
   gtk_widget_set_name(button_memo, "button_app");
   gtk_widget_show(button_datebook);
   gtk_widget_show(button_address);
   gtk_widget_show(button_todo);
   gtk_widget_show(button_memo);

   separator = gtk_hseparator_new();
   gtk_box_pack_start(GTK_BOX(g_vbox0), separator, FALSE, TRUE, 5);
   gtk_widget_show(separator);
   
   //This creates the 2 main boxes that are changeable
   create_main_boxes();

   gtk_widget_show(g_hbox);
   gtk_widget_show(g_hbox2);
   gtk_widget_show(g_vbox0);
   gtk_widget_show(g_vbox0_1);

   gtk_widget_show(window);

   style = gtk_widget_get_style(window);

   // Create "Datebook" pixmap
   pixmap = gdk_pixmap_create_from_xpm_d(window->window, &mask,
					 &style->bg[GTK_STATE_NORMAL],
					 datebook_xpm);
   pixmapwid = gtk_pixmap_new(pixmap, mask);
   gtk_widget_show(pixmapwid);
   gtk_container_add( GTK_CONTAINER(button_datebook), pixmapwid);

   // Create "Address" pixmap
   pixmap = gdk_pixmap_create_from_xpm_d(window->window, &mask,
					 &style->bg[GTK_STATE_NORMAL],
					 address_xpm);
   pixmapwid = gtk_pixmap_new(pixmap, mask);
   gtk_widget_show(pixmapwid);
   gtk_container_add(GTK_CONTAINER(button_address), pixmapwid);

   // Create "Todo" pixmap
   pixmap = gdk_pixmap_create_from_xpm_d(window->window, &mask,
					 &style->bg[GTK_STATE_NORMAL],
					 todo_xpm);
   pixmapwid = gtk_pixmap_new(pixmap, mask);
   gtk_widget_show(pixmapwid);
   gtk_container_add(GTK_CONTAINER(button_todo), pixmapwid);

   // Create "memo" pixmap
   pixmap = gdk_pixmap_create_from_xpm_d(window->window, &mask,
					 &style->bg[GTK_STATE_NORMAL],
					 memo_xpm);
   pixmapwid = gtk_pixmap_new(pixmap, mask);
   gtk_widget_show(pixmapwid);
   gtk_container_add(GTK_CONTAINER(button_memo), pixmapwid);

   gtk_main();

   return 0;
}

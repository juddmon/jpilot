/* monthview_gui.c
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
#include <ctype.h>
#include <stdlib.h>
#include "utils.h"
/*#include "prefs.h" */
#include "log.h"
#include "datebook.h"
/*#include "address.h" */
/*#include "todo.h" */
/*#include "memo.h" */
#include <pi-datebook.h>
/*#include <pi-address.h> */
/*#include <pi-todo.h> */
/*#include <pi-memo.h> */

static gboolean cb_destroy(GtkWidget *widget)
{
   return FALSE;
}

static void
  cb_quit(GtkWidget *widget,
	   gpointer   data)
{
   gtk_widget_destroy(data);
}

void cb_monthview_gui(GtkWidget *widget, gpointer data)
{
   static GtkWidget *window=NULL;
   GtkWidget *table;
   GtkWidget *text;
   GtkWidget *button;
   GtkWidget *vbox;
   int w, d;
   char str[50];
   
   if (GTK_IS_WIDGET(window)) {
      return;
   }

   window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
   gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
   gtk_container_set_border_width(GTK_CONTAINER(window), 10);
   gtk_window_set_title(GTK_WINDOW(window), PN" Monthly View");

   gtk_signal_connect(GTK_OBJECT(window), "destroy",
                      GTK_SIGNAL_FUNC(cb_destroy), window);

   vbox = gtk_vbox_new(FALSE, 0);
   gtk_container_add(GTK_CONTAINER(window), vbox);

/*   hbox = gtk_hbox_new(FALSE, 0); */
 /*  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0); */

   table = gtk_table_new(7, 6, TRUE);
   gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
   
   for (w=0; w<5; w++) {
      for (d=0; d<7; d++) {
/*	 label = gtk_label_new("1"); */
/*	 gtk_widget_set_name(GTK_WIDGET(label), "label_box_cal"); */
/*	 gtk_table_attach_defaults(GTK_TABLE(table), label, d, d+1, w, w+1); */
	 text = gtk_text_new(NULL, NULL);
	 gtk_widget_set_usize(GTK_WIDGET(text), 60, 90);
	 sprintf(str, "%d", w*7 + d + 1);
	 gtk_text_insert(GTK_TEXT(text), NULL,NULL, NULL, str, -1);
	 gtk_table_attach_defaults(GTK_TABLE(table), text, d, d+1, w, w+1);
      }
   }

   /* Create a "Quit" button */
   button = gtk_button_new_with_label("Close");
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_quit), window);
   gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);

   gtk_widget_show_all(window);
}

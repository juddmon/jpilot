/* mailer.c
 * A module of J-Pilot http://jpilot.org
 * 
 * Copyright (C) 2004 by Rik Wehbring
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
#include "utils.h"
#include "log.h"
#include "prefs.h"
#include <stdlib.h>
#include <string.h>
#include <gdk/gdk.h>
#include "mailer.h"

/*
 * Start of Dialer Dialog window code
 */
struct dialog_data {
   GtkWidget *entry_mail;
   GtkWidget *entry_command;
};

static void cb_dialog_button(GtkWidget *widget, gpointer data)
{
   GtkWidget *w;

   w = gtk_widget_get_toplevel(widget);
   gtk_widget_destroy(w);
}

static gboolean cb_destroy_dialog(GtkWidget *widget)
{
   struct dialog_data *Pdata;
   const gchar *txt;

   Pdata = gtk_object_get_data(GTK_OBJECT(widget), "dialog_data");
   if (!Pdata) {
      return TRUE;
   }
   txt = gtk_entry_get_text(GTK_ENTRY(Pdata->entry_command));
   set_pref(PREF_MAIL_COMMAND, 0, txt, FALSE);

   gtk_main_quit();

   return TRUE;
}

static void emailer(gpointer data)
{
   struct dialog_data *Pdata;
   char str[70];
   char command[1024];
   const char *pref_command;
   
   Pdata=data;

   g_snprintf(str, sizeof(str), "%s",
	      gtk_entry_get_text(GTK_ENTRY(Pdata->entry_mail)));
   str[sizeof(str)-1]='\0';

   pref_command = gtk_entry_get_text(GTK_ENTRY(Pdata->entry_command));

   /* Make a system call command string */
   g_snprintf(command, sizeof(command), pref_command,str);
   command[sizeof(str)-1]='\0';

   jp_logf(JP_LOG_STDOUT|JP_LOG_FILE, "executing command = [%s]\n", command);
   system(command);
}

static void cb_email_contact(GtkWidget *widget, gpointer data)
{
   emailer(data);
   cb_dialog_button(widget, data);
}

/*
 * Dialog window for calling external mail program
 */
int dialog_email(GtkWindow *main_window, char *string)
{
   GtkWidget *button, *label;
   GtkWidget *hbox1, *vbox1;
   GtkWidget *dialog;
   GtkWidget *entry;
   struct dialog_data *Pdata;
   const char *mail_command;

   dialog = gtk_widget_new(GTK_TYPE_WINDOW,
			   "type", GTK_WINDOW_TOPLEVEL,
			   "title", _("Email Contact"),
			   NULL);

   gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_MOUSE);

   gtk_signal_connect(GTK_OBJECT(dialog), "destroy",
                      GTK_SIGNAL_FUNC(cb_destroy_dialog), dialog);

   gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);

   gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(main_window));

   /* Set up a data structure for the window */
   Pdata = malloc(sizeof(struct dialog_data));

   gtk_object_set_data(GTK_OBJECT(dialog), "dialog_data", Pdata);

   vbox1 = gtk_vbox_new(FALSE, 2);

   gtk_container_set_border_width(GTK_CONTAINER(vbox1), 5);

   gtk_container_add(GTK_CONTAINER(dialog), vbox1);

   /* Email address entry */
   hbox1 = gtk_hbox_new(FALSE, 2);
   gtk_container_set_border_width(GTK_CONTAINER(hbox1), 5);
   gtk_box_pack_start(GTK_BOX(vbox1), hbox1, FALSE, FALSE, 2);

   label = gtk_label_new(_("Email address"));
   gtk_box_pack_start(GTK_BOX(hbox1), label, FALSE, FALSE, 2);

   entry = gtk_entry_new_with_max_length(50);
   gtk_entry_set_text(GTK_ENTRY(entry), string);
   gtk_box_pack_start(GTK_BOX(hbox1), entry, TRUE, TRUE, 1);

   Pdata->entry_mail=entry;

   /* Mail Button */
   button = gtk_button_new_with_label(_("Mail"));
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_email_contact), Pdata);
   gtk_box_pack_start(GTK_BOX(hbox1), button, TRUE, TRUE, 1);

   gtk_widget_grab_focus(GTK_WIDGET(button));

   /* Command Entry */
   hbox1 = gtk_hbox_new(FALSE, 2);
   gtk_container_set_border_width(GTK_CONTAINER(hbox1), 5);
   gtk_box_pack_start(GTK_BOX(vbox1), hbox1, FALSE, FALSE, 2);

   label = gtk_label_new(_("Mail Command"));
   gtk_box_pack_start(GTK_BOX(hbox1), label, FALSE, FALSE, 2);

   entry = gtk_entry_new_with_max_length(100);
   gtk_box_pack_start(GTK_BOX(hbox1), entry, TRUE, TRUE, 1);

   get_pref(PREF_MAIL_COMMAND, NULL, &mail_command);
   if (mail_command) {
      gtk_entry_set_text(GTK_ENTRY(entry), mail_command);
   }

   Pdata->entry_command=entry;

   /* Button Box */
   hbox1 = gtk_hbox_new(TRUE, 2);
   gtk_container_set_border_width(GTK_CONTAINER(hbox1), 5);
   gtk_box_pack_start(GTK_BOX(vbox1), hbox1, FALSE, FALSE, 2);

   /* Dismiss Button */
   button = gtk_button_new_with_label(_("Dismiss"));
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_dialog_button),
		      GINT_TO_POINTER(DIALOG_SAID_1));
   gtk_box_pack_start(GTK_BOX(hbox1), button, TRUE, TRUE, 1);

   gtk_widget_show_all(dialog);

   gtk_main();

   free(Pdata);

   return 0;
}


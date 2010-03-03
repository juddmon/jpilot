/* $Id: install_user.c,v 1.11 2010/03/03 14:42:03 rousseau Exp $ */

/*******************************************************************************
 * install_user.c
 * A module of J-Pilot http://jpilot.org
 *
 * Copyright (C) 1999-2005 by Judd Montgomery
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
#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>

#include "i18n.h"
#include "utils.h"
#include "sync.h"
#include "prefs.h"
#include "jpilot.h"

/****************************** Prototypes ************************************/
struct install_dialog_data {
   GtkWidget *user_entry;
   GtkWidget *ID_entry;
   int button_hit;
   char user[128];
   unsigned long id;
};

/****************************** Main Code *************************************/
/* Allocates memory and returns pointer, or NULL */
static char *jp_user_or_whoami(void)
{
   struct passwd *pw_ent;
   uid_t uid;
   const char *svalue;

   get_pref(PREF_USER, NULL, &svalue);
   if ((svalue) && strlen(svalue)) return strdup(svalue);

   uid = geteuid();
   pw_ent = getpwuid(uid);
   if ((pw_ent) && (pw_ent->pw_name)) {
      return strdup(pw_ent->pw_name);
   }
   return NULL;
}

static void cb_install_user_button(GtkWidget *widget, gpointer data)
{
   GtkWidget *w;
   struct install_dialog_data *Pdata;

   w = gtk_widget_get_toplevel(widget);
   Pdata = gtk_object_get_data(GTK_OBJECT(w), "install_dialog_data");
   if (Pdata) {
      Pdata->button_hit = GPOINTER_TO_INT(data);
      if (Pdata->button_hit == DIALOG_SAID_1) {
	 g_strlcpy(Pdata->user,
	       gtk_entry_get_text(GTK_ENTRY(Pdata->user_entry)),
	       sizeof(Pdata->user));
	 sscanf(gtk_entry_get_text(GTK_ENTRY(Pdata->ID_entry)), "%lu",
		&(Pdata->id));
      }
   }

   gtk_widget_destroy(w);
}

static gboolean cb_destroy_dialog(GtkWidget *widget)
{
   gtk_main_quit();

   return FALSE;
}

static int dialog_install_user(GtkWindow *main_window, 
                        char *user, int user_len, 
                        unsigned long *user_id)
{
   GtkWidget *button, *label;
   GtkWidget *user_entry, *ID_entry;
   GtkWidget *install_user_dialog;
   GtkWidget *vbox;
   GtkWidget *hbox;
   /* object data */
   struct install_dialog_data data;
   unsigned long id;
   char s_id[32];
   char *whoami;

   data.button_hit=0;

   install_user_dialog = gtk_widget_new(GTK_TYPE_WINDOW,
					"type", GTK_WINDOW_TOPLEVEL,
					"window_position", GTK_WIN_POS_MOUSE,
					"title", _("Install User"),
					NULL);
   gtk_window_set_modal(GTK_WINDOW(install_user_dialog), TRUE);
   if (main_window) {
      gtk_window_set_transient_for(GTK_WINDOW(install_user_dialog), GTK_WINDOW(main_window));
   }

   gtk_signal_connect(GTK_OBJECT(install_user_dialog), "destroy",
		      GTK_SIGNAL_FUNC(cb_destroy_dialog), install_user_dialog);

   gtk_object_set_data(GTK_OBJECT(install_user_dialog),
		       "install_dialog_data", &data);

   vbox = gtk_vbox_new(FALSE, 5);

   gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);

   gtk_container_add(GTK_CONTAINER(install_user_dialog), vbox);

   hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
   label = gtk_label_new(_("A PalmOS(c) device needs a user name and a user ID in order to sync properly."));
   gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
   gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
   gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);

   hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
   label = gtk_label_new(_("If you want to sync more than 1 PalmOS(c) device each one should have a different ID and preferably a different user name."));
   gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
   gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
   gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);

   srandom(time(NULL));
   /* RAND_MAX is 32768 on Solaris machines for some reason.
    * If someone knows how to fix this, let me know.
    */
   if (RAND_MAX==32768) {
      id = 1+(2000000000.0*random()/(2147483647+1.0));
   } else {
      id = 1+(2000000000.0*random()/(RAND_MAX+1.0));
   }
   g_snprintf(s_id, 30, "%ld", id);

   /* User Name entry */
   
   hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

   /* Instruction label */
   label = gtk_label_new(_("Most people choose their name or nickname for the user name."));
   gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
   gtk_label_set_line_wrap(GTK_LABEL(label), FALSE);
   gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);

   /* User Name */
   hbox = gtk_hbox_new(FALSE, 5);
   gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
   label = gtk_label_new(_("User Name"));
   user_entry = gtk_entry_new_with_max_length(128);
   entry_set_multiline_truncate(GTK_ENTRY(user_entry), TRUE);
   data.user_entry = user_entry;
   gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
   gtk_box_pack_start(GTK_BOX(hbox), user_entry, TRUE, TRUE, 2);

   /* Instruction label */
   hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
   label = gtk_label_new(_("The ID should be a random number."));
   gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
   gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
   gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);

   /* User ID */
   hbox = gtk_hbox_new(FALSE, 5);
   gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);
   label = gtk_label_new(_("User ID"));
   ID_entry = gtk_entry_new_with_max_length(32);
   entry_set_multiline_truncate(GTK_ENTRY(ID_entry), TRUE);
   data.ID_entry = ID_entry;
   gtk_entry_set_text(GTK_ENTRY(ID_entry), s_id);
   whoami = jp_user_or_whoami();
   if (whoami) {
      gtk_entry_set_text(GTK_ENTRY(user_entry), whoami);
      free(whoami);
   }
   gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 2);
   gtk_box_pack_start(GTK_BOX(hbox), ID_entry, TRUE, TRUE, 2);

   /* Cancel/Install buttons */
   hbox = gtk_hbutton_box_new();
   gtk_button_box_set_layout(GTK_BUTTON_BOX (hbox), GTK_BUTTONBOX_END);
   gtk_button_box_set_spacing(GTK_BUTTON_BOX(hbox), 6);
   gtk_container_set_border_width(GTK_CONTAINER(hbox), 5);
   gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 2);

   button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_install_user_button),
		      GINT_TO_POINTER(DIALOG_SAID_2));
   gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 1);

   button = gtk_button_new_with_label(_("Install User"));
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_install_user_button),
		      GINT_TO_POINTER(DIALOG_SAID_1));
   gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 1);
   GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
   gtk_widget_grab_default(button);
   gtk_widget_grab_focus(button);

   gtk_widget_show_all(install_user_dialog);

   gtk_main();
   
   g_strlcpy(user, data.user, user_len);
   *user_id = data.id;
   
   return data.button_hit;
}

void install_user_gui(GtkWidget *main_window)
{
   int r;
   char user[MAX_PREF_LEN];
   unsigned long user_id;
   const char *svalue;
   long ivalue;
   char *old_user;

   r = dialog_install_user(GTK_WINDOW(main_window), user, MAX_PREF_LEN, &user_id);
   if (r == DIALOG_SAID_1) {
      /* Temporarily set the user and user ID */
      get_pref(PREF_USER, NULL, &svalue);
      get_pref(PREF_USER_ID, &ivalue, NULL);
      if (svalue) {
	 old_user = strdup(svalue);
      } else {
	 old_user = strdup("");
      }

      set_pref(PREF_USER, 0, user, FALSE);
      set_pref(PREF_USER_ID, user_id, NULL, TRUE);

      jp_logf(JP_LOG_DEBUG, "user is %s\n", user);
      jp_logf(JP_LOG_DEBUG, "user id is %ld\n", user_id);
      setup_sync(SYNC_NO_PLUGINS | SYNC_INSTALL_USER);

      jp_logf(JP_LOG_DEBUG, "old user is %s\n", user);
      jp_logf(JP_LOG_DEBUG, "old user id is %ld\n", ivalue);
      set_pref(PREF_USER, 0, old_user, FALSE);
      set_pref(PREF_USER_ID, ivalue, NULL, TRUE);
      free(old_user);
   }
}


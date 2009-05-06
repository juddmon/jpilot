/* $Id: install_gui.c,v 1.27 2009/05/06 20:13:56 rousseau Exp $ */

/*******************************************************************************
 * install_gui.c
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
#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "i18n.h"
#include "utils.h"
#include "prefs.h"
#include "log.h"

/******************************* Global vars **********************************/
static int update_clist();

static GtkWidget *clist;
static int line_selected;
static GtkWidget *filew=NULL;

/****************************** Main Code *************************************/
/* the first line is 0 */
int install_remove_line(int deleted_line)
{
   FILE *in;
   FILE *out;
   char line[1002];
   char *Pc;
   int r, line_count;

   in = jp_open_home_file(EPN".install", "r");
   if (!in) {
      jp_logf(JP_LOG_DEBUG, "failed opening install_file\n");
      return EXIT_FAILURE;
   }

   out = jp_open_home_file(EPN".install.tmp", "w");
   if (!out) {
      fclose(in);
      jp_logf(JP_LOG_DEBUG, "failed opening install_file.tmp\n");
      return EXIT_FAILURE;
   }

   for (line_count=0; (!feof(in)); line_count++) {
      line[0]='\0';
      Pc = fgets(line, 1000, in);
      if (!Pc) {
	 break;
      }
      if (line_count == deleted_line) {
	 continue;
      }
      r = fprintf(out, "%s", line);
      if (r==EOF) {
	 break;
      }
   }
   fclose(in);
   fclose(out);

   rename_file(EPN".install.tmp", EPN".install");

   return EXIT_SUCCESS;
}

int install_append_line(const char *line)
{
   FILE *out;
   int r;

   out = jp_open_home_file(EPN".install", "a");
   if (!out) {
      return EXIT_FAILURE;
   }

   r = fprintf(out, "%s\n", line);
   if (r==EOF) {
      fclose(out);
      return EXIT_FAILURE;
   }
   fclose(out);

   return EXIT_SUCCESS;
}

static gboolean cb_destroy(GtkWidget *widget)
{
   filew = NULL;

   gtk_main_quit();

   return TRUE;
}

static void cb_quit(GtkWidget *widget, gpointer data)
{
   const char *sel;
   char dir[MAX_PREF_LEN+2];
   int i;

   jp_logf(JP_LOG_DEBUG, "Quit\n");

   sel = gtk_file_selection_get_filename(GTK_FILE_SELECTION(data));
   g_strlcpy(dir, sel, sizeof(dir));
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

   set_pref(PREF_INSTALL_PATH, 0, dir, TRUE);

   filew = NULL;

   gtk_widget_destroy(data);
}

static void cb_add(GtkWidget *widget, gpointer data)
{
   const char *sel;
   struct stat statb;

   jp_logf(JP_LOG_DEBUG, "Add\n");
   sel = gtk_file_selection_get_filename(GTK_FILE_SELECTION(data));
   jp_logf(JP_LOG_DEBUG, "file selected [%s]\n", sel);

   /* Check to see if its a regular file */
   if (stat(sel, &statb)) {
      jp_logf(JP_LOG_DEBUG, "File selected was not stat-able\n");
      return;
   }
   if (!S_ISREG(statb.st_mode)) {
      jp_logf(JP_LOG_DEBUG, "File selected was not a regular file\n");
      return;
   }

   install_append_line(sel);
   update_clist();
}

static void cb_remove(GtkWidget *widget, gpointer data)
{
   if (line_selected < 0) {
      return;
   }
   jp_logf(JP_LOG_DEBUG, "Remove line %d\n", line_selected);
   install_remove_line(line_selected);
   update_clist();
}

static int update_clist(void)
{
   FILE *in;
   char line[1002];
   char *Pc;
   char *new_line[2];
   int kept_line_selected;
   int count;
   int len;

   new_line[0]=line;
   new_line[1]=NULL;

   kept_line_selected = line_selected;

   in = jp_open_home_file(EPN".install", "r");
   if (!in) {
      return EXIT_FAILURE;
   }

   gtk_clist_freeze(GTK_CLIST(clist));
   gtk_clist_clear(GTK_CLIST(clist));
#ifdef __APPLE__
   gtk_clist_thaw(GTK_CLIST(clist));
   gtk_widget_hide(clist);
   gtk_widget_show_all(clist);
   gtk_clist_freeze(GTK_CLIST(clist));
#endif

   for (count=0; (!feof(in)); count++) {
      line[0]='\0';
      Pc = fgets(line, 1000, in);
      if (!Pc) {
	 break;
      }
      len=strlen(line);
      if ((line[len-1]=='\n') || (line[len-1]=='\r')) line[len-1]='\0';
      if ((line[len-2]=='\n') || (line[len-2]=='\r')) line[len-2]='\0';
      gtk_clist_append(GTK_CLIST(clist), new_line);
   }
   if (kept_line_selected > count -1) {
      kept_line_selected = count - 1;
   }
   clist_select_row(GTK_CLIST(clist), kept_line_selected, 0);
   fclose(in);
   gtk_clist_thaw(GTK_CLIST(clist));

   return EXIT_SUCCESS;
}

static void cb_clist_selection(GtkWidget      *clist,
			       gint           row,
			       gint           column,
			       GdkEventButton *event,
			       gpointer       data)
{
   line_selected = row;

   return;
}

int install_gui(GtkWidget *main_window, int w, int h, int x, int y)
{
   GtkWidget *scrolled_window;
   GtkWidget *button;
   GtkWidget *label;
   char temp[256];
   const char *svalue;
   gchar *titles[] = {_("Files to be installed")};

   if (filew) {
      return EXIT_SUCCESS;
   }

   line_selected = -1;

   g_snprintf(temp, sizeof(temp), "%s %s", PN, _("Install"));
   filew = gtk_widget_new(GTK_TYPE_FILE_SELECTION,
			  "type", GTK_WINDOW_TOPLEVEL,
			  "title", temp,
			  NULL);

   gtk_window_set_default_size(GTK_WINDOW(filew), w, h);
   gtk_widget_set_uposition(filew, x, y);

   gtk_window_set_modal(GTK_WINDOW(filew), TRUE);
   gtk_window_set_transient_for(GTK_WINDOW(filew), GTK_WINDOW(main_window));

   get_pref(PREF_INSTALL_PATH, NULL, &svalue);
   if (svalue && svalue[0]) {
      gtk_file_selection_set_filename(GTK_FILE_SELECTION(filew), svalue);
   }

   gtk_file_selection_hide_fileop_buttons((gpointer) filew);

   gtk_widget_hide((GTK_FILE_SELECTION(filew)->cancel_button));
   gtk_signal_connect(GTK_OBJECT(filew), "destroy",
                      GTK_SIGNAL_FUNC(cb_destroy), filew);

   /* Even though I hide the ok button I still want to connect its signal */
   /* because a double click on the file name also calls this callback */
   gtk_widget_hide(GTK_WIDGET(GTK_FILE_SELECTION(filew)->ok_button));
   gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(filew)->ok_button),
		      "clicked", GTK_SIGNAL_FUNC(cb_add), filew);

   clist = gtk_clist_new_with_titles(1, titles);
   gtk_widget_set_usize(GTK_WIDGET(clist), 0, 166);
   gtk_clist_column_titles_passive(GTK_CLIST(clist));
   gtk_clist_set_selection_mode(GTK_CLIST(clist), GTK_SELECTION_BROWSE);

   /* Scrolled Window for file list */
   scrolled_window = gtk_scrolled_window_new(NULL, NULL);
   gtk_container_add(GTK_CONTAINER(scrolled_window), GTK_WIDGET(clist));
   gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
   gtk_container_set_border_width(GTK_CONTAINER(scrolled_window), 5);
   gtk_box_pack_start(GTK_BOX(GTK_FILE_SELECTION(filew)->action_area),
		      scrolled_window, TRUE, TRUE, 0);

   gtk_signal_connect(GTK_OBJECT(clist), "select_row",
		      GTK_SIGNAL_FUNC(cb_clist_selection), NULL);
   gtk_widget_show(clist);
   gtk_widget_show(scrolled_window);

   label = gtk_label_new(_("To change to a hidden directory type it below and hit TAB"));
   gtk_box_pack_start(GTK_BOX(GTK_FILE_SELECTION(filew)->main_vbox),
		      label, FALSE, FALSE, 0);
   gtk_widget_show(label);

   /* Add/Remove/Quit buttons */
   button = gtk_button_new_from_stock(GTK_STOCK_ADD);
   gtk_box_pack_start(GTK_BOX(GTK_FILE_SELECTION(filew)->ok_button->parent),
		      button, TRUE, TRUE, 0);
   gtk_signal_connect(GTK_OBJECT(button),
		      "clicked", GTK_SIGNAL_FUNC(cb_add), filew);
   gtk_widget_show(button);

   button = gtk_button_new_from_stock(GTK_STOCK_REMOVE);
   gtk_box_pack_start(GTK_BOX(GTK_FILE_SELECTION(filew)->ok_button->parent),
		      button, TRUE, TRUE, 0);
   gtk_signal_connect(GTK_OBJECT(button),
		      "clicked", GTK_SIGNAL_FUNC(cb_remove), filew);
   gtk_widget_show(button);

   button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
   gtk_box_pack_start(GTK_BOX(GTK_FILE_SELECTION(filew)->ok_button->parent),
		      button, TRUE, TRUE, 0);
   gtk_signal_connect(GTK_OBJECT(button),
		      "clicked", GTK_SIGNAL_FUNC(cb_quit), filew);
   gtk_widget_show(button);

   gtk_widget_show(filew);

   update_clist();

   gtk_main();

   return EXIT_SUCCESS;
}


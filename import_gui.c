/* $Id: import_gui.c,v 1.17 2004/11/28 16:20:04 rousseau Exp $ */

/*******************************************************************************
 * import_gui.c
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

#include "config.h"
#include "i18n.h"
#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include "utils.h"
#include "prefs.h"
#include "log.h"

static GtkWidget *radio_types[MAX_IMPORT_TYPES+1];
static int radio_file_types[MAX_IMPORT_TYPES+1];
static int line_selected;
static GtkWidget *filew=NULL;
static GtkWidget *import_record_ask_window=NULL;
static int glob_import_record_ask_button_pressed;
int (*glob_import_callback)(GtkWidget *parent_window, const char *file_path, int type);
int glob_type_selected;


/*
 * This function reads until it finds a non separator character,
 * then reads a string.  Spaces, commas, etc. can be inside quotes.
 * Escaped quotes (double quotes) are converted to single.
 * Return value is size of text.
 *  -1 for EOL
 */
int read_csv_field(FILE *in, char *text, int size, int new_line)
{
   int n, c;
   char sep[]=",\t \r\n";
   int quoted;

   n=0;
   quoted=0;
   text[0]='\0';
   /* Read until a non separator character is found */
   while (1) {
      c=getc(in);
      if (feof(in)) {
	 return 0;
	 text[++n]='\0';
      }
      if (!strchr(sep, c)) {
	 ungetc(c, in);
	 break;
      }
   }
   /* Read the field */
   while (1) {
      c=fgetc(in);
      if (feof(in)) break;
      /* Look for quote */
      if (c=='"') {
	 if (quoted) {
	    c=fgetc(in);
	    if (c=='"') {
	       /* Found double quotes, convert to single */
	    } else {
	       quoted=(quoted&1)^1;
	       ungetc(c, in);
	       continue;
	    }
	 } else {
	    quoted=1;
	    continue;
	 }
      }
      /* Look for separators */
      if (strchr(sep, c)) {
	 if (!quoted) {
	    text[n++]='\0';
	    break;
	 }
      }
      text[n++]=c;
      if (n+1>=size)
      {
	  text[n++]='\0';
	  return n;
      }
   }
   text[n++]='\0';
   return n;
}

int guess_file_type(const char *path)
{
   FILE *in;
   char text[256];

   if (!path) return IMPORT_TYPE_UNKNOWN;
   in=fopen(path, "r");
   if (!in) return IMPORT_TYPE_UNKNOWN;
   if (dat_check_if_dat_file(in)) {
      fclose(in);
      return IMPORT_TYPE_DAT;
   }
   fseek(in, 0, SEEK_SET);
   fread(text, 1, 15, in);
   if (!strncmp(text, "CSV ", 4)) {
      fclose(in);
      return IMPORT_TYPE_CSV;
   }
   fclose(in);
   return IMPORT_TYPE_TEXT;
}

/* Main import file selection window */
static gboolean cb_destroy(GtkWidget *widget)
{
   filew = NULL;
   return FALSE;
}

static void
cb_quit(GtkWidget *widget,
	gpointer   data)
{
   const char *sel;
   char dir[MAX_PREF_VALUE+2];
   int i;

   jp_logf(JP_LOG_DEBUG, "Quit\n");

   sel = gtk_file_selection_get_filename(GTK_FILE_SELECTION(filew));
   strncpy(dir, sel, MAX_PREF_VALUE);
   dir[MAX_PREF_VALUE]='\0';
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

   set_pref(PREF_MEMO_IMPORT_PATH, 0, dir, TRUE);

   filew = NULL;
   gtk_widget_destroy(data);
}

static void
cb_type(GtkWidget *widget,
	gpointer   data)
{
   glob_type_selected=GPOINTER_TO_INT(data);
}

static void
cb_import(GtkWidget *widget,
	  gpointer   filesel)
{
   const char *sel;
   struct stat statb;

   jp_logf(JP_LOG_DEBUG, "cb_import\n");
   sel = gtk_file_selection_get_filename(GTK_FILE_SELECTION(filesel));
   jp_logf(JP_LOG_DEBUG, "file selected [%s]\n", sel);

   /*Check to see if its a regular file */
   if (stat(sel, &statb)) {
      jp_logf(JP_LOG_DEBUG, "File selected was not stat-able\n");
      return;
   }
   if (!S_ISREG(statb.st_mode)) {
      jp_logf(JP_LOG_DEBUG, "File selected was not a regular file\n");
      return;
   }
   glob_import_callback(filesel, sel, glob_type_selected);
}

static void cb_import_select_row(GtkWidget *file_clist,
				 gint row,
				 gint column,
				 GdkEventButton *bevent,
				 gpointer data)
{
   const char *sel;
   struct stat statb;
   int guessed_type;
   int i;

   jp_logf(JP_LOG_DEBUG, "cb_import_select_row\n");
   sel = gtk_file_selection_get_filename(GTK_FILE_SELECTION(filew));

   /*Check to see if its a regular file */
   if (stat(sel, &statb)) {
      jp_logf(JP_LOG_DEBUG, "File selected was not stat-able\n");
      return;
   }
   if (!S_ISREG(statb.st_mode)) {
      jp_logf(JP_LOG_DEBUG, "File selected was not a regular file\n");
      return;
   }

   guessed_type=guess_file_type(sel);
   for (i=0; i<MAX_IMPORT_TYPES; i++) {
      if (radio_types[i]==NULL) break;
      if (guessed_type==radio_file_types[i]) {
	 gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_types[i]), TRUE);
	 break;
      }
   }

   return;
}

void import_gui(GtkWidget *main_window, GtkWidget *main_pane,
		char *type_desc[], int type_int[],
		int (*import_callback)(GtkWidget *parent_window,
				       const char *file_path, int type))
{
   GtkWidget *button;
   GtkWidget *vbox, *hbox;
   GtkWidget *label;
   char temp[256];
   const char *svalue;
   GSList *group;
   int i;
   int pw, ph, px, py;

   if (filew) {
      return;
   }

   line_selected = -1;

   gdk_window_get_size(main_window->window, &pw, &ph);
   gdk_window_get_root_origin(main_window->window, &px, &py);

#ifdef ENABLE_GTK2
   pw = gtk_paned_get_position(GTK_PANED(main_pane));
#else
   pw = GTK_PANED(main_pane)->handle_xpos;
#endif
   px+=40;

   g_snprintf(temp, sizeof(temp), "%s %s", PN, _("Import"));

   filew = gtk_widget_new(GTK_TYPE_FILE_SELECTION,
			  "type", GTK_WINDOW_TOPLEVEL,
			  "title", temp,
			  NULL);

   gtk_window_set_default_size(GTK_WINDOW(filew), pw, ph);
   gtk_widget_set_uposition(filew, px, py);

   gtk_window_set_modal(GTK_WINDOW(filew), TRUE);
   gtk_window_set_transient_for(GTK_WINDOW(filew), GTK_WINDOW(main_window));

   get_pref(PREF_MEMO_IMPORT_PATH, NULL, &svalue);
   if (svalue && svalue[0]) {
      gtk_file_selection_set_filename(GTK_FILE_SELECTION(filew), svalue);
   }

   glob_import_callback=import_callback;

   /* Set the type to match the first button, which will be set */
   glob_type_selected=type_int[0];

   gtk_widget_hide((GTK_FILE_SELECTION(filew)->cancel_button));
   gtk_signal_connect(GTK_OBJECT(filew), "destroy",
                      GTK_SIGNAL_FUNC(cb_destroy), filew);

   /*Even though I hide the ok button I still want to connect its signal */
   /*because a double click on the file name also calls this callback */
   gtk_widget_hide(GTK_WIDGET(GTK_FILE_SELECTION(filew)->ok_button));
   gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(filew)->ok_button),
		      "clicked", GTK_SIGNAL_FUNC(cb_import), filew);

   label = gtk_label_new(_("To change to a hidden directory type it below and hit TAB"));
   gtk_box_pack_start(GTK_BOX(GTK_FILE_SELECTION(filew)->main_vbox),
		      label, FALSE, FALSE, 0);
   gtk_widget_show(label);


   button = gtk_button_new_with_label(_("Import"));
   gtk_box_pack_start(GTK_BOX(GTK_FILE_SELECTION(filew)->ok_button->parent),
		      button, TRUE, TRUE, 0);
   gtk_signal_connect(GTK_OBJECT(button),
		      "clicked", GTK_SIGNAL_FUNC(cb_import), filew);
   gtk_widget_show(button);

   button = gtk_button_new_with_label(_("Done"));
   gtk_box_pack_start(GTK_BOX(GTK_FILE_SELECTION(filew)->ok_button->parent),
		      button, TRUE, TRUE, 0);
   gtk_signal_connect(GTK_OBJECT(button),
		      "clicked", GTK_SIGNAL_FUNC(cb_quit), filew);
   gtk_widget_show(button);

   /* File Type */
   vbox=gtk_vbox_new(FALSE, 0);
   hbox=gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(GTK_FILE_SELECTION(filew)->action_area),
		      vbox, TRUE, TRUE, 0);
   gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
   label = gtk_label_new(_("Import File Type"));
   gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

   group = NULL;
   for (i=0; i<MAX_IMPORT_TYPES; i++) {
      if (type_desc[i]==NULL) break;
      radio_types[i] = gtk_radio_button_new_with_label(group, type_desc[i]);
      radio_file_types[i] = type_int[i];
      group = gtk_radio_button_group(GTK_RADIO_BUTTON(radio_types[i]));
      gtk_box_pack_start(GTK_BOX(vbox), radio_types[i], TRUE, TRUE, 0);
      gtk_signal_connect(GTK_OBJECT(radio_types[i]), "clicked",
			 GTK_SIGNAL_FUNC(cb_type), GINT_TO_POINTER(type_int[i]));
   }
   radio_types[i]=NULL;
   radio_file_types[i]=0;

   /* This callback is for a file guess algorithm and to pre-push
    * the type buttons */
#ifdef ENABLE_GTK2
   gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(filew)->file_list),
		      "cursor_changed", GTK_SIGNAL_FUNC(cb_import_select_row), NULL);
#else
   gtk_signal_connect(GTK_OBJECT(GTK_CLIST(GTK_FILE_SELECTION(filew)->file_list)),
		      "select_row", GTK_SIGNAL_FUNC(cb_import_select_row), NULL);
#endif

   gtk_widget_show_all(vbox);

   gtk_widget_show(filew);
}


/*
 * Import record display and choice.
 * Put up the record for viewing and ask if it should be imported.
 */
/* Main import file selection window */
static gboolean cb_import_record_ask_destroy(GtkWidget *widget)
{
   import_record_ask_window = NULL;
   gtk_main_quit();
   return FALSE;
}

static void
cb_import_record_ask_quit(GtkWidget *widget,
			  gpointer   data)
{
   glob_import_record_ask_button_pressed=GPOINTER_TO_INT(data);
   gtk_widget_destroy(import_record_ask_window);
}

int import_record_ask(GtkWidget *main_window, GtkWidget *pane,
		      char *text, struct CategoryAppInfo *cai,
		      char *old_cat_name,
		      int priv, int suggested_cat_num, int *new_cat_num)
{
   GtkWidget *button;
   GtkWidget *vbox;
   GtkWidget *temp_hbox;
   GtkWidget *textw;
#ifdef ENABLE_GTK2
   GObject   *textw_buffer;
#endif
   GtkWidget *label;
#ifdef ENABLE_GTK2
   GtkWidget *scrolled_window;
#else
   GtkWidget *vscrollbar;
#endif
   int pw, ph;
   gint px, py;
   char str[100];

   /* There is no support yet for changing the suggested category */
   /* Like a menu for selecting cat to be imported into, etc. */
   *new_cat_num = suggested_cat_num;

   glob_import_record_ask_button_pressed = DIALOG_SAID_IMPORT_QUIT;

   gdk_window_get_size(main_window->window, &pw, &ph);

   gdk_window_get_root_origin(main_window->window, &px, &py);

#ifdef ENABLE_GTK2
   pw = gtk_paned_get_position(GTK_PANED(pane));
#else
   pw = GTK_PANED(pane)->handle_xpos;
#endif
   px+=40;

   import_record_ask_window = gtk_widget_new(GTK_TYPE_WINDOW,
					     "type", GTK_WINDOW_TOPLEVEL,
					     "title", _("Import"),
					     NULL);
   gtk_window_set_default_size(GTK_WINDOW(import_record_ask_window), pw, ph);
   gtk_widget_set_uposition(GTK_WIDGET(import_record_ask_window), px, py);

   gtk_window_set_modal(GTK_WINDOW(import_record_ask_window), TRUE);
   gtk_window_set_transient_for(GTK_WINDOW(import_record_ask_window), GTK_WINDOW(main_window));

   gtk_container_set_border_width(GTK_CONTAINER(import_record_ask_window), 5);

   gtk_signal_connect(GTK_OBJECT(import_record_ask_window), "destroy",
                      GTK_SIGNAL_FUNC(cb_import_record_ask_destroy),
		      import_record_ask_window);

   vbox=gtk_vbox_new(FALSE, 0);
   gtk_container_add(GTK_CONTAINER(import_record_ask_window), vbox);

   /* Private */
   if (priv) {
      g_snprintf(str, sizeof(str), _("Record was marked as private"));
   } else {
      g_snprintf(str, sizeof(str), _("Record was not marked as private"));
   }
   label = gtk_label_new(str);
   gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
   gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);


   g_snprintf(str, sizeof(str), _("Category before import was: [%s]"), old_cat_name);
   label = gtk_label_new(str);
   gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
   gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);


   g_snprintf(str, sizeof(str), _("Record will be put in category [%s]"),
	      cai->name[suggested_cat_num]);
   label = gtk_label_new(str);
   gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
   gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);


   /* Make a text window with scrollbar */
   temp_hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox), temp_hbox, TRUE, TRUE, 0);

#ifdef ENABLE_GTK2
   textw = gtk_text_view_new();
   textw_buffer = G_OBJECT(gtk_text_view_get_buffer(GTK_TEXT_VIEW(textw)));
   gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(textw), FALSE);
   gtk_text_view_set_editable(GTK_TEXT_VIEW(textw), FALSE);
   gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(textw), GTK_WRAP_WORD);

   scrolled_window = gtk_scrolled_window_new(NULL, NULL);
   gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
   gtk_container_set_border_width(GTK_CONTAINER(scrolled_window), 1);
   gtk_container_add(GTK_CONTAINER(scrolled_window), textw);
   gtk_box_pack_start_defaults(GTK_BOX(temp_hbox), scrolled_window);
#else
   textw = gtk_text_new(NULL, NULL);
   gtk_text_set_editable(GTK_TEXT(textw), FALSE);
   gtk_text_set_word_wrap(GTK_TEXT(textw), TRUE);
   vscrollbar = gtk_vscrollbar_new(GTK_TEXT(textw)->vadj);
   gtk_box_pack_start(GTK_BOX(temp_hbox), textw, TRUE, TRUE, 0);
   gtk_box_pack_start(GTK_BOX(temp_hbox), vscrollbar, FALSE, FALSE, 0);
#endif

   if (text) {
#ifdef ENABLE_GTK2
      gtk_text_buffer_set_text(GTK_TEXT_BUFFER(textw_buffer), text, -1);
#else
      gtk_text_insert(GTK_TEXT(textw), NULL, NULL, NULL, text, -1);
#endif
   }

   /* Box for buttons  */
   temp_hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox), temp_hbox, FALSE, FALSE, 0);

   /* Import button */
   button = gtk_button_new_with_label(_("Import"));
   gtk_box_pack_start(GTK_BOX(temp_hbox), button, TRUE, TRUE, 0);
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_import_record_ask_quit),
		      GINT_TO_POINTER(DIALOG_SAID_IMPORT_YES));

   /* Import All button */
   button = gtk_button_new_with_label(_("Import All"));
   gtk_box_pack_start(GTK_BOX(temp_hbox), button, TRUE, TRUE, 0);
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_import_record_ask_quit),
		      GINT_TO_POINTER(DIALOG_SAID_IMPORT_ALL));

   /* Skip button */
   button = gtk_button_new_with_label(_("Skip"));
   gtk_box_pack_start(GTK_BOX(temp_hbox), button, TRUE, TRUE, 0);
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_import_record_ask_quit),
		      GINT_TO_POINTER(DIALOG_SAID_IMPORT_SKIP));

   /* Quit button */
   button = gtk_button_new_with_label(_("Quit"));
   gtk_box_pack_start(GTK_BOX(temp_hbox), button, TRUE, TRUE, 0);
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_import_record_ask_quit),
		      GINT_TO_POINTER(DIALOG_SAID_IMPORT_QUIT));

   gtk_widget_show_all(import_record_ask_window);

   gtk_main();

   return glob_import_record_ask_button_pressed;
}

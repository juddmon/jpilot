/*******************************************************************************
 * import_gui.c
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
#include "export.h"

/******************************* Global vars **********************************/
static GtkWidget *radio_types[MAX_IMPORT_TYPES+1];
static int radio_file_types[MAX_IMPORT_TYPES+1];
static int line_selected;
static GtkWidget *import_record_ask_window=NULL;
static int glob_import_record_ask_button_pressed;
static int glob_type_selected;

/****************************** Prototypes ************************************/
static int (*glob_import_callback)(GtkWidget *parent_window, const char *file_path, int type);

/****************************** Main Code *************************************/

/*
 * This function reads a CSV entry until it finds the next seperator(',').
 * Spaces, commas, newlines, etc. are allowed inside quotes.
 * Escaped quotes (double quotes) are converted to single occurrences.
 * Return value is size of text including null terminator.
 */
int read_csv_field(FILE *in, char *text, int size)
{
   int n, c;
   char sep[]=",\t \r\n";
   char whitespace[]="\t \r\n";
   int quoted;

   n=0;
   quoted=FALSE;
   text[0]='\0';

   /* Read the field */
   while (1) {
      c=fgetc(in);

      /* Look for EOF */
      if (feof(in)) 
         break;
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
            quoted=TRUE;
            continue;
         }
      }
      /* Look for separators */
      if (strchr(sep, c)) {
         if (!quoted) {
            if (c != ',') {
               /* skip whitespace  */
               while (1) {
                  c=getc(in);
                  if (feof(in)) {
                     text[n++]='\0';
                     return n;
                  }
                  if (strchr(whitespace, c)) {
                     continue;
                  } else {
                     ungetc(c, in);
                     break;
                  }
               }
            }
            /* after sep processing, break out of reading field */
            break;   
         } /* end if !quoted */
      } /* end if separator */

      /* Ordinary character, add to field */
      text[n++]=c;
      if (n+1>=size)
      {
         text[n++]='\0';
         return n;
      }
   }   /* end while(1) reading field */

   /* Terminate string and return */
   text[n++]='\0';
   return n;
}

static int guess_file_type(const char *path)
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
   if (fread(text, 1, 15, in) < 1) {
      jp_logf(JP_LOG_WARN, "fread failed %s %d\n", __FILE__, __LINE__);
   }
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
   widget = NULL;
   return FALSE;
}

static void cb_quit(GtkWidget *widget, gpointer data)
{
   const char *sel;
   char dir[MAX_PREF_LEN+2];
   int i;

   jp_logf(JP_LOG_DEBUG, "Quit\n");

   sel = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(widget));
   strncpy(dir, sel, MAX_PREF_LEN);
   dir[MAX_PREF_LEN]='\0';
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


   gtk_widget_destroy(widget);
}

static void cb_type(GtkWidget *widget, gpointer data)
{
   glob_type_selected=GPOINTER_TO_INT(data);
}

static void cb_import(GtkWidget *widget, gpointer filesel)
{
   const char *sel;
   struct stat statb;

   jp_logf(JP_LOG_DEBUG, "cb_import\n");
   //sel = gtk_file_selection_get_filename(GTK_FILE_SELECTION(filesel));
   sel = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(widget));
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
   glob_import_callback(widget, sel, glob_type_selected);
   cb_quit(widget, widget);
}

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

/*
 * Import record display and choice.
 * Put up the record for viewing and ask if it should be imported.
 */
int import_record_ask(GtkWidget *main_window, GtkWidget *pane,
                      char *text, struct CategoryAppInfo *cai,
                      char *old_cat_name,
                      int priv, int suggested_cat_num, int *new_cat_num)
{
   GtkWidget *button;
   GtkWidget *vbox;
   GtkWidget *temp_hbox;
   GtkWidget *textw;
   GObject   *textw_buffer;
   GtkWidget *label;
   GtkWidget *scrolled_window;
   int pw, ph;
   gint px, py;
   char str[100];
   char *l;
   long char_set;

   /* There is no support yet for changing the suggested category */
   /* A menu for selecting cat to be imported into is desirable */
   *new_cat_num = suggested_cat_num;

   glob_import_record_ask_button_pressed = DIALOG_SAID_IMPORT_QUIT;

   gdk_window_get_size(gtk_widget_get_window(main_window), &pw, &ph);
   gdk_window_get_root_origin(gtk_widget_get_window(main_window), &px, &py);
   pw = gtk_paned_get_position(GTK_PANED(pane));
   px+=40;

   import_record_ask_window = gtk_widget_new(GTK_TYPE_WINDOW,
                                             "type", GTK_WINDOW_TOPLEVEL,
                                             "title", _("Import"),
                                             NULL);
   gtk_window_set_default_size(GTK_WINDOW(import_record_ask_window), pw, ph);
   gtk_widget_set_uposition(GTK_WIDGET(import_record_ask_window), px, py);
   gtk_container_set_border_width(GTK_CONTAINER(import_record_ask_window), 5);
   gtk_window_set_modal(GTK_WINDOW(import_record_ask_window), TRUE);
   gtk_window_set_transient_for(GTK_WINDOW(import_record_ask_window), GTK_WINDOW(main_window));

   g_signal_connect(G_OBJECT(import_record_ask_window), "destroy",
                      G_CALLBACK(cb_import_record_ask_destroy),
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

   /* Category */
   get_pref(PREF_CHAR_SET, &char_set, NULL);
   l = charset_p2newj(old_cat_name, 16, char_set);
   g_snprintf(str, sizeof(str), _("Category before import was: [%s]"), l);
   g_free(l);
   label = gtk_label_new(str);
   gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
   gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);


   l = charset_p2newj(cai->name[suggested_cat_num], 16, char_set);
   g_snprintf(str, sizeof(str), _("Record will be put in category [%s]"), l);
   g_free(l);
   label = gtk_label_new(str);
   gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
   gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

   /* Text window with scrollbar to display record */
   temp_hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox), temp_hbox, TRUE, TRUE, 0);

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

   if (text) {
      gtk_text_buffer_set_text(GTK_TEXT_BUFFER(textw_buffer), text, -1);
   }

   temp_hbox = gtk_hbutton_box_new();
   gtk_button_box_set_spacing(GTK_BUTTON_BOX(temp_hbox), 6);
   gtk_container_set_border_width(GTK_CONTAINER(temp_hbox), 6);
   gtk_box_pack_start(GTK_BOX(vbox), temp_hbox, FALSE, FALSE, 0);

   /* Import button */
   button = gtk_button_new_with_label(_("Import"));
   gtk_box_pack_start(GTK_BOX(temp_hbox), button, TRUE, TRUE, 0);
   g_signal_connect(G_OBJECT(button), "clicked",
                      G_CALLBACK(cb_import_record_ask_quit),
                      GINT_TO_POINTER(DIALOG_SAID_IMPORT_YES));

   /* Import All button */
   button = gtk_button_new_with_label(_("Import All"));
   gtk_box_pack_start(GTK_BOX(temp_hbox), button, TRUE, TRUE, 0);
   g_signal_connect(G_OBJECT(button), "clicked",
                      G_CALLBACK(cb_import_record_ask_quit),
                      GINT_TO_POINTER(DIALOG_SAID_IMPORT_ALL));

   /* Skip button */
   button = gtk_button_new_with_label(_("Skip"));
   gtk_box_pack_start(GTK_BOX(temp_hbox), button, TRUE, TRUE, 0);
   g_signal_connect(G_OBJECT(button), "clicked",
                      G_CALLBACK(cb_import_record_ask_quit),
                      GINT_TO_POINTER(DIALOG_SAID_IMPORT_SKIP));

   /* Quit button */
   button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
   gtk_box_pack_start(GTK_BOX(temp_hbox), button, TRUE, TRUE, 0);
   g_signal_connect(G_OBJECT(button), "clicked",
                      G_CALLBACK(cb_import_record_ask_quit),
                      GINT_TO_POINTER(DIALOG_SAID_IMPORT_QUIT));

   gtk_widget_show_all(import_record_ask_window);

   gtk_main();

   return glob_import_record_ask_button_pressed;
}

void import_gui(GtkWidget *main_window, GtkWidget *main_pane,
                char *type_desc[], int type_int[],
                int (*import_callback)(GtkWidget *parent_window,
                const char *file_path, int type)) {
    GtkWidget *button;
    GtkWidget *vbox, *hbox;
    GtkWidget *label;
    char title[256];
    const char *svalue;
    GSList *group;
    int i;
    int pw, ph, px, py;
    GtkWidget *fileChooserWidget;


    line_selected = -1;


    g_snprintf(title, sizeof(title), "%s %s", PN, _("Import"));
    fileChooserWidget = gtk_file_chooser_dialog_new(_("Import"), main_window, GTK_FILE_CHOOSER_ACTION_OPEN,
                                                    GTK_STOCK_CLOSE, GTK_RESPONSE_CANCEL, "Import", GTK_RESPONSE_ACCEPT,
                                                    NULL);
    get_pref(PREF_MEMO_IMPORT_PATH, NULL, &svalue);
    if (svalue && svalue[0]) {
       gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(fileChooserWidget), svalue);
    }
    GtkBox *extraWidget = GTK_BOX(gtk_hbox_new(FALSE, 0));

    /* File Type radio buttons */
    vbox = gtk_vbox_new(FALSE, 0);
    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(extraWidget,
                       vbox, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
    label = gtk_label_new(_("Import File Type"));
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    group = NULL;
    for (i = 0; i < MAX_IMPORT_TYPES; i++) {
        if (type_desc[i] == NULL) break;
        radio_types[i] = gtk_radio_button_new_with_label(group, _(type_desc[i]));
        radio_file_types[i] = type_int[i];
        group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio_types[i]));
        gtk_box_pack_start(GTK_BOX(vbox), radio_types[i], TRUE, TRUE, 0);
        g_signal_connect(G_OBJECT(radio_types[i]), "clicked",
                           G_CALLBACK(cb_type), GINT_TO_POINTER(type_int[i]));
    }
    radio_types[i] = NULL;
    radio_file_types[i] = 0;
    gtk_widget_show_all(vbox);
    glob_import_callback = import_callback;

    /* Set the type to match the first button, which will be set */
    glob_type_selected = type_int[0];
    gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER(fileChooserWidget), GTK_WIDGET(extraWidget));
    if (gtk_dialog_run(GTK_DIALOG (fileChooserWidget)) == GTK_RESPONSE_ACCEPT) {
        //run other window here..  not sure where it is defined at the moment.
        cb_import(fileChooserWidget,NULL);
        gtk_widget_destroy(fileChooserWidget);
    } else {
        gtk_widget_destroy(fileChooserWidget);
    }



    g_signal_connect(G_OBJECT(fileChooserWidget), "destroy",
                       G_CALLBACK(cb_destroy), fileChooserWidget);
}





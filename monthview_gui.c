/* monthview_gui.c
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
 */

#include "config.h"
#include "i18n.h"
#include <gtk/gtk.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "utils.h"
#include "print.h"
#include "prefs.h"
#include "log.h"
#include "datebook.h"
#include <pi-datebook.h>
#include "config.h"

extern int datebook_category;

static GtkWidget *window=NULL;
static GtkWidget *glob_month_vbox;
static GtkWidget *glob_month_labels[37];
#ifdef ENABLE_GTK2
static GObject   *glob_month_text_buffers[37];
static GtkWidget *glob_month_text_views[37];
static GObject   *big_text_buffer;
#else
static GtkWidget *glob_month_texts[37];
static GtkWidget *big_text;
#endif
static GtkWidget *glob_month_month_label;
static GtkWidget *glob_last_hbox_row;
static int glob_offset;
static struct tm glob_month_date;

int display_months_appts(struct tm *glob_month_date, GtkWidget **glob_month_texts);
void hide_show_month_boxes();

static gboolean cb_destroy(GtkWidget *widget)
{
   int n;
   GString *gstr;
   GtkWidget *text;

   window = NULL;

   for (n=0; n<37; n++) {
#ifdef ENABLE_GTK2
      text = glob_month_text_views[n];
#else
      text = glob_month_texts[n];
#endif
      gstr = gtk_object_get_data(GTK_OBJECT(text), "gstr");
      if (gstr) {
	 g_string_free(gstr, TRUE);
	 gtk_object_remove_data(GTK_OBJECT(text), "gstr");
      }
   }
   return FALSE;
}

static void
cb_quit(GtkWidget *widget, gpointer data)
{
   int w, h;

   gdk_window_get_size(window->window, &w, &h);
   set_pref(PREF_MONTHVIEW_WIDTH, w, NULL, FALSE);
   set_pref(PREF_MONTHVIEW_HEIGHT, h, NULL, FALSE);

   gtk_widget_destroy(data);
}

static void
cb_month_move(GtkWidget *widget,
	      gpointer   data)
{
   if (GPOINTER_TO_INT(data)==-1) {
      glob_month_date.tm_mday=15;
      sub_days_from_date(&glob_month_date, 30);
   }
   if (GPOINTER_TO_INT(data)==1) {
      glob_month_date.tm_mday=15;
      add_days_to_date(&glob_month_date, 30);
   }
   hide_show_month_boxes();
#ifdef ENABLE_GTK2
   display_months_appts(&glob_month_date, glob_month_text_views);
#else
   display_months_appts(&glob_month_date, glob_month_texts);
#endif
}

/*----------------------------------------------------------------------
 * cb_month_print	This is where month printing is kicked off from
 *----------------------------------------------------------------------*/

static void cb_month_print(GtkWidget *widget, gpointer data)
{
   long paper_size;

   jp_logf(JP_LOG_DEBUG, "cb_month_print called\n");
   if (print_gui(window, DATEBOOK, 3, 0x04) == DIALOG_SAID_PRINT) {
      get_pref(PREF_PAPER_SIZE, &paper_size, NULL);
      if (paper_size==1) {
	 print_months_appts(&glob_month_date, PAPER_A4);
      } else {
	 print_months_appts(&glob_month_date, PAPER_Letter);
      }
   }
}

/*----------------------------------------------------------------------*/

static void
cb_enter_notify(GtkWidget *widget, GdkEvent *event, gpointer data)
{
   static int prev_day=-1;
   GString *gstr;

   if (prev_day==GPOINTER_TO_INT(data)+1-glob_offset) {
      return;
   }
   prev_day = GPOINTER_TO_INT(data)+1-glob_offset;

   gstr = gtk_object_get_data(GTK_OBJECT(widget), "gstr");

#ifdef ENABLE_GTK2
   if (gstr) {
      gtk_text_buffer_set_text(GTK_TEXT_BUFFER(big_text_buffer), gstr->str, -1);
   } else {
      gtk_text_buffer_set_text(GTK_TEXT_BUFFER(big_text_buffer), "", -1);
   }
#else
   gtk_text_set_point(GTK_TEXT(big_text),
		      gtk_text_get_length(GTK_TEXT(big_text)));
   gtk_text_backward_delete(GTK_TEXT(big_text),
			    gtk_text_get_length(GTK_TEXT(big_text)));
   if (gstr) {
      gtk_text_insert(GTK_TEXT(big_text), NULL, NULL, NULL, gstr->str, -1);
   }
#endif
}

/*
 * Hide, or show month boxes (days) according to the month.
 * Also, set a global offset for indexing day 1.
 * Also relabel day labels.
 */
void hide_show_month_boxes()
{
   int n;
   int dow, ndim;
   long fdow;
   char str[8];
   int d;
   GtkWidget *text;

   get_month_info(glob_month_date.tm_mon, 1, glob_month_date.tm_year, &dow, &ndim);

   get_pref(PREF_FDOW, &fdow, NULL);

   glob_offset = (7+dow-fdow)%7;

   d = 1 - glob_offset;

   if (glob_offset + ndim > 35) {
      gtk_widget_show(GTK_WIDGET(glob_last_hbox_row));
   } else {
      gtk_widget_hide(GTK_WIDGET(glob_last_hbox_row));
   }

   for (n=0; n<37; n++, d++) {
#ifdef ENABLE_GTK2
      text = glob_month_text_views[n];
#else
      text = glob_month_texts[n];
#endif
      g_snprintf(str, sizeof(str), "%d", d);
      gtk_label_set_text(GTK_LABEL(glob_month_labels[n]), str);
      if (n<7) {
	 if (d>0) {
	    gtk_widget_show(GTK_WIDGET(text));
	    gtk_widget_show(GTK_WIDGET(glob_month_labels[n]));
	 } else {
	    gtk_widget_hide(GTK_WIDGET(text));
	    gtk_widget_hide(GTK_WIDGET(glob_month_labels[n]));
	 }
      }
      if (n>27) {
	 if (d<=ndim) {
	    gtk_widget_show(GTK_WIDGET(text));
	    gtk_widget_show(GTK_WIDGET(glob_month_labels[n]));
	 } else {
	    gtk_widget_hide(GTK_WIDGET(text));
	    gtk_widget_hide(GTK_WIDGET(glob_month_labels[n]));
	 }
      }
   }
}

void create_month_boxes_texts(GtkWidget *month_vbox)
{
   int i, j, n;
   GtkWidget *hbox_row;
   GtkWidget *vbox;
   GtkWidget *text;
   char str[80];

   n=0;
   for (i=0; i<6; i++) {
      hbox_row = gtk_hbox_new(TRUE, 0);
      gtk_box_pack_start(GTK_BOX(month_vbox), hbox_row, TRUE, TRUE, 0);
      for (j=0; j<7; j++) {
	 vbox = gtk_vbox_new(FALSE, 0);
#ifdef ENABLE_GTK2
	 gtk_box_pack_start(GTK_BOX(hbox_row), vbox, TRUE, TRUE, 2);
#else
	 gtk_box_pack_start(GTK_BOX(hbox_row), vbox, TRUE, TRUE, 0);
#endif
	 n=i*7+j;
	 if (n<37) {
	    sprintf(str, "%d", n + 1);
	    /* Day of month labels */
	    glob_month_labels[n] = gtk_label_new(str);
	    gtk_misc_set_alignment(GTK_MISC(glob_month_labels[n]), 0.0, 0.5);
	    gtk_box_pack_start(GTK_BOX(vbox), glob_month_labels[n], FALSE, FALSE, 0);

#ifdef ENABLE_GTK2
	    /* text variable only used to save some typing */
	    text = glob_month_text_views[n] = gtk_text_view_new();
	    glob_month_text_buffers[n] = 
	      G_OBJECT(gtk_text_view_get_buffer(GTK_TEXT_VIEW(text)));
	    gtk_widget_set_usize(GTK_WIDGET(text), 10, 10);
	    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(text), FALSE);
	    gtk_text_view_set_editable(GTK_TEXT_VIEW(text), FALSE);
	    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text), GTK_WRAP_WORD);
	    /* motion notify is overkill but the enter_notify event doesn't work */
	    gtk_signal_connect(GTK_OBJECT(text), "motion_notify_event",
			       GTK_SIGNAL_FUNC(cb_enter_notify), GINT_TO_POINTER(n));
	    gtk_box_pack_start(GTK_BOX(vbox), text, TRUE, TRUE, 0);
#else
	    text = glob_month_texts[n] = gtk_text_new(NULL, NULL);
	    gtk_widget_set_usize(GTK_WIDGET(glob_month_texts[n]), 10, 10);
	    gtk_text_set_word_wrap(GTK_TEXT(glob_month_texts[n]), FALSE);
	    gtk_signal_connect(GTK_OBJECT(glob_month_texts[n]), "enter_notify_event",
			       GTK_SIGNAL_FUNC(cb_enter_notify), GINT_TO_POINTER(n));
	    gtk_box_pack_start(GTK_BOX(vbox), text, TRUE, TRUE, 0);
#endif
	 }
      }
   }
   glob_last_hbox_row = hbox_row;
   
#ifdef ENABLE_GTK2
   text = gtk_text_view_new();
   big_text_buffer = G_OBJECT(gtk_text_view_get_buffer(GTK_TEXT_VIEW(text)));
   gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(text), FALSE);
   gtk_text_view_set_editable(GTK_TEXT_VIEW(text), FALSE);
   gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text), GTK_WRAP_WORD);
   gtk_widget_set_usize(GTK_WIDGET(text), 10, 10);
   gtk_box_pack_start(GTK_BOX(month_vbox), text, TRUE, TRUE, 4);
#else
   big_text = gtk_text_new(NULL, NULL);
   gtk_widget_set_usize(GTK_WIDGET(big_text), 10, 10);
   gtk_box_pack_start(GTK_BOX(month_vbox), big_text, TRUE, TRUE, 0);
#endif
}

int display_months_appts(struct tm *date_in, GtkWidget **day_texts)
{
   AppointmentList *a_list;
   AppointmentList *temp_al;
   struct tm date;
   GtkWidget **texts;
#ifdef ENABLE_GTK2
   GObject   **text_buffers;
#endif
   char desc[100];
   char datef[20];
   char str[80];
   int dow;
   int ndim;
   int n;
   int mask;
   int num_shown;
#ifdef ENABLE_DATEBK
   int ret;
   int cat_bit;
   int db3_type;
   long use_db3_tags;
   struct db4_struct db4;
#endif
   GString *gstr;
   GtkWidget *temp_text;

   texts = &day_texts[glob_offset];
#ifdef ENABLE_GTK2
   text_buffers = &glob_month_text_buffers[glob_offset];
#endif

   a_list = NULL;
   mask=0;

   for (n=0; n<37; n++) {
#ifdef ENABLE_GTK2
      temp_text = glob_month_text_views[n];
#else
      temp_text = glob_month_texts[n];
#endif
      gstr = gtk_object_get_data(GTK_OBJECT(temp_text), "gstr");
      if (gstr) {
	 g_string_free(gstr, TRUE);
	 gtk_object_remove_data(GTK_OBJECT(temp_text), "gstr");
      }
   }

   /* Set Month name label */
   jp_strftime(str, sizeof(str), "%B %Y", date_in);
   gtk_label_set_text(GTK_LABEL(glob_month_month_label), str);

   memcpy(&date, date_in, sizeof(struct tm));

   /* Get all of the appointments */
   get_days_appointments2(&a_list, NULL, 2, 2, 2, NULL);

   get_month_info(date.tm_mon, 1, date.tm_year, &dow, &ndim);

   weed_datebook_list(&a_list, date.tm_mon, date.tm_year, 0, &mask);

   for (n=0, date.tm_mday=1; date.tm_mday<=ndim; date.tm_mday++, n++) {
      gstr=NULL;

      date.tm_sec=0;
      date.tm_min=0;
      date.tm_hour=11;
      date.tm_isdst=-1;
      date.tm_wday=0;
      date.tm_yday=1;
      mktime(&date);

#ifdef ENABLE_GTK2
      gtk_text_buffer_set_text(GTK_TEXT_BUFFER(text_buffers[n]), "", -1);
#else
      gtk_text_set_point(GTK_TEXT(texts[n]),
			 gtk_text_get_length(GTK_TEXT(texts[n])));
      gtk_text_backward_delete(GTK_TEXT(texts[n]),
			       gtk_text_get_length(GTK_TEXT(texts[n])));
#endif

      num_shown = 0;
      for (temp_al = a_list; temp_al; temp_al=temp_al->next) {
#ifdef ENABLE_DATEBK
	 get_pref(PREF_USE_DB3, &use_db3_tags, NULL);
	 if (use_db3_tags) {
	    ret = db3_parse_tag(temp_al->mappt.appt.note, &db3_type, &db4);
	    jp_logf(JP_LOG_DEBUG, "category = 0x%x\n", db4.category);
	    cat_bit=1<<db4.category;
	    if (!(cat_bit & datebook_category)) {
	       jp_logf(JP_LOG_DEBUG, "skipping rec not in this category\n");
	       continue;
	    }
	 }
#endif
	 if (isApptOnDate(&(temp_al->mappt.appt), &date)) {
	    if (num_shown) {
#ifdef ENABLE_GTK2
	       gtk_text_buffer_insert_at_cursor(GTK_TEXT_BUFFER(text_buffers[n]), "\n", -1);
#else
	       gtk_text_insert(GTK_TEXT(texts[n]), NULL, NULL, NULL, "\n", -1);
#endif
	       g_string_append(gstr, "\n");
	    } else {
	       gstr=g_string_new("");
	    }
	    num_shown++;
	    if (temp_al->mappt.appt.event) {
	       strcpy(desc, "*");
	    } else {
	       get_pref_time_no_secs(datef);
	       jp_strftime(desc, sizeof(desc), datef, &(temp_al->mappt.appt.begin));
	       strcat(desc, " ");
	    }
	    g_string_append(gstr, desc);
	    g_string_append(gstr, temp_al->mappt.appt.description);
	    if (temp_al->mappt.appt.description) {
	       strncat(desc, temp_al->mappt.appt.description, 36);
	       desc[35]='\0';
	    }
	    remove_cr_lfs(desc);
#ifdef ENABLE_GTK2
	    gtk_text_buffer_insert_at_cursor(GTK_TEXT_BUFFER(text_buffers[n]), desc, -1);
#else
	    gtk_text_insert(GTK_TEXT(texts[n]), NULL, NULL, NULL, desc, -1);
#endif
	 }
      }
      gtk_object_set_data(GTK_OBJECT(texts[n]), "gstr", gstr);
   }
   free_AppointmentList(&a_list);

   return 0;
}

void monthview_gui(struct tm *date_in)
{
   struct tm date;
   GtkWidget *label;
   GtkWidget *button;
   GtkWidget *arrow;
   GtkWidget *align;
   GtkWidget *vbox;
   GtkWidget *hbox;
   GtkWidget *hbox_temp;
   int i;
   char str[256];
   char str_dow[256];
   long fdow;
   const char *str_fdow;
   char title[200];
   unsigned long w, h;

   if (window) {
      return;
   }

   memcpy(&glob_month_date, date_in, sizeof(struct tm));

   get_pref(PREF_FDOW, &fdow, &str_fdow);

   get_pref(PREF_MONTHVIEW_WIDTH, &w, NULL);
   get_pref(PREF_MONTHVIEW_HEIGHT, &h, NULL);

   g_snprintf(title, sizeof(title), "%s %s", PN, _("Monthly View"));

   window = gtk_widget_new(GTK_TYPE_WINDOW,
			   "type", GTK_WINDOW_TOPLEVEL,
			   "title", title,
			   NULL);

   gtk_window_set_default_size(GTK_WINDOW(window), w, h);

   gtk_container_set_border_width(GTK_CONTAINER(window), 10);

   gtk_signal_connect(GTK_OBJECT(window), "destroy",
                      GTK_SIGNAL_FUNC(cb_destroy), window);

   vbox = gtk_vbox_new(FALSE, 0);
   gtk_container_add(GTK_CONTAINER(window), vbox);

   /* This box has the close button and arrows in it */
   align = gtk_alignment_new(0.5, 0.5, 0, 0);
   gtk_box_pack_start(GTK_BOX(vbox), align, FALSE, FALSE, 0);

   hbox_temp = gtk_hbox_new(FALSE, 0);

   gtk_container_add(GTK_CONTAINER(align), hbox_temp);

   /*Make a left arrow for going back a week */
   button = gtk_button_new();
   arrow = gtk_arrow_new(GTK_ARROW_LEFT, GTK_SHADOW_OUT);
   gtk_container_add(GTK_CONTAINER(button), arrow);
   gtk_signal_connect(GTK_OBJECT(button), "clicked", 
		      GTK_SIGNAL_FUNC(cb_month_move),
		      GINT_TO_POINTER(-1));
   gtk_box_pack_start(GTK_BOX(hbox_temp), button, FALSE, FALSE, 3);


   /* Create a "Quit" button */
   button = gtk_button_new_with_label(_("Close"));
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_quit), window);
   gtk_box_pack_start(GTK_BOX(hbox_temp), button, FALSE, FALSE, 0);

   /* Create a "Print" button */
   button = gtk_button_new_with_label(_("Print"));
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_month_print), window);
   gtk_box_pack_start(GTK_BOX(hbox_temp), button, FALSE, FALSE, 0);

   /*Make a right arrow for going forward a week */
   button = gtk_button_new();
   arrow = gtk_arrow_new(GTK_ARROW_RIGHT, GTK_SHADOW_OUT);
   gtk_container_add(GTK_CONTAINER(button), arrow);
   gtk_signal_connect(GTK_OBJECT(button), "clicked", 
		      GTK_SIGNAL_FUNC(cb_month_move),
		      GINT_TO_POINTER(1));
   gtk_box_pack_start(GTK_BOX(hbox_temp), button, FALSE, FALSE, 3);


   /* Month name label */
   jp_strftime(str, sizeof(str), "%B %Y", &glob_month_date);
   glob_month_month_label = gtk_label_new(str);
   gtk_box_pack_start(GTK_BOX(vbox), glob_month_month_label, FALSE, FALSE, 0);

   /* We know this is on a Sunday */
   memset(&date, 0, sizeof(date));
   date.tm_hour=12;
   date.tm_mday=3;
   date.tm_mon=1;
   date.tm_year=80;
   mktime(&date);
   /* Get to the first day of week */
   if (fdow) add_days_to_date(&date, fdow);

   /* Days of the week */
   hbox = gtk_hbox_new(TRUE, 0);
   gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
   for (i=0; i<7; i++) {
      jp_strftime(str_dow, sizeof(str_dow), "%A", &date);
      label = gtk_label_new(str_dow);
      gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
      add_days_to_date(&date, 1);
   }

   /* glob_month_vbox */
   glob_month_vbox = vbox;

   create_month_boxes_texts(glob_month_vbox);

   gtk_widget_show_all(window);

   hide_show_month_boxes();

#ifdef ENABLE_GTK2
   display_months_appts(&glob_month_date, glob_month_text_views);
#else
   display_months_appts(&glob_month_date, glob_month_texts);
#endif
}

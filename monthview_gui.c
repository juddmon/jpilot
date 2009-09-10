/* $Id: monthview_gui.c,v 1.49 2009/09/10 06:01:54 rikster5 Exp $ */

/*******************************************************************************
 * monthview_gui.c
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
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>

#include "jp-pi-calendar.h"

#include "utils.h"
#include "i18n.h"
#include "prefs.h"
#include "log.h"
#include "datebook.h"
#include "print.h"

/******************************* Global vars **********************************/
extern int datebk_category;
extern int glob_app;

GtkWidget *monthview_window=NULL;
static GtkWidget *month_day_label[37];
static GtkWidget *month_day[37];
static GObject   *month_day_buffer[37];
static GObject   *all_appts_buffer;
static GtkWidget *month_month_label;
static GtkWidget *glob_last_hbox_row;
static int glob_offset;
static struct tm glob_month_date;

/****************************** Prototypes ************************************/
int display_months_appts(struct tm *glob_month_date, GtkWidget **glob_month_texts);
void hide_show_month_boxes();

/****************************** Main Code *************************************/
static gboolean cb_destroy(GtkWidget *widget)
{
   int n;
   GString *gstr;
   GtkWidget *text;

   monthview_window = NULL;

   for (n=0; n<37; n++) {
      text = month_day[n];
      gstr = gtk_object_get_data(GTK_OBJECT(text), "gstr");
      if (gstr) {
	 g_string_free(gstr, TRUE);
	 gtk_object_remove_data(GTK_OBJECT(text), "gstr");
      }
   }
   return FALSE;
}

void cb_monthview_quit(GtkWidget *widget, gpointer data)
{
   int w, h;

   gdk_window_get_size(monthview_window->window, &w, &h);
   set_pref(PREF_MONTHVIEW_WIDTH, w, NULL, FALSE);
   set_pref(PREF_MONTHVIEW_HEIGHT, h, NULL, FALSE);

   gtk_widget_destroy(monthview_window);
}

static void cb_month_move(GtkWidget *widget, gpointer data)
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
   display_months_appts(&glob_month_date, month_day);
}

static void cb_month_print(GtkWidget *widget, gpointer data)
{
   long paper_size;

   jp_logf(JP_LOG_DEBUG, "cb_month_print called\n");
   if (print_gui(monthview_window, DATEBOOK, 3, 0x04) == DIALOG_SAID_PRINT) {
      get_pref(PREF_PAPER_SIZE, &paper_size, NULL);
      if (paper_size==1) {
	 print_months_appts(&glob_month_date, PAPER_A4);
      } else {
	 print_months_appts(&glob_month_date, PAPER_Letter);
      }
   }
}

static void cb_enter_notify(GtkWidget *widget, GdkEvent *event, gpointer data)
{
   static int prev_day=-1;
   GtkWidget *textview;
   GString *gstr;
   
   if (prev_day==GPOINTER_TO_INT(data)+1-glob_offset) {
      return;
   }
   prev_day = GPOINTER_TO_INT(data)+1-glob_offset;

   textview = gtk_bin_get_child(GTK_BIN(widget));
   gstr = gtk_object_get_data(GTK_OBJECT(textview), "gstr");
   if (gstr) {
      gtk_text_buffer_set_text(GTK_TEXT_BUFFER(all_appts_buffer), gstr->str, -1);
   } else {
      gtk_text_buffer_set_text(GTK_TEXT_BUFFER(all_appts_buffer), "", -1);
   }
}

/* Called when a day is clicked on in the month view */
static void cb_enter_selected_day(GtkWidget *widget, 
                                  GdkEvent  *event, 
				  gpointer   data)
{
   int day = GPOINTER_TO_INT(data) + 1 - glob_offset;

   if (glob_app != DATEBOOK)
      return;

   /* Redisplay the day view based on the date the user clicked on */
   datebook_gui_setdate(glob_month_date.tm_year, glob_month_date.tm_mon, day);
}

/*
 * Hide, or show month boxes (days) according to the month.
 * Also, set a global offset for indexing day 1.
 * Also relabel day labels.
 */
void hide_show_month_boxes(void)
{
   int n;
   int dow, ndim;
   int now_today;
   long fdow;
   char str[40];
   int d;
   GtkWidget *text;
   char *markup_str;

   /* Determine today for highlighting */
   now_today = get_highlighted_today(&glob_month_date);

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
      text = month_day[n];
      g_snprintf(str, sizeof(str), "%d", d);

      if (d == now_today)
      {
	 markup_str = g_markup_printf_escaped("<b>%s</b>", str);
         gtk_widget_set_name(text, "today");
      } else {
	 markup_str = g_markup_printf_escaped("%s", str);
         gtk_widget_set_name(text, "");
      }
      gtk_label_set_markup(GTK_LABEL(month_day_label[n]), markup_str);
      g_free(markup_str);

      if (n<7) {
	 if (d>0) {
	    gtk_widget_show(GTK_WIDGET(text));
	    gtk_widget_show(GTK_WIDGET(month_day_label[n]));
	 } else {
	    gtk_widget_hide(GTK_WIDGET(text));
	    gtk_widget_hide(GTK_WIDGET(month_day_label[n]));
	 }
      }
      if (n>27) {
	 if (d<=ndim) {
	    gtk_widget_show(GTK_WIDGET(text));
	    gtk_widget_show(GTK_WIDGET(month_day_label[n]));
	 } else {
	    gtk_widget_hide(GTK_WIDGET(text));
	    gtk_widget_hide(GTK_WIDGET(month_day_label[n]));
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
   GtkWidget *event_box;
   char str[80];

   n=0;
   for (i=0; i<6; i++) {
      hbox_row = gtk_hbox_new(TRUE, 0);
      gtk_box_pack_start(GTK_BOX(month_vbox), hbox_row, TRUE, TRUE, 0);
      for (j=0; j<7; j++) {
	 vbox = gtk_vbox_new(FALSE, 0);
	 gtk_box_pack_start(GTK_BOX(hbox_row), vbox, TRUE, TRUE, 2);
	 n=i*7+j;
	 if (n<37) {
	    sprintf(str, "%d", n + 1);
	    /* Day of month labels */
	    month_day_label[n] = gtk_label_new(str);
	    gtk_misc_set_alignment(GTK_MISC(month_day_label[n]), 0.0, 0.5);
	    gtk_box_pack_start(GTK_BOX(vbox), month_day_label[n], FALSE, FALSE, 0);

	    /* text variable only used to save some typing */
	    text = month_day[n] = gtk_text_view_new();
	    month_day_buffer[n] =
	      G_OBJECT(gtk_text_view_get_buffer(GTK_TEXT_VIEW(text)));
	    gtk_widget_set_usize(GTK_WIDGET(text), 10, 10);
	    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(text), FALSE);
	    gtk_text_view_set_editable(GTK_TEXT_VIEW(text), FALSE);
	    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text), GTK_WRAP_WORD);

            /* textview widget does not support window events such as 
             * enter_notify.  The widget must be wrapped in an event box 
             * in order to work correctly. */
            event_box = gtk_event_box_new();
            gtk_container_add(GTK_CONTAINER(event_box), text);

	    gtk_signal_connect(GTK_OBJECT(event_box), "enter_notify_event",
			       GTK_SIGNAL_FUNC(cb_enter_notify), GINT_TO_POINTER(n));
	    gtk_signal_connect(GTK_OBJECT(text), "button_release_event",
			       GTK_SIGNAL_FUNC(cb_enter_selected_day),
			       GINT_TO_POINTER(n));

	    gtk_box_pack_start(GTK_BOX(vbox), event_box, TRUE, TRUE, 0);

	 }
      }
   }
   glob_last_hbox_row = hbox_row;

   text = gtk_text_view_new();
   all_appts_buffer = G_OBJECT(gtk_text_view_get_buffer(GTK_TEXT_VIEW(text)));
   gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(text), FALSE);
   gtk_text_view_set_editable(GTK_TEXT_VIEW(text), FALSE);
   gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text), GTK_WRAP_WORD);
   gtk_widget_set_usize(GTK_WIDGET(text), 10, 10);
   gtk_box_pack_start(GTK_BOX(month_vbox), text, TRUE, TRUE, 4);
}

int display_months_appts(struct tm *date_in, GtkWidget **day_texts)
{
   AppointmentList *a_list;
   AppointmentList *temp_al;
   struct tm date;
   GtkWidget **texts;
   GObject   **text_buffers;
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
   text_buffers = &month_day_buffer[glob_offset];

   a_list = NULL;
   mask=0;

   for (n=0; n<37; n++) {
      temp_text = month_day[n];
      gstr = gtk_object_get_data(GTK_OBJECT(temp_text), "gstr");
      if (gstr) {
	 g_string_free(gstr, TRUE);
	 gtk_object_remove_data(GTK_OBJECT(temp_text), "gstr");
      }
   }

   /* Set Month name label */
   jp_strftime(str, sizeof(str), "%B %Y", date_in);
   gtk_label_set_text(GTK_LABEL(month_month_label), str);

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

      gtk_text_buffer_set_text(GTK_TEXT_BUFFER(text_buffers[n]), "", -1);

      num_shown = 0;
      for (temp_al = a_list; temp_al; temp_al=temp_al->next) {
#ifdef ENABLE_DATEBK
	 get_pref(PREF_USE_DB3, &use_db3_tags, NULL);
	 if (use_db3_tags) {
	    ret = db3_parse_tag(temp_al->mappt.appt.note, &db3_type, &db4);
	    jp_logf(JP_LOG_DEBUG, "category = 0x%x\n", db4.category);
	    cat_bit=1<<db4.category;
	    if (!(cat_bit & datebk_category)) {
	       jp_logf(JP_LOG_DEBUG, "skipping rec not in this category\n");
	       continue;
	    }
	 }
#endif
	 if (isApptOnDate(&(temp_al->mappt.appt), &date)) {
	    if (num_shown) {
	       gtk_text_buffer_insert_at_cursor(GTK_TEXT_BUFFER(text_buffers[n]), "\n", -1);
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

	    /* Append number of anniversary years if enabled & appropriate */
	    append_anni_years(desc, 35, &date, &temp_al->mappt.appt);

	    gtk_text_buffer_insert_at_cursor(GTK_TEXT_BUFFER(text_buffers[n]), desc, -1);
	 }
      }
      gtk_object_set_data(GTK_OBJECT(texts[n]), "gstr", gstr);
   }
   free_AppointmentList(&a_list);

   return EXIT_SUCCESS;
}

void monthview_gui(struct tm *date_in)
{
   struct tm date;
   GtkWidget *label;
   GtkWidget *button;
   GtkWidget *align;
   GtkWidget *vbox;
   GtkWidget *hbox;
   GtkWidget *hbox_temp;
   int i;
   char str[256];
   char str_dow[256];
   long fdow;
   char title[200];
   long w, h;

   if (monthview_window) {
       /* Delete any existing window to ensure that new window is biased
	* around currently selected date and so that the new window
	* contents are updated with any changes on the day view.
	*/
       gtk_widget_destroy(monthview_window);
   }

   memcpy(&glob_month_date, date_in, sizeof(struct tm));

   get_pref(PREF_FDOW, &fdow, NULL);

   get_pref(PREF_MONTHVIEW_WIDTH, &w, NULL);
   get_pref(PREF_MONTHVIEW_HEIGHT, &h, NULL);

   g_snprintf(title, sizeof(title), "%s %s", PN, _("Monthly View"));

   monthview_window = gtk_widget_new(GTK_TYPE_WINDOW,
	       		                      "type", GTK_WINDOW_TOPLEVEL,
			                            "title", title,
			                            NULL);

   gtk_window_set_default_size(GTK_WINDOW(monthview_window), w, h);

   gtk_container_set_border_width(GTK_CONTAINER(monthview_window), 10);

   gtk_signal_connect(GTK_OBJECT(monthview_window), "destroy",
                      GTK_SIGNAL_FUNC(cb_destroy), monthview_window);

   vbox = gtk_vbox_new(FALSE, 0);
   gtk_container_add(GTK_CONTAINER(monthview_window), vbox);

   /* This box has the close button and arrows in it */
   align = gtk_alignment_new(0.5, 0.5, 0, 0);
   gtk_box_pack_start(GTK_BOX(vbox), align, FALSE, FALSE, 0);

   hbox_temp = gtk_hbutton_box_new();
   gtk_button_box_set_spacing(GTK_BUTTON_BOX(hbox_temp), 6);
   gtk_container_set_border_width(GTK_CONTAINER(hbox_temp), 6);

   gtk_container_add(GTK_CONTAINER(align), hbox_temp);

   /* Make a left arrow for going back a week */
   button = gtk_button_new_from_stock(GTK_STOCK_GO_BACK);
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_month_move),
		      GINT_TO_POINTER(-1));
   gtk_box_pack_start(GTK_BOX(hbox_temp), button, FALSE, FALSE, 3);

   /* Close button */
   button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_monthview_quit), monthview_window);
   /* Closing the window via a delete event uses the same cleanup routine */
   gtk_signal_connect(GTK_OBJECT(monthview_window), "delete_event",
		      GTK_SIGNAL_FUNC(cb_monthview_quit), NULL);
   gtk_box_pack_start(GTK_BOX(hbox_temp), button, FALSE, FALSE, 0);

   /* Print button */
   button = gtk_button_new_from_stock(GTK_STOCK_PRINT);
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_month_print), monthview_window);
   gtk_box_pack_start(GTK_BOX(hbox_temp), button, FALSE, FALSE, 0);

   /* Make a right arrow for going forward a week */
   button = gtk_button_new_from_stock(GTK_STOCK_GO_FORWARD);
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_month_move),
		      GINT_TO_POINTER(1));
   gtk_box_pack_start(GTK_BOX(hbox_temp), button, FALSE, FALSE, 3);

   /* Month name label */
   jp_strftime(str, sizeof(str), "%B %Y", &glob_month_date);
   month_month_label = gtk_label_new(str);
   gtk_box_pack_start(GTK_BOX(vbox), month_month_label, FALSE, FALSE, 0);

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

   create_month_boxes_texts(vbox);

   gtk_widget_show_all(monthview_window);

   hide_show_month_boxes();

   display_months_appts(&glob_month_date, month_day);
}


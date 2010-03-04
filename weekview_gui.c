/* $Id: weekview_gui.c,v 1.51 2010/03/04 17:48:25 rousseau Exp $ */

/*******************************************************************************
 * weekview_gui.c
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

#include "pi-calendar.h"

#include "i18n.h"
#include "utils.h"
#include "prefs.h"
#include "log.h"
#include "datebook.h"
#include "calendar.h"
#include "print.h"
#include "jpilot.h"

/******************************* Global vars **********************************/
extern int datebk_category;
extern int glob_app;

GtkWidget *weekview_window=NULL;
static GtkWidget *week_day_label[8];
static GtkWidget *week_day_text[8];
static GObject   *week_day_text_buffer[8];
static struct tm glob_week_date;

/****************************** Prototypes ************************************/
int clear_weeks_appts(GtkWidget **day_texts);
int display_weeks_appts(struct tm *date_in, GtkWidget **day_texts);

/****************************** Main Code *************************************/
static gboolean cb_destroy(GtkWidget *widget)
{
   weekview_window = NULL;
   return FALSE;
}

void cb_weekview_quit(GtkWidget *widget, gpointer data)
{
   int w, h;

   gdk_window_get_size(weekview_window->window, &w, &h);
   set_pref(PREF_WEEKVIEW_WIDTH, w, NULL, FALSE);
   set_pref(PREF_WEEKVIEW_HEIGHT, h, NULL, FALSE);

   gtk_widget_destroy(weekview_window);
}

/* This is where week printing is kicked off from */
static void cb_week_print(GtkWidget *widget, gpointer data)
{
   long paper_size;

   jp_logf(JP_LOG_DEBUG, "cb_week_print called\n");
   if (print_gui(weekview_window, DATEBOOK, 2, 0x02) == DIALOG_SAID_PRINT) {
      get_pref(PREF_PAPER_SIZE, &paper_size, NULL);
      if (paper_size==1) {
	 print_weeks_appts(&glob_week_date, PAPER_A4);
      } else {
	 print_weeks_appts(&glob_week_date, PAPER_Letter);
      }
   }
}

static void freeze_weeks_appts(void)
{
   int i;

   for (i=0; i<8; i++) {
      gtk_widget_freeze_child_notify(week_day_text[i]);
   }
}

static void thaw_weeks_appts(void)
{
   int i;

   for (i=0; i<8; i++) {
      gtk_widget_thaw_child_notify(week_day_text[i]);
   }
}

static void cb_week_move(GtkWidget *widget, gpointer data)
{
   if (GPOINTER_TO_INT(data)==-1) {
      sub_days_from_date(&glob_week_date, 7);
   }
   if (GPOINTER_TO_INT(data)==1) {
      add_days_to_date(&glob_week_date, 7);
   }

   freeze_weeks_appts();

   clear_weeks_appts(week_day_text);
   display_weeks_appts(&glob_week_date, week_day_text);

   thaw_weeks_appts();
}

int clear_weeks_appts(GtkWidget **day_texts)
{
   int i;
   GObject   *text_buffer;

   for (i=0; i<8; i++) {
      text_buffer = G_OBJECT(gtk_text_view_get_buffer(GTK_TEXT_VIEW(day_texts[i])));
      gtk_text_buffer_set_text(GTK_TEXT_BUFFER(text_buffer), "", -1);
   }
   return EXIT_SUCCESS;
}

/*
 * This function requires that date_in be the date of the first day of
 * the week (be it a Sunday, or a Monday).
 * It will then print the next eight days to the day_texts array of
 * text boxes.
 */
int display_weeks_appts(struct tm *date_in, GtkWidget **day_texts)
{
   CalendarEventList *ce_list;
   CalendarEventList *temp_cel;
   struct tm date;
   GtkWidget **text;
   char desc[256];
   char datef[20];
   int n, i;
   const char *svalue;
   char str[82];
   char str_dow[32];
   char short_date[32];
   char default_date[]="%x";
   int now_today;
#ifdef ENABLE_DATEBK
   int ret;
   int cat_bit;
   int db3_type;
   long use_db3_tags;
   struct db4_struct db4;
#endif
   GObject *text_buffer;
   char    *markup_str;

   ce_list = NULL;
   text = day_texts;

   memcpy(&date, date_in, sizeof(struct tm));

   get_pref(PREF_SHORTDATE, NULL, &svalue);
   if (svalue==NULL) {
      svalue = default_date;
   }

   /* Label each day including special markup for TODAY */
   for (i=0; i<8; i++, add_days_to_date(&date, 1)) {
      strftime(short_date, sizeof(short_date), svalue, &date);
      jp_strftime(str_dow, sizeof(str_dow), "%A", &date);

      /* Determine today for highlighting */
      now_today = get_highlighted_today(&date);

      g_snprintf(str, sizeof(str), "%s %s", str_dow, short_date);

      if (date.tm_mday == now_today)
      {
	 markup_str = g_markup_printf_escaped("<b>%s</b>", str);
         gtk_widget_set_name(GTK_WIDGET(text[i]), "today");
      } else {
	 markup_str = g_markup_printf_escaped("%s", str);
         gtk_widget_set_name(GTK_WIDGET(text[i]), "");
      }
      gtk_label_set_markup(GTK_LABEL(week_day_label[i]), markup_str);
      g_free(markup_str);

   }

   /* Get all of the appointments */
   get_days_calendar_events2(&ce_list, NULL, 2, 2, 2, CATEGORY_ALL, NULL);

   memcpy(&date, date_in, sizeof(struct tm));

   /* Iterate through 8 days */
   for (n=0; n<8; n++, add_days_to_date(&date, 1)) {
      text_buffer = G_OBJECT(gtk_text_view_get_buffer(GTK_TEXT_VIEW(text[n])));
      for (temp_cel = ce_list; temp_cel; temp_cel=temp_cel->next) {
#ifdef ENABLE_DATEBK
	 get_pref(PREF_USE_DB3, &use_db3_tags, NULL);
	 if (use_db3_tags) {
	    ret = db3_parse_tag(temp_cel->mce.ce.note, &db3_type, &db4);
	    jp_logf(JP_LOG_DEBUG, "category = 0x%x\n", db4.category);
	    cat_bit=1<<db4.category;
	    if (!(cat_bit & datebk_category)) {
	       jp_logf(JP_LOG_DEBUG, "skipping rec not in this category\n");
	       continue;
	    }
	 }
#endif
	 if (calendar_isApptOnDate(&(temp_cel->mce.ce), &date)) {
	    if (temp_cel->mce.ce.event) {
	       strcpy(desc, "*");
	    } else {
	       get_pref_time_no_secs(datef);
	       strftime(desc, sizeof(desc), datef, &(temp_cel->mce.ce.begin));
	       strcat(desc, " ");
	    }
	    if (temp_cel->mce.ce.description) {
	       strncat(desc, temp_cel->mce.ce.description, 70);
               /* FIXME: This kind of truncation is bad for UTF-8 */
	       desc[62]='\0';
	    }
            /* FIXME: Add location in parentheses (loc) as the Palm does.
             * We would need to check strlen, etc., before adding */
	    remove_cr_lfs(desc);

	    /* Append number of anniversary years if enabled & appropriate */
	    append_anni_years(desc, 62, &date, NULL, &temp_cel->mce.ce);

	    strcat(desc, "\n");
	    gtk_text_buffer_insert_at_cursor(GTK_TEXT_BUFFER(text_buffer),desc,-1);
	 }
      }
   }
   free_CalendarEventList(&ce_list);

   return EXIT_SUCCESS;
}

/* Called when a day is clicked on the week view */
static void cb_enter_selected_day(GtkWidget *widget, 
                                  GdkEvent  *event, 
				  gpointer   data)
{
   struct tm date;

   if (glob_app != DATEBOOK)
      return;

   date = glob_week_date;

   /* Calculate the date which the user has clicked on */
   add_days_to_date(&date, GPOINTER_TO_INT(data));

   /* Redisplay the day view based on this date */
   datebook_gui_setdate(date.tm_year, date.tm_mon, date.tm_mday);
}

void weekview_gui(struct tm *date_in)
{
   GtkWidget *button;
   GtkWidget *align;
   GtkWidget *vbox, *hbox;
   GtkWidget *hbox_temp;
   GtkWidget *vbox_left, *vbox_right;
   long fdow;
   int i;
   char title[200];
   long w, h;

   if (weekview_window) {
       /* Delete any existing window to ensure that new window is biased
	* around currently selected date and so that the new window
	* contents are updated with any changes on the day view. */
       gtk_widget_destroy(weekview_window);
   }

   memcpy(&glob_week_date, date_in, sizeof(struct tm));

   get_pref(PREF_WEEKVIEW_WIDTH, &w, NULL);
   get_pref(PREF_WEEKVIEW_HEIGHT, &h, NULL);

   g_snprintf(title, sizeof(title), "%s %s", PN, _("Weekly View"));
   weekview_window = gtk_widget_new(GTK_TYPE_WINDOW,
			            "type", GTK_WINDOW_TOPLEVEL,
			            "title", title,
			            NULL);

   gtk_window_set_default_size(GTK_WINDOW(weekview_window), w, h);
   gtk_container_set_border_width(GTK_CONTAINER(weekview_window), 10);

   gtk_signal_connect(GTK_OBJECT(weekview_window), "destroy",
                      GTK_SIGNAL_FUNC(cb_destroy), weekview_window);

   vbox = gtk_vbox_new(FALSE, 0);
   gtk_container_add(GTK_CONTAINER(weekview_window), vbox);

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
		      GTK_SIGNAL_FUNC(cb_week_move),
		      GINT_TO_POINTER(-1));
   gtk_box_pack_start(GTK_BOX(hbox_temp), button, FALSE, FALSE, 0);

   /* Close button */
   button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_weekview_quit), NULL);
   /* Closing the window via a delete event uses the same cleanup routine */
   gtk_signal_connect(GTK_OBJECT(weekview_window), "delete_event",
		      GTK_SIGNAL_FUNC(cb_weekview_quit), NULL);

   gtk_box_pack_start(GTK_BOX(hbox_temp), button, FALSE, FALSE, 0);

   /* Print button */
   button = gtk_button_new_from_stock(GTK_STOCK_PRINT);
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_week_print), weekview_window);
   gtk_box_pack_start(GTK_BOX(hbox_temp), button, FALSE, FALSE, 0);

   /* Make a right arrow for going forward a week */
   button = gtk_button_new_from_stock(GTK_STOCK_GO_FORWARD);
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_week_move),
		      GINT_TO_POINTER(1));
   gtk_box_pack_start(GTK_BOX(hbox_temp), button, FALSE, FALSE, 0);

   get_pref(PREF_FDOW, &fdow, NULL);

   hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

   vbox_left = gtk_vbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(hbox), vbox_left, TRUE, TRUE, 0);

   vbox_right = gtk_vbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(hbox), vbox_right, TRUE, TRUE, 0);

   /* Get the first day of the week */
   sub_days_from_date(&glob_week_date, (7 - fdow + glob_week_date.tm_wday)%7);

   /* Make 8 boxes, 1 for each day, to hold appt. descriptions */
   for (i=0; i<8; i++) {
      week_day_label[i] = gtk_label_new("");
      gtk_misc_set_alignment(GTK_MISC(week_day_label[i]), 0.0, 0.5);
      week_day_text[i] = gtk_text_view_new();
      week_day_text_buffer[i] = G_OBJECT(gtk_text_view_get_buffer(GTK_TEXT_VIEW(week_day_text[i])));
      gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(week_day_text[i]), FALSE);
      gtk_text_view_set_editable(GTK_TEXT_VIEW(week_day_text[i]), FALSE);
      gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(week_day_text[i]), GTK_WRAP_WORD);
      gtk_container_set_border_width(GTK_CONTAINER(week_day_text[i]), 1);
      gtk_text_buffer_create_tag(GTK_TEXT_BUFFER(week_day_text_buffer[i]),
				 "gray_background", "background", "gray",
			         NULL);
      gtk_widget_set_usize(GTK_WIDGET(week_day_text[i]), 10, 10);
      gtk_signal_connect(GTK_OBJECT(week_day_text[i]), "button_release_event",
			 GTK_SIGNAL_FUNC(cb_enter_selected_day),
			 GINT_TO_POINTER(i));
      if (i>3) {
	 gtk_box_pack_start(GTK_BOX(vbox_right), week_day_label[i], FALSE, FALSE, 0);
	 gtk_box_pack_start(GTK_BOX(vbox_right), week_day_text[i], TRUE, TRUE, 0);
      } else {
	 gtk_box_pack_start(GTK_BOX(vbox_left), week_day_label[i], FALSE, FALSE, 0);
	 gtk_box_pack_start(GTK_BOX(vbox_left), week_day_text[i], TRUE, TRUE, 0);
      }
   }

   display_weeks_appts(&glob_week_date, week_day_text);

   gtk_widget_show_all(weekview_window);
}


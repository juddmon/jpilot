/* $Id: weekview_gui.c,v 1.28 2005/01/27 22:15:17 rikster5 Exp $ */

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

#include "config.h"
#include "i18n.h"
#include <gtk/gtk.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "print.h"
#include "utils.h"
#include "prefs.h"
#include "log.h"
#include "datebook.h"
#include <pi-datebook.h>


extern int datebook_category;


static GtkWidget *window=NULL;
static GtkWidget *glob_dow_labels[8];
static GtkWidget *glob_week_texts[8];
#ifdef ENABLE_GTK2
static GObject   *glob_week_text_buffers[8];
#endif
static struct tm glob_week_date;

/* Function prototypes */
int clear_weeks_appts(GtkWidget **day_texts);
int display_weeks_appts(struct tm *date_in, GtkWidget **day_texts);


static gboolean cb_destroy(GtkWidget *widget)
{
   window = NULL;
   return FALSE;
}

static void cb_quit(GtkWidget *widget, gpointer data)
{
   int w, h;

   gdk_window_get_size(window->window, &w, &h);
   set_pref(PREF_WEEKVIEW_WIDTH, w, NULL, FALSE);
   set_pref(PREF_WEEKVIEW_HEIGHT, h, NULL, FALSE);

   gtk_widget_destroy(data);
}

/*----------------------------------------------------------------------
 * cb_week_print     This is where week printing is kicked off from
 *----------------------------------------------------------------------*/

static void cb_week_print(GtkWidget *widget, gpointer data)
{
   long paper_size;

   jp_logf(JP_LOG_DEBUG, "cb_week_print called\n");
   if (print_gui(window, DATEBOOK, 2, 0x02) == DIALOG_SAID_PRINT) {
      get_pref(PREF_PAPER_SIZE, &paper_size, NULL);
      if (paper_size==1) {
	 print_weeks_appts(&glob_week_date, PAPER_A4);
      } else {
	 print_weeks_appts(&glob_week_date, PAPER_Letter);
      }
   }
}

/*----------------------------------------------------------------------*/

void freeze_weeks_appts()
{
   int i;

   for (i=0; i<8; i++) {
#ifdef ENABLE_GTK2
      gtk_widget_freeze_child_notify(glob_week_texts[i]);
#else
      gtk_text_freeze(GTK_TEXT(glob_week_texts[i]));
#endif
   }
}

void thaw_weeks_appts()
{
   int i;

   for (i=0; i<8; i++) {
#ifdef ENABLE_GTK2
      gtk_widget_thaw_child_notify(glob_week_texts[i]);
#else
      gtk_text_thaw(GTK_TEXT(glob_week_texts[i]));
#endif
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

   clear_weeks_appts(glob_week_texts);
   display_weeks_appts(&glob_week_date, glob_week_texts);

   thaw_weeks_appts();
}

int clear_weeks_appts(GtkWidget **day_texts)
{
   int i;
#ifdef ENABLE_GTK2
   GObject   *text_buffer;
#endif

   for (i=0; i<8; i++) {
#ifdef ENABLE_GTK2
      text_buffer = G_OBJECT(gtk_text_view_get_buffer(GTK_TEXT_VIEW(day_texts[i])));
      gtk_text_buffer_set_text(GTK_TEXT_BUFFER(text_buffer), "", -1);
#else
      gtk_text_set_point(GTK_TEXT(day_texts[i]), 0);
      gtk_text_forward_delete(GTK_TEXT(day_texts[i]),
			      gtk_text_get_length(GTK_TEXT(day_texts[i])));
#endif
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
   AppointmentList *a_list;
   AppointmentList *temp_al;
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
#ifdef ENABLE_GTK2
   GObject   *text_buffer;
#endif

   /* Determine today for highlighting */
   now_today = get_highlighted_today(date_in);

   a_list = NULL;
   text = day_texts;

   memcpy(&date, date_in, sizeof(struct tm));

   get_pref(PREF_SHORTDATE, NULL, &svalue);
   if (svalue==NULL) {
      svalue = default_date;
   }

   for (i=0; i<8; i++, add_days_to_date(&date, 1)) {
      strftime(short_date, sizeof(short_date), svalue, &date);
      jp_strftime(str_dow, sizeof(str_dow), "%A", &date);
      g_snprintf(str, sizeof(str), "%s %s%s", str_dow, short_date,
		 date.tm_mday == now_today ? _(" (TODAY)") : "");
      gtk_label_set_text(GTK_LABEL(glob_dow_labels[i]), str);
   }

   /* Get all of the appointments */
   get_days_appointments2(&a_list, NULL, 2, 2, 2, NULL);

   /* iterate through eight days */
   memcpy(&date, date_in, sizeof(struct tm));


   for (n=0; n<8; n++, add_days_to_date(&date, 1)) {
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
	    if (temp_al->mappt.appt.event) {
	       strcpy(desc, "*");
	    } else {
	       get_pref_time_no_secs(datef);
	       strftime(desc, sizeof(desc), datef, &(temp_al->mappt.appt.begin));
	       strcat(desc, " ");
	    }
	    if (temp_al->mappt.appt.description) {
	       strncat(desc, temp_al->mappt.appt.description, 70);
	       desc[62]='\0';
	    }
	    remove_cr_lfs(desc);

	    /* Append number of anniversary years if enabled & appropriate */
	    append_anni_years(desc, 62, &date, &temp_al->mappt.appt);

	    strcat(desc, "\n");
#ifdef ENABLE_GTK2
	    text_buffer = G_OBJECT(gtk_text_view_get_buffer(GTK_TEXT_VIEW(text[n])));
	    gtk_text_buffer_insert_at_cursor(GTK_TEXT_BUFFER(text_buffer),desc,-1);
#else
	    gtk_text_insert(GTK_TEXT(text[n]), NULL, NULL, NULL, desc, -1);
#endif
	 }
      }
   }
   free_AppointmentList(&a_list);

   return EXIT_SUCCESS;
}

/* Called when a day is clicked on the week view */
static void cb_enter_selected_day(GtkWidget *widget, 
                                  GdkEvent  *event, 
				  gpointer   data)
{
   struct tm date;

   date = glob_week_date;

   /* Calculate the date which the user has clicked on */
   add_days_to_date(&date, GPOINTER_TO_INT(data));

   /* Redisplay the day view based on this date */
   datebook_gui_setdate(date.tm_year, date.tm_mon, date.tm_mday);
}

void weekview_gui(struct tm *date_in)
{
   GtkWidget *button;
   GtkWidget *arrow;
   GtkWidget *align;
   GtkWidget *vbox, *hbox;
   GtkWidget *hbox_temp;
   GtkWidget *vbox_left, *vbox_right;
   long fdow;
   int i;
   char title[200];
   long w, h;

   if (window) {
#ifdef ENABLE_GTK2
      /* Shift focus to existing window if called again
         and window is still alive. */
      gtk_window_present(GTK_WINDOW(window));
#else
      gdk_window_raise(window->window);
#endif
      return;
   }

   memcpy(&glob_week_date, date_in, sizeof(struct tm));

   get_pref(PREF_WEEKVIEW_WIDTH, &w, NULL);
   get_pref(PREF_WEEKVIEW_HEIGHT, &h, NULL);

   g_snprintf(title, sizeof(title), "%s %s", PN, _("Weekly View"));
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
		      GTK_SIGNAL_FUNC(cb_week_move),
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
		      GTK_SIGNAL_FUNC(cb_week_print), window);
   gtk_box_pack_start(GTK_BOX(hbox_temp), button, FALSE, FALSE, 0);

   /*Make a right arrow for going forward a week */
   button = gtk_button_new();
   arrow = gtk_arrow_new(GTK_ARROW_RIGHT, GTK_SHADOW_OUT);
   gtk_container_add(GTK_CONTAINER(button), arrow);
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_week_move),
		      GINT_TO_POINTER(1));
   gtk_box_pack_start(GTK_BOX(hbox_temp), button, FALSE, FALSE, 3);

   get_pref(PREF_FDOW, &fdow, NULL);

   hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

   vbox_left = gtk_vbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(hbox), vbox_left, TRUE, TRUE, 0);

   vbox_right = gtk_vbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(hbox), vbox_right, TRUE, TRUE, 0);

   /* Get the first day of the week */
   sub_days_from_date(&glob_week_date, (7 - fdow + glob_week_date.tm_wday)%7);

   for (i=0; i<8; i++) {
      glob_dow_labels[i] = gtk_label_new("");
      gtk_misc_set_alignment(GTK_MISC(glob_dow_labels[i]), 0.0, 0.5);
#ifdef ENABLE_GTK2
      glob_week_texts[i] = gtk_text_view_new();
      glob_week_text_buffers[i] = G_OBJECT(gtk_text_view_get_buffer(GTK_TEXT_VIEW(glob_week_texts[i])));
      gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(glob_week_texts[i]), FALSE);
      gtk_text_view_set_editable(GTK_TEXT_VIEW(glob_week_texts[i]), FALSE);
      gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(glob_week_texts[i]), GTK_WRAP_WORD);
      gtk_container_set_border_width(GTK_CONTAINER(glob_week_texts[i]), 1);
      gtk_text_buffer_create_tag(GTK_TEXT_BUFFER(glob_week_text_buffers[i]),
				 "gray_background", "background", "gray",
			         NULL);
#else
      glob_week_texts[i] = gtk_text_new(NULL, NULL);
#endif
      gtk_widget_set_usize(GTK_WIDGET(glob_week_texts[i]), 10, 10);
      gtk_signal_connect(GTK_OBJECT(glob_week_texts[i]), "button_press_event",
			 GTK_SIGNAL_FUNC(cb_enter_selected_day),
			 GINT_TO_POINTER(i));
      if (i>3) {
	 gtk_box_pack_start(GTK_BOX(vbox_right), glob_dow_labels[i], FALSE, FALSE, 0);
	 gtk_box_pack_start(GTK_BOX(vbox_right), glob_week_texts[i], TRUE, TRUE, 0);
      } else {
	 gtk_box_pack_start(GTK_BOX(vbox_left), glob_dow_labels[i], FALSE, FALSE, 0);
	 gtk_box_pack_start(GTK_BOX(vbox_left), glob_week_texts[i], TRUE, TRUE, 0);
      }
   }

   display_weeks_appts(&glob_week_date, glob_week_texts);

   gtk_widget_show_all(window);
}

/*******************************************************************************
 * weekview_gui.c
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
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "pi-calendar.h"

#include "i18n.h"
#include "utils.h"
#include "prefs.h"
#include "log.h"
#include "datebook.h"
#include "calendar.h"
#include "print.h"
#include "jpilot.h"
#include "libsqlite.h"

/******************************* Global vars **********************************/
extern int datebk_category;
extern int glob_app;

GtkWidget *weekview_window=NULL;
static GtkWidget *week_day_label[8];
static GtkWidget *week_day_text[8];
static GObject   *week_day_text_buffer[8];
static struct tm glob_week_date;

/****************************** Prototypes ************************************/
static int clear_weeks_appts(GtkWidget **day_texts);
static int display_weeks_appts(struct tm *date_in, GtkWidget **day_texts);

/****************************** Main Code *************************************/
static gboolean cb_destroy(GtkWidget *widget)
{
   weekview_window = NULL;
   return FALSE;
}

void cb_weekview_quit(GtkWidget *widget, gpointer data)
{
   int w, h;

   w = gdk_window_get_width(gtk_widget_get_window(weekview_window));
   h = gdk_window_get_height(gtk_widget_get_window(weekview_window));
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

static int clear_weeks_appts(GtkWidget **day_texts)
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
static int display_weeks_appts(struct tm *date_in, GtkWidget **day_texts)
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
   //int ret;
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
   if (glob_sqlite) jpsqlite_DatebookSEL(&ce_list,NULL,2);
   else get_days_calendar_events2(&ce_list, NULL, 2, 2, 2, CATEGORY_ALL, NULL);

   memcpy(&date, date_in, sizeof(struct tm));

   /* Iterate through 8 days */
   for (n=0; n<8; n++, add_days_to_date(&date, 1)) {
      text_buffer = G_OBJECT(gtk_text_view_get_buffer(GTK_TEXT_VIEW(text[n])));
      for (temp_cel = ce_list; temp_cel; temp_cel=temp_cel->next) {
#ifdef ENABLE_DATEBK
         get_pref(PREF_USE_DB3, &use_db3_tags, NULL);
         if (use_db3_tags) {
            //ret = db3_parse_tag(temp_cel->mcale.cale.note, &db3_type, &db4);
            db3_parse_tag(temp_cel->mcale.cale.note, &db3_type, &db4);
            jp_logf(JP_LOG_DEBUG, "category = 0x%x\n", db4.category);
            cat_bit=1<<db4.category;
            if (!(cat_bit & datebk_category)) {
               jp_logf(JP_LOG_DEBUG, "skipping rec not in this category\n");
               continue;
            }
         }
#endif
         if (calendar_isApptOnDate(&(temp_cel->mcale.cale), &date)) {
            if (temp_cel->mcale.cale.event) {
               strcpy(desc, "*");
            } else {
               get_pref_time_no_secs(datef);
               strftime(desc, sizeof(desc), datef, &(temp_cel->mcale.cale.begin));
               strcat(desc, " ");
            }
            if (temp_cel->mcale.cale.description) {
               strncat(desc, temp_cel->mcale.cale.description, 70);
               /* FIXME: This kind of truncation is bad for UTF-8 */
               desc[62]='\0';
            }
            /* FIXME: Add location in parentheses (loc) as the Palm does.
             * We would need to check strlen, etc., before adding */
            remove_cr_lfs(desc);

            /* Append number of anniversary years if enabled & appropriate */
            append_anni_years(desc, 62, &date, NULL, &temp_cel->mcale.cale);

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
   GtkWidget *grid;
   GtkAccelGroup *accel_group;
   long fdow;
   int i;
   char title[200];
   long w, h, show_tooltips;
   gint left, top;

   if (weekview_window) {
      /* Delete any existing window to ensure that new window is biased
       * around currently selected date and so that the new window
       * contents are updated with any changes on the day view. */
      gtk_widget_destroy(weekview_window);
   }

   memcpy(&glob_week_date, date_in, sizeof(struct tm));

   get_pref(PREF_WEEKVIEW_WIDTH, &w, NULL);
   get_pref(PREF_WEEKVIEW_HEIGHT, &h, NULL);
   get_pref(PREF_SHOW_TOOLTIPS, &show_tooltips, NULL);

   g_snprintf(title, sizeof(title), "%s %s", PN, _("Weekly View"));
   weekview_window = gtk_widget_new(GTK_TYPE_WINDOW,
                                    "type", GTK_WINDOW_TOPLEVEL,
                                    "title", title,
                                    NULL);

   gtk_window_set_default_size(GTK_WINDOW(weekview_window), w, h);
   gtk_container_set_border_width(GTK_CONTAINER(weekview_window), 10);

   g_signal_connect(G_OBJECT(weekview_window), "destroy",
                      G_CALLBACK(cb_destroy), weekview_window);

   // Use a grid instead of boxes
   grid = gtk_grid_new();
   gtk_grid_set_row_spacing(GTK_GRID(grid), 5);
   gtk_grid_set_column_spacing(GTK_GRID(grid), 5);
   gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);
   gtk_widget_set_halign(grid, GTK_ALIGN_FILL);
   gtk_widget_set_valign(grid, GTK_ALIGN_FILL);
   gtk_widget_set_hexpand(grid, TRUE);
   gtk_widget_set_vexpand(grid, TRUE);
   gtk_container_add(GTK_CONTAINER(weekview_window), grid);

   /* Make accelerators for some buttons window */
   accel_group = gtk_accel_group_new();
   gtk_window_add_accel_group(GTK_WINDOW(gtk_widget_get_toplevel(grid)), accel_group);

   /* Make a left arrow for going back a week */
   button = gtk_button_new_with_label("Back");
   g_signal_connect(G_OBJECT(button), "clicked",
                    G_CALLBACK(cb_week_move),
                    GINT_TO_POINTER(-1));
   gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(button), 0, 0, 1, 1);

   /* Accelerator key for left arrow */
   gtk_widget_add_accelerator(GTK_WIDGET(button), "clicked", accel_group, 
                              GDK_KEY_Left, GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);
   set_tooltip(show_tooltips,
               button, _("Last week   Alt+LeftArrow"));

   /* Close button */
   button = gtk_button_new_with_label("Close");
   g_signal_connect(G_OBJECT(button), "clicked",
                      G_CALLBACK(cb_weekview_quit), NULL);
   /* Closing the window via a delete event uses the same cleanup routine */
   g_signal_connect(G_OBJECT(weekview_window), "delete_event",
                      G_CALLBACK(cb_weekview_quit), NULL);
   gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(button), 1, 0, 1, 1);

   /* Print button */
   button = gtk_button_new_with_label("Print");
   g_signal_connect(G_OBJECT(button), "clicked",
                      G_CALLBACK(cb_week_print), weekview_window);
   gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(button), 2, 0, 1, 1);

   /* Make a right arrow for going forward a week */
   button = gtk_button_new_with_label("Forward");
   g_signal_connect(G_OBJECT(button), "clicked",
                      G_CALLBACK(cb_week_move),
                      GINT_TO_POINTER(1));
   gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(button), 3, 0, 1, 1);

   /* Accelerator key for right arrow */
   gtk_widget_add_accelerator(GTK_WIDGET(button), "clicked", accel_group,
                              GDK_KEY_Right, GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);
   set_tooltip(show_tooltips,
               button, _("Next week   Alt+RightArrow"));

   get_pref(PREF_FDOW, &fdow, NULL);

   /* Get the first day of the week */
   sub_days_from_date(&glob_week_date, (7 - fdow + glob_week_date.tm_wday)%7);

   /* Make 8 labels and entries, 1 for each day, to hold appt. descriptions */
   for (i=0; i<8; i++) {
      week_day_label[i] = gtk_label_new("");
      week_day_text[i] = gtk_text_view_new();
      week_day_text_buffer[i] = G_OBJECT(gtk_text_view_get_buffer(GTK_TEXT_VIEW(week_day_text[i])));
      gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(week_day_text[i]), FALSE);
      gtk_text_view_set_editable(GTK_TEXT_VIEW(week_day_text[i]), FALSE);
      gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(week_day_text[i]), GTK_WRAP_WORD);
      gtk_container_set_border_width(GTK_CONTAINER(week_day_text[i]), 1);
      gtk_text_buffer_create_tag(GTK_TEXT_BUFFER(week_day_text_buffer[i]),
                                 "gray_background", "background", "gray",
                                 NULL);
      g_signal_connect(G_OBJECT(week_day_text[i]), "button_release_event",
                         G_CALLBACK(cb_enter_selected_day),
                         GINT_TO_POINTER(i));
      if (i<4) {
         left = 0;
         top = i*2+1;
      } else {
         left = 2;
         top = (i-4)*2+1;
      }
      gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(week_day_label[i]), left, top, 2, 1);
      gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(week_day_text[i]), left, top+1, 2, 1);
      gtk_widget_set_halign(week_day_label[i], GTK_ALIGN_FILL);
      gtk_widget_set_halign(week_day_text[i], GTK_ALIGN_FILL);
      gtk_widget_set_hexpand(week_day_label[i], TRUE);
      gtk_widget_set_vexpand(week_day_text[i], TRUE);
   }

   display_weeks_appts(&glob_week_date, week_day_text);

   gtk_widget_show_all(weekview_window);
}


/*******************************************************************************
 * monthview_gui.c
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

#include <pi-calendar.h>

#include "utils.h"
#include "i18n.h"
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

GtkWidget *monthview_window=NULL;
static char      glob_str_dow[7][32];
static GObject   *all_appts_buffer;
static GtkWidget *month_month_label;
static int glob_offset;
static struct tm glob_month_date;

/*
 * The calendar display is 6 rows and 7 columns
 * and contains this data.
 */
typedef struct {
   GtkWidget *label;
   GtkWidget *tv;
   GObject   *buffer;
} c_day;

static c_day glob_days[42];

/****************************** Prototypes ************************************/
static int display_months_appts(struct tm *glob_month_date);
static void set_day_labels(void);

/****************************** Main Code *************************************/
static gboolean cb_destroy(GtkWidget *widget)
{
   int n;
   GString *gstr;
   GtkWidget *text;
   /* FIXME: Do we need to do free buffers?  If so do for all_appts_buffer; */

   monthview_window = NULL;

   for (n=0; n<4; n++) {
      text = glob_days[n].tv;
      gstr =  g_object_get_data(G_OBJECT(text), "gstr");
      if (gstr) {
         g_string_free(gstr, TRUE);
         g_object_set_data(G_OBJECT(text), "gstr", NULL);
      }
   }
   return FALSE;
}

void cb_monthview_quit(GtkWidget *widget, gpointer data)
{
   int w, h;

   w = gdk_window_get_width(gtk_widget_get_window(monthview_window));
   h = gdk_window_get_height(gtk_widget_get_window(monthview_window));
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
   set_day_labels();
   display_months_appts(&glob_month_date);
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
   gstr =  g_object_get_data(G_OBJECT(textview), "gstr");
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
 * Relabel shown day labels according to the month.
 * Also, set a global offset for indexing day 1.
 */
static void set_day_labels(void)
{
   int n;
   int dow, ndim;
   int now_today;
   long fdow;
   char str[64];
   int d;
   GtkWidget *text;
   char *markup_str;

   /* Determine today for highlighting */
   now_today = get_highlighted_today(&glob_month_date);

   get_month_info(glob_month_date.tm_mon, 1, glob_month_date.tm_year, &dow, &ndim);

   get_pref(PREF_FDOW, &fdow, NULL);

   glob_offset = (7+dow-fdow)%7;

   d = 1 - glob_offset;

   for (n=0; n<42; n++, d++) {
      text = glob_days[n].tv;
      /* Clear the calendar day entry */
      gtk_text_buffer_set_text(GTK_TEXT_BUFFER(glob_days[n].buffer), "", -1);
      if ((d<1) || (d>ndim)) {
         str[0]='\0';
         gtk_widget_set_sensitive(glob_days[n].tv, FALSE);
      } else {
         snprintf(str, sizeof(str), "%d - %s", d, glob_str_dow[n%7]);
         str[sizeof(str)-1]='\0';
         gtk_widget_set_sensitive(glob_days[n].tv, TRUE);
      }

      if (d == now_today)
      {
         /* Make today's text bold */
         markup_str = g_markup_printf_escaped("<b>%s</b>", str);
         gtk_widget_set_name(text, "today");
      } else {
         markup_str = g_markup_printf_escaped("%s", str);
         gtk_widget_set_name(text, "");
      }
      gtk_label_set_markup(GTK_LABEL(glob_days[n].label), markup_str);
      g_free(markup_str);
   }
}

static void create_month_boxes_texts(GtkWidget *grid)
{
    int x, y;
    int count;
    GtkWidget *vbox;
    GtkWidget *tv;
    GtkWidget *label;
    GtkWidget *event_box;

    count = 0;
    /* 6 Rows of 7 days for a month */
    for (y=0; y<6; y++) {
        for (x=0; x<7; x++, count++) {
            /* Day of month labels */
            /* label variable only used to save some typing */
            label = glob_days[count].label = gtk_label_new("");
            gtk_widget_set_halign(GTK_WIDGET(label), GTK_ALIGN_START);
            vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
            gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(vbox), x, y, 1, 1);
            gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

            /* tv variable only used to save some typing */
            tv = glob_days[count].tv = gtk_text_view_new();
            glob_days[count].buffer = G_OBJECT(gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv)));
            gtk_widget_set_halign(tv, GTK_ALIGN_FILL);
            gtk_widget_set_valign(tv, GTK_ALIGN_FILL);
            gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(tv), FALSE);
            gtk_text_view_set_editable(GTK_TEXT_VIEW(tv), FALSE);
            gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(tv), GTK_WRAP_CHAR);

            /* textview widget does not support window events such as
             * enter_notify.  The widget must be wrapped in an event box
             * in order to work correctly. */
            event_box = gtk_event_box_new();
            gtk_widget_set_halign(event_box, GTK_ALIGN_FILL);
            gtk_widget_set_valign(event_box, GTK_ALIGN_FILL);

            /* Use a scrolled window to keep the main window from expanding */
            GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
            gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

            gtk_container_add(GTK_CONTAINER(scrolled), tv);

            gtk_container_add(GTK_CONTAINER(event_box), scrolled);

            g_signal_connect(G_OBJECT(event_box), "enter_notify_event",
                             G_CALLBACK(cb_enter_notify), GINT_TO_POINTER(count));
            g_signal_connect(G_OBJECT(tv), "button_release_event",
                             G_CALLBACK(cb_enter_selected_day),
                             GINT_TO_POINTER(count));

            gtk_box_pack_start(GTK_BOX(vbox), event_box, TRUE, TRUE, 2);
        }
    }

    /* Draw the text viewer at the botton */
    tv = gtk_text_view_new();
    all_appts_buffer = G_OBJECT(gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv)));
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(tv), FALSE);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(tv), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(tv), GTK_WRAP_WORD);
    gtk_widget_set_size_request(GTK_WIDGET(tv), 10, 10);
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(tv), 0, y, 7, 1);
}

static int display_months_appts(struct tm *date_in)
{
   CalendarEventList *ce_list;
   CalendarEventList *temp_cel;
   struct tm date;
   char desc[100];
   char datef[20];
   char str[64];
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
   long datebook_version;

   ce_list = NULL;
   mask=0;

   get_pref(PREF_DATEBOOK_VERSION, &datebook_version, NULL);

   /* Clear all of the boxes */
   for (n=0; n<42; n++) {
      temp_text = glob_days[n].tv;
      gstr =  g_object_get_data(G_OBJECT(temp_text), "gstr");
      if (gstr) {
         g_string_free(gstr, TRUE);
         g_object_set_data(G_OBJECT(temp_text), "gstr", NULL);
      }
   }

   /* Set month name label */
   jp_strftime(str, sizeof(str), "%B %Y", date_in);
   gtk_label_set_text(GTK_LABEL(month_month_label), str);

   memcpy(&date, date_in, sizeof(struct tm));

   /* Get all of the appointments */
   if (glob_sqlite) jpsqlite_DatebookSEL(&ce_list,NULL,2);
   else get_days_calendar_events2(&ce_list, NULL, 2, 2, 2, CATEGORY_ALL, NULL);

   get_month_info(date.tm_mon, 1, date.tm_year, &dow, &ndim);

   weed_calendar_event_list(&ce_list, date.tm_mon, date.tm_year, 0, &mask);

   /* Cycle through every day in the selected month, starting at dow (day of week) for the first day */
   for (n=dow, date.tm_mday=1; date.tm_mday<=ndim; date.tm_mday++, n++) {
      gstr=NULL;

      date.tm_sec=0;
      date.tm_min=0;
      date.tm_hour=11;
      date.tm_isdst=-1;
      date.tm_wday=0;
      date.tm_yday=1;
      mktime(&date);

      /* Clear all of the text buffers */
      gtk_text_buffer_set_text(GTK_TEXT_BUFFER(glob_days[n].buffer), "", -1);

      num_shown = 0;
      for (temp_cel = ce_list; temp_cel; temp_cel=temp_cel->next) {
#ifdef ENABLE_DATEBK
         get_pref(PREF_USE_DB3, &use_db3_tags, NULL);
         if (use_db3_tags) {
            ret = db3_parse_tag(temp_cel->mcale.cale.note, &db3_type, &db4);
            jp_logf(JP_LOG_DEBUG, "category = 0x%x\n, ret=%d", db4.category, ret);
            cat_bit=1<<db4.category;
            if (!(cat_bit & datebk_category)) {
               jp_logf(JP_LOG_DEBUG, "skipping rec not in this category\n");
               continue;
            }
         }
#endif
         if (calendar_isApptOnDate(&(temp_cel->mcale.cale), &date)) {
            if (num_shown) {
               /* Set each text buffer */
               gtk_text_buffer_insert_at_cursor(GTK_TEXT_BUFFER(glob_days[n].buffer), "\n", -1);
               g_string_append(gstr, "\n");
            } else {
               gstr=g_string_new("");
            }
            num_shown++;
            if (temp_cel->mcale.cale.event) {
               strcpy(desc, "*");
            } else {
               get_pref_time_no_secs(datef);
               jp_strftime(desc, sizeof(desc), datef, &(temp_cel->mcale.cale.begin));
               strcat(desc, " ");
            }
            g_string_append(gstr, desc);
            g_string_append(gstr, temp_cel->mcale.cale.description);
            if (temp_cel->mcale.cale.description) {
               strncat(desc, temp_cel->mcale.cale.description, 36);
               /* FIXME: This kind of truncation is bad for UTF-8 */ 
               desc[35]='\0';
            }
            /* FIXME: Add location in parentheses (loc) as the Palm does.
             * We would need to check strlen, etc., before adding */
            remove_cr_lfs(desc);

            /* Append number of anniversary years if enabled & appropriate */
            append_anni_years(desc, 35, &date, NULL, &temp_cel->mcale.cale);

            gtk_text_buffer_insert_at_cursor(GTK_TEXT_BUFFER(glob_days[n].buffer), desc, -1);
         }
      }
      g_object_set_data(G_OBJECT(glob_days[n].tv), "gstr", gstr);
   }
   free_CalendarEventList(&ce_list);

   return EXIT_SUCCESS;
}

void monthview_gui(struct tm *date_in)
{
   struct tm date;
   GtkWidget *grid1;
   GtkWidget *grid2;
   GtkWidget *button;
   GtkWidget *vbox;
   GtkAccelGroup *accel_group;
   int i;
   char str[64];
   long fdow;
   char title[200];
   long w, h, show_tooltips;

   if (monthview_window) {
       /* Delete any existing window to ensure that new window is biased
        * around currently selected date and so that the new window
        * contents are updated with any changes on the day view. */
      gtk_widget_destroy(monthview_window);
   }

   memcpy(&glob_month_date, date_in, sizeof(struct tm));

   get_pref(PREF_FDOW, &fdow, NULL);

   get_pref(PREF_MONTHVIEW_WIDTH, &w, NULL);
   get_pref(PREF_MONTHVIEW_HEIGHT, &h, NULL);
   get_pref(PREF_SHOW_TOOLTIPS, &show_tooltips, NULL);

   g_snprintf(title, sizeof(title), "%s %s", PN, _("Monthly View"));

   monthview_window = gtk_widget_new(GTK_TYPE_WINDOW,
                                     "type", GTK_WINDOW_TOPLEVEL,
                                     "title", title,
                                     NULL);

   gtk_window_set_default_size(GTK_WINDOW(monthview_window), w, h);

   gtk_container_set_border_width(GTK_CONTAINER(monthview_window), 10);

   g_signal_connect(G_OBJECT(monthview_window), "destroy",
                    G_CALLBACK(cb_destroy), monthview_window);

   /* Use a grid instead of boxes */
   grid1 = gtk_grid_new();
   gtk_grid_set_row_spacing(GTK_GRID(grid1), 5);
   gtk_grid_set_column_spacing(GTK_GRID(grid1), 5);

   grid2 = gtk_grid_new();
   gtk_grid_set_row_spacing(GTK_GRID(grid2), 5);
   gtk_grid_set_column_spacing(GTK_GRID(grid2), 5);
   gtk_grid_set_row_homogeneous(GTK_GRID(grid2), TRUE);
   gtk_grid_set_column_homogeneous(GTK_GRID(grid2), TRUE);
   gtk_widget_set_halign(grid2, GTK_ALIGN_FILL);
   gtk_widget_set_valign(grid2, GTK_ALIGN_FILL);
   gtk_widget_set_hexpand(grid2, TRUE);
   gtk_widget_set_vexpand(grid2, TRUE);

   vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
   gtk_container_add(GTK_CONTAINER(monthview_window), vbox);

   gtk_box_pack_start(GTK_BOX(vbox), grid1, FALSE, FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox), grid2, TRUE, TRUE, 0);

   /* Make accelerators for some buttons window */
   accel_group = gtk_accel_group_new();
   gtk_window_add_accel_group(GTK_WINDOW(gtk_widget_get_toplevel(vbox)), accel_group);

   /* Make a left arrow for going back a week */
   button = gtk_button_new_with_label("Back");
   g_signal_connect(G_OBJECT(button), "clicked",
                      G_CALLBACK(cb_month_move),
                      GINT_TO_POINTER(-1));
   gtk_grid_attach(GTK_GRID(grid1), GTK_WIDGET(button), 0, 0, 1, 1);

   /* Accelerator key for left arrow */
   gtk_widget_add_accelerator(GTK_WIDGET(button), "clicked", accel_group, 
                              GDK_KEY_Left, GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);
   set_tooltip(show_tooltips, button, _("Last month   Alt+LeftArrow"));

   /* Close button */
   button = gtk_button_new_with_label("Close");
   g_signal_connect(G_OBJECT(button), "clicked",
                      G_CALLBACK(cb_monthview_quit), monthview_window);
   /* Closing the window via a delete event uses the same cleanup routine */
   g_signal_connect(G_OBJECT(monthview_window), "delete_event",
                      G_CALLBACK(cb_monthview_quit), NULL);
   gtk_grid_attach(GTK_GRID(grid1), GTK_WIDGET(button), 1, 0, 1, 1);

   /* Print button */
   button = gtk_button_new_with_label("Print");
   g_signal_connect(G_OBJECT(button), "clicked",
                      G_CALLBACK(cb_month_print), monthview_window);
   gtk_grid_attach(GTK_GRID(grid1), GTK_WIDGET(button), 2, 0, 1, 1);

   /* Make a right arrow for going forward a week */
   button = gtk_button_new_with_label("Forward");
   g_signal_connect(G_OBJECT(button), "clicked",
                      G_CALLBACK(cb_month_move),
                      GINT_TO_POINTER(1));
   gtk_grid_attach(GTK_GRID(grid1), GTK_WIDGET(button), 3, 0, 1, 1);

   /* Accelerator key for right arrow */
   gtk_widget_add_accelerator(GTK_WIDGET(button), "clicked", accel_group, 
                              GDK_KEY_Right, GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);
   set_tooltip(show_tooltips, button, _("Next month   Alt+RightArrow"));

   /* Month name label */
   jp_strftime(str, sizeof(str), "%B %Y", &glob_month_date);
   month_month_label = gtk_label_new(str);
   gtk_grid_attach(GTK_GRID(grid1), GTK_WIDGET(month_month_label), 0, 1, 7, 1);

   /* Use a date that we know is on a Sunday */
   memset(&date, 0, sizeof(date));
   date.tm_hour=12;
   date.tm_mday=3;
   date.tm_mon=1;
   date.tm_year=80;
   mktime(&date);
   /* Get to the first day of week.
    * 0 for Sunday, 1 for Monday, etc.
    * Move the date to the firs day of the week according to settings.
    */
   if (fdow) add_days_to_date(&date, fdow);

   /* Make strings for the days of the week, i.e. Sunday, Monday... */
   for (i=0; i<7; i++) {
      jp_strftime(glob_str_dow[i], sizeof(glob_str_dow[i]), "%A", &date);
      add_days_to_date(&date, 1);
   }

   /* Attach 6 rows of 7 boxes for the calendar text views.
    * Also one large text view at the bottom.
    */
   create_month_boxes_texts(grid2);

   gtk_widget_show_all(monthview_window);

   set_day_labels();

   display_months_appts(&glob_month_date);
}

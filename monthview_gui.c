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

/* gtk2 */
#define GTK_ENABLE_BROKEN

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
static GtkWidget *glob_hbox_month_row[6];
static GtkWidget *glob_month_texts[31];
static GtkWidget *glob_month_month_label;
static GtkWidget *big_text;
static struct tm glob_month_date;


int display_months_appts(struct tm *glob_month_date, GtkWidget **glob_month_texts);
void create_month_boxes();
void create_month_texts();
void destroy_month_boxes();


static gboolean cb_destroy(GtkWidget *widget)
{
   window = NULL;
   return FALSE;
}

static void
cb_quit(GtkWidget *widget, gpointer data)
{
   window = NULL;
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
   destroy_month_boxes();
   create_month_boxes();
   create_month_texts();
   display_months_appts(&glob_month_date, glob_month_texts);
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
   AppointmentList *a_list;
   AppointmentList *temp_al;
   struct tm date;
   char desc[400];
   char datef[20];
   static int prev_day=-1;
#ifdef ENABLE_DATEBK
   int ret;
   int cat_bit;
   long use_db3_tags;
   int db3_type;
   struct db4_struct db4;
#endif

   if (prev_day==GPOINTER_TO_INT(data)+1) {
      return;
   }
   prev_day = GPOINTER_TO_INT(data)+1;

   a_list = NULL;

   memcpy(&date, &glob_month_date, sizeof(struct tm));
   date.tm_mday=GPOINTER_TO_INT(data)+1;
   mktime(&date);

   /* Get all of the appointments */
   get_days_appointments2(&a_list, &date, 2, 2, 2);

   gtk_text_set_point(GTK_TEXT(big_text),
		      gtk_text_get_length(GTK_TEXT(big_text)));
   gtk_text_backward_delete(GTK_TEXT(big_text),
			    gtk_text_get_length(GTK_TEXT(big_text)));

   for (temp_al = a_list; temp_al; temp_al=temp_al->next) {
#ifdef ENABLE_DATEBK
      get_pref(PREF_USE_DB3, &use_db3_tags, NULL);
      if (use_db3_tags) {
	 ret = db3_parse_tag(temp_al->ma.a.note, &db3_type, &db4);
	 jp_logf(JP_LOG_DEBUG, "category = 0x%x\n", db4.category);
	 cat_bit=1<<db4.category;
	 if (!(cat_bit & datebook_category)) {
	    jp_logf(JP_LOG_DEBUG, "skipping rec not in this category\n");
	    continue;
	 }
      }
#endif
      if (temp_al->ma.a.event) {
	 desc[0]='\0';
      } else {
	 get_pref_time_no_secs(datef);
	 strftime(desc, 20, datef, &(temp_al->ma.a.begin));
      }
      strcat(desc, " ");
      if (temp_al->ma.a.description) {
	 strncat(desc, temp_al->ma.a.description, 300);
	 desc[300]='\0';
      }
      remove_cr_lfs(desc);
      strcat(desc, "\n");
      gtk_text_insert(GTK_TEXT(big_text), NULL, NULL, NULL, desc, -1);
      }
   free_AppointmentList(&a_list);
}

void create_month_boxes()
{
   int i;

   for (i=0; i<6; i++) {
      glob_hbox_month_row[i] = gtk_hbox_new(TRUE, 0);
      gtk_box_pack_start(GTK_BOX(glob_month_vbox), glob_hbox_month_row[i], TRUE, TRUE, 0);
   }
}

void destroy_month_boxes()
{
   int i;

   for (i=0; i<6; i++) {
      if (glob_hbox_month_row[i]) {
	 gtk_widget_destroy(glob_hbox_month_row[i]);
	 glob_hbox_month_row[i]=NULL;
      }
   }
}

void create_month_texts()
{
   int n;
   int dow, ndim, d;
   int row, column;
   long fdow;
   const char *str_fdow;
   char str[80];
   GtkWidget *hbox;
   GdkColor color;
   GdkColormap *colormap;

   /* Month name label */
   strftime(str, 60, "%B %Y", &glob_month_date);
   gtk_label_set_text(GTK_LABEL(glob_month_month_label), str);

   get_month_info(glob_month_date.tm_mon, 1, glob_month_date.tm_year, &dow, &ndim);

   get_pref(PREF_FDOW, &fdow, &str_fdow);

   d=dow;
   row=0;
   column=0;
   for (n=(7+dow-fdow)%7; n>0; n--) {
      hbox = gtk_hbox_new(TRUE, 0);
      gtk_widget_set_usize(GTK_WIDGET(hbox), 110, 90);
      gtk_box_pack_start(GTK_BOX(glob_hbox_month_row[0]), hbox, TRUE, TRUE, 0);
      column++;
   }

   for (n=0; n<ndim; n++) {
      glob_month_texts[n] = gtk_text_new(NULL, NULL);
      gtk_widget_set_usize(GTK_WIDGET(glob_month_texts[n]), 110, 90);
      gtk_text_set_word_wrap(GTK_TEXT(glob_month_texts[n]), FALSE);
      gtk_signal_connect(GTK_OBJECT(glob_month_texts[n]), "enter_notify_event",
			 GTK_SIGNAL_FUNC(cb_enter_notify), GINT_TO_POINTER(n));
      gtk_box_pack_start(GTK_BOX(glob_hbox_month_row[row]), glob_month_texts[n], TRUE, TRUE, 0);
      if (++column > 6) {
	 column=0;
	 row++;
      }
   }

   color.red = 0xAAAA;
   color.green = 0xAAAA;
   color.blue = 0xAAAA;

   colormap = gtk_widget_get_colormap(glob_month_texts[0]);
   gdk_color_alloc(colormap, &color);

   for (n=0; n<ndim; n++) {
      sprintf(str, "%d\n", n + 1);
      gtk_text_insert(GTK_TEXT(glob_month_texts[n]), NULL, NULL, &color, str, -1);
   }

   for (n=column; n<7; n++) {
      if (column==0) break;
      hbox = gtk_hbox_new(TRUE, 0);
      gtk_widget_set_usize(GTK_WIDGET(hbox), 110, 90);
      gtk_box_pack_start(GTK_BOX(glob_hbox_month_row[row]), hbox, TRUE, TRUE, 0);
   }
}

int display_months_appts(struct tm *date_in, GtkWidget **day_texts)
{
   AppointmentList *a_list;
   AppointmentList *temp_al;
   struct tm date;
   GtkWidget **text;
   char desc[100];
   char datef[20];
   int dow;
   int ndim;
   int n;
   int mask;
#ifdef ENABLE_DATEBK
   int ret;
   int cat_bit;
   int db3_type;
   long use_db3_tags;
   struct db4_struct db4;
#endif

   a_list = NULL;
   text = day_texts;
   mask=0;
/*
   get_pref(PREF_CHAR_SET, &char_set, NULL);
   if (char_set==CHAR_SET_1250) {
       small_font = gdk_fontset_load("-misc-fixed-medium-r-*-*-*-100-*-*-*-iso8859-2");
   } else {
       small_font = gdk_fontset_load("-misc-fixed-medium-r-*-*-*-100-*-*-*-*-*");
   }
*/
   memcpy(&date, date_in, sizeof(struct tm));

   /* Get all of the appointments */
   get_days_appointments2(&a_list, NULL, 2, 2, 2);

   get_month_info(date.tm_mon, 1, date.tm_year, &dow, &ndim);

   weed_datebook_list(&a_list, date.tm_mon, date.tm_year, &mask);

   for (n=0, date.tm_mday=1; date.tm_mday<=ndim; date.tm_mday++, n++) {
      date.tm_sec=0;
      date.tm_min=0;
      date.tm_hour=11;
      date.tm_isdst=-1;
      date.tm_wday=0;
      date.tm_yday=1;
      mktime(&date);

      for (temp_al = a_list; temp_al; temp_al=temp_al->next) {
#ifdef ENABLE_DATEBK
	 get_pref(PREF_USE_DB3, &use_db3_tags, NULL);
	 if (use_db3_tags) {
	    ret = db3_parse_tag(temp_al->ma.a.note, &db3_type, &db4);
	    jp_logf(JP_LOG_DEBUG, "category = 0x%x\n", db4.category);
	    cat_bit=1<<db4.category;
	    if (!(cat_bit & datebook_category)) {
	       jp_logf(JP_LOG_DEBUG, "skipping rec not in this category\n");
	       continue;
	    }
	 }
#endif
	 if (isApptOnDate(&(temp_al->ma.a), &date)) {
	    if (temp_al->ma.a.event) {
	       desc[0]='\0';
	    } else {
	       get_pref_time_no_secs(datef);
	       strftime(desc, 20, datef, &(temp_al->ma.a.begin));
	    }
	    strcat(desc, " ");
	    if (temp_al->ma.a.description) {
	       strncat(desc, temp_al->ma.a.description, 40);
	       desc[35]='\0';
	    }
	    remove_cr_lfs(desc);
	    strcat(desc, "\n");
/*	    gtk_text_insert(GTK_TEXT(text[n]),
			    small_font, NULL, NULL, desc, -1);*/
	    gtk_text_insert(GTK_TEXT(text[n]),
			    NULL, NULL, NULL, desc, -1);
	 }
      }
   }
   free_AppointmentList(&a_list);

   gtk_widget_show_all(window);
   return 0;
}

void monthview_gui(struct tm *date_in)
{
   char *days[]={
      gettext_noop("Sunday"),
      gettext_noop("Monday"),
      gettext_noop("Tuesday"),
      gettext_noop("Wednesday"),
      gettext_noop("Thursday"),
      gettext_noop("Friday"),
      gettext_noop("Saturday"),
      gettext_noop("Sunday")};

   GtkWidget *label;
   GtkWidget *button;
   GtkWidget *arrow;
   GtkWidget *align;
   GtkWidget *vbox;
   GtkWidget *hbox;
   GtkWidget *hbox_temp;
   int i;
   char str[256];
   long fdow;
   const char *str_fdow;
   char title[200];

   if (window) {
      return;
   }

   for (i=0; i<6; i++) {
      glob_hbox_month_row[i]=NULL;
   }

   memcpy(&glob_month_date, date_in, sizeof(struct tm));

   get_pref(PREF_FDOW, &fdow, &str_fdow);

   window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
   gtk_container_set_border_width(GTK_CONTAINER(window), 10);
   g_snprintf(title, 200, "%s %s", PN, _("Monthly View"));
   title[199]='\0';
   gtk_window_set_title(GTK_WINDOW(window), title);

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
   strftime(str, 60, "%B %Y", &glob_month_date);
   glob_month_month_label = gtk_label_new(str);
   gtk_box_pack_start(GTK_BOX(vbox), glob_month_month_label, TRUE, TRUE, 0);

   /* Days of the week */
   hbox = gtk_hbox_new(TRUE, 0);
   gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
   for (i=0; i<7; i++) {
      label = gtk_label_new(_(days[i+fdow]));
      gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
   }

   /* glob_month_vbox */
   glob_month_vbox = gtk_vbox_new(TRUE, 0);
   gtk_box_pack_start(GTK_BOX(vbox), glob_month_vbox, TRUE, TRUE, 0);

   create_month_boxes();

   create_month_texts();

   big_text = gtk_text_new(NULL, NULL);
   gtk_widget_set_usize(GTK_WIDGET(big_text), 0, 100);
   gtk_box_pack_start(GTK_BOX(vbox), big_text, TRUE, TRUE, 0);

   display_months_appts(&glob_month_date, glob_month_texts);

   gtk_widget_show_all(window);
}

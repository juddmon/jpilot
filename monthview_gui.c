/* monthview_gui.c
 *
 * Copyright (C) 1999 by Judd Montgomery
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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
#include "prefs.h"
#include "log.h"
#include "datebook.h"
#include <pi-datebook.h>
#include "config.h"

static GtkWidget *window=NULL;
static GtkWidget *glob_month_vbox;
static GtkWidget *glob_hbox_month_row[6];
static GtkWidget *glob_month_texts[31];
static GtkWidget *glob_month_month_label;
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
cb_quit(GtkWidget *widget,
	gpointer   data)
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
      gtk_widget_destroy(glob_hbox_month_row[i]);
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
   long modified, deleted;
   struct tm date;
   GtkWidget **text;
   char desc[60];
   char datef[20];
   int dow;
   int ndim;
   int n;
   GdkFont *small_font;

   a_list = NULL;
   text = day_texts;

   small_font = gdk_fontset_load("-misc-fixed-medium-r-*-*-*-100-*-*-*-*-*");

   memcpy(&date, date_in, sizeof(struct tm));

   /* Get all of the appointments */
   get_days_appointments(&a_list, NULL);

   get_month_info(date.tm_mon, 1, date.tm_year, &dow, &ndim);

   weed_datebook_list(&a_list, date.tm_mon, date.tm_year);

   get_pref(PREF_SHOW_MODIFIED, &modified, NULL);
   get_pref(PREF_SHOW_DELETED, &deleted, NULL);

   for (n=0, date.tm_mday=1; date.tm_mday<=ndim; date.tm_mday++, n++) {
      date.tm_sec=0;
      date.tm_min=0;
      date.tm_hour=11;
      date.tm_isdst=-1;
      date.tm_wday=0;
      date.tm_yday=1;
      mktime(&date);

      for (temp_al = a_list; temp_al; temp_al=temp_al->next) {
	 if (temp_al->ma.rt == MODIFIED_PALM_REC) {
	    if (!modified) {
	       continue;
	    }
	 }
	 if (temp_al->ma.rt == DELETED_PALM_REC) {
	    if (!deleted) {
	       continue;
	    }
	 }
	 if (isApptOnDate(&(temp_al->ma.a), &date)) {
	    if (temp_al->ma.a.event) {
	       desc[0]='\0';
	    } else {
	       get_pref_time_no_secs(datef);
	       strftime(desc, 20, datef, &(temp_al->ma.a.begin));
	    }
	    strcat(desc, " ");
	    if (temp_al->ma.a.description) {
	       strncat(desc, temp_al->ma.a.description, 20);
	       desc[16]='\0';
	    }
	    remove_cr_lfs(desc);
	    strcat(desc, "\n");
	    gtk_text_insert(GTK_TEXT(text[n]),
			    small_font, NULL, NULL, desc, -1);
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

   if (window) {
      return;
   }
   
   memcpy(&glob_month_date, date_in, sizeof(struct tm));
   
   get_pref(PREF_FDOW, &fdow, &str_fdow);

   window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
   gtk_container_set_border_width(GTK_CONTAINER(window), 10);
   gtk_window_set_title(GTK_WINDOW(window), PN" Monthly View");

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
   button = gtk_button_new_with_label("Close");
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_quit), window);
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

   display_months_appts(&glob_month_date, glob_month_texts);
 
   gtk_widget_show_all(window);
}


/* weekview_gui.c
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
#include "print.h"
#include "utils.h"
#include "prefs.h"
#include "log.h"
#include "datebook.h"
#include <pi-datebook.h>


extern int datebook_category;


static GtkWidget *window=NULL;
static GtkWidget *glob_week_texts[8];
static struct tm glob_week_date;

/* Function prototypes */
int clear_weeks_appts(GtkWidget **day_texts);
int display_weeks_appts(struct tm *date_in, GtkWidget **day_texts);


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
      gtk_text_freeze(GTK_TEXT(glob_week_texts[i]));
   }
}

void thaw_weeks_appts()
{
   int i;

   for (i=0; i<8; i++) {
      gtk_text_thaw(GTK_TEXT(glob_week_texts[i]));
   }
}

static void
cb_week_move(GtkWidget *widget,
	     gpointer   data)
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

   for (i=0; i<8; i++) {
      gtk_text_set_point(GTK_TEXT(day_texts[i]), 0);
      gtk_text_forward_delete(GTK_TEXT(day_texts[i]),
			      gtk_text_get_length(GTK_TEXT(day_texts[i])));
   }
   return 0;
}

/*
 * This function requires that date_in be the date of the first day of
 * the week (be it a Sunday, or a Monday).
 * It will then print the next eight days to the day_texts array of
 * text boxes.
 */
int display_weeks_appts(struct tm *date_in, GtkWidget **day_texts)
{
   char *days[]={
      gettext_noop("Sunday"),
      gettext_noop("Monday"),
      gettext_noop("Tuesday"),
      gettext_noop("Wednesday"),
      gettext_noop("Thursday"),
      gettext_noop("Friday"),
      gettext_noop("Saturday")};
   AppointmentList *a_list;
   AppointmentList *temp_al;
   struct tm date;
   GtkWidget **text;
   char desc[256];
   char datef[20];
   int n, i;
   long ivalue;
   const char *svalue;
   char str[82];
   long fdow;
   char short_date[32];
   char default_date[]="%x";
   /* GdkFont *small_font; */
   GdkColor color;
   GdkColormap *colormap;
   /* long char_set;*/
#ifdef ENABLE_DATEBK
   int ret;
   int cat_bit;
   int db3_type;
   long use_db3_tags;
   struct db4_struct db4;
#endif

   a_list = NULL;
   text = day_texts;

   /*
   get_pref(PREF_CHAR_SET, &char_set, NULL);
   if (char_set==CHAR_SET_1250) {
       small_font = gdk_fontset_load("-misc-fixed-medium-r-*-*-*-100-*-*-*-iso8859-2");
   } else {
       small_font = gdk_fontset_load("-misc-fixed-medium-r-*-*-*-100-*-*-*-*-*");
   }
   */
   color.red = 0xAAAA;
   color.green = 0xAAAA;
   color.blue = 0xAAAA;

   colormap = gtk_widget_get_colormap(text[0]);
   gdk_color_alloc(colormap, &color);

   memcpy(&date, date_in, sizeof(struct tm));

   get_pref(PREF_FDOW, &fdow, &svalue);

   get_pref(PREF_SHORTDATE, &ivalue, &svalue);
   if (svalue==NULL) {
      svalue = default_date;
   }

   for (i=0; i<8; i++, add_days_to_date(&date, 1)) {
      strftime(short_date, 30, svalue, &date);
      g_snprintf(str, 80, "%s %s\n", _(days[(i + fdow)%7]), short_date);
      str[80]='\0';
      gtk_text_insert(GTK_TEXT(glob_week_texts[i]), NULL, NULL, &color, str, -1);
   }

   /* Get all of the appointments */
   get_days_appointments2(&a_list, NULL, 2, 2, 2);

   /* iterate through eight days */
   memcpy(&date, date_in, sizeof(struct tm));


   for (n=0; n<8; n++, add_days_to_date(&date, 1)) {
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
	       strncat(desc, temp_al->ma.a.description, 70);
	       desc[62]='\0';
	    }
	    remove_cr_lfs(desc);
	    strcat(desc, "\n");
	    /* gtk_text_insert(GTK_TEXT(text[n]),
			    small_font, NULL, NULL, desc, -1);*/
	    gtk_text_insert(GTK_TEXT(text[n]),
			    NULL, NULL, NULL, desc, -1);
	 }
      }
   }
   free_AppointmentList(&a_list);

   return 0;
}

void weekview_gui(struct tm *date_in)
{
   GtkWidget *button;
   GtkWidget *arrow;
   GtkWidget *align;
   GtkWidget *vbox, *hbox;
   GtkWidget *hbox_temp;
   GtkWidget *vbox_left, *vbox_right;
   const char *str_fdow;
   long fdow;
   int i;
   char title[200];

   if (window) {
      return;
   }

   memcpy(&glob_week_date, date_in, sizeof(struct tm));

   window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
   gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
   gtk_container_set_border_width(GTK_CONTAINER(window), 10);
   g_snprintf(title, 200, "%s %s", PN, _("Weekly View"));
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

   get_pref(PREF_FDOW, &fdow, &str_fdow);

   hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

   vbox_left = gtk_vbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(hbox), vbox_left, TRUE, TRUE, 0);

   vbox_right = gtk_vbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(hbox), vbox_right, TRUE, TRUE, 0);

   /* Get the first day of the week */
   sub_days_from_date(&glob_week_date, (7 - fdow + glob_week_date.tm_wday)%7);

   for (i=0; i<8; i++) {
      glob_week_texts[i] = gtk_text_new(NULL, NULL);
      if (i>3) {
	 gtk_box_pack_start(GTK_BOX(vbox_right), glob_week_texts[i], FALSE, FALSE, 0);
      } else {
	 gtk_box_pack_start(GTK_BOX(vbox_left), glob_week_texts[i], FALSE, FALSE, 0);
      }
   }

   display_weeks_appts(&glob_week_date, glob_week_texts);

   gtk_widget_show_all(window);
}

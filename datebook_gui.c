/* datebook_gui.c
 * 
 * Copyright (C) 1999 by Judd Montgomery
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

/*Hack #1: */
/*If you try to open a modal dialog from a clist and the clist is set to */
/*BROWSE mode it will lock up X windows (at least on my machine). */
/*I suspect that this is a GTK bug.  I have worked around it with this hack */
/*for now. */


#define EASTER

#include "config.h"
#include "i18n.h"
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdk.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pi-dlp.h>
#include <pi-datebook.h>

#include "datebook.h"
#include "log.h"
#include "prefs.h"
#include "utils.h"
#include "password.h"
#include "print.h"
#include "alarms.h"
#include "japanese.h"


#define PAGE_NONE  0
#define PAGE_DAY   1
#define PAGE_WEEK  2
#define PAGE_MONTH 3
#define PAGE_YEAR  4
#define BEGIN_DATE_BUTTON  5

#define CAL_DAY_SELECTED 327

#define CONNECT_SIGNALS 400
#define DISCONNECT_SIGNALS 401
   
/*#define DATE_CHART */

extern GtkTooltips *glob_tooltips;

static GtkWidget *pane;

static void highlight_days();

static int update_dayview_screen();
static void update_endon_button(GtkWidget *button, struct tm *t);
static void cb_toggle(GtkWidget *button, int category);
static void set_begin_end_labels(struct tm *begin, struct tm *end);

void cb_clist1_selection(GtkWidget      *clist,
			 gint           row,
			 gint           column,
			 GdkEventButton *event,
			 gpointer       data);
static void cb_add_new_record(GtkWidget *widget,
			      gpointer   data);

static void set_new_button_to(int new_state);
static void connect_changed_signals(int con_or_dis);

GtkWidget *main_calendar;
GtkWidget *dow_label;
GtkWidget *clist1;
GtkWidget *text1, *text2;
static GtkWidget *private_checkbox;
GtkWidget *check_button_alarm;
GtkWidget *check_button_day_endon;
GtkWidget *check_button_week_endon;
GtkWidget *check_button_mon_endon;
GtkWidget *check_button_year_endon;
GtkWidget *units_entry;
GtkWidget *repeat_day_entry;
GtkWidget *repeat_week_entry;
GtkWidget *repeat_mon_entry;
GtkWidget *repeat_year_entry;
GtkWidget *radio_button_alarm_min;
GtkWidget *radio_button_alarm_hour;
GtkWidget *radio_button_alarm_day;
GtkWidget *glob_endon_day_button;
struct tm glob_endon_day_tm;
GtkWidget *glob_endon_week_button;
struct tm glob_endon_week_tm;
GtkWidget *glob_endon_mon_button;
struct tm glob_endon_mon_tm;
GtkWidget *glob_endon_year_button;
struct tm glob_endon_year_tm;
GtkWidget *toggle_button_repeat_days[7];
GtkWidget *toggle_button_repeat_mon_byday;
GtkWidget *toggle_button_repeat_mon_bydate;
static GtkWidget *notebook;
int current_day;   /*range 1-31 */
int current_month; /*range 0-11 */
int current_year;  /*years since 1900 */
static int clist_row_selected;
static int record_changed;
int datebook_category=0xFFFF; /* This is a bitmask */
AppointmentList *current_al;

GtkWidget *hbox_alarm1, *hbox_alarm2;

GtkWidget *scrolled_window;

static struct tm begin_date, end_date;
static GtkWidget *begin_date_button;
static GtkWidget *begin_time_entry, *end_time_entry;
static GtkWidget *check_button_notime;

static GtkWidget *new_record_button;
static GtkWidget *apply_record_button;
static GtkWidget *add_record_button;

GtkAccelGroup *accel_group;

/*
 * Start Datebk3/4 code
 */
#ifdef USE_DB3
static GtkWidget *window_date_cats = NULL;
static GtkWidget *toggle_button[16];

static gboolean cb_destroy_date_cats(GtkWidget *widget)
{
   window_date_cats = NULL;
   return FALSE;
}

static void
cb_quit_date_cats(GtkWidget *widget,
		   gpointer   data)
{
   jpilot_logf(LOG_DEBUG, "cb_quit_date_cats\n");
   if (GTK_IS_WIDGET(data)) {
      gtk_widget_destroy(data);
   }
}

static void
cb_date_cats_all_none(GtkWidget *widget,
		      gpointer   data)
{
   int i, count;
   int flag=GPOINTER_TO_INT(data);
   
   jpilot_logf(LOG_DEBUG, "cb_date_cats_all_none\n");

   count=0;
   for (i=0; i<16; i++) {
      if (GTK_IS_WIDGET(toggle_button[i])) {
	 if ((GTK_TOGGLE_BUTTON(toggle_button[i])->active) != (flag)) {
	    count++;
	 }
      }
   }
   cb_toggle(NULL, count | 0x4000);
   for (i=0; i<16; i++) {
      if (GTK_IS_WIDGET(toggle_button[i])) {
	 gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle_button[i]), flag);
	 /*gtk_signal_emit_stop_by_name(GTK_OBJECT(toggle_button[i]), "toggled");*/
      }
   }
   if (flag) {
      datebook_category=0xFFFF;
   } else {
      datebook_category=0x0000;
   }
   update_dayview_screen();
}

static void cb_toggle(GtkWidget *widget, int category)
{
   int bit=1;
   int cat_bit;
   int on;
   static int ignore_count=0;

   if (category & 0x4000) {
      ignore_count=category & 0xFF;
      return;
   }
   if (ignore_count) {
      ignore_count--;
      return;
   }
   if (GTK_TOGGLE_BUTTON(toggle_button[category])->active) {
      on=1;
   } else {
      on=0;
   }
   cat_bit = bit << category;
   if (on) {
      datebook_category |= cat_bit;
   } else {
      datebook_category &= ~cat_bit;
   }
   
   update_dayview_screen();
}

void cb_date_cats(GtkWidget *widget, gpointer data)
{
   unsigned char *buf;
   int size;
   struct AppointmentAppInfo ai;
   int i;
   int bit;
   long char_set;
   GtkWidget *table;
   GtkWidget *button;
   GtkWidget *vbox, *hbox;
   
   jpilot_logf(LOG_DEBUG, "cb_date_cats\n");
   if (GTK_IS_WINDOW(window_date_cats)) {
      gdk_window_raise(window_date_cats->window);
      jpilot_logf(LOG_DEBUG, "date_cats window is already up\n");
      return;
   }

   get_app_info("DatebookDB", &buf, &size);
   jpilot_logf(LOG_DEBUG, "Got datebook app info, size = %d", size);
   if (size<1) {
      return;
   }
   unpack_AppointmentAppInfo(&ai, buf, size);
   free(buf);

   window_date_cats = gtk_window_new(GTK_WINDOW_TOPLEVEL);

   gtk_window_set_position(GTK_WINDOW(window_date_cats), GTK_WIN_POS_MOUSE);

   gtk_container_set_border_width(GTK_CONTAINER(window_date_cats), 10);
   gtk_window_set_title(GTK_WINDOW(window_date_cats), PN" Datebook Categories");

   gtk_signal_connect(GTK_OBJECT(window_date_cats), "destroy",
                      GTK_SIGNAL_FUNC(cb_destroy_date_cats), window_date_cats);

   vbox = gtk_vbox_new(FALSE, 0);
   gtk_container_add(GTK_CONTAINER(window_date_cats), vbox);

   /* Table */
   table = gtk_table_new(8, 2, TRUE);
   gtk_table_set_row_spacings(GTK_TABLE(table),0);
   gtk_table_set_col_spacings(GTK_TABLE(table),0);
   gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);

   get_pref(PREF_CHAR_SET, &char_set, NULL);
   for (i=0, bit=1; i<16; i++, bit <<= 1) {
      if (ai.category.name[i][0]) {
         if (char_set == CHAR_SET_JAPANESE)  Sjis2Euc(ai.category.name[i], 65536);
	 toggle_button[i]=gtk_toggle_button_new_with_label
	   (ai.category.name[i]);	 
	 gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle_button[i]),
				      datebook_category & bit);
	 gtk_table_attach_defaults
	   (GTK_TABLE(table), GTK_WIDGET(toggle_button[i]),
	    (i>7)?1:0, (i>7)?2:1, (i>7)?i-8:i, (i>7)?i-7:i+1);
	 gtk_signal_connect(GTK_OBJECT(toggle_button[i]), "toggled",
			    GTK_SIGNAL_FUNC(cb_toggle), GINT_TO_POINTER(i));
      } else {
	 toggle_button[i]=NULL;
      }
   }

   hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

   /* Create a "Quit" button */
   button = gtk_button_new_with_label(_("Done"));
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_quit_date_cats), window_date_cats);
   gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

   /* Create a "All" button */
   button = gtk_button_new_with_label(_("All"));
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_date_cats_all_none), GINT_TO_POINTER(1));
   gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

   /* Create a "None" button */
   button = gtk_button_new_with_label(_("None"));
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_date_cats_all_none), GINT_TO_POINTER(0));
   gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
   
   gtk_widget_show_all(window_date_cats);
}

#endif

int datebook_print()
{
   AppointmentList *a_list;
   struct tm date;
   
   date.tm_mon=current_month;
   date.tm_mday=current_day;
   date.tm_year=current_year;
   date.tm_sec=0;
   date.tm_min=0;
   date.tm_hour=11;
   date.tm_isdst=-1;
   mktime(&date);
   
   a_list = NULL;
   
   get_days_appointments2(&a_list, &date, 2, 2, 2);

   print_datebook(&date, a_list);

   return 0;
}

void cb_monthview(GtkWidget *widget,
		  gpointer   data)
{
   struct tm date;
   
   bzero(&date, sizeof(date));
   date.tm_mon=current_month;
   date.tm_mday=current_day;
   date.tm_year=current_year;
   monthview_gui(&date);
}

static void cb_cal_dialog(GtkWidget *widget,
			  gpointer   data)
{
   long fdow;
   int r = 0;
   struct tm *Pt;
   GtkWidget *Pcheck_button;
   GtkWidget *Pbutton;

   switch (GPOINTER_TO_INT(data)) {
    case PAGE_DAY:
      Pcheck_button = check_button_day_endon;
      Pt = &glob_endon_day_tm;
      Pbutton = glob_endon_day_button;
      break;
    case PAGE_WEEK:
      Pcheck_button = check_button_week_endon;
      Pt = &glob_endon_week_tm;
      Pbutton = glob_endon_week_button;
      break;
    case PAGE_MONTH:
      Pcheck_button = check_button_mon_endon;
      Pt = &glob_endon_mon_tm;
      Pbutton = glob_endon_mon_button;
      break;
    case PAGE_YEAR:
      Pcheck_button = check_button_year_endon;
      Pt = &glob_endon_year_tm;
      Pbutton = glob_endon_year_button;
      break;
    case BEGIN_DATE_BUTTON:
      Pcheck_button = NULL;
      Pt = &begin_date;
      Pbutton = begin_date_button;
      break;
    default:
      Pcheck_button = NULL;
      Pbutton = NULL;
      jpilot_logf(LOG_DEBUG, "default hit in cb_cal_dialog()\n");
      return;
   }

   get_pref(PREF_FDOW, &fdow, NULL);

   r = cal_dialog(_("End On Date"), fdow,
		  &(Pt->tm_mon),
		  &(Pt->tm_mday),
		  &(Pt->tm_year));

   if (GPOINTER_TO_INT(data) == BEGIN_DATE_BUTTON) {
      end_date.tm_mon = begin_date.tm_mon;
      end_date.tm_mday = begin_date.tm_mday;
      end_date.tm_year = begin_date.tm_year;
   }
   
   if (r==CAL_DONE) {
      if (Pcheck_button) {
	 gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(Pcheck_button), TRUE);
      }
      if (Pbutton) {
	 update_endon_button(Pbutton, Pt);
      }
   }
}

void cb_weekview(GtkWidget *widget,
		 gpointer   data)
{
   struct tm date;

   bzero(&date, sizeof(date));
   date.tm_mon=current_month;
   date.tm_mday=current_day;
   date.tm_year=current_year;
   date.tm_sec=0;
   date.tm_min=0;
   date.tm_hour=11;
   date.tm_isdst=-1;
   mktime(&date);
   weekview_gui(&date);
}

static void init()
{
   time_t ltime;
   struct tm *now;
   AppointmentList *a_list;
   AppointmentList *temp_al;

   record_changed=CLEAR_FLAG;
   time(&ltime);
   now = localtime(&ltime);
   current_day = now->tm_mday;
   current_month = now->tm_mon;
   current_year = now->tm_year;

   memcpy(&glob_endon_day_tm, now, sizeof(glob_endon_day_tm));
   memcpy(&glob_endon_week_tm, now, sizeof(glob_endon_week_tm));
   memcpy(&glob_endon_mon_tm, now, sizeof(glob_endon_mon_tm));
   memcpy(&glob_endon_year_tm, now, sizeof(glob_endon_year_tm));

   if (glob_find_id) {
      jpilot_logf(LOG_DEBUG, "init() glob_find_id = %d\n", glob_find_id);
      /* Search Appointments for this id to get its date */
      a_list = NULL;
   
      get_days_appointments2(&a_list, NULL, 1, 1, 1);

      for (temp_al = a_list; temp_al; temp_al=temp_al->next) {
	 if (temp_al->ma.unique_id == glob_find_id) {
	    jpilot_logf(LOG_DEBUG, "init() found glob_find_id\n");
	    current_month = temp_al->ma.a.begin.tm_mon;
	    current_day = temp_al->ma.a.begin.tm_mday;
	    current_year = temp_al->ma.a.begin.tm_year;
	    break;
	 }
      }
      free_AppointmentList(&a_list);
   }

   current_al = NULL;

   clist_row_selected=0;
}


int dialog_4_or_last(int dow)
{
   char *days[]={
      gettext_noop("Sunday"),
      gettext_noop("Monday"),
      gettext_noop("Tuesday"),
      gettext_noop("Wednesday"),
      gettext_noop("Thursday"),
      gettext_noop("Friday"),
      gettext_noop("Saturday")
   };
   char text[255];
   /* needs tagged */
   char *button_text[]={
       "4th", "Last"
   };
   
   /* needs tagged */
   sprintf(text,
	   "This appointment can either\n"
	   "repeat on the 4th %s\n"
	   "of the month, or on the last\n"
	   "%s of the month.\n"
	   "Which do you want?",
	   _(days[dow]), _(days[dow]));
   return dialog_generic(scrolled_window->parent->window,
			 200, 200, 
			 "Question?", "Answer: ",
			 text, 2, button_text);
}

int dialog_current_all_cancel()
{
   /* needs tagged */
   char text[]=
     /*--------------------------- */
     "This is a repeating event.\n"
     "Do you want to apply these\n"
     "changes to just the CURRENT\n"
     "event, or ALL of the\n"
     "occurrences of this event?";
   char *button_text[]={"Current","All","Cancel"
   };
   
   return dialog_generic(scrolled_window->parent->window,
			 200, 200, 
			 "Question?", "Answer: ",
			 text, 3, button_text);
}

#ifdef EASTER
int dialog_easter(int mday)
{
   char text[255];
   char who[50];
   char *button_text[]={"I'll send a present!!!"
   };
   if (mday==29) {
      strcpy(who, "Judd Montgomery");
   }
   if (mday==20) {
      strcpy(who, "Jacki Montgomery");
   }
   sprintf(text,
	   /*--------------------------- */
	   "Today is\n"
	   "%s\'s\n"
	   "Birthday!\n", who);
   
   return dialog_generic(scrolled_window->parent->window,
			 200, 200, 
			 "Happy Birthday to Me!", "(iiiii)",
			 text, 1, button_text);
}
/* */
/*End of Dialog window code */
/* */
#endif

/* */
/* month = 0-11 */
/* dom = day of month 1-31 */
/* year = calendar year - 1900 */
/* dow = day of week 0-6, where 0=Sunday, etc. */
/* */
/* Returns an enum from DayOfMonthType defined in pi-datebook.h */
/* */
long get_dom_type(int month, int dom, int year, int dow)
{
   long r;
   int ndim; /* ndim = number of days in month 28-31 */
   int dow_fdof; /*Day of the week for the first day of the month */
   int result;
   
   r=((int)((dom-1)/7))*7 + dow;

   /* If its the 5th occurence of this dow in the month then it is always */
   /*going to be the last occurrence of that dow in the month. */
   /*Sometimes this will occur in the 4th week, sometimes in the 5th. */
   /* If its the 4th occurence of this dow in the month and there is a 5th */
   /*then it always the 4th occurence. */
   /* If its the 4th occurence of this dow in the month and there is not a */
   /*5th then we need to ask if this appointment repeats on the last dow of */
   /*the month, or the 4th dow of every month. */
   /* This should be perfectly clear now, right? */

   /*These are the last 2 lines of the DayOfMonthType enum: */
   /*dom4thSun, dom4thMon, dom4thTue, dom4thWen, dom4thThu, dom4thFri, dom4thSat */
   /*domLastSun, domLastMon, domLastTue, domLastWen, domLastThu, domLastFri, domLastSat */

   if ((r>=dom4thSun) && (r<=dom4thSat)) {
      get_month_info(month, dom, year,  &dow_fdof, &ndim);
      if ((ndim - dom < 7)) {
	 /*This is the 4th dow, and there is no 5th in this month. */
	 result = dialog_4_or_last(dow);
	 /*If they want it to be the last dow in the month instead of the */
	 /*4th, then we need to add 7. */
	 if (result == DIALOG_SAID_LAST) {
	    r += 7;
	 }
      }
   }

   return r;
}

static void set_begin_end_labels(struct tm *begin, struct tm *end)
{
   long ivalue;
   char str[255];
   char time1_str[255];
   char time2_str[255];
   char pref_time[40];
   const char *pref_date;
   
   str[0]='\0';
   
   get_pref(PREF_SHORTDATE, &ivalue, &pref_date);

   if (GTK_TOGGLE_BUTTON(check_button_notime)->active) {
      gtk_entry_set_text(GTK_ENTRY(begin_time_entry), "");
      gtk_entry_set_text(GTK_ENTRY(end_time_entry), "");
   } else {
      get_pref_time_no_secs(pref_time);
      
      strftime(time1_str, 250, pref_time, begin);
      strftime(time2_str, 250, pref_time, end);

      gtk_entry_set_text(GTK_ENTRY(begin_time_entry), time1_str);
      gtk_entry_set_text(GTK_ENTRY(end_time_entry), time2_str);
   }
   
   strftime(str, 250, pref_date, begin);
   
   gtk_label_set_text(GTK_LABEL(GTK_BIN(begin_date_button)->child), str);
}

static void clear_begin_end_labels()
{
   begin_date.tm_mon = current_month;
   begin_date.tm_mday = current_day;
   begin_date.tm_year = current_year;
   begin_date.tm_hour = 8;
   begin_date.tm_min = 0;
   begin_date.tm_sec = 0;
   begin_date.tm_isdst = -1;

   end_date.tm_mon = current_month;
   end_date.tm_mday = current_day;
   end_date.tm_year = current_year;
   end_date.tm_hour = 9;
   end_date.tm_min = 0;
   end_date.tm_sec = 0;
   end_date.tm_isdst = -1;

   set_begin_end_labels(&begin_date, &end_date);
}

static void clear_details()
{
   int i;
   struct tm today;
   
   connect_changed_signals(DISCONNECT_SIGNALS);

   clear_begin_end_labels();

   gtk_text_set_point(GTK_TEXT(text1),
		      gtk_text_get_length(GTK_TEXT(text1)));
   gtk_text_set_point(GTK_TEXT(text2),
		      gtk_text_get_length(GTK_TEXT(text2)));
   gtk_text_backward_delete(GTK_TEXT(text1),
			    gtk_text_get_length(GTK_TEXT(text1)));
   gtk_text_backward_delete(GTK_TEXT(text2),
			    gtk_text_get_length(GTK_TEXT(text2)));
   gtk_notebook_set_page(GTK_NOTEBOOK(notebook), PAGE_NONE);
   /* Clear the notebook pages */
   
   gtk_entry_set_text(GTK_ENTRY(repeat_day_entry), "1");
   gtk_entry_set_text(GTK_ENTRY(repeat_week_entry), "1");
   gtk_entry_set_text(GTK_ENTRY(repeat_mon_entry), "1");
   gtk_entry_set_text(GTK_ENTRY(repeat_year_entry), "1");

   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_button_day_endon), FALSE);
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_button_week_endon), FALSE);
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_button_mon_endon), FALSE);
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_button_year_endon), FALSE);
   
   for(i=0; i<7; i++) {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle_button_repeat_days[i]), FALSE);
   }
   
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle_button_repeat_mon_bydate), TRUE);
   
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(private_checkbox), FALSE);

   bzero(&today, sizeof(today));
   today.tm_year = current_year;
   today.tm_mon = current_month;
   today.tm_mday = current_day;
   today.tm_hour = 12;
   today.tm_min = 0;
   mktime(&today);

   memcpy(&glob_endon_day_tm, &today, sizeof(glob_endon_day_tm));
   memcpy(&glob_endon_week_tm, &today, sizeof(glob_endon_week_tm));
   memcpy(&glob_endon_mon_tm, &today, sizeof(glob_endon_mon_tm));
   memcpy(&glob_endon_year_tm, &today, sizeof(glob_endon_year_tm));

   connect_changed_signals(CONNECT_SIGNALS);
}

static int get_details(struct Appointment *a, unsigned char *attrib)
{
   int i;
   time_t ltime, ltime2;
   struct tm *now;
   char str[30];
   gchar *text;
   gint page;
   int total_repeat_days;
   long ivalue;
   char datef[32];
   const char *svalue1, *svalue2;
   const char *period[] = {
      gettext_noop("none"),
      gettext_noop("day"),
      gettext_noop("week"),
      gettext_noop("month"),
      gettext_noop("year")
   };
   
   bzero(a, sizeof(*a));
   
   *attrib = 0;
   			       
   time(&ltime);
   now = localtime(&ltime);

   total_repeat_days=0;

   /*The first day of the week */
   /*I always use 0, Sunday is always 0 in this code */
   a->repeatWeekstart=0;

   a->exceptions = 0;
   a->exception = NULL;

   /*Set the daylight savings flags */
   a->end.tm_isdst=a->begin.tm_isdst=-1;

   a->begin.tm_mon  = begin_date.tm_mon;
   a->begin.tm_mday = begin_date.tm_mday;
   a->begin.tm_year = begin_date.tm_year;
   a->begin.tm_hour = begin_date.tm_hour;
   a->begin.tm_min  = begin_date.tm_min;
   a->begin.tm_sec  = 0;

   /*get the end times */
   a->end.tm_mon  = end_date.tm_mon;
   a->end.tm_mday = end_date.tm_mday;
   a->end.tm_year = end_date.tm_year;
   a->end.tm_hour = end_date.tm_hour;
   a->end.tm_min  = end_date.tm_min;
   a->end.tm_sec  = 0;

   if (GTK_TOGGLE_BUTTON(check_button_notime)->active) {
      a->event=1;
      /*This event doesn't have a time */
      a->begin.tm_hour = 0;
      a->begin.tm_min  = 0;
      a->begin.tm_sec  = 0;
      a->end.tm_hour = 0;
      a->end.tm_min  = 0;
      a->end.tm_sec  = 0;
   } else {
      a->event=0;
   }

   ltime = mktime(&a->begin);

   ltime2 = mktime(&a->end);
   
   if (ltime > ltime2) {
      memcpy(&(a->end), &(a->begin), sizeof(struct tm));
   }

   if (GTK_TOGGLE_BUTTON(check_button_alarm)->active) {
      a->alarm = 1;
      text = gtk_entry_get_text(GTK_ENTRY(units_entry));
      a->advance=atoi(text);
      jpilot_logf(LOG_DEBUG, "alarm advance %d", a->advance);
      if (GTK_TOGGLE_BUTTON(radio_button_alarm_min)->active) {
	 a->advanceUnits = advMinutes;
	 jpilot_logf(LOG_DEBUG, "min\n");
      }
      if (GTK_TOGGLE_BUTTON(radio_button_alarm_hour)->active) {
	 a->advanceUnits = advHours;
	 jpilot_logf(LOG_DEBUG, "hour\n");
      }
      if (GTK_TOGGLE_BUTTON(radio_button_alarm_day)->active) {
	 a->advanceUnits = advDays;
	 jpilot_logf(LOG_DEBUG, "day\n");
      }
   } else {
      a->alarm = 0;
   }

   page = gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook));

   a->repeatEnd.tm_hour = 0;
   a->repeatEnd.tm_min  = 0;
   a->repeatEnd.tm_sec  = 0;
   a->repeatEnd.tm_isdst= -1;
   
   switch (page) {
    case PAGE_NONE:
      a->repeatType=repeatNone;
      jpilot_logf(LOG_DEBUG, "no repeat\n");
      break;
    case PAGE_DAY:
      a->repeatType=repeatDaily;
      text = gtk_entry_get_text(GTK_ENTRY(repeat_day_entry));
      a->repeatFrequency = atoi(text);
      jpilot_logf(LOG_DEBUG, "every %d day(s)\n", a->repeatFrequency);
      if (GTK_TOGGLE_BUTTON(check_button_day_endon)->active) {
	 a->repeatForever=0;
	 jpilot_logf(LOG_DEBUG, "end on day\n");
	 a->repeatEnd.tm_mon = glob_endon_day_tm.tm_mon;
	 a->repeatEnd.tm_mday = glob_endon_day_tm.tm_mday;
	 a->repeatEnd.tm_year = glob_endon_day_tm.tm_year;
	 mktime(&a->repeatEnd);
      } else {
	 a->repeatForever=1;
      }
      break;
    case PAGE_WEEK:
      a->repeatType=repeatWeekly;
      text = gtk_entry_get_text(GTK_ENTRY(repeat_week_entry));
      a->repeatFrequency = atoi(text);
      jpilot_logf(LOG_DEBUG, "every %d week(s)\n", a->repeatFrequency);
      if (GTK_TOGGLE_BUTTON(check_button_week_endon)->active) {
	 a->repeatForever=0;
	 jpilot_logf(LOG_DEBUG, "end on week\n");
	 a->repeatEnd.tm_mon = glob_endon_week_tm.tm_mon;
	 a->repeatEnd.tm_mday = glob_endon_week_tm.tm_mday;
	 a->repeatEnd.tm_year = glob_endon_week_tm.tm_year;
	 mktime(&a->repeatEnd);

	 get_pref(PREF_SHORTDATE, &ivalue, &svalue1);
	 get_pref(PREF_TIME, &ivalue, &svalue2);
	 if ((svalue1==NULL) || (svalue2==NULL)) {
	    strcpy(datef, "%x %X");
	 } else {
	    sprintf(datef, "%s %s", svalue1, svalue2);
	 }
	 strftime(str, 30, datef, &a->repeatEnd);

	 jpilot_logf(LOG_DEBUG, "repeat_end time = %s\n",str);
      } else {
	 a->repeatForever=1;
      }
      jpilot_logf(LOG_DEBUG, "Repeat Days:");
      a->repeatWeekstart = 0;  /*We are going to always use 0 */
      for (i=0; i<7; i++) {
	 a->repeatDays[i]=(GTK_TOGGLE_BUTTON(toggle_button_repeat_days[i])->active);
	 total_repeat_days += a->repeatDays[i];
      }
      jpilot_logf(LOG_DEBUG, "\n");
      break;
    case PAGE_MONTH:
      text = gtk_entry_get_text(GTK_ENTRY(repeat_mon_entry));
      a->repeatFrequency = atoi(text);
      jpilot_logf(LOG_DEBUG, "every %d month(s)\n", a->repeatFrequency);
      if (GTK_TOGGLE_BUTTON(check_button_mon_endon)->active) {
	 a->repeatForever=0;
	 jpilot_logf(LOG_DEBUG, "end on month\n");
	 a->repeatEnd.tm_mon = glob_endon_mon_tm.tm_mon;
	 a->repeatEnd.tm_mday = glob_endon_mon_tm.tm_mday;
	 a->repeatEnd.tm_year = glob_endon_mon_tm.tm_year;
	 mktime(&a->repeatEnd);

	 get_pref(PREF_SHORTDATE, &ivalue, &svalue1);
	 get_pref(PREF_TIME, &ivalue, &svalue2);
	 if ((svalue1==NULL) || (svalue2==NULL)) {
	    strcpy(datef, "%x %X");
	 } else {
	    sprintf(datef, "%s %s", svalue1, svalue2);
	 }
	 strftime(str, 30, datef, &a->repeatEnd);

	 jpilot_logf(LOG_DEBUG, "repeat_end time = %s\n",str);
      } else {
	 a->repeatForever=1;
      }
      if (GTK_TOGGLE_BUTTON(toggle_button_repeat_mon_byday)->active) {
	 a->repeatType=repeatMonthlyByDay;
	 a->repeatDay = get_dom_type(a->begin.tm_mon, a->begin.tm_mday, a->begin.tm_year, a->begin.tm_wday);
	 jpilot_logf(LOG_DEBUG, "***by day\n");
      }
      if (GTK_TOGGLE_BUTTON(toggle_button_repeat_mon_bydate)->active) {
	 a->repeatType=repeatMonthlyByDate;
	 jpilot_logf(LOG_DEBUG, "***by date\n");
      }
      break;
    case PAGE_YEAR:
      a->repeatType=repeatYearly;
      text = gtk_entry_get_text(GTK_ENTRY(repeat_year_entry));
      a->repeatFrequency = atoi(text);
      jpilot_logf(LOG_DEBUG, "every %s year(s)\n", a->repeatFrequency);
      if (GTK_TOGGLE_BUTTON(check_button_year_endon)->active) {
	 a->repeatForever=0;
	 jpilot_logf(LOG_DEBUG, "end on year\n");
	 a->repeatEnd.tm_mon = glob_endon_year_tm.tm_mon;
	 a->repeatEnd.tm_mday = glob_endon_year_tm.tm_mday;
	 a->repeatEnd.tm_year = glob_endon_year_tm.tm_year;
	 mktime(&a->repeatEnd);
	 
	 get_pref(PREF_SHORTDATE, &ivalue, &svalue1);
	 get_pref(PREF_TIME, &ivalue, &svalue2);
	 if ((svalue1==NULL) || (svalue2==NULL)) {
	    strcpy(datef, "%x %X");
	 } else {
	    sprintf(datef, "%s %s", svalue1, svalue2);
	 }
	 str[0]='\0';
	 strftime(str, 30, datef, &a->repeatEnd);

	 jpilot_logf(LOG_DEBUG, "repeat_end time = %s\n",str);
      } else {
	 a->repeatForever=1;
      }
      break;
   }

   a->description = gtk_editable_get_chars(GTK_EDITABLE(text1), 0, -1);
   /* Empty appointment descriptions crash PalmOS 2.0, but are fine in
    * later versions */
   if (a->description[0]=='\0') {
      a->description=strdup(" ");
   }
   if (a->description) {
      jpilot_logf(LOG_DEBUG, "text1=[%s]\n",a->description);
   }
   
   a->note = gtk_editable_get_chars(GTK_EDITABLE(text2), 0, -1);
   if (a->note[0]=='\0') {
      a->note=NULL;
   }
   if (a->note) {
      jpilot_logf(LOG_DEBUG, "text2=[%s]\n",a->note);
   }
   
   /* We won't allow a repeat frequency of less than 1 */
   if ((page != PAGE_NONE) && (a->repeatFrequency < 1)) {
      jpilot_logf(LOG_WARN,
		  _("You cannot have an appointment that repeats every %d %s(s)\n"),
		  a->repeatFrequency, _(period[page]));
      a->repeatFrequency = 1;
      return -1;
   }

   /* We won't allow a weekly repeating that doesn't repeat on any day */
   if ((page == PAGE_WEEK) && (total_repeat_days == 0)) {
      jpilot_logf(LOG_WARN,
		  _("You can not have a weekly repeating appointment that doesn't repeat on any day of the week.\n"));
      return -1;
   }

   if (GTK_TOGGLE_BUTTON(private_checkbox)->active) {
      *attrib |= dlpRecAttrSecret;
   }

   return 0;
}


static void update_endon_button(GtkWidget *button, struct tm *t)
{
   long ivalue;
   const char *short_date;
   char str[255];
   
   get_pref(PREF_SHORTDATE, &ivalue, &short_date);
   strftime(str, 250, short_date, t);

   gtk_label_set_text(GTK_LABEL(GTK_BIN(button)->child), str);
}


static int update_dayview_screen()
{
   int num_entries, i;
   AppointmentList *temp_al;
   gchar *empty_line[] = { "","","","",""};
   char a_time[32];
   char begin_time[32];
   char end_time[32];
   char datef[20];
   GdkPixmap *pixmap_note;
   GdkPixmap *pixmap_alarm;
   GdkBitmap *mask_note;
   GdkBitmap *mask_alarm;
   int has_note;
#ifdef USE_DB3
   int ret;
   int category;
   int cat_bit;
   long use_db3_tags;
   GdkPixmap *pixmap_float_check;
   GdkPixmap *pixmap_float_checked;
   GdkBitmap *mask_float_check;
   GdkBitmap *mask_float_checked;
#endif
   struct tm new_time;
   GdkColor color;
   GdkColormap *colormap;
   int show_priv;

   jpilot_logf(LOG_DEBUG, "update_dayview_screen()\n");

   new_time.tm_sec=0;
   new_time.tm_min=0;
   new_time.tm_hour=11;
   new_time.tm_mday=current_day;
   new_time.tm_mon=current_month;
   new_time.tm_year=current_year;
   new_time.tm_isdst=-1;

   mktime(&new_time);

   free_AppointmentList(&current_al);

   num_entries = get_days_appointments2(&current_al, &new_time, 2, 2, 1);

   jpilot_logf(LOG_DEBUG, "get_days_appointments==>%d\n", num_entries);
#ifdef USE_DB3
   jpilot_logf(LOG_DEBUG, "datebook_category = 0x%x\n", datebook_category);
#endif

   gtk_clist_clear(GTK_CLIST(clist1));
   
   show_priv = show_privates(GET_PRIVATES, NULL);

   for (temp_al = current_al, i=0; temp_al; temp_al=temp_al->next, i++) {
      if ((show_priv != SHOW_PRIVATES) && 
	  (temp_al->ma.attrib & dlpRecAttrSecret)) {
	 i--;
	 continue;
      }
#ifdef USE_DB3
      get_pref(PREF_USE_DB3, &use_db3_tags, NULL);
      ret=0;
      if (use_db3_tags) {
	 ret = db3_is_float(&(temp_al->ma.a), &category);
	 jpilot_logf(LOG_DEBUG, "category = 0x%x\n", category);
	 cat_bit=1<<category;
	 if (!(cat_bit & datebook_category)) {
	    i--;
	    jpilot_logf(LOG_DEBUG, "skipping rec not in this category\n");
	    continue;
	 }
      }
#endif
      
      gtk_clist_append(GTK_CLIST(clist1), empty_line);
      if (temp_al->ma.a.event) {
	 /*This is a timeless event */
	 strcpy(a_time, _("No Time"));
      } else {
	 get_pref_time_no_secs_no_ampm(datef);
	 strftime(begin_time, 20, datef, &(temp_al->ma.a.begin));
	 get_pref_time_no_secs(datef);
	 strftime(end_time, 20, datef, &(temp_al->ma.a.end));
	 sprintf(a_time, "%s-%s", begin_time, end_time);
      }
      gtk_clist_set_text(GTK_CLIST(clist1), i, 0, a_time);
      gtk_clist_set_text(GTK_CLIST(clist1), i, 1, temp_al->ma.a.description);
      gtk_clist_set_row_data(GTK_CLIST(clist1), i, &(temp_al->ma));

      switch (temp_al->ma.rt) {
       case NEW_PC_REC:
	 colormap = gtk_widget_get_colormap(clist1);
	 color.red=CLIST_NEW_RED;
	 color.green=CLIST_NEW_GREEN;
	 color.blue=CLIST_NEW_BLUE;
	 gdk_color_alloc(colormap, &color);
	 gtk_clist_set_background(GTK_CLIST(clist1), i, &color);
	 break;
       case DELETED_PALM_REC:
	 colormap = gtk_widget_get_colormap(clist1);
	 color.red=CLIST_DEL_RED;
	 color.green=CLIST_DEL_GREEN;
	 color.blue=CLIST_DEL_BLUE;
	 gdk_color_alloc(colormap, &color);
	 gtk_clist_set_background(GTK_CLIST(clist1), i, &color);
	 break;
       case MODIFIED_PALM_REC:
	 colormap = gtk_widget_get_colormap(clist1);
	 color.red=CLIST_MOD_RED;
	 color.green=CLIST_MOD_GREEN;
	 color.blue=CLIST_MOD_BLUE;
	 gdk_color_alloc(colormap, &color);
	 gtk_clist_set_background(GTK_CLIST(clist1), i, &color);
	 break;
       default:
	 gtk_clist_set_background(GTK_CLIST(clist1), i, NULL);
      }

#ifdef USE_DB3
      if (use_db3_tags) {
	 if (ret & DB3_FLOAT) {
	    get_pixmaps(scrolled_window, PIXMAP_FLOAT_CHECK, 
			&pixmap_float_check, &mask_float_check);
	    gtk_clist_set_pixmap(GTK_CLIST(clist1), i, 4, pixmap_float_check, 
				 mask_float_check);
	 }
	 if (ret & DB3_FLOAT_COMPLETE) {
	    get_pixmaps(scrolled_window, PIXMAP_FLOAT_CHECKED, 
			&pixmap_float_checked, &mask_float_checked);
	    gtk_clist_set_pixmap(GTK_CLIST(clist1), i, 4, pixmap_float_checked, 
				 mask_float_checked);
	 }
      }
#endif

      has_note=0;
#ifdef USE_DB3
      if (use_db3_tags) {
	 has_note = (ret & DB3_FLOAT_HAS_NOTE);
      } else {
	 if (temp_al->ma.a.note) {
	    has_note=1;
	 }
      }
#else
      if (temp_al->ma.a.note) {
	 has_note=1;
      }
#endif
      if (has_note) {
	 /*Put a note pixmap up */
	 get_pixmaps(scrolled_window, PIXMAP_NOTE, &pixmap_note, &mask_note);
	 gtk_clist_set_pixmap(GTK_CLIST(clist1), i, 2, pixmap_note, mask_note);
      }

      if (temp_al->ma.a.alarm) {
	 /*Put an alarm pixmap up */
	 get_pixmaps(scrolled_window, PIXMAP_ALARM,&pixmap_alarm, &mask_alarm);
	 gtk_clist_set_pixmap(GTK_CLIST(clist1), i, 3, pixmap_alarm, mask_alarm);
      }
   }

   /*If there is an item in the list, select the first one */
   if (i>0) {
      gtk_clist_select_row(GTK_CLIST(clist1), 0, 1);
      cb_clist1_selection(clist1, 0, 0, (GdkEventButton *)455, "");
   } else {
      set_new_button_to(CLEAR_FLAG);
      clear_details();
   }
   return 0;
}

static void
set_new_button_to(int new_state)
{
   jpilot_logf(LOG_DEBUG, "set_new_button_to new %d old %d\n", new_state, record_changed);

   if (record_changed==new_state) {
      return;
   }

   switch (new_state) {
    case MODIFY_FLAG:
      gtk_widget_show(apply_record_button);
      break;
    case NEW_FLAG:
      gtk_widget_show(add_record_button);
      break;
    case CLEAR_FLAG:
      gtk_widget_show(new_record_button);
      break;
    default:
      return;
   }
   switch (record_changed) {
    case MODIFY_FLAG:
      gtk_widget_hide(apply_record_button);
      break;
    case NEW_FLAG:
      gtk_widget_hide(add_record_button);
      break;
    case CLEAR_FLAG:
      gtk_widget_hide(new_record_button);
      break;
   }
   record_changed=new_state;
}

static void
cb_record_changed(GtkWidget *widget,
		  gpointer   data)
{
   jpilot_logf(LOG_DEBUG, "cb_record_changed\n");
   if (record_changed==CLEAR_FLAG) {
      connect_changed_signals(DISCONNECT_SIGNALS);
      set_new_button_to(MODIFY_FLAG);
   }
}

static void cb_add_new_record(GtkWidget *widget,
			      gpointer   data)
{
   MyAppointment *ma;
   struct Appointment *a;
   struct Appointment new_a;
   int flag;
   int create_exception=0;
   int result;
   int r;
   unsigned char attrib;
   
   jpilot_logf(LOG_DEBUG, "cb_add_new_record\n");
   
   attrib = 0;

   flag=GPOINTER_TO_INT(data);
   
   if (flag==CLEAR_FLAG) {
      /*Clear button was hit */
      connect_changed_signals(DISCONNECT_SIGNALS);
      clear_details();
      set_new_button_to(NEW_FLAG);
      gtk_widget_grab_focus(GTK_WIDGET(text1));
      return;
   }
   if ((flag!=NEW_FLAG) && (flag!=MODIFY_FLAG)) {
      return;
   }
   if (flag==MODIFY_FLAG) {
      ma = gtk_clist_get_row_data(GTK_CLIST(clist1), clist_row_selected);
      if (ma < (MyAppointment *)CLIST_MIN_DATA) {
	 return;
      }
      if ((ma->rt==DELETED_PALM_REC) || (ma->rt==MODIFIED_PALM_REC)) {
	 jpilot_logf(LOG_INFO, "You can't modify a record that is deleted\n");
	 return;
      }
   } else {
      ma=NULL;
   }
   r = get_details(&new_a, &attrib);
   if (r < 0) {
      free_Appointment(&new_a);
      return;
   }
   if ((flag==MODIFY_FLAG) && (new_a.repeatType != repeatNone)) {
      /*We need more user input */
      /*Pop up a dialog */
      result = dialog_current_all_cancel();
      if (result==DIALOG_SAID_CANCEL) {
	 return;
      }
      if (result==DIALOG_SAID_CURRENT) {
 	 /*Create an exception in the appointment */
	 create_exception=1;
	 new_a.repeatType = repeatNone;
	 new_a.begin.tm_year = current_year;
	 new_a.begin.tm_mon = current_month;
	 new_a.begin.tm_mday = current_day;
	 mktime(&new_a.begin);
	 new_a.repeatType = repeatNone;
	 new_a.end.tm_year = current_year;
	 new_a.end.tm_mon = current_month;
	 new_a.end.tm_mday = current_day;
	 mktime(&new_a.end);
      }
      if (result==DIALOG_SAID_ALL) {
	 /*We still need to keep the exceptions of the original record */
	 new_a.exception = (struct tm *)malloc(ma->a.exceptions * sizeof(struct tm));
	 memcpy(new_a.exception, ma->a.exception, ma->a.exceptions * sizeof(struct tm));
	 new_a.exceptions = ma->a.exceptions;
      }
   }

   set_new_button_to(CLEAR_FLAG);

   pc_datebook_write(&new_a, NEW_PC_REC, attrib);
   free_Appointment(&new_a);
   if (flag==MODIFY_FLAG) {
      /*
       * We need to take care of the 2 options allowed when modifying
       * repeating appointments
       */
      if (create_exception) {
	 datebook_copy_appointment(&(ma->a), &a);
	 datebook_add_exception(a, current_year, current_month, current_day);
	 pc_datebook_write(a, NEW_PC_REC, attrib);
	 free_Appointment(a);
	 free(a);
      }
      delete_pc_record(DATEBOOK, ma, flag);
   }
   /*update_dayview_screen(); */
   /* Force the calendar redraw and re-read of appointments */
   gtk_signal_emit_by_name(GTK_OBJECT(main_calendar), "day_selected");

   highlight_days();

   /* Make sure that the next alarm will go off */
   alarms_find_next(NULL, NULL, TRUE);

   return;
}


void cb_delete_appt(GtkWidget *widget, gpointer data)
{
   MyAppointment *ma;
   struct Appointment *a;
   int flag;
   int result;
   
   ma = gtk_clist_get_row_data(GTK_CLIST(clist1), clist_row_selected);
   if (ma < (MyAppointment *)CLIST_MIN_DATA) {
      return;
   }
   flag = GPOINTER_TO_INT(data);
   if ((flag!=MODIFY_FLAG) && (flag!=DELETE_FLAG)) {
      return;
   }

   /* */
   /*We need to take care of the 2 options allowed when modifying */
   /*repeating appointments */
   /* */
   if (ma->a.repeatType != repeatNone) {
      /*We need more user input */
      /*Pop up a dialog */
      result = dialog_current_all_cancel();
      if (result==DIALOG_SAID_CANCEL) {
	 return;
      }
      if (result==DIALOG_SAID_CURRENT) {
 	 /*Create an exception in the appointment */
	 datebook_copy_appointment(&(ma->a), &a);
	 datebook_add_exception(a, current_year, current_month, current_day);
	 pc_datebook_write(a, NEW_PC_REC, ma->attrib);
	 free_Appointment(a);
	 free(a);
	 /*Since this was really a modify, and not a delete */
	 flag=MODIFY_FLAG;
      }
   }

   delete_pc_record(DATEBOOK, ma, flag);
   
   /* Force the calendar redraw and re-read of appointments */
   gtk_signal_emit_by_name(GTK_OBJECT(main_calendar), "day_selected");

   highlight_days();
}

void cb_check_button_alarm(GtkWidget *widget, gpointer data)
{
   if (GTK_TOGGLE_BUTTON(widget)->active) {
      gtk_widget_show(hbox_alarm2);
   } else {
      gtk_widget_hide(hbox_alarm2);
   }
}

void cb_check_button_notime(GtkWidget *widget, gpointer data)
{
   set_begin_end_labels(&begin_date, &end_date);
}

void cb_check_button_endon(GtkWidget *widget, gpointer data)
{
   GtkWidget *Pbutton;
   struct tm *Pt;
   
   switch (GPOINTER_TO_INT(data)) {
    case PAGE_DAY:
      Pbutton = glob_endon_day_button;
      Pt = &glob_endon_day_tm;
   break;
    case PAGE_WEEK:
      Pbutton = glob_endon_week_button;
      Pt = &glob_endon_week_tm;
      break;
    case PAGE_MONTH:
      Pbutton = glob_endon_mon_button;
      Pt = &glob_endon_mon_tm;
      break;
    case PAGE_YEAR:
      Pbutton = glob_endon_year_button;
      Pt = &glob_endon_year_tm;
      break;
    default:
      return;
   }
   if (GTK_TOGGLE_BUTTON(widget)->active) {
      update_endon_button(Pbutton, Pt);
   } else {
      gtk_label_set_text(GTK_LABEL(GTK_BIN(Pbutton)->child), _("No Date"));
   }
}

void cb_clist1_selection(GtkWidget      *clist,
			 gint           row,
			 gint           column,
			 GdkEventButton *event,
			 gpointer       data)
{
   struct Appointment *a;
   MyAppointment *ma;
   char temp[20];
   int i;
   
   if (!event) return;

   clist_row_selected=row;

   set_new_button_to(CLEAR_FLAG);
   
   connect_changed_signals(DISCONNECT_SIGNALS);

   a=NULL;
   ma = gtk_clist_get_row_data(GTK_CLIST(clist1), row);
   if (ma) {
      a=&(ma->a);
   }

   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_button_notime),
				a->event);

   gtk_text_set_point(GTK_TEXT(text1), 0);
   gtk_text_forward_delete(GTK_TEXT(text1),
			    gtk_text_get_length(GTK_TEXT(text1)));
   gtk_text_set_point(GTK_TEXT(text2), 0);
   gtk_text_forward_delete(GTK_TEXT(text2),
			    gtk_text_get_length(GTK_TEXT(text2)));
   
   if (a->alarm) {
      /* This is to insure that the callback gets called */
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_button_alarm), FALSE);
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_button_alarm), TRUE);
      switch (a->advanceUnits) {
       case advMinutes:
	 gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON 
				      (radio_button_alarm_min), TRUE);
	 gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON 
				      (radio_button_alarm_hour), FALSE);
	 gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON 
				      (radio_button_alarm_day), FALSE);
	 break;
       case advHours:
	 gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON 
				      (radio_button_alarm_min), FALSE);
	 gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON 
				      (radio_button_alarm_hour), TRUE);
	 gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON 
				      (radio_button_alarm_day), FALSE);
	 break;
       case advDays:
	 gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON 
				      (radio_button_alarm_min), FALSE);
	 gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON 
				      (radio_button_alarm_hour), FALSE);
	 gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
				      (radio_button_alarm_day), TRUE);
	 break;
       default:
	 jpilot_logf(LOG_WARN, "Error in DateBookDB advanceUnits = %d\n",a->advanceUnits);
      }
      sprintf(temp, "%d", a->advance);
      gtk_entry_set_text(GTK_ENTRY(units_entry), temp);
   } else {
      /* This is to insure that the callback gets called */
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_button_alarm), TRUE);
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_button_alarm), FALSE);
      gtk_entry_set_text(GTK_ENTRY(units_entry), "0");
   }
   if (a->description) {
      gtk_text_insert(GTK_TEXT(text1), NULL,NULL, NULL, a->description, -1);
   }
   if (a->note) {
      gtk_text_insert(GTK_TEXT(text2), NULL,NULL, NULL, a->note, -1);
   }

   begin_date.tm_mon = a->begin.tm_mon;
   begin_date.tm_mday = a->begin.tm_mday;
   begin_date.tm_year = a->begin.tm_year;
   begin_date.tm_hour = a->begin.tm_hour;
   begin_date.tm_min = a->begin.tm_min;

   end_date.tm_mon = a->end.tm_mon;
   end_date.tm_mday = a->end.tm_mday;
   end_date.tm_year = a->end.tm_year;
   end_date.tm_hour = a->end.tm_hour;
   end_date.tm_min = a->end.tm_min;

   set_begin_end_labels(&begin_date, &end_date);

   /*Do the Repeat information */
   switch (a->repeatType) {
    case repeatNone:
      gtk_notebook_set_page(GTK_NOTEBOOK(notebook), PAGE_NONE);
      break;
    case repeatDaily:
      if ((a->repeatForever)) {
	 gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
				      (check_button_day_endon), FALSE);
      }
      else {
	 gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
				      (check_button_day_endon), TRUE);
	 glob_endon_day_tm.tm_mon = a->repeatEnd.tm_mon;
	 glob_endon_day_tm.tm_mday = a->repeatEnd.tm_mday;
	 glob_endon_day_tm.tm_year = a->repeatEnd.tm_year;
	 update_endon_button(glob_endon_day_button, &glob_endon_day_tm);
      }
      sprintf(temp, "%d", a->repeatFrequency);
      gtk_entry_set_text(GTK_ENTRY(repeat_day_entry), temp);
      gtk_notebook_set_page(GTK_NOTEBOOK(notebook), PAGE_DAY);
      break;
    case repeatWeekly:
      if ((a->repeatForever)) {
	 gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
				      (check_button_week_endon), FALSE);
      }
      else {
	 gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
				      (check_button_week_endon), TRUE);
	 glob_endon_week_tm.tm_mon = a->repeatEnd.tm_mon;
	 glob_endon_week_tm.tm_mday = a->repeatEnd.tm_mday;
	 glob_endon_week_tm.tm_year = a->repeatEnd.tm_year;
	 update_endon_button(glob_endon_week_button, &glob_endon_week_tm);
      }
      sprintf(temp, "%d", a->repeatFrequency);
      gtk_entry_set_text(GTK_ENTRY(repeat_week_entry), temp);
      for (i=0; i<7; i++) {
	 gtk_toggle_button_set_active
	   (GTK_TOGGLE_BUTTON(toggle_button_repeat_days[i]),
	    /*a->repeatDays[(i + a->repeatWeekstart)%7]); */
	    a->repeatDays[i]);
      }
      gtk_notebook_set_page(GTK_NOTEBOOK(notebook), PAGE_WEEK);
      break;
    case repeatMonthlyByDate:
    case repeatMonthlyByDay:
      jpilot_logf(LOG_DEBUG, "repeat day=%d\n",a->repeatDay);
      if ((a->repeatForever)) {
	 gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
				      (check_button_mon_endon), FALSE);
      }
      else {
	 gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
				      (check_button_mon_endon), TRUE);
	 glob_endon_mon_tm.tm_mon = a->repeatEnd.tm_mon;
	 glob_endon_mon_tm.tm_mday = a->repeatEnd.tm_mday;
	 glob_endon_mon_tm.tm_year = a->repeatEnd.tm_year;
	 update_endon_button(glob_endon_mon_button, &glob_endon_mon_tm);
      }
      sprintf(temp, "%d", a->repeatFrequency);
      gtk_entry_set_text(GTK_ENTRY(repeat_mon_entry), temp);
      if (a->repeatType == repeatMonthlyByDay) {
	 gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
				      (toggle_button_repeat_mon_byday), TRUE);
      }
      if (a->repeatType == repeatMonthlyByDate) {
	 gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
				      (toggle_button_repeat_mon_bydate), TRUE);
      }
      gtk_notebook_set_page(GTK_NOTEBOOK(notebook), PAGE_MONTH);
      break;
    case repeatYearly:
      if ((a->repeatForever)) {
	 gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
				      (check_button_year_endon), FALSE);
      }
      else {
	 gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
				      (check_button_year_endon), TRUE);

	 glob_endon_year_tm.tm_mon = a->repeatEnd.tm_mon;
	 glob_endon_year_tm.tm_mday = a->repeatEnd.tm_mday;
	 glob_endon_year_tm.tm_year = a->repeatEnd.tm_year;
	 update_endon_button(glob_endon_year_button, &glob_endon_year_tm);
      }
      sprintf(temp, "%d", a->repeatFrequency);
      gtk_entry_set_text(GTK_ENTRY(repeat_year_entry), temp);

      gtk_notebook_set_page(GTK_NOTEBOOK(notebook), PAGE_YEAR);
      break;
    default:
      jpilot_logf(LOG_WARN, "unknown repeatType found in DatebookDB\n");
   }

   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(private_checkbox),
				ma->attrib & dlpRecAttrSecret);
     
   connect_changed_signals(CONNECT_SIGNALS);

   return;
}

void set_date_labels()
{
   struct tm now;
   char str[50];
   char datef[50];
   const char *svalue;
   long ivalue;

   now.tm_sec=0;
   now.tm_min=0;
   now.tm_hour=11;
   now.tm_isdst=-1;
   now.tm_wday=0;
   now.tm_yday=0;
   now.tm_mday = current_day;
   now.tm_mon = current_month;
   now.tm_year = current_year;
   mktime(&now);

   get_pref(PREF_LONGDATE, &ivalue, &svalue);
   if (svalue==NULL) {
      strcpy(datef, "%x");
   } else {
      sprintf(datef, "%%a., %s", svalue);
   }
   strftime(str, 50, datef, &now);
   gtk_label_set_text(GTK_LABEL(dow_label), str);
}

/*
 * When a calendar day is pressed
 */
void cb_cal_changed(GtkWidget *widget,
		    gpointer   data)
{
   int num;
   int y,m,d;
   int mon_changed;
#ifdef EASTER
   static int Easter=0;
#endif

   num = GPOINTER_TO_INT(data);

   if (num!=CAL_DAY_SELECTED) {
      return;
   }

   gtk_calendar_get_date(GTK_CALENDAR(main_calendar),&y,&m,&d);

   if (y < 1903) {
      y=1903;
      gtk_calendar_select_month(GTK_CALENDAR(main_calendar),
				m, 1903);
   }
   if (y > 2037) {
      y=2037;
      gtk_calendar_select_month(GTK_CALENDAR(main_calendar),
				m, 2037);
   }

   mon_changed=0;
   if (current_year!=y-1900) {
      current_year=y-1900;
      mon_changed=1;
   }
   if (current_month!=m) {
      current_month=m;
      mon_changed=1;
   }
   current_day=d;
   
   jpilot_logf(LOG_DEBUG, "cb_cal_changed, %02d/%02d/%02d\n", m,d,y);

   set_date_labels();
   /* */
   /*Easter Egg */
   /* */
#ifdef EASTER
   if (((current_day==29) && (current_month==7)) ||
       ((current_day==20) && (current_month==11))) {
      Easter++;
      if (Easter>4) {
	 Easter=0;
	 dialog_easter(current_day);
      }
   } else {
      Easter=0;
   }
#endif
   if (mon_changed) {
      highlight_days();
   }
   update_dayview_screen();
}

static void highlight_days()
{
   int bit, mask;
   int dow_int, ndim, i;
   const char *svalue;
   long ivalue;

   get_pref(PREF_HIGHLIGHT, &ivalue, &svalue);
   if (!ivalue) {
      return;
   }

   get_month_info(current_month, 1, current_year, &dow_int, &ndim);

   appointment_on_day_list(current_month, current_year, &mask);

   gtk_calendar_freeze(GTK_CALENDAR(main_calendar));

   for (i=1, bit=1; i<=ndim; i++, bit = bit<<1) {
      if (bit & mask) {
	 gtk_calendar_mark_day(GTK_CALENDAR(main_calendar), i);
      } else {
	 gtk_calendar_unmark_day(GTK_CALENDAR(main_calendar), i);
      }
   }
   gtk_calendar_thaw(GTK_CALENDAR(main_calendar));
}

static int datebook_find()
{
   int r, found_at, total_count;
   
   jpilot_logf(LOG_DEBUG, "datebook_find(), glob_find_id = %d\n", glob_find_id);
   if (glob_find_id) {
      r = clist_find_id(clist1,
			glob_find_id,
			&found_at,
			&total_count);
      if (r) {
	 if (total_count == 0) {
	    total_count = 1;
	 }
	 if (!gtk_clist_row_is_visible(GTK_CLIST(clist1), found_at)) {
	    move_scrolled_window_hack(scrolled_window,
				      (float)found_at/(float)total_count);
	 }
	 jpilot_logf(LOG_DEBUG, "datebook_find(), selecting row %d\n", found_at);
	 gtk_clist_select_row(GTK_CLIST(clist1), found_at, 1);
	 cb_clist1_selection(clist1, found_at, 1, (GdkEventButton *)455, "");
      }
      glob_find_id = 0;
   }
   return 0;
}

int datebook_refresh(int first)
{
   init();

   if (glob_find_id) {
      if (GTK_IS_WINDOW(window_date_cats)) {
	 cb_date_cats_all_none(NULL, GINT_TO_POINTER(1));
      } else {
	 datebook_category = 0xFFFF;
      }
   }

   if (first) {
      gtk_calendar_select_day(GTK_CALENDAR(main_calendar), current_day);
   } else {
      gtk_calendar_select_month(GTK_CALENDAR(main_calendar),
				current_month, current_year+1900);
      gtk_calendar_select_day(GTK_CALENDAR(main_calendar), current_day);
   }

   update_dayview_screen();
   highlight_days();

   datebook_find();
   return 0;
}

#define PRESSED_P            100
#define PRESSED_A            101
#define PRESSED_TAB_OR_MINUS 102

static void entry_key_pressed(int next_digit, int begin_or_end)
{
   struct tm t1, t2;
   struct tm *Pt;

   bzero(&t1, sizeof(t1));
   bzero(&t2, sizeof(t2));

   if (begin_or_end) {
      Pt = &end_date;
   } else {
      Pt = &begin_date;
   }

   if ((next_digit>=0) && (next_digit<=9)) {
      Pt->tm_hour = ((Pt->tm_hour)*10 + (Pt->tm_min)/10)%100;
      Pt->tm_min = ((Pt->tm_min)*10)%100 + next_digit;
   }
   if ((next_digit==PRESSED_P) && ((Pt->tm_hour)<12)) {
      (Pt->tm_hour) += 12;
   }
   if ((next_digit==PRESSED_A) && ((Pt->tm_hour)>11)) {
      (Pt->tm_hour) -= 12;
   }
   /* Don't let the first digit exceed 2 */
   if ((int)(Pt->tm_hour/10) > 2) {
      Pt->tm_hour -= ((int)(Pt->tm_hour/10)-2)*10;
   }
   /* Don't let the hour be > 23 */
   if (Pt->tm_hour > 23) {
      Pt->tm_hour = 23;
   }

   set_begin_end_labels(&begin_date, &end_date);
}

static gboolean
cb_entry_key_pressed(GtkWidget *widget, GdkEventKey *event,
		     gpointer data)
{
   int digit=-1;

   jpilot_logf(LOG_DEBUG, "cb_entry_key_pressed key = %d\n", event->keyval);

   if ((event->keyval >= GDK_0) && (event->keyval <= GDK_9)) {
      digit = (event->keyval)-GDK_0;
   }
   if ((event->keyval >= GDK_KP_0) && (event->keyval <= GDK_KP_9)) {
      digit = (event->keyval)-GDK_KP_0;
   }
   if ((event->keyval == GDK_P) || (event->keyval == GDK_p)) {
      digit = PRESSED_P;
   }
   if ((event->keyval == GDK_A) || (event->keyval == GDK_a)) {
      digit = PRESSED_A;
   }
   if ((event->keyval == GDK_KP_Subtract) || (event->keyval == GDK_minus)
       || (event->keyval == GDK_Tab)) {
      digit = PRESSED_TAB_OR_MINUS;
   }

   if (digit==PRESSED_TAB_OR_MINUS) {
      if (widget==begin_time_entry) {
	 gtk_widget_grab_focus(GTK_WIDGET(end_time_entry));
      }
      if (widget==end_time_entry) {
	 gtk_widget_grab_focus(GTK_WIDGET(begin_time_entry));
      }
   }

   if (digit>=0) {
      gtk_signal_emit_stop_by_name(GTK_OBJECT(widget), "key_press_event"); 
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_button_notime), FALSE);
      if (widget==begin_time_entry) {
	 entry_key_pressed(digit, 0);
      }
      if (widget==end_time_entry) {
	 entry_key_pressed(digit, 1);
      }
      return TRUE;
   }
   return FALSE; 
}

static gboolean
cb_key_pressed(GtkWidget *widget, GdkEventKey *event,
	       gpointer next_widget) 
{
  
   if (event->keyval == GDK_Tab) { 
      if (gtk_text_get_point(GTK_TEXT(widget)) ==
	  gtk_text_get_length(GTK_TEXT(widget))) {
	 gtk_signal_emit_stop_by_name(GTK_OBJECT(widget), "key_press_event"); 
	 gtk_widget_grab_focus(GTK_WIDGET(next_widget));
	 return TRUE;
      }
   }
   return FALSE; 
}

static gboolean
cb_keyboard(GtkWidget *widget, GdkEventKey *event, gpointer *p) 
{
   struct tm day;
   int up, down;
   
   up = down = 0;
   switch (event->keyval) {
    case GDK_Page_Up:
    case GDK_KP_Page_Up:
      up=1;
    case GDK_Page_Down:
    case GDK_KP_Page_Down:
      down=1;
      gtk_signal_emit_stop_by_name(GTK_OBJECT(widget), "key_press_event");

      bzero(&day, sizeof(day));
      day.tm_year = current_year;
      day.tm_mon = current_month;
      day.tm_mday = current_day;
      day.tm_hour = 12;
      day.tm_min = 0;

      if (up) {
	 sub_days_from_date(&day, 1);
      } else {
	 add_days_to_date(&day, 1);
      }

      current_year = day.tm_year;
      current_month = day.tm_mon;
      current_day = day.tm_mday;

      gtk_calendar_select_month(GTK_CALENDAR(main_calendar), current_month, current_year+1900);
      gtk_calendar_select_day(GTK_CALENDAR(main_calendar), current_day);
      
      return TRUE;
   }
   return FALSE; 
}

int datebook_gui_cleanup()
{
   GtkWidget *widget;
   
   connect_changed_signals(DISCONNECT_SIGNALS);
   set_pref(PREF_DATEBOOK_PANE, GTK_PANED(pane)->handle_xpos);
   if (GTK_IS_WIDGET(window_date_cats)) {
      gtk_widget_destroy(window_date_cats);
   }
   /* Remove the accelerators */
   for (widget=main_calendar; widget; widget=widget->parent) {
      if (GTK_IS_WINDOW(widget)) {
	 gtk_accel_group_detach(accel_group, GTK_OBJECT(widget));
	 gtk_signal_disconnect_by_func(GTK_OBJECT(widget),
				       GTK_SIGNAL_FUNC(cb_keyboard), NULL);
	 break;
      }
   }

   return 0;
}

static void connect_changed_signals(int con_or_dis)
{
   int i;
   static int connected=0;
   
   /* CONNECT */
   if ((con_or_dis==CONNECT_SIGNALS) && (!connected)) {
      connected=1;
      gtk_signal_connect(GTK_OBJECT(radio_button_alarm_min), "toggled",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);     
      gtk_signal_connect(GTK_OBJECT(radio_button_alarm_hour), "toggled",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);     
      gtk_signal_connect(GTK_OBJECT(radio_button_alarm_day), "toggled",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);     
     
      gtk_signal_connect(GTK_OBJECT(check_button_alarm), "toggled",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);

      gtk_signal_connect(GTK_OBJECT(check_button_notime), "toggled",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);

      gtk_signal_connect(GTK_OBJECT(begin_date_button), "pressed",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);

      gtk_signal_connect(GTK_OBJECT(begin_time_entry), "changed",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_connect(GTK_OBJECT(end_time_entry), "changed",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);
   
      gtk_signal_connect(GTK_OBJECT(text1), "changed",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_connect(GTK_OBJECT(text2), "changed",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);
   
      gtk_signal_connect(GTK_OBJECT(notebook), "switch-page",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);
 
      gtk_signal_connect(GTK_OBJECT(repeat_day_entry), "changed",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_connect(GTK_OBJECT(repeat_week_entry), "changed",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_connect(GTK_OBJECT(repeat_mon_entry), "changed",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_connect(GTK_OBJECT(repeat_year_entry), "changed",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);

      gtk_signal_connect(GTK_OBJECT(check_button_day_endon), "toggled",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_connect(GTK_OBJECT(check_button_week_endon), "toggled",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_connect(GTK_OBJECT(check_button_mon_endon), "toggled",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_connect(GTK_OBJECT(check_button_year_endon), "toggled",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);

      gtk_signal_connect(GTK_OBJECT(glob_endon_day_button), "pressed",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_connect(GTK_OBJECT(glob_endon_week_button), "pressed",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_connect(GTK_OBJECT(glob_endon_mon_button), "pressed",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_connect(GTK_OBJECT(glob_endon_year_button), "pressed",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);

      gtk_signal_connect(GTK_OBJECT(toggle_button_repeat_mon_byday), "toggled",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_connect(GTK_OBJECT(toggle_button_repeat_mon_bydate), "toggled",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_connect(GTK_OBJECT(private_checkbox), "toggled",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);

      for (i=0; i<7; i++) {
	 gtk_signal_connect(GTK_OBJECT(toggle_button_repeat_days[i]), "toggled",
			    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      }
   }

   /* DISCONNECT */
   if ((con_or_dis==DISCONNECT_SIGNALS) && (connected)) {
      connected=0;
      gtk_signal_disconnect_by_func(GTK_OBJECT(radio_button_alarm_min),
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);     
      gtk_signal_disconnect_by_func(GTK_OBJECT(radio_button_alarm_hour),
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);     
      gtk_signal_disconnect_by_func(GTK_OBJECT(radio_button_alarm_day),
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);     
     
      gtk_signal_disconnect_by_func(GTK_OBJECT(check_button_alarm),
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      
      gtk_signal_disconnect_by_func(GTK_OBJECT(check_button_notime),
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      
      gtk_signal_disconnect_by_func(GTK_OBJECT(begin_date_button),
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      
      gtk_signal_disconnect_by_func(GTK_OBJECT(begin_time_entry),
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_disconnect_by_func(GTK_OBJECT(end_time_entry),
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      
      gtk_signal_disconnect_by_func(GTK_OBJECT(text1),
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_disconnect_by_func(GTK_OBJECT(text2),
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      
      gtk_signal_disconnect_by_func(GTK_OBJECT(notebook),
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);

      gtk_signal_disconnect_by_func(GTK_OBJECT(repeat_day_entry),
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_disconnect_by_func(GTK_OBJECT(repeat_week_entry),
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_disconnect_by_func(GTK_OBJECT(repeat_mon_entry),
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_disconnect_by_func(GTK_OBJECT(repeat_year_entry),
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);

      gtk_signal_disconnect_by_func(GTK_OBJECT(check_button_day_endon),
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_disconnect_by_func(GTK_OBJECT(check_button_week_endon),
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_disconnect_by_func(GTK_OBJECT(check_button_mon_endon),
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_disconnect_by_func(GTK_OBJECT(check_button_year_endon),
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);

      gtk_signal_disconnect_by_func(GTK_OBJECT(glob_endon_day_button),
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_disconnect_by_func(GTK_OBJECT(glob_endon_week_button),
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_disconnect_by_func(GTK_OBJECT(glob_endon_mon_button),
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_disconnect_by_func(GTK_OBJECT(glob_endon_year_button),
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);

      gtk_signal_disconnect_by_func(GTK_OBJECT(toggle_button_repeat_mon_byday),
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_disconnect_by_func(GTK_OBJECT(toggle_button_repeat_mon_bydate),
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_disconnect_by_func(GTK_OBJECT(private_checkbox),
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);

      for (i=0; i<7; i++) {
	 gtk_signal_disconnect_by_func(GTK_OBJECT(toggle_button_repeat_days[i]),
				       GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      }
   }
}

int datebook_gui(GtkWidget *vbox, GtkWidget *hbox)
{
   extern GtkWidget *glob_date_label;
   extern gint glob_date_timer_tag;
   GtkWidget *widget;
   GtkWidget *button;
   GtkWidget *separator;
   GtkWidget *label;
   GtkWidget *vscrollbar;
   GtkWidget *vbox1, *vbox2;
   GtkWidget *hbox2;
   GtkWidget *hbox_temp;
   GtkWidget *vbox_repeat_day;
   GtkWidget *hbox_repeat_day1;
   GtkWidget *hbox_repeat_day2;
   GtkWidget *vbox_repeat_week;
   GtkWidget *hbox_repeat_week1;
   GtkWidget *hbox_repeat_week2;
   GtkWidget *hbox_repeat_week3;
   GtkWidget *vbox_repeat_mon;
   GtkWidget *hbox_repeat_mon1;
   GtkWidget *hbox_repeat_mon2;
   GtkWidget *hbox_repeat_mon3;
   GtkWidget *vbox_repeat_year;
   GtkWidget *hbox_repeat_year1;
   GtkWidget *hbox_repeat_year2;
   GtkWidget *notebook_tab1;
   GtkWidget *notebook_tab2;
   GtkWidget *notebook_tab3;
   GtkWidget *notebook_tab4;
   GtkWidget *notebook_tab5;

   GSList *group;
   char *titles[]={"","","","",""};
   
   long fdow;
   const char *str_fdow;
   long ivalue;
#ifdef USE_DB3
   long use_db3_tags;
#endif
   const char *svalue;
   
   time_t ltime;
   struct tm *now;
   
   int i, j;
#define MAX_STR 100
   char days2[12];
   char *days[] = {
      gettext_noop("Su"),
      gettext_noop("Mo"),
      gettext_noop("Tu"),
      gettext_noop("We"),
      gettext_noop("RTh"),
      gettext_noop("Fr"),
      gettext_noop("Sa"),
      gettext_noop("Su")
   };

   init();

   pane = gtk_hpaned_new();
   get_pref(PREF_DATEBOOK_PANE, &ivalue, &svalue);
   gtk_paned_set_position(GTK_PANED(pane), ivalue + 2);

   gtk_box_pack_start(GTK_BOX(hbox), pane, TRUE, TRUE, 5);

   hbox2 = gtk_hbox_new(FALSE, 0);
   vbox1 = gtk_vbox_new(FALSE, 0);
   vbox2 = gtk_vbox_new(FALSE, 0);

   gtk_paned_pack1(GTK_PANED(pane), vbox1, TRUE, TRUE);
   gtk_paned_pack2(GTK_PANED(pane), vbox2, TRUE, TRUE);

   separator = gtk_hseparator_new();
   gtk_box_pack_start(GTK_BOX(vbox1), separator, FALSE, FALSE, 5);

   /*Make the Today is: label */
   time(&ltime);
   now = localtime(&ltime);
   
   glob_date_label = gtk_label_new(" ");
   gtk_box_pack_start(GTK_BOX(vbox1), glob_date_label, FALSE, FALSE, 0);
   timeout_date(NULL);
   glob_date_timer_tag = gtk_timeout_add(CLOCK_TICK, timeout_date, NULL);


   separator = gtk_hseparator_new();
   gtk_box_pack_start(GTK_BOX(vbox1), separator, FALSE, FALSE, 5);

   gtk_box_pack_start(GTK_BOX(vbox1), hbox2, FALSE, FALSE, 3);


   /* Make the main calendar */
   hbox_temp = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox1), hbox_temp, FALSE, FALSE, 0);

   get_pref(PREF_FDOW, &fdow, NULL);
   if (fdow==1) {
      fdow = GTK_CALENDAR_WEEK_START_MONDAY;
   } else {
      fdow=0;
   }
   main_calendar = gtk_calendar_new();
   gtk_calendar_display_options(GTK_CALENDAR(main_calendar),
				GTK_CALENDAR_SHOW_HEADING |
				GTK_CALENDAR_SHOW_DAY_NAMES |
				GTK_CALENDAR_SHOW_WEEK_NUMBERS | fdow);
   gtk_signal_connect(GTK_OBJECT(main_calendar),
		      "day_selected", cb_cal_changed,
		      GINT_TO_POINTER(CAL_DAY_SELECTED));
   gtk_box_pack_start(GTK_BOX(hbox_temp), main_calendar, FALSE, FALSE, 0);
   

   /* Make accelerators for some buttons window */
   accel_group=NULL;
   for (widget=vbox; widget; widget=widget->parent) {
      if (GTK_IS_WINDOW(widget)) {
	 accel_group = gtk_accel_group_new();
	 gtk_accel_group_attach(accel_group, GTK_OBJECT(widget));

	 gtk_signal_connect(GTK_OBJECT(widget), "key_press_event",
			    GTK_SIGNAL_FUNC(cb_keyboard), NULL);
	 break;
      }
   }

   /*Make Weekview button */
   button = gtk_button_new_with_label(_("Week"));
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_weekview), NULL);
   gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);
   gtk_widget_show(button);

   gtk_label_set_pattern(GTK_LABEL(GTK_BIN(button)->child), "_");
   gtk_widget_add_accelerator(GTK_WIDGET(button), "clicked", accel_group, 'W',
			      GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);

   /*Make Monthview button */
   button = gtk_button_new_with_label(_("Month"));
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_monthview), NULL);
   gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);
   gtk_widget_show(button);

   gtk_label_set_pattern(GTK_LABEL(GTK_BIN(button)->child), "_");
   gtk_widget_add_accelerator(GTK_WIDGET(button), "clicked", accel_group, 'M',
			      GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
					
#ifdef USE_DB3
   get_pref(PREF_USE_DB3, &use_db3_tags, NULL);

   if (use_db3_tags) {
      /*Make Category button */
      button = gtk_button_new_with_label(_("Cats"));
      gtk_signal_connect(GTK_OBJECT(button), "clicked",
			 GTK_SIGNAL_FUNC(cb_date_cats), NULL);
      gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);
      gtk_widget_show(button);
   }
#endif
   
   /* Make the DOW label */
   dow_label = gtk_label_new("");
   gtk_box_pack_start(GTK_BOX(vbox1), dow_label, FALSE, FALSE, 0);

   /* create a new scrolled window. */
   scrolled_window = gtk_scrolled_window_new(NULL, NULL);

   gtk_container_set_border_width(GTK_CONTAINER(scrolled_window), 0);

   /* the policy is one of GTK_POLICY AUTOMATIC, or GTK_POLICY_ALWAYS.
    * GTK_POLICY_AUTOMATIC will automatically decide whether you need
    * scrollbars, whereas GTK_POLICY_ALWAYS will always leave the scrollbars
    * there.  The first one is the horizontal scrollbar, the second,
    * the vertical. */
   gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
   /* The dialog window is created with a vbox packed into it. */
   gtk_box_pack_start(GTK_BOX(vbox1), scrolled_window, TRUE, TRUE, 0);

#ifdef USE_DB3
   get_pref(PREF_USE_DB3, &use_db3_tags, NULL);
   if (use_db3_tags) {
      clist1 = gtk_clist_new_with_titles(5, titles);
   } else {
      clist1 = gtk_clist_new_with_titles(4, titles);
   }
#else 
   clist1 = gtk_clist_new_with_titles(4, titles);
#endif

   gtk_signal_connect(GTK_OBJECT(clist1), "select_row",
		      GTK_SIGNAL_FUNC(cb_clist1_selection),
		      NULL);

   gtk_clist_set_selection_mode(GTK_CLIST(clist1), GTK_SELECTION_BROWSE);

   /* This is a lame way to figure out the column size */
   gtk_clist_set_column_title(GTK_CLIST(clist1), 0, "00:00-00:00AM");
   gtk_clist_column_titles_show(GTK_CLIST(clist1));
/*#if defined(WITH_JAPANESE)*/
   /*gtk_clist_set_column_width(GTK_CLIST(clist1), 0, 110);*/
/*#else*/
   gtk_clist_columns_autosize(GTK_CLIST(clist1));
   gtk_clist_set_column_width(GTK_CLIST(clist1), 0, 
			      gtk_clist_optimal_column_width(GTK_CLIST(clist1), 0));
/*#endif*/
   /* gtk_clist_column_titles_hide(GTK_CLIST(clist1)); */
   gtk_clist_set_column_title(GTK_CLIST(clist1), 0, _("Time"));
   gtk_clist_set_column_title(GTK_CLIST(clist1), 1, _("Appointment"));

   gtk_clist_set_column_width(GTK_CLIST(clist1), 1, 180);
   gtk_clist_set_column_width(GTK_CLIST(clist1), 2, 11);
   gtk_clist_set_column_width(GTK_CLIST(clist1), 3, 16);
#ifdef USE_DB3
   if (use_db3_tags) {
      gtk_clist_set_column_width(GTK_CLIST(clist1), 4, 14);
   }
#endif

      /* gtk_clist_set_column_justification(GTK_CLIST(clist1), 0, GTK_JUSTIFY_FILL);*/
   gtk_widget_set_usize(GTK_WIDGET(clist1), 10, 10);
   gtk_widget_set_usize(GTK_WIDGET(scrolled_window), 10, 10);
   
   gtk_container_add(GTK_CONTAINER(scrolled_window), GTK_WIDGET(clist1));
   /*gtk_clist_set_sort_column (GTK_CLIST(clist1), 0); */
   /*gtk_clist_set_auto_sort(GTK_CLIST(clist1), TRUE); */

   /* */
   /* The right hand part of the main window follows: */
   /* */
   hbox_temp = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, FALSE, FALSE, 0);

   
   /*Add record modification buttons on right side */
   /* Create "event" button */

   /* Create "Delete" button */
   button = gtk_button_new_with_label(_("Delete"));
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_delete_appt),
		      GINT_TO_POINTER(DELETE_FLAG));
   gtk_box_pack_start(GTK_BOX(hbox_temp), button, TRUE, TRUE, 0);

   /* Create "Add It" button */
   button = gtk_button_new_with_label(_("Copy"));
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_add_new_record), 
		      GINT_TO_POINTER(NEW_FLAG));
   gtk_box_pack_start(GTK_BOX(hbox_temp), button, TRUE, TRUE, 0);
   
   /* Create "New" button */
   new_record_button = gtk_button_new_with_label(_("New Record"));
   gtk_signal_connect(GTK_OBJECT(new_record_button), "clicked",
		      GTK_SIGNAL_FUNC(cb_add_new_record),
		      GINT_TO_POINTER(CLEAR_FLAG));
   gtk_box_pack_start(GTK_BOX(hbox_temp), new_record_button, TRUE, TRUE, 0);

   /* Create "Add Record" button */
   add_record_button = gtk_button_new_with_label(_("Add Record"));
   gtk_signal_connect(GTK_OBJECT(add_record_button), "clicked",
		      GTK_SIGNAL_FUNC(cb_add_new_record),
		      GINT_TO_POINTER(NEW_FLAG));
   gtk_box_pack_start(GTK_BOX(hbox_temp), add_record_button, TRUE, TRUE, 0);
   gtk_widget_set_name(GTK_WIDGET(GTK_LABEL(GTK_BIN(add_record_button)->child)),
		       "label_high");

   /* Create "apply changes" button */
   apply_record_button = gtk_button_new_with_label(_("Apply Changes"));
   gtk_signal_connect(GTK_OBJECT(apply_record_button), "clicked",
		      GTK_SIGNAL_FUNC(cb_add_new_record),
		      GINT_TO_POINTER(MODIFY_FLAG));
   gtk_box_pack_start(GTK_BOX(hbox_temp), apply_record_button, TRUE, TRUE, 0);
   gtk_widget_set_name(GTK_WIDGET(GTK_LABEL(GTK_BIN(apply_record_button)->child)),
		       "label_high");


   /*start details */

   separator = gtk_hseparator_new();
   gtk_box_pack_start(GTK_BOX(vbox2), separator, FALSE, FALSE, 5);

   /*The checkbox for Alarm */
   hbox_alarm1 = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox2), hbox_alarm1, FALSE, FALSE, 0);

   check_button_alarm = gtk_check_button_new_with_label (_("Alarm"));
   gtk_box_pack_start(GTK_BOX(hbox_alarm1), check_button_alarm, FALSE, FALSE, 5);
   gtk_signal_connect(GTK_OBJECT(check_button_alarm), "clicked",
		      GTK_SIGNAL_FUNC(cb_check_button_alarm), NULL);

   hbox_alarm2 = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(hbox_alarm1), hbox_alarm2, FALSE, FALSE, 0);
   /*gtk_widget_show(hbox_alarm2); */

   /*Units entry for alarm */
   units_entry = gtk_entry_new_with_max_length(2);
   gtk_widget_set_usize(units_entry, 30, 0);
   gtk_box_pack_start(GTK_BOX(hbox_alarm2), units_entry, FALSE, FALSE, 0);


   radio_button_alarm_min = gtk_radio_button_new_with_label(NULL, _("Minutes"));

   group = NULL;
   group = gtk_radio_button_group(GTK_RADIO_BUTTON(radio_button_alarm_min));
   radio_button_alarm_hour = gtk_radio_button_new_with_label(group, _("Hours"));
   group = gtk_radio_button_group(GTK_RADIO_BUTTON(radio_button_alarm_hour));
   radio_button_alarm_day = gtk_radio_button_new_with_label(group, _("Days"));
   
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_button_alarm_min), TRUE);
   
   gtk_box_pack_start(GTK_BOX(hbox_alarm2),
		      radio_button_alarm_min, FALSE, FALSE, 0);
   gtk_box_pack_start(GTK_BOX(hbox_alarm2),
		      radio_button_alarm_hour, FALSE, FALSE, 0);
   gtk_box_pack_start(GTK_BOX(hbox_alarm2),
		      radio_button_alarm_day, FALSE, FALSE, 0);

   /*
    * Begin begin and end dates
    */
   /* The checkbox for timeless events */
   hbox_temp = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, FALSE, FALSE, 0);

   check_button_notime = gtk_check_button_new_with_label(
      _("This Event has no particular time"));
   gtk_box_pack_start(GTK_BOX(hbox_temp), check_button_notime, FALSE, FALSE, 5);
   gtk_signal_connect(GTK_OBJECT(check_button_notime), "clicked",
		      GTK_SIGNAL_FUNC(cb_check_button_notime), NULL);

   hbox_temp = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, FALSE, FALSE, 0);

   label = gtk_label_new(_("Starts on: "));
   gtk_box_pack_start(GTK_BOX(hbox_temp), label, FALSE, FALSE, 5);

   begin_date_button = gtk_button_new_with_label("");
   gtk_box_pack_start(GTK_BOX(hbox_temp), begin_date_button, FALSE, FALSE, 5);
   gtk_signal_connect(GTK_OBJECT(begin_date_button), "clicked",
		      GTK_SIGNAL_FUNC(cb_cal_dialog),
		      GINT_TO_POINTER(BEGIN_DATE_BUTTON));
   
   hbox_temp = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, FALSE, FALSE, 5);

   label = gtk_label_new(_("Time:"));
   gtk_box_pack_start(GTK_BOX(hbox_temp), label, FALSE, FALSE, 5);

   begin_time_entry = gtk_entry_new_with_max_length(8);
   gtk_box_pack_start(GTK_BOX(hbox_temp), begin_time_entry, FALSE, FALSE, 0);
   gtk_entry_set_editable(GTK_ENTRY(begin_time_entry), FALSE);

   label = gtk_label_new("-");
   gtk_box_pack_start(GTK_BOX(hbox_temp), label, FALSE, FALSE, 5);

   end_time_entry = gtk_entry_new_with_max_length(8);
   gtk_box_pack_start(GTK_BOX(hbox_temp), end_time_entry, FALSE, FALSE, 0);
   gtk_entry_set_editable(GTK_ENTRY(end_time_entry), FALSE);

   /*Private check box */
   private_checkbox = gtk_check_button_new_with_label(_("Private"));
   gtk_box_pack_end(GTK_BOX(hbox_temp), private_checkbox, FALSE, FALSE, 0);

   gtk_widget_set_usize(GTK_WIDGET(begin_time_entry), 80, 0);
   gtk_widget_set_usize(GTK_WIDGET(end_time_entry), 80, 0);

   gtk_signal_connect(GTK_OBJECT(begin_time_entry), "key_press_event",
		      GTK_SIGNAL_FUNC(cb_entry_key_pressed), NULL);
   gtk_signal_connect(GTK_OBJECT(end_time_entry), "key_press_event",
		      GTK_SIGNAL_FUNC(cb_entry_key_pressed), NULL);
   
   clear_begin_end_labels();

   /*End begin and end dates */

   /*Text 1 */
   hbox_temp = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, TRUE, TRUE, 0);

   text1 = gtk_text_new(NULL, NULL);
   gtk_text_set_word_wrap(GTK_TEXT(text1), TRUE);
   vscrollbar = gtk_vscrollbar_new(GTK_TEXT(text1)->vadj);
   gtk_box_pack_start(GTK_BOX(hbox_temp), text1, TRUE, TRUE, 0);
   gtk_box_pack_start(GTK_BOX(hbox_temp), vscrollbar, FALSE, FALSE, 0);
   /* gtk_widget_set_usize(GTK_WIDGET(text1), 255, 50); */
   gtk_widget_set_usize(GTK_WIDGET(text1), 10, 10);
   
   /*Text 2 */
   hbox_temp = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, TRUE, TRUE, 0);


   text2 = gtk_text_new(NULL, NULL);
   gtk_text_set_word_wrap(GTK_TEXT(text2), TRUE);
   vscrollbar = gtk_vscrollbar_new(GTK_TEXT(text2)->vadj);
   gtk_box_pack_start(GTK_BOX(hbox_temp), text2, TRUE, TRUE, 0);
   gtk_box_pack_start(GTK_BOX(hbox_temp), vscrollbar, FALSE, FALSE, 0);
   gtk_widget_set_usize(GTK_WIDGET(text2), 10, 10);

   hbox_temp = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, FALSE, FALSE, 2);

   /*Add the notebook for repeat types */
   notebook = gtk_notebook_new();
   gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_TOP);
   gtk_box_pack_start(GTK_BOX(hbox_temp), notebook, FALSE, FALSE, 5);
   /*labels for notebook */
   notebook_tab1 = gtk_label_new(_("None"));
   notebook_tab2 = gtk_label_new(_("Day"));
   notebook_tab3 = gtk_label_new(_("Week"));
   notebook_tab4 = gtk_label_new(_("Month"));
   notebook_tab5 = gtk_label_new(_("Year"));

  
   /*no repeat page for notebook */
   label = gtk_label_new(_("This event will not repeat"));
   gtk_notebook_append_page(GTK_NOTEBOOK(notebook), label, notebook_tab1);
   /*end no repeat page */

   /*Day Repeat page for notebook */
   vbox_repeat_day = gtk_vbox_new(FALSE, 0);
   hbox_repeat_day1 = gtk_hbox_new(FALSE, 0);
   hbox_repeat_day2 = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox_repeat_day), hbox_repeat_day1, FALSE, FALSE, 2);
   gtk_box_pack_start(GTK_BOX(vbox_repeat_day), hbox_repeat_day2, FALSE, FALSE, 2);
   label = gtk_label_new(_("Frequency is Every"));
   gtk_box_pack_start(GTK_BOX(hbox_repeat_day1), label, FALSE, FALSE, 2);
   repeat_day_entry = gtk_entry_new_with_max_length(2);
   gtk_widget_set_usize(repeat_day_entry, 30, 0);
   gtk_box_pack_start(GTK_BOX(hbox_repeat_day1), repeat_day_entry, FALSE, FALSE, 0);
   label = gtk_label_new(_("Day(s)"));
   gtk_box_pack_start(GTK_BOX(hbox_repeat_day1), label, FALSE, FALSE, 2);
   /*checkbutton */
   check_button_day_endon = gtk_check_button_new_with_label (_("End on"));
   gtk_signal_connect(GTK_OBJECT(check_button_day_endon), "clicked",
		      GTK_SIGNAL_FUNC(cb_check_button_endon),
		      GINT_TO_POINTER(PAGE_DAY));
   gtk_box_pack_start(GTK_BOX(hbox_repeat_day2), check_button_day_endon, FALSE, FALSE, 0);
   gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			    vbox_repeat_day, notebook_tab2);

   glob_endon_day_button = gtk_button_new_with_label(_("No Date"));
   gtk_box_pack_start(GTK_BOX(hbox_repeat_day2),
		      glob_endon_day_button, FALSE, FALSE, 0);
   gtk_signal_connect(GTK_OBJECT(glob_endon_day_button), "clicked",
		      GTK_SIGNAL_FUNC(cb_cal_dialog),
		      GINT_TO_POINTER(PAGE_DAY));

   /*The Week page */
   vbox_repeat_week = gtk_vbox_new(FALSE, 0);
   hbox_repeat_week1 = gtk_hbox_new(FALSE, 0);
   hbox_repeat_week2 = gtk_hbox_new(FALSE, 0);
   hbox_repeat_week3 = gtk_hbox_new(FALSE, 0);
   gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox_repeat_week, notebook_tab3);
   gtk_box_pack_start(GTK_BOX(vbox_repeat_week), hbox_repeat_week1, FALSE, FALSE, 2);
   gtk_box_pack_start(GTK_BOX(vbox_repeat_week), hbox_repeat_week2, FALSE, FALSE, 2);
   gtk_box_pack_start(GTK_BOX(vbox_repeat_week), hbox_repeat_week3, FALSE, FALSE, 2);
   label = gtk_label_new(_("Frequency is Every"));
   gtk_box_pack_start(GTK_BOX(hbox_repeat_week1), label, FALSE, FALSE, 2);
   repeat_week_entry = gtk_entry_new_with_max_length(2);
   gtk_widget_set_usize(repeat_week_entry, 30, 0);
   gtk_box_pack_start(GTK_BOX(hbox_repeat_week1), repeat_week_entry, FALSE, FALSE, 0);
   label = gtk_label_new(_("Week(s)"));
   gtk_box_pack_start(GTK_BOX(hbox_repeat_week1), label, FALSE, FALSE, 2);
   /*checkbutton */
   check_button_week_endon = gtk_check_button_new_with_label(_("End on"));
   gtk_signal_connect(GTK_OBJECT(check_button_week_endon), "clicked",
		      GTK_SIGNAL_FUNC(cb_check_button_endon),
		      GINT_TO_POINTER(PAGE_WEEK));
   gtk_box_pack_start(GTK_BOX(hbox_repeat_week2), check_button_week_endon, FALSE, FALSE, 0);

   glob_endon_week_button = gtk_button_new_with_label(_("No Date"));
   gtk_box_pack_start(GTK_BOX(hbox_repeat_week2),
		      glob_endon_week_button, FALSE, FALSE, 0);
   gtk_signal_connect(GTK_OBJECT(glob_endon_week_button), "clicked",
		      GTK_SIGNAL_FUNC(cb_cal_dialog),
		      GINT_TO_POINTER(PAGE_WEEK));

   label = gtk_label_new (_("Repeat on Days:"));
   gtk_box_pack_start(GTK_BOX(hbox_repeat_week3), label, FALSE, FALSE, 0);

   get_pref(PREF_FDOW, &fdow, &str_fdow);

   for (i=0, j=fdow; i<7; i++, j++) {
      if (j>6) {
	 j=0;
      }
      strncpy(days2, _(days[j]), 10);
      days2[10]='\0';
      /* If no translation occured then use the first letter only */
      if (!strcmp(days2, days[j])) {
	 days2[0]=days[j][0];
	 days2[1]='\0';
      }
      
      toggle_button_repeat_days[j] =
	gtk_toggle_button_new_with_label(days2);
      gtk_box_pack_start(GTK_BOX(hbox_repeat_week3),
			 toggle_button_repeat_days[j], FALSE, FALSE, 0);
   }

   /*end week page */

   
   /*The Month page */
   vbox_repeat_mon = gtk_vbox_new(FALSE, 0);
   hbox_repeat_mon1 = gtk_hbox_new(FALSE, 0);
   hbox_repeat_mon2 = gtk_hbox_new(FALSE, 0);
   hbox_repeat_mon3 = gtk_hbox_new(FALSE, 0);
   gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox_repeat_mon, notebook_tab4);
   gtk_box_pack_start(GTK_BOX(vbox_repeat_mon), hbox_repeat_mon1, FALSE, FALSE, 2);
   gtk_box_pack_start(GTK_BOX(vbox_repeat_mon), hbox_repeat_mon2, FALSE, FALSE, 2);
   gtk_box_pack_start(GTK_BOX(vbox_repeat_mon), hbox_repeat_mon3, FALSE, FALSE, 2);
   label = gtk_label_new(_("Frequency is Every"));
   gtk_box_pack_start(GTK_BOX(hbox_repeat_mon1), label, FALSE, FALSE, 2);
   repeat_mon_entry = gtk_entry_new_with_max_length(2);
   gtk_widget_set_usize(repeat_mon_entry, 30, 0);
   gtk_box_pack_start(GTK_BOX(hbox_repeat_mon1), repeat_mon_entry, FALSE, FALSE, 0);
   label = gtk_label_new (_("Month(s)"));
   gtk_box_pack_start(GTK_BOX(hbox_repeat_mon1), label, FALSE, FALSE, 2);
   /*checkbutton */
   check_button_mon_endon = gtk_check_button_new_with_label (_("End on"));
   gtk_signal_connect(GTK_OBJECT(check_button_mon_endon), "clicked",
		      GTK_SIGNAL_FUNC(cb_check_button_endon),
		      GINT_TO_POINTER(PAGE_MONTH));
   gtk_box_pack_start(GTK_BOX(hbox_repeat_mon2), check_button_mon_endon, FALSE, FALSE, 0);

   glob_endon_mon_button = gtk_button_new_with_label(_("No Date"));
   gtk_box_pack_start(GTK_BOX(hbox_repeat_mon2),
		      glob_endon_mon_button, FALSE, FALSE, 0);
   gtk_signal_connect(GTK_OBJECT(glob_endon_mon_button), "clicked",
		      GTK_SIGNAL_FUNC(cb_cal_dialog),
		      GINT_TO_POINTER(PAGE_MONTH));


   label = gtk_label_new (_("Repeat by:"));
   gtk_box_pack_start(GTK_BOX(hbox_repeat_mon3), label, FALSE, FALSE, 0);

   toggle_button_repeat_mon_byday = gtk_radio_button_new_with_label
     (NULL, _("Day of week"));

   gtk_box_pack_start(GTK_BOX(hbox_repeat_mon3),
		      toggle_button_repeat_mon_byday, FALSE, FALSE, 0);

   group = NULL;
   
   group = gtk_radio_button_group(GTK_RADIO_BUTTON(toggle_button_repeat_mon_byday));
   toggle_button_repeat_mon_bydate = gtk_radio_button_new_with_label
     (group, _("Date"));
   gtk_box_pack_start(GTK_BOX(hbox_repeat_mon3),
		      toggle_button_repeat_mon_bydate, FALSE, FALSE, 0);

   /*end Month page */

   /*Repeat year page for notebook */
   vbox_repeat_year = gtk_vbox_new(FALSE, 0);
   hbox_repeat_year1 = gtk_hbox_new(FALSE, 0);
   hbox_repeat_year2 = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox_repeat_year), hbox_repeat_year1, FALSE, FALSE, 2);
   gtk_box_pack_start(GTK_BOX(vbox_repeat_year), hbox_repeat_year2, FALSE, FALSE, 2);
   label = gtk_label_new(_("Frequency is Every"));
   gtk_box_pack_start(GTK_BOX (hbox_repeat_year1), label, FALSE, FALSE, 2);
   repeat_year_entry = gtk_entry_new_with_max_length(2);
   gtk_widget_set_usize(repeat_year_entry, 30, 0);
   gtk_box_pack_start(GTK_BOX(hbox_repeat_year1), repeat_year_entry, FALSE, FALSE, 0);
   label = gtk_label_new(_("Year(s)"));
   gtk_box_pack_start(GTK_BOX(hbox_repeat_year1), label, FALSE, FALSE, 2);
   /*checkbutton */
   check_button_year_endon = gtk_check_button_new_with_label (_("End on"));
   gtk_signal_connect(GTK_OBJECT(check_button_year_endon), "clicked",
		      GTK_SIGNAL_FUNC(cb_check_button_endon),
		      GINT_TO_POINTER(PAGE_YEAR));
   gtk_box_pack_start(GTK_BOX(hbox_repeat_year2), check_button_year_endon, FALSE, FALSE, 0);
   gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			    vbox_repeat_year, notebook_tab5);

   glob_endon_year_button = gtk_button_new_with_label(_("No Date"));
   gtk_box_pack_start(GTK_BOX(hbox_repeat_year2),
		      glob_endon_year_button, FALSE, FALSE, 0);
   gtk_signal_connect(GTK_OBJECT(glob_endon_year_button), "clicked",
		      GTK_SIGNAL_FUNC(cb_cal_dialog),
		      GINT_TO_POINTER(PAGE_YEAR));

   /*end repeat year page */

   gtk_notebook_set_page(GTK_NOTEBOOK(notebook), 0);
   
   /*end details */

   /* Capture the TAB key in text1 */
   gtk_signal_connect(GTK_OBJECT(text1), "key_press_event",
		      GTK_SIGNAL_FUNC(cb_key_pressed), text2);

   /* These have to be shown before the show_all */
   gtk_widget_show(notebook_tab1);
   gtk_widget_show(notebook_tab2);
   gtk_widget_show(notebook_tab3);
   gtk_widget_show(notebook_tab4);
   gtk_widget_show(notebook_tab5);

   gtk_widget_show_all(vbox);
   gtk_widget_show_all(hbox);
   
   gtk_widget_hide(add_record_button);
   gtk_widget_hide(apply_record_button);

   gtk_text_set_editable(GTK_TEXT(text1), TRUE);
   gtk_text_set_editable(GTK_TEXT(text2), TRUE);
   
   gtk_notebook_popup_enable(GTK_NOTEBOOK(notebook));
   
   datebook_refresh(TRUE);

   return 0;
}

/*
 * datebook_gui.c
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

//Hack #1:
//If you try to open a modal dialog from a clist and the clist is set to
//BROWSE mode it will lock up X windows (at least on my machine).
//I suspect that this is a GTK bug.  I have worked around it with this hack
//for now.

#include <gtk/gtk.h>
#include <time.h>
#include <stdio.h>

#include <pi-datebook.h>

#include "datebook.h"

#define PAGE_NONE  0
#define PAGE_DAY   1
#define PAGE_WEEK  2
#define PAGE_MONTH 3
#define PAGE_YEAR  4

#define CAL_INIT       327
#define CAL_LEFT_MON   328
#define CAL_LEFT_YEAR  329
#define CAL_RIGHT_MON  330
#define CAL_RIGHT_YEAR 331
   
#define SHADOW GTK_SHADOW_ETCHED_OUT

#define DIALOG_SAID_LAST 454
#define DIALOG_SAID_4TH  455

//#define DATE_CHART

#ifdef DATE_CHART
int get_date_chart(GtkWidget *widget, GtkWidget **out_pixmapwid)
{
   char *xpm[195];

   static int inited=0;
   static GdkPixmap *pixmap;
   static GdkBitmap *mask;
   GtkWidget *pixmapwid;
   GtkStyle *style;
   int i;
   
   if (inited) {
      *out_pixmapwid = gtk_pixmap_new(pixmap, mask);
      return;
   }
   
   inited=1;

   xpm[0] = (char *)strdup("200 200 2 1");
   xpm[1] = (char *)strdup("       c None");
   xpm[2] = (char *)strdup("X      c #000000000000");
   
   for (i=0; i<192; i++) {
      xpm[i+3] = (char *)malloc(201);
      if (i%8==0) {
	 memset(xpm[i+3], 'X', 200);
	 xpm[i+3][200]='\0';
      } else {
	 memset(xpm[i+3], ' ', 200);
	 xpm[i+3][200]='\0';
      }
   }
      
   style = gtk_widget_get_style(widget);
   pixmap = gdk_pixmap_create_from_xpm_d(widget->window,  &mask,
					 &style->bg[GTK_STATE_NORMAL],
					      (gchar **)xpm);
   pixmapwid = gtk_pixmap_new(pixmap, mask);
   //gtk_widget_show(pixmapwid);

   *out_pixmapwid = pixmapwid;
}
#endif



void cb_clist1_selection(GtkWidget      *clist,
			 gint           row,
			 gint           column,
			 GdkEventButton *event,
			 gpointer       data);

GtkWidget *table;
GtkWidget *day_button[31];
GtkWidget *month_label;
GtkWidget *dow_label;
GtkWidget *clist1;
GtkWidget *text1, *text2;
GtkWidget *check_button_notime;
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
GtkWidget *spinner_endon_day_mon;
GtkWidget *spinner_endon_day_day;
GtkWidget *spinner_endon_day_year;
GtkWidget *spinner_endon_week_mon;
GtkWidget *spinner_endon_week_day;
GtkWidget *spinner_endon_week_year;
GtkWidget *spinner_endon_mon_mon;
GtkWidget *spinner_endon_mon_day;
GtkWidget *spinner_endon_mon_year;
GtkWidget *spinner_endon_year_mon;
GtkWidget *spinner_endon_year_day;
GtkWidget *spinner_endon_year_year;
GtkAdjustment *adj_begin_mon, *adj_begin_day, *adj_begin_year,
 *adj_begin_hour, *adj_begin_min;
GtkAdjustment *adj_end_mon, *adj_end_day, *adj_end_year, 
 *adj_end_hour, *adj_end_min;
GtkAdjustment *adj_endon_day_mon, *adj_endon_day_day, *adj_endon_day_year;
GtkAdjustment *adj_endon_week_mon, *adj_endon_week_day, *adj_endon_week_year;
GtkAdjustment *adj_endon_mon_mon, *adj_endon_mon_day, *adj_endon_mon_year;
GtkAdjustment *adj_endon_year_mon, *adj_endon_year_day, *adj_endon_year_year;
GtkWidget *toggle_button_repeat_days[7];
GtkWidget *toggle_button_repeat_mon_byday;
GtkWidget *toggle_button_repeat_mon_bydate;
GtkWidget *notebook;
int current_day; //range 1-31
int current_month; //range 0-11
int current_year;
int clist_row_selected;
AppointmentList *current_al;

GtkWidget *spinner_begin_mon;
GtkWidget *spinner_begin_day;
GtkWidget *spinner_begin_year;
GtkWidget *spinner_begin_hour;
GtkWidget *spinner_begin_min;
GtkWidget *spinner_end_mon;
GtkWidget *spinner_end_day;
GtkWidget *spinner_end_year;
GtkWidget *spinner_end_hour;
GtkWidget *spinner_end_min;

GtkWidget *hbox_begin_date, *vbox_begin_mon, *vbox_begin_day;
GtkWidget *vbox_begin_year, *vbox_begin_hour, *vbox_begin_min;
GtkWidget *hbox_alarm1, *hbox_alarm2;

GtkWidget *scrolled_window1;

//Stuff for the dialog window
GtkWidget *dialog;
int dialog_result;

char *nums[]={"1","2","3","4","5","6","7","8","9","10","11","12","13","14",
     "15","16","17","18","19","20","21","22","23","24","25","26","27","28",
     "29","30","31"
};

char *days[]={"S","M","T","W","R","F","S"};
//gchar *hours[] = { "12M","1AM","2AM","3AM","4AM","5am","6am",
//     "7am","8am","9am","10am","11am","12N",
//     "1pm","2pm","3pm","4pm","5pm","6pm",
//     "7pm","8pm","9pm","10pm","11pm"
//};

init()
{
   time_t ltime, t;
   struct tm *now;

   time( &ltime );
   now = localtime( &ltime );
   current_day = now->tm_mday;
   current_month = now->tm_mon;
   current_year = now->tm_year;
   
   current_al = NULL;

   clist_row_selected=0;
}

void cb_dialog_button_4th(GtkWidget *widget,
			  gpointer   data)
{
   //dialog_result=GPOINTER_TO_INT(data);
   dialog_result=DIALOG_SAID_4TH;

   gtk_widget_destroy(dialog);
}

void cb_dialog_button_last(GtkWidget *widget,
		      gpointer   data)
{
   //dialog_result=GPOINTER_TO_INT(data);
   dialog_result=DIALOG_SAID_LAST;

   gtk_widget_destroy(dialog);
}

static gboolean cb_destroy_dialog(GtkWidget *widget)
{
   gtk_main_quit();

   return FALSE;
}

int dialog_4_or_last(int dow) {
   GtkWidget *button, *label1;
   GdkWindow *main_window;
   gint px, py, pw, ph = 0;
   gint x, y, w, h;
   
   GtkWidget *hbox1, *vbox1;
   GtkWidget *frame1;
   char *days[]={"Sunday","Monday","Tuesday","Wednesday","Thurday",
	"Friday","Saturday"};
   char text[255];

   main_window = scrolled_window1->parent->window;
   
   gdk_window_get_position(main_window, &px, &py);
   gdk_window_get_size(main_window, &pw, &ph);

   w=200;
   h=200;
   x=px+pw/2-w/2;
   y=py+ph/2-h/2;
   
   //dialog=gtk_window_new(GTK_WINDOW_TOPLEVEL);
   dialog = gtk_widget_new(GTK_TYPE_WINDOW,
			   "type", GTK_WINDOW_DIALOG,
			   "x", x, "y", y,
			   "width", w, "height", h,
			   "title", "Question?",
			   NULL);

   //gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_MOUSE);
   
   gtk_signal_connect(GTK_OBJECT(dialog), "destroy",
                      GTK_SIGNAL_FUNC(cb_destroy_dialog), dialog);

   gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
   
   frame1 = gtk_frame_new("Your answer:");
   vbox1 = gtk_vbox_new(TRUE, 5);
   hbox1 = gtk_hbox_new(TRUE, 5);

   gtk_container_set_border_width(GTK_CONTAINER(frame1), 5);
   gtk_container_set_border_width(GTK_CONTAINER(vbox1), 5);
   gtk_container_set_border_width(GTK_CONTAINER(hbox1), 5);
   
   gtk_container_add(GTK_CONTAINER(dialog), frame1);
   gtk_container_add(GTK_CONTAINER(frame1), vbox1);

   label1 = gtk_label_new(text);
   sprintf(text,
	   "This appointment can either\n"
	   "repeat on the 4th %s of\n"
	   "the month, or on the Last\n"
	   "%s of the month.\n"
	   "Which do you want?",
	   days[dow], days[dow]);
   label1 = gtk_label_new(text);
   //This doesn't seem to work...
   //gtk_label_set_line_wrap(GTK_LABEL(label1), TRUE);

   gtk_box_pack_start(GTK_BOX(vbox1), label1, FALSE, FALSE, 5);
   gtk_box_pack_start(GTK_BOX(vbox1), hbox1, TRUE, TRUE, 5);

   button = gtk_button_new_with_label("4th");
   gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
                             GTK_SIGNAL_FUNC(cb_dialog_button_4th),
                             GINT_TO_POINTER(DIALOG_SAID_4TH));
   gtk_box_pack_start(GTK_BOX(hbox1), button, TRUE, TRUE, 5);

   button = gtk_button_new_with_label("Last");
   gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
                             GTK_SIGNAL_FUNC(cb_dialog_button_last),
                             GINT_TO_POINTER(DIALOG_SAID_LAST));
   gtk_box_pack_start(GTK_BOX(hbox1), button, TRUE, TRUE, 5);

   gtk_widget_show_all(dialog);

   gtk_main();
   
   return dialog_result;
}

//
// month = 0-11
// dom = day of month 1-31
// year = calendar year - 1900
// dow = day of week 0-6, where 0=Sunday, etc.
//
// Returns an enum from DayOfMonthType defined in pi-datebook.h
//
long get_dom_type(int month, int dom, int year, int dow)
{
   long r;
   int occurrence;
   int ndim; // ndim = number of days in month 28-31
   int dow_fdof; //Day of the week for the first day of the month
   int result;
   
   r=((int)((dom-1)/7))*7 + dow;

   // If its the 5th occurence of this dow in the month then it is always
   //going to be the last occurrence of that dow in the month.
   //Sometimes this will occur in the 4th week, sometimes in the 5th.
   // If its the 4th occurence of this dow in the month and there is a 5th
   //then it always the 4th occurence.
   // If its the 4th occurence of this dow in the month and there is not a
   //5th then we need to ask if this appointment repeats on the last dow of
   //the month, or the 4th dow of every month.
   // This should be perfectly clear now, right?

   //These are the last 2 lines of the DayOfMonthType enum:
   //dom4thSun, dom4thMon, dom4thTue, dom4thWen, dom4thThu, dom4thFri, dom4thSat
   //domLastSun, domLastMon, domLastTue, domLastWen, domLastThu, domLastFri, domLastSat

   if ((r>=dom4thSun) && (r<=dom4thSat)) {
      get_month_info(month, dom, year,  &dow_fdof, &ndim);
      if ((ndim - dom < 7)) {
	 //This is the 4th dow, and there is no 5th in this month.
	 result = dialog_4_or_last(dow);
	 //If they want it to be the last dow in the month instead of the
	 //4th, then we need to add 7.
	 if (result == DIALOG_SAID_LAST) {
	    r += 7;
	 }
      }
   }

   return r;
}

clear_details()
{
   gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinner_begin_mon), current_month+1);
   gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinner_begin_day), current_day);
   gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinner_begin_year), current_year+1900);
   gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinner_begin_hour), 8);
   gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinner_begin_min), 0);
   gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinner_end_mon), current_month+1);
   gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinner_end_day), current_day);
   gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinner_end_year), current_year+1900);
   gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinner_end_hour), 9);
   gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinner_end_min), 0);
   gtk_text_backward_delete(GTK_TEXT(text1),
			    gtk_text_get_length(GTK_TEXT(text1)));
   gtk_text_backward_delete(GTK_TEXT(text2),
			    gtk_text_get_length(GTK_TEXT(text2)));
   gtk_notebook_set_page(GTK_NOTEBOOK(notebook), PAGE_NONE);
}

get_details(struct Appointment *a)
{
   int s, i;
   time_t ltime;
   struct tm *now;
   char str[30];
   gchar *text;
   gint page;
      
   time(&ltime);
   now = localtime(&ltime);

   //todo week start
   //I\'m not worried about it right now
   a->repeatWeekstart=0;

   a->exceptions = 0;
   a->exception = NULL;

   //Set the daylight savings flags
   a->end.tm_isdst=a->begin.tm_isdst=now->tm_isdst;

   a->begin.tm_mon  = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinner_begin_mon)) - 1;
   a->begin.tm_mday = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinner_begin_day));
   a->begin.tm_year = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinner_begin_year)) - 1900;
   a->begin.tm_hour = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinner_begin_hour));
   a->begin.tm_min  = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinner_begin_min));
   a->begin.tm_sec  = 0;

   //get the end times
   a->end.tm_mon  = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinner_end_mon)) - 1;
   a->end.tm_mday = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinner_end_day));
   a->end.tm_year = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinner_end_year)) - 1900;
   a->end.tm_hour = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinner_end_hour));
   a->end.tm_min  = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinner_end_min));
   a->end.tm_sec  = 0;

   if (GTK_TOGGLE_BUTTON(check_button_notime)->active) {
      a->event=1;
      //printf("notime\n");
      a->begin.tm_hour = 12;
      a->begin.tm_min  = 0;
      a->begin.tm_sec  = 0;
      a->end.tm_hour = 12;
      a->end.tm_min  = 0;
      a->end.tm_sec  = 0;
   } else {
      a->event=0;
   }

   mktime(&a->begin);
   strftime(str,30,"%x %X",&a->begin);
   //printf("begin time = %s\n",str);

   mktime(&a->end);
   strftime(str,30,"%x %X",&a->end);
   //printf("end time = %s\n",str);

   if (GTK_TOGGLE_BUTTON(check_button_alarm)->active) {
      a->alarm = 1;
      text = gtk_entry_get_text(GTK_ENTRY(units_entry));
      a->advance=atoi(text);
      //printf("alarm advance %s", text);
      if (GTK_TOGGLE_BUTTON(radio_button_alarm_min)->active) {
	 a->advanceUnits = advMinutes;
	 //printf("min\n");
      }
      if (GTK_TOGGLE_BUTTON(radio_button_alarm_hour)->active) {
	 a->advanceUnits = advHours;
	 //printf("hour\n");
      }
      if (GTK_TOGGLE_BUTTON(radio_button_alarm_day)->active) {
	 a->advanceUnits = advDays;
	 //printf("day\n");
      }
   } else {
      a->alarm = 0;
   }

   page = gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook));

   a->repeatEnd.tm_hour = 12;
   a->repeatEnd.tm_min  = 0;
   a->repeatEnd.tm_sec  = 0;
   a->repeatEnd.tm_isdst= 0;
   
   switch (page) {
    case PAGE_NONE:
      a->repeatType=repeatNone;
      //printf("no repeat\n");
      break;
    case PAGE_DAY:
      a->repeatType=repeatDaily;
      text = gtk_entry_get_text(GTK_ENTRY(repeat_day_entry));
      a->repeatFrequency = atoi(text);
      //printf("every %s day(s)\n", text);
      if (GTK_TOGGLE_BUTTON(check_button_day_endon)->active) {
	 a->repeatForever=0;
	 //printf("end on day\n");
	 a->repeatEnd.tm_mon = 
	   gtk_spin_button_get_value_as_int
	   (GTK_SPIN_BUTTON(spinner_endon_day_mon)) - 1;
	 a->repeatEnd.tm_mday = 
	   gtk_spin_button_get_value_as_int
	   (GTK_SPIN_BUTTON(spinner_endon_day_day));
	 a->repeatEnd.tm_year = 
	   gtk_spin_button_get_value_as_int
	   (GTK_SPIN_BUTTON(spinner_endon_day_year)) - 1900;
	 mktime(&a->repeatEnd);
	 strftime(str,30,"%x %X",&a->repeatEnd);
	 //printf("repeat_end time = %s\n",str);
      } else {
	 a->repeatForever=1;
      }
      break;
    case PAGE_WEEK:
      a->repeatType=repeatWeekly;
      text = gtk_entry_get_text(GTK_ENTRY(repeat_week_entry));
      a->repeatFrequency = atoi(text);
      //printf("every %s week(s)\n", text);
      if (GTK_TOGGLE_BUTTON(check_button_week_endon)->active) {
	 a->repeatForever=0;
	 //printf("end on week\n");
	 a->repeatEnd.tm_mon = 
	   gtk_spin_button_get_value_as_int
	   (GTK_SPIN_BUTTON(spinner_endon_week_mon)) - 1;
	 a->repeatEnd.tm_mday = 
	   gtk_spin_button_get_value_as_int
	   (GTK_SPIN_BUTTON(spinner_endon_week_day));
	 a->repeatEnd.tm_year = 
	   gtk_spin_button_get_value_as_int
	   (GTK_SPIN_BUTTON(spinner_endon_week_year)) - 1900;
	 mktime(&a->repeatEnd);
	 strftime(str,30,"%x %X",&a->repeatEnd);
	 //printf("repeat_end time = %s\n",str);
      } else {
	 a->repeatForever=1;
      }
      //printf("Repeat Days:");
      for (i=0; i<7; i++) {
	 a->repeatDays[i]=(GTK_TOGGLE_BUTTON(toggle_button_repeat_days[i])->active);
	 //printf("%d",(GTK_TOGGLE_BUTTON(toggle_button_repeat_days[i])->active));
      }
      //printf("\n");
      break;
    case PAGE_MONTH:
      text = gtk_entry_get_text(GTK_ENTRY(repeat_mon_entry));
      a->repeatFrequency = atoi(text);
      //printf("every %s month(s)\n", text);
      if (GTK_TOGGLE_BUTTON(check_button_mon_endon)->active) {
	 a->repeatForever=0;
	 //printf("end on month\n");
	 a->repeatEnd.tm_mon = 
	   gtk_spin_button_get_value_as_int
	   (GTK_SPIN_BUTTON(spinner_endon_mon_mon)) - 1;
	 a->repeatEnd.tm_mday = 
	   gtk_spin_button_get_value_as_int
	   (GTK_SPIN_BUTTON(spinner_endon_mon_day));
	 a->repeatEnd.tm_year = 
	   gtk_spin_button_get_value_as_int
	   (GTK_SPIN_BUTTON(spinner_endon_mon_year)) - 1900;
	 mktime(&a->repeatEnd);
	 strftime(str,30,"%x %X",&a->repeatEnd);
	 //printf("repeat_end time = %s\n",str);
      } else {
	 a->repeatForever=1;
      }
      if (GTK_TOGGLE_BUTTON(toggle_button_repeat_mon_byday)->active) {
	 a->repeatType=repeatMonthlyByDay;
	 a->repeatDay = get_dom_type(a->begin.tm_mon, a->begin.tm_mday, a->begin.tm_year, a->begin.tm_wday);
	 //printf("***by day\n");
      }
      if (GTK_TOGGLE_BUTTON(toggle_button_repeat_mon_bydate)->active) {
	 a->repeatType=repeatMonthlyByDate;
	 //printf("***by date\n");
      }
      break;
    case PAGE_YEAR:
      a->repeatType=repeatYearly;
      text = gtk_entry_get_text(GTK_ENTRY(repeat_year_entry));
      a->repeatFrequency = atoi(text);
      //printf("every %s years(s)\n", text);
      if (GTK_TOGGLE_BUTTON(check_button_year_endon)->active) {
	 a->repeatForever=0;
	 //printf("end on year\n");
	 a->repeatEnd.tm_mon = 
	   gtk_spin_button_get_value_as_int
	   (GTK_SPIN_BUTTON(spinner_endon_year_mon)) - 1;
	 a->repeatEnd.tm_mday = 
	   gtk_spin_button_get_value_as_int
	   (GTK_SPIN_BUTTON(spinner_endon_year_day));
	 a->repeatEnd.tm_year = 
	   gtk_spin_button_get_value_as_int
	   (GTK_SPIN_BUTTON(spinner_endon_year_year)) - 1900;
	 mktime(&a->repeatEnd);
	 strftime(str,30,"%x %X",&a->repeatEnd);
	 //printf("repeat_end time = %s\n",str);
      } else {
	 a->repeatForever=1;
      }
      break;
   }

   a->description = gtk_editable_get_chars(GTK_EDITABLE(text1), 0, -1);
   //printf("text1=[%s]\n",a->description);
   
   a->note = gtk_editable_get_chars(GTK_EDITABLE(text2), 0, -1);
   if (a->note[0]=='\0') {
      a->note=NULL;
   } else {
      //printf("text2=[%s]\n",a->note);
   }
   
   //We won't allow a repeat frequency of less than 1
   if ((page != PAGE_NONE) && (a->repeatFrequency < 1)) {
      a->repeatFrequency = 1;
   }
}

int update_dayview_screen()
{
   int num_entries, i;
   AppointmentList *temp_al;
   gchar *empty_line[] = { "","","",""};
   char a_time[16];
   GdkPixmap *pixmap_note;
   GdkPixmap *pixmap_alarm;
   GdkPixmap *pixmap_check;
   GdkPixmap *pixmap_checked;
   GdkBitmap *mask_note;
   GdkBitmap *mask_alarm;
   GdkBitmap *mask_check;
   GdkBitmap *mask_checked;
   struct tm new_time;
   GdkColor color;
   GdkColormap *colormap;
   time_t t;

   new_time.tm_sec=0;
   new_time.tm_min=0;
   new_time.tm_hour=11;
   new_time.tm_mday=current_day;
   new_time.tm_mon=current_month;
   new_time.tm_year=current_year;
   new_time.tm_isdst=0;

   t = mktime(&new_time);

   free_AppointmentList(&current_al);

   num_entries = get_days_appointments(&current_al, &new_time);

   gtk_clist_clear(GTK_CLIST(clist1));
   gtk_text_backward_delete(GTK_TEXT(text1),
			    gtk_text_get_length(GTK_TEXT(text1)));
   
   for (temp_al = current_al, i=0; temp_al; temp_al=temp_al->next, i++) {
      gtk_clist_append(GTK_CLIST(clist1), empty_line);
      if (temp_al->ma.a.event) {
	 //This is a timeless event
	 strcpy(a_time, "No Time");
      } else {
	 sprintf(a_time, "%02d:%02d-%02d:%02d",
		 temp_al->ma.a.begin.tm_hour,temp_al->ma.a.begin.tm_min,
		 temp_al->ma.a.end.tm_hour,temp_al->ma.a.end.tm_min);
      }
      gtk_clist_set_text(GTK_CLIST(clist1), i, 0, a_time);
      gtk_clist_set_text(GTK_CLIST(clist1), i, 1, temp_al->ma.a.description);
      gtk_clist_set_row_data(GTK_CLIST(clist1), i, &(temp_al->ma));

      if (temp_al->ma.rt == NEW_PC_REC) {
	 colormap = gtk_widget_get_colormap(clist1);
	 color.red=CLIST_NEW_RED;
	 color.green=CLIST_NEW_GREEN;
	 color.blue=CLIST_NEW_BLUE;
	 gdk_color_alloc(colormap, &color);
	 gtk_clist_set_background(GTK_CLIST(clist1), i, &color);
      }
      if (temp_al->ma.rt == DELETED_PALM_REC) {
	 colormap = gtk_widget_get_colormap(clist1);
	 color.red=CLIST_DEL_RED;
	 color.green=CLIST_DEL_GREEN;
	 color.blue=CLIST_DEL_BLUE;
	 gdk_color_alloc(colormap, &color);
	 gtk_clist_set_background(GTK_CLIST(clist1), i, &color);
      }
      
      if ( (temp_al->ma.a.note) || (temp_al->ma.a.alarm) ) {
	 get_pixmaps(scrolled_window1,
		     &pixmap_note, &pixmap_alarm, &pixmap_check, &pixmap_checked,
		     &mask_note, &mask_alarm, &mask_check, &mask_checked
		     );
      }
      if (temp_al->ma.a.note) {
	 //Put a note pixmap up
	 gtk_clist_set_pixmap(GTK_CLIST(clist1), i, 2, pixmap_note, mask_note);
      }

      if (temp_al->ma.a.alarm) {
	 //Put an alarm pixmap up
	 gtk_clist_set_pixmap(GTK_CLIST(clist1), i, 3, pixmap_alarm, mask_alarm);
      }
   }
   gtk_clist_append(GTK_CLIST(clist1), empty_line);
   gtk_clist_set_text(GTK_CLIST(clist1), i, 0, "New");
   gtk_clist_set_text(GTK_CLIST(clist1), i, 1, "Select to add an appointment");
   gtk_clist_set_row_data(GTK_CLIST(clist1), i,
			  GINT_TO_POINTER(CLIST_NEW_ENTRY_DATA));
   //If there is an item in the list, select the first one
   if (i>0) {
      gtk_clist_select_row(GTK_CLIST(clist1), 0, 1);
      cb_clist1_selection(clist1, 0, 0, (GdkEventButton *)455, "");
   }
}


void cb_spin_mon_day_year(GtkAdjustment *adj, gpointer data)
{
   int day, mon, year, hour1, hour2, min1, min2;
   int total1, total2;
   int days_in_month[]={31,28,31,30,31,30,31,31,30,31,30,31
   };
   //allow the days range to only go as high as the number of days
   //in the month
   if (GPOINTER_TO_INT(data) == 1) {
      //Take care of that leap year thing
      year = gtk_spin_button_get_value_as_int
	(GTK_SPIN_BUTTON(spinner_begin_year));
      if (year%4 == 0) {
	 days_in_month[1]=29;
      }
      //Make sure that illegal dates aren\'t entered (ex. Feb, 31)
      mon = gtk_spin_button_get_value_as_int
	(GTK_SPIN_BUTTON(spinner_begin_mon));
      day = gtk_spin_button_get_value_as_int
	(GTK_SPIN_BUTTON(spinner_begin_day));
      if (day > days_in_month[mon - 1]) {
	 day = days_in_month[mon - 1];
	 gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_begin_day), day);
      }
   }
   if (GPOINTER_TO_INT(data) == 2) {
      //Take care of that leap year thing
      year = gtk_spin_button_get_value_as_int
	(GTK_SPIN_BUTTON(spinner_end_year));
      if (year%4 == 0) {
	 days_in_month[1]=29;
      }
      //Make sure that illegal dates aren\'t entered (ex. Feb, 31)
      mon = gtk_spin_button_get_value_as_int
	(GTK_SPIN_BUTTON(spinner_end_mon));
      day = gtk_spin_button_get_value_as_int
	(GTK_SPIN_BUTTON(spinner_end_day));
      if (day > days_in_month[mon - 1]) {
	 day = days_in_month[mon - 1];
	 gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_end_day), day);
      }
   }
   //Make the appointment start and end on the same day
   //This seems to be required by the Palm Datebook Application
   if (GPOINTER_TO_INT(data) == 1) {
      gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_end_mon), mon);
      gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_end_day), day);
      gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_end_year), year);
   }
   if (GPOINTER_TO_INT(data) == 2) {
      gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_begin_mon), mon);
      gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_begin_day), day);
      gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_begin_year), year);
   }
   
   //Make sure the appointment start time is prior to the end time
   hour1 = gtk_spin_button_get_value_as_int
     (GTK_SPIN_BUTTON(spinner_begin_hour));
   min1 = gtk_spin_button_get_value_as_int
     (GTK_SPIN_BUTTON(spinner_begin_min));
   hour2 = gtk_spin_button_get_value_as_int
     (GTK_SPIN_BUTTON(spinner_end_hour));
   min2 = gtk_spin_button_get_value_as_int
     (GTK_SPIN_BUTTON(spinner_end_min));
   total1 = hour1 * 60 + min1;
   total2 = hour2 * 60 + min2;
   //User is violating the GUI constraints, terminate them!
   if (total2 < total1) {
      if (GPOINTER_TO_INT(data) == 1) {
	 gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_end_hour), hour1);
	 gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_end_min), min1);
      }
      if (GPOINTER_TO_INT(data) == 2) {
	 gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_begin_hour), hour2);
	 gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_begin_min), min2);
      }
   }
}


void cb_delete_appt(GtkWidget *widget, gpointer data)
{
   MyAppointment *ma;
   
   ma = gtk_clist_get_row_data(GTK_CLIST(clist1), clist_row_selected);
   if (ma < (MyAppointment *)CLIST_MIN_DATA) {
      return;
   }
   delete_pc_record(DATEBOOK, ma);
   //pc_datebook_delete(ma);
   //Force the DOW week button to be clicked on to force a re-read, redraw
   gtk_signal_emit_by_name(GTK_OBJECT(day_button[current_day-1]), "clicked");
}

void cb_check_button_notime(GtkWidget *widget,
			    gpointer   data)
{
   if (GTK_TOGGLE_BUTTON(widget)->active) {
      gtk_widget_hide(vbox_begin_hour);
      gtk_widget_hide(vbox_begin_min);
   } else {
      gtk_widget_show(vbox_begin_hour);
      gtk_widget_show(vbox_begin_min);
   }
}

void cb_check_button_alarm(GtkWidget *widget, gpointer data)
{
   if (GTK_TOGGLE_BUTTON(widget)->active) {
      gtk_widget_show(hbox_alarm2);
   } else {
      gtk_widget_hide(hbox_alarm2);
   }
}

/*
int cb_backup( GtkWidget *widget,
                 gpointer   data )
{
   cleanup_pc_files();
   return 0;

   g_print("fork\n");
   switch ( fork(0) ){
    case -1:
      perror("fork");
      exit(7);
    case 0:
      //system("ls");
      exit(0);
    default:
      return;
   }
}
*/

void cb_clist1_selection(GtkWidget      *clist,
			 gint           row,
			 gint           column,
			 GdkEventButton *event,
			 gpointer       data)
{
   struct Appointment *a, new_a;
   MyAppointment *ma;
   char temp[20];
   gchar *text;
   int i, rec_len, found;
   char record[65536];
   
   //g_print("event %d\n",event);
   //g_print("data %d\n",data);
   if (!event) return;

   clist_row_selected=row;

   ma = gtk_clist_get_row_data(GTK_CLIST(clist1), row);
   if (ma) {
      a=&(ma->a);
   }
   //Hack #1
   //If we are not in add data mode then switch the mode on the clist
   //back to what its supposed to be
   if (gtk_clist_find_row_from_data(GTK_CLIST(clist1),
				    GINT_TO_POINTER(CLIST_ADDING_ENTRY_DATA))
				    < 0) {
      gtk_clist_set_selection_mode(GTK_CLIST(clist1), GTK_SELECTION_BROWSE);
   }
   //End Hack
   if (ma==GINT_TO_POINTER(CLIST_ADDING_ENTRY_DATA)) {
      get_details(&new_a);
      pc_datebook_write(&new_a, NEW_PC_REC, 0);
      free_Appointment(&new_a);
      gtk_signal_emit_by_name(GTK_OBJECT(day_button[current_day-1]), "clicked");
      return;
   }

   if (ma==GINT_TO_POINTER(CLIST_NEW_ENTRY_DATA)) {
      gtk_clist_set_row_data(GTK_CLIST(clist1), row,
			     GINT_TO_POINTER(CLIST_ADDING_ENTRY_DATA));
      //Hack #1
      gtk_clist_set_selection_mode(GTK_CLIST(clist1), GTK_SELECTION_SINGLE);
      gtk_clist_unselect_row(GTK_CLIST(clist1), row, column);
      //End Hack
      gtk_clist_set_text(GTK_CLIST(clist1), row, 0, "Adding");
      gtk_clist_set_text(GTK_CLIST(clist1), row, 1, "Fill in Details, then click here");
      clear_details();
      return;
   }
   gtk_text_set_point(GTK_TEXT(text1), 0);
   gtk_text_forward_delete(GTK_TEXT(text1),
			    gtk_text_get_length(GTK_TEXT(text1)));
   gtk_text_set_point(GTK_TEXT(text2), 0);
   gtk_text_forward_delete(GTK_TEXT(text2),
			    gtk_text_get_length(GTK_TEXT(text2)));
   
   if (a->event) {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (check_button_notime), TRUE);
   } else {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (check_button_notime), FALSE);
   }
   if (a->alarm) {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (check_button_alarm), TRUE);
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
	 g_print("Error in DateBookDB advanceUnits = %d\n",a->advanceUnits);
      }
      sprintf(temp, "%d", a->advance);
      gtk_entry_set_text(GTK_ENTRY(units_entry), temp);
   } else {
      //gtk_toggle_button_get_active
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (check_button_alarm), FALSE);
      gtk_entry_set_text(GTK_ENTRY(units_entry), "0");
   }
   if (a->description) {
      gtk_text_insert(GTK_TEXT(text1), NULL,NULL, NULL, a->description, -1);
   }
   if (a->note) {
      gtk_text_insert(GTK_TEXT(text2), NULL,NULL, NULL, a->note, -1);
   }
   gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_begin_mon),a->begin.tm_mon+1);
   gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_begin_day),a->begin.tm_mday);
   gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_begin_year),a->begin.tm_year+1900);
   gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_begin_hour),a->begin.tm_hour);
   gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_begin_min),a->begin.tm_min);

   gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_end_mon),a->end.tm_mon+1);
   gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_end_day),a->end.tm_mday);
   gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_end_year),a->end.tm_year+1900);
   gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_end_hour),a->end.tm_hour);
   gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_end_min),a->end.tm_min);

   //Do the Repeat information
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
	 gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_endon_day_mon),
				  a->repeatEnd.tm_mon+1);
	 gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_endon_day_day),
				  a->repeatEnd.tm_mday);
	 gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_endon_day_year),
				  a->repeatEnd.tm_year+1900);
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
	 gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_endon_week_mon),
				  a->repeatEnd.tm_mon+1);
	 gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_endon_week_day),
				  a->repeatEnd.tm_mday);
	 gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_endon_week_year),
				  a->repeatEnd.tm_year+1900);
      }
      sprintf(temp, "%d", a->repeatFrequency);
      gtk_entry_set_text(GTK_ENTRY(repeat_week_entry), temp);
      for (i=0; i<7; i++) {
	 gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
				      (toggle_button_repeat_days[i]),
				      a->repeatDays[i]);
      }
      gtk_notebook_set_page(GTK_NOTEBOOK(notebook), PAGE_WEEK);
      break;
    case repeatMonthlyByDate:
    case repeatMonthlyByDay:
      //for day: printf("repeat day=%d\n",a->repeatDay);
      if ((a->repeatForever)) {
	 gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
				      (check_button_mon_endon), FALSE);
      }
      else {
	 gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
				      (check_button_mon_endon), TRUE);
	 gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_endon_mon_mon),
				  a->repeatEnd.tm_mon+1);
	 gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_endon_mon_day),
				  a->repeatEnd.tm_mday);
	 gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_endon_mon_year),
				  a->repeatEnd.tm_year+1900);
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
	 gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_endon_year_mon),
				  a->repeatEnd.tm_mon+1);
	 gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_endon_year_day),
				  a->repeatEnd.tm_mday);
	 gtk_adjustment_set_value(GTK_ADJUSTMENT(adj_endon_year_year),
				  a->repeatEnd.tm_year+1900);
      }
      sprintf(temp, "%d", a->repeatFrequency);
      gtk_entry_set_text(GTK_ENTRY(repeat_year_entry), temp);

      gtk_notebook_set_page(GTK_NOTEBOOK(notebook), PAGE_YEAR);
      break;
    default: g_print("unknown repeatType found in DatebookDB\n");
   }

   return;
}

void set_date_labels()
{
   struct tm now;
   char str[50];

   now.tm_sec=0;
   now.tm_min=0;
   now.tm_hour=11;
   now.tm_isdst=0;
   now.tm_wday=0;
   now.tm_yday=0;
   now.tm_mday = current_day;
   now.tm_mon = current_month;
   now.tm_year = current_year;
   mktime(&now);
   strftime(str,20,"%x",&now);
   gtk_label_set_text(GTK_LABEL(month_label), str);

   strftime(str, 50, "%a., %b. %d, %Y", &now);
   gtk_label_set_text(GTK_LABEL(dow_label), str);
}
//
//When a calendar day is pressed
//
void cb_day_button(GtkWidget *widget,
		   gpointer   data)
{
   int num;

   num = GPOINTER_TO_INT(data);
   if ((num<1) || (num>31)) {
      PRINT_FILE_LINE;
      printf("num is out of range\n");
      return;
   }
   //g_print ("%d:cb_day_button\n", num);
   if (GTK_IS_BUTTON(day_button[current_day-1])) {
      gtk_widget_set_name(GTK_WIDGET(day_button[current_day-1]), "button");
   }
   current_day = num;
   if (GTK_IS_BUTTON(day_button[current_day-1])) {
      gtk_widget_set_name(GTK_WIDGET(day_button[current_day-1]), "button_set");
   }
   set_date_labels();
   update_dayview_screen();
}

void paint_calendar(GtkWidget *widget, gpointer data)
{
   GtkWidget *button;
   time_t ltime;
   struct tm *now;
   int i, idata;
   char str[100];
   int dow_int, ndim;
   
   idata=GPOINTER_TO_INT(data);
   
   if ((idata!=CAL_INIT) &&
       (idata!=CAL_LEFT_MON) &&
       (idata!=CAL_LEFT_YEAR) &&
       (idata!=CAL_RIGHT_MON) &&
       (idata!=CAL_RIGHT_YEAR)) {
      return;
   }

   time(&ltime);
   now = localtime(&ltime);

   if (idata != CAL_INIT) {
      get_month_info(current_month, 1, current_year, &dow_int, &ndim);
      for (i=0; i<ndim; i++) {
	 if GTK_IS_BUTTON(day_button[i]) {
	    //gtk_signal_handlers_destroy(GTK_OBJECT(day_button[i]));
	    gtk_widget_destroy(GTK_WIDGET(day_button[i]));
	 }
	 //g_print("destroying %d\n",i+1);
      }
      if (GPOINTER_TO_INT(data) == CAL_LEFT_MON) {
	 current_month--;
	 if (current_month < 0) {
	    current_month=11;
	    current_year--;
	    if (current_year < 3) {
	       current_year=3;
	    }
	 }
	 set_date_labels();
      }
      if (GPOINTER_TO_INT(data) == CAL_LEFT_YEAR) {
	 current_year--;
	 if (current_year < 3) {
	    current_year=3;
	 }
	 set_date_labels();
      }
      if (GPOINTER_TO_INT(data) == CAL_RIGHT_MON) {
	 current_month++;
	 if (current_month > 11) {
	    current_month=0;
	    current_year++;
	    if (current_year > 137) {
	       current_year=137;
	    }
	 }
	 set_date_labels();
      }
      if (GPOINTER_TO_INT(data) == CAL_RIGHT_YEAR) {
	 current_year++;
	 if (current_year > 137) {
	    current_year=137;
	 }
	 set_date_labels();
      }
   } else {
      current_day = now->tm_mday;
      current_month = now->tm_mon;
      current_year = now->tm_year;
      get_this_month_info(&dow_int, &ndim);
      //Put the Day of the Week buttons up
      for (i=0; i<7; i++) {
	 button = gtk_button_new_with_label(days[i]);
	 gtk_widget_set_name(button, "button_dow");
	 gtk_table_attach_defaults(GTK_TABLE(table), button, i, i+1, 0, 1);
//	 gtk_table_attach(GTK_TABLE(table), button, i, i+1, 0, 1,
//			  GTK_FILL|GTK_SHRINK,GTK_FILL|GTK_SHRINK,GTK_FILL|GTK_SHRINK,GTK_FILL|GTK_SHRINK
//			  );
	 gtk_widget_show(button);
      }
      set_date_labels();
   }
   
   get_month_info(current_month, 1, current_year, &dow_int, &ndim);

   for (i=0; i<ndim; i++) {
      sprintf(str, "%d", i+1);
      day_button[i] = gtk_button_new_with_label(str);
      gtk_signal_connect(GTK_OBJECT(day_button[i]), "clicked",
			 GTK_SIGNAL_FUNC(cb_day_button), GINT_TO_POINTER(i+1));
      if ((i+1 == current_day)&&(current_month == now->tm_mon)) {
	 gtk_widget_set_name(GTK_WIDGET(day_button[i]), "button_set");
      }

      gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(day_button[i]),
				(i+dow_int)%7, (i+dow_int)%7+1,
				(i+dow_int)/7+1, (i+dow_int)/7+2);
      gtk_widget_show(GTK_WIDGET(day_button[i]));
   }
   //Click on the current day
   if (current_day > ndim) {
      current_day = ndim;
   }
   if (GTK_IS_BUTTON(GTK_WIDGET(day_button[current_day-1]))) {
      gtk_signal_emit_by_name(GTK_OBJECT(GTK_WIDGET(
			      day_button[current_day-1])), "clicked");
   //   cb_day_button(day_button[current_day-1], GINT_TO_POINTER(current_day));
   }
}


int datebook_gui(GtkWidget *vbox, GtkWidget *hbox)
{
   extern GtkWidget *glob_date_label;
   extern gint glob_date_timer_tag;
   GtkWidget *vbox1, *vbox2;
   GtkWidget *hbox2;
   GtkWidget *hbox_text1, *hbox_text2;
   GtkWidget *hbox_table;
   GtkWidget *hbox_notime;
   GtkWidget *vbox_detail;
   GtkWidget *vbox_todays_sched;
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
   GtkWidget *vbox_filler;
   GtkWidget *vbox_begin_labels;
   //GtkWidget *hbox_end_date;
   GtkWidget *dow_hbox;
   GtkWidget *button;
   GtkWidget *separator;
   GtkWidget *label;
   GtkWidget *notebook_tab1;
   GtkWidget *notebook_tab2;
   GtkWidget *notebook_tab3;
   GtkWidget *notebook_tab4;
   GtkWidget *notebook_tab5;
   GtkWidget *frame;
   GtkWidget *hbox_notebook;
   GtkWidget *vscrollbar;
   GtkWidget *arrow;

   GdkPixmap *pixmap_note;
   GdkPixmap *pixmap_alarm;
   GdkPixmap *pixmap_check;
   GdkPixmap *pixmap_checked;
   GdkBitmap *mask_note;
   GdkBitmap *mask_alarm;
   GdkBitmap *mask_check;
   GdkBitmap *mask_checked;

   GtkWidget *pixmapwid;
   //GdkPixmap *pixmap;
   //GdkPixmap *pixmap_note;
   //GdkBitmap *mask;
   //GtkStyle *style;
   //GtkWidget *list;
   GSList *group;
   
   time_t ltime;
   struct tm *now;
   
   int i;
#define MAX_STR 100
   char str[MAX_STR];
   char dow[50];
   char *day_letters[] = {"S","M","T","W","R","F","S"};

   init();
   
   dow_hbox = gtk_hbox_new(FALSE, 0);
   hbox2 = gtk_hbox_new(FALSE, 0);
   hbox_text1 = gtk_hbox_new(FALSE, 0);
   hbox_text2 = gtk_hbox_new(FALSE, 0);
   vbox1 = gtk_vbox_new(FALSE, 0);
   vbox2 = gtk_vbox_new(FALSE, 0);

   separator = gtk_hseparator_new ();
   gtk_box_pack_start(GTK_BOX(vbox1), separator, FALSE, FALSE, 5);
   gtk_widget_show(separator);

   // Create "Delete" button in left column
   button = gtk_button_new_with_label ("Delete");
   gtk_signal_connect(GTK_OBJECT (button), "clicked",
		      GTK_SIGNAL_FUNC (cb_delete_appt), NULL);
   gtk_box_pack_start(GTK_BOX (vbox), button, FALSE, FALSE, 0);
   gtk_widget_show(button);

   /*
   // Create "Backup" button in left column
   button = gtk_button_new_with_label ("Backup");
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_backup), NULL);
   gtk_box_pack_start(GTK_BOX (vbox), button, FALSE, FALSE, 0);
   gtk_widget_show(button);
*/
   //Make the Today is: label
   time(&ltime);
   now = localtime(&ltime);
   strftime(str, MAX_STR, "Today is %A, %x %X", now);
   glob_date_label = gtk_label_new(str);
   gtk_box_pack_start(GTK_BOX(vbox1), glob_date_label, FALSE, FALSE, 0);
   gtk_widget_show(glob_date_label);
   glob_date_timer_tag = gtk_timeout_add(1000, timeout_date, NULL);


   //Make the Today is: label
   //time(&ltime);
   //now = localtime(&ltime);
   //strftime(str, MAX_STR, "Today is: %A, %x", now);
   //today_label = gtk_label_new(str);
   //gtk_box_pack_start (GTK_BOX (vbox1), today_label, FALSE, FALSE, 0);
   //gtk_widget_show (today_label);

   separator = gtk_hseparator_new();
   gtk_box_pack_start(GTK_BOX(vbox1), separator, FALSE, FALSE, 5);
   gtk_widget_show(separator);

//   gtk_box_pack_start(GTK_BOX(hbox), vbox0, FALSE, FALSE, 3);
   gtk_box_pack_start(GTK_BOX(hbox), vbox1, TRUE, TRUE, 3);
   gtk_box_pack_start(GTK_BOX(hbox), vbox2, TRUE, TRUE, 0);
   gtk_box_pack_start(GTK_BOX(vbox1), dow_hbox, FALSE, FALSE, 3);
   gtk_box_pack_start(GTK_BOX(vbox1), hbox2, FALSE, FALSE, 3);

   //separator = gtk_hseparator_new();
   //gtk_box_pack_start(GTK_BOX(vbox1), separator, FALSE, TRUE, 5);
   //gtk_widget_show(separator);
   
   //Make the DOW label
   dow_label = gtk_label_new("");
   gtk_box_pack_start(GTK_BOX(dow_hbox), dow_label, FALSE, FALSE, 0);
   gtk_widget_show(dow_label);

   //Make a left arrow for going back a year
   button = gtk_button_new();
   arrow = gtk_arrow_new(GTK_ARROW_LEFT, GTK_SHADOW_OUT);
   gtk_container_add(GTK_CONTAINER(button), arrow);
   gtk_signal_connect(GTK_OBJECT(button), "clicked", 
		      GTK_SIGNAL_FUNC(paint_calendar),
		      GINT_TO_POINTER(CAL_LEFT_YEAR));
   gtk_widget_show(button);
   gtk_widget_show(arrow);
   gtk_box_pack_start(GTK_BOX(hbox2), button, FALSE, FALSE, 3);

   //Make a left arrow arrow for going back a month
   button = gtk_button_new();
   arrow = gtk_arrow_new(GTK_ARROW_LEFT, GTK_SHADOW_OUT);
   gtk_container_add(GTK_CONTAINER (button), arrow);
   gtk_signal_connect(GTK_OBJECT (button), "clicked",
		      GTK_SIGNAL_FUNC(paint_calendar),
		      GINT_TO_POINTER(CAL_LEFT_MON));
   gtk_widget_show(button);
   gtk_widget_show(arrow);
   gtk_box_pack_start(GTK_BOX (hbox2), button, FALSE, FALSE, 3);

   //Make the Month label
   month_label = gtk_label_new("");
   gtk_box_pack_start(GTK_BOX (hbox2), month_label, FALSE, FALSE, 0);
   gtk_widget_show(month_label);
   
   //Make a right arrow for going forward a month
   button = gtk_button_new();
   arrow = gtk_arrow_new(GTK_ARROW_RIGHT, GTK_SHADOW_OUT);
   gtk_container_add(GTK_CONTAINER(button), arrow);
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(paint_calendar),
		      GINT_TO_POINTER(CAL_RIGHT_MON));
   gtk_widget_show(button);
   gtk_widget_show(arrow);
   gtk_box_pack_start(GTK_BOX(hbox2), button, FALSE, FALSE, 3);

   //Make a right arrow for going forward a year
   button = gtk_button_new();
   arrow = gtk_arrow_new(GTK_ARROW_RIGHT, GTK_SHADOW_OUT);
   gtk_container_add(GTK_CONTAINER(button), arrow);
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(paint_calendar),
		      GINT_TO_POINTER(CAL_RIGHT_YEAR));
   gtk_widget_show(button);
   gtk_widget_show(arrow);
   gtk_box_pack_start(GTK_BOX(hbox2), button, FALSE, FALSE, 3);

   // Create a 7x6 table
   hbox_table = gtk_hbox_new (FALSE, 0);
   gtk_box_pack_start (GTK_BOX (vbox1), hbox_table, FALSE, FALSE, 0);
   gtk_widget_show(hbox_table);
   table = gtk_table_new (7, 7, TRUE);
   gtk_table_set_row_spacings(GTK_TABLE(table),0);
   gtk_table_set_col_spacings(GTK_TABLE(table),0);

   gtk_box_pack_start (GTK_BOX (hbox_table), table, FALSE, FALSE, 0);
   //gtk_widget_set_usize(table, 100, 200);


   
#ifdef DATE_CHART   
   get_date_chart(hbox_table, &pixmapwid);
   gtk_box_pack_start(GTK_BOX(hbox_table), pixmapwid, FALSE, FALSE, 0);
   gtk_widget_show(pixmapwid);
#endif
   

   frame = gtk_frame_new ("Daily Schedule");
   gtk_frame_set_label_align( GTK_FRAME(frame), 0.5, 0.0);
   gtk_box_pack_start (GTK_BOX (vbox1), frame, TRUE, TRUE, 0);
   vbox_todays_sched = gtk_vbox_new (FALSE, 0);
   gtk_container_add(GTK_CONTAINER (frame), vbox_todays_sched);
   gtk_widget_show(frame);
   gtk_widget_show(vbox_todays_sched);


   /* create a new scrolled window. */
   scrolled_window1 = gtk_scrolled_window_new(NULL, NULL);

//   gtk_widget_set_usize (GTK_WIDGET(scrolled_window1), 330, 200);
   //gtk_window_set_default_size(GTK_WINDOW(scrolled_window1), 310, 320);
   //requisition.width=610;
   //requisition.height=220;
   //gtk_widget_size_request(GTK_WIDGET(scrolled_window1), &requisition);
   
   gtk_container_set_border_width(GTK_CONTAINER(scrolled_window1), 5);

   /* the policy is one of GTK_POLICY AUTOMATIC, or GTK_POLICY_ALWAYS.
    * GTK_POLICY_AUTOMATIC will automatically decide whether you need
    * scrollbars, whereas GTK_POLICY_ALWAYS will always leave the scrollbars
    * there.  The first one is the horizontal scrollbar, the second,
    * the vertical. */
   gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window1),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
   /* The dialog window is created with a vbox packed into it. */
   gtk_box_pack_start(GTK_BOX (vbox_todays_sched), scrolled_window1,
		      TRUE, TRUE, 0);
   gtk_widget_show(scrolled_window1);

   clist1 = gtk_clist_new(4);

   /* When a selection is made, we want to know about it. The callback
    * used is selection_made, and its code can be found further down */
   gtk_signal_connect(GTK_OBJECT(clist1), "select_row",
		      GTK_SIGNAL_FUNC(cb_clist1_selection),
		      NULL);

   /* It isn't necessary to shadow the border, but it looks nice :) */
   gtk_clist_set_shadow_type (GTK_CLIST(clist1), SHADOW);
   gtk_clist_set_selection_mode(GTK_CLIST(clist1), GTK_SELECTION_BROWSE);
   
   /* What however is important, is that we set the column widths as
    * they will never be right otherwise. Note that the columns are
    * numbered from 0 and up.
    */
   gtk_clist_set_column_width(GTK_CLIST(clist1), 0, 70);
   gtk_clist_set_column_width(GTK_CLIST(clist1), 1, 205);
   gtk_clist_set_column_width(GTK_CLIST(clist1), 2, 16);
   gtk_clist_set_column_width(GTK_CLIST(clist1), 3, 16);
   //gtk_widget_set_usize(GTK_WIDGET(clist1), 330, 200);
   gtk_widget_set_usize(GTK_WIDGET(scrolled_window1), 330, 200);
   //gtk_window_set_default_size(GTK_WINDOW(scrolled_window1), 330, 320);
   
   gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW
					  (scrolled_window1), clist1);
   //gtk_clist_set_sort_column (GTK_CLIST(clist1), 0);
   //gtk_clist_set_auto_sort(GTK_CLIST(clist1), TRUE);
   gtk_widget_show(clist1);

   //start details

   frame = gtk_frame_new ("Details");
   gtk_frame_set_label_align( GTK_FRAME(frame), 0.5, 0.0);
   //gtk_container_set_border_width (GTK_CONTAINER (frame), 5);
   gtk_box_pack_start (GTK_BOX (vbox2), frame, TRUE, TRUE, 0);

   vbox_detail = gtk_vbox_new (FALSE, 0);

   //Get away from the edge
   //vbox_filler = gtk_vbox_new (FALSE, 0);
   //gtk_box_pack_start (GTK_BOX (hbox_alarm), vbox_filler, FALSE, FALSE, 2);
   //gtk_widget_show (vbox_filler);

   //The checkbox for Alarm
   hbox_alarm1 = gtk_hbox_new (FALSE, 0);
   gtk_box_pack_start (GTK_BOX (vbox_detail), hbox_alarm1, FALSE, FALSE, 0);
   gtk_widget_show (hbox_alarm1);

   check_button_alarm = gtk_check_button_new_with_label ("Alarm");
   gtk_box_pack_start (GTK_BOX (hbox_alarm1), check_button_alarm, FALSE, FALSE, 5);
   gtk_signal_connect (GTK_OBJECT (check_button_alarm), "clicked",
		       GTK_SIGNAL_FUNC (cb_check_button_alarm), NULL);
   gtk_widget_show (check_button_alarm);

   hbox_alarm2 = gtk_hbox_new (FALSE, 0);
   gtk_box_pack_start (GTK_BOX (hbox_alarm1), hbox_alarm2, FALSE, FALSE, 0);
   gtk_widget_show (hbox_alarm2);

   //Units entry for alarm
   units_entry = gtk_entry_new_with_max_length(2);
   gtk_widget_set_usize (units_entry, 30, 0);
   gtk_box_pack_start(GTK_BOX (hbox_alarm2), units_entry, FALSE, FALSE, 0);
   gtk_widget_show(units_entry);

   //Alarm units menu
   //list_units_menu = build_option_menu (items, 3, 0, "A");
  
   //list_units_menu = gtk_option_menu_new ();
      
   //menu = gtk_menu_new ();
   //group = NULL;
  
//
//
//
   radio_button_alarm_min = gtk_radio_button_new_with_label(NULL, "Minutes");

   group = NULL;
   group = gtk_radio_button_group(GTK_RADIO_BUTTON(radio_button_alarm_min));
   radio_button_alarm_hour = gtk_radio_button_new_with_label(group, "Hours");
   group = gtk_radio_button_group(GTK_RADIO_BUTTON(radio_button_alarm_hour));
   radio_button_alarm_day = gtk_radio_button_new_with_label(group, "Days");
   
   gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (radio_button_alarm_min), TRUE);
   
   gtk_box_pack_start (GTK_BOX (hbox_alarm2),
		       radio_button_alarm_min, FALSE, FALSE, 0);
   gtk_box_pack_start (GTK_BOX (hbox_alarm2),
		       radio_button_alarm_hour, FALSE, FALSE, 0);
   gtk_box_pack_start (GTK_BOX (hbox_alarm2),
		       radio_button_alarm_day, FALSE, FALSE, 0);
   gtk_widget_show(radio_button_alarm_min);
   gtk_widget_show(radio_button_alarm_hour);
   gtk_widget_show(radio_button_alarm_day);


   //The checkbox for timeless events
   hbox_notime = gtk_hbox_new (FALSE, 5);
   gtk_widget_show (hbox_notime);
   gtk_box_pack_start (GTK_BOX (vbox_detail), hbox_notime, FALSE, FALSE, 10);
   check_button_notime = gtk_check_button_new_with_label ("This Event has no particular time");
   gtk_signal_connect(GTK_OBJECT (check_button_notime), "clicked",
		      GTK_SIGNAL_FUNC (cb_check_button_notime), NULL);
   gtk_box_pack_start (GTK_BOX (hbox_notime), check_button_notime, FALSE, FALSE, 5);
   gtk_widget_show (check_button_notime);
   //end

   hbox_begin_date = gtk_hbox_new (FALSE, 0);
   //hbox_end_date = gtk_hbox_new (FALSE, 0);
   //gtk_container_set_border_width (GTK_CONTAINER (hbox_begin_date), 5);

   gtk_container_add(GTK_CONTAINER (frame), vbox_detail);

   gtk_box_pack_start(GTK_BOX (vbox_detail), hbox_begin_date, FALSE, FALSE, 0);

   //Begin spinners
   vbox_begin_labels = gtk_vbox_new (FALSE, 0);
   gtk_box_pack_start (GTK_BOX (hbox_begin_date), vbox_begin_labels, FALSE, FALSE, 0);

   label = gtk_label_new ("\nBegin:");
   gtk_misc_set_alignment (GTK_MISC (label), 1, 1);
   //gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_RIGHT);
   gtk_box_pack_start (GTK_BOX (vbox_begin_labels), label, FALSE, TRUE, 5);
   gtk_widget_show (label);

   vbox_begin_mon = gtk_vbox_new (FALSE, 0);
   gtk_box_pack_start (GTK_BOX (hbox_begin_date), vbox_begin_mon, FALSE, FALSE, 0);

   //Put Month spinner in
   label = gtk_label_new ("Month:");
   gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
   gtk_box_pack_start (GTK_BOX (vbox_begin_mon), label, FALSE, TRUE, 0);
   gtk_widget_show (label);

   adj_begin_mon = (GtkAdjustment *)gtk_adjustment_new(now->tm_mon+1, 1.0, 12.0, 1.0,
						       5.0, 0.0);
   spinner_begin_mon = gtk_spin_button_new (adj_begin_mon, 0, 0);
   gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinner_begin_mon), FALSE);
   gtk_spin_button_set_numeric(GTK_SPIN_BUTTON (spinner_begin_mon), TRUE);
   gtk_spin_button_set_shadow_type (GTK_SPIN_BUTTON (spinner_begin_mon),
				    SHADOW);
   gtk_box_pack_start (GTK_BOX (vbox_begin_mon), spinner_begin_mon, FALSE, TRUE, 0);

   vbox_begin_day = gtk_vbox_new (FALSE, 0);
   gtk_box_pack_start (GTK_BOX (hbox_begin_date), vbox_begin_day, FALSE, FALSE, 0);

   //Put Day spinner in
   label = gtk_label_new ("Day:");
   gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
   gtk_box_pack_start (GTK_BOX (vbox_begin_day), label, FALSE, TRUE, 0);
   gtk_widget_show (label);

   adj_begin_day = (GtkAdjustment *) gtk_adjustment_new (now->tm_mday, 1.0, 31.0, 1.0,
						   5.0, 0.0);
   spinner_begin_day = gtk_spin_button_new (adj_begin_day, 0, 0);
   gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinner_begin_day), FALSE);
   gtk_spin_button_set_numeric(GTK_SPIN_BUTTON (spinner_begin_day), TRUE);
   gtk_spin_button_set_shadow_type (GTK_SPIN_BUTTON (spinner_begin_day),
				    SHADOW);
   gtk_box_pack_start (GTK_BOX (vbox_begin_day), spinner_begin_day, FALSE, TRUE, 0);

   vbox_begin_year = gtk_vbox_new (FALSE, 0);
   gtk_box_pack_start (GTK_BOX (hbox_begin_date), vbox_begin_year, FALSE, FALSE, 0);

   //Put year spinner in
   label = gtk_label_new ("Year:");
   gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
   gtk_box_pack_start (GTK_BOX (vbox_begin_year), label, FALSE, FALSE, 0);
   gtk_widget_show (label);

   adj_begin_year = (GtkAdjustment *) gtk_adjustment_new (now->tm_year+1900, 0.0, 2037.0,
							  1.0, 100.0, 0.0);
   spinner_begin_year = gtk_spin_button_new (adj_begin_year, 0, 0);
   gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinner_begin_year), FALSE);
   gtk_spin_button_set_numeric(GTK_SPIN_BUTTON (spinner_begin_year), TRUE);
   gtk_spin_button_set_shadow_type (GTK_SPIN_BUTTON (spinner_begin_year),
				    SHADOW);
   gtk_widget_set_usize (spinner_begin_year, 55, 0);
   gtk_box_pack_start (GTK_BOX (vbox_begin_year), spinner_begin_year, FALSE, FALSE, 0);

   //Put hours spinner in
   vbox_begin_hour = gtk_vbox_new (FALSE, 0);
   gtk_box_pack_start (GTK_BOX (hbox_begin_date), vbox_begin_hour, FALSE, FALSE, 0);

   label = gtk_label_new ("Hour:");
   gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
   gtk_box_pack_start (GTK_BOX (vbox_begin_hour), label, FALSE, FALSE, 0);
   gtk_widget_show (label);

   adj_begin_hour = (GtkAdjustment *) gtk_adjustment_new (now->tm_hour, 0.0, 23.0,
							  1.0, 1.0, 0.0);
   spinner_begin_hour = gtk_spin_button_new (adj_begin_hour, 0, 0);
   gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinner_begin_hour), FALSE);
   gtk_spin_button_set_numeric(GTK_SPIN_BUTTON (spinner_begin_hour), TRUE);
   gtk_spin_button_set_shadow_type (GTK_SPIN_BUTTON (spinner_begin_hour),
				    SHADOW);
   //gtk_widget_set_usize (spinner_begin_hour, 55, 0);
   gtk_box_pack_start (GTK_BOX (vbox_begin_hour), spinner_begin_hour, FALSE, TRUE, 0);

   //Put minutes spinner in
   vbox_begin_min = gtk_vbox_new (FALSE, 0);
   gtk_box_pack_start (GTK_BOX (hbox_begin_date), vbox_begin_min, FALSE, FALSE, 0);

   label = gtk_label_new ("Min:");
   gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
   gtk_box_pack_start (GTK_BOX (vbox_begin_min), label, FALSE, FALSE, 0);
   gtk_widget_show (label);

   adj_begin_min = (GtkAdjustment *) gtk_adjustment_new (now->tm_min, 0.0, 59.0,
							 1.0, 1.0, 0.0);
   spinner_begin_min = gtk_spin_button_new (adj_begin_min, 0, 0);
   gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinner_begin_min), FALSE);
   gtk_spin_button_set_numeric(GTK_SPIN_BUTTON (spinner_begin_min), TRUE);
   gtk_spin_button_set_shadow_type (GTK_SPIN_BUTTON (spinner_begin_min),
				    SHADOW);
   //gtk_widget_set_usize (spinner_begin_min, 55, 0);
   gtk_box_pack_start (GTK_BOX (vbox_begin_min), spinner_begin_min, FALSE, FALSE, 0);

   //End Begin spinners
   //Begin End spinners
   label = gtk_label_new ("End: ");
   gtk_misc_set_alignment (GTK_MISC (label), 1, 1);
   gtk_box_pack_start (GTK_BOX (vbox_begin_labels), label, FALSE, TRUE, 5);
   gtk_widget_show (label);

   //vbox_end_mon = gtk_vbox_new (FALSE, 0);
   //gtk_box_pack_start (GTK_BOX (hbox_end_date), vbox_end_mon, TRUE, TRUE, 5);

   //Put Month spinner in
   //label = gtk_label_new ("Month :");
   //gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
   //gtk_box_pack_start (GTK_BOX (vbox_begin_mon), label, FALSE, TRUE, 0);
   //gtk_widget_show (label);

   adj_end_mon = (GtkAdjustment *) gtk_adjustment_new (now->tm_mon+1, 1.0, 12.0, 1.0,
							 5.0, 0.0);
   spinner_end_mon = gtk_spin_button_new (adj_end_mon, 0, 0);
   gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinner_end_mon), FALSE);
   gtk_spin_button_set_numeric(GTK_SPIN_BUTTON (spinner_end_mon), TRUE);
   gtk_spin_button_set_shadow_type (GTK_SPIN_BUTTON (spinner_end_mon),
				    SHADOW);
   gtk_box_pack_start (GTK_BOX (vbox_begin_mon), spinner_end_mon, FALSE, TRUE, 0);

   //vbox_end_day = gtk_vbox_new (FALSE, 0);
   //gtk_box_pack_start (GTK_BOX (hbox_begin_date), vbox_begin_day, TRUE, TRUE, 5);

   //Put Day spinner in
   //label = gtk_label_new ("Day :");
   //gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
   //gtk_box_pack_start (GTK_BOX (vbox_begin_day), label, FALSE, TRUE, 0);
   //gtk_widget_show (label);

   adj_end_day = (GtkAdjustment *) gtk_adjustment_new (now->tm_mday, 1.0, 31.0, 1.0,
						   5.0, 0.0);
   spinner_end_day = gtk_spin_button_new (adj_end_day, 0, 0);
   gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinner_end_day), FALSE);
   gtk_spin_button_set_numeric(GTK_SPIN_BUTTON (spinner_end_day), TRUE);
   gtk_spin_button_set_shadow_type (GTK_SPIN_BUTTON (spinner_end_day),
				    SHADOW);
   gtk_box_pack_start (GTK_BOX (vbox_begin_day), spinner_end_day, FALSE, TRUE, 0);

   //vbox_end_year = gtk_vbox_new (FALSE, 0);
   //gtk_box_pack_start (GTK_BOX (hbox_begin_date), vbox_end_year, TRUE, TRUE, 5);

   //Put year spinner in
   //label = gtk_label_new ("Year :");
   //gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
   //gtk_box_pack_start (GTK_BOX (vbox_begin_year), label, FALSE, TRUE, 0);
   //gtk_widget_show (label);

   adj_end_year = (GtkAdjustment *) gtk_adjustment_new (now->tm_year+1900, 0.0, 2037.0,
							  1.0, 100.0, 0.0);
   spinner_end_year = gtk_spin_button_new (adj_end_year, 0, 0);
   gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinner_end_year), FALSE);
   gtk_spin_button_set_numeric(GTK_SPIN_BUTTON (spinner_end_year), TRUE);
   gtk_spin_button_set_shadow_type (GTK_SPIN_BUTTON (spinner_end_year),
				    SHADOW);
   gtk_widget_set_usize (spinner_end_year, 55, 0);
   gtk_box_pack_start (GTK_BOX (vbox_begin_year), spinner_end_year, FALSE, TRUE, 0);

   //Put hours spinner in
   //vbox_end_hour = gtk_vbox_new (FALSE, 0);
   //gtk_box_pack_start (GTK_BOX (hbox_end_date), vbox_end_hour, TRUE, TRUE, 5);

   //label = gtk_label_new ("Hour :");
   //gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
   //gtk_box_pack_start (GTK_BOX (vbox_end_hour), label, FALSE, TRUE, 0);
   //gtk_widget_show (label);

   adj_end_hour = (GtkAdjustment *) gtk_adjustment_new (now->tm_hour, 0.0, 23.0,
							  1.0, 1.0, 0.0);
   spinner_end_hour = gtk_spin_button_new (adj_end_hour, 0, 0);
   gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinner_end_hour), FALSE);
   gtk_spin_button_set_numeric(GTK_SPIN_BUTTON (spinner_end_hour), TRUE);
   gtk_spin_button_set_shadow_type (GTK_SPIN_BUTTON (spinner_end_hour),
				    SHADOW);
   //gtk_widget_set_usize (spinner_end_hour, 55, 0);
   gtk_box_pack_start (GTK_BOX (vbox_begin_hour), spinner_end_hour, FALSE, TRUE, 0);

   //Put minutes spinner in
   //vbox_end_min = gtk_vbox_new (FALSE, 0);
   //gtk_box_pack_start (GTK_BOX (hbox_end_date), vbox_end_min, TRUE, TRUE, 5);

   //label = gtk_label_new ("Min :");
   //gtk_misc_set_alignment (GTK_MISC (label), 0, 0.5);
   //gtk_box_pack_start (GTK_BOX (vbox_end_min), label, FALSE, TRUE, 0);
   //gtk_widget_show (label);

   adj_end_min = (GtkAdjustment *) gtk_adjustment_new (now->tm_min, 0.0, 59.0,
							 1.0, 1.0, 0.0);
   spinner_end_min = gtk_spin_button_new (adj_end_min, 0, 0);
   gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinner_end_min), FALSE);
   gtk_spin_button_set_numeric(GTK_SPIN_BUTTON (spinner_end_min), TRUE);
   gtk_spin_button_set_shadow_type (GTK_SPIN_BUTTON (spinner_end_min),
				    SHADOW);
   //gtk_widget_set_usize (spinner_end_min, 55, 0);
   gtk_box_pack_start (GTK_BOX (vbox_begin_min), spinner_end_min, FALSE, TRUE, 0);

   gtk_signal_connect(GTK_OBJECT(adj_begin_mon), "value_changed",
   		      GTK_SIGNAL_FUNC(cb_spin_mon_day_year), GINT_TO_POINTER(1));
   gtk_signal_connect(GTK_OBJECT(adj_begin_day), "value_changed",
   		      GTK_SIGNAL_FUNC(cb_spin_mon_day_year), GINT_TO_POINTER(1));
   gtk_signal_connect(GTK_OBJECT(adj_begin_year), "value_changed",
   		      GTK_SIGNAL_FUNC(cb_spin_mon_day_year), GINT_TO_POINTER(1));
   gtk_signal_connect(GTK_OBJECT(adj_begin_hour), "value_changed",
   		      GTK_SIGNAL_FUNC(cb_spin_mon_day_year), GINT_TO_POINTER(1));
   gtk_signal_connect(GTK_OBJECT(adj_begin_min), "value_changed",
   		      GTK_SIGNAL_FUNC(cb_spin_mon_day_year), GINT_TO_POINTER(1));
   gtk_signal_connect(GTK_OBJECT(adj_end_mon), "value_changed",
   		      GTK_SIGNAL_FUNC(cb_spin_mon_day_year), GINT_TO_POINTER(2));
   gtk_signal_connect(GTK_OBJECT(adj_end_day), "value_changed",
   		      GTK_SIGNAL_FUNC(cb_spin_mon_day_year), GINT_TO_POINTER(2));
   gtk_signal_connect(GTK_OBJECT(adj_end_year), "value_changed",
   		      GTK_SIGNAL_FUNC(cb_spin_mon_day_year), GINT_TO_POINTER(2));
   gtk_signal_connect(GTK_OBJECT(adj_end_hour), "value_changed",
   		      GTK_SIGNAL_FUNC(cb_spin_mon_day_year), GINT_TO_POINTER(2));
   gtk_signal_connect(GTK_OBJECT(adj_end_min), "value_changed",
   		      GTK_SIGNAL_FUNC(cb_spin_mon_day_year), GINT_TO_POINTER(2));

   //Text 1
   gtk_box_pack_start (GTK_BOX (vbox_detail), hbox_text1, TRUE, TRUE, 0);

   vbox_filler = gtk_vbox_new (FALSE, 0);
   gtk_box_pack_start (GTK_BOX (hbox_text1), vbox_filler, FALSE, FALSE, 2);
   gtk_widget_show (vbox_filler);

   text1 = gtk_text_new (NULL, NULL);
   gtk_text_set_editable (GTK_TEXT (text1), TRUE);
   gtk_text_set_word_wrap(GTK_TEXT(text1), TRUE);
   vscrollbar = gtk_vscrollbar_new (GTK_TEXT(text1)->vadj);
   gtk_box_pack_start(GTK_BOX(hbox_text1), text1, TRUE, TRUE, 0);
   gtk_box_pack_start(GTK_BOX(hbox_text1), vscrollbar, FALSE, FALSE, 0);
   gtk_widget_set_usize (GTK_WIDGET(text1), 255, 50);
   gtk_widget_show (text1);
   gtk_widget_show (vscrollbar);   
   
   vbox_filler = gtk_vbox_new (FALSE, 0);
   gtk_box_pack_start (GTK_BOX (hbox_text1), vbox_filler, FALSE, FALSE, 2);
   gtk_widget_show (vbox_filler);
   //Text 2
   gtk_box_pack_start (GTK_BOX (vbox_detail), hbox_text2, TRUE, TRUE, 0);

   vbox_filler = gtk_vbox_new (FALSE, 0);
   gtk_box_pack_start (GTK_BOX (hbox_text2), vbox_filler, FALSE, FALSE, 2);
   gtk_widget_show (vbox_filler);

   text2 = gtk_text_new (NULL, NULL);
   gtk_text_set_editable (GTK_TEXT (text2), TRUE);
   gtk_text_set_word_wrap(GTK_TEXT(text2), TRUE);
   vscrollbar = gtk_vscrollbar_new (GTK_TEXT(text2)->vadj);
   gtk_box_pack_start(GTK_BOX(hbox_text2), text2, TRUE, TRUE, 0);
   gtk_box_pack_start(GTK_BOX(hbox_text2), vscrollbar, FALSE, FALSE, 0);
   gtk_widget_set_usize (GTK_WIDGET(text2), 100, 50);
   gtk_widget_show (text2);
   gtk_widget_show (vscrollbar);   

   vbox_filler = gtk_vbox_new (FALSE, 0);
   gtk_box_pack_start (GTK_BOX (hbox_text2), vbox_filler, FALSE, FALSE, 2);
   gtk_widget_show (vbox_filler);

   hbox_notebook = gtk_hbox_new (FALSE, 0);
   gtk_box_pack_start (GTK_BOX (vbox_detail), hbox_notebook, FALSE, FALSE, 2);
   gtk_widget_show (hbox_notebook);

   //Add the notebook for repeat types
   notebook = gtk_notebook_new();
   gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_TOP);
   gtk_box_pack_start (GTK_BOX (hbox_notebook), notebook, FALSE, FALSE, 5);
   //labels for notebook
   notebook_tab1 = gtk_label_new("None");
   notebook_tab2 = gtk_label_new("Day");
   notebook_tab3 = gtk_label_new("Week");
   notebook_tab4 = gtk_label_new("Month");
   notebook_tab5 = gtk_label_new("Year");
   gtk_widget_show (notebook_tab1);
   gtk_widget_show (notebook_tab2);
   gtk_widget_show (notebook_tab3);
   gtk_widget_show (notebook_tab4);
   gtk_widget_show (notebook_tab5);
   //no repeat page for notebook
   label = gtk_label_new ("This event will not repeat");
   gtk_notebook_append_page (GTK_NOTEBOOK(notebook), label, notebook_tab1);
   gtk_widget_show (label);
   //end no repeat page

   //Day Repeat page for notebook
   vbox_repeat_day = gtk_vbox_new (FALSE, 0);
   hbox_repeat_day1 = gtk_hbox_new (FALSE, 0);
   hbox_repeat_day2 = gtk_hbox_new (FALSE, 0);
   gtk_box_pack_start (GTK_BOX(vbox_repeat_day), hbox_repeat_day1, FALSE, FALSE, 2);
   gtk_box_pack_start (GTK_BOX(vbox_repeat_day), hbox_repeat_day2, FALSE, FALSE, 2);
   label = gtk_label_new ("Every");
   gtk_widget_show (label);
   gtk_box_pack_start (GTK_BOX (hbox_repeat_day1), label, FALSE, FALSE, 2);
   repeat_day_entry = gtk_entry_new_with_max_length(2);
   gtk_widget_show (repeat_day_entry);
   gtk_widget_set_usize (repeat_day_entry, 30, 0);
   gtk_box_pack_start(GTK_BOX (hbox_repeat_day1), repeat_day_entry, FALSE, FALSE, 0);
   gtk_widget_show(repeat_day_entry);
   label = gtk_label_new ("Day(s)");
   gtk_widget_show (label);
   gtk_box_pack_start (GTK_BOX(hbox_repeat_day1), label, FALSE, FALSE, 2);
   //checkbutton
   check_button_day_endon = gtk_check_button_new_with_label ("End on");
   gtk_box_pack_start (GTK_BOX(hbox_repeat_day2), check_button_day_endon, FALSE, FALSE, 0);
   gtk_widget_show (check_button_day_endon);
   gtk_widget_show(hbox_repeat_day1);
   gtk_widget_show(hbox_repeat_day2);
   gtk_widget_show(vbox_repeat_day);
   gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			    vbox_repeat_day, notebook_tab2);
   //Begin spinners
   //month
   adj_endon_day_mon = (GtkAdjustment *) gtk_adjustment_new (now->tm_mon+1, 1.0, 12.0, 1.0,
							 5.0, 0.0);
   spinner_endon_day_mon = gtk_spin_button_new (adj_endon_day_mon, 0, 0);
   gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinner_endon_day_mon), FALSE);
   gtk_spin_button_set_numeric(GTK_SPIN_BUTTON (spinner_endon_day_mon), TRUE);
   gtk_spin_button_set_shadow_type (GTK_SPIN_BUTTON (spinner_endon_day_mon),
				    SHADOW);
   gtk_box_pack_start (GTK_BOX (hbox_repeat_day2),
		       spinner_endon_day_mon, FALSE, TRUE, 0);
   //Day
   adj_endon_day_day = (GtkAdjustment *) gtk_adjustment_new
     (now->tm_mday, 1.0, 31.0, 1.0, 5.0, 0.0);
   spinner_endon_day_day = gtk_spin_button_new (adj_endon_day_day, 0, 0);
   gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinner_endon_day_day), FALSE);
   gtk_spin_button_set_numeric(GTK_SPIN_BUTTON (spinner_endon_day_day), TRUE);
   gtk_spin_button_set_shadow_type (GTK_SPIN_BUTTON (spinner_endon_day_day),
				    SHADOW);
   gtk_box_pack_start (GTK_BOX (hbox_repeat_day2),
		       spinner_endon_day_day, FALSE, TRUE, 0);
   //Year
   adj_endon_day_year = (GtkAdjustment *) gtk_adjustment_new
     (now->tm_year+1900, 0.0, 2037.0, 1.0, 100.0, 0.0);
   spinner_endon_day_year = gtk_spin_button_new (adj_endon_day_year, 0, 0);
   gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinner_endon_day_year), FALSE);
   gtk_spin_button_set_numeric(GTK_SPIN_BUTTON (spinner_endon_day_year), TRUE);
   gtk_spin_button_set_shadow_type (GTK_SPIN_BUTTON (spinner_endon_day_year),
				    SHADOW);
   gtk_widget_set_usize (spinner_endon_day_year, 55, 0);
   gtk_box_pack_start (GTK_BOX (hbox_repeat_day2),
		       spinner_endon_day_year, FALSE, TRUE, 0);

   gtk_widget_show(spinner_endon_day_mon);
   gtk_widget_show(spinner_endon_day_day);
   gtk_widget_show(spinner_endon_day_year);
   //end spinners
   
   //The Week page
   vbox_repeat_week = gtk_vbox_new (FALSE, 0);
   hbox_repeat_week1 = gtk_hbox_new (FALSE, 0);
   hbox_repeat_week2 = gtk_hbox_new (FALSE, 0);
   hbox_repeat_week3 = gtk_hbox_new (FALSE, 0);
   gtk_notebook_append_page (GTK_NOTEBOOK(notebook), vbox_repeat_week, notebook_tab3);
   gtk_box_pack_start (GTK_BOX(vbox_repeat_week), hbox_repeat_week1, FALSE, FALSE, 2);
   gtk_box_pack_start (GTK_BOX(vbox_repeat_week), hbox_repeat_week2, FALSE, FALSE, 2);
   gtk_box_pack_start (GTK_BOX(vbox_repeat_week), hbox_repeat_week3, FALSE, FALSE, 2);
   label = gtk_label_new ("Every");
   gtk_widget_show (label);
   gtk_box_pack_start (GTK_BOX (hbox_repeat_week1), label, FALSE, FALSE, 2);
   repeat_week_entry = gtk_entry_new_with_max_length(2);
   gtk_widget_show (repeat_week_entry);
   gtk_widget_set_usize (repeat_week_entry, 30, 0);
   gtk_box_pack_start(GTK_BOX (hbox_repeat_week1), repeat_week_entry, FALSE, FALSE, 0);
   gtk_widget_show(repeat_week_entry);
   label = gtk_label_new ("Week(s)");
   gtk_widget_show (label);
   gtk_box_pack_start (GTK_BOX(hbox_repeat_week1), label, FALSE, FALSE, 2);
   //checkbutton
   check_button_week_endon = gtk_check_button_new_with_label ("End on");
   gtk_box_pack_start (GTK_BOX(hbox_repeat_week2), check_button_week_endon, FALSE, FALSE, 0);
   gtk_widget_show (check_button_week_endon);
   gtk_widget_show(hbox_repeat_week1);
   gtk_widget_show(hbox_repeat_week2);
   gtk_widget_show(hbox_repeat_week3);
   gtk_widget_show(vbox_repeat_week);

   //Begin spinners
   //month
   adj_endon_week_mon = (GtkAdjustment *) gtk_adjustment_new (now->tm_mon+1, 1.0, 12.0, 1.0,
							      5.0, 0.0);
   spinner_endon_week_mon = gtk_spin_button_new (adj_endon_week_mon, 0, 0);
   gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinner_endon_week_mon), FALSE);
   gtk_spin_button_set_numeric(GTK_SPIN_BUTTON (spinner_endon_week_mon), TRUE);
   gtk_spin_button_set_shadow_type (GTK_SPIN_BUTTON (spinner_endon_week_mon),
				    SHADOW);
   gtk_box_pack_start (GTK_BOX (hbox_repeat_week2), spinner_endon_week_mon, FALSE, TRUE, 0);
   //Day
   adj_endon_week_day = (GtkAdjustment *) gtk_adjustment_new (now->tm_mday, 1.0, 31.0, 1.0,
							      5.0, 0.0);
   spinner_endon_week_day = gtk_spin_button_new (adj_endon_week_day, 0, 0);
   gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinner_endon_week_day), FALSE);
   gtk_spin_button_set_numeric(GTK_SPIN_BUTTON (spinner_endon_week_day), TRUE);
   gtk_spin_button_set_shadow_type (GTK_SPIN_BUTTON (spinner_endon_week_day),
				    SHADOW);
   gtk_box_pack_start (GTK_BOX (hbox_repeat_week2), spinner_endon_week_day, FALSE, TRUE, 0);
   //Year
   adj_endon_week_year = (GtkAdjustment *) gtk_adjustment_new
     (now->tm_year+1900, 0.0, 2037.0, 1.0, 100.0, 0.0);
   spinner_endon_week_year = gtk_spin_button_new (adj_endon_week_year, 0, 0);
   gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinner_endon_week_year), FALSE);
   gtk_spin_button_set_numeric(GTK_SPIN_BUTTON (spinner_endon_week_year), TRUE);
   gtk_spin_button_set_shadow_type
     (GTK_SPIN_BUTTON (spinner_endon_week_year), SHADOW);
   gtk_widget_set_usize (spinner_endon_week_year, 55, 0);
   gtk_box_pack_start (GTK_BOX (hbox_repeat_week2), spinner_endon_week_year, FALSE, TRUE, 0);

   gtk_widget_show(spinner_endon_week_mon);
   gtk_widget_show(spinner_endon_week_day);
   gtk_widget_show(spinner_endon_week_year);
   //end spinners

   label = gtk_label_new ("Repeat on Days:");
   gtk_widget_show(label);
   gtk_box_pack_start (GTK_BOX (hbox_repeat_week3), label, FALSE, FALSE, 0);

   for (i=0; i<7; i++) {
      toggle_button_repeat_days[i] = 
	gtk_toggle_button_new_with_label(day_letters[i]);
      gtk_box_pack_start (GTK_BOX (hbox_repeat_week3),
			  toggle_button_repeat_days[i], FALSE, FALSE, 0);
      gtk_widget_show(toggle_button_repeat_days[i]);
   }

   //end week page

   
   //The Month page
   vbox_repeat_mon = gtk_vbox_new (FALSE, 0);
   hbox_repeat_mon1 = gtk_hbox_new (FALSE, 0);
   hbox_repeat_mon2 = gtk_hbox_new (FALSE, 0);
   hbox_repeat_mon3 = gtk_hbox_new (FALSE, 0);
   gtk_notebook_append_page (GTK_NOTEBOOK(notebook), vbox_repeat_mon, notebook_tab4);
   gtk_box_pack_start (GTK_BOX(vbox_repeat_mon), hbox_repeat_mon1, FALSE, FALSE, 2);
   gtk_box_pack_start (GTK_BOX(vbox_repeat_mon), hbox_repeat_mon2, FALSE, FALSE, 2);
   gtk_box_pack_start (GTK_BOX(vbox_repeat_mon), hbox_repeat_mon3, FALSE, FALSE, 2);
   label = gtk_label_new ("Every");
   gtk_widget_show (label);
   gtk_box_pack_start (GTK_BOX (hbox_repeat_mon1), label, FALSE, FALSE, 2);
   repeat_mon_entry = gtk_entry_new_with_max_length(2);
   gtk_widget_show (repeat_mon_entry);
   gtk_widget_set_usize (repeat_mon_entry, 30, 0);
   gtk_box_pack_start(GTK_BOX (hbox_repeat_mon1), repeat_mon_entry, FALSE, FALSE, 0);
   gtk_widget_show(repeat_mon_entry);
   label = gtk_label_new ("Month(s)");
   gtk_widget_show (label);
   gtk_box_pack_start (GTK_BOX(hbox_repeat_mon1), label, FALSE, FALSE, 2);
   //checkbutton
   check_button_mon_endon = gtk_check_button_new_with_label ("End on");
   gtk_box_pack_start (GTK_BOX(hbox_repeat_mon2), check_button_mon_endon, FALSE, FALSE, 0);
   gtk_widget_show (check_button_mon_endon);
   gtk_widget_show(hbox_repeat_mon1);
   gtk_widget_show(hbox_repeat_mon2);
   gtk_widget_show(hbox_repeat_mon3);
   gtk_widget_show(vbox_repeat_mon);

   //Begin spinners
   //month
   adj_endon_mon_mon = (GtkAdjustment *) gtk_adjustment_new (now->tm_mon+1, 1.0, 12.0, 1.0,
							      5.0, 0.0);
   spinner_endon_mon_mon = gtk_spin_button_new (adj_endon_mon_mon, 0, 0);
   gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinner_endon_mon_mon), FALSE);
   gtk_spin_button_set_numeric(GTK_SPIN_BUTTON (spinner_endon_mon_mon), TRUE);
   gtk_spin_button_set_shadow_type (GTK_SPIN_BUTTON (spinner_endon_mon_mon),
				    SHADOW);
   gtk_box_pack_start (GTK_BOX (hbox_repeat_mon2), spinner_endon_mon_mon, FALSE, TRUE, 0);
   //Day
   adj_endon_mon_day = (GtkAdjustment *) gtk_adjustment_new (now->tm_mday, 1.0, 31.0, 1.0,
							      5.0, 0.0);
   spinner_endon_mon_day = gtk_spin_button_new (adj_endon_mon_day, 0, 0);
   gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinner_endon_mon_day), FALSE);
   gtk_spin_button_set_numeric(GTK_SPIN_BUTTON (spinner_endon_mon_day), TRUE);
   gtk_spin_button_set_shadow_type (GTK_SPIN_BUTTON (spinner_endon_mon_day),
				    SHADOW);
   gtk_box_pack_start (GTK_BOX (hbox_repeat_mon2), spinner_endon_mon_day, FALSE, TRUE, 0);
   //Year
   adj_endon_mon_year = (GtkAdjustment *) gtk_adjustment_new
     (now->tm_year+1900, 0.0, 2037.0, 1.0, 100.0, 0.0);
   spinner_endon_mon_year = gtk_spin_button_new (adj_endon_mon_year, 0, 0);
   gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinner_endon_mon_year), FALSE);
   gtk_spin_button_set_numeric(GTK_SPIN_BUTTON (spinner_endon_mon_year), TRUE);
   gtk_spin_button_set_shadow_type
     (GTK_SPIN_BUTTON (spinner_endon_mon_year), SHADOW);
   gtk_widget_set_usize (spinner_endon_mon_year, 55, 0);
   gtk_box_pack_start (GTK_BOX (hbox_repeat_mon2), spinner_endon_mon_year, FALSE, TRUE, 0);

   gtk_widget_show(spinner_endon_mon_mon);
   gtk_widget_show(spinner_endon_mon_day);
   gtk_widget_show(spinner_endon_mon_year);
   //end spinners

   label = gtk_label_new ("Repeat by:");
   gtk_widget_show(label);
   gtk_box_pack_start (GTK_BOX (hbox_repeat_mon3), label, FALSE, FALSE, 0);

   toggle_button_repeat_mon_byday = gtk_radio_button_new_with_label
     (NULL, "Day");

   gtk_box_pack_start (GTK_BOX (hbox_repeat_mon3),
		       toggle_button_repeat_mon_byday, FALSE, FALSE, 0);
   gtk_widget_show(toggle_button_repeat_mon_byday);

   group = NULL;
   
   group = gtk_radio_button_group(GTK_RADIO_BUTTON(toggle_button_repeat_mon_byday));
   toggle_button_repeat_mon_bydate = gtk_radio_button_new_with_label
     (group, "Date");
   gtk_box_pack_start (GTK_BOX (hbox_repeat_mon3),
		       toggle_button_repeat_mon_bydate, FALSE, FALSE, 0);
   gtk_widget_show(toggle_button_repeat_mon_bydate);

   //end Month page

   //Repeat year page for notebook
   vbox_repeat_year = gtk_vbox_new (FALSE, 0);
   hbox_repeat_year1 = gtk_hbox_new (FALSE, 0);
   hbox_repeat_year2 = gtk_hbox_new (FALSE, 0);
   gtk_box_pack_start (GTK_BOX(vbox_repeat_year), hbox_repeat_year1, FALSE, FALSE, 2);
   gtk_box_pack_start (GTK_BOX(vbox_repeat_year), hbox_repeat_year2, FALSE, FALSE, 2);
   label = gtk_label_new ("Every");
   gtk_widget_show (label);
   gtk_box_pack_start (GTK_BOX (hbox_repeat_year1), label, FALSE, FALSE, 2);
   repeat_year_entry = gtk_entry_new_with_max_length(2);
   gtk_widget_show (repeat_year_entry);
   gtk_widget_set_usize (repeat_year_entry, 30, 0);
   gtk_box_pack_start(GTK_BOX (hbox_repeat_year1), repeat_year_entry, FALSE, FALSE, 0);
   gtk_widget_show(repeat_year_entry);
   label = gtk_label_new ("Year(s)");
   gtk_widget_show (label);
   gtk_box_pack_start (GTK_BOX(hbox_repeat_year1), label, FALSE, FALSE, 2);
   //checkbutton
   check_button_year_endon = gtk_check_button_new_with_label ("End on");
   gtk_box_pack_start (GTK_BOX(hbox_repeat_year2), check_button_year_endon, FALSE, FALSE, 0);
   gtk_widget_show (check_button_year_endon);
   gtk_widget_show(hbox_repeat_year1);
   gtk_widget_show(hbox_repeat_year2);
   gtk_widget_show(vbox_repeat_year);
   gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			    vbox_repeat_year, notebook_tab5);
   //Begin spinners
   //month
   adj_endon_year_mon = (GtkAdjustment *) gtk_adjustment_new (now->tm_mon+1, 1.0, 12.0, 1.0,
							 5.0, 0.0);
   spinner_endon_year_mon = gtk_spin_button_new (adj_endon_year_mon, 0, 0);
   gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinner_endon_year_mon), FALSE);
   gtk_spin_button_set_numeric(GTK_SPIN_BUTTON (spinner_endon_year_mon), TRUE);
   gtk_spin_button_set_shadow_type (GTK_SPIN_BUTTON (spinner_endon_year_mon),
				    SHADOW);
   gtk_box_pack_start (GTK_BOX (hbox_repeat_year2),
		       spinner_endon_year_mon, FALSE, TRUE, 0);
   //Day
   adj_endon_year_day = (GtkAdjustment *) gtk_adjustment_new
     (now->tm_mday, 1.0, 31.0, 1.0, 5.0, 0.0);
   spinner_endon_year_day = gtk_spin_button_new (adj_endon_year_day, 0, 0);
   gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinner_endon_year_day), FALSE);
   gtk_spin_button_set_numeric(GTK_SPIN_BUTTON (spinner_endon_year_day), TRUE);
   gtk_spin_button_set_shadow_type (GTK_SPIN_BUTTON (spinner_endon_year_day),
				    SHADOW);
   gtk_box_pack_start (GTK_BOX (hbox_repeat_year2),
		       spinner_endon_year_day, FALSE, TRUE, 0);
   //Year
   adj_endon_year_year = (GtkAdjustment *) gtk_adjustment_new
     (now->tm_year+1900, 0.0, 2037.0, 1.0, 100.0, 0.0);
   spinner_endon_year_year = gtk_spin_button_new (adj_endon_year_year, 0, 0);
   gtk_spin_button_set_wrap (GTK_SPIN_BUTTON (spinner_endon_year_year), FALSE);
   gtk_spin_button_set_numeric(GTK_SPIN_BUTTON (spinner_endon_year_year), TRUE);
   gtk_spin_button_set_shadow_type (GTK_SPIN_BUTTON (spinner_endon_year_year),
				    SHADOW);
   gtk_widget_set_usize (spinner_endon_year_year, 55, 0);
   gtk_box_pack_start (GTK_BOX (hbox_repeat_year2),
		       spinner_endon_year_year, FALSE, TRUE, 0);

   gtk_widget_show(spinner_endon_year_mon);
   gtk_widget_show(spinner_endon_year_day);
   gtk_widget_show(spinner_endon_year_year);
   //end spinners
 
   //end repeat year page

   gtk_notebook_set_page (GTK_NOTEBOOK(notebook), 0);
   
   gtk_widget_show(notebook);
   //end details

//   gtk_container_add (GTK_CONTAINER (window), hbox);
   
   gtk_widget_show (button);

//end

   gtk_widget_show (dow_hbox);
   gtk_widget_show (hbox2);
   gtk_widget_show (hbox_text1);
   gtk_widget_show (hbox_text2);
   gtk_widget_show (frame);

   gtk_widget_show (vbox_detail);
   gtk_widget_show (hbox_begin_date);
   gtk_widget_show (vbox_begin_labels);
   gtk_widget_show (vbox_begin_mon);
   gtk_widget_show (vbox_begin_day);
   gtk_widget_show (vbox_begin_year);
   gtk_widget_show (vbox_begin_min);
   gtk_widget_show (vbox_begin_hour);
   //gtk_widget_show (hbox_end_date);
   //gtk_widget_show (vbox_end_mon);
   //gtk_widget_show (vbox_end_day);
   //gtk_widget_show (vbox_end_year);
   //gtk_widget_show (vbox_end_min);
   //gtk_widget_show (vbox_end_hour);
   gtk_widget_show (spinner_begin_mon);
   gtk_widget_show (spinner_begin_day);
   gtk_widget_show (spinner_begin_year);
   gtk_widget_show (spinner_begin_hour);
   gtk_widget_show (spinner_begin_min);
   gtk_widget_show (spinner_end_mon);
   gtk_widget_show (spinner_end_day);
   gtk_widget_show (spinner_end_year);
   gtk_widget_show (spinner_end_hour);
   gtk_widget_show (spinner_end_min);

   gtk_widget_show (table);
   gtk_widget_show (hbox);
   gtk_widget_show (vbox);
   gtk_widget_show (vbox1);
   gtk_widget_show (vbox2);

   paint_calendar(NULL, GINT_TO_POINTER(CAL_INIT));

   update_dayview_screen();

   return 0;
}

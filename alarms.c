/* alarms.c
 * A module of J-Pilot http://jpilot.org
 * 
 * Copyright (C) 2000-2002 by Judd Montgomery
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

/*
 * The PalmOS datebook will alarm on private records even when they are hidden
 * and they show up on the screen.  Right, or wrong, who knows.
 * I will do the same.
 */
#include "config.h"
#include <gtk/gtk.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pi-datebook.h>

#include "utils.h"
#include "log.h"
#include "prefs.h"
#include "i18n.h"
#include "datebook.h"

/* This is how often to check for alarms in seconds */
/* Every call takes CPU time(not much), so you may want it to be greater */
#define ALARM_INTERVAL 1


#define PREV_ALARM_MASK 1
#define NEXT_ALARM_MASK 2

/* #define ALARMS_DEBUG */


/*
 * Throughout the code, event_time is the time of the event
 * alarm_time is the event_time - advance
 * remind_time is the time a window is to be popped up (it may be postponed)
 */

typedef enum {
   ALARM_NONE = 0,
   ALARM_NEW,
   ALARM_MISSED,
   ALARM_POSTPONED
} AlarmType;

struct jp_alarms {
   unsigned int unique_id;
   AlarmType type;
   time_t event_time;
   time_t alarm_advance;
   struct jp_alarms *next;
};

struct alarm_dialog_data {
   unsigned int unique_id;
   time_t remind_time;
   GtkWidget *remind_entry;
   GtkWidget *radio1;
   GtkWidget *radio2;
   int button_hit;
};

static struct jp_alarms *alarm_list=NULL;
static struct jp_alarms *Plast_alarm_list=NULL;
static struct jp_alarms *next_alarm=NULL;

static int glob_skip_all_alarms;
static int total_alarm_windows;

void alarms_add_to_list(unsigned int unique_id,
			AlarmType type,
			time_t alarm_time,
			time_t alarm_advance);
int alarms_find_next(struct tm *date1, struct tm *date2, int soonest_only);

/*
 * Alarm GUI
 */
/*
 * Start of Dialog window code
 */
static void cb_dialog_button(GtkWidget *widget,
			     gpointer   data)
{
   struct alarm_dialog_data *Pdata;
   GtkWidget *w;

   w=gtk_widget_get_toplevel(widget);
   Pdata = gtk_object_get_data(GTK_OBJECT(w), "alarm");
   if (Pdata) {
      Pdata->button_hit = GPOINTER_TO_INT(data);
   }
   gtk_widget_destroy(GTK_WIDGET(w));
}

static gboolean cb_destroy_dialog(GtkWidget *widget)
{
   struct alarm_dialog_data *Pdata;
   const char *entry;
   time_t ltime;
   time_t advance;
   time_t remind;

   total_alarm_windows--;
#ifdef ALARMS_DEBUG
   printf("total_alarm_windows=%d\n",total_alarm_windows);
#endif
   Pdata = gtk_object_get_data(GTK_OBJECT(widget), "alarm");
   if (!Pdata) {
      return TRUE;
   }
   entry = gtk_entry_get_text(GTK_ENTRY(Pdata->remind_entry));
   if (Pdata->button_hit==DIALOG_SAID_2) {
      remind = atoi(entry);
      jp_logf(JP_LOG_DEBUG, "remind entry = [%s]\n", entry);
      set_pref(PREF_REMIND_IN, 0, entry, TRUE);
      if (GTK_TOGGLE_BUTTON(Pdata->radio1)->active) {
	 set_pref(PREF_REMIND_UNITS, 0, NULL, TRUE);
	 remind *= 60;
      } else {
	 set_pref(PREF_REMIND_UNITS, 1, NULL, TRUE);
	 remind *= 3600;
      }
      time(&ltime);
      localtime(&ltime);
      advance = -(ltime + remind - Pdata->remind_time);
      alarms_add_to_list(Pdata->unique_id,
			 ALARM_POSTPONED,
			 Pdata->remind_time,
			 advance);
   }
   free(Pdata);

   return TRUE;
}

int dialog_alarm(char *title, char *frame_text,
		 char *time_str, char *desc_str, char *note_str,
		 unsigned int unique_id,
		 time_t remind_time)
{
   GSList *group;
   GtkWidget *button, *label;
   GtkWidget *hbox1, *vbox1;
   GtkWidget *vbox_temp;
   GtkWidget *frame;
   GtkWidget *alarm_dialog;
   GtkWidget *remind_entry;
   GtkWidget *radio1;
   GtkWidget *radio2;
   struct alarm_dialog_data *Pdata;
   long pref_units;
   const char *pref_entry;

   /* Prevent alarms from going crazy and using all resources */
   if (total_alarm_windows>20) {
      return -1;
   }
   total_alarm_windows++;
#ifdef ALARMS_DEBUG
   printf("total_alarm_windows=%d\n",total_alarm_windows);
#endif
   alarm_dialog = gtk_widget_new(GTK_TYPE_WINDOW,
				 "type", GTK_WINDOW_TOPLEVEL,
				 "title", title,
				 NULL);

   gtk_signal_connect(GTK_OBJECT(alarm_dialog), "destroy",
                      GTK_SIGNAL_FUNC(cb_destroy_dialog), alarm_dialog);


   frame = gtk_frame_new(frame_text);
   gtk_frame_set_label_align(GTK_FRAME(frame), 0.5, 0.0);
   vbox1 = gtk_vbox_new(FALSE, 2);
   hbox1 = gtk_hbox_new(TRUE, 2);

   gtk_container_set_border_width(GTK_CONTAINER(frame), 5);
   gtk_container_set_border_width(GTK_CONTAINER(vbox1), 5);
   gtk_container_set_border_width(GTK_CONTAINER(hbox1), 5);

   gtk_container_add(GTK_CONTAINER(alarm_dialog), frame);
   gtk_container_add(GTK_CONTAINER(frame), vbox1);

   /* Label */
   label = gtk_label_new(time_str);
   gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
   gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
   gtk_box_pack_start(GTK_BOX(vbox1), label, FALSE, FALSE, 2);

   /* Label */
   label = gtk_label_new(desc_str);
   gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
   gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
   gtk_box_pack_start(GTK_BOX(vbox1), label, FALSE, FALSE, 2);

   /* Label */
   label = gtk_label_new(note_str);
   gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
   gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
   gtk_box_pack_start(GTK_BOX(vbox1), label, FALSE, FALSE, 2);

   /* Buttons */
   gtk_box_pack_start(GTK_BOX(vbox1), hbox1, TRUE, TRUE, 2);

   button = gtk_button_new_with_label(_("OK"));
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_dialog_button),
		      GINT_TO_POINTER(DIALOG_SAID_1));
   gtk_box_pack_start(GTK_BOX(hbox1), button, TRUE, TRUE, 1);

   button = gtk_button_new_with_label(_("Remind me"));
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_dialog_button),
		      GINT_TO_POINTER(DIALOG_SAID_2));
   gtk_box_pack_start(GTK_BOX(hbox1), button, TRUE, TRUE, 1);

   remind_entry = gtk_entry_new_with_max_length(4);
   gtk_box_pack_start(GTK_BOX(hbox1), remind_entry, TRUE, TRUE, 1);

   group = NULL;
   radio1 = gtk_radio_button_new_with_label(NULL, _("Minutes"));
   group = gtk_radio_button_group(GTK_RADIO_BUTTON(radio1));
   radio2 = gtk_radio_button_new_with_label(group, _("Hours"));
   group = gtk_radio_button_group(GTK_RADIO_BUTTON(radio2));

   gtk_widget_set_usize(GTK_WIDGET(remind_entry), 5, 0);
   gtk_widget_set_usize(GTK_WIDGET(radio1), 5, 0);
   gtk_widget_set_usize(GTK_WIDGET(radio2), 5, 0);

   Pdata = malloc(sizeof(struct alarm_dialog_data));
   if (Pdata) {
      Pdata->unique_id = unique_id;
      Pdata->remind_time = remind_time;
      /* Set the default button pressed to OK */
      Pdata->button_hit = DIALOG_SAID_1;     
      Pdata->remind_entry=remind_entry;
      Pdata->radio1=radio1;
      Pdata->radio2=radio2;
   }
   gtk_object_set_data(GTK_OBJECT(alarm_dialog), "alarm", Pdata);                    

   get_pref(PREF_REMIND_IN, NULL, &pref_entry);
   gtk_entry_set_text(GTK_ENTRY(remind_entry), pref_entry);

   get_pref(PREF_REMIND_UNITS, &pref_units, NULL);
   if (pref_units) {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio2), TRUE);
   } else {
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio1), TRUE);
   }

   vbox_temp = gtk_vbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(hbox1), vbox_temp, TRUE, TRUE, 1);
   gtk_box_pack_start(GTK_BOX(vbox_temp), radio1, TRUE, TRUE, 1);
   gtk_box_pack_start(GTK_BOX(vbox_temp), radio2, TRUE, TRUE, 1);

   gtk_widget_show_all(alarm_dialog);

   return 0;
}
/*
 * End Alarm GUI
 */

#ifdef ALARMS_DEBUG
const char *print_date(const time_t t1)
{
   struct tm *Pnow;
   static char str[100];

   Pnow = localtime(&t1);
   strftime(str, 80, "%B %d, %Y %H:%M:%S", Pnow);
   return str;
}
const char *print_type(AlarmType type)
{
   switch (type) {
    case ALARM_NONE:
      return "ALARM_NONE";
    case ALARM_NEW:
      return "ALARM_NEW";
    case ALARM_MISSED:
      return "ALARM_MISSED";
    case ALARM_POSTPONED:
      return "ALARM_POSTPONED";
    default:
      return "? ALARM_UNKNOWN";
   }
}
#else
inline const char *print_date(const time_t t1)
{
   return "";
}
inline const char *print_type(AlarmType type)
{
   return "";
}
#endif


void alarms_add_to_list(unsigned int unique_id,
			AlarmType type,
			time_t event_time,
			time_t alarm_advance)
{
   struct jp_alarms *temp_alarm;

#ifdef ALARMS_DEBUG
   printf("alarms_add_to_list()\n");
#endif

   temp_alarm = malloc(sizeof(struct jp_alarms));
   if (!temp_alarm) {
      jp_logf(JP_LOG_WARN, "alarms_add_to_list: Out of memory\n");
      return;
   }
   temp_alarm->unique_id = unique_id;
   temp_alarm->type = type;
   temp_alarm->event_time = event_time;
   temp_alarm->alarm_advance = alarm_advance;
   temp_alarm->next = NULL;
   if (Plast_alarm_list) {
      Plast_alarm_list->next=temp_alarm;
      Plast_alarm_list=temp_alarm;
   } else {
      alarm_list=Plast_alarm_list=temp_alarm;
   }
   Plast_alarm_list=temp_alarm;
}

void alarms_remove_from_to_list(unsigned int unique_id)
{
   struct jp_alarms *temp_alarm, *prev_alarm, *next_alarm;

#ifdef ALARMS_DEBUG
   printf("remove from list(%d)\n", unique_id);
#endif
   for(prev_alarm=NULL, temp_alarm=alarm_list;
       temp_alarm;
       temp_alarm=next_alarm) {
      if (temp_alarm->unique_id==unique_id) {
	 /* Tail of list? */
	 if (temp_alarm->next==NULL) {
	    Plast_alarm_list=prev_alarm;
	 }
	 /* Last of list? */
	 if (Plast_alarm_list==alarm_list) {
	    Plast_alarm_list=alarm_list=NULL;
	 }
	 if (prev_alarm) {
	    prev_alarm->next=temp_alarm->next;
	 } else {
	    /* Head of list */
	    alarm_list=temp_alarm->next;
	 }
	 free(temp_alarm);
	 return;
      } else {
	 prev_alarm=temp_alarm;
	 next_alarm=temp_alarm->next;
      }
   }
}

void free_alarms_list(int mask)
{
   struct jp_alarms *ta, *ta_next;

   if (mask&PREV_ALARM_MASK) {
      for (ta=alarm_list; ta; ta=ta_next) {
	 ta_next=ta->next;
	 free(ta);
      }
      Plast_alarm_list=alarm_list=NULL;
   }

   if (mask&NEXT_ALARM_MASK) {
      for (ta=next_alarm; ta; ta=ta_next) {
	 ta_next=ta->next;
	 free(ta);
      }
      next_alarm=NULL;
   }
}

void alarms_write_file(void)
{
   FILE *out;
   char line[256];
   int fail, n;
   time_t ltime;
   struct tm *now;

   jp_logf(JP_LOG_DEBUG, "alarms_write_file()\n");

   time(&ltime);
   now = localtime(&ltime);

   out=jp_open_home_file(EPN".alarms.tmp", "w");
   if (!out) {
      jp_logf(JP_LOG_WARN, "Could not open "EPN".alarms.tmp file\n");
      return;
   }
   fail=0;
   strncpy(line, 
	   "# This file was generated by "EPN", changes will be lost\n",
	   255);
   n = fwrite(line, strlen(line), 1, out);
   if (n<1) fail=1;

   strncpy(line, 
	   "# This is the last time that "EPN" was ran\n",
	   255);
   n = fwrite(line, strlen(line), 1, out);
   if (n<1) fail=1;

   sprintf(line, "UPTODATE %d %d %d %d %d\n",
	   now->tm_year+1900,
	   now->tm_mon+1,
	   now->tm_mday,
	   now->tm_hour,
	   now->tm_min
	   );
   n = fwrite(line, strlen(line), 1, out);
   if (n<1) fail=1;

   fclose(out);

   if (fail) {
      unlink_file(EPN".alarms.tmp");
   } else {
      rename_file(EPN".alarms.tmp", EPN".alarms");
   }
}

/* This attempts to make the command safe.
 * I'm sure I'm missing things.
 */
void make_command_safe(char *command)
{
   int i, len;
   char c;

   len = strlen(command);
   for (i=0; i<len; i++) {
      c=command[i];
      if (strchr("\r\n|&;()<>", c)) {
	 command[i]=' ';
      }
   }
}

/*
 * Pop up window.
 * Do alarm setting (play sound, or whatever).
 * if user postpones then put in postponed alarm list.
 */
int alarms_do_one(struct Appointment *a,
		  unsigned long unique_id,
		  time_t t_alarm,
		  AlarmType type)
{
   struct tm *Pnow;
   char time_str[255];
   char desc_str[255];
   char note_str[255];
   char pref_time[50];
   char time1_str[50];
   char time2_str[50];
   char date_str[50];
   char command[1024];
   char *reason;
   long ivalue;
   long wants_windows;
   long do_command;
   const char *pref_date;
   const char *pref_command;
   char c1, c2;
   int i, len;

   alarms_write_file();

   switch (type) {
    case ALARM_NONE:
      return 0;
    case ALARM_NEW:
      reason=_("Appointment Reminder");
      break;
    case ALARM_MISSED:
      reason=_("Past Appointment");
      break;
    case ALARM_POSTPONED:
      reason=_("Postponed Appointment");
      break;
    default:
      reason=_("Appointment");
   }
   get_pref(PREF_SHORTDATE, &ivalue, &pref_date);
   get_pref_time_no_secs(pref_time);

   Pnow = localtime(&t_alarm);

   strftime(date_str, 50, pref_date, Pnow);
   strftime(time1_str, 50, pref_time, Pnow);
   strftime(time2_str, 50, pref_time, &(a->end));
   sprintf(time_str, "%s %s-%s\n", date_str, time1_str, time2_str);
   desc_str[0]='\0';
   note_str[0]='\0';
   if (a->description) {
      strncpy(desc_str, a->description, 200);
      desc_str[200]='\0';
   }
   if (a->note) {
      strncpy(note_str, a->note, 200);
      note_str[200]='\0';
   }

   get_pref(PREF_ALARM_COMMAND, &ivalue, &pref_command);
   get_pref(PREF_DO_ALARM_COMMAND, &do_command, NULL);
#ifdef ALARMS_DEBUG
   printf("pref_command = [%s]\n", pref_command);
#endif
   bzero(command, 1024);
   if (do_command) {
      command[0]='\0';
      for (i=0; i<MAX_PREF_VALUE-1; i++) {
	 c1 = pref_command[i];
	 c2 = pref_command[i+1];
	 len = strlen(command);
	 /* expand '%t' */
	 if (c1=='%') {
	    if (c2=='t') {
	       i++;
	       strncat(command, time1_str, 1022-len);
	       continue;
	    }
	    /* expand '%d' */
	    if (c2=='d') {
	       i++;
	       strncat(command, date_str, 1022-len);
	       continue;
	    }
#ifdef ENABLE_ALARM_SHELL_DANGER
	    /* expand '%D' */
	    if (c2=='D') {
	       i++;
	       strncat(command, desc_str, 1022-len);
	       continue;
	    }
	    if (c2=='N') {
	       i++;
	       strncat(command, note_str, 1022-len);
	       continue;
	    }
#endif
	 }
	 if (len<1020) {
	    command[len++]=c1;
	    command[len]='\0';
	 }
	 if (c1=='\0') {
	    break;
	 }
      }
      command[1022]='\0';

      make_command_safe(command);
      jp_logf(JP_LOG_STDOUT|JP_LOG_FILE, "executing command = [%s]\n", command);
      system(command);
   }

   get_pref(PREF_OPEN_ALARM_WINDOWS, &wants_windows, NULL);

   if (wants_windows) {
      return dialog_alarm("J-Pilot Alarm", reason,
			  time_str, desc_str, note_str,
			  unique_id,
			  t_alarm);
   }
   return 0;
}


/*
 * See if next_alarm is due in less than ALARM_INTERVAL/2 secs.
 * If it is, then do_alarm and find_next_alarm.
 */
gint cb_timer_alarms(gpointer data)
{
   struct jp_alarms *temp_alarm, *ta_next;
   AppointmentList *a_list;
   AppointmentList *temp_al;
   static int first=0;
   time_t t, diff;
   time_t t_alarm_time;
   struct tm *Ptm;
   struct tm copy_tm;

   a_list=NULL;

   if (!first) {
      alarms_write_file();
      first=1;
   }

   time(&t);

   for (temp_alarm=alarm_list; temp_alarm; temp_alarm=ta_next) {
      ta_next=temp_alarm->next;
      diff = temp_alarm->event_time - t - temp_alarm->alarm_advance;
      if (temp_alarm->type!=ALARM_MISSED) {
	 if (diff >= ALARM_INTERVAL/2) {
	    continue;
	 }
      }
      if (a_list==NULL) {
	 get_days_appointments2(&a_list, NULL, 0, 0, 1);
      }
#ifdef ALARMS_DEBUG
      printf("unique_id=%d\n", temp_alarm->unique_id);
      printf("type=%s\n", print_type(temp_alarm->type));
      printf("event_time=%s\n", print_date(temp_alarm->event_time));
      printf("alarm_advance=%ld\n", temp_alarm->alarm_advance);
#endif
      for (temp_al = a_list; temp_al; temp_al=temp_al->next) {
	 if (temp_al->ma.unique_id == temp_alarm->unique_id) {
#ifdef ALARMS_DEBUG
	    printf("%s\n", temp_al->ma.a.description);
#endif
	    alarms_do_one(&(temp_al->ma.a),
			  temp_alarm->unique_id,
			  temp_alarm->event_time,
			  ALARM_MISSED);
	    break;
	 }
      }
      /* Be careful, this modifies the list we are parsing and
       removes the current node */
      alarms_remove_from_to_list(temp_al->ma.unique_id);
   }

   if (next_alarm) {
      diff = next_alarm->event_time - t - next_alarm->alarm_advance;
      if (diff <= ALARM_INTERVAL/2) {
	 if (a_list==NULL) {
	    get_days_appointments2(&a_list, NULL, 0, 0, 1);
	 }
	 for (temp_alarm=next_alarm; temp_alarm; temp_alarm=ta_next) {
	    for (temp_al = a_list; temp_al; temp_al=temp_al->next) {
	       if (temp_al->ma.unique_id == temp_alarm->unique_id) {
#ifdef ALARMS_DEBUG
		  printf("** next unique_id=%d\n", temp_alarm->unique_id);
		  printf("** next type=%s\n", print_type(temp_alarm->type));
		  printf("** next event_time=%s\n", print_date(temp_alarm->event_time));
		  printf("** next alarm_advance=%ld\n", temp_alarm->alarm_advance);
		  printf("** next %s\n", temp_al->ma.a.description);
#endif		  
		  alarms_do_one(&(temp_al->ma.a),
				temp_alarm->unique_id,
				temp_alarm->event_time,
				ALARM_NEW);
		  break;
	       }
	    }
	    /* This may not be exactly right */
	    t_alarm_time = temp_alarm->event_time + 1;
#ifdef ALARMS_DEBUG
	    printf("** t_alarm_time-->%s\n", print_date(t_alarm_time));
#endif
	    ta_next=temp_alarm->next;
	    free(temp_alarm);
	    next_alarm = ta_next;
	 }
	 Ptm = localtime(&t_alarm_time);
	 memcpy(&copy_tm, Ptm, sizeof(struct tm));
	 alarms_find_next(&copy_tm, &copy_tm, TRUE);
      }
   }
   if (a_list) {
      free_AppointmentList(&a_list);
   }

   return TRUE;
}

/*
 * This routine takes time (t) and either advances t to the next
 * occurence of a repeating appointment, or the previous occurence
 * 
 * a is the appointment passed in
 * t is an in/out param.
 * fdom is first day of month (Sunday being 0)
 * ndim is the number of days in the month
 * forward_or_backward should be -1 for backward or 1 for forward.
 */
int forward_backward_in_appt_time(const struct Appointment *a,
				  struct tm *t,
				  int fdom, int ndim,
				  int forward_or_backward)
{
   int count, dow, freq;

   freq = a->repeatFrequency;

   /* Go forward in time */
   if (forward_or_backward==1) {
      switch (a->repeatType) {
       case repeatNone:
	 break;
       case repeatDaily:
	 break;
       case repeatWeekly:
	 for (count=0, dow=t->tm_wday; count<14; count++) {
	    add_days_to_date(t, 1);
#ifdef ALARMS_DEBUG      
	    printf("fpn: weekly forward t.tm_wday=%d, freq=%d\n", t->tm_wday, freq);
#endif
	    dow++;
	    if (dow==7) {
#ifdef ALARMS_DEBUG      
	       printf("fpn: dow==7\n");
#endif
	       add_days_to_date(t, (freq-1)*7);
	       dow=0;
	    }
	    if (a->repeatDays[dow]) {
#ifdef ALARMS_DEBUG      
	       printf("fpn: repeatDay[dow] dow=%d\n", dow);
#endif
	       break;
	    }
	 }
	 break;
       case repeatMonthlyByDay:
	 add_months_to_date(t, freq);
	 get_month_info(t->tm_mon, 1, t->tm_year, &fdom, &ndim);
	 t->tm_mday=((a->repeatDay+7-fdom)%7) - ((a->repeatDay)%7) + a->repeatDay + 1;
	 if (t->tm_mday > ndim-1) {
	    t->tm_mday -= 7;
	 }
	 break;
       case repeatMonthlyByDate:
	 t->tm_mday=a->begin.tm_mday;
	 add_months_to_date(t, freq);
	 break;
       case repeatYearly:
	 t->tm_mday=a->begin.tm_mday;
	 add_years_to_date(t, freq);
	 break;
      }/*switch */
      return 0;
   }
   /* Go back in time */
   if (forward_or_backward==-1) {
      switch (a->repeatType) {
       case repeatNone:
	 break;
       case repeatDaily:
	 break;
       case repeatWeekly:
	 for (count=0, dow=t->tm_wday; count<14; count++) {
	    sub_days_from_date(t, 1);
#ifdef ALARMS_DEBUG      
	    printf("fpn: weekly backward t.tm_wday=%d, freq=%d\n", t->tm_wday, freq);
#endif
	    dow--;
	    if (dow==-1) {
#ifdef ALARMS_DEBUG      
	       printf("fpn: dow==-1\n");
#endif
	       sub_days_from_date(t, (freq-1)*7);
	       dow=6;
	    }
	    if (a->repeatDays[dow]) {
#ifdef ALARMS_DEBUG      
	       printf("fpn: repeatDay[dow] dow=%d\n", dow);
#endif
	       break;
	    }
	 }
	 break;
       case repeatMonthlyByDay:
	 sub_months_from_date(t, freq);
	 get_month_info(t->tm_mon, 1, t->tm_year, &fdom, &ndim);
	 t->tm_mday=((a->repeatDay+7-fdom)%7) - ((a->repeatDay)%7) + a->repeatDay + 1;
	 if (t->tm_mday > ndim-1) {
	    t->tm_mday -= 7;
	 }
	 break;
       case repeatMonthlyByDate:
	 t->tm_mday=a->begin.tm_mday;
	 sub_months_from_date(t, freq);
	 break;
       case repeatYearly:
	 t->tm_mday=a->begin.tm_mday;
	 sub_years_from_date(t, freq);
	 break;
      }/*switch */
   }
   return 0;
}

static int find_prev_next(struct Appointment *a,
			  int adv,
			  struct tm *date1,
			  struct tm *date2,
			  struct tm *tm_prev,
			  struct tm *tm_next,
			  int *prev_found,
			  int *next_found)
{
   struct tm t;
   struct tm *Pnow;
   time_t t_temp;
   time_t t1, t2;
   time_t t_begin, t_end;
   time_t t_alarm;
   time_t t_past;
   time_t t_future;
   time_t t_interval;
   int forward, backward;
   int offset;
   int freq;
   int found;
   int count;
   int i;
   int safety_counter;
   int fdom, ndim;
   long fdow;
   int days, begin_days;
   int found_exception;

#ifdef ALARMS_DEBUG
   printf("fpn: entered find_previous_next\n");
#endif
   *prev_found=*next_found=0;
   forward=backward=1;

   t1=mktime(date1);
   t2=mktime(date2);

   bzero(tm_prev, sizeof(struct tm));
   bzero(tm_next, sizeof(struct tm));

   bzero(&t, sizeof(struct tm));
   t.tm_year=a->begin.tm_year;
   t.tm_mon=a->begin.tm_mon;
   t.tm_mday=a->begin.tm_mday;
   t.tm_hour=a->begin.tm_hour;
   t.tm_min=a->begin.tm_min;
   t.tm_isdst=-1;

   freq = 0;
   switch (a->repeatType) {
    case repeatNone:
#ifdef ALARMS_DEBUG
      printf("fpn: repeatNone\n");
#endif
      t_alarm=mktime(&(a->begin)) - adv;
      if ((t_alarm < t2) && (t_alarm > t1)) {
	 memcpy(tm_prev, &(a->begin), sizeof(struct tm));
	 *prev_found=1;
#ifdef ALARMS_DEBUG
	 printf("fpn: prev_found none\n");
#endif
      } else if (t_alarm > t2) {
	 memcpy(tm_next, &(a->begin), sizeof(struct tm));
	 *next_found=1;
#ifdef ALARMS_DEBUG
	 printf("fpn: next_found none\n");
#endif
      }
      forward=backward=0;
      break;
    case repeatDaily:
#ifdef ALARMS_DEBUG
      printf("fpn: repeatDaily\n");
#endif
      freq = a->repeatFrequency;
      t_interval = a->repeatFrequency * 86400;
      if (t_interval==0) t_interval=1;
      t_alarm = mktime(&t);
      if ((t2 + adv) > t_alarm) {
	 t_past = ((t2 + adv - t_alarm) / t_interval) *t_interval + t_alarm;
	 t_future = (((t2 + adv - t_alarm) / t_interval) + 1) *t_interval + t_alarm;
	 *prev_found=*next_found=1;
      } else {
	 t_future = t_alarm;
	 *next_found=1;
      }
      Pnow = localtime(&t_past);
      memcpy(tm_prev, Pnow, sizeof(struct tm));
      Pnow = localtime(&t_future);
      memcpy(tm_next, Pnow, sizeof(struct tm));
      forward=backward=0;
#ifdef ALARMS_DEBUG
	{
	   char str[100];
	   strftime(str, 80, "%B %d, %Y %H:%M", tm_prev);
	   printf("fpn: daily tm_prev=%s\n", str);
	   strftime(str, 80, "%B %d, %Y %H:%M", tm_next);
	   printf("fpn: daily tm_next=%s\n", str);
	}
#endif
      break;
    case repeatWeekly:
#ifdef ALARMS_DEBUG
      printf("fpn: repeatWeekly\n");
#endif
      freq = a->repeatFrequency;
      t.tm_year=date2->tm_year;
      t.tm_mon=date2->tm_mon;
      t.tm_mday=date2->tm_mday;
      mktime(&t);
      begin_days = dateToDays(&(a->begin));
      days = dateToDays(&t);
#ifdef ALARMS_DEBUG
      printf("fpn: begin_days %d days %d\n", begin_days, days);
      printf("fpn: t.tm_wday %d a->begin.tm_wday %d\n", t.tm_wday, a->begin.tm_wday);
#endif
      get_pref(PREF_FDOW, &fdow, NULL);

      /* Offset is how many weeks we are off of an iteration */
      offset = ((int)((days - t.tm_wday) - (begin_days - a->begin.tm_wday))/7)%freq;
#ifdef ALARMS_DEBUG
      printf("fpn: offset %d\n", offset);
#endif
      if (offset > 0) {
	 sub_days_from_date(&t, offset*7);
      } else {
	 add_days_to_date(&t, offset*-7);
      }
      found=0;
      for (count=0, i=t.tm_wday; i>=0; i--, count++) {
	 if (a->repeatDays[i]) {
	    sub_days_from_date(&t, count);
	    found=1;
#ifdef ALARMS_DEBUG
	      {
		 char str[100];
		 strftime(str, 80, "%B %d, %Y %H:%M", &t);
		 printf("fpn: initial weekly=%s\n", str);
	      }
#endif
	    break;
	 }
      }
      if (!found) {
	 for (count=0, i=t.tm_wday; i<7; i++, count++) {
	    if (a->repeatDays[i]) {
	       add_days_to_date(&t, count);
	       found=1;
	       break;
	    }
	 }
      }
      break;
    case repeatMonthlyByDay:
#ifdef ALARMS_DEBUG
      printf("fpn: repeatMonthlyByDay\n");
#endif
      t.tm_mon=date2->tm_mon;
      t.tm_year=date2->tm_year;
      freq = a->repeatFrequency;
#ifdef ALARMS_DEBUG
      printf("fpn: freq=%d\n", freq);
#endif
      offset = ((t.tm_year - a->begin.tm_year)*12 +
		(t.tm_mon - a->begin.tm_mon))%(a->repeatFrequency);
      /* This will adjust for leap year, and exceeding the end of the month */
      if (((t.tm_year - a->begin.tm_year)*12 + (t.tm_mon - a->begin.tm_mon)) < 0) {
	 add_months_to_date(&t, offset);
      } else {
	 sub_months_from_date(&t, offset);
      }
      get_month_info(t.tm_mon, 1, t.tm_year, &fdom, &ndim);
      t.tm_mday=((a->repeatDay+7-fdom)%7) - ((a->repeatDay)%7) + a->repeatDay + 1;
#ifdef ALARMS_DEBUG
      printf("fpn: %02d/01/%02d, fdom=%d\n", t.tm_mon+1, t.tm_year+1900, fdom);
      printf("fpn: mday = %d\n", t.tm_mday);
#endif
      if (t.tm_mday > ndim) {
	 t.tm_mday -= 7;
      }
#ifdef ALARMS_DEBUG      
	{
	   char str[100];
	   strftime(str, 80, "%B %d, %Y %H:%M", &t);
	   printf("fpn: initial monthly by day=%s\n", str);
	}
#endif
      break;
    case repeatMonthlyByDate:
#ifdef ALARMS_DEBUG
      printf("fpn: repeatMonthlyByDate\n");
#endif
      t.tm_mon=date2->tm_mon;
      t.tm_year=date2->tm_year;
      freq = a->repeatFrequency;
      offset = ((t.tm_year - a->begin.tm_year)*12 +
		(t.tm_mon - a->begin.tm_mon))%(a->repeatFrequency);
      /* This will adjust for leap year, and exceeding the end of the month */
      if (((t.tm_year - a->begin.tm_year)*12 + (t.tm_mon - a->begin.tm_mon)) < 0) {
	 add_months_to_date(&t, offset);
      } else {
	 sub_months_from_date(&t, offset);
      }
      break;
    case repeatYearly:
#ifdef ALARMS_DEBUG
      printf("fpn: repeatYearly\n");
#endif
      t.tm_year=date2->tm_year;
      freq = a->repeatFrequency;
      offset = (t.tm_year - a->begin.tm_year)%(a->repeatFrequency);
#ifdef ALARMS_DEBUG      
      printf("fpn: (%d - %d)%%%d\n", t.tm_year,a->begin.tm_year,a->repeatFrequency);
      printf("fpn: *** years offset = %d\n", offset);
#endif
      /* This will adjust for leap year, and exceeding the end of the month */
      if ((t.tm_year - a->begin.tm_year) < 0) {
	 add_years_to_date(&t, offset);
      } else {
	 sub_years_from_date(&t, offset);
      }
      break;
   }

   safety_counter=0;
   while(forward || backward) {
      safety_counter++;
      if (safety_counter > 20) {
	 jp_logf(JP_LOG_STDOUT|JP_LOG_FILE, "find_prev_next():infinite loop, breaking\n");
	 if (a->description) {
	    jp_logf(JP_LOG_STDOUT|JP_LOG_FILE, "desc=[%s]\n", a->description);
	 }
	 break;
      }
      t_temp = mktime(&t);
#ifdef ALARMS_DEBUG
	{
	   char str[100];
	   strftime(str, 80, "%B %d, %Y %H:%M", &t);
	   printf("fpn: trying with=%s\n", str);
	}
#endif
      /* Check for exceptions */
      found_exception=0;
      for (i=0; i<a->exceptions; i++) {
	 if ((t.tm_mday==a->exception[i].tm_mday) &&
	     (t.tm_mon==a->exception[i].tm_mon) &&
	     (t.tm_year==a->exception[i].tm_year)
	     ) {
	    found_exception=1;
	    break;
	 }
      }
      if (found_exception) {
	 if (forward) {
	    forward_backward_in_appt_time(a, &t, fdom, ndim, 1);
	    continue;
	 }
	 if (backward) {
	    forward_backward_in_appt_time(a, &t, fdom, ndim, -1);
	    continue;
	 }
      }
      /* See that we aren't before then begin date */
      t_begin = mktime(&(a->begin));
      if (t_temp < t_begin - adv) {
#ifdef ALARMS_DEBUG      
	 printf("fpn:1\n");
#endif
	 backward=0;
      }
      /* If the appointment has an end date, see that we are not past it */
      if (!(a->repeatForever)) {
	 t_end = mktime(&(a->repeatEnd));
	 if (t_temp > t_end) {
	    forward=0;
#ifdef ALARMS_DEBUG      
	    printf("fpn: 2\n");
#endif
	 }
      }
      t_temp-=adv;
      if (t_temp >= t2) {
	 memcpy(tm_next, &t, sizeof(t));
	 *next_found=1;
	 forward=0;
#ifdef ALARMS_DEBUG      
	 printf("fpn: next found\n");
#endif
      } else {
	 memcpy(tm_prev, &t, sizeof(t));
	 *prev_found=1;
	 backward=0;
#ifdef ALARMS_DEBUG      
	 printf("fpn: prev_found\n");
#endif
      }
      if (forward) {
	 forward_backward_in_appt_time(a, &t, fdom, ndim, 1);
	 continue;
      }
      if (backward) {
	 forward_backward_in_appt_time(a, &t, fdom, ndim, -1);
	 continue;
      }
   }
   return 0;
}


/*
 * Find the next appointment alarm
 *  if soonest_only then return the next alarm, else return all alarms
 *  that occur between the 2 dates.
 */
int alarms_find_next(struct tm *date1_in, struct tm *date2_in, int soonest_only)
{
   AppointmentList *a_list;
   AppointmentList *temp_al;
   struct jp_alarms *ta;

   time_t adv;
   time_t ltime;
   time_t t1, t2;
   time_t t_alarm;
   time_t t_begin, t_end;
   time_t t_prev;
   time_t t_future;
   time_t t_interval;
   time_t t_soonest;
   struct tm *tm_temp;
   struct tm date1, date2;
   struct tm tm_prev, tm_next;
   int prev_found, next_found;
   int add_a_next;

   jp_logf(JP_LOG_DEBUG, "alarms_find_next()\n");

   if (glob_skip_all_alarms) return 0;

   if (!date1_in) {
      time(&ltime);
      tm_temp = localtime(&ltime);
   } else {
      tm_temp=date1_in;
   }
   bzero(&date1, sizeof(date1));
   date1.tm_year=tm_temp->tm_year;
   date1.tm_mon=tm_temp->tm_mon;
   date1.tm_mday=tm_temp->tm_mday;
   date1.tm_hour=tm_temp->tm_hour;
   date1.tm_min=tm_temp->tm_min;
   date1.tm_sec=tm_temp->tm_sec;
   date1.tm_isdst=tm_temp->tm_isdst;

   if (!date2_in) {
      time(&ltime);
      tm_temp = localtime(&ltime);
   } else {
      tm_temp=date2_in;
   }
   bzero(&date2, sizeof(date2));
   date2.tm_year=tm_temp->tm_year;
   date2.tm_mon=tm_temp->tm_mon;
   date2.tm_mday=tm_temp->tm_mday;
   date2.tm_hour=tm_temp->tm_hour;
   date2.tm_min=tm_temp->tm_min;
   date2.tm_sec=tm_temp->tm_sec;
   date2.tm_isdst=tm_temp->tm_isdst;

   t1=mktime(&date1);
   t2=mktime(&date2);

#ifdef ALARMS_DEBUG
     {
	char str[100];
	struct tm *Pnow;
	strftime(str, 80, "%B %d, %Y %H:%M", &date1);
	printf("date1=%s\n", str);
	strftime(str, 80, "%B %d, %Y %H:%M", &date2);
	printf("date2=%s\n", str);
	Pnow = localtime(&t1);
	strftime(str, 80, "%B %d, %Y %H:%M", Pnow);
	printf("^date1=%s\n", str);
     }
#endif

   if (!soonest_only) {
      free_alarms_list(PREV_ALARM_MASK | NEXT_ALARM_MASK);
   } else {
      free_alarms_list(NEXT_ALARM_MASK);
   }

   a_list=NULL;
   get_days_appointments2(&a_list, NULL, 0, 0, 1);

   t_soonest=0;

   for (temp_al=a_list; temp_al; temp_al=temp_al->next) {
      /*
       * No alarm, skip
       */
      if (!temp_al->ma.a.alarm) {
	 continue;
      }
#ifdef ALARMS_DEBUG      
      printf("\n[%s]\n", temp_al->ma.a.description);
#endif
      /*
       * See if the appointment starts before date1
       * and is a non repeating appointment
       */
      if (temp_al->ma.a.repeatType == repeatNone) {
	 t_alarm = mktime(&(temp_al->ma.a.begin));
	 if (t_alarm < t1) {
#ifdef ALARMS_DEBUG      
	    printf("non repeat before t1, t_alarm<t1, %ld<%ld\n",t_alarm,t1);
#endif
	    continue;
	 }
      }

      /* If the appointment has an end date, see that we are not past it */
      if (!(temp_al->ma.a.repeatForever)) {
	 t_end = mktime(&(temp_al->ma.a.repeatEnd));
	 /* We need to add 24 hours to the end date to make it inclusive */
	 t_end += 86400;
	 t_begin = mktime(&(temp_al->ma.a.begin));
	 if (t_end < t2) {
#ifdef ALARMS_DEBUG      
	    printf("past end date\n");
#endif
	    continue;
	 }
      }

#ifdef ALARMS_DEBUG      
      printf("alarm advance %d ", temp_al->ma.a.advance);
#endif
      adv = 0;
      if (temp_al->ma.a.advanceUnits == advMinutes) {
#ifdef ALARMS_DEBUG      
	 printf("minutes\n");
#endif
	 adv = temp_al->ma.a.advance*60;
      }
      if (temp_al->ma.a.advanceUnits == advHours) {
#ifdef ALARMS_DEBUG      
	 printf("hours\n");
#endif
	 adv = temp_al->ma.a.advance*3600;
      }
      if (temp_al->ma.a.advanceUnits == advDays) {
#ifdef ALARMS_DEBUG      
	 printf("days\n");
#endif
	 adv = temp_al->ma.a.advance*86400;
      }

#ifdef ALARMS_DEBUG      
      printf("adv=%ld\n", adv);
#endif

      t_prev=t_future=t_interval=0;
      prev_found=next_found=0;

      find_prev_next(&(temp_al->ma.a),
		     adv,
		     &date1,
		     &date2,
		     &tm_prev,
		     &tm_next,
		     &prev_found,
		     &next_found);
      t_prev=mktime(&tm_prev);
      t_future=mktime(&tm_next);

#ifdef ALARMS_DEBUG      
      printf("adv = %ld\n", adv);
#endif
      /*
       * Skip the alarms if they are before date1, or after date2
       */

      if (prev_found) {
	 if (t_prev - adv < t1) {
#ifdef ALARMS_DEBUG      
	    printf("failed prev is before t1\n");
#endif
	    prev_found=0;
	 }
	 if (t_prev - adv > t2) {
#ifdef ALARMS_DEBUG      
	    printf("failed prev is after t2\n");
#endif
	    continue;
	 }
      }
      if (next_found) {
	 if (!(temp_al->ma.a.repeatForever)) {
	    t_end = mktime(&(temp_al->ma.a.repeatEnd));
	    if (t_future > t_end) {
#ifdef ALARMS_DEBUG      
	       printf("failed future is after t_end\n");
#endif
	       next_found=0;
	    }
	 }
      }
#ifdef ALARMS_DEBUG
      printf("t1=       %s\n", print_date(t1));
      printf("t2=       %s\n", print_date(t2));
      printf("t_prev=   %s\n", prev_found ? print_date(t_prev):"None");
      printf("t_future= %s\n", next_found ? print_date(t_future):"None");
      printf("alarm me= %s\n", next_found ? print_date(t_future):"None");
      printf("desc=[%s]\n", temp_al->ma.a.description);
#endif
      if (!soonest_only) {
	 if (prev_found) {
	    alarms_add_to_list(temp_al->ma.unique_id, ALARM_MISSED, t_prev, adv);
	 }
      }
      if (next_found) {
	 add_a_next=0;
	 if (next_alarm==NULL) {
	    add_a_next=1;
	 } else if 
	   (t_future - adv <= next_alarm->event_time - next_alarm->alarm_advance) {
	      add_a_next=1;
	      if (t_future - adv < next_alarm->event_time - next_alarm->alarm_advance) {
#ifdef ALARMS_DEBUG
		 printf("next alarm=%s\n", print_date(next_alarm->event_time - next_alarm->alarm_advance));
		 printf("freeing next alarms\n");
#endif
		 free_alarms_list(NEXT_ALARM_MASK);
	      }
	   }
	 if (add_a_next) {
#ifdef ALARMS_DEBUG
	    printf("found a new next\n");
#endif
	    ta = malloc(sizeof(struct jp_alarms));
	    if (ta) {
	       ta->next = next_alarm;
	       next_alarm = ta;
	       next_alarm->unique_id = temp_al->ma.unique_id;
	       next_alarm->type = ALARM_NEW;
	       next_alarm->event_time = t_future;
	       next_alarm->alarm_advance = adv;
	    }
	 }
      }
   }
   free_AppointmentList(&a_list);

   return 0;
}

/*
 * At startup see when rc file was written and find all past-due alarms.
 * Add them to the postponed alarm list with 0 minute reminder.
 * Find next alarm and put it in list
 */
int alarms_init(unsigned char skip_past_alarms,
		unsigned char skip_all_alarms)
{
   FILE *in;
   time_t ltime;
   struct tm now, *Pnow;
   struct tm tm1;
   char line[256];
   int found_uptodate;
   int year, mon, day, hour, min, n;

   jp_logf(JP_LOG_DEBUG, "alarms_init()\n");

   alarm_list=NULL;
   Plast_alarm_list=NULL;
   next_alarm=NULL;

   total_alarm_windows = 0;
   glob_skip_all_alarms = skip_all_alarms;

   if (skip_past_alarms) {
      alarms_write_file();
   }
   if (skip_all_alarms) {
      alarms_write_file();
      return 0;
   }

   found_uptodate=0;
   in=jp_open_home_file(EPN".alarms", "r");
   if (!in) {
      jp_logf(JP_LOG_WARN, "Could not open "EPN".alarms file\n");
      return -1;
   }

   while (!feof(in)) {
      line[0]='\0';
      fgets(line, 255, in);
      line[255] = '\0';
      if (line[0]=='#') continue;
      if (!strncmp(line, "UPTODATE ", 9)) {
	 n = sscanf(line+9, "%d %d %d %d %d\n", &year, &mon, &day, &hour, &min);
	 if (n==5) {
	    found_uptodate=1;
	 }
	 jp_logf(JP_LOG_DEBUG, "UPTODATE %d %d %d %d %d\n", year, mon, day, hour, min);
      }
   }

   time(&ltime);
   Pnow = localtime(&ltime);
   bzero(&now, sizeof(now));
   now.tm_year=Pnow->tm_year;
   now.tm_mon=Pnow->tm_mon;
   now.tm_mday=Pnow->tm_mday;
   now.tm_hour=Pnow->tm_hour;
   now.tm_min=Pnow->tm_min;
   now.tm_isdst=-1;
   mktime(&now);

   if (!found_uptodate) {
      alarms_write_file();
      year = now.tm_year+1900;
      mon = now.tm_mon+1;
      day = now.tm_mday;
      hour = now.tm_hour;
      min = now.tm_min;
   }

   bzero(&tm1, sizeof(tm1));
   tm1.tm_year=year-1900;
   tm1.tm_mon=mon-1;
   tm1.tm_mday=day;
   tm1.tm_hour=hour;
   tm1.tm_min=min;
   tm1.tm_isdst=-1;

   mktime(&tm1);

   alarms_find_next(&tm1, &now, FALSE);

   gtk_timeout_add(ALARM_INTERVAL*1000, cb_timer_alarms, NULL);

   return 0;
}

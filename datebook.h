/* datebook.h
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
#ifndef __DATEBOOK_H__
#define __DATEBOOK_H__

#include <stdio.h>
#include <pi-datebook.h>
#include "utils.h"

#ifdef ENABLE_DATEBK

/* These defines depend on the defaults being zero so that the structure
   being zeroed out sets the defaults (0) */
# define DB3_TAG_TYPE_NONE   0
# define DB3_TAG_TYPE_DBplus 1
# define DB3_TAG_TYPE_DB3    3
# define DB3_TAG_TYPE_DB4    4

# define DB3_REGULAR         0
# define DB3_FLOAT           1
# define DB3_FLOAT_COMPLETE  2
# define DB3_FLOAT_DONE      3

# define DB3_FONT_NONE       0
# define DB3_FONT_BOLD       1
# define DB3_FONT_LARGE      2
# define DB3_FONT_LARGE_BOLD 3

# define DB3_SPAN_MID_NO     0
# define DB3_SPAN_MID_YES    1

# define DB3_LINK_NO         0
# define DB3_LINK_YES        1

struct db4_struct {
   int floating_event;
   /* The following are db3 and greater only tags */
   int custom_font;
   int category;
   int icon;
   int spans_midnight;
   int time_zone;
   /* The following are db4 and greater only tags */
   int link;
   int advance; /* I don't understand this and next field yet */
   int last_date; /* This may need to be a struct tm (not sure yet) */
   int custom_alarm_sound;
   int color;
   /* This isn't datebk specific */
   char *note; /* points to where the note starts after the tag (or null) */
};
#endif

int datebook_print(int type);
int datebook_import(GtkWidget *window);
int datebook_cleanup();
int pc_datebook_write(struct Appointment *a, PCRecType rt,
		      unsigned char attrib, unsigned int *unique_id);
void free_AppointmentList(AppointmentList **al);

/*
 * If Null is passed in for date, then all appointments will be returned
 */
int get_days_appointments(AppointmentList **al_out, struct tm *date);
/*
 * If Null is passed in for date, then all appointments will be returned
 * modified, deleted and private, 0 for no, 1 for yes, 2 for use prefs
 */
int get_days_appointments2(AppointmentList **appointment_list, struct tm *now,
			   int modified, int deleted, int privates);

/* This funtion removes appointments from the list that obviously will not
 * occur in this month */
int weed_datebook_list(AppointmentList **al, int mon, int year, int *mask);

/* Year is years since 1900 */
/* Mon is 0-11 */
/* Day is 1-31 */
/* */
int datebook_add_exception(struct Appointment *a, int year, int mon, int day);
int get_datebook_app_info(struct AppointmentAppInfo *ai);

int datebook_copy_appointment(struct Appointment *a1,
			     struct Appointment **a2);
/* returns a bit mask where bit 1 day one, etc. and it is set if an */
/* appointment occurs on that day, 0 if not. */
int appointment_on_day_list(int mon, int year, int *mask);

/*
 * returns 1 if an appointment does occur/re-occur on dat
 * else returns 0
 */
unsigned int isApptOnDate(struct Appointment *a, struct tm *date);

int dateToSecs(struct tm *tm1);

int dateToDays(struct tm *tm1);

int compareTimesToDay(struct tm *tm1, struct tm *tm2);

#ifdef ENABLE_DATEBK
/* Returns a bitmask
 * 0 if not a floating OR
 * bitmask:
 *  1 if float,
 *  2 if completed float
 *  16 if float has a note
 */
int db3_parse_tag(char *note, int *type, struct db4_struct *db4);
int db3_has_tags(struct Appointment *a);
int db3_which_icon(struct  Appointment *a);
#endif

int datebook_import(GtkWidget *window);
int datebook_export(GtkWidget *window);

#endif

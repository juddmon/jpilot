/* $Id: calendar.h,v 1.1 2010/03/02 18:59:00 judd Exp $ */

/*******************************************************************************
 * calendar.h
 * A module of J-Pilot http://jpilot.org
 * 
 * Copyright (C) 1999-2010 by Judd Montgomery
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

#ifndef __CALENDAR_H__
#define __CALENDAR_H__

#include <stdio.h>
#include <pi-datebook.h>
#include <pi-calendar.h>
#include "utils.h"

/* Copy AppInfo data structures */
int copy_appointment_ai_to_calendar_ai(const struct AppointmentAppInfo *aai, struct CalendarAppInfo *cai);

/* Copy AppInfo data structures */
int copy_calendar_ai_to_appointment_ai(const struct CalendarAppInfo *cai, struct AppointmentAppInfo *aai);

int copy_appointment_to_calendarEvent(const struct Appointment *a, struct CalendarEvent *ce);

int copy_appointments_to_calendarEvents(AppointmentList *al, CalendarEventList **cel);

int copy_calendarEvent_to_appointment(const struct CalendarEvent *ce, struct Appointment *a);

void free_CalendarEventList(CalendarEventList **cel);

int get_calendar_app_info(struct CalendarAppInfo *cai);

int calendar_sort(CalendarEventList **cel,
                  int (*compare_func)(const void*, const void*));

int get_days_calendar_events2(CalendarEventList **calendar_event_list, struct tm *now,
			      int modified, int deleted, int privates,
			      int category, int *total_records);

int get_days_calendar_events(CalendarEventList **calendar_event_list, struct tm *now, int category, int *total_records);

/*
 * If Null is passed in for date, then all appointments will be returned
 * modified, deleted and private, 0 for no, 1 for yes, 2 for use prefs
 */
int get_days_calendar_events2(CalendarEventList **calendar_event_list, struct tm *now,
			      int modified, int deleted, int privates,
			      int category, int *total_records);

int pc_calendar_write(struct CalendarEvent *ce, PCRecType rt,
		      unsigned char attrib, unsigned int *unique_id);

#endif

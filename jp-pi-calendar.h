/* $Id: jp-pi-calendar.h,v 1.1 2009/09/10 06:01:54 rikster5 Exp $ */

/*******************************************************************************
 * jp-pi-calendar.h:  Translate Palm calendar data formats
 * A module of J-Pilot http://jpilot.org
 *
 * Copyright (C) 2009 by Rik Wehbring 
 *
 * This code originally derived from pi-datebook.h in the pilot-link project.
 * Eventually, it should be integrated with that project and J-Pilot can
 * link against the pilot-link libraries.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Library General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef _JP_PILOT_CALENDAR_H_
#define _JP_PILOT_CALENDAR_H_

#include <time.h>

#include "pi-appinfo.h"
#include "pi-buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

   typedef enum {
      datebook_v1,
   } datebookType;

   extern char *DatebookAlarmTypeNames[];
   extern char *DatebookRepeatTypeNames[];

   enum alarmTypes { advMinutes, advHours, advDays };

   enum repeatTypes {
      repeatNone,
      repeatDaily,
      repeatWeekly,
      repeatMonthlyByDay,
      repeatMonthlyByDate,
      repeatYearly
   };

   /* This enumeration normally isn't of much use, as you can get just
      as useful results by taking the value mod 7 to get the day of the
      week, and div 7 to get the week value, with week 4 (of 0) meaning
      the last, be it fourth or fifth.
    */
   enum DayOfMonthType {
      dom1stSun, dom1stMon, dom1stTue, dom1stWen, dom1stThu,
      dom1stFri,
      dom1stSat,
      dom2ndSun, dom2ndMon, dom2ndTue, dom2ndWen, dom2ndThu,
      dom2ndFri,
      dom2ndSat,
      dom3rdSun, dom3rdMon, dom3rdTue, dom3rdWen, dom3rdThu,
      dom3rdFri,
      dom3rdSat,
      dom4thSun, dom4thMon, dom4thTue, dom4thWen, dom4thThu,
      dom4thFri,
      dom4thSat,
      domLastSun, domLastMon, domLastTue, domLastWen, domLastThu,
      domLastFri,
      domLastSat
   };

   typedef struct Appointment {
      int event;                       /* Is this a timeless event?                          */
      struct tm begin, end;            /* When does this appointment start and end?          */
      int alarm;                       /* Should an alarm go off?                            */
      int advance;                     /* How far in advance should it be?                   */
      int advanceUnits;                /* What am I measuring the advance in?                */
      enum repeatTypes repeatType;     /* How should I repeat this appointment, if at all?   */
      int repeatForever;               /* Do repetitions end at some date?                   */
      struct tm repeatEnd;             /* What date do they end on?                          */
      int repeatFrequency;             /* Should I skip an interval for each repetition?     */
      enum DayOfMonthType repeatDay;   /* for repeatMonthlyByDay                             */
      int repeatDays[7];               /* for repeatWeekly                                   */
      int repeatWeekstart;             /* What day did the user decide starts the week?      */
      int exceptions;                  /* How many repetitions are their to be ignored?      */
      struct tm *exception;            /* What are they?                                     */
      char *description;               /* What is the description of this appointment?       */
      char *note;                      /* Is there a note to go along with it?               */
      char *location;                  /* Where does this appointment take place?            */
   } Appointment_t;

   typedef struct AppointmentAppInfo {
      struct CategoryAppInfo category;
      int startOfWeek;
   } AppointmentAppInfo_t;

   extern void jp_free_Appointment
     PI_ARGS((struct Appointment *));
   extern int jp_unpack_Appointment
       PI_ARGS((struct Appointment *, const pi_buffer_t *record, datebookType type));
   extern int jp_pack_Appointment
       PI_ARGS((const struct Appointment *, pi_buffer_t *record, datebookType type));
   extern int jp_unpack_AppointmentAppInfo
     PI_ARGS((struct AppointmentAppInfo *, const unsigned char *AppInfo,
           size_t len));
   extern int jp_pack_AppointmentAppInfo
     PI_ARGS((const struct AppointmentAppInfo *, unsigned char *AppInfo,
           size_t len));

#ifdef __cplusplus
  };
#endif

#endif            /* _JP_PILOT_CALENDAR_H_ */

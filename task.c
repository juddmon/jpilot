/*******************************************************************************
 * task.c
 * A module of J-Pilot http://jpilot.org
 *
 * Copyright (C) 2004-2015 by Judd Montgomery
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 ******************************************************************************/

/*
 * This code was written to be included in pilot-link and belongs in the libpisock directory.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include "pi-macros.h"
#include "pi-task.h"

/* Maximum length of Description and Note fields */
#define DescMaxLength 256
#define NoteMaxLength 4096


/***********************************************************************
 *
 * Function:    free_Task
 *
 * Summary:     Free the memory and filehandle from the record alloc. 
 *
 * Parameters:  Task_t*
 *
 * Returns:     void
 *
 ***********************************************************************/
void
free_Task(Task_t *task)
{

    if (task->description != NULL) {
        free(task->description);
        task->description = NULL;
    }

    if (task->note != NULL) {
        free(task->note);
        task->note = NULL;
    }
}


/***********************************************************************
 *
 * Function:    unpack_Task
 *
 * Summary:     Unpack the Task structure from buffer into records 
 *              we can chew on.
 *
 * Parameters:  Task_t*, pi_buffer_t * of buffer, task type
 *
 * Returns:     -1 on fail, 0 on success
 *
 ***********************************************************************/
int
unpack_Task(Task_t *task, const pi_buffer_t *buf, taskType type)
{
    unsigned long d;
    unsigned short data_flags;
    unsigned short record_flags;
    int ofs;

    /* Note: There are possible timezone conversion problems related to
       the use of the due member of a struct Task. As it is kept in
       local (wall) time in struct tm's, the timezone of the Palm is
       irrelevant, _assuming_ that any UNIX program keeping time in
       time_t's converts them to the correct local time. If the Palm is
       in a different timezone than the UNIX box, it may not be simple
       to deduce that correct (desired) timezone.

       The easiest solution is to keep apointments in struct tm's, and
       out of time_t's. Of course, this might not actually be a help if
       you are constantly darting across timezones and trying to keep
       appointments.
       -- KJA */

    if (type != task_v1)
        return -1;

    if (buf == NULL || buf->data == NULL || buf->used < 3)
        return -1;

    data_flags = get_short(buf->data);
    //printf("byte=%04x\n", data_flags);
    if (data_flags & 0x8000) {
       task->indefinite = 0;
    } else {
       task->indefinite = 1;
    }
    if (data_flags & 0x4000) {
       task->flag_completion_date = 1;
    } else {
       task->flag_completion_date = 0;
    }
    if (data_flags & 0x2000) {
       task->flag_alarm = 1;
    } else {
       task->flag_alarm = 0;
    }
    if (data_flags & 0x1000) {
       task->flag_repeat = 1;
    } else {
       task->flag_repeat = 0;
    }
   
    record_flags = get_short(buf->data + 2);
    task->complete = (record_flags & 0x0001);
    task->priority = get_short(buf->data + 4);

    ofs = 6;

   if (task->indefinite == 0) {      
    d = (unsigned short int) get_short(buf->data + ofs);
    if (d != 0xffff) {
        task->due.tm_year = (d >> 9) + 4;
        task->due.tm_mon = ((d >> 5) & 15) - 1;
        task->due.tm_mday = d & 31;
        task->due.tm_hour = 0;
        task->due.tm_min = 0;
        task->due.tm_sec = 0;
        task->due.tm_isdst = -1;
        mktime(&task->due);
    }
    ofs += 2;
   }

   if (task->flag_completion_date) {      
      d = (unsigned short int) get_short(buf->data + ofs);
      if (d != 0xffff) {
     task->completion_date.tm_year = (d >> 9) + 4;
     task->completion_date.tm_mon = ((d >> 5) & 15) - 1;
     task->completion_date.tm_mday = d & 31;
     task->completion_date.tm_hour = 0;
     task->completion_date.tm_min = 0;
     task->completion_date.tm_sec = 0;
     task->completion_date.tm_isdst = -1;
     mktime(&task->completion_date);
      }
      ofs += 2;
   }

   if (task->flag_alarm) {
      // I don't know what a timetype is.  Assumed 2 bytes for now
      d = (unsigned short int) get_short(buf->data + ofs);
      if (d != 0xffff) {
     task->alarm_date.tm_year = (d >> 9) + 4;
     task->alarm_date.tm_mon = ((d >> 5) & 15) - 1;
     task->alarm_date.tm_mday = d & 31;
     task->alarm_date.tm_hour = 0;
     task->alarm_date.tm_min = 0;
     task->alarm_date.tm_sec = 0;
     task->alarm_date.tm_isdst = -1;
     mktime(&task->alarm_date);
      }
      ofs += 2;

      d = (unsigned short int) get_short(buf->data + ofs);
      task->advance = d;
      ofs += 2;
   }

   if (task->flag_repeat) {
       d = (unsigned short int) get_short(buf->data + ofs);
       if (d != 0xffff) {
           task->repeat_date.tm_year = (d >> 9) + 4;
           task->repeat_date.tm_mon = ((d >> 5) & 15) - 1;
           task->repeat_date.tm_mday = d & 31;
           task->repeat_date.tm_hour = 0;
           task->repeat_date.tm_min = 0;
           task->repeat_date.tm_sec = 0;
           task->repeat_date.tm_isdst = -1;
           mktime(&task->repeat_date);
       }
       ofs += 2;
       //time 2
       //advance 2
       //start date 2
       //repeattype 1
       task->repeat_type = get_byte(buf->data + ofs++);
       //reserved 1
       ofs++;
       //repeat end date 2
       d = (unsigned short int) get_short(buf->data + ofs);
       if (d != 0xffff) {
           task->repeat_end.tm_year = (d >> 9) + 4;
           task->repeat_end.tm_mon = ((d >> 5) & 15) - 1;
           task->repeat_end.tm_mday = d & 31;
           task->repeat_end.tm_hour = 0;
           task->repeat_end.tm_min = 0;
           task->repeat_end.tm_sec = 0;
           task->repeat_end.tm_isdst = -1;
           mktime(&task->repeat_end);
       }
       ofs += 2;
       // freq 1
       task->repeat_frequency = get_byte(buf->data + ofs++);
       // repeat on (1 byte)
       // Bitmask of the days of the week
       d = get_byte(buf->data + ofs++);
       task->repeatDays[0] =   (d & 0x01);
       task->repeatDays[1] = !!(d & 0x02);
       task->repeatDays[2] = !!(d & 0x04);
       task->repeatDays[3] = !!(d & 0x08);
       task->repeatDays[4] = !!(d & 0x10);
       task->repeatDays[5] = !!(d & 0x20);
       task->repeatDays[6] = !!(d & 0x40);
       task->repeatDays[7] = !!(d & 0x80);
       // repeat start of week 1
       task->fdow = get_byte(buf->data + ofs++);
       // reserved (1 byte)
       ofs += 1;
   }

    /* if flags.dueDate, a DateType for the due date
     * then if flags.completionDate, a DateType for the completion date
     * then if flags.alarm, a TimeType (alarmTime) and a UInt16 (advance days)
     * then if flags.repeat, a DateType for the start date and a RepeatInfoType
     * then a null terminated description string
     * then a null terminated note string
     */

    if (buf->used - ofs < 1)
        return -1;

    task->description = strdup((char *) buf->data + ofs);

    ofs += strlen(task->description) + 1;

    if (buf->used - ofs < 1) {
        free(task->description);
        task->description = 0;
        return -1;
    }
    task->note = strdup((char *) buf->data + ofs);

    return 0;
}


/***********************************************************************
 *
 * Function:    pack_Task
 *
 * Summary:     Pack the Task records into a structure
 *
 * Parameters:  Task_t*, pi_buffer_t *buf of record, record type
 *
 * Returns:     -1 on error, 0 on success.
 *
 ***********************************************************************/
int
pack_Task(const Task_t *task, pi_buffer_t *buf, taskType type)
{
    int pos;
    size_t destlen = 3;

    if (task == NULL || buf == NULL)
        return -1;

    if (type != task_v1)
        return -1;

    if (task->description)
        destlen += strlen(task->description);
    destlen++;
    if (task->note)
        destlen += strlen(task->note);
    destlen++;

    pi_buffer_expect (buf, destlen);
    buf->used = destlen;

    if (task->indefinite) {
        buf->data[0] = 0xff;
        buf->data[1] = 0xff;
    } else {
        set_short(buf->data, ((task->due.tm_year - 4) << 9) | ((task->due.tm_mon + 1) << 5) | task->due.tm_mday);
    }
    buf->data[2] = task->priority;
    if (task->complete) {
        buf->data[2] |= 0x80;
    }

    pos = 3;
    if (task->description) {
        strcpy((char *) buf->data + pos, task->description);
        pos += strlen(task->description) + 1;
    } else {
        buf->data[pos++] = 0;
    }

    if (task->note) {
        strcpy((char *) buf->data + pos, task->note);
        pos += strlen(task->note) + 1;
    } else {
        buf->data[pos++] = 0;
    }

    return 0;
}


/***********************************************************************
 *
 * Function:    unpack_TaskAppInfo
 *
 * Summary:     Unpack the Task AppInfo block from the structure
 *
 * Parameters:  TaskAppInfo_t*, char* to record, record length
 *
 * Returns:     effective record length
 *
 ***********************************************************************/
int
unpack_TaskAppInfo(TaskAppInfo_t *appinfo, const unsigned char *record, size_t len)
{
    int i;
    const unsigned char *start = record;

    appinfo->type = task_v1;

    i = unpack_CategoryAppInfo(&appinfo->category, record, len);
    if (!i)
        return 0;
    record += i;
    len -= i;

       /* Hack - unpack_CategoryAppInfo expects 3 bytes of alignment
        * at the end of the AppInfo.  We only have 1 */
    len += 3; record -= 3;

    if (len < 25)
        return 0;

    /* Alignment (1 byte) */
    record++; len--;

    /* Category colors edited (2 bytes) */
    appinfo->catColorsEdited = get_short(record);
    record += 2; len -=2;

    /* Category colors (16 bytes) */
    for (i=0; i < 16; i++) {
       appinfo->catColor[i] = get_byte(record);
       record++; len--;
    }

    /* Alignment (2 bytes) */
    record += 2; len -= 2;

    /* Application info dirty (2 bytes) */
    appinfo->dirty = get_short(record);
    record += 2;

    /* Sort Order (1 byte) */
    appinfo->sortOrder = get_byte(record);
    record ++;

    /* Alignment (1 byte) */
    record++; len--;

    return (record - start);
}


/***********************************************************************
 *
 * Function:    pack_TaskAppInfo
 *
 * Summary:     Pack the AppInfo block/record back into the structure
 *
 * Parameters:  TaskAppInfo_t*, char* to record, record length
 *
 * Returns:     effective buffer length
 *
 ***********************************************************************/
int
pack_TaskAppInfo(const TaskAppInfo_t *appinfo, unsigned char *record, size_t len)
{
    int i;
    unsigned char *start = record;

    i = pack_CategoryAppInfo(&appinfo->category, record, len);
    if (!record)
        return i + 4;
    if (!i)
        return 0;
    record += i;
    len -= i;
    if (len < 4)
        return 0;
    set_short(record, appinfo->dirty);
    //set_byte(record + 2, appinfo->sortByPriority);
    set_byte(record + 3, 0);    /* gapfill */
    record += 4;

    return (record - start);
}

/* vi: set ts=8 sw=4 sts=4 noexpandtab: cin */
/* ex: set tabstop=4 expandtab: */
/* Local Variables: */
/* indent-tabs-mode: t */
/* c-basic-offset: 8 */
/* End: */

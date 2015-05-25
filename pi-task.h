/*******************************************************************************
 * pi-task.h
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
#ifndef _PILOT_TASKS_H_
#define _PILOT_TASKS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <time.h>
#include "pi-appinfo.h"
#include "pi-buffer.h"

	typedef enum {
		task_v1,
		task_v2,
	} taskType;

	typedef enum {
		taskSortDueDatePriority,
		taskSortPriorityDueDate,
		taskSortCategoryPriority,
		taskSortCategoryDueDate
	} taskSortType;

	typedef struct Task {
		int indefinite;
		struct tm due;
		int flag_completion_date; /* New for tasks */
		struct tm completion_date; /* New for tasks */
		int flag_alarm; /* New for tasks */
		struct tm alarm_date; /* New for tasks */
		int advance; /* New for tasks */
		int flag_repeat; /* New for tasks */
		struct tm repeat_date; /* New for tasks */
		int repeat_type; /* New for tasks */
		struct tm repeat_end; /* New for tasks */
		int repeat_frequency; /* New for tasks */
		int repeatDays[7]; /* Sun-Sat, New for tasks */
		int fdow; /* First day of week, New for tasks */
		int priority;
		int complete;
		char *description;
		char *note;
	} Task_t;

	typedef struct TaskAppInfo {
		taskType type;
		struct CategoryAppInfo category;
		int dirty;
		int catColorsEdited;
		int catColor[16];
		int sortOrder;
	} TaskAppInfo_t;

	extern void free_Task PI_ARGS((Task_t *));
	extern int unpack_Task
	    PI_ARGS((Task_t *, const pi_buffer_t *record, taskType type));
	extern int pack_Task
	    PI_ARGS((const Task_t *, pi_buffer_t *record, taskType type));
	extern int unpack_TaskAppInfo
	    PI_ARGS((TaskAppInfo_t *, const unsigned char *record, size_t len));
	extern int pack_TaskAppInfo
	    PI_ARGS((const TaskAppInfo_t *, unsigned char *record, size_t len));

#ifdef __cplusplus
  };
#endif

#endif				/* _PILOT_TASK_H_ */

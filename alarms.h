/* $Id: alarms.h,v 1.5 2007/12/12 23:13:26 rikster5 Exp $ */

/*******************************************************************************
 * alarms.h
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
 ******************************************************************************/

int alarms_init(unsigned char skip_past_alarms,
		unsigned char skip_all_alarms);
/*
 * find the next alarm to happen after date2, and find all missed alarms
 * between date1 and date2.
 * soonest_only can be used to not find missed alarms.
 * date1, and date2 can be NULL, meaning the current time.
 */
int alarms_find_next(struct tm *date1, struct tm *date2, int soonest_only);

/* 
 * Finds the previous and next occurrences of a repeating event between
 * date1 and date2. 
 * Used to position datebook calendar correctly.
 */
int find_prev_next(struct Appointment *appt,
			  time_t adv,
			  struct tm *date1,
			  struct tm *date2,
			  struct tm *tm_prev,
			  struct tm *tm_next,
			  int *prev_found,
			  int *next_found);



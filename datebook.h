/* datebook.h
 * 
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
#include <stdio.h>
#include <pi-datebook.h>
#include "utils.h"

#define PATH ~/.jpilot/


int datebook_sync();
int datebook_cleanup();
int pc_datebook_write(struct Appointment *a, PCRecType rt, unsigned char attrib);
void free_AppointmentList(AppointmentList **al);
//
//If Null is passed in for date, then all appointments will be returned
//
int get_days_appointments(AppointmentList **al_out, struct tm *date);
//int datebook_dup_appointment(struct Appointment *a1, struct Appointment **a2);

//// Year is years since 1900
// Mon is 0-11
// Day is 1-31
//
int datebook_add_exception(struct Appointment *a, int year, int mon, int day);
int get_datebook_app_info(struct AppointmentAppInfo *ai);

int datebook_copy_appointment(struct Appointment *a1,
			     struct Appointment **a2);
int datebook_create_bogus_record(char *record, int size, int *rec_len);

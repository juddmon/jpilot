/* rcfile.h
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
#ifndef __PREFS_H__
#define __PREFS_H__

#define PREF_RCFILE 0
#define PREF_TIME 1
#define PREF_SHORTDATE 2
#define PREF_LONGDATE 3
#define PREF_FDOW 4 /*First Day Of the Week */
#define PREF_SHOW_DELETED 5
#define PREF_SHOW_MODIFIED 6
#define PREF_HIDE_COMPLETED 7
#define PREF_HIGHLIGHT 8
#define PREF_PORT 9
#define PREF_RATE 10
#define PREF_USER 11
#define PREF_USER_ID 12
#define PREF_NUM_BACKUPS 13
#define PREF_WINDOW_WIDTH 14
#define PREF_WINDOW_HEIGHT 15
#define PREF_DATEBOOK_PANE 16
#define PREF_ADDRESS_PANE 17
#define PREF_TODO_PANE 18
#define PREF_MEMO_PANE 19

#define NUM_PREFS 20

#define NUM_SHORTDATES  8
#define NUM_LONGDATES  7
#define NUM_TIMES  11
#define NUM_RATES  11

#define MAX_PREF_NUM_BACKUPS 99
#define INTTYPE 0
#define CHARTYPE 1

#define PREF_MDY 0
#define PREF_DMY 1
#define PREF_YMD 2

#define MAX_PREF_VALUE 80

typedef struct {
   char *name;
   int usertype;
   int filetype;
   long ivalue;
   char svalue[MAX_PREF_VALUE+2];
} prefType;

/*extern prefType glob_prefs[NUM_PREFS]; */

int read_rc_file();
int write_rc_file();
int get_pref_possibility(int which, int n, char *ret);
int get_pref(int which, long *n, const char **ret);
int get_pref_dmy_order();
int set_pref(int which, long n);
int set_pref_char(int which, char *string);
/*This function is used externally to free up any memory that prefs is using */
void free_prefs();

#endif

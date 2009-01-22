/* $Id: prefs.h,v 1.50 2009/01/22 22:09:38 rikster5 Exp $ */

/*******************************************************************************
 * prefs.h
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
 ******************************************************************************/

#ifndef __PREFS_H__
#define __PREFS_H__

#include "libplugin.h"

#define PREF_RCFILE 0
#define PREF_TIME 1
#define PREF_SHORTDATE 2
#define PREF_LONGDATE 3
#define PREF_FDOW 4 /*First Day Of the Week */
#define PREF_SHOW_DELETED 5
#define PREF_SHOW_MODIFIED 6
#define PREF_TODO_HIDE_COMPLETED 7
#define PREF_DATEBOOK_HIGHLIGHT_DAYS 8
#define PREF_PORT 9
#define PREF_RATE 10
#define PREF_USER 11
#define PREF_USER_ID 12
#define PREF_PC_ID 13
#define PREF_NUM_BACKUPS 14
#define PREF_WINDOW_WIDTH 15
#define PREF_WINDOW_HEIGHT 16
#define PREF_DATEBOOK_PANE 17
#define PREF_ADDRESS_PANE 18
#define PREF_TODO_PANE 19
#define PREF_MEMO_PANE 20
#define PREF_USE_DB3 21
#define PREF_LAST_APP 22
#define PREF_PRINT_THIS_MANY 23
#define PREF_PRINT_ONE_PER_PAGE 24
#define PREF_NUM_BLANK_LINES 25
#define PREF_PRINT_COMMAND 26
#define PREF_CHAR_SET 27
#define PREF_SYNC_DATEBOOK 28
#define PREF_SYNC_ADDRESS 29
#define PREF_SYNC_TODO 30
#define PREF_SYNC_MEMO 31
#define PREF_SYNC_MEMO32 32
#define PREF_ADDRESS_NOTEBOOK_PAGE 33
#define PREF_OUTPUT_HEIGHT 34
#define PREF_OPEN_ALARM_WINDOWS 35
#define PREF_DO_ALARM_COMMAND 36
#define PREF_ALARM_COMMAND 37
#define PREF_REMIND_IN 38
#define PREF_REMIND_UNITS 39
#define PREF_PASSWORD 40
#define PREF_MEMO32_MODE 41
#define PREF_PAPER_SIZE 42
#define PREF_DATEBOOK_EXPORT_FILENAME 43
#define PREF_DATEBOOK_IMPORT_PATH 44
#define PREF_ADDRESS_EXPORT_FILENAME 45
#define PREF_ADDRESS_IMPORT_PATH 46
#define PREF_TODO_EXPORT_FILENAME 47
#define PREF_TODO_IMPORT_PATH 48
#define PREF_MEMO_EXPORT_FILENAME 49
#define PREF_MEMO_IMPORT_PATH 50
#define PREF_MANANA_MODE 51
#define PREF_SYNC_MANANA 52
#define PREF_USE_JOS 53
#define PREF_PHONE_PREFIX1 54
#define PREF_CHECK_PREFIX1 55
#define PREF_PHONE_PREFIX2 56
#define PREF_CHECK_PREFIX2 57
#define PREF_PHONE_PREFIX3 58
#define PREF_CHECK_PREFIX3 59
#define PREF_DIAL_COMMAND 60
#define PREF_DATEBOOK_TODO_PANE 61
#define PREF_DATEBOOK_TODO_SHOW 62
#define PREF_TODO_HIDE_NOT_DUE 63
#define PREF_TODO_COMPLETION_DATE 64
#define PREF_INSTALL_PATH 65
#define PREF_MONTHVIEW_WIDTH 66
#define PREF_MONTHVIEW_HEIGHT 67
#define PREF_WEEKVIEW_WIDTH 68
#define PREF_WEEKVIEW_HEIGHT 69
#define PREF_LAST_DATE_CATEGORY 70
#define PREF_LAST_ADDR_CATEGORY 71
#define PREF_LAST_TODO_CATEGORY 72
#define PREF_LAST_MEMO_CATEGORY 73
#define PREF_MAIL_COMMAND 74
#define PREF_VERSION 75
#define PREF_UTF_ENCODING 76
#define PREF_CONFIRM_FILE_INSTALL 77
#define PREF_TODO_DAYS_DUE 78
#define PREF_TODO_DAYS_TILL_DUE 79
#define PREF_SHOW_TOOLTIPS 80
#define PREF_DATEBOOK_NOTE_PANE 81
#define PREF_DATEBOOK_HI_TODAY 82
#define PREF_DATEBOOK_ANNI_YEARS 83
#define PREF_KEYRING_PANE 84
#define PREF_EXPENSE_PANE 85
/* 0 for Datebook, 1 for Calendar */
#define PREF_DATEBOOK_VERSION 86
/* 0 for Address, 1 for Contacts */
#define PREF_ADDRESS_VERSION 87
/* 0 for Todo, 1 for Tasks */
#define PREF_TODO_VERSION 88
/* 0 for Memo, 1 for Memos, 2 for Memo32 */
#define PREF_MEMO_VERSION 89
#define PREF_CONTACTS_PHOTO_FILENAME 90
#define PREF_TODO_SORT_COLUMN 91
#define PREF_TODO_SORT_ORDER 92
#define PREF_ADDR_SORT_ORDER 93
#define PREF_ADDR_NAME_COL_SZ 94
#define PREF_TODO_NOTE_PANE 95
#define PREF_EXPENSE_SORT_COLUMN 96
#define PREF_EXPENSE_SORT_ORDER 97

/* Number of preferences in use */
#define NUM_PREFS 98
/* Maximum number of preferences */
#define MAX_NUM_PREFS 250

/* New code should use MAX_PREF_LEN for clarity over MAX_PREF_VALUE */
#define MAX_PREF_LEN   200
#define MAX_PREF_VALUE 200

#define MAX_PREF_NUM_BACKUPS 99

#define CHAR_SET_LATIN1   0  /* English, European, Latin based languages */
#define CHAR_SET_JAPANESE 1
#define CHAR_SET_1250     2  /* Czech, Polish (Unix: ISO-8859-2) */
#define CHAR_SET_1251     3  /* Russian; palm koi8-r, host win1251 */
#define CHAR_SET_1251_B   4  /* Russian; palm win1251, host koi8-r */
#define CHAR_SET_TRADITIONAL_CHINESE  5 /* Taiwan Chinese */
#define CHAR_SET_KOREAN   6  /* Korean Hangul */
#define CHAR_SET_UTF      7
#define CHAR_SET_1250_UTF 7  /* Czech, Polish (latin2, CP1250) */
#define CHAR_SET_1252_UTF 8  /* Latin European (latin1, CP1252) */
#define CHAR_SET_1253_UTF 9  /* Modern Greek (CP1253) */
#define CHAR_SET_ISO8859_2_UTF 10 /* Czech, Polish (latin2, ISO8859-2) */
#define CHAR_SET_KOI8_R_UTF 11 /* Cyrillic (KOI8-R) */
#define CHAR_SET_1251_UTF 12 /* Cyrillic (CP1251) */
#define CHAR_SET_GBK_UTF  13 /* Chinese (GB2312) */
#define CHAR_SET_SJIS_UTF 14 /* Japanese (SJIS) */
#define CHAR_SET_1255_UTF 15 /* Hebrew (CP1255) */
#define CHAR_SET_BIG5_UTF 16 /* Chinese (BIG-5) */
#define CHAR_SET_949_UTF  17 /* Korean (CP949) */
#define NUM_CHAR_SETS     18

void pref_init();
int pref_read_rc_file();
int pref_write_rc_file();
int get_pref(int which, long *n, const char **ret);
int set_pref(int which, long n, const char *string, int save);

/* Specialized functions */
int set_pref_possibility(int which, long n, int save);
int get_pref_possibility(int which, int n, char *ret);
int get_pref_dmy_order();
void get_pref_hour_ampm(char *datef);
int get_pref_time_no_secs(char *datef);
int get_pref_time_no_secs_no_ampm(char *datef);

/*
 * Get the preference value as long. If failed to do so, return the
 * specified default.
 */
long get_pref_int_default(int which, long defval);

int make_pref_menu(GtkWidget **pref_menu, int pref_num);

#endif

/* $Id: print.h,v 1.6 2004/11/22 00:52:42 rikster5 Exp $ */

/*******************************************************************************
 * print.h
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

#ifndef _PRINT_H__
#define _PRINT_H__

#include "utils.h"

#define DAILY   1
#define WEEKLY  2
#define MONTHLY 3

extern int print_day_week_month;

typedef enum {
    PAPER_Letter,    PAPER_Legal,  PAPER_Statement, PAPER_Tabloid,
    PAPER_Ledger,    PAPER_Folio,  PAPER_Quarto,    PAPER_7x9,
    PAPER_9x11,      PAPER_9x12,   PAPER_10x13,     PAPER_10x14,
    PAPER_Executive, PAPER_A0,     PAPER_A1,        PAPER_A2,
    PAPER_A3,        PAPER_A4,     PAPER_A5,        PAPER_A6,
    PAPER_A7,        PAPER_A8,     PAPER_A9,        PAPER_A10,
    PAPER_B0,        PAPER_B1,     PAPER_B2,        PAPER_B3,
    PAPER_B4,        PAPER_B5,     PAPER_B6,        PAPER_B7,
    PAPER_B8,        PAPER_B9,     PAPER_B10,       PAPER_ISOB0,
    PAPER_ISOB1,     PAPER_ISOB2,  PAPER_ISOB3,     PAPER_ISOB4,
    PAPER_ISOB5,     PAPER_ISOB6,  PAPER_ISOB7,     PAPER_ISOB8,
    PAPER_ISOB9,     PAPER_ISOB10, PAPER_C0,        PAPER_C1,
    PAPER_C2,        PAPER_C3,     PAPER_C4,        PAPER_C5,
    PAPER_C6,        PAPER_C7,     PAPER_DL,        PAPER_Filo
} PaperSize;

/* The print options window
 * The main window should be passed in if possible, or NULL
 * Returns:
 *  DIALOG_SAID_PRINT
 *  DIALOG_SAID_CANCEL
 *  <0 on error
 */
/* year_mon_day is a binary flag to choose which radio buttons appear for
 * datebook printing.
 * 1 = daily
 * 2 = weekly
 * 4 = monthly
 */
int print_gui(GtkWidget *main_window, int app, int date_button, int mon_week_day);

int print_days_appts(struct tm *date);
int print_months_appts(struct tm *date_in, PaperSize paper_size);
int print_weeks_appts(struct tm *date_in, PaperSize paper_size);

int print_addresses(AddressList *address_list);

int print_todos(ToDoList *todo_list, char *category_name);

int print_memos(MemoList *todo_list);

#endif

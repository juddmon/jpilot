/* print.h
 * 
 * Copyright (C) 2000 by Judd Montgomery
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
#ifndef _PRINT_H__
#define _PRINT_H__

#include "utils.h"
/* The print options window
 * The main window should be passed in if possible, or NULL
 * Returns:
 *  DIALOG_SAID_PRINT
 *  DIALOG_SAID_CANCEL
 *  <0 on error
 */
int print_gui(GtkWidget *main_window);

int print_datebook(int month, int day, int year);

int print_addresses(AddressList *address_list);

int print_todos(ToDoList *todo_list);

int print_memos(MemoList *todo_list);

#endif

/*******************************************************************************
 * todo.h
 * A module of J-Pilot http://jpilot.org
 * 
 * Copyright (C) 1999-2014 by Judd Montgomery
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

#ifndef __TODO_H__
#define __TODO_H__

#include <pi-todo.h>
#include "utils.h"

#define MAX_TODO_DESC_LEN     256
#define MAX_TODO_NOTE_LEN     4000

#define TODO_CHECK_COLUMN     0
#define TODO_PRIORITY_COLUMN  1
#define TODO_NOTE_COLUMN      2
#define TODO_DATE_COLUMN      3
#define TODO_TEXT_COLUMN      4

enum {
    TODO_CHECK_COLUMN_ENUM = 0,
    TODO_PRIORITY_COLUMN_ENUM,
    TODO_NOTE_COLUMN_ENUM,
    TODO_DATE_COLUMN_ENUM,
    TODO_TEXT_COLUMN_ENUM,
    TODO_DATA_COLUMN_ENUM,
    TODO_BACKGROUND_COLOR_ENUM,
    TODO_BACKGROUND_COLOR_ENABLED_ENUM,
    TODO_FOREGROUND_COLOR_ENUM,
    TODO_FORGROUND_COLOR_ENABLED_ENUM,
    TODO_NUM_COLS
};

void free_ToDoList(ToDoList **todo);
int get_todos(ToDoList **todo_list, int sort_order);
int get_todos2(ToDoList **todo_list, int sort_order,
               int modified, int deleted, int privates, int completed,
               int category);
int get_todo_app_info(struct ToDoAppInfo *ai);
int pc_todo_write(struct ToDo *todo, PCRecType rt, unsigned char attrib,
                  unsigned int *unique_id);
int todo_print(void);
int todo_import(GtkWidget *window);
int todo_export(GtkWidget *window);

/* Exported for datebook use only.  Don't use these */

void todo_liststore_clear(GtkListStore *pListStore);
void todo_update_liststore(GtkListStore *pListStore, GtkWidget *tooltip_widget,
                           ToDoList **todo_list, int category, int main);

#endif

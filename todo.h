/* todo.h
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
#ifndef __TODO_H__
#define __TODO_H__

#include <pi-todo.h>
#include "utils.h"

void free_ToDoList(ToDoList **todo);
int get_todos(ToDoList **todo_list, int sort_order);
int get_todos2(ToDoList **todo_list, int sort_order,
	       int modified, int deleted, int category);
int get_todo_app_info(struct ToDoAppInfo *ai);
int pc_todo_write(struct ToDo *todo, PCRecType rt, unsigned char attrib,
		  unsigned int *unique_id);
int todo_print();

#endif

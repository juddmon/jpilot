/* memo.h
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
 */
#ifndef __MEMO_H__
#define __MEMO_H__

#include <pi-memo.h>
#include "utils.h"

int get_memo_app_info(struct MemoAppInfo *ai);
void free_MemoList(MemoList **memo);
int get_memos(MemoList **memo_list, int sort_order);
int get_memos2(MemoList **memo_list, int sort_order,
 	       int modified, int deleted, int privates, int category);
int pc_memo_write(struct Memo *memo, PCRecType rt, unsigned char attrib,
		  unsigned int *unique_id);

int memo_print();
int memo_import(GtkWidget *window);
int memo_export(GtkWidget *window);

#endif

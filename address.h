/* address.h
 * A module of J-Pilot http://jpilot.org
 * 
 * Copyright (C) 1999-2001 by Judd Montgomery
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
#ifndef __ADDRESS_H__
#define __ADDRESS_H__

#include <pi-address.h>
#include "utils.h"

/* This flag will force sorting other than on the palm
 * 0 on palm: by lastname, first
 * 1 on palm: by company
 * 0 will sort same as on palm
 * 1 will reverse it
 */
extern int sort_override;

int get_address_app_info(struct AddressAppInfo *aai);
int pc_address_write(struct Address *a, PCRecType rt, unsigned char attrib,
		     unsigned int *unqiue_id);
void free_AddressList(AddressList **al);
/* 
 * sort_order: 0=descending,  1=ascending
 */
int get_addresses(AddressList **address_list, int sort_order);
int get_addresses2(AddressList **address_list, int sort_order,
		  int modified, int deleted, int privates, int category);

int address_print();
int address_import(GtkWidget *window);
int address_export(GtkWidget *window);

#endif

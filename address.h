/* $Id: address.h,v 1.11 2007/10/23 18:29:14 judd Exp $ */

/*******************************************************************************
 * address.h
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

#ifndef __ADDRESS_H__
#define __ADDRESS_H__

#include <pi-address.h>
#include "utils.h"

/* This flag affects sorting of address records
 * 0 : by lastname, first
 * 1 : by company name
 */
extern int sort_by_company;

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

/* Contact header */

int get_contact_app_info(struct ContactAppInfo *cai);

int pc_contact_write(struct Contact *c, PCRecType rt, unsigned char attrib,
		     unsigned int *unqiue_id);

void free_ContactList(ContactList **cl);

int get_contacts(ContactList **contact_list, int sort_order);
int get_contacts2(ContactList **contact_list, int sort_order,
		  int modified, int deleted, int privates, int category);

int copy_address_ai_to_contact_ai(const struct AddressAppInfo *aai, struct ContactAppInfo *cai);

int copy_contact_to_address(const struct Contact *c, struct Address *a);

int copy_address_to_contact(const struct Address *a, struct Contact *c);

int copy_addresses_to_contacts(AddressList *al, ContactList **cl);

#endif

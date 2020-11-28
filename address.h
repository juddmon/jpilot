/*******************************************************************************
 * address.h
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

#ifndef __ADDRESS_H__
#define __ADDRESS_H__

#include <pi-address.h>
#include "utils.h"

/*
 * This describes how to draw a GUI entry for each field in an address record
 */
typedef struct {
   int record_field;
   int notebook_page;
   int type;
} address_schema_entry;

static char *const ADDRESS_LAST_NAME_COMPANY = "Last Name/Company";
static char *const ADDRESS_FIRST_NAME_COMPANY = "First Name/Company";
static char *const ADDRESS_COMPANY_LAST_NAME = "Company/Last Name";
#define NUM_IMS 2
#define NUM_ADDRESSES 3
#define NUM_CONTACT_NOTEBOOK_PAGES 6
#define NUM_ADDRESS_NOTEBOOK_PAGES 4
#define NUM_ADDRESS_FIELDS 19

#define ADDRESS_GUI_LABEL_TEXT 2 /* Show a label and a textview widget */
#define ADDRESS_GUI_DIAL_SHOW_PHONE_MENU_TEXT 3 /* Show a dial button, show in list radio button, and a phone menu, and a textview */
#define ADDRESS_GUI_IM_MENU_TEXT 4 /* Show a IM menu and a textview */
#define ADDRESS_GUI_ADDR_MENU_TEXT 5 /* Show a address menu and a textview */
#define ADDRESS_GUI_WEBSITE_TEXT 6 /* Show a website button and a textview */
#define ADDRESS_GUI_BIRTHDAY 7 /* Show a birthdate checkbox and complex birthday GUI */

#define SORT_BY_LNAME 1
#define SORT_BY_FNAME 2
#define SORT_BY_COMPANY 4

/* This flag affects sorting of address records */
extern int addr_sort_order;

int get_address_app_info(struct AddressAppInfo *aai);

int pc_address_write(struct Address *a, PCRecType rt, unsigned char attrib,
                     unsigned int *unqiue_id);

void free_AddressList(AddressList **al);

int get_addresses(AddressList **address_list, int sort_order);
int get_addresses2(AddressList **address_list, int sort_order,
                  int modified, int deleted, int privates, int category);

int address_print(void);
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

/* $Id: jp-contact.c,v 1.12 2009/08/11 20:54:20 rikster5 Exp $ */

/*******************************************************************************
 * contact.c:  Translate Palm contact data formats
 * A module of J-Pilot http://jpilot.org
 *
 * Copyright (C) 2003-2006 by Judd Montgomery
 *
 * This code is NOT derived from contact.c from pilot-link
 *  pilot-link's contact.c was based on this code.
 * This code was however based on address.c and was originally written to 
 *  be part of pilot-link, however licensing issues 
 *  prevent this code from being part of pilot-link.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pi-macros.h"
#include "jp-pi-contact.h"
#include "config.h"
 
#include <pi-contact.h>
void jp_free_Contact(struct Contact *c)
{
   free_Contact(c);
}

int jp_unpack_Contact(struct Contact *c, pi_buffer_t *buf)
{
   // Pilot-link doesn't do anything with the contactsType parameter yet
   return unpack_Contact(c, buf, contacts_v10);
}

int jp_pack_Contact(struct Contact *c, pi_buffer_t *buf)
{
   // Pilot-link doesn't do anything with the contactsType parameter yet
   return pack_Contact(c, buf, contacts_v10);
}

int jp_Contact_add_blob(struct Contact *c, struct ContactBlob *blob)
{
   return Contact_add_blob(c, blob);
}

int jp_Contact_add_picture(struct Contact *c, struct ContactPicture *p)
{
   return Contact_add_picture(c, p);
}

int jp_unpack_ContactAppInfo(struct ContactAppInfo *ai, pi_buffer_t *buf)
{
   return unpack_ContactAppInfo(ai, buf);
}

int jp_pack_ContactAppInfo(struct ContactAppInfo *ai, pi_buffer_t *buf)
{
   return pack_ContactAppInfo(ai, buf);
}


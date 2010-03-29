/* $Id: jp-pi-contact.h,v 1.8 2010/03/29 05:44:29 rikster5 Exp $ */

/*******************************************************************************
 * jp-pi-contact.h:  Translate Palm contact data formats
 * A module of J-Pilot http://jpilot.org
 *
 * Copyright (C) 2003-2006 by Judd Montgomery
 *
 * This code is NOT derived from pi-contact.h from pilot-link.
 *  pilot-link's pi-contact.h was based on this code.
 * This code was however based on pi-address.h and was originally written to 
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

#include "config.h"
#include <pi-contact.h>

#define NUM_CONTACT_FIELDS  40

#ifdef __cplusplus
extern "C" {
#endif
extern void jp_free_Contact
    PI_ARGS((struct Contact *));
extern int jp_unpack_Contact
    PI_ARGS((struct Contact *, pi_buffer_t *));
extern int jp_pack_Contact
    PI_ARGS((struct Contact *, pi_buffer_t *));
extern int jp_unpack_ContactAppInfo
    PI_ARGS((struct ContactAppInfo *, pi_buffer_t *));
extern int jp_pack_ContactAppInfo
    PI_ARGS((struct ContactAppInfo *, pi_buffer_t *buf));

extern int jp_Contact_add_blob
    PI_ARGS((struct Contact *, struct ContactBlob *));
extern int jp_Contact_add_picture
    PI_ARGS((struct Contact *, struct ContactPicture *));
#ifdef __cplusplus
}
#endif


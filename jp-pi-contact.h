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
#ifndef _PILOT_CONTACT_H_
#define _PILOT_CONTACT_H_

#include <pi-args.h>
#include <pi-appinfo.h>
#include <pi-buffer.h>
#include <time.h>

#define MAX_CONTACT_VERSION 11

#define NUM_CONTACT_ENTRIES 39
#define NUM_CONTACT_FIELDS 40

#define NUM_CONTACT_V10_LABELS 49
#define NUM_CONTACT_V11_LABELS 53

/* Blob types, or blob creator IDs can range from BD00 - Bd09 for Contacts */
#define BLOB_TYPE_PICTURE_ID "Bd00"
#define MAX_CONTACT_BLOBS 10

#ifdef __cplusplus
extern "C" {
#endif

	/* Entry fields */
	enum {  contLastname, 
		contFirstname, 
		contCompany, 
		contTitle,
		contPhone1, 
		contPhone2,
		contPhone3,
		contPhone4,
		contPhone5,
		contPhone6,
		contPhone7,
		contIM1,
		contIM2,
		contWebsite,
		contCustom1,
		contCustom2,
		contCustom3,
		contCustom4,
		contCustom5,
		contCustom6,
		contCustom7,
		contCustom8,
		contCustom9,
		contAddress1,
		contCity1,
		contState1,
		contZip1,
		contCountry1,
		contAddress2,
		contCity2,
		contState2,
		contZip2,
		contCountry2,
		contAddress3,
		contCity3,
		contState3,
		contZip3,
		contCountry3,
		contNote
	};

	/* Non-entry fields */
	enum {	contBirthday = contNote + 1,
	     contPicture
	};

	struct ContactBlob {
		/* type ranges from "Bd00" - "Bd09" */
		char type[4];
		int length;
		unsigned char *data;
	};

	struct ContactPicture {
		/* The picture pointer is only for convienience and
		 * will point to the 3rd byte of the last picture blob.
		 * The data will not need to be freed.  The blob structure will. */
		unsigned int dirty;
		/* data points to blob data in jpeg format */
		unsigned int length;
		unsigned char *data;
	};

	struct Contact {
		int phoneLabel[7];
		int addressLabel[3];
		int IMLabel[2];
		int showPhone;
		int birthdayFlag;
		int reminder;
		int advance;
		int advanceUnits;	   
		struct tm birthday;
		char *entry[39];
		struct ContactBlob *blob[MAX_CONTACT_BLOBS];
		struct ContactPicture *picture;
	};

	struct ContactAppInfo {
		int version;
		int num_labels;
		struct CategoryAppInfo category;
		char unknown1[26];		/* Palm has not documented what this is */
		char labels[53][16];		/* Hairy to explain, obvious to look at 		*/
		/*int labelRenamed[53];*/	/* list of booleans showing which labels were modified 	*/
		int country;
		int sortByCompany;
		char phoneLabels[8][16];	/* Duplication of some labels, to greatly reduce hair 	*/
		char addrLabels[3][16];		/* Duplication of some labels, to greatly reduce hair 	*/
		char IMLabels[5][16];		/* Duplication of some labels, to greatly reduce hair 	*/
	};

	extern void free_Contact PI_ARGS((struct Contact *));
	extern int unpack_Contact
	    PI_ARGS((struct Contact *, pi_buffer_t *));
	extern int pack_Contact
	    PI_ARGS((struct Contact *, pi_buffer_t *));
	extern int unpack_ContactAppInfo
	    PI_ARGS((struct ContactAppInfo *, pi_buffer_t *));
	extern int pack_ContactAppInfo
	    PI_ARGS((struct ContactAppInfo *, pi_buffer_t *buf));

	extern int Contact_add_blob
	    PI_ARGS((struct Contact *, struct ContactBlob *));
	extern int Contact_add_picture
	    PI_ARGS((struct Contact *, struct ContactPicture *));

#ifdef __cplusplus
}
#include "pi-contact.hxx"
#endif				/* __cplusplus */
#endif				/* _PILOT_CONTACT_H_ */

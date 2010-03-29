/* $Id: password.h,v 1.10 2010/03/29 05:44:30 rikster5 Exp $ */

/*******************************************************************************
 * password.h
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

#ifndef __PASSWORD_H__
#define __PASSWORD_H__

#define SHOW_PRIVATES 1
#define MASK_PRIVATES 2
#define HIDE_PRIVATES 0
#define GET_PRIVATES -1

#define PASSWD_LEN 32

/*
 * hide passed 1 will set the hide flag
 * hide passed 0 will unset the hide flag
 * hide passed -1 will return the current hide flag
 * hide flag is always returned
 * The caller will have to ensure a password was entered before calling here
 */
int show_privates(int hide);

#ifdef ENABLE_PRIVATE
void palm_encode_hash(unsigned char *ascii, unsigned char *encoded);

void palm_encode_md5(unsigned char *ascii, unsigned char *encoded);

/* len is the length of the bin str, hex_str must be at least twice as long */
void bin_to_hex_str(unsigned char *bin, char *hex_str, int len);

int dialog_password(GtkWindow *main_window, char *ascii_password, int retry);

int verify_password(char *password);

#endif /* ENABLE_PRIVATE */

#endif

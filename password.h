/* password.h
 *
 * Copyright (C) 1999 by Judd Montgomery
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
#ifndef __PASSWORD_H__
#define __PASSWORD_H__

#ifdef ENABLE_PRIVATE

/* These are booleans, so don't change */
#define SHOW_PRIVATES 1
#define HIDE_PRIVATES 0
#define GET_PRIVATES -1

#define PASSWD_LEN 32

void palm_encode(unsigned char *ascii, unsigned char *encoded);
/*
 * hide passed 1 will set the hide flag
 * hide passed 0 will unset the hide flag and also need a correct password
 * hide passed -1 will return the current hide flag
 * hide flag is always returned
 */
int show_privates(int hide, char *password);

/* len is the length of the bin str, hex_str must be at least twice as long */
void bin_to_hex_str(unsigned char *bin, char *hex_str, int len);

#endif

int dialog_password(char *ascii_password);

#endif

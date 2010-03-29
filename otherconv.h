/* $Id: otherconv.h,v 1.5 2010/03/29 05:44:30 rikster5 Exp $ */

/*******************************************************************************
 * otherconv.h
 * A module of J-Pilot http://jpilot.org
 *
 * Copyright (C) 2004 by Amit Aronovitch 
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

/*
 * General charset conversion library header (using gconv)
 * Convert Palm  <-> Unix:
 * Palm : Any - according to the "other-pda-charset" setup option.
 * Unix : UTF-8
 */

/* otherconv_init: Call this before any conversion 
 * (also use whenever other-pda-charset option changed) 
 * 
 * Returns 0 if OK, -1 if iconv could not be initialized
 *  (probably because of bad charset string)
 */
int otherconv_init(void);
/* otherconv_free: Call this when done */ 
void otherconv_free(void); 

char *other_to_UTF(const char *buf, int buf_len);
void UTF_to_other(char *const buf, int buf_len);

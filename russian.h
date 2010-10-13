/* $Id: russian.h,v 1.5 2010/10/13 03:18:59 rikster5 Exp $ */

/*******************************************************************************
 * russian.h
 * A module of J-Pilot http://jpilot.org
 *
 * Copyright (C) 2000 by Gennady Kudelya
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
        Header for Russian library
        Convert Palm <-> Unix:
                Palm  : koi8
                Unix  : Win1251
*/

void koi8_to_win1251(char *const buf, int buf_len);
void win1251_to_koi8(char *const buf, int buf_len);

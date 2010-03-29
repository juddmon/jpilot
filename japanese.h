/* $Id: japanese.h,v 1.8 2010/03/29 05:44:29 rikster5 Exp $ */

/*******************************************************************************
 * japanese.h
 * A module of J-Pilot http://jpilot.org
 *
 * Copyright (C) 1999 by Hiroshi Kawashima
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
        Header for Japanization library
        Convert Palm <-> Unix Japanese Code, ie:
                Palm : SJIS
                Unix : EUC
*/

void Sjis2Euc(char *buf, int max_len);
void Euc2Sjis(char *buf, int max_len);
void jp_Sjis2Euc(char *buf, int max_len);

/*******************************************************************************
 * cp1250.h
 * A module of J-Pilot http://jpilot.org
 *
 * Copyright (C) 2002 by Jiri Rubes
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
 * Czech, Polish (and other CP 1250 languages) library header
 * Convert charsets: Palm <-> Unix:
 * Palm : CP 1250
 * Unix : ISO-8859-2
 */

void Win2Lat(char *const buf, int buf_len);
void Lat2Win(char *const buf, int buf_len);


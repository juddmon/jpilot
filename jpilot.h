/* $Id: jpilot.h,v 1.4 2010/10/12 03:25:38 rikster5 Exp $ */

/*******************************************************************************
 * jpilot.c
 * A module of J-Pilot http://jpilot.org
 *
 * Copyright (C) 1999-2003 by Judd Montgomery
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

/* GUI cleanup routines for weekview and monthview windows */
void cb_monthview_quit(GtkWidget *widget, gpointer data);
void cb_weekview_quit(GtkWidget *widget, gpointer data);


/*******************************************************************************
 * export.h
 * A module of J-Pilot http://jpilot.org
 * 
 * Copyright (C) 1999-2014 by Judd Montgomery
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

#ifndef __EXPORT_H__
#define __EXPORT_H__

#include <gtk/gtk.h>
#include <pi-appinfo.h>

int export_gui(GtkWidget *main_window,
               int w, int h, int x, int y,
               int columns,
               struct sorted_cats *sort_l,
               int pref_export,
               char *type_text[],
               int type_int[],
               void (*cb_export_menu)(GtkWidget *clist, int category),
               void (*cb_export_done)(GtkWidget *widget,
                                      const char *filename),
               void (*cb_export_ok)(GtkWidget *export_window,
                                    GtkWidget *clist,
                                    int type,
                                    const char *filename)
               );

/*
 * Actually, this should be in import.h, but I didn't want to create a whole
 * header file just for one function, so its here for now.
 * The code is in import_gui.c
 */
int read_csv_field(FILE *in, char *text, int size);

int export_browse(GtkWidget *main_window, int pref_export);

#endif

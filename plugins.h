/* $Id: plugins.h,v 1.11 2009/11/08 17:12:10 rousseau Exp $ */

/*******************************************************************************
 * plugins.h
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

#include "config.h"
#ifdef  ENABLE_PLUGINS

#ifndef __PLUGINS_H__
#define __PLUGINS_H__

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include "libplugin.h"
#include "prefs.h"

#define MAX_NUM_PLUGINS 50

struct plugin_s
{
   char *full_path;
   void *handle;
   unsigned char sync_on;
   unsigned char user_only;
   char *name;
   char *menu_name;
   char *help_name;
   char *db_name;
   int number;
   int (*plugin_get_name)(char *name, int len);
   int (*plugin_get_menu_name)(char *name, int len);
   int (*plugin_get_help_name)(char *name, int len);
   int (*plugin_get_db_name)(char *db_name, int len);
   int (*plugin_startup)(jp_startup_info *info);
   int (*plugin_gui)(GtkWidget *vbox, GtkWidget *hbox, unsigned int unique_id);
   int (*plugin_help)(char **text, int *width, int *height);
   int (*plugin_print)(void);
   int (*plugin_import)(GtkWidget *window);
   int (*plugin_export)(GtkWidget *window);
   int (*plugin_gui_cleanup)(void);
   int (*plugin_pre_sync_pre_connect)(void);
   int (*plugin_pre_sync)(void);
   int (*plugin_sync)(int sd);
   int (*plugin_search)(const char *search_string, int case_sense, struct search_result **sr);
   int (*plugin_post_sync)(void);
   int (*plugin_exit_cleanup)(void);
   int (*plugin_unpack_cai_from_ai)(struct CategoryAppInfo *cai,
				    unsigned char *ai_raw, int len);
   int (*plugin_pack_cai_into_ai)(struct CategoryAppInfo *cai,
				  unsigned char *ai_raw, int len);
};

int load_plugins(void);
GList *get_plugin_list(void);

/*
 * Free the search_result record list
 */
void free_search_result(struct search_result **sr);

/*
 * Write out the jpilot.plugins file that tells which plugins to sync
 */
void write_plugin_sync_file(void);

/*
 * Free the plugin_list
 */
void free_plugin_list(GList **plugin_list);

#endif
#endif

/* plugins.h
 * 
 * Copyright (C) 1999 by Judd Montgomery
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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
#include "config.h"
#ifdef  ENABLE_PLUGINS

# ifndef __PLUGINS_H__
#define __PLUGINS_H__

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>

struct search_result
{
   char *line;
   unsigned int unique_id;
   struct search_result *next;
};

typedef struct
{
   char *base_dir;
   int *major_version;
   int *minor_version;
} jp_startup_info;

struct plugin_s
{
   char *full_path;
   void *handle;
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
   int (*plugin_gui_cleanup)(void);
   int (*plugin_pre_sync)(void);
   int (*plugin_sync)(int sd);
   int (*plugin_search)(char *search_string, int case_sense, struct search_result **sr);
   int (*plugin_post_sync)(void);
   int (*plugin_exit_cleanup)(void);
};

int load_plugins();
GList *get_plugin_list();

/*
 * Read a pdb file out of the $(HOME)/.j directory
 * It also reads the PC file
 */
int jp_read_DB_files(char *DB_name, GList *records);

/*
 * Free the record list
 */
int jp_free_DB_records(GList **records);

/*
 * Free the search_result record list
 */
void free_search_result(struct search_result **sr);

/*
 * Free the plugin_list
 */
void free_plugin_list(GList **plugin_list);

/*
 *  * Write a new Record
 *  */
int jp_write_new_DB_record(void *new_record);

/*
 * Delete a Record
 */
int jp_delete_DB_record(void *record);

/* functions that may, or may not be implemented by the plugin */

/*
 * This will be called during the sync process.
 * DB_name is the name of a DB to be synced during the Sync process.
 * You can not implement this function if syncing isn't desired.
 */


/*
 * These should be implemented by the plugin library if needed
 */
int plugin_get_name(char *name, int len);
int plugin_get_menu_name(char *name, int len);
int plugin_get_help_name(char *name, int len);
int plugin_get_db_name(char *name, int len);
int plugin_startup(jp_startup_info *info);
int plugin_gui(GtkWidget *vbox, GtkWidget *hbox, unsigned int unique_id);
int plugin_help(char **text, int *width, int *height);
int plugin_gui_cleanup(void);
int plugin_pre_sync(void);
int plugin_sync(int sd);
int plugin_post_sync(void);
int plugin_exit_cleanup(void);
                            
#endif
#endif

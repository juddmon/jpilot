/* plugins.c
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
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utils.h"
#include "log.h"
#include "plugins.h"
#include <dlfcn.h>

GList *plugins = NULL;

static int get_plugin_info(struct plugin_s *p, char *path);
  
int load_plugins()
{
   DIR *dir;
   struct dirent *dirent;
   int i, r;
   char path[256];
   char full_name[256];
   int count, number;
   struct plugin_s temp_plugin, *new_plugin;
   GList *temp_list;
   
   count = 0;
   number = DATEBOOK + 100; /* I just made up this number */
   plugins = NULL;
   
   g_snprintf(path, 250, "%s/%s/%s/%s/", BASE_DIR, "share", EPN, "plugins");
   jpilot_logf(LOG_DEBUG, "opening dir %s\n", path);
   dir = opendir(path);
   if (dir) {
      for(i=0; (dirent = readdir(dir)); i++) {
	 if (strcmp(&(dirent->d_name[strlen(dirent->d_name)-3]), ".so")) {
	    continue;
	 } else {
	    jpilot_logf(LOG_DEBUG, "found plugin %s\n", dirent->d_name);
	    g_snprintf(full_name, 250, "%s/%s", path, dirent->d_name);
	    r = get_plugin_info(&temp_plugin, full_name);
	    temp_plugin.number = number;
	    if (r==0) {
	       if (temp_plugin.name) {
		  jpilot_logf(LOG_DEBUG, "plugin name is [%s]\n", temp_plugin.name);
	       }
	       new_plugin = malloc(sizeof(struct plugin_s));
	       if (!new_plugin) {
		  jpilot_logf(LOG_DEBUG, "Out of memory in load_plugins()\n");
		  return count;
	       }
	       memcpy(new_plugin, &temp_plugin, sizeof(struct plugin_s));
	       plugins = g_list_append(plugins, new_plugin);
	       count++;
	       number++;
	    }
	 }
      }
      if (dir) {
	 closedir(dir);
      }
   }
   
   get_home_file_name("", path, 240);
   strcat(path, "/plugins/");
   jpilot_logf(LOG_DEBUG, "opening dir %s\n", path);
   dir = opendir(path);
   if (dir) {
      for(; (dirent = readdir(dir)); i++) {
	 if (strcmp(&(dirent->d_name[strlen(dirent->d_name)-3]), ".so")) {
	    continue;
	 } else {
	    jpilot_logf(LOG_DEBUG, "found plugin %s\n", dirent->d_name);
	    g_snprintf(full_name, 250, "%s/%s", path, dirent->d_name);
	    r = get_plugin_info(&temp_plugin, full_name);
	    temp_plugin.number = number;
	    if (r==0) {
	       if (temp_plugin.name) {
		  jpilot_logf(LOG_DEBUG, "plugin name is [%s]\n", temp_plugin.name);
	       }
	       new_plugin = malloc(sizeof(struct plugin_s));
	       memcpy(new_plugin, &temp_plugin, sizeof(struct plugin_s));
	       if (!new_plugin) {
		  jpilot_logf(LOG_DEBUG, "Out of memory in load_plugins()\n");
		  return count;
	       }
	       plugins = g_list_append(plugins, new_plugin);
	       count++;
	       number++;
	    }
	 }
      }
   }
   if (dir) {
      closedir(dir);
   }
   /* Go to first entry in the list */
   for (temp_list = plugins; temp_list; temp_list = temp_list->prev) {
      plugins = temp_list;
   }
   return count;
}

static int get_plugin_info(struct plugin_s *p, char *path)
{
   void *h;
   const char *err;
   char name[52];
   char db_name[52];
   int version, major_version, minor_version;
   /* void (*plugin_set_jpilot_logf)(int (*Pjpilot_logf)(int level, char *format, ...));*/
   void (*plugin_version)(int *major_version, int *minor_version);
   
   p->full_path = NULL;
   p->handle = NULL;
   p->name = NULL;
   p->db_name = NULL;
   p->number = 0;
   p->plugin_get_name = NULL;
   p->plugin_get_menu_name = NULL;
   p->plugin_get_help_name = NULL;
   p->plugin_get_db_name = NULL;
   p->plugin_startup = NULL;
   p->plugin_gui = NULL;
   p->plugin_help = NULL;
   p->plugin_gui_cleanup = NULL;
   p->plugin_pre_sync = NULL;
   p->plugin_sync = NULL;
   p->plugin_post_sync = NULL;
   p->plugin_exit_cleanup = NULL;
   
   h = dlopen(path, RTLD_LAZY);
   if (!h) {
      jpilot_logf(LOG_WARN, "open failed on plugin [%s]\n error [%s]\n", path,
		  dlerror());
      return -1;
   }
   jpilot_logf(LOG_DEBUG, "opened plugin [%s]\n", path);
   p->handle=h;
   
   /* logf */
/*
   plugin_set_jpilot_logf = dlsym(h, "plugin_set_jpilot_logf");
   if ((err = dlerror()) != NULL)  {
      jpilot_logf(LOG_WARN, "plugin_set_jpilot_logf, [%s]\n [%s]\n", err, path);
   } else {
      plugin_set_jpilot_logf(jpilot_logf);
   }
*/

   p->full_path = strdup(path);

   /* plugin_version */
   plugin_version = dlsym(h, "plugin_version");
   if (plugin_version==NULL)  {
      err = dlerror();
      jpilot_logf(LOG_WARN, "plugin_version: [%s]\n", err);
      jpilot_logf(LOG_WARN, " plugin is invalid: [%s]\n", path);
      dlclose(h);
      p->handle=NULL;
      return -1;
   }
   plugin_version(&major_version, &minor_version);
   version=major_version*1000+minor_version;
   if ((major_version < 0) && (minor_version < 95)) {
      jpilot_logf(LOG_WARN, "This plugin version (%d.%d) is too old.\n",
		  major_version, minor_version);
      jpilot_logf(LOG_WARN, " plugin is invalid: [%s]\n", path);
      dlclose(h);
      p->handle=NULL;
      return -1;
   }
   jpilot_logf(LOG_DEBUG, "This plugin is version (%d.%d).\n",
	       major_version, minor_version);


   /* plugin_get_name */
   p->plugin_get_name = dlsym(h, "plugin_get_name");
   if (p->plugin_get_name==NULL)  {
      err = dlerror();
      jpilot_logf(LOG_WARN, "plugin_get_name: [%s]\n", err);
      jpilot_logf(LOG_WARN, " plugin is invalid: [%s]\n", path);
      dlclose(h);
      p->handle=NULL;
      return -1;
   }

   if (p->plugin_get_name) {
      p->plugin_get_name(name, 50);
      name[50]='\0';
      p->name = strdup(name);
   } else {
      p->name = NULL;
   }
   
   
   
   /* plugin_get_menu_name */
   p->plugin_get_menu_name = dlsym(h, "plugin_get_menu_name");
   if (p->plugin_get_menu_name) {
      p->plugin_get_menu_name(name, 50);
      name[50]='\0';
      p->menu_name = strdup(name);
   } else {
      p->menu_name = NULL;
   }
   

   /* plugin_get_help_name */
   p->plugin_get_help_name = dlsym(h, "plugin_get_help_name");
   if (p->plugin_get_help_name) {
      p->plugin_get_help_name(name, 50);
      name[50]='\0';
      p->help_name = strdup(name);
   } else {
      p->help_name = NULL;
   }
   



   /* plugin_get_db_name */
   p->plugin_get_db_name = dlsym(h, "plugin_get_db_name");

   if (p->plugin_get_db_name) {
      p->plugin_get_db_name(db_name, 50);
      db_name[50]='\0';
   } else {
      db_name[0]='\0';
   }
  
   p->db_name = strdup(db_name);

   
   /* plugin_gui */
   p->plugin_gui = dlsym(h, "plugin_gui");

   /* plugin_help */
   p->plugin_help = dlsym(h, "plugin_help");

   /* plugin_gui_cleanup */
   p->plugin_gui_cleanup = dlsym(h, "plugin_gui_cleanup");

   /* plugin_startup */
   p->plugin_startup = dlsym(h, "plugin_startup");

   /* plugin_pre_sync */
   p->plugin_pre_sync = dlsym(h, "plugin_pre_sync");

   /* plugin_sync */
   p->plugin_sync = dlsym(h, "plugin_sync");

   /* plugin_post_sync */
   p->plugin_post_sync = dlsym(h, "plugin_post_sync");

   /* plugin_search */
   p->plugin_search = dlsym(h, "plugin_search");

   /* plugin_exit_cleanup */
   p->plugin_exit_cleanup = dlsym(h, "plugin_exit_cleanup");

   return 0;
}

/* This will always return the first plugin list entry */
GList *get_plugin_list()
{
   return plugins;
}

void free_plugin_list(GList **plugin_list)
{
   GList *temp_list;
   struct plugin_s *p;
   
   /* Go to first entry in the list */
   for (temp_list = *plugin_list; temp_list; temp_list = temp_list->prev) {
      *plugin_list = temp_list;
   }
   for (temp_list = *plugin_list; temp_list; temp_list = temp_list->next) {
      if (temp_list->data) {
	 p=temp_list->data;
	 if (p->full_path) {
	    free(p->full_path);
	 }
	 if (p->name) {
	    free(p->name);
	 }
	 if (p->db_name) {
	    free(p->db_name);
	 }
	 free(p);
      }
   }
   g_list_free(*plugin_list);
   *plugin_list=NULL;
}

void free_search_result(struct search_result **sr)
{
   struct search_result *temp_sr, *temp_sr_next;

   for (temp_sr = *sr; temp_sr; temp_sr=temp_sr_next) {
      if (temp_sr->line) {
	 free(temp_sr->line);
      }
      temp_sr_next = temp_sr->next;
      free(temp_sr);
   }
   *sr = NULL;
}

#endif  /* ENABLE_PLUGINS */

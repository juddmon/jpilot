/* $Id: plugins.c,v 1.22 2008/08/06 19:16:17 rousseau Exp $ */

/*******************************************************************************
 * plugins.c
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

/********************************* Includes ***********************************/
#include "config.h"
#ifdef  ENABLE_PLUGINS
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "utils.h"
#include "log.h"
#include "plugins.h"
#include <dlfcn.h>
#include "i18n.h"

/******************************* Global vars **********************************/
GList *plugins = NULL;

/****************************** Prototypes ************************************/
static int get_plugin_info(struct plugin_s *p, char *path);
static int get_plugin_sync_bits();
gint plugin_sort(gconstpointer a, gconstpointer b);

/****************************** Main Code *************************************/
/* Write out the jpilot.plugins file that tells which plugins to sync */
void write_plugin_sync_file()
{
   FILE *out;
   GList *temp_list;
   struct plugin_s *Pplugin;

   out=jp_open_home_file(EPN".plugins", "w");
   if (!out) {
      return;
   }
   fwrite("Version 1\n", strlen("Version 1\n"), 1, out);
   for (temp_list = plugins; temp_list; temp_list = temp_list->next) {
      Pplugin = temp_list->data;
      if (Pplugin) {
	 if (Pplugin->sync_on) {
	    fwrite("Y ", 2, 1, out);
	 } else {
	    fwrite("N ", 2, 1, out);
	 }
	 fwrite(Pplugin->full_path, strlen(Pplugin->full_path), 1, out);
	 fwrite("\n", strlen("\n"), 1, out);
      }
   }
   fclose(out);
}

/*
 * This is just a repeated subroutine to load_plugins not needing
 * a name of its own.
 * Assumes dir has already been checked
 */
int load_plugins_sub1(DIR *dir, char *path, int *number, unsigned char user_only)
{
   int i, r;
   int count;
   struct dirent *dirent;
   char full_name[FILENAME_MAX];
   struct plugin_s temp_plugin, *new_plugin;
   GList *plugin_names = NULL; /* keep a list of plugins found so far */
   GList *temp_list = NULL;

   count = 0;
   for (i=0; (dirent = readdir(dir)); i++) {
      if (i>1000) {
	 jp_logf(JP_LOG_WARN, "load_plugins_sub1(): %s\n", _("infinite loop"));
	 return 0;
      }
      /* If the filename has either of these extensions then plug it in */
      if ((strcmp(&(dirent->d_name[strlen(dirent->d_name)-3]), ".so")) &&
	  (strcmp(&(dirent->d_name[strlen(dirent->d_name)-3]), ".sl")) &&
	  (strcmp(&(dirent->d_name[strlen(dirent->d_name)-6]), ".dylib"))) {
	 continue;
      } else {
	 jp_logf(JP_LOG_DEBUG, "found plugin %s\n", dirent->d_name);
	 /* We know path has a trailing slash after it */
	 g_snprintf(full_name, sizeof(full_name), "%s%s", path, dirent->d_name);
	 r = get_plugin_info(&temp_plugin, full_name);
	 temp_plugin.number = *number;
	 temp_plugin.user_only = user_only;
	 if (r==0) {
	    if (temp_plugin.name) {
	       jp_logf(JP_LOG_DEBUG, "plugin name is [%s]\n", temp_plugin.name);
	    }
	    if (g_list_find_custom(plugin_names, temp_plugin.name, (GCompareFunc)strcmp) == NULL) {
	       new_plugin = malloc(sizeof(struct plugin_s));
	       if (!new_plugin) {
		  jp_logf(JP_LOG_WARN, "load plugins(): %s\n", _("Out of memory"));
		  return count;
	       }
	       memcpy(new_plugin, &temp_plugin, sizeof(struct plugin_s));
	       plugins = g_list_prepend(plugins, new_plugin);
	       plugin_names = g_list_prepend(plugin_names, g_strdup(temp_plugin.name));
	       count++;
	       (*number)++;
	    }
	 }
      }
   }

   plugins = g_list_sort(plugins, plugin_sort);
   for (temp_list = plugin_names; temp_list; temp_list = temp_list->next) {
      if (temp_list->data) {
	 g_free(temp_list->data);
      }
   }
   g_list_free(plugin_names);

   return count;
}

gint plugin_sort(gconstpointer a, gconstpointer b)
{
   const char *ca = ((struct plugin_s *)a)->menu_name;
   const char *cb = ((struct plugin_s *)b)->menu_name;

   /* menu_name is NULL for plugin without menu entry */
   if (ca == NULL)
     ca = ((struct plugin_s *)a)->name;

   if (cb == NULL)
     cb = ((struct plugin_s *)b)->name;

   return strcasecmp(ca, cb);
}

int load_plugins()
{
   DIR *dir;
   char path[FILENAME_MAX];
   int count, number;

   count = 0;
   number = DATEBOOK + 100; /* I just made up this number */
   plugins = NULL;

   /* ABILIB is for Irix, should normally be "lib" */
   g_snprintf(path, sizeof(path), "%s/%s/%s/%s/", BASE_DIR, ABILIB, EPN, "plugins");
   jp_logf(JP_LOG_DEBUG, "opening dir %s\n", path);
   cleanup_path(path);
   dir = opendir(path);
   if (dir) {
      count += load_plugins_sub1(dir, path, &number, 0);
      closedir(dir);
   }

   get_home_file_name("plugins/", path, sizeof(path));
   cleanup_path(path);
   jp_logf(JP_LOG_DEBUG, "opening dir %s\n", path);
   dir = opendir(path);
   if (dir) {
      count += load_plugins_sub1(dir, path, &number, 1);
      closedir(dir);
   }

   get_plugin_sync_bits();

   return count;
}

/* Now we need to look in the jpilot_plugins file to see which plugins
 * are enabled to sync and which are not
 */
static int get_plugin_sync_bits()
{
   int i;
   GList *temp_list;
   struct plugin_s *Pplugin;
   char line[1024];
   char *Pline;
   char *Pc;
   FILE *in;

   in=jp_open_home_file(EPN".plugins", "r");
   if (!in) {
      return EXIT_SUCCESS;
   }
   for (i=0; (!feof(in)); i++) {
      if (i>MAX_NUM_PLUGINS) {
	 jp_logf(JP_LOG_WARN, "load_plugins(): %s\n", _("infinite loop"));
	 fclose(in);
	 return EXIT_FAILURE;
      }
      line[0]='\0';
      Pc = fgets(line, sizeof(line), in);
      if (!Pc) {
	 break;
      }
      if (line[strlen(line)-1]=='\n') {
	 line[strlen(line)-1]='\0';
      }
      if ((!strncmp(line, "Version", 7)) && (strcmp(line, "Version 1"))) {
	 jp_logf(JP_LOG_WARN, _("While reading %s%s line 1:[%s]\n"), EPN, ".plugins", line);
	 jp_logf(JP_LOG_WARN, _("Wrong Version\n"));
	 jp_logf(JP_LOG_WARN, _("Check preferences->conduits\n"));
	 fclose(in);
	 return EXIT_FAILURE;
      }
      if (i>0) {
	 if (toupper(line[0])=='N') {
	    Pline = line + 2;
	    for (temp_list = plugins; temp_list; temp_list = temp_list->next) {
	       Pplugin = temp_list->data;
	       if (!strcmp(Pline, Pplugin->full_path)) {
		  Pplugin->sync_on=0;
	       }
	    }
	 }
      }
   }
   fclose(in);
   return EXIT_SUCCESS;
}

static int get_plugin_info(struct plugin_s *p, char *path)
{
   void *h;
   const char *err;
   char name[52];
   char db_name[52];
   int version, major_version, minor_version;
   void (*plugin_versionM)(int *major_version, int *minor_version);

   p->full_path = NULL;
   p->handle = NULL;
   p->sync_on = 1;
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
   p->plugin_print = NULL;
   p->plugin_import = NULL;
   p->plugin_export = NULL;
   p->plugin_gui_cleanup = NULL;
   p->plugin_pre_sync_pre_connect = NULL;
   p->plugin_pre_sync = NULL;
   p->plugin_sync = NULL;
   p->plugin_post_sync = NULL;
   p->plugin_exit_cleanup = NULL;
   p->plugin_unpack_cai_from_ai = NULL;
   p->plugin_pack_cai_into_ai = NULL;

   h = dlopen(path, RTLD_LAZY);
   if (!h) {
      jp_logf(JP_LOG_WARN, _("Open failed on plugin [%s]\n error [%s]\n"), path,
		  dlerror());
      return EXIT_FAILURE;
   }
   jp_logf(JP_LOG_DEBUG, "opened plugin [%s]\n", path);
   p->handle=h;

   p->full_path = strdup(path);

   /* plugin_versionM */
#if defined __OpenBSD__ && !defined __ELF__
#define dlsym(x,y) dlsym(x, "_" y)
#endif
   plugin_versionM = dlsym(h, "plugin_version");
   if (plugin_versionM==NULL)  {
      err = dlerror();
      jp_logf(JP_LOG_WARN, "plugin_version: [%s]\n", err);
      jp_logf(JP_LOG_WARN, _(" plugin is invalid: [%s]\n"), path);
      dlclose(h);
      p->handle=NULL;
      return EXIT_FAILURE;
   }
   plugin_versionM(&major_version, &minor_version);
   version=major_version*1000+minor_version;
   if ((major_version <= 0) && (minor_version < 99)) {
      jp_logf(JP_LOG_WARN, _("Plugin:[%s]\n"), path);
      jp_logf(JP_LOG_WARN, _("This plugin is version (%d.%d).\n"),
		  major_version, minor_version);
      jp_logf(JP_LOG_WARN, _("It is too old to work with this version of J-Pilot.\n"));
      dlclose(h);
      p->handle=NULL;
      return EXIT_FAILURE;
   }
   jp_logf(JP_LOG_DEBUG, "This plugin is version (%d.%d).\n",
	       major_version, minor_version);

   /* plugin_get_name */
   jp_logf(JP_LOG_DEBUG, "getting plugin_get_name\n");
   p->plugin_get_name = dlsym(h, "plugin_get_name");
   if (p->plugin_get_name==NULL)  {
      err = dlerror();
      jp_logf(JP_LOG_WARN, "plugin_get_name: [%s]\n", err);
      jp_logf(JP_LOG_WARN, _(" plugin is invalid: [%s]\n"), path);
      dlclose(h);
      p->handle=NULL;
      return EXIT_FAILURE;
   }

   if (p->plugin_get_name) {
      p->plugin_get_name(name, 50);
      name[50]='\0';
      p->name = strdup(name);
   } else {
      p->name = NULL;
   }

   /* plugin_get_menu_name */
   jp_logf(JP_LOG_DEBUG, "getting plugin_get_menu_name\n");
   p->plugin_get_menu_name = dlsym(h, "plugin_get_menu_name");
   if (p->plugin_get_menu_name) {
      p->plugin_get_menu_name(name, 50);
      name[50]='\0';
      p->menu_name = strdup(name);
   } else {
      p->menu_name = NULL;
   }

   /* plugin_get_help_name */
   jp_logf(JP_LOG_DEBUG, "getting plugin_get_help_name\n");
   p->plugin_get_help_name = dlsym(h, "plugin_get_help_name");
   if (p->plugin_get_help_name) {
      p->plugin_get_help_name(name, 50);
      name[50]='\0';
      p->help_name = strdup(name);
   } else {
      p->help_name = NULL;
   }

   /* plugin_get_db_name */
   jp_logf(JP_LOG_DEBUG, "getting plugin_get_db_name\n");
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

   /* plugin_help */
   p->plugin_print = dlsym(h, "plugin_print");

   /* plugin_import */
   p->plugin_import = dlsym(h, "plugin_import");

   /* plugin_export */
   p->plugin_export = dlsym(h, "plugin_export");

   /* plugin_gui_cleanup */
   p->plugin_gui_cleanup = dlsym(h, "plugin_gui_cleanup");

   /* plugin_startup */
   p->plugin_startup = dlsym(h, "plugin_startup");

   /* plugin_pre_sync */
   p->plugin_pre_sync = dlsym(h, "plugin_pre_sync");

   /* plugin_pre_sync_pre_connect */
   p->plugin_pre_sync_pre_connect = dlsym(h, "plugin_pre_sync_pre_connect");

   /* plugin_sync */
   p->plugin_sync = dlsym(h, "plugin_sync");

   /* plugin_post_sync */
   p->plugin_post_sync = dlsym(h, "plugin_post_sync");

   /* plugin_search */
   p->plugin_search = dlsym(h, "plugin_search");

   /* plugin_exit_cleanup */
   p->plugin_exit_cleanup = dlsym(h, "plugin_exit_cleanup");

   p->plugin_unpack_cai_from_ai = dlsym(h, "plugin_unpack_cai_from_ai");
   p->plugin_pack_cai_into_ai = dlsym(h, "plugin_pack_cai_into_ai");

   return EXIT_SUCCESS;
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

   for (temp_list = *plugin_list; temp_list; temp_list = temp_list->next) {
      if (temp_list->data) {
	 p=temp_list->data;
	 if (p->full_path) free(p->full_path);
	 if (p->name)      free(p->name);
	 if (p->menu_name) free(p->menu_name);
	 if (p->help_name) free(p->help_name);
	 if (p->db_name)   free(p->db_name);

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

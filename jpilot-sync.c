/* $Id: jpilot-sync.c,v 1.34 2010/10/12 03:25:38 rikster5 Exp $ */

/*******************************************************************************
 * jpilot-sync.c
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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#ifdef HAVE_LOCALE_H
#  include <locale.h>
#endif

#include "i18n.h"
#include "utils.h"
#include "log.h"
#include "prefs.h"
#include "sync.h"
#include "plugins.h"
#include "otherconv.h"

/******************************* Global vars **********************************/
int pipe_to_parent, pipe_from_parent;
pid_t glob_child_pid;
unsigned char skip_plugins;

/* Start Hack */
/* FIXME: The following is a hack.  
 * The variables below are global variables in jpilot.c which are unused in
 * this code but must be instantiated for the code to compile.  
 * The same is true of the functions which are only used in GUI mode. */
pid_t jpilot_master_pid = -1;
int *glob_date_label;
GtkWidget *glob_dialog;
GtkTooltips *glob_tooltips;

void output_to_pane(const char *str) { return; }
void cb_app_button(GtkWidget *widget, gpointer data) { return; }
void cb_cancel_sync(GtkWidget *widget, unsigned int flags) { return; }
/* End Hack */

/****************************** Main Code *************************************/
static void fprint_jps_usage_string(FILE *out)
{
   fprintf(out, "%s-sync [ -v || -h || [-d] [-P] [-b] [-l] [-p port] ]\n", EPN);
   fprintf(out, _(" J-Pilot preferences are read to get sync info such as port, rate, number of backups, etc.\n"));
   fprintf(out, _(" -v display version and compile options\n"));
   fprintf(out, _(" -h display help text\n"));
   fprintf(out, _(" -d display debug info to stdout\n"));
   fprintf(out, _(" -P skip loading plugins\n"));
   fprintf(out, _(" -b sync, and then do a backup\n"));
   fprintf(out, _(" -l loop, otherwise sync once and exit\n"));
   fprintf(out, _(" -p {port} use this port to sync on instead of default\n"));
}

static void sig_handler(int sig)
{
#ifdef ENABLE_PLUGINS
   struct plugin_s *plugin;
   GList *plugin_list, *temp_list;
#endif

   jp_logf(JP_LOG_DEBUG, "caught signal %d\n", sig);

#ifdef ENABLE_PLUGINS
   plugin_list = get_plugin_list();

   for (temp_list = plugin_list; temp_list; temp_list = temp_list->next) {
      plugin = (struct plugin_s *)temp_list->data;
      if (plugin) {
         if (plugin->plugin_exit_cleanup) {
            jp_logf(JP_LOG_DEBUG, "calling plugin_exit_cleanup\n");
            plugin->plugin_exit_cleanup();
         }
      }
   }
#endif

   exit(0);
}

int main(int argc, char *argv[])
{
   int done;
   int cons_errors;
   int flags;
   int r, i;
   int loop;
   char port[MAX_PREF_LEN];
#ifdef ENABLE_PLUGINS
   struct plugin_s *plugin;
   GList *plugin_list, *temp_list;
   jp_startup_info info;
#endif

   /* enable internationalization(i18n) before printing any output */
#if defined(ENABLE_NLS)
#  ifdef HAVE_LOCALE_H
      char *current_locale;
      current_locale = setlocale(LC_ALL, "");
#  endif
   bindtextdomain(EPN, LOCALEDIR);
   textdomain(EPN);
#endif

   done=cons_errors=0;
   port[0]='\0';
   glob_child_pid=0;
   loop=0;
   skip_plugins=0;

   flags = SYNC_NO_FORK;

   /* Read preferences from jpilot.rc file */
   pref_init();
   pref_read_rc_file();
   if (otherconv_init()) {
      printf("Error: could not set encoding\n");
      exit(1);
   }

   pipe_from_parent=STDIN_FILENO;
   pipe_to_parent=STDOUT_FILENO;
   glob_log_stdout_mask = JP_LOG_INFO | JP_LOG_WARN | JP_LOG_FATAL |
                          JP_LOG_STDOUT | JP_LOG_GUI;

   /* Parse command line options */
   for (i=1; i<argc; i++) {
      if (!strncasecmp(argv[i], "-v", 2)) {
         char options[1024];
         get_compile_options(options, sizeof(options));
         printf("\n%s\n", options);
         exit(0);
      }
      if ( (!strncmp(argv[i], "-h", 2)) || 
           (!strncasecmp(argv[1], "-?", 2))) {
         fprint_jps_usage_string(stderr);
         exit(0);
      }
      if (!strncasecmp(argv[i], "-d", 2)) {
         glob_log_stdout_mask = 0xFFFF;
         glob_log_file_mask = 0xFFFF;
         jp_logf(JP_LOG_DEBUG, "Debug messages on.\n");
      }
      if (!strncmp(argv[i], "-b", 2)) {
         flags |= SYNC_FULL_BACKUP;
      }
      if (!strncmp(argv[i], "-l", 2)) {
         loop=1;
      }
      if (!strncmp(argv[i], "-P", 2)) {
         skip_plugins = 1;
         flags |= SYNC_NO_PLUGINS;
         jp_logf(JP_LOG_INFO, _("Not loading plugins.\n"));
      }
      if (!strncmp(argv[i], "-p", 2)) {
         i++;
         if (i<argc) {
            g_strlcpy(port, argv[i], MAX_PREF_LEN);
            /* preference is not saved, so this is not persistent */
            set_pref(PREF_PORT, 0, port, FALSE);
         }
      }
   }

#ifdef ENABLE_PLUGINS
   if (!skip_plugins) {
      load_plugins();
      plugin_list = get_plugin_list();
   } else {
      plugin_list = NULL;
   }

   for (temp_list = plugin_list; temp_list; temp_list = temp_list->next) {
      plugin = (struct plugin_s *)temp_list->data;
      jp_logf(JP_LOG_DEBUG, "plugin: [%s] was loaded\n", plugin->name);
      if (plugin) {
         if (plugin->plugin_startup) {
            info.base_dir = strdup(BASE_DIR);
            jp_logf(JP_LOG_DEBUG, "calling plugin_startup for [%s]\n", plugin->name);
            plugin->plugin_startup(&info);
            if (info.base_dir) {
               free(info.base_dir);
            }
         }
      }
   }
#endif

   /* After plugin startups are called we want to make sure that cleanups
    * will be called */
   signal(SIGHUP, sig_handler);
   signal(SIGINT, sig_handler);
   signal(SIGTERM, sig_handler);

   do {
      r = setup_sync(flags);
      switch (r) {
       case 0:
         /* sync successful */
         break;
       case SYNC_ERROR_BIND:
         printf("\n");
         printf(_("Error: connecting to port %s\n"), port);
         break;
       case SYNC_ERROR_LISTEN:
         printf("\n");
         printf(_("Error: pi_listen\n"));
         break;
       case SYNC_ERROR_OPEN_CONDUIT:
         printf("\n");
         printf(_("Error: opening conduit to handheld\n"));
         break;
       case SYNC_ERROR_PI_ACCEPT:
         printf("\n");
         printf(_("Error: pi_accept\n"));
         break;
       case SYNC_ERROR_NOT_SAME_USER:
         printf("\n");
         printf(_("Error: "));
         printf(_("This handheld does not have the same user name.\n"));
         printf(_("as the one that was synced the last time.\n"));

         printf(_("Syncing with different handhelds to the same directory can destroy data.\n"));
#ifdef ENABLE_PROMETHEON
         printf(_(" COPILOT_HOME"));
#else
         printf(_(" JPILOT_HOME"));
#endif
         printf(_(" environment variable can be used to sync different handhelds,\n"));
         printf(_(" to different directories for the same UNIX user name.\n"));
         break;
       case SYNC_ERROR_NOT_SAME_USERID:
         printf("\n");
         printf(_("This handheld does not have the same user ID.\n"));
         printf(_("as the one that was synced the last time.\n"));
         printf(_(" Syncing with different handhelds to the same directory can destroy data.\n"));
#ifdef ENABLE_PROMETHEON
         printf(_(" COPILOT_HOME"));
#else
         printf(_(" JPILOT_HOME"));
#endif
         printf(_(" environment variable can be used to sync different handhelds,\n"));
         printf(_(" to different directories for the same UNIX user name.\n"));
         break;
       case SYNC_ERROR_NULL_USERID:
         printf("\n");
         printf(_("Error: "));
         printf(_("This handheld has a NULL user ID.\n"));
         printf(_("Every handheld must have a unique user ID in order to sync properly.\n"));
         printf(_("If the handheld has been hard reset, \n"));
         printf(_("   use restore from within "EPN" to restore it.\n"));
         printf(_("Otherwise, to add a new user name and ID\n"));
         printf(_("   use \"install-user %s name numeric_id\"\n"), port);
         break;
       default:
         printf("\n");
         printf(_("Error: sync returned error %d\n"), r);
      }
      sleep(1);
   } while (loop);

   otherconv_free();

   return r;
}


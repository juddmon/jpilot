/* jpilot-sync.c
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
 */

#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include "utils.h"
#include "log.h"
#include "prefs.h"
#include "sync.h"
#include "plugins.h"
#include "i18n.h"

/* this is a hack for now until I clean up the code */
int *glob_date_label;
int pipe_to_parent, pipe_from_parent;
pid_t glob_child_pid;
GtkWidget *glob_dialog;
pid_t glob_child_pid;

unsigned char skip_plugins;

/* hack */
void cb_app_button(GtkWidget *widget, gpointer data)
{
   return;
}

void fprint_jps_usage_string(FILE *out)
{
   fprintf(out, "%s-sync [ -v || -h || [-d] [-P] [-b] [-l] [-p] port]\n", EPN);
   fprintf(out, "%s", _(" J-Pilot preferences are read to get port, rate, number of backups, etc.\n"));
   fprintf(out, "%s", _(" -v = version\n"));
   fprintf(out, "%s", _(" -h = help\n"));
   fprintf(out, "%s", _(" -d = run in debug mode\n"));
   fprintf(out, "%s", _(" -P = do not load plugins.\n"));
   fprintf(out, "%s", _(" -b = Do a sync and then a backup, otherwise just do a sync.\n"));
   fprintf(out, "%s", _(" -l = loop, otherwise sync once and exit.\n"));
   fprintf(out, "%s", _(" -p {port} = Use this port to sync with instead of getting preferences.\n"));
}

static void sig_handler(int sig);

int main(int argc, char *argv[])
{
   int done;
   int cons_errors;
   int flags;
   int r, i;
   int loop;
   char port[MAX_PREF_VALUE];
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

   pref_init();
   pref_read_rc_file();
#ifdef ENABLE_GTK2
   if (otherconv_init()) {
      printf("Error: could not set encoding\n");
      return 1;
   }
#endif

   pipe_from_parent=STDIN_FILENO;
   pipe_to_parent=STDOUT_FILENO;
   glob_log_stdout_mask = JP_LOG_INFO | JP_LOG_WARN | JP_LOG_FATAL |
     JP_LOG_STDOUT | JP_LOG_GUI;

   for (i=1; i<argc; i++) {
      if (!strncasecmp(argv[i], "-v", 2)) {
	 char options[1024];
	 get_compile_options(options, sizeof(options));
	 printf("\n%s\n", options);
	 exit(0);
      }
      if ( (!strncmp(argv[i], "-h", 2)) || (!strncasecmp(argv[1], "-?", 2))
	  ) {
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
	 jp_logf(JP_LOG_INFO, "Not loading plugins.\n");
      }
      if (!strncmp(argv[i], "-p", 2)) {
	 i++;
	 if (i<argc) {
	    strncpy(port, argv[i], MAX_PREF_VALUE);
	    port[MAX_PREF_VALUE-1]='\0';
	    /* Prefs are not saved, so this is not persistent */
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
   }

   for (temp_list = plugin_list; temp_list; temp_list = temp_list->next) {
      plugin = (struct plugin_s *)temp_list->data;
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

   /* After plugin startups are called we want to make sure cleanups are
      called */
   signal(SIGHUP, sig_handler);
   signal(SIGINT, sig_handler);
   signal(SIGTERM, sig_handler);

   do {
      r = setup_sync(flags);
      switch (r) {
       case 0:
	 break;
       case SYNC_ERROR_BIND:
	 printf("\n");
	 printf("Error: connecting to serial port\n");
	 break;
       case SYNC_ERROR_LISTEN:
	 printf("\n");
	 printf("Error: pi_listen\n");
	 break;
       case SYNC_ERROR_OPEN_CONDUIT:
	 printf("\n");
	 printf("Error: opening conduit to Palm\n");
	 break;
       case SYNC_ERROR_PI_ACCEPT:
	 printf("\n");
	 printf("Error: pi_accept\n");
	 break;
       case SYNC_ERROR_NOT_SAME_USER:
	 printf("\n");
	 printf("Error: this palm has a different User Name than the last sync.\n");
	 printf(" Syncing with different palms into the same directory could cross data.\n");
#ifdef ENABLE_PROMETHEON
	 printf(" COPILOT_HOME can be used to sync different palms under the same unix user.\n");
#else
	 printf(" JPILOT_HOME can be used to sync different palms under the same unix user.\n");
#endif
	 break;
       case SYNC_ERROR_NOT_SAME_USERID:
	 printf("\n");
	 printf("Error: this palm has a different ID Name than the last sync.\n");
	 printf(" Syncing with different palms into the same directory could cross data.\n");
#ifdef ENABLE_PROMETHEON
	 printf(" COPILOT_HOME can be used to sync different palms under the same unix user.\n");
#else
	 printf(" JPILOT_HOME can be used to sync different palms under the same unix user.\n");
#endif
	 break;
       case SYNC_ERROR_NULL_USERID:
	 printf("\n");
	 printf("Error: this palm has a NULL user ID.\n");
	 printf(" use \"install-user /dev/pilot name numeric_id\"\n");
	 break;
       default:
	 printf("\n");
	 printf("Error: sync returned error %d\n", r);
      }
      sleep(1);
   } while(loop);

#ifdef ENABLE_GTK2
   otherconv_free();
#endif

   return 0;
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

/* jpilot-sync.c
 * 
 * Copyright (C) 1999-2000 by Judd Montgomery
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
#include <stdlib.h>
#include <string.h>
#include "utils.h"
#include "log.h"
#include "prefs.h"
#include "sync.h"

/* this is a hack for now until I clean up the code */
int *glob_date_label;
int pipe_in, pipe_out;
pid_t glob_child_pid;
GtkWidget *glob_dialog;
pid_t glob_child_pid;

/* hack */
void cb_app_button(GtkWidget *widget, gpointer data)
{
   return;
}

#define USAGE_STRING "\njpilot-sync [ -v || -h || [-b] [-l] [-p]]\n"\
" J-Pilot preferences are read to get port, rate, number of backups, etc.\n"\
" -v = version\n"\
" -h = help\n"\
" -b = Do a sync and then a backup, otherwise just do a sync.\n"\
" -l = loop, otherwise sync once and exit.\n"\
" -p {port} = Use this port to sync with instead of getting preferences.\n"\

int main(int argc, char *argv[])
{
   int done;
   int cons_errors;
   int flags;
   int r, i;
   int loop;
   char port[MAX_PREF_VALUE];
   
   done=cons_errors=0;
   port[0]='\0';
   glob_child_pid=0;
   loop=0;

   flags = SYNC_NO_FORK;

   read_rc_file();
   
   for (i=1; i<argc; i++) {
      if (!strncasecmp(argv[i], "-v", 2)) {
	 printf("%s\n", VERSION_STRING);
	 exit(0);
      }
      if ( (!strncmp(argv[i], "-h", 2)) || (!strncasecmp(argv[1], "-?", 2))
	  ) {
	 printf("%s\n", USAGE_STRING);
	 exit(0);
      }
      if (!strncmp(argv[i], "-b", 2)) {
	 flags |= SYNC_FULL_BACKUP;
      }
      if (!strncmp(argv[i], "-l", 2)) {
	 loop=1;
      }
      if (!strncmp(argv[i], "-p", 2)) {
	 i++;
	 if (i<argc) {
	    strncpy(port, argv[i], MAX_PREF_VALUE);
	    port[MAX_PREF_VALUE-1]='\0';
	    /* Prefs are not saved, so this is not persistent */
	    set_pref_char(PREF_PORT, port);
	 }
      }

   }
   
   do {
      r = util_sync(flags);
      switch (r) {
       case 0:
	 break;
       case SYNC_ERROR_BIND:
	 printf("Error: connecting to serial port\n");
	 break;
       case SYNC_ERROR_LISTEN:
	 printf("Error: pi_listen\n");
	 break;
       case SYNC_ERROR_OPEN_CONDUIT:
	 printf("Error: opening conduit to Palm\n");
	 break;
       case SYNC_ERROR_PI_ACCEPT:
	 printf("Error: pi_accept\n");
	 break;
       case SYNC_ERROR_NOT_SAME_USER:
	 printf("Error: this palm has a different User Name than the last sync.\n");
	 printf(" Syncing with different palms into the same directory could cross data.\n");
	 printf(" Use JPILOT_HOME to sync different palms under the same unix user.\n");
	 break;
       case SYNC_ERROR_NOT_SAME_USERID:
	 printf("Error: this palm has a different ID Name than the last sync.\n");
	 printf(" Syncing with different palms into the same directory could cross data.\n");
	 printf(" Use JPILOT_HOME to sync different palms under the same unix user.\n");
	 break;
       case SYNC_ERROR_NULL_USERID:
	 printf("Error: this palm has a NULL user ID.\n");
	 printf(" use \"install-user /dev/pilot name numeric_id\"\n");
	 break;
       default:
	 printf("sync returned error %d\n", r);
      }
      sleep(1);
   } while(loop);

   return 0;
}

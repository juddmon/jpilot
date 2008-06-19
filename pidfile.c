/* pidfile.c
 * A module of J-Pilot http://jpilot.org
 *
 * Copyright (C) 2005 by Jason Day
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

/********************************* Includes ***********************************/
#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#include "i18n.h"
#include "log.h"
#include "utils.h"

/********************************* Constants **********************************/
#define JPILOT_PIDFILE  "jpilot.pid"

#ifndef O_SYNC
#  define O_SYNC  0       /* use it if we have it */
#endif

/******************************* Global vars **********************************/
static char pidfile[FILENAME_MAX];

/****************************** Main Code *************************************/
void setup_pidfile(void)
{
   memset(pidfile, 0, FILENAME_MAX);
   get_home_file_name(JPILOT_PIDFILE, pidfile, FILENAME_MAX);
}

pid_t check_for_jpilot(void)
{
   FILE *pidfp;
   int pid;

   pid = 0;
   if ((pidfp = fopen(pidfile, "r")) != NULL) {
      fscanf(pidfp, "%d", &pid);

      if (kill (pid, 0) == -1) {
         jp_logf(JP_LOG_WARN, _("removing stale pidfile\n"));
         pid = 0;
         unlink(pidfile);
      }

      fclose(pidfp);
   }

   return pid;
}

void write_pid(void)
{
   char tmp[20];
   int fd;

   jp_logf(JP_LOG_DEBUG, "pidfile: %s\n", pidfile);
   if ((fd = open(pidfile, O_WRONLY|O_CREAT|O_EXCL|O_SYNC, S_IRUSR|S_IWUSR)) != -1)
   {
      g_snprintf(tmp, sizeof(tmp), "%d\n", getpid());
      write(fd, tmp, strlen (tmp));
      close(fd);
   }
   else {
      jp_logf(JP_LOG_FATAL,_("create pidfile failed: %s\n"), strerror(errno));
      jp_logf(JP_LOG_WARN, _("Warning: hotplug syncing disabled.\n"));
   }
}

void cleanup_pidfile(void)
{
   if (getpid() == check_for_jpilot())
      unlink(pidfile);
}


/* synctime.c
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <pi-dlp.h>
#include "libplugin.h"


int plugin_get_name(char *name, int len)
{
   strncpy(name, "SyncTime 0.96", len);
   return 0;
}

int plugin_get_help_name(char *name, int len)
{
   strncpy(name, "About SyncTime", len);
   return 0;
}

int plugin_help(char **text, int *width, int *height)
{
   /* We could also pass back *text=NULL */
   *text = strdup(
	   /*-------------------------------------------*/
	   "SyncTime plugin for J-Pilot was written by\r\n"
	   "Judd Montgomery (c) 1999.\r\n"
	   "judd@engineer.com\r\n"
	   "http://jpilot.linuxbox.com\r\n"
	   "SyncTime WILL NOT work with PalmOS 3.3!\r\n"
	   );
   *height = 200;
   *width = 300;
}

int plugin_sync(int sd)
{
   int r;
   time_t ltime;

   jp_init();
   
   jp_logf(LOG_WARN, "synctime: setting the time on pilot\n");
   
   time(&ltime);
   r = dlp_SetSysDateTime(sd, ltime);
   
   return 0;
}

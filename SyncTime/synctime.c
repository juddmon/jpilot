/* $Id: synctime.c,v 1.10 2005/11/28 07:22:23 rikster5 Exp $ */

/*******************************************************************************
 * synctime.c
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
 ******************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <pi-dlp.h>
#include <pi-source.h>
#include "libplugin.h"

#include "../i18n.h"

void plugin_version(int *major_version, int *minor_version)
{
   *major_version=0;
   *minor_version=99;
}

int plugin_get_name(char *name, int len)
{
   strncpy(name, "SyncTime 0.99", len);
   return 0;
}

int plugin_get_help_name(char *name, int len)
{
   g_snprintf(name, len, _("About %s"), _("SyncTime"));
   return 0;
}

int plugin_help(char **text, int *width, int *height)
{
   /* We could also pass back *text=NULL */
   *text = strdup(
	   /*-------------------------------------------*/
	   "SyncTime plugin for J-Pilot was written by\n"
	   "Judd Montgomery (c) 1999.\n"
	   "judd@jpilot.org\n"
	   "http://jpilot.org\n"
	   "\n"
	   "SyncTime WILL NOT work with PalmOS 3.3!"
	   );
   *height = 0;
   *width = 0;
   return 0;
}

int plugin_sync(int sd)
{
   int r;
   time_t ltime;
   unsigned long ROMversion, majorVersion, minorVersion;

   jp_init();
   
   jp_logf(JP_LOG_DEBUG, "SyncTime: plugin_sync\n");

   dlp_ReadFeature(sd, makelong("psys"), 1, &ROMversion);
   
   majorVersion = (((ROMversion >> 28) & 0xf) * 10)+ ((ROMversion >> 24) & 0xf);
   minorVersion = (((ROMversion >> 20) & 0xf) * 10)+ ((ROMversion >> 16) & 0xf);

   jp_logf(JP_LOG_GUI, "synctime: Palm OS version %d.%d\n", majorVersion, minorVersion);

   if (majorVersion==3) {
      if ((minorVersion==30) || (minorVersion==25)) {
	 jp_logf(JP_LOG_GUI, "synctime: Palm OS Version 3.25 and 3.30 do not support SyncTime\n");
	 jp_logf(JP_LOG_GUI, "synctime: NOT setting the time on the pilot\n");
	 return 1;
      }
   }

   jp_logf(JP_LOG_GUI, "synctime: Setting the time on the pilot... ");
   
   time(&ltime);
   r = dlp_SetSysDateTime(sd, ltime);
   
   jp_logf(JP_LOG_GUI, "Done\n");

   return 0;
}

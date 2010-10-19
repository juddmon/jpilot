/* $Id: synctime.c,v 1.19 2010/10/19 00:00:09 rikster5 Exp $ */

/*******************************************************************************
 * synctime.c
 *
 * This is a plugin for J-Pilot which sets the time on the handheld
 * to the current time of the desktop.
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

/********************************* Includes ***********************************/
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <pi-dlp.h>
#include <pi-source.h>

#include "libplugin.h"
#include "i18n.h"

/********************************* Constants **********************************/
#define PLUGIN_MAJOR 1
#define PLUGIN_MINOR 0

/****************************** Main Code *************************************/
void plugin_version(int *major_version, int *minor_version)
{
   *major_version = PLUGIN_MAJOR;
   *minor_version = PLUGIN_MINOR;
}

static int static_plugin_get_name(char *name, int len)
{
   snprintf(name, len, "SyncTime %d.%d", PLUGIN_MAJOR, PLUGIN_MINOR);
   return EXIT_SUCCESS;
}

int plugin_get_name(char *name, int len)
{
   return static_plugin_get_name(name, len);
}

int plugin_get_help_name(char *name, int len)
{
   g_snprintf(name, len, _("About %s"), _("SyncTime"));
   return EXIT_SUCCESS;
}

int plugin_help(char **text, int *width, int *height)
{
   char plugin_name[200];

   static_plugin_get_name(plugin_name, sizeof(plugin_name));
   *text = g_strdup_printf(
      /*-------------------------------------------*/
      _("%s\n"
        "\n"
        "SyncTime plugin for J-Pilot was written by\n"
        "Judd Montgomery (c) 1999.\n"
        "judd@jpilot.org, http://jpilot.org\n"
        "\n"
        "SyncTime WILL NOT work with PalmOS 3.3!"
        ),
        plugin_name
      );
   *height = 0;
   *width = 0;

   return EXIT_SUCCESS;
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
         jp_logf(JP_LOG_GUI, _("synctime: Palm OS Version 3.25 and 3.30 do not support SyncTime\n"));
         jp_logf(JP_LOG_GUI, _("synctime: NOT setting the time on the pilot\n"));
         return EXIT_FAILURE;
      }
   }

   jp_logf(JP_LOG_GUI, _("synctime: Setting the time on the pilot... "));
   
   time(&ltime);
   r = dlp_SetSysDateTime(sd, ltime);
   
   jp_logf(JP_LOG_GUI, _("Done\n"));

   return EXIT_SUCCESS;
}

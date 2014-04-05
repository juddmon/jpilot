/*******************************************************************************
 * log.c
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
/*
 * Thanks to Jason Day for his patches that allowed plugins to log correctly
 */
/********************************* Includes ***********************************/
#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef USE_FLOCK
#  include <sys/file.h>
#else
#  include <fcntl.h>
#endif
#include <signal.h>
#include <utime.h>

#include "i18n.h"
#include "log.h"
#include "utils.h"
#include "sync.h"
#include "prefs.h"

/********************************* Constants **********************************/
#define WRITE_MAX_BUF 4096

/******************************* Global vars **********************************/
int pipe_to_parent;

int glob_log_file_mask;
int glob_log_stdout_mask;
int glob_log_gui_mask;

extern void output_to_pane(const char *str);
extern pid_t jpilot_master_pid;

/****************************** Prototypes ************************************/
static int jp_vlogf (int level, const char *format, va_list val);

/****************************** Main Code *************************************/
int jp_logf(int level, const char *format, ...)
{
   va_list val;
   int rval;

   if (!((level & glob_log_file_mask) ||
       (level & glob_log_stdout_mask) ||
       (level & glob_log_gui_mask))) {
      return EXIT_SUCCESS;
   }

   va_start(val, format);
   rval = jp_vlogf(level, format, val);
   va_end(val);
   return rval;
}

static int jp_vlogf (int level, const char *format, va_list val) {
   char                 real_buf[WRITE_MAX_BUF+32];
   char                 *buf, *local_buf;
   int                  size;
   int                  len;
   int                  r;
   static FILE          *fp=NULL;
   static int           err_count=0;
   char                 cmd[16];

   if (!((level & glob_log_file_mask) ||
         (level & glob_log_stdout_mask) ||
         (level & glob_log_gui_mask))) {
      return EXIT_SUCCESS;
   }

   if ((!fp) && (err_count>10)) {
      return EXIT_FAILURE;
   }
   if ((!fp) && (err_count==10)) {
      fprintf(stderr, _("Unable to open log file, giving up.\n"));
      err_count++;
      return EXIT_FAILURE;
   }
   if ((!fp) && (err_count<10)) {
      char fullname[FILENAME_MAX];
      get_home_file_name(EPN".log", fullname, sizeof(fullname));

      fp = fopen(fullname, "w");
      if (!fp) {
         fprintf(stderr, _("Unable to open log file\n"));
         err_count++;
      }
   }

   buf=&(real_buf[16]);
   buf[0] = '\0';

   size = g_vsnprintf(buf, WRITE_MAX_BUF, format, val);
   /* glibc >2.1 can return size > WRITE_MAX_BUF */
   /* just in case g_vsnprintf reached the max */
   buf[WRITE_MAX_BUF-1] = '\0';
   size=strlen(buf);

   local_buf = buf;
   /* UTF-8 text so transform in local encoding */
   if (g_utf8_validate(buf, -1, NULL))
   {
      local_buf = g_locale_from_utf8(buf, -1, NULL, NULL, NULL);
      if (NULL == local_buf)
         local_buf = buf;
   }

   if ((fp) && (level & glob_log_file_mask)) {
      fwrite(local_buf, size, 1, fp);
      fflush(fp);
   }

   if (level & glob_log_stdout_mask) {
      fputs(local_buf, stdout);
   }

   /* free the buffer is a conversion was used */
   if (local_buf != buf)
      g_free(local_buf);

   if ((pipe_to_parent) && (level & glob_log_gui_mask)) {
      /* do not use a pipe for intra-process log
       * otherwise we may have a dead lock (jpilot freezes) */
      if (getpid() == jpilot_master_pid)
         output_to_pane(buf);
      else {
         sprintf(cmd, "%d:", PIPE_PRINT);
         len = strlen(cmd);
         buf = buf-len;
         strncpy(buf, cmd, len);
         size += len;
         buf[size]='\0';
         buf[size+1]='\n';
         size += 2;
         r = write(pipe_to_parent, buf, size);
         if (r<0)
            fprintf(stderr, "write returned error %s %d\n", __FILE__, __LINE__);
      }
   }

   return EXIT_SUCCESS;
}

/*
 * This function writes data to the parent process.
 * A line feed, or a null must be the last character written.
 */
int write_to_parent(int command, const char *format, ...)
{
   va_list val;
   int len, size;
   char real_buf[WRITE_MAX_BUF+32];
   char *buf;
   char cmd[20];

   buf=&(real_buf[16]);
   buf[0] = '\0';

   va_start(val, format);
   g_vsnprintf(buf, WRITE_MAX_BUF, format, val);
   /* glibc >2.1 can return size > WRITE_MAX_BUF */
   /* just in case g_vsnprintf reached the max */
   buf[WRITE_MAX_BUF-1] = '\0';
   size=strlen(buf);
   va_end(val);

   /* This is for jpilot-sync */
   if (pipe_to_parent==STDOUT_FILENO) {
      if (command==PIPE_PRINT) {
         write(pipe_to_parent, buf, strlen(buf));
      }
      return TRUE;
   }

   sprintf(cmd, "%d:", command);
   len = strlen(cmd);
   buf = buf-len;
   strncpy(buf, cmd, len);
   size += len;
   /* The pipe doesn't flush unless a CR is written */
   /* This is our key to the parent for a record separator */
   buf[size]='\0';
   buf[size+1]='\n';
   size += 2;
   write(pipe_to_parent, buf, size);

   return TRUE;
}

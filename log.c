/*
 * log.c
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
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <signal.h>
#include <utime.h>
#include "log.h"
#include "utils.h"
#include "sync.h"


extern int pipe_in, pipe_out;


int glob_log_file_mask;
int glob_log_stdout_mask;
int glob_log_gui_mask;

int logf(int level, char *format, ...)
{
#define WRITE_MAX_BUF	4096
   va_list	       	val;
   char			buf[WRITE_MAX_BUF];
   int			size;
   static FILE		*fp=NULL;
   static int		err_count=0;

   buf[0] = '\0';

   if ((!fp) && (err_count>10)) {
      return;
   }
   if ((!fp) && (err_count==10)) {
      fprintf(stderr, "Cannot open log file, giving up.\n");
      err_count++;
      return;
   }
   if ((!fp) && (err_count<10)) {
      fp = open_file("jpilot.log", "w");
      if (!fp) {
	 fprintf(stderr, "Cannot open log file\n");
	 err_count++;
      }
   }

   va_start(val, format);
   size = vsnprintf(buf, WRITE_MAX_BUF ,format, val);
   //just in case vsnprintf reached the max
   if (size == -1) {
      buf[WRITE_MAX_BUF-1] = '\0';
      size=WRITE_MAX_BUF-1;
   }
   va_end(val);

   if ((fp) && (level & glob_log_file_mask)) {
      fwrite(buf, size, 1, fp);
   }

   if (level & glob_log_stdout_mask) {
      fputs(buf, stdout);
   }
   
   if ((pipe_out) && (level & glob_log_gui_mask)) {
      write(pipe_out, buf, size);
   }
   
   return 0;
}


/* log.c
 * A module of J-Pilot http://jpilot.org
 * 
 * Copyright (C) 1999-2001 by Judd Montgomery
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

/*
 * Thanks to Jason Day for his patches that allowed plugins to log correctly
 */

#include "config.h"
#include "i18n.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef USE_FLOCK
#include <sys/file.h>
#else
#include <fcntl.h>
#endif
#include <signal.h>
#include <utime.h>
#include "log.h"
#include "utils.h"
#include "sync.h"


extern int pipe_in, pipe_out;


int glob_log_file_mask;
int glob_log_stdout_mask;
int glob_log_gui_mask;


int jpilot_logf(int level, char *format, ...)
{
   va_list	       	val;
   int rval;
   
   if (!((level & glob_log_file_mask) ||
       (level & glob_log_stdout_mask) ||
       (level & glob_log_gui_mask))) {
      return 0;
   }
   
   va_start(val, format);
   rval = jpilot_vlogf(level, format, val);
   va_end(val);
   return rval;
}

int jpilot_vlogf (int level, char *format, va_list val) {
#define WRITE_MAX_BUF	4096
    char       		buf[WRITE_MAX_BUF];
    int			size;
    static FILE		*fp=NULL;
    static int		err_count=0;


    if (!((level & glob_log_file_mask) ||
          (level & glob_log_stdout_mask) ||
          (level & glob_log_gui_mask))) {
        return 0;
    }

    buf[0] = '\0';

    if ((!fp) && (err_count>10)) {
        return -1;
    }
    if ((!fp) && (err_count==10)) {
        fprintf(stderr, _("Cannot open log file, giving up.\n"));
        err_count++;
        return -1;
    }
    if ((!fp) && (err_count<10)) {
        fp = jp_open_home_file("jpilot.log", "w");
        if (!fp) {
            fprintf(stderr, _("Cannot open log file\n"));
            err_count++;
        }
    }

    size = g_vsnprintf(buf, WRITE_MAX_BUF ,format, val);
    /*just in case g_vsnprintf reached the max */
    if (size == -1) {
        buf[WRITE_MAX_BUF-1] = '\0';
        size=WRITE_MAX_BUF-1;
    }

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

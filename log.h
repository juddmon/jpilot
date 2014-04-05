/*******************************************************************************
 * log.h
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

#ifndef __LOG_H__
#define __LOG_H__

#include <stdarg.h>

#define JP_LOG_DEBUG  1    /*debugging info for programmers, and bug reports */
#define JP_LOG_INFO   2    /*info, and misc messages */
#define JP_LOG_WARN   4    /*worse messages */
#define JP_LOG_FATAL  8    /*even worse messages */
#define JP_LOG_STDOUT 256  /*messages always go to stdout */
#define JP_LOG_FILE   512  /*messages always go to the log file */
#define JP_LOG_GUI    1024 /*messages always go to the gui window */

/* pipe communication commands */
#define PIPE_PRINT           100
#define PIPE_USERID          101
#define PIPE_USERNAME        102
#define PIPE_PASSWORD        103
#define PIPE_WAITING_ON_USER 104
#define PIPE_FINISHED        105
#define PIPE_SYNC_CONTINUE   200
#define PIPE_SYNC_CANCEL     201

extern int glob_log_file_mask;
extern int glob_log_stdout_mask;
extern int glob_log_gui_mask;

int jp_logf(int log_level, const char *format, ...);
int write_to_parent(int command, const char *format, ...);

#endif

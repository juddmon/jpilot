/*
 * jpilot-syncd.c
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
#include "sync.h"

//todo - this is a hack for now until I clean up the code
int *glob_date_label;
int pipe_in, pipe_out;
extern pid_t glob_child_pid;

#define VERSION_STRING "\nJPilot version 0.92\n"\
" Copyright (C) 1999 by Judd Montgomery\n"

#define USAGE_STRING "\njpilot-syncd [ [-v] || [-h] || [port(/dev/??)] ]\n"\
" If you don't specify a port then it is read from the PILOTPORT env variable.\n"\
" If its not found there then it defaults to /dev/pilot.\n"

main(int argc, char *argv[])
{
   int done, r;
   int cons_errors;
   char *port;
   
   done=cons_errors=0;
   port=NULL;
   
   if (argc > 1) {
      if (!strncasecmp(argv[1], "-v", 2)) {
	 printf("%s\n", VERSION_STRING);
	 exit(0);
      }
   }

   if (argc > 1) {
      if (!strncasecmp(argv[1], "-h", 2)) {
	 printf("%s\n", USAGE_STRING);
	 exit(0);
      }
   }

   if (argc>1) {
      port=argv[1];
   }
   
   while(!done) {
      r = jpilot_sync(port);
      if (r) {
	 cons_errors++;
	 if (cons_errors>10) {
	    printf("Too many consecutive errors.  Quiting\n");
	    exit(1);
	 }
      } else {
	 cons_errors=0;
      }
   }
}

/* $Id: clist_mini_icons.h,v 1.6 2011/04/06 12:43:45 rousseau Exp $ */

/*******************************************************************************
 * clist_mini_icons.h
 * A module of J-Pilot http://jpilot.org
 *
 * Copyright (C) 1999-2010 by Judd Montgomery
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

/* Note pixmap */
const char * xpm_note[] = {
"12 16 3 1",
" 	c None",
".	c #000000",
"+	c #CCCCCC",
"            ",
" ......     ",
" .+++.+.    ",
" .+++.++.   ",
" .+++.+++.  ",
" .+++.....  ",
" .+++++++.  ",
" .+++++++.  ",
" .+++++++.  ",
" .+++++++.  ",
" .+++++++.  ",
" .+++++++.  ",
" .+++++++.  ",
" .........  ",
"            ",
"            "};

/* Alarm pixmap */
const char * xpm_alarm[] = {
"16 16 4 1",
"# c None",
". c None",
"a c #000000",
"b c #cccccc",
"................",
".#aaa.aaa.aaa#..",
".aaaaabbbaaaaa..",
".aaabbbbbbbaaa..",
".aabbbbbbbbbaa..",
"..abbbbabbbba...",
".abbbbbabbbbba..",
".abaaaaabbbbba..",
".abbbbbbbbbbba..",
"..abbbbbbbbba...",
"..abbbbbbbbba...",
"...abbbbbbba....",
"....aaaaaaa.....",
"....aaa.aaa.....",
"................",
"................"};

/* Old alarm pixmap: < 8/28/2009 */
/*
char * xpm_alarm[] = {
   "16 16 3 1",
   "       c None",
   ".      c #000000000000",
   "X      c #cccccccccccc",
   "                ",
   "   .       .    ",
   "  ...     ...   ",
   "  ...........   ",
   "   .XXXXXXXX.   ",
   "  .XXXX.XXXXX.  ",
   " .XXXXX.XXXXXX. ",
   " .X.....XXXXXX. ",
   " .XXXXXXXXXXX.  ",
   "  .XXXXXXXXX.   ",
   "   .XXXXXXX.    ",
   "   .........    ",
   "   .       .    ",
   " ....     ....  ",
   "                ",
   "                "
};
*/

/* ToDo checkbox, blank */
const char * xpm_check[] = {
   "12 16 3 1",
   "       c None",
   ".      c #000000000000",
   "X      c #cccccccccccc",
   "                ",
   " .........  ",
   " .XXXXXXX.  ",
   " .X     X.  ",
   " .X     X.  ",
   " .X     X.  ",
   " .X     X.  ",
   " .X     X.  ",
   " .X     X.  ",
   " .X     X.  ",
   " .X     X.  ",
   " .X     X.  ",
   " .XXXXXXX.  ",
   " .........  ",
   "            ",
   "            "
};

/* ToDo checkbox, checked */
const char * xpm_checked[] = {
   "12 16 4 1",
   "       c None",
   ".      c #000000000000",
   "X      c #cccccccccccc",
   "R      c #FFFF00000000",
   "            ",
   " .........  ",
   " .XXXXXXX.RR",
   " .X     XRR ",
   " .X     RR  ",
   " .X    RR.  ",
   " .X    RR.  ",
   " .X   RRX.  ",
   " RR  RR X.  ",
   " .RR RR X.  ",
   " .X RR  X.  ",
   " .X  R  X.  ",
   " .XXXXXXX.  ",
   " .........  ",
   "            ",
   "            "
};

const char * xpm_float_check[] = {
   "14 16 4 1",
   "       c None",
   ".      c #000000000000",
   "X      c #CCCCCCCCCCCC",
   "W      c #FFFFFFFFFFFF",
   "              ",
   "     ....     ",
   "    ......    ",
   "   ..XXXX..   ",
   "  ..XWWWWX..  ",
   " ..XWWWWWWX.. ",
   " ..XWWWWWWX.. ",
   " ..XWWWWWWX.. ",
   " ..XWWWWWWX.. ",
   " ..XWWWWWWX.. ",
   "  ..XWWWWX..  ",
   "   ..XXXX..   ",
   "    ......    ",
   "     ....     ",
   "              ",
   "              "
};

const char * xpm_float_checked[] = {
   "14 16 5 1",
   "       c None",
   ".      c #000000000000",
   "X      c #cccccccccccc",
   "R      c #FFFF00000000",
   "W      c #FFFFFFFFFFFF",
   "              ",
   "     ....     ",
   "    ...... RR ",
   "   ..XXXX.RR  ",
   "  ..XWWWWRR.  ",
   " ..XWWWWRRX.. ",
   " ..XWWWWRRX.. ",
   " ..XWWWRRWX.. ",
   " .RRWWRRWWX.. ",
   " ..RRWRRWWX.. ",
   "  ..XRRWWX..  ",
   "   ..XRXX..   ",
   "    ......    ",
   "     ....     ",
   "              ",
   "              "
};

/* SDcard pixmap */
const char * xpm_sdcard[] = {
"12 16 3 1",
" 	c None",
".	c #000000",
"+	c #CCCCCC",
"            ",
" ......     ",
" .+++++.    ",
" .++++++.   ",
" .+++++++.  ",
" .+++++++.  ",
" .+++++++.  ",
" .+++++++.  ",
" .+++++++.  ",
" .+++++++.  ",
" .+.+.+.+.  ",
" .+.+.+.+.  ",
" .+.+.+.+.  ",
" .........  ",
"            ",
"            "};



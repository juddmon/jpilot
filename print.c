/* print.c
 *
 * Copyright (C) 2000 by Judd Montgomery
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

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "datebook.h"
#include "address.h"
#include "todo.h"
#include "sync.h"
#include "prefs.h"
#include "print.h"
#include "log.h"

static FILE *out;
int fill_in();

#ifdef JPILOT_PRINTABLE
#define FLAG_CHAR 'A'
#define Q_FLAG_CHAR "A"
#else
#define FLAG_CHAR 010
#define Q_FLAG_CHAR "\\010"
#endif

FILE *print_open()
{
   long ivalue;
   const char *command;
   
   get_pref(PREF_PRINT_COMMAND, &ivalue, &command);
   if (command) {
      return popen(command, "w");
   } else {
      return NULL;
   }
}

void print_close(FILE *f)
{
   pclose(f);
}

int courier_12()
{
   /* fprintf(out, "/Courier 12 selectfont\n"); */
   fprintf(out, "%cC12\n", FLAG_CHAR);
   return 0;
}

int courier_bold_12()
{
   /* fprintf(out, "/Courier-Bold 12 selectfont\n"); */
   fprintf(out, "%cCB12\n", FLAG_CHAR);
   return 0;
}

int gsave()
{
   fprintf(out, "gsave\n");
   return 0;
}

int grestore()
{
   fprintf(out, "grestore\n");
   return 0;
}

int rotate(float deg)
{
   fprintf(out, "%f rotate\n", deg);
   return 0;
}

int box(float x1, float y1, float x2, float y2)
{
   fprintf(out, "newpath\n");
   fprintf(out, "%g inch %g inch moveto\n", x1, y1);
   fprintf(out, "%g inch %g inch lineto\n", x1, y2);
   fprintf(out, "%g inch %g inch lineto\n", x2, y2);
   fprintf(out, "%g inch %g inch lineto\n", x2, y1);
   fprintf(out, "closepath\n");
   return 0;
}

int clip_to_box(float x1, float y1, float x2, float y2)
{
   box(x1, y1, x2, y2);
   fprintf(out, "clip\n");
   return 0;
}

int puttext(float x, float y, char *text)
{
   fprintf(out, "%g inch %g inch moveto\n", x, y);
   fprintf(out, "(%s) show\n", text);
   return 0;
}

int setfont(char *name, int points)
{
   fprintf(out, "/%s findfont\n", name);
   fprintf(out, "%d scalefont\n", points);
   fprintf(out, "setfont\n");
   return 0;
}

int linefromto(float x1, float y1, float x2, float y2)
{
   fprintf(out, "%g inch %g inch moveto\n", x1, y1);
   fprintf(out, "%g inch %g inch lineto\n", x2, y2);
   return 0;
}

int setgray(float n)
{
   fprintf(out, "%g setgray\n", n);
   return 0;
}

int fill()
{
   fprintf(out, "fill\n");
   return 0;
}

int stroke()
{
   fprintf(out, "stroke\n");
   return 0;
}

int showpage()
{
   fprintf(out, "showpage\n");
   return 0;
}

int header()
{
   fprintf(out, "%%!postscript\n"
	   "/inch {72 mul} def\n");
   return 0;
}

int newpath()
{
   fprintf(out, "newpath\n");
   return 0;
}

int closepath()
{
   fprintf(out, "closepath\n");
   return 0;
}

int setlinewidth(int w)
{
   fprintf(out, "%d setlinewidth\n", w);
   return 0;
}

int moveto(float x, float y)
{
   fprintf(out, "%g inch %g inch moveto\n", x, y);
   return 0;
}

int lineto(float x, float y)
{
   fprintf(out, "%g inch %g inch lineto\n", x, y);
   return 0;
}

int print_dayview(struct tm *date)
{
   float x, y;
   int hour;
   char str[80];
   char datef[80];
   time_t ltime;
   struct tm *now;
   struct tm hours;
   const char *svalue;
   long ivalue;

   header();
   /* Draw the 2 gray columns and header block */
   newpath();
   setlinewidth(0);
   moveto(0.5, 3.5);
   lineto(0.5, 10.5);
   lineto(8, 10.5);
   lineto(8, 9.5);
   lineto(5, 9.5);
   lineto(5, 3.5);
   lineto(4.25, 3.5);
   lineto(4.25, 9.5);
   lineto(1.25, 9.5);
   lineto(1.25, 3.5);
   closepath();
   setgray(0.85);
   fill();
   stroke();

   /* make a couple of lines in the header box */
   newpath();
   setlinewidth(1);
   setgray(0);
   linefromto(0.5, 10.5, 8, 10.5);
   stroke();
   newpath();
   setlinewidth(2);
   linefromto(0.5, 9.5, 8, 9.5);
   stroke();
   newpath();
   setlinewidth(1);
   linefromto(0.5, 9.5625, 8, 9.5625);
   stroke();

   setfont("Times-Bold", 20);

   /* Put the Month name up */
   newpath();
   setgray(0);
   get_pref(PREF_LONGDATE, &ivalue, &svalue);
   /* strftime(str, 80, "%B %d, %Y", date); */
   strftime(str, 80, svalue, date);
   puttext(0.5, 10.25, str);

   /* Put the weekday name up */
   setfont("Times-Roman", 15);
   strftime(str, 80, "%A", date);   
   puttext(0.5, 10, str);

   setfont("Times-Roman", 14);
   hour=0;
   x=0.5; y=9.15;
   bzero(&hours, sizeof(hours));
   get_pref_time_no_secs(datef);
   for (hour=0; hour<24; hour++) {
      if (hour==12) {
	 x=4.25;
	 y=9.15;
      }
      hours.tm_hour=hour;
      strftime(str, 80, datef, &hours);
      puttext(x, y, str);
      y = y-0.5;
   }
   stroke();

   /* draw horizontal lines across hours */
   newpath();
   setlinewidth(0);
   for (y=9.0; y>3.25; y=y-0.5) {
      linefromto(1.25, y, 4.25, y);
      linefromto(5.0, y, 8.0, y);
   }
   stroke();
   
   newpath();
   setfont("Times-Roman", 10);

   time(&ltime);
   now = localtime(&ltime);
   get_pref(PREF_SHORTDATE, &ivalue, &svalue);
   strftime(str, 80, svalue, now);
   puttext(0.5, 0.9, str);
   puttext(7.5, 0.9, "J-Pilot");
   stroke();

   fill_in(date);
   
   showpage();
   
   return 0;
}

int print_monthview(struct tm *date)
{
   float x, y;
   int hour;
   char str[80];
   time_t ltime;
   struct tm *now;
   const char *svalue;
   long ivalue;

   out = fopen("ps_out.ps", "w");

   header();
   /* Draw the 2 gray columns and header block */
   rotate(90);
   newpath();
   setlinewidth(0);
   moveto(0.5, 3.5);
   lineto(0.5, 10.5);
   lineto(8, 10.5);
   lineto(8, 9.5);
   lineto(5, 9.5);
   lineto(5, 3.5);
   lineto(4.25, 3.5);
   lineto(4.25, 9.5);
   lineto(1.25, 9.5);
   lineto(1.25, 3.5);
   closepath();
   setgray(0.85);
   fill();
   stroke();

   /* make a couple of lines in the header box */
   newpath();
   setlinewidth(1);
   setgray(0);
   linefromto(0.5, 10.5, 8, 10.5);
   stroke();
   newpath();
   setlinewidth(2);
   linefromto(0.5, 9.5, 8, 9.5);
   stroke();
   newpath();
   setlinewidth(1);
   linefromto(0.5, 9.5625, 8, 9.5625);
   stroke();

   setfont("Times-Bold", 20);

   /* Put the Month name up */
   newpath();
   setgray(0);
   strftime(str, 80, "%B %d, %Y", date);
   puttext(0.5, 10.25, str);

   /* Put the weekday name up */
   setfont("Times-Roman", 15);
   strftime(str, 80, "%A", date);   
   puttext(0.5, 10, str);

   hour=0;
   x=0.5; y=9.15;
   for (hour=0; hour<24; hour++) {
      if (hour==12) {
	 x=4.25;
	 y=9.15;
      }
      if (hour<12) {
	 sprintf(str, "%d:00am", hour);
      } else {
	 sprintf(str, "%d:00pm", hour-12);
      }
      puttext(x, y, str);
      y = y-0.5;
   }
   stroke();

   /* draw horizontal lines across hours */
   newpath();
   setlinewidth(0);
   for (y=9.0; y>3.25; y=y-0.5) {
      linefromto(1.25, y, 4.25, y);
      linefromto(5.0, y, 8.0, y);
   }
   stroke();
   
   newpath();
   setfont("Times-Roman", 10);

   time(&ltime);
   now = localtime(&ltime);
   get_pref(PREF_SHORTDATE, &ivalue, &svalue);
   strftime(str, 80, svalue, now);
   puttext(0.5, 0.9, str);
   puttext(7.5, 0.9, "J-Pilot");
   stroke();
/*
   gsave();
   clip_to_box(1.25, 0.5, 4.25, 9.5);
   
   setfont("Times-Roman", 10);
   puttext(1.25, 5.4, "8:00 Arrive at work");
   puttext(1.25, 5.3, "8:00 Meeting");
   puttext(1.25, 5.2, "8:15 Coffee Break!");
   puttext(1.25, 5.1, "8:30 This is a very long appontment which should be too long");
   puttext(1.25, 5.0, "8:59 Last appointment ***");
   grestore();

   gsave();
   clip_to_box(5.0, 0.5, 8.0, 9.5);
   puttext(5.0, 5.1, "8:30 This is a very long appontment which should be too long");
   grestore();
*/ 
   
   showpage();
   
   fclose(out);
   
   return 0;
}

int fill_in(struct tm *date)
{
   AppointmentList *a_list;
   AppointmentList *temp_al;
   int i, r;
   int hours[24];
   int defaults1=0, defaults2=0;
   int hour24;
   int am;
   float top_y=9.40;
   float default_y=3.40;
   float indent1=1.25;
   float indent2=5.00;
   float step=0.12; /* This is the space between lines */
   float x,y;
   int max_per_line=4;
   char str[256];
   char datef[32];

   for (i=0; i<24; i++) {
      hours[i]=0;
   }
   
   a_list = NULL;
   
   r = get_days_appointments(&a_list, date);

   /* We have to go through them twice, once for AM, and once for PM
    * This is because of the clipping */
   for (i=0; i<2; i++) {
      am=i%2;
      gsave();
      if (am) {
	 clip_to_box(1.25, 0.5, 4.25, 9.5);
      } else {
	 clip_to_box(5.0, 0.5, 8.0, 9.5);
      }
      for (temp_al = a_list; temp_al; temp_al=temp_al->next) {
	 if (temp_al->ma.a.description == NULL) {
	    continue;
	 }
	 if (temp_al->ma.a.event) {
	    strcpy(str, " ");
	    if (!am) {
	       continue;
	    }
	    x=indent1;
	    y=default_y - defaults1 * step;
	    defaults1++;
	 } else {
	    hour24 = temp_al->ma.a.begin.tm_hour;
	    if ((hour24 > 11) && (am)) {
	       continue;
	    }
	    if ((hour24 < 12) && (!am)) {
	       continue;
	    }

	    get_pref_time_no_secs(datef);
	    strftime(str, 32, datef, &temp_al->ma.a.begin);

	    if (hour24 > 11) {
	       x=indent2;
	       y=top_y - (hour24 - 12) * 0.5 - (hours[hour24]) * step;
	       hours[hour24]++;
	       if (hours[hour24] > max_per_line) {
		  y=default_y - defaults2 * step;
		  defaults2++;
	       }
	    } else {
	       x=indent1;
	       y=top_y - (hour24) * 0.5 - (hours[hour24]) * step;
	       hours[hour24]++;
	       if (hours[hour24] > max_per_line) {
		  y=default_y - defaults1 * step;
		  defaults1++;
	       }
	    }
	 }
	 if (temp_al->ma.a.description) {
	    strncat(str, temp_al->ma.a.description, 150);
	    str[128]='\0';
	 }
	 if (y > 1.0) {
	    puttext(x, y, str);
	    /* printf("[%s]\n", str);*/
	 } else {
	    jpilot_logf(LOG_WARN, "Too many appointments, dropping one\n");
	 }
      }
      grestore();
   }
   free_AppointmentList(&a_list);

   return 0;
}

int print_datebook(int mon, int day, int year)
{
   struct tm date;
   
   date.tm_mon=mon;
   date.tm_mday=day;
   date.tm_year=year;
   date.tm_sec=0;
   date.tm_min=0;
   date.tm_hour=11;
   date.tm_isdst=-1;
   mktime(&date);
   
   out = print_open();
   if (!out) {
      return -1;
   }

   print_dayview(&date);

   print_close(out);
   
   return 0;
}

int f_indent_print(FILE *f, int indent, char *str) {
   char *P;
   int i, col;

   col=indent;
   for (P=str; *P; P++) {
      col++;
      if ((*P==10) || (*P==13)) {
	 fprintf(f, "%c", *P);
	 for (i=indent; i; i--) {
	    fprintf(f, " ");
	 }
	 col=indent;
	 continue;
      }
      if (col>75) {
	 fprintf(f, "\n");
	 for (i=indent; i; i--) {
	    fprintf(f, " ");
	 }
	 col=indent+1;
      }
      fprintf(f, "%c", *P);
   }
   return 0;
}

/*
 * 
 * Address code
 * 
 */
int print_address_header()
{
   time_t ltime;
   struct tm *date;
   long ivalue;
   const char *svalue;
   char str[256];
   
   time(&ltime);
   date = localtime(&ltime);
   
   get_pref(PREF_SHORTDATE, &ivalue, &svalue);
   strftime(str, 250, svalue, date);
   
   fprintf(out,
	   "%%!PS-Adobe-2.0 EPSF-2.0\n"
	   "%%%%Creator: J-Pilot\n"
	   "%%%%CreationDate: %s\n"
	   "%%%%Title: Addresses\n"
	   "%%%%BoundingBox: 36 36 576 756\n",
	   str);
   fprintf(out,
	   "/inch {72 mul} def\n"
	   "/left {0.5 inch} def\n"
	   "/bottom {1.0 inch} def\n"
	   "/bottom_hline {2.0 inch} def\n"
	   "/footer {0.9 inch} def\n"
	   "/top {10.5 inch 14 sub} def\n"
	   "/buffer 1024 string def\n"
	   "/scratch 128 string def\n"
	   "/printobject {\n"
	   "dup 128 string cvs dup (--nostringval--) eq {\n"
	   "pop type24 string cvs\n"
	   "}{\n"
	   "exch pop\n"
	   "} ifelse\n"
	   "} bind def\n");
   /* Checkbox stuff */
   fprintf(out,
	  "/checkboxcheck {\n"
	  "currentpoint 6 add moveto\n"
	  "4 -5 rlineto\n"
	  "6 12 rlineto\n"
	  "} bind def\n"
	  "/checkboxbox {\n"
	  "8 0 rlineto\n"
	  "0 8 rlineto\n"
	  "-8 0 rlineto\n"
	  "0 -8 rlineto\n"
	  "} bind def\n"
	  "/checkbox {\n"
	  "currentpoint\n"
	  "gsave\n"
	  "newpath\n"
	  "moveto\n"
	  "1 setlinewidth\n"
	  "checkboxbox\n"
	  "stroke\n"
	  "grestore\n"
	  "} bind def\n"
	  "/checkedbox {\n"
	  "currentpoint\n"
	  "gsave\n"
	  "newpath\n"
	  "moveto\n"
	  "1 setlinewidth\n"
	  "checkboxbox\n"
	  "checkboxcheck\n"
	  "stroke\n"
	  "grestore\n"
	  "} bind def\n"
	  );
   fprintf(out,
	  "/hline {\n"
	   "currentpoint 1 add currentpoint 1 add\n"
	   "currentpoint 4 add currentpoint 4 add\n"
	   "gsave\n"
	   "newpath\n"
	   "moveto\n"
	   "exch\n"
	   "1.0 inch add\n"
	   "exch\n"
	   "7 setlinewidth\n"
	   "lineto\n"
	   "stroke\n"
	   "%%\n"
	   "newpath\n"
	   "moveto\n"
	   "exch\n"
	   "7.5 inch add\n"
	   "exch\n"
	   "1 setlinewidth\n"
	   "lineto\n"
	   "stroke\n"
	   "grestore\n"
	   "} bind def\n"
	   "%%\n"
	   "%%\n");
   fprintf(out, 
	   "/setup\n"
	   "{\n"
	   "/Times-Roman 10 selectfont\n"
	   "left footer moveto\n"
	   "(%s) show\n"
	   "7.5 inch footer moveto\n"
	   "(J-Pilot) show\n"
	   "%% This assumes that the prev page number is on the stack\n"
	   "4.25 inch footer moveto\n"
	   "1 add dup printobject show\n"
	   "/Courier 12 selectfont\n"
	   "left top moveto\n"
	   "} bind def\n"
	   "/printit\n"
	   "{\n"
	   "{ %%loop\n"
	   "currentfile buffer readline { %%ifelse\n"
	   "("Q_FLAG_CHAR"LINEFEED) search { %%if\n"
	   "pop pop pop showpage setup ( )\n"
	   "currentpoint 14 add moveto\n"
	   "} if\n"
	   "("Q_FLAG_CHAR"HLINE) search { %%if\n"
	   "currentpoint exch pop bottom_hline le { %%if\n"
	   "pop pop pop\n"
	   "showpage setup\n"
	   "0 0 0\n"
	   "} if\n"
	   "hline\n"
	   "pop pop pop ( )\n"
	   "} if\n"
	   "("Q_FLAG_CHAR"C12) search {\n"
	   "/Courier 12 selectfont\n"
	   "currentpoint 14 add moveto\n"
	   "pop pop pop ( )\n"
	   "} if\n"
	   "("Q_FLAG_CHAR"CB12) search {\n"
	   "/Courier-Bold 12 selectfont\n"
	   "currentpoint 14 add moveto\n"
	   "pop pop pop ( )\n"
	   "} if\n",
	   str
	   );
   /* Check box */
   fprintf(out,
	   "("Q_FLAG_CHAR"CHECKBOX) search {\n"
	   "currentpoint exch pop bottom_hline le {\n"
	   "pop pop pop\n"
	   "showpage setup\n"
	   "0 0 0\n"
	   "} if\n"
	   "checkbox\n"
	   "currentpoint 14 add moveto\n"
	   "pop pop pop ( )\n"
	   "} if\n"
	   );
   /* Check box */
   fprintf(out,
	   "("Q_FLAG_CHAR"CHECKEDBOX) search {\n"
	   "currentpoint exch pop bottom_hline le {\n"
	   "pop pop pop\n"
	   "showpage setup\n"
	   "0 0 0\n"
	   "} if\n"
	   "checkedbox\n"
	   "currentpoint 14 add moveto\n"
	   "pop pop pop ( )\n"
	   "} if\n"
	   );
    fprintf(out,
	    "gsave show grestore\n"
	   "currentpoint 14 sub moveto\n"
	   "currentpoint exch pop bottom le { %%if\n"
	   "showpage setup\n"
	   "} if\n"
	   "}{ %%else\n"
	   "showpage exit\n"
	   "} ifelse\n"
	   "} loop\n"
	   "} bind def\n"
	   "0 %%The page number minus 1\n"
	   "setup printit\n"
	   );
   return 0;
}
   
int print_addresses(AddressList *address_list)
{
   long one_rec_per_page;
   long lines_between_recs;
   AddressList *temp_al;
   struct AddressAppInfo address_app_info;
   struct Address *a;
   int show1, show2, show3;
   int j, i, i2;
   int len;
   /*This is because the palm doesn\'t show the address entries in order */
   int order[22]={0,1,13,2,3,4,5,6,7,8,9,10,11,12,14,15,16,17,18,19,20,21
   };
   char str[100];
      
   out = print_open();
   if (!out) {
      return -1;
   }

   get_address_app_info(&address_app_info);

   print_address_header();
   
   if (address_app_info.sortByCompany) {
      show1=2; /*company */
      show2=0; /*last name */
      show3=1; /*first name */
   } else {
      show1=0; /*last name */
      show2=1; /*first name */
      show3=2; /*company */
   }

#define NUM_ADDRESS_ENTRIES 19
   get_pref(PREF_PRINT_ONE_PER_PAGE, &one_rec_per_page, NULL);
   get_pref(PREF_NUM_BLANK_LINES, &lines_between_recs, NULL);

   for (temp_al = address_list; temp_al; temp_al=temp_al->next) {

      fprintf(out, "%cHLINE\n", FLAG_CHAR);
      
      str[0]='\0';
      if (temp_al->ma.a.entry[show1] || temp_al->ma.a.entry[show2]) {
	 if (temp_al->ma.a.entry[show1] && temp_al->ma.a.entry[show2]) {
	    g_snprintf(str, 48, "%s, %s", temp_al->ma.a.entry[show1], temp_al->ma.a.entry[show2]);
	 }
	 if (temp_al->ma.a.entry[show1] && ! temp_al->ma.a.entry[show2]) {
	    strncpy(str, temp_al->ma.a.entry[show1], 48);
	 }
	 if (! temp_al->ma.a.entry[show1] && temp_al->ma.a.entry[show2]) {
	    strncpy(str, temp_al->ma.a.entry[show2], 48);
	 }
      } else if (temp_al->ma.a.entry[show3]) {
	    strncpy(str, temp_al->ma.a.entry[show3], 48);
      } else {
	    strcpy(str, "-Unnamed-");
      }
      
      courier_bold_12();
      fprintf(out, "%s\n", str);
      courier_12();

      a = &(temp_al->ma.a);
      for (i=0; i<NUM_ADDRESS_ENTRIES; i++) {
	 i2=order[i];
	 if (a->entry[i2]) {
	    if (i2>2 && i2<8) {
	       fprintf(out, "%s: ", address_app_info.phoneLabels[a->phoneLabel[i2-3]]);
	       len = strlen(address_app_info.phoneLabels[a->phoneLabel[i2-3]]);
	    } else {
	       fprintf(out, "%s: ", address_app_info.labels[i2]);
	       len = strlen(address_app_info.labels[i2]);
	    }
	    for (j=16-len; j; j--) {
	       fprintf(out, " ");
	    }
	    f_indent_print(out, 18, a->entry[i2]);
	    fprintf(out, "\n");
	 }
      }
      if (one_rec_per_page) {
	 fprintf(out, "%cLINEFEED\n", FLAG_CHAR);
      } else {
	 for (i=lines_between_recs; i>0; i--) {
	    fprintf(out, "\n");
	 }
      }
   }

   print_close(out);
   
   return 0;
}

/*
 * 
 * ToDo code
 * 
 */
int print_todos(ToDoList *todo_list)
{
   long one_rec_per_page;
   long lines_between_recs;
   long ivalue;
   ToDoList *temp_l;
   struct ToDo *todo;
   int i, indent;
   const char *datef;
   char str[100];
   time_t ltime;
   struct tm *now;

   out = print_open();
   if (!out) {
      return -1;
   }

   print_address_header();
   
   get_pref(PREF_PRINT_ONE_PER_PAGE, &one_rec_per_page, NULL);
   get_pref(PREF_NUM_BLANK_LINES, &lines_between_recs, NULL);

   get_pref(PREF_SHORTDATE, &ivalue, &datef);
   now = localtime(&ltime);
   strftime(str, 80, datef, now);
   indent=strlen(str) + 8;

   courier_12();

   for (temp_l = todo_list; temp_l; temp_l=temp_l->next) {
      todo = &(temp_l->mtodo.todo);

      if (todo->complete) {
	 fprintf(out, "%cCHECKEDBOX\n", FLAG_CHAR);
      } else {
	 fprintf(out, "%cCHECKBOX\n", FLAG_CHAR);
      }
      
      fprintf(out, "   %d ", todo->priority);

      if (todo->indefinite) {
	 sprintf(str, "%s           ", "No Due");
	 str[indent-8]='\0';
      } else {
	 strftime(str, 20, datef, &(todo->due));
      }
      fprintf(out, " %s  ", str);

      if (todo->description) {
	 f_indent_print(out, indent, todo->description);
      }

      if ((todo->note) && todo->note[0]) {
	 fprintf(out, "\n");
	 for (i=indent; i>0; i--) {
	    fprintf(out, " ");
	 }
	 fprintf(out, "Note:\n");
	 for (i=indent; i>0; i--) {
	    fprintf(out, " ");
	 }
	 f_indent_print(out, indent, todo->note);
      }

      fprintf(out, "\n");

      if (one_rec_per_page) {
	 fprintf(out, "%cLINEFEED\n", FLAG_CHAR);
      } else {
	 for (i=lines_between_recs; i>0; i--) {
	    fprintf(out, "\n");
	 }
      }
   }

   print_close(out);
   
   return 0;
}
/*
 * 
 * Memo code
 * 
 */
int print_memos(MemoList *memo_list)
{
   long one_rec_per_page;
   long lines_between_recs;
   MemoList *temp_l;
   struct Memo *memo;
   int i;

   out = print_open();
   if (!out) {
      return -1;
   }

   print_address_header();
   
   get_pref(PREF_PRINT_ONE_PER_PAGE, &one_rec_per_page, NULL);
   get_pref(PREF_NUM_BLANK_LINES, &lines_between_recs, NULL);

   courier_12();

   for (temp_l = memo_list; temp_l; temp_l=temp_l->next) {
      memo = &(temp_l->mmemo.memo);

      if (memo->text) {
	 fprintf(out, "%cHLINE\n", FLAG_CHAR);
	 f_indent_print(out, 0, memo->text);
	 fprintf(out, "\n");
      }
      
      if (one_rec_per_page) {
	 fprintf(out, "%cLINEFEED\n", FLAG_CHAR);
      } else {
	 for (i=lines_between_recs; i>0; i--) {
	    fprintf(out, "\n");
	 }
      }
   }

   print_close(out);
   
   return 0;
}

/* prefs.c
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

#include "config.h"
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utils.h"
#include "prefs.h"
#include "log.h"

/*These are the default settings */
/*name, usertype, filetype, ivalue, char svalue[MAX_PREF_VALUE+2]; */
static prefType glob_prefs[NUM_PREFS] = {
   {"jpilotrc", CHARTYPE, CHARTYPE, 0, "jpilotrc.default"},
     {"time", CHARTYPE, INTTYPE, 0, ""},
     {"sdate", CHARTYPE, INTTYPE, 0, ""},
     {"ldate", CHARTYPE, INTTYPE, 0, ""},
     {"fdow", CHARTYPE, INTTYPE, 0, ""},
     {"show_deleted", INTTYPE, INTTYPE, 0, ""},
     {"show_modified", INTTYPE, INTTYPE, 0, ""},
     {"hide_completed", INTTYPE, INTTYPE, 0, ""},
     {"highlight_days", INTTYPE, INTTYPE, 1, ""},
     {"port", CHARTYPE, CHARTYPE, 0, ""},
     {"rate", CHARTYPE, INTTYPE, 4, ""},
     {"user", CHARTYPE, CHARTYPE, 0, ""}
};

struct jlist {
   char *name;
   struct jlist *next;
};

static struct jlist *dir_list=NULL;


int get_pref_dmy_order()
{
   int n;
   
   get_pref(PREF_SHORTDATE, &n, NULL);
   if (n<2) {
      return PREF_MDY;
   }
   if ((n>1) && (n<5)) {
      return PREF_DMY;
   }
   if ((n>4)) {
      return PREF_YMD;
   }
   return 0;
}

/*This function is used externally to free up any memory that prefs is using */
void free_prefs()
{
   struct jlist *temp_list, *next_list;
   
   for (temp_list=dir_list; temp_list; temp_list=next_list) {
      next_list=temp_list->next;
      if (temp_list->name) {
	 free(temp_list->name);
      }
   }
   dir_list=NULL;
}

static int get_rcfile_name(int n, char *rc_copy)
{
   DIR *dir;
   struct dirent *dirent;
   char full_name[256];
   int i;
   char filename[256];
   int found, count;
   struct jlist *temp_list, *new_entry;
  

   if (dir_list == NULL) {
      i = found = count = 0;
      sprintf(filename, "%s/%s/%s/", BASE_DIR, "share", EPN);
      jpilot_logf(LOG_DEBUG, "opening dir %s\n", filename);
      dir = opendir(filename);
      if (dir) {
	 for(i=0; (dirent = readdir(dir)); i++) {
	    sprintf(filename, "%s%s", EPN, "rc");
	    if (strncmp(filename, dirent->d_name, strlen(filename))) {
	       continue;
	    } else {
	       jpilot_logf(LOG_DEBUG, "found %s\n", dirent->d_name);
	       new_entry = malloc(sizeof(struct jlist));
	       if (!new_entry) {
		  jpilot_logf(LOG_FATAL, "out of memory\n");
		  return -1;
	       }  
	       new_entry->name = strdup(dirent->d_name);
	       new_entry->next = dir_list;
	       dir_list = new_entry;
	    }
	 }
      }
      if (dir) {
	 closedir(dir);
      }

      get_home_file_name("", full_name, 255);
      jpilot_logf(LOG_DEBUG, "opening dir %s\n", full_name);
      dir = opendir(full_name);
      if (dir) {
	 for(; (dirent = readdir(dir)); i++) {
	    sprintf(filename, "%s%s", EPN, "rc");
	    if (strncmp(filename, dirent->d_name, strlen(filename))) {
	       continue;
	    } else {
	       jpilot_logf(LOG_DEBUG, "found %s\n", dirent->d_name);
	       new_entry = malloc(sizeof(struct jlist));
	       if (!new_entry) {
		  jpilot_logf(LOG_FATAL, "out of memory\n");
		  return -1;
	       }  
	       new_entry->name = strdup(dirent->d_name);
	       new_entry->next = dir_list;
	       dir_list = new_entry;
	    }
	 }
      }
      if (dir) {
	 closedir(dir);
      }
   }

   found = 0;
   for (i=0, temp_list=dir_list; temp_list; temp_list=temp_list->next, i++) {
      if (i == n) {
	 strncpy(rc_copy, temp_list->name, MAX_PREF_VALUE);
	 rc_copy[MAX_PREF_VALUE-1]='\0';
	 found=1;
	 break;
      }
   }
   
   if (found) {
      return 0;
   } else {
      rc_copy[0]='\0';
      return -1;
   }
}

/*if n is out of range then this function will fail */
int get_pref_possibility(int which, int n, char *pref_str)
{
   const char *short_date_formats[] = {
      "%x",
      "%m/%d/%y",
      "%d/%m/%y",
      "%d.%m.%y",
      "%d-%m-%y",
      "%y/%m/%d",
      "%y.%m.%d",
      "%y-%m-%d"
   };

   const char *long_date_formats[] = {
      "%x",
      "%B %d, %Y",
      "%d %B %Y",
      "%d. %B %Y",
      "%d %B, %Y",
      "%Y. %B. %d",
      "%Y %B %d"
   };

   const char *time_formats[] = {
      "%X",
      "%I:%M:%S %p",
      "%H:%M:%S",
      "%I.%M.%S %p",
      "%H.%M.%S",
      "%H,%M,%S",
      "%I:%M %p",
      "%H:%M",
      "%I.%M %p",
      "%H.%M",
      "%H,%M"
   };

   static const char *days[] = {
      "Sunday",
      "Monday"
   };

   static const char *rates[] = {
      "300",
	"1200",
	"2400",
	"4800",
	"9600",
	"19200",
	"38400",
	"57600",
	"115200",
	"230400",
	"460800"
   };

   switch(which) {

    case PREF_RCFILE:
	 return get_rcfile_name(n, pref_str);
      break;

    case PREF_TIME:
      if ((n >= NUM_TIMES) || (n<0)) {
	 pref_str[0]='\0';
	 return -1;
      }
      strcpy(pref_str, time_formats[n]);
      break;

    case PREF_SHORTDATE:
      if ((n >= NUM_SHORTDATES) || (n<0)) {
	 pref_str[0]='\0';
	 return -1;
      }
      strcpy(pref_str, short_date_formats[n]);
      break;
      
    case PREF_LONGDATE:
      if ((n >= NUM_LONGDATES) || (n<0)) {
	 pref_str[0]='\0';
	 return -1;
      }
      strcpy(pref_str, long_date_formats[n]);
      break;

    case PREF_FDOW:
      if ((n > 1) || (n<0)) {
	 pref_str[0]='\0';
	 return -1;
      }
      strcpy(pref_str, days[n]);
      break;

    case PREF_RATE:
      if ((n >= NUM_RATES) || (n<0)) {
	 pref_str[0]='\0';
	 return -1;
      }
      strcpy(pref_str, rates[n]);
      break;

    default:
      pref_str[0]='\0';
      jpilot_logf(LOG_DEBUG, "Unknown preference type\n");
      return -1;
   }

   return 0;
}

/*if n is out of range then this function will fail */
int get_pref(int which, int *n, const char **ret)
{
   if ((which < 0) || (which > NUM_PREFS)) {
      return -1;
   }
   *n = glob_prefs[which].ivalue;
   if (glob_prefs[which].usertype == CHARTYPE) {
      if (ret!=NULL) {
	 *ret = glob_prefs[which].svalue;
      }
   } else {
      if (ret!=NULL) {
	 *ret = NULL;
      }
   }
   return 0;
}

int set_pref(int which, int n)
{
   if ((which < 0) || (which > NUM_PREFS)) {
      return -1;
   }
   glob_prefs[which].ivalue = n;
   if (glob_prefs[which].usertype == CHARTYPE) {
      get_pref_possibility(which, glob_prefs[which].ivalue, glob_prefs[which].svalue);
   }
   return 0;
}

int set_pref_char(int which, char *string)
{
   if ((which < 0) || (which > NUM_PREFS)) {
      return -1;
   }
   if (string == NULL) {
      glob_prefs[which].svalue[0]='\0';
      return 0;
   }
   if (glob_prefs[which].filetype == CHARTYPE) {
      strncpy(glob_prefs[which].svalue, string, MAX_PREF_VALUE);
      glob_prefs[which].svalue[MAX_PREF_VALUE-1]='\0';
   }
   return 0;
}

static int validate_glob_prefs()
{
   int i, r;
   char svalue[MAX_PREF_VALUE];

   if (glob_prefs[PREF_TIME].ivalue >= NUM_TIMES) {
      glob_prefs[PREF_TIME].ivalue = NUM_TIMES - 1;
   }
   if (glob_prefs[PREF_TIME].ivalue < 0) {
      glob_prefs[PREF_TIME].ivalue = 0;
   }

   if (glob_prefs[PREF_SHORTDATE].ivalue >= NUM_SHORTDATES) {
      glob_prefs[PREF_SHORTDATE].ivalue = NUM_SHORTDATES - 1;
   }
   if (glob_prefs[PREF_SHORTDATE].ivalue < 0) {
      glob_prefs[PREF_SHORTDATE].ivalue = 0;
   }

   if (glob_prefs[PREF_LONGDATE].ivalue >= NUM_LONGDATES) {
      glob_prefs[PREF_LONGDATE].ivalue = NUM_LONGDATES - 1;
   }
   if (glob_prefs[PREF_LONGDATE].ivalue < 0) {
      glob_prefs[PREF_LONGDATE].ivalue = 0;
   }

   if (glob_prefs[PREF_FDOW].ivalue > 1) {
      glob_prefs[PREF_FDOW].ivalue = 1;
   }
   if (glob_prefs[PREF_FDOW].ivalue < 0) {
      glob_prefs[PREF_FDOW].ivalue = 0;
   }

   if (glob_prefs[PREF_SHOW_DELETED].ivalue > 1) {
      glob_prefs[PREF_SHOW_DELETED].ivalue = 1;
   }
   if (glob_prefs[PREF_SHOW_DELETED].ivalue < 0) {
      glob_prefs[PREF_SHOW_DELETED].ivalue = 0;
   }

   if (glob_prefs[PREF_SHOW_MODIFIED].ivalue > 1) {
      glob_prefs[PREF_SHOW_MODIFIED].ivalue = 1;
   }
   if (glob_prefs[PREF_SHOW_MODIFIED].ivalue < 0) {
      glob_prefs[PREF_SHOW_MODIFIED].ivalue = 0;
   }

   if (glob_prefs[PREF_HIDE_COMPLETED].ivalue > 1) {
      glob_prefs[PREF_HIDE_COMPLETED].ivalue = 1;
   }
   if (glob_prefs[PREF_HIDE_COMPLETED].ivalue < 0) {
      glob_prefs[PREF_HIDE_COMPLETED].ivalue = 0;
   }

   if (glob_prefs[PREF_HIGHLIGHT].ivalue > 1) {
      glob_prefs[PREF_HIGHLIGHT].ivalue = 1;
   }
   if (glob_prefs[PREF_HIGHLIGHT].ivalue < 0) {
      glob_prefs[PREF_HIGHLIGHT].ivalue = 0;
   }

   if (glob_prefs[PREF_RATE].ivalue >= NUM_RATES) {
      glob_prefs[PREF_RATE].ivalue = NUM_RATES - 1;
   }
   if (glob_prefs[PREF_RATE].ivalue < 0) {
      glob_prefs[PREF_RATE].ivalue = 0;
   }

   get_pref_possibility(PREF_TIME, glob_prefs[PREF_TIME].ivalue, glob_prefs[PREF_TIME].svalue);

   get_pref_possibility(PREF_SHORTDATE, glob_prefs[PREF_SHORTDATE].ivalue, glob_prefs[PREF_SHORTDATE].svalue);

   get_pref_possibility(PREF_LONGDATE, glob_prefs[PREF_LONGDATE].ivalue, glob_prefs[PREF_LONGDATE].svalue);

   get_pref_possibility(PREF_FDOW, glob_prefs[PREF_FDOW].ivalue, glob_prefs[PREF_FDOW].svalue);
   
   get_pref_possibility(PREF_RATE, glob_prefs[PREF_RATE].ivalue, glob_prefs[PREF_RATE].svalue);
   
   for (i=0; i<1000; i++) {
      r = get_pref_possibility(PREF_RCFILE, i, svalue);
      if (r) break;
      if (!strcmp(svalue, glob_prefs[PREF_RCFILE].svalue)) {
	 glob_prefs[PREF_RCFILE].ivalue = i;
	 break;
      }
   }
   return 0;
}

int read_rc_file()
{
   int i;
   FILE *in;
   char line[256];
   char *field1, *field2;
   char *Pc;

   in=open_file("jpilot.rc","r" );
   if (!in) {
      return -1;
   }

   while (!feof(in)) {
      fgets(line, 255, in);
      line[255] = '\0';
      field1 = (char *)strtok(line, " ");
      field2 = (char *)strtok(NULL, "\n");
      if ((field1 == NULL) || (field2 == NULL)) {
	 continue;
      }
      if ((Pc = (char *)index(field2, '\n'))) {
	 Pc[0]='\0';
      }
      for(i=0; i<NUM_PREFS; i++) {
	 if (!strcmp(glob_prefs[i].name, field1)) {
	    if (glob_prefs[i].filetype == INTTYPE) {
	       glob_prefs[i].ivalue = atoi(field2);
	    }
	    if (glob_prefs[i].filetype == CHARTYPE) {
	       strncpy(glob_prefs[i].svalue, field2, MAX_PREF_VALUE);
	       glob_prefs[i].svalue[MAX_PREF_VALUE-1]='\0';
	    }
	 }
      }
   }
   fclose(in);
   validate_glob_prefs();
   
   return 0;
}

int write_rc_file()
{
   int i;
   FILE *out;

   out=open_file("jpilot.rc","w" );
   if (!out) {
      return -1;
   }

   for(i=0; i<NUM_PREFS; i++) {

      if (glob_prefs[i].filetype == INTTYPE) {
	 fprintf(out, "%s %d\n", glob_prefs[i].name, glob_prefs[i].ivalue);
      }

      if (glob_prefs[i].filetype == CHARTYPE) {
	 fprintf(out, "%s %s\n", glob_prefs[i].name, glob_prefs[i].svalue);
      }
   }
   fclose(out);
   
   return 0;
}

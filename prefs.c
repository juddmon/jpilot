/* prefs.c
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
 */

#include "config.h"
#include "i18n.h"
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utils.h"
#include "prefs.h"
#include "log.h"

/*These are the default settings */
/*name, usertype, filetype, ivalue, char *svalue, svalue_size; */
static prefType glob_prefs[NUM_PREFS] = {
     {EPN"rc", CHARTYPE, CHARTYPE, 0, NULL, 0},
     {"time", CHARTYPE, INTTYPE, 0, NULL, 0},
     {"sdate", CHARTYPE, INTTYPE, 0, NULL, 0},
     {"ldate", CHARTYPE, INTTYPE, 0, NULL, 0},
     {"fdow", CHARTYPE, INTTYPE, 0, NULL, 0},
     {"show_deleted", INTTYPE, INTTYPE, 0, NULL, 0},
     {"show_modified", INTTYPE, INTTYPE, 0, NULL, 0},
     {"hide_completed", INTTYPE, INTTYPE, 0, NULL, 0},
     {"highlight_days", INTTYPE, INTTYPE, 1, NULL, 0},
     {"port", CHARTYPE, CHARTYPE, 0, NULL, 0},
     {"rate", CHARTYPE, INTTYPE, 4, NULL, 0},
     {"user", CHARTYPE, CHARTYPE, 0, NULL, 0},
     {"user_id", INTTYPE, INTTYPE, 0, NULL, 0},
     {"pc_id", INTTYPE, INTTYPE, 0, NULL, 0},
     {"num_backups", INTTYPE, INTTYPE, 2, NULL, 0},
     {"window_width", INTTYPE, INTTYPE, 640, NULL, 0},
     {"window_height", INTTYPE, INTTYPE, 480, NULL, 0},
     {"datebook_pane", INTTYPE, INTTYPE, 370, NULL, 0},
     {"address_pane", INTTYPE, INTTYPE, 340, NULL, 0},
     {"todo_pane", INTTYPE, INTTYPE, 370, NULL, 0},
     {"memo_pane", INTTYPE, INTTYPE, 370, NULL, 0},
     {"use_db3", INTTYPE, INTTYPE, 0, NULL, 0},
     {"last_app", INTTYPE, INTTYPE, DATEBOOK, NULL, 0},
     {"print_this_many", INTTYPE, INTTYPE, 3, NULL, 0},
     {"print_one_per_page", INTTYPE, INTTYPE, 0, NULL ,0},
     {"print_blank_lines", INTTYPE, INTTYPE, 1, NULL, 0},
     {"print_command", CHARTYPE, CHARTYPE, 0, NULL, 0},
     {"char_set", INTTYPE, INTTYPE, CHAR_SET_LATIN1, NULL, 0},
     {"sync_datebook", INTTYPE, INTTYPE, 1, NULL, 0},
     {"sync_address", INTTYPE, INTTYPE, 1, NULL, 0},
     {"sync_todo", INTTYPE, INTTYPE, 1, NULL, 0},
     {"sync_memo", INTTYPE, INTTYPE, 1, NULL, 0},
     {"sync_memo32", INTTYPE, INTTYPE, 0, NULL, 0},
     {"address_page", INTTYPE, INTTYPE, 0, NULL, 0},
     {"output_height", INTTYPE, INTTYPE, 40, NULL, 0},
     {"open_alarm_windows", INTTYPE, INTTYPE, 1, NULL, 0},
     {"do_alarm_command", INTTYPE, INTTYPE, 0, NULL, 0},
     {"alarm_command", CHARTYPE, CHARTYPE, 0, NULL, 0},
     {"remind_in", CHARTYPE, CHARTYPE, 0, NULL, 0},
     {"remind_units", INTTYPE, INTTYPE, 0, NULL, 0},
   /* This is actually the password, but I wanted to name it something more discreet */
     {"session_id", CHARTYPE, CHARTYPE, 0, NULL, 0},
     {"memo32_mode", INTTYPE, INTTYPE, 0, NULL, 0},
     {"paper_size", INTTYPE, INTTYPE, 0, NULL, 0},
     {"datebook_export_filename", CHARTYPE, CHARTYPE, 0, NULL, 0},
     {"datebook_import_path", CHARTYPE, CHARTYPE, 0, NULL, 0},
     {"address_export_filename", CHARTYPE, CHARTYPE, 0, NULL, 0},
     {"address_import_path", CHARTYPE, CHARTYPE, 0, NULL, 0},
     {"todo_export_filename", CHARTYPE, CHARTYPE, 0, NULL, 0},
     {"todo_import_path", CHARTYPE, CHARTYPE, 0, NULL, 0},
     {"memo_export_filename", CHARTYPE, CHARTYPE, 0, NULL, 0},
     {"memo_import_path", CHARTYPE, CHARTYPE, 0, NULL, 0},
     {"manana_mode", INTTYPE, INTTYPE, 0, NULL, 0},
     {"sync_manana", INTTYPE, INTTYPE, 0, NULL, 0},
     {"use_jos", INTTYPE, INTTYPE, 0, ""},
     {"phone_prefix1", CHARTYPE, CHARTYPE, 0, NULL, 0},
     {"phone_prefix1_on", INTTYPE, INTTYPE, 0, NULL, 0},
     {"phone_prefix2", CHARTYPE, CHARTYPE, 0, NULL, 0},
     {"phone_prefix2_on", INTTYPE, INTTYPE, 0, NULL, 0},
     {"phone_prefix3", CHARTYPE, CHARTYPE, 0, NULL, 0},
     {"phone_prefix3_on", INTTYPE, INTTYPE, 0, NULL, 0},
     {"dial_command", CHARTYPE, CHARTYPE, 0, NULL, 0},
     {"datebook_todo_pane", INTTYPE, INTTYPE, 350, NULL, 0},
     {"datebook_todo_show", INTTYPE, INTTYPE, 0, NULL, 0},
     {"hide_not_due", INTTYPE, INTTYPE, 0, NULL, 0}
};

struct name_list {
   char *name;
   struct name_list *next;
};

static struct name_list *dir_list=NULL;


void pref_init()
{
   int i;

   for (i=0; i<NUM_PREFS; i++) {
      switch (i) {
       case PREF_RCFILE:
	 glob_prefs[i].svalue=strdup(EPN"rc.default");
	 glob_prefs[i].svalue_size=strlen(glob_prefs[i].svalue)+1;
	 break;
       case PREF_PRINT_COMMAND:
	 glob_prefs[i].svalue=strdup("lpr -h");
	 glob_prefs[i].svalue_size=strlen(glob_prefs[i].svalue)+1;
	 break;
       case PREF_ALARM_COMMAND:
	 glob_prefs[i].svalue=strdup("echo %t %d");
	 glob_prefs[i].svalue_size=strlen(glob_prefs[i].svalue)+1;
	 break;
       case PREF_REMIND_IN:
	 glob_prefs[i].svalue=strdup("5");
	 glob_prefs[i].svalue_size=strlen(glob_prefs[i].svalue)+1;
	 break;
       case PREF_PASSWORD:
	 glob_prefs[i].svalue=strdup("09021345070413440c08135a3215135dd217ead3b5df556322e9a14a994b0f88");
	 glob_prefs[i].svalue_size=strlen(glob_prefs[i].svalue)+1;
	 break;
       case PREF_DIAL_COMMAND:
	 glob_prefs[i].svalue=strdup("jpilot-dial --lv 0 --rv 50 %n");
	 glob_prefs[i].svalue_size=strlen(glob_prefs[i].svalue)+1;
	 break;
       default:
	 glob_prefs[i].svalue=strdup("");
	 glob_prefs[i].svalue_size=1;
      }
   }
}

void jp_pref_init(prefType prefs[], int count)
{
   int i;

   for (i=0; i<count; i++) {
      if (prefs[i].svalue) {
	 prefs[i].svalue=strdup(prefs[i].svalue);
      } else {
	 prefs[i].svalue=strdup("");
      }
      prefs[i].svalue_size=strlen(prefs[i].svalue)+1;
   }
}

void jp_free_prefs(prefType prefs[], int count)
{
   int i;

   for (i=0; i<count; i++) {
      if (prefs[i].svalue) {
	 free(prefs[i].svalue);
	 prefs[i].svalue=NULL;
      }
   }
}

int get_pref_time_no_secs(char *datef)
{
   /* "%I:%M:%S %p" */
   /* "%I:%M:%S" */
   /* "%I:%M" */
   /* "%I:%M%p" */
   long ivalue;
   const char *svalue;
   int i1, i2;

   get_pref(PREF_TIME, &ivalue, &svalue);
   if (!svalue) {
      return -1;
   }
   for (i1=0, i2=0; ; i1++, i2++) {
      if (svalue[i2]=='S') {
	 i1-=2;
	 i2++;
      }
      if (svalue[i2]==' ') {
	 i1--;
	 continue;
      }
      datef[i1]=svalue[i2];
      if (svalue[i2]=='\0') {
	 break;
      }
   }
   return 0;
}

int get_pref_time_no_secs_no_ampm(char *datef)
{
   long ivalue;
   const char *svalue;

   get_pref(PREF_TIME, &ivalue, &svalue);
   if (!svalue) {
      return -1;
   }
   if (svalue) {
      strncpy(datef, svalue, 5);
      datef[5]='\0';
   } else {
      datef[0]='\0';
   }

   return 0;
}

int get_pref_dmy_order()
{
   long n;

   get_pref(PREF_SHORTDATE, &n, NULL);
   if (n<1) {
      return PREF_MDY;
   }
   if ((n>0) && (n<4)) {
      return PREF_DMY;
   }
   if ((n>3)) {
      return PREF_YMD;
   }
   return 0;
}


/*This function is used internally to free up any memory that prefs is using */
/* I'm not using this function right now.
void free_name_list(struct name_list **Plist)
{
   struct name_list *temp_list, *next_list;

   for (temp_list=*Plist; temp_list; temp_list=next_list) {
      next_list=temp_list->next;
      if (temp_list->name) {
	 free(temp_list->name);
      }
      free(temp_list);
   }
   *Plist=NULL;
}
*/
static int get_rcfile_name(int n, char *rc_copy)
{
   DIR *dir;
   struct dirent *dirent;
   char full_name[256];
   int i;
   char filename[256];
   int found, count;
   struct name_list *temp_list, *new_entry;


   if (dir_list == NULL) {
      i = found = count = 0;
      sprintf(filename, "%s/%s/%s/", BASE_DIR, "share", EPN);
      jp_logf(JP_LOG_DEBUG, "opening dir %s\n", filename);
      dir = opendir(filename);
      if (dir) {
	 for(i=0; (dirent = readdir(dir)); i++) {
	    sprintf(filename, "%s%s", EPN, "rc");
	    if (strncmp(filename, dirent->d_name, strlen(filename))) {
	       continue;
	    } else {
	       jp_logf(JP_LOG_DEBUG, "found %s\n", dirent->d_name);
	       new_entry = malloc(sizeof(struct name_list));
	       if (!new_entry) {
		  jp_logf(JP_LOG_FATAL, "get_rcfile_name(): Out of memory\n");
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
      jp_logf(JP_LOG_DEBUG, "opening dir %s\n", full_name);
      dir = opendir(full_name);
      if (dir) {
	 for(; (dirent = readdir(dir)); i++) {
	    sprintf(filename, "%s%s", EPN, "rc");
	    if (strncmp(filename, dirent->d_name, strlen(filename))) {
	       continue;
	    } else {
	       jp_logf(JP_LOG_DEBUG, "found %s\n", dirent->d_name);
	       new_entry = malloc(sizeof(struct name_list));
	       if (!new_entry) {
		  jp_logf(JP_LOG_FATAL, "get_rcfile_name(): Out of memory 2\n");
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

#define NUM_SHORTDATES 7
#define NUM_LONGDATES 6
#define NUM_TIMES 10
#define NUM_RATES 11
#define NUM_PAPER_SIZES 2

/*if n is out of range then this function will fail */
int get_pref_possibility(int which, int n, char *pref_str)
{
   const char *short_date_formats[] = {
      "%m/%d/%y",
      "%d/%m/%y",
      "%d.%m.%y",
      "%d-%m-%y",
      "%y/%m/%d",
      "%y.%m.%d",
      "%y-%m-%d"
   };

   const char *long_date_formats[] = {
      "%B %d, %Y",
      "%d %B %Y",
      "%d. %B %Y",
      "%d %B, %Y",
      "%Y. %B. %d",
      "%Y %B %d"
   };

   const char *time_formats[] = {
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

   static char *days[2];

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
      "H230400",
      "H460800"
   };

   static const char *char_sets[] = {
      "Latin1",
      "Japanese",
      "Host ISO-8859-2 <-> Palm Windows1250 (EE)", 
      "Host Windows1251 <-> Palm KOI8-R",
      "Host KOI8-R <-> Palm Windows-1251",
      "Chinese(Big5)",
      "Korean",
      "Host UTF-8 <-> Palm Windows1250 (EE)"
   };

   static const char *paper_sizes[] = {
      "US Letter",
      "A4"
   };

   days[0] = _("Sunday");
   days[1] = _("Monday");

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

    case PREF_CHAR_SET:
      if ((n >= NUM_CHAR_SETS) || (n<0)) {
	 pref_str[0]='\0';
	 return -1;
      }
      strcpy(pref_str, char_sets[n]);
      break;

    case PREF_PAPER_SIZE:
      if ((n >= NUM_PAPER_SIZES) || (n<0)) {
	 pref_str[0]='\0';
	 return -1;
      }
      strcpy(pref_str, paper_sizes[n]);
      break;

    default:
      pref_str[0]='\0';
      jp_logf(JP_LOG_DEBUG, "Unknown preference type\n");
      return -1;
   }

   return 0;
}

/*
 * WARNING
 * Caller must ensure that which is not out of range!
 */
int jp_get_pref(prefType prefs[], int which, long *n, const char **string)
{
   if (which < 0) {
      return -1;
   }
   if (n) {
      *n = prefs[which].ivalue;
   }
   if (prefs[which].usertype == CHARTYPE) {
      if (string!=NULL) {
	 *string = prefs[which].svalue;
      }
   } else {
      if (string!=NULL) {
	 *string = NULL;
      }
   }
   return 0;
}

int get_pref(int which, long *n, const char **string)
{
   if (which > NUM_PREFS) {
      return -1;
   }
   return jp_get_pref(glob_prefs, which, n, string);
}

/*
 * Get the preference value as integer. If failed to do so, return the
 * specified default (defval).
 */
long get_pref_int_default(int which, long defval)
{
   long val;

   if (get_pref(which, &val, NULL) == 0) {
      return val;
   }
   else {
      return defval;
   }
}

/*
 * Treats src==NULL as ""
 * Writes NULL at end of string
 */
char *pref_lstrncpy_realloc(char **dest, const char *src, int *size, int max_size)
{
   int new_size, len;
   const char null_str[]="";
   const char *Psrc;

   if (!src) {
      Psrc=null_str;
   } else {
      Psrc=src;
   }
   len=strlen(Psrc)+1;
   new_size=*size;
   if (len > *size) {
      new_size=len;
   }
   if (new_size > max_size) new_size=max_size;

   if (new_size > *size) {
      if (*size == 0) {
	 *dest=malloc(new_size);
      } else {
	 *dest=realloc(*dest, new_size);
      }
      if (!(*dest)) {
	 return 0;
      }
      *size=new_size;
   }
   strncpy(*dest, Psrc, new_size);
   (*dest)[new_size-1]='\0';

   return *dest;
}

int jp_set_pref(prefType prefs[], int which, long n, const char *string)
{
   const char null_str[]="";
   const char *Pstr;

   if (which < 0) {
      return -1;
   }
   prefs[which].ivalue = n;
   if (string == NULL) {
      Pstr=null_str;
   } else {
      Pstr=string;
   }
   if (prefs[which].usertype == CHARTYPE) {
      pref_lstrncpy_realloc(&(prefs[which].svalue), Pstr,
			    &(prefs[which].svalue_size), MAX_PREF_VALUE);
   }
   return 0;
}

int set_pref(int which, long n, const char *string, int save)
{
   const char *str;
   int r;

   if (which > NUM_PREFS) {
      return -1;
   }
   str=string;
   if ((which==PREF_RCFILE) ||
       (which==PREF_SHORTDATE) ||
       (which==PREF_LONGDATE) ||
       (which==PREF_TIME) ||
       (which==PREF_PAPER_SIZE)) {
      set_pref_possibility(which, n, FALSE);
      str=glob_prefs[which].svalue;
   }
   r = jp_set_pref(glob_prefs, which, n, str);
   /* #ifdef ENABLE_PROMETHEON */
   /* Some people like to just kill the window manager */
   /* Prometheon kills us, always be prepared for death */
   if (save) {
      pref_write_rc_file();
   }
   /* #endif */
   return r;
}

int set_pref_possibility(int which, long n, int save)
{
   char svalue[MAX_PREF_VALUE];
   char *str=NULL;
   int r;

   if (which > NUM_PREFS) {
      return -1;
   }
   if (glob_prefs[which].usertype == CHARTYPE) {
      get_pref_possibility(which, n, svalue);
      str=svalue;
   }
   r = jp_set_pref(glob_prefs, which, n, str);
   /* #ifdef ENABLE_PROMETHEON */
   /* Some people like to just kill the window manager */
   /* Prometheon kills us, always be prepared for death */
   if (save) {
      pref_write_rc_file();
   }
   /* #endif */
   return r;
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

   if (glob_prefs[PREF_CHAR_SET].ivalue >= NUM_CHAR_SETS) {
      glob_prefs[PREF_CHAR_SET].ivalue = NUM_CHAR_SETS - 1;
   }
   if (glob_prefs[PREF_CHAR_SET].ivalue < 0) {
      glob_prefs[PREF_CHAR_SET].ivalue = 0;
   }

   if (glob_prefs[PREF_PAPER_SIZE].ivalue >= NUM_PAPER_SIZES) {
      glob_prefs[PREF_PAPER_SIZE].ivalue = NUM_PAPER_SIZES - 1;
   }
   if (glob_prefs[PREF_PAPER_SIZE].ivalue < 0) {
      glob_prefs[PREF_PAPER_SIZE].ivalue = 0;
   }

   if (glob_prefs[PREF_NUM_BACKUPS].ivalue >= MAX_PREF_NUM_BACKUPS) {
      glob_prefs[PREF_NUM_BACKUPS].ivalue = MAX_PREF_NUM_BACKUPS;
   }
   if (glob_prefs[PREF_NUM_BACKUPS].ivalue < 1) {
      glob_prefs[PREF_NUM_BACKUPS].ivalue = 1;
   }

   get_pref_possibility(PREF_TIME, glob_prefs[PREF_TIME].ivalue, svalue);
   pref_lstrncpy_realloc(&(glob_prefs[PREF_TIME].svalue), svalue,
			 &(glob_prefs[PREF_TIME].svalue_size), MAX_PREF_VALUE);

   get_pref_possibility(PREF_SHORTDATE, glob_prefs[PREF_SHORTDATE].ivalue, svalue);
   pref_lstrncpy_realloc(&(glob_prefs[PREF_SHORTDATE].svalue), svalue,
			 &(glob_prefs[PREF_SHORTDATE].svalue_size), MAX_PREF_VALUE);


   get_pref_possibility(PREF_LONGDATE, glob_prefs[PREF_LONGDATE].ivalue, svalue);
   pref_lstrncpy_realloc(&(glob_prefs[PREF_LONGDATE].svalue), svalue,
			 &(glob_prefs[PREF_LONGDATE].svalue_size), MAX_PREF_VALUE);

   get_pref_possibility(PREF_FDOW, glob_prefs[PREF_FDOW].ivalue, svalue);
   pref_lstrncpy_realloc(&(glob_prefs[PREF_FDOW].svalue), svalue,
			 &(glob_prefs[PREF_FDOW].svalue_size), MAX_PREF_VALUE);

   get_pref_possibility(PREF_RATE, glob_prefs[PREF_RATE].ivalue, svalue);
   pref_lstrncpy_realloc(&(glob_prefs[PREF_RATE].svalue), svalue,
			 &(glob_prefs[PREF_RATE].svalue_size), MAX_PREF_VALUE);

   get_pref_possibility(PREF_PAPER_SIZE, glob_prefs[PREF_PAPER_SIZE].ivalue, svalue);
   pref_lstrncpy_realloc(&(glob_prefs[PREF_PAPER_SIZE].svalue), svalue,
			 &(glob_prefs[PREF_PAPER_SIZE].svalue_size), MAX_PREF_VALUE);

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

int jp_pref_read_rc_file(char *filename, prefType prefs[], int num_prefs)
{
   int i;
   FILE *in;
   char line[1024];
   char *field1, *field2;
   char *Pc;

   in=jp_open_home_file(filename, "r");
   if (!in) {
      return -1;
   }

   while (!feof(in)) {
      fgets(line, 1024, in);
      if (feof(in)) break;
      line[1023] = ' ';
      line[1023] = '\0';
      field1 = strtok(line, " ");
      field2 = (field1 != NULL)	? strtok(NULL, "\n") : NULL;/* jonh */
      if ((field1 == NULL) || (field2 == NULL)) {
	 continue;
      }
      if ((Pc = (char *)index(field2, '\n'))) {
	 Pc[0]='\0';
      }
      for(i=0; i<num_prefs; i++) {
	 if (!strcmp(prefs[i].name, field1)) {
	    if (prefs[i].filetype == INTTYPE) {
	       prefs[i].ivalue = atoi(field2);
	    }
	    if (prefs[i].filetype == CHARTYPE) {
	       if (pref_lstrncpy_realloc(&(prefs[i].svalue), field2,
					&(prefs[i].svalue_size),
					MAX_PREF_VALUE)==NULL) {
		  jp_logf(JP_LOG_WARN, "Out of memory: read_rc_file()\n");
		  continue;
	       }
	    }
	 }
      }
   }
   fclose(in);

   return 0;
}

int pref_read_rc_file()
{
   int r;

   r=jp_pref_read_rc_file(EPN".rc", glob_prefs, NUM_PREFS);

   validate_glob_prefs();

   return r;
}

int jp_pref_write_rc_file(char *filename, prefType prefs[], int num_prefs)
{
   int i;
   FILE *out;

   jp_logf(JP_LOG_DEBUG, "jp_pref_write_rc_file()\n");

   out=jp_open_home_file(filename,"w" );
   if (!out) {
      return -1;
   }

   for(i=0; i<num_prefs; i++) {

      if (prefs[i].filetype == INTTYPE) {
	 fprintf(out, "%s %ld\n", prefs[i].name, prefs[i].ivalue);
      }

      if (prefs[i].filetype == CHARTYPE) {
	 fprintf(out, "%s %s\n", prefs[i].name, prefs[i].svalue);
      }
   }
   fclose(out);

   return 0;
}

int pref_write_rc_file()
{
   return jp_pref_write_rc_file(EPN".rc", glob_prefs, NUM_PREFS);
}

/*
 * utils.c
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
#include <sys/stat.h>
#include <unistd.h>
#include "utils.h"
#include <pi-datebook.h>
#include <pi-address.h>
//#include <sys/types.h>
//#include <unistd.h>
#include <utime.h>

#include <pi-source.h>
#include <pi-socket.h>
#include <pi-dlp.h>
#include <pi-file.h>


gint timeout_date(gpointer data)
{
   extern GtkWidget *glob_date_label;
   char str[50];
   time_t ltime;
   struct tm *now;

   if (glob_date_label==NULL) {
      return FALSE;
   }
   time(&ltime);
   now = localtime(&ltime);
   strftime(str, 50, "Today is %A, %x %X", now);
   
   gtk_label_set_text(GTK_LABEL(glob_date_label), str);
   return TRUE;
}

int get_pixmaps(GtkWidget *widget,
		GdkPixmap **out_note, GdkPixmap **out_alarm,
		GdkPixmap **out_check, GdkPixmap **out_checked,
		GdkBitmap **out_mask_note, GdkBitmap **out_mask_alarm,
		GdkBitmap **out_mask_check, GdkBitmap **out_mask_checked)
{
//Note pixmap
char * xpm_note[] = {
   "16 16 3 1",
     "       c None",
     ".      c #000000000000",
//     "X      c #FFFFFFFFFFFF",
     "X      c #cccccccccccc",
     "                ",
     "   ......       ",
     "   .XXX.X.      ",
     "   .XXX.XX.     ",
     "   .XXX.XXX.    ",
     "   .XXX.....    ",
     "   .XXXXXXX.    ",
     "   .XXXXXXX.    ",
     "   .XXXXXXX.    ",
     "   .XXXXXXX.    ",
     "   .XXXXXXX.    ",
     "   .XXXXXXX.    ",
     "   .XXXXXXX.    ",
     "   .........    ",
     "                ",
     "                "
};

//Alarm pixmap
const char * xpm_alarm[] = {
   "16 16 3 1",
     "       c None",
     ".      c #000000000000",
//     "X      c #FFFFFFFFFFFF",
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

const char * xpm_check[] = {
   "16 16 3 1",
     "       c None",
     ".      c #000000000000",
//     "X      c #FFFFFFFFFFFF",
     "X      c #cccccccccccc",
     "                ",
     "   .........    ",
     "   .XXXXXXX.    ",
     "   .X     X.    ",
     "   .X     X.    ",
     "   .X     X.    ",
     "   .X     X.    ",
     "   .X     X.    ",
     "   .X     X.    ",
     "   .X     X.    ",
     "   .X     X.    ",
     "   .X     X.    ",
     "   .XXXXXXX.    ",
     "   .........    ",
     "                ",
     "                "
};

const char * xpm_checked[] = {
   "16 16 4 1",
     "       c None",
     ".      c #000000000000",
     "X      c #cccccccccccc",
     "R      c #FFFF00000000",
     "                ",
     "   .........    ",
     "   .XXXXXXX.RR  ",
     "   .X     XRR   ",
     "   .X     RR    ",
     "   .X    RR.    ",
     "   .X    RR.    ",
     "   .X   RRX.    ",
     "   RR  RR X.    ",
     "   .RR RR X.    ",
     "   .X RR  X.    ",
     "   .X  R  X.    ",
     "   .XXXXXXX.    ",
     "   .........    ",
     "                ",
     "                "
};

   static int inited=0;
   static GdkPixmap *pixmap_note;
   static GdkPixmap *pixmap_alarm;
   static GdkPixmap *pixmap_check;
   static GdkPixmap *pixmap_checked;
   static GdkBitmap *mask_note;
   static GdkBitmap *mask_alarm;
   static GdkBitmap *mask_check;
   static GdkBitmap *mask_checked;
   GtkWidget *pixmapwid_note;
   GtkWidget *pixmapwid_alarm;
   GtkWidget *pixmapwid_check;
   GtkWidget *pixmapwid_checked;
   GtkStyle *style;
   
   if (inited) {
      *out_note = pixmap_note;
      *out_alarm = pixmap_alarm;
      *out_check = pixmap_check;
      *out_checked = pixmap_checked;
      *out_mask_note = mask_note;
      *out_mask_alarm = mask_alarm;
      *out_mask_check = mask_check;
      *out_mask_checked = mask_checked;
      return;
   }
   
   inited=1;

   //Make the note pixmap
   //style = gtk_widget_get_style(window);
   style = gtk_widget_get_style(widget);
   pixmap_note = gdk_pixmap_create_from_xpm_d(widget->window,  &mask_note,
					      &style->bg[GTK_STATE_NORMAL],
					      (gchar **)xpm_note);
   pixmapwid_note = gtk_pixmap_new(pixmap_note, mask_note);
   gtk_widget_show(pixmapwid_note);

   //Make the alarm pixmap
   pixmap_alarm = gdk_pixmap_create_from_xpm_d(widget->window,  &mask_alarm,
					       &style->bg[GTK_STATE_NORMAL],
					       (gchar **)xpm_alarm);
   pixmapwid_alarm = gtk_pixmap_new(pixmap_alarm, mask_alarm);
   gtk_widget_show(pixmapwid_alarm);

   //Make the check pixmap
   pixmap_check = gdk_pixmap_create_from_xpm_d(widget->window,  &mask_check,
					       &style->bg[GTK_STATE_NORMAL],
					       (gchar **)xpm_check);
   pixmapwid_check = gtk_pixmap_new(pixmap_check, mask_check);
   gtk_widget_show(pixmapwid_check);

   //Make the checked pixmap
   pixmap_checked = gdk_pixmap_create_from_xpm_d(widget->window,  &mask_checked,
					       &style->bg[GTK_STATE_NORMAL],
					       (gchar **)xpm_checked);
   pixmapwid_checked = gtk_pixmap_new(pixmap_checked, mask_checked);
   gtk_widget_show(pixmapwid_checked);

   *out_note = pixmap_note;
   *out_alarm = pixmap_alarm;
   *out_check = pixmap_check;
   *out_checked = pixmap_checked;
   *out_mask_note = mask_note;
   *out_mask_alarm = mask_alarm;
   *out_mask_check = mask_check;
   *out_mask_checked = mask_checked;
}


//creates the full path name of a file in the ~/.jpilot dir
int get_home_file_name(char *file, char *full_name, int max_size)
{
   char *home, default_path[]=".";

   home = getenv("HOME");
   if (!home) {//Not home;
      printf("Can't get HOME environment variable\n");
   }
   if (strlen(home)>(max_size-strlen(file)-2)) {
      printf("Your HOME environment variable is too long for me\n");
      home=default_path;
   }
   sprintf(full_name, "%s/.jpilot/%s", home, file);
   return 0;
}


//
//Returns 0 if ok
//
int check_hidden_dir()
{
   struct stat statb;
   char hidden_dir[260];
   char test_file[260];
   FILE *out;
   
   get_home_file_name("", hidden_dir, 256);
   hidden_dir[strlen(hidden_dir)-1]='\0';
   //printf("home name = %s\n", hidden_dir);

   if (stat(hidden_dir, &statb)) {
      //directory isn\'t there, create it
      if (mkdir(hidden_dir, 0777)) {
	 //Can\'t create directory
	 printf("Can't create directory %s\n", hidden_dir);
	 return 1;
      }
      if (stat(hidden_dir, &statb)) {
	 printf("Can't create directory %s\n", hidden_dir);
	 return 1;
      }
   }
   //Is it a directory?
   if (!S_ISDIR(statb.st_mode)) {
      printf("%s doesn't appear to be a directory.\n"
	     "I need it to be.\n", hidden_dir);
      return 1;
   }
   //Can we write in it?
   get_home_file_name("test", test_file, 256);
   out = fopen(test_file, "w+");
   if (!out) {
      printf("I can't write files in directory %s\n", hidden_dir);
   } else {
      fclose(out);
      unlink(test_file);
   }
   
   return 0;
}

//
// month = 0-11
// day = day of month 1-31
// dow = day of week for first day of the month 0-6
// ndim = number of days in month 28-31
//
void get_month_info(int month, int day, int year, int *dow, int *ndim)
{
   time_t ltime, t;
   struct tm *now;
   struct tm new_time;
   int days_in_month[]={31,28,31,30,31,30,31,31,30,31,30,31
   };
   
   time( &ltime );
   now = localtime( &ltime );
   
   new_time.tm_sec=0;
   new_time.tm_min=0;
   new_time.tm_hour=11;
   new_time.tm_mday=day; //day of month 1-31
   new_time.tm_mon=month;
   new_time.tm_year=year;
   new_time.tm_isdst=now->tm_isdst;
   
   t = mktime(&new_time);
   *dow = new_time.tm_wday;
   
   //I know this isn't 100% correct
   if (month == 1) {
      if (year%4 == 0) {
	 days_in_month[1]++;
      }
   }
   *ndim = days_in_month[month];
}

void get_this_month_info(int *dow, int *ndim)
{
   time_t ltime;
   struct tm *now;
   
   time( &ltime );
   now = localtime( &ltime );
   
   get_month_info(now->tm_mon, now->tm_mday, now->tm_year, dow, ndim);
}

int get_next_unique_pc_id(unsigned int *next_unique_id)
{
   //PCFileHeader   file_header;
   FILE *pc_in_out;
   char file_name[256];

   pc_in_out = open_file("next_id", "a+");
   if (pc_in_out==NULL) {
      printf("Error opening %s\n",file_name);
      return -1;
   }

   if (ftell(pc_in_out)==0) {
      //We have to write out the file header
      //printf("writing pc header\n");
      *next_unique_id=1;
      if (fwrite(next_unique_id, sizeof(*next_unique_id), 1, pc_in_out) != 1) {
	 printf("Error writing pc header to file: next_id\n");
	 fclose(pc_in_out);
	 return 0;
      }
      fflush(pc_in_out);
   }
   fclose(pc_in_out);
   pc_in_out = open_file("next_id", "r+");
   if (pc_in_out==NULL) {
      printf("Error opening %s\n",file_name);
      return -1;
   }
   fread(next_unique_id, sizeof(*next_unique_id), 1, pc_in_out);
   (*next_unique_id)++;
   if (fseek(pc_in_out, 0, SEEK_SET)) {
      printf("fseek failed\n");
   }
   //rewind(pc_in_out);
   //todo - if > 16777216 then cleanup (thats a lot of records!)
   if (fwrite(next_unique_id, sizeof(*next_unique_id), 1, pc_in_out) != 1) {
      printf("Error writing pc header to file: next_id\n");
   }
   fflush(pc_in_out);
   fclose(pc_in_out);
}
   
int read_rc_file()
{
   char *home, default_path[]=".";
   char file_name[256];

   home = getenv("HOME");
   if (!home) {//Not home;
      printf("Can't get HOME environment variable\n");
   }
   if (strlen(home) > 256-20) {
      printf("Your HOME environment variable is too long for me\n");
      home=default_path;
   }
   sprintf(file_name, "%s/.jpilot/jpilotrc", home);
   
   gtk_rc_parse(file_name);
}

FILE *open_file(char *filename, char *mode)
{
   char *home, default_path[]=".";
   char file_name[256];
   FILE *pc_in;

   home = getenv("HOME");
   if (!home) {//Not home;
      printf("Can't get HOME environment variable\n");
   }
   if (strlen(home) > 256-10-strlen(filename)) {
      printf("Your HOME environment variable is too long for me\n");
      home=default_path;
   }
   sprintf(file_name, "%s/.jpilot/%s", home, filename);
   
   pc_in = fopen(file_name, mode);
   if (pc_in == NULL) {
      pc_in = fopen(file_name, "w+");
      fclose(pc_in);
      pc_in = fopen(file_name, mode);
   }
   return pc_in;
}

int unlink_file(char *filename)
{
   char *home, default_path[]=".";
   char file_name[256];

   home = getenv("HOME");
   if (!home) {//Not home;
      printf("Can't get HOME environment variable\n");
   }
   if (strlen(home) > 256-10-strlen(filename)) {
      printf("Your HOME environment variable is too long for me\n");
      home=default_path;
   }
   sprintf(file_name, "%s/.jpilot/%s", home, filename);
   
   return unlink(file_name);
}

//These next 2 functions were copied from pi-file.c in the pilot-link app
// Exact value of "Jan 1, 1970 0:00:00 GMT" - "Jan 1, 1904 0:00:00 GMT"
#define PILOT_TIME_DELTA (unsigned)(2082844800)
 
time_t
pilot_time_to_unix_time (unsigned long raw_time)
{
   return (time_t)(raw_time - PILOT_TIME_DELTA);
}

unsigned long
unix_time_to_pilot_time (time_t t)
{
   return (unsigned long)((unsigned long)t + PILOT_TIME_DELTA);
}

unsigned int bytes_to_bin(unsigned char *bytes, unsigned int num_bytes)
{
   unsigned int i, n;
   n=0;
   for (i=0;i<num_bytes;i++) {
      n = n*256+bytes[i];
   }
   return n;
}

int raw_header_to_header(RawDBHeader *rdbh, DBHeader *dbh)
{
   unsigned long temp;
   
   strncpy(dbh->db_name, rdbh->db_name, 31);
   dbh->db_name[31] = '\0';
   dbh->flags = bytes_to_bin(rdbh->flags, 2);
   dbh->version = bytes_to_bin(rdbh->version, 2);
   temp = bytes_to_bin(rdbh->creation_time, 4);
   dbh->creation_time = pilot_time_to_unix_time(temp);
   temp = bytes_to_bin(rdbh->modification_time, 4);
   dbh->modification_time = pilot_time_to_unix_time(temp);
   temp = bytes_to_bin(rdbh->backup_time, 4);
   dbh->backup_time = pilot_time_to_unix_time(temp);
   dbh->modification_number = bytes_to_bin(rdbh->modification_number, 4);
   dbh->app_info_offset = bytes_to_bin(rdbh->app_info_offset, 4);
   dbh->sort_info_offset = bytes_to_bin(rdbh->sort_info_offset, 4);
   strncpy(dbh->type, rdbh->type, 4);
   dbh->type[4] = '\0';
   strncpy(dbh->creator_id, rdbh->creator_id, 4);
   dbh->creator_id[4] = '\0';
   strncpy(dbh->unique_id_seed, rdbh->unique_id_seed, 4);
   dbh->unique_id_seed[4] = '\0';
   dbh->next_record_list_id = bytes_to_bin(rdbh->next_record_list_id, 4);
   dbh->number_of_records = bytes_to_bin(rdbh->number_of_records, 2);
}

//returns 1 if found
//        0 if eof
int find_next_offset(mem_rec_header *mem_rh, long fpos,
		     unsigned int *next_offset,
		     unsigned char *attrib, unsigned int *unique_id)
{
   mem_rec_header *temp_mem_rh;
   unsigned char found = 0;
   unsigned long found_at;

   found_at=0xFFFFFF;
   for (temp_mem_rh=mem_rh; temp_mem_rh; temp_mem_rh = temp_mem_rh->next) {
      if ((temp_mem_rh->offset > fpos) && (temp_mem_rh->offset < found_at)) {
	 found_at = temp_mem_rh->offset;
	 // *attrib = temp_mem_rh->attrib;
	 // *unique_id = temp_mem_rh->unique_id;
      }
      if ((temp_mem_rh->offset == fpos)) {
	 found = 1;
	 *attrib = temp_mem_rh->attrib;
	 *unique_id = temp_mem_rh->unique_id;
      }
   }
   *next_offset = found_at;
   return found;
}

void free_mem_rec_header(mem_rec_header **mem_rh)
{
   mem_rec_header *h, *next_h;

   for (h=*mem_rh; h; h=next_h) {
      next_h=h->next;
      free(h);
   }
   *mem_rh=NULL;
}


void print_string(char *str, int len)
{
   unsigned char c;
   int i;
   
   for (i=0;i<len;i++) {
      c=str[i];
      if (c < ' ' || c >= 0x7f)
	printf("%x",c);
      else
	putchar(c);
   }
   printf("\n");
}

//
//This deletes an appointment from the appropriate Datafile
//
int delete_pc_record(AppType app_type, void *VP)
{
//   int unique_id;
   FILE *pc_in;
   PCRecordHeader header;
   struct Appointment app;
   MyAppointment *mapp;
   struct Address address;
   MyAddress *maddress;
   struct ToDo todo;
   MyToDo *mtodo;
   struct Memo memo;
   MyMemo *mmemo;
   char filename[30];
   char record[65536];
   PCRecType record_type;
   unsigned int unique_id;

   if (VP==NULL) {
      return;
   }
   
   switch (app_type) {
    case DATEBOOK:
      mapp = (MyAppointment *) VP;
      record_type = mapp->rt;
      unique_id = mapp->unique_id;
      strcpy(filename, "DatebookDB.pc");
      break;
    case ADDRESS:
      maddress = (MyAddress *) VP;
      record_type = maddress->rt;
      unique_id = maddress->unique_id;
      strcpy(filename, "AddressDB.pc");
      break;
    case TODO:
      mtodo = (MyToDo *) VP;
      record_type = mtodo->rt;
      unique_id = mtodo->unique_id;
      strcpy(filename, "ToDoDB.pc");
      break;
    case MEMO:
      mmemo = (MyMemo *) VP;
      record_type = mmemo->rt;
      unique_id = mmemo->unique_id;
      strcpy(filename, "MemoDB.pc");
      break;
    default:
      return;
   }
   
   if (record_type==DELETED_PALM_REC) {
      printf("This record is already deleted.\n");
      printf("It is scheduled to be deleted from the Palm on the next sync.\n");
      return 0;
   }
   //printf("record type %d\n",record_type);
   switch (record_type) {
    case NEW_PC_REC:
      pc_in=open_file(filename, "r+");
      if (pc_in==NULL) {
	 printf("Couldn't open PC records file\n");
	 return -1;
      }
      while(!feof(pc_in)) {
	 fread(&header, sizeof(header), 1, pc_in);
	 if (feof(pc_in)) {
	    printf("couldn't find record to delete\n");
	    return -1;
	 }
	 if (header.unique_id==unique_id) {
	    if (fseek(pc_in, -sizeof(header), SEEK_CUR)) {
	       printf("fseek failed\n");
	    }
	    header.rt=DELETED_PC_REC;
	    fwrite(&header, sizeof(header), 1, pc_in);
	    //printf("record deleted\n");
	    fclose(pc_in);
	    return 0;
	 }
	 if (fseek(pc_in, header.rec_len, SEEK_CUR)) {
	    printf("fseek failed\n");
	 }
      }
      fclose(pc_in);
      return -1;
	 
    case PALM_REC:
      printf("Deleteing Palm ID %d\n",unique_id);
      
      //todo
      //find_palm_appt_by_ID(unique_id, &a);
      pc_in=open_file(filename, "a");
      if (pc_in==NULL) {
	 printf("Couldn't open PC records file\n");
	 return -1;
      }
      header.unique_id=unique_id;
      header.rt=DELETED_PALM_REC;
      switch (app_type) {
       case DATEBOOK:
	 memset(&app, 0, sizeof(app));
	 header.rec_len = pack_Appointment(&app, record, 65535);
	 if (!header.rec_len) {
	    PRINT_FILE_LINE;
	    printf("pack_Appointment error\n");
	 }
	 break;
       case ADDRESS:
	 memset(&address, 0, sizeof(address));
	 header.rec_len = pack_Address(&address, record, 65535);
	 if (!header.rec_len) {
	    PRINT_FILE_LINE;
	    printf("pack_Address error\n");
	 }
	 break;
       case TODO:
	 memset(&todo, 0, sizeof(todo));
	 header.rec_len = pack_ToDo(&todo, record, 65535);
	 if (!header.rec_len) {
	    PRINT_FILE_LINE;
	    printf("pack_Address error\n");
	 }
	 break;
       case MEMO:
	 memset(&memo, 0, sizeof(memo));
	 header.rec_len = pack_Memo(&memo, record, 65535);
	 if (!header.rec_len) {
	    PRINT_FILE_LINE;
	    printf("pack_Address error\n");
	 }
	 break;
       default:
	 return;
      }
      //todo check write
      fwrite(&header, sizeof(header), 1, pc_in);
      //todo write the real appointment from palm db
      //Right now I am just writing an empty record
      //This will be used for making sure that the palm record hasn't changed
      //before we delete it
      fwrite(record, header.rec_len, 1, pc_in);
      //printf("record deleted\n");
      fclose(pc_in);
      return 0;
      break;
   }
}


int cleanup_pc_file(AppType app_type)
{
   PCRecordHeader header;
   char filename[30];
   FILE *pc_file;
   int r;
   
   r=0;

   switch (app_type) {
    case DATEBOOK:
      strcpy(filename, "DatebookDB.pc");
      break;
    case ADDRESS:
      strcpy(filename, "AddressDB.pc");
      break;
    case TODO:
      strcpy(filename, "ToDoDB.pc");
      break;
    case MEMO:
      strcpy(filename, "MemoDB.pc");
      break;
    default:
      return;
   }

   pc_file=open_file(filename, "r");
   while(!feof(pc_file)) {
      fread(&header, sizeof(header), 1, pc_file);
      if (feof(pc_file)) {
	 break;
      }
      if (!(header.rt & SPENT_PC_RECORD_BIT)) {
	 r++;
      }
      if (fseek(pc_file, header.rec_len, SEEK_CUR)) {
	 printf("fseek failed\n");
	 return -1;
      }
   }
   
   //If there are no not-deleted records then remove the file
   if (r == 0) {
      unlink_file(filename);
   }
   
   return r;
}

int cleanup_pc_files()
{
   int ret;
   
   ret = cleanup_pc_file(DATEBOOK);
   ret += cleanup_pc_file(ADDRESS);
   ret += cleanup_pc_file(TODO);
   ret += cleanup_pc_file(MEMO);
   if (ret == 0) {
      unlink_file("next_id");
   }
}

void cb_sync(GtkWidget *widget,
	     gpointer  data)
{
   jpilot_sync(NULL);
   //datebook_cleanup();
   //todo - force a refresh of whatever app is running
   //Force a refresh of the calendar
   //if (day_button[current_day-1]) {
   //   gtk_signal_emit_by_name(GTK_OBJECT(day_button[current_day-1]), "clicked");
   //}
}

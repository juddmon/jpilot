/* utils.c
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
#include "utils.h"
#include "log.h"
#include "prefs.h"
#include "sync.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pi-datebook.h>
#include <pi-address.h>
#include <sys/types.h>
#include <sys/wait.h>
/*#include <unistd.h> */
#include <utime.h>
#include <time.h>
#include <errno.h>

#include <pi-source.h>
#include <pi-socket.h>
#include <pi-dlp.h>
#include <pi-file.h>

/*Stuff for the dialog window */
GtkWidget *dialog;
int dialog_result;

unsigned int glob_find_id;
unsigned int glob_find_mon;
unsigned int glob_find_day;
unsigned int glob_find_year;

gint timeout_date(gpointer data)
{
   extern GtkWidget *glob_date_label;
   char str[102];
   char datef[102];
   const char *svalue1, *svalue2;
   int ivalue;
   time_t ltime;
   struct tm *now;

   if (glob_date_label==NULL) {
      return FALSE;
   }
   time(&ltime);
   now = localtime(&ltime);

   
   /*Build a long date string */
   get_pref(PREF_LONGDATE, &ivalue, &svalue1);
   get_pref(PREF_TIME, &ivalue, &svalue2);
   if ((svalue1==NULL)||(svalue2==NULL)) {
      strcpy(datef, "Today is %A, %x %X");
   } else {
      sprintf(datef, "Today is %%A, %s %s", svalue1, svalue2);
   }
   strftime(str, 100, datef, now);
   str[100]='\0';
   
   gtk_label_set_text(GTK_LABEL(glob_date_label), str);
   return TRUE;
}


void free_AnyRecordList(AnyRecordList **rl)
{
   AnyRecordList *temp_rl, *temp_rl_next;

   for (temp_rl = *rl; temp_rl; temp_rl=temp_rl_next) {	   
      switch (temp_rl->app_type) {
       case DATEBOOK:
	 free_Appointment(&(temp_rl->any.mappo.a));
	 break;
       case ADDRESS:
	 free_Address(&(temp_rl->any.maddr.a));
	 break;
       case TODO:
	 free_ToDo(&(temp_rl->any.mtodo.todo));
	 break;
       case MEMO:
	 free_Memo(&(temp_rl->any.mmemo.memo));
	 break;
       default:
	 break;
      }
      temp_rl_next = temp_rl->next;
      free(temp_rl);
   }
   *rl = NULL;
}


gint cb_timer_move_scrolled_window(gpointer data);

struct move_sw {
   float percentage;
   GtkWidget *sw;
};

/* */
/*This function needs to be called when the screen isn't finished */
/*drawing yet.  Moving the scrollbar immediately would have no effect. */
/* */
void move_scrolled_window_hack(GtkWidget *sw, float percentage)
{
   /*This is so that the caller doesn't have to worry about making */
   /*sure they use a static variable (required for callback) */
   static struct move_sw move_this;

   move_this.percentage = percentage;
   move_this.sw = sw;
   
   gtk_timeout_add(50, cb_timer_move_scrolled_window, &move_this);
}

gint cb_timer_move_scrolled_window(gpointer data)
{
   struct move_sw *move_this;
   int r;
   
   move_this = data;
   r = move_scrolled_window(move_this->sw, move_this->percentage);
   /*if we return TRUE then this function will get called again */
   /*if we return FALSE then it will be taken out of timer */
   if (r) {
      return TRUE;
   } else {
      return FALSE;
   }
}

int move_scrolled_window(GtkWidget *sw, float percentage)
{
   GtkScrollbar *sb;
   gfloat upper, lower, page_size, new_val;

   if (!GTK_IS_SCROLLED_WINDOW(sw)) {
      return 0;
   }
   sb = GTK_SCROLLBAR(GTK_SCROLLED_WINDOW(sw)->vscrollbar);
   upper = GTK_ADJUSTMENT(sb->range.adjustment)->upper;
   lower = GTK_ADJUSTMENT(sb->range.adjustment)->lower;
   page_size = GTK_ADJUSTMENT(sb->range.adjustment)->page_size;
   
   /*The screen isn't done drawing yet, so we have to leave. */
   if (page_size == 0) {
      return 1;
   }
   new_val = (upper - lower) * percentage;
   if (new_val > upper - page_size) {
      new_val = upper - page_size;
   }
   gtk_adjustment_set_value(sb->range.adjustment, new_val);
   gtk_signal_emit_by_name(GTK_OBJECT(sb->range.adjustment), "changed");

   return 0;
}

/*returns 0 if not found, 1 if found */
int clist_find_id(GtkWidget *clist,
		  unsigned int unique_id,
		  int *found_at,
		  int *total_count)
{
   int i, found;
   MyAddress *ma;
   
   *found_at = 0;
   *total_count = 0;

   /*100000 is just to prevent ininite looping during a solar flare */
   for (found = i = 0; i<100000; i++) {
      ma = gtk_clist_get_row_data(GTK_CLIST(clist), i);
      if (ma < (MyAddress *)CLIST_MIN_DATA) {
	 break;
      }
      if (found) {
	 continue;
      }
      if (ma->unique_id==unique_id) {
	 found = 1;
	 *found_at = i;
      }
   }
   *total_count = i;
   
   return found;
}

int get_pixmaps(GtkWidget *widget,
		GdkPixmap **out_note, GdkPixmap **out_alarm,
		GdkPixmap **out_check, GdkPixmap **out_checked,
		GdkBitmap **out_mask_note, GdkBitmap **out_mask_alarm,
		GdkBitmap **out_mask_check, GdkBitmap **out_mask_checked)
{
/*Note pixmap */
char * xpm_note[] = {
   "16 16 3 1",
     "       c None",
     ".      c #000000000000",
/*     "X      c #FFFFFFFFFFFF", */
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

/*Alarm pixmap */
char * xpm_alarm[] = {
   "16 16 3 1",
     "       c None",
     ".      c #000000000000",
/*     "X      c #FFFFFFFFFFFF", */
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

char * xpm_check[] = {
   "16 16 3 1",
     "       c None",
     ".      c #000000000000",
/*     "X      c #FFFFFFFFFFFF", */
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

char * xpm_checked[] = {
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
      return 0;
   }
   
   inited=1;

   /*Make the note pixmap */
   /*style = gtk_widget_get_style(window); */
   style = gtk_widget_get_style(widget);
   pixmap_note = gdk_pixmap_create_from_xpm_d(widget->window,  &mask_note,
					      &style->bg[GTK_STATE_NORMAL],
					      (gchar **)xpm_note);
   pixmapwid_note = gtk_pixmap_new(pixmap_note, mask_note);
   gtk_widget_show(pixmapwid_note);

   /*Make the alarm pixmap */
   pixmap_alarm = gdk_pixmap_create_from_xpm_d(widget->window,  &mask_alarm,
					       &style->bg[GTK_STATE_NORMAL],
					       (gchar **)xpm_alarm);
   pixmapwid_alarm = gtk_pixmap_new(pixmap_alarm, mask_alarm);
   gtk_widget_show(pixmapwid_alarm);

   /*Make the check pixmap */
   pixmap_check = gdk_pixmap_create_from_xpm_d(widget->window,  &mask_check,
					       &style->bg[GTK_STATE_NORMAL],
					       (gchar **)xpm_check);
   pixmapwid_check = gtk_pixmap_new(pixmap_check, mask_check);
   gtk_widget_show(pixmapwid_check);

   /*Make the checked pixmap */
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
   
   return 0;
}


/* */
/*Start of Dialog window code */
/* */
gint cb_timer_raise_dialog(gpointer data)
{
   if (GTK_IS_WIDGET(dialog)) {
      gdk_window_raise(dialog->window);
   } else {
      return FALSE;
   }
   return TRUE;
}

void cb_dialog_button_1(GtkWidget *widget,
			gpointer   data)
{
   /*dialog_result=GPOINTER_TO_INT(data); */
   dialog_result=DIALOG_SAID_1;

   gtk_widget_destroy(dialog);
}

void cb_dialog_button_2(GtkWidget *widget,
			gpointer   data)
{
   /*dialog_result=GPOINTER_TO_INT(data); */
   dialog_result=DIALOG_SAID_2;

   gtk_widget_destroy(dialog);
}

void cb_dialog_button_3(GtkWidget *widget,
			gpointer   data)
{
   /*dialog_result=GPOINTER_TO_INT(data); */
   dialog_result=DIALOG_SAID_3;

   gtk_widget_destroy(dialog);
}

static gboolean cb_destroy_dialog(GtkWidget *widget)
{
   dialog = NULL;
   gtk_main_quit();

   return FALSE;
}

/*nob = number of buttons (1-3) */
int dialog_generic(GdkWindow *main_window,
		   int w, int h,
		   char *title, char *frame_text,
		   char *text, int nob, char *button_text[])
{
   GtkWidget *button, *label1;
   gint px, py, pw, ph = 0;
   gint x, y;
   
   GtkWidget *hbox1, *vbox1;
   GtkWidget *frame1;

   gdk_window_get_position(main_window, &px, &py);
   gdk_window_get_size(main_window, &pw, &ph);

   /*Center the window in the main window */
   x=px+pw/2-w/2;
   y=py+ph/2-h/2;
   
   /*dialog=gtk_window_new(GTK_WINDOW_TOPLEVEL); */
   dialog = gtk_widget_new(GTK_TYPE_WINDOW,
			   "type", GTK_WINDOW_DIALOG,
			   "x", x, "y", y,
			   "width", w, "height", h,
			   "title", title,
			   NULL);

   /*gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_MOUSE); */
   
   gtk_signal_connect(GTK_OBJECT(dialog), "destroy",
                      GTK_SIGNAL_FUNC(cb_destroy_dialog), dialog);

   gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
   
   frame1 = gtk_frame_new(frame_text);
   gtk_frame_set_label_align(GTK_FRAME(frame1), 0.5, 0.0);
   vbox1 = gtk_vbox_new(TRUE, 5);
   hbox1 = gtk_hbox_new(TRUE, 5);

   gtk_container_set_border_width(GTK_CONTAINER(frame1), 5);
   gtk_container_set_border_width(GTK_CONTAINER(vbox1), 5);
   gtk_container_set_border_width(GTK_CONTAINER(hbox1), 5);
   
   gtk_container_add(GTK_CONTAINER(dialog), frame1);
   gtk_container_add(GTK_CONTAINER(frame1), vbox1);

   label1 = gtk_label_new(text);
   /*This doesn\'t seem to work... */
   /*gtk_label_set_line_wrap(GTK_LABEL(label1), TRUE); */

   gtk_box_pack_start(GTK_BOX(vbox1), label1, FALSE, FALSE, 2);
   gtk_box_pack_start(GTK_BOX(vbox1), hbox1, TRUE, TRUE, 2);

   if (nob > 0) {
      button = gtk_button_new_with_label(button_text[0]);
      gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
				GTK_SIGNAL_FUNC(cb_dialog_button_1),
				GINT_TO_POINTER(DIALOG_SAID_1));
      gtk_box_pack_start(GTK_BOX(hbox1), button, TRUE, TRUE, 1);
   }

   if (nob > 1) {
      button = gtk_button_new_with_label(button_text[1]);
      gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
				GTK_SIGNAL_FUNC(cb_dialog_button_2),
				GINT_TO_POINTER(DIALOG_SAID_2));
      gtk_box_pack_start(GTK_BOX(hbox1), button, TRUE, TRUE, 1);
   }

   if (nob > 2) {
      button = gtk_button_new_with_label(button_text[2]);
      gtk_signal_connect_object(GTK_OBJECT(button), "clicked",
				GTK_SIGNAL_FUNC(cb_dialog_button_3),
				GINT_TO_POINTER(DIALOG_SAID_3));
      gtk_box_pack_start(GTK_BOX(hbox1), button, TRUE, TRUE, 1);
   }

   gtk_widget_show_all(dialog);

   gtk_timeout_add(1000, cb_timer_raise_dialog, NULL);

   gtk_main();
   
   return dialog_result;
}

/*creates the full path name of a file in the ~/.jpilot dir */
int get_home_file_name(char *file, char *full_name, int max_size)
{
   char *home, default_path[]=".";

   home = getenv("HOME");
   if (!home) {/*Not home; */
      jpilot_logf(LOG_WARN, "Can't get HOME environment variable\n");
   }
   if (strlen(home)>(max_size-strlen(file)-strlen("/.jpilot/")-2)) {
      jpilot_logf(LOG_WARN, "Your HOME environment variable is too long for me\n");
      home=default_path;
   }
   sprintf(full_name, "%s/.jpilot/%s", home, file);
   return 0;
}


/* */
/*Returns 0 if ok */
/* */
int check_hidden_dir()
{
   struct stat statb;
   char hidden_dir[260];
   char test_file[260];
   FILE *out;
   
   get_home_file_name("", hidden_dir, 256);
   hidden_dir[strlen(hidden_dir)-1]='\0';

   if (stat(hidden_dir, &statb)) {
      /*directory isn\'t there, create it */
      if (mkdir(hidden_dir, 0777)) {
	 /*Can\'t create directory */
	 jpilot_logf(LOG_WARN, "Can't create directory %s\n", hidden_dir);
	 return 1;
      }
      if (stat(hidden_dir, &statb)) {
	 jpilot_logf(LOG_WARN, "Can't create directory %s\n", hidden_dir);
	 return 1;
      }
   }
   /*Is it a directory? */
   if (!S_ISDIR(statb.st_mode)) {
      jpilot_logf(LOG_WARN, "%s doesn't appear to be a directory.\n"
		  "I need it to be.\n", hidden_dir);
      return 1;
   }
   /*Can we write in it? */
   get_home_file_name("test", test_file, 256);
   out = fopen(test_file, "w+");
   if (!out) {
      jpilot_logf(LOG_WARN, "I can't write files in directory %s\n", hidden_dir);
   } else {
      fclose(out);
      unlink(test_file);
   }
   
   return 0;
}

/* */
/* month = 0-11 */
/* day = day of month 1-31 */
/* dow = day of week for first day of the month 0-6 */
/* ndim = number of days in month 28-31 */
/* */
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
   new_time.tm_mday=day; /*day of month 1-31 */
   new_time.tm_mon=month;
   new_time.tm_year=year;
   new_time.tm_isdst=now->tm_isdst;
   
   t = mktime(&new_time);
   *dow = new_time.tm_wday;
   
   /*I know this isn't 100% correct */
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
   /*PCFileHeader   file_header; */
   FILE *pc_in_out;
   char file_name[256];

   pc_in_out = open_file("next_id", "a+");
   if (pc_in_out==NULL) {
      jpilot_logf(LOG_WARN, "Error opening %s\n",file_name);
      return -1;
   }

   if (ftell(pc_in_out)==0) {
      /*We have to write out the file header */
      *next_unique_id=1;
      if (fwrite(next_unique_id, sizeof(*next_unique_id), 1, pc_in_out) != 1) {
	 jpilot_logf(LOG_WARN, "Error writing pc header to file: next_id\n");
	 fclose(pc_in_out);
	 return 0;
      }
      fflush(pc_in_out);
   }
   fclose(pc_in_out);
   pc_in_out = open_file("next_id", "r+");
   if (pc_in_out==NULL) {
      jpilot_logf(LOG_WARN, "Error opening %s\n",file_name);
      return -1;
   }
   fread(next_unique_id, sizeof(*next_unique_id), 1, pc_in_out);
   (*next_unique_id)++;
   if (fseek(pc_in_out, 0, SEEK_SET)) {
      jpilot_logf(LOG_WARN, "fseek failed\n");
   }
   /*rewind(pc_in_out); */
   /*todo - if > 16777216 then cleanup (thats a lot of records!) */
   if (fwrite(next_unique_id, sizeof(*next_unique_id), 1, pc_in_out) != 1) {
      jpilot_logf(LOG_WARN, "Error writing pc header to file: next_id\n");
   }
   fflush(pc_in_out);
   fclose(pc_in_out);
   
   return 0;
}
   
int read_gtkrc_file()
{
   char filename[256];
   char fullname[256];
   struct stat buf;
   int ivalue;
   const char *svalue;
   
   get_pref(PREF_RCFILE, &ivalue, &svalue);
   if (svalue) {
     jpilot_logf(LOG_DEBUG, "rc file from prefs is %s\n", svalue);
   } else {
     jpilot_logf(LOG_DEBUG, "rc file from prefs is NULL\n");
   }

   strncpy(filename, svalue, 255);
   filename[255]='\0';
   
   /*Try to read the file out of the home directory first */
   get_home_file_name(filename, fullname, 255);

   if (stat(fullname, &buf)==0) {
      jpilot_logf(LOG_DEBUG, "parsing %s\n", fullname);
      gtk_rc_parse(fullname);
      return 0;
   }
   
   g_snprintf(fullname, 255, "%s/%s/%s/%s", BASE_DIR, "share", EPN, filename);
   fullname[255]='\0';
   if (stat(fullname, &buf)==0) {
      jpilot_logf(LOG_DEBUG, "parsing %s\n", fullname);
      gtk_rc_parse(fullname);
      return 0;
   }
   return -1;
}

FILE *open_file(char *filename, char *mode)
{
   char fullname[256];
   FILE *pc_in;

   get_home_file_name(filename, fullname, 255);
   
   pc_in = fopen(fullname, mode);
   if (pc_in == NULL) {
      pc_in = fopen(fullname, "w+");
      if (pc_in) {
	 fclose(pc_in);
	 pc_in = fopen(fullname, mode);
      }
   }
   return pc_in;
}

int rename_file(char *old_filename, char *new_filename)
{
   char old_fullname[256];
   char new_fullname[256];

   get_home_file_name(old_filename, old_fullname, 255);
   get_home_file_name(new_filename, new_fullname, 255);
   
   return rename(old_fullname, new_fullname);
}


int unlink_file(char *filename)
{
   char fullname[256];

   get_home_file_name(filename, fullname, 255);
   
   return unlink(fullname);
}

/* This function will copy an empty DB file */
/* from the share directory to the users HOME directory */
/* if it doesn't exist already and its length is > 0 */
int check_copy_DBs_to_home()
{
   FILE *in, *out;
   struct stat sbuf;
   int i, c, r;
   char destname[1024];
   char srcname[1024];
   char *dbname[]={
      "DatebookDB.pdb",
	"AddressDB.pdb",
	"ToDoDB.pdb",
	"MemoDB.pdb",
	NULL
   };

   for (i=0; dbname[i]!=NULL; i++) {
      get_home_file_name(dbname[i], destname, 1000);
      r = stat(destname, &sbuf);
      if (((r)&&(errno==ENOENT)) || (sbuf.st_size==0)) {
	 /*The file doesn't exist or is zero in size, copy an empty DB file */
	 if ((strlen(BASE_DIR) + strlen(EPN) + strlen(dbname[i])) > 1000) {
	    jpilot_logf(LOG_DEBUG, "copy_DB_to_home filename too long\n");
	    return -1;
	 }
	 sprintf(srcname, "%s/%s/%s/%s", BASE_DIR, "share", EPN, dbname[i]);
	 in = fopen(srcname, "r");
	 out = fopen(destname, "w");
	 if (!in) {
	    jpilot_logf(LOG_WARN, "Couldn't find empty DB file.\n");
	    jpilot_logf(LOG_WARN, "jpilot may not be installed.\n");
	    return -1;
	 }
	 if (!out) {
	    fclose(in);
	    return -1;
	 }
	 while (!feof(in)) {
	    c = fgetc(in);
	    fputc(c, out);
	 }
	 fclose(in);
	 fclose(out);
      }
   }
   return 0;
}


/*These next 2 functions were copied from pi-file.c in the pilot-link app */
/* Exact value of "Jan 1, 1970 0:00:00 GMT" - "Jan 1, 1904 0:00:00 GMT" */
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
   
   return 0;
}

/*returns 1 if found */
/*        0 if eof */
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
	 /* *attrib = temp_mem_rh->attrib; */
	 /* *unique_id = temp_mem_rh->unique_id; */
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
	jpilot_logf(LOG_STDOUT, "%x",c);
      else
	putchar(c);
   }
   jpilot_logf(LOG_STDOUT, "\n");
}

/* */
/*Warning, this function will move the file pointer */
/* */
int get_app_info_size(FILE *in, int *size)
{
   RawDBHeader rdbh;
   DBHeader dbh;
   unsigned int offset;
   record_header rh;

   fseek(in, 0, SEEK_SET);
   
   fread(&rdbh, sizeof(RawDBHeader), 1, in);
   if (feof(in)) {
      jpilot_logf(LOG_WARN, "Error reading file in get_app_info_size\n");
      return -1;
   }
   
   raw_header_to_header(&rdbh, &dbh);

   if (dbh.app_info_offset==0) {
      *size=0;
      return 0;
   }
   if (dbh.sort_info_offset!=0) {
      *size = dbh.sort_info_offset - dbh.app_info_offset;
      return 0;
   }
   if (dbh.number_of_records==0) {
      fseek(in, 0, SEEK_END);
      *size=ftell(in) - dbh.app_info_offset;
      return 0;
   }

   fread(&rh, sizeof(record_header), 1, in);
   offset = ((rh.Offset[0]*256+rh.Offset[1])*256+rh.Offset[2])*256+rh.Offset[3];
   *size=offset - dbh.app_info_offset;
   
   return 0;
}

/* */
/*This deletes a record from the appropriate Datafile */
/* */
int delete_pc_record(AppType app_type, void *VP, int flag)
{
/*   int unique_id; */
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
      return -1;
   }
   
   mapp=NULL;/*to keep the compiler happy */
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
      return 0;
   }
   
   if ((record_type==DELETED_PALM_REC) || (record_type==MODIFIED_PALM_REC)) {
      jpilot_logf(LOG_INFO, "This record is already deleted.\n"
	   "It is scheduled to be deleted from the Palm on the next sync.\n");
      return 0;
   }
   switch (record_type) {
    case NEW_PC_REC:
      pc_in=open_file(filename, "r+");
      if (pc_in==NULL) {
	 jpilot_logf(LOG_WARN, "Couldn't open PC records file\n");
	 return -1;
      }
      while(!feof(pc_in)) {
	 fread(&header, sizeof(header), 1, pc_in);
	 if (feof(pc_in)) {
	    jpilot_logf(LOG_WARN, "couldn't find record to delete\n");
	    return -1;
	 }
	 if (header.unique_id==unique_id) {
	    if (fseek(pc_in, -sizeof(header), SEEK_CUR)) {
	       jpilot_logf(LOG_WARN, "fseek failed\n");
	    }
	    header.rt=DELETED_PC_REC;
	    fwrite(&header, sizeof(header), 1, pc_in);
	    jpilot_logf(LOG_DEBUG, "record deleted\n");
	    fclose(pc_in);
	    return 0;
	 }
	 if (fseek(pc_in, header.rec_len, SEEK_CUR)) {
	    jpilot_logf(LOG_WARN, "fseek failed\n");
	 }
      }
      fclose(pc_in);
      return -1;
	 
    case PALM_REC:
      jpilot_logf(LOG_DEBUG, "Deleteing Palm ID %d\n",unique_id);
      pc_in=open_file(filename, "a");
      if (pc_in==NULL) {
	 jpilot_logf(LOG_WARN, "Couldn't open PC records file\n");
	 return -1;
      }
      header.unique_id=unique_id;
      if (flag==MODIFY_FLAG) {
	 header.rt=MODIFIED_PALM_REC;
      } else {
	 header.rt=DELETED_PALM_REC;
      }
      switch (app_type) {
       case DATEBOOK:
	 app=mapp->a;
	 /*memset(&app, 0, sizeof(app)); */
	 header.rec_len = pack_Appointment(&app, record, 65535);
	 if (!header.rec_len) {
	    PRINT_FILE_LINE;
	    jpilot_logf(LOG_WARN, "pack_Appointment error\n");
	 }
	 break;
       case ADDRESS:
	 memset(&address, 0, sizeof(address));
	 header.rec_len = pack_Address(&address, record, 65535);
	 if (!header.rec_len) {
	    PRINT_FILE_LINE;
	    jpilot_logf(LOG_WARN, "pack_Address error\n");
	 }
	 break;
       case TODO:
	 memset(&todo, 0, sizeof(todo));
	 header.rec_len = pack_ToDo(&todo, record, 65535);
	 if (!header.rec_len) {
	    PRINT_FILE_LINE;
	    jpilot_logf(LOG_WARN, "pack_ToDo error\n");
	 }
	 break;
       case MEMO:
	 memset(&memo, 0, sizeof(memo));
	 header.rec_len = pack_Memo(&memo, record, 65535);
	 if (!header.rec_len) {
	    PRINT_FILE_LINE;
	    jpilot_logf(LOG_WARN, "pack_Memo error\n");
	 }
	 break;
       default:
	 fclose(pc_in);
	 return 0;
      }
      jpilot_logf(LOG_DEBUG, "writing header to pc file\n");
      fwrite(&header, sizeof(header), 1, pc_in);
      /*todo write the real appointment from palm db */
      /*Right now I am just writing an empty record */
      /*This will be used for making sure that the palm record hasn't changed */
      /*before we delete it */
      jpilot_logf(LOG_DEBUG, "writing record to pc file, %d bytes\n", header.rec_len);
      fwrite(record, header.rec_len, 1, pc_in);
      jpilot_logf(LOG_DEBUG, "record deleted\n");
      fclose(pc_in);
      return 0;
      break;
    default:
      break;
   }
   return 0;
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
      return 0;
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
	 jpilot_logf(LOG_WARN, "fseek failed\n");
	 return -1;
      }
   }

   /*If there are no not-deleted records then remove the file */
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
   return 0;
}

static void util_sync(int full_backup)
{
   int ivalue;
   const char *svalue;
#ifndef HAVE_SETENV
   char str[80];
#endif
   
   get_pref(PREF_RATE, &ivalue, &svalue);
   jpilot_logf(LOG_DEBUG, "setting PILOTRATE=[%s]\n", svalue);
   if (svalue) {
#ifdef HAVE_SETENV
      setenv("PILOTRATE", svalue, TRUE);
#else
      sprintf(str, "PILOTRATE=%s", svalue);
      putenv(str);
#endif
   }

   get_pref(PREF_PORT, &ivalue, &svalue);
   jpilot_logf(LOG_DEBUG, "pref port=[%s]\n", svalue);
   sync_once(svalue, full_backup);
   
   return;
}

void cb_sync(GtkWidget *widget,
	     gpointer   data)
{
   util_sync(FALSE);
   return;
}

void cb_backup(GtkWidget *widget,
	       gpointer   data)
{
   util_sync(TRUE);
   return;
}

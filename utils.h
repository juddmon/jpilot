/* utils.h
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
#ifndef __UTILS_H__
#define __UTILS_H__

#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <pi-datebook.h>
#include <pi-address.h>
#include <pi-todo.h>
#include <pi-memo.h>
#include <gtk/gtk.h>

#define PRINT_FILE_LINE printf("%s line %d\n", __FILE__, __LINE__)
#define PN "J-Pilot"
#define EPN "jpilot"
#define VERSION "0.96"
#define VERSION_STRING "\n"PN" version "VERSION"\n"\
" Copyright (C) 1999 by Judd Montgomery\n"

#define USAGE_STRING "\njpilot [ [-v] || [-h] || [-d]\n"\
" -v displays version and exits.\n"\
" -h displays help and exits.\n"\
" -d displays debug info to stdout.\n"\
" -p do not load plugins.\n"\
" The PILOTPORT, and PILOTRATE env variables are used to specify which\n"\
" port to sync on, and at what speed.\n"\
" If PILOTPORT is not set then it defaults to /dev/pilot.\n"

/*This is how often the clock updates in milliseconds */
#define CLOCK_TICK 1000

#define CATEGORY_ALL 100

#define SHADOW GTK_SHADOW_ETCHED_OUT

/*Used to mark the entry in the clist to add a record */
#define CLIST_NEW_ENTRY_DATA 100
#define CLIST_ADDING_ENTRY_DATA 101
#define CLIST_MIN_DATA 199

/*#define CLIST_DEL_RED 65535; */
/*#define CLIST_DEL_GREEN 55000; */
/*#define CLIST_DEL_BLUE 55000; */
#define CLIST_DEL_RED 0xCCCC;
#define CLIST_DEL_GREEN 0xCCCC;
#define CLIST_DEL_BLUE 0xCCCC;
#define CLIST_NEW_RED 55000;
#define CLIST_NEW_GREEN 55000;
#define CLIST_NEW_BLUE 65535;
#define CLIST_MOD_RED 55000;
#define CLIST_MOD_GREEN 65535;
#define CLIST_MOD_BLUE 65535;

#define SPENT_PC_RECORD_BIT 256

#define CLEAR_FLAG 1
#define CANCEL_FLAG 2
#define DELETE_FLAG 3
#define MODIFY_FLAG 4
#define NEW_FLAG 5
  
#define DIALOG_SAID_1        454
#define DIALOG_SAID_FOURTH   454
#define DIALOG_SAID_CURRENT  454
#define DIALOG_SAID_2        455
#define DIALOG_SAID_LAST     455
#define DIALOG_SAID_ALL      455
#define DIALOG_SAID_3        456
#define DIALOG_SAID_CANCEL   456

extern unsigned int glob_find_id;

typedef enum {
   PALM_REC = 100L,
   MODIFIED_PALM_REC = 101L,
   DELETED_PALM_REC = 102L,
   NEW_PC_REC = 103L,
   DELETED_PC_REC =  SPENT_PC_RECORD_BIT + 104L,
   DELETED_DELETED_PALM_REC =  SPENT_PC_RECORD_BIT + 105L
} PCRecType;

typedef struct {
   unsigned int rec_len;
   unsigned int unique_id;
   PCRecType rt;
   unsigned char attrib;
} PCRecordHeader;

typedef struct {
   unsigned char db_name[32];
   unsigned char flags[2];
   unsigned char version[2];
   unsigned char creation_time[4];
   unsigned char modification_time[4];
   unsigned char backup_time[4];
   unsigned char modification_number[4];
   unsigned char app_info_offset[4];
   unsigned char sort_info_offset[4];
   unsigned char type[4];/*Database ID */
   unsigned char creator_id[4];/*Application ID */
   unsigned char unique_id_seed[4];
   unsigned char next_record_list_id[4];
   unsigned char number_of_records[2];
} RawDBHeader;

typedef struct {
   char db_name[32];
   unsigned int flags;
   unsigned int version;
   time_t creation_time;
   time_t modification_time;
   time_t backup_time;
   unsigned int modification_number;
   unsigned int app_info_offset;
   unsigned int sort_info_offset;
   char type[5];/*Database ID */
   char creator_id[5];/*Application ID */
   char unique_id_seed[5];
   unsigned int next_record_list_id;
   unsigned int number_of_records;
} DBHeader;

typedef struct {
  unsigned char Offset[4];  /*4 bytes offset from BOF to record */
  unsigned char attrib;
  unsigned char unique_ID[3];
} record_header;

typedef struct mem_rec_header_s {
   unsigned int rec_num;
   unsigned int offset;
   unsigned int unique_id;
   unsigned char attrib;
   struct mem_rec_header_s *next;
} mem_rec_header;

typedef enum {
   DATEBOOK = 100L,
   ADDRESS,
   TODO,
   MEMO
} AppType;

typedef struct {
   PCRecType rt;
   unsigned int unique_id;
   unsigned char attrib;
   struct Appointment a;
} MyAppointment;

typedef struct AppointmentList_s {
   AppType app_type;
   struct AppointmentList_s *next;
   MyAppointment ma;
} AppointmentList;

typedef struct {
   PCRecType rt;
   unsigned int unique_id;
   unsigned char attrib;
   struct Address a;
} MyAddress;

typedef struct AddressList_s {
   AppType app_type;
   struct AddressList_s *next;
   MyAddress ma;
} AddressList;

typedef struct {
   PCRecType rt;
   unsigned int unique_id;
   unsigned char attrib;
   struct ToDo todo;
} MyToDo;

typedef struct ToDoList_s {
   AppType app_type;
   struct ToDoList_s *next;
   MyToDo mtodo;
} ToDoList;

typedef struct {
   PCRecType rt;
   unsigned int unique_id;
   unsigned char attrib;
   struct Memo memo;
} MyMemo;

typedef struct MemoList_s {
   AppType app_type;
   struct MemoList_s *next;
   MyMemo mmemo;
} MemoList;

struct search_record
{
   AppType app_type;
   int plugin_flag;
   unsigned int unique_id;
   struct search_record *next;
};

gint timeout_date(gpointer data);
int get_pixmaps(GtkWidget *widget,
		GdkPixmap **out_note, GdkPixmap **out_alarm,
		GdkPixmap **out_check, GdkPixmap **out_checked,
		GdkBitmap **out_mask_note, GdkBitmap **out_mask_alarm,
		GdkBitmap **out_mask_check, GdkBitmap **out_mask_checked);
int check_hidden_dir();
int read_gtkrc_file();
int get_home_file_name(char *file, char *full_name, int max_size);
FILE *open_file(char *filename, char *mode);
int rename_file(char *old_filename, char *new_filename);

int raw_header_to_header(RawDBHeader *rdbh, DBHeader *dbh);
int find_next_offset(mem_rec_header *mem_rh, long fpos,
		     unsigned int *next_offset,
		     unsigned char *attrib, unsigned int *unique_id);
int get_next_unique_pc_id(unsigned int *next_unique_id);
/*The VP is a pointer to MyAddress, MyAppointment, etc. */
int delete_pc_record(AppType app_type, void *VP, int flag);

void get_month_info(int month, int day, int year, int *dow, int *ndim);
void get_this_month_info(int *dow, int *ndim);
time_t pilot_time_to_unix_time (unsigned long raw_time);
unsigned long unix_time_to_pilot_time (time_t t);
unsigned int bytes_to_bin(unsigned char *bytes, unsigned int num_bytes);
void free_mem_rec_header(mem_rec_header **mem_rh);
void print_string(char *str, int len);
/* */
/*Warning, this function will move the file pointer */
/* */
int get_app_info_size(FILE *in, int *size);
int cleanup_pc_files();
void cb_sync(GtkWidget *widget, unsigned int flags);
/*Returns the number of the button that was pressed */
int dialog_generic(GdkWindow *main_window,
		   int w, int h,
		   char *title, char *frame_text,
		   char *text, int nob, char *button_text[]);

int clist_find_id(GtkWidget *clist,
		  unsigned int unique_id,
		  int *found_at,
		  int *total_count);
int clist_count(GtkWidget *clist,
		int *total_count);
int move_scrolled_window(GtkWidget *sw, float percentage);
void move_scrolled_window_hack(GtkWidget *sw, float percentage);

int check_copy_DBs_to_home();

/*search_gui.c */
void cb_search_gui(GtkWidget *widget, gpointer data);

/*install_gui.c */
void cb_install_gui(GtkWidget *widget, gpointer data);

/*weekview_gui.c */
void cb_weekview_gui(GtkWidget *widget, gpointer data);

/*monthview_gui.c */
void cb_monthview_gui(GtkWidget *widget, gpointer data);

void free_search_record_list(struct search_record **sr);


int datebook_gui(GtkWidget *vbox, GtkWidget *hbox);
int address_gui(GtkWidget *vbox, GtkWidget *hbox);
int todo_gui(GtkWidget *vbox, GtkWidget *hbox);
int memo_gui(GtkWidget *vbox, GtkWidget *hbox);

/*from jpilot.c */
void cb_app_button(GtkWidget *widget, gpointer data);
void call_plugin_gui(int number, int unique_id);
/*datebook_gui */
int datebook_refresh(int first);
/*address_gui */
int address_refresh();
/*todo_gui */
int todo_refresh();
/*memo_gui */
int memo_refresh();

#endif

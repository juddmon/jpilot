/* $Id: libplugin.h,v 1.27 2009/05/06 19:29:47 rousseau Exp $ */

/*******************************************************************************
 * libplugin.h
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

#ifndef __LIBPLUGIN_H__
#define __LIBPLUGIN_H__

#include "config.h"
#include <gtk/gtk.h>
#include <time.h>
#include <pi-appinfo.h>

/*
 * PLUGIN API for J-Pilot
 */

#ifdef ENABLE_PROMETHEON
#define PN "CoPilot"
#else
#define PN "J-Pilot"
#endif

#ifdef ENABLE_PROMETHEON
#define EPN "copilot"
#else
#define EPN "jpilot"
#endif

/*
 * For versioning of files
 */
#define FILE_VERSION     "version"
#define FILE_VERSION2    "version2"
#define FILE_VERSION2_CR "version2\n"


typedef struct {
  unsigned char Offset[4];  /*4 bytes offset from BOF to record */
  unsigned char attrib;
  unsigned char unique_ID[3];
} record_header;

typedef struct {
   unsigned long header_len;
   unsigned long header_version;
   unsigned long rec_len;
   unsigned long unique_id;
   unsigned long rt; /* Record Type */
   unsigned char attrib;
} PC3RecordHeader;

typedef struct mem_rec_header_s {
   unsigned int rec_num;
   unsigned int offset;
   unsigned int unique_id;
   unsigned char attrib;
   struct mem_rec_header_s *next;
} mem_rec_header;

typedef struct {
            char db_name[32];
   unsigned char flags[2];
   unsigned char version[2];
   unsigned char creation_time[4];
   unsigned char modification_time[4];
   unsigned char backup_time[4];
   unsigned char modification_number[4];
   unsigned char app_info_offset[4];
   unsigned char sort_info_offset[4];
            char type[4]; /* Database ID */
            char creator_id[4]; /* Application ID */
            char unique_id_seed[4];
   unsigned char next_record_list_id[4];
   unsigned char number_of_records[2];
} RawDBHeader;

#define LEN_RAW_DB_HEADER 78

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

int get_next_unique_pc_id(unsigned int *next_unique_id);

/* used for jp_delete_record */
#define CLEAR_FLAG    1
#define CANCEL_FLAG   2
#define DELETE_FLAG   3
#define MODIFY_FLAG   4
#define NEW_FLAG      5
#define COPY_FLAG     6
#define UNDELETE_FLAG 7

#define CLIST_DEL_RED 0xCCCC
#define CLIST_DEL_GREEN 0xCCCC
#define CLIST_DEL_BLUE 0xCCCC
#define CLIST_NEW_RED 55000
#define CLIST_NEW_GREEN 55000
#define CLIST_NEW_BLUE 65535
#define CLIST_MOD_RED 55000
#define CLIST_MOD_GREEN 65535
#define CLIST_MOD_BLUE 65535
#define CLIST_PRIVATE_RED 60000
#define CLIST_PRIVATE_GREEN 55000
#define CLIST_PRIVATE_BLUE 55000
#define CLIST_OVERDUE_RED 0xD900
#define CLIST_OVERDUE_GREEN 0x0000
#define CLIST_OVERDUE_BLUE 0x0000


#define DIALOG_SAID_1        454
#define DIALOG_SAID_PRINT    454
#define DIALOG_SAID_FOURTH   454
#define DIALOG_SAID_CURRENT  454
#define DIALOG_SAID_2        455
#define DIALOG_SAID_LAST     455
#define DIALOG_SAID_ALL      455
#define DIALOG_SAID_3        456
#define DIALOG_SAID_CANCEL   456

#define JP_LOG_DEBUG  1    /*debugging info for programmers, and bug reports */
#define JP_LOG_INFO   2    /*info, and misc messages */
#define JP_LOG_WARN   4    /*worse messages */
#define JP_LOG_FATAL  8    /*even worse messages */
#define JP_LOG_STDOUT 256  /*messages always go to stdout */
#define JP_LOG_FILE   512  /*messages always go to the log file */
#define JP_LOG_GUI    1024 /*messages always go to the gui window */

#define JPILOT_EOF -7

extern int jp_logf(int level, char *format, ...);

/* This bit means that this record is of no importance anymore */
#define SPENT_PC_RECORD_BIT 256

typedef enum {
   PALM_REC = 100L,
   MODIFIED_PALM_REC = 101L,
   DELETED_PALM_REC = 102L,
   NEW_PC_REC = 103L,
   DELETED_PC_REC =  SPENT_PC_RECORD_BIT | 104L,
   DELETED_DELETED_PALM_REC =  SPENT_PC_RECORD_BIT | 105L,
   REPLACEMENT_PALM_REC = 106L
} PCRecType;

typedef struct
{
   PCRecType rt;
   unsigned int unique_id;
   unsigned char attrib;
   void *buf;
   int size;
} buf_rec;

typedef struct
{
   char *base_dir;
   int *major_version;
   int *minor_version;
} jp_startup_info;

struct search_result
{
   char *line;
   unsigned int unique_id;
   struct search_result *next;
};

int plugin_get_name(char *name, int len);
int plugin_get_menu_name(char *name, int len);
int plugin_get_db_name(char *db_name, int len);
int plugin_startup(jp_startup_info *info);
int plugin_gui(GtkWidget *vbox, GtkWidget *hbox, unsigned int unique_id);
int plugin_help(char **text, int *width, int *height);
int plugin_gui_cleanup(void);
int plugin_pre_sync_pre_connect(void);
int plugin_pre_sync(void);
int plugin_sync(int sd);
int plugin_search(const char *search_string, int case_sense, struct search_result **sr);
int plugin_post_sync(void);
int plugin_exit_cleanup(void);
int plugin_unpack_cai_from_ai(struct CategoryAppInfo *cai,
			      unsigned char *ai_raw, int len);
int plugin_pack_cai_into_ai(struct CategoryAppInfo *cai,
			    unsigned char *ai_raw, int len);
/* callbacks are needed for print */

void jp_init(void);
extern FILE *jp_open_home_file(char *filename, char *mode);

/* This takes the value of $JPILOT_HOME and appends /.jpilot/ and {file}
 * onto it and puts it into full_name.  max_size is the size if the
 * supplied buffer full_name
 */
int jp_get_home_file_name(char *file, char *full_name, int max_size);

/*
 * DB_name should be without filename ext, e.g. MemoDB
 * bufp is the packed app info block
 * size_in is the size of bufp
 */
int jp_pdb_file_write_app_block(char *DB_name, void *bufp, int size_in);

/*
 * widget is a widget inside the main window used to get main window handle
 * db_name should be without filename ext, e.g. MemoDB
 * cai is the category app info.  This should be unpacked by the user since
 * category unpack functions are database specific.
 */
int jp_edit_cats(GtkWidget *widget, char *db_name, struct CategoryAppInfo *cai);

/*************************************
 * convert char code
 *************************************/
extern void jp_charset_j2p(char *buf, int max_len);
extern void jp_charset_p2j(char *buf, int max_len);
extern char* jp_charset_p2newj(const char *buf, int max_len);

/* file must not be open elsewhere when this is called, the first line is 0 */
int jp_install_remove_line(int deleted_line);

int jp_install_append_line(char *line);

/*
 * Get the application info block
 */
int jp_get_app_info(char *DB_name, unsigned char **buf, int *buf_size);
/*
 * Read a pdb file out of the $(JPILOT_HOME || HOME)/.jpilot/ directory
 * It also reads the PC file
 */
int jp_read_DB_files(char *DB_name, GList **records);

/*
 *This deletes a record from the appropriate Datafile
 */
int jp_delete_record(char *DB_name, buf_rec *br, int flag);
/*
 *This undeletes a record from the appropriate Datafile
 */
int jp_undelete_record(char *DB_name, buf_rec *br, int flag);
/*
 * Free the record list
 */
int jp_free_DB_records(GList **records);

int jp_pc_write(char *DB_name, buf_rec *br);

const char *jp_strstr(const char *haystack, const char *needle, int case_sense);

int read_header(FILE *pc_in, PC3RecordHeader *header);

int write_header(FILE *pc_out, PC3RecordHeader *header);

/*
 * These 2 functions don't take full path names.
 * They are relative to $JPILOT_HOME/.jpilot/
 */
int rename_file(char *old_filename, char *new_filename);
int unlink_file(char *filename);

/* */
/*Warning, this function will move the file pointer */
/* */
int get_app_info_size(FILE *in, int *size);

/*
 * Widget must be some widget used to get the main window from.
 * The main window passed in would be fastest.
 * changed is MODIFY_FLAG, or NEW_FLAG
 */
int dialog_save_changed_record(GtkWidget *widget, int changed);

/* mon 0-11
 * day 1-31
 * year (year - 1900)
 * This function will bring up the cal at mon, day, year
 * After a new date is selected it will return mon, day, year
 */
int jp_cal_dialog(GtkWindow *main_window,
		  const char *title, int monday_is_fdow,
		  int *mon, int *day, int *year);

/*
 * The preferences interface makes it easy to read and write name/value pairs
 * to a file.  Also access them efficiently.
 */

#define INTTYPE 1
#define CHARTYPE 2

/* I explain these below */
typedef struct {
   char *name;
   int usertype;
   int filetype;
   long ivalue;
   char *svalue;
   int svalue_size;
} prefType;

/* char *name; */
/*   The name of the preference, will be written to column 1 of the rc file
 *   This needs to be set before reading the rc file.
 */
/* int usertype; */
/*   INTTYPE or CHARTYPE, this is the type of value that the pref is.
 *   This type of value will be returned and set by pref calls.
 */
/* int filetype; */
/*   INTTYPE or CHARTYPE, this is the type of value that the pref is when
 *   it is read from, or written to a file.
 *   i.e., For some of my menus I have file type of int and usertype
 *   of char.  I want to use char, except I don't store the char because
 *   of translations, so I store 3 for the 3rd option.  It also allows
 *   predefined allowed values for strings instead of anything goes. */
/* long ivalue; */
/*   The long value to be returned if of type INT
 */
/* char *svalue; */
/*   The long value to be returned if of type CHAR
 */
/* int svalue_size; */
/*   The size of the memory allocated for the string, Do not change. */

/*
 * To use prefs you must allocate an array of prefType and call this function
 * before any others.
 *  count is how many preferences in the array.
 */
void jp_pref_init(prefType prefs[], int count);
/*
 * This function can be called to free strings allocated by preferences.
 * It should be called in the cleanup routine.
 */
void jp_free_prefs(prefType prefs[], int count);
/*
 * This function retrieves a long value and a pointer to a string of a
 * preference structure.  *string can be passed in as a NULL and NULL can
 * be returned if the preference is of type INT.
 */
int jp_get_pref(prefType prefs[], int which, long *n, const char **string);
/*
 * This function sets a long value and a string of a preference structure.
 *  string can be NULL if the preference is type INT.
 *  string can be any length, memory will be allocated.
 */
int jp_set_pref(prefType prefs[], int which, long n, const char *string);
/*
 * This function reads an rc file and sets the preferences from it.
 */
int jp_pref_read_rc_file(char *filename, prefType prefs[], int num_prefs);
/*
 * This function writes preferences to an rc file.
 */
int jp_pref_write_rc_file(char *filename, prefType prefs[], int num_prefs);
#endif

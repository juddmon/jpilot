/* libplugin.h
 *
 * Copyright (C) 1999 by Judd Montgomery
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
#ifndef __LIBPLUGIN_H__
#define __LIBPLUGIN_H__

#include <gtk/gtk.h>

/*
 * PLUGIN API for J-Pilot
 */

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

int get_next_unique_pc_id(unsigned int *next_unique_id);

/* used for jp_delete_record */
#define CLEAR_FLAG  1
#define CANCEL_FLAG 2
#define DELETE_FLAG 3
#define MODIFY_FLAG 4
#define NEW_FLAG    5

#define CLIST_DEL_RED 0xCCCC;
#define CLIST_DEL_GREEN 0xCCCC;
#define CLIST_DEL_BLUE 0xCCCC;
#define CLIST_NEW_RED 55000;
#define CLIST_NEW_GREEN 55000;
#define CLIST_NEW_BLUE 65535;
#define CLIST_MOD_RED 55000;
#define CLIST_MOD_GREEN 65535;
#define CLIST_MOD_BLUE 65535;

#define LOG_DEBUG  1    /*debugging info for programers, and bug reports */
#define LOG_INFO   2    /*info, and misc messages */
#define LOG_WARN   4    /*worse messages */
#define LOG_FATAL  8    /*even worse messages */
#define LOG_STDOUT 256  /*messages always go to stdout */
#define LOG_FILE   512  /*messages always go to the log file */
#define LOG_GUI    1024 /*messages always go to the gui window */

#define JPILOT_EOF -7

extern int jpilot_logf(int level, char *format, ...);
/* FIXME: Need a policy.  Should all symbols avaliable to 
 * plugins start with jp or jpilot?
 */
#define jp_logf jpilot_logf

#define SPENT_PC_RECORD_BIT 256

typedef enum {
   PALM_REC = 100L,
   MODIFIED_PALM_REC = 101L,
   DELETED_PALM_REC = 102L,
   NEW_PC_REC = 103L,
   DELETED_PC_REC =  SPENT_PC_RECORD_BIT + 104L,
   DELETED_DELETED_PALM_REC =  SPENT_PC_RECORD_BIT + 105L
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

/* void free_buf_rec_list(GList **br_list); */

int plugin_get_name(char *name, int len);
int plugin_get_menu_name(char *name, int len);
int plugin_get_db_name(char *db_name, int len);
int plugin_startup(jp_startup_info *info);
int plugin_gui(GtkWidget *vbox, GtkWidget *hbox, unsigned int unique_id);
int plugin_help(char **text, int *width, int *height);
int plugin_gui_cleanup(void);
int plugin_pre_sync(void);
int plugin_sync(int sd);
int plugin_search(char *search_string, int case_sense, struct search_result **sr);
int plugin_post_sync(void);
int plugin_exit_cleanup(void);
/* callbacks are needed for print */

void jp_init();
extern FILE *jp_open_home_file(char *filename, char *mode);

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

#endif

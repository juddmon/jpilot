/* libplugin.h
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
#include <gtk/gtk.h>

/*
 * PLUGIN API for J-Pilot
 */

#define JPILOT_EOF -7

/* used for jp_delete_record */
#define DELETE_FLAG 3
#define MODIFY_FLAG 4

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

int jpilot_logf(int level, char *format, ...);
int (*jp_logf)(int level, char *format, ...);
/* void plugin_set_jpilot_logf(int (*Pjpilot_logf)(int level, char *format, ...));*/

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
} jp_startup_info;

struct search_result
{
   char *line;
   unsigned int unique_id;
   struct search_result *next;
};

void free_buf_rec_list(GList **br_list);

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

/* */
/*file must not be open elsewhere when this is called */
/*the first line is 0 */
int jp_install_remove_line(int deleted_line);

int jp_install_append_line(char *line);

/*
 * Get the application info block
 */
int jp_get_app_info(char *DB_name, unsigned char **buf, int *buf_size);
/*
 * Read a pdb file out of the $(HOME)/.jpilot/ directory
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

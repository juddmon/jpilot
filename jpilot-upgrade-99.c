/* jpilot-upgrade-99.c
 * A module of J-Pilot http://jpilot.org
 * 
 * Copyright (C) 1999-2001 by Judd Montgomery
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

#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>

#include "utils.h"

#define DEBUG

/* To keep things happy */
GtkWidget *glob_dialog;
GtkWidget *glob_date_label;
int pipe_out;

/* This is the obsolete structure */
typedef struct {
   unsigned int rec_len;
   unsigned int unique_id;
   PCRecType rt;
   unsigned char attrib;
} PCRecordHeader;

/* To keep things happy */
int sync_once(void *vP)
{
   return 0;
}

GList *get_plugin_list()
{
   return NULL;
}

void print_pc_header(PCRecordHeader *h)
{
   printf("rec_len=%d\n", h->rec_len);
   printf("unique_id=%d\n", h->unique_id);
   printf("rt=%d\n", h->rt);
   printf("attrib=%d\n", h->attrib);
}

int convert_pc_file(char *DB_name)
{
   PCRecordHeader header;
   PC3RecordHeader header3;
   char pc_filename[256];
   char pc_filename3[256];
   FILE *pc_file;
   FILE *pc_file3;
   char *record;
   int r;
   int ret;
   int num;
   int max_id;
   int next_id;
   
   r=0;
   max_id = 0;
   next_id = 1;
   record = NULL;
   pc_file = pc_file3 = NULL;

   g_snprintf(pc_filename, 255, "%s.pc", DB_name);
   g_snprintf(pc_filename3, 255, "%s.pc3", DB_name);

   pc_file = jp_open_home_file(pc_filename , "r");
   if (!pc_file) {
      return -1;
   }

   pc_file3=jp_open_home_file(pc_filename3, "w");
   if (!pc_file3) {
      fclose(pc_file);
      return -1;
   }

   while(!feof(pc_file)) {
      fread(&header, sizeof(header), 1, pc_file);
      if (feof(pc_file)) {
	 break;
      }
#ifdef DEBUG
      print_pc_header(&header);
#endif
      if (header.rt & SPENT_PC_RECORD_BIT) {
	 r++;
	 if (fseek(pc_file, header.rec_len, SEEK_CUR)) {
	    printf("fseek failed\n");
	    r = -1;
	    break;
	 }
	 continue;
      } else {
	 if (header.rt == NEW_PC_REC) {
	    header.unique_id = next_id++;
	 }
	 if ((header.unique_id > max_id)
	     && (header.rt != PALM_REC)
	     && (header.rt != MODIFIED_PALM_REC)
	     && (header.rt != DELETED_PALM_REC) ){
	    max_id = header.unique_id;
	 }
	 record = malloc(header.rec_len);
	 if (!record) {
	    printf("jpilot-upgrade-99: Out of memory\n");
	    r = -1;
	    break;
	 }
	 num = fread(record, header.rec_len, 1, pc_file);
	 if (num != 1) {
	    if (ferror(pc_file)) {
	       r = -1;
	       break;
	    }
	 }
	 header3.rec_len=header.rec_len;
	 header3.unique_id=header.unique_id;
	 header3.rt=header.rt;
	 header3.attrib=header.attrib;
	 ret = write_header(pc_file3, &header3);
	 if (ret < 1) {
	    printf("error writing pc3 header\n");
	    r = -1;
	    break;
	 }
	 ret = fwrite(record, header.rec_len, 1, pc_file3);
	 if (ret != 1) {
	    printf("error writing pc3 record\n");
	    r = -1;
	    break;
	 }
	 free(record);
	 record = NULL;
      }
      printf(" Converted a record.\n");
   }

   if (record) {
      free(record);
   }
   if (pc_file) {
      fclose(pc_file);
   }
   if (pc_file3) {
      fclose(pc_file3);
   }

#ifdef DELETE_OLD
   if (r>=0) {
      unlink_file(pc_filename);
   }
#endif
   
   return r;
}

int main()
{
   DIR *dir;
   struct dirent *dirent;
   char home_dir[300];
   char DB_name[300];
   char last4[8];
   int ret;
   char c;

   printf("\n\nThis program is used to upgrade J-Pilot .pc files to .pc3 files\n");
   printf("\nJ-Pilot versions before 0.99 used a .pc file to store data on the desktop.\n");
   printf(" These are not architecture independent.\n");
   printf(" For example they cannot be copied from one architecture to another,\n");
   printf(" or NFS mounted across different architectures.\n");
   printf(" The new .pc3 files used by 0.99 can be.\n");
   printf("If you synced before upgrading and your .pc files are 0 bytes in size\n");
   printf(" then you can delete them and you don't need this program.\n");
   printf("If everything goes well and you are happy with the results\n");
   printf(" then you can delete the .pc files.\n");
   
   printf("\nThis conversion will overwrite current .pc3 files.\n");
   printf("Do you want to go ahead with the conversion? (y/n) >");

   c = getchar();
   
   if ((c!='y') && (c!='Y')) {
      printf("Program aborted\n");
      exit(0);
   }
   
   printf("\n");
   
   get_home_file_name("", home_dir, 255);
   printf("Upgrading files in %s\n", home_dir);

   dir = opendir(home_dir);
   if (dir) {
      while ((dirent = readdir(dir))) {
	 strcpy(last4, dirent->d_name+strlen(dirent->d_name)-3);
	 if ((strcmp(last4, ".pc")==0)) {
	    strncpy(DB_name, dirent->d_name, strlen(dirent->d_name)-3);
	    DB_name[strlen(dirent->d_name)-3]='\0';
	    /* printf("Upgrading %s\n", DB_name);*/
	    printf("Upgrading %s\n", dirent->d_name);
	    ret = convert_pc_file(DB_name);
	    if (ret) {
	       printf(" Failed\n");
	    } else {
	       printf(" Succeeded\n");
	    }
	 }
      }
      closedir(dir);
   }

   return 0;
}

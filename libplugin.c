/* $Id: libplugin.c,v 1.31 2007/10/25 18:53:32 rousseau Exp $ */

/*******************************************************************************
 * libplugin.c
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

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <sys/types.h>
#include <netinet/in.h>

#include <glib.h>

#include "libplugin.h"
#include "i18n.h"

void jp_init()
{
   jp_logf(0, "jp_init()\n");
}

const char *jp_strstr(const char *haystack, const char *needle, int case_sense)
{
   char *needle2;
   char *haystack2;
   register char *Ps2;
   register const char *Ps1;
   char *r;

   if (!haystack) {
      return NULL;
   }
   if (!needle) {
      return haystack;
   }
   if (case_sense) {
      return strstr(haystack, needle);
   } else {
      needle2 = malloc(strlen(needle)+2);
      haystack2 = malloc(strlen(haystack)+2);

      Ps1 = needle;
      Ps2 = needle2;
      while (Ps1[0]) {
	 Ps2[0] = tolower(Ps1[0]);
	 Ps1++;
	 Ps2++;
      }
      Ps2[0]='\0';

      Ps1 = haystack;
      Ps2 = haystack2;
      while (Ps1[0]) {
	 Ps2[0] = tolower(Ps1[0]);
	 Ps1++;
	 Ps2++;
      }
      Ps2[0]='\0';

      r = strstr(haystack2, needle2);
      if (r) {
	 r = (char *)((r-haystack2)+haystack);
      }
      free(needle2);
      free(haystack2);
      return r;
   }
}

void jp_pack_htonl(unsigned char *dest, unsigned long l)
{
    dest[3]=l & 0xFF;
    dest[2]=l>>8 & 0xFF;
    dest[1]=l>>16 & 0xFF;
    dest[0]=l>>24 & 0xFF;
}

void jp_unpack_ntohl(unsigned long *l, unsigned char *src)
{
    *l=src[0]<<24 | src[1]<<16 | src[2]<<8 | src[3];
}

static int pack_header(PC3RecordHeader *header, unsigned char *packed_header)
{
   unsigned char *p;
   unsigned long l;

   l=0;
   p=packed_header;
   /*
    * Header structure:
    * unsigned long header_len;
    * unsigned long header_version;
    * unsigned long rec_len;
    * unsigned long unique_id;
    * unsigned long rt;
    * unsigned char attrib;
    */
    /* 4 + 4 + 4 + 4 + 4 + 1 */
   header->header_len = 21;

   header->header_version = 2;

   jp_pack_htonl(p, header->header_len);
   jp_pack_htonl(p+4, header->header_version);
   jp_pack_htonl(p+8, header->rec_len);
   jp_pack_htonl(p+12, header->unique_id);
   jp_pack_htonl(p+16, header->rt);
   memcpy(p+20, &header->attrib, 1);

   return header->header_len;
}

static int unpack_header(PC3RecordHeader *header, unsigned char *packed_header)
{
   unsigned char *p;

   /*
    * Header structure:
    * unsigned long header_len;
    * unsigned long header_version;
    * unsigned long rec_len;
    * unsigned long unique_id;
    * unsigned long rt;
    * unsigned char attrib;
    */
   p = packed_header;

   jp_unpack_ntohl(&(header->header_len), p);
   jp_unpack_ntohl(&(header->header_version), p+4);

   if (header->header_version > 2) {
      jp_logf(JP_LOG_WARN, _("Unknown PC header version = %d\n"), header->header_version);
   }

   jp_unpack_ntohl(&(header->rec_len), p+8);
   jp_unpack_ntohl(&(header->unique_id), p+12);
   jp_unpack_ntohl(&(header->rt), p+16);
   header->attrib = p[20];

   return EXIT_SUCCESS;
}

/* FIXME: Add jp_ and document. */
int read_header(FILE *pc_in, PC3RecordHeader *header)
{
   unsigned char packed_header[256];
   int num;

   num = fread(packed_header, 4, 1, pc_in);
   if (feof(pc_in)) {
      return JPILOT_EOF;
   }
   if (num!=1) {
      return num;
   }
   jp_unpack_ntohl(&(header->header_len), packed_header);
   if (header->header_len > sizeof(packed_header)-1) {
      jp_logf(JP_LOG_WARN, "read_header() %s\n", _("error"));
      return EXIT_FAILURE;
   }
   num = fread(packed_header+4, (header->header_len)-4, 1, pc_in);
   if (feof(pc_in)) {
      return JPILOT_EOF;
   }
   if (num!=1) {
      return num;
   }
   unpack_header(header, packed_header);
#ifdef DEBUG
   printf("header_len    =%ld\n", header->header_len);
   printf("header_version=%ld\n", header->header_version);
   printf("rec_len       =%ld\n", header->rec_len);
   printf("unique_id     =%ld\n", header->unique_id);
   printf("rt            =%ld\n", header->rt);
   printf("attrib        =%d\n", header->attrib);
#endif
   return 1;
}

/* Add jp_ and document */
int write_header(FILE *pc_out, PC3RecordHeader *header)
{
   unsigned long len;
   unsigned char packed_header[256];

   len = pack_header(header, packed_header);
   if (len>0) {
      fwrite(packed_header, len, 1, pc_out);
   } else {
      jp_logf(JP_LOG_WARN, "%s:%d pack_header returned error\n", __FILE__, __LINE__);
   }

   return len;
}

int get_home_file_name(char *file, char *full_name, int max_size);
int jp_get_home_file_name(char *file, char *full_name, int max_size)
{
   return get_home_file_name(file, full_name, max_size);
}

int pdb_file_write_app_block(char *DB_name, void *bufp, int size_in);
int jp_pdb_file_write_app_block(char *DB_name, void *bufp, int size_in)
{
   return pdb_file_write_app_block(DB_name, bufp, size_in);
}

int edit_cats(GtkWidget *widget, char *db_name, struct CategoryAppInfo *cai);
int jp_edit_cats(GtkWidget *widget, char *db_name, struct CategoryAppInfo *cai)
{
   return edit_cats(widget, db_name, cai);
}

/*
 * file must not be open elsewhere when this is called
 * the first line in file is 0
 */
int jp_install_remove_line(int deleted_line)
{
   FILE *in;
   FILE *out;
   char line[1002];
   char *Pc;
   int r, line_count;

   in = jp_open_home_file(EPN"_to_install", "r");
   if (!in) {
      jp_logf(JP_LOG_DEBUG, "failed opening install_file\n");
      return EXIT_FAILURE;
   }

   out = jp_open_home_file(EPN"_to_install.tmp", "w");
   if (!out) {
      jp_close_home_file(in);
      jp_logf(JP_LOG_DEBUG, "failed opening install_file.tmp\n");
      return EXIT_FAILURE;
   }

   for (line_count=0; (!feof(in)); line_count++) {
      line[0]='\0';
      Pc = fgets(line, sizeof(line), in);
      if (!Pc) {
	 break;
      }
      if (line_count == deleted_line) {
	 continue;
      }
      r = fprintf(out, "%s", line);
      if (r==EOF) {
	 break;
      }
   }
   jp_close_home_file(in);
   jp_close_home_file(out);

   rename_file(EPN"_to_install.tmp", EPN"_to_install");

   return EXIT_SUCCESS;
}

int jp_install_append_line(char *line)
{
   FILE *out;
   int r;

   out = jp_open_home_file(EPN"_to_install", "a");
   if (!out) {
      return EXIT_FAILURE;
   }

   r = fprintf(out, "%s\n", line);
   if (r==EOF) {
      jp_close_home_file(out);
      return EXIT_FAILURE;
   }
   jp_close_home_file(out);

   return EXIT_SUCCESS;
}

/*returns 1 if found */
/*        0 if eof */
static int find_next_offset(mem_rec_header *mem_rh, long fpos,
			    long *next_offset,
			    unsigned char *attrib, unsigned int *unique_id)
{
   mem_rec_header *temp_mem_rh;
   unsigned char found = 0;
   unsigned long found_at;

   found_at=0x1000000;
   for (temp_mem_rh=mem_rh; temp_mem_rh; temp_mem_rh = temp_mem_rh->next) {
      if ((temp_mem_rh->offset > fpos) && (temp_mem_rh->offset < found_at)) {
	 found_at = temp_mem_rh->offset;
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

static void free_mem_rec_header(mem_rec_header **mem_rh)
{
   mem_rec_header *h, *next_h;

   for (h=*mem_rh; h; h=next_h) {
      next_h=h->next;
      free(h);
   }
   *mem_rh=NULL;
}

/* int jp_free_DB_records(GList **records) */
/* void free_buf_rec_list(GList **br_list) */
int jp_free_DB_records(GList **br_list)
{
   GList *temp_list;
   buf_rec *br;

   for (temp_list = *br_list; temp_list; temp_list = temp_list->next) {
      if (temp_list->data) {
	 br=temp_list->data;
	 if (br->buf) {
	    free(br->buf);
	    temp_list->data=NULL;
	 }
	 free(br);
      }
   }
   g_list_free(*br_list);
   *br_list=NULL;

   return EXIT_SUCCESS;
}

/* These next 2 functions were copied from pi-file.c in the pilot-link app */
/* Exact value of "Jan 1, 1970 0:00:00 GMT" - "Jan 1, 1904 0:00:00 GMT" */
#define PILOT_TIME_DELTA (unsigned)(2082844800)

#if 0
static time_t
pilot_time_to_unix_time (unsigned long raw_time)
{
   return (time_t)(raw_time - PILOT_TIME_DELTA);
}
#endif

/*
static unsigned long
unix_time_to_pilot_time (time_t t)
{
   return (unsigned long)((unsigned long)t + PILOT_TIME_DELTA);
}
*/

#if 0
static unsigned int bytes_to_bin(unsigned char *bytes, unsigned int num_bytes)
{
   unsigned int i, n;
   n=0;
   for (i=0;i<num_bytes;i++) {
      n = n*256+bytes[i];
   }
   return n;
}
#endif

int jp_get_app_info(char *DB_name, unsigned char **buf, int *buf_size)
{
   FILE *in;
   int num;
   int rec_size;
   unsigned char raw_header[LEN_RAW_DB_HEADER];
   DBHeader dbh;
   char PDB_name[FILENAME_MAX];

   if ((!buf_size) || (!buf)) {
      return EXIT_FAILURE;
   }
   *buf = NULL;
   *buf_size=0;

   g_snprintf(PDB_name, sizeof(PDB_name), "%s.pdb", DB_name);
   in = jp_open_home_file(PDB_name, "r");
   if (!in) {
      jp_logf(JP_LOG_WARN, _("%s:%d Error opening file: %s\n"), __FILE__, __LINE__, PDB_name);
      return EXIT_FAILURE;
   }
   num = fread(raw_header, LEN_RAW_DB_HEADER, 1, in);
   if (num != 1) {
      if (ferror(in)) {
	 jp_logf(JP_LOG_WARN, _("%s:%d Error reading file: %s\n"), __FILE__, __LINE__, PDB_name);
	 jp_close_home_file(in);
	 return EXIT_FAILURE;
      }
      if (feof(in)) {
	 jp_close_home_file(in);
	 return JPILOT_EOF;
      }
   }
   unpack_db_header(&dbh, raw_header);

   num = get_app_info_size(in, &rec_size);
   if (num) {
      jp_close_home_file(in);
      return EXIT_FAILURE;
   }

   fseek(in, dbh.app_info_offset, SEEK_SET);
   *buf=malloc(rec_size);
   if (!(*buf)) {
      jp_logf(JP_LOG_WARN, "jp_get_app_info(): %s\n", _("Out of memory"));
      jp_close_home_file(in);
      return EXIT_FAILURE;
   }
   num = fread(*buf, rec_size, 1, in);
   if (num != 1) {
      if (ferror(in)) {
	 jp_close_home_file(in);
	 free(*buf);
	 jp_logf(JP_LOG_WARN, _("%s:%d Error reading file: %s\n"), __FILE__, __LINE__, PDB_name);
	 return EXIT_FAILURE;
      }
   }
   jp_close_home_file(in);

   *buf_size=rec_size;

   return EXIT_SUCCESS;
}

/*
 * This deletes a record from the appropriate Datafile
 */
int jp_delete_record(char *DB_name, buf_rec *br, int flag)
{
   FILE *pc_in;
   PC3RecordHeader header;
   char PC_name[FILENAME_MAX];

   if (br==NULL) {
      return EXIT_FAILURE;
   }

   g_snprintf(PC_name, sizeof(PC_name), "%s.pc3", DB_name);

   if ((br->rt==DELETED_PALM_REC) || (br->rt==MODIFIED_PALM_REC)) {
      jp_logf(JP_LOG_INFO, _("This record is already deleted.\n"
		  "It is scheduled to be deleted from the Palm on the next sync.\n"));
      return EXIT_SUCCESS;
   }
   switch (br->rt) {
    case NEW_PC_REC:
    case REPLACEMENT_PALM_REC:
      pc_in=jp_open_home_file(PC_name, "r+");
      if (pc_in==NULL) {
	 jp_logf(JP_LOG_WARN, _("Unable to open PC records file\n"));
	 return EXIT_FAILURE;
      }
      while(!feof(pc_in)) {
	 read_header(pc_in, &header);
	 if (feof(pc_in)) {
	    jp_logf(JP_LOG_WARN, _("Couldn't find record to delete\n"));
	    jp_close_home_file(pc_in);
	    return EXIT_FAILURE;
	 }
	 if (header.header_version==2) {
	    /* Keep unique ID intact */
	    if ((header.unique_id==br->unique_id) &&
	       ((header.rt==NEW_PC_REC) || (header.rt==REPLACEMENT_PALM_REC))) {
	       if (fseek(pc_in, -header.header_len, SEEK_CUR)) {
		  jp_logf(JP_LOG_WARN, "fseek failed\n");
	       }
	       header.rt=DELETED_PC_REC;
	       write_header(pc_in, &header);
	       jp_logf(JP_LOG_DEBUG, "record deleted\n");
	       jp_close_home_file(pc_in);
	       return EXIT_SUCCESS;
	    }
	 } else {
	    jp_logf(JP_LOG_WARN, _("Unknown header version %d\n"), header.header_version);
	 }
	 if (fseek(pc_in, header.rec_len, SEEK_CUR)) {
	    jp_logf(JP_LOG_WARN, "fseek failed\n");
	 }
      }
      jp_close_home_file(pc_in);
      return EXIT_FAILURE;

    case PALM_REC:
      jp_logf(JP_LOG_DEBUG, "Deleting Palm ID %d\n", br->unique_id);
      pc_in=jp_open_home_file(PC_name, "a");
      if (pc_in==NULL) {
	 jp_logf(JP_LOG_WARN, _("Couldn't open PC records file\n"));
	 return EXIT_FAILURE;
      }
      header.unique_id=br->unique_id;
      if (flag==MODIFY_FLAG) {
	 header.rt=MODIFIED_PALM_REC;
      } else {
	 header.rt=DELETED_PALM_REC;
      }

      header.rec_len = br->size;

      jp_logf(JP_LOG_DEBUG, "writing header to pc file\n");
      write_header(pc_in, &header);
      /*todo write the real appointment from palm db */
      /*Right now I am just writing an empty record */
      /*This will be used for making sure that the palm record hasn't changed */
      /*before we delete it */
      jp_logf(JP_LOG_DEBUG, "writing record to pc file, %d bytes\n", header.rec_len);
      fwrite(br->buf, header.rec_len, 1, pc_in);
      jp_logf(JP_LOG_DEBUG, "record deleted\n");
      jp_close_home_file(pc_in);
      break;
    default:
      break;
   }

   return EXIT_SUCCESS;
}

/*
 * This undeletes a record from the appropriate Datafile
 */
int jp_undelete_record(char *DB_name, buf_rec *br, int flag)
{
   char filename[FILENAME_MAX];
   char filename2[FILENAME_MAX];
   FILE *pc_file  = NULL;
   FILE *pc_file2 = NULL;
   PC3RecordHeader header;
   char *record;
   unsigned int unique_id;
   int found;
   int ret = -1;
   int num;

   if (br==NULL) {
      return EXIT_FAILURE;
   }

   unique_id = br->unique_id;
   found  = FALSE;
   record = NULL;

   g_snprintf(filename, sizeof(filename), "%s.pc3", DB_name);
   g_snprintf(filename2, sizeof(filename2), "%s.pct", filename);

   pc_file = jp_open_home_file(filename , "r");
   if (!pc_file) {
      return EXIT_FAILURE;
   }

   pc_file2=jp_open_home_file(filename2, "w");
   if (!pc_file2) {
      jp_close_home_file(pc_file);
      return EXIT_FAILURE;
   }

   while(!feof(pc_file)) {
      read_header(pc_file, &header);
      if (feof(pc_file)) {
	 break;
      }
      /* Skip copying DELETED_PALM_REC entry which undeletes it */
      if (header.unique_id == unique_id &&
	  header.rt == DELETED_PALM_REC) {
	 found = TRUE;
	 if (fseek(pc_file, header.rec_len, SEEK_CUR)) {
	    jp_logf(JP_LOG_WARN, "fseek failed\n");
	    ret = -1;
	    break;
	 }
	 continue;
      }
      /* Change header on DELETED_PC_REC to undelete this type */
      if (header.unique_id == unique_id &&
          header.rt == DELETED_PC_REC) {
	  found = TRUE;
          header.rt = NEW_PC_REC;
      }

      /* Otherwise, keep whatever is there by copying it to the new pc3 file */
      record = malloc(header.rec_len);
      if (!record) {
	 jp_logf(JP_LOG_WARN, "cleanup_pc_file(): Out of memory\n");
	 ret = -1;
	 break;
      }
      num = fread(record, header.rec_len, 1, pc_file);
      if (num != 1) {
	 if (ferror(pc_file)) {
	    ret = -1;
	    break;
	 }
      }
      ret = write_header(pc_file2, &header);
      ret = fwrite(record, header.rec_len, 1, pc_file2);
      if (ret != 1) {
	 ret = -1;
	 break;
      }
      free(record);
      record = NULL;
   }

   if (record) {
      free(record);
   }
   if (pc_file) {
      jp_close_home_file(pc_file);
   }
   if (pc_file2) {
      jp_close_home_file(pc_file2);
   }

   if (found) {
      rename_file(filename2, filename);
   } else {
      unlink_file(filename2);
   }

   return ret;
}

/*
 * if buf_rec->unique_id==0 then the palm assigns an ID, else
 *  use buf_rec->unique_id.
 */
int jp_pc_write(char *DB_name, buf_rec *br)
{
   PC3RecordHeader header;
   FILE *out;
   unsigned int next_unique_id;
   unsigned char packed_header[256];
   int len;
   char PC_name[FILENAME_MAX];

   g_snprintf(PC_name, sizeof(PC_name), "%s.pc3", DB_name);
   if (br->unique_id==0) {
      get_next_unique_pc_id(&next_unique_id);
      header.unique_id=next_unique_id;
      br->unique_id=next_unique_id;
   } else {
      header.unique_id=br->unique_id;
   }
#ifdef JPILOT_DEBUG
   jp_logf(JP_LOG_DEBUG, "br->unique id = %d\n",br->unique_id);
#endif

   out = jp_open_home_file(PC_name, "a");
   if (!out) {
      jp_logf(JP_LOG_WARN, _("Error opening file: %s\n"), PC_name);
      return EXIT_FAILURE;
   }

   header.rec_len=br->size;
   header.rt=br->rt;
   header.attrib=br->attrib;

   len = pack_header(&header, packed_header);
   write_header(out, &header);
   fwrite(br->buf, header.rec_len, 1, out);

   jp_close_home_file(out);

   return EXIT_SUCCESS;
}

static int pc_read_next_rec(FILE *in, buf_rec *br)
{
   PC3RecordHeader header;
   int rec_len, num;
   char *record;

   if (feof(in)) {
      return JPILOT_EOF;
   }
   num = read_header(in, &header);
   if (num < 1) {
      if (ferror(in)) {
	 jp_logf(JP_LOG_WARN, _("Error reading PC file 1\n"));
	 return JPILOT_EOF;
      }
      if (feof(in)) {
	 return JPILOT_EOF;
      }
   }
   rec_len = header.rec_len;
   record = malloc(rec_len);
   if (!record) {
      jp_logf(JP_LOG_WARN, "pc_read_next_rec(): %s\n", _("Out of memory"));
      return JPILOT_EOF;
   }
   num = fread(record, rec_len, 1, in);
   if (num != 1) {
      if (ferror(in)) {
	 jp_logf(JP_LOG_WARN, _("Error reading PC file 2\n"));
	 free(record);
	 return JPILOT_EOF;
      }
   }
   br->rt = header.rt;
   br->unique_id = header.unique_id;
   br->attrib = header.attrib;
   br->buf = record;
   br->size = rec_len;

   return EXIT_SUCCESS;
}

int jp_read_DB_files(char *DB_name, GList **records)
{
   FILE *in;
   FILE *pc_in;
   char *buf;
   GList *temp_list;
   GList *end_of_list;
   int num_records, recs_returned, i, num, r;
   long offset, prev_offset, next_offset, rec_size;
   int out_of_order;
   long fpos, fend;  /*file position indicator */
   int ret;
   unsigned char attrib;
   unsigned int unique_id;
   mem_rec_header *mem_rh, *temp_mem_rh, *last_mem_rh;
   record_header rh;
   unsigned char raw_header[LEN_RAW_DB_HEADER];
   DBHeader dbh;
   buf_rec *temp_br;
   int temp_br_used;
   char PDB_name[FILENAME_MAX];
   char PC_name[FILENAME_MAX];

   jp_logf(JP_LOG_DEBUG, "Entering jp_read_DB_files: %s\n", DB_name);

   mem_rh = last_mem_rh = NULL;
   *records = end_of_list = NULL;
   recs_returned = 0;

   g_snprintf(PDB_name, sizeof(PDB_name), "%s.pdb", DB_name);
   g_snprintf(PC_name, sizeof(PC_name), "%s.pc3", DB_name);
   in = jp_open_home_file(PDB_name, "r");
   if (!in) {
      jp_logf(JP_LOG_WARN, _("Error opening file: %s\n"), PDB_name);
      return -1;
   }
   /*Read the database header */
   num = fread(raw_header, LEN_RAW_DB_HEADER, 1, in);
   if (num != 1) {
      if (ferror(in)) {
	 jp_logf(JP_LOG_WARN, _("Error reading file: %s\n"), PDB_name);
	 jp_close_home_file(in);
	 return -1;
      }
      if (feof(in)) {
	 jp_close_home_file(in);
	 return JPILOT_EOF;
      }
   }
   unpack_db_header(&dbh, raw_header);

#ifdef JPILOT_DEBUG
   jp_logf(JP_LOG_DEBUG, "db_name = %s\n", dbh.db_name);
   jp_logf(JP_LOG_DEBUG, "num records = %d\n", dbh.number_of_records);
   jp_logf(JP_LOG_DEBUG, "app info offset = %d\n", dbh.app_info_offset);
#endif

   /*Read each record entry header */
   num_records = dbh.number_of_records;
   out_of_order = 0;
   prev_offset = 0;

   for (i=1; i<num_records+1; i++) {
      num = fread(&rh, sizeof(record_header), 1, in);
      if (num != 1) {
	 if (ferror(in)) {
	    jp_logf(JP_LOG_WARN, _("Error reading file: %s\n"), PDB_name);
	    break;
	 }
	 if (feof(in)) {
	    jp_close_home_file(in);
	    return JPILOT_EOF;
	 }
      }

      offset = ((rh.Offset[0]*256+rh.Offset[1])*256+rh.Offset[2])*256+rh.Offset[3];

      if (offset < prev_offset) {
	 out_of_order = 1;
      }
      prev_offset = offset;

#ifdef JPILOT_DEBUG
      jp_logf(JP_LOG_DEBUG, "record header %u offset = %u\n",i, offset);
      jp_logf(JP_LOG_DEBUG, "       attrib 0x%x\n",rh.attrib);
      jp_logf(JP_LOG_DEBUG, "    unique_ID %d %d %d = ",rh.unique_ID[0],rh.unique_ID[1],rh.unique_ID[2]);
      jp_logf(JP_LOG_DEBUG, "%d\n",(rh.unique_ID[0]*256+rh.unique_ID[1])*256+rh.unique_ID[2]);
#endif
      temp_mem_rh = malloc(sizeof(mem_rec_header));
      if (!temp_mem_rh) {
	 jp_logf(JP_LOG_WARN, "jp_read_DB_files(): %s 1\n", _("Out of memory"));
	 break;
      }
      temp_mem_rh->next = NULL;
      temp_mem_rh->rec_num = i;
      temp_mem_rh->offset = offset;
      temp_mem_rh->attrib = rh.attrib;
      temp_mem_rh->unique_id = (rh.unique_ID[0]*256+rh.unique_ID[1])*256+rh.unique_ID[2];
      if (mem_rh == NULL) {
	 mem_rh = temp_mem_rh;
	 last_mem_rh = temp_mem_rh;
      } else {
	 last_mem_rh->next = temp_mem_rh;
	 last_mem_rh = temp_mem_rh;
      }
   }

   temp_mem_rh = mem_rh;
   if (num_records) {
      if (out_of_order) {
	 ret=find_next_offset(mem_rh, 0, &next_offset, &attrib, &unique_id);
      } else {
	 if (mem_rh) {
	    next_offset = mem_rh->offset;
	    attrib = mem_rh->attrib;
	    unique_id = mem_rh->unique_id;
	 }
      }
      fseek(in, next_offset, SEEK_SET);
      while(!feof(in)) {
	 fpos = ftell(in);
	 if (out_of_order) {
	    ret = find_next_offset(mem_rh, fpos, &next_offset, &attrib, &unique_id);
	    if (!ret) {
	       /* Next offset should be end of file */
	       fseek(in, 0, SEEK_END);
	       fend = ftell(in);
	       fseek(in, fpos, SEEK_SET);
	       next_offset = fend + 1;
	    }
	 } else {
	    if (temp_mem_rh) {
	       attrib = temp_mem_rh->attrib;
	       unique_id = temp_mem_rh->unique_id;
	       if (temp_mem_rh->next) {
		  temp_mem_rh = temp_mem_rh->next;
		  next_offset = temp_mem_rh->offset;
	       } else {
		  /* Next offset should be end of file */
		  fseek(in, 0, SEEK_END);
		  fend = ftell(in);
		  fseek(in, fpos, SEEK_SET);
		  next_offset = fend + 1;
	       }
	    }
	 }
	 rec_size = next_offset - fpos;
#ifdef JPILOT_DEBUG
	 jp_logf(JP_LOG_DEBUG, "rec_size = %u\n",rec_size);
	 jp_logf(JP_LOG_DEBUG, "fpos,next_offset = %u %u\n",fpos,next_offset);
	 jp_logf(JP_LOG_DEBUG, "----------\n");
#endif
	 buf = malloc(rec_size);
	 if (!buf) break;
	 num = fread(buf, 1, rec_size, in);
	 if (num<rec_size) {
	    rec_size=num;
	    buf = realloc(buf, rec_size);
	 }
	 if ((num < 1)) {
	    if (ferror(in)) {
	       jp_logf(JP_LOG_WARN, _("Error reading %s 5\n"), PDB_name);
	       free(buf);
	       break;
	    }
	 }

	 temp_br = malloc(sizeof(buf_rec));
	 if (!temp_br) {
	    jp_logf(JP_LOG_WARN, "jp_read_DB_files(): %s 2\n", _("Out of memory"));
	    break;
	 }
	 temp_br->rt = PALM_REC;
	 temp_br->unique_id = unique_id;
	 temp_br->attrib = attrib;
	 temp_br->buf = buf;
	 temp_br->size = rec_size;

	 /* g_list_append parses the list to get to the end on every call.
	  * To speed it up we have to give the last record
	  */
         /*
	 if (*records==NULL) {
	    *records = g_list_append(*records, temp_br);
	    end_of_list=*records;
	 } else {
	    *records = g_list_append(end_of_list, temp_br);
	    if (end_of_list->next) {
	       end_of_list=end_of_list->next;
	    }
	 }
         */
         *records = g_list_prepend(*records, temp_br);

	 recs_returned++;
      }
   }
   jp_close_home_file(in);

   free_mem_rec_header(&mem_rh);
   /* */
   /* Get the appointments out of the PC database */
   /* */
   pc_in = jp_open_home_file(PC_name, "r");
   if (pc_in==NULL) {
      jp_logf(JP_LOG_DEBUG, "jp_open_home_file failed: %s\n", PC_name);
      return -1;
   }

   while(!feof(pc_in)) {
      temp_br_used = 0;
      temp_br = malloc(sizeof(buf_rec));
      if (!temp_br) {
	 jp_logf(JP_LOG_WARN, "jp_read_DB_files(): %s 3\n", _("Out of memory"));
	 recs_returned = -1;
	 break;
      }
      r = pc_read_next_rec(pc_in, temp_br);
      if ((r==JPILOT_EOF) || (r<0)) {
	 free(temp_br);
	 break;
      }
      if (temp_br->rt!=DELETED_PALM_REC  &&
	  temp_br->rt!=MODIFIED_PALM_REC &&
	  temp_br->rt!=DELETED_DELETED_PALM_REC) {
         *records = g_list_prepend(*records, temp_br);
         temp_br_used = 1;
	 recs_returned++;
      }
      if ((temp_br->rt==DELETED_PALM_REC) || (temp_br->rt==MODIFIED_PALM_REC)) {
	 for (temp_list=*records; temp_list; temp_list=temp_list->next) {
	    if (((buf_rec *)temp_list->data)->unique_id == temp_br->unique_id) {
	       if (((buf_rec *)temp_list->data)->rt == PALM_REC) {
		  ((buf_rec *)temp_list->data)->rt = temp_br->rt;
	       }
	    }
	 }
      }

      if (!temp_br_used) {
         free(temp_br->buf);
         free(temp_br);
      }
   }
   jp_close_home_file(pc_in);

   jp_logf(JP_LOG_DEBUG, "Leaving jp_read_DB_files\n");

   return recs_returned;
}

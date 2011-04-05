#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <utime.h>
#include <stdio.h>
#include <ctype.h>

#include <pi-source.h>
#include <pi-socket.h>
#include <pi-dlp.h>
#include <pi-file.h>

#include <gdk/gdk.h>

#include "libplugin.h"
#include "utils.h"
#include "sync.h"

/* In jpilot.c, referenced from utils.c */
gint glob_date_timer_tag;


void output_to_pane(const char *str);
void output_to_pane(const char *str) {}
int sync_once(struct my_sync_info *sync_info) { return EXIT_SUCCESS; }
GtkWidget *glob_dialog;
int *glob_date_label;
pid_t jpilot_master_pid = -1;
int edit_cats(GtkWidget *widget, char *db_name, struct CategoryAppInfo *cai) {}
int jp_pack_Contact(struct Contact *c, pi_buffer_t *buf) {}

int read_pc_recs(char *file_name, GList **records)
{
   FILE *pc_in;
   int recs_returned;
   buf_rec *temp_br;
   int r;


   /* Get the records out of the PC database */
   pc_in = fopen(file_name, "r");
   if (pc_in==NULL) {
      fprintf(stderr, "open file failed: %s\n", file_name);
      return -1;
   }

   while(!feof(pc_in)) {
      temp_br = malloc(sizeof(buf_rec));
      if (!temp_br) {
	 fprintf(stderr, "Out of memory");
	 recs_returned = -1;
	 break;
      }
      r = pc_read_next_rec(pc_in, temp_br);
      if ((r==JPILOT_EOF) || (r<0)) {
	 free(temp_br);
	 break;
      }

      *records = g_list_prepend(*records, temp_br);
      recs_returned++;

   }
   fclose(pc_in);

   return 0;
}

int merge_pdb_file(char *src_pdb_file, char *src_pc_file, char *dest_pdb_file)
{
   struct pi_file *pf1, *pf2;
   struct DBInfo infop;
   void *app_info;
   void *sort_info;
   void *record;
   int r;
   int idx;
   size_t size;
   int attr;
   int cat;
   pi_uid_t uid;
   buf_rec *temp_br_pdb;
   buf_rec *temp_br_pc;
   GList *Ppdb_record = NULL;
   GList *Ppc_record = NULL;
   GList *pdb_records = NULL;
   GList *pc_records = NULL;
   int dont_add;
   int next_available_unique_id;
   // Statistics
   int pdb_count=0;
   int recs_added = 0;
   int recs_deleted = 0;
   int recs_modified = 0;
   int recs_written = 0;
   

   if (access(src_pdb_file, R_OK)) {
      fprintf(stderr, "Can not open %s for reading\n", src_pdb_file);
      return 1;
   }

   if (access(src_pc_file, R_OK)) {
      fprintf(stderr, "Can not open %s for reading\n", src_pc_file);
      return 1;
   }

   r = read_pc_recs(src_pc_file, &pc_records);
   if (r < 0) {
      fprintf(stderr, "read_pc_recs returned %d\n", r);
      exit(1);
   }

   pf1 = pi_file_open(src_pdb_file);
   if (!pf1) {
      fprintf(stderr, "%s: Could not open %s\n", "pi_file_open", src_pdb_file);
      exit(1);
   }
   pi_file_get_info(pf1, &infop);
   pf2 = pi_file_create(dest_pdb_file, &infop);
   if (!pf2) {
      fprintf(stderr, "%s: Could not open %s\n", "pi_file_open", dest_pdb_file);
      exit(1);
   }

   pi_file_get_app_info(pf1, &app_info, &size);
   pi_file_set_app_info(pf2, app_info, size);

   pi_file_get_sort_info(pf1, &sort_info, &size);  
   pi_file_set_sort_info(pf2, sort_info, size);

   for(idx=0;;idx++) {
      r = pi_file_read_record(pf1, idx, &record, &size, &attr, &cat, &uid);
      //printf("attr=%d, cat=%d\n", attr, cat);
      if (r<0) break;

      pdb_count++;

      temp_br_pdb = malloc(sizeof(buf_rec));
      temp_br_pdb->rt = PALM_REC;
      temp_br_pdb->unique_id = uid;
      temp_br_pdb->attrib = attr | cat;
      //temp_br_pdb->buf = record;
      temp_br_pdb->buf = malloc(size);
      memcpy(temp_br_pdb->buf, record, size);
      temp_br_pdb->size = size;

      dont_add=0;

      // Look through the pc record list
      for (Ppc_record=pc_records; Ppc_record; Ppc_record=Ppc_record->next) {
	 temp_br_pc = (buf_rec *)Ppc_record->data;
	 if ((temp_br_pc->rt==DELETED_PC_REC) || (temp_br_pc->rt==DELETED_DELETED_PALM_REC)) {
	    continue;
	 }
	 if ((temp_br_pc->rt==DELETED_PALM_REC) || (temp_br_pc->rt==MODIFIED_PALM_REC)) {
	    if (temp_br_pdb->unique_id == temp_br_pc->unique_id) {
	       // Don't add it to the pdb list
	       dont_add=1;
	       if (temp_br_pc->rt==DELETED_PALM_REC) {
		  recs_deleted++;
	       }
	       break;
	    }
	 }

	 if ((temp_br_pc->rt==REPLACEMENT_PALM_REC)) {
	    if (temp_br_pdb->unique_id == temp_br_pc->unique_id) {
	       // Replace the record data in the pdb record with replacement record data
	       //printf("REPLACEMENT\n");
	       dont_add=1;
	       pdb_records = g_list_prepend(pdb_records, temp_br_pc);
	       recs_modified++;
	       //break;
	    }
	 }
      }

      if (! dont_add) {
	 pdb_records = g_list_prepend(pdb_records, temp_br_pdb);
      }
   }


   // Find the next available unique ID
   next_available_unique_id = 0;
   for (Ppdb_record=pdb_records; Ppdb_record; Ppdb_record=Ppdb_record->next) {
      temp_br_pdb = (buf_rec *)Ppdb_record->data;
      if (temp_br_pdb->unique_id > next_available_unique_id) {
	 next_available_unique_id = temp_br_pdb->unique_id + 1;
      }
   }

   // Add the NEW records to the list
   for (Ppc_record=pc_records; Ppc_record; Ppc_record=Ppc_record->next) {
      temp_br_pc = (buf_rec *)Ppc_record->data;
      if ((temp_br_pc->rt==NEW_PC_REC)) {
	 temp_br_pc->unique_id = next_available_unique_id++;
	 pdb_records = g_list_prepend(pdb_records, temp_br_pc);
	 recs_added++;
	 continue;
      }
   }


   pdb_records = g_list_reverse(pdb_records);

   for (Ppdb_record=pdb_records; Ppdb_record; Ppdb_record=Ppdb_record->next) {
      temp_br_pdb = (buf_rec *)Ppdb_record->data;
      //printf("rt=%d\n", temp_br_pdb->rt);
      //printf("unique_id=%d\n", temp_br_pdb->unique_id);
      //printf("attrib=%d\n", temp_br_pdb->attrib);
      //printf("buf=%ld\n", temp_br_pdb->buf);
      //printf("size=%d\n", temp_br_pdb->size);
      pi_file_append_record(pf2, temp_br_pdb->buf, temp_br_pdb->size, (temp_br_pdb->attrib)&0xF0, (temp_br_pdb->attrib)&0x0F, temp_br_pdb->unique_id);
      recs_written++;
   }

   pi_file_close(pf1);
   pi_file_close(pf2);

   printf("Records read from pdb = %d\n", pdb_count);
   printf("Records added         = %d\n", recs_added);
   printf("Records deleted       = %d\n", recs_deleted);
   printf("Records modified      = %d\n", recs_modified);
   printf("Records written       = %d\n", recs_written);

   return 0;
}



int main(int argc, char *argv[])
{
   if (argc < 3) {
      fprintf(stderr, "Usage: %s {input pdb file} {input pc file} {output pdb file}\n", argv[0]);
      fprintf(stderr, "  This program will merge an unsynced records file (pc3)\n");
      fprintf(stderr, "  into the corresponding palm database (pdb) file.\n");
      fprintf(stderr, "  Only run it if you know what you are doing!!!\n");
      fprintf(stderr, "  This will leave your databases in an unsync-able state.\n");
      fprintf(stderr, "  It is possible to merge address records into datebook, etc., you do not want to do this!!!\n");
      exit(1);
   }
   char *in_pdb;
   char *in_pc;
   char *out_pdb;
   in_pdb = argv[1];
   in_pc = argv[2];
   out_pdb = argv[3];
   
   merge_pdb_file(in_pdb, in_pc, out_pdb);
   
   return 0;
}

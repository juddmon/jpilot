/* category.c
 * A module of J-Pilot http://jpilot.org
 * 
 * Copyright (C) 2002 by Judd Montgomery
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
#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <utime.h>
#include <unistd.h>
#include <pi-file.h>
#include "utils.h"
#include "log.h"
#include "prefs.h"


/* #define EDIT_CATS_DEBUG 1 */

#define EDIT_CAT_START        100
#define EDIT_CAT_NEW          101
#define EDIT_CAT_RENAME       102
#define EDIT_CAT_DELETE       103
#define EDIT_CAT_ENTRY_OK     104
#define EDIT_CAT_ENTRY_CANCEL 105


struct dialog_cats_data {
   int button_hit;  
   int selected;
   int state;
   GtkWidget *clist;
   GtkWidget *button_box;
   GtkWidget *entry_box;
   GtkWidget *entry;
   GtkWidget *label;
   char db_name[16];
   struct CategoryAppInfo cai1;
   struct CategoryAppInfo cai2;
};


/*
 * commented in header file
 */
int jp_count_records_in_cat(char *db_name, int cat_index)
{
   GList *records;
   GList *temp_list;
   int count, i, num;
   buf_rec *br;

   jp_logf(JP_LOG_DEBUG, "jp_count_records_in_cat\n");

   count = 0;

   num = jp_read_DB_files(db_name, &records);

   /* Go to first entry in the list */
   for (temp_list = records; temp_list; temp_list = temp_list->prev) {
      records = temp_list;
   }
   for (i=0, temp_list = records; temp_list; temp_list = temp_list->next, i++) {
      if (temp_list->data) {
	 br=temp_list->data;
      } else {
	 continue;
      }
      if (!br->buf) {
	 continue;
      }

      if ( (br->rt==DELETED_PALM_REC) || (br->rt==MODIFIED_PALM_REC) ) {
	 continue;
      }

      if ( (br->attrib & 0x0F) != cat_index) {
	 continue;
      }
      count++;
   }

   jp_free_DB_records(&records);

   jp_logf(JP_LOG_DEBUG, "Leaving jp_count_records_in_cat()\n");

   return count;
}

/*
 * This changes every record with index old_index and changes it to new_index
 * returns the number of record's categories changed.
 */
int pdb_file_change_indexes(char *DB_name, int old_index, int new_index)
{
   char local_pdb_file[256];
   char full_local_pdb_file[256];
   char full_local_pdb_file2[256];
   struct pi_file *pf1, *pf2;
   struct DBInfo infop;
   void *app_info;
   void *sort_info;
   void *record;
   int r;
   int idx;
   int size;
   int attr;
   int cat, new_cat;
   int count;
   pi_uid_t uid;
   struct stat statb;
   struct utimbuf times;

   jp_logf(JP_LOG_DEBUG, "pi_file_change_indexes\n");

   g_snprintf(local_pdb_file, 250, "%s.pdb", DB_name);
   get_home_file_name(local_pdb_file, full_local_pdb_file, 250);
   strcpy(full_local_pdb_file2, full_local_pdb_file);
   strcat(full_local_pdb_file2, "2");

   /* After we are finished, set the create and modify times of new file
      to the same as the old */
   stat(full_local_pdb_file, &statb);
   times.actime = statb.st_atime;
   times.modtime = statb.st_mtime;

   pf1 = pi_file_open(full_local_pdb_file);
   if (!pf1) {
      jp_logf(JP_LOG_WARN, "Couldn't open [%s]\n", full_local_pdb_file);
      return -1;
   }
   pi_file_get_info(pf1, &infop);
   pf2 = pi_file_create(full_local_pdb_file2, &infop);
   if (!pf2) {
      jp_logf(JP_LOG_WARN, "Couldn't open [%s]\n", full_local_pdb_file2);
      return -1;
   }

   pi_file_get_app_info(pf1, &app_info, &size);
   pi_file_set_app_info(pf2, app_info, size);

   pi_file_get_sort_info(pf1, &sort_info, &size);  
   pi_file_set_sort_info(pf2, sort_info, size);

   count = 0;

   for(idx=0;;idx++) {
      r = pi_file_read_record(pf1, idx, &record, &size, &attr, &cat, &uid);
      if (r<0) break;
      new_cat=cat;
      if (cat==old_index) {
	 cat=new_index;
	 count++;
      }
      pi_file_append_record(pf2, record, size, attr, cat, uid);
   }

   pi_file_close(pf1);
   pi_file_close(pf2);

   if (rename(full_local_pdb_file2, full_local_pdb_file) < 0) {
      jp_logf(JP_LOG_WARN, "change_indexes: rename failed\n");
   }

   utime(full_local_pdb_file, &times);

   return 0;
}

int edit_cats_delete_cats_pc3(char *DB_name, int cat)
{
   char local_pc_file[256];
   int num;
   FILE *pc_in;
   PC3RecordHeader header;
   int rec_len;
   int count=0;

   g_snprintf(local_pc_file, 250, "%s.pc3", DB_name);

   pc_in = jp_open_home_file(local_pc_file, "r+");
   if (pc_in==NULL) {
      jp_logf(JP_LOG_WARN, _("Unable to open %s\n"), local_pc_file);
      return -1;
   }

   while(!feof(pc_in)) {
      num = read_header(pc_in, &header);
      if (num!=1) {
	 if (ferror(pc_in)) {
	    break;
	 }
	 if (feof(pc_in)) {
	    break;
	 }
      }
      rec_len = header.rec_len;
      if (rec_len > 0x10000) {
	 jp_logf(JP_LOG_WARN, _("PC file corrupt?\n"));
	 fclose(pc_in);
	 return -1;
      }
      if (((header.rt==NEW_PC_REC) || (header.rt==REPLACEMENT_PALM_REC)) &&
	  ((header.attrib&0x0F)==cat)) {
	 if (fseek(pc_in, -(header.header_len), SEEK_CUR)) {
	    jp_logf(JP_LOG_WARN, _("fseek failed - fatal error\n"));
	    fclose(pc_in);
	    return -1;
	 }
	 count++;
	 header.rt=DELETED_PC_REC;
	 write_header(pc_in, &header);
      }
      /*skip this record now that we are done with it */
      if (fseek(pc_in, rec_len, SEEK_CUR)) {
	 jp_logf(JP_LOG_WARN, _("fseek failed - fatal error\n"));
	 fclose(pc_in);
	 return -1;
      }
   }

   fclose(pc_in);
   return count;
}

int _edit_cats_change_cats_pc3(char *DB_name, int old_cat,
			       int new_cat, int swap)
{
   char local_pc_file[256];
   int num;
   FILE *pc_in;
   PC3RecordHeader header;
   int rec_len;
   int count=0;

   g_snprintf(local_pc_file, 250, "%s.pc3", DB_name);

   pc_in = jp_open_home_file(local_pc_file, "r+");
   if (pc_in==NULL) {
      jp_logf(JP_LOG_WARN, _("Unable to open %s\n"), local_pc_file);
      return -1;
   }

   while(!feof(pc_in)) {
      num = read_header(pc_in, &header);
      if (num!=1) {
	 if (ferror(pc_in)) {
	    break;
	 }
	 if (feof(pc_in)) {
	    break;
	 }
      }
      rec_len = header.rec_len;
      if (rec_len > 0x10000) {
	 jp_logf(JP_LOG_WARN, _("PC file corrupt?\n"));
	 fclose(pc_in);
	 return -1;
      }
      /* No matter what the record type we will change the cat if needed */
      if ((header.attrib&0x0F)==old_cat) {
	 if (fseek(pc_in, -(header.header_len), SEEK_CUR)) {
	    jp_logf(JP_LOG_WARN, _("fseek failed - fatal error\n"));
	    fclose(pc_in);
	    return -1;
	 }
	 count++;
	 header.attrib=(header.attrib&0xFFFFFFF0) | new_cat;
	 write_header(pc_in, &header);
      }
      /* No matter what the record type we will change the cat if needed */
      if ((swap) && ((header.attrib&0x0F)==new_cat)) {
	 if (fseek(pc_in, -(header.header_len), SEEK_CUR)) {
	    jp_logf(JP_LOG_WARN, _("fseek failed - fatal error\n"));
	    fclose(pc_in);
	    return -1;
	 }
	 count++;
	 header.attrib=(header.attrib&0xFFFFFFF0) | old_cat;
	 write_header(pc_in, &header);
      }
      /*skip this record now that we are done with it */
      if (fseek(pc_in, rec_len, SEEK_CUR)) {
	 jp_logf(JP_LOG_WARN, _("fseek failed - fatal error\n"));
	 fclose(pc_in);
	 return -1;
      }
   }

   fclose(pc_in);
   return count;
}

int edit_cats_change_cats_pc3(char *DB_name, int old_cat,
			       int new_cat)
{
   return _edit_cats_change_cats_pc3(DB_name, old_cat, new_cat, 0);
}

int edit_cats_swap_cats_pc3(char *DB_name, int old_cat,
			    int new_cat)
{
   return _edit_cats_change_cats_pc3(DB_name, old_cat, new_cat, 1);
}


/*
 * This routine changes records from old_cat to new_cat.
 *  It does not modify a local pdb file.
 *  It does this by writing a modified record to the pc3 file.
 */
int edit_cats_change_cats_pdb(char *DB_name, int old_cat, int new_cat)
{
   int i, r, count;
   buf_rec *br;
   GList *records;
   GList *temp_list;

   jp_logf(JP_LOG_DEBUG, "edit_cats_change_cats_pdb\n");
#ifdef EDIT_CATS_DEBUG 
  printf("edit_cats_change_cats_pdb\n");
#endif

   count=0;
   r = jp_read_DB_files(DB_name, &records);

   /* Go to first entry in the list */
   for (temp_list = records; temp_list; temp_list = temp_list->prev) {
      records = temp_list;
   }
   for (i=0, temp_list = records; temp_list; temp_list = temp_list->next, i++) {
      if (temp_list->data) {
	 br=temp_list->data;
      } else {
	 continue;
      }
      if (!br->buf) {
	 continue;
      }
      if ( (br->rt==DELETED_PALM_REC) || (br->rt==MODIFIED_PALM_REC) ) {
	 continue;
      }
      if ( (br->attrib & 0x0F) == old_cat) {
	 if (new_cat==-1) {
	    /* write a deleted rec */
	    jp_delete_record(DB_name, br, DELETE_FLAG);
	    count++;
	 } else {
	    /* write a deleted rec */
	    br->attrib=(br->attrib&0xFFFFFFF0) | (new_cat&0x0F);
	    jp_delete_record(DB_name, br, MODIFY_FLAG);
	    br->rt=REPLACEMENT_PALM_REC;
	    jp_pc_write(DB_name, br);
	    count++;
	 }
      }
   }

   jp_free_DB_records(&records);

   return count;
}

/*
 * This routine changes deletes records in category cat.
 *  It does not modify a local pdb file.
 *  It does this by writing a modified record to the pc3 file.
 */
int edit_cats_delete_cats_pdb(char *DB_name, int cat)
{
   jp_logf(JP_LOG_DEBUG, "edit_cats_delete_cats_pdb\n");

   return edit_cats_change_cats_pdb(DB_name, cat, -1);
}

static void cb_edit_button(GtkWidget *widget, gpointer data)
{
   struct dialog_cats_data *Pdata;
   int i, r, count;
   int id;
   int button;
   char *button_text[]={gettext_noop("OK")};
   char *move_text[]={gettext_noop("Move"), gettext_noop("Delete"), gettext_noop("Cancel")};
   char *text;
   const char *entry_text;
   char temp[256];

   button = GPOINTER_TO_INT(data);
   Pdata = gtk_object_get_data(GTK_OBJECT(gtk_widget_get_toplevel(widget)), "dialog_cats_data");

   if (Pdata) {
      switch (button) {
       case EDIT_CAT_NEW:
	 count=0;
	 for (i=1; i<16; i++) {
	    r = gtk_clist_get_text(GTK_CLIST(Pdata->clist), i, 0, &text);
	    if ((r) && (text[0])) {
	       count++;
	    }
	 }
	 if (count>14) {
	    dialog_generic(GTK_WINDOW(gtk_widget_get_toplevel(widget)), 0, 0,
			   _("Edit Categories"), NULL,
			   _("The maximum number of categories (16) are already used"), 1, button_text);
	    return;
	 }
	 gtk_label_set_text(GTK_LABEL(Pdata->label), _("Enter New Category"));
	 gtk_entry_set_text(GTK_ENTRY(Pdata->entry), "");
	 gtk_widget_show(Pdata->entry_box);
	 gtk_widget_hide(Pdata->button_box);
	 gtk_widget_grab_focus(GTK_WIDGET(Pdata->entry));
	 Pdata->state=EDIT_CAT_NEW;
	 break;
       case EDIT_CAT_RENAME:
	 if ((Pdata->selected<0) || (Pdata->cai2.name[Pdata->selected][0]=='\0')) {
	    dialog_generic(GTK_WINDOW(gtk_widget_get_toplevel(widget)), 0, 0,
			   _("Edit Categories"), NULL,
			   _("You must select a category to rename"), 1, button_text);
	    return;
	 }
	 if (Pdata->selected==0) {
	    sprintf(temp, _("You can't edit category %s.\n"), Pdata->cai1.name[0]);
	    dialog_generic(GTK_WINDOW(gtk_widget_get_toplevel(widget)), 0, 0,
			   _("Edit Categories"), NULL,
			   temp, 1, button_text);
	    return;
	 }
	 r = gtk_clist_get_text(GTK_CLIST(Pdata->clist), Pdata->selected, 0, &text);
	 gtk_label_set_text(GTK_LABEL(Pdata->label), _("Enter New Category Name"));
	 gtk_entry_set_text(GTK_ENTRY(Pdata->entry), text);
	 gtk_widget_show(Pdata->entry_box);
	 gtk_widget_hide(Pdata->button_box);
	 gtk_widget_grab_focus(GTK_WIDGET(Pdata->entry));
	 Pdata->state=EDIT_CAT_RENAME;
	 break;
       case EDIT_CAT_DELETE:
#ifdef EDIT_CATS_DEBUG
	 printf("delete cat\n");
#endif
	 if (Pdata->selected<0) {
	    dialog_generic(GTK_WINDOW(gtk_widget_get_toplevel(widget)), 0, 0,
			   _("Edit Categories"), NULL,
			   _("You must select a category to delete"), 1, button_text);
	    return;
	 }
	 if (Pdata->selected==0) {
	    sprintf(temp, _("You can't delete category %s.\n"), Pdata->cai1.name[0]);
	    dialog_generic(GTK_WINDOW(gtk_widget_get_toplevel(widget)), 0, 0,
			   _("Edit Categories"), NULL,
			   temp, 1, button_text);
	    return;
	 }
	 /* See if category is not-empty */
	 if (Pdata->cai2.name[Pdata->selected][0]=='\0') {
	    return;
	 }
	 /* check to see if any records are in this cat. */
	 count = jp_count_records_in_cat(Pdata->db_name, Pdata->selected);
#ifdef EDIT_CATS_DEBUG
	 printf("count=%d\n", count);
#endif
	 if (count>0) {
	    sprintf(temp, _("There are %d records in %s.\n"
		    "Do you want to move them to %s, or delete them?"),
		    count, Pdata->cai1.name[Pdata->selected], Pdata->cai1.name[0]);
	    r = dialog_generic(GTK_WINDOW(gtk_widget_get_toplevel(widget)), 0, 0,
			       _("Edit Categories"), NULL,
			       temp, 3, move_text);
	    switch (r) {
	     case DIALOG_SAID_1:
#ifdef EDIT_CATS_DEBUG
	       printf("MOVE THEM\n");
#endif
	       r = edit_cats_change_cats_pc3(Pdata->db_name, Pdata->selected, 0);
#ifdef EDIT_CATS_DEBUG
	       printf("moved %d pc records\n", r);
#endif
	       r = edit_cats_change_cats_pdb(Pdata->db_name, Pdata->selected, 0);
#ifdef EDIT_CATS_DEBUG
	       printf("moved %d pdb->pc records\n", r);
#endif
	       break;
	     case DIALOG_SAID_2:
#ifdef EDIT_CATS_DEBUG
	       printf("DELETE THEM\n");
#endif
	       r = edit_cats_delete_cats_pc3(Pdata->db_name, Pdata->selected);
#ifdef EDIT_CATS_DEBUG
	       printf("deleted %d pc records\n", r);
#endif
	       r = edit_cats_delete_cats_pdb(Pdata->db_name, Pdata->selected);
#ifdef EDIT_CATS_DEBUG
	       printf("deleted %d pdb->pc records\n", r);
#endif
	       break;
	     case DIALOG_SAID_3:
#ifdef EDIT_CATS_DEBUG
	       printf("DO Nothing\n");
#endif
	       return;
	    }
	 }
	 /* delete the category */
#ifdef EDIT_CATS_DEBUG
	 printf("DELETE category\n");
#endif
	 Pdata->cai2.ID[Pdata->selected]=Pdata->cai1.ID[Pdata->selected];
	 Pdata->cai2.name[Pdata->selected][0]='\0';
	 Pdata->cai2.renamed[Pdata->selected]=1;
	 gtk_clist_set_text(GTK_CLIST(Pdata->clist), Pdata->selected, 0, "");
	 break;
       case EDIT_CAT_ENTRY_OK:
	 if ((Pdata->state!=EDIT_CAT_RENAME) && (Pdata->state!=EDIT_CAT_NEW)) {
	    jp_logf(JP_LOG_WARN, "invalid state file %s line %d\n", __FILE__, __LINE__);
	    return;
	 }
	 /* Can't make an empty category, could do a dialog telling user */
	 entry_text = gtk_entry_get_text(GTK_ENTRY(Pdata->entry));
	 if ( (!entry_text) || (!entry_text[0]) ) {
	    return;
	 }
	 if (Pdata->state==EDIT_CAT_RENAME) {
#ifdef EDIT_CATS_DEBUG
	    printf("rename cat\n");
#endif
	    i=Pdata->selected;
	    entry_text = gtk_entry_get_text(GTK_ENTRY(Pdata->entry));
	    strncpy(Pdata->cai2.name[i], entry_text, 16);
	    Pdata->cai2.name[i][15]='\0';
	    Pdata->cai2.renamed[i]=1;
	    gtk_clist_set_text(GTK_CLIST(Pdata->clist), i, 0, Pdata->cai2.name[i]);
	 }
	 if (Pdata->state==EDIT_CAT_NEW) {
#ifdef EDIT_CATS_DEBUG
	    printf("new cat\n");
#endif
	    /* Check for category being used more than once */
	    entry_text = gtk_entry_get_text(GTK_ENTRY(Pdata->entry));
	    for (i=0; i<16; i++) {
	       if (!strcmp(entry_text, Pdata->cai2.name[i])) {
		  sprintf(temp, _("The category %s can't be used more than once"), entry_text);
		  dialog_generic(GTK_WINDOW(gtk_widget_get_toplevel(widget)), 0, 0,
				 _("Edit Categories"), NULL,
				 temp, 1, button_text);
		  return;
	       }
	    }
	    /* Find a new category ID */
	    id=128;
	    for (i=1; i<16; i++) {
	       if (Pdata->cai2.ID[i]==id) {
		  id++;
		  i=0;
	       }
	    }
	    /* Find an empty slot */
	    /* When the new button was pressed we already checked for an empty slot */
	    for (i=1; i<16; i++) {
	       if (Pdata->cai2.name[i][0]=='\0') {
#ifdef EDIT_CATS_DEBUG
		  printf("slot %d is empty\n", i);
#endif
		  Pdata->cai2.ID[i]=id;
		  strncpy(Pdata->cai2.name[i], entry_text, 16);
		  Pdata->cai2.name[i][15]='\0';
		  Pdata->cai2.renamed[i]=1;
		  gtk_clist_set_text(GTK_CLIST(Pdata->clist), i, 0, Pdata->cai2.name[i]);
		  break;
	       }
	    }
	 }
	 gtk_widget_hide(Pdata->entry_box);
	 gtk_widget_show(Pdata->button_box);
	 Pdata->state=EDIT_CAT_START;
	 break;
       case EDIT_CAT_ENTRY_CANCEL:
	 gtk_widget_hide(Pdata->entry_box);
	 gtk_widget_show(Pdata->button_box);
	 Pdata->state=EDIT_CAT_START;
	 break;
       default:
	 jp_logf(JP_LOG_WARN, "cb_edit_button: unknown button\n");
      }
   }
}


void cb_clist_edit_cats(GtkWidget *widget,
			gint row, gint column,
			GdkEventButton *event, gpointer data)
{
   struct dialog_cats_data *Pdata;

   Pdata=data;
#ifdef EDIT_CATS_DEBUG
   printf("row=%d\n", row);
#endif

   if (Pdata->state==EDIT_CAT_START) {
      Pdata->selected=row;
   } else {
#ifdef EDIT_CATS_DEBUG
      printf("cb_clist_edit_cats not in start state\n");
#endif
      if (Pdata->selected!=row) {
	 gtk_clist_select_row(GTK_CLIST(Pdata->clist), Pdata->selected, 0);
      }
   }
}

#ifdef EDIT_CATS_DEBUG
static void cb_edit_cats_debug(GtkWidget *widget, gpointer data)
{
   struct dialog_cats_data *Pdata;
   int i;

   Pdata=data;
   for (i=0; i<16; i++) {
      printf("cai %2d [%16s] ID %3d %d: [%16s] ID %3d %d\n", i,
	     Pdata->cai1.name[i], Pdata->cai1.ID[i], Pdata->cai1.renamed[i],
	     Pdata->cai2.name[i], Pdata->cai2.ID[i], Pdata->cai2.renamed[i]);
   }
}
#endif

static void cb_dialog_button(GtkWidget *widget, gpointer data)
{
   struct dialog_cats_data *Pdata;
   GtkWidget *w;

   w = gtk_widget_get_toplevel(widget);
   Pdata = gtk_object_get_data(GTK_OBJECT(w), "dialog_cats_data");
   Pdata->button_hit=GPOINTER_TO_INT(data);

   gtk_widget_destroy(GTK_WIDGET(w));
}

static gboolean cb_destroy_dialog(GtkWidget *widget)
{
   gtk_main_quit();

   return TRUE;
}

int edit_cats(GtkWidget *widget, char *db_name, struct CategoryAppInfo *cai)
{
   GtkWidget *button;
   GtkWidget *hbox, *vbox;
   GtkWidget *vbox1, *vbox2;
   GtkWidget *dialog;
   GtkWidget *clist;
   GtkWidget *entry;
   GtkWidget *separator;
   GtkWidget *label;
   struct dialog_cats_data Pdata;
   int i;
   long char_set;
   char *titles[]={"category name"};
   gchar *empty_line[] = {""};

   jp_logf(JP_LOG_DEBUG, "edit_cats\n");

   Pdata.selected=-1;
   Pdata.state=EDIT_CAT_START;
   strncpy(Pdata.db_name, db_name, 16);
   Pdata.db_name[15]='\0';
#ifdef EDIT_CATS_DEBUG
   for (i = 0; i < 16; i++) {
      if (cai->name[i][0] != '\0') {
	 printf("cat %d [%s] ID %d renamed %d\n", i, cai->name[i],
		cai->ID[i], cai->renamed[i]);
      }
   }
#endif
   get_pref(PREF_CHAR_SET, &char_set, NULL);
   if (char_set != CHAR_SET_LATIN1) {
      for (i = 0; i < 16; i++) {
	 if (cai->name[i][0] != '\0') {
	    charset_p2j((unsigned char*)cai->name[i], 16, char_set);
	 }
      }
   }

   dialog = gtk_widget_new(GTK_TYPE_WINDOW,
			   "type", GTK_WINDOW_TOPLEVEL,
			   "title", _("Edit Categories"),
			   NULL);

   gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_MOUSE);

   gtk_signal_connect(GTK_OBJECT(dialog), "destroy",
                      GTK_SIGNAL_FUNC(cb_destroy_dialog), dialog);

   gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);

   gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(gtk_widget_get_toplevel(widget)));

   hbox = gtk_hbox_new(FALSE, 0);

   gtk_container_set_border_width(GTK_CONTAINER(hbox), 5);

   gtk_container_add(GTK_CONTAINER(dialog), hbox);

   vbox1 = gtk_vbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(hbox), vbox1, FALSE, FALSE, 1);

   vbox2 = gtk_vbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(hbox), vbox2, FALSE, FALSE, 1);

   clist = gtk_clist_new_with_titles(1, titles);

   gtk_clist_set_column_auto_resize(GTK_CLIST(clist), 0, TRUE);

   Pdata.clist = clist;
   gtk_clist_set_selection_mode(GTK_CLIST(clist), GTK_SELECTION_SINGLE);
   gtk_signal_connect(GTK_OBJECT(clist), "select_row",
		      GTK_SIGNAL_FUNC(cb_clist_edit_cats), &Pdata);
   gtk_box_pack_start(GTK_BOX(vbox1), clist, TRUE, TRUE, 1);

   /* Buttons */
   hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, FALSE, 1);

   button = gtk_button_new_with_label(_("New"));
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_edit_button),
		      GINT_TO_POINTER(EDIT_CAT_NEW));
   gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 1);

   button = gtk_button_new_with_label(_("Rename"));
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_edit_button),
		      GINT_TO_POINTER(EDIT_CAT_RENAME));
   gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 1);

   button = gtk_button_new_with_label(_("Delete"));
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_edit_button),
		      GINT_TO_POINTER(EDIT_CAT_DELETE));
   gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 1);

   Pdata.button_box = hbox;

   /* Edit entry and boxes, etc */
   vbox = gtk_vbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox2), vbox, FALSE, FALSE, 10);
   separator = gtk_hseparator_new();
   gtk_box_pack_start(GTK_BOX(vbox), separator, FALSE, FALSE, 0);
   label = gtk_label_new("");
   gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
   hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 1);
   separator = gtk_hseparator_new();
   gtk_box_pack_start(GTK_BOX(vbox), separator, FALSE, FALSE, 0);

   Pdata.label = label;

   entry = gtk_entry_new_with_max_length(16);
   gtk_signal_connect(GTK_OBJECT(entry), "activate",
		      GTK_SIGNAL_FUNC(cb_edit_button),
		      GINT_TO_POINTER(EDIT_CAT_ENTRY_OK));
   gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, FALSE, 0);
   button = gtk_button_new_with_label(_("OK"));
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_edit_button),
		      GINT_TO_POINTER(EDIT_CAT_ENTRY_OK));
   gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 1);

   button = gtk_button_new_with_label(_("Cancel"));
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_edit_button),
		      GINT_TO_POINTER(EDIT_CAT_ENTRY_CANCEL));
   gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 1);

   Pdata.entry_box = vbox;
   Pdata.entry = entry;


   for (i=0; i<16; i++) {
      gtk_clist_append(GTK_CLIST(clist), empty_line);
      if (cai->name[i][0] != '\0') {
	 gtk_clist_set_text(GTK_CLIST(clist), i, 0, cai->name[i]);
      }
   }

   /* Button Box */
   hbox = gtk_hbox_new(TRUE, 2);
   gtk_container_set_border_width(GTK_CONTAINER(hbox), 5);
   gtk_box_pack_start(GTK_BOX(vbox1), hbox, FALSE, FALSE, 2);

   /* Buttons */
   button = gtk_button_new_with_label(_("OK"));
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_dialog_button),
		      GINT_TO_POINTER(DIALOG_SAID_1));
   gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 1);

   button = gtk_button_new_with_label(_("Cancel"));
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_dialog_button),
		      GINT_TO_POINTER(DIALOG_SAID_2));
   gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 1);

#ifdef EDIT_CATS_DEBUG
   button = gtk_button_new_with_label(_("debug"));
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_edit_cats_debug), &Pdata);
   gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 1);
#endif

   /* Set the default button pressed to CANCEL */
   /* Initialize data structure */
   Pdata.button_hit = DIALOG_SAID_2;
   memcpy(&(Pdata.cai1), cai, sizeof(struct CategoryAppInfo));
   memcpy(&(Pdata.cai2), cai, sizeof(struct CategoryAppInfo));

   gtk_object_set_data(GTK_OBJECT(dialog), "dialog_cats_data", &Pdata);

   gtk_widget_show_all(dialog);

   gtk_widget_hide(Pdata.entry_box);

   gtk_main();
   
   /* OK, we're back */
#ifdef EDIT_CATS_DEBUG
   if (Pdata.button_hit==DIALOG_SAID_1) {
      printf("pressed 1\n");
   }
#endif
   if (Pdata.button_hit==DIALOG_SAID_2) {
#ifdef EDIT_CATS_DEBUG
      printf("pressed 2\n");
#endif
      return DIALOG_SAID_2;
   }
#ifdef EDIT_CATS_DEBUG
   for (i=0; i<16; i++) {
      printf("name %d [%s] ID %d [%s] ID %d\n", i,
			 Pdata.cai1.name[i], Pdata.cai1.ID[i],
			 Pdata.cai2.name[i], Pdata.cai2.ID[i]);
   }
#endif

   get_pref(PREF_CHAR_SET, &char_set, NULL);
   if (char_set != CHAR_SET_LATIN1) {
      for (i = 0; i < 16; i++) {
	 if (Pdata.cai2.name[i][0] != '\0') {
	    charset_j2p((unsigned char*)Pdata.cai2.name[i], 16, char_set);
	 }
      }
   }

   memcpy(cai, &(Pdata.cai2), sizeof(struct CategoryAppInfo));

   return 0;
}

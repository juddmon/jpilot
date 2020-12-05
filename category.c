/*******************************************************************************
 * category.c
 * A module of J-Pilot http://jpilot.org
 *
 * Copyright (C) 2009-2014 by Judd Montgomery
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

/********************************* Includes ***********************************/
#include "config.h"
#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <utime.h>
#include <unistd.h>
#include <stdio.h>
#include <pi-file.h>

#include "i18n.h"
#include "utils.h"
#include "log.h"
#include "prefs.h"

/********************************* Constants **********************************/
#define EDIT_CAT_START        100
#define EDIT_CAT_NEW          101
#define EDIT_CAT_RENAME       102
#define EDIT_CAT_DELETE       103
#define EDIT_CAT_ENTRY_OK     104
#define EDIT_CAT_ENTRY_CANCEL 105

/* #define EDIT_CATS_DEBUG 1 */

/****************************** Prototypes ************************************/
struct dialog_cats_data {
    int button_hit;
    int selected;
    int state;
    GtkTreeView *treeView;
    GtkListStore *listStore;
    GtkWidget *button_box;
    GtkWidget *entry_box;
    GtkWidget *entry;
    GtkWidget *label;
    char *entryText;
    char db_name[16];
    struct CategoryAppInfo cai1;
    struct CategoryAppInfo cai2;
};

enum {
    CATEGORY_TITLE_COLUMN_ENUM,
    CATEGORY_DATA_COLUMN_ENUM,
    CATEGORY_NUM_COLS
};

/****************************** Main Code *************************************/
static int count_records_in_cat(char *db_name, int cat_index) {
    GList *records;
    GList *temp_list;
    int count, num;
    buf_rec *br;

    jp_logf(JP_LOG_DEBUG, "count_records_in_cat\n");

    count = 0;

    num = jp_read_DB_files(db_name, &records);
    if (-1 == num)
        return 0;

    for (temp_list = records; temp_list; temp_list = temp_list->next) {
        if (temp_list->data) {
            br = temp_list->data;
        } else {
            continue;
        }

        if (!br->buf) continue;
        if ((br->rt == DELETED_PALM_REC) ||
            (br->rt == DELETED_PC_REC) ||
            (br->rt == MODIFIED_PALM_REC))
            continue;
        if ((br->attrib & 0x0F) != cat_index) continue;

        count++;
    }

    jp_free_DB_records(&records);

    jp_logf(JP_LOG_DEBUG, "Leaving count_records_in_cat()\n");

    return count;
}

static int edit_cats_delete_cats_pc3(char *DB_name, int cat) {
    char local_pc_file[FILENAME_MAX];
    FILE *pc_in;
    PC3RecordHeader header;
    int num;
    int rec_len;
    int count = 0;

    g_snprintf(local_pc_file, sizeof(local_pc_file), "%s.pc3", DB_name);

    pc_in = jp_open_home_file(local_pc_file, "r+");
    if (pc_in == NULL) {
        jp_logf(JP_LOG_WARN, _("Unable to open file: %s\n"), local_pc_file);
        return EXIT_FAILURE;
    }

    while (!feof(pc_in)) {
        num = read_header(pc_in, &header);
        if (num != 1) {
            if (ferror(pc_in)) break;
            if (feof(pc_in)) break;
        }

        rec_len = (int) header.rec_len;
        if (rec_len > 0x10000) {
            jp_logf(JP_LOG_WARN, _("PC file corrupt?\n"));
            fclose(pc_in);
            return EXIT_FAILURE;
        }
        if (((header.rt == NEW_PC_REC) || (header.rt == REPLACEMENT_PALM_REC)) &&
            ((header.attrib & 0x0F) == cat)) {
            if (fseek(pc_in, -(header.header_len), SEEK_CUR)) {
                jp_logf(JP_LOG_WARN, _("fseek failed - fatal error\n"));
                fclose(pc_in);
                return EXIT_FAILURE;
            }
            header.rt = DELETED_PC_REC;
            write_header(pc_in, &header);
            count++;
        }
        /* Skip this record now that we are done with it */
        if (fseek(pc_in, rec_len, SEEK_CUR)) {
            jp_logf(JP_LOG_WARN, _("fseek failed - fatal error\n"));
            fclose(pc_in);
            return EXIT_FAILURE;
        }
    }

    fclose(pc_in);
    return count;
}

/* Helper routine to change categories.
 * Function changes category regardless of record type */
static int _edit_cats_change_cats_pc3(char *DB_name,
                                      int old_cat, int new_cat,
                                      int swap) {
    char local_pc_file[FILENAME_MAX];
    FILE *pc_in;
    PC3RecordHeader header;
    int rec_len;
    int num;
    int current_cat;
    int count = 0;

    g_snprintf(local_pc_file, sizeof(local_pc_file), "%s.pc3", DB_name);

    pc_in = jp_open_home_file(local_pc_file, "r+");
    if (pc_in == NULL) {
        jp_logf(JP_LOG_WARN, _("Unable to open file: %s\n"), local_pc_file);
        return EXIT_FAILURE;
    }

    while (!feof(pc_in)) {
        num = read_header(pc_in, &header);
        if (num != 1) {
            if (ferror(pc_in)) break;
            if (feof(pc_in)) break;
        }
        rec_len = (int) header.rec_len;
        if (rec_len > 0x10000) {
            jp_logf(JP_LOG_WARN, _("PC file corrupt?\n"));
            fclose(pc_in);
            return EXIT_FAILURE;
        }

        current_cat = header.attrib & 0x0F;
        if (current_cat == old_cat) {
            if (fseek(pc_in, -(header.header_len), SEEK_CUR)) {
                jp_logf(JP_LOG_WARN, _("fseek failed - fatal error\n"));
                fclose(pc_in);
                return EXIT_FAILURE;
            }
            header.attrib = (unsigned char) ((header.attrib & 0xFFFFFFF0) | new_cat);
            write_header(pc_in, &header);
            count++;
        }
        if ((swap) && (current_cat == new_cat)) {
            if (fseek(pc_in, -(header.header_len), SEEK_CUR)) {
                jp_logf(JP_LOG_WARN, _("fseek failed - fatal error\n"));
                fclose(pc_in);
                return EXIT_FAILURE;
            }
            header.attrib = (unsigned char) ((header.attrib & 0xFFFFFFF0) | old_cat);
            write_header(pc_in, &header);
            count++;
        }
        /* Skip the rest of the record now that we are done with it */
        if (fseek(pc_in, rec_len, SEEK_CUR)) {
            jp_logf(JP_LOG_WARN, _("fseek failed - fatal error\n"));
            fclose(pc_in);
            return EXIT_FAILURE;
        }
    }

    fclose(pc_in);
    return count;
}

/* Exported routine to change categories in PC3 file */
int edit_cats_change_cats_pc3(char *DB_name, int old_cat, int new_cat) {
    return _edit_cats_change_cats_pc3(DB_name, old_cat, new_cat, FALSE);
}

/* Exported routine to swap categories in PC3 file */
int edit_cats_swap_cats_pc3(char *DB_name, int old_cat, int new_cat) {
    return _edit_cats_change_cats_pc3(DB_name, old_cat, new_cat, TRUE);
}

/*
 * This routine changes records from old_cat to new_cat.
 *  It does not modify the local pdb file.
 *  It does this by writing a modified record to the pc3 file.
 */
int edit_cats_change_cats_pdb(char *DB_name, int old_cat, int new_cat) {
    GList *temp_list;
    GList *records;
    buf_rec *br;
    int num, count;

    jp_logf(JP_LOG_DEBUG, "edit_cats_change_cats_pdb\n");

    count = 0;
    num = jp_read_DB_files(DB_name, &records);
    if (-1 == num)
        return 0;

    for (temp_list = records; temp_list; temp_list = temp_list->next) {
        if (temp_list->data) {
            br = temp_list->data;
        } else {
            continue;
        }

        if (!br->buf) continue;
        if ((br->rt == DELETED_PALM_REC) || (br->rt == MODIFIED_PALM_REC)) continue;

        if ((br->attrib & 0x0F) == old_cat) {
            if (new_cat == -1) {
                /* write a deleted rec */
                jp_delete_record(DB_name, br, DELETE_FLAG);
                count++;
            } else {
                /* write a deleted rec */
                br->attrib = (unsigned char) ((br->attrib & 0xFFFFFFF0) | (new_cat & 0x0F));
                jp_delete_record(DB_name, br, MODIFY_FLAG);
                br->rt = REPLACEMENT_PALM_REC;
                jp_pc_write(DB_name, br);
                count++;
            }
        }
    }

    jp_free_DB_records(&records);

    return count;
}

/*
 * This helper routine changes the category index of records in the pdb file.
 * It will change all old_index records to new_index and with the swap 
 * option will also change new_index records to old_index ones.
 * Returns the number of records where the category was changed.
 */
static int _change_cat_pdb(char *DB_name,
                           int old_index, int new_index,
                           int swap) {
    char local_pdb_file[FILENAME_MAX];
    char full_local_pdb_file[FILENAME_MAX];
    char full_local_pdb_file2[FILENAME_MAX];
    pi_file_t *pf1, *pf2;
    struct DBInfo infop;
    void *app_info;
    void *sort_info;
    void *record;
    size_t size;
    pi_uid_t uid;
    struct stat statb;
    struct utimbuf times;
    int idx;
    int attr;
    int cat;
    int count;

    jp_logf(JP_LOG_DEBUG, "_change_cat_pdb\n");

    g_snprintf(local_pdb_file, sizeof(local_pdb_file), "%s.pdb", DB_name);
    get_home_file_name(local_pdb_file, full_local_pdb_file, sizeof(full_local_pdb_file));
    strcpy(full_local_pdb_file2, full_local_pdb_file);
    strcat(full_local_pdb_file2, "2");

    /* After we are finished, set the create and modify times of new file
       to the same as the old */
    stat(full_local_pdb_file, &statb);
    times.actime = statb.st_atime;
    times.modtime = statb.st_mtime;

    pf1 = pi_file_open(full_local_pdb_file);
    if (!pf1) {
        jp_logf(JP_LOG_WARN, _("Unable to open file: %s\n"), full_local_pdb_file);
        return EXIT_FAILURE;
    }
    pi_file_get_info(pf1, &infop);
    pf2 = pi_file_create(full_local_pdb_file2, &infop);
    if (!pf2) {
        jp_logf(JP_LOG_WARN, _("Unable to open file: %s\n"), full_local_pdb_file2);
        return EXIT_FAILURE;
    }

    pi_file_get_app_info(pf1, &app_info, &size);
    pi_file_set_app_info(pf2, app_info, size);

    pi_file_get_sort_info(pf1, &sort_info, &size);
    pi_file_set_sort_info(pf2, sort_info, size);

    idx = 0;
    count = 0;
    while ((pi_file_read_record(pf1, idx, &record, &size, &attr, &cat, &uid)) > 0) {
        if (cat == old_index) {
            cat = new_index;
            count++;
        } else if ((swap) && (cat == new_index)) {
            cat = old_index;
            count++;
        }
        pi_file_append_record(pf2, record, size, attr, cat, uid);
        idx++;
    }

    pi_file_close(pf1);
    pi_file_close(pf2);

    if (rename(full_local_pdb_file2, full_local_pdb_file) < 0) {
        jp_logf(JP_LOG_WARN, "pdb_file_change_indexes(): %s\n, ", _("rename failed"));
    }

    utime(full_local_pdb_file, &times);

    return EXIT_SUCCESS;
}

/* Exported routine to change categories in pdb file */
int pdb_file_change_indexes(char *DB_name, int old_cat, int new_cat) {
    return _change_cat_pdb(DB_name, old_cat, new_cat, FALSE);
}

/* Exported routine to swap categories in pdb file */
int pdb_file_swap_indexes(char *DB_name, int old_cat, int new_cat) {
    return _change_cat_pdb(DB_name, old_cat, new_cat, TRUE);
}

/*
 * This routine changes deletes records in category cat.
 *  It does not modify a local pdb file.
 *  It does this by writing a modified record to the pc3 file.
 */
static int edit_cats_delete_cats_pdb(char *DB_name, int cat) {
    jp_logf(JP_LOG_DEBUG, "edit_cats_delete_cats_pdb\n");

    return edit_cats_change_cats_pdb(DB_name, cat, -1);
}

gboolean
deleteCategory(GtkTreeModel *model,
               GtkTreePath *path,
               GtkTreeIter *iter,
               gpointer data) {
    struct dialog_cats_data *Pdata;
    Pdata = data;
    int *i = gtk_tree_path_get_indices(path);
    if (Pdata->selected == i[0]) {
        gtk_list_store_remove(GTK_LIST_STORE(Pdata->listStore), iter);
        return TRUE;
    }
    return FALSE;
}

gboolean
renameCategory(GtkTreeModel *model,
               GtkTreePath *path,
               GtkTreeIter *iter,
               gpointer data) {
    struct dialog_cats_data *Pdata;
    char *text;
    Pdata = data;
    int *i = gtk_tree_path_get_indices(path);
    if (Pdata->selected == i[0]) {
        text = Pdata->entryText;
        gtk_list_store_set(GTK_LIST_STORE(Pdata->listStore), iter, CATEGORY_TITLE_COLUMN_ENUM, text, -1);
        return TRUE;
    }
    return FALSE;
}

gboolean
editCategoryRename(GtkTreeModel *model,
                   GtkTreePath *path,
                   GtkTreeIter *iter,
                   gpointer data) {
    struct dialog_cats_data *Pdata;
    char *text;
    Pdata = data;

    int *i = gtk_tree_path_get_indices(path);
    if (Pdata->selected == i[0]) {
        gtk_tree_model_get(GTK_TREE_MODEL(Pdata->listStore), iter, CATEGORY_TITLE_COLUMN_ENUM, &text,-1);
        gtk_label_set_text(GTK_LABEL(Pdata->label), _("Enter New Category Name"));
        gtk_entry_set_text(GTK_ENTRY(Pdata->entry), text);
        gtk_widget_show(Pdata->entry_box);
        gtk_widget_hide(Pdata->button_box);
        gtk_widget_grab_focus(GTK_WIDGET(Pdata->entry));
        Pdata->state = EDIT_CAT_RENAME;
        return TRUE;
    }
    return FALSE;
}

gboolean
selectCategoryRecordByRow(GtkTreeModel *model,
                          GtkTreePath *path,
                          GtkTreeIter *iter,
                          gpointer data) {
    struct dialog_cats_data *Pdata;
    Pdata = data;
    int *i = gtk_tree_path_get_indices(path);
    int rowToCheck = Pdata->selected;
    //rowToCheck is > than number of rows, so select the last row.
    if (Pdata->selected >= gtk_tree_model_iter_n_children(GTK_TREE_MODEL(model), NULL)) {
        rowToCheck--;
    }
    if (i[0] == rowToCheck) {
        GtkTreeSelection *selection = NULL;
        selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(Pdata->treeView));
        gtk_tree_selection_select_path(selection, path);
        gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(Pdata->treeView), path, CATEGORY_TITLE_COLUMN_ENUM, FALSE, 1.0, 0.0);
        return TRUE;
    }

    return FALSE;
}

static void cb_edit_button(GtkWidget *widget, gpointer data) {
    struct dialog_cats_data *Pdata;
    int i, r, count;
    int j;
    long char_set;
    int id;
    int button;
    int catnum;
     char pilotentry[HOSTCAT_NAME_SZ];   /* entry text, in Pilot character set */
    char *button_text[] = {N_("OK")};
    char *move_text[] = {N_("Move"), N_("Delete"), N_("Cancel")};
    const char *entry_text;
    char temp[256];

    get_pref(PREF_CHAR_SET, &char_set, NULL); /* JPA be prepared to make conversions */
    button = GPOINTER_TO_INT(data);
    Pdata =  g_object_get_data(G_OBJECT(gtk_widget_get_toplevel(widget)), "dialog_cats_data");

    /* JPA get the selected category number */
    catnum = 0;
    i = 0;
    while ((i <= Pdata->selected) && (catnum < NUM_CATEGORIES)) {
        if (Pdata->cai2.name[++catnum][0]) i++;
    }
    if (catnum >= NUM_CATEGORIES) catnum = -1;  /* not found */

    if (Pdata) {
        switch (button) {
            case EDIT_CAT_NEW:
                count = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(Pdata->listStore), NULL);
                if (count > NUM_CATEGORIES - 2) {
                    dialog_generic(GTK_WINDOW(gtk_widget_get_toplevel(widget)),
                                   _("Edit Categories"), DIALOG_ERROR,
                                   _("The maximum number of categories (16) are already used"), 1, button_text);
                    return;
                }
                gtk_label_set_text(GTK_LABEL(Pdata->label), _("Enter New Category"));
                gtk_entry_set_text(GTK_ENTRY(Pdata->entry), "");
                gtk_widget_show(Pdata->entry_box);
                gtk_widget_hide(Pdata->button_box);
                gtk_widget_grab_focus(GTK_WIDGET(Pdata->entry));
                Pdata->state = EDIT_CAT_NEW;
                break;

            case EDIT_CAT_RENAME:
                if ((catnum < 0) || (Pdata->cai2.name[catnum][0] == '\0')) {
                    dialog_generic(GTK_WINDOW(gtk_widget_get_toplevel(widget)),
                                   _("Edit Categories Error"), DIALOG_ERROR,
                                   _("You must select a category to rename"), 1, button_text);
                    return;
                }
#ifdef EDIT_CATS_DEBUG
                if (catnum == 0) {
                   printf("Trying to rename category 0!\n");
                }
#endif
                gtk_tree_model_foreach(GTK_TREE_MODEL(Pdata->listStore), editCategoryRename, Pdata);

                break;

            case EDIT_CAT_DELETE:
#ifdef EDIT_CATS_DEBUG
                printf("delete cat\n");
#endif
                if (catnum < 0) {
                    dialog_generic(GTK_WINDOW(gtk_widget_get_toplevel(widget)),
                                   _("Edit Categories Error"), DIALOG_ERROR,
                                   _("You must select a category to delete"), 1, button_text);
                    return;
                }
#ifdef EDIT_CATS_DEBUG
                if (catnum == 0) {
                   printf("Trying to delete category 0!\n");
                }
#endif
                /* Check if category is empty */
                if (Pdata->cai2.name[catnum][0] == '\0') {
                    return;
                }
                /* Check to see if there are records are in this category */
                count = count_records_in_cat(Pdata->db_name, catnum);
#ifdef EDIT_CATS_DEBUG
                printf("count=%d\n", count);
#endif
                if (count > 0) {
                    g_snprintf(temp, sizeof(temp), _("There are %d records in %s.\n"
                                                     "Do you want to move them to %s, or delete them?"),
                               count, Pdata->cai1.name[catnum], Pdata->cai1.name[0]);
                    r = dialog_generic(GTK_WINDOW(gtk_widget_get_toplevel(widget)),
                                       _("Edit Categories"), DIALOG_QUESTION,
                                       temp, 3, move_text);
                    switch (r) {
                        case DIALOG_SAID_1:
#ifdef EDIT_CATS_DEBUG
                            printf("MOVE THEM\n");
#endif
                            r = edit_cats_change_cats_pc3(Pdata->db_name, catnum, 0);
#ifdef EDIT_CATS_DEBUG
                            printf("moved %d pc records\n", r);
#endif
                            r = edit_cats_change_cats_pdb(Pdata->db_name, catnum, 0);
#ifdef EDIT_CATS_DEBUG
                            printf("moved %d pdb->pc records\n", r);
#endif
                            break;
                        case DIALOG_SAID_2:
#ifdef EDIT_CATS_DEBUG
                            printf("DELETE THEM\n");
#endif
                            r = edit_cats_delete_cats_pc3(Pdata->db_name, catnum);
#ifdef EDIT_CATS_DEBUG
                            printf("deleted %d pc records\n", r);
#endif
                            r = edit_cats_delete_cats_pdb(Pdata->db_name, catnum);
#ifdef EDIT_CATS_DEBUG
                            printf("deleted %d pdb->pc records\n", r);
#endif
                            break;
                        case DIALOG_SAID_3:
#ifdef EDIT_CATS_DEBUG
                            printf("Delete Canceled\n");
#endif
                            return;
                        default:
                            return;
                    }
                }
                /* delete the category */
#ifdef EDIT_CATS_DEBUG
                printf("DELETE category\n");
#endif
                Pdata->cai2.name[catnum][0] = '\0';
                Pdata->cai2.ID[catnum] = Pdata->cai1.ID[catnum];
                Pdata->cai2.renamed[catnum] = 0;
                /* JPA move category names upward in listbox */
                /* we get the old text from listbox, to avoid making */
                /* character set conversions */
                gtk_tree_model_foreach(GTK_TREE_MODEL(Pdata->listStore), deleteCategory, Pdata);
                gtk_tree_model_foreach(GTK_TREE_MODEL(Pdata->listStore), selectCategoryRecordByRow, Pdata);

                break;

            case EDIT_CAT_ENTRY_OK:
                if ((Pdata->state != EDIT_CAT_RENAME) && (Pdata->state != EDIT_CAT_NEW)) {
                    jp_logf(JP_LOG_WARN, _("invalid state file %s line %d\n"), __FILE__, __LINE__);
                    return;
                }
                entry_text = gtk_entry_get_text(GTK_ENTRY(Pdata->entry));

                /* Can't make an empty category, could do a dialog telling user */
                if ((!entry_text) || (!entry_text[0])) {
                    return;
                }

                if ((Pdata->state == EDIT_CAT_RENAME) || (Pdata->state == EDIT_CAT_NEW)) {
                    /* Check for category being used more than once */
                    /* moved here by JPA for use in both new and rename cases */
                    /* JPA convert entry to Pilot character set before checking */
                    /* note : don't know Pilot size until converted */
                    g_strlcpy(pilotentry, entry_text, HOSTCAT_NAME_SZ);
                    charset_j2p(pilotentry, HOSTCAT_NAME_SZ, char_set);
                    pilotentry[PILOTCAT_NAME_SZ - 1] = '\0';
                    for (i = 0; i < NUM_CATEGORIES; i++) {
                        /* JPA allow a category to be renamed to its previous name */
                        if (((i != catnum) || (Pdata->state != EDIT_CAT_RENAME))
                            && !strcmp(pilotentry, Pdata->cai2.name[i])) {
                            sprintf(temp, _("The category %s can't be used more than once"), entry_text);
                            dialog_generic(GTK_WINDOW(gtk_widget_get_toplevel(widget)),
                                           _("Edit Categories"), DIALOG_ERROR,
                                           temp, 1, button_text);
                            return;
                        }
                    }
                }
                if (Pdata->state == EDIT_CAT_RENAME) {
#ifdef EDIT_CATS_DEBUG
                    printf("rename cat\n");
#endif
                    Pdata->entryText = (char *) entry_text;
                    gtk_tree_model_foreach(GTK_TREE_MODEL(Pdata->listStore), renameCategory, Pdata);
                    /* JPA enter new category name in Palm Pilot character set */
                    charset_j2p((char *) entry_text, HOSTCAT_NAME_SZ, char_set);
                    g_strlcpy(Pdata->cai2.name[catnum], entry_text, PILOTCAT_NAME_SZ);
                    Pdata->cai2.renamed[catnum] = 1;

                }

                if (Pdata->state == EDIT_CAT_NEW) {
#ifdef EDIT_CATS_DEBUG
                    printf("new cat\n");
#endif
                    /* JPA have already checked category is not being used more than once */
                    /* Find a new category ID */
                    id = 128;
                    for (i = 1; i < NUM_CATEGORIES; i++) {
                        if (Pdata->cai2.ID[i] == id) {
                            id++;
                            i = 1;
                        }
                    }
                    /* Find an empty slot */
                    /* When the new button was pressed we already checked for an empty slot */

                    GtkTreeIter iter;
                    gtk_list_store_append(Pdata->listStore, &iter);
                    gtk_list_store_set(Pdata->listStore, &iter, CATEGORY_TITLE_COLUMN_ENUM, entry_text, -1);
                    for (i = 1; i < NUM_CATEGORIES; i++) {
                        if (Pdata->cai2.name[i][0] == '\0') {
#ifdef EDIT_CATS_DEBUG
                            printf("slot %d is empty\n", i);
#endif
                            /* JPA get the old text from listbox, to avoid making */
                            /* character set conversions */
                            strcpy(Pdata->cai2.name[i], pilotentry);
                            Pdata->cai2.ID[i] = (unsigned char) id;
                            Pdata->cai2.renamed[i] = 1;

                            break;
                        }
                    }
                }
                gtk_widget_hide(Pdata->entry_box);
                gtk_widget_show(Pdata->button_box);
                Pdata->state = EDIT_CAT_START;
                break;

            case EDIT_CAT_ENTRY_CANCEL:
                gtk_widget_hide(Pdata->entry_box);
                gtk_widget_show(Pdata->button_box);
                Pdata->state = EDIT_CAT_START;
                break;

            default:
                jp_logf(JP_LOG_WARN, "cb_edit_button(): %s\n", "unknown button");
        }
    }
}

static gboolean handleCategorySelection(GtkTreeSelection *selection,
                                        GtkTreeModel *model,
                                        GtkTreePath *path,
                                        gboolean path_currently_selected,
                                        gpointer data) {
    struct dialog_cats_data *Pdata;
    GtkTreeIter iter;
    Pdata = data;
    if ((gtk_tree_model_get_iter(model, &iter, path)) && (!path_currently_selected)) {
        int *i = gtk_tree_path_get_indices(path);
        if (Pdata->state == EDIT_CAT_START) {
            Pdata->selected = i[0];
        }
    }
    return TRUE;
}


#ifdef EDIT_CATS_DEBUG
static void cb_edit_cats_debug(GtkWidget *widget, gpointer data)
{
   struct dialog_cats_data *Pdata;
   int i;

   Pdata=data;
   for (i=0; i<NUM_CATEGORIES; i++) {
      printf("cai %2d [%16s] ID %3d %d: [%16s] ID %3d %d\n", i,
             Pdata->cai1.name[i], Pdata->cai1.ID[i], Pdata->cai1.renamed[i],
             Pdata->cai2.name[i], Pdata->cai2.ID[i], Pdata->cai2.renamed[i]);
   }
}
#endif

static void cb_dialog_button(GtkWidget *widget, gpointer data) {
    struct dialog_cats_data *Pdata;
    GtkWidget *w;

    w = gtk_widget_get_toplevel(widget);
    Pdata =  g_object_get_data(G_OBJECT(w), "dialog_cats_data");
    Pdata->button_hit = GPOINTER_TO_INT(data);

    gtk_widget_destroy(GTK_WIDGET(w));
}

static gboolean cb_destroy_dialog(GtkWidget *widget) {
    gtk_main_quit();

    return TRUE;
}

int edit_cats(GtkWidget *widget, char *db_name, struct CategoryAppInfo *cai) {
    GtkWidget *button;
    GtkWidget *hbox, *vbox;
    GtkWidget *vbox1, *vbox2, *vbox3;
    GtkWidget *dialog;
    GtkListStore *listStore;
    GtkTreeView *treeView;
    GtkWidget *entry;
    GtkWidget *label;
    GtkWidget *separator;
    struct dialog_cats_data Pdata;
    int i, j;
    long char_set;
    char *catname_hchar;    /* Category names in host character set */
    char *titles[2] = {_("Category")};
    gchar *empty_line[] = {""};

    jp_logf(JP_LOG_DEBUG, "edit_cats\n");

    Pdata.selected = -1;
    Pdata.state = EDIT_CAT_START;
    g_strlcpy(Pdata.db_name, db_name, 16);
#ifdef EDIT_CATS_DEBUG
    for (i = 0; i < NUM_CATEGORIES; i++) {
       if (cai->name[i][0] != '\0') {
          printf("cat %d [%s] ID %d renamed %d\n", i, cai->name[i],
                 cai->ID[i], cai->renamed[i]);
       }
    }
#endif

/* removed by JPA : do not change category names as they will
 * be written back to file.
 *  We will however have to make a conversion for host dialog display
 *
 *   get_pref(PREF_CHAR_SET, &char_set, NULL);
 *   if (char_set != CHAR_SET_LATIN1) {
 *      for (i = 0; i < 16; i++) {
 *       if (cai->name[i][0] != '\0') {
 *          charset_p2j((unsigned char*)cai->name[i], 16, char_set);
 *       }
 *      }
 *   }
 *
 */

    dialog = gtk_widget_new(GTK_TYPE_WINDOW,
                            "type", GTK_WINDOW_TOPLEVEL,
                            "title", _("Edit Categories"),
                            NULL);

    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_MOUSE);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(gtk_widget_get_toplevel(widget)));

    g_signal_connect(G_OBJECT(dialog), "destroy",
                       G_CALLBACK(cb_destroy_dialog), dialog);

    vbox3 = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(dialog), vbox3);

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 5);
    gtk_container_add(GTK_CONTAINER(vbox3), hbox);

    vbox1 = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), vbox1, FALSE, FALSE, 1);

    vbox2 = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), vbox2, FALSE, FALSE, 1);
    listStore = gtk_list_store_new(CATEGORY_NUM_COLS, G_TYPE_STRING, G_TYPE_INT);
    treeView = gtk_tree_view_new_with_model(GTK_TREE_MODEL(listStore));
    GtkCellRenderer *titleRenderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *titleColumn = gtk_tree_view_column_new_with_attributes("Category",
                                                                              titleRenderer,
                                                                              "text", CATEGORY_TITLE_COLUMN_ENUM,
                                                                              NULL);
    gtk_tree_view_column_set_clickable(titleColumn, gtk_false());
    gtk_tree_view_column_set_min_width(titleColumn, 100);
    gtk_tree_view_column_set_sizing(titleColumn, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    gtk_tree_view_insert_column(treeView, titleColumn, CATEGORY_TITLE_COLUMN_ENUM);
    gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(treeView)),
                                GTK_SELECTION_BROWSE);

    gtk_tree_selection_set_select_function(gtk_tree_view_get_selection(GTK_TREE_VIEW(treeView)),
                                           handleCategorySelection, &Pdata, NULL);
    gtk_widget_set_events(treeView, GDK_BUTTON1_MOTION_MASK);
    g_signal_connect (G_OBJECT(treeView), "motion_notify_event",
                      G_CALLBACK(motion_notify_event), NULL);
    g_signal_connect (G_OBJECT(treeView), "button-press-event",
                      G_CALLBACK(button_pressed_for_motion), NULL);
    g_signal_connect (G_OBJECT(treeView), "button-release-event",
                      G_CALLBACK(button_released_for_motion), NULL);
    gtk_box_pack_start(GTK_BOX(vbox1), GTK_WIDGET(treeView), TRUE, TRUE, 1);

    /* Fill list with categories except for category 0, Unfiled,
     * which is not editable */
    get_pref(PREF_CHAR_SET, &char_set, NULL);
    GtkTreeIter iter;
    for (i = j = 1; i < NUM_CATEGORIES; i++, j++) {


        /* Hide void category names */
        while ((j < NUM_CATEGORIES) && ((cai->name[j][0] == '\0') || (!cai->ID[j]))) {
            /* Remove categories which have a null ID
             * to facilitate recovering from errors, */
            /* however we cannot synchronize them to the Palm Pilot */
            if (!cai->ID[j]) cai->name[j][0] = '\0';
            j++;
        }
        if (j < NUM_CATEGORIES) {
            /* Must do character set conversion from Palm to Host */
            catname_hchar = charset_p2newj(cai->name[j], PILOTCAT_NAME_SZ, (int) char_set);
            gtk_list_store_append(listStore, &iter);
            gtk_list_store_set(listStore, &iter, CATEGORY_TITLE_COLUMN_ENUM, catname_hchar, CATEGORY_DATA_COLUMN_ENUM,
                               j, -1);

            free(catname_hchar);
        }
    }


    Pdata.treeView = treeView;
    Pdata.listStore = listStore;

    /* Buttons */
    hbox = gtk_hbutton_box_new();
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 12);
    gtk_button_box_set_layout(GTK_BUTTON_BOX (hbox), GTK_BUTTONBOX_END);
    gtk_box_set_spacing(GTK_BOX(hbox), 6);
    gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, FALSE, 1);

#ifdef ENABLE_STOCK_BUTTONS
    button = gtk_button_new_with_label("_New");
#else
    button = gtk_button_new_with_label(_("New"));
#endif
    g_signal_connect(G_OBJECT(button), "clicked",
                       G_CALLBACK(cb_edit_button),
                       GINT_TO_POINTER(EDIT_CAT_NEW));
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 1);

    button = gtk_button_new_with_label(_("Rename"));
    g_signal_connect(G_OBJECT(button), "clicked",
                       G_CALLBACK(cb_edit_button),
                       GINT_TO_POINTER(EDIT_CAT_RENAME));
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 1);

#ifdef ENABLE_STOCK_BUTTONS
    button = gtk_button_new_with_label("_Delete");
#else
    button = gtk_button_new_with_label(_("Delete"));
#endif
    g_signal_connect(G_OBJECT(button), "clicked",
                       G_CALLBACK(cb_edit_button),
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
    separator = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(vbox), separator, FALSE, FALSE, 0);

    Pdata.label = label;

    entry =  gtk_entry_new();
    gtk_entry_set_max_length(entry,HOSTCAT_NAME_SZ - 1);
    g_signal_connect(G_OBJECT(entry), "activate",
                       G_CALLBACK(cb_edit_button),
                       GINT_TO_POINTER(EDIT_CAT_ENTRY_OK));
    gtk_box_pack_start(GTK_BOX(vbox), entry, FALSE, FALSE, 0);

    hbox = gtk_hbutton_box_new();
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 12);
    gtk_button_box_set_layout(GTK_BUTTON_BOX (hbox), GTK_BUTTONBOX_END);
    gtk_box_set_spacing(GTK_BOX(hbox), 6);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 1);
    button = gtk_button_new_with_label("Cancel");
    g_signal_connect(G_OBJECT(button), "clicked",
                       G_CALLBACK(cb_edit_button),
                       GINT_TO_POINTER(EDIT_CAT_ENTRY_CANCEL));
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 1);

    button = gtk_button_new_with_label("OK");
    g_signal_connect(G_OBJECT(button), "clicked",
                       G_CALLBACK(cb_edit_button),
                       GINT_TO_POINTER(EDIT_CAT_ENTRY_OK));
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 1);

    separator = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(vbox), separator, FALSE, FALSE, 0);

    Pdata.entry_box = vbox;
    Pdata.entry = entry;

    /* Button Box */
    hbox = gtk_hbutton_box_new();
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 12);
    gtk_button_box_set_layout(GTK_BUTTON_BOX (hbox), GTK_BUTTONBOX_END);
    gtk_box_set_spacing(GTK_BOX(hbox), 6);
    gtk_box_pack_start(GTK_BOX(vbox3), hbox, FALSE, FALSE, 2);

    /* Buttons */
    button = gtk_button_new_with_label("Cancel");
    g_signal_connect(G_OBJECT(button), "clicked",
                       G_CALLBACK(cb_dialog_button),
                       GINT_TO_POINTER(DIALOG_SAID_2));
    gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 1);

    button = gtk_button_new_with_label("OK");
    g_signal_connect(G_OBJECT(button), "clicked",
                       G_CALLBACK(cb_dialog_button),
                       GINT_TO_POINTER(DIALOG_SAID_1));
    gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 1);

#ifdef EDIT_CATS_DEBUG
    button = gtk_button_new_with_label("DEBUG");
    g_signal_connect(G_OBJECT(button), "clicked",
                       G_CALLBACK(cb_edit_cats_debug), &Pdata);
    gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 1);
#endif

    /* Set the default button pressed to CANCEL */
    Pdata.button_hit = DIALOG_SAID_2;
    /* Initialize data structures */
    memcpy(&(Pdata.cai1), cai, sizeof(struct CategoryAppInfo));
    memcpy(&(Pdata.cai2), cai, sizeof(struct CategoryAppInfo));
     g_object_set_data(G_OBJECT(dialog), "dialog_cats_data", &Pdata);

    gtk_widget_show_all(dialog);
    gtk_widget_hide(Pdata.entry_box);

    gtk_main();

    /* OK, back from gtk_main loop */
#ifdef EDIT_CATS_DEBUG
    if (Pdata.button_hit==DIALOG_SAID_1) {
       printf("pressed 1\n");
    }
#endif
    if (Pdata.button_hit == DIALOG_SAID_2) {
#ifdef EDIT_CATS_DEBUG
        printf("pressed 2\n");
#endif
        return DIALOG_SAID_2;
    }
#ifdef EDIT_CATS_DEBUG
    for (i=0; i<NUM_CATEGORIES; i++) {
       printf("name %d [%s] ID %d [%s] ID %d\n", i,
                          Pdata.cai1.name[i], Pdata.cai1.ID[i],
                          Pdata.cai2.name[i], Pdata.cai2.ID[i]);
    }
#endif

    memcpy(cai, &(Pdata.cai2), sizeof(struct CategoryAppInfo));

    return EXIT_SUCCESS;
}

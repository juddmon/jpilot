/*******************************************************************************
 * memo_gui.c
 * A module of J-Pilot http://jpilot.org
 *
 * Copyright (C) 1999-2014 by Judd Montgomery
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
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <pi-dlp.h>

#include "memo.h"
#include "i18n.h"
#include "utils.h"
#include "log.h"
#include "prefs.h"
#include "password.h"
#include "print.h"
#include "export.h"
#include "stock_buttons.h"

/********************************* Constants **********************************/
#define NUM_MEMO_CAT_ITEMS 16

#define MEMO_MAX_COLUMN_LEN 80
#define MEMO_LIST_CHAR_WIDTH 50

#define NUM_MEMO_CSV_FIELDS 3

#define CONNECT_SIGNALS 400
#define DISCONNECT_SIGNALS 401

/******************************* Global vars **********************************/
/* Keeps track of whether code is using Memo, or Memos database
 * 0 is Memo, 1 is Memos */
static long memo_version = 0;

extern GtkWidget *glob_date_label;
extern int glob_date_timer_tag;

static struct MemoAppInfo memo_app_info;
static int memo_category = CATEGORY_ALL;
static int row_selected;

static GtkWidget *treeView;
static GtkListStore *listStore;
static GtkWidget *memo_text;
static GObject *memo_text_buffer;
static GtkWidget *private_checkbox;
static GtkWidget *category_menu1;
static GtkWidget *category_menu2;
static GtkWidget *pane;
static struct sorted_cats sort_l[NUM_MEMO_CAT_ITEMS];
static GtkWidget *new_record_button;
static GtkWidget *apply_record_button;
static GtkWidget *add_record_button;
static GtkWidget *delete_record_button;
static GtkWidget *undelete_record_button;
static GtkWidget *copy_record_button;
static GtkWidget *cancel_record_button;
static int record_changed;

static MemoList *glob_memo_list = NULL;
static MemoList *export_memo_list = NULL;

/****************************** Prototypes ************************************/
static int memo_clear_details(void);

static int memo_redraw(void);

static void connect_changed_signals(int con_or_dis);

static int memo_find(void);

static int memo_get_details(struct Memo *new_memo, unsigned char *attrib);

static void memo_update_liststore(GtkListStore *listStore, GtkWidget *tooltip_widget,
                                  MemoList **memo_list, int category, int main);

static void cb_add_new_record(GtkWidget *widget, gpointer data);

static void cb_edit_cats(GtkWidget *widget, gpointer data);

void initializeTreeView();

gboolean handleRowSelectionForMemo(GtkTreeSelection *selection,
                                   GtkTreeModel *model,
                                   GtkTreePath *path,
                                   gboolean path_currently_selected,
                                   gpointer userdata);

gboolean
addNewRecordMemo(GtkTreeModel *model,
                 GtkTreePath *path,
                 GtkTreeIter *iter,
                 gpointer data);

gboolean deleteRecordMemo(GtkTreeModel *model,
                          GtkTreePath *path,
                          GtkTreeIter *iter,
                          gpointer data);

void delete_memo(MyMemo *mmemo, gpointer data);

void undelete_memo(MyMemo *mmemo, gpointer data);

gboolean undeleteRecordMemo(GtkTreeModel *model,
                            GtkTreePath *path,
                            GtkTreeIter *iter,
                            gpointer data);

gboolean printRecordMemo(GtkTreeModel *model,
                         GtkTreePath *path,
                         GtkTreeIter *iter,
                         gpointer data);

gboolean
findRecordMemo(GtkTreeModel *model,
               GtkTreePath *path,
               GtkTreeIter *iter,
               gpointer data);

enum {
    MEMO_COLUMN_ENUM = 0,
    MEMO_DATA_COLUMN_ENUM,
    MEMO_BACKGROUND_COLOR_ENUM,
    MEMO_BACKGROUND_COLOR_ENABLED_ENUM,
    MEMO_NUM_COLS
};

gboolean
selectRecordByRowMemo(GtkTreeModel *model,
                      GtkTreePath *path,
                      GtkTreeIter *iter,
                      gpointer data);

int print_memo(MyMemo *mmemo);

gboolean addNewMemo(MyMemo *mmemo, const void *data);

/****************************** Main Code *************************************/
static void set_new_button_to(int new_state) {
    jp_logf(JP_LOG_DEBUG, "set_new_button_to new %d old %d\n", new_state, record_changed);

    if (record_changed == new_state) {
        return;
    }

    switch (new_state) {
        case MODIFY_FLAG:
            gtk_widget_show(cancel_record_button);
            gtk_widget_show(copy_record_button);
            gtk_widget_show(apply_record_button);

            gtk_widget_hide(add_record_button);
            gtk_widget_hide(delete_record_button);
            gtk_widget_hide(new_record_button);
            gtk_widget_hide(undelete_record_button);

            break;
        case NEW_FLAG:
            gtk_widget_show(cancel_record_button);
            gtk_widget_show(add_record_button);

            gtk_widget_hide(apply_record_button);
            gtk_widget_hide(copy_record_button);
            gtk_widget_hide(delete_record_button);
            gtk_widget_hide(new_record_button);
            gtk_widget_hide(undelete_record_button);

            break;
        case CLEAR_FLAG:
            gtk_widget_show(delete_record_button);
            gtk_widget_show(copy_record_button);
            gtk_widget_show(new_record_button);

            gtk_widget_hide(add_record_button);
            gtk_widget_hide(apply_record_button);
            gtk_widget_hide(cancel_record_button);
            gtk_widget_hide(undelete_record_button);

            break;
        case UNDELETE_FLAG:
            gtk_widget_show(undelete_record_button);
            gtk_widget_show(copy_record_button);
            gtk_widget_show(new_record_button);

            gtk_widget_hide(add_record_button);
            gtk_widget_hide(apply_record_button);
            gtk_widget_hide(cancel_record_button);
            gtk_widget_hide(delete_record_button);
            break;

        default:
            return;
    }

    record_changed = new_state;
}

static void cb_record_changed(GtkWidget *widget, gpointer data) {
    jp_logf(JP_LOG_DEBUG, "cb_record_changed\n");
    if (record_changed == CLEAR_FLAG) {
        connect_changed_signals(DISCONNECT_SIGNALS);
        if (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(listStore), NULL) > 0) {
            set_new_button_to(MODIFY_FLAG);
        } else {
            set_new_button_to(NEW_FLAG);
        }
    } else if (record_changed == UNDELETE_FLAG) {
        jp_logf(JP_LOG_INFO | JP_LOG_GUI,
                _("This record is deleted.\n"
                  "Undelete it or copy it to make changes.\n"));
    }
}

static void connect_changed_signals(int con_or_dis) {
    int i;
    static int connected = 0;

    /* CONNECT */
    if ((con_or_dis == CONNECT_SIGNALS) && (!connected)) {
        connected = 1;

        if(category_menu2){
            g_signal_connect(G_OBJECT(category_menu2),"changed",G_CALLBACK(cb_record_changed),NULL);
        }
        g_signal_connect(memo_text_buffer, "changed",
                         G_CALLBACK(cb_record_changed), NULL);

        g_signal_connect(G_OBJECT(private_checkbox), "toggled",
                           G_CALLBACK(cb_record_changed), NULL);
    }

    /* DISCONNECT */
    if ((con_or_dis == DISCONNECT_SIGNALS) && (connected)) {
        connected = 0;
        if(category_menu2) {
            g_signal_handlers_disconnect_by_func(G_OBJECT(category_menu2), G_CALLBACK(cb_record_changed), NULL);
        }
        g_signal_handlers_disconnect_by_func(memo_text_buffer,
                                             G_CALLBACK(cb_record_changed), NULL);
        g_signal_handlers_disconnect_by_func(G_OBJECT(private_checkbox),
                                      G_CALLBACK(cb_record_changed), NULL);
    }
}

int print_memo(MyMemo *mmemo) {
    long this_many;
    MemoList *memo_list;
    MemoList memo_list1;

    get_pref(PREF_PRINT_THIS_MANY, &this_many, NULL);

    memo_list = NULL;
    if (this_many == 1) {
        if (mmemo < (MyMemo *) LIST_MIN_DATA) {
            return EXIT_FAILURE;
        }
        memcpy(&(memo_list1.mmemo), mmemo, sizeof(MyMemo));
        memo_list1.next = NULL;
        memo_list = &memo_list1;
    }
    if (this_many == 2) {
        get_memos2(&memo_list, SORT_ASCENDING, 2, 2, 2, memo_category);
    }
    if (this_many == 3) {
        get_memos2(&memo_list, SORT_ASCENDING, 2, 2, 2, CATEGORY_ALL);
    }

    print_memos(memo_list);

    if ((this_many == 2) || (this_many == 3)) {
        free_MemoList(&memo_list);
    }

    return EXIT_SUCCESS;
}

gboolean printRecordMemo(GtkTreeModel *model,
                         GtkTreePath *path,
                         GtkTreeIter *iter,
                         gpointer data) {
    int *i = gtk_tree_path_get_indices(path);
    if (i[0] == row_selected) {
        MyMemo *mmemo = NULL;
        gtk_tree_model_get(model, iter, MEMO_DATA_COLUMN_ENUM, &mmemo, -1);
        print_memo(mmemo);
        return TRUE;
    }

    return FALSE;


}

int memo_print(void) {
    gtk_tree_model_foreach(GTK_TREE_MODEL(listStore), printRecordMemo, NULL);
    return EXIT_SUCCESS;
}

/* Start Import Code */

static int cb_memo_import(GtkWidget *parent_window,
                          const char *file_path,
                          int type) {
    FILE *in;
    char line[256];
    char text[65536];
    struct Memo new_memo;
    unsigned char attrib;
    unsigned int len;
    unsigned int text_len;
    int i, ret, index;
    int import_all;
    MemoList *memolist;
    MemoList *temp_memolist;
    struct CategoryAppInfo cai;
    char old_cat_name[32];
    int suggested_cat_num;
    int new_cat_num;
    int priv;

    in = fopen(file_path, "r");
    if (!in) {
        jp_logf(JP_LOG_WARN, _("Unable to open file: %s\n"), file_path);
        return EXIT_FAILURE;
    }

    /* TEXT */
    if (type == IMPORT_TYPE_TEXT) {
        jp_logf(JP_LOG_DEBUG, "Memo import text [%s]\n", file_path);

        /* Trick to get currently selected category into attrib */
        memo_get_details(&new_memo, &attrib);
        free_Memo(&new_memo);

        text_len = 0;
        text[0] = '\0';
        while (!feof(in)) {
            line[0] = '\0';
            if (fgets(line, 255, in) == NULL) {
                jp_logf(JP_LOG_WARN, "write failed %s %d\n", __FILE__, __LINE__);
            }
            line[255] = '\0';
            len = (unsigned int) strlen(line);
            if (text_len + len > 65534) {
                len = 65534 - text_len;
                line[len] = '\0';
                jp_logf(JP_LOG_WARN, _("Memo text > 65535, truncating\n"));
                strcat(text, line);
                break;
            }
            text_len += len;
            strcat(text, line);
        }

        /* convert to valid UTF-8 and recalculate the length */
        jp_charset_p2j(text, sizeof(text));
        text_len = (unsigned int) strlen(text);
#ifdef JPILOT_DEBUG
        printf("text=[%s]\n", text);
        printf("text_len=%d\n", text_len);
        printf("strlen(text)=%d\n", strlen(text));
#endif

        new_memo.text = text;

        ret = import_record_ask(parent_window, pane,
                                new_memo.text,
                                &(memo_app_info.category),
                                _("Unfiled"),
                                0,
                                attrib & 0x0F,
                                &new_cat_num);
        if ((ret == DIALOG_SAID_IMPORT_ALL) || (ret == DIALOG_SAID_IMPORT_YES)) {
            pc_memo_write(&new_memo, NEW_PC_REC, attrib, NULL);
            jp_logf(JP_LOG_WARN, _("Imported Memo %s\n"), file_path);
        }
    }

    /* CSV */
    if (type == IMPORT_TYPE_CSV) {
        jp_logf(JP_LOG_DEBUG, "Memo import CSV [%s]\n", file_path);
        /* Get the first line containing the format and check for reasonableness */
        if (fgets(text, sizeof(text), in) == NULL) {
            jp_logf(JP_LOG_WARN, "fgets failed %s %d\n", __FILE__, __LINE__);
        }
        ret = verify_csv_header(text, NUM_MEMO_CSV_FIELDS, file_path);
        if (EXIT_FAILURE == ret) return EXIT_FAILURE;

        import_all = FALSE;
        while (1) {
            /* Read the category field */
            ret = read_csv_field(in, text, sizeof(text));
            if (feof(in)) break;
#ifdef JPILOT_DEBUG
            printf("category is [%s]\n", text);
#endif
            g_strlcpy(old_cat_name, text, 16);
            /* Figure out what the best category number is */
            suggested_cat_num = 0;
            for (i = 0; i < NUM_MEMO_CAT_ITEMS; i++) {
                if (!memo_app_info.category.name[i][0]) continue;
                if (!strcmp(memo_app_info.category.name[i], old_cat_name)) {
                    suggested_cat_num = i;
                    break;
                }
            }

            /* Read the private field */
            ret = read_csv_field(in, text, sizeof(text));
#ifdef JPILOT_DEBUG
            printf("private is [%s]\n", text);
#endif
            sscanf(text, "%d", &priv);

            ret = read_csv_field(in, text, sizeof(text));
#ifdef JPILOT_DEBUG
            printf("memo text is [%s]\n", text);
#endif
            new_memo.text = text;
            if (!import_all) {
                ret = import_record_ask(parent_window, pane,
                                        new_memo.text,
                                        &(memo_app_info.category),
                                        old_cat_name,
                                        priv,
                                        suggested_cat_num,
                                        &new_cat_num);
            } else {
                new_cat_num = suggested_cat_num;
            }
            if (ret == DIALOG_SAID_IMPORT_QUIT) break;
            if (ret == DIALOG_SAID_IMPORT_SKIP) continue;
            if (ret == DIALOG_SAID_IMPORT_ALL) import_all = TRUE;

            attrib = (unsigned char) ((new_cat_num & 0x0F) |
                                      (priv ? dlpRecAttrSecret : 0));
            if ((ret == DIALOG_SAID_IMPORT_YES) || (import_all)) {
                pc_memo_write(&new_memo, NEW_PC_REC, attrib, NULL);
            }
        }
    }

    /* Palm Desktop DAT format */
    if (type == IMPORT_TYPE_DAT) {
        jp_logf(JP_LOG_DEBUG, "Memo import DAT [%s]\n", file_path);
        if (dat_check_if_dat_file(in) != DAT_MEMO_FILE) {
            jp_logf(JP_LOG_WARN, _("File doesn't appear to be memopad.dat format\n"));
            fclose(in);
            return EXIT_FAILURE;
        }
        memolist = NULL;
        dat_get_memos(in, &memolist, &cai);
        import_all = FALSE;
        for (temp_memolist = memolist; temp_memolist; temp_memolist = temp_memolist->next) {
#ifdef JPILOT_DEBUG
            printf("category=%d\n", temp_memolist->mmemo.unique_id);
            printf("attrib=%d\n", temp_memolist->mmemo.attrib);
            printf("private=%d\n", temp_memolist->mmemo.attrib & 0x10);
            printf("memo=[%s]\n", temp_memolist->mmemo.memo.text);
#endif
            new_memo.text = temp_memolist->mmemo.memo.text;
            index = temp_memolist->mmemo.unique_id - 1;
            if (index < 0) {
                g_strlcpy(old_cat_name, _("Unfiled"), 16);
                index = 0;
            } else {
                g_strlcpy(old_cat_name, cai.name[index], 16);
            }
            attrib = 0;
            /* Figure out what category it was in the dat file */
            index = temp_memolist->mmemo.unique_id - 1;
            suggested_cat_num = 0;
            if (index > -1) {
                for (i = 0; i < NUM_MEMO_CAT_ITEMS; i++) {
                    if (memo_app_info.category.name[i][0] == '\0') continue;
                    if (!strcmp(memo_app_info.category.name[i], old_cat_name)) {
                        suggested_cat_num = i;
                        break;
                    }
                }
            }

            ret = 0;
            if (!import_all) {
                ret = import_record_ask(parent_window, pane,
                                        new_memo.text,
                                        &(memo_app_info.category),
                                        old_cat_name,
                                        (temp_memolist->mmemo.attrib & 0x10),
                                        suggested_cat_num,
                                        &new_cat_num);
            } else {
                new_cat_num = suggested_cat_num;
            }
            if (ret == DIALOG_SAID_IMPORT_QUIT) break;
            if (ret == DIALOG_SAID_IMPORT_SKIP) continue;
            if (ret == DIALOG_SAID_IMPORT_ALL) import_all = TRUE;

            attrib = (unsigned char) ((new_cat_num & 0x0F) |
                                      ((temp_memolist->mmemo.attrib & 0x10) ? dlpRecAttrSecret : 0));
            if ((ret == DIALOG_SAID_IMPORT_YES) || (import_all)) {
                pc_memo_write(&new_memo, NEW_PC_REC, attrib, NULL);
            }
        }
        free_MemoList(&memolist);
    }

    memo_refresh();
    fclose(in);
    return EXIT_SUCCESS;
}

int memo_import(GtkWidget *window) {
    char *type_desc[] = {
            N_("Text"),
            N_("CSV (Comma Separated Values)"),
            N_("DAT/MPA (Palm Archive Formats)"),
            NULL
    };
    int type_int[] = {
            IMPORT_TYPE_TEXT,
            IMPORT_TYPE_CSV,
            IMPORT_TYPE_DAT,
            0
    };

    /* Hide ABA import of Memos until file format has been decoded */
    if (memo_version == 1) {
        type_desc[2] = NULL;
        type_int[2] = 0;
    }

    import_gui(window, pane, type_desc, type_int, cb_memo_import);
    return EXIT_SUCCESS;
}

/* End Import Code */

/* Start Export code */

static void cb_memo_export_ok(GtkWidget *export_window, GtkWidget *treeView,
                              int type, const char *filename) {
    MyMemo *mmemo;
    GList *list, *temp_list;
    FILE *out;
    struct stat statb;
    int i, r, len;
    const char *short_date;
    time_t ltime;
    struct tm *now;
    char *button_text[] = {N_("OK")};
    char *button_overwrite_text[] = {N_("No"), N_("Yes")};
    char text[1024];
    char str1[256], str2[256];
    char date_string[1024];
    char pref_time[40];
    char csv_text[65550];
    long char_set;
    char *utf;
    int cat;
    int j;

    /* Open file for export, including corner cases where file exists or
     * can't be opened */
    if (!stat(filename, &statb)) {
        if (S_ISDIR(statb.st_mode)) {
            g_snprintf(text, sizeof(text), _("%s is a directory"), filename);
            dialog_generic(GTK_WINDOW(export_window),
                           _("Error Opening File"),
                           DIALOG_ERROR, text, 1, button_text);
            return;
        }
        g_snprintf(text, sizeof(text), _("Do you want to overwrite file %s?"), filename);
        r = dialog_generic(GTK_WINDOW(export_window),
                           _("Overwrite File?"),
                           DIALOG_ERROR, text, 2, button_overwrite_text);
        if (r != DIALOG_SAID_2) {
            return;
        }
    }

    out = fopen(filename, "w");
    if (!out) {
        g_snprintf(text, sizeof(text), _("Error opening file: %s"), filename);
        dialog_generic(GTK_WINDOW(export_window),
                       _("Error Opening File"),
                       DIALOG_ERROR, text, 1, button_text);
        return;
    }

    /* Write a header for TEXT file */
    if (type == EXPORT_TYPE_TEXT) {
        get_pref(PREF_SHORTDATE, NULL, &short_date);
        get_pref_time_no_secs(pref_time);
        time(&ltime);
        now = localtime(&ltime);
        strftime(str1, sizeof(str1), short_date, now);
        strftime(str2, sizeof(str2), pref_time, now);
        g_snprintf(date_string, sizeof(date_string), "%s %s", str1, str2);
        if (memo_version == 0) {
            fprintf(out, _("Memo exported from %s %s on %s\n\n"),
                    PN, VERSION, date_string);
        } else {
            fprintf(out, _("Memos exported from %s %s on %s\n\n"),
                    PN, VERSION, date_string);
        }
    }

    /* Write a header to the CSV file */
    if (type == EXPORT_TYPE_CSV) {
        if (memo_version == 0) {
            fprintf(out, "CSV memo version "VERSION": Category, Private, Memo Text\n");
        } else {
            fprintf(out, "CSV memos version "VERSION": Category, Private, Memo Text\n");
        }
    }

    /* Write a header to the BFOLDERS CSV file */
    if (type == EXPORT_TYPE_BFOLDERS) {
        fprintf(out, "Notes:\n");
        fprintf(out, "Note,Folder\n");
    }

    /* Write a header to the KeePassX XML file */
    if (type == EXPORT_TYPE_KEEPASSX) {
        fprintf(out, "<!DOCTYPE KEEPASSX_DATABASE>\n");
        fprintf(out, "<database>\n");
        fprintf(out, " <group>\n");
        fprintf(out, "  <title>Memos</title>\n");
        fprintf(out, "  <icon>7</icon>\n");
    }

    get_pref(PREF_CHAR_SET, &char_set, NULL);
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeView));
    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeView));
    list = gtk_tree_selection_get_selected_rows(selection, &model);

    for (i = 0, temp_list = list; temp_list; temp_list = temp_list->next, i++) {
        GtkTreePath *path = temp_list->data;
        GtkTreeIter iter;
        if (gtk_tree_model_get_iter(model, &iter, path)) {
            gtk_tree_model_get(model, &iter, MEMO_DATA_COLUMN_ENUM, &mmemo, -1);
            if (!mmemo) {
                continue;
                jp_logf(JP_LOG_WARN, _("Can't export memo %d\n"), (long) temp_list->data + 1);
            }
            switch (type) {
                case EXPORT_TYPE_CSV:
                    len = 0;
                    if (mmemo->memo.text) {
                        len = (unsigned int) strlen(mmemo->memo.text) * 2 + 4;
                    }
                    if (len < 256) len = 256;
                    utf = charset_p2newj(memo_app_info.category.name[mmemo->attrib & 0x0F], 16, (int) char_set);
                    str_to_csv_str(csv_text, utf);
                    fprintf(out, "\"%s\",", csv_text);
                    g_free(utf);
                    fprintf(out, "\"%s\",", (mmemo->attrib & dlpRecAttrSecret) ? "1" : "0");
                    str_to_csv_str(csv_text, mmemo->memo.text);
                    fprintf(out, "\"%s\"\n", csv_text);
                    break;

                case EXPORT_TYPE_BFOLDERS:
                    len = 0;
                    str_to_csv_str(csv_text, mmemo->memo.text);
                    fprintf(out, "\"%s\",", csv_text);

                    if (mmemo->memo.text) {
                        len = (unsigned int) strlen(mmemo->memo.text) * 2 + 4;
                    }
                    if (len < 256) len = 256;
                    printf("\"RAW %d %s\"\n", mmemo->attrib & 0x0F, memo_app_info.category.name[mmemo->attrib & 0x0F]);
                    utf = charset_p2newj(memo_app_info.category.name[mmemo->attrib & 0x0F], 16, (int) char_set);
                    str_to_csv_str(csv_text, utf);
                    fprintf(out, "\"Memos > %s\"\n", csv_text);
                    printf("\"Memos > %s\"\n", utf);
                    g_free(utf);
                    break;

                case EXPORT_TYPE_TEXT:
                    get_pref(PREF_SHORTDATE, NULL, &short_date);
                    get_pref_time_no_secs(pref_time);
                    time(&ltime);
                    now = localtime(&ltime);
                    strftime(str1, sizeof(str1), short_date, now);
                    strftime(str2, sizeof(str2), pref_time, now);
                    g_snprintf(text, sizeof(text), "%s %s", str1, str2);

                    fprintf(out, _("Memo: %ld\n"), (long) temp_list->data + 1);
                    utf = charset_p2newj(memo_app_info.category.name[mmemo->attrib & 0x0F], 16, (int) char_set);
                    fprintf(out, _("Category: %s\n"), utf);
                    g_free(utf);
                    fprintf(out, _("Private: %s\n"),
                            (mmemo->attrib & dlpRecAttrSecret) ? _("Yes") : _("No"));
                    fprintf(out, _("----- Start of Memo -----\n"));
                    fprintf(out, "%s", mmemo->memo.text);
                    fprintf(out, _("\n----- End of Memo -----\n\n"));
                    break;

                case EXPORT_TYPE_KEEPASSX:
                    break;

                default:
                    jp_logf(JP_LOG_WARN, _("Unknown export type\n"));
            }
        }
    }

    /* I'm writing a second loop for the KeePassX XML file because I want to
     * put each category into a folder and we need to write the tag for a folder
     * and then find each record in that category/folder
     */
    if (type == EXPORT_TYPE_KEEPASSX) {
        for (cat = 0; cat < 16; cat++) {
            if (memo_app_info.category.name[cat][0] == '\0') {
                continue;
            }
            /* Write a folder XML tag */
            utf = charset_p2newj(memo_app_info.category.name[cat], 16, (int) char_set);
            fprintf(out, "  <group>\n");
            fprintf(out, "   <title>%s</title>\n", utf);
            fprintf(out, "   <icon>7</icon>\n");
            g_free(utf);
            for (i = 0, temp_list = list; temp_list; temp_list = temp_list->next, i++) {
                GtkTreePath *path = temp_list->data;
                GtkTreeIter iter;
                if (gtk_tree_model_get_iter(model, &iter, path)) {
                    gtk_tree_model_get(model, &iter, MEMO_DATA_COLUMN_ENUM, &mmemo, -1);
                    if (!mmemo) {
                        continue;
                        jp_logf(JP_LOG_WARN, _("Can't export memo %d\n"), (long) temp_list->data + 1);
                    }
                    if ((mmemo->attrib & 0x0F) != cat) {
                        continue;
                    }
                    fprintf(out, "   <entry>\n");

                    /* Create a title (which is the first line of the memo) */
                    for (j = 0; j < 100; j++) {
                        str1[j] = mmemo->memo.text[j];
                        if (str1[j] == '\0') {
                            break;
                        }
                        if (str1[j] == '\n') {
                            str1[j] = '\0';
                            break;
                        }
                    }
                    str1[100] = '\0';
                    str_to_keepass_str(csv_text, str1);
                    fprintf(out, "    <title>%s</title>\n", csv_text);
                    /* No keyring field for username */
                    /* No keyring field for password */
                    /* No keyring field for url */
                    str_to_keepass_str(csv_text, mmemo->memo.text);
                    fprintf(out, "    <comment>%s</comment>\n", csv_text);
                    fprintf(out, "    <icon>7</icon>\n");
                    /* No keyring field for creation */
                    /* No keyring field for lastaccess */
                    /* No keyring field for lastmod */
                    /* No keyring field for expire */
                    fprintf(out, "    <expire>Never</expire>\n");
                    fprintf(out, "   </entry>\n");
                }
                fprintf(out, "  </group>\n");
            }
        }

        /* Write a footer to the KeePassX XML file */
        if (type == EXPORT_TYPE_KEEPASSX) {
            fprintf(out, " </group>\n");
            fprintf(out, "</database>\n");
        }
    }

    if (out) {
        fclose(out);
    }
}

static void cb_memo_update_listStore(GtkWidget *treeView, int category) {
    memo_update_liststore(GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(treeView))), NULL, &export_memo_list,
                          category, FALSE);
}

static GtkWidget *cb_memo_init_export_treeView() {
    GtkListStore *listStore = gtk_list_store_new(MEMO_NUM_COLS, G_TYPE_STRING, G_TYPE_POINTER, GDK_TYPE_RGBA,
                                                 G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_BOOLEAN);
    GtkWidget *treeView = GTK_WIDGET(gtk_tree_view_new_with_model(listStore));
    GtkTreeSelection *treeSelection = NULL;
    treeView = GTK_WIDGET(gtk_tree_view_new_with_model(GTK_TREE_MODEL(listStore)));
    GtkCellRenderer *columnRenderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes("", columnRenderer, "text", MEMO_COLUMN_ENUM,
                                                                         "cell-background-gdk",
                                                                         MEMO_BACKGROUND_COLOR_ENUM,
                                                                         "cell-background-set",
                                                                         MEMO_BACKGROUND_COLOR_ENABLED_ENUM, NULL);
    gtk_tree_view_column_set_fixed_width(column, (gint) 50);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeView), FALSE);
    gtk_tree_view_insert_column(GTK_TREE_VIEW(treeView), column, 0);
    gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
    return GTK_WIDGET(treeView);
}

static void cb_memo_export_done(GtkWidget *widget, const char *filename) {
    free_MemoList(&export_memo_list);

    set_pref(PREF_MEMO_EXPORT_FILENAME, 0, filename, TRUE);
}

int memo_export(GtkWidget *window) {
    int w, h, x, y;
    char *type_text[] = {N_("Text"),
                         N_("CSV"),
                         N_("B-Folders CSV"),
                         N_("KeePassX XML"),
                         NULL};
    int type_int[] = {EXPORT_TYPE_TEXT, EXPORT_TYPE_CSV, EXPORT_TYPE_BFOLDERS, EXPORT_TYPE_KEEPASSX};

    w = gdk_window_get_width(gtk_widget_get_window(window));
    h = gdk_window_get_height(gtk_widget_get_window(window));
    gdk_window_get_root_origin(gtk_widget_get_window(window), &x, &y);

    w = gtk_paned_get_position(GTK_PANED(pane));
    x += 40;

    export_gui(window,
               w, h, x, y, 1, sort_l,
               PREF_MEMO_EXPORT_FILENAME,
               type_text,
               type_int,
               cb_memo_init_export_treeView,
               cb_memo_update_listStore,
               cb_memo_export_done,
               cb_memo_export_ok
    );

    return EXIT_SUCCESS;
}

/* End Export Code */

/* Find position of category in sorted category array 
 * via its assigned category number */
static int find_sort_cat_pos(int cat) {
    int i;

    for (i = 0; i < NUM_MEMO_CAT_ITEMS; i++) {
        if (sort_l[i].cat_num == cat) {
            return i;
        }
    }

    return -1;
}

/* Find a category's position in the category menu.
 * This is equal to the category number except for the Unfiled category.
 * The Unfiled category is always in the last position which changes as
 * the number of categories changes */
static int find_menu_cat_pos(int cat) {
    int i;

    if (cat != NUM_MEMO_CAT_ITEMS - 1) {
        return cat;
    } else { /* Unfiled category */
        /* Count how many category entries are filled */
        for (i = 0; i < NUM_MEMO_CAT_ITEMS; i++) {
            if (!sort_l[i].Pcat[0]) {
                return i;
            }
        }
        return 0;
    }
}

gboolean deleteRecordMemo(GtkTreeModel *model,
                          GtkTreePath *path,
                          GtkTreeIter *iter,
                          gpointer data) {
    int *i = gtk_tree_path_get_indices(path);
    if (i[0] == row_selected) {
        MyMemo *mmemo = NULL;
        gtk_tree_model_get(model, iter, MEMO_DATA_COLUMN_ENUM, &mmemo, -1);
        delete_memo(mmemo, data);
        return TRUE;
    }

    return FALSE;


}

gboolean undeleteRecordMemo(GtkTreeModel *model,
                            GtkTreePath *path,
                            GtkTreeIter *iter,
                            gpointer data) {
    int *i = gtk_tree_path_get_indices(path);
    if (i[0] == row_selected) {
        MyMemo *mmemo = NULL;
        gtk_tree_model_get(model, iter, MEMO_DATA_COLUMN_ENUM, &mmemo, -1);
        undelete_memo(mmemo, data);
        return TRUE;
    }

    return FALSE;

}

void delete_memo(MyMemo *mmemo, gpointer data) {

    int flag;
    int show_priv;
    long char_set;

    if (mmemo < (MyMemo *) LIST_MIN_DATA) {
        return;
    }
    /* Convert to Palm character set */
    get_pref(PREF_CHAR_SET, &char_set, NULL);
    if (char_set != CHAR_SET_LATIN1) {
        if (mmemo->memo.text)
            charset_j2p(mmemo->memo.text, strlen(mmemo->memo.text) + 1, (int) char_set);
    }

    /* Do masking like Palm OS 3.5 */
    show_priv = show_privates(GET_PRIVATES);
    if ((show_priv != SHOW_PRIVATES) &&
        (mmemo->attrib & dlpRecAttrSecret)) {
        return;
    }
    /* End Masking */
    jp_logf(JP_LOG_DEBUG, "mmemo->unique_id = %d\n", mmemo->unique_id);
    jp_logf(JP_LOG_DEBUG, "mmemo->rt = %d\n", mmemo->rt);
    flag = GPOINTER_TO_INT(data);
    if ((flag == MODIFY_FLAG) || (flag == DELETE_FLAG)) {
        delete_pc_record(MEMO, mmemo, flag);
        if (flag == DELETE_FLAG) {
            /* when we redraw we want to go to the line above the deleted one */
            if (row_selected > 0) {
                row_selected--;
            }
        }
    }

    if (flag == DELETE_FLAG) {
        memo_redraw();
    }
}

static void cb_delete_memo(GtkWidget *widget,
                           gpointer data) {
    gtk_tree_model_foreach(GTK_TREE_MODEL(listStore), deleteRecordMemo, data);
    return;

}

void undelete_memo(MyMemo *mmemo, gpointer data) {

    int flag;
    int show_priv;


    if (mmemo < (MyMemo *) LIST_MIN_DATA) {
        return;
    }

    /* Do masking like Palm OS 3.5 */
    show_priv = show_privates(GET_PRIVATES);
    if ((show_priv != SHOW_PRIVATES) &&
        (mmemo->attrib & dlpRecAttrSecret)) {
        return;
    }
    /* End Masking */

    jp_logf(JP_LOG_DEBUG, "mmemo->unique_id = %d\n", mmemo->unique_id);
    jp_logf(JP_LOG_DEBUG, "mmemo->rt = %d\n", mmemo->rt);

    flag = GPOINTER_TO_INT(data);
    if (flag == UNDELETE_FLAG) {
        if (mmemo->rt == DELETED_PALM_REC ||
            mmemo->rt == DELETED_PC_REC) {
            undelete_pc_record(MEMO, mmemo, flag);
        }
        /* Possible later addition of undelete for modified records
        else if (mmemo->rt == MODIFIED_PALM_REC)
        {
           cb_add_new_record(widget, GINT_TO_POINTER(COPY_FLAG));
        }
        */
    }

    memo_redraw();
}

static void cb_undelete_memo(GtkWidget *widget,
                             gpointer data) {

    gtk_tree_model_foreach(GTK_TREE_MODEL(listStore), undeleteRecordMemo, data);
    return;


}

static void cb_cancel(GtkWidget *widget, gpointer data) {
    set_new_button_to(CLEAR_FLAG);
    memo_refresh();
}

static void cb_edit_cats(GtkWidget *widget, gpointer data) {
    struct MemoAppInfo ai;
    char db_name[FILENAME_MAX];
    char pdb_name[FILENAME_MAX];
    char full_name[FILENAME_MAX];
    unsigned char buffer[65536];
    int num;
    size_t size;
    void *buf;
    struct pi_file *pf;
    long memo_version;

    jp_logf(JP_LOG_DEBUG, "cb_edit_cats\n");

    get_pref(PREF_MEMO_VERSION, &memo_version, NULL);

    switch (memo_version) {
        case 0:
        default:
            strcpy(pdb_name, "MemoDB.pdb");
            strcpy(db_name, "MemoDB");
            break;
        case 1:
            strcpy(pdb_name, "MemosDB-PMem.pdb");
            strcpy(db_name, "MemosDB-PMem");
            break;
        case 2:
            strcpy(pdb_name, "Memo32DB.pdb");
            strcpy(db_name, "Memo32DB");
            break;
    }

    get_home_file_name(pdb_name, full_name, sizeof(full_name));

    buf = NULL;
    memset(&ai, 0, sizeof(ai));

    pf = pi_file_open(full_name);
    pi_file_get_app_info(pf, &buf, &size);

    num = unpack_MemoAppInfo(&ai, buf, size);
    if (num <= 0) {
        jp_logf(JP_LOG_WARN, _("Error reading file: %s\n"), pdb_name);
        return;
    }

    pi_file_close(pf);

    edit_cats(widget, db_name, &(ai.category));

    size = pack_MemoAppInfo(&ai, buffer, sizeof(buffer));

    pdb_file_write_app_block(db_name, buffer, size);


    cb_app_button(NULL, GINT_TO_POINTER(REDRAW));
}

static void cb_category(GtkComboBox *item, int selection) {
    int b;
    if (!item) return;
    if (gtk_combo_box_get_active(GTK_COMBO_BOX(item)) < 0) {
        return;
    }
    int selectedItem = get_selected_category_from_combo_box(item);
    if (selectedItem == -1) {
        return;
    }

    if (memo_category == selectedItem) { return; }

    b = dialog_save_changed_record_with_cancel(pane, record_changed);
    if (b == DIALOG_SAID_1) { /* Cancel */
        int index, index2;

        if (memo_category == CATEGORY_ALL) {
            index = 0;
            index2 = 0;
        } else {
            index = find_sort_cat_pos(memo_category);
            index2 = find_menu_cat_pos(index) + 1;
            index += 1;
        }

        if (index < 0) {
            jp_logf(JP_LOG_WARN, _("Category is not legal\n"));
        } else {
            gtk_combo_box_set_active(GTK_COMBO_BOX(category_menu1), index2);
        }

        return;
    }
    if (b == DIALOG_SAID_3) { /* Save */
        cb_add_new_record(NULL, GINT_TO_POINTER(record_changed));
    }

    if (selectedItem == CATEGORY_EDIT) {
        cb_edit_cats(item, NULL);
    } else {
        memo_category = selectedItem;
    }
    row_selected = 0;
    jp_logf(JP_LOG_DEBUG, "cb_category() cat=%d\n", memo_category);
    memo_update_liststore(listStore, category_menu1, &glob_memo_list, memo_category, TRUE);
    jp_logf(JP_LOG_DEBUG, "Leaving cb_category()\n");

}

static int memo_clear_details(void) {
    int new_cat;
    int sorted_position;

    jp_logf(JP_LOG_DEBUG, "memo_clear_details()\n");

    /* Need to disconnect signals first */
    connect_changed_signals(DISCONNECT_SIGNALS);

    gtk_text_buffer_set_text(GTK_TEXT_BUFFER(memo_text_buffer), "", -1);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(private_checkbox), FALSE);

    if (memo_category == CATEGORY_ALL) {
        new_cat = 0;
    } else {
        new_cat = memo_category;
    }
    sorted_position = find_sort_cat_pos(new_cat);
    if (sorted_position < 0) {
        jp_logf(JP_LOG_WARN, _("Category is not legal\n"));
    } else {
        gtk_combo_box_set_active(GTK_COMBO_BOX(category_menu2),find_menu_cat_pos(sorted_position));
    }

    set_new_button_to(CLEAR_FLAG);
    connect_changed_signals(CONNECT_SIGNALS);
    jp_logf(JP_LOG_DEBUG, "Leaving memo_clear_details()\n");

    return EXIT_SUCCESS;
}

static int memo_get_details(struct Memo *new_memo, unsigned char *attrib) {
    int i;
    GtkTextIter start_iter;
    GtkTextIter end_iter;

    gtk_text_buffer_get_bounds(GTK_TEXT_BUFFER(memo_text_buffer), &start_iter, &end_iter);
    new_memo->text = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(memo_text_buffer), &start_iter, &end_iter, TRUE);
    if (new_memo->text[0] == '\0') {
        free(new_memo->text);
        new_memo->text = NULL;
    }

    /* Get the category that is set from the menu */
    if (GTK_IS_WIDGET(category_menu2)) {
        *attrib = get_selected_category_from_combo_box(GTK_COMBO_BOX(category_menu2));
    }
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(private_checkbox))) {
        *attrib |= dlpRecAttrSecret;
    }
    return EXIT_SUCCESS;
}

gboolean
addNewRecordMemo(GtkTreeModel *model,
                 GtkTreePath *path,
                 GtkTreeIter *iter,
                 gpointer data) {

    int *i = gtk_tree_path_get_indices(path);

    if (i[0] == row_selected) {
        MyMemo *mmemo = NULL;
        gtk_tree_model_get(model, iter, MEMO_DATA_COLUMN_ENUM, &mmemo, -1);
        return addNewMemo(mmemo, data);

    }

    return FALSE;


}

gboolean addNewMemo(MyMemo *mmemo, const void *data) {
    struct Memo new_memo;
    unsigned char attrib;
    int flag;
    unsigned int unique_id;
    int show_priv;

    flag = GPOINTER_TO_INT(data);


    unique_id = 0;

    /* Do masking like Palm OS 3.5 */
    if ((flag == COPY_FLAG) || (flag == MODIFY_FLAG)) {
        show_priv = show_privates(GET_PRIVATES);


        if (mmemo < (MyMemo *) LIST_MIN_DATA) {
            return TRUE;
        }
        if ((show_priv != SHOW_PRIVATES) &&
            (mmemo->attrib & dlpRecAttrSecret)) {
            return TRUE;
        }
    }
    /* End Masking */
    if (flag == CLEAR_FLAG) {
        /* Clear button was hit */
        memo_clear_details();
        connect_changed_signals(DISCONNECT_SIGNALS);
        set_new_button_to(NEW_FLAG);
        gtk_widget_grab_focus(GTK_WIDGET(memo_text));
        return TRUE;
    }
    if ((flag != NEW_FLAG) && (flag != MODIFY_FLAG) && (flag != COPY_FLAG)) {
        return TRUE;
    }
    if (flag == MODIFY_FLAG) {

        unique_id = mmemo->unique_id;
        if (mmemo < (MyMemo *) LIST_MIN_DATA) {
            return TRUE;
        }
        if ((mmemo->rt == DELETED_PALM_REC) ||
            (mmemo->rt == DELETED_PC_REC) ||
            (mmemo->rt == MODIFIED_PALM_REC)) {
            jp_logf(JP_LOG_INFO, _("You can't modify a record that is deleted\n"));
            return TRUE;
        }
    }
    memo_get_details(&new_memo, &attrib);

    set_new_button_to(CLEAR_FLAG);

    /* Keep unique ID intact */
    if (flag == MODIFY_FLAG) {
        cb_delete_memo(NULL, data);
        if ((mmemo->rt == PALM_REC) || (mmemo->rt == REPLACEMENT_PALM_REC)) {
            pc_memo_write(&new_memo, REPLACEMENT_PALM_REC, attrib, &unique_id);
        } else {
            unique_id = 0;
            pc_memo_write(&new_memo, NEW_PC_REC, attrib, &unique_id);
        }
    } else {
        unique_id = 0;
        pc_memo_write(&new_memo, NEW_PC_REC, attrib, &unique_id);
    }

    free_Memo(&new_memo);
    /* Don't return to modified record if search gui active */
    if (!glob_find_id) {
        glob_find_id = unique_id;
    }
    memo_redraw();
    return TRUE;
}

static void cb_add_new_record(GtkWidget *widget, gpointer data) {

    if (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(listStore), NULL) != 0) {
        gtk_tree_model_foreach(GTK_TREE_MODEL(listStore), addNewRecordMemo, data);
    } else {
        //no records exist in category yet.
        addNewMemo(NULL, data);
    }
}

/* Do masking like Palm OS 3.5 */
static void clear_mymemo(MyMemo *mmemo) {
    mmemo->unique_id = 0;
    mmemo->attrib = (unsigned char) (mmemo->attrib & 0xF8);
    if (mmemo->memo.text) {
        free(mmemo->memo.text);
        mmemo->memo.text = strdup("");
    }

    return;
}

/* End Masking */


static gboolean cb_key_pressed_left_side(GtkWidget *widget,
                                         GdkEventKey *event,
                                         gpointer next_widget) {
    GtkTextBuffer *text_buffer;
    GtkTextIter iter;

    if (event->keyval == GDK_KEY_Return) {
         g_signal_stop_emission_by_name(G_OBJECT(widget), "key_press_event");
        gtk_widget_grab_focus(GTK_WIDGET(next_widget));
        /* Position cursor at start of text */
        text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(next_widget));
        gtk_text_buffer_get_start_iter(text_buffer, &iter);
        gtk_text_buffer_place_cursor(text_buffer, &iter);
        return TRUE;
    }

    return FALSE;
}

static gboolean cb_key_pressed_right_side(GtkWidget *widget,
                                          GdkEventKey *event,
                                          gpointer next_widget) {
    /* Switch to treeView */
    if ((event->keyval == GDK_KEY_Return) && (event->state & GDK_SHIFT_MASK)) {
         g_signal_stop_emission_by_name(G_OBJECT(widget), "key_press_event");
        gtk_widget_grab_focus(GTK_WIDGET(next_widget));
        return TRUE;
    }
    /* Call external editor for memo_text */
    if ((event->keyval == GDK_KEY_e) && (event->state & GDK_CONTROL_MASK)) {
         g_signal_stop_emission_by_name(G_OBJECT(widget), "key_press_event");

        /* Get current text and place in temporary file */
        GtkTextIter start_iter;
        GtkTextIter end_iter;
        char *text_out;

        gtk_text_buffer_get_bounds(GTK_TEXT_BUFFER(memo_text_buffer),
                                   &start_iter, &end_iter);
        text_out = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(memo_text_buffer),
                                            &start_iter, &end_iter, TRUE);


        char tmp_fname[] = "jpilot.XXXXXX";
        int tmpfd = mkstemp(tmp_fname);
        if (tmpfd < 0) {
            jp_logf(JP_LOG_WARN, _("Could not get temporary file name\n"));
            if (text_out)
                free(text_out);
            return TRUE;
        }

        FILE *fptr = fdopen(tmpfd, "w");
        if (!fptr) {
            jp_logf(JP_LOG_WARN, _("Could not open temporary file for external editor\n"));
            if (text_out)
                free(text_out);
            return TRUE;
        }
        fwrite(text_out, strlen(text_out), 1, fptr);
        fwrite("\n", 1, 1, fptr);
        fclose(fptr);

        /* Call external editor */
        char command[1024];
        const char *ext_editor;

        get_pref(PREF_EXTERNAL_EDITOR, NULL, &ext_editor);
        if (!ext_editor) {
            jp_logf(JP_LOG_INFO, "External Editor command empty\n");
            if (text_out)
                free(text_out);
            return TRUE;
        }

        if ((strlen(ext_editor) + strlen(tmp_fname) + 1) > sizeof(command)) {
            jp_logf(JP_LOG_WARN, _("External editor command too long to execute\n"));
            if (text_out)
                free(text_out);
            return TRUE;
        }
        g_snprintf(command, sizeof(command), "%s %s", ext_editor, tmp_fname);

        /* jp_logf(JP_LOG_STDOUT|JP_LOG_FILE, _("executing command = [%s]\n"), command); */
        if (system(command) == -1) {
            /* Read data back from temporary file into memo */
            char text_in[0xFFFF];
            size_t bytes_read;

            fptr = fopen(tmp_fname, "rb");
            if (!fptr) {
                jp_logf(JP_LOG_WARN, _("Could not open temporary file from external editor\n"));
                return TRUE;
            }
            bytes_read = fread(text_in, 1, 0xFFFF, fptr);
            fclose(fptr);
            unlink(tmp_fname);

            text_in[--bytes_read] = '\0';  /* Strip final newline */
            /* Only update text if it has changed */
            if (strcmp(text_out, text_in)) {
                gtk_text_buffer_set_text(GTK_TEXT_BUFFER(memo_text_buffer),
                                         text_in, -1);
            }
        }

        if (text_out)
            free(text_out);

        return TRUE;
    }   /* End of external editor if */

    return FALSE;
}

gboolean
selectRecordByRowMemo(GtkTreeModel *model,
                      GtkTreePath *path,
                      GtkTreeIter *iter,
                      gpointer data) {
    int *i = gtk_tree_path_get_indices(path);
    if (i[0] == row_selected) {
        GtkTreeSelection *selection = NULL;
        selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeView));
        gtk_tree_selection_select_path(selection, path);
        gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(treeView), path, MEMO_COLUMN_ENUM, FALSE, 1.0, 0.0);
        return TRUE;
    }

    return FALSE;
}

static void memo_update_liststore(GtkListStore *pListStore, GtkWidget *tooltip_widget,
                                  MemoList **memo_list, int category, int main) {
    int num_entries, entries_shown;
    GtkTreeIter iter;
    size_t copy_max_length;
    char *last;
    char str2[MEMO_MAX_COLUMN_LEN];
    MemoList *temp_memo;
    char str[MEMO_LIST_CHAR_WIDTH + 10];
    int len, len1;
    int show_priv;
    long show_tooltips;

    jp_logf(JP_LOG_DEBUG, "memo_update_liststore()\n");

    free_MemoList(memo_list);

    /* Need to get all records including private ones for the tooltips calculation */
    num_entries = get_memos2(memo_list, SORT_ASCENDING, 2, 2, 1, CATEGORY_ALL);

    /* Start by clearing existing entry if in main window */
    if (main) {
        memo_clear_details();
    }
    GtkTreeSelection* treeSelection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeView));
    gtk_tree_selection_set_select_function(treeSelection, NULL, NULL, NULL);
    gtk_list_store_clear(GTK_LIST_STORE(pListStore));

    show_priv = show_privates(GET_PRIVATES);

    entries_shown = 0;
    for (temp_memo = *memo_list; temp_memo; temp_memo = temp_memo->next) {
        if (((temp_memo->mmemo.attrib & 0x0F) != category) &&
            category != CATEGORY_ALL) {
            continue;
        }

        /* Do masking like Palm OS 3.5 */
        if ((show_priv == MASK_PRIVATES) &&
            (temp_memo->mmemo.attrib & dlpRecAttrSecret)) {
            clear_mymemo(&temp_memo->mmemo);
            gtk_list_store_append(pListStore, &iter);
            gtk_list_store_set(pListStore, &iter,
                               MEMO_COLUMN_ENUM, "----------------------------------------",
                               MEMO_DATA_COLUMN_ENUM, &(temp_memo->mmemo),
                               -1);
            entries_shown++;
            continue;
        }
        /* End Masking */

        /* Hide the private records if need be */
        if ((show_priv != SHOW_PRIVATES) &&
            (temp_memo->mmemo.attrib & dlpRecAttrSecret)) {
            continue;
        }

        sprintf(str, "%d. ", entries_shown + 1);

        len1 = (int) strlen(str);
        len = ((int) strlen(temp_memo->mmemo.memo.text)) + 1;
        /* ..memo treeView does not display '/n' */
        if ((copy_max_length = (size_t) len) > MEMO_LIST_CHAR_WIDTH) {
            copy_max_length = MEMO_LIST_CHAR_WIDTH;
        }
        last = multibyte_safe_memccpy(str + len1, temp_memo->mmemo.memo.text, '\n', copy_max_length);
        if (last) {
            *(last - 1) = '\0';
        } else {
            str[copy_max_length + len1] = '\0';
        }
        lstrncpy_remove_cr_lfs(str2, str, MEMO_MAX_COLUMN_LEN);
        gtk_list_store_append(pListStore, &iter);

        /* Highlight row background depending on status */
        GdkColor bgColor;
        gboolean showBgColor = FALSE;
        switch (temp_memo->mmemo.rt) {
            case NEW_PC_REC:
            case REPLACEMENT_PALM_REC:
                bgColor = get_color(LIST_NEW_RED, LIST_NEW_GREEN, LIST_NEW_BLUE);
                showBgColor = TRUE;
                break;
            case DELETED_PALM_REC:
            case DELETED_PC_REC:
                bgColor = get_color(LIST_DEL_RED, LIST_DEL_GREEN, LIST_DEL_BLUE);
                showBgColor = TRUE;
                break;
            case MODIFIED_PALM_REC:
                bgColor = get_color(LIST_MOD_RED, LIST_MOD_GREEN, LIST_MOD_BLUE);
                showBgColor = TRUE;
                break;
            default:
                if (temp_memo->mmemo.attrib & dlpRecAttrSecret) {
                    bgColor = get_color(LIST_PRIVATE_RED, LIST_PRIVATE_GREEN, LIST_PRIVATE_BLUE);
                    showBgColor = TRUE;
                } else {
                    showBgColor = FALSE;
                }
        }
        gtk_list_store_set(pListStore, &iter,
                           MEMO_COLUMN_ENUM, str2,
                           MEMO_DATA_COLUMN_ENUM, &(temp_memo->mmemo),
                           MEMO_BACKGROUND_COLOR_ENUM, showBgColor ? &bgColor : NULL,
                           MEMO_BACKGROUND_COLOR_ENABLED_ENUM, showBgColor,
                           -1);
        entries_shown++;
    }

    jp_logf(JP_LOG_DEBUG, "entries_shown=%d\n", entries_shown);

    /* If there are items in the list, highlight the selected row */
    if ((main) && (entries_shown > 0)) {
        /* First, select any record being searched for */
        if (glob_find_id) {
            memo_find();
        }
            /* Second, try the currently selected row */
        else if (row_selected < entries_shown) {
            gtk_tree_model_foreach(GTK_TREE_MODEL(pListStore), selectRecordByRowMemo, NULL);
        }
            /* Third, select row 0 if nothing else is possible */
        else {
            row_selected = 0;
            gtk_tree_model_foreach(GTK_TREE_MODEL(pListStore), selectRecordByRowMemo, NULL);
        }
    }


    if (tooltip_widget) {
        get_pref(PREF_SHOW_TOOLTIPS, &show_tooltips, NULL);
        if (memo_list == NULL) {
            set_tooltip((int) show_tooltips, tooltip_widget, _("0 records"));
        } else {
            sprintf(str, _("%d of %d records"), entries_shown, num_entries);
            set_tooltip((int) show_tooltips, tooltip_widget, str);
        }
    }

    if (main) {
        connect_changed_signals(CONNECT_SIGNALS);
    }

    /* return focus to treeView after any big operation which requires a redraw */
    gtk_widget_grab_focus(GTK_WIDGET(treeView));
    gtk_tree_selection_set_select_function(treeSelection, handleRowSelectionForMemo, NULL, NULL);
    jp_logf(JP_LOG_DEBUG, "Leaving memo_update_liststore()\n");
}

gboolean
findRecordMemo(GtkTreeModel *model,
               GtkTreePath *path,
               GtkTreeIter *iter,
               gpointer data) {

    if (glob_find_id) {
        MyMemo *mmemo = NULL;
        gtk_tree_model_get(model, iter, MEMO_DATA_COLUMN_ENUM, &mmemo, -1);

        if (mmemo->unique_id == glob_find_id) {
            GtkTreeSelection *selection = NULL;
            selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeView));
            gtk_tree_selection_set_select_function(selection, handleRowSelectionForMemo, NULL, NULL);
            gtk_tree_selection_select_path(selection, path);
            gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(treeView), path, MEMO_DATA_COLUMN_ENUM, FALSE, 1.0, 0.0);
            glob_find_id = 0;
            return TRUE;
        }
    }
    return FALSE;
}

static int memo_find(void) {
    gtk_tree_model_foreach(GTK_TREE_MODEL(listStore), findRecordMemo, NULL);
    return EXIT_SUCCESS;
}

static int memo_redraw(void) {
    memo_update_liststore(listStore, category_menu1, &glob_memo_list, memo_category, TRUE);
    return EXIT_SUCCESS;
}

int memo_cycle_cat(void) {
    int b;
    int i, new_cat;

    b = dialog_save_changed_record(pane, record_changed);
    if (b == DIALOG_SAID_2) {
        cb_add_new_record(NULL, GINT_TO_POINTER(record_changed));
    }

    if (memo_category == CATEGORY_ALL) {
        new_cat = -1;
    } else {
        new_cat = find_sort_cat_pos(memo_category);
    }

    for (i = 0; i < NUM_MEMO_CAT_ITEMS; i++) {
        new_cat++;
        if (new_cat >= NUM_MEMO_CAT_ITEMS) {
            memo_category = CATEGORY_ALL;
            break;
        }
        if ((sort_l[new_cat].Pcat) && (sort_l[new_cat].Pcat[0])) {
            memo_category = sort_l[new_cat].cat_num;
            break;
        }
    }

    row_selected = 0;

    return EXIT_SUCCESS;
}

int memo_refresh(void) {
    int index, index2;

    if (glob_find_id) {
        memo_category = CATEGORY_ALL;
    }
    if (memo_category == CATEGORY_ALL) {
        index = 0;
        index2 = 0;
    } else {
        index = find_sort_cat_pos(memo_category);
        index2 = find_menu_cat_pos(index) + 1;
        index += 1;
    }
    memo_update_liststore(listStore, category_menu1, &glob_memo_list, memo_category, TRUE);
    if (index < 0) {
        jp_logf(JP_LOG_WARN, _("Category is not legal\n"));
    } else {
        gtk_combo_box_set_active(GTK_COMBO_BOX(category_menu1), index2);
    }

    return EXIT_SUCCESS;
}

int memo_gui_cleanup(void) {
    int b;

    b = dialog_save_changed_record(pane, record_changed);
    if (b == DIALOG_SAID_2) {
        cb_add_new_record(NULL, GINT_TO_POINTER(record_changed));
    }
    free_MemoList(&glob_memo_list);
    connect_changed_signals(DISCONNECT_SIGNALS);
    set_pref(PREF_MEMO_PANE, gtk_paned_get_position(GTK_PANED(pane)), NULL, TRUE);
    set_pref(PREF_LAST_MEMO_CATEGORY, memo_category, NULL, TRUE);
    GtkTreeSelection* treeSelection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeView));
    gtk_tree_selection_set_select_function(treeSelection, NULL, NULL, NULL);
    gtk_list_store_clear(GTK_LIST_STORE(listStore));

    return EXIT_SUCCESS;
}

/* Main function */
int memo_gui(GtkWidget *vbox, GtkWidget *hbox) {
    int i;
    GtkWidget *scrolled_window;
    GtkWidget *vbox1, *vbox2, *hbox_temp;
    GtkWidget *separator;
    long ivalue;
    GtkAccelGroup *accel_group;
    long char_set;
    long show_tooltips;
    char *cat_name;

    get_pref(PREF_MEMO_VERSION, &memo_version, NULL);

    /* Do some initialization */
    row_selected = 0;

    record_changed = CLEAR_FLAG;

    get_memo_app_info(&memo_app_info);
    listStore = gtk_list_store_new(MEMO_NUM_COLS, G_TYPE_STRING, G_TYPE_POINTER, GDK_TYPE_RGBA,
                                   G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_BOOLEAN);

    /* Initialize categories */
    get_pref(PREF_CHAR_SET, &char_set, NULL);
    for (i = 1; i < NUM_MEMO_CAT_ITEMS; i++) {
        cat_name = charset_p2newj(memo_app_info.category.name[i], 31, (int) char_set);
        strcpy(sort_l[i - 1].Pcat, cat_name);
        free(cat_name);
        sort_l[i - 1].cat_num = i;
    }
    /* put reserved 'Unfiled' category at end of list */
    cat_name = charset_p2newj(memo_app_info.category.name[0], 31, (int) char_set);
    strcpy(sort_l[NUM_MEMO_CAT_ITEMS - 1].Pcat, cat_name);
    free(cat_name);
    sort_l[NUM_MEMO_CAT_ITEMS - 1].cat_num = 0;

    qsort(sort_l, NUM_MEMO_CAT_ITEMS - 1, sizeof(struct sorted_cats), cat_compare);

#ifdef JPILOT_DEBUG
    for (i=0; i<NUM_MEMO_CAT_ITEMS; i++) {
       printf("cat %d [%s]\n", sort_l[i].cat_num, sort_l[i].Pcat);
    }
#endif

    get_pref(PREF_LAST_MEMO_CATEGORY, &ivalue, NULL);
    memo_category = (int) ivalue;

    if ((memo_category != CATEGORY_ALL)
        && (memo_app_info.category.name[memo_category][0] == '\0')) {
        memo_category = CATEGORY_ALL;
    }

    /* Create basic GUI with left and right boxes and sliding pane */
    accel_group = gtk_accel_group_new();
    gtk_window_add_accel_group(GTK_WINDOW(gtk_widget_get_toplevel(vbox)),
                               accel_group);
    get_pref(PREF_SHOW_TOOLTIPS, &show_tooltips, NULL);

    pane = gtk_hpaned_new();
    get_pref(PREF_MEMO_PANE, &ivalue, NULL);
    gtk_paned_set_position(GTK_PANED(pane), (gint) ivalue);

    gtk_box_pack_start(GTK_BOX(hbox), pane, TRUE, TRUE, 5);

    vbox1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    gtk_paned_pack1(GTK_PANED(pane), vbox1, TRUE, FALSE);
    gtk_paned_pack2(GTK_PANED(pane), vbox2, TRUE, FALSE);

    /* Left side of GUI */

    /* Separator */
    separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(vbox1), separator, FALSE, FALSE, 5);

    /* 'Today is:' label */
    glob_date_label = gtk_label_new(" ");
    gtk_box_pack_start(GTK_BOX(vbox1), glob_date_label, FALSE, FALSE, 0);
    timeout_date(NULL);
    glob_date_timer_tag = g_timeout_add(CLOCK_TICK, timeout_sync_up, NULL);

    /* Separator */
    separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(vbox1), separator, FALSE, FALSE, 5);

    /* Left-side Category menu */
    hbox_temp = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(vbox1), hbox_temp, FALSE, FALSE, 0);

    make_category_menu(&category_menu1,
                       sort_l, cb_category, TRUE, TRUE);
    gtk_box_pack_start(GTK_BOX(hbox_temp), category_menu1, TRUE, TRUE, 0);

    /* Memo list scrolled window */
    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_set_border_width(GTK_CONTAINER(scrolled_window), 0);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(vbox1), scrolled_window, TRUE, TRUE, 0);


    initializeTreeView();
    gtk_container_add(GTK_CONTAINER(scrolled_window), GTK_WIDGET(treeView));

    /* Right side of GUI */

    hbox_temp = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
    gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, FALSE, FALSE, 0);

    /* Cancel button */
    CREATE_BUTTON(cancel_record_button, _("Cancel"), CANCEL, _("Cancel the modifications"), GDK_KEY_Escape, 0, "ESC")
    g_signal_connect(G_OBJECT(cancel_record_button), "clicked",
                       G_CALLBACK(cb_cancel), NULL);

    /* Delete Button */
    CREATE_BUTTON(delete_record_button, _("Delete"), DELETE, _("Delete the selected record"), GDK_d, GDK_CONTROL_MASK,
                  "Ctrl+D")
    g_signal_connect(G_OBJECT(delete_record_button), "clicked",
                       G_CALLBACK(cb_delete_memo),
                       GINT_TO_POINTER(DELETE_FLAG));

    /* Undelete Button */
    CREATE_BUTTON(undelete_record_button, _("Undelete"), UNDELETE, _("Undelete the selected record"), 0, 0, "")
    g_signal_connect(G_OBJECT(undelete_record_button), "clicked",
                       G_CALLBACK(cb_undelete_memo),
                       GINT_TO_POINTER(UNDELETE_FLAG));

    /* Copy button */
    CREATE_BUTTON(copy_record_button, _("Copy"), COPY, _("Copy the selected record"), GDK_c,
                  GDK_CONTROL_MASK | GDK_SHIFT_MASK, "Ctrl+Shift+C")
    g_signal_connect(G_OBJECT(copy_record_button), "clicked",
                       G_CALLBACK(cb_add_new_record),
                       GINT_TO_POINTER(COPY_FLAG));

    /* New button */
    CREATE_BUTTON(new_record_button, _("New Record"), NEW, _("Add a new record"), GDK_n, GDK_CONTROL_MASK, "Ctrl+N")
    g_signal_connect(G_OBJECT(new_record_button), "clicked",
                       G_CALLBACK(cb_add_new_record),
                       GINT_TO_POINTER(CLEAR_FLAG));

    /* "Add Record" button */
    CREATE_BUTTON(add_record_button, _("Add Record"), ADD, _("Add the new record"), GDK_KEY_Return, GDK_CONTROL_MASK,
                  "Ctrl+Enter")
    g_signal_connect(G_OBJECT(add_record_button), "clicked",
                       G_CALLBACK(cb_add_new_record),
                       GINT_TO_POINTER(NEW_FLAG));
#ifndef ENABLE_STOCK_BUTTONS
    gtk_widget_set_name(GTK_WIDGET(GTK_LABEL(gtk_bin_get_child(GTK_BIN(add_record_button)))),
                        "label_high");
#endif

    /* "Apply Changes" button */
    CREATE_BUTTON(apply_record_button, _("Apply Changes"), APPLY, _("Commit the modifications"), GDK_KEY_Return,
                  GDK_CONTROL_MASK, "Ctrl+Enter")
    g_signal_connect(G_OBJECT(apply_record_button), "clicked",
                       G_CALLBACK(cb_add_new_record),
                       GINT_TO_POINTER(MODIFY_FLAG));
#ifndef ENABLE_STOCK_BUTTONS
    gtk_widget_set_name(GTK_WIDGET(GTK_LABEL(gtk_bin_get_child(GTK_BIN(apply_record_button)))),
                        "label_high");
#endif

    /* Separator */
    separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(vbox2), separator, FALSE, FALSE, 5);

    /* Private check box */
    hbox_temp = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, FALSE, FALSE, 0);
    private_checkbox = gtk_check_button_new_with_label(_("Private"));
    gtk_box_pack_end(GTK_BOX(hbox_temp), private_checkbox, FALSE, FALSE, 0);

    /* Right-side Category menu */
   make_category_menu(&category_menu2,
                       sort_l, NULL, FALSE, FALSE);
    gtk_box_pack_start(GTK_BOX(hbox_temp), category_menu2, TRUE, TRUE, 0);

    /* Description text box */
    hbox_temp = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, FALSE, FALSE, 0);

    hbox_temp = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, TRUE, TRUE, 0);

    memo_text = gtk_text_view_new();
    memo_text_buffer = G_OBJECT(gtk_text_view_get_buffer(GTK_TEXT_VIEW(memo_text)));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(memo_text), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(memo_text), GTK_WRAP_WORD);

    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
    gtk_container_set_border_width(GTK_CONTAINER(scrolled_window), 1);
    gtk_container_add(GTK_CONTAINER(scrolled_window), memo_text);
    gtk_box_pack_start(GTK_BOX(hbox_temp), scrolled_window,TRUE,TRUE,0);
    /* Capture the Enter & Shift-Enter key combinations to move back and
     * forth between the left- and right-hand sides of the display. */
    g_signal_connect(G_OBJECT(treeView), "key_press_event",
                       G_CALLBACK(cb_key_pressed_left_side), memo_text);

    g_signal_connect(G_OBJECT(memo_text), "key_press_event",
                       G_CALLBACK(cb_key_pressed_right_side), treeView);

    /**********************************************************************/

    gtk_widget_show_all(vbox);
    gtk_widget_show_all(hbox);

    gtk_widget_hide(add_record_button);
    gtk_widget_hide(apply_record_button);
    gtk_widget_hide(undelete_record_button);
    gtk_widget_hide(cancel_record_button);

    memo_refresh();

    return EXIT_SUCCESS;
}

void initializeTreeView() {
    GtkTreeSelection *treeSelection = NULL;
    treeView = gtk_tree_view_new_with_model(GTK_TREE_MODEL(listStore));
    GtkCellRenderer *columnRenderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes("", columnRenderer, "text", MEMO_COLUMN_ENUM,
                                                                         "cell-background-gdk",
                                                                         MEMO_BACKGROUND_COLOR_ENUM,
                                                                         "cell-background-set",
                                                                         MEMO_BACKGROUND_COLOR_ENABLED_ENUM, NULL);
    gtk_tree_view_column_set_fixed_width(column, (gint) 50);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeView), FALSE);
    gtk_tree_view_insert_column(GTK_TREE_VIEW(treeView), column, 0);
    gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
    treeSelection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeView));
    gtk_tree_selection_set_select_function(treeSelection, handleRowSelectionForMemo, NULL, NULL);
    gtk_widget_set_events(treeView, GDK_BUTTON1_MOTION_MASK);
    g_signal_connect (G_OBJECT(treeView), "motion_notify_event",
                      G_CALLBACK(motion_notify_event), NULL);
    g_signal_connect (G_OBJECT(treeView), "button-press-event",
                      G_CALLBACK(button_pressed_for_motion), NULL);
    g_signal_connect (G_OBJECT(treeView), "button-release-event",
                      G_CALLBACK(button_released_for_motion), NULL);

}

gboolean handleRowSelectionForMemo(GtkTreeSelection *selection,
                                   GtkTreeModel *model,
                                   GtkTreePath *path,
                                   gboolean path_currently_selected,
                                   gpointer userdata) {
    GtkTreeIter iter;

    struct Memo *memo;
    MyMemo *mmemo;
    int b;
    int index, sorted_position;
    int unique_id;

    if ((gtk_tree_model_get_iter(model, &iter, path)) && (!path_currently_selected)) {
        int *i = gtk_tree_path_get_indices(path);
        row_selected = i[0];
        gtk_tree_model_get(model, &iter, MEMO_DATA_COLUMN_ENUM, &mmemo, -1);
        if ((record_changed == MODIFY_FLAG) || (record_changed == NEW_FLAG)) {
            if (mmemo != NULL) {
                unique_id = mmemo->unique_id;
            }

            b = dialog_save_changed_record_with_cancel(pane, record_changed);
            if (b == DIALOG_SAID_1) { /* Cancel */
                return TRUE;
            }
            if (b == DIALOG_SAID_3) { /* Save */
                cb_add_new_record(NULL, GINT_TO_POINTER(record_changed));
            }

            set_new_button_to(CLEAR_FLAG);
            return TRUE;
        }
        if (mmemo == NULL) {
            return TRUE;
        }

        if (mmemo->rt == DELETED_PALM_REC ||
            (mmemo->rt == DELETED_PC_REC))
            /* Possible later addition of undelete code for modified deleted records
               || mmemo->rt == MODIFIED_PALM_REC
            */
        {
            set_new_button_to(UNDELETE_FLAG);
        } else {
            set_new_button_to(CLEAR_FLAG);
        }

        connect_changed_signals(DISCONNECT_SIGNALS);

        memo = &(mmemo->memo);

        index = mmemo->attrib & 0x0F;
        sorted_position = find_sort_cat_pos(index);
        int pos = findSortedPostion(sorted_position, GTK_COMBO_BOX(category_menu2));
        if (pos != sorted_position && index != 0) {
            /* Illegal category */
            jp_logf(JP_LOG_DEBUG, "Category is not legal\n");
            index = sorted_position = 0;
        }

        if (sorted_position < 0) {
            jp_logf(JP_LOG_WARN, _("Category is not legal\n"));
        }
        gtk_combo_box_set_active(GTK_COMBO_BOX(category_menu2),find_menu_cat_pos(sorted_position));

        gtk_text_buffer_set_text(GTK_TEXT_BUFFER(memo_text_buffer), memo->text, -1);

        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(private_checkbox),
                                     mmemo->attrib & dlpRecAttrSecret);

        connect_changed_signals(CONNECT_SIGNALS);
        return TRUE; /* allow selection state to change */
    }
    return TRUE; /* allow selection state to change */
}
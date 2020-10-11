/*******************************************************************************
 * restore_gui.c
 * A module of J-Pilot http://jpilot.org
 *
 * Copyright (C) 2001-2014 by Judd Montgomery
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
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <gtk/gtk.h>

#include "i18n.h"
#include "utils.h"
#include "prefs.h"
#include "sync.h"
#include "log.h"
#include "restore.h"

/******************************* Global vars **********************************/
static GtkWidget *user_entry;
static GtkWidget *user_id_entry;
static GtkWidget *restoreTreeView;

enum {
    RESTORE_DISPLAY_COLUMN_ENUM = 0,
    RESTORE_DATA_COLUMN_ENUM,
    RESTORE_NUM_COLS
};

gboolean
checkForDuplication (GtkTreeModel *model,
                     GtkTreePath  *path,
                     GtkTreeIter  *iter,
                     gpointer data);

/****************************** Main Code *************************************/
static gboolean cb_restore_destroy(GtkWidget *widget) {
    gtk_main_quit();

    return FALSE;
}

static void cb_restore_ok(GtkWidget *widget, gpointer data) {
    GList *list, *temp_list;

    char *text;
    char file[FILENAME_MAX], backup_file[FILENAME_MAX];
    char home_dir[FILENAME_MAX];
    struct stat buf, backup_buf;
    int r1, r2;

    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(restoreTreeView));
    GtkListStore *listStore = GTK_LIST_STORE(model);
    GtkTreeIter iter;
    list = gtk_tree_selection_get_selected_rows(gtk_tree_view_get_selection(GTK_TREE_VIEW(restoreTreeView)),&model);

    get_home_file_name("", home_dir, sizeof(home_dir));

    /* Remove anything that was supposed to be installed */
    g_snprintf(file, sizeof(file), "%s/"EPN".install", home_dir);
    unlink(file);

    jp_logf(JP_LOG_WARN, "%s%s%s\n", "-----===== ", _("Restore Handheld"), " ======-----");

    for (temp_list = list; temp_list; temp_list = temp_list->next) {
        GtkTreePath * path = temp_list->data;
        GtkTreeIter iter;
        if(gtk_tree_model_get_iter(model,&iter,path)) {
            gtk_tree_model_get(model, &iter, RESTORE_DISPLAY_COLUMN_ENUM, &text, -1);
            int * i = gtk_tree_path_get_indices ( path ) ;
            jp_logf(JP_LOG_DEBUG, "row %ld [%s]\n", i[0], text);
            /* Look for the file in the JPILOT_HOME and JPILOT_HOME/backup.
             * Restore the newest modified date one, or the only one.  */
            g_snprintf(file, sizeof(file), "%s/%s", home_dir, text);
            g_snprintf(backup_file, sizeof(backup_file), "%s/backup/%s", home_dir, text);
            r1 = !stat(file, &buf);
            r2 = !stat(backup_file, &backup_buf);
            if (r1 && r2) {
                /* found in JPILOT_HOME and JPILOT_HOME/backup */
                if (buf.st_mtime > backup_buf.st_mtime) {
                    jp_logf(JP_LOG_DEBUG, "Restore: found in home and backup, using home file %s\n", text);
                    install_append_line(file);
                } else {
                    jp_logf(JP_LOG_DEBUG, "Restore: found in home and backup, using home/backup file %s\n", text);
                    install_append_line(backup_file);
                }
            } else if (r1) {
                /* only found in JPILOT_HOME */
                install_append_line(file);
                jp_logf(JP_LOG_DEBUG, "Restore: using home file %s\n", text);
            } else if (r2) {
                /* only found in JPILOT_HOME/backup */
                jp_logf(JP_LOG_DEBUG, "Restore: using home/backup file %s\n", text);
                install_append_line(backup_file);
            }
        }
    }

    setup_sync(SYNC_NO_PLUGINS | SYNC_OVERRIDE_USER | SYNC_RESTORE);

    gtk_widget_destroy(data);
}

static void cb_restore_quit(GtkWidget *widget, gpointer data) {
    gtk_widget_destroy(data);
}


/*
 * path is the dir to open
 * check_for_dups will check the listStore and not add if its a duplicate
 * check_exts will not add if its not a pdb, prc, or pqa.
 */
static int populate_listStore_subpath(char *path, int check_for_dups, int check_exts) {
    char *row_text[1];
    GtkListStore *listStore = GTK_LIST_STORE(gtk_tree_view_get_model(restoreTreeView));
    GtkTreeIter iter;
    DIR *dir;
    struct dirent *dirent;
    char last4[8];
    char *text;
    int i, num, len, found;

    jp_logf(JP_LOG_DEBUG, "opening dir %s\n", path);
    dir = opendir(path);
    num = 0;
    if (!dir) {
        jp_logf(JP_LOG_DEBUG, "opening dir failed\n");
    } else {
        for (i = 0; (dirent = readdir(dir)); i++) {
            if (i > 1000) {
                jp_logf(JP_LOG_WARN, "populate_listStore_subpath(): %s\n", _("infinite loop"));
                closedir(dir);
                return EXIT_FAILURE;
            }
            if (dirent->d_name[0] == '.') {
                continue;
            }
            if (!strncmp(dirent->d_name, "Unsaved Preferences", 17)) {
                jp_logf(JP_LOG_DEBUG, "skipping %s\n", dirent->d_name);
                continue;
            }
            if (check_exts) {
                len = strlen(dirent->d_name);
                if (len < 4) {
                    continue;
                }
                strncpy(last4, dirent->d_name + len - 4, 4);
                last4[4] = '\0';
                if (strcmp(last4, ".pdb") &&
                    strcmp(last4, ".prc") &&
                    strcmp(last4, ".pqa")) {
                    continue;
                }
            }

            if (check_for_dups) {
                found = 0;
                RestoreDataSearchElement * element = malloc(sizeof(RestoreDataSearchElement));;
                element ->found = 0;
                element ->textToSearch = dirent->d_name;
                gtk_tree_model_foreach(GTK_TREE_MODEL(listStore), checkForDuplication, element);
                found = element -> found;
                g_free(element);
                if (found == 1)
                    continue;

            }

            row_text[0] = dirent->d_name;
            {
                gchar *utf8_text;

                utf8_text = g_locale_to_utf8(row_text[0], -1, NULL, NULL, NULL);
                if (!utf8_text) {
                    jp_logf(JP_LOG_GUI, _("Unable to convert filename for GTK display\n"));
                    jp_logf(JP_LOG_GUI, _("See console log to find which file will not be restored\n"));
                    jp_logf(JP_LOG_STDOUT | JP_LOG_FILE, _("Unable to convert filename for GTK display\n"));
                    jp_logf(JP_LOG_STDOUT | JP_LOG_FILE, _("File %s will not be restored\n"), row_text[0]);
                    continue;
                }
                row_text[0] = utf8_text;
                gtk_list_store_append(listStore,&iter);
                gtk_list_store_set(listStore,&iter,RESTORE_DISPLAY_COLUMN_ENUM,row_text[0],-1);
                g_free(utf8_text);
            }
            num++;
        }
        closedir(dir);
    }


    return num;
}

static int populate_listStore(void) {
    char path[FILENAME_MAX];

    get_home_file_name("backup", path, sizeof(path));
    cleanup_path(path);
    populate_listStore_subpath(path, 0, 0);

    get_home_file_name("", path, sizeof(path));
    cleanup_path(path);
    //populate_listStore_subpath(path, 1, 1);
    populate_listStore_subpath(path, 1, 1);
    gtk_tree_selection_select_all(gtk_tree_view_get_selection(GTK_TREE_VIEW(restoreTreeView)));

    return EXIT_SUCCESS;
}
gboolean
checkForDuplication (GtkTreeModel *model,
            GtkTreePath  *path,
            GtkTreeIter  *iter,
            gpointer data) {
    char * rowText;
    RestoreDataSearchElement *element = data;
    gtk_tree_model_get(model,iter,RESTORE_DISPLAY_COLUMN_ENUM,&rowText,-1);
    if (data != NULL) {
        if ((strcmp(element->textToSearch, rowText))) {
            element->found = 1;
            return TRUE;
        }
    }
    return FALSE;

}

int restore_gui(GtkWidget *main_window, int w, int h, int x, int y) {
    GtkWidget *restore_window;
    GtkWidget *button;
    GtkWidget *vbox;
    GtkWidget *hbox;
    GtkWidget *scrolled_window;
    GtkWidget *label;
    GtkListStore *listStore;
    const char *svalue;
    long ivalue;
    long char_set;
    char str_int[20];

    jp_logf(JP_LOG_DEBUG, "restore_gui()\n");

    restore_window = gtk_widget_new(GTK_TYPE_WINDOW,
                                    "type", GTK_WINDOW_TOPLEVEL,
                                    "title", _("Restore Handheld"),
                                    NULL);

    gtk_window_set_default_size(GTK_WINDOW(restore_window), w, h);
    //gtk_widget_set_uposition(restore_window, x, y);
    gtk_container_set_border_width(GTK_CONTAINER(restore_window), 5);
    gtk_window_set_default_size(GTK_WINDOW(restore_window), w, h);
    gtk_window_set_modal(GTK_WINDOW(restore_window), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(restore_window), GTK_WINDOW(main_window));

    g_signal_connect(G_OBJECT(restore_window), "destroy",
                       G_CALLBACK(cb_restore_destroy), restore_window);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(restore_window), vbox);

    /* Label for instructions */
    label = gtk_label_new(_("To restore your handheld:"));
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
    label = gtk_label_new(_("1. Choose the applications you wish to restore.  The default is all."));
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
    label = gtk_label_new(_("2. Enter the User Name and User ID."));
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
    label = gtk_label_new(_("3. Press the OK button."));
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
    label = gtk_label_new(_("This will overwrite data that is currently on the handheld."));
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

    /* List of files to restore */
    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_set_border_width(GTK_CONTAINER(scrolled_window), 0);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 0);
    listStore = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_POINTER);
    restoreTreeView = gtk_tree_view_new_with_model(GTK_TREE_MODEL(listStore));
    GtkCellRenderer *textRenderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *textColumn = gtk_tree_view_column_new_with_attributes("",
                                                                             textRenderer,
                                                                             "text", RESTORE_DISPLAY_COLUMN_ENUM,
                                                                             NULL);
    gtk_tree_view_column_set_clickable(textColumn, gtk_false());
    gtk_tree_view_column_set_sizing(textColumn, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(restoreTreeView)),
                                GTK_SELECTION_MULTIPLE);
    gtk_tree_view_insert_column(GTK_TREE_VIEW(restoreTreeView),textColumn,RESTORE_DISPLAY_COLUMN_ENUM);
    gtk_tree_view_column_set_sort_column_id(textColumn, RESTORE_DISPLAY_COLUMN_ENUM);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(restoreTreeView), gtk_false());
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(listStore), RESTORE_DISPLAY_COLUMN_ENUM, GTK_SORT_ASCENDING);

    gtk_container_add(GTK_CONTAINER(scrolled_window), GTK_WIDGET(restoreTreeView));
    g_object_unref(listStore);
    /* User entry */
    hbox = gtk_hbox_new(FALSE, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    label = gtk_label_new(_("User Name"));
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    user_entry = gtk_entry_new();
    gtk_entry_set_max_length(user_entry,126);
    entry_set_multiline_truncate(GTK_ENTRY(user_entry), TRUE);
    get_pref(PREF_USER, NULL, &svalue);
    if ((svalue) && (svalue[0])) {
        /* Convert User Name stored in Palm character set */
        char user_name[128];

        get_pref(PREF_CHAR_SET, &char_set, NULL);
        g_strlcpy(user_name, svalue, 128);
        charset_p2j(user_name, 128, char_set);
        gtk_entry_set_text(GTK_ENTRY(user_entry), user_name);
    }
    gtk_box_pack_start(GTK_BOX(hbox), user_entry, TRUE, TRUE, 0);

    /* User ID entry */
    hbox = gtk_hbox_new(FALSE, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    label = gtk_label_new(_("User ID"));
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    user_id_entry = gtk_entry_new();
    gtk_entry_set_max_length(user_id_entry,10);
    entry_set_multiline_truncate(GTK_ENTRY(user_id_entry), TRUE);
    get_pref(PREF_USER_ID, &ivalue, NULL);
    sprintf(str_int, "%ld", ivalue);
    gtk_entry_set_text(GTK_ENTRY(user_id_entry), str_int);
    gtk_box_pack_start(GTK_BOX(hbox), user_id_entry, TRUE, TRUE, 0);

    /* Cancel/OK buttons */
    hbox = gtk_hbutton_box_new();
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 12);
    gtk_button_box_set_layout(GTK_BUTTON_BOX (hbox), GTK_BUTTONBOX_END);
     gtk_box_set_spacing(GTK_BOX(hbox), 6);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    button = gtk_button_new_with_label("Cancel");
    gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
    g_signal_connect(G_OBJECT(button), "clicked",
                       G_CALLBACK(cb_restore_quit), restore_window);

    button = gtk_button_new_with_label("OK");
    gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
    g_signal_connect(G_OBJECT(button), "clicked",
                       G_CALLBACK(cb_restore_ok), restore_window);

    populate_listStore();

    gtk_widget_show_all(restore_window);

    gtk_main();

    return EXIT_SUCCESS;
}


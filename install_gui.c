/*******************************************************************************
 * install_gui.c
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
#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "i18n.h"
#include "utils.h"
#include "prefs.h"
#include "log.h"

/********************************* Constants **********************************/
#define INST_SDCARD_COLUMN 0
#define INST_FNAME_COLUMN  1

/******************************* Global vars **********************************/
static GtkWidget *filew = NULL;
static GtkWidget *clist;
static GtkWidget *treeView;
static GtkListStore *listStore;
static int clist_row_selected;
static int column_selected;
enum {
    INSTALL_SDCARD_COLUMN_ENUM = 0,
    INSTALL_FNAME_COLUMN_ENUM,
    INSTALL_DATA_COLUMN_ENUM,
    INSTALL_BACKGROUND_COLOR_ENUM,
    INSTALL_BACKGROUND_COLOR_ENABLED_ENUM,
    INSTALL_NUM_COLS
};

/****************************** Prototypes ************************************/
static int install_update_clist(void);
static int install_update_listStore(void);
/****************************** Main Code *************************************/
static int install_remove_line(int deleted_line_num) {
    FILE *in;
    FILE *out;
    char line[1002];
    char *Pc;
    int r, line_count;

    in = jp_open_home_file(EPN".install", "r");
    if (!in) {
        jp_logf(JP_LOG_DEBUG, "failed opening install_file\n");
        return EXIT_FAILURE;
    }

    out = jp_open_home_file(EPN".install.tmp", "w");
    if (!out) {
        fclose(in);
        jp_logf(JP_LOG_DEBUG, "failed opening install_file.tmp\n");
        return EXIT_FAILURE;
    }

    /* Delete line by copying file and skipping over line to delete */
    for (line_count = 0; !feof(in); line_count++) {
        line[0] = '\0';
        Pc = fgets(line, 1000, in);
        if (!Pc) {
            break;
        }
        if (line_count == deleted_line_num) {
            continue;
        }
        r = fprintf(out, "%s", line);
        if (r == EOF) {
            break;
        }
    }
    fclose(in);
    fclose(out);

    rename_file(EPN".install.tmp", EPN".install");

    return EXIT_SUCCESS;
}

int install_append_line(const char *line) {
    FILE *out;
    int r;

    out = jp_open_home_file(EPN".install", "a");
    if (!out) {
        return EXIT_FAILURE;
    }

    r = fprintf(out, "%s\n", line);
    if (r == EOF) {
        fclose(out);
        return EXIT_FAILURE;
    }
    fclose(out);

    return EXIT_SUCCESS;
}

static int install_modify_line(int modified_line_num, const char *modified_line) {
    FILE *in;
    FILE *out;
    char line[1002];
    char *Pc;
    int r, line_count;

    in = jp_open_home_file(EPN".install", "r");
    if (!in) {
        jp_logf(JP_LOG_DEBUG, "failed opening install_file\n");
        return EXIT_FAILURE;
    }

    out = jp_open_home_file(EPN".install.tmp", "w");
    if (!out) {
        fclose(in);
        jp_logf(JP_LOG_DEBUG, "failed opening install_file.tmp\n");
        return EXIT_FAILURE;
    }

    /* Delete line by copying file and skipping over line to delete */
    for (line_count = 0; !feof(in); line_count++) {
        line[0] = '\0';
        Pc = fgets(line, 1000, in);
        if (!Pc) {
            break;
        }
        if (line_count == modified_line_num) {
            r = fprintf(out, "%s\n", modified_line);
        } else {
            r = fprintf(out, "%s", line);
        }
        if (r == EOF) {
            break;
        }
    }
    fclose(in);
    fclose(out);

    rename_file(EPN".install.tmp", EPN".install");

    return EXIT_SUCCESS;
}

static gboolean cb_destroy(GtkWidget *widget) {
    filew = NULL;

    gtk_main_quit();

    return TRUE;
}

/* Save working directory for future installs */
static void cb_quit(GtkWidget *widget, gpointer data) {
    const char *sel;
    char dir[MAX_PREF_LEN + 2];
    struct stat statb;
    int i;

    jp_logf(JP_LOG_DEBUG, "Quit\n");

    sel = gtk_file_selection_get_filename(GTK_FILE_SELECTION(data));

    g_strlcpy(dir, sel, MAX_PREF_LEN);

    if (stat(sel, &statb)) {
        jp_logf(JP_LOG_WARN, "File selected was not stat-able\n");
    }

    if (S_ISDIR(statb.st_mode)) {
        /* For directory, add '/' indicator to path */
        i = strlen(dir);
        dir[i] = '/', dir[i + 1] = '\0';
    } else {
        /* Otherwise, strip off filename to find actual directory */
        for (i = strlen(dir); i >= 0; i--) {
            if (dir[i] == '/') {
                dir[i + 1] = '\0';
                break;
            }
        }
    }

    set_pref(PREF_INSTALL_PATH, 0, dir, TRUE);

    filew = NULL;

    gtk_widget_destroy(data);
}

static void cb_add(GtkWidget *widget, gpointer data) {


    const char *sel;
    struct stat statb;

    jp_logf(JP_LOG_DEBUG, "install: cb_add\n");
    sel = gtk_file_selection_get_filename(GTK_FILE_SELECTION(data));
    jp_logf(JP_LOG_DEBUG, "file selected [%s]\n", sel);

    /* Check to see if its a regular file */
    if (stat(sel, &statb)) {
        jp_logf(JP_LOG_DEBUG, "File selected was not stat-able\n");
        return;
    }
    if (!S_ISREG(statb.st_mode)) {
        jp_logf(JP_LOG_DEBUG, "File selected was not a regular file\n");
        return;
    }

    install_append_line(sel);
    //install_update_clist();
    install_update_listStore();
}

static void cb_remove(GtkWidget *widget, gpointer data) {
    if (clist_row_selected < 0) {
        return;
    }
    jp_logf(JP_LOG_DEBUG, "Remove line %d\n", clist_row_selected);
    install_remove_line(clist_row_selected);
    install_update_listStore();
}

static void cb_clist_selection(GtkWidget *clist,
                               gint row,
                               gint column,
                               GdkEventButton *event,
                               gpointer data) {
    char fname[1000];
    char *gtk_str;

    clist_row_selected = row;

    if (column == INST_SDCARD_COLUMN) {
        /* Toggle display of SDCARD pixmap */
        if (gtk_clist_get_text(GTK_CLIST(clist), row, column, NULL)) {
            GdkPixmap *pixmap;
            GdkBitmap *mask;
            get_pixmaps(clist, PIXMAP_SDCARD, &pixmap, &mask);
            gtk_clist_set_pixmap(GTK_CLIST(clist), row, column, pixmap, mask);

            gtk_clist_get_text(GTK_CLIST(clist), row, INST_FNAME_COLUMN, &gtk_str);
            fname[0] = '\001';
            g_strlcpy(&fname[1], gtk_str, sizeof(fname) - 1);
            install_modify_line(row, fname);

        } else {
            gtk_clist_set_text(GTK_CLIST(clist), row, column, "");
            gtk_clist_get_text(GTK_CLIST(clist), row, INST_FNAME_COLUMN, &gtk_str);
            g_strlcpy(&fname[0], gtk_str, sizeof(fname));
            install_modify_line(row, fname);
        }
    }

    return;
}
gboolean
selectInstallRecordByRow (GtkTreeModel *model,
                   GtkTreePath  *path,
                   GtkTreeIter  *iter,
                   gpointer data) {
    int * i = gtk_tree_path_get_indices ( path ) ;
    if(i[0] == clist_row_selected){
        GtkTreeSelection * selection = NULL;
        selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeView));
        gtk_tree_selection_select_path(selection, path);
        gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(treeView), path,  INSTALL_SDCARD_COLUMN_ENUM,FALSE, 1.0, 0.0);
        return TRUE;
    }

    return FALSE;
}
static int install_update_listStore(void) {
    GtkTreeIter iter;
    GdkPixbuf *sdCardColumnDisplay;
    FILE *in;
    char line[1002];
    char *Pc;
    char *new_line[3];

    int last_row_selected;
    int count;
    int len;
    int sdcard_install;

    new_line[0] = "";
    new_line[1] = line;
    new_line[2] = NULL;

    last_row_selected = clist_row_selected;

    in = jp_open_home_file(EPN".install", "r");
    if (!in) {
        return EXIT_FAILURE;
    }


    gtk_list_store_clear(listStore);
    for (count = 0; !feof(in); count++) {
        line[0] = '\0';
        sdCardColumnDisplay = NULL;
        Pc = fgets(line, 1000, in);
        if (!Pc) {
            break;
        }

        /* Strip newline characters from end of string */
        len = strlen(line);
        if ((line[len - 1] == '\n') || (line[len - 1] == '\r')) line[len - 1] = '\0';
        if ((line[len - 2] == '\n') || (line[len - 2] == '\r')) line[len - 2] = '\0';

        sdcard_install = (line[0] == '\001');
        /* Strip char indicating SDCARD install from start of string */
        if (sdcard_install) {
            new_line[1] = &line[1];
        } else {
            new_line[1] = &line[0];
        }

        //gtk_clist_append(GTK_CLIST(clist), new_line);

        /* Add SDCARD icon for files to be installed on SDCARD */
        if (sdcard_install) {
          //  GdkPixmap *pixmap;
          //  GdkBitmap *mask;
          //  get_pixmaps(clist, PIXMAP_SDCARD, &pixmap, &mask);
            get_pixbufs(PIXMAP_SDCARD, &sdCardColumnDisplay);
          //  gtk_clist_set_pixmap(GTK_CLIST(clist), count, INST_SDCARD_COLUMN, pixmap, mask);
        }
        gtk_list_store_append(listStore, &iter);
        gtk_list_store_set(listStore, &iter,
                           INSTALL_SDCARD_COLUMN_ENUM, sdCardColumnDisplay,
                           INSTALL_FNAME_COLUMN_ENUM, line,
                           -1);
    }
    fclose(in);


    if (last_row_selected > count - 1) {
        last_row_selected = count - 1;
    }

    if (last_row_selected >= 0) {
        clist_row_selected = last_row_selected;
        gtk_tree_model_foreach(GTK_TREE_MODEL(listStore), selectInstallRecordByRow, NULL);
    }
   // gtk_clist_thaw(GTK_CLIST(clist));

    return EXIT_SUCCESS;
}
static int install_update_clist(void) {
    FILE *in;
    char line[1002];
    char *Pc;
    char *new_line[3];

    int last_row_selected;
    int count;
    int len;
    int sdcard_install;

    new_line[0] = "";
    new_line[1] = line;
    new_line[2] = NULL;

    last_row_selected = clist_row_selected;

    in = jp_open_home_file(EPN".install", "r");
    if (!in) {
        return EXIT_FAILURE;
    }

    gtk_signal_disconnect_by_func(GTK_OBJECT(clist),
                                  GTK_SIGNAL_FUNC(cb_clist_selection), NULL);

    gtk_clist_freeze(GTK_CLIST(clist));
    gtk_clist_clear(GTK_CLIST(clist));
#ifdef __APPLE__
    gtk_clist_thaw(GTK_CLIST(clist));
    gtk_widget_hide(clist);
    gtk_widget_show_all(clist);
    gtk_clist_freeze(GTK_CLIST(clist));
#endif

    for (count = 0; !feof(in); count++) {
        line[0] = '\0';
        Pc = fgets(line, 1000, in);
        if (!Pc) {
            break;
        }

        /* Strip newline characters from end of string */
        len = strlen(line);
        if ((line[len - 1] == '\n') || (line[len - 1] == '\r')) line[len - 1] = '\0';
        if ((line[len - 2] == '\n') || (line[len - 2] == '\r')) line[len - 2] = '\0';

        sdcard_install = (line[0] == '\001');
        /* Strip char indicating SDCARD install from start of string */
        if (sdcard_install) {
            new_line[1] = &line[1];
        } else {
            new_line[1] = &line[0];
        }

        gtk_clist_append(GTK_CLIST(clist), new_line);

        /* Add SDCARD icon for files to be installed on SDCARD */
        if (sdcard_install) {
            GdkPixmap *pixmap;
            GdkBitmap *mask;
            get_pixmaps(clist, PIXMAP_SDCARD, &pixmap, &mask);
            gtk_clist_set_pixmap(GTK_CLIST(clist), count, INST_SDCARD_COLUMN, pixmap, mask);
        }
    }
    fclose(in);

    gtk_signal_connect(GTK_OBJECT(clist), "select_row",
                       GTK_SIGNAL_FUNC(cb_clist_selection), NULL);

    if (last_row_selected > count - 1) {
        last_row_selected = count - 1;
    }
    if (last_row_selected >= 0) {
        clist_select_row(GTK_CLIST(clist), last_row_selected, INST_FNAME_COLUMN);
    }
    gtk_clist_thaw(GTK_CLIST(clist));

    return EXIT_SUCCESS;
}

static gboolean handleInstallRowSelection(GtkTreeSelection *selection,
                                          GtkTreeModel *model,
                                          GtkTreePath *path,
                                          gboolean path_currently_selected,
                                          gpointer userdata) {
    GtkTreeIter iter;
    char fname[1000];
    char *gtk_str;
    if ((gtk_tree_model_get_iter(model, &iter, path)) && (!path_currently_selected)) {

        int *i = gtk_tree_path_get_indices(path);
        clist_row_selected = i[0];


        if (column_selected == INST_SDCARD_COLUMN) {
            /* Toggle display of SDCARD pixmap */
            if (gtk_clist_get_text(GTK_CLIST(clist), i[0], column_selected, NULL)) {
                GdkPixmap *pixmap;
                GdkBitmap *mask;
                get_pixmaps(clist, PIXMAP_SDCARD, &pixmap, &mask);
                gtk_clist_set_pixmap(GTK_CLIST(clist), i[0], column_selected, pixmap, mask);

                gtk_clist_get_text(GTK_CLIST(clist), i[0], INST_FNAME_COLUMN, &gtk_str);
                fname[0] = '\001';
                g_strlcpy(&fname[1], gtk_str, sizeof(fname) - 1);
                install_modify_line(i[0], fname);

            } else {
                gtk_clist_set_text(GTK_CLIST(clist), i[0], column_selected, "");
                gtk_clist_get_text(GTK_CLIST(clist), i[0], INST_FNAME_COLUMN, &gtk_str);
                g_strlcpy(&fname[0], gtk_str, sizeof(fname));
                install_modify_line(i[0], fname);
            }
        }
    }

    return TRUE;
}

static void column_clicked_cb(GtkTreeViewColumn *column) {
    column_selected = column->sort_column_id;
    g_print("column was clicked\n");

}

int install_gui(GtkWidget *main_window, int w, int h, int x, int y) {
    GtkWidget *scrolled_window;
    GtkWidget *button;
    GtkWidget *label;
    GtkWidget *pixmapwid;
    GdkPixmap *pixmap;
    GdkBitmap *mask;
    char temp_str[256];
    const char *svalue;
    gchar *titles[] = {"", _("Files to install")};

    if (filew) {
        return EXIT_SUCCESS;
    }

    clist_row_selected = 0;

    g_snprintf(temp_str, sizeof(temp_str), "%s %s", PN, _("Install"));
    filew = gtk_widget_new(GTK_TYPE_FILE_SELECTION,
                           "type", GTK_WINDOW_TOPLEVEL,
                           "title", temp_str,
                           NULL);

    gtk_window_set_default_size(GTK_WINDOW(filew), w, h);
    gtk_widget_set_uposition(filew, x, y);

    gtk_window_set_modal(GTK_WINDOW(filew), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(filew), GTK_WINDOW(main_window));

    get_pref(PREF_INSTALL_PATH, NULL, &svalue);
    if (svalue && svalue[0]) {
        gtk_file_selection_set_filename(GTK_FILE_SELECTION(filew), svalue);
    }

    gtk_file_selection_hide_fileop_buttons((gpointer) filew);

    gtk_widget_hide((GTK_FILE_SELECTION(filew)->cancel_button));
    gtk_signal_connect(GTK_OBJECT(filew), "destroy",
                       GTK_SIGNAL_FUNC(cb_destroy), filew);

    /* Even though I hide the ok button I still want to connect its signal */
    /* because a double click on the file name also calls this callback */
    gtk_widget_hide(GTK_WIDGET(GTK_FILE_SELECTION(filew)->ok_button));
    gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(filew)->ok_button),
                       "clicked", GTK_SIGNAL_FUNC(cb_add), filew);

    clist = gtk_clist_new_with_titles(2, titles);
    listStore = gtk_list_store_new(INSTALL_NUM_COLS, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_POINTER,
                                   GDK_TYPE_COLOR, G_TYPE_BOOLEAN);
    treeView = gtk_tree_view_new_with_model(GTK_TREE_MODEL(listStore));
    GtkCellRenderer *sdRenderer = gtk_cell_renderer_pixbuf_new();
    GtkTreeViewColumn *sdColumn = gtk_tree_view_column_new_with_attributes("",
                                                                           sdRenderer,
                                                                           "pixbuf", INSTALL_SDCARD_COLUMN_ENUM,
                                                                           "cell-background-gdk",
                                                                           INSTALL_BACKGROUND_COLOR_ENUM,
                                                                           "cell-background-set",
                                                                           INSTALL_BACKGROUND_COLOR_ENABLED_ENUM,
                                                                           NULL);
    GtkCellRenderer *fileNameRenderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *fileNameColumn = gtk_tree_view_column_new_with_attributes("Files to install",
                                                                                 fileNameRenderer,
                                                                                 "text", INSTALL_FNAME_COLUMN_ENUM,
                                                                                 "cell-background-gdk",
                                                                                 INSTALL_BACKGROUND_COLOR_ENUM,
                                                                                 "cell-background-set",
                                                                                 INSTALL_BACKGROUND_COLOR_ENABLED_ENUM,
                                                                                 NULL);

    gtk_tree_view_column_set_clickable(sdColumn, gtk_false());
    gtk_tree_view_column_set_clickable(fileNameColumn, gtk_false());
    gtk_tree_view_column_set_sizing(sdColumn, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    gtk_widget_set_size_request(GTK_WIDGET(treeView), 0, 166);
    gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(treeView)),
                                GTK_SELECTION_BROWSE);

    gtk_widget_set_usize(GTK_WIDGET(clist), 0, 166);
    gtk_clist_column_titles_passive(GTK_CLIST(clist));
    gtk_clist_set_column_auto_resize(GTK_CLIST(clist), INST_SDCARD_COLUMN, TRUE);
    gtk_clist_set_selection_mode(GTK_CLIST(clist), GTK_SELECTION_BROWSE);
    gtk_tree_view_insert_column(GTK_TREE_VIEW (treeView), sdColumn, INSTALL_SDCARD_COLUMN_ENUM);
    gtk_tree_view_insert_column(GTK_TREE_VIEW (treeView), fileNameColumn, INSTALL_FNAME_COLUMN_ENUM);

    get_pixmaps(clist, PIXMAP_SDCARD, &pixmap, &mask);
#ifdef __APPLE__
    mask = NULL;
#endif
    pixmapwid = gtk_pixmap_new(pixmap, mask);
    gtk_tree_view_column_set_widget(sdColumn, pixmapwid);
    gtk_tree_view_column_set_alignment(sdColumn, GTK_JUSTIFY_CENTER);
    gtk_clist_set_column_widget(GTK_CLIST(clist), INST_SDCARD_COLUMN, pixmapwid);
    gtk_clist_set_column_justification(GTK_CLIST(clist), INST_SDCARD_COLUMN, GTK_JUSTIFY_CENTER);

    gtk_signal_connect(GTK_OBJECT(clist), "select_row",
                       GTK_SIGNAL_FUNC(cb_clist_selection), NULL);
    GtkTreeSelection *treeSelection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeView));

    gtk_tree_selection_set_select_function(treeSelection, handleInstallRowSelection, NULL, NULL);
    g_signal_connect (sdColumn, "clicked", G_CALLBACK(column_clicked_cb), NULL);
    g_signal_connect (fileNameColumn, "clicked", G_CALLBACK(column_clicked_cb), NULL);

    /* Scrolled Window for file list */
    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scrolled_window), GTK_WIDGET(treeView));
    // gtk_container_add(GTK_CONTAINER(scrolled_window), GTK_WIDGET(clist));
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_set_border_width(GTK_CONTAINER(scrolled_window), 5);
    gtk_box_pack_start(GTK_BOX(GTK_FILE_SELECTION(filew)->action_area),
                       scrolled_window, TRUE, TRUE, 0);

    label = gtk_label_new(_("To change to a hidden directory type it below and hit TAB"));
    gtk_box_pack_start(GTK_BOX(GTK_FILE_SELECTION(filew)->main_vbox),
                       label, FALSE, FALSE, 0);

    /* Add/Remove/Quit buttons */
    button = gtk_button_new_from_stock(GTK_STOCK_ADD);
    gtk_box_pack_start(GTK_BOX(GTK_FILE_SELECTION(filew)->ok_button->parent),
                       button, TRUE, TRUE, 0);
    gtk_signal_connect(GTK_OBJECT(button),
                       "clicked", GTK_SIGNAL_FUNC(cb_add), filew);

    button = gtk_button_new_from_stock(GTK_STOCK_REMOVE);
    gtk_box_pack_start(GTK_BOX(GTK_FILE_SELECTION(filew)->ok_button->parent),
                       button, TRUE, TRUE, 0);
    gtk_signal_connect(GTK_OBJECT(button),
                       "clicked", GTK_SIGNAL_FUNC(cb_remove), filew);

    button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
    gtk_box_pack_start(GTK_BOX(GTK_FILE_SELECTION(filew)->ok_button->parent),
                       button, TRUE, TRUE, 0);
    gtk_signal_connect(GTK_OBJECT(button),
                       "clicked", GTK_SIGNAL_FUNC(cb_quit), filew);

    /**********************************************************************/
    gtk_widget_show_all(filew);

    /* Hide default buttons not used by Jpilot file selector */
    gtk_widget_hide(GTK_FILE_SELECTION(filew)->cancel_button);
    gtk_widget_hide(GTK_FILE_SELECTION(filew)->ok_button);

    //install_update_clist();
    install_update_listStore();
    gtk_main();

    return EXIT_SUCCESS;
}


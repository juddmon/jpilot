/*******************************************************************************
 * export_gui.c
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
#include <sys/stat.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include <string.h>
#include <pi-appinfo.h>

#include "i18n.h"
#include "utils.h"
#include "log.h"
#include "prefs.h"
#include "export.h"

/********************************* Constants **********************************/
#define NUM_CAT_ITEMS 16

#define BROWSE_OK     1
#define BROWSE_CANCEL 2

/******************************* Global vars **********************************/
static GtkWidget *export_treeView;
static int export_category;

static int glob_export_browse_pressed;
static int glob_pref_export;

static GtkWidget *export_radio_type[10];
static int glob_export_type;
static GtkWidget *save_as_entry;
GtkWidget *category_menu;

/****************************** Prototypes ************************************/
static void (*glob_cb_export_menu)(GtkWidget *treeView, int category);

static GtkWidget *(*glob_cb_init_menu)();

static void (*glob_cb_export_done)(GtkWidget *widget, const char *filename);

static void (*glob_cb_export_ok)(GtkWidget *export_window,
                                 GtkWidget *treeView,
                                 int type,
                                 const char *filename);

/****************************** Main Code *************************************/
/* 
 * Browse GUI
 */
static gboolean cb_export_browse_destroy(GtkWidget *widget) {
    gtk_main_quit();
    return FALSE;
}

static void cb_export_browse_cancel(GtkWidget *widget, gpointer data) {
    glob_export_browse_pressed = BROWSE_CANCEL;
    gtk_widget_destroy(widget);
}

static void cb_export_browse_ok(GtkWidget *widget, gpointer data) {
    char *sel;
    int pref = GPOINTER_TO_INT(data);
    glob_export_browse_pressed = BROWSE_OK;
    if (pref) {
        sel = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER (widget));
        set_pref(pref, 0, sel, TRUE);
        gtk_entry_set_text(GTK_ENTRY(save_as_entry), sel);
    }
    gtk_widget_destroy(widget);
}

int export_browse(GtkWidget *main_window, int pref_export) {
    GtkWidget *fileChooserWidget;
    const char *svalue;
    char dir[MAX_PREF_LEN + 2];
    int i;

    glob_export_browse_pressed = 0;
    if (pref_export) {
        glob_pref_export = pref_export;
    } else {
        glob_pref_export = 0;
    }

    if (pref_export) {
        get_pref(pref_export, NULL, &svalue);
        g_strlcpy(dir, svalue, sizeof(dir));
        i = strlen(dir) - 1;
        if (i < 0) i = 0;
        if (dir[i] != '/') {
            for (i = strlen(dir); i >= 0; i--) {
                if (dir[i] == '/') {
                    dir[i + 1] = '\0';
                    break;
                }
            }
        }

        if (chdir(dir) < 0) {
            jp_logf(JP_LOG_WARN, "chdir failed %s %d\n", __FILE__, __LINE__);
        }
    }
    fileChooserWidget = gtk_file_chooser_dialog_new(_("File Browser"), GTK_WINDOW(main_window), GTK_FILE_CHOOSER_ACTION_SAVE,
                                                    "Cancel", GTK_RESPONSE_CANCEL, "Open",
                                                    GTK_RESPONSE_ACCEPT, NULL);
    gtk_file_chooser_set_show_hidden(GTK_FILE_CHOOSER(fileChooserWidget), TRUE);
    //This blocks main thread until they close the dialog.
    if (gtk_dialog_run(GTK_DIALOG (fileChooserWidget)) == GTK_RESPONSE_ACCEPT) {
        cb_export_browse_ok(fileChooserWidget, GINT_TO_POINTER(pref_export));
        glob_export_browse_pressed = BROWSE_OK;
    } else {
        cb_export_browse_cancel(fileChooserWidget, NULL);
    }
    cb_export_browse_destroy(fileChooserWidget);

    gtk_main();
    return glob_export_browse_pressed;
}

/* End Export Browse */

/*
 * Start Export code
 */
static gboolean cb_export_destroy(GtkWidget *widget) {
    const char *filename;

    filename = gtk_entry_get_text(GTK_ENTRY(save_as_entry));
    if (glob_cb_export_done) {
        glob_cb_export_done(widget, filename);
    }
    gtk_list_store_clear(GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(export_treeView))));
    if (category_menu != NULL) {
        GtkTreeModel *clearingmodel = gtk_combo_box_get_model(GTK_COMBO_BOX(category_menu));
        gtk_list_store_clear(GTK_LIST_STORE(clearingmodel));
    }
    gtk_main_quit();

    return FALSE;
}

static void cb_ok(GtkWidget *widget, gpointer data) {
    const char *filename;

    filename = gtk_entry_get_text(GTK_ENTRY(save_as_entry));

    if (glob_cb_export_ok) {
        glob_cb_export_ok(data, export_treeView, glob_export_type, filename);
    }

    gtk_widget_destroy(data);
}

static void cb_export_browse(GtkWidget *widget, gpointer data) {
    export_browse(GTK_WIDGET(data), glob_pref_export);
}

static void cb_export_quit(GtkWidget *widget, gpointer data) {
    gtk_widget_destroy(data);
}

static void cb_export_type(GtkWidget *widget, gpointer data) {
    glob_export_type = GPOINTER_TO_INT(data);
}

static void cb_export_category(GtkComboBox *item, int selection) {
    if (!item) return;
    if (gtk_combo_box_get_active(GTK_COMBO_BOX(item)) < 0) {
        return;
    }
    int selectedItem = get_selected_category_from_combo_box(item);
    if (selectedItem == -1) {
        return;
    }
    export_category = selectedItem;
    jp_logf(JP_LOG_DEBUG, "cb_export_category() cat=%d\n", export_category);
    if (glob_cb_export_menu && export_treeView != NULL) {
        glob_cb_export_menu(export_treeView, export_category);
        gtk_tree_selection_select_all(gtk_tree_view_get_selection(GTK_TREE_VIEW(export_treeView)));
    }


    jp_logf(JP_LOG_DEBUG, "Leaving cb_export_category()\n");
    //gtk_combo_box_set_active(GTK_COMBO_BOX(item), pos);

}

int export_gui(GtkWidget *main_window,
               int w, int h, int x, int y,
               int columns,
               struct sorted_cats *sort_l,
               int pref_export,
               char *type_text[],
               int type_int[],
               GtkWidget *(*cb_init_menu)(),
               void (*cb_export_menu)(GtkWidget *treeView, int category),
               void (*cb_export_done)(GtkWidget *widget,
                                      const char *filename),
               void (*cb_export_ok)(GtkWidget *export_window,
                                    GtkWidget *treeView,
                                    int type,
                                    const char *filename)
) {
    GtkWidget *export_window;
    GtkWidget *button;
    GtkWidget *vbox;
    GtkWidget *hbox;


    GtkWidget *scrolled_window;
    GtkWidget *label;
    char title[256];
    GSList *group;
    int i;
    const char *svalue;

    jp_logf(JP_LOG_DEBUG, "export_gui()\n");

    export_category = CATEGORY_ALL;

    /* Set the export type to the first type available */
    glob_export_type = type_int[0];
    glob_cb_init_menu = cb_init_menu;
    glob_cb_export_menu = cb_export_menu;
    glob_cb_export_done = cb_export_done;
    glob_cb_export_ok = cb_export_ok;

    glob_pref_export = pref_export;

    g_snprintf(title, sizeof(title), "%s %s", PN, _("Export"));

    export_window = gtk_widget_new(GTK_TYPE_WINDOW,
                                   "type", GTK_WINDOW_TOPLEVEL,
                                   "title", title,
                                   NULL);

    gtk_window_set_default_size(GTK_WINDOW(export_window), w, h);
    //gtk_widget_set_uposition(GTK_WIDGET(export_window), x, y);

    gtk_window_set_modal(GTK_WINDOW(export_window), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(export_window), GTK_WINDOW(main_window));

    gtk_container_set_border_width(GTK_CONTAINER(export_window), 5);

    g_signal_connect(G_OBJECT(export_window), "destroy",
                       G_CALLBACK(cb_export_destroy), export_window);

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(export_window), vbox);

    /* Label for instructions */
    label = gtk_label_new(_("Select records to be exported"));
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
    label = gtk_label_new(_("Use Ctrl and Shift Keys"));
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

    /* Put the export category menu up */
    if (category_menu  && GTK_IS_COMBO_BOX(category_menu)) {
        GtkTreeModel *clearingmodel = gtk_combo_box_get_model(GTK_COMBO_BOX(category_menu));
        gtk_list_store_clear(GTK_LIST_STORE(clearingmodel));
    }
    make_category_menu(&category_menu, sort_l,
                       cb_export_category, TRUE, FALSE);
    gtk_combo_box_set_active(GTK_COMBO_BOX(category_menu), 0);
    gtk_box_pack_start(GTK_BOX(vbox), category_menu, FALSE, FALSE, 0);

    /* Put the record list window up */
    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_set_border_width(GTK_CONTAINER(scrolled_window), 0);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 0);


    if (glob_cb_init_menu) {
        export_treeView = glob_cb_init_menu();
    } else {
        export_treeView = gtk_tree_view_new();
    }
    gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(export_treeView)),
                                GTK_SELECTION_MULTIPLE);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(export_treeView), gtk_false());

    gtk_container_add(GTK_CONTAINER(scrolled_window), GTK_WIDGET(export_treeView));

    /* Export Type buttons */
    group = NULL;
    for (i = 0; i < 100; i++) {
        if (type_text[i] == NULL) break;
        export_radio_type[i] = gtk_radio_button_new_with_label(group, _(type_text[i]));
        group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(export_radio_type[i]));
        gtk_box_pack_start(GTK_BOX(vbox), export_radio_type[i], FALSE, FALSE, 0);
        g_signal_connect(G_OBJECT(export_radio_type[i]), "pressed",
                           G_CALLBACK(cb_export_type),
                           GINT_TO_POINTER(type_int[i]));
    }
    export_radio_type[i] = NULL;

    /* Save As entry */
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    label = gtk_label_new(_("Save as"));
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    save_as_entry = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(save_as_entry), 250);
    svalue = NULL;
    if (glob_pref_export) {
        get_pref(glob_pref_export, NULL, &svalue);
    }
    if (svalue) {
        gtk_entry_set_text(GTK_ENTRY(save_as_entry), svalue);
    }
    gtk_box_pack_start(GTK_BOX(hbox), save_as_entry, TRUE, TRUE, 0);

    /* Browse button */
    button = gtk_button_new_with_label(_("Browse"));
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(button), "clicked",
                       G_CALLBACK(cb_export_browse), export_window);

    /* Cancel/OK buttons */
    hbox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 12);
    gtk_button_box_set_layout(GTK_BUTTON_BOX (hbox), GTK_BUTTONBOX_END);
     gtk_box_set_spacing(GTK_BOX(hbox), 6);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    button = gtk_button_new_with_label("Cancel");
    gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
    g_signal_connect(G_OBJECT(button), "clicked",
                       G_CALLBACK(cb_export_quit), export_window);

    button = gtk_button_new_with_label("OK");
    gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
    g_signal_connect(G_OBJECT(button), "clicked",
                       G_CALLBACK(cb_ok), export_window);

    if (glob_cb_export_menu) {
        glob_cb_export_menu(GTK_WIDGET(export_treeView), export_category);
    }

    gtk_widget_show_all(export_window);

    gtk_tree_selection_select_all(gtk_tree_view_get_selection(GTK_TREE_VIEW(export_treeView)));

    gtk_main();

    return EXIT_SUCCESS;
}


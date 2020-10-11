/*******************************************************************************
 * search_gui.c
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
#include <ctype.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <pi-calendar.h>
#include <pi-address.h>
#include <pi-todo.h>
#include <pi-memo.h>

#include "i18n.h"
#include "utils.h"
#include "prefs.h"
#include "log.h"
#include "datebook.h"
#include "calendar.h"
#include "address.h"
#include "todo.h"
#include "memo.h"

#ifdef ENABLE_PLUGINS

#  include "plugins.h"

#endif

/********************************* Constants **********************************/
#define SEARCH_MAX_COLUMN_LEN 80

/******************************* Global vars **********************************/
static struct search_record *search_rl = NULL;
static GtkWidget *case_sense_checkbox;
static GtkWidget *window = NULL;
static GtkWidget *entry = NULL;
static GtkAccelGroup *accel_group = NULL;

static int row_selected;

/****************************** Prototypes ************************************/

void selectFirstRow(const GtkTreeView *treeView);

enum {
    SEARCH_APP_NAME_COLUMN_ENUM = 0,
    SEARCH_TEXT_COLUMN_ENUM,
    SEARCH_DATA_ENUM,
    SEARCH_NUM_COLS
};

/****************************** Main Code *************************************/
static int datebook_search_sort_compare(const void *v1, const void *v2) {
    CalendarEventList **cel1, **cel2;
    struct CalendarEvent *ce1, *ce2;
    time_t time1, time2;

    cel1 = (CalendarEventList **) v1;
    cel2 = (CalendarEventList **) v2;

    ce1 = &((*cel1)->mcale.cale);
    ce2 = &((*cel2)->mcale.cale);

    time1 = mktime(&(ce1->begin));
    time2 = mktime(&(ce2->begin));

    return (time2 - time1);
}

static int search_datebook(const char *needle, GtkListStore *listStore, GtkTreeIter *iter) {
    gchar *empty_line[] = {"", ""};
    CalendarEventList *ce_list;
    CalendarEventList *temp_cel;
    int found, count;
    int case_sense;
    char str[202];
    char str2[SEARCH_MAX_COLUMN_LEN + 2];
    char date_str[52];
    char datef[52];
    const char *svalue1;
    struct search_record *new_sr;
    long datebook_version = 0;
    char *appName;
    char *text;

    get_pref(PREF_DATEBOOK_VERSION, &datebook_version, NULL);

    /* Search Appointments */
    ce_list = NULL;

    get_days_calendar_events2(&ce_list, NULL, 2, 2, 2, CATEGORY_ALL, NULL);

    if (ce_list == NULL) {
        return 0;
    }

    /* Sort returned results according to date rather than just HH:MM */
    calendar_sort(&ce_list, datebook_search_sort_compare);

    count = 0;
    case_sense = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(case_sense_checkbox));

    for (temp_cel = ce_list; temp_cel; temp_cel = temp_cel->next) {
        found = 0;
        if ((temp_cel->mcale.cale.description) &&
            (temp_cel->mcale.cale.description[0])) {
            if (jp_strstr(temp_cel->mcale.cale.description, needle, case_sense)) {
                found = 1;
            }
        }
        if (!found &&
            (temp_cel->mcale.cale.note) &&
            (temp_cel->mcale.cale.note[0])) {
            if (jp_strstr(temp_cel->mcale.cale.note, needle, case_sense)) {
                found = 2;
            }
        }
        if (datebook_version) {
            if (!found &&
                (temp_cel->mcale.cale.location) &&
                (temp_cel->mcale.cale.location[0])) {
                if (jp_strstr(temp_cel->mcale.cale.location, needle, case_sense)) {
                    found = 3;
                }
            }
        }

        if (found) {
            text = NULL;
            gtk_list_store_append(listStore, iter);
            if (datebook_version == 0) {
                appName = _("datebook");
            } else {
                appName = _("calendar");
            }

            /* get the date */
            get_pref(PREF_SHORTDATE, NULL, &svalue1);
            if (svalue1 == NULL) {
                strcpy(datef, "%x");
            } else {
                strncpy(datef, svalue1, sizeof(datef));
            }
            strftime(date_str, sizeof(date_str), datef, &temp_cel->mcale.cale.begin);
            date_str[sizeof(date_str) - 1] = '\0';

            if (found == 1) {
                g_snprintf(str, sizeof(str), "%s\t%s",
                           date_str,
                           temp_cel->mcale.cale.description);
            } else if (found == 2) {
                g_snprintf(str, sizeof(str), "%s\t%s",
                           date_str,
                           temp_cel->mcale.cale.note);
            } else {
                g_snprintf(str, sizeof(str), "%s\t%s",
                           date_str,
                           temp_cel->mcale.cale.location);
            }
            lstrncpy_remove_cr_lfs(str2, str, SEARCH_MAX_COLUMN_LEN);
            text = str2;

            /* Add to the search list */
            new_sr = malloc(sizeof(struct search_record));
            new_sr->app_type = DATEBOOK;
            new_sr->plugin_flag = 0;
            new_sr->unique_id = temp_cel->mcale.unique_id;
            new_sr->next = search_rl;
            search_rl = new_sr;

            gtk_list_store_set(listStore, iter,
                               SEARCH_APP_NAME_COLUMN_ENUM, appName,
                               SEARCH_TEXT_COLUMN_ENUM, text,
                               SEARCH_DATA_ENUM, new_sr, -1);

            count++;
        }
    }

    jp_logf(JP_LOG_DEBUG, "calling free_CalendarEventList\n");
    free_CalendarEventList(&ce_list);

    return count;
}

static int search_address_or_contacts(const char *needle, GtkListStore *listStore, GtkTreeIter *iter) {
    gchar *empty_line[] = {"", ""};
    char str2[SEARCH_MAX_COLUMN_LEN + 2];
    AddressList *addr_list;
    ContactList *cont_list;
    ContactList *temp_cl;
    char *appName;
    char *text;
    struct search_record *new_sr;
    int i, count;
    int case_sense;
    long address_version = 0;

    get_pref(PREF_ADDRESS_VERSION, &address_version, NULL);

    /* Get addresses and move to a contacts structure, or get contacts directly */
    if (address_version == 0) {
        addr_list = NULL;
        get_addresses2(&addr_list, SORT_ASCENDING, 2, 2, 2, CATEGORY_ALL);
        copy_addresses_to_contacts(addr_list, &cont_list);
        free_AddressList(&addr_list);
    } else {
        cont_list = NULL;
        get_contacts2(&cont_list, SORT_ASCENDING, 2, 2, 2, CATEGORY_ALL);
    }

    if (cont_list == NULL) {
        return 0;
    }

    count = 0;
    case_sense = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(case_sense_checkbox));

    for (temp_cl = cont_list; temp_cl; temp_cl = temp_cl->next) {
        for (i = 0; i < NUM_CONTACT_ENTRIES; i++) {
            if (temp_cl->mcont.cont.entry[i]) {
                if (jp_strstr(temp_cl->mcont.cont.entry[i], needle, case_sense)) {
                    gtk_list_store_append(listStore, iter);
                    if (address_version == 0) {
                        appName = _("address");
                    } else {
                        appName = _("contact");
                    }
                    lstrncpy_remove_cr_lfs(str2, temp_cl->mcont.cont.entry[i], SEARCH_MAX_COLUMN_LEN);
                    text = str2;
                    /* Add to the search list */
                    new_sr = malloc(sizeof(struct search_record));
                    new_sr->app_type = ADDRESS;
                    new_sr->plugin_flag = 0;
                    new_sr->unique_id = temp_cl->mcont.unique_id;
                    new_sr->next = search_rl;
                    search_rl = new_sr;

                    gtk_list_store_set(listStore, iter,
                                       SEARCH_APP_NAME_COLUMN_ENUM, appName,
                                       SEARCH_TEXT_COLUMN_ENUM, text,
                                       SEARCH_DATA_ENUM, new_sr, -1);


                    count++;

                    break;
                }
            }
        }
    }

    jp_logf(JP_LOG_DEBUG, "calling free_ContactList\n");
    free_ContactList(&cont_list);

    return count;
}

static int search_todo(const char *needle, GtkListStore *listStore, GtkTreeIter *iter) {
    gchar *empty_line[] = {"", ""};
    char str2[SEARCH_MAX_COLUMN_LEN + 2];
    char *appName = _("todo");
    ToDoList *todo_list;
    ToDoList *temp_todo;
    struct search_record *new_sr;
    int found, count;
    int case_sense;

    /* Search Appointments */
    todo_list = NULL;

    get_todos2(&todo_list, SORT_DESCENDING, 2, 2, 2, 1, CATEGORY_ALL);

    if (todo_list == NULL) {
        return 0;
    }

    count = 0;
    case_sense = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(case_sense_checkbox));

    for (temp_todo = todo_list; temp_todo; temp_todo = temp_todo->next) {
        found = 0;
        if ((temp_todo->mtodo.todo.description) &&
            (temp_todo->mtodo.todo.description[0])) {
            if (jp_strstr(temp_todo->mtodo.todo.description, needle, case_sense)) {
                found = 1;
            }
        }
        if (!found &&
            (temp_todo->mtodo.todo.note) &&
            (temp_todo->mtodo.todo.note[0])) {
            if (jp_strstr(temp_todo->mtodo.todo.note, needle, case_sense)) {
                found = 2;
            }
        }

        if (found) {
            gtk_list_store_append(listStore, iter);

            if (found == 1) {
                lstrncpy_remove_cr_lfs(str2, temp_todo->mtodo.todo.description, SEARCH_MAX_COLUMN_LEN);
            } else {
                lstrncpy_remove_cr_lfs(str2, temp_todo->mtodo.todo.note, SEARCH_MAX_COLUMN_LEN);
            }


            /* Add to the search list */
            new_sr = malloc(sizeof(struct search_record));
            new_sr->app_type = TODO;
            new_sr->plugin_flag = 0;
            new_sr->unique_id = temp_todo->mtodo.unique_id;
            new_sr->next = search_rl;
            search_rl = new_sr;
            gtk_list_store_set(listStore, iter,
                               SEARCH_APP_NAME_COLUMN_ENUM, appName,
                               SEARCH_TEXT_COLUMN_ENUM, str2,
                               SEARCH_DATA_ENUM, new_sr, -1);


            count++;
        }
    }

    jp_logf(JP_LOG_DEBUG, "calling free_ToDoList\n");
    free_ToDoList(&todo_list);

    return count;
}

static int search_memo(const char *needle, GtkListStore *listStore, GtkTreeIter *iter) {
    gchar *empty_line[] = {"", ""};
    char str2[SEARCH_MAX_COLUMN_LEN + 2];
    MemoList *memo_list;
    MemoList *temp_memo;
    struct search_record *new_sr;
    int count;
    int case_sense;
    long memo_version = 0;
    char *appName;
    char *text;

    get_pref(PREF_MEMO_VERSION, &memo_version, NULL);

    /* Search Memos */
    memo_list = NULL;

    get_memos2(&memo_list, SORT_DESCENDING, 2, 2, 2, CATEGORY_ALL);

    if (memo_list == NULL) {
        return 0;
    }

    count = 0;
    case_sense = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(case_sense_checkbox));

    for (temp_memo = memo_list; temp_memo; temp_memo = temp_memo->next) {
        if (jp_strstr(temp_memo->mmemo.memo.text, needle, case_sense)) {
            text = NULL;
            gtk_list_store_append(listStore, iter);
            if (memo_version == 0) {
                appName = _("memo");

            } else {
                appName = _("memos");
            }
            if (temp_memo->mmemo.memo.text) {
                lstrncpy_remove_cr_lfs(str2, temp_memo->mmemo.memo.text, SEARCH_MAX_COLUMN_LEN);
                text = str2;
            }

            /* Add to the search list */
            new_sr = malloc(sizeof(struct search_record));
            new_sr->app_type = MEMO;
            new_sr->plugin_flag = 0;
            new_sr->unique_id = temp_memo->mmemo.unique_id;
            new_sr->next = search_rl;
            search_rl = new_sr;

            gtk_list_store_set(listStore, iter,
                               SEARCH_APP_NAME_COLUMN_ENUM, appName,
                               SEARCH_TEXT_COLUMN_ENUM, text,
                               SEARCH_DATA_ENUM, new_sr, -1);

            count++;
        }
    }

    jp_logf(JP_LOG_DEBUG, "calling free_MemoList\n");
    free_MemoList(&memo_list);

    return count;
}

#ifdef ENABLE_PLUGINS

static int search_plugins(const char *needle, const GtkListStore *listStore, GtkTreeIter *iter) {
    GList *plugin_list, *temp_list;
    gchar *empty_line[] = {"", ""};
    char str2[SEARCH_MAX_COLUMN_LEN + 2];
    int count;
    int case_sense;
    struct search_result *sr, *temp_sr;
    struct plugin_s *plugin;
    struct search_record *new_sr;
    char *appName;
    char *text;

    plugin_list = NULL;
    plugin_list = get_plugin_list();

    case_sense = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(case_sense_checkbox));

    count = 0;
    for (temp_list = plugin_list; temp_list; temp_list = temp_list->next) {
        plugin = (struct plugin_s *) temp_list->data;
        if (plugin) {
            sr = NULL;
            if (plugin->plugin_search) {
                if (plugin->plugin_search(needle, case_sense, &sr) > 0) {
                    for (temp_sr = sr; temp_sr; temp_sr = temp_sr->next) {
                        text = NULL;
                        gtk_list_store_append(listStore, iter);
                        if (plugin->menu_name) {
                            appName = plugin->menu_name;
                        } else {
                            appName = _("plugin ?");
                        }
                        if (temp_sr->line) {
                            lstrncpy_remove_cr_lfs(str2, temp_sr->line, SEARCH_MAX_COLUMN_LEN);
                            text = str2;
                        }

                        /* Add to the search list */
                        new_sr = malloc(sizeof(struct search_record));
                        new_sr->app_type = plugin->number;
                        new_sr->plugin_flag = 1;
                        new_sr->unique_id = temp_sr->unique_id;
                        new_sr->next = search_rl;
                        search_rl = new_sr;

                        gtk_list_store_set(listStore, iter,
                                           SEARCH_APP_NAME_COLUMN_ENUM, appName,
                                           SEARCH_TEXT_COLUMN_ENUM, text,
                                           SEARCH_DATA_ENUM, new_sr, -1);

                        count++;
                    }
                    free_search_result(&sr);
                }
            }
        }
    }

    return count;
}

#endif

static gboolean cb_destroy(GtkWidget *widget) {
    if (search_rl) {
        free_search_record_list(&search_rl);
        search_rl = NULL;
    }
    window = NULL;

    return FALSE;
}

static void cb_quit(GtkWidget *widget, gpointer data) {
    gtk_widget_destroy(data);
}

static void cb_entry(GtkWidget *widget, gpointer data) {
    gchar *empty_line[] = {"", ""};
    GtkListStore *listStore;
    GtkTreeView *treeView;
    GtkTreeIter iter;
    const char *entry_text;
    int count = 0;

    jp_logf(JP_LOG_DEBUG, "enter cb_entry\n");

    entry_text = gtk_entry_get_text(GTK_ENTRY(widget));
    if (!entry_text || !strlen(entry_text)) {
        return;
    }

    jp_logf(JP_LOG_DEBUG, "entry text = %s\n", entry_text);

    treeView = data;
    listStore = GTK_LIST_STORE(gtk_tree_view_get_model(treeView));
    gtk_list_store_clear(listStore);
    count += search_address_or_contacts(entry_text, listStore, &iter);
    count += search_todo(entry_text, listStore, &iter);
    count += search_memo(entry_text, listStore, &iter);
#ifdef ENABLE_PLUGINS
    count += search_plugins(entry_text, listStore, &iter);
#endif

    /* sort the results */
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(listStore), SEARCH_TEXT_COLUMN_ENUM, GTK_SORT_ASCENDING);

   /* the datebook events are already sorted by date */
      count += search_datebook(entry_text, listStore, &iter);

    if (count == 0) {
        gtk_list_store_append(listStore, &iter);
        gtk_list_store_set(listStore, &iter,
        SEARCH_TEXT_COLUMN_ENUM,("No records found"),-1);

    }

    /* Highlight the first row in the list of returned items.
     * This does NOT cause the main window to jump to the selected record. */

    /* select the first record found */
    selectFirstRow(treeView);

    return;
}

void selectFirstRow(const GtkTreeView *treeView) {
    GtkTreeSelection * selection = NULL;
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeView));
    GtkTreePath  *path = NULL;
    GtkTreeIter firstIter;
    gtk_tree_model_get_iter_first(gtk_tree_view_get_model(treeView),&firstIter);
    path = gtk_tree_model_get_path(gtk_tree_view_get_model(treeView),&firstIter);
    gtk_tree_selection_select_path(selection, path);
    gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(treeView), path, SEARCH_TEXT_COLUMN_ENUM, FALSE, 1.0, 0.0);
    gtk_tree_path_free(path);
}

static void cb_search(GtkWidget *widget, gpointer data) {
    cb_entry(entry, data);
}

static gboolean handleSearchRowSelection(GtkTreeSelection *selection,
                                   GtkTreeModel *model,
                                   GtkTreePath *path,
                                   gboolean path_currently_selected,
                                   gpointer userdata) {
    GtkTreeIter iter;
    struct search_record *sr;
    int row;
    if ((gtk_tree_model_get_iter(model, &iter, path)) && (!path_currently_selected)) {
        int * i = gtk_tree_path_get_indices ( path ) ;
        row_selected = i[0];
        row = i[0];
        gtk_tree_model_get(model, &iter, SEARCH_DATA_ENUM, &sr, -1);
        if(sr == NULL){
            return TRUE;
        }switch (sr->app_type) {
            case DATEBOOK:
                glob_find_id = sr->unique_id;
                cb_app_button(NULL, GINT_TO_POINTER(DATEBOOK));
                break;
            case ADDRESS:
                glob_find_id = sr->unique_id;
                cb_app_button(NULL, GINT_TO_POINTER(ADDRESS));
                break;
            case TODO:
                glob_find_id = sr->unique_id;
                cb_app_button(NULL, GINT_TO_POINTER(TODO));
                break;
            case MEMO:
                glob_find_id = sr->unique_id;
                cb_app_button(NULL, GINT_TO_POINTER(MEMO));
                break;
            default:
#ifdef ENABLE_PLUGINS
                /* Not one of the main 4 apps so it must be a plugin */
                jp_logf(JP_LOG_DEBUG, "choosing search result from plugin %d\n", sr->app_type);
                call_plugin_gui(sr->app_type, sr->unique_id);
#endif
                break;
        }

    }
    return TRUE;
}

static gboolean cb_key_pressed_in_list(GtkWidget *widget,
                                       GdkEventKey *event,
                                       gpointer data) {
    if (event->keyval == GDK_KEY_Return) {
         g_signal_stop_emission_by_name(G_OBJECT(widget), "key_press_event");
        return TRUE;
    }

    return FALSE;
}

void cb_search_gui(GtkWidget *widget, gpointer data) {
    GtkWidget *scrolled_window;
    GtkTreeView *treeView;
    GtkListStore *listStore;
    GtkWidget *label;
    GtkWidget *button;
    GtkWidget *vbox, *hbox;
    char temp[256];

    if (GTK_IS_WIDGET(window)) {
        /* Shift focus to existing window if called again
           and window is still alive. */
        gtk_window_present(GTK_WINDOW(window));
        gtk_widget_grab_focus(GTK_WIDGET(entry));
        return;
    }

    if (search_rl) {
        free_search_record_list(&search_rl);
        search_rl = NULL;
    }

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(window), 500, 300);
    g_snprintf(temp, sizeof(temp), "%s %s", PN, _("Search"));
    gtk_window_set_title(GTK_WINDOW(window), temp);

    g_signal_connect(G_OBJECT(window), "destroy",
                       G_CALLBACK(cb_destroy), window);

    accel_group = gtk_accel_group_new();
    gtk_window_add_accel_group(GTK_WINDOW(window), accel_group);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 6);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    /* Search label */
    label = gtk_label_new(_("Search for: "));
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    /* Search entry */
    entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);
    gtk_widget_grab_focus(GTK_WIDGET(entry));

    /* Case Sensitive checkbox */
    case_sense_checkbox = gtk_check_button_new_with_label(_("Case Sensitive"));
    gtk_box_pack_start(GTK_BOX(hbox), case_sense_checkbox, FALSE, FALSE, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(case_sense_checkbox),
                                 FALSE);

    /* Scrolled window for search results */
    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_set_border_width(GTK_CONTAINER(scrolled_window), 3);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 0);
    listStore = gtk_list_store_new(SEARCH_NUM_COLS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);
    treeView = gtk_tree_view_new_with_model(GTK_TREE_MODEL(listStore));
    GtkCellRenderer *appNameRenderer = gtk_cell_renderer_text_new();

    GtkTreeViewColumn *appNameColumn = gtk_tree_view_column_new_with_attributes("",
                                                                                appNameRenderer,
                                                                                "text", SEARCH_APP_NAME_COLUMN_ENUM,
                                                                                NULL);
    gtk_tree_view_column_set_sort_column_id(appNameColumn, SEARCH_APP_NAME_COLUMN_ENUM);
    GtkCellRenderer *textRenderer = gtk_cell_renderer_text_new();

    GtkTreeViewColumn *textColumn = gtk_tree_view_column_new_with_attributes("",
                                                                             textRenderer,
                                                                             "text", SEARCH_TEXT_COLUMN_ENUM,
                                                                             NULL);
    gtk_tree_view_column_set_sort_column_id(textColumn, SEARCH_TEXT_COLUMN_ENUM);
    gtk_tree_view_insert_column(GTK_TREE_VIEW (treeView), appNameColumn, SEARCH_APP_NAME_COLUMN_ENUM);
    gtk_tree_view_insert_column(GTK_TREE_VIEW (treeView), textColumn, SEARCH_TEXT_COLUMN_ENUM);
    gtk_tree_view_column_set_clickable(appNameColumn, gtk_false());
    gtk_tree_view_column_set_clickable(textColumn, gtk_false());
    gtk_tree_view_column_set_sizing(appNameColumn, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    gtk_tree_view_column_set_sizing(textColumn, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    gtk_tree_view_set_headers_visible(treeView, gtk_false());
    gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(treeView)),
                                GTK_SELECTION_BROWSE);
    gtk_tree_selection_set_select_function(gtk_tree_view_get_selection(GTK_TREE_VIEW(treeView)), handleSearchRowSelection, NULL, NULL);


    g_signal_connect(G_OBJECT(treeView), "key_press_event",
                       G_CALLBACK(cb_key_pressed_in_list),
                       NULL);

    gtk_container_add(GTK_CONTAINER(scrolled_window), GTK_WIDGET(treeView));

    g_signal_connect(G_OBJECT(entry), "activate",
                       G_CALLBACK(cb_entry),
                       treeView);

    hbox = gtk_hbutton_box_new();
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 6);
    gtk_button_box_set_layout(GTK_BUTTON_BOX (hbox), GTK_BUTTONBOX_END);
     gtk_box_set_spacing(GTK_BOX(hbox), 6);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    /* Search button */
    button = gtk_button_new_with_label("Search");
    g_signal_connect(G_OBJECT(button), "clicked",
                       G_CALLBACK(cb_search), treeView);
    gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);

    /* clicking on "Case Sensitive" also starts a search */
    g_signal_connect(G_OBJECT(case_sense_checkbox), "clicked",
                       G_CALLBACK(cb_search), treeView);

    /* Done button */
    button = gtk_button_new_with_label("Done");
    g_signal_connect(G_OBJECT(button), "clicked",
                       G_CALLBACK(cb_quit), window);
    gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
    gtk_widget_add_accelerator(button, "clicked", accel_group, GDK_KEY_Escape, 0, 0);

    gtk_widget_show_all(window);
}


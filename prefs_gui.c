/*******************************************************************************
 * prefs_gui.c
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
#include <gtk/gtk.h>

#include "prefs_gui.h"
#include "prefs.h"
#include "i18n.h"
#include "utils.h"
#include "log.h"
#include "plugins.h"
#include "libsqlite.h"

/******************************* Global vars **********************************/
static GtkWidget *window;
static GtkWidget *main_window;
static GtkWidget *port_entry;
static GtkWidget *backups_entry;
static GtkWidget *ext_editor_entry;
static GtkWidget *alarm_command_entry;
static GtkWidget *mail_command_entry;
static GtkWidget *todo_days_due_entry;

extern int glob_app;
#ifdef ENABLE_PLUGINS
extern unsigned char skip_plugins;
#endif

/* Sync Port Menu */
static GtkWidget *port_menu;
static const char *port_choices[] = {
        "other", "usb:",
        "/dev/ttyUSB0", "/dev/ttyUSB1", "/dev/ttyUSB2", "/dev/ttyUSB3",
        "/dev/ttyS0", "/dev/ttyS1", "/dev/ttyS2", "/dev/ttyS3",
        NULL
};

/* Serial Rate Menu */
static GtkWidget *rate_menu;

/****************************** Main Code *************************************/
#ifdef COLORS

/* This doesn't work quite right.  There is supposedly no way to do it in GTK. */
static void r(GtkWidget *w, gpointer data)
{
  /* GtkStyle *style;

   style = gtk_rc_get_style(GTK_WIDGET(w));
   if (style) gtk_rc_style_unref(style);
   if (GTK_IS_CONTAINER(w)) {
      gtk_container_foreach(GTK_CONTAINER(w), r, w);
   } */
}

static void set_colors()
{
 /*  GtkStyle* style;
   int i;

   r(main_window, NULL);
   r(window, NULL);

   gtk_rc_reparse_all();
   read_gtkrc_file();
   gtk_rc_reparse_all();
   gtk_widget_reset_rc_styles(window);
   gtk_widget_reset_rc_styles(main_window);
   gtk_rc_reparse_all();
   gtk_widget_queue_draw(window);
   gtk_widget_queue_draw(main_window); */
}
#endif /* #ifdef COLORS */

/* Sync Port menu code */
static void cb_serial_port_menu(GtkComboBox *widget,
                                gpointer data) {
    if (!widget)
        return;
    int choice = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
    //don't do things if 'other' or nothing is selected..
    if (choice < 1) {
        return;
    }

    const char *port_str = port_choices[choice];
    gtk_entry_set_text(GTK_ENTRY(port_entry), port_str);
    if (!strcmp(port_str, "usb:")) {
        gtk_widget_set_sensitive(rate_menu, FALSE);
    } else {
        gtk_widget_set_sensitive(rate_menu, TRUE);
    }


    return;
}

static int make_serial_port_menu(GtkWidget **port_menu) {
    int i, selected;
    const char *entry_text;

    *port_menu = gtk_combo_box_text_new();

    // menu = gtk_menu_new();
    // group = NULL;
    selected = 0;

    entry_text = gtk_entry_get_text(GTK_ENTRY(port_entry));

    for (i = 0; port_choices[i]; i++) {
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT (*port_menu), port_choices[i]);
        if (!strcmp(entry_text, port_choices[i])) {
            gtk_combo_box_set_active(GTK_COMBO_BOX (*port_menu), i);
            selected = i;
        }

        /* We don't want a callback if "other" is selected */
        // gtk_widget_show(port_menu_item[i]);
    }
    g_signal_connect(G_OBJECT(*port_menu), "changed",
                     G_CALLBACK(cb_serial_port_menu),
                     GINT_TO_POINTER(selected));
   gtk_combo_box_set_active(GTK_COMBO_BOX(*port_menu), selected);

    return EXIT_SUCCESS;
}

/* End Sync Port Menu code */

static void cb_pref_menu(GtkComboBox *widget, gpointer data) {
    int pref;

    if (!widget) {
        return;
    }
    if (gtk_combo_box_get_active(GTK_COMBO_BOX(widget)) < 0) {
        return;
    }

    pref = get_selected_category_from_combo_box(widget);
    set_pref_possibility(pref, gtk_combo_box_get_active(GTK_COMBO_BOX(widget)), TRUE);
    
    jp_logf(JP_LOG_DEBUG, "pref %d, value %d\n", pref, GPOINTER_TO_INT(data) & 0xFF);
#ifdef COLORS
    if (pref==PREF_RCFILE) {
       set_colors();
    }
#endif
    return;
}

int make_pref_menu(GtkWidget **pref_menu, int pref_num) {
    int i, r;
    long ivalue;
    const char *svalue;
    char format_text[MAX_PREF_LEN];
    char human_text[MAX_PREF_LEN];
    time_t ltime;
    struct tm *now;
    GtkListStore *catListStore = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT);
    GtkTreeIter iter;

    time(&ltime);
    now = localtime(&ltime);


    //menu = gtk_menu_new();
    ///group = NULL;

    get_pref(pref_num, &ivalue, &svalue);


    for (i = 0; i < MAX_NUM_PREFS; i++) {
        r = get_pref_possibility(pref_num, i, format_text);
        if (r) {
            break;
        }
        switch (pref_num) {
            case PREF_SHORTDATE:
            case PREF_TIME:
                jp_strftime(human_text, MAX_PREF_LEN, format_text, now);
                break;
            case PREF_LONGDATE:
                jp_strftime(human_text, MAX_PREF_LEN, _(format_text), now);
                break;
            default:
                strncpy(human_text, format_text, MAX_PREF_LEN);
                break;
        }
        gtk_list_store_append (catListStore, &iter);
        gtk_list_store_set (catListStore, &iter, 0, human_text, 1, pref_num, -1);


    }
    *pref_menu = gtk_combo_box_new_with_model(GTK_TREE_MODEL (catListStore));
    gtk_combo_box_set_active(GTK_COMBO_BOX(*pref_menu), ivalue);
    g_signal_connect(G_OBJECT(*pref_menu), "changed", G_CALLBACK(cb_pref_menu),
                     GINT_TO_POINTER(pref_num));
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (*pref_menu), renderer, TRUE);
    gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (*pref_menu), renderer,
                                    "text", 0,
                                    NULL);
    //gtk_option_menu_set_menu(GTK_(*pref_menu), menu);

    return EXIT_SUCCESS;
}

static void cb_backups_entry(GtkWidget *widget, gpointer data) {
    const char *entry_text;
    int num_backups;

    entry_text = gtk_entry_get_text(GTK_ENTRY(backups_entry));
    sscanf(entry_text, "%d", &num_backups);

    if (num_backups < 1) {
        num_backups = 1;
    }
    if (num_backups > 99) {
        num_backups = 99;
    }

    set_pref(PREF_NUM_BACKUPS, num_backups, NULL, FALSE);
}

static void cb_checkbox_todo_days_till_due(GtkWidget *widget, gpointer data) {
    int num_days;
    const char *entry_text;

    entry_text = gtk_entry_get_text(GTK_ENTRY(todo_days_due_entry));

    sscanf(entry_text, "%d", &num_days);

    set_pref(PREF_TODO_DAYS_TILL_DUE, num_days, NULL, TRUE);
}

static void cb_checkbox_show_tooltips(GtkWidget *widget, gpointer data) {
    set_pref(PREF_SHOW_TOOLTIPS, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)), NULL, TRUE);
}

static void cb_text_entry(GtkWidget *widget, gpointer data) {
    const char *entry_text;
    int i, found;

    entry_text = gtk_entry_get_text(GTK_ENTRY(widget));
    set_pref(GPOINTER_TO_INT(data), 0, entry_text, FALSE);

    if (GPOINTER_TO_INT(data) == PREF_PORT) {
        if (GTK_IS_WIDGET(port_menu)) {
            found = 0;
            for (i = 0; port_choices[i]; i++) {
                if (!strcmp(entry_text, port_choices[i])) {
                    gtk_combo_box_set_active(GTK_COMBO_BOX(port_menu), i);
                    found = 1;
                }
            }
            if (!found) {
                gtk_combo_box_set_active(GTK_COMBO_BOX(port_menu), 0);
            }
        }
    }
}

static void cb_checkbox_set_pref(GtkWidget *widget, gpointer data) {
    unsigned long pref, value;

    pref = GPOINTER_TO_INT(data);
    value = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
    set_pref(pref, value, NULL, TRUE);
}

/*
 * upper 16 bits of data is pref to set
 * lower 16 bits of data is value to set it to
 */
static void cb_radio_set_pref(GtkWidget *widget, gpointer data) {
    unsigned long pref, value;

    pref = GPOINTER_TO_INT(data);
    value = pref & 0xFFFF;
    pref >>= 16;
    set_pref(pref, value, NULL, TRUE);
}


#ifdef ENABLE_PLUGINS

static void cb_sync_plugin(GtkWidget *widget, gpointer data) {
    GList *plugin_list, *temp_list;
    struct plugin_s *Pplugin;
    int number;

    number = GPOINTER_TO_INT(data);

    plugin_list = NULL;

    plugin_list = get_plugin_list();

    for (temp_list = plugin_list; temp_list; temp_list = temp_list->next) {
        Pplugin = (struct plugin_s *) temp_list->data;
        if (Pplugin) {
            if (number == Pplugin->number) {
                if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
                    Pplugin->sync_on = 1;
                } else {
                    Pplugin->sync_on = 0;
                }
            }
        }
    }
    write_plugin_sync_file();
}

#endif

static gboolean cb_destroy(GtkWidget *widget) {
    jp_logf(JP_LOG_DEBUG, "Pref GUI Cleanup\n");

    pref_write_rc_file();

    window = NULL;

    /* Preference changes can affect visual elements of applications.
     * Redraw the screen to incorporate any changes made. */
    cb_app_button(NULL, GINT_TO_POINTER(REDRAW));

    return FALSE;
}

static void cb_quit(GtkWidget *widget, gpointer data) {
    jp_logf(JP_LOG_DEBUG, "cb_quit\n");
    if (GTK_IS_WIDGET(data)) {
        gtk_widget_destroy(data);
    }
}

/* This function adds a simple option checkbutton for the supplied text +
 * option.  */
static void add_checkbutton(const char *text,
                            int which,
                            GtkWidget *vbox,
                            void cb(GtkWidget *widget, gpointer data)) {
    /* Create button */
    GtkWidget *checkbutton = gtk_check_button_new_with_label(text);
    gtk_box_pack_start(GTK_BOX(vbox), checkbutton, FALSE, FALSE, 0);
    gtk_widget_show(checkbutton);

    /* Set the button state based on option value */
    if (get_pref_int_default(which, 0))
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbutton), TRUE);

    /* Set button callback */
    g_signal_connect(G_OBJECT(checkbutton), "clicked", G_CALLBACK(cb),
                     GINT_TO_POINTER(which));
}

void cb_prefs_gui(GtkWidget *widget, gpointer data) {
    GtkWidget *checkbutton;
  //  GtkWidget *pref_menu;
    GtkWidget * pref_char_set;
    GtkWidget * pref_short_date;
    GtkWidget * pref_long_date;
    GtkWidget * pref_time;
    GtkWidget * pref_rc_file;
    GtkWidget *label;
    GtkWidget *button;
    GtkWidget *grid;
    GtkWidget *vbox;
    GtkWidget *vbox_locale;
    GtkWidget *vbox_settings;
    GtkWidget *vbox_datebook;
    GtkWidget *vbox_address;
    GtkWidget *vbox_todo;
    GtkWidget *vbox_memo;
    GtkWidget *vbox_alarms;
    GtkWidget *vbox_conduits;
    GtkWidget *hbox_temp;
    GtkWidget *hseparator;
    GtkWidget *notebook;
    /* FIXME: Uncomment when support for Task has been added */
#if 0
    GtkWidget *radio_button_todo_version[2];
#endif
    GtkWidget *radio_button_datebook_version[2];
    GtkWidget *radio_button_address_version[2];
    GtkWidget *radio_button_memo_version[3];
    long ivalue;
    const char *cstr;
    char temp_str[10];
    char temp[256];
    GSList *group;
#ifdef ENABLE_PLUGINS
    GList *plugin_list, *temp_list;
    struct plugin_s *Pplugin;
#endif

    jp_logf(JP_LOG_DEBUG, "cb_prefs_gui\n");
    if (window) {
      jp_logf(JP_LOG_DEBUG, "pref_window is already up\n");
        /* Shift focus to existing window if called again
           and window is still alive. */
        gtk_window_present(GTK_WINDOW(window));
        return;
    }

    main_window = data;

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_container_set_border_width(GTK_CONTAINER(window), 10);
    g_snprintf(temp, sizeof(temp), "%s %s", PN, _("Preferences"));
    gtk_window_set_title(GTK_WINDOW(window), temp);

    g_signal_connect(G_OBJECT(window), "destroy",
                     G_CALLBACK(cb_destroy), window);

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    /* Boxes for each preference tax */
    vbox_locale = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    vbox_settings = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    vbox_datebook = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    vbox_address = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    vbox_todo = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    vbox_memo = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    vbox_alarms = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    vbox_conduits = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    gtk_container_set_border_width(GTK_CONTAINER(vbox_locale), 5);
    gtk_container_set_border_width(GTK_CONTAINER(vbox_settings), 5);
    gtk_container_set_border_width(GTK_CONTAINER(vbox_datebook), 5);
    gtk_container_set_border_width(GTK_CONTAINER(vbox_address), 5);
    gtk_container_set_border_width(GTK_CONTAINER(vbox_todo), 5);
    gtk_container_set_border_width(GTK_CONTAINER(vbox_memo), 5);
    gtk_container_set_border_width(GTK_CONTAINER(vbox_alarms), 5);
    gtk_container_set_border_width(GTK_CONTAINER(vbox_conduits), 5);

    /* Notebook for preference tabs */
    notebook = gtk_notebook_new();
    gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_TOP);
    label = gtk_label_new(_("Locale"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox_locale, label);
    label = gtk_label_new(_("Settings"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox_settings, label);
    label = gtk_label_new(_("Datebook"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox_datebook, label);
    label = gtk_label_new(C_("prefgui", "Address"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox_address, label);
    label = gtk_label_new(_("ToDo"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox_todo, label);
    label = gtk_label_new(_("Memo"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox_memo, label);
    label = gtk_label_new(_("Alarms"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox_alarms, label);
    label = gtk_label_new(_("Conduits"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox_conduits, label);
    gtk_box_pack_start(GTK_BOX(vbox), notebook, FALSE, FALSE, 0);

    /************************************************************/
    /* Locale preference tab */
    grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 2);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 5);
    gtk_box_pack_start(GTK_BOX(vbox_locale), grid, FALSE, FALSE, 0);

    /* Locale preference tab */

    /* Character Set */
    label = gtk_label_new(_("Character Set"));
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(label), 0, 0, 1, 1);

    make_pref_menu(&pref_char_set, PREF_CHAR_SET);
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(pref_char_set), 1, 0, 1, 1);

    get_pref(PREF_CHAR_SET, &ivalue, NULL);
    gtk_combo_box_set_active(GTK_COMBO_BOX(pref_char_set), ivalue);
    /* Shortdate */
    label = gtk_label_new(_("Short date format"));
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(label), 0, 1, 1, 1);

    make_pref_menu(&pref_short_date, PREF_SHORTDATE);
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(pref_short_date), 1, 1, 1, 1);

    get_pref(PREF_SHORTDATE, &ivalue, NULL);
    gtk_combo_box_set_active(GTK_COMBO_BOX(pref_short_date), ivalue);

    /* Longdate */
    label = gtk_label_new(_("Long date format"));
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(label), 0, 2, 1, 1);

    make_pref_menu(&pref_long_date, PREF_LONGDATE);
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(pref_long_date), 1, 2, 1, 1);

    get_pref(PREF_LONGDATE, &ivalue, NULL);
    gtk_combo_box_set_active(GTK_COMBO_BOX(pref_long_date), ivalue);

    /* Time */
    label = gtk_label_new(_("Time format"));
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(label), 0, 3, 1, 1);

    make_pref_menu(&pref_time, PREF_TIME);
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(pref_time), 1, 3, 1, 1);

    get_pref(PREF_TIME, &ivalue, NULL);
    gtk_combo_box_set_active(GTK_COMBO_BOX(pref_time), ivalue);

    /**********************************************************************/
    /* Settings preference tab */
    grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 2);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 5);
    gtk_box_pack_start(GTK_BOX(vbox_settings), grid, FALSE, FALSE, 0);

    /* GTK colors file */
    label = gtk_label_new(_("GTK color theme file"));
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(label), 1, 0, 1, 1);

    make_pref_menu(&pref_rc_file, PREF_RCFILE);
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(pref_rc_file), 2, 0, 1, 1);


    get_pref(PREF_RCFILE, &ivalue, NULL);
    gtk_combo_box_set_active(GTK_COMBO_BOX(pref_rc_file), ivalue);

    /* Port */
    label = gtk_label_new(_("Sync Port"));
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(label), 0, 1, 1, 1);

    port_entry = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(port_entry), MAX_PREF_LEN - 2);
    entry_set_multiline_truncate(GTK_ENTRY(port_entry), TRUE);
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(port_entry), 2, 1, 1, 1);
    get_pref(PREF_PORT, NULL, &cstr);
    if (cstr) {
        gtk_entry_set_text(GTK_ENTRY(port_entry), cstr);
    }
    g_signal_connect(G_OBJECT(port_entry),
                     "changed", G_CALLBACK(cb_text_entry),
                     GINT_TO_POINTER(PREF_PORT));

    /* Sync Port Menu */
    /* Note that port_entry must exist before we call this function */
    make_serial_port_menu(&port_menu);
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(port_menu), 1, 1, 1, 1);

    /* Serial Rate */
    label = gtk_label_new(_("Serial Rate"));
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(label), 1, 2, 1, 1);

    make_pref_menu(&rate_menu, PREF_RATE);
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(rate_menu), 2, 2, 1, 1);

    get_pref(PREF_RATE, &ivalue, NULL);
    gtk_combo_box_set_active(GTK_COMBO_BOX(rate_menu), ivalue);

    /* Disable Serial Rate menu if sync port is USB */
    if (!strcmp(cstr, "usb:")) {
        gtk_widget_set_sensitive(rate_menu, FALSE);
    } else {
        gtk_widget_set_sensitive(rate_menu, TRUE);
    }

    /* Number of backups */
    label = gtk_label_new(_("Number of backups to be archived"));
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(label), 0, 3, 2, 1);

    backups_entry = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(backups_entry), 2);
    entry_set_multiline_truncate(GTK_ENTRY(backups_entry), TRUE);
    gtk_widget_set_size_request(backups_entry, 30, 0);
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(backups_entry), 2, 3, 1, 1);
    get_pref(PREF_NUM_BACKUPS, &ivalue, NULL);
    sprintf(temp_str, "%ld", ivalue);
    gtk_entry_set_text(GTK_ENTRY(backups_entry), temp_str);
    g_signal_connect(G_OBJECT(backups_entry),
                     "changed", G_CALLBACK(cb_backups_entry),
                     NULL);

	if (!glob_sqlite) {
		/* Show deleted files check box */
		add_checkbutton(_("Show deleted records (default NO)"),
						PREF_SHOW_DELETED, vbox_settings, cb_checkbox_set_pref);

		/* Show modified files check box */
		add_checkbutton(_("Show modified deleted records (default NO)"),
						PREF_SHOW_MODIFIED, vbox_settings, cb_checkbox_set_pref);
	}

    /* Confirm file installation */
    add_checkbutton(
            _("Ask confirmation for file installation (J-Pilot -> PDA) (default YES)"),
            PREF_CONFIRM_FILE_INSTALL, vbox_settings, cb_checkbox_set_pref);

    /* Show tooltips check box */
    add_checkbutton(_("Show popup tooltips (default YES) (requires restart)"),
                    PREF_SHOW_TOOLTIPS, vbox_settings,
                    cb_checkbox_show_tooltips);

    /**********************************************************************/
    /* Datebook preference tab */

    /* Radio box to choose which database to use: Datebook/Calendar */
    group = NULL;
    radio_button_datebook_version[0] =
            gtk_radio_button_new_with_label(group, _("Use Datebook database (Palm OS < 5.2.1)"));
    group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio_button_datebook_version[0]));
    radio_button_datebook_version[1] =
            gtk_radio_button_new_with_label(group, _("Use Calendar database (Palm OS > 5.2)"));
    group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio_button_datebook_version[1]));
    gtk_box_pack_start(GTK_BOX(vbox_datebook), radio_button_datebook_version[0],
                       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox_datebook), radio_button_datebook_version[1],
                       FALSE, FALSE, 0);

    g_signal_connect(G_OBJECT(radio_button_datebook_version[0]), "pressed",
                     G_CALLBACK(cb_radio_set_pref),
                     GINT_TO_POINTER((PREF_DATEBOOK_VERSION << 16) | 0));
    g_signal_connect(G_OBJECT(radio_button_datebook_version[1]), "pressed",
                     G_CALLBACK(cb_radio_set_pref),
                     GINT_TO_POINTER((PREF_DATEBOOK_VERSION << 16) | 1));

    get_pref(PREF_DATEBOOK_VERSION, &ivalue, NULL);
    if (ivalue) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_button_datebook_version[1]), TRUE);
    } else {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_button_datebook_version[0]), TRUE);
    }

    /* Separate database selection from less important options */
    hseparator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(vbox_datebook), hseparator, FALSE, FALSE, 3);

    /* Show highlight days check box */
    add_checkbutton(_("Highlight calendar days with appointments"),
                    PREF_DATEBOOK_HIGHLIGHT_DAYS, vbox_datebook,
                    cb_checkbox_set_pref);

    /* Highlight today on month and week view */
    add_checkbutton(_("Annotate today in day, week, and month views"),
                    PREF_DATEBOOK_HI_TODAY, vbox_datebook, cb_checkbox_set_pref);

    /* Show number of years on anniversaries in month and week view */
    add_checkbutton(_("Append years on anniversaries in day, week, and month views"),
                    PREF_DATEBOOK_ANNI_YEARS, vbox_datebook,
                    cb_checkbox_set_pref);

#ifdef ENABLE_DATEBK
    /* Show use DateBk check box */
    add_checkbutton(_("Use DateBk note tags"),
                    PREF_USE_DB3, vbox_datebook, cb_checkbox_set_pref);
#else
    checkbutton = gtk_check_button_new_with_label(_("DateBk support disabled in this build"));
    gtk_widget_set_sensitive(checkbutton, FALSE);
    gtk_box_pack_start(GTK_BOX(vbox_datebook), checkbutton, FALSE, FALSE, 0);
    gtk_widget_show(checkbutton);
#endif

    /**********************************************************************/
    /* Address preference tab */

    /* Radio box to choose which database to use: Address/Contacts */
    group = NULL;
    radio_button_address_version[0] =
            gtk_radio_button_new_with_label(group, _("Use Address database (Palm OS < 5.2.1)"));
    group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio_button_address_version[0]));
    radio_button_address_version[1] =
            gtk_radio_button_new_with_label(group, _("Use Contacts database (Palm OS > 5.2)"));
    group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio_button_address_version[1]));
    gtk_box_pack_start(GTK_BOX(vbox_address), radio_button_address_version[0],
                       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox_address), radio_button_address_version[1],
                       FALSE, FALSE, 0);

    g_signal_connect(G_OBJECT(radio_button_address_version[0]), "pressed",
                     G_CALLBACK(cb_radio_set_pref),
                     GINT_TO_POINTER((PREF_ADDRESS_VERSION << 16) | 0));
    g_signal_connect(G_OBJECT(radio_button_address_version[1]), "pressed",
                     G_CALLBACK(cb_radio_set_pref),
                     GINT_TO_POINTER((PREF_ADDRESS_VERSION << 16) | 1));

    get_pref(PREF_ADDRESS_VERSION, &ivalue, NULL);
    if (ivalue) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_button_address_version[1]), TRUE);
    } else {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_button_address_version[0]), TRUE);
    }

    /* Separate database selection from less important options */
    hseparator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(vbox_address), hseparator, FALSE, FALSE, 3);

    /* Command to use for e-mailing from address book */
    hbox_temp = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(vbox_address), hbox_temp, FALSE, FALSE, 0);

    label = gtk_label_new(_("Mail Command"));
    gtk_box_pack_start(GTK_BOX(hbox_temp), label, FALSE, FALSE, 0);

    mail_command_entry = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(mail_command_entry), MAX_PREF_LEN - 2);

    get_pref(PREF_MAIL_COMMAND, NULL, &cstr);
    if (cstr) {
        gtk_entry_set_text(GTK_ENTRY(mail_command_entry), cstr);
    }
    g_signal_connect(G_OBJECT(mail_command_entry),
                     "changed", G_CALLBACK(cb_text_entry),
                     GINT_TO_POINTER(PREF_MAIL_COMMAND));
    gtk_box_pack_start(GTK_BOX(hbox_temp), mail_command_entry, TRUE, TRUE, 1);

    label = gtk_label_new(_("%s is replaced by the e-mail address"));
    gtk_box_pack_start(GTK_BOX(vbox_address), label, FALSE, FALSE, 0);
    gtk_widget_set_halign(GTK_WIDGET(label), GTK_ALIGN_START);

    /**********************************************************************/
    /* ToDo preference tab */

    /* FIXME: undef when support for Task has been coded */
#if 0
    /* Radio box to choose which database to use: Todo/Task */
    group = NULL;
    radio_button_task_version[0] =
      gtk_radio_button_new_with_label(group, _("Use ToDo database (Palm OS < 5.2.1)"));
    group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio_button_todo_version[0]));
    radio_button_todo_version[1] =
      gtk_radio_button_new_with_label(group, _("Use Task database (Palm OS > 5.2)"));
    group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio_button_todo_version[1]));
    gtk_box_pack_start(GTK_BOX(vbox_todo), radio_button_todo_version[0],
                       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox_todo), radio_button_todo_version[1],
                       FALSE, FALSE, 0);

    g_signal_connect(G_OBJECT(radio_button_todo_version[0]), "pressed",
                       G_CALLBACK(cb_radio_set_pref),
                       GINT_TO_POINTER((PREF_TODO_VERSION<<16)|0));
    g_signal_connect(G_OBJECT(radio_button_todo_version[1]), "pressed",
                       G_CALLBACK(cb_radio_set_pref),
                       GINT_TO_POINTER((PREF_TODO_VERSION<<16)|1));

    get_pref(PREF_TODO_VERSION, &ivalue, NULL);
    if (ivalue) {
       gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_button_todo_version[1]), TRUE);
    } else {
       gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_button_todo_version[0]), TRUE);
    }

    /* Separate database selection from less important options */
    hseparator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(vbox_todo), hseparator, FALSE, FALSE, 3);
#endif

    /* hide completed check box */
    add_checkbutton(_("Hide Completed ToDos"),
                    PREF_TODO_HIDE_COMPLETED, vbox_todo, cb_checkbox_set_pref);

    /* hide todos not yet due check box */
    add_checkbutton(_("Hide ToDos not yet due"),
                    PREF_TODO_HIDE_NOT_DUE, vbox_todo, cb_checkbox_set_pref);

    /* record todo completion date check box */
    add_checkbutton(_("Record Completion Date"),
                    PREF_TODO_COMPLETION_DATE, vbox_todo, cb_checkbox_set_pref);

#ifdef ENABLE_MANANA
    /* Use Manana check box */
    add_checkbutton(_("Use Manana database"),
                    PREF_MANANA_MODE, vbox_todo, cb_checkbox_set_pref);
#endif

    /* Default Number of Days Due for ToDos */
    hbox_temp = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(vbox_todo), hbox_temp, FALSE, FALSE, 0);

    add_checkbutton(_("Use default number of days due"),
                    PREF_TODO_DAYS_DUE, hbox_temp, cb_checkbox_set_pref);

    todo_days_due_entry = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(todo_days_due_entry), MAX_PREF_LEN - 2);
    entry_set_multiline_truncate(GTK_ENTRY(todo_days_due_entry), TRUE);
    get_pref(PREF_TODO_DAYS_TILL_DUE, &ivalue, NULL);
    temp[0] = '\0';
    g_snprintf(temp, sizeof(temp), "%ld", ivalue);
    gtk_entry_set_text(GTK_ENTRY(todo_days_due_entry), temp);
    gtk_box_pack_start(GTK_BOX(hbox_temp), todo_days_due_entry, FALSE, FALSE, 0);

    g_signal_connect(G_OBJECT(todo_days_due_entry),
                     "changed", G_CALLBACK(cb_checkbox_todo_days_till_due),
                     NULL);

    gtk_widget_show_all(hbox_temp);

    /**********************************************************************/
    /* Memo preference tab */
    /* Radio box to choose which database to use: Memo/Memos/Memo32 */
    group = NULL;
    radio_button_memo_version[0] =
            gtk_radio_button_new_with_label(group, _("Use Memo database (Palm OS < 5.2.1)"));
    group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio_button_memo_version[0]));
    radio_button_memo_version[1] =
            gtk_radio_button_new_with_label(group, _("Use Memos database (Palm OS > 5.2)"));
    group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio_button_memo_version[1]));
    radio_button_memo_version[2] =
            gtk_radio_button_new_with_label(group, _("Use Memo32 database (pedit32)"));
    group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio_button_memo_version[2]));
    gtk_box_pack_start(GTK_BOX(vbox_memo), radio_button_memo_version[0],
                       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox_memo), radio_button_memo_version[1],
                       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox_memo), radio_button_memo_version[2],
                       FALSE, FALSE, 0);

    g_signal_connect(G_OBJECT(radio_button_memo_version[0]), "pressed",
                     G_CALLBACK(cb_radio_set_pref),
                     GINT_TO_POINTER((PREF_MEMO_VERSION << 16) | 0));
    g_signal_connect(G_OBJECT(radio_button_memo_version[1]), "pressed",
                     G_CALLBACK(cb_radio_set_pref),
                     GINT_TO_POINTER((PREF_MEMO_VERSION << 16) | 1));
    g_signal_connect(G_OBJECT(radio_button_memo_version[2]), "pressed",
                     G_CALLBACK(cb_radio_set_pref),
                     GINT_TO_POINTER((PREF_MEMO_VERSION << 16) | 2));

    get_pref(PREF_MEMO_VERSION, &ivalue, NULL);
    switch (ivalue) {
        case 0:
        default:
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_button_memo_version[0]), TRUE);
            break;
        case 1:
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_button_memo_version[1]), TRUE);
            break;
        case 2:
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_button_memo_version[2]), TRUE);
            break;
    }

    hseparator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(vbox_memo), hseparator, FALSE, FALSE, 3);

    /* External Editor Command to execute */
    hbox_temp = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(vbox_memo), hbox_temp, FALSE, FALSE, 0);

    label = gtk_label_new(_("External Editor"));
    gtk_box_pack_start(GTK_BOX(hbox_temp), label, FALSE, FALSE, 0);

    ext_editor_entry = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(ext_editor_entry), MAX_PREF_LEN - 2);
    get_pref(PREF_EXTERNAL_EDITOR, NULL, &cstr);
    if (cstr) {
        gtk_entry_set_text(GTK_ENTRY(ext_editor_entry), cstr);
    }
    g_signal_connect(G_OBJECT(ext_editor_entry),
                     "changed", G_CALLBACK(cb_text_entry),
                     GINT_TO_POINTER(PREF_EXTERNAL_EDITOR));
    gtk_box_pack_start(GTK_BOX(hbox_temp), ext_editor_entry, TRUE, TRUE, 1);

    label = gtk_label_new(_("Use Ctrl-E inside a memo to launch external editor for memo text"));
    gtk_widget_set_halign(GTK_WIDGET(label), GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox_memo), label, FALSE, FALSE, 0);

    /**********************************************************************/
    /* Alarms preference tab */

    /* Open alarm windows check box */
    add_checkbutton(_("Open alarm windows for appointment reminders"),
                    PREF_OPEN_ALARM_WINDOWS, vbox_alarms, cb_checkbox_set_pref);

    /* Execute alarm command check box */
    add_checkbutton(_("Execute this command"),
                    PREF_DO_ALARM_COMMAND, vbox_alarms, cb_checkbox_set_pref);

    /* Shell warning label */
    label = gtk_label_new(_("WARNING: executing arbitrary shell commands can be dangerous!!!"));
    gtk_widget_set_halign(GTK_WIDGET(label), GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox_alarms), label, FALSE, FALSE, 0);

    /* Alarm Command to execute */
    hbox_temp = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(vbox_alarms), hbox_temp, FALSE, FALSE, 0);

    label = gtk_label_new(_("Alarm Command"));
    gtk_box_pack_start(GTK_BOX(hbox_temp), label, FALSE, FALSE, 10);

    alarm_command_entry = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(alarm_command_entry), MAX_PREF_LEN - 2);
    get_pref(PREF_ALARM_COMMAND, NULL, &cstr);
    if (cstr) {
        gtk_entry_set_text(GTK_ENTRY(alarm_command_entry), cstr);
    }
    g_signal_connect(G_OBJECT(alarm_command_entry),
                     "changed", G_CALLBACK(cb_text_entry),
                     GINT_TO_POINTER(PREF_ALARM_COMMAND));
    gtk_box_pack_start(GTK_BOX(hbox_temp), alarm_command_entry, FALSE, FALSE, 0);

    label = gtk_label_new(_("%t is replaced with the alarm time"));
    gtk_widget_set_halign(GTK_WIDGET(label), GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox_alarms), label, FALSE, FALSE, 0);

    label = gtk_label_new(_("%d is replaced with the alarm date"));
    gtk_widget_set_halign(GTK_WIDGET(label), GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox_alarms), label, FALSE, FALSE, 0);

#ifdef ENABLE_ALARM_SHELL_DANGER
    label = gtk_label_new(_("%D is replaced with the alarm description"));
    gtk_widget_set_halign(GTK_WIDGET(label), GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox_alarms), label, FALSE, FALSE, 0);

    label = gtk_label_new(_("%N is replaced with the alarm note"));
    gtk_widget_set_halign(GTK_WIDGET(label), GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox_alarms), label, FALSE, FALSE, 0);
#else
    label = gtk_label_new(_("%D (description substitution) is disabled in this build"));
    gtk_widget_set_sensitive(label, FALSE);
    gtk_widget_set_halign(GTK_WIDGET(label), GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox_alarms), label, FALSE, FALSE, 0);

    label = gtk_label_new(_("%N (note substitution) is disabled in this build"));
    gtk_widget_set_sensitive(label, FALSE);
    gtk_widget_set_halign(GTK_WIDGET(label), GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox_alarms), label, FALSE, FALSE, 0);
#endif

    /**********************************************************************/
    /* Conduits preference tab */

    /* Sync datebook check box */
    add_checkbutton(_("Sync datebook"),
                    PREF_SYNC_DATEBOOK, vbox_conduits, cb_checkbox_set_pref);

    /* Sync address check box */
    add_checkbutton(_("Sync address"),
                    PREF_SYNC_ADDRESS, vbox_conduits, cb_checkbox_set_pref);

    /* Sync todo check box */
    add_checkbutton(_("Sync todo"),
                    PREF_SYNC_TODO, vbox_conduits, cb_checkbox_set_pref);

    /* Sync memo check box */
    add_checkbutton(_("Sync memo"),
                    PREF_SYNC_MEMO, vbox_conduits, cb_checkbox_set_pref);

#ifdef ENABLE_MANANA
    /* Show sync Manana check box */
    add_checkbutton(_("Sync Manana"),
                    PREF_SYNC_MANANA, vbox_conduits, cb_checkbox_set_pref);
#endif
    get_pref(PREF_CHAR_SET, &ivalue, NULL);
    if (ivalue == CHAR_SET_JAPANESE || ivalue == CHAR_SET_SJIS_UTF) {
        /* Show use Japanese Kana extention check box */
        add_checkbutton(_("Use J-OS (Not Japanese PalmOS:WorkPad/CLIE)"),
                        PREF_USE_JOS, vbox_settings, cb_checkbox_set_pref);
    }

#ifdef  ENABLE_PLUGINS
    if (!skip_plugins) {
        plugin_list = NULL;
        plugin_list = get_plugin_list();

        for (temp_list = plugin_list; temp_list; temp_list = temp_list->next) {
            Pplugin = (struct plugin_s *) temp_list->data;
            if (Pplugin) {
                /* Make a Sync checkbox for each plugin */
                g_snprintf(temp, sizeof(temp), _("Sync %s (%s)"), Pplugin->name, Pplugin->full_path);
                checkbutton = gtk_check_button_new_with_label(temp);
                gtk_box_pack_start(GTK_BOX(vbox_conduits), checkbutton, FALSE, FALSE, 0);
                gtk_widget_show(checkbutton);
                if (Pplugin->sync_on) {
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbutton), TRUE);
                }
                g_signal_connect(G_OBJECT(checkbutton), "clicked",
                                 G_CALLBACK(cb_sync_plugin),
                                 GINT_TO_POINTER(Pplugin->number));
            }
        }
    }

#endif

    /* Done button */
    hbox_temp = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_button_box_set_layout(GTK_BUTTON_BOX (hbox_temp), GTK_BUTTONBOX_END);
    gtk_box_pack_start(GTK_BOX(vbox), hbox_temp, FALSE, FALSE, 1);

    button = gtk_button_new_with_label("OK");
    g_signal_connect(G_OBJECT(button), "clicked",
                     G_CALLBACK(cb_quit), window);
    gtk_box_pack_end(GTK_BOX(hbox_temp), button, FALSE, FALSE, 0);

    gtk_widget_show_all(window);
}


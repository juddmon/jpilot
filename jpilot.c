/*******************************************************************************
 * jpilot.c
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
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef HAVE_LOCALE_H

#  include <locale.h>

#endif
#ifdef HAVE_LANGINFO_H

#  include <langinfo.h>

#endif

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "utils.h"
#include "i18n.h"
#include "otherconv.h"
#include "libplugin.h"
#include "datebook.h"
#include "address.h"
#include "install_user.h"
#include "todo.h"
#include "memo.h"
#include "sync.h"
#include "log.h"
#include "prefs_gui.h"
#include "prefs.h"
#include "plugins.h"
#include "alarms.h"
#include "print.h"
#include "restore.h"
#include "password.h"
#include "pidfile.h"
#include "jpilot.h"
#include "libsqlite.h"

#include "icons/jpilot-icon4.xpm"
#include "icons/datebook.xpm"
#include "icons/address.xpm"
#include "icons/todo.xpm"
#include "icons/memo.xpm"
#include "icons/appl_menu_icons.h"
#include "icons/lock_icons.h"
#include "icons/sync.xpm"
#include "icons/cancel_sync.xpm"
#include "icons/backup.xpm"

/********************************* Constants **********************************/
#define OUTPUT_MINIMIZE 383
#define OUTPUT_RESIZE   384
#define OUTPUT_SETSIZE  385
#define OUTPUT_CLEAR    386

#define MASK_WIDTH  0x08
#define MASK_HEIGHT 0x04
#define MASK_X      0x02
#define MASK_Y      0x01
#define PIPE_DEBUG 1
/* #define PIPE_DEBUG */
/******************************* Global vars **********************************/
/* Application-wide globals */
int pipe_from_child, pipe_to_parent;
int pipe_from_parent, pipe_to_child;
/* Main GTK window for application */
GtkWidget *window;
GtkWidget *glob_date_label;
GtkWidget *glob_dialog = NULL;
int glob_app = 0;
int glob_sqlite = 0;	// global variable whether to use SQLite3 for storage, default=no
int glob_rc_file_write = 1;	// global variable whether to write to rc-file, default=yes
unsigned char skip_plugins;
gint glob_date_timer_tag;
pid_t glob_child_pid;
pid_t jpilot_master_pid;
int plugin_number = DATEBOOK + 100;
/* jpilot.c file globals */
static GtkWidget *g_hbox, *g_vbox0;
static GtkWidget *g_hbox2, *g_vbox0_1;
static GtkWidget *g_output_text_scrolled_window;
static GtkTextView *g_output_text;
static GtkTextBuffer *g_output_text_buffer;
static GtkWidget *output_pane;
static GtkWidget *button_locked;
static GtkWidget *button_masklocked;
static GtkWidget *button_unlocked;
static GtkWidget *button_sync;
static GtkWidget *button_cancel_sync;
static GtkWidget *button_backup;
static GtkWidget *menu_hide_privates;
static GtkWidget *menu_show_privates;
static GtkWidget *menu_mask_privates;

extern GtkWidget *weekview_window;
extern GtkWidget *monthview_window;

/****************************** Prototypes ************************************/
static void cb_delete_event(GtkWidget *widget, GdkEvent *event, gpointer data);

static void install_gui_and_size(GtkWidget *main_window);

static void cb_private(GtkWidget *widget, gpointer data);

char *getViewMenuXmlString();
#ifdef ENABLE_PLUGINS


static void call_plugin_help(int number);

#endif

/****************************** Main Code *************************************/

static int create_main_boxes(void) {
    g_hbox2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    g_vbox0_1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    gtk_box_pack_start(GTK_BOX(g_hbox), g_hbox2, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(g_vbox0), g_vbox0_1, FALSE, FALSE, 0);
    return EXIT_SUCCESS;
}

static int gui_cleanup(void) {
#ifdef ENABLE_PLUGINS
    struct plugin_s *plugin;
    GList *plugin_list, *temp_list;
#endif

#ifdef ENABLE_PLUGINS
    plugin_list = NULL;
    plugin_list = get_plugin_list();

    /* Find out which (if any) plugin to call a gui_cleanup on */
    for (temp_list = plugin_list; temp_list; temp_list = temp_list->next) {
        plugin = (struct plugin_s *) temp_list->data;
        if (plugin) {
            if (plugin->number == glob_app) {
                if (plugin->plugin_gui_cleanup) {
                    plugin->plugin_gui_cleanup();
                }
                break;
            }
        }
    }
#endif

    // if you uncomment this line, changing themes will switch live
    // however, there are some side effects that I haven't figured out
    // how to overcome. (things overridden in a theme, but not defined in another
    // do not revert back to default.)
  //   read_gtkrc_file();
    switch (glob_app) {
        case DATEBOOK:
            datebook_gui_cleanup();
            break;
        case ADDRESS:
            address_gui_cleanup();
            break;
        case TODO:
            todo_gui_cleanup();
            break;
        case MEMO:
            memo_gui_cleanup();
            break;
        default:
            break;
    }
    return EXIT_SUCCESS;
}

#ifdef ENABLE_PLUGINS

void call_plugin_gui(int number, int unique_id) {
    struct plugin_s *plugin;
    GList *plugin_list, *temp_list;

    if (!number) {
        return;
    }

    gui_cleanup();

    plugin_list = NULL;
    plugin_list = get_plugin_list();

    /* Destroy main boxes and recreate them */
    gtk_widget_destroy(g_vbox0_1);
    gtk_widget_destroy(g_hbox2);
    create_main_boxes();
    if (glob_date_timer_tag) {
        g_source_remove(glob_date_timer_tag);
        glob_date_timer_tag = 0;
    }

    /* Find out which plugin we are calling */
    for (temp_list = plugin_list; temp_list; temp_list = temp_list->next) {
        plugin = (struct plugin_s *) temp_list->data;
        if (plugin) {
            if (plugin->number == number) {
                glob_app = plugin->number;
                if (plugin->plugin_gui) {
                    plugin->plugin_gui(g_vbox0_1, g_hbox2, unique_id);
                }
                break;
            }
        }
    }
}

//static void cb_plugin_setup() {
//    call_plugin_gui(plugin_number++, 0);
//}

static void cb_plugin_gui(GtkAction *action, int number) {
    call_plugin_gui(number, 0);
}

/* Redraws plugin GUI after structure changing event such as category editing */
void plugin_gui_refresh(int unique_id) {
    call_plugin_gui(glob_app, unique_id);
}
static void cb_plugin_help(GtkAction *action,
                        gpointer   user_data){
   call_plugin_help(GPOINTER_TO_INT(user_data));
}
static void call_plugin_help(int number) {
    struct plugin_s *plugin;
    GList *plugin_list, *temp_list;
    char *button_text[] = {N_("OK")};
    char *text;
    int width, height;

    if (!number) {
        return;
    }

    plugin_list = NULL;
    plugin_list = get_plugin_list();

    /* Find out which plugin we are calling */
    for (temp_list = plugin_list; temp_list; temp_list = temp_list->next) {
        plugin = (struct plugin_s *) temp_list->data;
        if (plugin) {
            if (plugin->number == number) {
                if (plugin->plugin_help) {
                    text = NULL;
                    plugin->plugin_help(&text, &width, &height);
                    if (text) {
                        dialog_generic(GTK_WINDOW(window),
                                       plugin->help_name, DIALOG_INFO, text, 1, button_text);
                        free(text);
                    }
                }
                break;
            }
        }
    }
}



#endif

static void cb_print(GtkWidget *widget, gpointer data) {
#ifdef ENABLE_PLUGINS
    struct plugin_s *plugin;
    GList *plugin_list, *temp_list;
#endif
    char *button_text[] = {N_("OK")};

    switch (glob_app) {
        case DATEBOOK:
            if (print_gui(window, DATEBOOK, 1, 0x07) == DIALOG_SAID_PRINT) {
                datebook_print(print_day_week_month);
            }
            return;
        case ADDRESS:
            if (print_gui(window, ADDRESS, 0, 0x00) == DIALOG_SAID_PRINT) {
                address_print();
            }
            return;
        case TODO:
            if (print_gui(window, TODO, 0, 0x00) == DIALOG_SAID_PRINT) {
                todo_print();
            }
            return;
        case MEMO:
            if (print_gui(window, MEMO, 0, 0x00) == DIALOG_SAID_PRINT) {
                memo_print();
            }
            return;
    }
#ifdef ENABLE_PLUGINS
    plugin_list = NULL;
    plugin_list = get_plugin_list();

    for (temp_list = plugin_list; temp_list; temp_list = temp_list->next) {
        plugin = (struct plugin_s *) temp_list->data;
        if (plugin) {
            if (glob_app == plugin->number) {
                if (plugin->plugin_print) {
                    plugin->plugin_print();
                    return;
                }
            }
        }
    }
#endif
    dialog_generic(GTK_WINDOW(window),
                   _("Print"), DIALOG_WARNING,
                   _("There is no print support for this conduit."),
                   1, button_text);
}

static void cb_restore(GtkWidget *widget, gpointer data) {
    int r;
    int w, h, x, y;

    jp_logf(JP_LOG_DEBUG, "cb_restore()\n");

    w = gdk_window_get_width(gtk_widget_get_window(window));
    h = gdk_window_get_height(gtk_widget_get_window(window));

    gdk_window_get_root_origin(gtk_widget_get_window(window), &x, &y);

    w = w / 2;
    x += 40;

    r = restore_gui(window, w, h, x, y);

    /* Fork successful, child sync process started */
    if (glob_child_pid && (r == EXIT_SUCCESS)) {
        gtk_widget_hide(button_sync);
        gtk_widget_show(button_cancel_sync);
    }
}

static void cb_import(GtkWidget *widget, gpointer data) {
#ifdef ENABLE_PLUGINS
    struct plugin_s *plugin;
    GList *plugin_list, *temp_list;
#endif
    char *button_text[] = {N_("OK")};

    switch (glob_app) {
        case DATEBOOK:
            datebook_import(window);
            return;
        case ADDRESS:
            address_import(window);
            return;
        case TODO:
            todo_import(window);
            return;
        case MEMO:
            memo_import(window);
            return;
    }
#ifdef ENABLE_PLUGINS
    plugin_list = NULL;
    plugin_list = get_plugin_list();

    for (temp_list = plugin_list; temp_list; temp_list = temp_list->next) {
        plugin = (struct plugin_s *) temp_list->data;
        if (plugin) {
            if (glob_app == plugin->number) {
                if (plugin->plugin_import) {
                    plugin->plugin_import(window);
                    return;
                }
            }
        }
    }
#endif
    dialog_generic(GTK_WINDOW(window),
                   _("Import"), DIALOG_WARNING,
                   _("There is no import support for this conduit."),
                   1, button_text);
}

static void cb_export(GtkWidget *widget, gpointer data) {
#ifdef ENABLE_PLUGINS
    struct plugin_s *plugin;
    GList *plugin_list, *temp_list;
#endif
    char *button_text[] = {N_("OK")};

    switch (glob_app) {
        case DATEBOOK:
            datebook_export(window);
            return;
        case ADDRESS:
            address_export(window);
            return;
        case TODO:
            todo_export(window);
            return;
        case MEMO:
            memo_export(window);
            return;
    }
#ifdef ENABLE_PLUGINS
    plugin_list = NULL;
    plugin_list = get_plugin_list();

    for (temp_list = plugin_list; temp_list; temp_list = temp_list->next) {
        plugin = (struct plugin_s *) temp_list->data;
        if (plugin) {
            if (glob_app == plugin->number) {
                if (plugin->plugin_export) {
                    plugin->plugin_export(window);
                    return;
                }
            }
        }
    }
#endif
    dialog_generic(GTK_WINDOW(window),
                   _("Export"), DIALOG_WARNING,
                   _("There is no export support for this conduit."),
                   1, button_text);
}

//FIX remove
//static void cb_private_from_radio(GtkRadioAction *action) {
//    cb_private(NULL, GINT_TO_POINTER(gtk_radio_action_get_current_value(action)));
//}

static void cb_private(GtkWidget *widget, gpointer data) {
    int privates, was_privates;
    int r_dialog = 0;
    static int skip_false_call = 0;
#ifdef ENABLE_PRIVATE
    char ascii_password[64];
    int r_pass;
    int retry;
#endif

    if (skip_false_call) {
        skip_false_call = 0;
        return;
    }

    was_privates = show_privates(GET_PRIVATES);
    privates = show_privates(GPOINTER_TO_INT(data));

    /* no changes */
    if (was_privates == privates)
        return;

    switch (privates) {
        case MASK_PRIVATES:
            gtk_widget_hide(button_locked);
            gtk_widget_show(button_masklocked);
            gtk_widget_hide(button_unlocked);
            if (! gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menu_mask_privates))) {
                skip_false_call = 1;
                gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_mask_privates), TRUE);
            }
            break;
        case HIDE_PRIVATES:
            gtk_widget_show(button_locked);
            gtk_widget_hide(button_masklocked);
            gtk_widget_hide(button_unlocked);
            if (! gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menu_hide_privates))) {
                skip_false_call = 1;
                gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_hide_privates), TRUE);
            }
            break;
        case SHOW_PRIVATES:
            /* Ask for the password, or don't depending on configure option */
#ifdef ENABLE_PRIVATE
            memset(ascii_password, 0, sizeof(ascii_password));
            if (was_privates != SHOW_PRIVATES) {
                retry = FALSE;
                do {
                    r_dialog = dialog_password(GTK_WINDOW(window),
                                               ascii_password, retry);
                    r_pass = verify_password(ascii_password);
                    retry = TRUE;
                } while ((r_pass == FALSE) && (r_dialog == 2));
            }
#else
            r_dialog = 2;
#endif
            if (r_dialog == 2) {
                gtk_widget_hide(button_locked);
                gtk_widget_hide(button_masklocked);
                gtk_widget_show(button_unlocked);
                if (! gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menu_show_privates))) {
                    skip_false_call = 1;
                    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_show_privates), TRUE);
                }
            } else {
                /* wrong or canceled password, hide the entries */
                privates = show_privates(HIDE_PRIVATES);
                if (! gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menu_hide_privates))) {
                    skip_false_call = 1;
                    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_hide_privates), TRUE);
                }
                cb_app_button(NULL, GINT_TO_POINTER(REDRAW));
                return;
            }
            break;
    }

    if (was_privates != privates)
        cb_app_button(NULL, GINT_TO_POINTER(REDRAW));
}

static void cb_install_user(GtkWidget *widget, gpointer data) {
    int r;

    r = install_user_gui(window);

    /* Fork successful, child sync process started */
    if (glob_child_pid && (r == EXIT_SUCCESS)) {
        gtk_widget_hide(button_sync);
        gtk_widget_show(button_cancel_sync);
    }
}

void cb_datebook_app_button() {
    cb_app_button(NULL, GINT_TO_POINTER(DATEBOOK));
}

void cb_address_app_button() {
    cb_app_button(NULL, GINT_TO_POINTER(ADDRESS));
}

void cb_todo_app_button() {
    cb_app_button(NULL, GINT_TO_POINTER(TODO));
}

void cb_memo_app_button() {
    cb_app_button(NULL, GINT_TO_POINTER(MEMO));
}

void cb_app_button(GtkWidget *widget, gpointer data) {
    int app;
    int refresh;

    app = GPOINTER_TO_INT(data);

    /* If the current and selected apps are the same then just refresh screen */
    refresh = (app == glob_app);

    /* Tear down GUI when switching apps or on forced REDRAW */
    if ((!refresh) || (app == REDRAW)) {
        gui_cleanup();
        if (glob_date_timer_tag) {
            g_source_remove(glob_date_timer_tag);
            glob_date_timer_tag = 0;
        }
        gtk_widget_destroy(g_vbox0_1);
        gtk_widget_destroy(g_hbox2);
        create_main_boxes();
        if (app == REDRAW) {
            app = glob_app;
        }
    }

    switch (app) {
        case DATEBOOK:
            if (refresh) {
                datebook_refresh(TRUE, TRUE);
            } else {
                glob_app = DATEBOOK;
                datebook_gui(g_vbox0_1, g_hbox2);
            }
            break;
        case ADDRESS:
            if (refresh) {
                address_cycle_cat();
                address_refresh();
            } else {
                glob_app = ADDRESS;
                address_gui(g_vbox0_1, g_hbox2);
            }
            break;
        case TODO:
            if (refresh) {
                todo_cycle_cat();
                todo_refresh();
            } else {
                glob_app = TODO;
                todo_gui(g_vbox0_1, g_hbox2);
            }
            break;
        case MEMO:
            if (refresh) {
                memo_cycle_cat();
                memo_refresh();
            } else {
                glob_app = MEMO;
                memo_gui(g_vbox0_1, g_hbox2);
            }
            break;
        default:
            /* recursion */
            if ((glob_app == DATEBOOK) ||
                (glob_app == ADDRESS) ||
                (glob_app == TODO) ||
                (glob_app == MEMO))
                cb_app_button(NULL, GINT_TO_POINTER(glob_app));
            break;
    }
}

static void sync_sig_handler(int sig) {
    unsigned int flags;
    int r;

    flags = skip_plugins ? SYNC_NO_PLUGINS : 0;

    r = setup_sync(flags);

    /* Fork successful, child sync process started */
    if (glob_child_pid && (r == EXIT_SUCCESS)) {
        gtk_widget_hide(button_sync);
        gtk_widget_show(button_cancel_sync);
    }
}

static void cb_sync(GtkWidget *widget, unsigned int flags) {
    long ivalue;
    int r;

    /* confirm file installation */
    get_pref(PREF_CONFIRM_FILE_INSTALL, &ivalue, NULL);
    if (ivalue) {
        char file[FILENAME_MAX];
        char home_dir[FILENAME_MAX];
        struct stat buf;

        /* If there are files to be installed, ask the user right before sync */
        get_home_file_name("", home_dir, sizeof(home_dir));
        g_snprintf(file, sizeof(file), "%s/"EPN".install", home_dir);

        if (!stat(file, &buf)) {
            if (buf.st_size > 0) {
                install_gui_and_size(window);
            }
        }
    }

    r = setup_sync(flags);

    /* Fork successful, child sync process started */
    if (glob_child_pid && (r == EXIT_SUCCESS)) {
        gtk_widget_hide(button_sync);
        gtk_widget_show(button_cancel_sync);
    }

}

void cb_cancel_sync(GtkWidget *widget, unsigned int flags);

char *getFileMenuXmlString();

void addUiFromString(const GtkUIManager *uiManager, const char *menuXml);


#ifdef WEBMENU
static const char *getWebMenuXmlString();
#endif



void cb_cancel_sync(GtkWidget *widget, unsigned int flags) {
    if (glob_child_pid) {
        jp_logf(JP_LOG_GUI, "****************************************\n");
        jp_logf(JP_LOG_GUI, _(" Cancelling HotSync\n"));
        jp_logf(JP_LOG_GUI, "****************************************\n");
        kill(glob_child_pid, SIGTERM);
    }
    gtk_widget_hide(button_cancel_sync);
    gtk_widget_show(button_sync);
}

/*
 * This is called when the user name from the palm doesn't match
 * or the user ID from the palm is 0
 */
static int bad_sync_exit_status(int exit_status) {
    char text1[] =
            /*-------------------------------------------*/
            N_("This handheld does not have the same user name or user ID\n"
               "as the one that was synced the last time.\n"
               "Syncing could have unwanted effects including data loss.\n"
               "\n"
               "Read the user manual if you are uncertain.");
    char text2[] =
            /*-------------------------------------------*/
            N_("This handheld has a NULL user ID.\n"
               "Every handheld must have a unique user ID in order to sync properly.\n"
               "If the handheld has been hard reset, \n"
               "   use restore from the menu to restore it.\n"
               "Otherwise, to add a new user name and ID\n"
               "   use install-user from the menu.\n"
               "\n"
               "Read the user manual if you are uncertain.");
    char *button_text[] = {N_("Cancel Sync"), N_("Sync Anyway")
    };

    if (!GTK_IS_WINDOW(window)) {
        return EXIT_FAILURE;
    }
    if ((exit_status == SYNC_ERROR_NOT_SAME_USERID) ||
        (exit_status == SYNC_ERROR_NOT_SAME_USER)) {
        return dialog_generic(GTK_WINDOW(window),
                              _("Sync Problem"), DIALOG_WARNING, _(text1), 2, button_text);
    }
    if (exit_status == SYNC_ERROR_NULL_USERID) {
        return dialog_generic(GTK_WINDOW(window),
                              _("Sync Problem"), DIALOG_ERROR, _(text2), 1, button_text);
    }
    return EXIT_FAILURE;
}

void output_to_pane(const char *str) {
    int h, new_y;
    long ivalue;
    GtkWidget *pane_hbox;
    GtkRequisition size_requisition;

    /* Adjust window height to user preference or minimum size */
    get_pref(PREF_OUTPUT_HEIGHT, &ivalue, NULL);
    /* Make them look at something if output happens */
    if (ivalue < 50) {
        /* Ask GTK for size which is just large enough to show both buttons */
        pane_hbox = gtk_paned_get_child2(GTK_PANED(output_pane));
        //gtk2 gtk_widget_size_request(pane_hbox, &size_requisition);
        gtk_widget_get_preferred_size(pane_hbox, NULL, &size_requisition);
        ivalue = size_requisition.height + 1;
        set_pref(PREF_OUTPUT_HEIGHT, ivalue, NULL, TRUE);
    }
    //w = gdk_window_get_width(gtk_widget_get_window(window));
    h = gdk_window_get_height(gtk_widget_get_window(window));
    new_y = h - ivalue;
    gtk_paned_set_position(GTK_PANED(output_pane), new_y);

    /* Output text to window */
    GtkTextIter end_iter;
    GtkTextMark *end_mark;
    gboolean scroll_to_end = FALSE;
    gdouble sbar_value, sbar_page_size, sbar_upper;

    /* The window position scrolls with new input if the user has left
     * the scrollbar at the bottom of the window.  Otherwise, if the user
     * is scrolling back through the log then jpilot does nothing and defers
     * to the user. */

    /* Get position of scrollbar */
    //gtk2 GtkAdjustment * vadjustment =  gtk_text_view_get_vadjustment (g_output_text);
    GtkAdjustment * vadjustment = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(g_output_text));
    sbar_value = gtk_adjustment_get_value(vadjustment);
    sbar_page_size = gtk_adjustment_get_page_size(vadjustment);
    sbar_upper = gtk_adjustment_get_upper(vadjustment);
    /* Keep scrolling to the end only if we are already near(1 window) of the end
     * OR the window has just been created and is blank */
    if ((abs((sbar_value + sbar_page_size) - sbar_upper) < sbar_page_size)
        || sbar_page_size == 1) {
        scroll_to_end = TRUE;
    }

    gtk_text_buffer_get_end_iter(g_output_text_buffer, &end_iter);

    if (!g_utf8_validate(str, -1, NULL)) {
        gchar *utf8_text;

        utf8_text = g_locale_to_utf8(str, -1, NULL, NULL, NULL);
        gtk_text_buffer_insert(g_output_text_buffer, &end_iter, utf8_text, -1);
        g_free(utf8_text);
    } else
        gtk_text_buffer_insert(g_output_text_buffer, &end_iter, str, -1);

    if (scroll_to_end) {
        end_mark = gtk_text_buffer_create_mark(g_output_text_buffer, NULL, &end_iter, TRUE);
        gtk_text_buffer_move_mark(g_output_text_buffer, end_mark, &end_iter);
        gtk_text_view_scroll_to_mark(g_output_text, end_mark, 0, TRUE, 0.0, 0.0);
        gtk_text_buffer_delete_mark(g_output_text_buffer, end_mark);
    }
}


gboolean cb_read_pipe_from_child(GIOChannel *channel, GIOCondition cond, gpointer data) {
    int num;
    char buf_space[1026];
    char *buf;
    int buf_len;
    fd_set fds;
    struct timeval tv;
    int ret, done;
    char *Pstr1, *Pstr2, *Pstr3;
    int user_len;
    char password[MAX_PREF_LEN];
    int password_len;
    unsigned long user_id;
    int i, reason;
    int command;
    char user[MAX_PREF_LEN];
    char command_str[80];
    long char_set;
    const char *svalue;
    char title[MAX_PREF_LEN + 256];
    char *user_name;
    gint in = g_io_channel_unix_get_fd(channel);

    /* This is so we can always look at the previous char in buf */
    buf = &buf_space[1];
    buf[-1] = 'A'; /* that looks weird */
    done = 0;
    while (!done) {
        buf[0] = '\0';
        buf_len = 0;
        /* Read until "\0\n", or buffer full */

        for (i = 0; i < 1022; i++) {
            buf[i] = '\0';
            /* Linux modifies tv in the select call */
            tv.tv_sec = 0;
            tv.tv_usec = 0;
            FD_ZERO(&fds);
            FD_SET(in, &fds);
            ret = select(in + 1, &fds, NULL, NULL, &tv);
            if ((ret < 1) || (!FD_ISSET(in, &fds))) {
                done = 1;
                break;
            }
            ret = read(in, &(buf[i]), 1);
            if (ret <= 0) {
                done = 1;
                break;
            }
            if ((buf[i - 1] == '\0') && (buf[i] == '\n')) {
                buf_len = buf_len - 1;
                break;
            }
            buf_len++;
            if (buf_len >= 1022) {
                buf[buf_len] = '\0';
                break;
            }
        }

        if (buf_len < 1) break;

        /* Look for the command */
        command = 0;
        sscanf(buf, "%d:", &command);

        Pstr1 = strstr(buf, ":");
        if (Pstr1 != NULL) {
            Pstr1++;
        }
#ifdef PIPE_DEBUG
        printf("command=%d [%s]\n", command, Pstr1);
#endif
        if (Pstr1) {
            switch (command) {
                case PIPE_PRINT:
                    /* Output the text to the Sync window */
                    output_to_pane(Pstr1);
                    break;
                case PIPE_USERID:
                    /* Save user ID as pref */
                    num = sscanf(Pstr1, "%lu", &user_id);
                    if (num > 0) {
                        jp_logf(JP_LOG_DEBUG, "pipe_read: user id = %lu\n", user_id);
                        set_pref(PREF_USER_ID, user_id, NULL, TRUE);
                    } else {
                        jp_logf(JP_LOG_DEBUG, "pipe_read: trouble reading user id\n");
                    }
                    break;
                case PIPE_USERNAME:
                    /* Save username as pref */
                    Pstr2 = strchr(Pstr1, '\"');
                    if (Pstr2) {
                        Pstr2++;
                        Pstr3 = strchr(Pstr2, '\"');
                        if (Pstr3) {
                            user_len = Pstr3 - Pstr2;
                            if (user_len > MAX_PREF_LEN) {
                                user_len = MAX_PREF_LEN;
                            }
                            g_strlcpy(user, Pstr2, user_len + 1);
                            jp_logf(JP_LOG_DEBUG, "pipe_read: user = %s\n", user);
                            set_pref(PREF_USER, 0, user, TRUE);
                        }
                    }
                    break;
                case PIPE_PASSWORD:
                    /* Save password as pref */
                    Pstr2 = strchr(Pstr1, '\"');
                    if (Pstr2) {
                        Pstr2++;
                        Pstr3 = strchr(Pstr2, '\"');
                        if (Pstr3) {
                            password_len = Pstr3 - Pstr2;
                            if (password_len > MAX_PREF_LEN) {
                                password_len = MAX_PREF_LEN;
                            }
                            g_strlcpy(password, Pstr2, password_len + 1);
                            jp_logf(JP_LOG_DEBUG, "pipe_read: password = %s\n", password);
                            set_pref(PREF_PASSWORD, 0, password, TRUE);
                        }
                    }
                    break;
                case PIPE_WAITING_ON_USER:
#ifdef PIPE_DEBUG
                    printf("waiting on user\n");
#endif
                    /* Look for the reason */
                    num = sscanf(Pstr1, "%d", &reason);
#ifdef PIPE_DEBUG
                    printf("reason %d\n", reason);
#endif
                    if (num > 0) {
                        jp_logf(JP_LOG_DEBUG, "pipe_read: reason = %d\n", reason);
                    } else {
                        jp_logf(JP_LOG_DEBUG, "pipe_read: trouble reading reason\n");
                    }
                    if ((reason == SYNC_ERROR_NOT_SAME_USERID) ||
                        (reason == SYNC_ERROR_NOT_SAME_USER) ||
                        (reason == SYNC_ERROR_NULL_USERID)) {
                        /* Future code */
                        /* This is where to add an option for adding user or
                         user id to possible ids to sync with. */
                        ret = bad_sync_exit_status(reason);
#ifdef PIPE_DEBUG
                        printf("ret=%d\n", ret);
#endif
                        if (ret == DIALOG_SAID_2) {
                            sprintf(command_str, "%d:\n", PIPE_SYNC_CONTINUE);
                        } else {
                            sprintf(command_str, "%d:\n", PIPE_SYNC_CANCEL);
                        }
                        if (write(pipe_to_child, command_str, strlen(command_str)) < 0) {
                            jp_logf(JP_LOG_WARN, "write failed %s %d\n", __FILE__, __LINE__);
                        }
                        fsync(pipe_to_child);
                    }
                    break;
                case PIPE_FINISHED:
                    /* Update main window title as user name may have changed */
                    get_pref(PREF_CHAR_SET, &char_set, NULL);
                    get_pref(PREF_USER, NULL, &svalue);
                    strcpy(title, PN" "VERSION);
                    if ((svalue) && (svalue[0])) {
                        strcat(title, _(" User: "));
                        user_name = charset_p2newj(svalue, -1, char_set);
                        strcat(title, user_name);
                        gtk_window_set_title(GTK_WINDOW(window), title);
                        free(user_name);
                    }
                    /* And redraw GUI */
                    if (Pstr1) {
                        cb_app_button(NULL, GINT_TO_POINTER(REDRAW));
                    }
                    break;
                default:
                    jp_logf(JP_LOG_WARN, _("Unknown command from sync process\n"));
                    jp_logf(JP_LOG_WARN, "buf=[%s]\n", buf);
            }
        }
    }
    return TRUE;
}

static void cb_about(GtkWidget *widget, gpointer data) {
    char *button_text[] = {N_("OK")};
    char about[256];
    char options[1024];
    int w;

    w = gdk_window_get_width(gtk_widget_get_window(window));
    //h = gdk_window_get_height(gtk_widget_get_window(window));

    w = w / 2;
    //h = 1;

    g_snprintf(about, sizeof(about), _("About %s"), PN);

    get_compile_options(options, sizeof(options));

    if (GTK_IS_WINDOW(window)) {
        dialog_generic(GTK_WINDOW(window),
                       about, DIALOG_INFO, options, 1, button_text);
    }
}

/* Experimental webmenu that has never been used in practice */
#ifdef WEBMENU

#define NETSCAPE_EXISTING   0
#define NETSCAPE_NEW_WINDOW 1
#define NETSCAPE_NEW        2
#define MOZILLA_EXISTING    3
#define MOZILLA_NEW_WINDOW  4
#define MOZILLA_NEW_TAB     5
#define MOZILLA_NEW         6
#define GALEON_EXISTING     7
#define GALEON_NEW_WINDOW   8
#define GALEON_NEW_TAB      9
#define GALEON_NEW          10
#define OPERA_EXISTING      11
#define OPERA_NEW_WINDOW    12
#define OPERA_NEW           13
#define GNOME_URL           14
#define LYNX_NEW            15
#define LINKS_NEW           16
#define W3M_NEW             17
#define KONQUEROR_NEW       18

static struct url_command {
   int id;
   char *desc;
   char *command;
};

/* These strings were taken from xchat 2.0.0 */
static struct url_command url_commands[]={
     {NETSCAPE_EXISTING, "/Web/Netscape/Open jpilot.org in existing", "netscape -remote 'openURL(http://jpilot.org)'"},
     {NETSCAPE_NEW_WINDOW, "/Web/Netscape/Open jpilot.org in new window", "netscape -remote 'openURL(http://jpilot.org,new-window)'"},
     {NETSCAPE_NEW, "/Web/Netscape/Open jpilot.org in new Netscape", "netscape http://jpilot.org"},

     {MOZILLA_EXISTING, "/Web/Mozilla/Open jpilot.org in existing", "mozilla -remote 'openURL(http://jpilot.org)'"},
     {MOZILLA_NEW_WINDOW, "/Web/Mozilla/Open jpilot.org in new window", "mozilla -remote 'openURL(http://jpilot.org,new-window)'"},
     {MOZILLA_NEW_TAB, "/Web/Mozilla/Open jpilot.org in new tab", "mozilla -remote 'openURL(http://jpilot.org,new-tab)'"},
     {MOZILLA_NEW, "/Web/Mozilla/Open jpilot.org in new Mozilla", "mozilla http://jpilot.org"},

     {GALEON_EXISTING, "/Web/Galeon/Open jpilot.org in existing", "galeon -x 'http://jpilot.org'"},
     {GALEON_NEW_WINDOW, "/Web/Galeon/Open jpilot.org in new window", "galeon -w 'http://jpilot.org'"},
     {GALEON_NEW_TAB, "/Web/Galeon/Open jpilot.org in new tab", "galeon -n 'http://jpilot.org'"},
     {GALEON_NEW, "/Web/Galeon/Open jpilot.org in new Galeon", "galeon 'http://jpilot.org'"},

     {OPERA_EXISTING, "/Web/Opera/Open jpilot.org in existing", "opera -remote 'openURL(http://jpilot.org)' &"},
     {OPERA_NEW_WINDOW, "/Web/Opera/Open jpilot.org in new window", "opera -remote 'openURL(http://jpilot.org,new-window)' &"},
     {OPERA_NEW, "/Web/Opera/Open jpilot.org in new Opera", "opera http://jpilot.org &"},

     {GNOME_URL, "/Web/GnomeUrl/Gnome URL Handler for jpilot.org", "gnome-moz-remote http://jpilot.org"},

     {LYNX_NEW, "/Web/Lynx/Lynx jpilot.org", "xterm -e lynx http://jpilot.org &"},

     {LINKS_NEW, "/Web/Links/Links jpilot.org", "xterm -e links http://jpilot.org &"},

     {W3M_NEW, "/Web/W3M/w3m jpilot.org", "xterm -e w3m http://jpilot.org &"},

     {KONQUEROR_NEW, "/Web/Konqueror/Konqueror jpilot.org", "konqueror http://jpilot.org"}
};



static void cb_web(GtkWidget *widget, gpointer data);
void openNetscapeExisting(){
    cb_web(NULL,GINT_TO_POINTER(NETSCAPE_EXISTING));
}
void openNetscapeNewWindow(){
    cb_web(NULL,GINT_TO_POINTER(NETSCAPE_NEW_WINDOW));
}
void openNetscapeNew(){
    cb_web(NULL,GINT_TO_POINTER(NETSCAPE_NEW));
}
void openMozillaExisting(){
    cb_web(NULL,GINT_TO_POINTER(MOZILLA_EXISTING));
}
void openMozillaNewWindow(){
    cb_web(NULL,GINT_TO_POINTER(MOZILLA_NEW_WINDOW));
}
void openMozillaNewTab(){
    cb_web(NULL,GINT_TO_POINTER(MOZILLA_NEW_TAB));
}
void openMozillaNew(){
    cb_web(NULL,GINT_TO_POINTER(MOZILLA_NEW));
}
void openGaleonExisting(){
    cb_web(NULL,GINT_TO_POINTER(GALEON_EXISTING));
}
void openGaleonNewWindow(){
    cb_web(NULL,GINT_TO_POINTER(GALEON_NEW_WINDOW));
}
void openGaleonNewTab(){
    cb_web(NULL,GINT_TO_POINTER(GALEON_NEW_TAB));
}
void openGaleonNew(){
    cb_web(NULL,GINT_TO_POINTER(GALEON_NEW));
}
void openOperaExisting(){
    cb_web(NULL,GINT_TO_POINTER(OPERA_EXISTING));
}
void openOperaNewWindow(){
    cb_web(NULL,GINT_TO_POINTER(OPERA_NEW_WINDOW));
}
void openOperaNew(){
    cb_web(NULL,GINT_TO_POINTER(OPERA_NEW));
}
void openGnomeNew(){
    cb_web(NULL,GINT_TO_POINTER(GNOME_URL));
}
void openLynxNew(){
    cb_web(NULL,GINT_TO_POINTER(LYNX_NEW));
}
void openLinksNew(){
    cb_web(NULL,GINT_TO_POINTER(LINKS_NEW));
}
void openW3MNew(){
    cb_web(NULL,GINT_TO_POINTER(W3M_NEW));
}
void openKonquerorNew(){
    cb_web(NULL,GINT_TO_POINTER(KONQUEROR_NEW));
}
static void cb_web(GtkWidget *widget, gpointer data)
{
   int sel;

   sel=GPOINTER_TO_INT(data);
   jp_logf(JP_LOG_INFO, PN": executing %s\n", url_commands[sel].command);
   if (system(url_commands[sel].command) == -1) {
      jp_logf(JP_LOG_WARN, "system call failed %s %d\n", __FILE__, __LINE__);
   }
}

#endif

static void install_gui_and_size(GtkWidget *main_window) {
    int w, h, x, y;

    w = gdk_window_get_width(gtk_widget_get_window(window));
    h = gdk_window_get_height(gtk_widget_get_window(window));
    gdk_window_get_root_origin(gtk_widget_get_window(window), &x, &y);

    w = w / 2;
    x += 40;

    install_gui(main_window, w, h, x, y);
}

static void cb_install_gui(GtkWidget *widget, gpointer data) {
    jp_logf(JP_LOG_DEBUG, "cb_install_gui()\n");

    install_gui_and_size(window);
}

static GtkWidget* create_menu_item(GtkWidget *menu,
                                   GtkAccelGroup *accel_group,
                                   gchar *icon_name,
                                   const char **pixbuf_data,
                                   gchar *text,
                                   guint accel_key,
                                   GdkModifierType accel_mods,
                                   void *callback,
                                   gpointer *data)
{
    GtkWidget *box;
    GtkWidget *icon;
    GtkWidget *label;
    GtkWidget *menu_item;
    if (accel_key > 0) {
        label = gtk_accel_label_new(text);
    } else {
        label = gtk_label_new(text);
    }
    menu_item = gtk_menu_item_new();
    box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);

    if (pixbuf_data) {
        GtkWidget *image;
        GdkPixbuf *pixbuf;

        pixbuf = gdk_pixbuf_new_from_xpm_data(pixbuf_data);
        image = gtk_image_new_from_pixbuf(pixbuf);
        gtk_container_add(GTK_CONTAINER(box), image);
        // FIX - Do I need to unref?
        // g_object_unref(pixbuf);
    } else {
        icon = gtk_image_new_from_icon_name(icon_name, GTK_ICON_SIZE_MENU);
        gtk_container_add(GTK_CONTAINER(box), icon);
    }

    //gtk_label_set_use_underline(GTK_LABEL(label), TRUE);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0);

    if ((accel_key > 0) && (accel_group)) {
        gtk_widget_add_accelerator(menu_item, "activate", accel_group,
                                   accel_key, accel_mods, GTK_ACCEL_VISIBLE);
        gtk_accel_label_set_accel_widget(GTK_ACCEL_LABEL(label), menu_item);
    }

    gtk_box_pack_end(GTK_BOX(box), label, TRUE, TRUE, 0);

    gtk_container_add(GTK_CONTAINER(menu_item), box);

    g_signal_connect(G_OBJECT(menu_item), "activate", callback, data);
    gtk_widget_show(menu_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);

    return menu_item;
}

static void get_main_menu(GtkWidget *my_window,
                          GtkWidget **menubar,
                          GtkWidget **menubar_help,
                          GList *plugin_list) {
    #ifdef ENABLE_PLUGINS
       int count, help_count;
       struct plugin_s *p;
       GList *temp_list;
       int F_KEYS[]={GDK_KEY_F5,GDK_KEY_F6,GDK_KEY_F7,GDK_KEY_F8,GDK_KEY_F9,GDK_KEY_F10,GDK_KEY_F11,GDK_KEY_F12};
    #endif
    /*
     * New Menu Code
     */
    *menubar = gtk_menu_bar_new();
    *menubar_help = gtk_menu_bar_new();
    gtk_widget_set_hexpand(*menubar, FALSE);
    gtk_widget_set_hexpand(*menubar_help, FALSE);

    GtkWidget *menu1 = gtk_menu_new();
    GtkWidget *menu2 = gtk_menu_new();
#ifdef ENABLE_PLUGINS
    GtkWidget *menu3 = gtk_menu_new();
#endif
    GtkWidget *menu4 = gtk_menu_new();

    GtkWidget *m = gtk_menu_item_new_with_mnemonic("_File");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(m), menu1);
    gtk_menu_shell_append(GTK_MENU_SHELL(*menubar), m);
    gtk_widget_show(m);

    m = gtk_menu_item_new_with_mnemonic("_View");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(m), menu2);
    gtk_menu_shell_append(GTK_MENU_SHELL(*menubar), m);
    gtk_widget_show(m);

#ifdef ENABLE_PLUGINS
    /* plugin_list will be NULL if there are none */
    if (plugin_list) {
        m = gtk_menu_item_new_with_mnemonic("_Plugins");
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(m), menu3);
        gtk_menu_shell_append(GTK_MENU_SHELL(*menubar), m);
        gtk_widget_show(m);
    }
#endif

    m = gtk_menu_item_new_with_mnemonic("_Help");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(m), menu4);
    gtk_menu_shell_append(GTK_MENU_SHELL(*menubar_help), m);
    gtk_widget_show(m);

    GtkAccelGroup *accel_group = gtk_accel_group_new();
    gtk_window_add_accel_group(GTK_WINDOW(window), accel_group);

    // First menu ("File")
    create_menu_item(menu1, accel_group, "gtk-find", NULL, "Find", GDK_KEY_f, GDK_CONTROL_MASK, cb_search_gui, NULL);
    GtkWidget *sep = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu1), sep);
    create_menu_item(menu1, accel_group, "gtk-open", NULL, "Install", GDK_KEY_i, GDK_CONTROL_MASK, cb_install_gui, NULL);
    create_menu_item(menu1, NULL, "gtk-go-forward", NULL, "Import", -1, GDK_CONTROL_MASK, cb_import, NULL);
    create_menu_item(menu1, NULL, "gtk-go-back", NULL, "Export", -1, GDK_CONTROL_MASK, cb_export, NULL);
    create_menu_item(menu1, accel_group, "gtk-preferences", NULL, "Preferences", GDK_KEY_s, GDK_CONTROL_MASK, cb_prefs_gui, NULL);
    create_menu_item(menu1, accel_group, "gtk-print", NULL, "Print", GDK_KEY_p, GDK_CONTROL_MASK, cb_print, NULL);
    sep = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu1), sep);
    create_menu_item(menu1, NULL, "", user_icon, "Install User", -1, GDK_CONTROL_MASK, cb_install_user, NULL);
    create_menu_item(menu1, NULL, "gtk-redo", NULL, "Restore Handheld", -1, GDK_CONTROL_MASK, cb_restore, NULL);
    sep = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu1), sep);
    create_menu_item(menu1, accel_group, "gtk-quit", NULL, "Quit", GDK_KEY_q, GDK_CONTROL_MASK, cb_delete_event, NULL);

    // First menu ("View")
    // RADIO
    GSList *group = NULL;
    char *private_menu_text[] = {N_("Hide Private Records"), N_("Show Private Records"), N_("Mask Private Records")};
    int params[] = {HIDE_PRIVATES, SHOW_PRIVATES, MASK_PRIVATES};
    GtkWidget *last_item = NULL;
    GtkWidget *radio_item[3];
    int i;
    for (i = 0; i < 3; i++) {
        radio_item[i] = gtk_radio_menu_item_new_with_label(group, private_menu_text[i]);
        g_signal_connect(G_OBJECT(radio_item[i]), "activate", G_CALLBACK(cb_private), GINT_TO_POINTER(params[i]));
        gtk_radio_menu_item_join_group(GTK_RADIO_MENU_ITEM(radio_item[i]), GTK_RADIO_MENU_ITEM(last_item));
        last_item = radio_item[i];
        gtk_menu_shell_append(GTK_MENU_SHELL(menu2), radio_item[i]);
    }
    menu_hide_privates = radio_item[0];
    menu_show_privates = radio_item[1];
    menu_mask_privates = radio_item[2];

    sep = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu2), sep);
    create_menu_item(menu2, accel_group, "", date_menu_icon, "Datebook", GDK_KEY_F1, 0, cb_app_button, GINT_TO_POINTER(DATEBOOK));
    create_menu_item(menu2, accel_group, "", addr_menu_icon, "Address", GDK_KEY_F2, 0, cb_app_button, GINT_TO_POINTER(ADDRESS));
    create_menu_item(menu2, accel_group, "", todo_menu_icon, "Todos", GDK_KEY_F3, 0, cb_app_button, GINT_TO_POINTER(TODO));
    create_menu_item(menu2, accel_group, "gtk-justify-left", NULL, "Memos", GDK_KEY_F4, 0, cb_app_button, GINT_TO_POINTER(MEMO));

#ifdef ENABLE_PLUGINS
    /* Count the plugin entries */
    for (count=0, temp_list = plugin_list;
         temp_list;
         temp_list = temp_list->next) {
        p = (struct plugin_s *)temp_list->data;
        if (p->menu_name) {
            count++;
        }
    }

    /* Count the plugin help entries */
    for (help_count=0, temp_list = plugin_list;
         temp_list;
         temp_list = temp_list->next) {
        p = (struct plugin_s *)temp_list->data;
        if (p->help_name) {
            help_count++;
        }
    }

    /* Create plugin menu */
    for (i=0, temp_list = plugin_list; temp_list; temp_list = temp_list->next, i++) {
        p = (struct plugin_s *)temp_list->data;
        if (p->menu_name) {
            create_menu_item(menu3, accel_group, "package_system", NULL, p->menu_name, F_KEYS[i], 0, cb_plugin_gui, GINT_TO_POINTER(p->number));
        }
    }
    // About menu
    create_menu_item(menu4, accel_group, "gtk-info", NULL, "About J-Pilot", 0, 0, cb_about, NULL);

    /* Append plugin help menu strings */
    for (temp_list = plugin_list; temp_list; temp_list = temp_list->next) {
        p = (struct plugin_s *)temp_list->data;
        if (p->help_name) {
            create_menu_item(menu4, accel_group, "gtk-info", NULL, p->help_name, 0, 0, cb_plugin_help, GINT_TO_POINTER(p->number));
        }
    }
#endif
}

static void cb_delete_event(GtkWidget *widget, GdkEvent *event, gpointer data) {
    int pw, ph;
    int x, y;
#ifdef ENABLE_PLUGINS
    struct plugin_s *plugin;
    GList *plugin_list, *temp_list;
#endif

    /* gdk_window_get_deskrelative_origin(window->window, &x, &y); */
    gdk_window_get_origin(gtk_widget_get_window(window), &x, &y);
    jp_logf(JP_LOG_DEBUG, "x=%d, y=%d\n", x, y);

    pw = gdk_window_get_width(gtk_widget_get_window(window));
    ph = gdk_window_get_height(gtk_widget_get_window(window));
    set_pref(PREF_WINDOW_WIDTH, pw, NULL, FALSE);
    set_pref(PREF_WINDOW_HEIGHT, ph, NULL, FALSE);
    set_pref(PREF_LAST_APP, glob_app, NULL, TRUE);

    gui_cleanup();

    if (weekview_window)
        cb_weekview_quit(weekview_window, NULL);
    if (monthview_window)
        cb_monthview_quit(monthview_window, NULL);

#ifdef ENABLE_PLUGINS
    plugin_list = get_plugin_list();

    for (temp_list = plugin_list; temp_list; temp_list = temp_list->next) {
        plugin = (struct plugin_s *) temp_list->data;
        if (plugin) {
            if (plugin->plugin_exit_cleanup) {
                jp_logf(JP_LOG_DEBUG, "calling plugin_exit_cleanup\n");
                plugin->plugin_exit_cleanup();
            }
        }
    }
#endif

    if (glob_child_pid) {
        jp_logf(JP_LOG_DEBUG, "killing %d\n", glob_child_pid);
        kill(glob_child_pid, SIGTERM);
    }
    /* Save preferences in jpilot.rc */
    pref_write_rc_file();

    if (glob_sqlite) jpsqlite_close();
    else cleanup_pc_files();

    cleanup_pidfile();

    gtk_main_quit();
}

static void cb_output(GtkWidget *widget, gpointer data) {
    int flags;
    int h, output_height;

    flags = GPOINTER_TO_INT(data);

    if ((flags == OUTPUT_MINIMIZE) || (flags == OUTPUT_RESIZE)) {
        jp_logf(JP_LOG_DEBUG, "paned pos = %d\n", gtk_paned_get_position(GTK_PANED(output_pane)));
        h = gdk_window_get_height(gtk_widget_get_window(window));
        output_height = (h - gtk_paned_get_position(GTK_PANED(output_pane)));
        set_pref(PREF_OUTPUT_HEIGHT, output_height, NULL, TRUE);
        if (flags == OUTPUT_MINIMIZE) {
            gtk_paned_set_position(GTK_PANED(output_pane), h);
        }
        jp_logf(JP_LOG_DEBUG, "output height = %d\n", output_height);
    }
    if (flags == OUTPUT_CLEAR) {
        gtk_text_buffer_set_text(g_output_text_buffer, "", -1);
    }
}

static gint cb_output_idle(gpointer data) {
    cb_output(NULL, data);
    /* returning false removes this handler from being called again */
    return FALSE;
}

static gint cb_output2(GtkWidget *widget, GdkEventButton *event, gpointer data) {
    /* Because the pane isn't redrawn yet we can't get positions from it.
     * So we have to call back after everything is drawn */
    g_idle_add(cb_output_idle, data);

    return EXIT_SUCCESS;
}

static gint cb_check_version(gpointer main_window) {
    int major, minor, micro;
    int r;
    char str_ver[8];

    jp_logf(JP_LOG_DEBUG, "cb_check_version\n");

    r = sscanf(VERSION, "%d.%d.%d", &major, &minor, &micro);
    if (r != 3) {
        jp_logf(JP_LOG_DEBUG, "couldn't parse VERSION\n");
        return FALSE;
    }
    /* These shouldn't be greater than 100, but just in case */
    major %= 100;
    minor %= 100;
    micro %= 100;

    sprintf(str_ver, "%02d%02d%02d", major, minor, micro);

    set_pref(PREF_VERSION, 0, str_ver, 1);

    return FALSE;
}

int main(int argc, char *argv[]) {
    GtkWidget *main_vbox;
    GtkWidget *temp_hbox;
    GtkWidget *temp_vbox;
    GtkWidget *button_datebook, *button_address, *button_todo, *button_memo;
    GtkWidget *button;
    GtkWidget *separator;
    //gtk2 GtkStyle *style;
    GtkWidget *pixbufwid;
    GdkPixbuf *pixbuf;
    GtkWidget *menubar = NULL;
    GtkWidget *menubar_help = NULL;
    GtkAccelGroup *accel_group;
/* Extract first day of week preference from locale in GTK2 */
    int pref_fdow = 0;
#ifdef HAVE__NL_TIME_FIRST_WEEKDAY
    char *langinfo;
    int week_1stday = 0;
    int first_weekday = 1;
    unsigned int week_origin;
#else
#  ifdef ENABLE_NLS
    char *week_start;
#  endif
#endif
    unsigned char skip_past_alarms;
    unsigned char skip_all_alarms;
    int filedesc[2];
    long ivalue;
    const char *svalue;
    int c, i, height;
    char title[MAX_PREF_LEN + 256];
    long pref_width, pref_height, show_tooltips;
    long char_set;
    char options[1024];
#ifdef PARSE_GEOMETRY
    char *geometry_str = NULL;
#endif
    int iconify = 0;
#ifdef ENABLE_PLUGINS
    GList *plugin_list;
    GList *temp_list;
    struct plugin_s *plugin;
    jp_startup_info info;
#endif
    int pid;
    int remote_sync = FALSE;

    skip_plugins = FALSE;
    skip_past_alarms = FALSE;
    skip_all_alarms = FALSE;
    /* log all output to a file */
    glob_log_file_mask = JP_LOG_INFO | JP_LOG_WARN | JP_LOG_FATAL | JP_LOG_STDOUT;
    glob_log_stdout_mask = JP_LOG_INFO | JP_LOG_WARN | JP_LOG_FATAL | JP_LOG_STDOUT;
    glob_log_gui_mask = JP_LOG_FATAL | JP_LOG_WARN | JP_LOG_GUI;
    glob_find_id = 0;

	/* parse command line options before anything else */
	while ((c = getopt(argc,argv,"AadgiprSsv?")) != -1) {
		switch (c) {
			case 'h':	// help
			case '?':
				fprint_usage_string(stderr);
				exit(0);
			case 'A':	// ignore all alarms
				skip_all_alarms = TRUE;
				jp_logf(JP_LOG_INFO, _("Ignoring all alarms.\n"));
				break;
			case 'a':	// ignore missed alarms
				skip_past_alarms = TRUE;
				jp_logf(JP_LOG_INFO, _("Ignoring past alarms.\n"));
				break;
			case 'd':	// display debug info to stdout
				glob_log_stdout_mask = 0xFFFF;
				glob_log_file_mask = 0xFFFF;
				jp_logf(JP_LOG_DEBUG, "Debug messages on.\n");
				break;
			case 'g':	// geometry
#ifdef PARSE_GEOMETRY
				geometry_str = optarg;
#endif
				fprintf(stdout, "Geometry handling in GTK is deprecated as of version 3.20\n"
					"J-Pilot can be compiled with define PARSE_GEOMETRY to use it if its available.\n");
				break;
			case 'i':	// iconify after launch
				iconify = 1;
				break;
			case 'p':	// no plugins loaded
				skip_plugins = TRUE;
				jp_logf(JP_LOG_INFO, _("Not loading plugins.\n"));
				break;
			case 'r':	// no writing to rc-file or PREF table
				glob_rc_file_write = 0;
				break;
			case 'S':	// store data in SQLite
				//jp_logf(JP_LOG_INFO, _("Using SQLite3.\n"));	// Can also seen when there is no HotSync button
				glob_sqlite = TRUE;
				break;
			case 's':	// initiate a HotSync on running instance
				remote_sync = TRUE;
				break;
			case 'v':	// show version
				get_compile_options(options, sizeof(options));
				printf("\n%s\n", options);
				exit(0);
			default:
				 fprintf(stderr,"%s: unknown option %c\n", argv[0], c);
				 break;
		}
	}
#if 0
    for (i = 1; i < argc; i++) {
        if (!strncasecmp(argv[i], "-v", 3)) {
            char options[1024];
            get_compile_options(options, sizeof(options));
            printf("\n%s\n", options);
            exit(0);
        }
        if ((!strncasecmp(argv[i], "-h", 3)) ||
            (!strncasecmp(argv[i], "-?", 3))) {
            fprint_usage_string(stderr);
            exit(0);
        }
        if (!strncasecmp(argv[i], "-d", 3)) {
            glob_log_stdout_mask = 0xFFFF;
            glob_log_file_mask = 0xFFFF;
            jp_logf(JP_LOG_DEBUG, "Debug messages on.\n");
        }
        if (!strncasecmp(argv[i], "-p", 3)) {
            skip_plugins = TRUE;
            jp_logf(JP_LOG_INFO, _("Not loading plugins.\n"));
        }
        if (!strncmp(argv[i], "-A", 3)) {
            skip_all_alarms = TRUE;
            jp_logf(JP_LOG_INFO, _("Ignoring all alarms.\n"));
        }
        if (!strncmp(argv[i], "-a", 3)) {
            skip_past_alarms = TRUE;
            jp_logf(JP_LOG_INFO, _("Ignoring past alarms.\n"));
        }
        if ((!strncmp(argv[i], "-s", 3)) ||
            (!strncmp(argv[i], "--remote-sync", 14))) {
            remote_sync = TRUE;
        }
        if (!strcmp(argv[i],"-S") || !strcmp(argv[i],"--sqlite")) {
            jp_logf(JP_LOG_INFO, _("Using SQLite3 storage method.\n"));
            glob_sqlite = TRUE;
        }
        if ((!strncasecmp(argv[i], "-i", 3)) ||
            (!strncasecmp(argv[i], "--iconic", 9))) {
            iconify = 1;
        }
        if (!strncasecmp(argv[i], "-geometry", 9)) {
#ifdef PARSE_GEOMETRY
            /* The '=' isn't specified in `man X`, but we will be nice */
            if (argv[i][9] == '=') {
                geometry_str = argv[i] + 9;
            } else {
                if (i < argc) {
                    geometry_str = argv[i + 1];
                }
            }
#endif
            fprintf(stdout, "Geometry handling in GTK is deprecated as of version 3.20\n");
            fprintf(stdout, "J-Pilot can be compiled with define PARSE_GEOMETRY to use it if its available.\n");
        }
    }
#endif

    /* Directory ~/.jpilot is created with permissions of 700 to prevent anyone
     * but the user from looking at potentially sensitive files.
     * Files within the directory have permission 600 */
    umask(0077);

    /* enable internationalization(i18n) before printing any output */
#if defined(ENABLE_NLS)
#  ifdef HAVE_LOCALE_H
    setlocale(LC_ALL, "");
#  endif
    bindtextdomain(EPN, LOCALEDIR);
    textdomain(EPN);
#endif

    pref_init();

	if (glob_sqlite) {
		if (jpsqlite_open()) {
			jp_logf(JP_LOG_FATAL, _("Unable to open SQLite database\n"));
			exit(-1);
		}
		if (jpsqlite_prepareAllStmt()) {
			jp_logf(JP_LOG_FATAL, _("Cannot prepare SQLite statements\n"));
			exit(-1);
		}
	}

    /* read jpilot.rc file for preferences */
    pref_read_rc_file();

    /* Extract first day of week preference from locale in GTK2 */
#  ifdef HAVE__NL_TIME_FIRST_WEEKDAY
    /* GTK 2.8 libraries */
    langinfo = nl_langinfo(_NL_TIME_FIRST_WEEKDAY);
    first_weekday = langinfo[0];
    langinfo = nl_langinfo(_NL_TIME_WEEK_1STDAY);
    week_origin = GPOINTER_TO_INT(langinfo);
    if (week_origin == 19971130)      /* Sunday */
        week_1stday = 0;
    else if (week_origin == 19971201) /* Monday */
        week_1stday = 1;
    else
        g_warning ("Unknown value of _NL_TIME_WEEK_1STDAY.\n");

    pref_fdow = (week_1stday + first_weekday - 1) % 7;
#  else
    /* GTK 2.6 libraries */
#     if defined(ENABLE_NLS)
       week_start = dgettext("gtk20", "calendar:week_start:0");
       if (strncmp("calendar:week_start:", week_start, 20) == 0) {
          pref_fdow = *(week_start + 20) - '0';
       } else {
          pref_fdow = -1;
       }
#     endif
#  endif
    if (pref_fdow > 1)
        pref_fdow = 1;
    if (pref_fdow < 0)
        pref_fdow = 0;

    set_pref_possibility(PREF_FDOW, pref_fdow, TRUE);

    if (otherconv_init()) {
        printf("Error: could not set character encoding\n");
        exit(0);
    }

    get_pref(PREF_WINDOW_WIDTH, &pref_width, NULL);
    get_pref(PREF_WINDOW_HEIGHT, &pref_height, NULL);

    // previously argv-handling

    /* Enable UTF8 *AFTER* potential printf to stdout for -h or -v */
    /* Not all terminals(xterm, rxvt, etc.) are UTF8 compliant */
#if defined(ENABLE_NLS)
    /* generate UTF-8 strings from gettext() & _() */
    bind_textdomain_codeset(EPN, "UTF-8");
#endif

    /* Check to see if ~/.jpilot is there, or create it */
    jp_logf(JP_LOG_DEBUG, "calling check_hidden_dir\n");
    if (check_hidden_dir()) {
        exit(1);
    }

    /* Setup the pid file and check for a running jpilot */
    setup_pidfile();
    pid = check_for_jpilot();

    if (remote_sync) {
        if (pid) {
            printf("jpilot: syncing jpilot at %d\n", pid);
            kill(pid, SIGUSR1);
            exit(0);
        } else {
            fprintf(stderr, "%s\n", "J-Pilot not running.");
            exit(1);
        }
    } else if (!pid) {
        /* JPilot not running, install signal handler and write pid file */
        signal(SIGUSR1, sync_sig_handler);
        write_pid();
    }

    /* Check to see if DB files are there */
    /* If not copy some empty ones over */
    if (!glob_sqlite) check_copy_DBs_to_home();

#ifdef ENABLE_PLUGINS
    plugin_list = NULL;
    if (!skip_plugins) {
        load_plugins();
    }
    plugin_list = get_plugin_list();

    for (temp_list = plugin_list; temp_list; temp_list = temp_list->next) {
        plugin = (struct plugin_s *) temp_list->data;
        jp_logf(JP_LOG_DEBUG, "plugin: [%s] was loaded\n", plugin->name);
        if (plugin) {
            if (plugin->plugin_startup) {
                info.base_dir = strdup(BASE_DIR);
                jp_logf(JP_LOG_DEBUG, "calling plugin_startup for [%s]\n", plugin->name);
                plugin->plugin_startup(&info);
                if (info.base_dir) {
                    free(info.base_dir);
                }
            }
        }
    }
#endif

    glob_date_timer_tag = 0;
    glob_child_pid = 0;

    /* Create a pipe to send data from sync child to parent */
    if (pipe(filedesc) < 0) {
        jp_logf(JP_LOG_FATAL, _("Unable to open pipe\n"));
        exit(-1);
    }
    pipe_from_child = filedesc[0];
    pipe_to_parent = filedesc[1];

    /* Create a pipe to send data from parent to sync child */
    if (pipe(filedesc) < 0) {
        jp_logf(JP_LOG_FATAL, _("Unable to open pipe\n"));
        exit(-1);
    }
    pipe_from_parent = filedesc[0];
    pipe_to_child = filedesc[1];

    get_pref(PREF_CHAR_SET, &char_set, NULL);
    /* Deprecated in GTK3 */
#if 0
    switch (char_set) {
        case CHAR_SET_JAPANESE:
            gtk_rc_parse("gtkrc.ja");
            break;
        case CHAR_SET_TRADITIONAL_CHINESE:
            gtk_rc_parse("gtkrc.zh_TW.Big5");
            break;
        case CHAR_SET_KOREAN:
            gtk_rc_parse("gtkrc.ko");
            break;
            /* Since Now, these are not supported yet. */
#if 0
            case CHAR_SET_SIMPLIFIED_CHINESE:
              gtk_rc_parse("gtkrc.zh_CN");
              break;
            case CHAR_SET_1250:
              gtk_rc_parse("gtkrc.???");
              break;
            case CHAR_SET_1251:
              gtk_rc_parse("gtkrc.iso-8859-5");
              break;
            case CHAR_SET_1251_B:
              gtk_rc_parse("gtkrc.ru");
              break;
#endif
        default:
            break;
            /* do nothing */
    }
#endif

    jpilot_master_pid = getpid();

    gtk_init(&argc, &argv);

    read_gtkrc_file();

    get_pref(PREF_USER, NULL, &svalue);

    strcpy(title, PN" "VERSION);
    if ((svalue) && (svalue[0])) {
        strcat(title, _(" User: "));
        /* Convert user name so that it can be displayed in window title */
        /* We assume user name is coded in jpilot.rc as it is on the Palm Pilot */
        {
            char *newvalue;

            newvalue = charset_p2newj(svalue, -1, char_set);
            strcat(title, newvalue);
            free(newvalue);
        }
    }

    window = gtk_widget_new(GTK_TYPE_WINDOW,
                            "type", GTK_WINDOW_TOPLEVEL,
                            "title", title,
                            NULL);

    /* Set default size and position of main window */
#ifdef PARSE_GEOMETRY
    int ret = 0;
    if (geometry_str) {
        ret = gtk_window_parse_geometry(GTK_WINDOW(window), geometry_str);
    }
    if ((!geometry_str) || (ret != 1)) {
        gtk_window_set_default_size(GTK_WINDOW(window), pref_width, pref_height);
    }
#else
    gtk_window_set_default_size(GTK_WINDOW(window), pref_width, pref_height);
#endif
    if (iconify) {
        gtk_window_iconify(GTK_WINDOW(window));
    }

    /* Set a handler for delete_event that immediately exits GTK. */
    g_signal_connect(G_OBJECT(window), "delete_event",
                       G_CALLBACK(cb_delete_event), NULL);

    gtk_container_set_border_width(GTK_CONTAINER(window), 0);

    main_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    g_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    g_vbox0 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    /* Output Pane */
    output_pane = gtk_paned_new(GTK_ORIENTATION_VERTICAL);

    g_signal_connect(G_OBJECT(output_pane), "button_release_event",
                       G_CALLBACK(cb_output2),
                       GINT_TO_POINTER(OUTPUT_RESIZE));

    gtk_container_add(GTK_CONTAINER(window), output_pane);

    gtk_paned_pack1(GTK_PANED(output_pane), main_vbox, FALSE, FALSE);

    /* Create the Menu Bar at the top */
#ifdef ENABLE_PLUGINS
    get_main_menu(window, &menubar, &menubar_help, plugin_list);
#else
    get_main_menu(window, &menubar, &menubar_help, NULL);
#endif

    temp_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(main_vbox), temp_hbox, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(temp_hbox), menubar, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(temp_hbox), menubar_help, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(main_vbox), g_hbox, TRUE, TRUE, 3);
    gtk_container_set_border_width(GTK_CONTAINER(g_hbox), 10);
    gtk_box_pack_start(GTK_BOX(g_hbox), g_vbox0, FALSE, FALSE, 3);

    /* Output Text scrolled window */
    temp_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_set_border_width(GTK_CONTAINER(temp_hbox), 5);
    gtk_paned_pack2(GTK_PANED(output_pane), temp_hbox, FALSE, TRUE);

    temp_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
    gtk_container_set_border_width(GTK_CONTAINER(temp_vbox), 6);
    gtk_box_pack_end(GTK_BOX(temp_hbox), temp_vbox, FALSE, FALSE, 0);

    g_output_text = GTK_TEXT_VIEW(gtk_text_view_new());
    g_output_text_buffer = gtk_text_view_get_buffer(g_output_text);
    gtk_text_view_set_cursor_visible(g_output_text, FALSE);
    gtk_text_view_set_editable(g_output_text, FALSE);
    gtk_text_view_set_wrap_mode(g_output_text, GTK_WRAP_WORD);

    g_output_text_scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(g_output_text_scrolled_window),
                                   GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);

    gtk_container_add(GTK_CONTAINER(g_output_text_scrolled_window), GTK_WIDGET(g_output_text));
    gtk_box_pack_start(GTK_BOX(temp_hbox), GTK_WIDGET(g_output_text_scrolled_window), TRUE, TRUE,0);

    button = gtk_button_new_with_label("Clear");
    gtk_box_pack_start(GTK_BOX(temp_vbox), button, TRUE, TRUE, 3);
    g_signal_connect(G_OBJECT(button), "clicked",
                       G_CALLBACK(cb_output),
                       GINT_TO_POINTER(OUTPUT_CLEAR));

    button = gtk_button_new_with_label("Remove");
    gtk_box_pack_start(GTK_BOX(temp_vbox), button, TRUE, TRUE, 3);
    g_signal_connect(G_OBJECT(button), "clicked",
                       G_CALLBACK(cb_output),
                       GINT_TO_POINTER(OUTPUT_MINIMIZE));

    /* "Datebook" button */
    temp_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    button_datebook = gtk_button_new();
    g_signal_connect(G_OBJECT(button_datebook), "clicked",
                       G_CALLBACK(cb_app_button), GINT_TO_POINTER(DATEBOOK));
    gtk_box_pack_start(GTK_BOX(g_vbox0), temp_hbox, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(temp_hbox), button_datebook, TRUE, FALSE, 0);

    /* "Address" button */
    temp_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    button_address = gtk_button_new();
    g_signal_connect(G_OBJECT(button_address), "clicked",
                       G_CALLBACK(cb_app_button), GINT_TO_POINTER(ADDRESS));
    gtk_box_pack_start(GTK_BOX(g_vbox0), temp_hbox, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(temp_hbox), button_address, TRUE, FALSE, 0);

    /* "Todo" button */
    temp_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    button_todo = gtk_button_new();
    g_signal_connect(G_OBJECT(button_todo), "clicked",
                       G_CALLBACK(cb_app_button), GINT_TO_POINTER(TODO));
    gtk_box_pack_start(GTK_BOX(g_vbox0), temp_hbox, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(temp_hbox), button_todo, TRUE, FALSE, 0);

    /* "Memo" button */
    temp_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    button_memo = gtk_button_new();
    g_signal_connect(G_OBJECT(button_memo), "clicked",
                       G_CALLBACK(cb_app_button), GINT_TO_POINTER(MEMO));
    gtk_box_pack_start(GTK_BOX(g_vbox0), temp_hbox, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(temp_hbox), button_memo, TRUE, FALSE, 0);

    gtk_widget_set_name(button_datebook, "button_app");
    gtk_widget_set_name(button_address, "button_app");
    gtk_widget_set_name(button_todo, "button_app");
    gtk_widget_set_name(button_memo, "button_app");


    /* Get preference to show tooltips */
    get_pref(PREF_SHOW_TOOLTIPS, &show_tooltips, NULL);

    /* Create key accelerator */
    accel_group = gtk_accel_group_new();
    gtk_window_add_accel_group(GTK_WINDOW(window), accel_group);

    /* Lock/Unlock/Mask buttons */
    button_locked = gtk_button_new();
    button_masklocked = gtk_button_new();
    button_unlocked = gtk_button_new();
    g_signal_connect(G_OBJECT(button_locked), "clicked",
                       G_CALLBACK(cb_private),
                       GINT_TO_POINTER(SHOW_PRIVATES));
    g_signal_connect(G_OBJECT(button_masklocked), "clicked",
                       G_CALLBACK(cb_private),
                       GINT_TO_POINTER(HIDE_PRIVATES));
    g_signal_connect(G_OBJECT(button_unlocked), "clicked",
                       G_CALLBACK(cb_private),
                       GINT_TO_POINTER(MASK_PRIVATES));
    gtk_box_pack_start(GTK_BOX(g_vbox0), button_locked, FALSE, FALSE, 20);
    gtk_box_pack_start(GTK_BOX(g_vbox0), button_masklocked, FALSE, FALSE, 20);
    gtk_box_pack_start(GTK_BOX(g_vbox0), button_unlocked, FALSE, FALSE, 20);

    set_tooltip(show_tooltips,
                button_locked, _("Show private records   Ctrl+Z"));
    gtk_widget_add_accelerator(button_locked, "clicked", accel_group,
                               GDK_KEY_z, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);

    set_tooltip(show_tooltips,
                button_masklocked, _("Hide private records   Ctrl+Z"));
    gtk_widget_add_accelerator(button_masklocked, "clicked", accel_group,
                               GDK_KEY_z, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);

    set_tooltip(show_tooltips,
                button_unlocked, _("Mask private records   Ctrl+Z"));
    gtk_widget_add_accelerator(button_unlocked, "clicked", accel_group,
                               GDK_KEY_z, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);

	if (!glob_sqlite) {
		/* "Sync" button */
		button_sync = gtk_button_new();
		g_signal_connect(G_OBJECT(button_sync), "clicked",
						   G_CALLBACK(cb_sync),
						   GINT_TO_POINTER(skip_plugins ? SYNC_NO_PLUGINS : 0));
		gtk_box_pack_start(GTK_BOX(g_vbox0), button_sync, FALSE, FALSE, 3);

		set_tooltip(show_tooltips,
					button_sync, _("Sync your palm to the desktop   Ctrl+Y"));
		gtk_widget_add_accelerator(button_sync, "clicked", accel_group, GDK_KEY_y,
								   GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);

		/* "Cancel Sync" button */
		button_cancel_sync = gtk_button_new();
		g_signal_connect(G_OBJECT(button_cancel_sync), "clicked",
						   G_CALLBACK(cb_cancel_sync),
						   NULL);
		gtk_box_pack_start(GTK_BOX(g_vbox0), button_cancel_sync, FALSE, FALSE, 3);

		set_tooltip(show_tooltips,
					button_cancel_sync, _("Stop Sync process"));

		/* "Backup" button in left column */
		button_backup = gtk_button_new();
		g_signal_connect(G_OBJECT(button_backup), "clicked",
						   G_CALLBACK(cb_sync),
						   GINT_TO_POINTER
								   (skip_plugins ? SYNC_NO_PLUGINS | SYNC_FULL_BACKUP
												 : SYNC_FULL_BACKUP));
		gtk_box_pack_start(GTK_BOX(g_vbox0), button_backup, FALSE, FALSE, 3);

		set_tooltip(show_tooltips,
					button_backup, _("Sync your palm to the desktop\n"
									 "and then do a backup"));
	}

    /* Separator */
    separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(g_vbox0), separator, FALSE, TRUE, 5);

    /* Creates the 2 main boxes that are changeable */
    create_main_boxes();

    gtk_widget_show_all(window);
    gtk_widget_show(window);

    //gtk2 style = gtk_widget_get_style(window);

    /* Jpilot Icon pixbuf */
    //pixbuf = gdk_pixmap_create_from_xpm_d(window->window, &mask, NULL, jpilot_icon4_xpm);
    //this method no longer exists in gtk3.  X11 handles this anyway.
    // gdk_window_set_icon(window->window, NULL, pixbuf, mask);
    gdk_window_set_icon_name(gtk_widget_get_window(window), PN);

    /* Create "Datebook" pixbuf */
    pixbuf = gdk_pixbuf_new_from_xpm_data((const char **) datebook_xpm);
    pixbufwid = gtk_image_new_from_pixbuf(pixbuf);
    gtk_widget_show(pixbufwid);
    gtk_container_add(GTK_CONTAINER(button_datebook), pixbufwid);
    gtk_button_set_relief(GTK_BUTTON(button_datebook), GTK_RELIEF_NONE);

    /* Create "Address" pixbuf */
    pixbuf = gdk_pixbuf_new_from_xpm_data((const char **) address_xpm);
    pixbufwid = gtk_image_new_from_pixbuf(pixbuf);
    gtk_widget_show(pixbufwid);
    gtk_container_add(GTK_CONTAINER(button_address), pixbufwid);
    gtk_button_set_relief(GTK_BUTTON(button_address), GTK_RELIEF_NONE);

    /* Create "Todo" pixbuf */
    pixbuf = gdk_pixbuf_new_from_xpm_data((const char **) todo_xpm);
    pixbufwid = gtk_image_new_from_pixbuf(pixbuf);
    gtk_widget_show(pixbufwid);
    gtk_container_add(GTK_CONTAINER(button_todo), pixbufwid);
    gtk_button_set_relief(GTK_BUTTON(button_todo), GTK_RELIEF_NONE);

    /* Create "Memo" pixbuf */
    pixbuf = gdk_pixbuf_new_from_xpm_data((const char **) memo_xpm);
    pixbufwid = gtk_image_new_from_pixbuf(pixbuf);
    gtk_widget_show(pixbufwid);
    gtk_container_add(GTK_CONTAINER(button_memo), pixbufwid);
    gtk_button_set_relief(GTK_BUTTON(button_memo), GTK_RELIEF_NONE);

    /* Show locked/unlocked/masked button */
#ifdef ENABLE_PRIVATE
    gtk_widget_show(button_locked);
    gtk_widget_hide(button_masklocked);
    gtk_widget_hide(button_unlocked);
#else
    gtk_widget_hide(button_locked);
    gtk_widget_hide(button_masklocked);
    gtk_widget_show(button_unlocked);
#endif

    /* Create "locked" pixbuf */
    pixbuf = gdk_pixbuf_new_from_xpm_data((const char **) locked_xpm);
    pixbufwid = gtk_image_new_from_pixbuf(pixbuf);
    gtk_widget_show(pixbufwid);
    gtk_container_add(GTK_CONTAINER(button_locked), pixbufwid);

    /* Create "masked" pixbuf */
    pixbuf = gdk_pixbuf_new_from_xpm_data((const char **) masklocked_xpm);
    pixbufwid = gtk_image_new_from_pixbuf(pixbuf);
    gtk_widget_show(pixbufwid);
    gtk_container_add(GTK_CONTAINER(button_masklocked), pixbufwid);

    /* Create "unlocked" pixbuf */
    pixbuf = gdk_pixbuf_new_from_xpm_data((const char **) unlocked_xpm);
    pixbufwid = gtk_image_new_from_pixbuf(pixbuf);
    gtk_widget_show(pixbufwid);
    gtk_container_add(GTK_CONTAINER(button_unlocked), pixbufwid);

    /* Create "sync" pixbuf */
    pixbuf = gdk_pixbuf_new_from_xpm_data((const char **) sync_xpm);
    pixbufwid = gtk_image_new_from_pixbuf(pixbuf);
    gtk_widget_show(pixbufwid);
    gtk_container_add(GTK_CONTAINER(button_sync), pixbufwid);

    /* Create "cancel sync" pixbuf */
    /* Hide until sync process started */
    gtk_widget_hide(button_cancel_sync);
    pixbuf = gdk_pixbuf_new_from_xpm_data((const char **) cancel_sync_xpm);
    pixbufwid = gtk_image_new_from_pixbuf(pixbuf);
    gtk_widget_show(pixbufwid);
    gtk_container_add(GTK_CONTAINER(button_cancel_sync), pixbufwid);


    /* Create "backup" pixbuf */
    gtk_widget_hide(button_cancel_sync);
    pixbuf = gdk_pixbuf_new_from_xpm_data((const char **) backup_xpm);
    pixbufwid = gtk_image_new_from_pixbuf(pixbuf);
    gtk_widget_show(pixbufwid);
    gtk_container_add(GTK_CONTAINER(button_backup), pixbufwid);

    set_tooltip(show_tooltips, button_datebook, _("Datebook/Go to Today"));
    set_tooltip(show_tooltips, button_address, _("Address Book"));
    set_tooltip(show_tooltips, button_todo, _("ToDo List"));
    set_tooltip(show_tooltips, button_memo, _("Memo Pad"));

    /* Set a callback for our pipe from the sync child process */
    GIOChannel *channel = g_io_channel_unix_new(pipe_from_child);
    g_io_add_watch(channel,
                   G_IO_IN | G_IO_HUP | G_IO_ERR,
                   cb_read_pipe_from_child,
                   window);

    //gtk2 gdk_input_add(pipe_from_child, GDK_INPUT_READ, cb_read_pipe_from_child, window);

    get_pref(PREF_LAST_APP, &ivalue, NULL);
    /* We don't want to start up to a plugin because the plugin might
     * repeatedly segfault.  Of course main apps can do that, but since I
     * handle the email support...  */
    if ((ivalue == ADDRESS) ||
        (ivalue == DATEBOOK) ||
        (ivalue == TODO) ||
        (ivalue == MEMO)) {
        cb_app_button(NULL, GINT_TO_POINTER(ivalue));
    }

    /* Set the pane size */
    height = 0;
    // width = gdk_window_get_width(gtk_widget_get_window(window));
    height = gdk_window_get_height(gtk_widget_get_window(window));
    gtk_paned_set_position(GTK_PANED(output_pane), height);

    alarms_init(skip_past_alarms, skip_all_alarms);

    /* In-line code to switch user to UTF-8 encoding.
     * Should probably be made a subroutine */
    {
        long utf_encoding;
        char *button_text[] =
                {N_("Do it now"), N_("Remind me later"), N_("Don't tell me again!")};

        /* check if a UTF-8 one is used */
        if (char_set >= CHAR_SET_UTF)
            set_pref(PREF_UTF_ENCODING, 1, NULL, TRUE);

        get_pref(PREF_UTF_ENCODING, &utf_encoding, NULL);
        if (0 == utf_encoding) {
            /* user has not switched to UTF */
            int ret;
            char text[1000];

            g_snprintf(text, sizeof(text),
                       _("J-Pilot uses the GTK2 graphical toolkit. "
                         "This version of the toolkit uses UTF-8 to encode characters.\n"
                         "You should select a UTF-8 charset so that you can see non-ASCII characters (accents for example).\n"
                         "\n"
                         /* Coded to reuse existing i18n strings */
                         "Go to the menu \"%s\" and change the \"%s\"."), _("/File/Preferences"), _("Character Set"));
            ret = dialog_generic(GTK_WINDOW(window),
                                 _("Select a UTF-8 encoding"), DIALOG_QUESTION, text, 3, button_text);

            switch (ret) {
                case DIALOG_SAID_1:
                    cb_prefs_gui(NULL, window);
                    break;
                case DIALOG_SAID_2:
                    /* Do nothing and remind user at next program invocation */
                    break;
                case DIALOG_SAID_3:
                    set_pref(PREF_UTF_ENCODING, 1, NULL, TRUE);
                    break;
            }
        }
    }

    g_idle_add(cb_check_version, window);

    gtk_main();

    otherconv_free();

    return EXIT_SUCCESS;
}

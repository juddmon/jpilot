/* $Id: jpilot.c,v 1.194 2010/11/10 03:57:46 rikster5 Exp $ */

/*******************************************************************************
 * jpilot.c
 * A module of J-Pilot http://jpilot.org
 *
 * Copyright (C) 1999-2003 by Judd Montgomery
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

/* #define PIPE_DEBUG */
/******************************* Global vars **********************************/
/* Application-wide globals */
int pipe_from_child, pipe_to_parent;
int pipe_from_parent, pipe_to_child;
/* Main GTK window for application */
GtkWidget *window;
GtkWidget *glob_date_label;
GtkTooltips *glob_tooltips;
GtkWidget *glob_dialog=NULL;
int glob_app = 0;
unsigned char skip_plugins;
gint glob_date_timer_tag;
pid_t glob_child_pid;
pid_t jpilot_master_pid;

/* jpilot.c file globals */
static GtkWidget *g_hbox, *g_vbox0;
static GtkWidget *g_hbox2, *g_vbox0_1;
static GtkTextView *g_output_text;
static GtkTextBuffer *g_output_text_buffer;
static GtkWidget *output_pane;
static GtkWidget *button_locked;
static GtkWidget *button_masklocked;
static GtkWidget *button_unlocked;
static GtkWidget *button_sync;
static GtkWidget *button_cancel_sync;
static GtkWidget *button_backup;
static GtkCheckMenuItem *menu_hide_privates;
static GtkCheckMenuItem *menu_show_privates;
static GtkCheckMenuItem *menu_mask_privates;

extern GtkWidget *weekview_window;
extern GtkWidget *monthview_window;

/****************************** Prototypes ************************************/
static void cb_delete_event(GtkWidget *widget, GdkEvent *event, gpointer data);
static void install_gui_and_size(GtkWidget *main_window);

/****************************** Main Code *************************************/

static int create_main_boxes(void)
{
   g_hbox2 = gtk_hbox_new(FALSE, 0);
   g_vbox0_1 = gtk_vbox_new(FALSE, 0);

   gtk_box_pack_start(GTK_BOX(g_hbox), g_hbox2, TRUE, TRUE, 0);
   gtk_box_pack_start(GTK_BOX(g_vbox0), g_vbox0_1, FALSE, FALSE, 0);
   return EXIT_SUCCESS;
}

static int gui_cleanup(void)
{
#ifdef ENABLE_PLUGINS
   struct plugin_s *plugin;
   GList *plugin_list, *temp_list;
#endif

#ifdef ENABLE_PLUGINS
   plugin_list = NULL;
   plugin_list = get_plugin_list();

   /* Find out which (if any) plugin to call a gui_cleanup on */
   for (temp_list = plugin_list; temp_list; temp_list = temp_list->next) {
      plugin = (struct plugin_s *)temp_list->data;
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
void call_plugin_gui(int number, int unique_id)
{
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
      gtk_timeout_remove(glob_date_timer_tag);
   }

   /* Find out which plugin we are calling */
   for (temp_list = plugin_list; temp_list; temp_list = temp_list->next) {
      plugin = (struct plugin_s *)temp_list->data;
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

static void cb_plugin_gui(GtkWidget *widget, int number)
{
   call_plugin_gui(number, 0);
}

/* Redraws plugin GUI after structure changing event such as category editing */
void plugin_gui_refresh(int unique_id)
{
   call_plugin_gui(glob_app, unique_id); 
}

static void call_plugin_help(int number)
{
   struct plugin_s *plugin;
   GList *plugin_list, *temp_list;
   char *button_text[]={N_("OK")};
   char *text;
   int width, height;

   if (!number) {
      return;
   }

   plugin_list = NULL;
   plugin_list = get_plugin_list();

   /* Find out which plugin we are calling */
   for (temp_list = plugin_list; temp_list; temp_list = temp_list->next) {
      plugin = (struct plugin_s *)temp_list->data;
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

static void cb_plugin_help(GtkWidget *widget, int number)
{
   call_plugin_help(number);
}

#endif

static void cb_print(GtkWidget *widget, gpointer data)
{
#ifdef ENABLE_PLUGINS
   struct plugin_s *plugin;
   GList *plugin_list, *temp_list;
#endif
   char *button_text[]={N_("OK")};

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
      plugin = (struct plugin_s *)temp_list->data;
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

static void cb_restore(GtkWidget *widget, gpointer data)
{
   int r;
   int w, h, x, y;

   jp_logf(JP_LOG_DEBUG, "cb_restore()\n");

   gdk_window_get_size(window->window, &w, &h);
   gdk_window_get_root_origin(window->window, &x, &y);

   w = w/2;
   x+=40;

   r = restore_gui(window, w, h, x, y);

   /* Fork successful, child sync process started */
   if (glob_child_pid && (r == EXIT_SUCCESS)) {
      gtk_widget_hide(button_sync);
      gtk_widget_show(button_cancel_sync);
   }
}

static void cb_import(GtkWidget *widget, gpointer data)
{
#ifdef ENABLE_PLUGINS
   struct plugin_s *plugin;
   GList *plugin_list, *temp_list;
#endif
   char *button_text[]={N_("OK")};

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
      plugin = (struct plugin_s *)temp_list->data;
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

static void cb_export(GtkWidget *widget, gpointer data)
{
#ifdef ENABLE_PLUGINS
   struct plugin_s *plugin;
   GList *plugin_list, *temp_list;
#endif
   char *button_text[]={N_("OK")};

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
      plugin = (struct plugin_s *)temp_list->data;
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

static void cb_private(GtkWidget *widget, gpointer data)
{
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
      break;
    case HIDE_PRIVATES:
      gtk_widget_show(button_locked);
      gtk_widget_hide(button_masklocked);
      gtk_widget_hide(button_unlocked);
      break;
    case SHOW_PRIVATES:
      /* Ask for the password, or don't depending on configure option */
#ifdef ENABLE_PRIVATE
      memset(ascii_password, 0, sizeof(ascii_password)); 
      if (was_privates != SHOW_PRIVATES) {
         retry=FALSE;
         do {
            r_dialog = dialog_password(GTK_WINDOW(window),
                                       ascii_password, retry);
            r_pass = verify_password(ascii_password);
            retry=TRUE;
         } while ((r_pass==FALSE) && (r_dialog==2));
      }
#else
      r_dialog = 2;
#endif
      if (r_dialog==2) {
         gtk_widget_hide(button_locked);
         gtk_widget_hide(button_masklocked);
         gtk_widget_show(button_unlocked);
      }
      else {
         /* wrong or canceled password, hide the entries */
         gtk_check_menu_item_set_active(menu_hide_privates, TRUE);
         cb_private(NULL, GINT_TO_POINTER(HIDE_PRIVATES));
         return;
      }
      break;
   }

   if (widget) {
      /* Setting the state of the menu causes a signal to be emitted 
       * which calls cb_private again.
       * This second call needs to be ignored */
      skip_false_call = 1;
      switch (privates) {
       case MASK_PRIVATES:
         gtk_check_menu_item_set_active(menu_mask_privates, TRUE);
         break;
       case HIDE_PRIVATES:
         gtk_check_menu_item_set_active(menu_hide_privates, TRUE);
         break;
       case SHOW_PRIVATES:
         gtk_check_menu_item_set_active(menu_show_privates, TRUE);
         break;
      }
   }

   if (was_privates != privates)
      cb_app_button(NULL, GINT_TO_POINTER(REDRAW));
}

static void cb_install_user(GtkWidget *widget, gpointer data)
{
   int r;

   r = install_user_gui(window);

   /* Fork successful, child sync process started */
   if (glob_child_pid && (r == EXIT_SUCCESS)) {
      gtk_widget_hide(button_sync);
      gtk_widget_show(button_cancel_sync);
   }
}

void cb_app_button(GtkWidget *widget, gpointer data)
{
   int app;
   int refresh;

   app = GPOINTER_TO_INT(data);

   /* If the current and selected apps are the same then just refresh screen */
   refresh = (app==glob_app);  

   /* Tear down GUI when switching apps or on forced REDRAW */
   if ((!refresh) || (app==REDRAW)) {
      gui_cleanup();
      if (glob_date_timer_tag) {
         gtk_timeout_remove(glob_date_timer_tag);
      }
      gtk_widget_destroy(g_vbox0_1);
      gtk_widget_destroy(g_hbox2);
      create_main_boxes();
      if (app==REDRAW) {
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
      if ((glob_app==DATEBOOK) ||
          (glob_app==ADDRESS) ||
          (glob_app==TODO) ||
          (glob_app==MEMO) )
         cb_app_button(NULL, GINT_TO_POINTER(glob_app));
      break;
   }
}

static void sync_sig_handler (int sig)
{
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

static void cb_sync(GtkWidget *widget, unsigned int flags)
{
   long ivalue;
   int r;

   /* confirm file installation */
   get_pref(PREF_CONFIRM_FILE_INSTALL, &ivalue, NULL);
   if (ivalue)
   {
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

void cb_cancel_sync(GtkWidget *widget, unsigned int flags)
{
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
static int bad_sync_exit_status(int exit_status)
{
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
   char *button_text[]={N_("Cancel Sync"), N_("Sync Anyway")
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

void output_to_pane(const char *str)
{
   int w, h, new_y;
   long ivalue;
   GtkWidget *pane_hbox;
   GtkRequisition size_requisition;

   /* Adjust window height to user preference or minimum size */
   get_pref(PREF_OUTPUT_HEIGHT, &ivalue, NULL);
   /* Make them look at something if output happens */
   if (ivalue < 50) {
      /* Ask GTK for size which is just large enough to show both buttons */
      pane_hbox = gtk_paned_get_child2(GTK_PANED(output_pane));
      gtk_widget_size_request(pane_hbox, &size_requisition);
      ivalue = size_requisition.height + 1;
      set_pref(PREF_OUTPUT_HEIGHT, ivalue, NULL, TRUE);
   }
   gdk_window_get_size(window->window, &w, &h);
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
   sbar_value = g_output_text->vadjustment->value;
   sbar_page_size = g_output_text->vadjustment->page_size;
   sbar_upper = g_output_text->vadjustment->upper;
   /* Keep scrolling to the end only if we are already near(1 window) of the end
    * OR the window has just been created and is blank */
   if ((abs((sbar_value+sbar_page_size) - sbar_upper) < sbar_page_size) 
       || sbar_page_size == 1) {
      scroll_to_end = TRUE;
   }

   gtk_text_buffer_get_end_iter(g_output_text_buffer, &end_iter);

   if (! g_utf8_validate(str, -1, NULL)) {
      gchar *utf8_text;

      utf8_text = g_locale_to_utf8 (str, -1, NULL, NULL, NULL);
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

static void cb_read_pipe_from_child(gpointer data,
                                    gint in,
                                    GdkInputCondition condition)
{
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
   char title[MAX_PREF_LEN+256];
   char *user_name;

   /* This is so we can always look at the previous char in buf */
   buf = &buf_space[1];
   buf[-1]='A'; /* that looks weird */

   done=0;
   while (!done) {
      buf[0]='\0';
      buf_len=0;
      /* Read until "\0\n", or buffer full */
      for (i=0; i<1022; i++) {
         buf[i]='\0';
         /* Linux modifies tv in the select call */
         tv.tv_sec=0;
         tv.tv_usec=0;
         FD_ZERO(&fds);
         FD_SET(in, &fds);
         ret=select(in+1, &fds, NULL, NULL, &tv);
         if ((ret<1) || (!FD_ISSET(in, &fds))) {
            done=1; break;
         }
         ret = read(in, &(buf[i]), 1);
         if (ret <= 0) {
            done=1; break;
         }
         if ((buf[i-1]=='\0')&&(buf[i]=='\n')) {
            buf_len=buf_len-1;
            break;
         }
         buf_len++;
         if (buf_len >= 1022) {
            buf[buf_len]='\0';
            break;
         }
      }

      if (buf_len < 1) break;

      /* Look for the command */
      command=0;
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
                  g_strlcpy(user, Pstr2, user_len+1);
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
                  g_strlcpy(password, Pstr2, password_len+1);
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
               write(pipe_to_child, command_str, strlen(command_str));
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
}

static void cb_about(GtkWidget *widget, gpointer data)
{
   char *button_text[]={N_("OK")};
   char about[256];
   char options[1024];
   int w, h;

   gdk_window_get_size(window->window, &w, &h);

   w = w/2;
   h = 1;

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

static void cb_web(GtkWidget *widget, gpointer data)
{
   int sel;

   sel=GPOINTER_TO_INT(data);
   jp_logf(JP_LOG_INFO, PN": executing %s\n", url_commands[sel].command);
   system(url_commands[sel].command);
}

#endif

static void install_gui_and_size(GtkWidget *main_window)
{
   int w, h, x, y;

   gdk_window_get_size(window->window, &w, &h);
   gdk_window_get_root_origin(window->window, &x, &y);

   w = w/2;
   x+=40;

   install_gui(main_window, w, h, x, y);
}

static void cb_install_gui(GtkWidget *widget, gpointer data)
{
   jp_logf(JP_LOG_DEBUG, "cb_install_gui()\n");

   install_gui_and_size(window);
}

#include <gdk-pixbuf/gdk-pixdata.h>

static guint8 *get_inline_pixbuf_data(const char **xpm_icon_data, 
                                      gint icon_size)
{
   GdkPixbuf  *pixbuf;
   GdkPixdata *pixdata;
   GdkPixbuf  *scaled_pb;
   guint8     *data;
   guint       len;

   pixbuf = gdk_pixbuf_new_from_xpm_data(xpm_icon_data);
   if (!pixbuf)
      return NULL;

   if (gdk_pixbuf_get_width(pixbuf)  != icon_size ||
       gdk_pixbuf_get_height(pixbuf) != icon_size) {
      scaled_pb = gdk_pixbuf_scale_simple(pixbuf, icon_size, icon_size,
                                          GDK_INTERP_BILINEAR);
      gdk_pixbuf_unref(pixbuf);
      pixbuf = scaled_pb;
   }

   pixdata = (GdkPixdata*)g_malloc(sizeof(GdkPixdata));
   gdk_pixdata_from_pixbuf(pixdata, pixbuf, FALSE);
   data = gdk_pixdata_serialize(pixdata, &len);

   gdk_pixbuf_unref(pixbuf);
   g_free(pixdata);

   return data;
}

static void get_main_menu(GtkWidget  *my_window,
                          GtkWidget **menubar,
                          GList *plugin_list)
{
#define ICON(icon) "<StockItem>", icon
#define ICON_XPM(icon, size) "<ImageItem>", get_inline_pixbuf_data(icon, size)

   GtkItemFactoryEntry menu_items1[]={
   { _("/_File"),                           NULL,         NULL,           0,                  "<Branch>", NULL },
   { _("/File/tear"),                       NULL,         NULL,           0,                  "<Tearoff>", NULL },
   { _("/File/_Find"),                      "<control>F", cb_search_gui,  0,                  ICON(GTK_STOCK_FIND) },
   { _("/File/sep1"),                       NULL,         NULL,           0,                  "<Separator>", NULL },
   { _("/File/_Install"),                   "<control>I", cb_install_gui, 0,                  ICON(GTK_STOCK_OPEN) },
   { _("/File/Import"),                     NULL,         cb_import,      0,                  ICON(GTK_STOCK_GO_FORWARD) },
   { _("/File/Export"),                     NULL,         cb_export,      0,                  ICON(GTK_STOCK_GO_BACK) },
   { _("/File/Preferences"),                "<control>S", cb_prefs_gui,   0,                  ICON(GTK_STOCK_PREFERENCES) },
   { _("/File/_Print"),                     "<control>P", cb_print,       0,                  ICON(GTK_STOCK_PRINT) },
   { _("/File/sep1"),                       NULL,         NULL,           0,                  "<Separator>", NULL },
   { _("/File/Install User"),               NULL,         cb_install_user,0,                  ICON_XPM(user_icon, 16) },
   { _("/File/Restore Handheld"),           NULL,         cb_restore,     0,                  ICON(GTK_STOCK_REDO) },
   { _("/File/sep1"),                       NULL,         NULL,           0,                  "<Separator>", NULL },
   { _("/File/_Quit"),                      "<control>Q", cb_delete_event,0,                  ICON(GTK_STOCK_QUIT) },
   { _("/_View"),                           NULL,         NULL,           0,                  "<Branch>", NULL },
   { _("/View/Hide Private Records"),       NULL,         cb_private,     HIDE_PRIVATES,      "<RadioItem>", NULL },
   { _("/View/Show Private Records"),       NULL,         cb_private,     SHOW_PRIVATES,      _("/View/Hide Private Records"), NULL },
   { _("/View/Mask Private Records"),       NULL,         cb_private,     MASK_PRIVATES,      _("/View/Hide Private Records"), NULL },
   { _("/View/sep1"),                       NULL,         NULL,           0,                  "<Separator>", NULL },
   { _("/View/Datebook"),                   "F1",         cb_app_button,  DATEBOOK,           ICON_XPM(date_menu_icon, 16) },
   { _("/View/Addresses"),                  "F2",         cb_app_button,  ADDRESS,            ICON_XPM(addr_menu_icon, 16) },
   { _("/View/Todos"),                      "F3",         cb_app_button,  TODO,               ICON_XPM(todo_menu_icon, 14) },
   { _("/View/Memos"),                      "F4",         cb_app_button,  MEMO,               ICON(GTK_STOCK_JUSTIFY_LEFT) },
   { _("/_Plugins"),                        NULL,         NULL,           0,                  "<Branch>", NULL },
#ifdef WEBMENU
   { _("/_Web"),                            NULL,         NULL,           0,                  "<Branch>", NULL },/* web */
   { _("/Web/Netscape"),                    NULL,         NULL,           0,                  "<Branch>", NULL },
   { url_commands[NETSCAPE_EXISTING].desc,  NULL,         cb_web,         NETSCAPE_EXISTING,  NULL, NULL },
   { url_commands[NETSCAPE_NEW_WINDOW].desc,NULL,         cb_web,         NETSCAPE_NEW_WINDOW,NULL, NULL },
   { url_commands[NETSCAPE_NEW].desc,       NULL,         cb_web,         NETSCAPE_NEW,       NULL, NULL },
   { _("/Web/Mozilla"),                     NULL,         NULL,           0,                  "<Branch>", NULL },
   { url_commands[MOZILLA_EXISTING].desc,   NULL,         cb_web,         MOZILLA_EXISTING,   NULL, NULL },
   { url_commands[MOZILLA_NEW_WINDOW].desc, NULL,         cb_web,         MOZILLA_NEW_WINDOW, NULL, NULL },
   { url_commands[MOZILLA_NEW_TAB].desc,    NULL,         cb_web,         MOZILLA_NEW_TAB,    NULL, NULL },
   { url_commands[MOZILLA_NEW].desc,        NULL,         cb_web,         MOZILLA_NEW,        NULL, NULL },
   { _("/Web/Galeon"),                      NULL,         NULL,           0,                  "<Branch>", NULL },
   { url_commands[GALEON_EXISTING].desc,    NULL,         cb_web,         GALEON_EXISTING,    NULL, NULL },
   { url_commands[GALEON_NEW_WINDOW].desc,  NULL,         cb_web,         GALEON_NEW_WINDOW,  NULL, NULL },
   { url_commands[GALEON_NEW_TAB].desc,     NULL,         cb_web,         GALEON_NEW_TAB,     NULL, NULL },
   { url_commands[GALEON_NEW].desc,         NULL,         cb_web,         GALEON_NEW,         NULL, NULL },
   { _("/Web/Opera"),                       NULL,         NULL,           0,                  "<Branch>", NULL },
   { url_commands[OPERA_EXISTING].desc,     NULL,         cb_web,         OPERA_EXISTING,     NULL, NULL },
   { url_commands[OPERA_NEW_WINDOW].desc,   NULL,         cb_web,         OPERA_NEW_WINDOW,   NULL, NULL },
   { url_commands[OPERA_NEW].desc,          NULL,         cb_web,         OPERA_NEW,          NULL, NULL },
   { _("/Web/GnomeUrl"),                    NULL,         NULL,           0,                  "<Branch>", NULL },
   { url_commands[GNOME_URL].desc,          NULL,         cb_web,         GNOME_URL,          NULL, NULL },
   { _("/Web/Lynx"),                        NULL,         NULL,           0,                  "<Branch>", NULL },
   { url_commands[LYNX_NEW].desc,           NULL,         cb_web,         LYNX_NEW,           NULL, NULL },
   { _("/Web/Links"),                       NULL,         NULL,           0,                  "<Branch>", NULL },
   { url_commands[LINKS_NEW].desc,          NULL,         cb_web,         LINKS_NEW,          NULL, NULL },
   { _("/Web/W3M"),                         NULL,         NULL,           0,                  "<Branch>", NULL },
   { url_commands[W3M_NEW].desc,            NULL,         cb_web,         W3M_NEW,            NULL, NULL },
   { _("/Web/Konqueror"),                   NULL,         NULL,           0,                  "<Branch>", NULL },
   { url_commands[KONQUEROR_NEW].desc,      NULL,         cb_web,         KONQUEROR_NEW,      NULL, NULL },
#endif
   { _("/_Help"),                           NULL,         NULL,           0,                  "<LastBranch>", NULL },
   { _("/Help/About J-Pilot"),              NULL,         cb_about,       0,                  ICON(GTK_STOCK_DIALOG_INFO) },
   { "END",                                 NULL,         NULL,           0,                  NULL, NULL }
   };

   GtkItemFactory *item_factory;
   GtkAccelGroup *accel_group;
   gint nmenu_items;
   GtkItemFactoryEntry *menu_items2;
   int i1, i2;
   unsigned int i;
   char temp_str[255];

#ifdef ENABLE_PLUGINS
   int count, help_count;
   struct plugin_s *p;
   int str_i;
   char **plugin_menu_strings;
   char **plugin_help_strings;
   GList *temp_list;
   char *F_KEYS[]={"F5","F6","F7","F8","F9","F10","F11","F12"};
   int f_key_count;
#endif

   /* Irix doesn't like non-constant expressions in a static initializer */
   /* So we have to do this to keep the compiler happy */
   for (i=0; i<sizeof(menu_items1)/sizeof(menu_items1[0]); i++) {
      if (menu_items1[i].callback==cb_prefs_gui) {
         menu_items1[i].callback_action = GPOINTER_TO_INT(my_window);
         break;
      }
   }

#ifdef ENABLE_PLUGINS
   /* Count the plugin/ entries */
   for (count=0, temp_list = plugin_list;
        temp_list; 
        temp_list = temp_list->next) {
      p = (struct plugin_s *)temp_list->data;
      if (p->menu_name) {
         count++;
      }
   }

   /* Count the help/ entries */
   for (help_count=0, temp_list = plugin_list;
        temp_list; 
        temp_list = temp_list->next) {
      p = (struct plugin_s *)temp_list->data;
      if (p->help_name) {
         help_count++;
      }
   }

   plugin_menu_strings = plugin_help_strings = NULL;
   if (count) {
      plugin_menu_strings = malloc(count * sizeof(char *));
   }
   if (help_count) {
      plugin_help_strings = malloc(help_count * sizeof(char *));
   }

   /* Create plugin menu strings */
   str_i = 0;
   for (temp_list = plugin_list; temp_list; temp_list = temp_list->next) {
      p = (struct plugin_s *)temp_list->data;
      if (p->menu_name) {
         g_snprintf(temp_str, sizeof(temp_str), _("/_Plugins/%s"), p->menu_name);
         plugin_menu_strings[str_i++]=strdup(temp_str);
      }
   }

   /* Create help menu strings */
   str_i = 0;
   for (temp_list = plugin_list; temp_list; temp_list = temp_list->next) {
      p = (struct plugin_s *)temp_list->data;
      if (p->help_name) {
         g_snprintf(temp_str, sizeof(temp_str), _("/_Help/%s"), p->help_name);
         plugin_help_strings[str_i++]=strdup(temp_str);
      }
   }
#endif

   nmenu_items = (sizeof (menu_items1) / sizeof (menu_items1[0])) - 2;

#ifdef ENABLE_PLUGINS
   if (count) {
      nmenu_items = nmenu_items + count + 1;
   }
   nmenu_items = nmenu_items + help_count;
#endif

   menu_items2=malloc(nmenu_items * sizeof(GtkItemFactoryEntry));
   if (!menu_items2) {
      jp_logf(JP_LOG_WARN, "get_main_menu(): %s\n", _("Out of memory"));
      return;
   }
   /* Copy the first part of the array until Plugins */
   for (i1=i2=0; ; i1++, i2++) {
      if (!strcmp(menu_items1[i1].path, _("/_Plugins"))) {
         break;
      }
      menu_items2[i2]=menu_items1[i1];
   }

#ifdef ENABLE_PLUGINS
   if (count) {
      /* This is the /_Plugins entry */
      menu_items2[i2]=menu_items1[i1];
      i1++; i2++;
      str_i=0;
      for (temp_list = plugin_list, f_key_count=0;
           temp_list;
           temp_list = temp_list->next, f_key_count++) {
         p = (struct plugin_s *)temp_list->data;
         if (!p->menu_name) {
            f_key_count--;
            continue;
         }
         menu_items2[i2].path=plugin_menu_strings[str_i];
         if (f_key_count < 8) {
            menu_items2[i2].accelerator=F_KEYS[f_key_count];
         } else {
            menu_items2[i2].accelerator=NULL;
         }
         menu_items2[i2].callback=cb_plugin_gui;
         menu_items2[i2].callback_action=p->number;
         menu_items2[i2].item_type=0;
         str_i++;
         i2++;
      }
   } else {
      /* Skip the /_Plugins entry */
      i1++;
   }
#else
   /* Skip the /_Plugins entry */
   i1++;
#endif
   /* Copy the last part of the array until END */
   for (; strcmp(menu_items1[i1].path, "END"); i1++, i2++) {
      menu_items2[i2]=menu_items1[i1];
   }

#ifdef ENABLE_PLUGINS
   if (help_count) {
      str_i=0;
      for (temp_list = plugin_list;
           temp_list;
           temp_list = temp_list->next) {
         p = (struct plugin_s *)temp_list->data;
         if (!p->help_name) {
            continue;
         }
         menu_items2[i2].path=plugin_help_strings[str_i];
         menu_items2[i2].accelerator=NULL;
         menu_items2[i2].callback=cb_plugin_help;
         menu_items2[i2].callback_action=p->number;
         menu_items2[i2].item_type=0;
         str_i++;
         i2++;
      }
   }
#endif

   accel_group = gtk_accel_group_new();

   /* This function initializes the item factory.
    * Param 1: The type of menu - can be GTK_TYPE_MENU_BAR, GTK_TYPE_MENU,
    * or GTK_TYPE_OPTION_MENU.
    * Param 2: The path of the menu.
    * Param 3: A pointer to a gtk_accel_group.  The item factory sets up
    * the accelerator table while generating menus. */
   item_factory = gtk_item_factory_new(GTK_TYPE_MENU_BAR, "<main>",
                                       accel_group);

   /* This function generates the menu items. Pass the item factory,
    * the number of items in the array, the array itself, and any
    * callback data for the the menu items. */
   gtk_item_factory_create_items(item_factory, nmenu_items, menu_items2, NULL);

   /* Attach the new accelerator group to the window. */
   gtk_window_add_accel_group(GTK_WINDOW(my_window), accel_group);

   if (menubar) {
      /* Finally, return the actual menu bar created by the item factory. */
      *menubar = gtk_item_factory_get_widget (item_factory, "<main>");
   }

   free(menu_items2);

#ifdef ENABLE_PLUGINS
   if (count) {
      for (str_i=0; str_i < count; str_i++) {
         free(plugin_menu_strings[str_i]);
      }
      free(plugin_menu_strings);
   }
   if (help_count) {
      for (str_i=0; str_i < help_count; str_i++) {
         free(plugin_help_strings[str_i]);
      }
      free(plugin_help_strings);
   }
#endif

   menu_hide_privates = GTK_CHECK_MENU_ITEM(gtk_item_factory_get_widget(
                                            item_factory,
                                            _("/View/Hide Private Records")));
   menu_show_privates = GTK_CHECK_MENU_ITEM(gtk_item_factory_get_widget(
                                            item_factory,
                                            _("/View/Show Private Records")));
   menu_mask_privates = GTK_CHECK_MENU_ITEM(gtk_item_factory_get_widget(
                                            item_factory,
                                            _("/View/Mask Private Records")));
}

static void cb_delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
   int pw, ph;
   int x,y;
#ifdef ENABLE_PLUGINS
   struct plugin_s *plugin;
   GList *plugin_list, *temp_list;
#endif

   /* gdk_window_get_deskrelative_origin(window->window, &x, &y); */
   gdk_window_get_origin(window->window, &x, &y);
   jp_logf(JP_LOG_DEBUG, "x=%d, y=%d\n", x, y);

   gdk_window_get_size(window->window, &pw, &ph);
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
      plugin = (struct plugin_s *)temp_list->data;
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

   cleanup_pc_files();

   cleanup_pidfile();

   gtk_main_quit();
}

static void cb_output(GtkWidget *widget, gpointer data)
{
   int flags;
   int w, h, output_height;

   flags=GPOINTER_TO_INT(data);

   if ((flags==OUTPUT_MINIMIZE) || (flags==OUTPUT_RESIZE)) {
      jp_logf(JP_LOG_DEBUG,"paned pos = %d\n", gtk_paned_get_position(GTK_PANED(output_pane)));
      gdk_window_get_size(window->window, &w, &h);
      output_height = (h - gtk_paned_get_position(GTK_PANED(output_pane)));
      set_pref(PREF_OUTPUT_HEIGHT, output_height, NULL, TRUE);
      if (flags==OUTPUT_MINIMIZE) {
         gtk_paned_set_position(GTK_PANED(output_pane), h);
      }
      jp_logf(JP_LOG_DEBUG,"output height = %d\n", output_height);
   }
   if (flags==OUTPUT_CLEAR) {
      gtk_text_buffer_set_text(g_output_text_buffer, "", -1);
   }
}

static gint cb_output_idle(gpointer data)
{
   cb_output(NULL, data);
   /* returning false removes this handler from being called again */
   return FALSE;
}

static gint cb_output2(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
   /* Because the pane isn't redrawn yet we can't get positions from it.
    * So we have to call back after everything is drawn */
   gtk_idle_add(cb_output_idle, data);

   return EXIT_SUCCESS;
}

static gint cb_check_version(gpointer main_window)
{
   int major, minor, micro;
   int r;
   char str_ver[8];

   jp_logf(JP_LOG_DEBUG, "cb_check_version\n");

   r = sscanf(VERSION, "%d.%d.%d", &major, &minor, &micro);
   if (r!=3) {
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

int main(int argc, char *argv[])
{
   GtkWidget *main_vbox;
   GtkWidget *temp_hbox;
   GtkWidget *temp_vbox;
   GtkWidget *button_datebook,*button_address,*button_todo,*button_memo;
   GtkWidget *button;
   GtkWidget *separator;
   GtkStyle *style;
   GdkBitmap *mask;
   GtkWidget *pixmapwid;
   GdkPixmap *pixmap;
   GtkWidget *menubar = NULL;
   GtkWidget *scrolled_window;
   GtkAccelGroup *accel_group;
/* Extract first day of week preference from locale in GTK2 */
   int   pref_fdow = 0;
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
   int i, ret, height, width;
   char title[MAX_PREF_LEN+256];
   long pref_width, pref_height, show_tooltips;
   long char_set;
   char *geometry_str=NULL;
   int iconify = 0;
#ifdef ENABLE_PLUGINS
   GList *plugin_list;
   GList *temp_list;
   struct plugin_s *plugin;
   jp_startup_info info;
#endif
   int pid;
   int remote_sync = FALSE;
#if defined(ENABLE_NLS)
#  ifdef HAVE_LOCALE_H
   char *current_locale;
#  endif
#endif

   skip_plugins=FALSE;
   skip_past_alarms=FALSE;
   skip_all_alarms=FALSE;
   /* log all output to a file */
   glob_log_file_mask = JP_LOG_INFO | JP_LOG_WARN | JP_LOG_FATAL | JP_LOG_STDOUT;
   glob_log_stdout_mask = JP_LOG_INFO | JP_LOG_WARN | JP_LOG_FATAL | JP_LOG_STDOUT;
   glob_log_gui_mask = JP_LOG_FATAL | JP_LOG_WARN | JP_LOG_GUI;
   glob_find_id = 0;

   /* Directory ~/.jpilot is created with permissions of 700 to prevent anyone
    * but the user from looking at potentially sensitive files.
    * Files within the directory have permission 600 */
   umask(0077);

   /* enable internationalization(i18n) before printing any output */
#if defined(ENABLE_NLS)
#  ifdef HAVE_LOCALE_H
      current_locale = setlocale(LC_ALL, "");
#  endif
   bindtextdomain(EPN, LOCALEDIR);
   textdomain(EPN);
#endif

   pref_init();

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

   /* parse command line options */
   for (i=1; i<argc; i++) {
      if (!strncasecmp(argv[i], "-v", 3)) {
         char options[1024];
         get_compile_options(options, sizeof(options));
         printf("\n%s\n", options);
         exit(0);
      }
      if ( (!strncasecmp(argv[i], "-h", 3)) ||
          (!strncasecmp(argv[i], "-?", 3)) ) {
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
      if ( (!strncmp(argv[i], "-s", 3)) ||
           (!strncmp(argv[i], "--remote-sync", 14))) {
         remote_sync = TRUE;
      }
      if ( (!strncasecmp(argv[i], "-i", 3)) ||
           (!strncasecmp(argv[i], "--iconic", 9))){
         iconify = 1;
      }
      if (!strncasecmp(argv[i], "-geometry", 9)) {
         /* The '=' isn't specified in `man X`, but we will be nice */
         if (argv[i][9]=='=') {
            geometry_str=argv[i]+9;
         } else {
            if (i<argc) {
               geometry_str=argv[i+1];
            }
         }
      }
   }

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
         printf ("jpilot: syncing jpilot at %d\n", pid);
         kill (pid, SIGUSR1);
         exit (0);
      }
      else {
         fprintf (stderr, "%s\n", "J-Pilot not running.");
         exit (1);
      }
   }
   else if (!pid) {
      /* JPilot not running, install signal handler and write pid file */
      signal(SIGUSR1, sync_sig_handler);
      write_pid();
   }

   /* Check to see if DB files are there */
   /* If not copy some empty ones over */
   check_copy_DBs_to_home();

#ifdef ENABLE_PLUGINS
   plugin_list=NULL;
   if (!skip_plugins) {
      load_plugins();
   }
   plugin_list = get_plugin_list();

   for (temp_list = plugin_list; temp_list; temp_list = temp_list->next) {
      plugin = (struct plugin_s *)temp_list->data;
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

   glob_date_timer_tag=0;
   glob_child_pid=0;

   /* Create a pipe to send data from sync child to parent */
   if (pipe(filedesc) < 0) {
      jp_logf(JP_LOG_FATAL, _("Unable to open pipe\n"));
      exit(-1);
   }
   pipe_from_child = filedesc[0];
   pipe_to_parent  = filedesc[1];

   /* Create a pipe to send data from parent to sync child */
   if (pipe(filedesc) < 0) {
      jp_logf(JP_LOG_FATAL, _("Unable to open pipe\n"));
      exit(-1);
   }
   pipe_from_parent = filedesc[0];
   pipe_to_child    = filedesc[1];

   get_pref(PREF_CHAR_SET, &char_set, NULL);
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
   ret = 0;
   if (geometry_str) {
      ret = gtk_window_parse_geometry(GTK_WINDOW(window), geometry_str);
   } 
   if ((!geometry_str) || (ret != 1)) {
      gtk_window_set_default_size(GTK_WINDOW(window), pref_width, pref_height);
   }

   if (iconify) {
      gtk_window_iconify(GTK_WINDOW(window));
   }

   /* Set a handler for delete_event that immediately exits GTK. */
   gtk_signal_connect(GTK_OBJECT(window), "delete_event",
                      GTK_SIGNAL_FUNC(cb_delete_event), NULL);

   gtk_container_set_border_width(GTK_CONTAINER(window), 0);

   main_vbox = gtk_vbox_new(FALSE, 0);
   g_hbox = gtk_hbox_new(FALSE, 0);
   g_vbox0 = gtk_vbox_new(FALSE, 0);

   /* Output Pane */
   output_pane = gtk_vpaned_new();

   gtk_signal_connect(GTK_OBJECT(output_pane), "button_release_event",
                      GTK_SIGNAL_FUNC(cb_output2),
                      GINT_TO_POINTER(OUTPUT_RESIZE));

   gtk_container_add(GTK_CONTAINER(window), output_pane);

   gtk_paned_pack1(GTK_PANED(output_pane), main_vbox, FALSE, FALSE);

   /* Create the Menu Bar at the top */
#ifdef ENABLE_PLUGINS
   get_main_menu(window, &menubar, plugin_list);
#else
   get_main_menu(window, &menubar, NULL);
#endif
   gtk_box_pack_start(GTK_BOX(main_vbox), menubar, FALSE, FALSE, 0);

   gtk_box_pack_start(GTK_BOX(main_vbox), g_hbox, TRUE, TRUE, 3);
   gtk_container_set_border_width(GTK_CONTAINER(g_hbox), 10);
   gtk_box_pack_start(GTK_BOX(g_hbox), g_vbox0, FALSE, FALSE, 3);

   /* Output Text scrolled window */
   temp_hbox = gtk_hbox_new(FALSE, 0);
   gtk_container_set_border_width(GTK_CONTAINER(temp_hbox), 5);
   gtk_paned_pack2(GTK_PANED(output_pane), temp_hbox, FALSE, TRUE);

   temp_vbox = gtk_vbox_new(FALSE, 3);
   gtk_container_set_border_width(GTK_CONTAINER(temp_vbox), 6);
   gtk_box_pack_end(GTK_BOX(temp_hbox), temp_vbox, FALSE, FALSE, 0);

   g_output_text = GTK_TEXT_VIEW(gtk_text_view_new());
   g_output_text_buffer = gtk_text_view_get_buffer(g_output_text);
   gtk_text_view_set_cursor_visible(g_output_text, FALSE);
   gtk_text_view_set_editable(g_output_text, FALSE);
   gtk_text_view_set_wrap_mode(g_output_text, GTK_WRAP_WORD);

   scrolled_window = gtk_scrolled_window_new(NULL, NULL);
   gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                  GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);

   gtk_container_add(GTK_CONTAINER(scrolled_window), GTK_WIDGET(g_output_text));
   gtk_box_pack_start_defaults(GTK_BOX(temp_hbox), scrolled_window);

   button = gtk_button_new_from_stock(GTK_STOCK_CLEAR);
   gtk_box_pack_start(GTK_BOX(temp_vbox), button, TRUE, TRUE, 3);
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
                      GTK_SIGNAL_FUNC(cb_output),
                      GINT_TO_POINTER(OUTPUT_CLEAR));

   button = gtk_button_new_from_stock(GTK_STOCK_REMOVE);
   gtk_box_pack_start(GTK_BOX(temp_vbox), button, TRUE, TRUE, 3);
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
                      GTK_SIGNAL_FUNC(cb_output),
                      GINT_TO_POINTER(OUTPUT_MINIMIZE));

   /* "Datebook" button */
   temp_hbox = gtk_hbox_new(FALSE, 0);
   button_datebook = gtk_button_new();
   gtk_signal_connect(GTK_OBJECT(button_datebook), "clicked",
                      GTK_SIGNAL_FUNC(cb_app_button), GINT_TO_POINTER(DATEBOOK));
   gtk_box_pack_start(GTK_BOX(g_vbox0), temp_hbox, FALSE, FALSE, 0);
   gtk_box_pack_start(GTK_BOX(temp_hbox), button_datebook, TRUE, FALSE, 0);

   /* "Address" button */
   temp_hbox = gtk_hbox_new(FALSE, 0);
   button_address = gtk_button_new();
   gtk_signal_connect(GTK_OBJECT(button_address), "clicked",
                      GTK_SIGNAL_FUNC(cb_app_button), GINT_TO_POINTER(ADDRESS));
   gtk_box_pack_start(GTK_BOX(g_vbox0), temp_hbox, FALSE, FALSE, 0);
   gtk_box_pack_start(GTK_BOX(temp_hbox), button_address, TRUE, FALSE, 0);

   /* "Todo" button */
   temp_hbox = gtk_hbox_new(FALSE, 0);
   button_todo = gtk_button_new();
   gtk_signal_connect(GTK_OBJECT(button_todo), "clicked",
                      GTK_SIGNAL_FUNC(cb_app_button), GINT_TO_POINTER(TODO));
   gtk_box_pack_start(GTK_BOX(g_vbox0), temp_hbox, FALSE, FALSE, 0);
   gtk_box_pack_start(GTK_BOX(temp_hbox), button_todo, TRUE, FALSE, 0);

   /* "Memo" button */
   temp_hbox = gtk_hbox_new(FALSE, 0);
   button_memo = gtk_button_new();
   gtk_signal_connect(GTK_OBJECT(button_memo), "clicked",
                      GTK_SIGNAL_FUNC(cb_app_button), GINT_TO_POINTER(MEMO));
   gtk_box_pack_start(GTK_BOX(g_vbox0), temp_hbox, FALSE, FALSE, 0);
   gtk_box_pack_start(GTK_BOX(temp_hbox), button_memo, TRUE, FALSE, 0);

   gtk_widget_set_name(button_datebook, "button_app");
   gtk_widget_set_name(button_address, "button_app");
   gtk_widget_set_name(button_todo, "button_app");
   gtk_widget_set_name(button_memo, "button_app");

   /* Create tooltips */
   glob_tooltips = gtk_tooltips_new();

   /* Get preference to show tooltips */
   get_pref(PREF_SHOW_TOOLTIPS, &show_tooltips, NULL);

   /* Create key accelerator */
   accel_group = gtk_accel_group_new();
   gtk_window_add_accel_group(GTK_WINDOW(window), accel_group);

   /* Lock/Unlock/Mask buttons */
   button_locked = gtk_button_new();
   button_masklocked = gtk_button_new();
   button_unlocked = gtk_button_new();
   gtk_signal_connect(GTK_OBJECT(button_locked), "clicked",
                      GTK_SIGNAL_FUNC(cb_private),
                      GINT_TO_POINTER(SHOW_PRIVATES));
   gtk_signal_connect(GTK_OBJECT(button_masklocked), "clicked",
                      GTK_SIGNAL_FUNC(cb_private),
                      GINT_TO_POINTER(HIDE_PRIVATES));
   gtk_signal_connect(GTK_OBJECT(button_unlocked), "clicked",
                      GTK_SIGNAL_FUNC(cb_private), 
                      GINT_TO_POINTER(MASK_PRIVATES));
   gtk_box_pack_start(GTK_BOX(g_vbox0), button_locked, FALSE, FALSE, 20);
   gtk_box_pack_start(GTK_BOX(g_vbox0), button_masklocked, FALSE, FALSE, 20);
   gtk_box_pack_start(GTK_BOX(g_vbox0), button_unlocked, FALSE, FALSE, 20);

   set_tooltip(show_tooltips, glob_tooltips, 
               button_locked, _("Show private records   Ctrl+Z"), NULL);
   gtk_widget_add_accelerator(button_locked, "clicked", accel_group,
                              GDK_z, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);

   set_tooltip(show_tooltips, glob_tooltips, 
               button_masklocked, _("Hide private records   Ctrl+Z"), NULL);
   gtk_widget_add_accelerator(button_masklocked, "clicked", accel_group,
                              GDK_z, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);

   set_tooltip(show_tooltips, glob_tooltips, 
               button_unlocked, _("Mask private records   Ctrl+Z"), NULL);
   gtk_widget_add_accelerator(button_unlocked, "clicked", accel_group,
                              GDK_z, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);

   /* "Sync" button */
   button_sync = gtk_button_new();
   gtk_signal_connect(GTK_OBJECT(button_sync), "clicked",
                      GTK_SIGNAL_FUNC(cb_sync),
                      GINT_TO_POINTER(skip_plugins ? SYNC_NO_PLUGINS : 0));
   gtk_box_pack_start(GTK_BOX(g_vbox0), button_sync, FALSE, FALSE, 3);

   set_tooltip(show_tooltips, glob_tooltips, 
               button_sync, _("Sync your palm to the desktop   Ctrl+Y"), NULL);
   gtk_widget_add_accelerator(button_sync, "clicked", accel_group, GDK_y,
                              GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);

   /* "Cancel Sync" button */
   button_cancel_sync = gtk_button_new();
   gtk_signal_connect(GTK_OBJECT(button_cancel_sync), "clicked",
                      GTK_SIGNAL_FUNC(cb_cancel_sync),
                      NULL);
   gtk_box_pack_start(GTK_BOX(g_vbox0), button_cancel_sync, FALSE, FALSE, 3);

   set_tooltip(show_tooltips, glob_tooltips, 
               button_cancel_sync, _("Stop Sync process"), NULL);

   /* "Backup" button in left column */
   button_backup = gtk_button_new();
   gtk_signal_connect(GTK_OBJECT(button_backup), "clicked",
                      GTK_SIGNAL_FUNC(cb_sync),
                      GINT_TO_POINTER
                      (skip_plugins ? SYNC_NO_PLUGINS | SYNC_FULL_BACKUP
                      : SYNC_FULL_BACKUP));
   gtk_box_pack_start(GTK_BOX(g_vbox0), button_backup, FALSE, FALSE, 3);

   set_tooltip(show_tooltips, glob_tooltips, 
               button_backup, _("Sync your palm to the desktop\n"
                                "and then do a backup"), NULL);

   /* Separator */
   separator = gtk_hseparator_new();
   gtk_box_pack_start(GTK_BOX(g_vbox0), separator, FALSE, TRUE, 5);

   /* Creates the 2 main boxes that are changeable */
   create_main_boxes();

   gtk_widget_show_all(window);
   gtk_widget_show(window);

   style = gtk_widget_get_style(window);

   /* Jpilot Icon pixmap */
   pixmap = gdk_pixmap_create_from_xpm_d(window->window, &mask, NULL, jpilot_icon4_xpm);
   gdk_window_set_icon(window->window, NULL, pixmap, mask);
   gdk_window_set_icon_name(window->window, PN);

   /* Create "Datebook" pixmap */
   pixmap = gdk_pixmap_create_from_xpm_d(window->window, &mask,
                                         &style->bg[GTK_STATE_NORMAL],
                                         datebook_xpm);
#ifdef __APPLE__
   mask = NULL;
#endif
   pixmapwid = gtk_pixmap_new(pixmap, mask);
   gtk_widget_show(pixmapwid);
   gtk_container_add(GTK_CONTAINER(button_datebook), pixmapwid);
   gtk_button_set_relief(GTK_BUTTON(button_datebook), GTK_RELIEF_NONE);

   /* Create "Address" pixmap */
   pixmap = gdk_pixmap_create_from_xpm_d(window->window, &mask,
                                         &style->bg[GTK_STATE_NORMAL],
                                         address_xpm);
#ifdef __APPLE__
   mask = NULL;
#endif
   pixmapwid = gtk_pixmap_new(pixmap, mask);
   gtk_widget_show(pixmapwid);
   gtk_container_add(GTK_CONTAINER(button_address), pixmapwid);
   gtk_button_set_relief(GTK_BUTTON(button_address), GTK_RELIEF_NONE);

   /* Create "Todo" pixmap */
   pixmap = gdk_pixmap_create_from_xpm_d(window->window, &mask,
                                         &style->bg[GTK_STATE_NORMAL],
                                         todo_xpm);
#ifdef __APPLE__
   mask = NULL;
#endif
   pixmapwid = gtk_pixmap_new(pixmap, mask);
   gtk_widget_show(pixmapwid);
   gtk_container_add(GTK_CONTAINER(button_todo), pixmapwid);
   gtk_button_set_relief(GTK_BUTTON(button_todo), GTK_RELIEF_NONE);

   /* Create "Memo" pixmap */
   pixmap = gdk_pixmap_create_from_xpm_d(window->window, &mask,
                                         &style->bg[GTK_STATE_NORMAL],
                                         memo_xpm);
#ifdef __APPLE__
   mask = NULL;
#endif
   pixmapwid = gtk_pixmap_new(pixmap, mask);
   gtk_widget_show(pixmapwid);
   gtk_container_add(GTK_CONTAINER(button_memo), pixmapwid);
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

   /* Create "locked" pixmap */
   pixmap = gdk_pixmap_create_from_xpm_d(window->window, &mask,
                                         &style->bg[GTK_STATE_NORMAL],
                                         locked_xpm);
#ifdef __APPLE__
   mask = NULL;
#endif
   pixmapwid = gtk_pixmap_new(pixmap, mask);
   gtk_widget_show(pixmapwid);
   gtk_container_add(GTK_CONTAINER(button_locked), pixmapwid);

   /* Create "masked" pixmap */
   pixmap = gdk_pixmap_create_from_xpm_d(window->window, &mask,
                                         &style->bg[GTK_STATE_NORMAL],
                                         masklocked_xpm);
#ifdef __APPLE__
   mask = NULL;
#endif
   pixmapwid = gtk_pixmap_new(pixmap, mask);
   gtk_widget_show(pixmapwid);
   gtk_container_add(GTK_CONTAINER(button_masklocked), pixmapwid);

   /* Create "unlocked" pixmap */
   pixmap = gdk_pixmap_create_from_xpm_d(window->window, &mask,
                                         &style->bg[GTK_STATE_NORMAL],
                                         unlocked_xpm);
#ifdef __APPLE__
   mask = NULL;
#endif
   pixmapwid = gtk_pixmap_new(pixmap, mask);
   gtk_widget_show(pixmapwid);
   gtk_container_add(GTK_CONTAINER(button_unlocked), pixmapwid);

   /* Create "sync" pixmap */
   pixmap = gdk_pixmap_create_from_xpm_d(window->window, &mask,
                                         &style->bg[GTK_STATE_NORMAL],
                                         sync_xpm);
#ifdef __APPLE__
   mask = NULL;
#endif
   pixmapwid = gtk_pixmap_new(pixmap, mask);
   gtk_widget_show(pixmapwid);
   gtk_container_add(GTK_CONTAINER(button_sync), pixmapwid);

   /* Create "cancel sync" pixmap */
   /* Hide until sync process started */
   gtk_widget_hide(button_cancel_sync);
   pixmap = gdk_pixmap_create_from_xpm_d(window->window, &mask,
                                         &style->bg[GTK_STATE_NORMAL],
                                         cancel_sync_xpm);
#ifdef __APPLE__
   mask = NULL;
#endif
   pixmapwid = gtk_pixmap_new(pixmap, mask);
   gtk_widget_show(pixmapwid);
   gtk_container_add(GTK_CONTAINER(button_cancel_sync), pixmapwid);


   /* Create "backup" pixmap */
   pixmap = gdk_pixmap_create_from_xpm_d(window->window, &mask,
                                         &style->bg[GTK_STATE_NORMAL],
                                         backup_xpm);
#ifdef __APPLE__
   mask = NULL;
#endif
   pixmapwid = gtk_pixmap_new(pixmap, mask);
   gtk_widget_show(pixmapwid);
   gtk_container_add(GTK_CONTAINER(button_backup), pixmapwid);

   set_tooltip(show_tooltips, glob_tooltips, button_datebook, _("Datebook/Go to Today"), NULL);
   set_tooltip(show_tooltips, glob_tooltips, button_address, _("Address Book"), NULL);
   set_tooltip(show_tooltips, glob_tooltips, button_todo, _("ToDo List"), NULL);
   set_tooltip(show_tooltips, glob_tooltips, button_memo, _("Memo Pad"), NULL);

   /* Set a callback for our pipe from the sync child process */
   gdk_input_add(pipe_from_child, GDK_INPUT_READ, cb_read_pipe_from_child, window);

   get_pref(PREF_LAST_APP, &ivalue, NULL);
   /* We don't want to start up to a plugin because the plugin might
    * repeatedly segfault.  Of course main apps can do that, but since I
    * handle the email support...  */
   if ((ivalue==ADDRESS) ||
       (ivalue==DATEBOOK) ||
       (ivalue==TODO) ||
       (ivalue==MEMO)) {
      cb_app_button(NULL, GINT_TO_POINTER(ivalue));
   }

   /* Set the pane size */
   width = height = 0;
   gdk_window_get_size(window->window, &width, &height);
   gtk_paned_set_position(GTK_PANED(output_pane), height);

   alarms_init(skip_past_alarms, skip_all_alarms);

   /* In-line code to switch user to UTF-8 encoding.
    * Should probably be made a subroutine */
{
   long utf_encoding;
   char *button_text[] = 
      { N_("Do it now"), N_("Remind me later"), N_("Don't tell me again!") };

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

   gtk_idle_add(cb_check_version, window);

   gtk_main();

   otherconv_free();

   return EXIT_SUCCESS;
}


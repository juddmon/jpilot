/* jpilot.c
 * 
 * Copyright (C) 1999 by Judd Montgomery
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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
#include <gtk/gtk.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include <pi-datebook.h>
#include <gdk/gdkkeysyms.h>

/*#include "datebook.h" */
#include "utils.h"
#include "log.h"
#include "prefs_gui.h"
#include "prefs.h"

#include "datebook.xpm"
#include "address.xpm"
#include "todo.xpm"
#include "memo.xpm"

/*#define SHADOW GTK_SHADOW_IN */
/*#define SHADOW GTK_SHADOW_OUT */
/*#define SHADOW GTK_SHADOW_ETCHED_IN */
#define SHADOW GTK_SHADOW_ETCHED_OUT

GtkWidget *g_hbox, *g_vbox0;
GtkWidget *g_hbox2, *g_vbox0_1;

GtkWidget *glob_date_label;
GtkTooltips *glob_tooltips;
gint glob_date_timer_tag;
pid_t glob_child_pid;
int glob_app = 0;

int pipe_in, pipe_out;

GtkWidget *sync_window = NULL;

static void delete_event(GtkWidget *widget, GdkEvent *event, gpointer data);

int create_main_boxes()
{
   g_hbox2 = gtk_hbox_new(FALSE, 0);
   g_vbox0_1 = gtk_vbox_new(FALSE, 0);

   gtk_box_pack_start(GTK_BOX(g_hbox), g_hbox2, TRUE, TRUE, 0);
   gtk_box_pack_start(GTK_BOX(g_vbox0), g_vbox0_1, FALSE, FALSE, 0);
   return 0;
}

void cb_app_button(GtkWidget *widget, gpointer data)
{
   int app;
   
   app = GPOINTER_TO_INT(data);
   
   switch(app) {
    case DATEBOOK:
      if (glob_app == DATEBOOK) {
	 /*refresh screen */
	 datebook_refresh(FALSE);
      } else {
/*	 gtk_container_remove(GTK_CONTAINER(g_vbox0_1->parent), */
/*			      GTK_WIDGET(g_vbox0_1)); */
/*	 gtk_container_remove(GTK_CONTAINER(g_hbox2->parent), */
/*			      GTK_WIDGET(g_hbox2)); */
	 gtk_widget_destroy(g_vbox0_1);
	 gtk_widget_destroy(g_hbox2);
	 create_main_boxes();
	 if (glob_date_timer_tag) {
	    gtk_timeout_remove(glob_date_timer_tag);
	 }
	 glob_app = DATEBOOK;
	 datebook_gui(g_vbox0_1, g_hbox2);
      }
      break;
    case ADDRESS:
      if (glob_app == ADDRESS) {
	 /*refresh screen */
	 address_refresh();
      } else {
/*	 gtk_container_remove(GTK_CONTAINER(g_vbox0_1->parent), */
/*			      GTK_WIDGET(g_vbox0_1)); */
/*	 gtk_container_remove(GTK_CONTAINER(g_hbox2->parent), */
/*			      GTK_WIDGET(g_hbox2)); */
	 gtk_widget_destroy(g_vbox0_1);
	 gtk_widget_destroy(g_hbox2);
	 create_main_boxes();
	 if (glob_date_timer_tag) {
	    gtk_timeout_remove(glob_date_timer_tag);
	 }
	 glob_app = ADDRESS;
	 address_gui(g_vbox0_1, g_hbox2);
      }
      break;
    case TODO:
      if (glob_app == TODO) {
	 /*refresh screen */
	 todo_refresh();
      } else {
	 gtk_widget_destroy(g_vbox0_1);
	 gtk_widget_destroy(g_hbox2);
	 create_main_boxes();
	 if (glob_date_timer_tag) {
	    gtk_timeout_remove(glob_date_timer_tag);
	 }
	 glob_app = TODO;
	 todo_gui(g_vbox0_1, g_hbox2);
      }
      break;
    case MEMO:
      if (glob_app == MEMO) {
	 /*refresh screen */
	 memo_refresh();
      } else {
/*	 gtk_container_remove(GTK_CONTAINER(g_vbox0_1->parent), */
/*			      GTK_WIDGET(g_vbox0_1)); */
/*	 gtk_container_remove(GTK_CONTAINER(g_hbox2->parent), */
/*			      GTK_WIDGET(g_hbox2)); */
	 gtk_widget_destroy(g_vbox0_1);
	 gtk_widget_destroy(g_hbox2);
	 create_main_boxes();
	 if (glob_date_timer_tag) {
	    gtk_timeout_remove(glob_date_timer_tag);
	 }
	 glob_app = MEMO;
	 memo_gui(g_vbox0_1, g_hbox2);
      }
      break;
    default:
      /*recursion */
      if ((glob_app==DATEBOOK) ||
	  (glob_app==ADDRESS) ||
	  (glob_app==TODO) ||
	  (glob_app==MEMO) )
	cb_app_button(NULL, GINT_TO_POINTER(glob_app));
      break;
   }
}

void cb_sync_hide(GtkWidget *widget, gpointer data)
{
   GtkWidget *window;
   
   window=data;
   gtk_widget_destroy(window);
   
   sync_window=NULL;
}
   
void cb_read_pipe(gpointer data,
		  gint in,
		  GdkInputCondition condition)
{
   int num;
   char buf[1024];
   fd_set fds;
   struct timeval tv;
   int ret;
   char *Pstr1, *Pstr2, *Pstr3;
   int user_len;
   char user[MAX_PREF_VALUE];

   GtkWidget *main_window, *button, *hbox1, *vbox1, *vscrollbar;
   static GtkWidget *text;
   int pw, ph, px, py, w, h, x, y;

   main_window = data;
   
   if (!GTK_IS_WINDOW(sync_window)) {
      gdk_window_get_position(main_window->window, &px, &py);
      gdk_window_get_size(main_window->window, &pw, &ph);

      w=400;
      h=200;
      x=px+pw/2-w/2;
      y=py+ph/2-h/2;

#ifdef JPILOT_DEBUG
      jpilot_logf(LOG_DEBUG, "px=%d, py=%d, pw=%d, ph=%d\n", px, py, pw, ph);
#endif
      sync_window = gtk_widget_new(GTK_TYPE_WINDOW,
				   "type", GTK_WINDOW_DIALOG,
				   "x", x, "y", y,
				   "width", w, "height", h,
				   "title", "Output",
				   NULL);

      vbox1 = gtk_vbox_new(FALSE, 0);

      hbox1 = gtk_hbox_new(FALSE, 0);

      gtk_container_add(GTK_CONTAINER(sync_window), vbox1);
       
      gtk_box_pack_start(GTK_BOX(vbox1), hbox1, TRUE, TRUE, 0);

      /*text box */
      text = gtk_text_new(NULL, NULL);
      gtk_text_set_editable(GTK_TEXT(text), FALSE);
      gtk_text_set_word_wrap(GTK_TEXT(text), TRUE);
      vscrollbar = gtk_vscrollbar_new(GTK_TEXT(text)->vadj);
      gtk_box_pack_start(GTK_BOX(hbox1), text, TRUE, TRUE, 0);
      gtk_box_pack_start(GTK_BOX(hbox1), vscrollbar, FALSE, FALSE, 0);

      /*Button */
      button = gtk_button_new_with_label ("Hide this window");
      gtk_signal_connect(GTK_OBJECT(button), "clicked",
			 GTK_SIGNAL_FUNC(cb_sync_hide),
			 sync_window);
      gtk_box_pack_start(GTK_BOX(vbox1), button, FALSE, FALSE, 0);

      /*show it */
      gtk_widget_show_all(GTK_WIDGET(sync_window));
   }

   if (GTK_IS_WINDOW(sync_window)) {
      gdk_window_raise(sync_window->window);
   }
   
   while(1) {
      /*Linux modifies tv in the select call */
      tv.tv_sec=0;
      tv.tv_usec=0;
      FD_ZERO(&fds);
      FD_SET(in, &fds);
      ret=select(in+1, &fds, NULL, NULL, &tv);
      if (!ret) break;
      num = read(in, buf, 1022);
      if (num >= 1022) {
	 buf[1022] = '\0';
      }
      if (num>0) {
	 gtk_text_insert(GTK_TEXT(text), NULL, NULL, NULL, buf, num);
      }
      /*Look for the username */
      Pstr1 = strstr(buf, "sername");
      if (Pstr1) {
	 Pstr2 = strchr(Pstr1, '\"');
	 if (Pstr2) {
	    Pstr2++;
	    Pstr3 = strchr(Pstr2, '\"');
	    if (Pstr3) {
	       user_len = Pstr3 - Pstr2;
	       if (user_len > MAX_PREF_VALUE) {
		  user_len = MAX_PREF_VALUE;
	       }
	       strncpy(user, Pstr2, user_len);
	       user[user_len] = '\0';
	       jpilot_logf(LOG_DEBUG, "pipe_read: user = %s\n", user);
	       set_pref_char(PREF_USER, user);
	    }
	 }
      }
   }
}

void cb_about(GtkWidget *widget, gpointer data)
{
   char text[255];
   char *button_text[]={"OK!"
   };
   GtkWidget *window;
   
   window = data;
   sprintf(text,
	   /*------------------------------- */
	   PN" was written by\r\n"
	   "Judd Montgomery (c) 1999.\r\n"
	   "judd@engineer.com\r\n"
	   "http://jpilot.linuxbox.com\r\n"
	   "Please consider helping to fund his efforts.\r\n"
	   );
   if (GTK_IS_WINDOW(window)) {
      dialog_generic(GTK_WIDGET(window)->window,
		     300, 200,
		     "About "PN, "oOo", text, 1, button_text);
   }
}
	 
void get_main_menu(GtkWidget  *window,
		   GtkWidget **menubar)
/*This code was copied from the gtk_tut.txt file */
{
   GtkItemFactoryEntry menu_items[] = {
	{ "/_File",         NULL,         NULL, 0, "<Branch>" },
	{ "/_File/tear",         NULL,         NULL, 0, "<Tearoff>" },
	{ "/File/_Search",    "<control>S", cb_search_gui, 0, NULL },
	{ "/File/sep1",     NULL,         NULL, 0, "<Separator>" },
      	{ "/File/_Install",    NULL, cb_install_gui, 0, NULL },
      	/*{ "/File/_WeekView",    "F5", cb_weekview_gui, 0, NULL }, */
      	/*{ "/File/_MonthView",    "F6", cb_monthview_gui, 0, NULL }, */
	{ "/File/Preferences",    NULL, cb_prefs_gui, 0, NULL },
	{ "/File/sep1",     NULL,         NULL, 0, "<Separator>" },
	{ "/File/Quit",     "<control>Q", delete_event, 0, NULL },
	{ "/_View",      NULL,         NULL, 0, "<Branch>" },
	{ "/View/Datebook",  "F1",         cb_app_button, DATEBOOK, NULL },
	{ "/View/Addresses",  "F2",         cb_app_button, ADDRESS, NULL },
	{ "/View/Todos",  "F3",         cb_app_button, TODO, NULL },
	{ "/View/Memos",  "F4",         cb_app_button, MEMO, NULL },
	{ "/_Help",         NULL,         NULL, 0, "<LastBranch>" },
	{ "/_Help/About",   NULL,         cb_about, GPOINTER_TO_INT(window), NULL },
   };
   GtkItemFactory *item_factory;
   GtkAccelGroup *accel_group;
   gint nmenu_items = sizeof (menu_items) / sizeof (menu_items[0]);

   accel_group = gtk_accel_group_new();

   /* This function initializes the item factory.
    Param 1: The type of menu - can be GTK_TYPE_MENU_BAR, GTK_TYPE_MENU,
    or GTK_TYPE_OPTION_MENU.
    Param 2: The path of the menu.
    Param 3: A pointer to a gtk_accel_group.  The item factory sets up
    the accelerator table while generating menus.
    */
   item_factory = gtk_item_factory_new(GTK_TYPE_MENU_BAR, "<main>",
				       accel_group);

   /* This function generates the menu items. Pass the item factory,
    the number of items in the array, the array itself, and any
    callback data for the the menu items. */
   gtk_item_factory_create_items (item_factory, nmenu_items, menu_items, NULL);
   
   /* Attach the new accelerator group to the window. */
   gtk_accel_group_attach(accel_group, GTK_OBJECT (window));

   if (menubar)
     /* Finally, return the actual menu bar created by the item factory. */
     *menubar = gtk_item_factory_get_widget (item_factory, "<main>");
}

static void delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
   if (glob_child_pid) {
      jpilot_logf(LOG_DEBUG, "killing %d\n", glob_child_pid);
	 kill(glob_child_pid, SIGTERM);
   }
   write_rc_file();  /*jpilot.rc */
   gtk_main_quit();
}

int main(int   argc,
	 char *argv[])
{
   GtkWidget *window;
   GtkWidget *main_vbox;
   GtkWidget *button_datebook,*button_address,*button_todo,*button_memo;
   GtkWidget *button;
   GtkWidget *separator;
   GtkStyle *style;
   GdkBitmap *mask;
   GtkWidget *pixmapwid;
   GdkPixmap *pixmap;
   GtkWidget *menubar;
   int filedesc[2];
   int ivalue;
   const char *svalue;
   int sync_only;
   int i;
   char title[MAX_PREF_VALUE+40];
   
   sync_only=FALSE;
   /*log all output to a file */
   glob_log_file_mask = LOG_INFO | LOG_WARN | LOG_FATAL | LOG_STDOUT;
   glob_log_stdout_mask = LOG_INFO | LOG_WARN | LOG_FATAL | LOG_STDOUT;
   glob_log_gui_mask = LOG_FATAL | LOG_WARN | LOG_GUI;
   glob_find_id = 0;

   for (i=1; i<argc; i++) {
      if (!strncasecmp(argv[1], "-v", 2)) {
	 printf("%s\n", VERSION_STRING);
	 exit(0);
      }
      if (!strncasecmp(argv[1], "-h", 2)) {
	 printf("%s\n", USAGE_STRING);
	 exit(0);
      }
      if (!strncasecmp(argv[1], "-d", 2)) {
	 glob_log_stdout_mask = 0xFFFF;
	 glob_log_file_mask = 0xFFFF;
	 jpilot_logf(LOG_DEBUG, "Debug messages on.\n");
      }
      if (!strncasecmp(argv[1], "-s", 2)) {
	 sync_only=TRUE;
      }
   }

   /*Check to see if ~/.jpilot is there, or create it */
   jpilot_logf(LOG_DEBUG, "calling check_hidden_dir\n");
   if (check_hidden_dir()) {
      exit(1);
   }

   /*Check to see if DB files are there */
   /*If not copy some empty ones over */
   check_copy_DBs_to_home();
   
   glob_date_timer_tag=0;
   glob_child_pid=0;

   /*Create a pipe to send the sync output, etc. */
   if (pipe(filedesc) <0) {
      jpilot_logf(LOG_FATAL, "Could not open pipe\n");
      exit(-1);
   }
   pipe_in = filedesc[0];
   pipe_out = filedesc[1];
   
   gtk_set_locale();
#if defined(WITH_JAPANESE)
   gtk_rc_parse("gtkrc.ja");
#endif


   gtk_init(&argc, &argv);

   read_rc_file();  /*jpilot.rc */

   read_gtkrc_file();

   window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

   /*gtk_window_set_default_size(GTK_WINDOW(window), 770, 440); */
   gtk_window_set_default_size(GTK_WINDOW(window), 770, 500);

   get_pref(PREF_USER, &ivalue, &svalue);
   
   strcpy(title, PN" "VERSION);
   if ((svalue) && (svalue[0])) {
      strcat(title, " User: ");
      strcat(title, svalue);
   }
   gtk_window_set_title(GTK_WINDOW(window), title);

   /* Set a handler for delete_event that immediately */
   /* exits GTK. */
   gtk_signal_connect(GTK_OBJECT(window), "delete_event",
		      GTK_SIGNAL_FUNC(delete_event), NULL);

   gtk_container_set_border_width(GTK_CONTAINER(window), 0);

   main_vbox = gtk_vbox_new(FALSE, 0);
   g_hbox = gtk_hbox_new(FALSE, 0);
   g_vbox0 = gtk_vbox_new(FALSE, 0);

   gtk_container_add(GTK_CONTAINER(window), main_vbox);
   gtk_widget_show(main_vbox);

   /*Create the Menu Bar at the top */
   get_main_menu(window, &menubar);
   gtk_box_pack_start(GTK_BOX(main_vbox), menubar, FALSE, FALSE, 0);
   gtk_menu_bar_set_shadow_type(GTK_MENU_BAR(menubar), GTK_SHADOW_NONE);
   gtk_widget_show(menubar);

   
   gtk_box_pack_start(GTK_BOX(main_vbox), g_hbox, TRUE, TRUE, 3);
   gtk_container_set_border_width(GTK_CONTAINER(g_hbox), 10);
   gtk_box_pack_start(GTK_BOX(g_hbox), g_vbox0, FALSE, FALSE, 3);
   

   /* Create "Datebook" button */
   button_datebook = gtk_button_new();
   gtk_signal_connect(GTK_OBJECT(button_datebook), "clicked",
		      GTK_SIGNAL_FUNC(cb_app_button), GINT_TO_POINTER(DATEBOOK));
   gtk_box_pack_start(GTK_BOX(g_vbox0), button_datebook, FALSE, FALSE, 0);
   gtk_widget_set_usize(button_datebook, 46, 46);
   
   /* Create "Address" button */
   button_address = gtk_button_new();
   gtk_signal_connect(GTK_OBJECT(button_address), "clicked",
		      GTK_SIGNAL_FUNC(cb_app_button), GINT_TO_POINTER(ADDRESS));
   gtk_box_pack_start(GTK_BOX(g_vbox0), button_address, FALSE, FALSE, 0);
   gtk_widget_set_usize(button_address, 46, 46);

   /* Create "Todo" button */
   button_todo = gtk_button_new();
   gtk_signal_connect(GTK_OBJECT(button_todo), "clicked",
		      GTK_SIGNAL_FUNC(cb_app_button), GINT_TO_POINTER(TODO));
   gtk_box_pack_start(GTK_BOX(g_vbox0), button_todo, FALSE, FALSE, 0);
   gtk_widget_set_usize(button_todo, 46, 46);

   /* Create "memo" button */
   button_memo = gtk_button_new();
   gtk_signal_connect(GTK_OBJECT(button_memo), "clicked",
		      GTK_SIGNAL_FUNC(cb_app_button), GINT_TO_POINTER(MEMO));
   gtk_box_pack_start(GTK_BOX(g_vbox0), button_memo, FALSE, FALSE, 0);
   gtk_widget_set_usize(button_memo, 46, 46);

   gtk_widget_set_name(button_datebook, "button_app");
   gtk_widget_set_name(button_address, "button_app");
   gtk_widget_set_name(button_todo, "button_app");
   gtk_widget_set_name(button_memo, "button_app");
   gtk_widget_show(button_datebook);
   gtk_widget_show(button_address);
   gtk_widget_show(button_todo);
   gtk_widget_show(button_memo);

   separator = gtk_hseparator_new();
   gtk_box_pack_start(GTK_BOX(g_vbox0), separator, FALSE, TRUE, 5);
   gtk_widget_show(separator);
   
   /* Create "Quit" button */
   button = gtk_button_new_with_label("Quit!");
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(delete_event), NULL);
   gtk_box_pack_start(GTK_BOX(g_vbox0), button, FALSE, FALSE, 0);
   gtk_widget_show(button);

   /* Create "Sync" button */
   button = gtk_button_new_with_label("Sync");
   gtk_signal_connect (GTK_OBJECT(button), "clicked",
		       GTK_SIGNAL_FUNC(cb_sync), NULL);
   gtk_box_pack_start(GTK_BOX (g_vbox0), button, FALSE, FALSE, 0);
   gtk_widget_show (button);

   /* Create "Backup" button in left column */
   button = gtk_button_new_with_label("Backup");
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_backup), NULL);
   gtk_box_pack_start(GTK_BOX(g_vbox0), button, FALSE, FALSE, 0);
   gtk_widget_show(button);

   /*Separator */
   separator = gtk_hseparator_new();
   gtk_box_pack_start(GTK_BOX(g_vbox0), separator, FALSE, TRUE, 5);
   gtk_widget_show(separator);

   /*This creates the 2 main boxes that are changeable */
   create_main_boxes();

   gtk_widget_show(g_hbox);
   gtk_widget_show(g_hbox2);
   gtk_widget_show(g_vbox0);
   gtk_widget_show(g_vbox0_1);

   gtk_widget_show(window);

   style = gtk_widget_get_style(window);

   /* Create "Datebook" pixmap */
   pixmap = gdk_pixmap_create_from_xpm_d(window->window, &mask,
					 &style->bg[GTK_STATE_NORMAL],
					 datebook_xpm);
   pixmapwid = gtk_pixmap_new(pixmap, mask);
   gtk_widget_show(pixmapwid);
   gtk_container_add(GTK_CONTAINER(button_datebook), pixmapwid);

   /* Create "Address" pixmap */
   pixmap = gdk_pixmap_create_from_xpm_d(window->window, &mask,
					 &style->bg[GTK_STATE_NORMAL],
					 address_xpm);
   pixmapwid = gtk_pixmap_new(pixmap, mask);
   gtk_widget_show(pixmapwid);
   gtk_container_add(GTK_CONTAINER(button_address), pixmapwid);

   /* Create "Todo" pixmap */
   pixmap = gdk_pixmap_create_from_xpm_d(window->window, &mask,
					 &style->bg[GTK_STATE_NORMAL],
					 todo_xpm);
   pixmapwid = gtk_pixmap_new(pixmap, mask);
   gtk_widget_show(pixmapwid);
   gtk_container_add(GTK_CONTAINER(button_todo), pixmapwid);

   /* Create "memo" pixmap */
   pixmap = gdk_pixmap_create_from_xpm_d(window->window, &mask,
					 &style->bg[GTK_STATE_NORMAL],
					 memo_xpm);
   pixmapwid = gtk_pixmap_new(pixmap, mask);
   gtk_widget_show(pixmapwid);
   gtk_container_add(GTK_CONTAINER(button_memo), pixmapwid);

   glob_tooltips = gtk_tooltips_new();
   
   gtk_tooltips_set_tip(glob_tooltips, button_datebook, "Datebook/Go to Today", NULL);

   gtk_tooltips_set_tip(glob_tooltips, button_address, "Address Book", NULL);

   gtk_tooltips_set_tip(glob_tooltips, button_todo, "ToDo List", NULL);

   gtk_tooltips_set_tip(glob_tooltips, button_memo, "Memo Pad", NULL);

   /*Set a callback for our pipe from the sync child process */
   gdk_input_add(pipe_in, GDK_INPUT_READ, cb_read_pipe, window);

   gtk_main();

   return 0;
}

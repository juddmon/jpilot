/* $Id: expense.c,v 1.77 2010/11/10 03:57:48 rikster5 Exp $ */

/*******************************************************************************
 * expense.c
 *
 * This is a plugin for J-Pilot which implements an interface to the
 * Palm Expense application.
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
 ******************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <gtk/gtk.h>

#include <pi-expense.h>
#include <pi-dlp.h>
#include <pi-file.h>

#include "libplugin.h"
#include "i18n.h"
#include "utils.h"
#include "prefs.h"
#include "stock_buttons.h"

/******************************************************************************/
/* Constants                                                                  */
/******************************************************************************/
#define EXPENSE_TYPE     3
#define EXPENSE_PAYMENT  4
#define EXPENSE_CURRENCY 5

#define EXP_DATE_COLUMN 0

#define PLUGIN_MAX_INACTIVE_TIME 1

#define NUM_EXP_CAT_ITEMS 16
#define MAX_EXPENSE_TYPES 28
#define MAX_PAYMENTS       8
#define MAX_CURRENCYS     34

#define CONNECT_SIGNALS    400
#define DISCONNECT_SIGNALS 401

/*  This was copied out of the pilot-link package.
 *  I just like it here for quick reference.
struct Expense {
   struct tm date;
   enum ExpenseType type;
   enum ExpensePayment payment;
   int currency;
   char * amount;
   char * vendor;
   char * city;
   char * attendees;
   char * note;
};
*/

/* This is my wrapper to the expense structure so that I can put
 * a few more fields in with it 
 */
struct MyExpense {
   PCRecType rt;
   unsigned int unique_id;
   unsigned char attrib;
   struct Expense ex;
   struct MyExpense *next;
};

struct currency_s {
   const char *country;
   int currency;
};

/******************************************************************************/
/* Global vars                                                                */
/******************************************************************************/
static struct currency_s glob_currency[MAX_CURRENCYS]={
   {N_("Australia"),         0},
   {N_("Austria"),           1},
   {N_("Belgium"),           2},
   {N_("Brazil"),            3},
   {N_("Canada"),            4},
   {N_("Denmark"),           5},
   {N_("EU (Euro)"),       133},
   {N_("Finland"),           6},
   {N_("France"),            7},
   {N_("Germany"),           8},
   {N_("Hong Kong"),         9},
   {N_("Iceland"),          10},
   {N_("India"),            24},
   {N_("Indonesia"),        25},
   {N_("Ireland"),          11},
   {N_("Italy"),            12},
   {N_("Japan"),            13},
   {N_("Korea"),            26},
   {N_("Luxembourg"),       14},
   {N_("Malaysia"),         27},
   {N_("Mexico"),           15},
   {N_("Netherlands"),      16},
   {N_("New Zealand"),      17},
   {N_("Norway"),           18},
   {N_("P.R.C."),           28},
   {N_("Philippines"),      29},
   {N_("Singapore"),        30},
   {N_("Spain"),            19},
   {N_("Sweden"),           20},
   {N_("Switzerland"),      21},
   {N_("Taiwan"),           32},
   {N_("Thailand"),         31},
   {N_("United Kingdom"),   22},
   {N_("United States"),    23}
};

/* Left-hand side of GUI */
static struct sorted_cats sort_l[NUM_EXP_CAT_ITEMS];
static GtkWidget *category_menu1;
/* Need two extra slots for the ALL category and Edit Categories... */
static GtkWidget *exp_cat_menu_item1[NUM_EXP_CAT_ITEMS+2];
static GtkWidget *scrolled_window;
static GtkWidget *clist;

/* Right-hand side of GUI */
static GtkWidget *new_record_button;
static GtkWidget *apply_record_button;
static GtkWidget *add_record_button;
static GtkWidget *delete_record_button;
static GtkWidget *copy_record_button;
static GtkWidget *table;
static GtkWidget *category_menu2;
static GtkWidget *exp_cat_menu_item2[NUM_EXP_CAT_ITEMS];
static GtkWidget *menu_payment;
static GtkWidget *menu_item_payment[MAX_PAYMENTS];
static GtkWidget *menu_expense_type;
static GtkWidget *menu_item_expense_type[MAX_EXPENSE_TYPES];
static GtkWidget *menu_currency;
static GtkWidget *menu_item_currency[MAX_CURRENCYS];
static GtkWidget *spinner_mon, *spinner_day, *spinner_year;
static GtkAdjustment *adj_mon, *adj_day, *adj_year;
static GtkWidget *entry_amount;
static GtkWidget *entry_vendor;
static GtkWidget *entry_city;
static GtkWidget *pane=NULL;
#ifndef ENABLE_STOCK_BUTTONS
static GtkAccelGroup *accel_group;
#endif
static GtkWidget *attendees;
static GObject   *attendees_buffer;
static GtkWidget *note;
static GObject   *note_buffer;

/* This is the category that is currently being displayed */
static int exp_category = CATEGORY_ALL;
static time_t plugin_last_time = 0;

static int record_changed;
static int clist_row_selected;
static int clist_col_selected;
static int connected=0;

static int glob_detail_type;
static int glob_detail_payment;
static int glob_detail_currency_pos;

static struct MyExpense *glob_myexpense_list=NULL;

/******************************************************************************/
/* Prototypes                                                                 */
/******************************************************************************/
static void display_records(void);
static void connect_changed_signals(int con_or_dis);
static void cb_clist_selection(GtkWidget      *clist,
                               gint           row,
                               gint           column,
                               GdkEventButton *event,
                               gpointer       data);
static void cb_add_new_record(GtkWidget *widget, gpointer data);
static void cb_pulldown_menu(GtkWidget *item, unsigned int value);
static int  make_menu(const char *items[], int menu_index, GtkWidget **Poption_menu,
                     GtkWidget *menu_items[]);
static int  expense_find(int unique_id);
static void cb_category(GtkWidget *item, int selection);
static int find_sort_cat_pos(int cat);
static int find_menu_cat_pos(int cat);

/******************************************************************************/
/* Start of code                                                              */
/******************************************************************************/
int plugin_unpack_cai_from_ai(struct CategoryAppInfo *cai,
                              unsigned char *ai_raw, int len)
{
   struct ExpenseAppInfo ai;
   int r;
   
   jp_logf(JP_LOG_DEBUG, "unpack_expense_cai_from_ai\n");

   memset(&ai, 0, sizeof(ai));
   r = unpack_ExpenseAppInfo(&ai, ai_raw, len);
   if (r <= 0) {
      jp_logf(JP_LOG_DEBUG, "unpack_ExpenseAppInfo failed %s %d\n", __FILE__, __LINE__);
      return EXIT_FAILURE;
   }
   memcpy(cai, &(ai.category), sizeof(struct CategoryAppInfo));
          
   return EXIT_SUCCESS;
}

int plugin_pack_cai_into_ai(struct CategoryAppInfo *cai,
                            unsigned char *ai_raw, int len)
{
   struct ExpenseAppInfo ai;
   int r;

   jp_logf(JP_LOG_DEBUG, "pack_expense_cai_into_ai\n");

   r = unpack_ExpenseAppInfo(&ai, ai_raw, len);
   if (r <= 0) {
      jp_logf(JP_LOG_DEBUG, "unpack_ExpenseAppInfo failed %s %d\n", __FILE__, __LINE__);
      return EXIT_FAILURE;
   }
   memcpy(&(ai.category), cai, sizeof(struct CategoryAppInfo));

   r = pack_ExpenseAppInfo(&ai, ai_raw, len);
   if (r <= 0) {
      jp_logf(JP_LOG_DEBUG, "pack_ExpenseAppInfo failed %s %d\n", __FILE__, __LINE__);
      return EXIT_FAILURE;
   }
   
   return EXIT_SUCCESS;
}

static gint sort_compare_date(GtkCList *clist,
                       gconstpointer ptr1,
                       gconstpointer ptr2)
{
   const GtkCListRow *row1, *row2;
   struct MyExpense *mexp1, *mexp2;
   time_t time1, time2;

   row1 = (const GtkCListRow *) ptr1;
   row2 = (const GtkCListRow *) ptr2;

   mexp1 = row1->data;
   mexp2 = row2->data;

   time1 = mktime(&(mexp1->ex.date));
   time2 = mktime(&(mexp2->ex.date));

   return(time1 - time2);
}

static void cb_clist_click_column(GtkWidget *clist, int column)
{
   struct MyExpense *mexp;
   unsigned int unique_id;

   /* Remember currently selected item and return to it after sort 
    * This is critically important because sorting without updating the 
    * global variable clist_row_selected can cause data loss */
   mexp = gtk_clist_get_row_data(GTK_CLIST(clist), clist_row_selected);
   if (mexp < (struct MyExpense *)CLIST_MIN_DATA) {
      unique_id = 0;
   } else {
      unique_id = mexp->unique_id;
   }
   
   /* Clicking on same column toggles ascending/descending sort */
   if (clist_col_selected == column)
   {
      if (GTK_CLIST(clist)->sort_type == GTK_SORT_ASCENDING) {
         gtk_clist_set_sort_type(GTK_CLIST (clist), GTK_SORT_DESCENDING);
      }
      else {
         gtk_clist_set_sort_type(GTK_CLIST (clist), GTK_SORT_ASCENDING);
      }
   }
   else /* Always sort in ascending order when changing sort column */
   {
      gtk_clist_set_sort_type(GTK_CLIST (clist), GTK_SORT_ASCENDING);
   }

   clist_col_selected = column;

   gtk_clist_set_sort_column(GTK_CLIST(clist), column);
   switch (column) {
    case EXP_DATE_COLUMN:  /* Date column */
      gtk_clist_set_compare_func(GTK_CLIST(clist), sort_compare_date);
      break;
    default: /* All other columns can use GTK default sort function */
      gtk_clist_set_compare_func(GTK_CLIST(clist), NULL);
      break;
   }
   gtk_clist_sort(GTK_CLIST(clist));

   /* Return to previously selected item */
   expense_find(unique_id);
}

static void set_new_button_to(int new_state)
{
   jp_logf(JP_LOG_DEBUG, "set_new_button_to new %d old %d\n", new_state, record_changed);

   if (record_changed==new_state) {
      return;
   }

   switch (new_state) {
    case MODIFY_FLAG:
      gtk_widget_show(copy_record_button);
      gtk_widget_show(apply_record_button);

      gtk_widget_hide(add_record_button);
      gtk_widget_hide(delete_record_button);
      gtk_widget_hide(new_record_button);
      break;
    case NEW_FLAG:
      gtk_widget_show(add_record_button);

      gtk_widget_hide(apply_record_button);
      gtk_widget_hide(copy_record_button);
      gtk_widget_hide(delete_record_button);
      gtk_widget_hide(new_record_button);
      break;
    case CLEAR_FLAG:
      gtk_widget_show(delete_record_button);
      gtk_widget_show(copy_record_button);
      gtk_widget_show(new_record_button);

      gtk_widget_hide(add_record_button);
      gtk_widget_hide(apply_record_button);
      break;
    default:
      return;
   }
   record_changed=new_state;
}

static void cb_record_changed(GtkWidget *widget, gpointer data)
{
   jp_logf(JP_LOG_DEBUG, "cb_record_changed\n");

   if (record_changed==CLEAR_FLAG) {
      connect_changed_signals(DISCONNECT_SIGNALS);
      if (GTK_CLIST(clist)->rows > 0) {
         set_new_button_to(MODIFY_FLAG);
      } else {
         set_new_button_to(NEW_FLAG);
      }
   }
}

static void connect_changed_signals(int con_or_dis)
{
   int i;

   /* CONNECT */
   if ((con_or_dis==CONNECT_SIGNALS) && (!connected)) {
      jp_logf(JP_LOG_DEBUG, "Expense: connect_changed_signals\n");
      connected=1;

      for (i=0; i<NUM_EXP_CAT_ITEMS; i++) {
         if (exp_cat_menu_item2[i]) {
            gtk_signal_connect(GTK_OBJECT(exp_cat_menu_item2[i]), "toggled",
                               GTK_SIGNAL_FUNC(cb_record_changed), NULL);
         }
      }
      for (i=0; i<MAX_EXPENSE_TYPES; i++) {
         if (menu_item_expense_type[i]) {
            gtk_signal_connect(GTK_OBJECT(menu_item_expense_type[i]), 
                               "toggled",
                               GTK_SIGNAL_FUNC(cb_record_changed), 
                               NULL);
         }
      }
      for (i=0; i<MAX_PAYMENTS; i++) {
         if (menu_item_payment[i]) {
            gtk_signal_connect(GTK_OBJECT(menu_item_payment[i]), 
                               "toggled",
                               GTK_SIGNAL_FUNC(cb_record_changed), 
                               NULL);
         }
      }
      for (i=0; i<MAX_CURRENCYS; i++) {
         if (menu_item_currency[i]) {
            gtk_signal_connect(GTK_OBJECT(menu_item_currency[i]), 
                               "toggled",
                               GTK_SIGNAL_FUNC(cb_record_changed), 
                               NULL);
         }
      }
      gtk_signal_connect(GTK_OBJECT(spinner_mon), "changed",
                         GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_connect(GTK_OBJECT(spinner_day), "changed",
                         GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_connect(GTK_OBJECT(spinner_year), "changed",
                         GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_connect(GTK_OBJECT(entry_amount), "changed",
                         GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_connect(GTK_OBJECT(entry_vendor), "changed",
                         GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_connect(GTK_OBJECT(entry_city), "changed",
                         GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      g_signal_connect(attendees_buffer, "changed",
                         GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      g_signal_connect(note_buffer, "changed",
                         GTK_SIGNAL_FUNC(cb_record_changed), NULL);
   }

   /* DISCONNECT */
   if ((con_or_dis==DISCONNECT_SIGNALS) && (connected)) {
      jp_logf(JP_LOG_DEBUG, "Expense: disconnect_changed_signals\n");
      connected=0;

      for (i=0; i<NUM_EXP_CAT_ITEMS; i++) {
         if (exp_cat_menu_item2[i]) {
            gtk_signal_disconnect_by_func(GTK_OBJECT(exp_cat_menu_item2[i]), 
                                          GTK_SIGNAL_FUNC(cb_record_changed), NULL);
         }
      }
      for (i=0; i<MAX_EXPENSE_TYPES; i++) {
         if (menu_item_expense_type[i]) {
            gtk_signal_disconnect_by_func(GTK_OBJECT(menu_item_expense_type[i]),
                                          GTK_SIGNAL_FUNC(cb_record_changed), NULL);
         }
      }
      for (i=0; i<MAX_PAYMENTS; i++) {
         if (menu_item_payment[i]) {
            gtk_signal_disconnect_by_func(GTK_OBJECT(menu_item_payment[i]), 
                                          GTK_SIGNAL_FUNC(cb_record_changed), NULL);
         }
      }
      for (i=0; i<MAX_CURRENCYS; i++) {
         if (menu_item_currency[i]) {
            gtk_signal_disconnect_by_func(GTK_OBJECT(menu_item_currency[i]), 
                                          GTK_SIGNAL_FUNC(cb_record_changed), NULL);
         }
      }
      gtk_signal_disconnect_by_func(GTK_OBJECT(spinner_mon),
                                    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_disconnect_by_func(GTK_OBJECT(spinner_day),
                                    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_disconnect_by_func(GTK_OBJECT(spinner_year),
                                    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_disconnect_by_func(GTK_OBJECT(entry_amount),
                                    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_disconnect_by_func(GTK_OBJECT(entry_vendor),
                                    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_disconnect_by_func(GTK_OBJECT(entry_city),
                                    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      g_signal_handlers_disconnect_by_func(attendees_buffer,
                                    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      g_signal_handlers_disconnect_by_func(note_buffer,
                                    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
   }
}

static void free_myexpense_list(struct MyExpense **PPmexp)
{
   struct MyExpense *mexp, *next_mexp;

   jp_logf(JP_LOG_DEBUG, "Expense: free_myexpense_list\n");

   for (mexp = *PPmexp; mexp; mexp=next_mexp) {
      free_Expense(&(mexp->ex));
      next_mexp = mexp->next;
      free(mexp);
   }
   *PPmexp=NULL;
}

#define PLUGIN_MAJOR 1
#define PLUGIN_MINOR 1

/* This is a mandatory plugin function. */
void plugin_version(int *major_version, int *minor_version)
{
   *major_version = PLUGIN_MAJOR;
   *minor_version = PLUGIN_MINOR;
}

static int static_plugin_get_name(char *name, int len)
{
   jp_logf(JP_LOG_DEBUG, "Expense: plugin_get_name\n");
   snprintf(name, len, "Expense %d.%d", PLUGIN_MAJOR, PLUGIN_MINOR);
   return EXIT_SUCCESS;
}

/* This is a mandatory plugin function. */
int plugin_get_name(char *name, int len)
{
   return static_plugin_get_name(name, len);
}

/*
 * This is an optional plugin function.
 * This is the name that will show up in the plugins menu in J-Pilot.
 */
int plugin_get_menu_name(char *name, int len)
{
   strncpy(name, _("Expense"), len);
   return EXIT_SUCCESS;
}

/*
 * This is an optional plugin function.
 * This is the name that will show up in the plugins help menu in J-Pilot.
 * If this function is used then plugin_help must also be defined.
 */
int plugin_get_help_name(char *name, int len)
{
   g_snprintf(name, len, _("About %s"), _("Expense"));
   return EXIT_SUCCESS;
}

/*
 * This is an optional plugin function.
 * This is the palm database that will automatically be synced.
 */
int plugin_get_db_name(char *name, int len)
{
   strncpy(name, "ExpenseDB", len);
   return EXIT_SUCCESS;
}

/*
 * A utility function for getting textual data from an enum.
 */
static char *get_entry_type(enum ExpenseType type)
{
   switch(type) {
    case etAirfare:
      return _("Airfare");
    case etBreakfast:
      return _("Breakfast");
    case etBus:
      return _("Bus");
    case etBusinessMeals:
      return _("BusinessMeals");
    case etCarRental:
      return _("CarRental");
    case etDinner:
      return _("Dinner");
    case etEntertainment:
      return _("Entertainment");
    case etFax:
      return _("Fax");
    case etGas:
      return _("Gas");
    case etGifts:
      return _("Gifts");
    case etHotel:
      return _("Hotel");
    case etIncidentals:
      return _("Incidentals");
    case etLaundry:
      return _("Laundry");
    case etLimo:
      return _("Limo");
    case etLodging:
      return _("Lodging");
    case etLunch:
      return _("Lunch");
    case etMileage:
      return _("Mileage");
    case etOther:
      return _("Other");
    case etParking:
      return _("Parking");
    case etPostage:
      return _("Postage");
    case etSnack:
      return _("Snack");
    case etSubway:
      return _("Subway");
    case etSupplies:
      return _("Supplies");
    case etTaxi:
      return _("Taxi");
    case etTelephone:
      return _("Telephone");
    case etTips:
      return _("Tips");
    case etTolls:
      return _("Tolls");
    case etTrain:
      return _("Train");
    default:
      return NULL;
   }
}

/* This function gets called when the "delete" button is pressed */
static void cb_delete(GtkWidget *widget, gpointer data)
{
   struct MyExpense *mexp;
   int size;
   unsigned char buf[0xFFFF];
   buf_rec br;
   int flag;

   jp_logf(JP_LOG_DEBUG, "Expense: cb_delete\n");

   flag=GPOINTER_TO_INT(data);

   mexp = gtk_clist_get_row_data(GTK_CLIST(clist), clist_row_selected);
   if (!mexp) {
      return;
   }

   /* The record that we want to delete should be written to the pc file
    * so that it can be deleted at sync time.  We need the original record
    * so that if it has changed on the pilot we can warn the user that
    * the record has changed on the pilot. */
   size = pack_Expense(&(mexp->ex), buf, 0xFFFF);
   
   br.rt = mexp->rt;
   br.unique_id = mexp->unique_id;
   br.attrib = mexp->attrib;
   br.buf = buf;
   br.size = size;

   if ((flag==MODIFY_FLAG) || (flag==DELETE_FLAG)) {
      jp_delete_record("ExpenseDB", &br, DELETE_FLAG);
   }

   if (flag == DELETE_FLAG) {
      /* when we redraw we want to go to the line above the deleted one */
      if (clist_row_selected > 0) {
         clist_row_selected--;
      }
      display_records();
   }
}

/*
 * This is called when the "Clear" button is pressed.
 * It just clears out all the detail fields.
 */
static void exp_clear_details(void)
{
   time_t ltime;
   struct tm *now;
   int new_cat;
   int sorted_position;
   
   jp_logf(JP_LOG_DEBUG, "Expense: exp_clear_details\n");

   time(&ltime);
   now = localtime(&ltime);
   
   /* Disconnect signals to prevent callbacks while we change values */
   connect_changed_signals(DISCONNECT_SIGNALS);

   gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinner_mon), now->tm_mon+1);
   gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinner_day), now->tm_mday);
   gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinner_year), now->tm_year+1900);

   gtk_entry_set_text(GTK_ENTRY(entry_amount), "");
   gtk_entry_set_text(GTK_ENTRY(entry_vendor), "");
   gtk_entry_set_text(GTK_ENTRY(entry_city), "");
   gtk_text_buffer_set_text(GTK_TEXT_BUFFER(attendees_buffer), "", -1);
   gtk_text_buffer_set_text(GTK_TEXT_BUFFER(note_buffer), "", -1);

   if (exp_category==CATEGORY_ALL) {
      new_cat = 0;
   } else {
      new_cat = exp_category;
   }
   sorted_position = find_sort_cat_pos(new_cat);
   if (sorted_position<0) {
      jp_logf(JP_LOG_WARN, _("Category is not legal\n"));
   } else {
      gtk_check_menu_item_set_active
        (GTK_CHECK_MENU_ITEM(exp_cat_menu_item2[sorted_position]), TRUE);
      gtk_option_menu_set_history(GTK_OPTION_MENU(category_menu2), 
                                  find_menu_cat_pos(sorted_position));
   }

   set_new_button_to(CLEAR_FLAG);

   connect_changed_signals(CONNECT_SIGNALS);
}

/* returns position, position starts at zero */
static int currency_id_to_position(int currency)
{
   int i;
   int found=0;
   
   for (i=0; i<MAX_CURRENCYS; i++) {
      if (glob_currency[i].currency==currency) {
         found=i;
         break;
      }
   }
   return found;
}

/* returns currency id, position starts at zero */
static int position_to_currency_id(int position)
{
   if (position<MAX_CURRENCYS) {
      return glob_currency[position].currency;
   } else {
      return 0;
   }
}

/*
 * This function is called when the user presses the "Add" button.
 * We collect all of the data from the GUI and pack it into an expense
 * record and then write it out.
 */
static void cb_add_new_record(GtkWidget *widget, gpointer data)
{
   struct Expense ex;
   buf_rec br;
   const char *text;
   unsigned char buf[0xFFFF];
   int size;
   int flag;
   int i;
   unsigned int unique_id = 0;
   struct MyExpense *mexp = NULL;
   GtkTextIter start_iter;
   GtkTextIter end_iter;

   jp_logf(JP_LOG_DEBUG, "Expense: cb_add_new_record\n");

   flag=GPOINTER_TO_INT(data);
   
   if (flag==CLEAR_FLAG) {
      exp_clear_details();
      connect_changed_signals(DISCONNECT_SIGNALS);
      set_new_button_to(NEW_FLAG);
      return;
   }
   if ((flag!=NEW_FLAG) && (flag!=MODIFY_FLAG) && (flag!=COPY_FLAG)) {
      return;
   }
   if (flag==MODIFY_FLAG) {
      mexp = gtk_clist_get_row_data(GTK_CLIST(clist), clist_row_selected);
      if (!mexp) {
         return;
      }
      unique_id = mexp->unique_id;
   }

   /* Grab details of record from widgets on right-hand side of screen */
   ex.type = glob_detail_type;
   ex.payment = glob_detail_payment;
   ex.currency = position_to_currency_id(glob_detail_currency_pos);

   /* gtk_entry_get_text *does not* allocate memory */
   text = gtk_entry_get_text(GTK_ENTRY(entry_amount));
   ex.amount = (char *)text;
   if (ex.amount[0]=='\0') {
      ex.amount=NULL;
   }

   text = gtk_entry_get_text(GTK_ENTRY(entry_vendor));
   ex.vendor = (char *)text;
   if (ex.vendor[0]=='\0') {
      ex.vendor=NULL;
   }

   text = gtk_entry_get_text(GTK_ENTRY(entry_city));
   ex.city = (char *)text;
   if (ex.city[0]=='\0') {
      ex.city=NULL;
   }

   ex.date.tm_mon  = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinner_mon)) - 1;;
   ex.date.tm_mday = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinner_day));;
   ex.date.tm_year = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinner_year)) - 1900;;
   ex.date.tm_hour = 12;
   ex.date.tm_min  = 0;
   ex.date.tm_sec  = 0;

   gtk_text_buffer_get_bounds(GTK_TEXT_BUFFER(attendees_buffer),&start_iter,&end_iter);
   ex.attendees = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(attendees_buffer),&start_iter,&end_iter,TRUE);
   if (ex.attendees[0]=='\0') {
      free(ex.attendees);
      ex.attendees=NULL;
   }

   gtk_text_buffer_get_bounds(GTK_TEXT_BUFFER(note_buffer),&start_iter,&end_iter);
   ex.note = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(note_buffer),&start_iter,&end_iter,TRUE);
   if (ex.note[0]=='\0') {
      free(ex.note);
      ex.note=NULL;
   }

   /*
    * The record must be packed into a palm record (blob of data)
    * pack_Expense just happens to be already written in pilot-link,
    * however, a pack function must be written for each record type.
    */
   size = pack_Expense(&ex, buf, 0xFFFF);

   if (ex.attendees) {
      free(ex.attendees);
   }
   if (ex.note) {
      free(ex.note);
   }
   
   /* This is a new record from the PC, and not yet on the palm */
   br.rt = NEW_PC_REC;
   
   /* jp_pc_write will give us a temporary PC unique ID. */
   /* The palm will give us an "official" unique ID during the sync */
   
   /* Any attributes go here.  Usually just the category */
   /* Get the category that is set from the menu */
   for (i=0; i<NUM_EXP_CAT_ITEMS; i++) {
      if (GTK_IS_WIDGET(exp_cat_menu_item2[i])) {
         if (GTK_CHECK_MENU_ITEM(exp_cat_menu_item2[i])->active) {
            br.attrib = sort_l[i].cat_num;
            break;
         }
      }
   }
   jp_logf(JP_LOG_DEBUG, "category is %d\n", br.attrib);
   br.buf = buf;
   br.size = size;
   br.unique_id = 0;

   if (flag==MODIFY_FLAG) {
      cb_delete(NULL, data);
      if ((mexp->rt==PALM_REC) || (mexp->rt==REPLACEMENT_PALM_REC)) {
         /* This code is to keep the unique ID intact */
         br.unique_id = unique_id;
         br.rt = REPLACEMENT_PALM_REC;
      }
   }

   /* Write out the record.  It goes to the .pc3 file until it gets synced */
   jp_pc_write("ExpenseDB", &br);

   set_new_button_to(CLEAR_FLAG);
   display_records();

   return;
}

/*
 * This function just adds the record to the clist on the left side of
 * the screen.
 */
static int display_record(struct MyExpense *mexp, int at_row)
{
   char *Ptype;
   char date[12];
   GdkColor color;
   GdkColormap *colormap;

   jp_logf(JP_LOG_DEBUG, "Expense: display_record\n");

   switch (mexp->rt) {
    case NEW_PC_REC:
    case REPLACEMENT_PALM_REC:
      colormap = gtk_widget_get_colormap(clist);
      color.red   = CLIST_NEW_RED;
      color.green = CLIST_NEW_GREEN;
      color.blue  = CLIST_NEW_BLUE;
      gdk_color_alloc(colormap, &color);
      gtk_clist_set_background(GTK_CLIST(clist), at_row, &color);
      break;
    case DELETED_PALM_REC:
    case DELETED_PC_REC:
      colormap = gtk_widget_get_colormap(clist);
      color.red   = CLIST_DEL_RED;
      color.green = CLIST_DEL_GREEN;
      color.blue  = CLIST_DEL_BLUE;
      gdk_color_alloc(colormap, &color);
      gtk_clist_set_background(GTK_CLIST(clist), at_row, &color);
      break;
    case MODIFIED_PALM_REC:
      colormap = gtk_widget_get_colormap(clist);
      color.red   = CLIST_MOD_RED;
      color.green = CLIST_MOD_GREEN;
      color.blue  = CLIST_MOD_BLUE;
      gdk_color_alloc(colormap, &color);
      gtk_clist_set_background(GTK_CLIST(clist), at_row, &color);
      break;
    default:
      if (mexp->attrib & dlpRecAttrSecret) {
         colormap = gtk_widget_get_colormap(clist);
         color.red   = CLIST_PRIVATE_RED;
         color.green = CLIST_PRIVATE_GREEN;
         color.blue  = CLIST_PRIVATE_BLUE;
         gdk_color_alloc(colormap, &color);
         gtk_clist_set_background(GTK_CLIST(clist), at_row, &color);
      } else {
         gtk_clist_set_background(GTK_CLIST(clist), at_row, NULL);
      }
   }

   gtk_clist_set_row_data(GTK_CLIST(clist), at_row, mexp);

   sprintf(date, "%02d/%02d", mexp->ex.date.tm_mon+1, mexp->ex.date.tm_mday);
   gtk_clist_set_text(GTK_CLIST(clist), at_row, 0, date);

   Ptype = get_entry_type(mexp->ex.type);
   gtk_clist_set_text(GTK_CLIST(clist), at_row, 1, Ptype);

   if (mexp->ex.amount) {
      gtk_clist_set_text(GTK_CLIST(clist), at_row, 2, mexp->ex.amount);
   }
   
   return EXIT_SUCCESS;
}

/*
 * This function lists the records in the clist on the left side of
 * the screen.
 */
static void display_records(void)
{
   int num, i;
   int entries_shown;
   struct MyExpense *mexp;
   GList *records;
   GList *temp_list;
   buf_rec *br;
   gchar *empty_line[] = { "","","" };
   
   jp_logf(JP_LOG_DEBUG, "Expense: display_records\n");

   records=NULL;
   
   free_myexpense_list(&glob_myexpense_list);

   /* Clear left-hand side of window */
   exp_clear_details();

   /* Freeze clist to prevent flicker during updating */
   gtk_clist_freeze(GTK_CLIST(clist));
   connect_changed_signals(DISCONNECT_SIGNALS);
   gtk_signal_disconnect_by_func(GTK_OBJECT(clist),
                                 GTK_SIGNAL_FUNC(cb_clist_selection), NULL);
   gtk_clist_clear(GTK_CLIST(clist));
#ifdef __APPLE__
   gtk_clist_thaw(GTK_CLIST(clist));
   gtk_widget_hide(clist);
   gtk_widget_show_all(clist);
   gtk_clist_freeze(GTK_CLIST(clist));
#endif

   /* This function takes care of reading the Database for us */
   num = jp_read_DB_files("ExpenseDB", &records);
   if (-1 == num)
      return;

   entries_shown = 0;
   for (i=0, temp_list = records; temp_list; temp_list = temp_list->next, i++) {
      if (temp_list->data) {
         br=temp_list->data;
      } else {
         continue;
      }
      if (!br->buf) {
         continue;
      }

      /* Since deleted and modified records are also returned and we don't
       * want to see those we skip over them. */
      if ((br->rt == DELETED_PALM_REC) || 
          (br->rt == DELETED_PC_REC)   ||
          (br->rt == MODIFIED_PALM_REC) ) {
         continue;
      }
      if (exp_category < NUM_EXP_CAT_ITEMS) {
         if ( ((br->attrib & 0x0F) != exp_category) &&
                exp_category != CATEGORY_ALL) {
            continue;
         }
      }
      
      mexp = malloc(sizeof(struct MyExpense));
      mexp->next = NULL;
      mexp->attrib = br->attrib;
      mexp->unique_id = br->unique_id;
      mexp->rt = br->rt;

      /* We need to unpack the record blobs from the database.
       * unpack_Expense is already written in pilot-link, but normally
       * an unpack must be written for each type of application */
      if (unpack_Expense(&(mexp->ex), br->buf, br->size)!=0) {
         gtk_clist_append(GTK_CLIST(clist), empty_line);
         display_record(mexp, entries_shown);
         entries_shown++;
      }

      /* Prepend entry at head of list */
      mexp->next = glob_myexpense_list;
      glob_myexpense_list = mexp;
   }

   jp_free_DB_records(&records);

   /* Sort the clist */
   gtk_clist_sort(GTK_CLIST(clist));

   gtk_signal_connect(GTK_OBJECT(clist), "select_row",
                      GTK_SIGNAL_FUNC(cb_clist_selection), NULL);
   
   /* Select the existing requested row, or row 0 if that is impossible */
   if (clist_row_selected <= entries_shown) {
      gtk_clist_select_row(GTK_CLIST(clist), clist_row_selected, 0);
      if (!gtk_clist_row_is_visible(GTK_CLIST(clist), clist_row_selected)) {
         gtk_clist_moveto(GTK_CLIST(clist), clist_row_selected, 0, 0.5, 0.0);
      }
   }
   else
   {
      gtk_clist_select_row(GTK_CLIST(clist), 0, 0);
   }

   /* Unfreeze clist after all changes */
   gtk_clist_thaw(GTK_CLIST(clist));

   connect_changed_signals(CONNECT_SIGNALS);

   jp_logf(JP_LOG_DEBUG, "Expense: leave display_records\n");
}

/* Find position of category in sorted category array 
 * via its assigned category number */
static int find_sort_cat_pos(int cat)
{
   int i;

   for (i=0; i<NUM_EXP_CAT_ITEMS; i++) {
      if (sort_l[i].cat_num==cat) {
         return i;
      }
   }

   return -1;
}

/* Find a category's position in the category menu.
 * This is equal to the category number except for the Unfiled category.
 * The Unfiled category is always in the last position which changes as
 * the number of categories changes */
static int find_menu_cat_pos(int cat)
{
   int i;

   if (cat != NUM_EXP_CAT_ITEMS-1) {
      return cat;
   } else { /* Unfiled category */
      /* Count how many category entries are filled */
      for (i=0; i<NUM_EXP_CAT_ITEMS; i++) {
         if (!sort_l[i].Pcat[0]) {
            return i;
         }
      }
      return 0;
   }
}

static void cb_edit_cats(GtkWidget *widget, gpointer data)
{
   struct ExpenseAppInfo ai;
   char full_name[256];
   char buffer[65536];
   int num;
   size_t size;
   void *buf;
   struct pi_file *pf;

   jp_logf(JP_LOG_DEBUG, "cb_edit_cats\n");

   jp_get_home_file_name("ExpenseDB.pdb", full_name, 250);

   buf=NULL;
   memset(&ai, 0, sizeof(ai));

   pf = pi_file_open(full_name);
   pi_file_get_app_info(pf, &buf, &size);

   num = unpack_ExpenseAppInfo(&ai, buf, size);
   if (num <= 0) {
      jp_logf(JP_LOG_WARN, _("Error reading file: %s\n"), "ExpenseDB.pdb");
      return;
   }

   pi_file_close(pf);

   jp_edit_cats(widget, "ExpenseDB", &(ai.category));

   size = pack_ExpenseAppInfo(&ai, (unsigned char *)buffer, 65535);

   jp_pdb_file_write_app_block("ExpenseDB", buffer, size);
   
   /* Force refresh and display of CATEGORY_ALL */
   plugin_gui_refresh(-1);
}

/* Called when left-hand category menu is used */
static void cb_category(GtkWidget *item, int selection)
{
   int b;

   if ((GTK_CHECK_MENU_ITEM(item))->active) {
      if (exp_category == selection) { return; }

      b=dialog_save_changed_record_with_cancel(pane, record_changed);
      if (b==DIALOG_SAID_1) { /* Cancel */
         int index, index2;

         if (exp_category==CATEGORY_ALL) {
            index  = 0;
            index2 = 0;
         } else {
            index  = find_sort_cat_pos(exp_category);
            index2 = find_menu_cat_pos(index) + 1;
            index += 1;
         }

         if (index<0) {
            jp_logf(JP_LOG_WARN, _("Category is not legal\n"));
         } else {
            gtk_check_menu_item_set_active
              (GTK_CHECK_MENU_ITEM(exp_cat_menu_item1[index]), TRUE);
            gtk_option_menu_set_history(GTK_OPTION_MENU(category_menu1), index2);
         }

         return;
      }
      if (b==DIALOG_SAID_3) { /* Save */
         cb_add_new_record(NULL, GINT_TO_POINTER(record_changed));
      }

      if (selection==NUM_EXP_CAT_ITEMS+1) {
         cb_edit_cats(item, NULL);
      } else {
         exp_category = selection;
      }
      jp_logf(JP_LOG_DEBUG, "cb_category() cat=%d\n", exp_category);

      clist_row_selected = 0;
      display_records();
      jp_logf(JP_LOG_DEBUG, "Leaving cb_category()\n");
   }
}

/*
 * This function just displays a record on the right hand side of the screen
 * (the details) after a row has been selected in the clist on the left.
 */
static void cb_clist_selection(GtkWidget      *clist,
                               gint           row,
                               gint           column,
                               GdkEventButton *event,
                               gpointer       data)
{
   struct MyExpense *mexp;
   int b;
   int index, sorted_position;
   int currency_position;
   unsigned int unique_id = 0;

   jp_logf(JP_LOG_DEBUG, "Expense: cb_clist_selection\n");

   if ((record_changed==MODIFY_FLAG) || (record_changed==NEW_FLAG)) {
      mexp = gtk_clist_get_row_data(GTK_CLIST(clist), row);
      if (mexp!=NULL) {
         unique_id = mexp->unique_id;
      }

      b=dialog_save_changed_record(scrolled_window, record_changed);
      if (b==DIALOG_SAID_2) {
         cb_add_new_record(NULL, GINT_TO_POINTER(record_changed));
      }
      set_new_button_to(CLEAR_FLAG);

      if (unique_id)
      {
         expense_find(unique_id);
      } else {
         clist_select_row(GTK_CLIST(clist), row, column);
      }
      return;
   }
   
   clist_row_selected = row;

   mexp = gtk_clist_get_row_data(GTK_CLIST(clist), row);
   if (mexp==NULL) {
      return;
   }

   set_new_button_to(CLEAR_FLAG);
   /* Need to disconnect signals while changing values */
   connect_changed_signals(DISCONNECT_SIGNALS);

   index = mexp->attrib & 0x0F;
   sorted_position = find_sort_cat_pos(index);
   if (exp_cat_menu_item2[sorted_position]==NULL) {
      /* Illegal category */
      jp_logf(JP_LOG_DEBUG, "Category is not legal\n");
      index = sorted_position = 0;
   }
   if (sorted_position<0) {
      jp_logf(JP_LOG_WARN, _("Category is not legal\n"));
   } else {
      gtk_check_menu_item_set_active
        (GTK_CHECK_MENU_ITEM(exp_cat_menu_item2[sorted_position]), TRUE);
   }
   gtk_option_menu_set_history(GTK_OPTION_MENU(category_menu2), 
                               find_menu_cat_pos(sorted_position));
   
   if (mexp->ex.type < MAX_EXPENSE_TYPES) {
      gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM
                                     (menu_item_expense_type[mexp->ex.type]), TRUE);
   } else {
      jp_logf(JP_LOG_WARN, _("Expense: Unknown expense type\n"));
   }
   if (mexp->ex.payment < MAX_PAYMENTS) {
      gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM
                                     (menu_item_payment[mexp->ex.payment]), TRUE);
   } else {
      jp_logf(JP_LOG_WARN, _("Expense: Unknown payment type\n"));
   }
   currency_position = currency_id_to_position(mexp->ex.currency);

   gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM
       (menu_item_currency[currency_position]), TRUE);
   gtk_option_menu_set_history(GTK_OPTION_MENU(menu_expense_type), mexp->ex.type);
   gtk_option_menu_set_history(GTK_OPTION_MENU(menu_payment), mexp->ex.payment);
   gtk_option_menu_set_history(GTK_OPTION_MENU(menu_currency), currency_position);
   
   gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinner_mon), mexp->ex.date.tm_mon+1);
   gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinner_day), mexp->ex.date.tm_mday);
   gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinner_year), mexp->ex.date.tm_year+1900);

   if (mexp->ex.amount) {
      gtk_entry_set_text(GTK_ENTRY(entry_amount), mexp->ex.amount);
   } else {
      gtk_entry_set_text(GTK_ENTRY(entry_amount), "");
   }
   
   if (mexp->ex.vendor) {
      gtk_entry_set_text(GTK_ENTRY(entry_vendor), mexp->ex.vendor);
   } else {
      gtk_entry_set_text(GTK_ENTRY(entry_vendor), "");
   }
   
   if (mexp->ex.city) {
      gtk_entry_set_text(GTK_ENTRY(entry_city), mexp->ex.city);
   } else {
      gtk_entry_set_text(GTK_ENTRY(entry_city), "");
   }
   
   gtk_text_buffer_set_text(GTK_TEXT_BUFFER(attendees_buffer), "", -1);

   if (mexp->ex.attendees) {
      gtk_text_buffer_set_text(GTK_TEXT_BUFFER(attendees_buffer), mexp->ex.attendees, -1);
   }

   gtk_text_buffer_set_text(GTK_TEXT_BUFFER(note_buffer), "", -1);
   if (mexp->ex.note) {
      gtk_text_buffer_set_text(GTK_TEXT_BUFFER(note_buffer), mexp->ex.note, -1);
   }

   connect_changed_signals(CONNECT_SIGNALS);

   jp_logf(JP_LOG_DEBUG, "Expense: leaving cb_clist_selection\n");
}

/*
 * All menus use this same callback function.  I use the value parameter
 * to determine which menu was changed and which item was selected from it.
 */
static void cb_pulldown_menu(GtkWidget *item, unsigned int value)
{
   int menu, sel;
   
   jp_logf(JP_LOG_DEBUG, "Expense: cb_pulldown_menu\n");

   if (!item) return; 
   if (!(GTK_CHECK_MENU_ITEM(item))->active) return;
   
   menu = (value & 0xFF00) >> 8;
   sel  =  value & 0x00FF;

   switch (menu) {
    case EXPENSE_TYPE:
      glob_detail_type = sel;
      break;
    case EXPENSE_PAYMENT:
      glob_detail_payment = sel;
      break;
    case EXPENSE_CURRENCY:
      glob_detail_currency_pos = sel;
      break;
   }
}

/*
 * Just a convenience function for passing in an array of strings and getting
 * them all stuffed into a menu.
 */
static int make_menu(const char *items[], int menu_index, GtkWidget **Poption_menu,
                     GtkWidget *menu_items[])
{
   int i, item_num;
   GSList *group;
   GtkWidget *option_menu;
   GtkWidget *menu_item;
   GtkWidget *menu;
   
   jp_logf(JP_LOG_DEBUG, "Expense: make_menu\n");

   *Poption_menu = option_menu = gtk_option_menu_new();
   
   menu = gtk_menu_new();

   group = NULL;
   
   for (i=0; items[i]; i++) {
      menu_item = gtk_radio_menu_item_new_with_label(group, _(items[i]));
      menu_items[i] = menu_item;
      item_num = i;
      gtk_signal_connect(GTK_OBJECT(menu_item), "activate",
                         GTK_SIGNAL_FUNC(cb_pulldown_menu), 
                         GINT_TO_POINTER(menu_index<<8 | item_num));
      group = gtk_radio_menu_item_group(GTK_RADIO_MENU_ITEM(menu_item));
      gtk_menu_append(GTK_MENU(menu), menu_item);
      gtk_widget_show(menu_item);
   }

   gtk_option_menu_set_menu(GTK_OPTION_MENU(option_menu), menu);
   /* Make this one show up by default */
   gtk_option_menu_set_history(GTK_OPTION_MENU(option_menu), 0);

   gtk_widget_show(option_menu);

   return EXIT_SUCCESS;
}

/* 
 * This function makes all of the menus on the screen.
 */
static void make_menus(void)
{
   struct ExpenseAppInfo exp_app_info;
   unsigned char *buf;
   int buf_size;
   int i;
   long char_set;
   char *cat_name;

   const char *payment[MAX_PAYMENTS+1]={
      N_("American Express"),
      N_("Cash"),
      N_("Check"),
      N_("Credit Card"),
      N_("Master Card"),
      N_("Prepaid"),
      N_("VISA"),
      N_("Unfiled"),
      NULL
   };
   const char *expense_type[MAX_CURRENCYS+1]={
      N_("Airfare"),
      N_("Breakfast"),
      N_("Bus"),
      N_("BusinessMeals"),
      N_("CarRental"),
      N_("Dinner"),
      N_("Entertainment"),
      N_("Fax"),
      N_("Gas"),
      N_("Gifts"),
      N_("Hotel"), 
      N_("Incidentals"),
      N_("Laundry"),
      N_("Limo"),
      N_("Lodging"),
      N_("Lunch"),
      N_("Mileage"),
      N_("Other"),
      N_("Parking"),
      N_("Postage"),
      N_("Snack"),
      N_("Subway"),
      N_("Supplies"),
      N_("Taxi"),
      N_("Telephone"),
      N_("Tips"),
      N_("Tolls"),
      N_("Train"),
      NULL
   };
   const char *currency[MAX_CURRENCYS+1];


   jp_logf(JP_LOG_DEBUG, "Expense: make_menus\n");

   /* Point the currency array to the country names and NULL terminate it */
   for (i=0; i<MAX_CURRENCYS; i++) {
      currency[i]=glob_currency[i].country;
   }
   currency[MAX_CURRENCYS]=NULL;

   /* Do some category initialization */
   for (i=0; i<NUM_EXP_CAT_ITEMS; i++) {
      exp_cat_menu_item2[i] = NULL;
   }

   /* This gets the application specific data out of the database for us.
    * We still need to write a function to unpack it from its blob form. */
   jp_get_app_info("ExpenseDB", &buf, &buf_size);
   unpack_ExpenseAppInfo(&exp_app_info, buf, buf_size);
   if (buf) {
      free(buf);
   }

   get_pref(PREF_CHAR_SET, &char_set, NULL);

   for (i=1; i<NUM_EXP_CAT_ITEMS; i++) {
      cat_name = charset_p2newj(exp_app_info.category.name[i], 31, char_set);
      strcpy(sort_l[i-1].Pcat, cat_name);
      free(cat_name);
      sort_l[i-1].cat_num = i;
   }
   /* put reserved 'Unfiled' category at end of list */ 
   cat_name = charset_p2newj(exp_app_info.category.name[0], 31, char_set);
   strcpy(sort_l[NUM_EXP_CAT_ITEMS-1].Pcat, cat_name);
   free(cat_name);
   sort_l[NUM_EXP_CAT_ITEMS-1].cat_num = 0;

   qsort(sort_l, NUM_EXP_CAT_ITEMS-1, sizeof(struct sorted_cats), cat_compare);

#ifdef JPILOT_DEBUG
   for (i=0; i<NUM_EXP_CAT_ITEMS; i++) {
      printf("cat %d [%s]\n", sort_l[i].cat_num, sort_l[i].Pcat);
   }
#endif

   if ((exp_category != CATEGORY_ALL) && (exp_app_info.category.name[exp_category][0]=='\0')) {
      exp_category=CATEGORY_ALL;
   }

   make_category_menu(&category_menu1, exp_cat_menu_item1,
                      sort_l, cb_category, TRUE, TRUE);
   /* Skip the ALL category for this menu */
   make_category_menu(&category_menu2, exp_cat_menu_item2,
                      sort_l, NULL, FALSE, FALSE);
   make_menu(payment, EXPENSE_PAYMENT, &menu_payment, menu_item_payment);
   make_menu(expense_type, EXPENSE_TYPE, &menu_expense_type, menu_item_expense_type);
   make_menu(currency, EXPENSE_CURRENCY, &menu_currency, menu_item_currency);
}

/* returns 1 if found, 0 if not found */
static int expense_clist_find_id(GtkWidget *clist,
                                 unsigned int unique_id,
                                 int *found_at)
{
   int i, found;
   struct MyExpense *mexp;

   jp_logf(JP_LOG_DEBUG, "Expense: expense_clist_find_id\n");
   
   *found_at = 0;
   for (found = i = 0; i<=GTK_CLIST(clist)->rows; i++) {
      mexp = gtk_clist_get_row_data(GTK_CLIST(clist), i);
      if (!mexp) {
         break;
      }
      if (mexp->unique_id==unique_id) {
         found = TRUE;
         *found_at = i;
         break;
      }
   }
   
   return found;
}

static int expense_find(int unique_id)
{
   int r, found_at;
   
   jp_logf(JP_LOG_DEBUG, "Expense: expense_find, unique_id=%d\n",unique_id);

   if (unique_id)
   {
      r = expense_clist_find_id(clist,
                        unique_id,
                        &found_at);
      if (r) {
         gtk_clist_select_row(GTK_CLIST(clist), found_at, 0);
         if (!gtk_clist_row_is_visible(GTK_CLIST(clist), found_at)) {
            gtk_clist_moveto(GTK_CLIST(clist), found_at, 0, 0.5, 0.0);
         }
      }
   }

   return EXIT_SUCCESS;
}

/*
 * This function is called by J-Pilot when the user selects this plugin
 * from the plugin menu, or from the search window when a search result
 * record is chosen.  In the latter case, unique ID will be set.  This
 * application should go directly to that record if the ID is set.
 */
int plugin_gui(GtkWidget *vbox, GtkWidget *hbox, unsigned int unique_id)
{
   GtkWidget *vbox1, *vbox2;
   GtkWidget *hbox_temp;
   GtkWidget *temp_vbox;
   GtkWidget *label;
   GtkWidget *separator;
   time_t ltime;
   struct tm *now;
   long ivalue;
   long show_tooltips;
   char *titles[]={"","",""};
   int i;
   int cycle_category=FALSE;
   int new_cat;
   int index, index2;

   jp_logf(JP_LOG_DEBUG, "Expense: plugin gui started, unique_id=%d\n", unique_id);

   record_changed = CLEAR_FLAG;

   if (difftime(time(NULL), plugin_last_time) > PLUGIN_MAX_INACTIVE_TIME) {
      cycle_category = FALSE;
   } else {
      cycle_category = TRUE;
   }
   /* reset time entered */
   plugin_last_time = time(NULL);

   /* called to display the result of a search */
   if (unique_id) {
      cycle_category = FALSE;
   }

   clist_row_selected = 0;

   time(&ltime);
   now = localtime(&ltime);

   /************************************************************/
   /* Build the GUI */

   get_pref(PREF_SHOW_TOOLTIPS, &show_tooltips, NULL);

   /* Make the menus */
   make_menus();

   pane = gtk_hpaned_new();
   get_pref(PREF_EXPENSE_PANE, &ivalue, NULL);
   gtk_paned_set_position(GTK_PANED(pane), ivalue);

   gtk_box_pack_start(GTK_BOX(hbox), pane, TRUE, TRUE, 5);

   /* left and right main boxes */
   vbox1 = gtk_vbox_new(FALSE, 0);
   vbox2 = gtk_vbox_new(FALSE, 0);
   gtk_paned_pack1(GTK_PANED(pane), vbox1, TRUE, FALSE);
   gtk_paned_pack2(GTK_PANED(pane), vbox2, TRUE, FALSE);

   gtk_widget_set_usize(GTK_WIDGET(vbox1), 0, 230);
   gtk_widget_set_usize(GTK_WIDGET(vbox2), 0, 230);

   /* Make accelerators for some buttons window */
#ifndef ENABLE_STOCK_BUTTONS
   accel_group = gtk_accel_group_new();
   gtk_window_add_accel_group(GTK_WINDOW(gtk_widget_get_toplevel(vbox)), accel_group);
#endif

   /************************************************************/
   /* Left half of screen */

   /* Make menu box for category menu */
   hbox_temp = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox1), hbox_temp, FALSE, FALSE, 0);
   gtk_box_pack_start(GTK_BOX(hbox_temp), category_menu1, TRUE, TRUE, 0);

   /* Scrolled Window */
   scrolled_window = gtk_scrolled_window_new(NULL, NULL);
   gtk_container_set_border_width(GTK_CONTAINER(scrolled_window), 0);
   gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
   gtk_box_pack_start(GTK_BOX(vbox1), scrolled_window, TRUE, TRUE, 0);
   
   /* Clist */
   clist = gtk_clist_new_with_titles(3, titles);

   gtk_clist_set_column_title(GTK_CLIST(clist), 0, _("Date"));
   gtk_clist_set_column_title(GTK_CLIST(clist), 1, _("Type"));
   gtk_clist_set_column_title(GTK_CLIST(clist), 2, _("Amount"));

   /* auto resize is used in all the other application clists but here
    * it produces a cramped layout so a minimum width is specified */
   gtk_clist_set_column_auto_resize(GTK_CLIST(clist), 0, TRUE);
   gtk_clist_set_column_min_width(GTK_CLIST(clist), 0, 44);
   gtk_clist_set_column_auto_resize(GTK_CLIST(clist), 1, TRUE);
   gtk_clist_set_column_min_width(GTK_CLIST(clist), 1, 100);
   gtk_clist_set_column_auto_resize(GTK_CLIST(clist), 2, FALSE);
  
   gtk_clist_column_titles_active(GTK_CLIST(clist));
   gtk_signal_connect(GTK_OBJECT(clist), "click_column",
                      GTK_SIGNAL_FUNC (cb_clist_click_column), NULL);

   gtk_signal_connect(GTK_OBJECT(clist), "select_row",
                      GTK_SIGNAL_FUNC(cb_clist_selection),
                      NULL);
   gtk_clist_set_shadow_type(GTK_CLIST(clist), SHADOW);
   gtk_clist_set_selection_mode(GTK_CLIST(clist), GTK_SELECTION_BROWSE);

   /* Restore previous sorting configuration */
   get_pref(PREF_EXPENSE_SORT_COLUMN, &ivalue, NULL);
   clist_col_selected = ivalue;
   gtk_clist_set_sort_column(GTK_CLIST(clist), clist_col_selected);
   switch (clist_col_selected) {
    case EXP_DATE_COLUMN:  /* Date column */
      gtk_clist_set_compare_func(GTK_CLIST(clist),sort_compare_date);
      break;
    default: /* All other columns can use GTK default sort function */
      gtk_clist_set_compare_func(GTK_CLIST(clist),NULL);
      break;
   }
   get_pref(PREF_EXPENSE_SORT_ORDER, &ivalue, NULL);
   gtk_clist_set_sort_type(GTK_CLIST(clist), ivalue);
   
   gtk_container_add(GTK_CONTAINER(scrolled_window), GTK_WIDGET(clist));
   
   /************************************************************/
   /* Right half of screen */
   hbox_temp = gtk_hbox_new(FALSE, 3);
   gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, FALSE, FALSE, 0);

   /* Delete, Copy, New, etc. buttons */
   CREATE_BUTTON(delete_record_button, _("Delete"), DELETE, _("Delete the selected record"), GDK_d, GDK_CONTROL_MASK, "Ctrl+D");
   gtk_signal_connect(GTK_OBJECT(delete_record_button), "clicked",
                      GTK_SIGNAL_FUNC(cb_delete),
                      GINT_TO_POINTER(DELETE_FLAG));
   
   CREATE_BUTTON(copy_record_button, _("Copy"), COPY, _("Copy the selected record"), GDK_c, GDK_CONTROL_MASK|GDK_SHIFT_MASK, "Ctrl+Shift+C")
   gtk_signal_connect(GTK_OBJECT(copy_record_button), "clicked",
                      GTK_SIGNAL_FUNC(cb_add_new_record), 
                      GINT_TO_POINTER(COPY_FLAG));

   CREATE_BUTTON(new_record_button, _("New Record"), NEW, _("Add a new record"), GDK_n, GDK_CONTROL_MASK, "Ctrl+N")
   gtk_signal_connect(GTK_OBJECT(new_record_button), "clicked",
                      GTK_SIGNAL_FUNC(cb_add_new_record),
                      GINT_TO_POINTER(CLEAR_FLAG));

   CREATE_BUTTON(add_record_button, _("Add Record"), ADD, _("Add the new record"), GDK_Return, GDK_CONTROL_MASK, "Ctrl+Enter")
   gtk_signal_connect(GTK_OBJECT(add_record_button), "clicked",
                      GTK_SIGNAL_FUNC(cb_add_new_record),
                      GINT_TO_POINTER(NEW_FLAG));
#ifndef ENABLE_STOCK_BUTTONS
   gtk_widget_set_name(GTK_WIDGET(GTK_LABEL(GTK_BIN(add_record_button)->child)),
                       "label_high");
#endif

   CREATE_BUTTON(apply_record_button, _("Apply Changes"), APPLY, _("Commit the modifications"), GDK_Return, GDK_CONTROL_MASK, "Ctrl+Enter")
   gtk_signal_connect(GTK_OBJECT(apply_record_button), "clicked",
                      GTK_SIGNAL_FUNC(cb_add_new_record),
                      GINT_TO_POINTER(MODIFY_FLAG));
#ifndef ENABLE_STOCK_BUTTONS
   gtk_widget_set_name(GTK_WIDGET(GTK_LABEL(GTK_BIN(apply_record_button)->child)),
                       "label_high");
#endif
   
   /*Separator */
   separator = gtk_hseparator_new();
   gtk_box_pack_start(GTK_BOX(vbox2), separator, FALSE, FALSE, 5);

   table = gtk_table_new(8, 2, FALSE);
   gtk_box_pack_start(GTK_BOX(vbox2), table, FALSE, FALSE, 0);

   /* Category Menu */
   label = gtk_label_new(_("Category:"));
   gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
   gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(label),
                    0, 1, 0, 1, GTK_FILL, GTK_FILL, 2, 0);
   gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(category_menu2),
                             1, 2, 0, 1);

   /* Type Menu */
   label = gtk_label_new(_("Type:"));
   gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
   gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(label),
                    0, 1, 1, 2, GTK_FILL, GTK_FILL, 2, 0);
   gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(menu_expense_type),
                             1, 2, 1, 2);

   /* Payment Menu */
   label = gtk_label_new(_("Payment:"));
   gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
   gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(label),
                    0, 1, 2, 3, GTK_FILL, GTK_FILL, 2, 0);
   gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(menu_payment),
                             1, 2, 2, 3);

   /* Currency Menu */
   label = gtk_label_new(_("Currency:"));
   gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
   gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(label),
                    0, 1, 3, 4, GTK_FILL, GTK_FILL, 2, 0);
   gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(menu_currency),
                             1, 2, 3, 4);

   /* Date Spinners */
   label = gtk_label_new(_("Date:"));
   gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.8);
   gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(label),
                    0, 1, 4, 5, GTK_FILL, GTK_FILL, 2, 0);

   hbox_temp = gtk_hbox_new(FALSE, 0);
   gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(hbox_temp),
                             1, 2, 4, 5);

   /* Month spinner */
   temp_vbox = gtk_vbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(hbox_temp), temp_vbox, FALSE, FALSE, 0);
   label = gtk_label_new(_("Month:"));
   gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
   gtk_box_pack_start(GTK_BOX(temp_vbox), label, FALSE, TRUE, 0);

   adj_mon = GTK_ADJUSTMENT(gtk_adjustment_new(now->tm_mon+1, 1.0, 12.0, 1.0,
                                               5.0, 0.0));
   spinner_mon = gtk_spin_button_new(adj_mon, 0, 0);
   gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(spinner_mon), FALSE);
   gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spinner_mon), TRUE);
   gtk_box_pack_start(GTK_BOX(temp_vbox), spinner_mon, FALSE, TRUE, 0);

   /* Day spinner */
   temp_vbox = gtk_vbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(hbox_temp), temp_vbox, FALSE, FALSE, 0);
   label = gtk_label_new(_("Day:"));
   gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
   gtk_box_pack_start(GTK_BOX(temp_vbox), label, FALSE, TRUE, 0);

   adj_day = GTK_ADJUSTMENT(gtk_adjustment_new(now->tm_mday, 1.0, 31.0, 1.0,
                                               5.0, 0.0));
   spinner_day = gtk_spin_button_new(adj_day, 0, 0);
   gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(spinner_day), FALSE);
   gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spinner_day), TRUE);
   gtk_box_pack_start(GTK_BOX(temp_vbox), spinner_day, FALSE, TRUE, 0);

   /* Year spinner */
   temp_vbox = gtk_vbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(hbox_temp), temp_vbox, FALSE, FALSE, 0);
   label = gtk_label_new(_("Year:"));
   gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
   gtk_box_pack_start(GTK_BOX(temp_vbox), label, FALSE, TRUE, 0);

   adj_year = GTK_ADJUSTMENT(gtk_adjustment_new(now->tm_year+1900, 0.0, 2037.0,
                                                1.0, 100.0, 0.0));
   spinner_year = gtk_spin_button_new(adj_year, 0, 0);
   gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(spinner_year), FALSE);
   gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spinner_year), TRUE);
   gtk_box_pack_start(GTK_BOX(temp_vbox), spinner_year, FALSE, TRUE, 0);
   gtk_widget_set_usize(spinner_year, 55, 0);

   /* Amount Entry */
   label = gtk_label_new(_("Amount:"));
   gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
   gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(label),
                    0, 1, 5, 6, GTK_FILL, GTK_FILL, 2, 0);
   entry_amount = gtk_entry_new();
   entry_set_multiline_truncate(GTK_ENTRY(entry_amount), TRUE);
   gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(entry_amount),
                             1, 2, 5, 6);

   /* Vendor Entry */
   label = gtk_label_new(_("Vendor:"));
   gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
   gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(label),
                    0, 1, 6, 7, GTK_FILL, GTK_FILL, 2, 0);
   entry_vendor = gtk_entry_new();
   entry_set_multiline_truncate(GTK_ENTRY(entry_vendor), TRUE);
   gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(entry_vendor),
                             1, 2, 6, 7);

   /* City */
   label = gtk_label_new(_("City:"));
   gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);
   gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(label),
                    0, 1, 7, 8, GTK_FILL, GTK_FILL, 2, 0);
   entry_city = gtk_entry_new();
   entry_set_multiline_truncate(GTK_ENTRY(entry_city), TRUE);
   gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(entry_city),
                             1, 2, 7, 8);

   /* Attendees */
   label = gtk_label_new(_("Attendees"));
   gtk_box_pack_start(GTK_BOX(vbox2), label, FALSE, FALSE, 0);

   /* Attendees textbox */
   scrolled_window = gtk_scrolled_window_new(NULL, NULL);
   /*gtk_widget_set_usize(GTK_WIDGET(scrolled_window), 150, 0); */
   gtk_container_set_border_width(GTK_CONTAINER(scrolled_window), 0);
   gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
   gtk_box_pack_start(GTK_BOX(vbox2), scrolled_window, TRUE, TRUE, 0);

   attendees = gtk_text_view_new();
   attendees_buffer = G_OBJECT(gtk_text_view_get_buffer(GTK_TEXT_VIEW(attendees)));
   gtk_text_view_set_editable(GTK_TEXT_VIEW(attendees), TRUE);
   gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(attendees), GTK_WRAP_WORD);
   gtk_container_add(GTK_CONTAINER(scrolled_window), GTK_WIDGET(attendees));

   label = gtk_label_new(_("Note"));
   gtk_box_pack_start(GTK_BOX(vbox2), label, FALSE, FALSE, 0);

   /* Note textbox */
   scrolled_window = gtk_scrolled_window_new(NULL, NULL);
   /*gtk_widget_set_usize(GTK_WIDGET(scrolled_window), 150, 0); */
   gtk_container_set_border_width(GTK_CONTAINER(scrolled_window), 0);
   gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
   gtk_box_pack_start(GTK_BOX(vbox2), scrolled_window, TRUE, TRUE, 0);

   note = gtk_text_view_new();
   note_buffer = G_OBJECT(gtk_text_view_get_buffer(GTK_TEXT_VIEW(note)));
   gtk_text_view_set_editable(GTK_TEXT_VIEW(note), TRUE);
   gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(note), GTK_WRAP_WORD);
   gtk_container_add(GTK_CONTAINER(scrolled_window), GTK_WIDGET(note));

   gtk_widget_show_all(hbox);
   gtk_widget_show_all(vbox);
   
   gtk_widget_hide(add_record_button);
   gtk_widget_hide(apply_record_button);

   if (cycle_category) {
      /* First cycle exp_category var */
      if (exp_category == CATEGORY_ALL) {
         new_cat = -1;
      } else {
         new_cat = find_sort_cat_pos(exp_category);
      }
      for (i=0; i<NUM_EXP_CAT_ITEMS; i++) {
         new_cat++;
         if (new_cat >= NUM_EXP_CAT_ITEMS) {
            exp_category = CATEGORY_ALL;
            break;
         }
         if ((sort_l[new_cat].Pcat) && (sort_l[new_cat].Pcat[0])) {
            exp_category = sort_l[new_cat].cat_num;
            break;
         }
      }
      /* Then update menu with new exp_category */
      if (exp_category==CATEGORY_ALL) {
         index  = 0;
         index2 = 0; 
      } else {
         index  = find_sort_cat_pos(exp_category);
         index2 = find_menu_cat_pos(index) + 1;
         index += 1;
      }
      if (index<0) {
         jp_logf(JP_LOG_WARN, _("Category is not legal\n"));
      } else {
         gtk_check_menu_item_set_active
           (GTK_CHECK_MENU_ITEM(exp_cat_menu_item1[index]), TRUE);
         gtk_option_menu_set_history(GTK_OPTION_MENU(category_menu1), index2);
      }
   }
   else
   {
      exp_category = CATEGORY_ALL;
   }

   /* The focus doesn't do any good on the application button */
   gtk_widget_grab_focus(GTK_WIDGET(clist));

   jp_logf(JP_LOG_DEBUG, "Expense: calling display_records\n");
   display_records();
   jp_logf(JP_LOG_DEBUG, "Expense: after display_records\n");
   
   if (unique_id) {
      expense_find(unique_id);
   }

   return EXIT_SUCCESS;
}

/*
 * This function is called by J-Pilot before switching to another application.
 * The plugin is expected to perform any desirable cleanup operations before
 * its windows are terminated.  Desirable actions include freeing allocated 
 * memory, storing state variables, etc. 
 */
int plugin_gui_cleanup(void) {
   int b;
   
   jp_logf(JP_LOG_DEBUG, "Expense: plugin_gui_cleanup\n");

   b=dialog_save_changed_record(scrolled_window, record_changed);
   if (b==DIALOG_SAID_2) {
      cb_add_new_record(NULL, GINT_TO_POINTER(record_changed));
   }

   connect_changed_signals(DISCONNECT_SIGNALS);

   free_myexpense_list(&glob_myexpense_list);

   if (pane) {
      /* Remove the accelerators */
#ifndef ENABLE_STOCK_BUTTONS
      gtk_window_remove_accel_group(GTK_WINDOW(gtk_widget_get_toplevel(pane)), accel_group);
#endif

      set_pref(PREF_EXPENSE_PANE, gtk_paned_get_position(GTK_PANED(pane)), NULL, TRUE);
      pane = NULL;
   }
   set_pref(PREF_EXPENSE_SORT_COLUMN, clist_col_selected, NULL, TRUE);
   set_pref(PREF_EXPENSE_SORT_ORDER, GTK_CLIST(clist)->sort_type, NULL, TRUE);

   plugin_last_time = time(NULL);

   return EXIT_SUCCESS;
}

/*
 * This is a plugin callback function that is executed when J-Pilot starts up.
 * base_dir is where J-Pilot is compiled to be installed at (e.g. /usr/local/)
 */
int plugin_startup(jp_startup_info *info)
{
   jp_init();

   jp_logf(JP_LOG_DEBUG, "Expense: plugin_startup\n");
   if (info) {
      if (info->base_dir) {
         jp_logf(JP_LOG_DEBUG, "Expense: base_dir = [%s]\n", info->base_dir);
      }
   }

   return EXIT_SUCCESS;
}

/*
 * This is a plugin callback function that is executed before a sync occurs.
 * Any sync preperation can be done here.
 */
int plugin_pre_sync(void)
{
   jp_logf(JP_LOG_DEBUG, "Expense: plugin_pre_sync\n");
   return EXIT_SUCCESS;
}

/*
 * This is a plugin callback function that is executed during a sync.
 * Notice that I don't need to sync the Expense application.  Since I used
 * the plugin_get_db_name call to tell J-Pilot what to sync for me.  It will
 * be done automatically.
 */
int plugin_sync(int sd)
{
   jp_logf(JP_LOG_DEBUG, "Expense: plugin_sync\n");
   return EXIT_SUCCESS;
}

static int add_search_result(const char *line, 
                             int unique_id, 
                             struct search_result **sr)
{
   struct search_result *temp_sr;

   jp_logf(JP_LOG_DEBUG, "Expense: add_search_result for [%s]\n", line);

   temp_sr=malloc(sizeof(struct search_result));
   if (!temp_sr) {
      return EXIT_FAILURE;
   }
   temp_sr->unique_id=unique_id;
   temp_sr->line=strdup(line);
   temp_sr->next = *sr;
   *sr = temp_sr;

   return 0;
}

/*
 * This function is called when the user does a search.  It should return
 * records which match the search string.
 */
int plugin_search(const char *search_string, int case_sense, struct search_result **sr)
{
   GList *records;
   GList *temp_list;
   buf_rec *br;
   struct MyExpense mexp;
   int num, count;
   char *line;
   
   jp_logf(JP_LOG_DEBUG, "Expense: plugin_search\n");

   records = NULL;
   *sr = NULL;

   /* This function takes care of reading the Database for us */
   num = jp_read_DB_files("ExpenseDB", &records);
   if (-1 == num)
      return 0;

   count = 0;
   
   for (temp_list = records; temp_list; temp_list = temp_list->next) {
      if (temp_list->data) {
         br=temp_list->data;
      } else {
         continue;
      }
      if (!br->buf) {
         continue;
      }
      /* Since deleted and modified records are also returned and we don't
       * want to see those we skip over them. */
      if ((br->rt == DELETED_PALM_REC) || 
          (br->rt == DELETED_PC_REC)   ||
          (br->rt == MODIFIED_PALM_REC)) {
         continue;
      }
      
      mexp.attrib = br->attrib;
      mexp.unique_id = br->unique_id;
      mexp.rt = br->rt;

      /* We need to unpack the record blobs from the database.
       * unpack_Expense is already written in pilot-link, but normally
       * an unpack must be written for each type of application */
      if (unpack_Expense(&(mexp.ex), br->buf, br->size)!=0) {
         line = NULL;

         if (jp_strstr(mexp.ex.amount, search_string, case_sense))
            line = mexp.ex.amount;

         if (jp_strstr(mexp.ex.vendor, search_string, case_sense))
            line = mexp.ex.vendor;

         if (jp_strstr(mexp.ex.city, search_string, case_sense))
            line = mexp.ex.city;

         if (jp_strstr(mexp.ex.attendees, search_string, case_sense))
            line = mexp.ex.attendees;

         if (jp_strstr(mexp.ex.note, search_string, case_sense))
            line = mexp.ex.note;

         if (line) {
            /* Add it to our result list */
            jp_logf(JP_LOG_DEBUG, "Expense: calling add_search_result\n");
            add_search_result(line, br->unique_id, sr);
            jp_logf(JP_LOG_DEBUG, "Expense: back from add_search_result\n");
            count++;
         }
         free_Expense(&(mexp.ex));
      }
   }
   jp_free_DB_records(&records);

   return count;
}

int plugin_help(char **text, int *width, int *height)
{
   /* We could also pass back *text=NULL
    * and implement whatever we wanted to here.
    */
   char plugin_name[200];

   static_plugin_get_name(plugin_name, sizeof(plugin_name));
   *text = g_strdup_printf(
      /*-------------------------------------------*/
      _("%s\n"
        "\n"
        "Expense plugin for J-Pilot was written by\n"
        "Judd Montgomery (c) 1999.\n"
        "judd@jpilot.org, http://jpilot.org"
        ),
        plugin_name
      );
   *height = 0;
   *width = 0;
   
   return EXIT_SUCCESS;
}
         
/*
 * This is a plugin callback function called after a sync.
 */
int plugin_post_sync(void)
{
   jp_logf(JP_LOG_DEBUG, "Expense: plugin_post_sync\n");
   return EXIT_SUCCESS;
}

/*
 * This is a plugin callback function called during program exit.
 */
int plugin_exit_cleanup(void)
{
   jp_logf(JP_LOG_DEBUG, "Expense: plugin_exit_cleanup\n");
   return EXIT_SUCCESS;
}


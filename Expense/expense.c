/* expense.c
 *
 * This is an example plugin for J-Pilot to demonstrate the plugin API.
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <gtk/gtk.h>

#include "libplugin.h"
#include "../i18n.h"

#include <pi-expense.h>
#include <pi-dlp.h>

#define EXPENSE_CAT1 1
#define EXPENSE_CAT2 2
#define EXPENSE_TYPE 3
#define EXPENSE_PAYMENT 4
#define EXPENSE_CURRENCY 5

#define CATEGORY_ALL 200

#define CONNECT_SIGNALS 400
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

/* Global vars */
/* This is the category that is currently being displayed */
static int show_category;
static int glob_category_number_from_menu_item[16];
static void cb_clist_selection(GtkWidget      *clist,
			       gint           row,
			       gint           column,
			       GdkEventButton *event,
			       gpointer       data);
static void cb_add_new_record(GtkWidget *widget, gpointer data);

static GtkWidget *clist;
static GtkWidget *entry_amount;
static GtkWidget *entry_vendor;
static GtkWidget *entry_city;
static GtkWidget *text_attendees;
static GtkWidget *text_note;
static GtkWidget *menu_payment;
static GtkWidget *menu_item_payment[8];
static GtkWidget *menu_category1;
static GtkWidget *menu_category2;
static GtkWidget *menu_item_category2[16];
static GtkWidget *menu_expense_type;
static GtkWidget *menu_item_expense_type[28];
static GtkWidget *spinner_mon, *spinner_day, *spinner_year;
static GtkAdjustment *adj_mon, *adj_day, *adj_year;
static GtkWidget *scrolled_window;
static GtkWidget *new_record_button;
static GtkWidget *apply_record_button;
static GtkWidget *add_record_button;

static int record_changed;
static int clist_hack;

static int glob_detail_category;
static int glob_detail_type;
static int glob_detail_payment;
static int clist_row_selected;

static void display_records();

static void connect_changed_signals(int con_or_dis);

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

struct MyExpense *glob_myexpense_list=NULL;

static int move_scrolled_window(GtkWidget *sw, float percentage);

struct move_sw {
   float percentage;
   GtkWidget *sw;
};

static gint cb_timer_move_scrolled_window(gpointer data)
{
   struct move_sw *move_this;
   int r;
   
   move_this = data;
   r = move_scrolled_window(move_this->sw, move_this->percentage);
   /*if we return TRUE then this function will get called again */
   /*if we return FALSE then it will be taken out of timer */
   if (r) {
      return TRUE;
   } else {
      return FALSE;
   }
}

static void move_scrolled_window_hack(GtkWidget *sw, float percentage)
{
   /*This is so that the caller doesn't have to worry about making */
   /*sure they use a static variable (required for callback) */
   static struct move_sw move_this;

   move_this.percentage = percentage;
   move_this.sw = sw;
   
   gtk_timeout_add(50, cb_timer_move_scrolled_window, &move_this);
}

static int move_scrolled_window(GtkWidget *sw, float percentage)
{
   GtkScrollbar *sb;
   gfloat upper, lower, page_size, new_val;

   if (!GTK_IS_SCROLLED_WINDOW(sw)) {
      return 0;
   }
   sb = GTK_SCROLLBAR(GTK_SCROLLED_WINDOW(sw)->vscrollbar);
   upper = GTK_ADJUSTMENT(sb->range.adjustment)->upper;
   lower = GTK_ADJUSTMENT(sb->range.adjustment)->lower;
   page_size = GTK_ADJUSTMENT(sb->range.adjustment)->page_size;
   
   /*The screen isn't done drawing yet, so we have to leave. */
   if (page_size == 0) {
      return 1;
   }
   new_val = (upper - lower) * percentage;
   if (new_val > upper - page_size) {
      new_val = upper - page_size;
   }
   gtk_adjustment_set_value(sb->range.adjustment, new_val);
   gtk_signal_emit_by_name(GTK_OBJECT(sb->range.adjustment), "changed");

   return 0;
}


static void
set_new_button_to(int new_state)
{
   jpilot_logf(LOG_DEBUG, "set_new_button_to new %d old %d\n", new_state, record_changed);
   if (record_changed==new_state) {
      return;
   }

   switch (new_state) {
    case MODIFY_FLAG:
      gtk_clist_set_selection_mode(GTK_CLIST(clist), GTK_SELECTION_SINGLE);
      clist_hack=TRUE;
      /* The line selected on the clist becomes unhighlighted, so we do this */
      gtk_clist_select_row(GTK_CLIST(clist), clist_row_selected, 0);
      gtk_widget_show(apply_record_button);
      break;
    case NEW_FLAG:
      gtk_clist_set_selection_mode(GTK_CLIST(clist), GTK_SELECTION_SINGLE);
      clist_hack=TRUE;
      /* The line selected on the clist becomes unhighlighted, so we do this */
      gtk_clist_select_row(GTK_CLIST(clist), clist_row_selected, 0);
      gtk_widget_show(add_record_button);
      break;
    case CLEAR_FLAG:
      gtk_clist_set_selection_mode(GTK_CLIST(clist), GTK_SELECTION_BROWSE);
      clist_hack=FALSE;
      gtk_widget_show(new_record_button);
      break;
    default:
      return;
   }
   switch (record_changed) {
    case MODIFY_FLAG:
      gtk_widget_hide(apply_record_button);
      break;
    case NEW_FLAG:
      gtk_widget_hide(add_record_button);
      break;
    case CLEAR_FLAG:
      gtk_widget_hide(new_record_button);
      break;
   }
   record_changed=new_state;
}

static void
cb_record_changed(GtkWidget *widget,
		  gpointer   data)
{
   jpilot_logf(LOG_DEBUG, "cb_record_changed\n");
   if (record_changed==CLEAR_FLAG) {
      connect_changed_signals(DISCONNECT_SIGNALS);
      if (((GtkCList *)clist)->rows > 0) {
	 set_new_button_to(MODIFY_FLAG);
      } else {
	 set_new_button_to(NEW_FLAG);
      }
   }
}

static void connect_changed_signals(int con_or_dis)
{
   static int connected=0;

   /* CONNECT */
   if ((con_or_dis==CONNECT_SIGNALS) && (!connected)) {
      jp_logf(LOG_DEBUG, "Expense: connect_changed_signals\n");
      connected=1;

      gtk_signal_connect(GTK_OBJECT(spinner_mon), "changed",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_connect(GTK_OBJECT(spinner_day), "changed",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_connect(GTK_OBJECT(spinner_year), "changed",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_connect(GTK_OBJECT(text_attendees), "changed",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_connect(GTK_OBJECT(text_note), "changed",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_connect(GTK_OBJECT(entry_amount), "changed",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_connect(GTK_OBJECT(entry_vendor), "changed",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_connect(GTK_OBJECT(entry_city), "changed",
			 GTK_SIGNAL_FUNC(cb_record_changed), NULL);
   }

   /* DISCONNECT */
   if ((con_or_dis==DISCONNECT_SIGNALS) && (connected)) {
      jp_logf(LOG_DEBUG, "Expense: disconnect_changed_signals\n");
      connected=0;

      gtk_signal_disconnect_by_func(GTK_OBJECT(spinner_mon),
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_disconnect_by_func(GTK_OBJECT(spinner_day),
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_disconnect_by_func(GTK_OBJECT(spinner_year),
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_disconnect_by_func(GTK_OBJECT(text_attendees),
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_disconnect_by_func(GTK_OBJECT(text_note),
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_disconnect_by_func(GTK_OBJECT(entry_amount),
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_disconnect_by_func(GTK_OBJECT(entry_vendor),
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
      gtk_signal_disconnect_by_func(GTK_OBJECT(entry_city),
				    GTK_SIGNAL_FUNC(cb_record_changed), NULL);
   }
}

static void free_myexpense_list(struct MyExpense **PPme)
{
   struct MyExpense *me, *next_me;

   jp_logf(LOG_DEBUG, "Expense: free_myexpense_list\n");
   for (me = *PPme; me; me=next_me) {
      next_me = me->next;
      free(me);
   }
   *PPme=NULL;
}

/*
 * This is a mandatory plugin function.
 */
void plugin_version(int *major_version, int *minor_version)
{
   *major_version=0;
   *minor_version=99;
}

/*
 * This is a mandatory plugin function.
 */
int plugin_get_name(char *name, int len)
{
   jp_logf(LOG_DEBUG, "Expense: plugin_get_name\n");
   strncpy(name, "Expense 0.99", len);
   return 0;
}

/*
 * This is an optional plugin function.
 * This is the name that will show up in the plugins menu in J-Pilot.
 */
int plugin_get_menu_name(char *name, int len)
{
   strncpy(name, _("Expense"), len);
   return 0;
}

/*
 * This is an optional plugin function.
 * This is the name that will show up in the plugins help menu in J-Pilot.
 * If this function is used then plugin_help must be also.
 */
int plugin_get_help_name(char *name, int len)
{
   strncpy(name, _("About Expense"), len);
   return 0;
}

/*
 * This is an optional plugin function.
 * This is the palm database that will automatically be synced.
 */
int plugin_get_db_name(char *name, int len)
{
   strncpy(name, "ExpenseDB", len);
   return 0;
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

#ifdef JPILOT_DEBUG
/*
 * A utility function for getting textual data from an enum.
 */
static char *get_pay_type(enum ExpensePayment type)
{
   switch (type) {
    case epAmEx:
      return _("AmEx");
    case epCash:
      return _("Cash");
    case epCheck:
      return _("Check");
    case epCreditCard:
      return _("CreditCard");
    case epMasterCard:
      return _("MasterCard");
    case epPrepaid:
      return _("Prepaid");
    case epVISA:
      return _("VISA");
    case epUnfiled:
      return _("Unfiled");
    default:
      return NULL;
   }
}
#endif

/* This function gets called when the "delete" button is pressed */
static void cb_delete(GtkWidget *widget, int data)
{
   struct MyExpense *mex;
   int size;
   char buf[0xFFFF];
   buf_rec br;
   int flag;

   jp_logf(LOG_DEBUG, "Expense: cb_delete\n");

   flag=GPOINTER_TO_INT(data);

   mex = gtk_clist_get_row_data(GTK_CLIST(clist), clist_row_selected);
   if (!mex) {
      return;
   }

   connect_changed_signals(DISCONNECT_SIGNALS);
   set_new_button_to(CLEAR_FLAG);

   /* The record that we want to delete should be written to the pc file
    * so that it can be deleted at sync time.  We need the original record
    * so that if it has changed on the pilot we can warn the user that
    * the record has changed on the pilot. */
   size = pack_Expense(&(mex->ex), buf, 0xFFFF);
   
   br.rt = mex->rt;
   br.unique_id = mex->unique_id;
   br.attrib = mex->attrib;
   br.buf = buf;
   br.size = size;

   if ((flag==MODIFY_FLAG) || (flag==DELETE_FLAG)) {
      jp_delete_record("ExpenseDB", &br, DELETE_FLAG);
   }

   display_records();
		 
   connect_changed_signals(CONNECT_SIGNALS);
}

/*
 * This is called when the "Clear" button is pressed.
 * It just clears out all the detail fields.
 */
static void clear_details()
{
   time_t ltime;
   struct tm *now;
   
   time(&ltime);
   now = localtime(&ltime);
   
   jp_logf(LOG_DEBUG, "Expense: cb_clear\n");

   connect_changed_signals(DISCONNECT_SIGNALS);
   set_new_button_to(NEW_FLAG);

   gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinner_mon), now->tm_mon+1);
   gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinner_day), now->tm_mday);
   gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinner_year), now->tm_year+1900);

   gtk_entry_set_text(GTK_ENTRY(entry_amount), "");
   gtk_entry_set_text(GTK_ENTRY(entry_vendor), "");
   gtk_entry_set_text(GTK_ENTRY(entry_city), "");
   gtk_text_backward_delete(GTK_TEXT(text_attendees),
			    gtk_text_get_length(GTK_TEXT(text_attendees)));
   gtk_text_backward_delete(GTK_TEXT(text_note),
			    gtk_text_get_length(GTK_TEXT(text_note)));

   connect_changed_signals(CONNECT_SIGNALS);
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
   char *text;
   unsigned char buf[0xFFFF];
   int size;
   int flag;
   
   jp_logf(LOG_DEBUG, "Expense: cb_add_new_record\n");

   flag=GPOINTER_TO_INT(data);
   
   if (flag==CLEAR_FLAG) {
      connect_changed_signals(DISCONNECT_SIGNALS);
      clear_details();
      set_new_button_to(NEW_FLAG);
      return;
   }
   if ((flag!=NEW_FLAG) && (flag!=MODIFY_FLAG) && (flag!=COPY_FLAG)) {
      return;
   }

   ex.date.tm_mon  = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinner_mon)) - 1;;
   ex.date.tm_mday = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinner_day));;
   ex.date.tm_year = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinner_year)) - 1900;;
   ex.date.tm_hour = 12;
   ex.date.tm_min  = 0;
   ex.date.tm_sec  = 0;

   ex.type = glob_detail_type;
   ex.payment = glob_detail_payment;
   ex.currency=23;
   text = gtk_entry_get_text(GTK_ENTRY(entry_amount));
   ex.amount = text;
   if (ex.amount[0]=='\0') {
      ex.amount=NULL;
   }
   /* gtk_entry_get_text *does not* allocate memory */
   text = gtk_entry_get_text(GTK_ENTRY(entry_vendor));
   ex.vendor = text;
   if (ex.vendor[0]=='\0') {
      ex.vendor=NULL;
   }
   text = gtk_entry_get_text(GTK_ENTRY(entry_city));
   ex.city = text;
   if (ex.city[0]=='\0') {
      ex.city=NULL;
   }
   /* gtk_editable_get_chars *does* allocate memory */
   ex.attendees = gtk_editable_get_chars(GTK_EDITABLE(text_attendees), 0, -1);
   if (ex.attendees[0]=='\0') {
      ex.attendees=NULL;
   }
   ex.note = gtk_editable_get_chars(GTK_EDITABLE(text_note), 0, -1);
   if (ex.note[0]=='\0') {
      ex.note=NULL;
   }

   /* The record must be packed into a palm record (blob of data)
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
   
   br.unique_id = 0;
   /* Any attributes go here.  Usually just the category */

   br.attrib = glob_category_number_from_menu_item[glob_detail_category];
   jp_logf(LOG_DEBUG, "category is %d\n", br.attrib);
   br.buf = buf;
   br.size = size;

   /* Write out the record.  It goes to the .pc3 file until it gets synced */
   jp_pc_write("ExpenseDB", &br);

   connect_changed_signals(CONNECT_SIGNALS);
   set_new_button_to(CLEAR_FLAG);
   
   if (flag==MODIFY_FLAG) {
      cb_delete(NULL, MODIFY_FLAG);
   } else {
      display_records();
   }

   return;
}

/*
 * This function just adds the record to the clist on the left side of
 * the screen.
 */
static int display_record(struct MyExpense *mex, int at_row)
{
   char *Ptype;
   char date[12];
   GdkColor color;
   GdkColormap *colormap;

   /* jp_logf(LOG_DEBUG, "Expense: display_record\n");*/

   switch (mex->rt) {
    case NEW_PC_REC:
      colormap = gtk_widget_get_colormap(clist);
      color.red=CLIST_NEW_RED;
      color.green=CLIST_NEW_GREEN;
      color.blue=CLIST_NEW_BLUE;
      gdk_color_alloc(colormap, &color);
      gtk_clist_set_background(GTK_CLIST(clist), at_row, &color);
    case DELETED_PALM_REC:
      colormap = gtk_widget_get_colormap(clist);
      color.red=CLIST_DEL_RED;
      color.green=CLIST_DEL_GREEN;
      color.blue=CLIST_DEL_BLUE;
      gdk_color_alloc(colormap, &color);
      gtk_clist_set_background(GTK_CLIST(clist), at_row, &color);
      break;
    case MODIFIED_PALM_REC:
      colormap = gtk_widget_get_colormap(clist);
      color.red=CLIST_MOD_RED;
      color.green=CLIST_MOD_GREEN;
      color.blue=CLIST_MOD_BLUE;
      gdk_color_alloc(colormap, &color);
      gtk_clist_set_background(GTK_CLIST(clist), at_row, &color);
      break;
    default:
      if (mex->attrib & dlpRecAttrSecret) {
	 colormap = gtk_widget_get_colormap(clist);
	 color.red=CLIST_PRIVATE_RED;
	 color.green=CLIST_PRIVATE_GREEN;
	 color.blue=CLIST_PRIVATE_BLUE;
	 gdk_color_alloc(colormap, &color);
	 gtk_clist_set_background(GTK_CLIST(clist), at_row, &color);
      } else {
	 gtk_clist_set_background(GTK_CLIST(clist), at_row, NULL);
      }
   }

   gtk_clist_set_row_data(GTK_CLIST(clist), at_row, mex);

   sprintf(date, "%02d/%02d", mex->ex.date.tm_mon+1, mex->ex.date.tm_mday);
   gtk_clist_set_text(GTK_CLIST(clist), at_row, 0, date);

   Ptype = get_entry_type(mex->ex.type);
   gtk_clist_set_text(GTK_CLIST(clist), at_row, 1, Ptype);

   if (mex->ex.amount) {
      gtk_clist_set_text(GTK_CLIST(clist), at_row, 2, mex->ex.amount);
   }
   
   return 0;
}

/*
 * This function lists the records in the clist on the left side of
 * the screen.
 */
static void display_records()
{
   int num, i;
   int row_count, entries_shown;
   struct MyExpense *mex;
   GList *records;
   GList *temp_list;
   buf_rec *br;
   gchar *empty_line[] = { "","","" };
   
   records=NULL;
   
   jp_logf(LOG_DEBUG, "Expense: display_records\n");
   
   row_count=((GtkCList *)clist)->rows;

   connect_changed_signals(DISCONNECT_SIGNALS);
   set_new_button_to(CLEAR_FLAG);

   if (glob_myexpense_list!=NULL) {
      free_myexpense_list(&glob_myexpense_list);
   }

   gtk_clist_freeze(GTK_CLIST(clist));

   /* This function takes care of reading the Database for us */
   num = jp_read_DB_files("ExpenseDB", &records);
   /* Go to first entry in the list */
   for (temp_list = records; temp_list; temp_list = temp_list->prev) {
      records = temp_list;
   }
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
      if ((br->rt == DELETED_PALM_REC) || (br->rt == MODIFIED_PALM_REC)) {
	 continue;
      }
      if (show_category < 16) {
	 if ( ((br->attrib & 0x0F) != 
	       glob_category_number_from_menu_item[show_category]) &&
	     show_category != CATEGORY_ALL) {
	    continue;
	 }
      }
      
      mex = malloc(sizeof(struct MyExpense));
      mex->next=NULL;
      mex->attrib = br->attrib;
      mex->unique_id = br->unique_id;
      mex->rt = br->rt;

      /* We need to unpack the record blobs from the database.
       * unpack_Expense is already written in pilot-link, but normally
       * an unpack must be written for each type of application */
      if (unpack_Expense(&(mex->ex), br->buf, br->size)!=0) {
	 entries_shown++;
	 if (entries_shown>row_count) {
	    gtk_clist_append(GTK_CLIST(clist), empty_line);
	 }
	 display_record(mex, entries_shown-1);
      }

      if (glob_myexpense_list==NULL) {
	 glob_myexpense_list=mex;
      } else {
	 glob_myexpense_list->next=mex;
      }
   }
   
   /* Delete any extra rows, the data is already freed by freeing the list */
   for (i=row_count-1; i>=entries_shown; i--) {
      gtk_clist_set_row_data(GTK_CLIST(clist), i, NULL);
      gtk_clist_remove(GTK_CLIST(clist), i);
   }
      
   gtk_clist_thaw(GTK_CLIST(clist));

   jp_free_DB_records(&records);

   connect_changed_signals(CONNECT_SIGNALS);

   if (entries_shown) {
      gtk_clist_select_row(GTK_CLIST(clist), clist_row_selected, 0);
      cb_clist_selection(clist, clist_row_selected, 0, (gpointer)455, NULL);
   }

   jp_logf(LOG_DEBUG, "Expense: leave display_records\n");
}

/*
 * This function just displays a record on the right hand side of the screen
 * (the details)
 */
static void cb_clist_selection(GtkWidget      *clist,
			       gint           row,
			       gint           column,
			       GdkEventButton *event,
			       gpointer       data)
{
   struct MyExpense *mex;
   int i, item_num, category;
   int keep, b;
   
   jp_logf(LOG_DEBUG, "Expense: cb_clist_selection\n");

   if ((!event) && (clist_hack)) return;

   if (row<0) {
      return;
   }

   /* HACK */
   if (clist_hack) {
      keep=record_changed;
      gtk_clist_select_row(GTK_CLIST(clist), clist_row_selected, column);
      b=dialog_save_changed_record(scrolled_window, record_changed);
      if (b==DIALOG_SAID_1) {
	 cb_add_new_record(NULL, GINT_TO_POINTER(record_changed));
      }
      set_new_button_to(CLEAR_FLAG);
      /* This doesn't cause an event to occur, it does highlight
       * the line, so we do the next call also */
      gtk_clist_select_row(GTK_CLIST(clist), row, column);
      cb_clist_selection(clist, row, column, GINT_TO_POINTER(1), NULL);
      return;
   }
   
   clist_row_selected = row;

   mex = gtk_clist_get_row_data(GTK_CLIST(clist), row);
   if (mex==NULL) {
      return;
   }

   set_new_button_to(CLEAR_FLAG);
   /* Need to disconnect these signals first */
   connect_changed_signals(DISCONNECT_SIGNALS);
   
   category = mex->attrib & 0x0F;
   item_num=0;
   for (i=0; i<16; i++) {
      if (glob_category_number_from_menu_item[i]==category) {
	 item_num=i;
	 break;
      }
   }

   gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM
       (menu_item_category2[item_num]), TRUE);
   gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM
       (menu_item_expense_type[mex->ex.type]), TRUE);
   gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM
       (menu_item_payment[mex->ex.payment]), TRUE);
   gtk_option_menu_set_history(GTK_OPTION_MENU(menu_category2), item_num);
   gtk_option_menu_set_history(GTK_OPTION_MENU(menu_expense_type), mex->ex.type);
   gtk_option_menu_set_history(GTK_OPTION_MENU(menu_payment), mex->ex.payment);

   
   gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinner_mon), mex->ex.date.tm_mon+1);
   gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinner_day), mex->ex.date.tm_mday);
   gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinner_year), mex->ex.date.tm_year+1900);

   if (mex->ex.amount) {
      gtk_entry_set_text(GTK_ENTRY(entry_amount), mex->ex.amount);
   } else {
      gtk_entry_set_text(GTK_ENTRY(entry_amount), "");
   }
   
   if (mex->ex.vendor) {
      gtk_entry_set_text(GTK_ENTRY(entry_vendor), mex->ex.vendor);
   } else {
      gtk_entry_set_text(GTK_ENTRY(entry_vendor), "");
   }
   
   if (mex->ex.city) {
      gtk_entry_set_text(GTK_ENTRY(entry_city), mex->ex.city);
   } else {
      gtk_entry_set_text(GTK_ENTRY(entry_city), "");
   }
   
   gtk_text_set_point(GTK_TEXT(text_attendees), 0);
   gtk_text_forward_delete(GTK_TEXT(text_attendees),
			   gtk_text_get_length(GTK_TEXT(text_attendees)));
   if (mex->ex.attendees) {
      gtk_text_insert(GTK_TEXT(text_attendees), NULL,NULL,NULL, mex->ex.attendees, -1);
   }

   gtk_text_set_point(GTK_TEXT(text_note), 0);
   gtk_text_forward_delete(GTK_TEXT(text_note),
			   gtk_text_get_length(GTK_TEXT(text_note)));
   if (mex->ex.note) {
      gtk_text_insert(GTK_TEXT(text_note), NULL,NULL,NULL, mex->ex.note, -1);
   }
   set_new_button_to(CLEAR_FLAG);
   connect_changed_signals(CONNECT_SIGNALS);

   jp_logf(LOG_DEBUG, "Expense: leaving cb_clist_selection\n");
}

/*
 * All menus use this same callback function.  I use the value parameter
 * to determine which menu was changed and which item was selected from it.
 */
static void cb_category(GtkWidget *item, unsigned int value)
{
   int menu, sel;
   int b;
   
   jp_logf(LOG_DEBUG, "Expense: cb_category\n");
   if (!item) {
      return;
   }
   if (!(GTK_CHECK_MENU_ITEM(item))->active) {
      return;
   }
   
   menu=(value & 0xFF00) >> 8;
   sel=value & 0x00FF;

   switch (menu) {
    case EXPENSE_CAT1:
      b=dialog_save_changed_record(scrolled_window, record_changed);
      if (b==DIALOG_SAID_1) {
	 cb_add_new_record(NULL, GINT_TO_POINTER(record_changed));
      }
   
      show_category=sel;
      display_records();
      break;
    case EXPENSE_CAT2:
      cb_record_changed(NULL, NULL);
      glob_detail_category=sel;
      break;
    case EXPENSE_TYPE:
      cb_record_changed(NULL, NULL);
      glob_detail_type=sel;
      break;
    case EXPENSE_PAYMENT:
      cb_record_changed(NULL, NULL);
      glob_detail_payment=sel;
      break;
   }
}


/*
 * Just a convenience function for passing in an array of strings and getting
 * them all stuffed into a menu.
 */
static int make_menu(char *items[], int menu_index, GtkWidget **Poption_menu,
		     GtkWidget *menu_items[])
{
   int i, item_num;
   GSList *group;
   GtkWidget *option_menu;
   GtkWidget *menu_item;
   GtkWidget *menu;
   
   jp_logf(LOG_DEBUG, "Expense: make_menu\n");

   *Poption_menu = option_menu = gtk_option_menu_new();
   
   menu = gtk_menu_new();

   group = NULL;
   
   for (i=0; items[i]; i++) {
      menu_item = gtk_radio_menu_item_new_with_label(group, gettext(items[i]));
      menu_items[i] = menu_item;
      if (menu_index==EXPENSE_CAT1) {
	 if (i==0) {
	    item_num=CATEGORY_ALL;
	 } else {
	    item_num = i - 1;
	 }
      } else {
	 item_num = i;
      }
      gtk_signal_connect(GTK_OBJECT(menu_item), "activate",
			 cb_category, GINT_TO_POINTER(menu_index<<8 | item_num));
      group = gtk_radio_menu_item_group(GTK_RADIO_MENU_ITEM(menu_item));
      gtk_menu_append(GTK_MENU(menu), menu_item);
      gtk_widget_show(menu_item);
   }
   gtk_option_menu_set_menu(GTK_OPTION_MENU(option_menu), menu);
   /* Make this one show up by default */
   gtk_option_menu_set_history(GTK_OPTION_MENU(option_menu), 0);

   gtk_widget_show(option_menu);

   return 0;
}

/* 
 * This function makes all of the menus on the screen.
 */
static void make_menus()
{
   GList *records;
   struct ExpenseAppInfo eai;
   unsigned char *buf;
   int buf_size;
   int i, count;
   char all[]="All";
   GtkWidget *menu_item_category1[17];
   char *categories[18];

   char *payment[]={
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
   char *expense_type[]={
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
     
   jp_logf(LOG_DEBUG, "Expense: make_menus\n");

   /* This gets the application specific data out of the database for us.
    * We still need to write a function to unpack it from its blob form. */
   
   jp_get_app_info("ExpenseDB", &buf, &buf_size);
   unpack_ExpenseAppInfo(&eai, buf, buf_size);
   
   categories[0]=all;
   for (i=0, count=0; i<16; i++) {
      glob_category_number_from_menu_item[i]=0;
      if (eai.category.name[i][0]=='\0') {
	 continue;
      }
      categories[count+1]=eai.category.name[i];
      jp_charset_p2j(categories[count+1], strlen(categories[count+1])+1);
      glob_category_number_from_menu_item[count++]=i;
   }
   categories[count+1]=NULL;
   
   records=NULL;
   
   make_menu(categories, EXPENSE_CAT1, &menu_category1, menu_item_category1);
   /* Skip the ALL for this menu */
   make_menu(&categories[1], EXPENSE_CAT2, &menu_category2, menu_item_category2);
   make_menu(payment, EXPENSE_PAYMENT, &menu_payment, menu_item_payment);
   make_menu(expense_type, EXPENSE_TYPE, &menu_expense_type, menu_item_expense_type);
   
}

/*returns 0 if not found, 1 if found */
static int clist_find_id(GtkWidget *clist,
			 unsigned int unique_id,
			 int *found_at,
			 int *total_count)
{
   int i, found;
   struct MyExpense *mex;
   
   *found_at = 0;
   *total_count = 0;

   jp_logf(LOG_DEBUG, "Expense: clist_find_id\n");

   /*100000 is just to prevent ininite looping during a solar flare */
   for (found = i = 0; i<100000; i++) {
      mex = gtk_clist_get_row_data(GTK_CLIST(clist), i);
      if (!mex) {
	 break;
      }
      if (found) {
	 continue;
      }
      if (mex->unique_id==unique_id) {
	 found = 1;
	 *found_at = i;
      }
   }
   *total_count = i;
   
   return found;
}


static int expense_find(int unique_id)
{
   int r, found_at, total_count;
   
   jp_logf(LOG_DEBUG, "Expense: expense_find\n");

   r = clist_find_id(clist,
		     unique_id,
		     &found_at,
		     &total_count);
   if (r) {
      if (total_count == 0) {
	 total_count = 1;
      }
      gtk_clist_select_row(GTK_CLIST(clist), found_at, 0);
      cb_clist_selection(clist, found_at, 0, (gpointer)455, NULL);
      move_scrolled_window_hack(scrolled_window,
				(float)found_at/(float)total_count);
   }

   return 0;
}

/*
 * This function is called by J-Pilot when the user selects this plugin
 * from the plugin menu, or from the search window when a search result
 * record is chosen.  In the latter case, unique ID will be set.  This
 * application should go directly to that record in the case.
 */
int plugin_gui(GtkWidget *vbox, GtkWidget *hbox, unsigned int unique_id)
{
   GtkWidget *vbox1, *vbox2;
   GtkWidget *temp_hbox;
   GtkWidget *temp_vbox;
   GtkWidget *button;
   GtkWidget *label;
   GtkWidget *vscrollbar;
   time_t ltime;
   struct tm *now;
   
   jp_logf(LOG_DEBUG, "Expense: plugin gui started, unique_id=%d\n", unique_id);

   record_changed=CLEAR_FLAG;
   show_category = CATEGORY_ALL;
   clist_row_selected = 0;

   time(&ltime);
   now = localtime(&ltime);

   /* Make the menus */
   jp_logf(LOG_DEBUG, "Expense: calling make_menus\n");
   make_menus();

   /* Add buttons in left vbox */
   
   /* left and right main boxes */
   vbox1 = gtk_vbox_new(FALSE, 0);
   vbox2 = gtk_vbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(hbox), vbox1, TRUE, TRUE, 5);
   gtk_box_pack_start(GTK_BOX(hbox), vbox2, TRUE, TRUE, 5);

   gtk_widget_set_usize(GTK_WIDGET(vbox1), 0, 230);
   gtk_widget_set_usize(GTK_WIDGET(vbox2), 0, 230);

   /* Make a temporary hbox */
   temp_hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox1), temp_hbox, FALSE, FALSE, 0);
   
   label = gtk_label_new(_("Category: "));
   gtk_box_pack_start(GTK_BOX(temp_hbox), label, FALSE, FALSE, 0);
   gtk_box_pack_start(GTK_BOX(temp_hbox), menu_category1, TRUE, TRUE, 0);

   
   /* Scrolled Window */
   scrolled_window = gtk_scrolled_window_new(NULL, NULL);
   gtk_container_set_border_width(GTK_CONTAINER(scrolled_window), 0);
   gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
   gtk_box_pack_start(GTK_BOX(vbox1), scrolled_window, TRUE, TRUE, 0);
   
   /* Clist */
   clist = gtk_clist_new(3);
   clist_hack=FALSE;
   gtk_signal_connect(GTK_OBJECT(clist), "select_row",
		      GTK_SIGNAL_FUNC(cb_clist_selection),
		      NULL);
   /* gtk_clist_set_shadow_type(GTK_CLIST(clist), SHADOW);*/
   gtk_clist_set_selection_mode(GTK_CLIST(clist), GTK_SELECTION_BROWSE);
   gtk_clist_set_column_width(GTK_CLIST(clist), 0, 50);
   gtk_clist_set_column_width(GTK_CLIST(clist), 1, 140);
   gtk_clist_set_column_width(GTK_CLIST(clist), 2, 70);
   gtk_container_add(GTK_CONTAINER(scrolled_window), GTK_WIDGET(clist));
   
   /* -------------------- */
   /* Right half of screen */
   /* -------------------- */
   
   temp_hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox2), temp_hbox, FALSE, FALSE, 0);

   /* Add record button */
   button = gtk_button_new_with_label(_("Delete"));
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_delete),
		      GINT_TO_POINTER(DELETE_FLAG));
   gtk_box_pack_start(GTK_BOX(temp_hbox), button, TRUE, TRUE, 0);
   
   button = gtk_button_new_with_label(_("Copy"));
   gtk_box_pack_start(GTK_BOX(temp_hbox), button, TRUE, TRUE, 0);
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_add_new_record), 
		      GINT_TO_POINTER(COPY_FLAG));

   new_record_button = gtk_button_new_with_label(_("New Record"));
   gtk_box_pack_start(GTK_BOX(temp_hbox), new_record_button, TRUE, TRUE, 0);
   gtk_signal_connect(GTK_OBJECT(new_record_button), "clicked",
		      GTK_SIGNAL_FUNC(cb_add_new_record),
		      GINT_TO_POINTER(CLEAR_FLAG));

   add_record_button = gtk_button_new_with_label(_("Add Record"));
   gtk_box_pack_start(GTK_BOX(temp_hbox), add_record_button, TRUE, TRUE, 0);
   gtk_signal_connect(GTK_OBJECT(add_record_button), "clicked",
		      GTK_SIGNAL_FUNC(cb_add_new_record),
		      GINT_TO_POINTER(NEW_FLAG));
   gtk_widget_set_name(GTK_WIDGET(GTK_LABEL(GTK_BIN(add_record_button)->child)),
		       "label_high");

   apply_record_button = gtk_button_new_with_label(_("Apply Changes"));
   gtk_box_pack_start(GTK_BOX(temp_hbox), apply_record_button, TRUE, TRUE, 0);
   gtk_signal_connect(GTK_OBJECT(apply_record_button), "clicked",
		      GTK_SIGNAL_FUNC(cb_add_new_record),
		      GINT_TO_POINTER(MODIFY_FLAG));
   gtk_widget_set_name(GTK_WIDGET(GTK_LABEL(GTK_BIN(apply_record_button)->child)),
		       "label_high");
   
   /* Category Menu */
   temp_hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox2), temp_hbox, FALSE, FALSE, 0);
   
   label = gtk_label_new(_("Category: "));
   gtk_box_pack_start(GTK_BOX(temp_hbox), label, FALSE, FALSE, 0);
   gtk_box_pack_start(GTK_BOX(temp_hbox), menu_category2, TRUE, TRUE, 0);


   /* Type Menu */
   temp_hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox2), temp_hbox, FALSE, FALSE, 0);
   
   label = gtk_label_new(_("Type: "));
   gtk_box_pack_start(GTK_BOX(temp_hbox), label, FALSE, FALSE, 0);
   gtk_box_pack_start(GTK_BOX(temp_hbox), menu_expense_type, TRUE, TRUE, 0);

   
   /* Payment Menu */
   temp_hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox2), temp_hbox, FALSE, FALSE, 0);
   
   label = gtk_label_new(_("Payment: "));
   gtk_box_pack_start(GTK_BOX(temp_hbox), label, FALSE, FALSE, 0);
   gtk_box_pack_start(GTK_BOX(temp_hbox), menu_payment, TRUE, TRUE, 0);


   /* Date Spinners */
   temp_hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox2), temp_hbox, FALSE, FALSE, 0);

   /* Month spinner */
   temp_vbox = gtk_vbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(temp_hbox), temp_vbox, FALSE, FALSE, 0);
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
   gtk_box_pack_start(GTK_BOX(temp_hbox), temp_vbox, FALSE, FALSE, 0);
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
   gtk_box_pack_start(GTK_BOX(temp_hbox), temp_vbox, FALSE, FALSE, 0);
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
   temp_hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox2), temp_hbox, FALSE, FALSE, 0);

   label = gtk_label_new(_("Amount: "));
   gtk_box_pack_start(GTK_BOX(temp_hbox), label, FALSE, FALSE, 0);
   entry_amount = gtk_entry_new();
   gtk_box_pack_start(GTK_BOX(temp_hbox), entry_amount, TRUE, TRUE, 0);

   /* Vendor Entry */
   temp_hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox2), temp_hbox, FALSE, FALSE, 0);

   label = gtk_label_new(_("Vendor: "));
   gtk_box_pack_start(GTK_BOX(temp_hbox), label, FALSE, FALSE, 0);
   entry_vendor = gtk_entry_new();
   gtk_box_pack_start(GTK_BOX(temp_hbox), entry_vendor, TRUE, TRUE, 0);

   /* City */
   temp_hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox2), temp_hbox, FALSE, FALSE, 0);

   label = gtk_label_new(_("City: "));
   gtk_box_pack_start(GTK_BOX(temp_hbox), label, FALSE, FALSE, 0);
   entry_city = gtk_entry_new();
   gtk_box_pack_start(GTK_BOX(temp_hbox), entry_city, TRUE, TRUE, 0);

   
   label = gtk_label_new(_("Attendees"));
   gtk_box_pack_start(GTK_BOX(vbox2), label, FALSE, FALSE, 0);

   /* Attendees textbox */
   temp_hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox2), temp_hbox, TRUE, TRUE, 0);

   text_attendees = gtk_text_new(NULL, NULL);
   gtk_text_set_editable(GTK_TEXT(text_attendees), TRUE);
   gtk_text_set_word_wrap(GTK_TEXT(text_attendees), TRUE);
   vscrollbar = gtk_vscrollbar_new(GTK_TEXT(text_attendees)->vadj);
   gtk_box_pack_start(GTK_BOX(temp_hbox), text_attendees, TRUE, TRUE, 0);
   gtk_box_pack_start(GTK_BOX(temp_hbox), vscrollbar, FALSE, FALSE, 0);

   label = gtk_label_new(_("Note"));
   gtk_box_pack_start(GTK_BOX(vbox2), label, FALSE, FALSE, 0);

   /* Note textbox */
   temp_hbox = gtk_hbox_new(FALSE, 0);
   gtk_box_pack_start(GTK_BOX(vbox2), temp_hbox, TRUE, TRUE, 0);

   text_note = gtk_text_new(NULL, NULL);
   gtk_text_set_editable(GTK_TEXT(text_note), TRUE);
   gtk_text_set_word_wrap(GTK_TEXT(text_note), TRUE);
   vscrollbar = gtk_vscrollbar_new(GTK_TEXT(text_note)->vadj);
   gtk_box_pack_start(GTK_BOX(temp_hbox), text_note, TRUE, TRUE, 0);
   gtk_box_pack_start(GTK_BOX(temp_hbox), vscrollbar, FALSE, FALSE, 0);

   gtk_widget_show_all(hbox);
   gtk_widget_show_all(vbox);
   
   gtk_widget_hide(add_record_button);
   gtk_widget_hide(apply_record_button);

   jp_logf(LOG_DEBUG, "Expense: calling display_records\n");

   display_records();

   jp_logf(LOG_DEBUG, "Expense: after display_records\n");
   
   if (unique_id) {
      expense_find(unique_id);
   }

   return 0;
}

/*
 * This function is called by J-Pilot when the user selects this plugin
 * from the plugin menu, or from the search window when a search result
 * record is chosen.  In the latter case, unique ID will be set.  This
 * application should go directly to that record in the case.
 */
int plugin_gui_cleanup() {
   int b;
   
   b=dialog_save_changed_record(scrolled_window, record_changed);
   if (b==DIALOG_SAID_1) {
      cb_add_new_record(NULL, GINT_TO_POINTER(record_changed));
   }

   connect_changed_signals(DISCONNECT_SIGNALS);

   jp_logf(LOG_DEBUG, "Expense: plugin_gui_cleanup\n");
   if (glob_myexpense_list!=NULL) {
      free_myexpense_list(&glob_myexpense_list);
   }
   return 0;
}

/*
 * This is a plugin callback function that is executed when J-Pilot starts up.
 * base_dir is where J-Pilot is compiled to be installed at (e.g. /usr/local/)
 */
int plugin_startup(jp_startup_info *info)
{
   jp_init();

   jp_logf(LOG_DEBUG, "Expense: plugin_startup\n");
   if (info) {
      if (info->base_dir) {
	 jp_logf(LOG_DEBUG, "Expense: base_dir = [%s]\n", info->base_dir);
      }
   }
   return 0;
}

/*
 * This is a plugin callback function that is executed before a sync occurs.
 * Any sync preperation can be done here.
 */
int plugin_pre_sync(void)
{
   jp_logf(LOG_DEBUG, "Expense: plugin_pre_sync\n");
   return 0;
}

/*
 * This is a plugin callback function that is executed during a sync.
 * Notice that I don't need to sync the Expense application.  Since I used
 * the plugin_get_db_name call to tell J-Pilot what to sync for me.  It will
 * be done automatically.
 */
int plugin_sync(int sd)
{
   jp_logf(LOG_DEBUG, "Expense: plugin_sync\n");
   return 0;
}


static int add_search_result(const char *line, int unique_id, struct search_result **sr)
{
   struct search_result *temp_sr;

   jp_logf(LOG_DEBUG, "Expense: add_search_result for [%s]\n", line);
   temp_sr=malloc(sizeof(struct search_result));
   if (!temp_sr) {
      return -1;
   }
   temp_sr->next=NULL;
   temp_sr->unique_id=unique_id;
   temp_sr->line=strdup(line);
   if (!(*sr)) {
      (*sr)=temp_sr;
   } else {
      (*sr)->next=temp_sr;
   }

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
   struct MyExpense mex;
   int num, count;
   char *line;
   
   records=NULL;
   
   *sr=NULL;

   jp_logf(LOG_DEBUG, "Expense: plugin_search\n");

   /* This function takes care of reading the Database for us */
   num = jp_read_DB_files("ExpenseDB", &records);

   /* Go to first entry in the list */
   for (temp_list = records; temp_list; temp_list = temp_list->prev) {
      records = temp_list;
   }
   
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
      if ((br->rt == DELETED_PALM_REC) || (br->rt == MODIFIED_PALM_REC)) {
	 continue;
      }
      
      mex.attrib = br->attrib;
      mex.unique_id = br->unique_id;
      mex.rt = br->rt;

      /* We need to unpack the record blobs from the database.
       * unpack_Expense is already written in pilot-link, but normally
       * an unpack must be written for each type of application */
      if (unpack_Expense(&(mex.ex), br->buf, br->size)!=0) {
	 if (jp_strstr(mex.ex.amount, search_string, case_sense)) {
	    /* Add it to our result list */
	    line = strdup(mex.ex.amount);
	    jp_logf(LOG_DEBUG, "Expense: calling add_search_result\n");
	    add_search_result(line, br->unique_id, sr);
	    jp_logf(LOG_DEBUG, "Expense: back from add_search_result\n");
	    count++;
	 }
	 if (jp_strstr(mex.ex.vendor, search_string, case_sense)) {
	    /* Add it to our result list */
	    line = strdup(mex.ex.vendor);
	    jp_logf(LOG_DEBUG, "Expense: calling add_search_result\n");
	    add_search_result(line, br->unique_id, sr);
	    jp_logf(LOG_DEBUG, "Expense: back from add_search_result\n");
	    count++;
	 }
	 if (jp_strstr(mex.ex.city, search_string, case_sense)) {
	    /* Add it to our result list */
	    line = strdup(mex.ex.city);
	    jp_logf(LOG_DEBUG, "Expense: calling add_search_result\n");
	    add_search_result(line, br->unique_id, sr);
	    jp_logf(LOG_DEBUG, "Expense: back from add_search_result\n");
	    count++;
	 }
	 if (jp_strstr(mex.ex.attendees, search_string, case_sense)) {
	    /* Add it to our result list */
	    line = strdup(mex.ex.attendees);
	    jp_logf(LOG_DEBUG, "Expense: calling add_search_result\n");
	    add_search_result(line, br->unique_id, sr);
	    jp_logf(LOG_DEBUG, "Expense: back from add_search_result\n");
	    count++;
	 }
	 if (jp_strstr(mex.ex.note, search_string, case_sense)) {
	    /* Add it to our result list */
	    line = strdup(mex.ex.note);
	    jp_logf(LOG_DEBUG, "Expense: calling add_search_result\n");
	    add_search_result(line, br->unique_id, sr);
	    jp_logf(LOG_DEBUG, "Expense: back from add_search_result\n");
	    count++;
	 }
	 free_Expense(&(mex.ex));
      }
   }

   return count;
}

int plugin_help(char **text, int *width, int *height)
{
   /* We could also pass back *text=NULL
    * and implement whatever we wanted to here.
    */
   *text = strdup(
	   /*-------------------------------------------*/
	   "Expense plugin for J-Pilot was written by\n"
	   "Judd Montgomery (c) 1999.\n"
	   "judd@jpilot.org\n"
	   "http://jpilot.org\n"
	   );
   *height = 0;
   *width = 0;
   
   return 0;
}
	 
/*
 * This is a plugin callback function called after a sync.
 */
int plugin_post_sync(void)
{
   jp_logf(LOG_DEBUG, "Expense: plugin_post_sync\n");
   return 0;
}

/*
 * This is a plugin callback function called during program exit.
 */
int plugin_exit_cleanup(void)
{
   jp_logf(LOG_DEBUG, "Expense: plugin_exit_cleanup\n");
   return 0;
}

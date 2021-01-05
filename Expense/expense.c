/*******************************************************************************
 * expense.c
 *
 * This is a plugin for J-Pilot which implements an interface to the
 * Palm Expense application.
 *
 * Copyright (C) 1999-2014 by Judd Montgomery
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
static struct currency_s glob_currency[MAX_CURRENCYS] = {
        {N_("Australia"),      0},
        {N_("Austria"),        1},
        {N_("Belgium"),        2},
        {N_("Brazil"),         3},
        {N_("Canada"),         4},
        {N_("Denmark"),        5},
        {N_("EU (Euro)"),      133},
        {N_("Finland"),        6},
        {N_("France"),         7},
        {N_("Germany"),        8},
        {N_("Hong Kong"),      9},
        {N_("Iceland"),        10},
        {N_("India"),          24},
        {N_("Indonesia"),      25},
        {N_("Ireland"),        11},
        {N_("Italy"),          12},
        {N_("Japan"),          13},
        {N_("Korea"),          26},
        {N_("Luxembourg"),     14},
        {N_("Malaysia"),       27},
        {N_("Mexico"),         15},
        {N_("Netherlands"),    16},
        {N_("New Zealand"),    17},
        {N_("Norway"),         18},
        {N_("P.R.C."),         28},
        {N_("Philippines"),    29},
        {N_("Singapore"),      30},
        {N_("Spain"),          19},
        {N_("Sweden"),         20},
        {N_("Switzerland"),    21},
        {N_("Taiwan"),         32},
        {N_("Thailand"),       31},
        {N_("United Kingdom"), 22},
        {N_("United States"),  23}
};

const char *date_formats[] = {
        "%1$02d/%2$02d",
        "%2$02d/%1$02d",
        "%2$02d.%1$02d.",
        "%2$02d-%1$02d",
        "%1$02d/%2$02d",
        "%1$02d.%2$02d.",
        "%1$02d-%2$02d"
};

/* Left-hand side of GUI */
static struct sorted_cats sort_l[NUM_EXP_CAT_ITEMS];
static GtkWidget *category_menu1;
static GtkWidget *scrolled_window;
static GtkTreeView *treeView;
static GtkListStore *listStore;

/* Right-hand side of GUI */
static GtkWidget *new_record_button;
static GtkWidget *apply_record_button;
static GtkWidget *add_record_button;
static GtkWidget *delete_record_button;
static GtkWidget *copy_record_button;
static GtkWidget *table;
static GtkComboBox *category_menu2;
static GtkWidget *menu_payment;
static GtkWidget *menu_expense_type;
static GtkWidget *menu_currency;
static GtkWidget *spinner_mon, *spinner_day, *spinner_year;
static GtkAdjustment *adj_mon, *adj_day, *adj_year;
static GtkWidget *entry_amount;
static GtkWidget *entry_vendor;
static GtkWidget *entry_city;
static GtkWidget *pane = NULL;
#ifndef ENABLE_STOCK_BUTTONS
static GtkAccelGroup *accel_group;
#endif
static GtkWidget *attendees;
static GObject *attendees_buffer;
static GtkWidget *note;
static GObject *note_buffer;

/* This is the category that is currently being displayed */
static int exp_category = CATEGORY_ALL;
static time_t plugin_last_time = 0;

static int record_changed;
static int row_selected;
static int column_selected;
static int connected = 0;

static int glob_detail_type;
static int glob_detail_payment;
static int glob_detail_currency_pos;

static struct MyExpense *glob_myexpense_list = NULL;

enum {
    EXPENSE_DATE_COLUMN_ENUM = 0,
    EXPENSE_TYPE_COLUMN_ENUM,
    EXPENSE_AMOUNT_COLUMN_ENUM,
    EXPENSE_DATA_COLUMN_ENUM,
    EXPENSE_BACKGROUND_COLOR_ENUM,
    EXPENSE_BACKGROUND_COLOR_ENABLED_ENUM,
    EXPENSE_NUM_COLS
};
/******************************************************************************/
/* Prototypes                                                                 */
/******************************************************************************/
static void display_records(void);

static void connect_changed_signals(int con_or_dis);

static void cb_add_new_record(GtkWidget *widget, gpointer data);

static void cb_pulldown_menu(GtkComboBox *item, unsigned int value);

static int make_menu(const char *items[], int menu_index, GtkWidget **Poption_menu);

static int expense_find(int unique_id);

static void cb_category(GtkComboBox *item, int selection);

static int find_sort_cat_pos(int cat);

static int find_menu_cat_pos(int cat);
void addNewExpenseRecordToDataStructure(struct MyExpense *mexp, gpointer data);
void deleteExpense(struct MyExpense *mexp,gpointer data);

/******************************************************************************/
/* Start of code                                                              */
/******************************************************************************/
int plugin_unpack_cai_from_ai(struct CategoryAppInfo *cai,
                              unsigned char *ai_raw, int len) {
    struct ExpenseAppInfo ai;
    int r;

    jp_logf(JP_LOG_DEBUG, "unpack_expense_cai_from_ai\n");

    memset(&ai, 0, sizeof(ai));
    r = unpack_ExpenseAppInfo(&ai, ai_raw, (size_t) len);
    if (r <= 0) {
        jp_logf(JP_LOG_DEBUG, "unpack_ExpenseAppInfo failed %s %d\n", __FILE__, __LINE__);
        return EXIT_FAILURE;
    }
    memcpy(cai, &(ai.category), sizeof(struct CategoryAppInfo));

    return EXIT_SUCCESS;
}

int plugin_pack_cai_into_ai(struct CategoryAppInfo *cai,
                            unsigned char *ai_raw, int len) {
    struct ExpenseAppInfo ai;
    int r;

    jp_logf(JP_LOG_DEBUG, "pack_expense_cai_into_ai\n");

    r = unpack_ExpenseAppInfo(&ai, ai_raw, (size_t) len);
    if (r <= 0) {
        jp_logf(JP_LOG_DEBUG, "unpack_ExpenseAppInfo failed %s %d\n", __FILE__, __LINE__);
        return EXIT_FAILURE;
    }
    memcpy(&(ai.category), cai, sizeof(struct CategoryAppInfo));

    r = pack_ExpenseAppInfo(&ai, ai_raw, (size_t) len);
    if (r <= 0) {
        jp_logf(JP_LOG_DEBUG, "pack_ExpenseAppInfo failed %s %d\n", __FILE__, __LINE__);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static gint sortDateColumn(GtkTreeModel *model,
                           GtkTreeIter *left,
                           GtkTreeIter *right,
                           gpointer columnId) {
    gint sortcol = GPOINTER_TO_INT(columnId);
    gint ret = 0;
    struct MyExpense *mexp1, *mexp2;
    time_t time1, time2;
    switch (sortcol) {
        case EXPENSE_DATE_COLUMN_ENUM: {
            gtk_tree_model_get(GTK_TREE_MODEL(model), left, EXPENSE_DATA_COLUMN_ENUM, &mexp1, -1);
            gtk_tree_model_get(GTK_TREE_MODEL(model), right, EXPENSE_DATA_COLUMN_ENUM, &mexp2, -1);
            if(mexp1 == NULL && mexp2 == NULL){
                ret = 0;
            } else if(mexp1 == NULL && mexp2 != NULL){
                ret = -1;
            }else if (mexp1 != NULL && mexp2 == NULL){
                ret = 1;
            } else if (mexp1 != NULL && mexp2 != NULL) {
                time1 = mktime(&(mexp1->ex.date));
                time2 = mktime(&(mexp2->ex.date));
                ret = (gint) (time1 - time2);
            }
        }
            break;
        default:
            break;
    }
    return ret;

}

static void set_new_button_to(int new_state) {
    jp_logf(JP_LOG_DEBUG, "set_new_button_to new %d old %d\n", new_state, record_changed);

    if (record_changed == new_state) {
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
    record_changed = new_state;
}

static void cb_record_changed(GtkWidget *widget, gpointer data) {
    jp_logf(JP_LOG_DEBUG, "cb_record_changed\n");

    if (record_changed == CLEAR_FLAG) {
        connect_changed_signals(DISCONNECT_SIGNALS);
        if (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(listStore),NULL) > 0) {
            set_new_button_to(MODIFY_FLAG);
        } else {
            set_new_button_to(NEW_FLAG);
        }
    }
}

static void connect_changed_signals(int con_or_dis) {
    int i;

    /* CONNECT */
    if ((con_or_dis == CONNECT_SIGNALS) && (!connected)) {
        jp_logf(JP_LOG_DEBUG, "Expense: connect_changed_signals\n");
        connected = 1;

        if(category_menu2){
            g_signal_connect(G_OBJECT(category_menu2),"changed",G_CALLBACK(cb_record_changed),NULL);
        }
        if(menu_expense_type){
            g_signal_connect(G_OBJECT(menu_expense_type),"changed",G_CALLBACK(cb_record_changed),NULL);
        }
        if(menu_payment){
            g_signal_connect(G_OBJECT(menu_payment),"changed",G_CALLBACK(cb_record_changed),NULL);
        }
        if(menu_currency){
            g_signal_connect(G_OBJECT(menu_currency),"changed",G_CALLBACK(cb_record_changed),NULL);
        }
        g_signal_connect(G_OBJECT(spinner_mon), "changed",
                           G_CALLBACK(cb_record_changed), NULL);
        g_signal_connect(G_OBJECT(spinner_day), "changed",
                           G_CALLBACK(cb_record_changed), NULL);
        g_signal_connect(G_OBJECT(spinner_year), "changed",
                           G_CALLBACK(cb_record_changed), NULL);
        g_signal_connect(G_OBJECT(entry_amount), "changed",
                           G_CALLBACK(cb_record_changed), NULL);
        g_signal_connect(G_OBJECT(entry_vendor), "changed",
                           G_CALLBACK(cb_record_changed), NULL);
        g_signal_connect(G_OBJECT(entry_city), "changed",
                           G_CALLBACK(cb_record_changed), NULL);
        g_signal_connect(attendees_buffer, "changed",
                         G_CALLBACK(cb_record_changed), NULL);
        g_signal_connect(note_buffer, "changed",
                         G_CALLBACK(cb_record_changed), NULL);
    }

    /* DISCONNECT */
    if ((con_or_dis == DISCONNECT_SIGNALS) && (connected)) {
        jp_logf(JP_LOG_DEBUG, "Expense: disconnect_changed_signals\n");
        connected = 0;

        if(category_menu2) {
            g_signal_handlers_disconnect_by_func(G_OBJECT(category_menu2), G_CALLBACK(cb_record_changed), NULL);
        }

        if(menu_expense_type){
            g_signal_handlers_disconnect_by_func(G_OBJECT(menu_expense_type),
                                                 G_CALLBACK(cb_record_changed), NULL);
        }
        if(menu_payment){
            g_signal_handlers_disconnect_by_func(G_OBJECT(menu_payment),
                                                 G_CALLBACK(cb_record_changed), NULL);
        }
        if(menu_currency){
            g_signal_handlers_disconnect_by_func(G_OBJECT(menu_currency),
                                                 G_CALLBACK(cb_record_changed), NULL);
        }

        g_signal_handlers_disconnect_by_func(G_OBJECT(spinner_mon),
                                      G_CALLBACK(cb_record_changed), NULL);
        g_signal_handlers_disconnect_by_func(G_OBJECT(spinner_day),
                                      G_CALLBACK(cb_record_changed), NULL);
        g_signal_handlers_disconnect_by_func(G_OBJECT(spinner_year),
                                      G_CALLBACK(cb_record_changed), NULL);
        g_signal_handlers_disconnect_by_func(G_OBJECT(entry_amount),
                                      G_CALLBACK(cb_record_changed), NULL);
        g_signal_handlers_disconnect_by_func(G_OBJECT(entry_vendor),
                                      G_CALLBACK(cb_record_changed), NULL);
        g_signal_handlers_disconnect_by_func(G_OBJECT(entry_city),
                                      G_CALLBACK(cb_record_changed), NULL);
        g_signal_handlers_disconnect_by_func(attendees_buffer,
                                             G_CALLBACK(cb_record_changed), NULL);
        g_signal_handlers_disconnect_by_func(note_buffer,
                                             G_CALLBACK(cb_record_changed), NULL);
    }
}

static void free_myexpense_list(struct MyExpense **PPmexp) {
    struct MyExpense *mexp, *next_mexp;

    jp_logf(JP_LOG_DEBUG, "Expense: free_myexpense_list\n");

    for (mexp = *PPmexp; mexp; mexp = next_mexp) {
        free_Expense(&(mexp->ex));
        next_mexp = mexp->next;
        free(mexp);
    }
    *PPmexp = NULL;
}

#define PLUGIN_MAJOR 1
#define PLUGIN_MINOR 1

/* This is a mandatory plugin function. */
void plugin_version(int *major_version, int *minor_version) {
    *major_version = PLUGIN_MAJOR;
    *minor_version = PLUGIN_MINOR;
}

static int static_plugin_get_name(char *name, int len) {
    jp_logf(JP_LOG_DEBUG, "Expense: plugin_get_name\n");
    snprintf(name, (size_t) len, "Expense %d.%d", PLUGIN_MAJOR, PLUGIN_MINOR);
    return EXIT_SUCCESS;
}

/* This is a mandatory plugin function. */
int plugin_get_name(char *name, int len) {
    return static_plugin_get_name(name, len);
}

/*
 * This is an optional plugin function.
 * This is the name that will show up in the plugins menu in J-Pilot.
 */
int plugin_get_menu_name(char *name, int len) {
    strncpy(name, _("Expense"), (size_t) len);
    return EXIT_SUCCESS;
}

/*
 * This is an optional plugin function.
 * This is the name that will show up in the plugins help menu in J-Pilot.
 * If this function is used then plugin_help must also be defined.
 */
int plugin_get_help_name(char *name, int len) {
    g_snprintf(name, (gulong) len, _("About %s"), _("Expense"));
    return EXIT_SUCCESS;
}

/*
 * This is an optional plugin function.
 * This is the palm database that will automatically be synced.
 */
int plugin_get_db_name(char *name, int len) {
    strncpy(name, "ExpenseDB", (size_t) len);
    return EXIT_SUCCESS;
}

/*
 * A utility function for getting textual data from an enum.
 */
static char *get_entry_type(enum ExpenseType type) {
    switch (type) {
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
gboolean deleteExpenseRecord(GtkTreeModel *model,
                      GtkTreePath  *path,
                      GtkTreeIter  *iter,
                      gpointer data) {
    int * i = gtk_tree_path_get_indices ( path ) ;
    if(i[0] == row_selected){
        struct MyExpense *mexp = NULL;
        gtk_tree_model_get(model,iter,EXPENSE_DATA_COLUMN_ENUM,&mexp,-1);
        deleteExpense(mexp,data);
        //
        return TRUE;
    }

    return FALSE;


}
void deleteExpense(struct MyExpense *mexp,gpointer data) {
    int size;
    unsigned char buf[0xFFFF];
    buf_rec br;
    int flag;

    jp_logf(JP_LOG_DEBUG, "Expense: cb_delete\n");

    flag = GPOINTER_TO_INT(data);


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

    if ((flag == MODIFY_FLAG) || (flag == DELETE_FLAG)) {
        jp_delete_record("ExpenseDB", &br, DELETE_FLAG);
    }

    if (flag == DELETE_FLAG) {
        /* when we redraw we want to go to the line above the deleted one */
        if (row_selected > 0) {
            row_selected--;
        }
        display_records();
    }
}
/* This function gets called when the "delete" button is pressed */
static void cb_delete(GtkWidget *widget, gpointer data) {
    gtk_tree_model_foreach(GTK_TREE_MODEL(listStore), deleteExpenseRecord, data);
    return;

}

/*
 * This is called when the "Clear" button is pressed.
 * It just clears out all the detail fields.
 */
static void exp_clear_details(void) {
    time_t ltime;
    struct tm *now;
    int new_cat;
    int sorted_position;

    jp_logf(JP_LOG_DEBUG, "Expense: exp_clear_details\n");

    time(&ltime);
    now = localtime(&ltime);

    /* Disconnect signals to prevent callbacks while we change values */
    connect_changed_signals(DISCONNECT_SIGNALS);

    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinner_mon), now->tm_mon + 1);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinner_day), now->tm_mday);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinner_year), now->tm_year + 1900);

    gtk_entry_set_text(GTK_ENTRY(entry_amount), "");
    gtk_entry_set_text(GTK_ENTRY(entry_vendor), "");
    gtk_entry_set_text(GTK_ENTRY(entry_city), "");
    gtk_text_buffer_set_text(GTK_TEXT_BUFFER(attendees_buffer), "", -1);
    gtk_text_buffer_set_text(GTK_TEXT_BUFFER(note_buffer), "", -1);

    if (exp_category == CATEGORY_ALL) {
        new_cat = 0;
    } else {
        new_cat = exp_category;
    }
    sorted_position = find_sort_cat_pos(new_cat);
    if (sorted_position < 0) {
        jp_logf(JP_LOG_WARN, _("Category is not legal\n"));
    } else {
        gtk_combo_box_set_active(GTK_COMBO_BOX(category_menu2),find_menu_cat_pos(sorted_position));
    }

    set_new_button_to(CLEAR_FLAG);

    connect_changed_signals(CONNECT_SIGNALS);
}

/* returns position, position starts at zero */
static int currency_id_to_position(int currency) {
    int i;
    int found = 0;

    for (i = 0; i < MAX_CURRENCYS; i++) {
        if (glob_currency[i].currency == currency) {
            found = i;
            break;
        }
    }
    return found;
}

/* returns currency id, position starts at zero */
static int position_to_currency_id(int position) {
    if (position < MAX_CURRENCYS) {
        return glob_currency[position].currency;
    } else {
        return 0;
    }
}
gboolean
addNewExpenseRecord (GtkTreeModel *model,
              GtkTreePath  *path,
              GtkTreeIter  *iter,
              gpointer data) {

    int * i = gtk_tree_path_get_indices ( path ) ;
    if(i[0] == row_selected){
        struct MyExpense *mexp = NULL;
        gtk_tree_model_get(model,iter,EXPENSE_DATA_COLUMN_ENUM,&mexp,-1);
        addNewExpenseRecordToDataStructure(mexp,data);
        return TRUE;
    }
    return FALSE;


}
void addNewExpenseRecordToDataStructure(struct MyExpense *mexp, gpointer data) {
    struct Expense ex;
    buf_rec br;
    const char *text;
    unsigned char buf[0xFFFF];
    int size;
    int flag;
    int i;
    unsigned int unique_id = 0;
    GtkTextIter start_iter;
    GtkTextIter end_iter;

    jp_logf(JP_LOG_DEBUG, "Expense: cb_add_new_record\n");

    flag = GPOINTER_TO_INT(data);

    if (flag == CLEAR_FLAG) {
        exp_clear_details();
        connect_changed_signals(DISCONNECT_SIGNALS);
        set_new_button_to(NEW_FLAG);
        return;
    }
    if ((flag != NEW_FLAG) && (flag != MODIFY_FLAG) && (flag != COPY_FLAG)) {
        return;
    }
    if (flag == MODIFY_FLAG) {
        if (!mexp) {
            return;
        }
        unique_id = mexp->unique_id;
    }

    /* Grab details of record from widgets on right-hand side of screen */
    ex.type = (enum ExpenseType) glob_detail_type;
    ex.payment = (enum ExpensePayment) glob_detail_payment;
    ex.currency = position_to_currency_id(glob_detail_currency_pos);

    /* gtk_entry_get_text *does not* allocate memory */
    text = gtk_entry_get_text(GTK_ENTRY(entry_amount));
    ex.amount = (char *) text;
    if (ex.amount[0] == '\0') {
        ex.amount = NULL;
    }

    text = gtk_entry_get_text(GTK_ENTRY(entry_vendor));
    ex.vendor = (char *) text;
    if (ex.vendor[0] == '\0') {
        ex.vendor = NULL;
    }

    text = gtk_entry_get_text(GTK_ENTRY(entry_city));
    ex.city = (char *) text;
    if (ex.city[0] == '\0') {
        ex.city = NULL;
    }

    ex.date.tm_mon = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinner_mon)) - 1;
    ex.date.tm_mday = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinner_day));
    ex.date.tm_year = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinner_year)) - 1900;
    ex.date.tm_hour = 12;
    ex.date.tm_min = 0;
    ex.date.tm_sec = 0;

    gtk_text_buffer_get_bounds(GTK_TEXT_BUFFER(attendees_buffer), &start_iter, &end_iter);
    ex.attendees = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(attendees_buffer), &start_iter, &end_iter, TRUE);
    if (ex.attendees[0] == '\0') {
        free(ex.attendees);
        ex.attendees = NULL;
    }

    gtk_text_buffer_get_bounds(GTK_TEXT_BUFFER(note_buffer), &start_iter, &end_iter);
    ex.note = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(note_buffer), &start_iter, &end_iter, TRUE);
    if (ex.note[0] == '\0') {
        free(ex.note);
        ex.note = NULL;
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
    if (GTK_IS_WIDGET(category_menu2)) {
        br.attrib = get_selected_category_from_combo_box(GTK_COMBO_BOX(category_menu2));
    }

    jp_logf(JP_LOG_DEBUG, "category is %d\n", br.attrib);
    br.buf = buf;
    br.size = size;
    br.unique_id = 0;

    if (flag == MODIFY_FLAG) {
        cb_delete(NULL, data);
        if ((mexp->rt == PALM_REC) || (mexp->rt == REPLACEMENT_PALM_REC)) {
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
 * This function is called when the user presses the "Add" button.
 * We collect all of the data from the GUI and pack it into an expense
 * record and then write it out.
 */
static void cb_add_new_record(GtkWidget *widget, gpointer data) {
    if(gtk_tree_model_iter_n_children(GTK_TREE_MODEL(listStore), NULL) != 0) {
        gtk_tree_model_foreach(GTK_TREE_MODEL(listStore), addNewExpenseRecord, data);
    }else {
        //no records exist in category yet.
        addNewExpenseRecordToDataStructure(NULL,data);
    }

}

/*
 * This function just adds the record to the treeView on the left side of
 * the screen.
 */
static int display_record(struct MyExpense *mexp, int at_row, const char *dateformat, GtkTreeIter *iter) {
    char *Ptype;
    char date[12];
    GdkColor bgColor;
    gboolean showBgColor;
    char *amount = "";

    jp_logf(JP_LOG_DEBUG, "Expense: display_record\n");

    switch (mexp->rt) {
        case NEW_PC_REC:
        case REPLACEMENT_PALM_REC:
            bgColor = get_color(LIST_NEW_RED, LIST_NEW_GREEN, LIST_NEW_BLUE);
            showBgColor = TRUE;
            break;
        case DELETED_PALM_REC:
        case DELETED_PC_REC:
            bgColor = get_color(LIST_DEL_RED, LIST_DEL_GREEN, LIST_DEL_BLUE);
            showBgColor = TRUE;
            break;
        case MODIFIED_PALM_REC:
            bgColor = get_color(LIST_MOD_RED, LIST_MOD_GREEN, LIST_MOD_BLUE);
            showBgColor = TRUE;
            break;
        default:
            if (mexp->attrib & dlpRecAttrSecret) {
                bgColor = get_color(LIST_PRIVATE_RED, LIST_PRIVATE_GREEN, LIST_PRIVATE_BLUE);
                showBgColor = TRUE;
            } else {
                showBgColor = FALSE;
            }
    }
    gtk_list_store_append(listStore, iter);
    sprintf(date, dateformat, mexp->ex.date.tm_mon + 1, mexp->ex.date.tm_mday);
    Ptype = get_entry_type(mexp->ex.type);
    if (mexp->ex.amount) {
        amount = mexp->ex.amount;
    }
    gtk_list_store_set(listStore, iter,
                       EXPENSE_DATE_COLUMN_ENUM, date,
                       EXPENSE_TYPE_COLUMN_ENUM, Ptype,
                       EXPENSE_AMOUNT_COLUMN_ENUM, amount,
                       EXPENSE_DATA_COLUMN_ENUM, mexp,
                       EXPENSE_BACKGROUND_COLOR_ENUM, showBgColor ? &bgColor : NULL,
                       EXPENSE_BACKGROUND_COLOR_ENABLED_ENUM, showBgColor,
                       -1);
    return EXIT_SUCCESS;
}

gboolean
selectExpenseRecordByRow(GtkTreeModel *model,
                         GtkTreePath *path,
                         GtkTreeIter *iter,
                         gpointer data) {
    int *i = gtk_tree_path_get_indices(path);
    if (i[0] == row_selected) {
        GtkTreeSelection *selection = NULL;
        selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeView));
        gtk_tree_selection_select_path(selection, path);
        gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(treeView), path, gtk_tree_view_get_column(treeView,EXPENSE_DATE_COLUMN_ENUM), FALSE, 1.0, 0.0);
        return TRUE;
    }

    return FALSE;
}

/*
 * This function lists the records in the treeView on the left side of
 * the screen.
 */
static void display_records(void) {
    int num, i;
    int entries_shown;
    struct MyExpense *mexp;
    GList *records;
    GList *temp_list;
    buf_rec *br;
    const char *dateformat = date_formats[get_pref_int_default(PREF_SHORTDATE, 0)];
    GtkTreeIter iter;

    jp_logf(JP_LOG_DEBUG, "Expense: display_records\n");

    records = NULL;

    free_myexpense_list(&glob_myexpense_list);

    /* Clear left-hand side of window */
    exp_clear_details();
    gtk_list_store_clear(GTK_LIST_STORE(listStore));


    /* This function takes care of reading the Database for us */
    num = jp_read_DB_files("ExpenseDB", &records);
    if (-1 == num)
        return;

    entries_shown = 0;
    for (i = 0, temp_list = records; temp_list; temp_list = temp_list->next, i++) {
        if (temp_list->data) {
            br = temp_list->data;
        } else {
            continue;
        }
        if (!br->buf) {
            continue;
        }

        /* Since deleted and modified records are also returned and we don't
         * want to see those we skip over them. */
        if ((br->rt == DELETED_PALM_REC) ||
            (br->rt == DELETED_PC_REC) ||
            (br->rt == MODIFIED_PALM_REC)) {
            continue;
        }
        if (exp_category < NUM_EXP_CAT_ITEMS) {
            if (((br->attrib & 0x0F) != exp_category) &&
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
        if (unpack_Expense(&(mexp->ex), br->buf, br->size) != 0) {
            display_record(mexp, entries_shown, dateformat, &iter);
            entries_shown++;
        }

        /* Prepend entry at head of list */
        mexp->next = glob_myexpense_list;
        glob_myexpense_list = mexp;
    }

    jp_free_DB_records(&records);

    /* Select the existing requested row, or row 0 if that is impossible */
    if (row_selected <= entries_shown) {
        gtk_tree_model_foreach(GTK_TREE_MODEL(listStore), selectExpenseRecordByRow, NULL);

    } else {
        row_selected = 0;
        gtk_tree_model_foreach(GTK_TREE_MODEL(listStore), selectExpenseRecordByRow, NULL);
    }


    jp_logf(JP_LOG_DEBUG, "Expense: leave display_records\n");
}


/* Find position of category in sorted category array
 * via its assigned category number */
static int find_sort_cat_pos(int cat) {
    int i;

    for (i = 0; i < NUM_EXP_CAT_ITEMS; i++) {
        if (sort_l[i].cat_num == cat) {
            return i;
        }
    }

    return -1;
}

/* Find a category's position in the category menu.
 * This is equal to the category number except for the Unfiled category.
 * The Unfiled category is always in the last position which changes as
 * the number of categories changes */
static int find_menu_cat_pos(int cat) {
    int i;

    if (cat != NUM_EXP_CAT_ITEMS - 1) {
        return cat;
    } else { /* Unfiled category */
        /* Count how many category entries are filled */
        for (i = 0; i < NUM_EXP_CAT_ITEMS; i++) {
            if (!sort_l[i].Pcat[0]) {
                return i;
            }
        }
        return 0;
    }
}

static void cb_edit_cats(GtkWidget *widget, gpointer data) {
    struct ExpenseAppInfo ai;
    char full_name[256];
    char buffer[65536];
    int num;
    size_t size;
    void *buf;
    struct pi_file *pf;

    jp_logf(JP_LOG_DEBUG, "cb_edit_cats\n");

    jp_get_home_file_name("ExpenseDB.pdb", full_name, 250);

    buf = NULL;
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

    size = (size_t) pack_ExpenseAppInfo(&ai, (unsigned char *) buffer, 65535);

    jp_pdb_file_write_app_block("ExpenseDB", buffer, (int) size);

    /* Force refresh and display of CATEGORY_ALL */
    plugin_gui_refresh(-1);
}

/* Called when left-hand category menu is used */
static void cb_category(GtkComboBox *item, int selection) {
    int b;
    if (!item) return;
    if (gtk_combo_box_get_active(GTK_COMBO_BOX(item)) < 0) {
        return;
    }
    int selectedItem = get_selected_category_from_combo_box(item);
    if (selectedItem == -1) {
        return;
    }

        if (exp_category == selectedItem) { return; }

        b = dialog_save_changed_record_with_cancel(pane, record_changed);
        if (b == DIALOG_SAID_1) { /* Cancel */
            int index, index2;

            if (exp_category == CATEGORY_ALL) {
                index = 0;
                index2 = 0;
            } else {
                index = find_sort_cat_pos(exp_category);
                index2 = find_menu_cat_pos(index) + 1;
                index += 1;
            }

            if (index < 0) {
                jp_logf(JP_LOG_WARN, _("Category is not legal\n"));
            } else {
                gtk_combo_box_set_active(GTK_COMBO_BOX(category_menu1),index2);
            }

            return;
        }
        if (b == DIALOG_SAID_3) { /* Save */
            cb_add_new_record(NULL, GINT_TO_POINTER(record_changed));
        }

        if (selectedItem == CATEGORY_EDIT) {
            cb_edit_cats(item, NULL);
        } else {
            exp_category = selectedItem;
        }
        jp_logf(JP_LOG_DEBUG, "cb_category() cat=%d\n", exp_category);

        row_selected = 0;
        display_records();
        jp_logf(JP_LOG_DEBUG, "Leaving cb_category()\n");

}

static gboolean handleExpenseRowSelection(GtkTreeSelection *selection,
                                   GtkTreeModel *model,
                                   GtkTreePath *path,
                                   gboolean path_currently_selected,
                                   gpointer userdata) {
    GtkTreeIter iter;
    struct MyExpense *mexp;
    int b;
    int index, sorted_position;
    int currency_position;
    unsigned int unique_id = 0;
    if ((gtk_tree_model_get_iter(model, &iter, path)) && (!path_currently_selected)) {

        int *i = gtk_tree_path_get_indices(path);
        row_selected = i[0];
        gtk_tree_model_get(model, &iter, EXPENSE_DATA_COLUMN_ENUM, &mexp, -1);
        jp_logf(JP_LOG_DEBUG, "Expense: handleExpenseRowSelection\n");

        if ((record_changed == MODIFY_FLAG) || (record_changed == NEW_FLAG)) {
            if (mexp != NULL) {
                unique_id = mexp->unique_id;
            }

            b = dialog_save_changed_record(scrolled_window, record_changed);
            if (b == DIALOG_SAID_2) {
                cb_add_new_record(NULL, GINT_TO_POINTER(record_changed));
            }
            set_new_button_to(CLEAR_FLAG);

            if (unique_id) {
                expense_find(unique_id);
            }
            return TRUE;
        }
        if (mexp == NULL) {
            return TRUE;
        }

        set_new_button_to(CLEAR_FLAG);
        /* Need to disconnect signals while changing values */
        connect_changed_signals(DISCONNECT_SIGNALS);

        index = mexp->attrib & 0x0F;
        sorted_position = find_sort_cat_pos(index);
        int pos = findSortedPostion(sorted_position, GTK_COMBO_BOX(category_menu2));
        if (pos != sorted_position && index != 0) {
            /* Illegal category */
            jp_logf(JP_LOG_DEBUG, "Category is not legal\n");
            sorted_position = 0;
        }
        if (sorted_position < 0) {
            jp_logf(JP_LOG_WARN, _("Category is not legal\n"));
        }
        gtk_combo_box_set_active(GTK_COMBO_BOX(category_menu2),find_menu_cat_pos(sorted_position));

        if (mexp->ex.type < MAX_EXPENSE_TYPES) {
            gtk_combo_box_set_active(GTK_COMBO_BOX(menu_expense_type),mexp->ex.type);
        } else {
            jp_logf(JP_LOG_WARN, _("Expense: Unknown expense type\n"));
        }
        if (mexp->ex.payment < MAX_PAYMENTS) {
            gtk_combo_box_set_active(GTK_COMBO_BOX(menu_payment),mexp->ex.payment);
        } else {
            jp_logf(JP_LOG_WARN, _("Expense: Unknown payment type\n"));
        }
        currency_position = currency_id_to_position(mexp->ex.currency);

        gtk_combo_box_set_active(GTK_COMBO_BOX(menu_currency),currency_position);

        gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinner_mon), mexp->ex.date.tm_mon + 1);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinner_day), mexp->ex.date.tm_mday);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinner_year), mexp->ex.date.tm_year + 1900);

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

        jp_logf(JP_LOG_DEBUG, "Expense: leaving handleExpenseRowSelection\n");
    }
    return TRUE;
}

/*
 * All menus use this same callback function.  I use the value parameter
 * to determine which menu was changed and which item was selected from it.
 */
static void cb_pulldown_menu(GtkComboBox *item, unsigned int value) {
    int menu, sel;

    jp_logf(JP_LOG_DEBUG, "Expense: cb_pulldown_menu\n");

    if (!item)
        return;
    if(gtk_combo_box_get_active(GTK_COMBO_BOX(item)) < 0){
        return;
    }


    sel = gtk_combo_box_get_active(GTK_COMBO_BOX(item));
    menu = findSortedPostion(sel,GTK_COMBO_BOX(item));
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
        default:
            break;
    }
}

/*
 * Just a convenience function for passing in an array of strings and getting
 * them all stuffed into a menu.
 */
static int make_menu(const char *items[], int menu_index, GtkWidget **Poption_menu) {
    int i, item_num;
    GSList *group;
    GtkWidget *option_menu;
    GtkWidget *menu_item;
    GtkWidget *menu;

    jp_logf(JP_LOG_DEBUG, "Expense: make_menu\n");
    GtkListStore *catListStore = gtk_list_store_new(2,G_TYPE_STRING,G_TYPE_INT);
    GtkTreeIter iter;

    for (i = 0; items[i]; i++) {
        gtk_list_store_append (catListStore, &iter);
        gtk_list_store_set (catListStore, &iter, 0, _(items[i]), 1, menu_index, -1);
    }
    *Poption_menu =  gtk_combo_box_new_with_model (GTK_TREE_MODEL (catListStore));
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new ();
    gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (*Poption_menu), renderer, TRUE);
    gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (*Poption_menu), renderer,
                                    "text", 0,
                                    NULL);
    g_signal_connect(G_OBJECT( *Poption_menu),"changed",G_CALLBACK(cb_pulldown_menu),
                     GINT_TO_POINTER(menu_index << 8 | item_num));

    return EXIT_SUCCESS;
}

/* 
 * This function makes all of the menus on the screen.
 */
static void make_menus(void) {
    struct ExpenseAppInfo exp_app_info;
    unsigned char *buf;
    int buf_size;
    int i;
    long char_set;
    char *cat_name;

    const char *payment[MAX_PAYMENTS + 1] = {
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
    const char *expense_type[MAX_CURRENCYS + 1] = {
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
    const char *currency[MAX_CURRENCYS + 1];


    jp_logf(JP_LOG_DEBUG, "Expense: make_menus\n");

    /* Point the currency array to the country names and NULL terminate it */
    for (i = 0; i < MAX_CURRENCYS; i++) {
        currency[i] = glob_currency[i].country;
    }
    currency[MAX_CURRENCYS] = NULL;

    /* Do some category initialization */

    /* This gets the application specific data out of the database for us.
     * We still need to write a function to unpack it from its blob form. */
    jp_get_app_info("ExpenseDB", &buf, &buf_size);
    unpack_ExpenseAppInfo(&exp_app_info, buf, (size_t) buf_size);
    if (buf) {
        free(buf);
    }

    get_pref(PREF_CHAR_SET, &char_set, NULL);

    for (i = 1; i < NUM_EXP_CAT_ITEMS; i++) {
        cat_name = charset_p2newj(exp_app_info.category.name[i], 31, (int) char_set);
        strcpy(sort_l[i - 1].Pcat, cat_name);
        free(cat_name);
        sort_l[i - 1].cat_num = i;
    }
    /* put reserved 'Unfiled' category at end of list */
    cat_name = charset_p2newj(exp_app_info.category.name[0], 31, (int) char_set);
    strcpy(sort_l[NUM_EXP_CAT_ITEMS - 1].Pcat, cat_name);
    free(cat_name);
    sort_l[NUM_EXP_CAT_ITEMS - 1].cat_num = 0;

    qsort(sort_l, NUM_EXP_CAT_ITEMS - 1, sizeof(struct sorted_cats), cat_compare);

#ifdef JPILOT_DEBUG
    for (i=0; i<NUM_EXP_CAT_ITEMS; i++) {
       printf("cat %d [%s]\n", sort_l[i].cat_num, sort_l[i].Pcat);
    }
#endif

    if ((exp_category != CATEGORY_ALL) && (exp_app_info.category.name[exp_category][0] == '\0')) {
        exp_category = CATEGORY_ALL;
    }

    make_category_menu(&category_menu1,
                       sort_l, cb_category, TRUE, TRUE);
    if(exp_category == CATEGORY_ALL){
        gtk_combo_box_set_active(GTK_COMBO_BOX(category_menu1), 0);
    }
    /* Skip the ALL category for this menu */
    make_category_menu(&category_menu2,
                       sort_l, NULL, FALSE, FALSE);
    make_menu(payment, EXPENSE_PAYMENT, &menu_payment);
    make_menu(expense_type, EXPENSE_TYPE, &menu_expense_type);
    make_menu(currency, EXPENSE_CURRENCY, &menu_currency);
}


gboolean
findExpenseRecord (GtkTreeModel *model,
            GtkTreePath  *path,
            GtkTreeIter  *iter,
            gpointer data) {
    int uniqueId = GPOINTER_TO_INT(data);
    if (uniqueId) {
        struct MyExpense *mexp = NULL;

        gtk_tree_model_get(model,iter,EXPENSE_DATA_COLUMN_ENUM,&mexp,-1);
        if(mexp->unique_id == uniqueId){
            GtkTreeSelection * selection = NULL;
            selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeView));
            if(gtk_tree_selection_get_select_function(selection) == NULL) {
                gtk_tree_selection_set_select_function(selection, handleExpenseRowSelection, NULL, NULL);
            }
            gtk_tree_selection_select_path(selection, path);
            gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(treeView), path, gtk_tree_view_get_column(treeView,EXPENSE_DATE_COLUMN_ENUM), FALSE, 1.0, 0.0);
            return TRUE;
        }
    }
    return FALSE;
}
static int expense_find(int unique_id) {
    gtk_tree_model_foreach(GTK_TREE_MODEL(listStore), findExpenseRecord, GINT_TO_POINTER(unique_id));
    return EXIT_SUCCESS;
}

/*
 * This function is called by J-Pilot when the user selects this plugin
 * from the plugin menu, or from the search window when a search result
 * record is chosen.  In the latter case, unique ID will be set.  This
 * application should go directly to that record if the ID is set.
 */
int plugin_gui(GtkWidget *vbox, GtkWidget *hbox, unsigned int unique_id) {
    GtkWidget *vbox1, *vbox2;
    GtkWidget *hbox_temp;
    GtkWidget *temp_vbox;
    GtkWidget *label;
    GtkWidget *separator;
    time_t ltime;
    struct tm *now;
    long ivalue;
    long show_tooltips;
    int i;
    int cycle_category = FALSE;
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

    row_selected = 0;

    time(&ltime);
    now = localtime(&ltime);

    /************************************************************/
    /* Build the GUI */

    get_pref(PREF_SHOW_TOOLTIPS, &show_tooltips, NULL);

    /* Make the menus */
    make_menus();

    pane = gtk_hpaned_new();
    get_pref(PREF_EXPENSE_PANE, &ivalue, NULL);
    gtk_paned_set_position(GTK_PANED(pane), (gint) ivalue);

    gtk_box_pack_start(GTK_BOX(hbox), pane, TRUE, TRUE, 5);

    /* left and right main boxes */
    vbox1 = gtk_vbox_new(FALSE, 0);
    vbox2 = gtk_vbox_new(FALSE, 0);
    gtk_paned_pack1(GTK_PANED(pane), vbox1, TRUE, FALSE);
    gtk_paned_pack2(GTK_PANED(pane), vbox2, TRUE, FALSE);

    gtk_widget_set_size_request(GTK_WIDGET(vbox1), 0, 230);
    gtk_widget_set_size_request(GTK_WIDGET(vbox2), 0, 230);

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

    listStore = gtk_list_store_new(EXPENSE_NUM_COLS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                                   G_TYPE_POINTER, gdk_rgba_get_type(), G_TYPE_BOOLEAN);
    treeView = GTK_TREE_VIEW(gtk_tree_view_new_with_model(GTK_TREE_MODEL(listStore)));
    GtkCellRenderer *dateRenderer = gtk_cell_renderer_text_new();
    GtkCellRenderer *typeRenderer = gtk_cell_renderer_text_new();
    GtkCellRenderer *amountRenderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *dateColumn = gtk_tree_view_column_new_with_attributes("Date",
                                                                             dateRenderer,
                                                                             "text", EXPENSE_DATE_COLUMN_ENUM,
                                                                             "cell-background-gdk",
                                                                             EXPENSE_BACKGROUND_COLOR_ENUM,
                                                                             "cell-background-set",
                                                                             EXPENSE_BACKGROUND_COLOR_ENABLED_ENUM,
                                                                             NULL);
    GtkTreeViewColumn *typeColumn = gtk_tree_view_column_new_with_attributes("Type",
                                                                             typeRenderer,
                                                                             "text", EXPENSE_TYPE_COLUMN_ENUM,
                                                                             "cell-background-gdk",
                                                                             EXPENSE_BACKGROUND_COLOR_ENUM,
                                                                             "cell-background-set",
                                                                             EXPENSE_BACKGROUND_COLOR_ENABLED_ENUM,
                                                                             NULL);
    GtkTreeViewColumn *amountColumn = gtk_tree_view_column_new_with_attributes("Amount",
                                                                               amountRenderer,
                                                                               "text", EXPENSE_AMOUNT_COLUMN_ENUM,
                                                                               "cell-background-gdk",
                                                                               EXPENSE_BACKGROUND_COLOR_ENUM,
                                                                               "cell-background-set",
                                                                               EXPENSE_BACKGROUND_COLOR_ENABLED_ENUM,
                                                                               NULL);
    gtk_tree_view_column_set_sort_column_id(dateColumn, EXPENSE_DATE_COLUMN_ENUM);
    gtk_tree_view_column_set_sort_column_id(typeColumn, EXPENSE_TYPE_COLUMN_ENUM);
    gtk_tree_view_column_set_sort_column_id(amountColumn, EXPENSE_AMOUNT_COLUMN_ENUM);

    gtk_tree_view_insert_column(treeView, dateColumn, EXPENSE_DATE_COLUMN_ENUM);
    gtk_tree_view_insert_column(treeView, typeColumn, EXPENSE_TYPE_COLUMN_ENUM);
    gtk_tree_view_insert_column(treeView, amountColumn, EXPENSE_AMOUNT_COLUMN_ENUM);
    gtk_tree_view_column_set_clickable(dateColumn, gtk_true());
    gtk_tree_view_column_set_clickable(typeColumn, gtk_true());
    gtk_tree_view_column_set_clickable(amountColumn, gtk_true());
    gtk_tree_view_column_set_sizing(dateColumn, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    gtk_tree_view_column_set_min_width(dateColumn, 44);
    gtk_tree_view_column_set_sizing(typeColumn, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    gtk_tree_view_column_set_min_width(typeColumn, 100);


    gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(treeView)),
                                GTK_SELECTION_BROWSE);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(scrolled_window),
                                    GTK_POLICY_NEVER,
                                    GTK_POLICY_AUTOMATIC);
    GtkTreeSelection *treeSelection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeView));

    gtk_tree_selection_set_select_function(treeSelection, handleExpenseRowSelection, NULL, NULL);
    gtk_widget_set_events(treeView, GDK_BUTTON1_MOTION_MASK);
    g_signal_connect (G_OBJECT(treeView), "motion_notify_event",
                      G_CALLBACK(motion_notify_event), NULL);
    g_signal_connect (G_OBJECT(treeView), "button-press-event",
                      G_CALLBACK(button_pressed_for_motion), NULL);
    g_signal_connect (G_OBJECT(treeView), "button-release-event",
                      G_CALLBACK(button_released_for_motion), NULL);

    /* Restore previous sorting configuration */
    get_pref(PREF_EXPENSE_SORT_COLUMN, &ivalue, NULL);
    column_selected = (int) ivalue;

    for (int x = 0; x < EXPENSE_NUM_COLS - 3; x++) {
        gtk_tree_view_column_set_sort_indicator(gtk_tree_view_get_column(GTK_TREE_VIEW(treeView), x), gtk_false());
    }
    gtk_tree_view_column_set_sort_indicator(gtk_tree_view_get_column(GTK_TREE_VIEW(treeView), column_selected),
                                            gtk_true());

    get_pref(PREF_EXPENSE_SORT_ORDER, &ivalue, NULL);
    gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(listStore), EXPENSE_DATE_COLUMN_ENUM, sortDateColumn,
                                    GINT_TO_POINTER(EXPENSE_DATE_COLUMN_ENUM), NULL);

    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(listStore), column_selected, (GtkSortType) ivalue);



    gtk_container_add(GTK_CONTAINER(scrolled_window), GTK_WIDGET(treeView));

    /************************************************************/
    /* Right half of screen */
    hbox_temp = gtk_hbox_new(FALSE, 3);
    gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, FALSE, FALSE, 0);

    /* Delete, Copy, New, etc. buttons */
    CREATE_BUTTON(delete_record_button, _("Delete"), DELETE, _("Delete the selected record"), GDK_d, GDK_CONTROL_MASK,
                  "Ctrl+D")
    g_signal_connect(G_OBJECT(delete_record_button), "clicked",
                       G_CALLBACK(cb_delete),
                       GINT_TO_POINTER(DELETE_FLAG));

    CREATE_BUTTON(copy_record_button, _("Copy"), COPY, _("Copy the selected record"), GDK_c,
                  GDK_CONTROL_MASK | GDK_SHIFT_MASK, "Ctrl+Shift+C")
    g_signal_connect(G_OBJECT(copy_record_button), "clicked",
                       G_CALLBACK(cb_add_new_record),
                       GINT_TO_POINTER(COPY_FLAG));

    CREATE_BUTTON(new_record_button, _("New Record"), NEW, _("Add a new record"), GDK_n, GDK_CONTROL_MASK, "Ctrl+N")
    g_signal_connect(G_OBJECT(new_record_button), "clicked",
                       G_CALLBACK(cb_add_new_record),
                       GINT_TO_POINTER(CLEAR_FLAG));

    CREATE_BUTTON(add_record_button, _("Add Record"), ADD, _("Add the new record"), GDK_KEY_Return, GDK_CONTROL_MASK,
                  "Ctrl+Enter")
    g_signal_connect(G_OBJECT(add_record_button), "clicked",
                       G_CALLBACK(cb_add_new_record),
                       GINT_TO_POINTER(NEW_FLAG));
#ifndef ENABLE_STOCK_BUTTONS
    gtk_widget_set_name(GTK_WIDGET(GTK_LABEL(gtk_bin_get_child(GTK_BIN(add_record_button)))),
                        "label_high");
#endif

    CREATE_BUTTON(apply_record_button, _("Apply Changes"), APPLY, _("Commit the modifications"), GDK_KEY_Return,
                  GDK_CONTROL_MASK, "Ctrl+Enter")
    g_signal_connect(G_OBJECT(apply_record_button), "clicked",
                       G_CALLBACK(cb_add_new_record),
                       GINT_TO_POINTER(MODIFY_FLAG));
#ifndef ENABLE_STOCK_BUTTONS
    gtk_widget_set_name(GTK_WIDGET(GTK_LABEL(gtk_bin_get_child(GTK_BIN(apply_record_button)))),
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

    adj_mon = GTK_ADJUSTMENT(gtk_adjustment_new(now->tm_mon + 1, 1.0, 12.0, 1.0,
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

    adj_year = GTK_ADJUSTMENT(gtk_adjustment_new(now->tm_year + 1900, 0.0, 2037.0,
                                                 1.0, 100.0, 0.0));
    spinner_year = gtk_spin_button_new(adj_year, 0, 0);
    gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(spinner_year), FALSE);
    gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spinner_year), TRUE);
    gtk_box_pack_start(GTK_BOX(temp_vbox), spinner_year, FALSE, TRUE, 0);
    gtk_widget_set_size_request(spinner_year, 55, 0);

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
    /*gtk_widget_set_size_request(GTK_WIDGET(scrolled_window), 150, 0); */
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
    /*gtk_widget_set_size_request(GTK_WIDGET(scrolled_window), 150, 0); */
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
        for (i = 0; i < NUM_EXP_CAT_ITEMS; i++) {
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
        if (exp_category == CATEGORY_ALL) {
            index = 0;
            index2 = 0;
        } else {
            index = find_sort_cat_pos(exp_category);
            index2 = find_menu_cat_pos(index) + 1;
            index += 1;
        }
        if (index < 0) {
            jp_logf(JP_LOG_WARN, _("Category is not legal\n"));
        } else {
            gtk_combo_box_set_active(GTK_COMBO_BOX(category_menu1),index2);
        }
    } else {
        exp_category = CATEGORY_ALL;
    }

    /* The focus doesn't do any good on the application button */
    gtk_widget_grab_focus(GTK_WIDGET(treeView));

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

    b = dialog_save_changed_record(scrolled_window, record_changed);
    if (b == DIALOG_SAID_2) {
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
    set_pref(PREF_EXPENSE_SORT_COLUMN, column_selected, NULL, TRUE);
    set_pref(PREF_EXPENSE_SORT_ORDER, gtk_tree_view_column_get_sort_order(gtk_tree_view_get_column(treeView, row_selected)), NULL, TRUE);

    plugin_last_time = time(NULL);

    return EXIT_SUCCESS;
}

/*
 * This is a plugin callback function that is executed when J-Pilot starts up.
 * base_dir is where J-Pilot is compiled to be installed at (e.g. /usr/local/)
 */
int plugin_startup(jp_startup_info *info) {
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
int plugin_pre_sync(void) {
    jp_logf(JP_LOG_DEBUG, "Expense: plugin_pre_sync\n");
    return EXIT_SUCCESS;
}

/*
 * This is a plugin callback function that is executed during a sync.
 * Notice that I don't need to sync the Expense application.  Since I used
 * the plugin_get_db_name call to tell J-Pilot what to sync for me.  It will
 * be done automatically.
 */
int plugin_sync(int sd) {
    jp_logf(JP_LOG_DEBUG, "Expense: plugin_sync\n");
    return EXIT_SUCCESS;
}

static int add_search_result(const char *line,
                             int unique_id,
                             struct search_result **sr) {
    struct search_result *temp_sr;

    jp_logf(JP_LOG_DEBUG, "Expense: add_search_result for [%s]\n", line);

    temp_sr = malloc(sizeof(struct search_result));
    if (!temp_sr) {
        return EXIT_FAILURE;
    }
    temp_sr->unique_id = (unsigned int) unique_id;
    temp_sr->line = strdup(line);
    temp_sr->next = *sr;
    *sr = temp_sr;

    return 0;
}

/*
 * This function is called when the user does a search.  It should return
 * records which match the search string.
 */
int plugin_search(const char *search_string, int case_sense, struct search_result **sr) {
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
            br = temp_list->data;
        } else {
            continue;
        }
        if (!br->buf) {
            continue;
        }
        /* Since deleted and modified records are also returned and we don't
         * want to see those we skip over them. */
        if ((br->rt == DELETED_PALM_REC) ||
            (br->rt == DELETED_PC_REC) ||
            (br->rt == MODIFIED_PALM_REC)) {
            continue;
        }

        mexp.attrib = br->attrib;
        mexp.unique_id = br->unique_id;
        mexp.rt = br->rt;

        /* We need to unpack the record blobs from the database.
         * unpack_Expense is already written in pilot-link, but normally
         * an unpack must be written for each type of application */
        if (unpack_Expense(&(mexp.ex), br->buf, br->size) != 0) {
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

int plugin_help(char **text, int *width, int *height) {
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
int plugin_post_sync(void) {
    jp_logf(JP_LOG_DEBUG, "Expense: plugin_post_sync\n");
    return EXIT_SUCCESS;
}

/*
 * This is a plugin callback function called during program exit.
 */
int plugin_exit_cleanup(void) {
    jp_logf(JP_LOG_DEBUG, "Expense: plugin_exit_cleanup\n");
    return EXIT_SUCCESS;
}


/*******************************************************************************
 * address_gui.c
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
#include <signal.h>
#include <ctype.h>
#include <sys/stat.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include "address.h"
#include "i18n.h"
#include "prefs.h"
#include "print.h"
#include "password.h"
#include "export.h"
#include "stock_buttons.h"
#include "libsqlite.h"

/********************************* Constants **********************************/
#define NUM_ADDRESS_CAT_ITEMS 16
#define NUM_PHONE_ENTRIES 7
#define NUM_PHONE_LABELS 8
#define MAX_NUM_TEXTS contNote+1
#define NUM_IM_LABELS 5

#define ADDRESS_MAX_LIST_NAME 30
#define ADDRESS_MAX_COLUMN_LEN 80
#define NUM_CONT_CSV_FIELDS 56
#define NUM_ADDR_CSV_FIELDS 27

/* Size of photo to display in Jpilot.  Actual photo can be larger */
#define PHOTO_X_SZ 139
#define PHOTO_Y_SZ 144

/* Many RFCs require that the line termination be CRLF rather than just \n.
 * For conformance to standards this requires adding the two-byte string to
 * the end of strings destined for export */
#define CRLF "\x0D\x0A"

#define CONNECT_SIGNALS 400
#define DISCONNECT_SIGNALS 401

/******************************* Global vars **********************************/
static address_schema_entry *schema;
static int schema_size;

static address_schema_entry contact_schema[NUM_CONTACT_FIELDS] = {
        {contLastname,  0, ADDRESS_GUI_LABEL_TEXT},
        {contFirstname, 0, ADDRESS_GUI_LABEL_TEXT},
        {contCompany,   0, ADDRESS_GUI_LABEL_TEXT},
        {contTitle,     0, ADDRESS_GUI_LABEL_TEXT},
        {contPhone1,    0, ADDRESS_GUI_DIAL_SHOW_PHONE_MENU_TEXT},
        {contPhone2,    0, ADDRESS_GUI_DIAL_SHOW_PHONE_MENU_TEXT},
        {contPhone3,    0, ADDRESS_GUI_DIAL_SHOW_PHONE_MENU_TEXT},
        {contPhone4,    0, ADDRESS_GUI_DIAL_SHOW_PHONE_MENU_TEXT},
        {contPhone5,    0, ADDRESS_GUI_DIAL_SHOW_PHONE_MENU_TEXT},
        {contPhone6,    0, ADDRESS_GUI_DIAL_SHOW_PHONE_MENU_TEXT},
        {contPhone7,    0, ADDRESS_GUI_DIAL_SHOW_PHONE_MENU_TEXT},
        {contIM1,       0, ADDRESS_GUI_IM_MENU_TEXT},
        {contIM2,       0, ADDRESS_GUI_IM_MENU_TEXT},
        {contWebsite,   0, ADDRESS_GUI_WEBSITE_TEXT},
        {contAddress1,  1, ADDRESS_GUI_ADDR_MENU_TEXT},
        {contCity1,     1, ADDRESS_GUI_LABEL_TEXT},
        {contState1,    1, ADDRESS_GUI_LABEL_TEXT},
        {contZip1,      1, ADDRESS_GUI_LABEL_TEXT},
        {contCountry1,  1, ADDRESS_GUI_LABEL_TEXT},
        {contAddress2,  2, ADDRESS_GUI_ADDR_MENU_TEXT},
        {contCity2,     2, ADDRESS_GUI_LABEL_TEXT},
        {contState2,    2, ADDRESS_GUI_LABEL_TEXT},
        {contZip2,      2, ADDRESS_GUI_LABEL_TEXT},
        {contCountry2,  2, ADDRESS_GUI_LABEL_TEXT},
        {contAddress3,  3, ADDRESS_GUI_ADDR_MENU_TEXT},
        {contCity3,     3, ADDRESS_GUI_LABEL_TEXT},
        {contState3,    3, ADDRESS_GUI_LABEL_TEXT},
        {contZip3,      3, ADDRESS_GUI_LABEL_TEXT},
        {contCountry3,  3, ADDRESS_GUI_LABEL_TEXT},
        {contBirthday,  4, ADDRESS_GUI_BIRTHDAY},
        {contCustom1,   4, ADDRESS_GUI_LABEL_TEXT},
        {contCustom2,   4, ADDRESS_GUI_LABEL_TEXT},
        {contCustom3,   4, ADDRESS_GUI_LABEL_TEXT},
        {contCustom4,   4, ADDRESS_GUI_LABEL_TEXT},
        {contCustom5,   4, ADDRESS_GUI_LABEL_TEXT},
        {contCustom6,   4, ADDRESS_GUI_LABEL_TEXT},
        {contCustom7,   4, ADDRESS_GUI_LABEL_TEXT},
        {contCustom8,   4, ADDRESS_GUI_LABEL_TEXT},
        {contCustom9,   4, ADDRESS_GUI_LABEL_TEXT},
        {contNote,      5, ADDRESS_GUI_LABEL_TEXT}
};

static address_schema_entry address_schema[NUM_ADDRESS_FIELDS] = {
        {contLastname,  0, ADDRESS_GUI_LABEL_TEXT},
        {contFirstname, 0, ADDRESS_GUI_LABEL_TEXT},
        {contTitle,     0, ADDRESS_GUI_LABEL_TEXT},
        {contCompany,   0, ADDRESS_GUI_LABEL_TEXT},
        {contPhone1,    0, ADDRESS_GUI_DIAL_SHOW_PHONE_MENU_TEXT},
        {contPhone2,    0, ADDRESS_GUI_DIAL_SHOW_PHONE_MENU_TEXT},
        {contPhone3,    0, ADDRESS_GUI_DIAL_SHOW_PHONE_MENU_TEXT},
        {contPhone4,    0, ADDRESS_GUI_DIAL_SHOW_PHONE_MENU_TEXT},
        {contPhone5,    0, ADDRESS_GUI_DIAL_SHOW_PHONE_MENU_TEXT},
        {contAddress1,  1, ADDRESS_GUI_LABEL_TEXT},
        {contCity1,     1, ADDRESS_GUI_LABEL_TEXT},
        {contState1,    1, ADDRESS_GUI_LABEL_TEXT},
        {contZip1,      1, ADDRESS_GUI_LABEL_TEXT},
        {contCountry1,  1, ADDRESS_GUI_LABEL_TEXT},
        {contCustom1,   2, ADDRESS_GUI_LABEL_TEXT},
        {contCustom2,   2, ADDRESS_GUI_LABEL_TEXT},
        {contCustom3,   2, ADDRESS_GUI_LABEL_TEXT},
        {contCustom4,   2, ADDRESS_GUI_LABEL_TEXT},
        {contNote,      3, ADDRESS_GUI_LABEL_TEXT}
};

/* Keeps track of whether code is using Address or Contacts database.
 * 0 is AddressDB, 1 is ContactsDB */
static long address_version = 0;

static GtkWidget *treeView;
static GtkTreeSelection *treeSelection;
static GtkListStore *listStore;
static GtkWidget *addr_text[MAX_NUM_TEXTS];
static GObject *addr_text_buffer[MAX_NUM_TEXTS];
static GtkWidget *addr_all;
static GObject *addr_all_buffer;
static GtkWidget *notebook_label[NUM_CONTACT_NOTEBOOK_PAGES];
static GtkWidget *phone_type_list_menu[NUM_PHONE_ENTRIES];
static GtkWidget *address_type_list_menu[NUM_ADDRESSES];
static GtkWidget *IM_type_list_menu[NUM_IMS];
static int address_phone_label_selected[NUM_PHONE_ENTRIES];
static int address_type_selected[NUM_ADDRESSES];
static int IM_type_selected[NUM_IMS];

/* Need two extra slots for the ALL category and Edit Categories... */
//static GtkWidget *address_cat_menu_item1[NUM_ADDRESS_CAT_ITEMS + 2];
//static GtkWidget *address_cat_menu_item2[NUM_ADDRESS_CAT_ITEMS];
static GtkWidget *category_menu1;
static GtkWidget *category_menu2;
static GtkWidget *address_quickfind_entry;
static GtkWidget *notebook;
static GtkWidget *pane;
static GtkWidget *radio_button[NUM_PHONE_ENTRIES];
static GtkWidget *dial_button[NUM_PHONE_ENTRIES];

static struct AddressAppInfo address_app_info;
static struct ContactAppInfo contact_app_info;
static struct sorted_cats sort_l[NUM_ADDRESS_CAT_ITEMS];
static int address_category = CATEGORY_ALL;
static int rowSelected;

static ContactList *glob_contact_list = NULL;
static ContactList *export_contact_list = NULL;

static GtkWidget *new_record_button;
static GtkWidget *apply_record_button;
static GtkWidget *add_record_button;
static GtkWidget *delete_record_button;
static GtkWidget *undelete_record_button;
static GtkWidget *copy_record_button;
static GtkWidget *cancel_record_button;
static int record_changed;

static GtkWidget *private_checkbox;
static GtkWidget *picture_button;
static GtkWidget *birthday_checkbox;
static GtkWidget *birthday_button;
static GtkWidget *birthday_box;
static GtkWidget *reminder_checkbox;
static GtkWidget *reminder_entry;
static GtkWidget *reminder_box;
static struct tm birthday;
static GtkWidget *image = NULL;
static struct ContactPicture contact_picture;
static GList *changed_list = NULL;

extern GtkWidget *glob_date_label;
extern int glob_date_timer_tag;

/****************************** Prototypes ************************************/
static void connect_changed_signals(int con_or_dis);

static void address_update_listStore(GtkListStore *listStore, GtkWidget *tooltip_widget,
                                     ContactList **cont_list, int category,
                                     int main);

gboolean
findAddressRecordAndSelect(GtkTreeModel *model,
                           GtkTreePath *path,
                           GtkTreeIter *iter,
                           gpointer data);

gboolean
findAddressRecordByTextAndSelect(GtkTreeModel *model,
                                 GtkTreePath *path,
                                 GtkTreeIter *iter,
                                 const gpointer data);

gboolean
selectRecordAddressByRow(GtkTreeModel *model,
                         GtkTreePath *path,
                         GtkTreeIter *iter,
                         gpointer data);

gboolean
findAndSetGlobalAddressId(GtkTreeModel *model,
                          GtkTreePath *path,
                          GtkTreeIter *iter,
                          gpointer data);

gboolean printAddressRecord(GtkTreeModel *model,
                            GtkTreePath *path,
                            GtkTreeIter *iter,
                            gpointer data);

gboolean deleteAddressRecord(GtkTreeModel *model,
                             GtkTreePath *path,
                             GtkTreeIter *iter,
                             gpointer data);

gboolean addNewAddressRecord(GtkTreeModel *model,
                             GtkTreePath *path,
                             GtkTreeIter *iter,
                             gpointer data);

gboolean deleteAddressContactRecord(GtkTreeModel *model,
                                    GtkTreePath *path,
                                    GtkTreeIter *iter,
                                    gpointer data);

gboolean undeleteAddressRecord(GtkTreeModel *model,
                               GtkTreePath *path,
                               GtkTreeIter *iter,
                               gpointer data);

void undeleteAddress(MyContact *mcont, gpointer data);

static void cb_delete_address_or_contact(GtkWidget *widget, gpointer data);

void deleteAddress(MyContact *mcont, gpointer data);

void deleteAddressContact(MyContact *mcont, gpointer data);

void addNewAddressRecordToDataStructure(MyContact *mcont, gpointer data);

static int address_redraw(void);

int printAddress(MyContact *mcont, gpointer data);

static int address_find(void);

static void get_address_attrib(unsigned char *attrib);


static gboolean handleRowSelectionForAddress(GtkTreeSelection *selection,
                                             GtkTreeModel *model,
                                             GtkTreePath *path,
                                             gboolean path_currently_selected,
                                             gpointer userdata);


enum {
    ADDRESS_NAME_COLUMN_ENUM,
    ADDRESS_NOTE_COLUMN_ENUM,
    ADDRESS_PHONE_COLUMN_ENUM,
    ADDRESS_DATA_COLUMN_ENUM,
    ADDRESS_BACKGROUND_COLOR_ENUM,
    ADDRESS_BACKGROUND_COLOR_ENABLED_ENUM,
    ADDRESS_FOREGROUND_COLOR_ENUM,
    ADDRESSS_FOREGROUND_COLOR_ENABLED_ENUM,
    ADDRESS_NUM_COLS
};
/****************************** Main Code *************************************/
/* Called once on initialization of GUI */
static void init(void) {
    time_t ltime;
    struct tm *now;

    if (address_version) {
        jp_logf(JP_LOG_DEBUG, "setting schema to contacts\n");
        schema = contact_schema;
        schema_size = NUM_CONTACT_FIELDS;
    } else {
        jp_logf(JP_LOG_DEBUG, "setting schema to addresses\n");
        schema = address_schema;
        schema_size = NUM_ADDRESS_FIELDS;
    }

    time(&ltime);
    now = localtime(&ltime);
    memcpy(&birthday, now, sizeof(struct tm));

    contact_picture.dirty = 0;
    contact_picture.length = 0;
    contact_picture.data = NULL;

    rowSelected = 0;

    changed_list = NULL;
    record_changed = CLEAR_FLAG;
}

static void set_new_button_to(int new_state) {
    jp_logf(JP_LOG_DEBUG, "set_new_button_to new %d old %d\n", new_state, record_changed);

    if (record_changed == new_state) {
        return;
    }

    switch (new_state) {
        case MODIFY_FLAG:
            gtk_widget_show(cancel_record_button);
            gtk_widget_show(copy_record_button);
            gtk_widget_show(apply_record_button);

            gtk_widget_hide(add_record_button);
            gtk_widget_hide(delete_record_button);
            gtk_widget_hide(new_record_button);
            gtk_widget_hide(undelete_record_button);

            break;
        case NEW_FLAG:
            gtk_widget_show(cancel_record_button);
            gtk_widget_show(add_record_button);

            gtk_widget_hide(apply_record_button);
            gtk_widget_hide(copy_record_button);
            gtk_widget_hide(delete_record_button);
            gtk_widget_hide(new_record_button);
            gtk_widget_hide(undelete_record_button);

            break;
        case CLEAR_FLAG:
            gtk_widget_show(delete_record_button);
            gtk_widget_show(copy_record_button);
            gtk_widget_show(new_record_button);

            gtk_widget_hide(add_record_button);
            gtk_widget_hide(apply_record_button);
            gtk_widget_hide(cancel_record_button);
            gtk_widget_hide(undelete_record_button);

            break;
        case UNDELETE_FLAG:
            gtk_widget_show(undelete_record_button);
            gtk_widget_show(copy_record_button);
            gtk_widget_show(new_record_button);

            gtk_widget_hide(add_record_button);
            gtk_widget_hide(apply_record_button);
            gtk_widget_hide(cancel_record_button);
            gtk_widget_hide(delete_record_button);
            break;

        default:
            return;
    }

    record_changed = new_state;
}

static void cb_record_changed(GtkWidget *widget,
                              gpointer data) {
    jp_logf(JP_LOG_DEBUG, "cb_record_changed\n");
    if (record_changed == CLEAR_FLAG) {
        connect_changed_signals(DISCONNECT_SIGNALS);
        if (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(listStore), NULL) > 0) {
            set_new_button_to(MODIFY_FLAG);
        } else {
            set_new_button_to(NEW_FLAG);
        }
    } else if (record_changed == UNDELETE_FLAG) {
        jp_logf(JP_LOG_INFO | JP_LOG_GUI,
                _("This record is deleted.\n"
                  "Undelete it or copy it to make changes.\n"));
    }
}

static void connect_changed_signals(int con_or_dis) {
    GtkWidget *w;
    GList *temp_list;
    static int connected = 0;

    /* Connect signals */
    if ((con_or_dis == CONNECT_SIGNALS)) {
        if (connected) return;
        connected = 1;
        for (temp_list = changed_list; temp_list; temp_list = temp_list->next) {
            if (!(w = temp_list->data)) {
                continue;
            }
            if (GTK_IS_TEXT_BUFFER(w) ||
                GTK_IS_ENTRY(w) ||
                GTK_IS_TEXT_VIEW(w) ||
                GTK_IS_COMBO_BOX(w)
                    ) {
                g_signal_connect(w, "changed", G_CALLBACK(cb_record_changed), NULL);
                continue;
            }
            if (GTK_IS_CHECK_MENU_ITEM(w) ||
                GTK_IS_RADIO_BUTTON(w) ||
                GTK_IS_CHECK_BUTTON(w)
                    ) {
                g_signal_connect(w, "toggled", G_CALLBACK(cb_record_changed), NULL);
                continue;
            }
            if (GTK_IS_BUTTON(w)) {
                g_signal_connect(w, "pressed", G_CALLBACK(cb_record_changed), NULL);
                continue;
            }
            jp_logf(JP_LOG_DEBUG, "connect_changed_signals(): Encountered unknown object type.  Skipping\n");
        }
        return;
    }

    /* Disconnect signals */
    if ((con_or_dis == DISCONNECT_SIGNALS)) {
        if (!connected) return;
        connected = 0;
        for (temp_list = changed_list; temp_list; temp_list = temp_list->next) {
            if (!(temp_list->data)) {
                continue;
            }
            w = temp_list->data;
            g_signal_handlers_disconnect_by_func(w, G_CALLBACK(cb_record_changed), NULL);
        }
    }
}

gboolean printAddressRecord(GtkTreeModel *model,
                            GtkTreePath *path,
                            GtkTreeIter *iter,
                            gpointer data) {
    int *i = gtk_tree_path_get_indices(path);
    if (i[0] == rowSelected) {
        MyContact *myContact = NULL;
        gtk_tree_model_get(model, iter, ADDRESS_DATA_COLUMN_ENUM, &myContact, -1);
        printAddress(myContact, data);
        return TRUE;
    }

    return FALSE;
}

int printAddress(MyContact *mcont, gpointer data) {
    long this_many;
    AddressList *addr_list;
    ContactList *cont_list;
    ContactList cont_list1;
    int get_category;

    get_pref(PREF_PRINT_THIS_MANY, &this_many, NULL);

    cont_list = NULL;
    if (this_many == 1) {
        if (mcont < (MyContact *) LIST_MIN_DATA) {
            return EXIT_FAILURE;
        }
        memcpy(&(cont_list1.mcont), mcont, sizeof(MyContact));
        cont_list1.next = NULL;
        cont_list = &cont_list1;
    }

    /* Get Contacts, or Addresses */
    if ((this_many == 2) || (this_many == 3)) {
        get_category = CATEGORY_ALL;
        if (this_many == 2) {
            get_category = address_category;
        }
        if (address_version == 0) {
            addr_list = NULL;
            if (glob_sqlite) jpsqlite_AddrSEL(&addr_list,addr_sort_order,1,get_category);
            else get_addresses2(&addr_list, SORT_ASCENDING, 2, 2, 1, get_category);
            copy_addresses_to_contacts(addr_list, &cont_list);
            free_AddressList(&addr_list);
        } else {
            get_contacts2(&cont_list, SORT_ASCENDING, 2, 2, 1, get_category);
        }
    }

    print_contacts(cont_list, &contact_app_info, schema, schema_size);

    if ((this_many == 2) || (this_many == 3)) {
        free_ContactList(&cont_list);
    }

    return EXIT_SUCCESS;
}

int address_print(void) {
    gtk_tree_model_foreach(GTK_TREE_MODEL(listStore), printAddressRecord, NULL);
    return EXIT_SUCCESS;

}

static GString *contact_to_gstring(struct Contact *cont) {
    GString *s;
    int i;
    int address_i, IM_i, phone_i;
    char birthday_str[255];
    const char *pref_date;
    char NL[2];
    char *utf;
    long char_set;

    get_pref(PREF_CHAR_SET, &char_set, NULL);

    s = g_string_sized_new(4096);
    NL[0] = '\0';
    NL[1] = '\0';

    address_i = IM_i = phone_i = 0;
    for (i = 0; i < schema_size; i++) {
        switch (schema[i].type) {
            case ADDRESS_GUI_LABEL_TEXT:
            case ADDRESS_GUI_WEBSITE_TEXT:
                if (cont->entry[schema[i].record_field] == NULL) continue;
                if (address_version) {
                    g_string_append_printf(s, _("%s%s: %s"),
                                           NL, contact_app_info.labels[schema[i].record_field],
                                           cont->entry[schema[i].record_field]);
                } else {
                    utf = charset_p2newj(contact_app_info.labels[schema[i].record_field], 16, char_set);
                    g_string_append_printf(s, _("%s%s: %s"),
                                      NL, utf, cont->entry[schema[i].record_field]);
                    g_free(utf);
                }
                NL[0] = '\n';
                break;
            case ADDRESS_GUI_DIAL_SHOW_PHONE_MENU_TEXT:
                if (cont->entry[schema[i].record_field] == NULL) {
                    phone_i++;
                    continue;
                }
                utf = charset_p2newj(contact_app_info.phoneLabels[cont->phoneLabel[phone_i]], 16, char_set);
                g_string_append_printf(s, _("%s%s: %s"),
                                  NL, utf,
                                  cont->entry[schema[i].record_field]);
                g_free(utf);
                NL[0] = '\n';
                phone_i++;
                break;
            case ADDRESS_GUI_IM_MENU_TEXT:
                if (cont->entry[schema[i].record_field] == NULL) {
                    IM_i++;
                    continue;
                }
                utf = charset_p2newj(contact_app_info.IMLabels[cont->IMLabel[IM_i]], 16, char_set);
                g_string_append_printf(s, _("%s%s: %s"),
                                  NL, utf,
                                  cont->entry[schema[i].record_field]);
                g_free(utf);
                NL[0] = '\n';
                IM_i++;
                break;
            case ADDRESS_GUI_ADDR_MENU_TEXT:
                if (cont->entry[schema[i].record_field] == NULL) {
                    address_i++;
                    continue;
                }
                utf = charset_p2newj(contact_app_info.addrLabels[cont->addressLabel[address_i]], 16, char_set);
                g_string_append_printf(s, _("%s%s: %s"),
                                  NL, utf,
                                  cont->entry[schema[i].record_field]);
                g_free(utf);
                NL[0] = '\n';
                address_i++;
                break;
            case ADDRESS_GUI_BIRTHDAY:
                if (cont->birthdayFlag == 0) continue;
                get_pref(PREF_LONGDATE, NULL, &pref_date);
                strftime(birthday_str, sizeof(birthday_str), pref_date, &cont->birthday);

                utf = charset_p2newj(contact_app_info.labels[schema[i].record_field], 16, char_set);
                g_string_append_printf(s, _("%s%s: %s"), NL, utf, birthday_str);
                g_free(utf);
                NL[0] = '\n';
                break;
            default:
                break;
        }
    }
    return s;
}

/* Start Import Code */
static int cb_addr_import(GtkWidget *parent_window,
                          const char *file_path,
                          int type) {
    FILE *in;
    char text[65536];
    struct Address new_addr;
    struct Contact new_cont;
    struct CategoryAppInfo *p_cai;

    unsigned char attrib;
    int i, j, ret, index;
    int address_i, IM_i, phone_i;
    int import_all;
    char old_cat_name[32];
    int new_cat_num, suggested_cat_num;
    int priv;
    int year, month, day;
    GString *cont_text;

    AddressList *addrlist;
    AddressList *temp_addrlist;
    struct CategoryAppInfo cai;

    in = fopen(file_path, "r");
    if (!in) {
        jp_logf(JP_LOG_WARN, _("Unable to open file: %s\n"), file_path);
        return EXIT_FAILURE;
    }

    switch (type) {

        case IMPORT_TYPE_CSV:
            jp_logf(JP_LOG_DEBUG, "Address import CSV [%s]\n", file_path);

            /* Switch between contacts & address data structures */
            if (address_version) {
                p_cai = &contact_app_info.category;
            } else {
                p_cai = &address_app_info.category;
            }

            /* Get the first line containing the format and check for reasonableness */
            if (fgets(text, sizeof(text), in) == NULL) {
                jp_logf(JP_LOG_WARN, _("Unable to read file: %s\n"), file_path);
            }
            if (address_version) {
                ret = verify_csv_header(text, NUM_CONT_CSV_FIELDS, file_path);
            } else {
                ret = verify_csv_header(text, NUM_ADDR_CSV_FIELDS, file_path);
            }
            if (EXIT_FAILURE == ret) return EXIT_FAILURE;

            import_all = FALSE;
            while (1) {
                /* Read the category field */
                read_csv_field(in, text, sizeof(text));
                if (feof(in)) break;
#ifdef JPILOT_DEBUG
                printf("category is [%s]\n", text);
#endif
                g_strlcpy(old_cat_name, text, 16);
                /* Try to match imported category name to an existing category number */
                suggested_cat_num = 0;
                for (i = 0; i < NUM_ADDRESS_CAT_ITEMS; i++) {
                    if (!p_cai->name[i][0]) continue;
                    if (!strcmp(p_cai->name[i], old_cat_name)) {
                        suggested_cat_num = i;
                        break;
                    }
                }

                /* Read the private field */
                read_csv_field(in, text, sizeof(text));
#ifdef JPILOT_DEBUG
                printf("private is [%s]\n", text);
#endif
                sscanf(text, "%d", &priv);

                /* Need to clear record if doing multiple imports */
                memset(&new_cont, 0, sizeof(new_cont));
                address_i = phone_i = IM_i = 0;
                for (i = 0; i < schema_size; i++) {
                    read_csv_field(in, text, sizeof(text));
                    switch (schema[i].type) {
                        case ADDRESS_GUI_IM_MENU_TEXT:
                            for (j = 0; j < NUM_IM_LABELS; j++) {
                                if (strcmp(text, contact_app_info.IMLabels[j]) == 0) {
                                    new_cont.IMLabel[IM_i] = j;
                                    break;
                                }
                            }
                            read_csv_field(in, text, sizeof(text));
                            new_cont.entry[schema[i].record_field] = strdup(text);
                            IM_i++;
                            break;
                        case ADDRESS_GUI_DIAL_SHOW_PHONE_MENU_TEXT:
                            for (j = 0; j < NUM_PHONE_LABELS; j++) {
                                if (address_version) {
                                    if (strcmp(text, contact_app_info.phoneLabels[j]) == 0) {
                                        new_cont.phoneLabel[phone_i] = j;
                                        break;
                                    }
                                } else {
                                    if (strcmp(text, address_app_info.phoneLabels[j]) == 0) {
                                        new_cont.phoneLabel[phone_i] = j;
                                        break;
                                    }
                                }
                            }
                            read_csv_field(in, text, sizeof(text));
                            new_cont.entry[schema[i].record_field] = strdup(text);
                            phone_i++;
                            break;
                        case ADDRESS_GUI_ADDR_MENU_TEXT:
                            for (j = 0; j < NUM_ADDRESSES; j++) {
                                if (strcmp(text, contact_app_info.addrLabels[j]) == 0) {
                                    new_cont.addressLabel[address_i] = j;
                                    break;
                                }
                            }
                            read_csv_field(in, text, sizeof(text));
                            new_cont.entry[schema[i].record_field] = strdup(text);
                            address_i++;
                            break;
                        case ADDRESS_GUI_LABEL_TEXT:
                        case ADDRESS_GUI_WEBSITE_TEXT:
                            new_cont.entry[schema[i].record_field] = strdup(text);
                            break;
                        case ADDRESS_GUI_BIRTHDAY:
                            if (text[0]) {
                                new_cont.birthdayFlag = 1;
                                sscanf(text, "%d/%d/%d", &year, &month, &day);
                                memset(&(new_cont.birthday), 0, sizeof(new_cont.birthday));
                                new_cont.birthday.tm_year = year - 1900;
                                new_cont.birthday.tm_mon = month - 1;
                                new_cont.birthday.tm_mday = day;
                            }
                            /* Also get Reminder Advance field */
                            read_csv_field(in, text, sizeof(text));
                            if (text[0]) {
                                new_cont.reminder = TRUE;
                                sscanf(text, "%d", &(new_cont.advance));
                                new_cont.advanceUnits = 1;  /* Days */
                            }
                            break;
                        default:
                            break;
                    }
                }

                ret = read_csv_field(in, text, sizeof(text));
                sscanf(text, "%d", &(new_cont.showPhone));

                cont_text = contact_to_gstring(&new_cont);
                if (!import_all) {
                    ret = import_record_ask(parent_window, pane,
                                            cont_text->str,
                                            p_cai,
                                            old_cat_name,
                                            priv,
                                            suggested_cat_num,
                                            &new_cat_num);
                } else {
                    new_cat_num = suggested_cat_num;
                }
                g_string_free(cont_text, TRUE);

                if (ret == DIALOG_SAID_IMPORT_QUIT) {
                    jp_free_Contact(&new_cont);
                    break;
                }
                if (ret == DIALOG_SAID_IMPORT_SKIP) {
                    jp_free_Contact(&new_cont);
                    continue;
                }
                if (ret == DIALOG_SAID_IMPORT_ALL) {
                    import_all = TRUE;
                }

                attrib = (new_cat_num & 0x0F) |
                         (priv ? dlpRecAttrSecret : 0);
                if ((ret == DIALOG_SAID_IMPORT_YES) || import_all) {
                    if (address_version) {
                        pc_contact_write(&new_cont, NEW_PC_REC, attrib, NULL);
                        jp_free_Contact(&new_cont);
                    } else {
                        copy_contact_to_address(&new_cont, &new_addr);
                        jp_free_Contact(&new_cont);
                        if (glob_sqlite) jpsqlite_AddrINS(&new_addr, NEW_PC_REC, attrib, NULL);
                        else pc_address_write(&new_addr, NEW_PC_REC, attrib, NULL);
                        free_Address(&new_addr);
                    }
                }
            }
            break;

        case IMPORT_TYPE_DAT:  /* Palm Desktop DAT format */
            jp_logf(JP_LOG_DEBUG, "Address import DAT [%s]\n", file_path);
            if (dat_check_if_dat_file(in) != DAT_ADDRESS_FILE) {
                jp_logf(JP_LOG_WARN, _("File doesn't appear to be address.dat format\n"));
                fclose(in);
                return EXIT_FAILURE;
            }
            addrlist = NULL;
            dat_get_addresses(in, &addrlist, &cai);
            import_all = FALSE;
            for (temp_addrlist = addrlist; temp_addrlist; temp_addrlist = temp_addrlist->next) {
                index = temp_addrlist->maddr.unique_id - 1;
                if (index < 0) {
                    g_strlcpy(old_cat_name, _("Unfiled"), 16);
                } else {
                    g_strlcpy(old_cat_name, cai.name[index], 16);
                }
                /* Figure out what category it was in the dat file */
                index = temp_addrlist->maddr.unique_id - 1;
                suggested_cat_num = 0;
                if (index > -1) {
                    for (i = 0; i < NUM_ADDRESS_CAT_ITEMS; i++) {
                        if (!address_app_info.category.name[i][0]) continue;
                        if (!strcmp(address_app_info.category.name[i], old_cat_name)) {
                            suggested_cat_num = i;
                            break;
                        }
                    }
                }

                ret = 0;
                if (!import_all) {
                    copy_address_to_contact(&(temp_addrlist->maddr.addr), &new_cont);
                    cont_text = contact_to_gstring(&new_cont);
                    ret = import_record_ask(parent_window, pane,
                                            cont_text->str,
                                            &(address_app_info.category),
                                            old_cat_name,
                                            (temp_addrlist->maddr.attrib & 0x10),
                                            suggested_cat_num,
                                            &new_cat_num);
                    g_string_free(cont_text, TRUE);
                    jp_free_Contact(&new_cont);
                } else {
                    new_cat_num = suggested_cat_num;
                }
                if (ret == DIALOG_SAID_IMPORT_QUIT) break;
                if (ret == DIALOG_SAID_IMPORT_SKIP) continue;
                if (ret == DIALOG_SAID_IMPORT_ALL) import_all = TRUE;

                attrib = (new_cat_num & 0x0F) |
                         ((temp_addrlist->maddr.attrib & 0x10) ? dlpRecAttrSecret : 0);
                if ((ret == DIALOG_SAID_IMPORT_YES) || (import_all)) {
                    if (glob_sqlite) jpsqlite_AddrINS(&(temp_addrlist->maddr.addr), NEW_PC_REC, attrib, NULL);
                    else pc_address_write(&(temp_addrlist->maddr.addr), NEW_PC_REC, attrib, NULL);
                }
            }
            free_AddressList(&addrlist);
            break;
        default:
            break;
    }  /* end switch for import types */

    address_refresh();
    fclose(in);
    return EXIT_SUCCESS;
}

int address_import(GtkWidget *window) {
    char *type_desc[] = {
            N_("CSV (Comma Separated Values)"),
            N_("DAT/ABA (Palm Archive Formats)"),
            NULL
    };
    int type_int[] = {
            IMPORT_TYPE_CSV,
            IMPORT_TYPE_DAT,
            0
    };

    /* Hide ABA import of Contacts until file format has been decoded */
    if (address_version == 1) {
        type_desc[1] = NULL;
        type_int[1] = 0;

    }

    import_gui(window, pane, type_desc, type_int, cb_addr_import);
    return EXIT_SUCCESS;
}

/* End Import Code */

/* Start Export code */

static const char *ldifMapType(int label) {
    switch (label) {
        case 0:
            return "telephoneNumber";
        case 1:
            return "homePhone";
        case 2:
            return "facsimileTelephoneNumber";
        case 3:
            return "xotherTelephoneNumber";
        case 4:
            return "mail";
        case 5:
            return "xmainTelephoneNumber";
        case 6:
            return "pager";
        case 7:
            return "mobile";
        default:
            return "xunknownTelephoneNumber";
    }
}

static const char *vCardMapType(int label) {
    switch (label) {
        case 0:
            return "work";
        case 1:
            return "home";
        case 2:
            return "fax";
        case 3:
            return "x-other";
        case 4:
            return "email";
        case 5:
            return "x-main";
        case 6:
            return "pager";
        case 7:
            return "cell";
        default:
            return "x-unknown";
    }
}


static void cb_addr_export_ok(GtkWidget *export_window, GtkWidget *treeView,
                              int type, const char *filename) {
    MyContact *mcont;
    GList *list, *temp_list;
    FILE *out;
    struct stat statb;
    const char *short_date;
    time_t ltime;
    struct tm *now;
    char str1[256], str2[256];
    char pref_time[40];
    int i, r, n;
    int record_num;
    char *button_text[] = {N_("OK")};
    char *button_overwrite_text[] = {N_("No"), N_("Yes")};
    char text[1024];
    char date_string[1024];
    char csv_text[65550];
    long char_set;
    char username[256];
    char hostname[256];
    const char *svalue;
    long userid;
    char birthday_str[255];
    const char *pref_date;
    int address_i, IM_i, phone_i;
    int index = 0;
    char *utf;

    /* Open file for export, including corner cases where file exists or
    * can't be opened */
    if (!stat(filename, &statb)) {
        if (S_ISDIR(statb.st_mode)) {
            g_snprintf(text, sizeof(text), _("%s is a directory"), filename);
            dialog_generic(GTK_WINDOW(export_window),
                           _("Error Opening File"),
                           DIALOG_ERROR, text, 1, button_text);
            return;
        }
        g_snprintf(text, sizeof(text), _("Do you want to overwrite file %s?"), filename);
        r = dialog_generic(GTK_WINDOW(export_window),
                           _("Overwrite File?"),
                           DIALOG_QUESTION, text, 2, button_overwrite_text);
        if (r != DIALOG_SAID_2) {
            return;
        }
    }

    out = fopen(filename, "w");
    if (!out) {
        g_snprintf(text, sizeof(text), _("Error opening file: %s"), filename);
        dialog_generic(GTK_WINDOW(export_window),
                       _("Error Opening File"),
                       DIALOG_ERROR, text, 1, button_text);
        return;
    }

    /* Write a header for TEXT file */
    if (type == EXPORT_TYPE_TEXT) {
        get_pref(PREF_SHORTDATE, NULL, &short_date);
        get_pref_time_no_secs(pref_time);
        time(&ltime);
        now = localtime(&ltime);
        strftime(str1, sizeof(str1), short_date, now);
        strftime(str2, sizeof(str2), pref_time, now);
        g_snprintf(date_string, sizeof(date_string), "%s %s", str1, str2);
        if (address_version == 0) {
            fprintf(out, _("Address exported from %s %s on %s\n\n"),
                    PN, VERSION, date_string);
        } else {
            fprintf(out, _("Contact exported from %s %s on %s\n\n"),
                    PN, VERSION, date_string);
        }
    }

    /* Write a header to the CSV file */
    if (type == EXPORT_TYPE_CSV) {
        if (address_version) {
            fprintf(out, "CSV contacts version "VERSION": Category, Private, ");
        } else {
            fprintf(out, "CSV address version "VERSION": Category, Private, ");
        }

        address_i = phone_i = IM_i = 0;
        for (i = 0; i < schema_size; i++) {
            switch (schema[i].type) {
                case ADDRESS_GUI_IM_MENU_TEXT:
                    fprintf(out, "IM %d label, ", IM_i);
                    fprintf(out, "IM %d, ", IM_i);
                    IM_i++;
                    break;
                case ADDRESS_GUI_DIAL_SHOW_PHONE_MENU_TEXT:
                    fprintf(out, "Phone %d label, ", phone_i);
                    fprintf(out, "Phone %d, ", phone_i);
                    phone_i++;
                    break;
                case ADDRESS_GUI_ADDR_MENU_TEXT:
                    fprintf(out, "Address %d label, ", address_i);
                    fprintf(out, "Address %d, ", address_i);
                    address_i++;
                    break;
                case ADDRESS_GUI_BIRTHDAY:
                    fprintf(out, "%s, ", contact_app_info.labels[schema[i].record_field]);
                    fprintf(out, "Reminder Advance, ");
                    break;
                case ADDRESS_GUI_LABEL_TEXT:
                case ADDRESS_GUI_WEBSITE_TEXT:
                    fprintf(out, "%s, ", contact_app_info.labels[schema[i].record_field]);
                    break;
                default:
                    break;
            }
        }

        fprintf(out, "Show in List\n");
    }  /* end writing CSV header */

    /* Write a header to the B-Folders CSV file */
    if (type == EXPORT_TYPE_BFOLDERS) {
        fprintf(out, "%s",
                "Contacts:\nName, Email, Phone (mobile), Company, Title, Website, Phone (work),"
                "Phone 2(work), Fax (work), Address (work), Phone (home), Address (home), "
                "\"Custom Label 1\", \"Custom Value 1\", \"Custom Label 2\", \"Custom Value 2\","
                "\"Custom Label 3\",\"Custom Value 3\",\"Custom Label 4\",\"Custom Value 4\","
                "\"Custom Label 5\",\"Custom Value 5\",Note,Folder");
    }  /* end writing CSV header */


    /* Special setup for VCARD export */
    if ((type == EXPORT_TYPE_VCARD) ||
        (type == EXPORT_TYPE_VCARD_GMAIL)) {
        get_pref(PREF_CHAR_SET, &char_set, NULL);
        get_pref(PREF_USER, NULL, &svalue);
        /* Convert User Name stored in Palm character set */
        g_strlcpy(text, svalue, 128);
        charset_p2j(text, 128, char_set);
        str_to_ical_str(username, sizeof(username), text);
        get_pref(PREF_USER_ID, &userid, NULL);
        gethostname(text, sizeof(text));
        text[sizeof(text) - 1] = '\0';
        str_to_ical_str(hostname, sizeof(hostname), text);
    }

    /* Check encoding for LDIF output */
    if (type == EXPORT_TYPE_LDIF) {
        get_pref(PREF_CHAR_SET, &char_set, NULL);
        if (char_set < CHAR_SET_UTF) {
            jp_logf(JP_LOG_WARN, _("Host character encoding is not UTF-8 based.\n"
                                   " Exported ldif file may not be standards-compliant\n"));
        }
    }

    get_pref(PREF_CHAR_SET, &char_set, NULL);
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeView));
    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeView));
    list = gtk_tree_selection_get_selected_rows(selection, &model);

    /* Loop over list of records to export */
    for (record_num = 0, temp_list = list; temp_list; temp_list = temp_list->next, record_num++) {
        GtkTreePath *path = temp_list->data;
        GtkTreeIter iter;
        if (gtk_tree_model_get_iter(model, &iter, path)) {
            gtk_tree_model_get(model, &iter, ADDRESS_DATA_COLUMN_ENUM, &mcont, -1);
            if (!mcont) {
                continue;
                jp_logf(JP_LOG_WARN, _("Can't export address %d\n"), (long) temp_list->data + 1);
            }

            switch (type) {
                case EXPORT_TYPE_TEXT:
                    utf = charset_p2newj(contact_app_info.category.name[mcont->attrib & 0x0F], 16, char_set);
                    fprintf(out, _("Category: %s\n"), utf);
                    g_free(utf);
                    fprintf(out, _("Private: %s\n"),
                            (mcont->attrib & dlpRecAttrSecret) ? _("Yes") : _("No"));

                    for (i = 0; i < schema_size; i++) {
                        /* Special handling for birthday which doesn't have an entry
                 * field but instead has a flag and a tm struct field */
                        if (schema[i].type == ADDRESS_GUI_BIRTHDAY) {
                            if (mcont->cont.birthdayFlag) {
                                fprintf(out, _("%s: "), contact_app_info.labels[schema[i].record_field]
                                                        ? contact_app_info.labels[schema[i].record_field] : "");
                                birthday_str[0] = '\0';
                                get_pref(PREF_SHORTDATE, NULL, &pref_date);
                                strftime(birthday_str, sizeof(birthday_str), pref_date, &(mcont->cont.birthday));
                                fprintf(out, _("%s\n"), birthday_str);
                                continue;
                            }
                        }

                        if (mcont->cont.entry[schema[i].record_field]) {
                            /* Print labels for menu selectable fields (Work, Fax, etc.) */
                            switch (schema[i].type) {
                                case ADDRESS_GUI_IM_MENU_TEXT:
                                    index = mcont->cont.IMLabel[i - contIM1];
                                    fprintf(out, _("%s: "), contact_app_info.IMLabels[index]);
                                    break;
                                case ADDRESS_GUI_DIAL_SHOW_PHONE_MENU_TEXT:
                                    index = mcont->cont.phoneLabel[i - contPhone1];
                                    fprintf(out, _("%s: "), contact_app_info.phoneLabels[index]);
                                    break;
                                case ADDRESS_GUI_ADDR_MENU_TEXT:
                                    switch (schema[i].record_field) {
                                        case contAddress1 :
                                            index = 0;
                                            break;
                                        case contAddress2 :
                                            index = 1;
                                            break;
                                        case contAddress3 :
                                            index = 2;
                                            break;
                                        default:
                                            break;
                                    }
                                    index = mcont->cont.addressLabel[index];
                                    fprintf(out, _("%s: "),
                                            contact_app_info.addrLabels[mcont->cont.addressLabel[index]]);
                                    break;
                                default:
                                    fprintf(out, _("%s: "), contact_app_info.labels[schema[i].record_field]
                                                            ? contact_app_info.labels[schema[i].record_field] : "");
                            }
                            /* Next print the entry field */
                            switch (schema[i].type) {
                                case ADDRESS_GUI_LABEL_TEXT:
                                case ADDRESS_GUI_DIAL_SHOW_PHONE_MENU_TEXT:
                                case ADDRESS_GUI_IM_MENU_TEXT:
                                case ADDRESS_GUI_ADDR_MENU_TEXT:
                                case ADDRESS_GUI_WEBSITE_TEXT:
                                    fprintf(out, "%s\n", mcont->cont.entry[schema[i].record_field]);
                                    break;
                                default:
                                    break;
                            }
                        }
                    }
                    fprintf(out, "\n");

                    break;

                case EXPORT_TYPE_CSV:
                    /* Category name */
                    utf = charset_p2newj(contact_app_info.category.name[mcont->attrib & 0x0F], 16, char_set);
                    str_to_csv_str(csv_text, utf);
                    fprintf(out, "\"%s\",", csv_text);
                    g_free(utf);

                    /* Private */
                    fprintf(out, "\"%s\",", (mcont->attrib & dlpRecAttrSecret) ? "1" : "0");

                    address_i = phone_i = IM_i = 0;
                    /* The Contact entry values */
                    for (i = 0; i < schema_size; i++) {
                        switch (schema[i].type) {
                            /* For labels that are menu selectable ("Work", Fax", etc)
                  * we list what they are set to in the record */
                            case ADDRESS_GUI_IM_MENU_TEXT:
                                str_to_csv_str(csv_text, contact_app_info.IMLabels[mcont->cont.IMLabel[IM_i]]);
                                fprintf(out, "\"%s\",", csv_text);
                                str_to_csv_str(csv_text, mcont->cont.entry[schema[i].record_field] ?
                                                         mcont->cont.entry[schema[i].record_field] : "");
                                fprintf(out, "\"%s\",", csv_text);
                                IM_i++;
                                break;
                            case ADDRESS_GUI_DIAL_SHOW_PHONE_MENU_TEXT:
                                str_to_csv_str(csv_text, contact_app_info.phoneLabels[mcont->cont.phoneLabel[phone_i]]);
                                fprintf(out, "\"%s\",", csv_text);
                                str_to_csv_str(csv_text, mcont->cont.entry[schema[i].record_field] ?
                                                         mcont->cont.entry[schema[i].record_field] : "");
                                fprintf(out, "\"%s\",", csv_text);
                                phone_i++;
                                break;
                            case ADDRESS_GUI_ADDR_MENU_TEXT:
                                str_to_csv_str(csv_text,
                                               contact_app_info.addrLabels[mcont->cont.addressLabel[address_i]]);
                                fprintf(out, "\"%s\",", csv_text);
                                str_to_csv_str(csv_text, mcont->cont.entry[schema[i].record_field] ?
                                                         mcont->cont.entry[schema[i].record_field] : "");
                                fprintf(out, "\"%s\",", csv_text);
                                address_i++;
                                break;
                            case ADDRESS_GUI_LABEL_TEXT:
                            case ADDRESS_GUI_WEBSITE_TEXT:
                                str_to_csv_str(csv_text, mcont->cont.entry[schema[i].record_field] ?
                                                         mcont->cont.entry[schema[i].record_field] : "");
                                fprintf(out, "\"%s\",", csv_text);
                                break;
                            case ADDRESS_GUI_BIRTHDAY:
                                if (mcont->cont.birthdayFlag) {
                                    birthday_str[0] = '\0';
                                    strftime(birthday_str, sizeof(birthday_str), "%Y/%02m/%02d",
                                             &(mcont->cont.birthday));
                                    fprintf(out, "\"%s\",", birthday_str);

                                    if (mcont->cont.reminder) {
                                        fprintf(out, "\"%d\",", mcont->cont.advance);
                                    } else {
                                        fprintf(out, "\"\",");
                                    }

                                } else {
                                    fprintf(out, "\"\",");  /* for null Birthday field */
                                    fprintf(out, "\"\",");  /* for null Birthday Reminder field */
                                }
                                break;
                            default:
                                break;
                        }
                    }

                    fprintf(out, "\"%d\"\n", mcont->cont.showPhone);
                    break;

                case EXPORT_TYPE_BFOLDERS:
                    /* fprintf(out, "%s",
                     "Name, Email, Phone (mobile), Company, Title, Website, Phone (work),"
                     "Phone 2(work), Fax (work), Address (work), Phone (home), Address (home), "
                     "\"Custom Label 1\", \"Custom Value 1\", \"Custom Label 2\", \"Custom Value 2\","
                     "\"Custom Label 3\",\"Custom Value 3\",\"Custom Label 4\",\"Custom Value 4\","
                     "\"Custom Label 5\",\"Custom Value 5\",Note,Folder");
              */
                    //address_i = phone_i = IM_i = 0;
                    for (i = 0; i < schema_size; i++) {
                        if (schema[i].record_field == contLastname) {
                            str_to_csv_str(csv_text, mcont->cont.entry[schema[i].record_field] ?
                                                     mcont->cont.entry[schema[i].record_field] : "");
                            fprintf(out, "\"%s, ", csv_text);
                        }
                    }
                    for (i = 0; i < schema_size; i++) {
                        if (schema[i].record_field == contFirstname) {
                            str_to_csv_str(csv_text, mcont->cont.entry[schema[i].record_field] ?
                                                     mcont->cont.entry[schema[i].record_field] : "");
                            fprintf(out, "%s\",", csv_text);
                        }
                    }
                    /* E-Mail */
                    /*
             for (i=0; i<schema_size; i++) {
                if (!strcasecmp(contact_app_info.phoneLabels[cont->phoneLabel[phone_i]], _("E-mail"))) {
                    g_object_set_data(G_OBJECT(dial_button[phone_i]), "mail", GINT_TO_POINTER(1));
                   gtk_button_set_label(GTK_BUTTON(dial_button[phone_i]), _("Mail"));
                }
                fprintf(out, "%s\",", csv_text);
             }
    */
                    fprintf(out, "%s",
                            "\"\", \"\", \"\", \"\", \"\","
                            "\"\", \"\", \"\", \"\", \"\", "
                            "\"Custom Label 1\", \"Custom Value 1\", \"Custom Label 2\", \"Custom Value 2\","
                            "\"Custom Label 3\",\"Custom Value 3\",\"Custom Label 4\",\"Custom Value 4\","
                            "\"Custom Label 5\",\"Custom Value 5\",\"Note\",\"Contacts\"");
                    fprintf(out, "\n");

                    break;

                case EXPORT_TYPE_VCARD:
                case EXPORT_TYPE_VCARD_GMAIL:
                    /* RFC 2426: vCard MIME Directory Profile */
                    fprintf(out, "BEGIN:VCARD"CRLF);
                    fprintf(out, "VERSION:3.0"CRLF);
                    fprintf(out, "PRODID:%s"CRLF, FPI_STRING);
                    if (mcont->attrib & dlpRecAttrSecret) {
                        fprintf(out, "CLASS:PRIVATE"CRLF);
                    }
                    fprintf(out, "UID:palm-addressbook-%08x-%08lx-%s@%s"CRLF,
                            mcont->unique_id, userid, username, hostname);
                    utf = charset_p2newj(contact_app_info.category.name[mcont->attrib & 0x0F], 16, char_set);
                    str_to_vcard_str(csv_text, sizeof(csv_text), utf);
                    fprintf(out, "CATEGORIES:%s"CRLF, csv_text);
                    g_free(utf);
                    if (mcont->cont.entry[contLastname] || mcont->cont.entry[contFirstname]) {
                        char *last = mcont->cont.entry[contLastname];
                        char *first = mcont->cont.entry[contFirstname];
                        fprintf(out, "FN:");
                        if (first) {
                            str_to_vcard_str(csv_text, sizeof(csv_text), first);
                            fprintf(out, "%s", csv_text);
                        }
                        if (first && last) {
                            fprintf(out, " ");
                        }
                        if (last) {
                            str_to_vcard_str(csv_text, sizeof(csv_text), last);
                            fprintf(out, "%s", csv_text);
                        }
                        fprintf(out, CRLF);
                        fprintf(out, "N:");
                        if (last) {
                            str_to_vcard_str(csv_text, sizeof(csv_text), last);
                            fprintf(out, "%s", csv_text);
                        }
                        fprintf(out, ";");
                        /* split up first into first + middle and do first;middle,middle*/
                        if (first) {
                            str_to_vcard_str(csv_text, sizeof(csv_text), first);
                            fprintf(out, "%s", csv_text);
                        }
                        fprintf(out, CRLF);
                    } else if (mcont->cont.entry[contCompany]) {
                        str_to_vcard_str(csv_text, sizeof(csv_text), mcont->cont.entry[contCompany]);
                        fprintf(out, "FN:%s"CRLF"N:%s"CRLF, csv_text, csv_text);
                    } else {
                        fprintf(out, "FN:-Unknown-"CRLF"N:known-;-Un"CRLF);
                    }
                    if (mcont->cont.entry[contTitle]) {
                        str_to_vcard_str(csv_text, sizeof(csv_text), mcont->cont.entry[contTitle]);
                        fprintf(out, "TITLE:%s"CRLF, csv_text);
                    }
                    if (mcont->cont.entry[contCompany]) {
                        str_to_vcard_str(csv_text, sizeof(csv_text), mcont->cont.entry[contCompany]);
                        fprintf(out, "ORG:%s"CRLF, csv_text);
                    }
                    for (n = contPhone1; n < contPhone7 + 1; n++) {
                        if (mcont->cont.entry[n]) {
                            str_to_vcard_str(csv_text, sizeof(csv_text), mcont->cont.entry[n]);
                            /* E-mail should be the Palm dropdown menu item for email */
                            if (!strcasecmp(contact_app_info.phoneLabels[mcont->cont.phoneLabel[n - contPhone1]],
                                            _("E-mail"))) {
                                fprintf(out, "EMAIL:%s"CRLF, csv_text);
                            } else {
                                fprintf(out, "TEL;TYPE=%s", vCardMapType(mcont->cont.phoneLabel[n - contPhone1]));
                                if (mcont->cont.showPhone == n - contPhone1) {
                                    fprintf(out, ",pref");
                                }
                                fprintf(out, ":%s"CRLF, csv_text);
                            }
                        }
                    }
                    for (i = 0; i < NUM_ADDRESSES; i++) {
                        int address_il = 0, city_i = 0, state_i = 0, zip_i = 0, country_i = 0;
                        switch (i) {
                            case 0:
                                address_il = contAddress1;
                                city_i = contCity1;
                                state_i = contState1;
                                zip_i = contZip1;
                                country_i = contCountry1;
                                break;
                            case 1:
                                address_il = contAddress2;
                                city_i = contCity2;
                                state_i = contState2;
                                zip_i = contZip2;
                                country_i = contCountry2;
                                break;
                            case 2:
                                address_il = contAddress3;
                                city_i = contCity3;
                                state_i = contState3;
                                zip_i = contZip3;
                                country_i = contCountry3;
                                break;
                            default:
                                break;
                        }
                        if (mcont->cont.entry[address_il] ||
                            mcont->cont.entry[city_i] ||
                            mcont->cont.entry[state_i] ||
                            mcont->cont.entry[zip_i] ||
                            mcont->cont.entry[country_i]) {

                            /* Should we rely on the label, or the label index, for the addr
                    * type?  The label depends on the translated text.  I'll go
                    * with index for now.  The text is here:
                   contact_app_info.addrLabels[mcont->cont.addressLabel[i]] */

                            switch (mcont->cont.addressLabel[i]) {
                                case 0:
                                    fprintf(out, "ADR;TYPE=WORK:;;");
                                    break;
                                case 1:
                                    fprintf(out, "ADR;TYPE=HOME:;;");
                                    break;
                                default:
                                    fprintf(out, "ADR:;;");
                            }

                            for (n = address_il; n < country_i + 1; n++) {
                                if (mcont->cont.entry[n]) {
                                    str_to_vcard_str(csv_text, sizeof(csv_text), mcont->cont.entry[n]);
                                    fprintf(out, "%s", csv_text);
                                }
                                if (n < country_i) {
                                    fprintf(out, ";");
                                }
                            }
                            fprintf(out, CRLF);
                        }
                    }
                    for (i = 0; i < NUM_IMS; i++) {
                        int im_i = 0;
                        switch (i) {
                            case 0:
                                im_i = contIM1;
                                break;
                            case 1:
                                im_i = contIM2;
                                break;
                            default:
                                break;
                        }
                        if (mcont->cont.entry[im_i]) {
                            int i_label = mcont->cont.IMLabel[i];
                            const gchar *label = contact_app_info.IMLabels[i_label];
                            gchar *vlabel;
                            if (strcmp(label, "AOL ICQ") == 0)
                                label = "ICQ";
                            vlabel = g_strcanon(g_ascii_strup(label, -1),
                                                "ABCDEFGHIJKLMNOPQRSTUVWXYZ-", '-');
                            fprintf(out, "X-%s:", vlabel);
                            g_free(vlabel);
                            str_to_vcard_str(csv_text, sizeof(csv_text), mcont->cont.entry[im_i]);
                            fprintf(out, "%s"CRLF, csv_text);
                        }
                    }
                    if (mcont->cont.entry[contWebsite]) {
                        str_to_vcard_str(csv_text, sizeof(csv_text),
                                         mcont->cont.entry[contWebsite]);
                        fprintf(out, "URL:%s"CRLF, csv_text);
                    }
                    if (mcont->cont.birthdayFlag) {
                        char birthday_str_l[255];
                        strftime(birthday_str_l, sizeof(birthday_str), "%F", &mcont->cont.birthday);
                        str_to_vcard_str(csv_text, sizeof(csv_text), birthday_str_l);
                        fprintf(out, "BDAY:%s"CRLF, birthday_str);
                    }
                    if (type == EXPORT_TYPE_VCARD_GMAIL) {
                        /* Gmail contacts don't have fields for the custom fields,
                 * rather than lose them we can stick them all in a note field */
                        int printed_note = 0;
                        for (n = contCustom1; n <= contCustom9; n++) {
                            if (mcont->cont.entry[n]) {
                                if (!printed_note) {
                                    printed_note = 1;
                                    fprintf(out, "NOTE:");
                                } else {
                                    fprintf(out, " ");
                                }
                                str_to_vcard_str(csv_text, sizeof(csv_text), mcont->cont.entry[n]);
                                fprintf(out, "%s:%s\\n"CRLF, contact_app_info.customLabels[n - contCustom1], csv_text);
                            }
                        }
                        if (mcont->cont.entry[contNote]) {
                            if (!printed_note) {
                                fprintf(out, "NOTE:");
                            } else {
                                fprintf(out, " note:");
                            }
                            str_to_vcard_str(csv_text, sizeof(csv_text), mcont->cont.entry[contNote]);
                            fprintf(out, "%s\\n"CRLF, csv_text);
                        }
                    } else { /* Not a Gmail optimized export */
                        if (mcont->cont.entry[contCustom1] ||
                            mcont->cont.entry[contCustom2] ||
                            mcont->cont.entry[contCustom3] ||
                            mcont->cont.entry[contCustom4] ||
                            mcont->cont.entry[contCustom5] ||
                            mcont->cont.entry[contCustom6] ||
                            mcont->cont.entry[contCustom7] ||
                            mcont->cont.entry[contCustom8] ||
                            mcont->cont.entry[contCustom9]) {
                            for (n = contCustom1; n <= contCustom9; n++) {
                                if (mcont->cont.entry[n]) {
                                    const gchar *label = contact_app_info.customLabels[n - contCustom1];
                                    gchar *vlabel;
                                    vlabel = g_strcanon(g_ascii_strup(label, -1),
                                                        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ-", '-');
                                    fprintf(out, "X-%s:", vlabel);
                                    g_free(vlabel);
                                    str_to_vcard_str(csv_text, sizeof(csv_text), mcont->cont.entry[n]);
                                    fprintf(out, "%s"CRLF, csv_text);
                                }
                            }
                        }
                        if (mcont->cont.entry[contNote]) {
                            fprintf(out, "NOTE:");
                            str_to_vcard_str(csv_text, sizeof(csv_text), mcont->cont.entry[contNote]);
                            fprintf(out, "%s\\n"CRLF, csv_text);
                        }
                    }

                    fprintf(out, "END:VCARD"CRLF);
                    break;

                case EXPORT_TYPE_LDIF:
                    /* RFC 2256 - organizationalPerson */
                    /* RFC 2798 - inetOrgPerson */
                    /* RFC 2849 - LDIF file format */
                    if (record_num == 0) {
                        fprintf(out, "version: 1\n");
                    }
                    {
                        char *cn;
                        char *email = NULL;
                        char *last = mcont->cont.entry[contLastname];
                        char *first = mcont->cont.entry[contFirstname];
                        for (n = contPhone1; n <= contPhone7; n++) {
                            if (mcont->cont.entry[n] && mcont->cont.phoneLabel[n - contPhone1] == 4) {
                                email = mcont->cont.entry[n];
                                break;
                            }
                        }
                        if (first || last) {
                            cn = csv_text;
                            snprintf(csv_text, sizeof(csv_text), "%s%s%s", first ? first : "",
                                     first && last ? " " : "", last ? last : "");
                            if (!last) {
                                last = first;
                                first = NULL;
                            }
                        } else if (mcont->cont.entry[contCompany]) {
                            last = mcont->cont.entry[contCompany];
                            cn = last;
                        } else {
                            last = "Unknown";
                            cn = last;
                        }
                        /* maybe add dc=%s for each part of the email address? */
                        /* Mozilla just does mail=%s */
                        ldif_out(out, "dn", "cn=%s%s%s", cn, email ? ",mail=" : "",
                                 email ? email : "");
                        fprintf(out, "dnQualifier: %s\n", PN);
                        fprintf(out, "objectClass: top\nobjectClass: person\n");
                        fprintf(out, "objectClass: organizationalPerson\n");
                        fprintf(out, "objectClass: inetOrgPerson\n");
                        ldif_out(out, "cn", "%s", cn);
                        ldif_out(out, "sn", "%s", last);
                        if (first)
                            ldif_out(out, "givenName", "%s", first);
                        if (mcont->cont.entry[contCompany])
                            ldif_out(out, "o", "%s", mcont->cont.entry[contCompany]);
                        for (n = contPhone1; n <= contPhone7; n++) {
                            if (mcont->cont.entry[n]) {
                                ldif_out(out, ldifMapType(mcont->cont.phoneLabel[n - contPhone1]), "%s",
                                         mcont->cont.entry[n]);
                            }
                        }
                        if (mcont->cont.entry[contAddress1])
                            ldif_out(out, "postalAddress", "%s", mcont->cont.entry[contAddress1]);
                        if (mcont->cont.entry[contCity1])
                            ldif_out(out, "l", "%s", mcont->cont.entry[contCity1]);
                        if (mcont->cont.entry[contState1])
                            ldif_out(out, "st", "%s", mcont->cont.entry[contState1]);
                        if (mcont->cont.entry[contZip1])
                            ldif_out(out, "postalCode", "%s", mcont->cont.entry[contZip1]);
                        if (mcont->cont.entry[contCountry1])
                            ldif_out(out, "c", "%s", mcont->cont.entry[contCountry1]);

                        if (mcont->cont.entry[contAddress2])
                            ldif_out(out, "postalAddress", "%s", mcont->cont.entry[contAddress2]);
                        if (mcont->cont.entry[contCity2])
                            ldif_out(out, "l", "%s", mcont->cont.entry[contCity2]);
                        if (mcont->cont.entry[contState2])
                            ldif_out(out, "st", "%s", mcont->cont.entry[contState2]);
                        if (mcont->cont.entry[contZip2])
                            ldif_out(out, "postalCode", "%s", mcont->cont.entry[contZip2]);
                        if (mcont->cont.entry[contCountry2])
                            ldif_out(out, "c", "%s", mcont->cont.entry[contCountry2]);

                        if (mcont->cont.entry[contAddress3])
                            ldif_out(out, "postalAddress", "%s", mcont->cont.entry[contAddress3]);
                        if (mcont->cont.entry[contCity3])
                            ldif_out(out, "l", "%s", mcont->cont.entry[contCity3]);
                        if (mcont->cont.entry[contState3])
                            ldif_out(out, "st", "%s", mcont->cont.entry[contState3]);
                        if (mcont->cont.entry[contZip3])
                            ldif_out(out, "postalCode", "%s", mcont->cont.entry[contZip3]);
                        if (mcont->cont.entry[contCountry3])
                            ldif_out(out, "c", "%s", mcont->cont.entry[contCountry3]);

                        if (mcont->cont.entry[contIM1]) {
                            strncpy(text, contact_app_info.IMLabels[mcont->cont.IMLabel[0]], 100);
                            ldif_out(out, text, "%s", mcont->cont.entry[contIM1]);
                        }
                        if (mcont->cont.entry[contIM2]) {
                            strncpy(text, contact_app_info.IMLabels[mcont->cont.IMLabel[1]], 100);
                            ldif_out(out, text, "%s", mcont->cont.entry[contIM2]);
                        }

                        if (mcont->cont.entry[contWebsite])
                            ldif_out(out, "website", "%s", mcont->cont.entry[contWebsite]);
                        if (mcont->cont.entry[contTitle])
                            ldif_out(out, "title", "%s", mcont->cont.entry[contTitle]);
                        if (mcont->cont.entry[contCustom1])
                            ldif_out(out, "custom1", "%s", mcont->cont.entry[contCustom1]);
                        if (mcont->cont.entry[contCustom2])
                            ldif_out(out, "custom2", "%s", mcont->cont.entry[contCustom2]);
                        if (mcont->cont.entry[contCustom3])
                            ldif_out(out, "custom3", "%s", mcont->cont.entry[contCustom3]);
                        if (mcont->cont.entry[contCustom4])
                            ldif_out(out, "custom4", "%s", mcont->cont.entry[contCustom4]);
                        if (mcont->cont.entry[contCustom5])
                            ldif_out(out, "custom5", "%s", mcont->cont.entry[contCustom5]);
                        if (mcont->cont.entry[contCustom6])
                            ldif_out(out, "custom6", "%s", mcont->cont.entry[contCustom6]);
                        if (mcont->cont.entry[contCustom7])
                            ldif_out(out, "custom7", "%s", mcont->cont.entry[contCustom7]);
                        if (mcont->cont.entry[contCustom8])
                            ldif_out(out, "custom8", "%s", mcont->cont.entry[contCustom8]);
                        if (mcont->cont.entry[contCustom9])
                            ldif_out(out, "custom9", "%s", mcont->cont.entry[contCustom9]);
                        if (mcont->cont.entry[contNote])
                            ldif_out(out, "description", "%s", mcont->cont.entry[contNote]);
                        fprintf(out, "\n");
                        break;
                    }

                default:
                    jp_logf(JP_LOG_WARN, _("Unknown export type\n"));
            }
        }
    }
    if (out) {
        fclose(out);
    }
}

static GtkWidget *cb_addr_export_init_treeView() {
    GtkListStore *localListStore = gtk_list_store_new(ADDRESS_NUM_COLS, G_TYPE_STRING, GDK_TYPE_PIXBUF,
                                                      G_TYPE_STRING, G_TYPE_POINTER, GDK_TYPE_RGBA,
                                                      G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_BOOLEAN);
    GtkTreeModel *model = GTK_TREE_MODEL(localListStore);
    GtkTreeView *localTreeView = GTK_TREE_VIEW(gtk_tree_view_new_with_model(model));
    GtkCellRenderer *nameRenderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *nameColumn = gtk_tree_view_column_new_with_attributes(ADDRESS_LAST_NAME_COMPANY, nameRenderer,
                                                                             "text",
                                                                             ADDRESS_NAME_COLUMN_ENUM,
                                                                             "cell-background-rgba",
                                                                             ADDRESS_BACKGROUND_COLOR_ENUM,
                                                                             "cell-background-set",
                                                                             ADDRESS_BACKGROUND_COLOR_ENABLED_ENUM,
                                                                             NULL);
    gtk_tree_view_column_set_clickable(nameColumn, FALSE);

    GtkCellRenderer *noteRenderer = gtk_cell_renderer_pixbuf_new();
    GtkTreeViewColumn *noteColumn = gtk_tree_view_column_new_with_attributes("", noteRenderer, "pixbuf",
                                                                             ADDRESS_NOTE_COLUMN_ENUM,
                                                                             "cell-background-rgba",
                                                                             ADDRESS_BACKGROUND_COLOR_ENUM,
                                                                             "cell-background-set",
                                                                             ADDRESS_BACKGROUND_COLOR_ENABLED_ENUM,
                                                                             NULL);
    gtk_tree_view_column_set_clickable(noteColumn, FALSE);

    GtkCellRenderer *phoneRenderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *phoneColumn = gtk_tree_view_column_new_with_attributes("Phone", phoneRenderer, "text",
                                                                              ADDRESS_PHONE_COLUMN_ENUM,
                                                                              "cell-background-rgba",
                                                                              ADDRESS_BACKGROUND_COLOR_ENUM,
                                                                              "cell-background-set",
                                                                              ADDRESS_BACKGROUND_COLOR_ENABLED_ENUM,
                                                                              NULL);
    gtk_tree_view_column_set_clickable(phoneColumn, FALSE);
    gtk_tree_view_column_set_sizing(nameColumn, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    gtk_tree_view_column_set_sizing(noteColumn, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    gtk_tree_view_column_set_sizing(phoneColumn, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    gtk_tree_view_insert_column(GTK_TREE_VIEW(localTreeView), nameColumn, ADDRESS_NAME_COLUMN_ENUM);
    gtk_tree_view_insert_column(GTK_TREE_VIEW(localTreeView), noteColumn, ADDRESS_NOTE_COLUMN_ENUM);
    gtk_tree_view_insert_column(GTK_TREE_VIEW(localTreeView), phoneColumn, ADDRESS_PHONE_COLUMN_ENUM);
    return GTK_WIDGET(localTreeView);
}

static void cb_addr_update_listStore(GtkWidget *ptreeView, int category) {
    address_update_listStore(GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(ptreeView))), NULL,
                             &export_contact_list, category, FALSE);
}


static void cb_addr_export_done(GtkWidget *widget, const char *filename) {
    free_ContactList(&export_contact_list);
    set_pref(PREF_ADDRESS_EXPORT_FILENAME, 0, filename, TRUE);
}


//TODO: fix this when working on exports.
int address_export(GtkWidget *window) {
    int w, h, x, y;
    char *type_text[] = {N_("Text"),
                         N_("CSV"),
                         N_("vCard"),
                         N_("vCard (Optimized for Gmail/Android Import)"),
                         N_("ldif"),
                         N_("B-Folders CSV"),
                         NULL};
    int type_int[] = {EXPORT_TYPE_TEXT,
                      EXPORT_TYPE_CSV,
                      EXPORT_TYPE_VCARD,
                      EXPORT_TYPE_VCARD_GMAIL,
                      EXPORT_TYPE_LDIF,
                      EXPORT_TYPE_BFOLDERS};

    w = gdk_window_get_width(gtk_widget_get_window(window));
    h = gdk_window_get_height(gtk_widget_get_window(window));
    gdk_window_get_root_origin(gtk_widget_get_window(window), &x, &y);

    w = gtk_paned_get_position(GTK_PANED(pane));
    x += 40;

    export_gui(window,
               w, h, x, y, 3, sort_l,
               PREF_ADDRESS_EXPORT_FILENAME,
               type_text,
               type_int,
               cb_addr_export_init_treeView,
               cb_addr_update_listStore,
               cb_addr_export_done,
               cb_addr_export_ok
    );

    return EXIT_SUCCESS;
}

/* End Export Code */

static void cb_resize_name_column(GtkTreeViewColumn *column) {
    if (column == NULL) {
        return;
    }
    int width = gtk_tree_view_column_get_width(column);
    set_pref(PREF_ADDR_NAME_COL_SZ, width, NULL, TRUE);
}

/* Find position of category in sorted category array
 * via its assigned category number */
static int find_sort_cat_pos(int cat) {
    int i;

    jp_logf(JP_LOG_DEBUG, "find_sort_cat_pos(), cat=%d\n",cat);
    for (i = 0; i < NUM_ADDRESS_CAT_ITEMS; i++) {
        if (sort_l[i].cat_num == cat) {
            return i;
        }
    }

    jp_logf(JP_LOG_DEBUG, "find_sort_cat_pos(), not found, returning (-1)\n");
    return -1;
}

/* Find a category's position in the category menu.
 * This is equal to the category number except for the Unfiled category.
 * The Unfiled category is always in the last position which changes as
 * the number of categories changes */
static int find_menu_cat_pos(int cat) {
    int i;

    jp_logf(JP_LOG_DEBUG, "find_menu_cat_pos(), cat=%d\n",cat);
    if (cat != NUM_ADDRESS_CAT_ITEMS - 1) {
        return cat;
    } else { /* Unfiled category */
        /* Count how many category entries are filled */
        for (i = 0; i < NUM_ADDRESS_CAT_ITEMS; i++) {
            if (!sort_l[i].Pcat[0]) {
                return i;
            }
        }
        return 0;
    }
}

gboolean deleteAddressRecord(GtkTreeModel *model,
                             GtkTreePath *path,
                             GtkTreeIter *iter,
                             gpointer data) {
    int *i = gtk_tree_path_get_indices(path);
    if (i[0] == rowSelected) {
        MyContact *mcont = NULL;
        gtk_tree_model_get(model, iter, ADDRESS_DATA_COLUMN_ENUM, &mcont, -1);
        deleteAddress(mcont, data);
        return TRUE;
    }

    return FALSE;


}

gboolean addNewAddressRecord(GtkTreeModel *model,
                             GtkTreePath *path,
                             GtkTreeIter *iter,
                             gpointer data) {
    int *i = gtk_tree_path_get_indices(path);
    if (i[0] == rowSelected) {
        MyContact *mcont = NULL;
        gtk_tree_model_get(model, iter, ADDRESS_DATA_COLUMN_ENUM, &mcont, -1);
        addNewAddressRecordToDataStructure(mcont, data);
        return TRUE;
    }
    return FALSE;
}

void addNewAddressRecordToDataStructure(MyContact *mcont, gpointer data) {
    int i;
    struct Contact cont;
    struct Address addr;
    unsigned char attrib;
    int address_i, IM_i, phone_i;
    int flag, type;
    unsigned int unique_id;
    int show_priv;
    GtkTextIter start_iter;
    GtkTextIter end_iter;

    memset(&cont, 0, sizeof(cont));
    flag = GPOINTER_TO_INT(data);
    unique_id = 0;

    /* Do masking like Palm OS 3.5 */
    if ((flag == COPY_FLAG) || (flag == MODIFY_FLAG)) {
        show_priv = show_privates(GET_PRIVATES);
        if (mcont < (MyContact *) LIST_MIN_DATA) {
            return;
        }
        if ((show_priv != SHOW_PRIVATES) &&
            (mcont->attrib & dlpRecAttrSecret)) {
            return;
        }
    }
    /* End Masking */
    if ((flag == NEW_FLAG) || (flag == COPY_FLAG) || (flag == MODIFY_FLAG)) {
        /* These rec_types are both the same for now */
        if (flag == MODIFY_FLAG) {
            unique_id = mcont->unique_id;
            if (mcont < (MyContact *) LIST_MIN_DATA) {
                return;
            }
            if ((mcont->rt == DELETED_PALM_REC) ||
                (mcont->rt == DELETED_PC_REC) ||
                (mcont->rt == MODIFIED_PALM_REC)) {
                jp_logf(JP_LOG_INFO, _("You can't modify a record that is deleted\n"));
                return;
            }
        }

        cont.showPhone = 0;

        /* Get the menu labels and settings */
        address_i = IM_i = phone_i = 0;
        for (i = 0; i < schema_size; i++) {
            switch (schema[i].type) {
                case ADDRESS_GUI_DIAL_SHOW_PHONE_MENU_TEXT:
                    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio_button[phone_i]))) {
                        cont.showPhone = phone_i;
                    }
                    cont.phoneLabel[phone_i] = address_phone_label_selected[phone_i];
                    phone_i++;
                    break;
                case ADDRESS_GUI_IM_MENU_TEXT:
                    cont.IMLabel[IM_i] = IM_type_selected[IM_i];
                    IM_i++;
                    break;
                case ADDRESS_GUI_ADDR_MENU_TEXT:
                    cont.addressLabel[address_i] = address_type_selected[address_i];
                    address_i++;
                    break;
                case ADDRESS_GUI_BIRTHDAY:
                    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(birthday_checkbox))) {
                        cont.birthdayFlag = 1;
                        memcpy(&cont.birthday, &birthday, sizeof(struct tm));
                    }
                    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(reminder_checkbox))) {
                        cont.reminder = 1;
                        cont.advance = atoi(gtk_entry_get_text(GTK_ENTRY(reminder_entry)));
                        cont.advanceUnits = 1; /* Days */
                    }
                    break;
                case ADDRESS_GUI_LABEL_TEXT:
                case ADDRESS_GUI_WEBSITE_TEXT:
                    break;
                default:
                    break;
            }
        }

        /* Get the entry texts */
        for (i = 0; i < schema_size; i++) {
            switch (schema[i].type) {
                case ADDRESS_GUI_LABEL_TEXT:
                case ADDRESS_GUI_DIAL_SHOW_PHONE_MENU_TEXT:
                case ADDRESS_GUI_IM_MENU_TEXT:
                case ADDRESS_GUI_ADDR_MENU_TEXT:
                case ADDRESS_GUI_WEBSITE_TEXT:
                    gtk_text_buffer_get_bounds(GTK_TEXT_BUFFER(addr_text_buffer[schema[i].record_field]), &start_iter,
                                               &end_iter);
                    cont.entry[schema[i].record_field] =
                            gtk_text_buffer_get_text(GTK_TEXT_BUFFER(addr_text_buffer[schema[i].record_field]),
                                                     &start_iter, &end_iter, TRUE);
                    break;
                case ADDRESS_GUI_BIRTHDAY:
                    break;
                default:
                    break;
            }
        }

        /* Get the picture */
        if (contact_picture.data) {
            jp_Contact_add_picture(&cont, &contact_picture);
        }

        /* Get the attributes */
        get_address_attrib(&attrib);

        set_new_button_to(CLEAR_FLAG);

        if (flag == MODIFY_FLAG) {
            cb_delete_address_or_contact(NULL, data);
            if ((mcont->rt == PALM_REC) || (mcont->rt == REPLACEMENT_PALM_REC)) {
                type = REPLACEMENT_PALM_REC;
            } else {
                unique_id = 0;
                type = NEW_PC_REC;
            }
        } else {
            unique_id = 0;
            type = NEW_PC_REC;
        }

        if (address_version == 0) {
            copy_contact_to_address(&cont, &addr);
            jp_free_Contact(&cont);
            if (glob_sqlite) jpsqlite_AddrINS(&addr, type, attrib, &unique_id);
            else pc_address_write(&addr, type, attrib, &unique_id);
            free_Address(&addr);
        } else {
            pc_contact_write(&cont, type, attrib, &unique_id);
            jp_free_Contact(&cont);
        }

        /* Don't return to modified record if search gui active */
        if (!glob_find_id) {
            glob_find_id = unique_id;
        }
        address_redraw();
    }
}

gboolean deleteAddressContactRecord(GtkTreeModel *model,
                                    GtkTreePath *path,
                                    GtkTreeIter *iter,
                                    gpointer data) {
    int *i = gtk_tree_path_get_indices(path);
    if (i[0] == rowSelected) {
        MyContact *mcont = NULL;
        gtk_tree_model_get(model, iter, ADDRESS_DATA_COLUMN_ENUM, &mcont, -1);
        deleteAddressContact(mcont, data);
        return TRUE;
    }

    return FALSE;


}

gboolean undeleteAddressRecord(GtkTreeModel *model,
                               GtkTreePath *path,
                               GtkTreeIter *iter,
                               gpointer data) {
    int *i = gtk_tree_path_get_indices(path);
    if (i[0] == rowSelected) {
        MyContact *mcont = NULL;
        gtk_tree_model_get(model, iter, ADDRESS_DATA_COLUMN_ENUM, &mcont, -1);
        undeleteAddress(mcont, data);
        return TRUE;
    }

    return FALSE;


}

void deleteAddressContact(MyContact *mcont, gpointer data) {
    int flag;
    int show_priv;
    long char_set;
    int i;

    if (mcont < (MyContact *) LIST_MIN_DATA) {
        return;
    }
    /* convert to Palm character set */
    get_pref(PREF_CHAR_SET, &char_set, NULL);
    if (char_set != CHAR_SET_LATIN1) {
        for (i = 0; i < NUM_CONTACT_ENTRIES; i++) {
            if (mcont->cont.entry[i]) {
                charset_j2p(mcont->cont.entry[i],
                            strlen(mcont->cont.entry[i]) + 1, char_set);
            }
        }
    }

    /* Do masking like Palm OS 3.5 */
    show_priv = show_privates(GET_PRIVATES);
    if ((show_priv != SHOW_PRIVATES) &&
        (mcont->attrib & dlpRecAttrSecret)) {
        return;
    }
    /* End Masking */
    flag = GPOINTER_TO_INT(data);
    if ((flag == MODIFY_FLAG) || (flag == DELETE_FLAG)) {
        if (glob_sqlite) jpsqlite_Delete(CONTACTS, mcont);
        else delete_pc_record(CONTACTS, mcont, flag);
        if (flag == DELETE_FLAG) {
            /* when we redraw we want to go to the line above the deleted one */
            if (rowSelected > 0) {
                rowSelected--;
            }
        }
    }

    if (flag == DELETE_FLAG) {
        address_redraw();
    }
}

void deleteAddress(MyContact *mcont, gpointer data) {
    MyAddress maddr;
    int flag;
    int show_priv;
    long char_set;
    int i;

    jp_logf(JP_LOG_DEBUG, "deleteAddress()\n");
    if (mcont < (MyContact *) LIST_MIN_DATA) {
        return;
    }

    copy_contact_to_address(&(mcont->cont), &(maddr.addr));
    maddr.rt = mcont->rt;
    maddr.unique_id = mcont->unique_id;
    maddr.attrib = mcont->attrib;

    /* convert to Palm character set */
    get_pref(PREF_CHAR_SET, &char_set, NULL);
    if (char_set != CHAR_SET_LATIN1) {
        for (i = 0; i < NUM_ADDRESS_FIELDS; i++) {
            if (maddr.addr.entry[i]) {
                charset_j2p(maddr.addr.entry[i],
                            strlen(maddr.addr.entry[i]) + 1, char_set);
            }
        }
    }

    /* Do masking like Palm OS 3.5 */
    show_priv = show_privates(GET_PRIVATES);
    if ((show_priv != SHOW_PRIVATES) &&
        (maddr.attrib & dlpRecAttrSecret)) {
        free_Address(&(maddr.addr));
        return;
    }
    /* End Masking */
    flag = GPOINTER_TO_INT(data);
    if ((flag == MODIFY_FLAG) || (flag == DELETE_FLAG)) {
        if (glob_sqlite) { if (flag == DELETE_FLAG) jpsqlite_Delete(ADDRESS, &maddr); }
        else delete_pc_record(ADDRESS, &maddr, flag);
        if (flag == DELETE_FLAG) {
            /* when we redraw we want to go to the line above the deleted one */
            if (rowSelected > 0) {
                rowSelected--;
            }
        }
    }

    free_Address(&(maddr.addr));

    if (flag == DELETE_FLAG) {
        address_redraw();
    }
}

static void cb_delete_address(GtkWidget *widget, gpointer data) {
    gtk_tree_model_foreach(GTK_TREE_MODEL(listStore), deleteAddressRecord, data);
}

static void cb_delete_contact(GtkWidget *widget, gpointer data) {
    gtk_tree_model_foreach(GTK_TREE_MODEL(listStore), deleteAddressContactRecord, data);
}

static void cb_delete_address_or_contact(GtkWidget *widget, gpointer data) {
    if (address_version == 0) {
        cb_delete_address(widget, data);
    } else {
        cb_delete_contact(widget, data);
    }
}

void undeleteAddress(MyContact *mcont, gpointer data) {
    int flag;
    int show_priv;

    if (mcont < (MyContact *) LIST_MIN_DATA) {
        return;
    }

    /* Do masking like Palm OS 3.5 */
    show_priv = show_privates(GET_PRIVATES);
    if ((show_priv != SHOW_PRIVATES) &&
        (mcont->attrib & dlpRecAttrSecret)) {
        return;
    }
    /* End Masking */

    jp_logf(JP_LOG_DEBUG, "mcont->unique_id = %d\n", mcont->unique_id);
    jp_logf(JP_LOG_DEBUG, "mcont->rt = %d\n", mcont->rt);

    flag = GPOINTER_TO_INT(data);
    if (flag == UNDELETE_FLAG) {
        if (mcont->rt == DELETED_PALM_REC ||
            (mcont->rt == DELETED_PC_REC)) {
            if (address_version == 0) {
                MyAddress maddr;
                maddr.unique_id = mcont->unique_id;
                undelete_pc_record(ADDRESS, &maddr, flag);
            } else {
                undelete_pc_record(CONTACTS, mcont, flag);
            }
        }
        /* Possible later addition of undelete for modified records
      else if (mcont->rt == MODIFIED_PALM_REC) {
         cb_add_new_record(widget, GINT_TO_POINTER(COPY_FLAG));
      }
      */
    }

    address_redraw();
}

static void cb_undelete_address(GtkWidget *widget,
                                gpointer data) {

    gtk_tree_model_foreach(GTK_TREE_MODEL(listStore), undeleteAddressRecord, data);

}

static void cb_cancel(GtkWidget *widget, gpointer data) {
    set_new_button_to(CLEAR_FLAG);
    address_refresh();
}

/**
 * This is a bit hacky.
 * The sort should probably be done on the column sort,
 * but since there are 3 ways to sort, I'm not positive
 * it will work well.
 * @param nameColumn
 */
static void cb_resortNameColumn(GtkTreeViewColumn *nameColumn) {
    addr_sort_order = addr_sort_order << 1;
    if (!(addr_sort_order & 0x07)) addr_sort_order = SORT_BY_LNAME;
    set_pref(PREF_ADDR_SORT_ORDER, addr_sort_order, NULL, TRUE);
    //find Id to keep selected.
    gtk_tree_model_foreach(GTK_TREE_MODEL(listStore), findAndSetGlobalAddressId, NULL);
    address_redraw();
    switch (addr_sort_order) {
        case SORT_BY_LNAME:
        default:
            addr_sort_order = SORT_BY_LNAME;  /* Initialize variable if default case taken */
            gtk_tree_view_column_set_title(nameColumn, ADDRESS_LAST_NAME_COMPANY);
            break;
        case SORT_BY_FNAME:
            gtk_tree_view_column_set_title(nameColumn, ADDRESS_FIRST_NAME_COMPANY);
            break;
        case SORT_BY_COMPANY:
            gtk_tree_view_column_set_title(nameColumn, ADDRESS_COMPANY_LAST_NAME);
            break;
    }


}


static void cb_phone_menu(GtkComboBox *item, unsigned int value) {
    if (!item)
        return;
    if (gtk_combo_box_get_active(GTK_COMBO_BOX(item)) < 0) {
        return;
    }
    int selectedIndex = gtk_combo_box_get_active(GTK_COMBO_BOX(item));
    address_phone_label_selected[value] = selectedIndex;

}

static void cb_IM_type_menu(GtkComboBox *item, unsigned int value) {
    if (!item)
        return;
    if (gtk_combo_box_get_active(GTK_COMBO_BOX(item)) < 0) {
        return;
    }
    int selectedIndex = gtk_combo_box_get_active(GTK_COMBO_BOX(item));
    IM_type_selected[value] = selectedIndex;
}

/* The least significant byte of value is the selection of the menu,
 * i.e., which item is chosen (Work, Office, Home).
 * The next to least significant byte is the address type menu
 * that is being selected (there are 3 addresses and 3 pulldown menus) */
static void cb_address_type_menu(GtkComboBox *item, unsigned int value) {
    int menu, selection;
    int address_i, i;

    if (!item)
        return;
    if (gtk_combo_box_get_active(GTK_COMBO_BOX(item)) < 0) {
        return;
    }
    int selectedIndex = gtk_combo_box_get_active(GTK_COMBO_BOX(item));
    menu = value;
    selection = selectedIndex;
    jp_logf(JP_LOG_DEBUG, "addr_type_menu = %d\n", menu);
    jp_logf(JP_LOG_DEBUG, "selection = %d\n", selection);
    address_type_selected[menu] = selection;
    /* We want to make the notebook page tab label match the type of
   * address from the menu.  So, we'll find the nth address menu
   * and set whatever page the schema says it resides on */
    address_i = 0;
    for (i = 0; i < schema_size; i++) {
        if (schema[i].type == ADDRESS_GUI_ADDR_MENU_TEXT) {
            if (address_i == menu) {
                gtk_label_set_text(GTK_LABEL(notebook_label[schema[i].notebook_page]),
                                   contact_app_info.addrLabels[selection]);
            }
            address_i++;
        }
    }
}

static void cb_notebook_changed(GtkWidget *widget,
                                GtkWidget *widget2,
                                int page,
                                gpointer data) {
    int prev_page;

    /* GTK calls this function while it is destroying the notebook
    * I use this function to tell if it is being destroyed */
    prev_page = gtk_notebook_get_current_page(GTK_NOTEBOOK(widget));
    if (prev_page < 0) return;

    jp_logf(JP_LOG_DEBUG, "cb_notebook_changed(), prev_page=%d, page=%d\n", prev_page, page);
    set_pref(PREF_ADDRESS_NOTEBOOK_PAGE, page, NULL, TRUE);
}

static void get_address_attrib(unsigned char *attrib) {
    /* Get the category that is set from the menu */
    *attrib = 0;
    if (GTK_IS_WIDGET(category_menu2)) {
        *attrib = get_selected_category_from_combo_box(GTK_COMBO_BOX(category_menu2));
    }

    /* Get private flag */
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(private_checkbox))) {
        *attrib |= dlpRecAttrSecret;
    }
}

static void cb_add_new_record(GtkWidget *widget, gpointer data) {

    if (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(listStore), NULL) != 0) {
        gtk_tree_model_foreach(GTK_TREE_MODEL(listStore), addNewAddressRecord, data);
    } else {
        //no records exist in category yet.
        addNewAddressRecordToDataStructure(NULL, data);
    }
}

static void addr_clear_details(void) {
    int i;
    int new_cat;
    int sorted_position;
    int address_i, IM_i, phone_i;
    long ivalue;
    char reminder_str[10];
    /* Palm has phone popup menus in one order and the display of phone tabs
    * in another. This reorders the tabs to produce the more usable order of
    * the Palm desktop software */
    int phone_btn_order[NUM_PHONE_ENTRIES] = {0, 1, 4, 7, 5, 3, 2};

    /* Need to disconnect signals first */
    connect_changed_signals(DISCONNECT_SIGNALS);

    /* Clear the quickview */
    gtk_text_buffer_set_text(GTK_TEXT_BUFFER(addr_all_buffer), "", -1);

    /* Clear all of the text fields */
    for (i = 0; i < schema_size; i++) {
        switch (schema[i].type) {
            case ADDRESS_GUI_LABEL_TEXT:
            case ADDRESS_GUI_DIAL_SHOW_PHONE_MENU_TEXT:
            case ADDRESS_GUI_ADDR_MENU_TEXT:
            case ADDRESS_GUI_IM_MENU_TEXT:
            case ADDRESS_GUI_WEBSITE_TEXT:
                gtk_text_buffer_set_text(GTK_TEXT_BUFFER(addr_text_buffer[schema[i].record_field]), "", -1);
            default:
                break;
        }
    }

    address_i = IM_i = phone_i = 0;
    for (i = 0; i < schema_size; i++) {
        switch (schema[i].type) {
            case ADDRESS_GUI_DIAL_SHOW_PHONE_MENU_TEXT:
                if (phone_i == 0) {
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_button[0]), TRUE);
                }
                if (address_version) {
                    gtk_combo_box_set_active(GTK_COMBO_BOX(phone_type_list_menu[phone_i]), phone_btn_order[phone_i]);
                } else {
                    gtk_combo_box_set_active(GTK_COMBO_BOX(phone_type_list_menu[phone_i]), phone_i);
                }
                phone_i++;
                break;
            case ADDRESS_GUI_IM_MENU_TEXT:
                gtk_combo_box_set_active(GTK_COMBO_BOX(IM_type_list_menu[IM_i]), IM_i);
                IM_i++;
                break;
            case ADDRESS_GUI_ADDR_MENU_TEXT:
                gtk_combo_box_set_active(GTK_COMBO_BOX(address_type_list_menu[address_i]), address_i);
                address_i++;
                break;
            case ADDRESS_GUI_WEBSITE_TEXT:
                break;
            case ADDRESS_GUI_BIRTHDAY:
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(birthday_checkbox), 0);
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(reminder_checkbox), 0);
                get_pref(PREF_TODO_DAYS_TILL_DUE, &ivalue, NULL);
                reminder_str[0] = '\0';
                g_snprintf(reminder_str, sizeof(reminder_str), "%ld", ivalue);
                gtk_entry_set_text(GTK_ENTRY(reminder_entry), reminder_str);
                break;
            default:
                break;
        }
    }

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(private_checkbox), FALSE);

    if (image) {
        gtk_widget_destroy(image);
        image = NULL;
    }
    if (contact_picture.data) {
        free(contact_picture.data);
        contact_picture.dirty = 0;
        contact_picture.length = 0;
        contact_picture.data = NULL;
    }

    if (address_category == CATEGORY_ALL) {
        new_cat = 0;
    } else {
        new_cat = address_category;
    }
    sorted_position = find_sort_cat_pos(new_cat);
    if (sorted_position < 0) {
        jp_logf(JP_LOG_WARN, _("Category is not legal\n"));
    } else {
        gtk_combo_box_set_active(GTK_COMBO_BOX(category_menu2), find_menu_cat_pos(sorted_position));
    }

    set_new_button_to(CLEAR_FLAG);

    connect_changed_signals(CONNECT_SIGNALS);
}

static void cb_address_clear(GtkWidget *widget,
                             gpointer data) {
    addr_clear_details();
    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), 0);
    connect_changed_signals(DISCONNECT_SIGNALS);
    set_new_button_to(NEW_FLAG);
    gtk_widget_grab_focus(GTK_WIDGET(addr_text[0]));
}

/* Attempt to make the best possible string out of whatever garbage we find
 * Remove illegal characters, stop at carriage return and at least 1 digit
 */
static void parse_phone_str(char *dest, char *src, int max_len) {
    int i1, i2;

    for (i1 = 0, i2 = 0; (i1 < max_len) && src[i1]; i1++) {
        if (isdigit(src[i1]) || (src[i1] == ',')
            || (src[i1] == 'A') || (src[i1] == 'B') || (src[i1] == 'C')
            || (src[i1] == 'D') || (src[i1] == '*') || (src[i1] == '#')
                ) {
            dest[i2] = src[i1];
            i2++;
        } else if (((src[i1] == '\n') || (src[i1] == '\r') ||
                    (src[i1] == 'x')) && i2) {
            break;
        }
    }
    dest[i2] = '\0';
}

static void email_contact(GtkWidget *widget, gchar *str) {
    char command[1024];
    const char *pref_command;

    get_pref(PREF_MAIL_COMMAND, NULL, &pref_command);
    if (!pref_command) {
        jp_logf(JP_LOG_DEBUG, "email command empty\n");
        return;
    }

    /* Make a system call command string */
    g_snprintf(command, sizeof(command), pref_command, str);
    command[1023] = '\0';

    jp_logf(JP_LOG_STDOUT | JP_LOG_FILE, _("executing command = [%s]\n"), command);
    if (system(command) == -1) {
        jp_logf(JP_LOG_STDOUT | JP_LOG_FILE, _("Failed to execute [%s] at %s %d\n"), command, __FILE__, __LINE__);
    }

}

static void dial_contact(GtkWidget *widget, gchar *str) {
    char *Px;
    char number[100];
    char ext[100];

    number[0] = ext[0] = '\0';

    parse_phone_str(number, str, sizeof(number));

    Px = strstr(str, "x");
    if (Px) {
        parse_phone_str(ext, Px, sizeof(ext));
    }

    dialog_dial(GTK_WINDOW(gtk_widget_get_toplevel(widget)), number, ext);
}

static void cb_dial_or_mail(GtkWidget *widget, gpointer data) {
    GtkWidget *text;
    gchar *str;
    int is_mail;
    GtkTextIter start_iter;
    GtkTextIter end_iter;
    GtkTextBuffer *text_buffer;
    text = data;

    text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));
    gtk_text_buffer_get_bounds(text_buffer, &start_iter, &end_iter);
    str = gtk_text_buffer_get_text(text_buffer, &start_iter, &end_iter, TRUE);

    if (!str) return;
    printf("[%s]\n", str);

    is_mail = GPOINTER_TO_INT( g_object_get_data(G_OBJECT(widget), "mail"));
    if (is_mail) {
        email_contact(widget, str);
    } else {
        dial_contact(widget, str);
    }

    g_free(str);
}

static void cb_address_quickfind(GtkWidget *widget,
                                 gpointer data) {
    const gchar *entry_text;
    gchar *copied_text;
    jp_logf(JP_LOG_DEBUG, "cb_address_quickfind\n");

    entry_text = gtk_entry_get_text(GTK_ENTRY(widget));
    jp_logf(JP_LOG_DEBUG, "cb_address_quickfind(): entry_text=%s\n",entry_text);
    if (!strlen(entry_text)) {
        return;
    } else {
        copied_text = g_strdup(entry_text);
    }
    gtk_tree_model_foreach(GTK_TREE_MODEL(listStore), findAddressRecordByTextAndSelect, copied_text);
    g_free(copied_text);
}

static void cb_edit_cats_contacts(GtkWidget *widget, gpointer data) {
    struct ContactAppInfo cai;
    char full_name[FILENAME_MAX];
    int num;
    size_t size;
    void *buf;
    struct pi_file *pf;
    pi_buffer_t *pi_buf;

    jp_logf(JP_LOG_DEBUG, "cb_edit_cats_contacts\n");

    get_home_file_name("ContactsDB-PAdd.pdb", full_name, sizeof(full_name));

    buf = NULL;
    memset(&cai, 0, sizeof(cai));

    /* Contacts App Info is 1152 or so */
    pi_buf = pi_buffer_new(1500);

    pf = pi_file_open(full_name);
    pi_file_get_app_info(pf, &buf, &size);

    pi_buf = pi_buffer_append(pi_buf, buf, size);

    num = jp_unpack_ContactAppInfo(&cai, pi_buf);
    if (num <= 0) {
        jp_logf(JP_LOG_WARN, _("Error reading file: %s\n"), "ContactsDB-PAdd.pdb");
        pi_buffer_free(pi_buf);
        return;
    }

    pi_file_close(pf);

    edit_cats(widget, "ContactsDB-PAdd", &(cai.category));

    size = jp_pack_ContactAppInfo(&cai, pi_buf);

    pdb_file_write_app_block("ContactsDB-PAdd", pi_buf->data, pi_buf->used);

    pi_buffer_free(pi_buf);
}

static void cb_edit_cats_address(GtkWidget *widget, gpointer data) {
    struct AddressAppInfo aai;
    char full_name[FILENAME_MAX];
    char buffer[65536];
    int num;
    size_t size;
    void *buf;
    struct pi_file *pf;

    jp_logf(JP_LOG_DEBUG, "cb_edit_cats_address\n");

	if (glob_sqlite) {
		memset(&aai, 0, sizeof(aai));
		jpsqlite_AddrCatSEL(&aai);	// read from database
		edit_cats(widget, "AddressDB", &(aai.category));	// GUI changes categories
		jpsqlite_CatDELINS("AddressDB", &(aai.category));	// write categories to database
	} else {
		get_home_file_name("AddressDB.pdb", full_name, sizeof(full_name));

		buf = NULL;
		memset(&aai, 0, sizeof(aai));

		pf = pi_file_open(full_name);
		pi_file_get_app_info(pf, &buf, &size);

		num = unpack_AddressAppInfo(&aai, buf, size);

		if (num <= 0) {
			jp_logf(JP_LOG_WARN, _("Error reading file: %s\n"), "AddressDB.pdb");
			return;
		}

		pi_file_close(pf);

		edit_cats(widget, "AddressDB", &(aai.category));

		size = pack_AddressAppInfo(&aai, (unsigned char *) buffer, sizeof(buffer));

		pdb_file_write_app_block("AddressDB", buffer, size);
	}
}

static void cb_edit_cats(GtkWidget *widget, gpointer data) {
    if (address_version) {
        cb_edit_cats_contacts(widget, data);
    } else {
        cb_edit_cats_address(widget, data);
    }

    cb_app_button(NULL, GINT_TO_POINTER(REDRAW));
}

static void cb_category(GtkComboBox *item, int selection) {
    int b;

    jp_logf(JP_LOG_DEBUG, "cb_edit_cats_address(): selection=%d\n",selection);
    if (!item) return;
    if (gtk_combo_box_get_active(GTK_COMBO_BOX(item)) < 0) {
        return;
    }
    int selectedItem = get_selected_category_from_combo_box(item);
    if (selectedItem == -1) {
        return;
    }

    if (address_category == selectedItem) { return; }

    b = dialog_save_changed_record_with_cancel(pane, record_changed);
    if (b == DIALOG_SAID_1) { /* Cancel */
        int index, index2;

        if (address_category == CATEGORY_ALL) {
            index = 0;
            index2 = 0;
        } else {
            index = find_sort_cat_pos(address_category);
            index2 = find_menu_cat_pos(index) + 1;
            index += 1;
        }

        if (index < 0) {
            jp_logf(JP_LOG_WARN, _("Category is not legal\n"));
        } else {
            gtk_combo_box_set_active(GTK_COMBO_BOX(category_menu1), index2);
        }

        return;
    }
    if (b == DIALOG_SAID_3) { /* Save */
        cb_add_new_record(NULL, GINT_TO_POINTER(record_changed));
    }

    if (selectedItem == CATEGORY_EDIT) {
        cb_edit_cats(GTK_WIDGET(item), NULL);
    } else {
        address_category = selectedItem;
    }
    rowSelected = 0;
    jp_logf(JP_LOG_DEBUG, "address_category = %d\n", address_category);
    address_update_listStore(listStore, category_menu1, &glob_contact_list,
                             address_category, TRUE);
    /* gives the focus to the search field */
    gtk_widget_grab_focus(address_quickfind_entry);

}

static void clear_mycontact(MyContact *mcont) {
    mcont->unique_id = 0;
    mcont->attrib = mcont->attrib & 0xF8;
    jp_free_Contact(&(mcont->cont));
    memset(&(mcont->cont), 0, sizeof(struct Contact));

    return;
}

static void set_button_label_to_date(GtkWidget *button, struct tm *date) {
    char birthday_str[255];
    const char *pref_date;

    birthday_str[0] = '\0';
    get_pref(PREF_SHORTDATE, NULL, &pref_date);
    strftime(birthday_str, sizeof(birthday_str), pref_date, date);
    gtk_button_set_label(GTK_BUTTON(button), birthday_str);
}

static void cb_button_birthday(GtkWidget *widget, gpointer data) {
    long fdow;
    int r;

    get_pref(PREF_FDOW, &fdow, NULL);
    r = cal_dialog(GTK_WINDOW(gtk_widget_get_toplevel(widget)),
                   _("Birthday"), fdow,
                   &(birthday.tm_mon),
                   &(birthday.tm_mday),
                   &(birthday.tm_year));
    if (r == CAL_DONE) {
        set_button_label_to_date(birthday_button, &birthday);
    }
}

static void cb_check_button_birthday(GtkWidget *widget, gpointer data) {
    time_t ltime;
    struct tm *now;

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
        gtk_widget_show(birthday_box);
        set_button_label_to_date(birthday_button, &birthday);
    } else {
        gtk_widget_hide(birthday_box);
        gtk_widget_hide(reminder_box);
        time(&ltime);
        now = localtime(&ltime);
        memcpy(&birthday, now, sizeof(struct tm));
    }
}

static void cb_check_button_reminder(GtkWidget *widget, gpointer data) {
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
        gtk_widget_show(reminder_box);
    } else {
        gtk_widget_hide(reminder_box);
    }
}

/* Photo Code */

static GtkWidget *image_from_data(void *buf, size_t size) {
    GdkPixbufLoader *loader;
    GError *error;
    GdkPixbuf *pb;
    GtkWidget *tmp_image = NULL;

    error = NULL;
    loader = gdk_pixbuf_loader_new();
    gdk_pixbuf_loader_write(loader, buf, size, &error);
    pb = gdk_pixbuf_loader_get_pixbuf(loader);
    tmp_image = g_object_ref(gtk_image_new_from_pixbuf(pb));

    if (loader) {
        gdk_pixbuf_loader_close(loader, &error);
        g_object_unref(loader);
    }

    /* Force down reference count to prevent memory leak */
    if (tmp_image) {
        g_object_unref(tmp_image);
    }

    return tmp_image;
}

typedef void (*sighandler_t)(int);

static int change_photo(char *filename) {
    FILE *in;
    char command[FILENAME_MAX + 256];
    char buf[0xFFFF];
    int total_read, count, r;
    sighandler_t old_sighandler;

    /* SIGCHLD handler installed by sync process interferes with pclose.
    * Temporarily restore SIGCHLD to its default value (null) while
    * processing command through pipe */
    old_sighandler = signal(SIGCHLD, SIG_DFL);

    sprintf(command, "convert -resize %dx%d %s jpg:-",
            PHOTO_X_SZ, PHOTO_Y_SZ, filename);

    in = popen(command, "r");

    if (!in) {
        return EXIT_FAILURE;
    }

    total_read = 0;
    while (!feof(in)) {
        count = fread(buf + total_read, 1, 0xFFFF - total_read, in);
        total_read += count;
        if ((count == 0) || (total_read >= 0xFFFF)) break;
    }
    r = pclose(in);

    if (r) {
        dialog_generic_ok(gtk_widget_get_toplevel(notebook),
                          _("External program not found, or other error"),
                          DIALOG_ERROR,
                          _("J-Pilot can not find the external program \"convert\"\nor an error occurred while executing convert.\nYou may need to install package ImageMagick"));
        jp_logf(JP_LOG_WARN, _("Command executed was \"%s\"\n"), command);
        jp_logf(JP_LOG_WARN, _("return code was %d\n"), r);
        return EXIT_FAILURE;
    }

    if (image) {
        gtk_widget_destroy(image);
        image = NULL;
    }
    if (contact_picture.data) {
        free(contact_picture.data);
        contact_picture.dirty = 0;
        contact_picture.length = 0;
        contact_picture.data = NULL;
    }
    contact_picture.data = malloc(total_read);
    memcpy(contact_picture.data, buf, total_read);
    contact_picture.length = total_read;
    contact_picture.dirty = 0;
    image = image_from_data(contact_picture.data, contact_picture.length);
    gtk_container_add(GTK_CONTAINER(picture_button), image);
    gtk_widget_show(image);

    signal(SIGCHLD, old_sighandler);

    return EXIT_SUCCESS;
}

// TODO: make a common filesel function
static void cb_photo_browse_cancel(GtkWidget *widget, gpointer data) {
    gtk_widget_destroy(widget);
}

static void cb_photo_browse_ok(GtkWidget *widget, gpointer data) {
    const char *sel;
    char **Pselection;

    sel = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER (widget));
    set_pref(PREF_CONTACTS_PHOTO_FILENAME, 0, sel, TRUE);

    Pselection = g_object_get_data(G_OBJECT(GTK_FILE_CHOOSER(widget)), "selection");
    if (Pselection) {
        jp_logf(JP_LOG_DEBUG, "setting selection to %s\n", sel);
        *Pselection = strdup(sel);
    }

    gtk_widget_destroy(widget);
}

static int browse_photo(GtkWidget *main_window) {
    GtkWidget *fileChooserWidget;
    const char *svalue;
    char dir[MAX_PREF_LEN + 2];
    int i;
    char *selection;

    get_pref(PREF_CONTACTS_PHOTO_FILENAME, NULL, &svalue);
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

    if (chdir(dir)) {
        jp_logf(JP_LOG_WARN, _("chdir() failed\n"));
    }
    selection = NULL;
    fileChooserWidget = gtk_file_chooser_dialog_new(_("Add Photo"), GTK_WINDOW(main_window), GTK_FILE_CHOOSER_ACTION_OPEN,
                                                    "Cancel", GTK_RESPONSE_CANCEL, "Open",
                                                    GTK_RESPONSE_ACCEPT, NULL);
    g_object_set_data(G_OBJECT(GTK_FILE_CHOOSER(fileChooserWidget)), "selection", &selection);
    if (gtk_dialog_run(GTK_DIALOG (fileChooserWidget)) == GTK_RESPONSE_ACCEPT) {
        cb_photo_browse_ok(fileChooserWidget, NULL);
    } else {
        cb_photo_browse_cancel(fileChooserWidget, NULL);
    }
    if (selection) {
        jp_logf(JP_LOG_DEBUG, "browse_photo(): selection = %s\n", selection);
        change_photo(selection);
        free(selection);
        return 1;
    }

    return 0;
}

static void cb_photo_menu_select(GtkWidget *item,
                                 GtkPositionType selected) {
    if (selected == 1) {
        if (0 == browse_photo(gtk_widget_get_toplevel(treeView)))
            /* change photo canceled */
            return;
    }
    if (selected == 2) {
        if (image) {
            gtk_widget_destroy(image);
            image = NULL;
        }
        if (contact_picture.data) {
            free(contact_picture.data);
            contact_picture.dirty = 0;
            contact_picture.length = 0;
            contact_picture.data = NULL;
        }
    }

    cb_record_changed(NULL, NULL);
}

static gint cb_photo_menu_popup(GtkWidget *widget, GdkEvent *event) {
    GtkMenu *menu;
    GdkEventButton *event_button;

    g_return_val_if_fail(widget != NULL, FALSE);
    g_return_val_if_fail(GTK_IS_MENU(widget), FALSE);
    g_return_val_if_fail(event != NULL, FALSE);

    if (event->type == GDK_BUTTON_PRESS) {
        event_button = (GdkEventButton *) event;
        if (event_button->button == 1) {
            menu = GTK_MENU (widget);
            // TODO verify that this change is correct
            //gtk_menu_popup(menu, NULL, NULL, NULL, NULL,
            //               event_button->button, event_button->time);
            gtk_menu_popup_at_pointer(menu, event);
            return TRUE;
        }
    }

    return FALSE;
}

/* End Photo code */

static gboolean cb_key_pressed_left_side(GtkWidget *widget,
                                         GdkEventKey *event) {
    GtkWidget *entry_widget;
    GtkTextBuffer *text_buffer;
    GtkTextIter iter;

    if (event->keyval == GDK_KEY_Return) {
        g_signal_stop_emission_by_name(G_OBJECT(widget), "key_press_event");

        if (address_version == 0) {
            switch (gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook))) {
                case 0 :
                    entry_widget = addr_text[contLastname];
                    break;
                case 1 :
                    entry_widget = addr_text[contAddress1];
                    break;
                case 2 :
                    entry_widget = addr_text[contCustom1];
                    break;
                case 3 :
                    entry_widget = addr_text[contNote];
                    break;
                default:
                    entry_widget = addr_text[0];
            }
        } else {
            switch (gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook))) {
                case 0 :
                    entry_widget = addr_text[contLastname];
                    break;
                case 1 :
                    entry_widget = addr_text[contAddress1];
                    break;
                case 2 :
                    entry_widget = addr_text[contAddress2];
                    break;
                case 3 :
                    entry_widget = addr_text[contAddress3];
                    break;
                case 4 :
                    entry_widget = addr_text[contCustom1];
                    break;
                case 5 :
                    entry_widget = addr_text[contNote];
                    break;
                default:
                    entry_widget = addr_text[0];
            }
        }

        gtk_widget_grab_focus(entry_widget);
        /* Position cursor at start of text */
        text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(entry_widget));
        gtk_text_buffer_get_start_iter(text_buffer, &iter);
        gtk_text_buffer_place_cursor(text_buffer, &iter);

        return TRUE;
    }

    return FALSE;
}

static gboolean cb_key_pressed_right_side(GtkWidget *widget,
                                          GdkEventKey *event,
                                          gpointer data) {
    if ((event->keyval == GDK_KEY_Return) && (event->state & GDK_SHIFT_MASK)) {
        g_signal_stop_emission_by_name(G_OBJECT(widget), "key_press_event");
        gtk_widget_grab_focus(GTK_WIDGET(treeView));
        return TRUE;
    }
    /* Call external editor for note text */
    if (data != NULL &&
        (event->keyval == GDK_KEY_e) && (event->state & GDK_CONTROL_MASK)) {
        g_signal_stop_emission_by_name(G_OBJECT(widget), "key_press_event");

        /* Get current text and place in temporary file */
        GtkTextIter start_iter;
        GtkTextIter end_iter;
        char *text_out;
        GObject *note_buffer = addr_text_buffer[schema[GPOINTER_TO_INT(data)].record_field];

        gtk_text_buffer_get_bounds(GTK_TEXT_BUFFER(note_buffer),
                                   &start_iter, &end_iter);
        text_out = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(note_buffer),
                                            &start_iter, &end_iter, TRUE);


        char tmp_fname[] = "jpilot.XXXXXX";
        int tmpfd = mkstemp(tmp_fname);
        if (tmpfd < 0) {
            jp_logf(JP_LOG_WARN, _("Could not get temporary file name\n"));
            if (text_out)
                free(text_out);
            return TRUE;
        }

        FILE *fptr = fdopen(tmpfd, "w");
        if (!fptr) {
            jp_logf(JP_LOG_WARN, _("Could not open temporary file for external editor\n"));
            if (text_out)
                free(text_out);
            return TRUE;
        }
        fwrite(text_out, strlen(text_out), 1, fptr);
        fwrite("\n", 1, 1, fptr);
        fclose(fptr);

        /* Call external editor */
        char command[1024];
        const char *ext_editor;

        get_pref(PREF_EXTERNAL_EDITOR, NULL, &ext_editor);
        if (!ext_editor) {
            jp_logf(JP_LOG_INFO, "External Editor command empty\n");
            if (text_out)
                free(text_out);
            return TRUE;
        }

        if ((strlen(ext_editor) + strlen(tmp_fname) + 1) > sizeof(command)) {
            if (text_out)
                free(text_out);
            return TRUE;
        }
        g_snprintf(command, sizeof(command), "%s %s", ext_editor, tmp_fname);

        /* jp_logf(JP_LOG_STDOUT|JP_LOG_FILE, _("executing command = [%s]\n"), command); */
        if (system(command) == -1) {
            /* Read data back from temporary file into memo */
            char text_in[0xFFFF];
            size_t bytes_read;

            fptr = fopen(tmp_fname, "rb");
            if (!fptr) {
                jp_logf(JP_LOG_WARN, _("Could not open temporary file from external editor\n"));
                return TRUE;
            }
            bytes_read = fread(text_in, 1, 0xFFFF, fptr);
            fclose(fptr);
            unlink(tmp_fname);

            text_in[--bytes_read] = '\0';  /* Strip final newline */
            /* Only update text if it has changed */
            if (strcmp(text_out, text_in)) {
                gtk_text_buffer_set_text(GTK_TEXT_BUFFER(note_buffer), text_in, -1);
            }
        }

        if (text_out)
            free(text_out);

        return TRUE;
    }   /* End of external editor if */

    return FALSE;
}

gboolean
findAddressRecordByTextAndSelect(GtkTreeModel *model,
                                 GtkTreePath *path,
                                 GtkTreeIter *iter,
                                 gpointer data) {
    int *i = gtk_tree_path_get_indices(path);
    char *list_text;
    char *entry_text = data;

    gtk_tree_model_get(model, iter, ADDRESS_NAME_COLUMN_ENUM, &list_text, -1);
    int result = strncasecmp(list_text, entry_text, strlen(entry_text));
    if (!result) {
        GtkTreeSelection *selection = NULL;
        selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeView));
        gtk_tree_selection_set_select_function(selection, handleRowSelectionForAddress, NULL, NULL);
        gtk_tree_selection_select_path(selection, path);
        gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(treeView), path, ADDRESS_NAME_COLUMN_ENUM, FALSE, 1.0, 0.0);
        rowSelected = i[0];
        return TRUE;
    }
    return FALSE;
}

gboolean
findAddressRecordAndSelect(GtkTreeModel *model,
                           GtkTreePath *path,
                           GtkTreeIter *iter,
                           gpointer data) {

    if (glob_find_id) {
        MyContact *maddr = NULL;

        gtk_tree_model_get(model, iter, ADDRESS_DATA_COLUMN_ENUM, &maddr, -1);
        if (maddr->unique_id == glob_find_id) {
            GtkTreeSelection *selection = NULL;
            selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeView));
            gtk_tree_selection_select_path(selection, path);
            gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(treeView), path, (GtkTreeViewColumn *)ADDRESS_PHONE_COLUMN_ENUM, FALSE, 1.0, 0.0);
            glob_find_id = 0;
            return TRUE;
        }
    }
    return FALSE;
}

gboolean
findAndSetGlobalAddressId(GtkTreeModel *model,
                          GtkTreePath *path,
                          GtkTreeIter *iter,
                          gpointer data) {
    int *i = gtk_tree_path_get_indices(path);
    if (i[0] == rowSelected) {
        MyContact *maddr = NULL;

        gtk_tree_model_get(model, iter, ADDRESS_DATA_COLUMN_ENUM, &maddr, -1);
        if (maddr != NULL) {
            glob_find_id = maddr->unique_id;
        } else {
            glob_find_id = 0;
        }
        return TRUE;
    }

    return FALSE;
}

gboolean
selectRecordAddressByRow(GtkTreeModel *model,
                         GtkTreePath *path,
                         GtkTreeIter *iter,
                         gpointer data) {
    int *i = gtk_tree_path_get_indices(path);
    jp_logf(JP_LOG_DEBUG, "selectRecordAddressByRow(): i[0]=%d, rowSelected=%d\n",i[0],rowSelected);
    if (i[0] == rowSelected) {
        GtkTreeSelection *selection = NULL;
        selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeView));
        gtk_tree_selection_select_path(selection, path);
        gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(treeView), path, NULL /*(GtkTreeViewColumn *)ADDRESS_PHONE_COLUMN_ENUM*/, FALSE, 1.0, 0.0);
        return TRUE;
    }

    return FALSE;
}

static void address_update_listStore(GtkListStore *pListStore, GtkWidget *tooltip_widget,
                                     ContactList **cont_list, int category,
                                     int main) {
    GtkTreeIter iter;
    int num_entries, entries_shown;
    int show1, show2, show3;
    GdkPixbuf *pixbuf_note;
    GdkPixbuf *noteColumnDisplay;
    ContactList *temp_cl;
    char str[ADDRESS_MAX_COLUMN_LEN + 2];
    char phone[ADDRESS_MAX_COLUMN_LEN + 2];
    char name[ADDRESS_MAX_COLUMN_LEN + 2];
    int show_priv;
    long use_jos, char_set, show_tooltips;
    char *tmp_p1, *tmp_p2, *tmp_p3;
    char blank[] = "";
    char slash[] = " / ";
    char comma_space[] = ", ";
    char *field1, *field2, *field3;
    char *delim1, *delim2;
    char *tmp_delim1, *tmp_delim2;
    AddressList *addr_list;

	jp_logf(JP_LOG_DEBUG, "address_update_listStore(): category=%d, main=%d\n",category,main);
    free_ContactList(cont_list);

    if (address_version == 0) {
        addr_list = NULL;
        num_entries = glob_sqlite ? jpsqlite_AddrSEL(&addr_list,addr_sort_order,1,CATEGORY_ALL) : get_addresses2(&addr_list, SORT_ASCENDING, 2, 2, 1, CATEGORY_ALL);
        copy_addresses_to_contacts(addr_list, cont_list);
        free_AddressList(&addr_list);
    } else {
        /* Need to get all records including private ones for the tooltips calculation */
        num_entries = get_contacts2(cont_list, SORT_ASCENDING, 2, 2, 1, CATEGORY_ALL);
    }

    /* Start by clearing existing entry if in main window */
    if (main) {
        addr_clear_details();
        gtk_text_buffer_set_text(GTK_TEXT_BUFFER(addr_all_buffer), "", -1);
    }
    GtkTreeSelection* treeSelection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeView));
    gtk_tree_selection_set_select_function(treeSelection, NULL, NULL, NULL);
    gtk_list_store_clear(GTK_LIST_STORE(pListStore));
    /* Collect preferences and pixmaps before loop */
    get_pref(PREF_CHAR_SET, &char_set, NULL);
    get_pref(PREF_USE_JOS, &use_jos, NULL);
    show_priv = show_privates(GET_PRIVATES);
    get_pixbufs(PIXMAP_NOTE, &pixbuf_note);


    switch (addr_sort_order) {
        case SORT_BY_LNAME:
        default:
            show1 = contLastname;
            show2 = contFirstname;
            show3 = contCompany;
            delim1 = comma_space;
            delim2 = slash;
            break;
        case SORT_BY_FNAME:
            show1 = contFirstname;
            show2 = contLastname;
            show3 = contCompany;
            delim1 = comma_space;
            delim2 = slash;
            break;
        case SORT_BY_COMPANY:
            show1 = contCompany;
            show2 = contLastname;
            show3 = contFirstname;
            delim1 = slash;
            delim2 = comma_space;
            break;
    }

    entries_shown = 0;

    for (temp_cl = *cont_list; temp_cl; temp_cl = temp_cl->next) {
        if (((temp_cl->mcont.attrib & 0x0F) != category) &&
            category != CATEGORY_ALL) {
            continue;
        }

        /* Do masking like Palm OS 3.5 */
        if ((show_priv == MASK_PRIVATES) &&
            (temp_cl->mcont.attrib & dlpRecAttrSecret)) {
            gtk_list_store_append(pListStore, &iter);
            clear_mycontact(&temp_cl->mcont);
            gtk_list_store_set(pListStore, &iter,
                               ADDRESS_NAME_COLUMN_ENUM, "---------------",
                               ADDRESS_PHONE_COLUMN_ENUM, "---------------",
                               ADDRESS_DATA_COLUMN_ENUM, &temp_cl->mcont,
                               -1);
            entries_shown++;
            continue;
        }
        /* End Masking */

        /* Hide the private records if need be */
        if ((show_priv != SHOW_PRIVATES) &&
            (temp_cl->mcont.attrib & dlpRecAttrSecret)) {
            continue;
        }

        if (!use_jos && (char_set == CHAR_SET_JAPANESE || char_set == CHAR_SET_SJIS_UTF)) {
            str[0] = '\0';
            if (temp_cl->mcont.cont.entry[show1] || temp_cl->mcont.cont.entry[show2]) {
                if (temp_cl->mcont.cont.entry[show1] && temp_cl->mcont.cont.entry[show2]) {
                    if ((tmp_p1 = strchr(temp_cl->mcont.cont.entry[show1], '\1'))) *tmp_p1 = '\0';
                    if ((tmp_p2 = strchr(temp_cl->mcont.cont.entry[show2], '\1'))) *tmp_p2 = '\0';
                    g_snprintf(str, ADDRESS_MAX_LIST_NAME, "%s, %s", temp_cl->mcont.cont.entry[show1],
                               temp_cl->mcont.cont.entry[show2]);
                    if (tmp_p1) *tmp_p1 = '\1';
                    if (tmp_p2) *tmp_p2 = '\1';
                }
                if (temp_cl->mcont.cont.entry[show1] && !temp_cl->mcont.cont.entry[show2]) {
                    if ((tmp_p1 = strchr(temp_cl->mcont.cont.entry[show1], '\1'))) *tmp_p1 = '\0';
                    if (temp_cl->mcont.cont.entry[show3]) {
                        if ((tmp_p3 = strchr(temp_cl->mcont.cont.entry[show3], '\1'))) *tmp_p3 = '\0';
                        g_snprintf(str, ADDRESS_MAX_LIST_NAME, "%s, %s", temp_cl->mcont.cont.entry[show1],
                                   temp_cl->mcont.cont.entry[show3]);
                        if (tmp_p3) *tmp_p3 = '\1';
                    } else {
                        multibyte_safe_strncpy(str, temp_cl->mcont.cont.entry[show1], ADDRESS_MAX_LIST_NAME);
                    }
                    if (tmp_p1) *tmp_p1 = '\1';
                }
                if (!temp_cl->mcont.cont.entry[show1] && temp_cl->mcont.cont.entry[show2]) {
                    if ((tmp_p2 = strchr(temp_cl->mcont.cont.entry[show2], '\1'))) *tmp_p2 = '\0';
                    multibyte_safe_strncpy(str, temp_cl->mcont.cont.entry[show2], ADDRESS_MAX_LIST_NAME);
                    if (tmp_p2) *tmp_p2 = '\1';
                }
            } else if (temp_cl->mcont.cont.entry[show3]) {
                if ((tmp_p3 = strchr(temp_cl->mcont.cont.entry[show3], '\1'))) *tmp_p3 = '\0';
                multibyte_safe_strncpy(str, temp_cl->mcont.cont.entry[show3], ADDRESS_MAX_LIST_NAME);
                if (tmp_p3) *tmp_p3 = '\1';
            } else {
                strcpy(str, _("-Unnamed-"));
            }
        } else {
            str[0] = '\0';
            field1 = field2 = field3 = blank;
            tmp_delim1 = delim1;
            tmp_delim2 = delim2;
            if (temp_cl->mcont.cont.entry[show1]) field1 = temp_cl->mcont.cont.entry[show1];
            if (temp_cl->mcont.cont.entry[show2]) field2 = temp_cl->mcont.cont.entry[show2];
            if (temp_cl->mcont.cont.entry[show3]) field3 = temp_cl->mcont.cont.entry[show3];
            switch (addr_sort_order) {
                case SORT_BY_LNAME:
                default:
                    if ((!field1[0]) || (!field2[0])) tmp_delim1 = blank;
                    if (!(field3[0])) tmp_delim2 = blank;
                    if ((!field1[0]) && (!field2[0])) tmp_delim2 = blank;
                    break;
                case SORT_BY_FNAME:
                    if ((!field1[0]) || (!field2[0])) tmp_delim1 = blank;
                    if (!(field3[0])) tmp_delim2 = blank;
                    if ((!field1[0]) && (!field2[0])) tmp_delim2 = blank;
                    break;
                case SORT_BY_COMPANY:
                    if (!(field1[0])) tmp_delim1 = blank;
                    if ((!field2[0]) || (!field3[0])) tmp_delim2 = blank;
                    if ((!field2[0]) && (!field3[0])) tmp_delim1 = blank;
                    break;
            }
            g_snprintf(str, ADDRESS_MAX_COLUMN_LEN, "%s%s%s%s%s",
                       field1, tmp_delim1, field2, tmp_delim2, field3);
            if (strlen(str) < 1) strcpy(str, _("-Unnamed-"));
            str[ADDRESS_MAX_COLUMN_LEN] = '\0';


        }

        lstrncpy_remove_cr_lfs(name, str, ADDRESS_MAX_COLUMN_LEN);
        phone[0] = '\0';
        lstrncpy_remove_cr_lfs(phone, temp_cl->mcont.cont.entry[temp_cl->mcont.cont.showPhone + 4],
                               ADDRESS_MAX_COLUMN_LEN);
        GdkRGBA bgColor;
        gboolean showBgColor = FALSE;
        GdkRGBA fgColor;
        gboolean showFgColor = FALSE;
        /* Highlight row background depending on status */
        switch (temp_cl->mcont.rt) {
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
                if (temp_cl->mcont.attrib & dlpRecAttrSecret) {
                    bgColor = get_color(LIST_PRIVATE_RED, LIST_PRIVATE_GREEN, LIST_PRIVATE_BLUE);
                    showBgColor = TRUE;
                } else {
                    showBgColor = FALSE;
                }
        }

        /* Put a note pixmap up */
        if (temp_cl->mcont.cont.entry[contNote]) {
            noteColumnDisplay = pixbuf_note;
        } else {
            noteColumnDisplay = NULL;
        }
        gtk_list_store_append(pListStore, &iter);
        gtk_list_store_set(pListStore, &iter, ADDRESS_NAME_COLUMN_ENUM, name,
                           ADDRESS_NOTE_COLUMN_ENUM, noteColumnDisplay,
                           ADDRESS_PHONE_COLUMN_ENUM, phone,
                           ADDRESS_DATA_COLUMN_ENUM, &(temp_cl->mcont),
                           ADDRESS_BACKGROUND_COLOR_ENUM, showBgColor ? &bgColor : NULL,
                           ADDRESS_BACKGROUND_COLOR_ENABLED_ENUM, showBgColor,
                           ADDRESS_FOREGROUND_COLOR_ENUM, showFgColor ? gdk_rgba_to_string(&fgColor) : NULL,
                           ADDRESSS_FOREGROUND_COLOR_ENABLED_ENUM, showFgColor, -1);
        entries_shown++;
    }
	jp_logf(JP_LOG_DEBUG, "address_update_listStore(): #1 num_entries=%d, entries_shown=%d, glob_find_id=%d, rowSelected=%d, name=%s, phone=%s\n",num_entries,entries_shown,glob_find_id,rowSelected,name,phone);

    // Set callback for a row selected
    gtk_tree_selection_set_select_function(treeSelection, handleRowSelectionForAddress, NULL, NULL);
	jp_logf(JP_LOG_DEBUG, "address_update_listStore(): #2 after gtk_tree_selection_set_select_function()\n");

    /* If there are items in the list, highlight the selected row */
    if ((main) && (entries_shown > 0)) {
        /* First, select any record being searched for */
        if (glob_find_id) {
            address_find();
        }
        /* Second, try the currently selected row */
        else if (rowSelected < entries_shown) {
            gtk_tree_model_foreach(GTK_TREE_MODEL(pListStore), selectRecordAddressByRow, NULL);
        } else
            /* Third, select row 0 if nothing else is possible */
        {
            rowSelected = 0;
            gtk_tree_model_foreach(GTK_TREE_MODEL(pListStore), selectRecordAddressByRow, NULL);
        }
    }
	jp_logf(JP_LOG_DEBUG, "address_update_listStore(): #3 num_entries=%d, entries_shown=%d, glob_find_id=%d, rowSelected=%d\n",num_entries,entries_shown,glob_find_id,rowSelected);

    if (tooltip_widget) {
        get_pref(PREF_SHOW_TOOLTIPS, &show_tooltips, NULL);
        if (cont_list == NULL) {
            set_tooltip(show_tooltips, category_menu1, _("0 records"));
        } else {
            sprintf(str, _("%d of %d records"), entries_shown, num_entries);
            set_tooltip(show_tooltips, category_menu1, str);
        }
    }
    /* return focus to treeView after any big operation which requires a redraw */
    gtk_widget_grab_focus(GTK_WIDGET(treeView));
	jp_logf(JP_LOG_DEBUG, "address_update_listStore(): num_entries=%d, entries_shown=%d\n",num_entries,entries_shown);
}

/* default set is which menu item is to be set on by default */
/* set is which set in the phone_type_menu_item array to use */
static int make_IM_type_menu(int default_set, unsigned int callback_id, int set) {
    int i;

    IM_type_list_menu[set] = gtk_combo_box_text_new();

    for (i = 0; i < NUM_IM_LABELS; i++) {
        if (contact_app_info.IMLabels[i][0]) {
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT (IM_type_list_menu[set]), contact_app_info.IMLabels[i]);
            changed_list = g_list_prepend(changed_list, IM_type_list_menu[set]);
        }
    }
    g_signal_connect(G_OBJECT(IM_type_list_menu[set]), "changed", G_CALLBACK(cb_IM_type_menu),
                     GINT_TO_POINTER(set));

    gtk_combo_box_set_active(GTK_COMBO_BOX(IM_type_list_menu[set]), default_set);

    return EXIT_SUCCESS;
}


// TODO: rewrite this crappy function
/* default set is which menu item is to be set on by default */
/* set is which set in the menu_item array to use */
static int make_address_type_menu(int default_set, int set) {
    int i;

    address_type_list_menu[set] = gtk_combo_box_text_new();
    for (i = 0; i < NUM_ADDRESSES; i++) {
        if (contact_app_info.addrLabels[i][0]) {
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT (address_type_list_menu[set]),
                                           contact_app_info.addrLabels[i]);
            changed_list = g_list_prepend(changed_list, address_type_list_menu[set]);
        }
    }
    g_signal_connect(G_OBJECT(address_type_list_menu[set]), "changed", G_CALLBACK(cb_address_type_menu),
                     GINT_TO_POINTER(set));

    gtk_combo_box_set_active(GTK_COMBO_BOX(address_type_list_menu[set]), default_set);
    return EXIT_SUCCESS;
}

/* default set is which menu item is to be set on by default */
/* set is which set in the phone_type_menu_item array to use */
static int make_phone_menu(int default_set, unsigned int callback_id, int set) {
    int i;
    char *utf;
    long char_set;

    jp_logf(JP_LOG_DEBUG, "make_phone_menu(): default_set=%d, callback_id=%d, set=%d\n",default_set,callback_id,set);
    get_pref(PREF_CHAR_SET, &char_set, NULL);

    phone_type_list_menu[set] = gtk_combo_box_text_new();

    for (i = 0; i < NUM_PHONE_LABELS; i++) {
        utf = charset_p2newj(contact_app_info.phoneLabels[i], 16, char_set);
        if (contact_app_info.phoneLabels[i][0]) {
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT (phone_type_list_menu[set]), utf);
            changed_list = g_list_prepend(changed_list, phone_type_list_menu[set]);
        }
        g_free(utf);
    }
    g_signal_connect(G_OBJECT(phone_type_list_menu[set]), "changed", G_CALLBACK(cb_phone_menu),
                     GINT_TO_POINTER(set));

    /* Set this one to active */
    gtk_combo_box_set_active(GTK_COMBO_BOX(phone_type_list_menu[set]), default_set);

    return EXIT_SUCCESS;
}


/* returns 1 if found, 0 if not */
static int address_find(void) {
    jp_logf(JP_LOG_DEBUG, "address_find()\n");
    gtk_tree_model_foreach(GTK_TREE_MODEL(listStore), findAddressRecordAndSelect, NULL);
    return EXIT_SUCCESS;
}

static int address_redraw(void) {
    address_update_listStore(listStore, category_menu1, &glob_contact_list,
                             address_category, TRUE);
    return EXIT_SUCCESS;
}

int address_cycle_cat(void) {
    int b;
    int i, new_cat;

    jp_logf(JP_LOG_DEBUG, "address_cycle_cat()\n");
    b = dialog_save_changed_record(pane, record_changed);
    if (b == DIALOG_SAID_2) {
        cb_add_new_record(NULL, GINT_TO_POINTER(record_changed));
    }

    if (address_category == CATEGORY_ALL) {
        new_cat = -1;
    } else {
        new_cat = find_sort_cat_pos(address_category);
    }

    for (i = 0; i < NUM_ADDRESS_CAT_ITEMS; i++) {
        new_cat++;
        if (new_cat >= NUM_ADDRESS_CAT_ITEMS) {
            address_category = CATEGORY_ALL;
            break;
        }
        if ((sort_l[new_cat].Pcat) && (sort_l[new_cat].Pcat[0])) {
            address_category = sort_l[new_cat].cat_num;
            break;
        }
    }

    rowSelected = 0;

    return EXIT_SUCCESS;
}

int address_refresh(void) {
    int index, index2;

    jp_logf(JP_LOG_DEBUG, "address_refresh()\n");
    if (glob_find_id) {
        address_category = CATEGORY_ALL;
    }
    if (address_category == CATEGORY_ALL) {
        index = 0;
        index2 = 0;
    } else {
        index = find_sort_cat_pos(address_category);
        index2 = find_menu_cat_pos(index) + 1;
        index += 1;
    }
    address_update_listStore(listStore, category_menu1, &glob_contact_list,
                             address_category, TRUE);
    if (index < 0) {
        jp_logf(JP_LOG_WARN, _("Category is not legal\n"));
    } else {
        //gtk_check_menu_item_set_active
        //   (GTK_CHECK_MENU_ITEM(address_cat_menu_item1[index]), TRUE);
        // gtk_option_menu_set_history(GTK_OPTION_MENU(category_menu1), index2);
        gtk_combo_box_set_active(GTK_COMBO_BOX(category_menu1), index2);
    }

    /* gives the focus to the search field */
    gtk_widget_grab_focus(address_quickfind_entry);

    return EXIT_SUCCESS;
}


static gboolean
cb_key_pressed_quickfind(GtkWidget *widget, GdkEventKey *event, gpointer data) {
    int row_count;
    int select_row;

	if (event->keyval == GDK_KEY_KP_Down || event->keyval == GDK_KEY_Down) {
		select_row = rowSelected + 1;
	} else if (event->keyval == GDK_KEY_KP_Up || event->keyval == GDK_KEY_Up) {
		select_row = rowSelected - 1;
    } else if (event->keyval == GDK_KEY_KP_Page_Up || event->keyval == GDK_KEY_Page_Up) {
		select_row = rowSelected - 20;
	} else if (event->keyval == GDK_KEY_KP_Page_Down || event->keyval == GDK_KEY_Page_Down) {
		select_row = rowSelected + 20;
	} else if ((event->keyval == GDK_KEY_KP_Begin || event->keyval == GDK_KEY_Begin) && event->state == GDK_MOD2_MASK) {	// this key already bound in edit-field
		select_row = 0;
	} else if (event->keyval == GDK_KEY_KP_End || event->keyval == GDK_KEY_End) {
		select_row = -1;	// trigger to later set to max-1
	} else {
		return FALSE;	// nothing to do
	}

    row_count = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(listStore), NULL);
    if (row_count <= 0) return FALSE;

    g_signal_stop_emission_by_name(G_OBJECT(widget), "key_press_event");

    if (select_row > row_count - 1) {
        select_row = 0;	// wrap around from top to bottom
    }
    if (select_row < 0) {
        select_row = row_count - 1;	// wrap around from bottom to top
    }
    rowSelected = select_row;
    gtk_tree_model_foreach(GTK_TREE_MODEL(listStore), selectRecordAddressByRow, NULL);
    return TRUE;
}

static gboolean cb_key_pressed(GtkWidget *widget, GdkEventKey *event) {
    GtkTextIter cursor_pos_iter;
    GtkTextBuffer *text_buffer;
    int page, next;
    int i, j, found, break_loop;

    if ((event->keyval != GDK_KEY_Tab) &&
        (event->keyval != GDK_KEY_ISO_Left_Tab)) {
        return FALSE;
    }

    if (event->keyval == GDK_KEY_Tab) {
        /* See if they are at the end of the text */
        text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(widget));
        gtk_text_buffer_get_iter_at_mark(text_buffer, &cursor_pos_iter, gtk_text_buffer_get_insert(text_buffer));
        if (!(gtk_text_iter_is_end(&cursor_pos_iter))) {
            return FALSE;
        }
    }
    g_signal_stop_emission_by_name(G_OBJECT(widget), "key_press_event");

    /* Initialize page and next widget in case search fails */
    page = schema[0].notebook_page;
    next = schema[0].record_field;

    found = break_loop = 0;
    for (i = j = 0; i < schema_size && !break_loop; i++) {
        switch (schema[i].type) {
            case ADDRESS_GUI_LABEL_TEXT:
            case ADDRESS_GUI_DIAL_SHOW_PHONE_MENU_TEXT:
            case ADDRESS_GUI_ADDR_MENU_TEXT:
            case ADDRESS_GUI_IM_MENU_TEXT:
            case ADDRESS_GUI_WEBSITE_TEXT:
                if (found) {
                    page = schema[i].notebook_page;
                    next = schema[i].record_field;
                    break_loop = 1;
                    break;
                }
                if (addr_text[schema[i].record_field] == widget) {
                    found = 1;
                } else {
                    j = i;
                }
                break;
            default:
                break;
        }
    }

    if (event->keyval == GDK_KEY_ISO_Left_Tab) {
        j = (j < 0 ? 0 : j);
        page = schema[j].notebook_page;
        next = schema[j].record_field;
    }

    gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), page);
    gtk_widget_grab_focus(GTK_WIDGET(addr_text[next]));

    return TRUE;
}

int address_gui_cleanup(void) {
    int b;

    jp_logf(JP_LOG_DEBUG, "address_gui_cleanup()\n");
    b = dialog_save_changed_record(pane, record_changed);
    if (b == DIALOG_SAID_2) {
        cb_add_new_record(NULL, GINT_TO_POINTER(record_changed));
    }

    g_list_free(changed_list);
    changed_list = NULL;

    free_ContactList(&glob_contact_list);
    free_ContactList(&export_contact_list);
    connect_changed_signals(DISCONNECT_SIGNALS);
    set_pref(PREF_ADDRESS_PANE, gtk_paned_get_position(GTK_PANED(pane)), NULL, TRUE);
    set_pref(PREF_LAST_ADDR_CATEGORY, address_category, NULL, TRUE);
    GtkTreeSelection* treeSelection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeView));
    gtk_tree_selection_set_select_function(treeSelection, NULL, NULL, NULL);
    gtk_list_store_clear(GTK_LIST_STORE(listStore));
    if (image) {
        gtk_widget_destroy(image);
        image = NULL;
    }
    if (contact_picture.data) {
        free(contact_picture.data);
    }
    contact_picture.dirty = 0;
    contact_picture.length = 0;
    contact_picture.data = NULL;

    return EXIT_SUCCESS;
}


static gboolean handleRowSelectionForAddress(GtkTreeSelection *selection,
                                             GtkTreeModel *model,
                                             GtkTreePath *path,
                                             gboolean path_currently_selected,
                                             gpointer userdata) {
    GtkTreeIter iter;
    /* The rename-able phone entries are indexes 3,4,5,6,7 */
    struct Contact *cont;
    MyContact *mcont;
    int b;
    int i, index, sorted_position;
    unsigned int unique_id = 0;
    char *list_text;
    const char *entry_text;
    int address_i, IM_i, phone_i;
    char birthday_str[255];
    long ivalue;
    char reminder_str[10];
    GString *s;
    long char_set;

    jp_logf(JP_LOG_DEBUG, "handleRowSelectionForAddress(): path_currently_selected=%d\n",path_currently_selected);
    if ((gtk_tree_model_get_iter(model, &iter, path)) && (!path_currently_selected)) {

        int *path_index = gtk_tree_path_get_indices(path);
        rowSelected = path_index[0];
        get_pref(PREF_CHAR_SET, &char_set, NULL);

        gtk_tree_model_get(model, &iter, ADDRESS_DATA_COLUMN_ENUM, &mcont, -1);

        if ((record_changed == MODIFY_FLAG) || (record_changed == NEW_FLAG)) {

            if (mcont != NULL) {
                unique_id = mcont->unique_id;
            }

            // We need to turn this "scroll with mouse held down" thing off
            button_set_for_motion(0);
            b = dialog_save_changed_record_with_cancel(pane, record_changed);
            if (b == DIALOG_SAID_1) { /* Cancel */
                // https://developer.gnome.org/gtk3/stable/GtkTreeSelection.html#gtk-tree-selection-set-select-function
                // return false is the node selected should not be changed
                return FALSE;
            }
            if (b == DIALOG_SAID_2) { /* No */
                set_new_button_to(CLEAR_FLAG);
                return TRUE;
            }
            if (b == DIALOG_SAID_3) { /* Save */
                cb_add_new_record(NULL, GINT_TO_POINTER(record_changed));
            }

            set_new_button_to(CLEAR_FLAG);

            if (unique_id) {
                glob_find_id = unique_id;
                address_find();
            }
            return TRUE;
        }

        if (mcont == NULL) {
            return TRUE;
        }

        if (mcont->rt == DELETED_PALM_REC ||
            (mcont->rt == DELETED_PC_REC))
            /* Possible later addition of undelete code for modified deleted records
             || mcont->rt == MODIFIED_PALM_REC
          */
        {
            set_new_button_to(UNDELETE_FLAG);
        } else {
            set_new_button_to(CLEAR_FLAG);
        }

        connect_changed_signals(DISCONNECT_SIGNALS);

        if (mcont->cont.picture && mcont->cont.picture->data) {
            if (contact_picture.data) {
                free(contact_picture.data);
            }
            /* Set global variables to keep the picture data */
            contact_picture.data = malloc(mcont->cont.picture->length);
            memcpy(contact_picture.data, mcont->cont.picture->data, mcont->cont.picture->length);
            contact_picture.length = mcont->cont.picture->length;
            contact_picture.dirty = 0;
            if (image) {
                gtk_widget_destroy(image);
            }
            image = image_from_data(contact_picture.data, contact_picture.length);
            gtk_container_add(GTK_CONTAINER(picture_button), image);
            gtk_widget_show(image);
        } else {
            if (image) {
                gtk_widget_destroy(image);
                image = NULL;
            }
            if (contact_picture.data) {
                free(contact_picture.data);
                contact_picture.dirty = 0;
                contact_picture.length = 0;
                contact_picture.data = NULL;
            }
        }

        cont = &(mcont->cont);
        list_text = NULL;
        gtk_tree_model_get(model, &iter, ADDRESS_NAME_COLUMN_ENUM, &list_text, -1);
        entry_text = gtk_entry_get_text(GTK_ENTRY(address_quickfind_entry));
        if (strncasecmp(list_text, entry_text, strlen(entry_text))) {
            gtk_entry_set_text(GTK_ENTRY(address_quickfind_entry), "");
        }

        /* category menu */
        index = mcont->attrib & 0x0F;
        sorted_position = find_sort_cat_pos(index);
        int pos = findSortedPostion(sorted_position, GTK_COMBO_BOX(category_menu2));
        if (pos != sorted_position && index != 0) {
            /* Illegal category, Assume that category 0 is Unfiled and valid */
            jp_logf(JP_LOG_WARN, _("Category is not legal\n"));
            index = 0;
            sorted_position = find_sort_cat_pos(index);
        }

        if (sorted_position < 0) {
            jp_logf(JP_LOG_WARN, _("Category is not legal\n"));
        } else {
            gtk_combo_box_set_active(GTK_COMBO_BOX(category_menu2), find_menu_cat_pos(sorted_position));
        }
        /* End category menu */

        /* Freeze the "All" text buffer to prevent flicker while updating */
        gtk_widget_freeze_child_notify(addr_all);

        gtk_text_buffer_set_text(GTK_TEXT_BUFFER(addr_all_buffer), "", -1);

        /* Fill out the "All" text buffer */
        s = contact_to_gstring(cont);
        if (s->len) {
            gtk_text_buffer_insert_at_cursor(GTK_TEXT_BUFFER(addr_all_buffer), _("Category: "), -1);
            char *utf;
            utf = charset_p2newj(contact_app_info.category.name[mcont->attrib & 0x0F], 16, char_set);
            gtk_text_buffer_insert_at_cursor(GTK_TEXT_BUFFER(addr_all_buffer), utf, -1);
            g_free(utf);
            gtk_text_buffer_insert_at_cursor(GTK_TEXT_BUFFER(addr_all_buffer), "\n", -1);

            gtk_text_buffer_insert_at_cursor(GTK_TEXT_BUFFER(addr_all_buffer), s->str, -1);
        }
        g_string_free(s, TRUE);

        address_i = phone_i = IM_i = 0;
        for (i = 0; i < schema_size; i++) {
            switch (schema[i].type) {
                case ADDRESS_GUI_LABEL_TEXT:
                    goto set_text;
                case ADDRESS_GUI_DIAL_SHOW_PHONE_MENU_TEXT:
                    /* Set dial/email button text and callback data */
                    if (!strcasecmp(contact_app_info.phoneLabels[cont->phoneLabel[phone_i]], _("E-mail"))) {
                         g_object_set_data(G_OBJECT(dial_button[phone_i]), "mail", GINT_TO_POINTER(1));
                        gtk_button_set_label(GTK_BUTTON(dial_button[phone_i]), _("Mail"));
                    } else {
                         g_object_set_data(G_OBJECT(dial_button[phone_i]), "mail", 0);
                        gtk_button_set_label(GTK_BUTTON(dial_button[phone_i]), _("Dial"));
                    }
                    if ((phone_i < NUM_PHONE_ENTRIES) && (cont->phoneLabel[phone_i] < NUM_PHONE_LABELS)) {
                        if (GTK_IS_WIDGET(phone_type_list_menu[phone_i])) {
                            gtk_combo_box_set_active(GTK_COMBO_BOX(phone_type_list_menu[phone_i]),
                                                     cont->phoneLabel[phone_i]);
                        }

                    }
                    phone_i++;
                    goto set_text;
                case ADDRESS_GUI_IM_MENU_TEXT:
                    if (GTK_IS_WIDGET(IM_type_list_menu[IM_i])) {
                        gtk_combo_box_set_active(GTK_COMBO_BOX(IM_type_list_menu[IM_i]), cont->IMLabel[IM_i]);
                    }
                    IM_i++;
                    goto set_text;
                case ADDRESS_GUI_ADDR_MENU_TEXT:
                    if (GTK_IS_WIDGET(address_type_list_menu[address_i])) {
                        gtk_combo_box_set_active(GTK_COMBO_BOX(address_type_list_menu[address_i]),
                                                 cont->addressLabel[address_i]);
                        /* We want to make the notebook page tab label match the type of
                 * address from the menu.  So, we'll find the nth address menu
                 * and set whatever page the schema says it resides on */
                        if (GTK_IS_LABEL(notebook_label[schema[i].notebook_page])) {
                            gtk_label_set_text(GTK_LABEL(notebook_label[schema[i].notebook_page]),
                                               contact_app_info.addrLabels[cont->addressLabel[address_i]]);
                        }
                    }
                    address_i++;
                    goto set_text;
                case ADDRESS_GUI_WEBSITE_TEXT:
                set_text:
                    if (cont->entry[schema[i].record_field]) {
                        gtk_text_buffer_set_text(GTK_TEXT_BUFFER(addr_text_buffer[schema[i].record_field]),
                                                 cont->entry[schema[i].record_field], -1);
                    } else {
                        gtk_text_buffer_set_text(GTK_TEXT_BUFFER(addr_text_buffer[schema[i].record_field]), "", -1);
                    }
                    break;
                case ADDRESS_GUI_BIRTHDAY:
                    get_pref(PREF_TODO_DAYS_TILL_DUE, &ivalue, NULL);
                    reminder_str[0] = '\0';
                    g_snprintf(reminder_str, sizeof(reminder_str), "%ld", ivalue);

                    if (cont->birthdayFlag) {
                        memcpy(&birthday, &cont->birthday, sizeof(struct tm));
                        set_button_label_to_date(birthday_button, &birthday);

                        /* Birthday checkbox */
                        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(birthday_checkbox),
                                                     TRUE);

                        if (cont->reminder) {
                            sprintf(birthday_str, "%d", cont->advance);
                            gtk_entry_set_text(GTK_ENTRY(reminder_entry), birthday_str);

                            /* Reminder checkbox */
                            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(reminder_checkbox),
                                                         cont->reminder);
                        } else {
                            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(reminder_checkbox), FALSE);
                            gtk_entry_set_text(GTK_ENTRY(reminder_entry), reminder_str);
                        }
                    } else {
                        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(birthday_checkbox),
                                                     FALSE);
                        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(reminder_checkbox),
                                                     FALSE);
                        gtk_entry_set_text(GTK_ENTRY(reminder_entry), reminder_str);
                    }
                    break;
                default:
                    break;
            }
        }

        /* Set phone grouped radio buttons */
        if ((cont->showPhone > -1) && (cont->showPhone < NUM_PHONE_ENTRIES)) {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_button[cont->showPhone]), TRUE);
        }

        /* Private checkbox */
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(private_checkbox),
                                     mcont->attrib & dlpRecAttrSecret);

        gtk_widget_thaw_child_notify(addr_all);
        connect_changed_signals(CONNECT_SIGNALS);
    }
    return TRUE;
}


/* Main function */
int address_gui(GtkWidget *vbox, GtkWidget *hbox) {
    GtkWidget *scrolled_window;
    GtkWidget *pixmapwid;
    GdkPixbuf *pixmap;
    GtkWidget *vbox1, *vbox2;
    GtkWidget *hbox_temp;
    GtkWidget *vbox_temp;
    GtkWidget *separator;
    GtkWidget *label;
    GtkWidget *button;
    GtkWidget *grid;
    GtkWidget *notebook_tab;
    GSList *group;
    long ivalue, notebook_page;
    long show_tooltips;
    GtkAccelGroup *accel_group;
    int address_type_i, IM_type_i, page_i, grid_y_i;
    int x, y;

    int i, j, phone_i, num_pages;
    long char_set;
    char *cat_name;

    /* Note that the contact pages labeled "Address" will change
    * dynamically if the address type pulldown is selected */
    char *contact_page_names[] = {
            N_("Name"),
            N_("Address"),
            N_("Address"),
            N_("Address"),
            N_("Other"),
            N_("Note")
    };
    char *address_page_names[] = {
            N_("Name"),
            N_("Address"),
            N_("Other"),
            N_("Note")
    };
    char **page_names;

    jp_logf(JP_LOG_DEBUG, "address_gui()\n");
    get_pref(PREF_ADDRESS_VERSION, &address_version, NULL);
    if (address_version) {
        unsigned char *buf;
        int rec_size;

        if ((EXIT_SUCCESS != jp_get_app_info("ContactsDB-PAdd", &buf, &rec_size)) || (0 == rec_size)) {
            jp_logf(JP_LOG_WARN, _("Reverting to Address database\n"));
            address_version = 0;
        } else {
            if (buf) free(buf);
        }
    }

    init();

    if (address_version) {
        page_names = contact_page_names;
        num_pages = NUM_CONTACT_NOTEBOOK_PAGES;
        get_contact_app_info(&contact_app_info);
    } else {
        page_names = address_page_names;
        num_pages = NUM_ADDRESS_NOTEBOOK_PAGES;
        if (glob_sqlite) {
            jpsqlite_AddrLabelSEL(&address_app_info);
            jpsqlite_PhoneLabelSEL(&address_app_info);
            jpsqlite_AddrCatSEL(&address_app_info);
        } else get_address_app_info(&address_app_info);
        jpsqlite_prtAddrAppInfo(&address_app_info);
        copy_address_ai_to_contact_ai(&address_app_info, &contact_app_info);
    }
    listStore = gtk_list_store_new(ADDRESS_NUM_COLS, G_TYPE_STRING, GDK_TYPE_PIXBUF,
                                   G_TYPE_STRING, G_TYPE_POINTER, GDK_TYPE_RGBA,
                                   G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_BOOLEAN);
    /* Initialize categories */
    get_pref(PREF_CHAR_SET, &char_set, NULL);
    for (i = 1; i < NUM_ADDRESS_CAT_ITEMS; i++) {
        cat_name = charset_p2newj(contact_app_info.category.name[i], 31, char_set);
        strcpy(sort_l[i - 1].Pcat, cat_name);
        free(cat_name);
        sort_l[i - 1].cat_num = i;
    }
    /* put reserved 'Unfiled' category at end of list */
    cat_name = charset_p2newj(contact_app_info.category.name[0], 31, char_set);
    strcpy(sort_l[NUM_ADDRESS_CAT_ITEMS - 1].Pcat, cat_name);
    free(cat_name);
    sort_l[NUM_ADDRESS_CAT_ITEMS - 1].cat_num = 0;

    qsort(sort_l, NUM_ADDRESS_CAT_ITEMS - 1, sizeof(struct sorted_cats), cat_compare);
#ifdef JPILOT_DEBUG
   for (i=0; i<NUM_ADDRESS_CAT_ITEMS; i++) {
      printf("cat %d [%s]\n", sort_l[i].cat_num, sort_l[i].Pcat);
   }
#endif

    get_pref(PREF_LAST_ADDR_CATEGORY, &ivalue, NULL);
    address_category = ivalue;

    if ((address_category != CATEGORY_ALL)
        && (contact_app_info.category.name[address_category][0] == '\0')) {
        address_category = CATEGORY_ALL;
    }

    /* Create basic GUI with left and right boxes and sliding pane */
    accel_group = gtk_accel_group_new();
    gtk_window_add_accel_group(GTK_WINDOW(gtk_widget_get_toplevel(vbox)),
                               accel_group);
    get_pref(PREF_SHOW_TOOLTIPS, &show_tooltips, NULL);

    pane = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    get_pref(PREF_ADDRESS_PANE, &ivalue, NULL);
    gtk_paned_set_position(GTK_PANED(pane), ivalue);

    gtk_box_pack_start(GTK_BOX(hbox), pane, TRUE, TRUE, 5);

    vbox1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_paned_pack1(GTK_PANED(pane), vbox1, TRUE, FALSE);
    gtk_paned_pack2(GTK_PANED(pane), vbox2, TRUE, FALSE);

    /* Left side of GUI */

    /* Separator */
    separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(vbox1), separator, FALSE, FALSE, 5);

    /* Make the 'Today is:' label */
    glob_date_label = gtk_label_new(" ");
    gtk_box_pack_start(GTK_BOX(vbox1), glob_date_label, FALSE, FALSE, 0);
    timeout_date(NULL);
    glob_date_timer_tag = g_timeout_add(CLOCK_TICK, timeout_sync_up, NULL);

    /* Separator */
    separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(vbox1), separator, FALSE, FALSE, 5);

    /* Left-side Category box */
    hbox_temp = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(vbox1), hbox_temp, FALSE, FALSE, 0);

    /* Left-side Category menu */
    make_category_menu(&category_menu1,
                       sort_l, cb_category, TRUE, TRUE);
    gtk_box_pack_start(GTK_BOX(hbox_temp), category_menu1, TRUE, TRUE, 0);

    /* Address list scrolled window */
    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_set_border_width(GTK_CONTAINER(scrolled_window), 0);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(vbox1), scrolled_window, TRUE, TRUE, 0);
    get_pref(PREF_ADDR_SORT_ORDER, &ivalue, NULL);
    addr_sort_order = (int) ivalue;
    GtkTreeModel *model = GTK_TREE_MODEL(listStore);
    treeView = gtk_tree_view_new_with_model(model);
    get_pref(PREF_ADDR_NAME_COL_SZ, &ivalue, NULL);
    GtkCellRenderer *nameRenderer = gtk_cell_renderer_text_new();
    gtk_cell_renderer_set_fixed_size(nameRenderer, ivalue, 18);
    GtkTreeViewColumn *nameColumn = gtk_tree_view_column_new_with_attributes(ADDRESS_LAST_NAME_COMPANY, nameRenderer,
                                                                             "text",
                                                                             ADDRESS_NAME_COLUMN_ENUM,
                                                                             "cell-background-rgba",
                                                                             ADDRESS_BACKGROUND_COLOR_ENUM,
                                                                             "cell-background-set",
                                                                             ADDRESS_BACKGROUND_COLOR_ENABLED_ENUM,
                                                                             NULL);
    gtk_tree_view_column_set_clickable(nameColumn, TRUE);

    GtkCellRenderer *noteRenderer = gtk_cell_renderer_pixbuf_new();
    gtk_cell_renderer_set_alignment(noteRenderer,0,0);
    gtk_cell_renderer_set_padding(noteRenderer,4,0);
    GtkTreeViewColumn *noteColumn = gtk_tree_view_column_new_with_attributes("", noteRenderer, "pixbuf",
                                                                             ADDRESS_NOTE_COLUMN_ENUM,
                                                                             "cell-background-rgba",
                                                                             ADDRESS_BACKGROUND_COLOR_ENUM,
                                                                             "cell-background-set",
                                                                             ADDRESS_BACKGROUND_COLOR_ENABLED_ENUM,
                                                                             NULL);
    gtk_tree_view_column_set_clickable(noteColumn, FALSE);
    GtkCellRenderer *phoneRenderer = gtk_cell_renderer_text_new();
    // Set the phone column width to something small and let it expand to the pane
    // Set the height to 1 so we do not see line wraps making verticle gaps in the view
    gtk_cell_renderer_set_fixed_size(phoneRenderer, 100, 18);
    GtkTreeViewColumn *phoneColumn = gtk_tree_view_column_new_with_attributes("Phone", phoneRenderer, "text",
                                                                              ADDRESS_PHONE_COLUMN_ENUM,
                                                                              "cell-background-rgba",
                                                                              ADDRESS_BACKGROUND_COLOR_ENUM,
                                                                              "cell-background-set",
                                                                              ADDRESS_BACKGROUND_COLOR_ENABLED_ENUM,
                                                                              NULL);
    gtk_tree_view_column_set_clickable(phoneColumn, FALSE);

    gtk_tree_view_insert_column(GTK_TREE_VIEW(treeView), nameColumn, ADDRESS_NAME_COLUMN_ENUM);
    gtk_tree_view_insert_column(GTK_TREE_VIEW(treeView), noteColumn, ADDRESS_NOTE_COLUMN_ENUM);
    gtk_tree_view_insert_column(GTK_TREE_VIEW(treeView), phoneColumn, ADDRESS_PHONE_COLUMN_ENUM);
    treeSelection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeView));

    gtk_tree_selection_set_select_function(treeSelection, handleRowSelectionForAddress, NULL, NULL);
    gtk_widget_set_events(treeView, GDK_BUTTON1_MOTION_MASK);
    g_signal_connect (G_OBJECT(treeView), "motion_notify_event",
                      G_CALLBACK(motion_notify_event), NULL);
    g_signal_connect (G_OBJECT(treeView), "button-press-event",
                      G_CALLBACK(button_pressed_for_motion), NULL);
    g_signal_connect (G_OBJECT(treeView), "button-release-event",
                      G_CALLBACK(button_released_for_motion), NULL);

    switch (addr_sort_order) {
        case SORT_BY_LNAME:
        default:
            addr_sort_order = SORT_BY_LNAME;  /* Initialize variable if default case taken */
            gtk_tree_view_column_set_title(nameColumn, ADDRESS_LAST_NAME_COMPANY);
            break;
        case SORT_BY_FNAME:
            gtk_tree_view_column_set_title(nameColumn, ADDRESS_FIRST_NAME_COMPANY);
            break;
        case SORT_BY_COMPANY:
            gtk_tree_view_column_set_title(nameColumn, ADDRESS_COMPANY_LAST_NAME);
            break;
    }
    g_signal_connect (nameColumn, "clicked", G_CALLBACK(cb_resortNameColumn), NULL);



    /* Put pretty pictures in the list column headings */
    get_pixbufs(PIXMAP_NOTE, &pixmap);
    pixmapwid = gtk_image_new_from_pixbuf(pixmap);
    gtk_widget_show(GTK_WIDGET(pixmapwid));
    gtk_tree_view_column_set_widget(noteColumn, pixmapwid);

    gtk_tree_view_column_set_alignment(noteColumn, GTK_JUSTIFY_LEFT);

    gtk_tree_view_column_set_min_width(nameColumn, 60);


    gtk_tree_view_column_set_resizable(nameColumn, TRUE);
    gtk_tree_view_column_set_resizable(noteColumn, FALSE);
    gtk_tree_view_column_set_resizable(phoneColumn, TRUE);



    g_signal_connect(G_OBJECT(nameColumn), "notify::width",
                     G_CALLBACK(cb_resize_name_column), NULL);

    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(scrolled_window),
                                    GTK_POLICY_NEVER,
                                    GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scrolled_window), GTK_WIDGET(treeView));
    hbox_temp = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(vbox1), hbox_temp, FALSE, FALSE, 0);

    label = gtk_label_new(_("Quick Find: "));
    gtk_box_pack_start(GTK_BOX(hbox_temp), label, FALSE, FALSE, 0);

    address_quickfind_entry = gtk_entry_new();
    entry_set_multiline_truncate(GTK_ENTRY(address_quickfind_entry), TRUE);
    g_signal_connect(G_OBJECT(address_quickfind_entry), "key_press_event",
                       G_CALLBACK(cb_key_pressed_quickfind), NULL);
    g_signal_connect(G_OBJECT(address_quickfind_entry), "changed",
                     G_CALLBACK(cb_address_quickfind),
                       NULL);
    gtk_box_pack_start(GTK_BOX(hbox_temp), address_quickfind_entry, TRUE, TRUE, 0);

    /* Right side of GUI */

    hbox_temp = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
    gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, FALSE, FALSE, 0);

    /* Cancel button */
    CREATE_BUTTON(cancel_record_button, _("Cancel"), CANCEL, _("Cancel the modifications"), GDK_KEY_Escape, 0, "ESC")
    g_signal_connect(G_OBJECT(cancel_record_button), "clicked",
                     G_CALLBACK(cb_cancel), NULL);

    /* Delete Button */
    CREATE_BUTTON(delete_record_button, _("Delete"), DELETE, _("Delete the selected record"), GDK_d, GDK_CONTROL_MASK,
                  "Ctrl+D")
    g_signal_connect(G_OBJECT(delete_record_button), "clicked",
                       G_CALLBACK(cb_delete_address_or_contact),
                       GINT_TO_POINTER(DELETE_FLAG));

    /* Undelete Button */
    CREATE_BUTTON(undelete_record_button, _("Undelete"), UNDELETE, _("Undelete the selected record"), 0, 0, "")
    g_signal_connect(G_OBJECT(undelete_record_button), "clicked",
                       G_CALLBACK(cb_undelete_address),
                       GINT_TO_POINTER(UNDELETE_FLAG));

    /* Copy button */
    CREATE_BUTTON(copy_record_button, _("Copy"), COPY, _("Copy the selected record"), GDK_c,
                  GDK_CONTROL_MASK | GDK_SHIFT_MASK, "Ctrl+Shift+C")
    g_signal_connect(G_OBJECT(copy_record_button), "clicked",
                       G_CALLBACK(cb_add_new_record),
                       GINT_TO_POINTER(COPY_FLAG));

    /* New button */
    CREATE_BUTTON(new_record_button, _("New Record"), NEW, _("Add a new record"), GDK_n, GDK_CONTROL_MASK, "Ctrl+N")
    g_signal_connect(G_OBJECT(new_record_button), "clicked",
                       G_CALLBACK(cb_address_clear), NULL);

    /* "Add Record" button */
    CREATE_BUTTON(add_record_button, _("Add Record"), ADD, _("Add the new record"), GDK_KEY_Return, GDK_CONTROL_MASK,
                  "Ctrl+Enter")
    g_signal_connect(G_OBJECT(add_record_button), "clicked",
                       G_CALLBACK(cb_add_new_record),
                       GINT_TO_POINTER(NEW_FLAG));


#ifndef ENABLE_STOCK_BUTTONS
    gtk_widget_set_name(GTK_WIDGET(GTK_LABEL(gtk_bin_get_child(GTK_BIN(add_record_button)))),
                    "label_high");
#endif

    /* "Apply Changes" button */
    CREATE_BUTTON(apply_record_button, _("Apply Changes"), APPLY, _("Commit the modifications"), GDK_KEY_Return,
                  GDK_CONTROL_MASK, "Ctrl+Enter")
    g_signal_connect(G_OBJECT(apply_record_button), "clicked",
                       G_CALLBACK(cb_add_new_record),
                       GINT_TO_POINTER(MODIFY_FLAG));
#ifndef ENABLE_STOCK_BUTTONS
    gtk_widget_set_name(GTK_WIDGET(GTK_LABEL(gtk_bin_get_child(GTK_BIN(apply_record_button)))),
         "label_high");
#endif

    /* Separator */
    separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(vbox2), separator, FALSE, FALSE, 5);

    hbox_temp = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, FALSE, FALSE, 0);

    /* Right-side Category menu */
    make_category_menu(&category_menu2,
                       sort_l, NULL, FALSE, FALSE);

    gtk_box_pack_start(GTK_BOX(hbox_temp), category_menu2, TRUE, TRUE, 0);

    //for (i = 0; i < NUM_ADDRESS_CAT_ITEMS; i++) {
    //  changed_list = g_list_prepend(changed_list, address_cat_menu_item2[i]);
    // }
    changed_list = g_list_prepend(changed_list, GTK_COMBO_BOX(category_menu2));
    /* Private check box */
    private_checkbox = gtk_check_button_new_with_label(_("Private"));
    gtk_box_pack_end(GTK_BOX(hbox_temp), private_checkbox, FALSE, FALSE, 0);

    changed_list = g_list_prepend(changed_list, private_checkbox);

    /* Notebook for new entries */
    notebook = gtk_notebook_new();
    gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_TOP);
    gtk_notebook_popup_enable(GTK_NOTEBOOK(notebook));
    g_signal_connect(G_OBJECT(notebook), "switch-page",
                       G_CALLBACK(cb_notebook_changed), NULL);

    gtk_box_pack_start(GTK_BOX(vbox2), notebook, TRUE, TRUE, 0);

    /* Clear GTK option menus before use */
    for (i = 0; i < NUM_ADDRESSES; i++) {
        for (j = 0; j < NUM_PHONE_LABELS; j++) {
            if (GTK_IS_COMBO_BOX(phone_type_list_menu[i]) &&
                gtk_combo_box_get_has_entry(GTK_COMBO_BOX(phone_type_list_menu[i]))) {
                gtk_combo_box_text_remove(GTK_COMBO_BOX_TEXT(phone_type_list_menu[i]), j);
            }
        }
    }

    /* Add notebook pages and their widgets */
    phone_i = address_type_i = IM_type_i = 0;
    for (page_i = 0; page_i < num_pages; page_i++) {
        x = y = 0;
        for (i = 0; i < schema_size; i++) {
            /* Figure out the table X and Y size */
            if (schema[i].notebook_page != page_i) continue;
            switch (schema[i].type) {
                case ADDRESS_GUI_LABEL_TEXT:
                    if (x < 2) x = 2;
                    y++;
                    break;
                case ADDRESS_GUI_DIAL_SHOW_PHONE_MENU_TEXT:
                    if (x < 4) x = 4;
                    y++;
                    break;
                case ADDRESS_GUI_ADDR_MENU_TEXT:
                    if (x < 2) x = 2;
                    y++;
                    break;
                case ADDRESS_GUI_IM_MENU_TEXT:
                    if (x < 2) x = 2;
                    y++;
                    break;
                case ADDRESS_GUI_WEBSITE_TEXT:
                    if (x < 2) x = 2;
                    y++;
                    break;
                case ADDRESS_GUI_BIRTHDAY:
                    if (x < 2) x = 2;
                    y++;
                    break;
                default:
                    break;
            }
        }

        if ((x == 0) || (y == 0)) {
            continue;
        }

        /* Add a notebook page */
        grid_y_i = 0;
        notebook_label[page_i] = gtk_label_new(_(page_names[page_i]));
        grid = gtk_grid_new();
        gtk_grid_set_row_spacing(GTK_GRID(grid), 2);
        gtk_notebook_append_page(GTK_NOTEBOOK(notebook), grid, notebook_label[page_i]);

        gtk_widget_show(label);

        /* Grid on the right side */

        if ((page_i == 0) && (grid_y_i == 0) && (address_version == 1)) {
            GtkWidget *menu, *menu_item;

            picture_button = gtk_button_new();
            gtk_widget_set_size_request(GTK_WIDGET(picture_button), PHOTO_X_SZ + 10, PHOTO_Y_SZ + 10);
            gtk_container_set_border_width(GTK_CONTAINER(picture_button), 0);
            gtk_widget_set_vexpand(GTK_WIDGET(picture_button), FALSE);
            gtk_widget_set_valign(GTK_WIDGET(picture_button), GTK_ALIGN_START);
            gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(picture_button),
                            0, 0, 2, 4);

            /* Create a photo menu */
            menu = gtk_menu_new();

            menu_item = gtk_menu_item_new_with_label(_("Change Photo"));
            gtk_widget_show(menu_item);
            g_signal_connect(G_OBJECT(menu_item), "activate",
                               G_CALLBACK(cb_photo_menu_select), GINT_TO_POINTER(1));
            gtk_menu_attach(GTK_MENU(menu), menu_item, 0, 1, 0, 1);
            menu_item = gtk_menu_item_new_with_label(_("Remove Photo"));
            gtk_widget_show(menu_item);
            g_signal_connect(G_OBJECT(menu_item), "activate",
                               G_CALLBACK(cb_photo_menu_select), GINT_TO_POINTER(2));
            gtk_menu_attach(GTK_MENU(menu), menu_item, 0, 1, 1, 2);

            g_signal_connect_swapped(picture_button, "button_press_event",
                                     G_CALLBACK(cb_photo_menu_popup), menu);

        }

        /* Add widgets for each notebook page */

        group = NULL;
        for (i = 0; i < schema_size; i++) {
            char *utf;

            if (schema[i].notebook_page != page_i) continue;
            switch (schema[i].type) {
                case ADDRESS_GUI_LABEL_TEXT:
                    /* special case for Note which has a scrollbar and no label */
                    if (schema[i].record_field != contNote) {
                        if (address_version) {
                            label = gtk_label_new(contact_app_info.labels[schema[i].record_field]);
                        } else {
                            utf = charset_p2newj(contact_app_info.labels[schema[i].record_field], 16, char_set);
                            label = gtk_label_new(utf);
                            g_free(utf);
                        }
                        gtk_widget_set_margin_end(GTK_WIDGET(label), 5);
                        gtk_label_set_xalign(GTK_LABEL(label), 1);
                        gtk_label_set_yalign(GTK_LABEL(label), 0);
                        // These are the labels "Last Name", "First Name", "Company", "Title" just to the right of the picture
                        gtk_widget_set_vexpand(GTK_WIDGET(label), FALSE);
                        gtk_widget_set_valign(GTK_WIDGET(label), GTK_ALIGN_START);
                        gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(label),
                                        x - 2, grid_y_i, 1, 1);
                    }
                    /* Text */
                    addr_text[schema[i].record_field] = gtk_text_view_new();
                    addr_text_buffer[schema[i].record_field] = G_OBJECT(
                            gtk_text_view_get_buffer(GTK_TEXT_VIEW(addr_text[schema[i].record_field])));
                    gtk_text_view_set_editable(GTK_TEXT_VIEW(addr_text[schema[i].record_field]), TRUE);
                    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(addr_text[schema[i].record_field]), GTK_WRAP_CHAR);
                    gtk_widget_set_hexpand(addr_text[schema[i].record_field], TRUE);
                    gtk_widget_set_halign(addr_text[schema[i].record_field], GTK_ALIGN_FILL);
                    gtk_widget_set_vexpand(addr_text[schema[i].record_field], TRUE);
                    gtk_widget_set_valign(addr_text[schema[i].record_field], GTK_ALIGN_FILL);
                    gtk_container_set_border_width(GTK_CONTAINER(addr_text[schema[i].record_field]), 1);

                    /* special case for Note which has a scrollbar and no label */
                    if (schema[i].record_field == contNote) {
                        scrolled_window = gtk_scrolled_window_new(NULL, NULL);
                        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                                       GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
                        gtk_container_add(GTK_CONTAINER(scrolled_window), addr_text[schema[i].record_field]);
                    }


                    /* special case for Note which has a scrollbar and no label */
                    if (schema[i].record_field == contNote) {
                        gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(scrolled_window),
                                        x - 1, grid_y_i, 10, 1);

                    } else {
                        gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(addr_text[schema[i].record_field]),
                                        x - 1, grid_y_i, 10, 1);
                    }

                    changed_list = g_list_prepend(changed_list, addr_text_buffer[schema[i].record_field]);
                    break;
                case ADDRESS_GUI_DIAL_SHOW_PHONE_MENU_TEXT:
                    if (!strcasecmp(contact_app_info.phoneLabels[phone_i], _("E-mail"))) {
                        dial_button[phone_i] = gtk_button_new_with_label(_("Mail"));
                         g_object_set_data(G_OBJECT(dial_button[phone_i]), "mail", GINT_TO_POINTER(1));
                    } else {
                        dial_button[phone_i] = gtk_button_new_with_label(_("Dial"));
                         g_object_set_data(G_OBJECT(dial_button[phone_i]), "mail", 0);
                    }
                    gtk_widget_set_vexpand(GTK_WIDGET(dial_button[phone_i]), FALSE);
                    gtk_widget_set_valign(GTK_WIDGET(dial_button[phone_i]), GTK_ALIGN_START);
                    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(dial_button[phone_i]),
                                    x - 4, grid_y_i, 1, 1);

                    radio_button[phone_i] = gtk_radio_button_new_with_label(group, _("Show In List"));
                    group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio_button[phone_i]));
                    gtk_widget_set_vexpand(GTK_WIDGET(radio_button[phone_i]), FALSE);
                    gtk_widget_set_valign(GTK_WIDGET(radio_button[phone_i]), GTK_ALIGN_START);
                    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(radio_button[phone_i]),
                                    x - 3, grid_y_i, 1, 1);

                    changed_list = g_list_prepend(changed_list, radio_button[phone_i]);

                    make_phone_menu(phone_i, phone_i, phone_i);
                    gtk_widget_set_vexpand(GTK_WIDGET(phone_type_list_menu[phone_i]), FALSE);
                    gtk_widget_set_valign(GTK_WIDGET(phone_type_list_menu[phone_i]), GTK_ALIGN_START);
                    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(phone_type_list_menu[phone_i]),
                                    x - 2, grid_y_i, 1, 1);

                    /* Text */
                    addr_text[schema[i].record_field] = gtk_text_view_new();
                    addr_text_buffer[schema[i].record_field] = G_OBJECT(
                    gtk_text_view_get_buffer(GTK_TEXT_VIEW(addr_text[schema[i].record_field])));
                    gtk_text_view_set_editable(GTK_TEXT_VIEW(addr_text[schema[i].record_field]), TRUE);
                    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(addr_text[schema[i].record_field]), GTK_WRAP_CHAR);
                    gtk_container_set_border_width(GTK_CONTAINER(addr_text[schema[i].record_field]), 1);
                    gtk_widget_set_hexpand(addr_text[schema[i].record_field], TRUE);
                    gtk_widget_set_halign(addr_text[schema[i].record_field], GTK_ALIGN_FILL);
                    gtk_widget_set_vexpand(addr_text[schema[i].record_field], TRUE);
                    gtk_widget_set_valign(addr_text[schema[i].record_field], GTK_ALIGN_FILL);
                    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(addr_text[schema[i].record_field]),
                                    x - 1, grid_y_i, 1, 1);

                    g_signal_connect(G_OBJECT(dial_button[phone_i]), "clicked",
                                       G_CALLBACK(cb_dial_or_mail),
                                       addr_text[schema[i].record_field]);
                    changed_list = g_list_prepend(changed_list, addr_text_buffer[schema[i].record_field]);
                    phone_i++;
                    break;
                case ADDRESS_GUI_ADDR_MENU_TEXT:
                    make_address_type_menu(address_type_i, address_type_i);
                    gtk_widget_set_vexpand(GTK_WIDGET(address_type_list_menu[address_type_i]), FALSE);
                    gtk_widget_set_valign(GTK_WIDGET(address_type_list_menu[address_type_i]), GTK_ALIGN_START);
                    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(address_type_list_menu[address_type_i]),
                                    x - 2, grid_y_i, 1, 1);
                    address_type_i++;

                    /* Text */
                    addr_text[schema[i].record_field] = gtk_text_view_new();
                    addr_text_buffer[schema[i].record_field] = G_OBJECT(
                            gtk_text_view_get_buffer(GTK_TEXT_VIEW(addr_text[schema[i].record_field])));
                    gtk_text_view_set_editable(GTK_TEXT_VIEW(addr_text[schema[i].record_field]), TRUE);
                    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(addr_text[schema[i].record_field]), GTK_WRAP_CHAR);
                    gtk_container_set_border_width(GTK_CONTAINER(addr_text[schema[i].record_field]), 1);
                    gtk_widget_set_hexpand(addr_text[schema[i].record_field], TRUE);
                    gtk_widget_set_halign(addr_text[schema[i].record_field], GTK_ALIGN_FILL);
                    gtk_widget_set_vexpand(addr_text[schema[i].record_field], TRUE);
                    gtk_widget_set_valign(addr_text[schema[i].record_field], GTK_ALIGN_FILL);
                    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(addr_text[schema[i].record_field]),
                                    x - 1, grid_y_i, 1, 1);

                    changed_list = g_list_prepend(changed_list, addr_text_buffer[schema[i].record_field]);
                    break;
                case ADDRESS_GUI_IM_MENU_TEXT:
                    make_IM_type_menu(IM_type_i, IM_type_i, IM_type_i);
                    gtk_widget_set_vexpand(GTK_WIDGET(IM_type_list_menu[IM_type_i]), FALSE);
                    gtk_widget_set_valign(GTK_WIDGET(IM_type_list_menu[IM_type_i]), GTK_ALIGN_START);
                    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(IM_type_list_menu[IM_type_i]),
                                    x - 2, grid_y_i, 1, 1);
                    IM_type_i++;

                    /* Text */
                    addr_text[schema[i].record_field] = gtk_text_view_new();
                    addr_text_buffer[schema[i].record_field] = G_OBJECT(
                            gtk_text_view_get_buffer(GTK_TEXT_VIEW(addr_text[schema[i].record_field])));
                    gtk_text_view_set_editable(GTK_TEXT_VIEW(addr_text[schema[i].record_field]), TRUE);
                    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(addr_text[schema[i].record_field]), GTK_WRAP_CHAR);
                    gtk_container_set_border_width(GTK_CONTAINER(addr_text[schema[i].record_field]), 1);
                    gtk_widget_set_hexpand(addr_text[schema[i].record_field], TRUE);
                    gtk_widget_set_halign(addr_text[schema[i].record_field], GTK_ALIGN_FILL);
                    gtk_widget_set_vexpand(addr_text[schema[i].record_field], TRUE);
                    gtk_widget_set_valign(addr_text[schema[i].record_field], GTK_ALIGN_FILL);
                    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(addr_text[schema[i].record_field]),
                                    x - 1, grid_y_i, 1, 1);

                    changed_list = g_list_prepend(changed_list, addr_text_buffer[schema[i].record_field]);
                    break;
                case ADDRESS_GUI_WEBSITE_TEXT:
                    /* Button used as label */
                    button = gtk_button_new_with_label(contact_app_info.labels[schema[i].record_field]);
                    /* Remove normal button behavior to accept focus */
                    gtk_widget_set_focus_on_click(GTK_WIDGET(button), FALSE);
                    gtk_widget_set_vexpand(GTK_WIDGET(button), FALSE);
                    gtk_widget_set_valign(GTK_WIDGET(button), GTK_ALIGN_START);
                    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(button),
                                    x - 2, grid_y_i, 1, 1);
                    /* Text */
                    addr_text[schema[i].record_field] = gtk_text_view_new();
                    addr_text_buffer[schema[i].record_field] = G_OBJECT(
                            gtk_text_view_get_buffer(GTK_TEXT_VIEW(addr_text[schema[i].record_field])));
                    gtk_text_view_set_editable(GTK_TEXT_VIEW(addr_text[schema[i].record_field]), TRUE);
                    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(addr_text[schema[i].record_field]), GTK_WRAP_CHAR);
                    gtk_container_set_border_width(GTK_CONTAINER(addr_text[schema[i].record_field]), 1);
                    gtk_widget_set_hexpand(addr_text[schema[i].record_field], TRUE);
                    gtk_widget_set_halign(addr_text[schema[i].record_field], GTK_ALIGN_FILL);
                    gtk_widget_set_vexpand(addr_text[schema[i].record_field], TRUE);
                    gtk_widget_set_valign(addr_text[schema[i].record_field], GTK_ALIGN_FILL);
                    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(addr_text[schema[i].record_field]),
                                    x - 1, grid_y_i, 10, 1);

                    changed_list = g_list_prepend(changed_list, addr_text_buffer[schema[i].record_field]);
                    break;
                case ADDRESS_GUI_BIRTHDAY:
                    hbox_temp = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
                    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(hbox_temp),
                                    0, grid_y_i, 1, 1);

                    birthday_checkbox = gtk_check_button_new_with_label(
                            contact_app_info.labels[schema[i].record_field]);
                    gtk_box_pack_start(GTK_BOX(hbox_temp), birthday_checkbox, FALSE, FALSE, 0);
                    g_signal_connect(G_OBJECT(birthday_checkbox), "clicked",
                                       G_CALLBACK(cb_check_button_birthday), NULL);

                    changed_list = g_list_prepend(changed_list, birthday_checkbox);

                    birthday_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
                    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(birthday_box),
                                    1, grid_y_i, 1, 1);

                    birthday_button = gtk_button_new_with_label("");
                    gtk_box_pack_start(GTK_BOX(birthday_box), birthday_button, FALSE, FALSE, 0);
                    g_signal_connect(G_OBJECT(birthday_button), "clicked",
                                       G_CALLBACK(cb_button_birthday), NULL);

                    changed_list = g_list_prepend(changed_list, birthday_button);

                    reminder_checkbox = gtk_check_button_new_with_label(_("Reminder"));
                    gtk_box_pack_start(GTK_BOX(birthday_box), reminder_checkbox, FALSE, FALSE, 0);
                    g_signal_connect(G_OBJECT(reminder_checkbox), "clicked",
                                       G_CALLBACK(cb_check_button_reminder), NULL);

                    changed_list = g_list_prepend(changed_list, reminder_checkbox);

                    reminder_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
                    gtk_box_pack_start(GTK_BOX(birthday_box), reminder_box, FALSE, FALSE, 0);

                    reminder_entry = gtk_entry_new();
                    gtk_entry_set_max_length(GTK_ENTRY(reminder_entry), 2);
                    entry_set_multiline_truncate(GTK_ENTRY(reminder_entry), TRUE);
                    gtk_box_pack_start(GTK_BOX(reminder_box), reminder_entry, FALSE, FALSE, 0);

                    changed_list = g_list_prepend(changed_list, reminder_entry);

                    label = gtk_label_new(_("Days"));
                    gtk_box_pack_start(GTK_BOX(reminder_box), label, FALSE, FALSE, 0);

                    break;
                default:
                    break;
            }
            grid_y_i++;
        }
    }

    /* Connect keypress signals to callbacks */

    /* Capture the Enter key to move to the left-hand side of the display */
    g_signal_connect(G_OBJECT(treeView), "key_press_event",
                       G_CALLBACK(cb_key_pressed_left_side),
                       NULL);

    for (i = 0; i < schema_size; i++) {
        switch (schema[i].type) {
            case ADDRESS_GUI_LABEL_TEXT:
            case ADDRESS_GUI_DIAL_SHOW_PHONE_MENU_TEXT:
            case ADDRESS_GUI_ADDR_MENU_TEXT:
            case ADDRESS_GUI_IM_MENU_TEXT:
            case ADDRESS_GUI_WEBSITE_TEXT:
                /* Capture the Shift-Enter key combination to move back to
          * the right-hand side of the display. */
                if (schema[i].record_field != contNote) {
                    g_signal_connect(G_OBJECT(addr_text[schema[i].record_field]),
                                       "key_press_event",
                                       G_CALLBACK(cb_key_pressed_right_side),
                                       NULL);
                } else {
                    g_signal_connect(G_OBJECT(addr_text[schema[i].record_field]),
                                       "key_press_event",
                                       G_CALLBACK(cb_key_pressed_right_side),
                                       GINT_TO_POINTER(i));
                }

                g_signal_connect(G_OBJECT(addr_text[schema[i].record_field]), "key_press_event",
                                   G_CALLBACK(cb_key_pressed), 0);
                break;
            default:
                break;
        }
    }

    /* The Quickview (ALL) page */
    notebook_tab = gtk_label_new(_("All"));
    vbox_temp = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox_temp, notebook_tab);
    /* Notebook tabs have to be shown before call to show_all */
    gtk_widget_show(vbox_temp);
    gtk_widget_show(notebook_tab);

    hbox_temp = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(vbox_temp), hbox_temp, TRUE, TRUE, 0);

    addr_all = gtk_text_view_new();
    addr_all_buffer = G_OBJECT(gtk_text_view_get_buffer(GTK_TEXT_VIEW(addr_all)));
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(addr_all), FALSE);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(addr_all), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(addr_all), GTK_WRAP_CHAR);
    gtk_container_set_border_width(GTK_CONTAINER(addr_all), 1);
    gtk_widget_set_hexpand(addr_all, TRUE);
    gtk_widget_set_halign(addr_all, GTK_ALIGN_FILL);
    gtk_widget_set_vexpand(addr_all, TRUE);
    gtk_widget_set_valign(addr_all, GTK_ALIGN_FILL);

    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                   GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
    gtk_container_add(GTK_CONTAINER(scrolled_window), addr_all);
    gtk_box_pack_start(GTK_BOX(hbox_temp), scrolled_window, TRUE, TRUE, 0);

    /**********************************************************************/

    gtk_widget_show_all(vbox);
    gtk_widget_show_all(hbox);

    gtk_widget_hide(add_record_button);
    gtk_widget_hide(apply_record_button);
    gtk_widget_hide(undelete_record_button);
    gtk_widget_hide(cancel_record_button);
    if (address_version) {
        gtk_widget_hide(reminder_box);
        gtk_widget_hide(birthday_box);
    }

    get_pref(PREF_ADDRESS_NOTEBOOK_PAGE, &notebook_page, NULL);

    if ((notebook_page < 6) && (notebook_page > -1)) {
        gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), notebook_page);
    }

    address_refresh();

    return EXIT_SUCCESS;
}


/*******************************************************************************
 * todo_gui.c
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
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <pi-dlp.h>
#include <slcurses.h>

#include "todo.h"
#include "i18n.h"
#include "utils.h"
#include "log.h"
#include "prefs.h"
#include "password.h"
#include "print.h"
#include "export.h"
#include "stock_buttons.h"

/********************************* Constants **********************************/
#define TODO_MAX_COLUMN_LEN 80
#define MAX_RADIO_BUTTON_LEN 100

#define NUM_TODO_PRIORITIES 5
#define NUM_TODO_CAT_ITEMS 16
#define NUM_TODO_CSV_FIELDS 8
#define CONNECT_SIGNALS 400
#define DISCONNECT_SIGNALS 401

/* RFCs use CRLF for Internet newline */
#define CRLF "\x0D\x0A"

/******************************* Global vars **********************************/
/* Keeps track of whether code is using ToDo, or Tasks database
 * 0 is ToDo, 1 is Tasks */
static long todo_version = 0;

extern GtkWidget *glob_date_label;
extern int glob_date_timer_tag;

static GtkWidget *clist;
static GtkWidget *treeView;
static GtkTreeSelection *treeSelection;
static GtkListStore *listStore;
static GtkWidget *todo_desc, *todo_note;
static GObject *todo_desc_buffer, *todo_note_buffer;
static GtkWidget *todo_completed_checkbox;
static GtkWidget *private_checkbox;
static struct tm due_date;
static GtkWidget *due_date_button;
static GtkWidget *todo_no_due_date_checkbox;
static GtkWidget *radio_button_todo[NUM_TODO_PRIORITIES];
/* Need two extra slots for the ALL category and Edit Categories... */
static GtkWidget *todo_cat_menu_item1[NUM_TODO_CAT_ITEMS + 2];
static GtkWidget *todo_cat_menu_item2[NUM_TODO_CAT_ITEMS];
static GtkWidget *new_record_button;
static GtkWidget *apply_record_button;
static GtkWidget *add_record_button;
static GtkWidget *delete_record_button;
static GtkWidget *undelete_record_button;
static GtkWidget *copy_record_button;
static GtkWidget *cancel_record_button;
static GtkWidget *category_menu1;
static GtkWidget *category_menu2;
static GtkWidget *pane;
static GtkWidget *note_pane;

static ToDoList *glob_todo_list = NULL;
static ToDoList *export_todo_list = NULL;

static struct sorted_cats sort_l[NUM_TODO_CAT_ITEMS];

static struct ToDoAppInfo todo_app_info;
static int todo_category = CATEGORY_ALL;
static int clist_col_selected;
static int clist_row_selected;
static int record_changed;

/****************************** Prototypes ************************************/
static int todo_clear_details(void);

static int todo_clist_redraw(void);

static int todo_find(void);

static void cb_add_new_record(GtkWidget *widget, gpointer data);

static void connect_changed_signals(int con_or_dis);

void todo_update_liststore(GtkListStore *pListStore, GtkWidget *tooltip_widget,
                           ToDoList **todo_list, int category, int main);

enum {
    TODO_CHECK_COLUMN_ENUM = 0,
    TODO_PRIORITY_COLUMN_ENUM,
    TODO_NOTE_COLUMN_ENUM,
    TODO_DATE_COLUMN_ENUM,
    TODO_TEXT_COLUMN_ENUM,
    TODO_DATA_COLUMN_ENUM,
    TODO_NUM_COLS
};
/****************************** Main Code *************************************/
/* Called once on initialization of GUI */
static void init(void) {
    time_t ltime;
    struct tm *now;
    long ivalue;

    time(&ltime);
    now = localtime(&ltime);

    memcpy(&due_date, now, sizeof(struct tm));

    get_pref(PREF_TODO_DAYS_TILL_DUE, &ivalue, NULL);
    add_days_to_date(&due_date, ivalue);

    clist_row_selected = 0;
    clist_col_selected = 1;

    record_changed = CLEAR_FLAG;
}

static void update_due_button(GtkWidget *button, struct tm *t) {
    const char *short_date;
    char str[255];

    if (t) {
        get_pref(PREF_SHORTDATE, NULL, &short_date);
        strftime(str, sizeof(str), short_date, t);

        gtk_label_set_text(GTK_LABEL(GTK_BIN(button)->child), str);
    } else {
        gtk_label_set_text(GTK_LABEL(GTK_BIN(button)->child), _("No Date"));
    }
}

static void cb_cal_dialog(GtkWidget *widget,
                          gpointer data) {
    long fdow;
    int r = 0;
    struct tm t;
    GtkWidget *Pcheck_button;
    GtkWidget *Pbutton;

    Pcheck_button = todo_no_due_date_checkbox;
    memcpy(&t, &due_date, sizeof(t));
    Pbutton = due_date_button;

    get_pref(PREF_FDOW, &fdow, NULL);

    r = cal_dialog(GTK_WINDOW(gtk_widget_get_toplevel(widget)), _("Due Date"), fdow,
                   &(t.tm_mon),
                   &(t.tm_mday),
                   &(t.tm_year));

    if (r == CAL_DONE) {
        mktime(&t);
        memcpy(&due_date, &t, sizeof(due_date));
        if (Pcheck_button) {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(Pcheck_button), FALSE);
            /* The above call sets due_date forward by n days, so we correct it */
            memcpy(&due_date, &t, sizeof(due_date));
            update_due_button(Pbutton, &t);
        }
        if (Pbutton) {
            update_due_button(Pbutton, &t);
        }
    }
}


int todo_print(void) {
    long this_many;
    MyToDo *mtodo;
    ToDoList *todo_list;
    ToDoList todo_list1;

    get_pref(PREF_PRINT_THIS_MANY, &this_many, NULL);

    todo_list = NULL;
    if (this_many == 1) {
        mtodo = gtk_clist_get_row_data(GTK_CLIST(clist), clist_row_selected);
        if (mtodo < (MyToDo *) CLIST_MIN_DATA) {
            return EXIT_FAILURE;
        }
        memcpy(&(todo_list1.mtodo), mtodo, sizeof(MyToDo));
        todo_list1.next = NULL;
        todo_list = &todo_list1;
    }
    if (this_many == 2) {
        get_todos2(&todo_list, SORT_ASCENDING, 2, 2, 2, 2, todo_category);
    }
    if (this_many == 3) {
        get_todos2(&todo_list, SORT_ASCENDING, 2, 2, 2, 2, CATEGORY_ALL);
    }

    print_todos(todo_list, PN);

    if ((this_many == 2) || (this_many == 3)) {
        free_ToDoList(&todo_list);
    }

    return EXIT_SUCCESS;
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
        if (GTK_CLIST(clist)->rows > 0) {
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
    int i;
    static int connected = 0;

    /* CONNECT */
    if ((con_or_dis == CONNECT_SIGNALS) && (!connected)) {
        connected = 1;

        for (i = 0; i < NUM_TODO_CAT_ITEMS; i++) {
            if (todo_cat_menu_item2[i]) {
                gtk_signal_connect(GTK_OBJECT(todo_cat_menu_item2[i]), "toggled",
                                   GTK_SIGNAL_FUNC(cb_record_changed), NULL);
            }
        }
        for (i = 0; i < NUM_TODO_PRIORITIES; i++) {
            if (radio_button_todo[i]) {
                gtk_signal_connect(GTK_OBJECT(radio_button_todo[i]), "toggled",
                                   GTK_SIGNAL_FUNC(cb_record_changed), NULL);
            }
        }
        g_signal_connect(todo_desc_buffer, "changed",
                         GTK_SIGNAL_FUNC(cb_record_changed), NULL);
        g_signal_connect(todo_note_buffer, "changed",
                         GTK_SIGNAL_FUNC(cb_record_changed), NULL);

        gtk_signal_connect(GTK_OBJECT(todo_completed_checkbox), "toggled",
                           GTK_SIGNAL_FUNC(cb_record_changed), NULL);
        gtk_signal_connect(GTK_OBJECT(private_checkbox), "toggled",
                           GTK_SIGNAL_FUNC(cb_record_changed), NULL);
        gtk_signal_connect(GTK_OBJECT(todo_no_due_date_checkbox), "toggled",
                           GTK_SIGNAL_FUNC(cb_record_changed), NULL);
        gtk_signal_connect(GTK_OBJECT(due_date_button), "pressed",
                           GTK_SIGNAL_FUNC(cb_record_changed), NULL);
    }

    /* DISCONNECT */
    if ((con_or_dis == DISCONNECT_SIGNALS) && (connected)) {
        connected = 0;

        for (i = 0; i < NUM_TODO_CAT_ITEMS; i++) {
            if (todo_cat_menu_item2[i]) {
                gtk_signal_disconnect_by_func(GTK_OBJECT(todo_cat_menu_item2[i]),
                                              GTK_SIGNAL_FUNC(cb_record_changed), NULL);
            }
        }
        for (i = 0; i < NUM_TODO_PRIORITIES; i++) {
            gtk_signal_disconnect_by_func(GTK_OBJECT(radio_button_todo[i]),
                                          GTK_SIGNAL_FUNC(cb_record_changed), NULL);
        }
        g_signal_handlers_disconnect_by_func(todo_desc_buffer,
                                             GTK_SIGNAL_FUNC(cb_record_changed), NULL);
        g_signal_handlers_disconnect_by_func(todo_note_buffer,
                                             GTK_SIGNAL_FUNC(cb_record_changed), NULL);

        gtk_signal_disconnect_by_func(GTK_OBJECT(todo_completed_checkbox),
                                      GTK_SIGNAL_FUNC(cb_record_changed), NULL);
        gtk_signal_disconnect_by_func(GTK_OBJECT(private_checkbox),
                                      GTK_SIGNAL_FUNC(cb_record_changed), NULL);
        gtk_signal_disconnect_by_func(GTK_OBJECT(todo_no_due_date_checkbox),
                                      GTK_SIGNAL_FUNC(cb_record_changed), NULL);
        gtk_signal_disconnect_by_func(GTK_OBJECT(due_date_button),
                                      GTK_SIGNAL_FUNC(cb_record_changed), NULL);
    }
}


static int todo_to_text(struct ToDo *todo, char *text, int len) {
    char yes[] = "Yes";
    char no[] = "No";
    char empty[] = "";
    char *complete;
    char *description;
    char *note;
    char due[20];
    const char *short_date;

    if (todo->indefinite) {
        strcpy(due, "Never");
    } else {
        get_pref(PREF_SHORTDATE, NULL, &short_date);
        strftime(due, sizeof(due), short_date, &(todo->due));
    }
    complete = todo->complete ? yes : no;
    description = todo->description ? todo->description : empty;
    note = todo->note ? todo->note : empty;

    g_snprintf(text, len, "Due: %s\nPriority: %d\nComplete: %s\n\
Description: %s\nNote: %s\n", due, todo->priority, complete,
               description, note);
    return EXIT_SUCCESS;
}

/*
 * Start Import Code
 */
static int cb_todo_import(GtkWidget *parent_window,
                          const char *file_path, int type) {
    FILE *in;
    char text[65536];
    char description[65536];
    char note[65536];
    struct ToDo new_todo;
    unsigned char attrib;
    int i, ret, index;
    int import_all;
    ToDoList *todolist;
    ToDoList *temp_todolist;
    struct CategoryAppInfo cai;
    char old_cat_name[32];
    int suggested_cat_num;
    int new_cat_num;
    int priv, indefinite, priority, completed;
    int year, month, day;

    in = fopen(file_path, "r");
    if (!in) {
        jp_logf(JP_LOG_WARN, _("Unable to open file: %s\n"), file_path);
        return EXIT_FAILURE;
    }

    /* CSV */
    if (type == IMPORT_TYPE_CSV) {
        jp_logf(JP_LOG_DEBUG, "Todo import CSV [%s]\n", file_path);
        /* Get the first line containing the format and check for reasonableness */
        if (fgets(text, sizeof(text), in) == NULL) {
            jp_logf(JP_LOG_WARN, "fgets failed %s %d\n", __FILE__, __LINE__);
        }
        ret = verify_csv_header(text, NUM_TODO_CSV_FIELDS, file_path);
        if (EXIT_FAILURE == ret) return EXIT_FAILURE;

        import_all = FALSE;
        while (1) {
            /* Read the category field */
            ret = read_csv_field(in, text, sizeof(text));
            if (feof(in)) break;
#ifdef JPILOT_DEBUG
            printf("category is [%s]\n", text);
#endif
            g_strlcpy(old_cat_name, text, 16);
            attrib = 0;
            /* Figure out what the best category number is */
            suggested_cat_num = 0;
            for (i = 0; i < NUM_TODO_CAT_ITEMS; i++) {
                if (todo_app_info.category.name[i][0] == '\0') continue;
                if (!strcmp(todo_app_info.category.name[i], old_cat_name)) {
                    suggested_cat_num = i;
                    break;
                }
            }

            /* Read the private field */
            ret = read_csv_field(in, text, sizeof(text));
#ifdef JPILOT_DEBUG
            printf("private is [%s]\n", text);
#endif
            sscanf(text, "%d", &priv);

            /* Read the indefinite field */
            ret = read_csv_field(in, text, sizeof(text));
#ifdef JPILOT_DEBUG
            printf("indefinite is [%s]\n", text);
#endif
            sscanf(text, "%d", &indefinite);

            /* Read the Due Date field */
            ret = read_csv_field(in, text, sizeof(text));
#ifdef JPILOT_DEBUG
            printf("due date is [%s]\n", text);
#endif
            sscanf(text, "%d/%d/%d", &year, &month, &day);

            /* Read the Priority field */
            ret = read_csv_field(in, text, sizeof(text));
#ifdef JPILOT_DEBUG
            printf("priority is [%s]\n", text);
#endif
            sscanf(text, "%d", &priority);

            /* Read the Completed field */
            ret = read_csv_field(in, text, sizeof(text));
#ifdef JPILOT_DEBUG
            printf("completed is [%s]\n", text);
#endif
            sscanf(text, "%d", &completed);

            /* Read the Description field */
            ret = read_csv_field(in, description, sizeof(text));
#ifdef JPILOT_DEBUG
            printf("todo description [%s]\n", description);
#endif

            /* Read the Note field */
            ret = read_csv_field(in, note, sizeof(text));
#ifdef JPILOT_DEBUG
            printf("todo note [%s]\n", note);
#endif

            new_todo.indefinite = indefinite;
            memset(&(new_todo.due), 0, sizeof(new_todo.due));
            new_todo.due.tm_year = year - 1900;
            new_todo.due.tm_mon = month - 1;
            new_todo.due.tm_mday = day;
            new_todo.priority = priority;
            new_todo.complete = completed;
            new_todo.description = description;
            new_todo.note = note;

            todo_to_text(&new_todo, text, sizeof(text));
            if (!import_all) {
                ret = import_record_ask(parent_window, pane,
                                        text,
                                        &(todo_app_info.category),
                                        old_cat_name,
                                        priv,
                                        suggested_cat_num,
                                        &new_cat_num);
            } else {
                new_cat_num = suggested_cat_num;
            }
            if (ret == DIALOG_SAID_IMPORT_QUIT) break;
            if (ret == DIALOG_SAID_IMPORT_SKIP) continue;
            if (ret == DIALOG_SAID_IMPORT_ALL) import_all = TRUE;

            attrib = (new_cat_num & 0x0F) | (priv ? dlpRecAttrSecret : 0);
            if ((ret == DIALOG_SAID_IMPORT_YES) || (import_all)) {
                pc_todo_write(&new_todo, NEW_PC_REC, attrib, NULL);
            }
        }
    }

    /* Palm Desktop DAT format */
    if (type == IMPORT_TYPE_DAT) {
        jp_logf(JP_LOG_DEBUG, "Todo import DAT [%s]\n", file_path);
        if (dat_check_if_dat_file(in) != DAT_TODO_FILE) {
            jp_logf(JP_LOG_WARN, _("File doesn't appear to be todo.dat format\n"));
            fclose(in);
            return EXIT_FAILURE;
        }
        todolist = NULL;
        dat_get_todos(in, &todolist, &cai);
        import_all = FALSE;
        for (temp_todolist = todolist; temp_todolist; temp_todolist = temp_todolist->next) {
            index = temp_todolist->mtodo.unique_id - 1;
            if (index < 0) {
                g_strlcpy(old_cat_name, _("Unfiled"), 16);
                index = 0;
            } else {
                g_strlcpy(old_cat_name, cai.name[index], 16);
            }
            /* Figure out what category it was in the dat file */
            index = temp_todolist->mtodo.unique_id - 1;
            suggested_cat_num = 0;
            if (index > -1) {
                for (i = 0; i < NUM_TODO_CAT_ITEMS; i++) {
                    if (todo_app_info.category.name[i][0] == '\0') continue;
                    if (!strcmp(todo_app_info.category.name[i], old_cat_name)) {
                        suggested_cat_num = i;
                        break;
                    }
                }
            }

            ret = 0;
            todo_to_text(&(temp_todolist->mtodo.todo), text, sizeof(text));
            if (!import_all) {
                ret = import_record_ask(parent_window, pane,
                                        text,
                                        &(todo_app_info.category),
                                        old_cat_name,
                                        (temp_todolist->mtodo.attrib & 0x10),
                                        suggested_cat_num,
                                        &new_cat_num);
            } else {
                new_cat_num = suggested_cat_num;
            }
            if (ret == DIALOG_SAID_IMPORT_QUIT) break;
            if (ret == DIALOG_SAID_IMPORT_SKIP) continue;
            if (ret == DIALOG_SAID_IMPORT_ALL) import_all = TRUE;

            attrib = (new_cat_num & 0x0F) |
                     ((temp_todolist->mtodo.attrib & 0x10) ? dlpRecAttrSecret : 0);
            if ((ret == DIALOG_SAID_IMPORT_YES) || (import_all)) {
                pc_todo_write(&(temp_todolist->mtodo.todo), NEW_PC_REC,
                              attrib, NULL);
            }
        }
        free_ToDoList(&todolist);
    }

    todo_refresh();
    fclose(in);
    return EXIT_SUCCESS;
}

int todo_import(GtkWidget *window) {
    char *type_desc[] = {
            N_("CSV (Comma Separated Values)"),
            N_("DAT/TDA (Palm Archive Formats)"),
            NULL
    };
    int type_int[] = {
            IMPORT_TYPE_CSV,
            IMPORT_TYPE_DAT,
            0
    };

    /* Hide ABA import of TaskDB until file format has been decoded */
    /* FIXME: Uncomment when support for Tasks has been added
    if (todo_version==1) {
       type_desc[1] = NULL;
       type_int[1] = 0;
    }
    */

    import_gui(window, pane, type_desc, type_int, cb_todo_import);
    return EXIT_SUCCESS;
}
/*
 * End Import Code
 */

/*
 * Start Export code
 */

static void cb_todo_export_ok(GtkWidget *export_window, GtkWidget *clist,
                              int type, const char *filename) {
    MyToDo *mtodo;
    GList *list, *temp_list;
    FILE *out;
    struct stat statb;
    int i, r;
    const char *short_date;
    time_t ltime;
    struct tm *now = NULL;
    char *button_text[] = {N_("OK")};
    char *button_overwrite_text[] = {N_("No"), N_("Yes")};
    char text[1024];
    char date_string[1024];
    char str1[256], str2[256];
    char pref_time[40];
    char csv_text[65550];
    char *p;
    gchar *end;
    char username[256];
    char hostname[256];
    const char *svalue;
    long userid;
    long char_set;
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
        fprintf(out, _("ToDo exported from %s %s on %s\n\n"),
                PN, VERSION, date_string);
    }

    /* Write a header to the CSV file */
    if (type == EXPORT_TYPE_CSV) {
        fprintf(out,
                "CSV todo version "VERSION": Category, Private, Indefinite, Due Date, Priority, Completed, ToDo Text, Note\n");
    }

    /* Special setup for ICAL export */
    if (type == EXPORT_TYPE_ICALENDAR) {
        get_pref(PREF_CHAR_SET, &char_set, NULL);
        if (char_set < CHAR_SET_UTF) {
            jp_logf(JP_LOG_WARN, _("Host character encoding is not UTF-8 based.\n"
                                   " Exported ical file may not be standards-compliant\n"));
        }

        get_pref(PREF_USER, NULL, &svalue);
        /* Convert User Name stored in Palm character set */
        g_strlcpy(text, svalue, 128);
        text[127] = '\0';
        charset_p2j(text, 128, char_set);
        str_to_ical_str(username, sizeof(username), text);
        get_pref(PREF_USER_ID, &userid, NULL);
        gethostname(text, sizeof(hostname));
        text[sizeof(hostname) - 1] = '\0';
        str_to_ical_str(hostname, sizeof(hostname), text);
        time(&ltime);
        now = gmtime(&ltime);
    }

    get_pref(PREF_CHAR_SET, &char_set, NULL);
    list = GTK_CLIST(clist)->selection;

    for (i = 0, temp_list = list; temp_list; temp_list = temp_list->next, i++) {
        mtodo = gtk_clist_get_row_data(GTK_CLIST(clist), GPOINTER_TO_INT(temp_list->data));
        if (!mtodo) {
            continue;
            jp_logf(JP_LOG_WARN, _("Can't export todo %d\n"), (long) temp_list->data + 1);
        }
        switch (type) {
            case EXPORT_TYPE_CSV:
                utf = charset_p2newj(todo_app_info.category.name[mtodo->attrib & 0x0F], 16, char_set);
                str_to_csv_str(csv_text, utf);
                fprintf(out, "\"%s\",", csv_text);
                g_free(utf);
                fprintf(out, "\"%s\",", (mtodo->attrib & dlpRecAttrSecret) ? "1" : "0");
                fprintf(out, "\"%s\",", mtodo->todo.indefinite ? "1" : "0");
                if (mtodo->todo.indefinite) {
                    fprintf(out, "\"\",");
                } else {
                    strftime(text, sizeof(text), "%Y/%02m/%02d", &(mtodo->todo.due));
                    fprintf(out, "\"%s\",", text);
                }
                fprintf(out, "\"%d\",", mtodo->todo.priority);
                fprintf(out, "\"%s\",", mtodo->todo.complete ? "1" : "0");
                if (mtodo->todo.description) {
                    str_to_csv_str(csv_text, mtodo->todo.description);
                    fprintf(out, "\"%s\",", csv_text);
                } else {
                    fprintf(out, "\"\",");
                }
                if (mtodo->todo.note) {
                    str_to_csv_str(csv_text, mtodo->todo.note);
                    fprintf(out, "\"%s\"\n", csv_text);
                } else {
                    fprintf(out, "\"\",");
                }
                break;

            case EXPORT_TYPE_TEXT:
                utf = charset_p2newj(todo_app_info.category.name[mtodo->attrib & 0x0F], 16, char_set);
                fprintf(out, _("Category: %s\n"), utf);
                g_free(utf);

                fprintf(out, _("Private: %s\n"),
                        (mtodo->attrib & dlpRecAttrSecret) ? _("Yes") : _("No"));
                if (mtodo->todo.indefinite) {
                    fprintf(out, _("Due Date: None\n"));
                } else {
                    strftime(text, sizeof(text), short_date, &(mtodo->todo.due));
                    fprintf(out, _("Due Date: %s\n"), text);
                }
                fprintf(out, _("Priority: %d\n"), mtodo->todo.priority);
                fprintf(out, _("Completed: %s\n"), mtodo->todo.complete ? _("Yes") : _("No"));
                if (mtodo->todo.description) {
                    fprintf(out, _("Description: %s\n"), mtodo->todo.description);
                }
                if (mtodo->todo.note) {
                    fprintf(out, _("Note: %s\n\n"), mtodo->todo.note);
                }
                break;

            case EXPORT_TYPE_ICALENDAR:
                /* RFC 2445: Internet Calendaring and Scheduling Core
                 *           Object Specification */
                if (i == 0) {
                    fprintf(out, "BEGIN:VCALENDAR"CRLF);
                    fprintf(out, "VERSION:2.0"CRLF);
                    fprintf(out, "PRODID:%s"CRLF, FPI_STRING);
                }
                fprintf(out, "BEGIN:VTODO"CRLF);
                if (mtodo->attrib & dlpRecAttrSecret) {
                    fprintf(out, "CLASS:PRIVATE"CRLF);
                }
                fprintf(out, "UID:palm-todo-%08x-%08lx-%s@%s"CRLF,
                        mtodo->unique_id, userid, username, hostname);
                fprintf(out, "DTSTAMP:%04d%02d%02dT%02d%02d%02dZ"CRLF,
                        now->tm_year + 1900,
                        now->tm_mon + 1,
                        now->tm_mday,
                        now->tm_hour,
                        now->tm_min,
                        now->tm_sec);
                str_to_ical_str(text, sizeof(text),
                                todo_app_info.category.name[mtodo->attrib & 0x0F]);
                fprintf(out, "CATEGORIES:%s"CRLF, text);
                if (mtodo->todo.description) {
                    g_strlcpy(str1, mtodo->todo.description, 51);
                    /* truncate the string on a UTF-8 character boundary */
                    if (char_set > CHAR_SET_UTF) {
                        if (!g_utf8_validate(str1, -1, (const gchar **) &end))
                            *end = 0;
                    }
                } else {
                    /* Handle pathological case with null description. */
                    str1[0] = '\0';
                }
                if ((p = strchr(str1, '\n'))) {
                    *p = '\0';
                }
                str_to_ical_str(text, sizeof(text), str1);
                fprintf(out, "SUMMARY:%s%s"CRLF, text,
                        strlen(str1) > 49 ? "..." : "");
                str_to_ical_str(text, sizeof(text), mtodo->todo.description);
                fprintf(out, "DESCRIPTION:%s", text);
                if (mtodo->todo.note && mtodo->todo.note[0]) {
                    str_to_ical_str(text, sizeof(text), mtodo->todo.note);
                    fprintf(out, "\\n"CRLF" %s"CRLF, text);
                } else {
                    fprintf(out, ""CRLF);
                }
                fprintf(out, "STATUS:%s"CRLF, mtodo->todo.complete ? "COMPLETED" : "NEEDS-ACTION");
                fprintf(out, "PRIORITY:%d"CRLF, mtodo->todo.priority);
                if (!mtodo->todo.indefinite) {
                    fprintf(out, "DUE;VALUE=DATE:%04d%02d%02d"CRLF,
                            mtodo->todo.due.tm_year + 1900,
                            mtodo->todo.due.tm_mon + 1,
                            mtodo->todo.due.tm_mday);
                }
                fprintf(out, "END:VTODO"CRLF);
                if (temp_list->next == NULL) {
                    fprintf(out, "END:VCALENDAR"CRLF);
                }
                break;
            default:
                jp_logf(JP_LOG_WARN, _("Unknown export type\n"));
        }
    }

    if (out) {
        fclose(out);
    }
}


static void cb_todo_update_clist(GtkWidget *clist, int category) {
    todo_update_clist(clist, NULL, &export_todo_list, category, FALSE);
}


static void cb_todo_export_done(GtkWidget *widget, const char *filename) {
    free_ToDoList(&export_todo_list);

    set_pref(PREF_TODO_EXPORT_FILENAME, 0, filename, TRUE);
}

int todo_export(GtkWidget *window) {
    int w, h, x, y;
    char *type_text[] = {N_("Text"),
                         N_("CSV"),
                         N_("iCalendar"),
                         NULL};
    int type_int[] = {EXPORT_TYPE_TEXT, EXPORT_TYPE_CSV, EXPORT_TYPE_ICALENDAR};

    gdk_window_get_size(window->window, &w, &h);
    gdk_window_get_root_origin(window->window, &x, &y);

    w = gtk_paned_get_position(GTK_PANED(pane));
    x += 40;

    export_gui(window,
               w, h, x, y, 5, sort_l,
               PREF_TODO_EXPORT_FILENAME,
               type_text,
               type_int,
               cb_todo_update_clist,
               cb_todo_export_done,
               cb_todo_export_ok
    );

    return EXIT_SUCCESS;
}

/*
 * End Export Code
 */


/* Find position of category in sorted category array 
 * via its assigned category number */
static int find_sort_cat_pos(int cat) {
    int i;

    for (i = 0; i < NUM_TODO_CAT_ITEMS; i++) {
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

    if (cat != NUM_TODO_CAT_ITEMS - 1) {
        return cat;
    } else { /* Unfiled category */
        /* Count how many category entries are filled */
        for (i = 0; i < NUM_TODO_CAT_ITEMS; i++) {
            if (!sort_l[i].Pcat[0]) {
                return i;
            }
        }
        return 0;
    }
}

static void cb_delete_todo(GtkWidget *widget,
                           gpointer data) {
    MyToDo *mtodo;
    int flag;
    int show_priv;
    long char_set;

    mtodo = gtk_clist_get_row_data(GTK_CLIST(clist), clist_row_selected);
    if (mtodo < (MyToDo *) CLIST_MIN_DATA) {
        return;
    }

    /* Convert to Palm character set */
    get_pref(PREF_CHAR_SET, &char_set, NULL);
    if (char_set != CHAR_SET_LATIN1) {
        if (mtodo->todo.description)
            charset_j2p(mtodo->todo.description, strlen(mtodo->todo.description) + 1, char_set);
        if (mtodo->todo.note)
            charset_j2p(mtodo->todo.note, strlen(mtodo->todo.note) + 1, char_set);
    }

    /* Do masking like Palm OS 3.5 */
    show_priv = show_privates(GET_PRIVATES);
    if ((show_priv != SHOW_PRIVATES) &&
        (mtodo->attrib & dlpRecAttrSecret)) {
        return;
    }
    /* End Masking */
    flag = GPOINTER_TO_INT(data);
    if ((flag == MODIFY_FLAG) || (flag == DELETE_FLAG)) {
        jp_logf(JP_LOG_DEBUG, "calling delete_pc_record\n");
        delete_pc_record(TODO, mtodo, flag);
        if (flag == DELETE_FLAG) {
            /* when we redraw we want to go to the line above the deleted one */
            if (clist_row_selected > 0) {
                clist_row_selected--;
            }
        }
    }

    if (flag == DELETE_FLAG) {
        todo_clist_redraw();
    }
}

static void cb_undelete_todo(GtkWidget *widget,
                             gpointer data) {
    MyToDo *mtodo;
    int flag;
    int show_priv;

    mtodo = gtk_clist_get_row_data(GTK_CLIST(clist), clist_row_selected);
    if (mtodo < (MyToDo *) CLIST_MIN_DATA) {
        return;
    }

    /* Do masking like Palm OS 3.5 */
    show_priv = show_privates(GET_PRIVATES);
    if ((show_priv != SHOW_PRIVATES) &&
        (mtodo->attrib & dlpRecAttrSecret)) {
        return;
    }
    /* End Masking */

    jp_logf(JP_LOG_DEBUG, "mtodo->unique_id = %d\n", mtodo->unique_id);
    jp_logf(JP_LOG_DEBUG, "mtodo->rt = %d\n", mtodo->rt);

    flag = GPOINTER_TO_INT(data);
    if (flag == UNDELETE_FLAG) {
        if (mtodo->rt == DELETED_PALM_REC ||
            mtodo->rt == DELETED_PC_REC) {
            undelete_pc_record(TODO, mtodo, flag);
        }
        /* Possible later addition of undelete for modified records
        else if (mtodo->rt == MODIFIED_PALM_REC)
        {
           cb_add_new_record(widget, GINT_TO_POINTER(COPY_FLAG));
        }
        */
    }

    todo_clist_redraw();
}

static void cb_cancel(GtkWidget *widget, gpointer data) {
    set_new_button_to(CLEAR_FLAG);
    todo_refresh();
}

static void cb_edit_cats(GtkWidget *widget, gpointer data) {
    struct ToDoAppInfo ai;
    char db_name[FILENAME_MAX];
    char pdb_name[FILENAME_MAX];
    char full_name[FILENAME_MAX];
    unsigned char buffer[65536];
    int num;
    size_t size;
    void *buf;
    struct pi_file *pf;
#ifdef ENABLE_MANANA
    long ivalue;
#endif

    jp_logf(JP_LOG_DEBUG, "cb_edit_cats\n");

#ifdef ENABLE_MANANA
    get_pref(PREF_MANANA_MODE, &ivalue, NULL);
    if (ivalue) {
        strcpy(pdb_name, "MananaDB.pdb");
        strcpy(db_name, "MananaDB");
    } else {
        strcpy(pdb_name, "ToDoDB.pdb");
        strcpy(db_name, "ToDoDB");
    }
#else
    strcpy(pdb_name, "ToDoDB.pdb");
    strcpy(db_name, "ToDoDB");
#endif

    get_home_file_name(pdb_name, full_name, sizeof(full_name));

    buf = NULL;
    memset(&ai, 0, sizeof(ai));

    pf = pi_file_open(full_name);
    pi_file_get_app_info(pf, &buf, &size);

    num = unpack_ToDoAppInfo(&ai, buf, size);
    if (num <= 0) {
        jp_logf(JP_LOG_WARN, _("Error reading file: %s\n"), pdb_name);
        return;
    }

    pi_file_close(pf);

    edit_cats(widget, db_name, &(ai.category));

    size = pack_ToDoAppInfo(&ai, buffer, sizeof(buffer));

    pdb_file_write_app_block(db_name, buffer, size);

    cb_app_button(NULL, GINT_TO_POINTER(REDRAW));
}

static void cb_category(GtkWidget *item, int selection) {
    int b;

    if ((GTK_CHECK_MENU_ITEM(item))->active) {
        if (todo_category == selection) { return; }

        b = dialog_save_changed_record_with_cancel(pane, record_changed);
        if (b == DIALOG_SAID_1) { /* Cancel */
            int index, index2;

            if (todo_category == CATEGORY_ALL) {
                index = 0;
                index2 = 0;
            } else {
                index = find_sort_cat_pos(todo_category);
                index2 = find_menu_cat_pos(index) + 1;
                index += 1;
            }

            if (index < 0) {
                jp_logf(JP_LOG_WARN, _("Category is not legal\n"));
            } else {
                gtk_check_menu_item_set_active
                        (GTK_CHECK_MENU_ITEM(todo_cat_menu_item1[index]), TRUE);
                gtk_option_menu_set_history(GTK_OPTION_MENU(category_menu1), index2);
            }

            return;
        }
        if (b == DIALOG_SAID_3) { /* Save */
            cb_add_new_record(NULL, GINT_TO_POINTER(record_changed));
        }

        if (selection == NUM_TODO_CAT_ITEMS + 1) {
            cb_edit_cats(item, NULL);
        } else {
            todo_category = selection;
        }
        clist_row_selected = 0;
        jp_logf(JP_LOG_DEBUG, "todo_category = %d\n", todo_category);
        todo_update_clist(clist, category_menu1, &glob_todo_list, todo_category, TRUE);
    }
}

static void cb_check_button_no_due_date(GtkWidget *widget, gpointer data) {
    long till_due;
    struct tm *now;
    time_t ltime;

    if (GTK_TOGGLE_BUTTON(widget)->active) {
        update_due_button(due_date_button, NULL);
    } else {
        time(&ltime);
        now = localtime(&ltime);
        memcpy(&due_date, now, sizeof(struct tm));

        get_pref(PREF_TODO_DAYS_TILL_DUE, &till_due, NULL);
        add_days_to_date(&due_date, till_due);

        update_due_button(due_date_button, &due_date);
    }
}

static int todo_clear_details(void) {
    time_t ltime;
    struct tm *now;
    int new_cat;
    int sorted_position;
    long default_due, till_due;

    time(&ltime);
    now = localtime(&ltime);

    /* Need to disconnect these signals first */
    connect_changed_signals(DISCONNECT_SIGNALS);

    gtk_widget_freeze_child_notify(todo_desc);
    gtk_widget_freeze_child_notify(todo_note);

    gtk_text_buffer_set_text(GTK_TEXT_BUFFER(todo_desc_buffer), "", -1);
    gtk_text_buffer_set_text(GTK_TEXT_BUFFER(todo_note_buffer), "", -1);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_button_todo[0]), TRUE);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(todo_completed_checkbox), FALSE);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(private_checkbox), FALSE);

    get_pref(PREF_TODO_DAYS_DUE, &default_due, NULL);
    get_pref(PREF_TODO_DAYS_TILL_DUE, &till_due, NULL);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(todo_no_due_date_checkbox),
                                 !default_due);

    memcpy(&due_date, now, sizeof(struct tm));

    if (default_due) {
        add_days_to_date(&due_date, till_due);
        update_due_button(due_date_button, &due_date);
    } else {
        update_due_button(due_date_button, NULL);
    }

    gtk_widget_thaw_child_notify(todo_desc);
    gtk_widget_thaw_child_notify(todo_note);

    if (todo_category == CATEGORY_ALL) {
        new_cat = 0;
    } else {
        new_cat = todo_category;
    }
    sorted_position = find_sort_cat_pos(new_cat);
    if (sorted_position < 0) {
        jp_logf(JP_LOG_WARN, _("Category is not legal\n"));
    } else {
        gtk_check_menu_item_set_active
                (GTK_CHECK_MENU_ITEM(todo_cat_menu_item2[sorted_position]), TRUE);
        gtk_option_menu_set_history(GTK_OPTION_MENU(category_menu2),
                                    find_menu_cat_pos(sorted_position));
    }

    set_new_button_to(CLEAR_FLAG);
    connect_changed_signals(CONNECT_SIGNALS);

    return EXIT_SUCCESS;
}

static int todo_get_details(struct ToDo *new_todo, unsigned char *attrib) {
    int i;
    GtkTextIter start_iter;
    GtkTextIter end_iter;

    new_todo->indefinite = (GTK_TOGGLE_BUTTON(todo_no_due_date_checkbox)->active);
    if (!(new_todo->indefinite)) {
        new_todo->due.tm_mon = due_date.tm_mon;
        new_todo->due.tm_mday = due_date.tm_mday;
        new_todo->due.tm_year = due_date.tm_year;
        jp_logf(JP_LOG_DEBUG, "todo_get_details: setting due date=%d/%d/%d\n", new_todo->due.tm_mon,
                new_todo->due.tm_mday, new_todo->due.tm_year);
    } else {
        memset(&(new_todo->due), 0, sizeof(new_todo->due));
    }
    new_todo->priority = 1;
    for (i = 0; i < NUM_TODO_PRIORITIES; i++) {
        if (GTK_TOGGLE_BUTTON(radio_button_todo[i])->active) {
            new_todo->priority = i + 1;
            break;
        }
    }
    new_todo->complete = (GTK_TOGGLE_BUTTON(todo_completed_checkbox)->active);
    /* Can there be an entry with no description? */
    /* Yes, but the Palm Pilot gui doesn't allow it to be entered on the Palm, */
    /* it will show it though.  I allow it. */
    gtk_text_buffer_get_bounds(GTK_TEXT_BUFFER(todo_desc_buffer),
                               &start_iter, &end_iter);
    new_todo->description = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(todo_desc_buffer),
                                                     &start_iter, &end_iter, TRUE);
    gtk_text_buffer_get_bounds(GTK_TEXT_BUFFER(todo_note_buffer),
                               &start_iter, &end_iter);
    new_todo->note = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(todo_note_buffer),
                                              &start_iter, &end_iter, TRUE);
    if (new_todo->note[0] == '\0') {
        free(new_todo->note);
        new_todo->note = NULL;
    }

    for (i = 0; i < NUM_TODO_CAT_ITEMS; i++) {
        if (GTK_IS_WIDGET(todo_cat_menu_item2[i])) {
            if (GTK_CHECK_MENU_ITEM(todo_cat_menu_item2[i])->active) {
                *attrib = sort_l[i].cat_num;
                break;
            }
        }
    }
    if (GTK_TOGGLE_BUTTON(private_checkbox)->active) {
        *attrib |= dlpRecAttrSecret;
    }

#ifdef JPILOT_DEBUG
    jp_logf(JP_LOG_DEBUG, "attrib = %d\n", *attrib);
    jp_logf(JP_LOG_DEBUG, "indefinite=%d\n",new_todo->indefinite);
    if (!new_todo->indefinite) {
       jp_logf(JP_LOG_DEBUG, "due: %d/%d/%d\n",new_todo->due.tm_mon,
               new_todo->due.tm_mday,
               new_todo->due.tm_year);
    }
    jp_logf(JP_LOG_DEBUG, "priority=%d\n",new_todo->priority);
    jp_logf(JP_LOG_DEBUG, "complete=%d\n",new_todo->complete);
    jp_logf(JP_LOG_DEBUG, "description=[%s]\n",new_todo->description);
    jp_logf(JP_LOG_DEBUG, "note=[%s]\n",new_todo->note);
#endif

    return EXIT_SUCCESS;
}

static void cb_add_new_record(GtkWidget *widget, gpointer data) {
    MyToDo *mtodo;
    struct ToDo new_todo;
    unsigned char attrib = 0;
    int flag;
    int show_priv;
    unsigned int unique_id;

    flag = GPOINTER_TO_INT(data);
    unique_id = 0;
    mtodo = NULL;

    /* Do masking like Palm OS 3.5 */
    if ((flag == COPY_FLAG) || (flag == MODIFY_FLAG)) {
        show_priv = show_privates(GET_PRIVATES);
        mtodo = gtk_clist_get_row_data(GTK_CLIST(clist), clist_row_selected);
        if (mtodo < (MyToDo *) CLIST_MIN_DATA) {
            return;
        }
        if ((show_priv != SHOW_PRIVATES) &&
            (mtodo->attrib & dlpRecAttrSecret)) {
            return;
        }
    }
    /* End Masking */
    if (flag == CLEAR_FLAG) {
        /* Clear button was hit */
        todo_clear_details();
        connect_changed_signals(DISCONNECT_SIGNALS);
        set_new_button_to(NEW_FLAG);
        gtk_widget_grab_focus(GTK_WIDGET(todo_desc));
        return;
    }
    if ((flag != NEW_FLAG) && (flag != MODIFY_FLAG) && (flag != COPY_FLAG)) {
        return;
    }
    if (flag == MODIFY_FLAG) {
        mtodo = gtk_clist_get_row_data(GTK_CLIST(clist), clist_row_selected);
        unique_id = mtodo->unique_id;
        if (mtodo < (MyToDo *) CLIST_MIN_DATA) {
            return;
        }
        if ((mtodo->rt == DELETED_PALM_REC) ||
            (mtodo->rt == DELETED_PC_REC) ||
            (mtodo->rt == MODIFIED_PALM_REC)) {
            jp_logf(JP_LOG_INFO, _("You can't modify a record that is deleted\n"));
            return;
        }
    }
    todo_get_details(&new_todo, &attrib);

    set_new_button_to(CLEAR_FLAG);

    if (flag == MODIFY_FLAG) {
        cb_delete_todo(NULL, data);
        if ((mtodo->rt == PALM_REC) || (mtodo->rt == REPLACEMENT_PALM_REC)) {
            pc_todo_write(&new_todo, REPLACEMENT_PALM_REC, attrib, &unique_id);
        } else {
            unique_id = 0;
            pc_todo_write(&new_todo, NEW_PC_REC, attrib, &unique_id);
        }
    } else {
        unique_id = 0;
        pc_todo_write(&new_todo, NEW_PC_REC, attrib, &unique_id);
    }
    free_ToDo(&new_todo);

    /* Don't return to modified record if search gui active */
    if (!glob_find_id) {
        glob_find_id = unique_id;
    }
    todo_clist_redraw();

    return;
}

/* Do masking like Palm OS 3.5 */
static void clear_mytodos(MyToDo *mtodo) {
    mtodo->unique_id = 0;
    mtodo->attrib = mtodo->attrib & 0xF8;
    mtodo->todo.complete = 0;
    mtodo->todo.priority = 1;
    mtodo->todo.indefinite = 1;
    if (mtodo->todo.description) {
        free(mtodo->todo.description);
        mtodo->todo.description = strdup("");
    }
    if (mtodo->todo.note) {
        free(mtodo->todo.note);
        mtodo->todo.note = strdup("");
    }

    return;
}
/* End Masking */


/* Function is used to sort clist based on the completed checkbox */
static gint GtkClistCompareCheckbox(GtkCList *clist,
                                    gconstpointer ptr1,
                                    gconstpointer ptr2) {
    GtkCListRow *row1;
    GtkCListRow *row2;
    MyToDo *mtodo1;
    MyToDo *mtodo2;
    struct ToDo *todo1;
    struct ToDo *todo2;

    row1 = (GtkCListRow *) ptr1;
    row2 = (GtkCListRow *) ptr2;

    mtodo1 = row1->data;
    mtodo2 = row2->data;

    todo1 = &(mtodo1->todo);
    todo2 = &(mtodo2->todo);

    if (todo1->complete && !todo2->complete) {
        return -1;
    } else if (todo2->complete && !todo1->complete) {
        return 1;
    } else {
        return 0;
    }

}


static gint GtkTreeColumnCompare(GtkTreeModel *model,
                                 GtkTreeIter *left,
                                 GtkTreeIter *right,
                                 gpointer columnId) {
    gint sortcol = GPOINTER_TO_INT(columnId);
    gint ret = 0;
    clist_col_selected = sortcol;
    switch (sortcol) {

        case TODO_NOTE_COLUMN_ENUM: {
            GdkPixbuf *note1, *note2;

            gtk_tree_model_get(model, left, TODO_NOTE_COLUMN_ENUM, &note1, -1);
            gtk_tree_model_get(model, right, TODO_NOTE_COLUMN_ENUM, &note2, -1);

            if (note1 == NULL && note2 == NULL) {
                ret = 0;
            } else if (note1 == NULL && note2 != NULL) {
                ret = -1;
            } else if (note1 != NULL && note2 == NULL) {
                ret = 1;
            } else {
                ret = 0;
            }
        }
            break;
        case TODO_CHECK_COLUMN_ENUM: {
            gboolean *name1, *name2;

            gtk_tree_model_get(model, left, TODO_CHECK_COLUMN_ENUM, &name1, -1);
            gtk_tree_model_get(model, right, TODO_CHECK_COLUMN_ENUM, &name2, -1);
            if(!name1 && name2){
                ret = 1;
            }else if(name1 && !name2){
                ret = -1;
            }else {
                ret = 0;
            }


        }
            break;
    }
    return ret;

}

/* Function is used to sort clist based on the Due Date field */
static gint GtkClistCompareDates(GtkCList *clist,
                                 gconstpointer ptr1,
                                 gconstpointer ptr2) {
    GtkCListRow *row1, *row2;
    MyToDo *mtodo1, *mtodo2;
    struct ToDo *todo1, *todo2;
    time_t time1, time2;

    row1 = (GtkCListRow *) ptr1;
    row2 = (GtkCListRow *) ptr2;

    mtodo1 = row1->data;
    mtodo2 = row2->data;

    todo1 = &(mtodo1->todo);
    todo2 = &(mtodo2->todo);

    if (!(todo1->indefinite) && (todo2->indefinite)) {
        return -1;
    }
    if ((todo1->indefinite) && !(todo2->indefinite)) {
        return 1;
    }

    /* Both todos have due dates which requires further comparison */
    time1 = mktime(&(todo1->due));
    time2 = mktime(&(todo2->due));

    return (time1 - time2);
}

static void column_clicked_cb(GtkTreeViewColumn *column) {
    clist_col_selected = column->sort_column_id;

}

static void cb_clist_click_column(GtkWidget *clist, int column) {
    MyToDo *mtodo;

    /* Remember currently selected item and return to it after sort
     * This is critically important because sorting without updating the
     * global variable clist_row_selected can cause data loss */
    mtodo = gtk_clist_get_row_data(GTK_CLIST(clist), clist_row_selected);
    if (mtodo < (MyToDo *) CLIST_MIN_DATA) {
        glob_find_id = 0;
    } else {
        glob_find_id = mtodo->unique_id;
    }

    /* Clicking on same column toggles ascending/descending sort */
    if (clist_col_selected == column) {
        if (GTK_CLIST(clist)->sort_type == GTK_SORT_ASCENDING) {
            gtk_clist_set_sort_type(GTK_CLIST (clist), GTK_SORT_DESCENDING);
        } else {
            gtk_clist_set_sort_type(GTK_CLIST (clist), GTK_SORT_ASCENDING);
        }
    } else /* Always sort in ascending order when changing sort column */
    {
        gtk_clist_set_sort_type(GTK_CLIST (clist), GTK_SORT_ASCENDING);
    }

    clist_col_selected = column;

    gtk_clist_set_sort_column(GTK_CLIST(clist), column);
    switch (column) {
        case TODO_CHECK_COLUMN: /* Checkbox column */
            gtk_clist_set_compare_func(GTK_CLIST(clist), GtkClistCompareCheckbox);
            break;
        case TODO_DATE_COLUMN:  /* Due Date column */
            gtk_clist_set_compare_func(GTK_CLIST(clist), GtkClistCompareDates);
            break;
        default: /* All other columns can use GTK default sort function */
            gtk_clist_set_compare_func(GTK_CLIST(clist), NULL);
            break;
    }
    gtk_clist_sort(GTK_CLIST (clist));

    /* Return to previously selected item */
    todo_find();
}

static gboolean
tree_view_get_cell_from_pos(GtkTreeView *view, guint x, guint y, GtkCellRenderer **cell)
{
    GtkTreeViewColumn *col = NULL;
    GList             *node, *columns, *cells;
    guint              colx = 0;

    g_return_val_if_fail ( view != NULL, FALSE );
    g_return_val_if_fail ( cell != NULL, FALSE );

    /* (1) find column and column x relative to tree view coordinates */

    columns = gtk_tree_view_get_columns(view);

    for (node = columns;  node != NULL && col == NULL;  node = node->next)
    {
        GtkTreeViewColumn *checkcol = (GtkTreeViewColumn*) node->data;

        if (x >= colx  &&  x < (colx + checkcol->width))
            col = checkcol;
        else
            colx += checkcol->width;
    }

    g_list_free(columns);

    if (col == NULL)
        return FALSE; /* not found */

    /* (2) find the cell renderer within the column */

    cells = gtk_tree_view_column_get_cell_renderers(col);

    for (node = cells;  node != NULL;  node = node->next)
    {
        GtkCellRenderer *checkcell = (GtkCellRenderer*) node->data;
        guint            width = 0, height = 0;

        /* Will this work for all packing modes? doesn't that
         *  return a random width depending on the last content
         * rendered? */
        gtk_cell_renderer_get_size(checkcell, GTK_WIDGET(view), NULL, NULL, NULL, &width, NULL);

        if (x >= colx && x < (colx + width))
        {
            *cell = checkcell;
            g_list_free(cells);
            return TRUE;
        }

        colx += width;
    }

    g_list_free(cells);
    return FALSE; /* not found */
}


static void populateChecks(GtkTreeViewColumn *col,
                           GtkCellRendererToggle   *renderer,
                           GtkTreeModel      *model,
                           GtkTreeIter       *iter,
                           gpointer           user_data) {
    gboolean isChecked;
   // GtkCellRendererToggle *toggle = (GtkCellRendererToggle)renderer;

    gtk_tree_model_get(model, iter, TODO_CHECK_COLUMN_ENUM, &isChecked, -1);
    renderer ->active = isChecked;

}



static void checkedCallBack(GtkCellRendererToggle* renderer, gchar* pathStr, gpointer data)
{
    GtkTreeIter iter;
    gboolean enabled;
    GtkTreePath* path = gtk_tree_path_new_from_string(pathStr);
    gtk_tree_model_get_iter(GTK_TREE_MODEL (data), &iter, path);
    gtk_tree_model_get(GTK_TREE_MODEL (data), &iter, TODO_CHECK_COLUMN_ENUM, &enabled, -1);
    enabled = !enabled;
    //gtk_list_store_set_value(GTK_LIST_STORE(data),&iter,TODO_CHECK_COLUMN_ENUM,&enabled);
    //gtk_list_store_set(store, &iter, LIST_ITEM, str, DUMMY_ITEM,po, -1);
    gtk_list_store_set(GTK_LIST_STORE (data), &iter, TODO_CHECK_COLUMN_ENUM, &enabled,-1);
}


static gboolean handleRowSelection(GtkTreeSelection *selection,
                                   GtkTreeModel *model,
                                   GtkTreePath *path,
                                   gboolean path_currently_selected,
                                   gpointer userdata) {
    GtkTreeIter iter;

    struct ToDo *todo;
    MyToDo *mtodo;
    int b;
    int index, sorted_position;
    unsigned int unique_id = 0;
    time_t ltime;
    struct tm *now;

    if ((gtk_tree_model_get_iter(model, &iter, path)) && (!path_currently_selected)) {

      int * i = gtk_tree_path_get_indices ( path ) ;
        g_print("%d %d is going to be selected.\n", i[0],userdata);
        gtk_tree_model_get(model, &iter, TODO_DATA_COLUMN_ENUM, &mtodo, -1);
        g_print("%s is going to be selected.\n", mtodo->todo.description);
        if ((record_changed == MODIFY_FLAG) || (record_changed == NEW_FLAG)) {
            if (mtodo != NULL) {
                unique_id = mtodo->unique_id;
            }
            b = dialog_save_changed_record_with_cancel(pane, record_changed);
            if (b == DIALOG_SAID_1) { /* Cancel */
                /** if (clist_row_selected >=0)
                 {
                     clist_select_row(GTK_CLIST(clist), clist_row_selected, 0);
                 } else {
                     clist_row_selected = 0;
                     clist_select_row(GTK_CLIST(clist), 0, 0);
                 } */
                return TRUE;
            }
            if (b == DIALOG_SAID_3) { /* Save */
                cb_add_new_record(NULL, GINT_TO_POINTER(record_changed));
            }
            set_new_button_to(CLEAR_FLAG);

            if (unique_id) {
                glob_find_id = unique_id;
                todo_find();
            } else {
                // clist_select_row(GTK_CLIST(clist), row, column);
            }
            return TRUE;
        }
        time(&ltime);
        now = localtime(&ltime);
        if (mtodo == NULL) {
            return TRUE;
        }
        if (mtodo->rt == DELETED_PALM_REC ||
            (mtodo->rt == DELETED_PC_REC))
            /* Possible later addition of undelete code for modified deleted records
               || mtodo->rt == MODIFIED_PALM_REC
            */
        {
            set_new_button_to(UNDELETE_FLAG);
        } else {
            set_new_button_to(CLEAR_FLAG);
        }
        connect_changed_signals(DISCONNECT_SIGNALS);
        //Not sure this is really needed as about 10 lines up does the same check.
        if (mtodo == NULL) {
            return TRUE;
        }
        todo = &(mtodo->todo);
        gtk_widget_freeze_child_notify(todo_desc);
        gtk_widget_freeze_child_notify(todo_note);
        gtk_text_buffer_set_text(GTK_TEXT_BUFFER(todo_desc_buffer), "", -1);
        gtk_text_buffer_set_text(GTK_TEXT_BUFFER(todo_note_buffer), "", -1);
        index = mtodo->attrib & 0x0F;
        sorted_position = find_sort_cat_pos(index);
        if (todo_cat_menu_item2[sorted_position] == NULL) {
            /* Illegal category */
            jp_logf(JP_LOG_DEBUG, "Category is not legal\n");
            index = sorted_position = 0;
            sorted_position = find_sort_cat_pos(index);
        }
        if (sorted_position < 0) {
            jp_logf(JP_LOG_WARN, _("Category is not legal\n"));
        } else {
            gtk_check_menu_item_set_active
                    (GTK_CHECK_MENU_ITEM(todo_cat_menu_item2[sorted_position]), TRUE);
        }
        gtk_option_menu_set_history(GTK_OPTION_MENU(category_menu2),
                                    find_menu_cat_pos(sorted_position));

        if (todo->description) {
            if (todo->description[0]) {
                gtk_text_buffer_set_text(GTK_TEXT_BUFFER(todo_desc_buffer), todo->description, -1);
            }
        }

        if (todo->note) {
            if (todo->note[0]) {
                gtk_text_buffer_set_text(GTK_TEXT_BUFFER(todo_note_buffer), todo->note, -1);
            }
        }

        if ((todo->priority < 1) || (todo->priority > 5)) {
            jp_logf(JP_LOG_WARN, _("Priority out of range\n"));
        } else {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_button_todo[todo->priority - 1]), TRUE);
        }

        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(todo_completed_checkbox), todo->complete);

        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(private_checkbox),
                                     mtodo->attrib & dlpRecAttrSecret);

        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(todo_no_due_date_checkbox),
                                     todo->indefinite);
        if (!todo->indefinite) {
            update_due_button(due_date_button, &(todo->due));
            due_date.tm_mon = todo->due.tm_mon;
            due_date.tm_mday = todo->due.tm_mday;
            due_date.tm_year = todo->due.tm_year;
        } else {
            update_due_button(due_date_button, NULL);
            due_date.tm_mon = now->tm_mon;
            due_date.tm_mday = now->tm_mday;
            due_date.tm_year = now->tm_year;
        }

        gtk_widget_thaw_child_notify(todo_desc);
        gtk_widget_thaw_child_notify(todo_note);
        /* If they have clicked on the checkmark box then do a modify */
       /* if (column == 0) {
            gtk_signal_emit_by_name(GTK_OBJECT(todo_completed_checkbox), "clicked");
            gtk_signal_emit_by_name(GTK_OBJECT(apply_record_button), "clicked");
        }*/
        connect_changed_signals(CONNECT_SIGNALS);
        return TRUE;
    }
    // gtk_tree_model_get(model, &iter, TODO_TEXT_COLUMN_ENUM, &name, -1);


    return TRUE; /* allow selection state to change */
}

static void cb_clist_selection(GtkWidget *clist,
                               gint row,
                               gint column,
                               GdkEventButton *event,
                               gpointer data) {
    struct ToDo *todo;
    MyToDo *mtodo;
    int b;
    int index, sorted_position;
    unsigned int unique_id = 0;

    time_t ltime;
    struct tm *now;

    if ((record_changed == MODIFY_FLAG) || (record_changed == NEW_FLAG)) {
        if (clist_row_selected == row) { return; }

        mtodo = gtk_clist_get_row_data(GTK_CLIST(clist), row);
        if (mtodo != NULL) {
            unique_id = mtodo->unique_id;
        }

        b = dialog_save_changed_record_with_cancel(pane, record_changed);
        if (b == DIALOG_SAID_1) { /* Cancel */
            if (clist_row_selected >= 0) {
                clist_select_row(GTK_CLIST(clist), clist_row_selected, 0);
            } else {
                clist_row_selected = 0;
                clist_select_row(GTK_CLIST(clist), 0, 0);
            }
            return;
        }
        if (b == DIALOG_SAID_3) { /* Save */
            cb_add_new_record(NULL, GINT_TO_POINTER(record_changed));
        }

        set_new_button_to(CLEAR_FLAG);

        if (unique_id) {
            glob_find_id = unique_id;
            todo_find();
        } else {
            clist_select_row(GTK_CLIST(clist), row, column);
        }
        return;
    }

    time(&ltime);
    now = localtime(&ltime);

    clist_row_selected = row;

    mtodo = gtk_clist_get_row_data(GTK_CLIST(clist), row);
    if (mtodo == NULL) {
        return;
    }

    if (mtodo->rt == DELETED_PALM_REC ||
        (mtodo->rt == DELETED_PC_REC))
        /* Possible later addition of undelete code for modified deleted records
           || mtodo->rt == MODIFIED_PALM_REC
        */
    {
        set_new_button_to(UNDELETE_FLAG);
    } else {
        set_new_button_to(CLEAR_FLAG);
    }

    connect_changed_signals(DISCONNECT_SIGNALS);

    if (mtodo == NULL) {
        return;
    }
    todo = &(mtodo->todo);

    gtk_widget_freeze_child_notify(todo_desc);
    gtk_widget_freeze_child_notify(todo_note);

    gtk_text_buffer_set_text(GTK_TEXT_BUFFER(todo_desc_buffer), "", -1);
    gtk_text_buffer_set_text(GTK_TEXT_BUFFER(todo_note_buffer), "", -1);

    index = mtodo->attrib & 0x0F;
    sorted_position = find_sort_cat_pos(index);
    if (todo_cat_menu_item2[sorted_position] == NULL) {
        /* Illegal category */
        jp_logf(JP_LOG_DEBUG, "Category is not legal\n");
        index = sorted_position = 0;
        sorted_position = find_sort_cat_pos(index);
    }

    if (sorted_position < 0) {
        jp_logf(JP_LOG_WARN, _("Category is not legal\n"));
    } else {
        gtk_check_menu_item_set_active
                (GTK_CHECK_MENU_ITEM(todo_cat_menu_item2[sorted_position]), TRUE);
    }
    gtk_option_menu_set_history(GTK_OPTION_MENU(category_menu2),
                                find_menu_cat_pos(sorted_position));

    if (todo->description) {
        if (todo->description[0]) {
            gtk_text_buffer_set_text(GTK_TEXT_BUFFER(todo_desc_buffer), todo->description, -1);
        }
    }

    if (todo->note) {
        if (todo->note[0]) {
            gtk_text_buffer_set_text(GTK_TEXT_BUFFER(todo_note_buffer), todo->note, -1);
        }
    }

    if ((todo->priority < 1) || (todo->priority > 5)) {
        jp_logf(JP_LOG_WARN, _("Priority out of range\n"));
    } else {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_button_todo[todo->priority - 1]), TRUE);
    }

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(todo_completed_checkbox), todo->complete);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(private_checkbox),
                                 mtodo->attrib & dlpRecAttrSecret);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(todo_no_due_date_checkbox),
                                 todo->indefinite);
    if (!todo->indefinite) {
        update_due_button(due_date_button, &(todo->due));
        due_date.tm_mon = todo->due.tm_mon;
        due_date.tm_mday = todo->due.tm_mday;
        due_date.tm_year = todo->due.tm_year;
    } else {
        update_due_button(due_date_button, NULL);
        due_date.tm_mon = now->tm_mon;
        due_date.tm_mday = now->tm_mday;
        due_date.tm_year = now->tm_year;
    }

    gtk_widget_thaw_child_notify(todo_desc);
    gtk_widget_thaw_child_notify(todo_note);

    /* If they have clicked on the checkmark box then do a modify */
    if (column == 0) {
        gtk_signal_emit_by_name(GTK_OBJECT(todo_completed_checkbox), "clicked");
        gtk_signal_emit_by_name(GTK_OBJECT(apply_record_button), "clicked");
    }
    connect_changed_signals(CONNECT_SIGNALS);
}

static gboolean cb_key_pressed_left_side(GtkWidget *widget,
                                         GdkEventKey *event,
                                         gpointer next_widget) {
    GtkTextBuffer *text_buffer;
    GtkTextIter iter;

    if (event->keyval == GDK_Return) {
        gtk_signal_emit_stop_by_name(GTK_OBJECT(widget), "key_press_event");
        gtk_widget_grab_focus(GTK_WIDGET(next_widget));
        /* Position cursor at start of text */
        text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(next_widget));
        gtk_text_buffer_get_start_iter(text_buffer, &iter);
        gtk_text_buffer_place_cursor(text_buffer, &iter);
        return TRUE;
    }

    return FALSE;
}

static gboolean cb_key_pressed_right_side(GtkWidget *widget,
                                          GdkEventKey *event,
                                          gpointer data) {
    if ((event->keyval == GDK_Return) && (event->state & GDK_SHIFT_MASK)) {
        gtk_signal_emit_stop_by_name(GTK_OBJECT(widget), "key_press_event");
        /* Call clist_selection to handle any cleanup such as a modified record */
        cb_clist_selection(clist, clist_row_selected, TODO_PRIORITY_COLUMN,
                           GINT_TO_POINTER(1), NULL);
        gtk_widget_grab_focus(GTK_WIDGET(clist));
        return TRUE;
    }
    /* Call external editor for note text */
    if (data != NULL &&
        (event->keyval == GDK_e) && (event->state & GDK_CONTROL_MASK)) {
        gtk_signal_emit_stop_by_name(GTK_OBJECT(widget), "key_press_event");

        /* Get current text and place in temporary file */
        GtkTextIter start_iter;
        GtkTextIter end_iter;
        char *text_out;

        gtk_text_buffer_get_bounds(GTK_TEXT_BUFFER(todo_note_buffer),
                                   &start_iter, &end_iter);
        text_out = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(todo_note_buffer),
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
            jp_logf(JP_LOG_WARN, _("External editor command too long to execute\n"));
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
                gtk_text_buffer_set_text(GTK_TEXT_BUFFER(todo_note_buffer),
                                         text_in, -1);
            }
        }

        if (text_out)
            free(text_out);

        return TRUE;
    }   /* End of external editor if */

    return FALSE;
}

void todo_liststore_clear(GtkListStore *pListStore) {
    gtk_list_store_clear(pListStore);

}

void todo_clist_clear(GtkCList *clist) {
    GtkStyle *base_style, *row_style, *cell_style;
    int i;

    base_style = gtk_widget_get_style(GTK_WIDGET(clist));

    for (i = 0; i < GTK_CLIST(clist)->rows; i++) {
        row_style = gtk_clist_get_row_style(GTK_CLIST(clist), i);
        if (row_style && (row_style != base_style)) {
            g_object_unref(row_style);
        }
        cell_style = gtk_clist_get_cell_style(GTK_CLIST(clist), i, TODO_DATE_COLUMN);
        if (cell_style && (cell_style != base_style)) {
            g_object_unref(cell_style);
        }
    }

    gtk_clist_clear(GTK_CLIST(clist));

}

void todo_update_clist(GtkWidget *clist, GtkWidget *tooltip_widget,
                       ToDoList **todo_list, int category, int main) {
    int num_entries, entries_shown;
    gchar *empty_line[] = {"", "", "", "", ""};
    GdkPixmap *pixmap_note;
    GdkPixmap *pixmap_check;
    GdkPixmap *pixmap_checked;
    GdkBitmap *mask_note;
    GdkBitmap *mask_check;
    GdkBitmap *mask_checked;
    ToDoList *temp_todo;
    char str[50];
    char str2[TODO_MAX_COLUMN_LEN + 2];
    const char *svalue;
    long hide_completed, hide_not_due;
    long show_tooltips;
    int show_priv;
    time_t ltime;
    struct tm *now, *due;
    int comp_now, comp_due;

    free_ToDoList(todo_list);

    /* Need to get all records including private ones for the tooltips calculation */
    num_entries = get_todos2(todo_list, SORT_ASCENDING, 2, 2, 1, 1, CATEGORY_ALL);

    /* Start by clearing existing entry if in main window */
    if (main) {
        todo_clear_details();
    }

    /* Freeze clist to prevent flicker during updating */
    gtk_clist_freeze(GTK_CLIST(clist));
    if (main) {
        gtk_signal_disconnect_by_func(GTK_OBJECT(clist),
                                      GTK_SIGNAL_FUNC(cb_clist_selection), NULL);
    }
    todo_clist_clear(GTK_CLIST(clist));
#ifdef __APPLE__
    gtk_clist_thaw(GTK_CLIST(clist));
    gtk_widget_hide(clist);
    gtk_widget_show_all(clist);
    gtk_clist_freeze(GTK_CLIST(clist));
#endif

    /* Collect preferences and constant pixmaps for loop */
    get_pref(PREF_TODO_HIDE_COMPLETED, &hide_completed, NULL);
    get_pref(PREF_TODO_HIDE_NOT_DUE, &hide_not_due, NULL);
    show_priv = show_privates(GET_PRIVATES);
    get_pixmaps(clist, PIXMAP_NOTE, &pixmap_note, &mask_note);
    get_pixmaps(clist, PIXMAP_BOX_CHECK, &pixmap_check, &mask_check);
    get_pixmaps(clist, PIXMAP_BOX_CHECKED, &pixmap_checked, &mask_checked);
#ifdef __APPLE__
    mask_note = NULL;
    mask_check = NULL;
    mask_checked = NULL;
#endif
    /* Current time used for calculating overdue items */
    time(&ltime);
    now = localtime(&ltime);
    comp_now = now->tm_year * 380 + now->tm_mon * 31 + now->tm_mday - 1;

    entries_shown = 0;
    for (temp_todo = *todo_list; temp_todo; temp_todo = temp_todo->next) {
        if (((temp_todo->mtodo.attrib & 0x0F) != category) &&
            category != CATEGORY_ALL) {
            continue;
        }

        /* Do masking like Palm OS 3.5 */
        if ((show_priv == MASK_PRIVATES) &&
            (temp_todo->mtodo.attrib & dlpRecAttrSecret)) {
            gtk_clist_append(GTK_CLIST(clist), empty_line);
            gtk_clist_set_text(GTK_CLIST(clist), entries_shown, TODO_CHECK_COLUMN, "---");
            gtk_clist_set_text(GTK_CLIST(clist), entries_shown, TODO_PRIORITY_COLUMN, "---");
            gtk_clist_set_text(GTK_CLIST(clist), entries_shown, TODO_TEXT_COLUMN, "--------------------");
            gtk_clist_set_text(GTK_CLIST(clist), entries_shown, TODO_DATE_COLUMN, "----------");
            clear_mytodos(&temp_todo->mtodo);
            gtk_clist_set_row_data(GTK_CLIST(clist), entries_shown, &(temp_todo->mtodo));
            gtk_clist_set_row_style(GTK_CLIST(clist), entries_shown, NULL);
            entries_shown++;
            continue;
        }
        /* End Masking */

        /* Allow a record found through search window to temporarily be
           displayed even if it would normally be hidden by option settings */
        if (!glob_find_id || (glob_find_id != temp_todo->mtodo.unique_id)) {
            /* Hide the completed records if need be */
            if (hide_completed && temp_todo->mtodo.todo.complete) {
                continue;
            }

            /* Hide the not due yet records if need be */
            if ((hide_not_due) && (!(temp_todo->mtodo.todo.indefinite))) {
                due = &(temp_todo->mtodo.todo.due);
                comp_due = due->tm_year * 380 + due->tm_mon * 31 + due->tm_mday - 1;
                if (comp_due > comp_now) {
                    continue;
                }
            }
        }

        /* Hide the private records if need be */
        if ((show_priv != SHOW_PRIVATES) &&
            (temp_todo->mtodo.attrib & dlpRecAttrSecret)) {
            continue;
        }

        /* Add entry to clist */
        gtk_clist_append(GTK_CLIST(clist), empty_line);

        /* Put a checkbox or checked checkbox pixmap up */
        if (temp_todo->mtodo.todo.complete) {
            gtk_clist_set_pixmap(GTK_CLIST(clist), entries_shown, TODO_CHECK_COLUMN, pixmap_checked, mask_checked);
        } else {
            gtk_clist_set_pixmap(GTK_CLIST(clist), entries_shown, TODO_CHECK_COLUMN, pixmap_check, mask_check);
        }

        /* Print the priority number */
        sprintf(str, "%d", temp_todo->mtodo.todo.priority);
        gtk_clist_set_text(GTK_CLIST(clist), entries_shown, TODO_PRIORITY_COLUMN, str);

        /* Put a note pixmap up */
        if (temp_todo->mtodo.todo.note[0]) {
            gtk_clist_set_pixmap(GTK_CLIST(clist), entries_shown, TODO_NOTE_COLUMN, pixmap_note, mask_note);
        } else {
            gtk_clist_set_text(GTK_CLIST(clist), entries_shown, TODO_NOTE_COLUMN, "");
        }

        /* Print the due date */
        if (!temp_todo->mtodo.todo.indefinite) {
            get_pref(PREF_SHORTDATE, NULL, &svalue);
            strftime(str, sizeof(str), svalue, &(temp_todo->mtodo.todo.due));
        } else {
            sprintf(str, _("No date"));
        }
        gtk_clist_set_text(GTK_CLIST(clist), entries_shown, TODO_DATE_COLUMN, str);
        /* Print the todo text */
        lstrncpy_remove_cr_lfs(str2, temp_todo->mtodo.todo.description, TODO_MAX_COLUMN_LEN);
        gtk_clist_set_text(GTK_CLIST(clist), entries_shown, TODO_TEXT_COLUMN, str2);

        gtk_clist_set_row_data(GTK_CLIST(clist), entries_shown, &(temp_todo->mtodo));

        /* Highlight row background depending on status */
        switch (temp_todo->mtodo.rt) {
            case NEW_PC_REC:
            case REPLACEMENT_PALM_REC:
                set_bg_rgb_clist_row(clist, entries_shown,
                                     CLIST_NEW_RED, CLIST_NEW_GREEN, CLIST_NEW_BLUE);
                break;
            case DELETED_PALM_REC:
            case DELETED_PC_REC:
                set_bg_rgb_clist_row(clist, entries_shown,
                                     CLIST_DEL_RED, CLIST_DEL_GREEN, CLIST_DEL_BLUE);
                break;
            case MODIFIED_PALM_REC:
                set_bg_rgb_clist_row(clist, entries_shown,
                                     CLIST_MOD_RED, CLIST_MOD_GREEN, CLIST_MOD_BLUE);
                break;
            default:
                if (temp_todo->mtodo.attrib & dlpRecAttrSecret) {
                    set_bg_rgb_clist_row(clist, entries_shown,
                                         CLIST_PRIVATE_RED, CLIST_PRIVATE_GREEN, CLIST_PRIVATE_BLUE);
                } else {
                    gtk_clist_set_row_style(GTK_CLIST(clist), entries_shown, NULL);
                }
        }

        /* Highlight dates of items overdue or due today */
        if (!(temp_todo->mtodo.todo.indefinite)) {
            due = &(temp_todo->mtodo.todo.due);
            comp_due = due->tm_year * 380 + due->tm_mon * 31 + due->tm_mday - 1;

            if (comp_due < comp_now) {
                set_fg_rgb_clist_cell(clist, entries_shown, TODO_DATE_COLUMN, CLIST_OVERDUE_RED, CLIST_OVERDUE_GREEN,
                                      CLIST_OVERDUE_BLUE);
            } else if (comp_due == comp_now) {
                set_fg_rgb_clist_cell(clist, entries_shown, TODO_DATE_COLUMN, CLIST_DUENOW_RED, CLIST_DUENOW_GREEN,
                                      CLIST_DUENOW_BLUE);
            }
        }

        entries_shown++;
    }
    //todo resume logic copy from here..
    jp_logf(JP_LOG_DEBUG, "entries_shown=%d\n", entries_shown);

    /* Sort the clist */
    gtk_clist_sort(GTK_CLIST(clist));

    if (main) {
        gtk_signal_connect(GTK_OBJECT(clist), "select_row",
                           GTK_SIGNAL_FUNC(cb_clist_selection), NULL);
    }

    /* If there are items in the list, highlight the selected row */
    if ((main) && (entries_shown > 0)) {
        /* First, select any record being searched for */
        if (glob_find_id) {
            todo_find();
        }
            /* Second, try the currently selected row */
        else if (clist_row_selected < entries_shown) {
            clist_select_row(GTK_CLIST(clist), clist_row_selected, TODO_PRIORITY_COLUMN);
            if (!gtk_clist_row_is_visible(GTK_CLIST(clist), clist_row_selected)) {
                gtk_clist_moveto(GTK_CLIST(clist), clist_row_selected, 0, 0.5, 0.0);
            }
        }
            /* Third, select row 0 if nothing else is possible */
        else {
            clist_select_row(GTK_CLIST(clist), 0, TODO_PRIORITY_COLUMN);
        }
    }

    /* Unfreeze clist after all changes */
    gtk_clist_thaw(GTK_CLIST(clist));

    if (tooltip_widget) {
        get_pref(PREF_SHOW_TOOLTIPS, &show_tooltips, NULL);
        if (todo_list == NULL) {
            set_tooltip(show_tooltips, glob_tooltips, tooltip_widget, _("0 records"), NULL);
        } else {
            sprintf(str, _("%d of %d records"), entries_shown, num_entries);
            set_tooltip(show_tooltips, glob_tooltips, tooltip_widget, str, NULL);
        }
    }

    /* return focus to clist after any big operation which requires a redraw */
    gtk_widget_grab_focus(GTK_WIDGET(clist));

}

static int todo_find(void) {
    int r, found_at;

    if (glob_find_id) {
        r = clist_find_id(clist,
                          glob_find_id,
                          &found_at);
        if (r) {
            clist_select_row(GTK_CLIST(clist), found_at, TODO_PRIORITY_COLUMN);
            if (!gtk_clist_row_is_visible(GTK_CLIST(clist), found_at)) {
                gtk_clist_moveto(GTK_CLIST(clist), found_at, 0, 0.5, 0.0);
            }
        }
        glob_find_id = 0;
    }
    return EXIT_SUCCESS;
}

static gboolean cb_key_pressed_tab(GtkWidget *widget,
                                   GdkEventKey *event,
                                   gpointer next_widget) {
    GtkTextIter cursor_pos_iter;
    GtkTextBuffer *text_buffer;

    if (event->keyval == GDK_Tab) {
        /* See if they are at the end of the text */
        text_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(widget));
        gtk_text_buffer_get_iter_at_mark(text_buffer, &cursor_pos_iter, gtk_text_buffer_get_insert(text_buffer));
        if (gtk_text_iter_is_end(&cursor_pos_iter)) {
            gtk_signal_emit_stop_by_name(GTK_OBJECT(widget), "key_press_event");
            gtk_widget_grab_focus(GTK_WIDGET(next_widget));
            return TRUE;
        }
    }
    return FALSE;
}

static gboolean cb_key_pressed_shift_tab(GtkWidget *widget,
                                         GdkEventKey *event,
                                         gpointer next_widget) {
    if (event->keyval == GDK_ISO_Left_Tab) {
        gtk_signal_emit_stop_by_name(GTK_OBJECT(widget), "key_press_event");
        gtk_widget_grab_focus(GTK_WIDGET(next_widget));
        return TRUE;
    }
    return FALSE;
}

/* This redraws the clist and goes back to the same line number */
static int todo_clist_redraw(void) {
    todo_update_clist(clist, category_menu1, &glob_todo_list, todo_category, TRUE);

    return EXIT_SUCCESS;
}

int todo_cycle_cat(void) {
    int b;
    int i, new_cat;

    b = dialog_save_changed_record(pane, record_changed);
    if (b == DIALOG_SAID_2) {
        cb_add_new_record(NULL, GINT_TO_POINTER(record_changed));
    }

    if (todo_category == CATEGORY_ALL) {
        new_cat = -1;
    } else {
        new_cat = find_sort_cat_pos(todo_category);
    }

    for (i = 0; i < NUM_TODO_CAT_ITEMS; i++) {
        new_cat++;
        if (new_cat >= NUM_TODO_CAT_ITEMS) {
            todo_category = CATEGORY_ALL;
            break;
        }
        if ((sort_l[new_cat].Pcat) && (sort_l[new_cat].Pcat[0])) {
            todo_category = sort_l[new_cat].cat_num;
            break;
        }
    }

    clist_row_selected = 0;

    return EXIT_SUCCESS;
}

int todo_refresh(void) {
    int index, index2;

    if (glob_find_id) {
        todo_category = CATEGORY_ALL;
    }
    if (todo_category == CATEGORY_ALL) {
        index = 0;
        index2 = 0;
    } else {
        index = find_sort_cat_pos(todo_category);
        index2 = find_menu_cat_pos(index) + 1;
        index += 1;
    }
    todo_update_clist(clist, category_menu1, &glob_todo_list, todo_category, TRUE);
    todo_update_liststore(listStore, category_menu1, &glob_todo_list, todo_category, TRUE);
    if (index < 0) {
        jp_logf(JP_LOG_WARN, _("Category is not legal\n"));
    } else {
        gtk_check_menu_item_set_active
                (GTK_CHECK_MENU_ITEM(todo_cat_menu_item1[index]), TRUE);
        gtk_option_menu_set_history(GTK_OPTION_MENU(category_menu1), index2);
    }

    return EXIT_SUCCESS;
}

void todo_update_liststore(GtkListStore *pListStore, GtkWidget *tooltip_widget,
                           ToDoList **todo_list, int category, int main) {
    GtkTreeIter iter;
    int num_entries, entries_shown;
    gchar *empty_line[] = {"", "", "", "", ""};
    GdkPixbuf *pixbuf_note;
    GdkPixbuf *pixbuf_check;
    GdkPixbuf *pixbuf_checked;
    gboolean checkColumnDisplay = gtk_true();
    GdkPixbuf *noteColumnDisplay;
    ToDoList *temp_todo;
    char dateDisplay[50];
    char priorityDisplay[50];
    char textDisplay[50];
    char descriptionDisplay[TODO_MAX_COLUMN_LEN + 2];
    const char *svalue;
    long hide_completed, hide_not_due;
    long show_tooltips;
    int show_priv;
    time_t ltime;
    struct tm *now, *due;
    int comp_now, comp_due;

    free_ToDoList(todo_list);

    /* Need to get all records including private ones for the tooltips calculation */
    num_entries = get_todos2(todo_list, SORT_ASCENDING, 2, 2, 1, 1, CATEGORY_ALL);

    /* Start by clearing existing entry if in main window */
    if (main) {
        todo_clear_details();
    }

    /* Freeze clist to prevent flicker during updating */
    if (main) {
        /** todo  hook this signal up
        gtk_signal_disconnect_by_func(GTK_OBJECT(clist),
                                      GTK_SIGNAL_FUNC(cb_clist_selection), NULL);
       */
    }
    todo_liststore_clear(pListStore);
/*#ifdef __APPLE__
    gtk_clist_thaw(GTK_CLIST(clist));
   gtk_widget_hide(clist);
   gtk_widget_show_all(clist);
   gtk_clist_freeze(GTK_CLIST(clist));
#endif
 */

    /* Collect preferences and constant pixmaps for loop */
    get_pref(PREF_TODO_HIDE_COMPLETED, &hide_completed, NULL);
    get_pref(PREF_TODO_HIDE_NOT_DUE, &hide_not_due, NULL);
    show_priv = show_privates(GET_PRIVATES);
    get_pixbufs(PIXMAP_NOTE, &pixbuf_note);
    get_pixbufs(PIXMAP_BOX_CHECK, &pixbuf_check);
    get_pixbufs(PIXMAP_BOX_CHECKED, &pixbuf_checked);
/*#ifdef __APPLE__
    mask_note = NULL;
   mask_check = NULL;
   mask_checked = NULL;
#endif
 */
    /* Current time used for calculating overdue items */
    time(&ltime);
    now = localtime(&ltime);
    comp_now = now->tm_year * 380 + now->tm_mon * 31 + now->tm_mday - 1;

    entries_shown = 0;
    for (temp_todo = *todo_list; temp_todo; temp_todo = temp_todo->next) {

        if (((temp_todo->mtodo.attrib & 0x0F) != category) &&
            category != CATEGORY_ALL) {
            continue;
        }

        /* Do masking like Palm OS 3.5 */
        if ((show_priv == MASK_PRIVATES) &&
            (temp_todo->mtodo.attrib & dlpRecAttrSecret)) {
            gtk_list_store_append(pListStore, &iter);
            gtk_list_store_set(pListStore, &iter,
                               TODO_CHECK_COLUMN_ENUM, "---",
                               TODO_PRIORITY_COLUMN_ENUM, "---",
                               TODO_TEXT_COLUMN_ENUM, "--------------------",
                               TODO_DATE_COLUMN_ENUM, "----------",
                               TODO_DATA_COLUMN_ENUM, &(temp_todo->mtodo),
                               -1);
            clear_mytodos(&temp_todo->mtodo);
            // gtk_clist_set_row_data(GTK_CLIST(clist), entries_shown, &(temp_todo->mtodo));
            //  gtk_clist_set_row_style(GTK_CLIST(clist), entries_shown, NULL);
            entries_shown++;
            continue;
        }
        /* End Masking */

        /* Allow a record found through search window to temporarily be
           displayed even if it would normally be hidden by option settings */
        if (!glob_find_id || (glob_find_id != temp_todo->mtodo.unique_id)) {
            /* Hide the completed records if need be */
            if (hide_completed && temp_todo->mtodo.todo.complete) {
                continue;
            }

            /* Hide the not due yet records if need be */
            if ((hide_not_due) && (!(temp_todo->mtodo.todo.indefinite))) {
                due = &(temp_todo->mtodo.todo.due);
                comp_due = due->tm_year * 380 + due->tm_mon * 31 + due->tm_mday - 1;
                if (comp_due > comp_now) {
                    continue;
                }
            }
        }

        /* Hide the private records if need be */
        if ((show_priv != SHOW_PRIVATES) &&
            (temp_todo->mtodo.attrib & dlpRecAttrSecret)) {
            continue;
        }



        /* Put a checkbox or checked checkbox pixmap up */
        if (temp_todo->mtodo.todo.complete) {
            checkColumnDisplay = gtk_true();
        } else {
            checkColumnDisplay = gtk_false();
        }

        /* Print the priority number */
        sprintf(priorityDisplay, "%d", temp_todo->mtodo.todo.priority);

        /* Put a note pixmap up */
        if (temp_todo->mtodo.todo.note[0]) {
            noteColumnDisplay = pixbuf_note;
        } else {
            noteColumnDisplay = NULL;
        }

        /* Print the due date */
        if (!temp_todo->mtodo.todo.indefinite) {
            get_pref(PREF_SHORTDATE, NULL, &svalue);
            strftime(dateDisplay, sizeof(dateDisplay), svalue, &(temp_todo->mtodo.todo.due));
        } else {
            sprintf(dateDisplay, _("No date"));
        }

        /* Print the todo text */
        lstrncpy_remove_cr_lfs(descriptionDisplay, temp_todo->mtodo.todo.description, TODO_MAX_COLUMN_LEN);
        gtk_list_store_append(pListStore, &iter);
        gtk_list_store_set(pListStore, &iter,
                           TODO_CHECK_COLUMN_ENUM, checkColumnDisplay,
                           TODO_PRIORITY_COLUMN_ENUM, priorityDisplay,
                           TODO_NOTE_COLUMN_ENUM, noteColumnDisplay,
                           TODO_TEXT_COLUMN_ENUM, descriptionDisplay,
                           TODO_DATE_COLUMN_ENUM, dateDisplay,
                           TODO_DATA_COLUMN_ENUM, &(temp_todo->mtodo),
                           -1);
        /* Highlight row background depending on status */
        /*  switch (temp_todo->mtodo.rt) {
              case NEW_PC_REC:
              case REPLACEMENT_PALM_REC:
                  set_bg_rgb_clist_row(clist, entries_shown,
                                       CLIST_NEW_RED, CLIST_NEW_GREEN, CLIST_NEW_BLUE);
                  break;
              case DELETED_PALM_REC:
              case DELETED_PC_REC:
                  set_bg_rgb_clist_row(clist, entries_shown,
                                       CLIST_DEL_RED, CLIST_DEL_GREEN, CLIST_DEL_BLUE);
                  break;
              case MODIFIED_PALM_REC:
                  set_bg_rgb_clist_row(clist, entries_shown,
                                       CLIST_MOD_RED, CLIST_MOD_GREEN, CLIST_MOD_BLUE);
                  break;
              default:
                  if (temp_todo->mtodo.attrib & dlpRecAttrSecret) {
                      set_bg_rgb_clist_row(clist, entries_shown,
                                           CLIST_PRIVATE_RED, CLIST_PRIVATE_GREEN, CLIST_PRIVATE_BLUE);
                  } else {
                      gtk_clist_set_row_style(GTK_CLIST(clist), entries_shown, NULL);
                  }
          } */

        /* Highlight dates of items overdue or due today */
        /*  if (!(temp_todo->mtodo.todo.indefinite)) {
              due = &(temp_todo->mtodo.todo.due);
              comp_due=due->tm_year*380+due->tm_mon*31+due->tm_mday-1;

              if (comp_due < comp_now) {
                  set_fg_rgb_clist_cell(clist, entries_shown, TODO_DATE_COLUMN, CLIST_OVERDUE_RED, CLIST_OVERDUE_GREEN, CLIST_OVERDUE_BLUE);
              } else if (comp_due == comp_now) {
                  set_fg_rgb_clist_cell(clist, entries_shown, TODO_DATE_COLUMN, CLIST_DUENOW_RED, CLIST_DUENOW_GREEN, CLIST_DUENOW_BLUE);
              }
          } */

        entries_shown++;
    }
    /* gtk_list_store_append (pListStore, &iter);

     gtk_list_store_set(pListStore,&iter,
                        TODO_CHECK_COLUMN_ENUM,pixbuf_checked,
                        TODO_PRIORITY_COLUMN_ENUM,num_entries,
                        TODO_TEXT_COLUMN_ENUM,"text3",-1);*/
}

int todo_gui_cleanup(void) {
    int b;

    b = dialog_save_changed_record(pane, record_changed);
    if (b == DIALOG_SAID_2) {
        cb_add_new_record(NULL, GINT_TO_POINTER(record_changed));
    }
    free_ToDoList(&glob_todo_list);
    connect_changed_signals(DISCONNECT_SIGNALS);
    set_pref(PREF_TODO_PANE, gtk_paned_get_position(GTK_PANED(pane)), NULL, TRUE);
    set_pref(PREF_TODO_NOTE_PANE, gtk_paned_get_position(GTK_PANED(note_pane)), NULL, TRUE);
    set_pref(PREF_LAST_TODO_CATEGORY, todo_category, NULL, TRUE);
    set_pref(PREF_TODO_SORT_COLUMN, clist_col_selected, NULL, TRUE);
    set_pref(PREF_TODO_SORT_ORDER, GTK_CLIST(clist)->sort_type, NULL, TRUE);
    todo_clist_clear(GTK_CLIST(clist));
    todo_liststore_clear(listStore);
    return EXIT_SUCCESS;
}

/* Main function */
int todo_gui(GtkWidget *vbox, GtkWidget *hbox) {
    GtkWidget *scrolled_window;
    GtkWidget *pixmapwid;
    GdkPixmap *pixmap;
    GdkBitmap *mask;
    GtkWidget *vbox1, *vbox2;
    GtkWidget *hbox_temp, *hbox_temp2;
    GtkWidget *vbox_temp;
    GtkWidget *separator;
    GtkWidget *label;
    char str[MAX_RADIO_BUTTON_LEN];
    int i;
    GSList *group;
    long ivalue;
    char *titles[] = {"", "", "", "", ""};

    GtkAccelGroup *accel_group;
    long char_set;
    long show_tooltips;
    char *cat_name;

    get_pref(PREF_TODO_VERSION, &todo_version, NULL);

    init();

    get_todo_app_info(&todo_app_info);

    /* Initialize categories */
    get_pref(PREF_CHAR_SET, &char_set, NULL);
    for (i = 1; i < NUM_TODO_CAT_ITEMS; i++) {
        cat_name = charset_p2newj(todo_app_info.category.name[i], 31, char_set);
        strcpy(sort_l[i - 1].Pcat, cat_name);
        free(cat_name);
        sort_l[i - 1].cat_num = i;
    }
    /* put reserved 'Unfiled' category at end of list */
    cat_name = charset_p2newj(todo_app_info.category.name[0], 31, char_set);
    strcpy(sort_l[NUM_TODO_CAT_ITEMS - 1].Pcat, cat_name);
    free(cat_name);
    sort_l[NUM_TODO_CAT_ITEMS - 1].cat_num = 0;

    qsort(sort_l, NUM_TODO_CAT_ITEMS - 1, sizeof(struct sorted_cats), cat_compare);

#ifdef JPILOT_DEBUG
    for (i=0; i<NUM_TODO_CAT_ITEMS; i++) {
       printf("cat %d [%s]\n", sort_l[i].cat_num, sort_l[i].Pcat);
    }
#endif

    get_pref(PREF_LAST_TODO_CATEGORY, &ivalue, NULL);
    todo_category = ivalue;

    if ((todo_category != CATEGORY_ALL)
        && (todo_app_info.category.name[todo_category][0] == '\0')) {
        todo_category = CATEGORY_ALL;
    }

    /* Create basic GUI with left and right boxes and sliding pane */
    accel_group = gtk_accel_group_new();
    gtk_window_add_accel_group(GTK_WINDOW(gtk_widget_get_toplevel(vbox)),
                               accel_group);
    get_pref(PREF_SHOW_TOOLTIPS, &show_tooltips, NULL);

    pane = gtk_hpaned_new();
    get_pref(PREF_TODO_PANE, &ivalue, NULL);
    gtk_paned_set_position(GTK_PANED(pane), ivalue);

    gtk_box_pack_start(GTK_BOX(hbox), pane, TRUE, TRUE, 5);

    vbox1 = gtk_vbox_new(FALSE, 0);
    vbox2 = gtk_vbox_new(FALSE, 0);

    gtk_paned_pack1(GTK_PANED(pane), vbox1, TRUE, FALSE);
    gtk_paned_pack2(GTK_PANED(pane), vbox2, TRUE, FALSE);

    /* Left side of GUI */

    /* Separator */
    separator = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(vbox1), separator, FALSE, FALSE, 5);

    //time(&ltime);
    //now = localtime(&ltime);

    /* Make the 'Today is:' label */
    glob_date_label = gtk_label_new(" ");
    gtk_box_pack_start(GTK_BOX(vbox1), glob_date_label, FALSE, FALSE, 0);
    timeout_date(NULL);
    glob_date_timer_tag = gtk_timeout_add(CLOCK_TICK, timeout_sync_up, NULL);

    /* Separator */
    separator = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(vbox1), separator, FALSE, FALSE, 5);

    /* Left-side Category box */
    hbox_temp = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox1), hbox_temp, FALSE, FALSE, 0);

    /* Left-side Category menu */
    make_category_menu(&category_menu1, todo_cat_menu_item1,
                       sort_l, cb_category, TRUE, TRUE);
    gtk_box_pack_start(GTK_BOX(hbox_temp), category_menu1, TRUE, TRUE, 0);

    /* Todo list scrolled window */
    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_set_border_width(GTK_CONTAINER(scrolled_window), 0);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(scrolled_window, GTK_TYPE_SHADOW_TYPE);
    gtk_box_pack_start(GTK_BOX(vbox1), scrolled_window, TRUE, TRUE, 0);
    //
    clist = gtk_clist_new_with_titles(5, titles);

    listStore = gtk_list_store_new(TODO_NUM_COLS, G_TYPE_BOOLEAN, G_TYPE_STRING, GDK_TYPE_PIXBUF, G_TYPE_STRING,
                                   G_TYPE_STRING, G_TYPE_POINTER);
    GtkTreeSortable *sortable;
    sortable = GTK_TREE_SORTABLE(listStore);
    gtk_tree_sortable_set_sort_func(sortable, TODO_NOTE_COLUMN_ENUM, GtkTreeColumnCompare,
                                    GINT_TO_POINTER(TODO_NOTE_COLUMN_ENUM), NULL);
    gtk_tree_sortable_set_sort_func(sortable, TODO_CHECK_COLUMN_ENUM, GtkTreeColumnCompare,
                                    GINT_TO_POINTER(TODO_CHECK_COLUMN_ENUM), NULL);
    GtkTreeModel *model = GTK_TREE_MODEL(listStore);
    treeView = gtk_tree_view_new_with_model(model);
    //GtkTreeIter    iter;
    //
    //    store = gtk_list_store_new (NUM_COLS, G_TYPE_STRING, G_TYPE_UINT,G_TYPE_STRING);
    //
    //    /* Append a row and fill in some data */
    //    gtk_list_store_append (store, &iter);
    //    gtk_list_store_set (store, &iter,
    //                        COL_NAME, "Heinz El-Mann",
    //                        COL_AGE, 51,
    //                        DUMMMY,"",
    //                        -1);


    /**
     *  TODO_PRIORITY_COLUMN_ENUM,
     TODO_NOTE_COLUMN_ENUM,
     TODO_DATE_COLUMN_ENUM,
     TODO_TEXT_COLUMN_ENUM,
     */
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();

    GtkTreeViewColumn *taskColumn = gtk_tree_view_column_new_with_attributes("Task",
                                                                             renderer,
                                                                             "text", TODO_TEXT_COLUMN_ENUM,
                                                                             NULL);
    gtk_tree_view_column_set_sort_column_id(taskColumn, TODO_TEXT_COLUMN_ENUM);


    renderer = gtk_cell_renderer_text_new();

    GtkTreeViewColumn *dateColumn = gtk_tree_view_column_new_with_attributes("Due",
                                                                             renderer,
                                                                             "text", TODO_DATE_COLUMN_ENUM,
                                                                             NULL);
    gtk_tree_view_column_set_sort_column_id(dateColumn, TODO_DATE_COLUMN_ENUM);

    renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *priorityColumn = gtk_tree_view_column_new_with_attributes("",
                                                                                 renderer,
                                                                                 "text", TODO_PRIORITY_COLUMN_ENUM,
                                                                                 NULL);
    gtk_tree_view_column_set_sort_column_id(priorityColumn, TODO_PRIORITY_COLUMN_ENUM);

    renderer = gtk_cell_renderer_pixbuf_new();
    GtkTreeViewColumn *noteColumn = gtk_tree_view_column_new_with_attributes("",
                                                                             renderer,
                                                                             "pixbuf", TODO_NOTE_COLUMN_ENUM,
                                                                             NULL);
    gtk_tree_view_column_set_sort_column_id(noteColumn, TODO_NOTE_COLUMN_ENUM);
//



    renderer = gtk_cell_renderer_toggle_new();

    GtkTreeViewColumn *checkColumn = gtk_tree_view_column_new_with_attributes("",renderer,"active",TODO_CHECK_COLUMN_ENUM,NULL);
    gtk_tree_view_column_set_cell_data_func(checkColumn, renderer, populateChecks, NULL, NULL);
    g_signal_connect (renderer, "toggled",
                       G_CALLBACK(checkedCallBack),
                      GTK_TREE_MODEL(listStore));
   // gtk_tree_view_column_set_title(checkColumn, "");
    //gtk_tree_view_column_pack_start(checkColumn, renderer, gtk_true());
   // gtk_tree_view_column_set_cell_data_func(checkColumn, renderer, checkedCallBack, NULL, NULL);
    //g_signal_connect(treeView,"button-pressed-event",onButtonPress,NULL);
    gtk_tree_view_column_set_attributes(checkColumn,renderer,NULL,TODO_CHECK_COLUMN_ENUM,NULL);
    gtk_tree_view_column_set_sort_column_id(checkColumn, TODO_CHECK_COLUMN_ENUM);
    gtk_tree_view_insert_column(GTK_TREE_VIEW (treeView), checkColumn, TODO_CHECK_COLUMN_ENUM);
    gtk_tree_view_insert_column(GTK_TREE_VIEW (treeView), priorityColumn, TODO_PRIORITY_COLUMN_ENUM);
    gtk_tree_view_insert_column(GTK_TREE_VIEW (treeView), noteColumn, TODO_NOTE_COLUMN_ENUM);
    gtk_tree_view_insert_column(GTK_TREE_VIEW (treeView), dateColumn, TODO_DATE_COLUMN_ENUM);
    gtk_tree_view_insert_column(GTK_TREE_VIEW (treeView), taskColumn, TODO_TEXT_COLUMN_ENUM);
    gtk_tree_view_column_set_clickable(checkColumn, gtk_true());
    gtk_tree_view_column_set_clickable(priorityColumn, gtk_true());
    gtk_tree_view_column_set_clickable(noteColumn, gtk_true());
    gtk_tree_view_column_set_clickable(dateColumn, gtk_true());
    gtk_tree_view_column_set_clickable(taskColumn, gtk_true());
    gtk_tree_view_column_set_sizing(checkColumn, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    gtk_tree_view_column_set_sizing(dateColumn, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    gtk_tree_view_column_set_sizing(priorityColumn, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    gtk_tree_view_column_set_sizing(noteColumn, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    gtk_tree_view_column_set_sizing(taskColumn, GTK_TREE_VIEW_COLUMN_FIXED);
    gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(treeView)),
                                GTK_SELECTION_BROWSE);

    //gtk_tree_view_column_set_
    //gtk_clist_set_shadow_type(GTK_CLIST(clist), SHADOW);
    //   gtk_clist_set_selection_mode(GTK_CLIST(clist), GTK_SELECTION_BROWSE);
    // gtk_tree_view_column_set_
    gtk_clist_set_column_title(GTK_CLIST(clist), TODO_TEXT_COLUMN, _("Task"));
    gtk_clist_set_column_title(GTK_CLIST(clist), TODO_DATE_COLUMN, _("Due"));
    /* Put pretty pictures in the clist column headings */
    get_pixmaps(vbox, PIXMAP_NOTE, &pixmap, &mask);
#ifdef __APPLE__
    mask = NULL;
#endif
    pixmapwid = gtk_pixmap_new(pixmap, mask);
    gtk_tree_view_column_set_widget(noteColumn, pixmapwid);
    gtk_tree_view_column_set_alignment(noteColumn, GTK_JUSTIFY_CENTER);

    gtk_clist_set_column_widget(GTK_CLIST(clist), TODO_NOTE_COLUMN, pixmapwid);
    gtk_clist_set_column_justification(GTK_CLIST(clist), TODO_NOTE_COLUMN, GTK_JUSTIFY_CENTER);

    get_pixmaps(vbox, PIXMAP_BOX_CHECKED, &pixmap, &mask);
#ifdef __APPLE__
    mask = NULL;
#endif
    pixmapwid = gtk_pixmap_new(pixmap, mask);
    gtk_tree_view_column_set_widget(checkColumn, pixmapwid);
    gtk_tree_view_column_set_alignment(checkColumn, GTK_JUSTIFY_CENTER);
    gtk_clist_set_column_widget(GTK_CLIST(clist), TODO_CHECK_COLUMN, pixmapwid);
    gtk_clist_set_column_justification(GTK_CLIST(clist), TODO_CHECK_COLUMN, GTK_JUSTIFY_CENTER);

    //todo:  Find the gtk_tree_view equivalant.
    gtk_clist_column_titles_active(GTK_CLIST(clist));
    // register function to handle column header clicks..
    g_signal_connect (taskColumn, "clicked", G_CALLBACK(column_clicked_cb), NULL);
    g_signal_connect (noteColumn, "clicked", G_CALLBACK(column_clicked_cb), NULL);
    g_signal_connect (checkColumn, "clicked", G_CALLBACK(column_clicked_cb), NULL);
    g_signal_connect (priorityColumn, "clicked", G_CALLBACK(column_clicked_cb), NULL);
    g_signal_connect (dateColumn, "clicked", G_CALLBACK(column_clicked_cb), NULL);
     treeSelection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeView));

    gtk_tree_selection_set_select_function(treeSelection, handleRowSelection, NULL, NULL);

    gtk_signal_connect(GTK_OBJECT(clist), "click_column",
                       GTK_SIGNAL_FUNC(cb_clist_click_column), NULL);
    // register function to handle row selection.
    gtk_signal_connect(GTK_OBJECT(clist), "select_row",
                       GTK_SIGNAL_FUNC(cb_clist_selection), NULL);
    gtk_clist_set_shadow_type(GTK_CLIST(clist), SHADOW);
    gtk_clist_set_selection_mode(GTK_CLIST(clist), GTK_SELECTION_BROWSE);

    //todo: figure gtk_tree_view equivalant..
    gtk_clist_set_column_auto_resize(GTK_CLIST(clist), TODO_CHECK_COLUMN, TRUE);
    gtk_clist_set_column_auto_resize(GTK_CLIST(clist), TODO_PRIORITY_COLUMN, TRUE);
    gtk_clist_set_column_auto_resize(GTK_CLIST(clist), TODO_NOTE_COLUMN, TRUE);
    gtk_clist_set_column_auto_resize(GTK_CLIST(clist), TODO_DATE_COLUMN, TRUE);
    gtk_clist_set_column_auto_resize(GTK_CLIST(clist), TODO_TEXT_COLUMN, FALSE);

    /* Restore previous sorting configuration */
    get_pref(PREF_TODO_SORT_COLUMN, &ivalue, NULL);
    clist_col_selected = ivalue;
    gtk_clist_set_sort_column(GTK_CLIST(clist), clist_col_selected);
    for (int x = 0; x < TODO_NUM_COLS; x++) {
        gtk_tree_view_column_set_sort_indicator(gtk_tree_view_get_column(treeView, x), gtk_false());
    }
    gtk_tree_view_column_set_sort_indicator(gtk_tree_view_get_column(treeView, clist_col_selected), gtk_true());
    gtk_tree_view_columns_autosize(treeView);


    switch (clist_col_selected) {
        case TODO_CHECK_COLUMN: /* Checkbox column */
            //todo: implement sort..
            //gtk_tree_sortable_set_sort_func()
            gtk_clist_set_compare_func(GTK_CLIST(clist), GtkClistCompareCheckbox);
            break;
        case TODO_DATE_COLUMN:  /* Due Date column */
            //todo: implement sort..
            gtk_clist_set_compare_func(GTK_CLIST(clist), GtkClistCompareDates);
            break;
        default: /* All other columns can use GTK default sort function */
            //todo: implement sort..
            gtk_clist_set_compare_func(GTK_CLIST(clist), NULL);
            break;
    }
    get_pref(PREF_TODO_SORT_ORDER, &ivalue, NULL);
    gtk_tree_sortable_set_sort_column_id(sortable, clist_col_selected, ivalue);
    gtk_clist_set_sort_type(GTK_CLIST (clist), ivalue);

    // gtk_container_add(GTK_CONTAINER(scrolled_window), GTK_WIDGET(clist));
    //todo: make this display
    ////GTK_TREE_MODEL (store);
    //    gtk_tree_view_set_model (GTK_TREE_VIEW (view), model);

    g_object_unref(model);
    gtk_container_add(GTK_CONTAINER(scrolled_window), GTK_WIDGET(treeView));
    /* Right side of GUI */

    hbox_temp = gtk_hbox_new(FALSE, 3);
    gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, FALSE, FALSE, 0);

    /* Cancel button */
    CREATE_BUTTON(cancel_record_button, _("Cancel"), CANCEL, _("Cancel the modifications"), GDK_Escape, 0, "ESC")
    gtk_signal_connect(GTK_OBJECT(cancel_record_button), "clicked",
                       GTK_SIGNAL_FUNC(cb_cancel), NULL);

    /* Delete button */
    CREATE_BUTTON(delete_record_button, _("Delete"), DELETE, _("Delete the selected record"), GDK_d, GDK_CONTROL_MASK,
                  "Ctrl+D")
    gtk_signal_connect(GTK_OBJECT(delete_record_button), "clicked",
                       GTK_SIGNAL_FUNC(cb_delete_todo),
                       GINT_TO_POINTER(DELETE_FLAG));

    /* Undelete Button */
    CREATE_BUTTON(undelete_record_button, _("Undelete"), UNDELETE, _("Undelete the selected record"), 0, 0, "")
    gtk_signal_connect(GTK_OBJECT(undelete_record_button), "clicked",
                       GTK_SIGNAL_FUNC(cb_undelete_todo),
                       GINT_TO_POINTER(UNDELETE_FLAG));

    /* Copy button */
    CREATE_BUTTON(copy_record_button, _("Copy"), COPY, _("Copy the selected record"), GDK_c,
                  GDK_CONTROL_MASK | GDK_SHIFT_MASK, "Ctrl+Shift+C")
    gtk_signal_connect(GTK_OBJECT(copy_record_button), "clicked",
                       GTK_SIGNAL_FUNC(cb_add_new_record),
                       GINT_TO_POINTER(COPY_FLAG));

    /* New button */
    CREATE_BUTTON(new_record_button, _("New Record"), NEW, _("Add a new record"), GDK_n, GDK_CONTROL_MASK, "Ctrl+N")
    gtk_signal_connect(GTK_OBJECT(new_record_button), "clicked",
                       GTK_SIGNAL_FUNC(cb_add_new_record),
                       GINT_TO_POINTER(CLEAR_FLAG));

    /* "Add Record" button */
    CREATE_BUTTON(add_record_button, _("Add Record"), ADD, _("Add the new record"), GDK_Return, GDK_CONTROL_MASK,
                  "Ctrl+Enter")
    gtk_signal_connect(GTK_OBJECT(add_record_button), "clicked",
                       GTK_SIGNAL_FUNC(cb_add_new_record),
                       GINT_TO_POINTER(NEW_FLAG));
#ifndef ENABLE_STOCK_BUTTONS
    gtk_widget_set_name(GTK_WIDGET(GTK_LABEL(GTK_BIN(add_record_button)->child)),
                        "label_high");
#endif

    /* "Apply Changes" button */
    CREATE_BUTTON(apply_record_button, _("Apply Changes"), APPLY, _("Commit the modifications"), GDK_Return,
                  GDK_CONTROL_MASK, "Ctrl+Enter")
    gtk_signal_connect(GTK_OBJECT(apply_record_button), "clicked",
                       GTK_SIGNAL_FUNC(cb_add_new_record),
                       GINT_TO_POINTER(MODIFY_FLAG));
#ifndef ENABLE_STOCK_BUTTONS
    gtk_widget_set_name(GTK_WIDGET(GTK_LABEL(GTK_BIN(apply_record_button)->child)),
                        "label_high");
#endif

    /* Separator */
    separator = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(vbox2), separator, FALSE, FALSE, 5);

    hbox_temp = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, FALSE, FALSE, 0);

    /* Right-side Category menu */
    /* Clear GTK option menus before use */
    for (i = 0; i < NUM_TODO_CAT_ITEMS; i++) {
        todo_cat_menu_item2[i] = NULL;
    }
    make_category_menu(&category_menu2, todo_cat_menu_item2,
                       sort_l, NULL, FALSE, FALSE);
    gtk_box_pack_start(GTK_BOX(hbox_temp), category_menu2, TRUE, TRUE, 0);

    /* Private checkbox */
    private_checkbox = gtk_check_button_new_with_label(_("Private"));
    gtk_box_pack_end(GTK_BOX(hbox_temp), private_checkbox, FALSE, FALSE, 0);

    /* Completed checkbox */
    todo_completed_checkbox = gtk_check_button_new_with_label(_("Completed"));
    gtk_box_pack_start(GTK_BOX(vbox2), todo_completed_checkbox, FALSE, FALSE, 0);

    /* Priority radio buttons */
    hbox_temp = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, FALSE, FALSE, 0);

    label = gtk_label_new(_("Priority:"));
    gtk_box_pack_start(GTK_BOX(hbox_temp), label, FALSE, FALSE, 2);
    set_tooltip(show_tooltips, glob_tooltips, label, _("Set priority   Alt+#"), NULL);

    group = NULL;
    for (i = 0; i < NUM_TODO_PRIORITIES; i++) {
        sprintf(str, "%d", i + 1);
        radio_button_todo[i] = gtk_radio_button_new_with_label(group, str);
        group = gtk_radio_button_group(GTK_RADIO_BUTTON(radio_button_todo[i]));
        gtk_widget_add_accelerator(radio_button_todo[i], "clicked", accel_group,
                                   GDK_1 + i, GDK_MOD1_MASK, GTK_ACCEL_VISIBLE);
        gtk_box_pack_start(GTK_BOX(hbox_temp),
                           radio_button_todo[i], FALSE, FALSE, 0);
    }

    /* "Date Due:" label */
    hbox_temp = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, FALSE, FALSE, 0);

    /* Spacer to line up */
    hbox_temp2 = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox_temp), hbox_temp2, FALSE, FALSE, 1);

    label = gtk_label_new(_("Date Due:"));
    gtk_box_pack_start(GTK_BOX(hbox_temp), label, FALSE, FALSE, 0);

    /* "Date Due" button */
    due_date_button = gtk_button_new_with_label("");
    gtk_box_pack_start(GTK_BOX(hbox_temp), due_date_button, FALSE, FALSE, 5);
    gtk_signal_connect(GTK_OBJECT(due_date_button), "clicked",
                       GTK_SIGNAL_FUNC(cb_cal_dialog), NULL);

    /* "No Date" check box */
    todo_no_due_date_checkbox = gtk_check_button_new_with_label(_("No Date"));
    gtk_signal_connect(GTK_OBJECT(todo_no_due_date_checkbox), "clicked",
                       GTK_SIGNAL_FUNC(cb_check_button_no_due_date), NULL);
    gtk_box_pack_start(GTK_BOX(hbox_temp), todo_no_due_date_checkbox, FALSE, FALSE, 0);

    note_pane = gtk_vpaned_new();
    get_pref(PREF_TODO_NOTE_PANE, &ivalue, NULL);
    gtk_paned_set_position(GTK_PANED(note_pane), ivalue);
    gtk_box_pack_start(GTK_BOX(vbox2), note_pane, TRUE, TRUE, 0);

    /* Description text box */
    hbox_temp = gtk_hbox_new(FALSE, 0);
    gtk_paned_pack1(GTK_PANED(note_pane), hbox_temp, TRUE, FALSE);

    todo_desc = gtk_text_view_new();
    todo_desc_buffer = G_OBJECT(gtk_text_view_get_buffer(GTK_TEXT_VIEW(todo_desc)));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(todo_desc), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(todo_desc), GTK_WRAP_WORD);

    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_set_border_width(GTK_CONTAINER(scrolled_window), 1);
    gtk_container_add(GTK_CONTAINER(scrolled_window), todo_desc);
    gtk_box_pack_start_defaults(GTK_BOX(hbox_temp), scrolled_window);

    /* Note text box */
    vbox_temp = gtk_vbox_new(FALSE, 0);
    gtk_paned_pack2(GTK_PANED(note_pane), vbox_temp, TRUE, FALSE);

    label = gtk_label_new(_("Note"));
    /* Center "Note" label visually */
    gtk_misc_set_alignment(GTK_MISC(label), 0.506, 0.5);
    gtk_box_pack_start(GTK_BOX(vbox_temp), label, FALSE, FALSE, 0);

    todo_note = gtk_text_view_new();
    todo_note_buffer = G_OBJECT(gtk_text_view_get_buffer(GTK_TEXT_VIEW(todo_note)));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(todo_note), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(todo_note), GTK_WRAP_WORD);

    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_set_border_width(GTK_CONTAINER(scrolled_window), 1);
    gtk_container_add(GTK_CONTAINER(scrolled_window), todo_note);
    gtk_box_pack_start_defaults(GTK_BOX(vbox_temp), scrolled_window);

    /* Capture the TAB key to change focus with it */
    gtk_signal_connect(GTK_OBJECT(todo_desc), "key_press_event",
                       GTK_SIGNAL_FUNC(cb_key_pressed_tab), todo_note);

    gtk_signal_connect(GTK_OBJECT(todo_note), "key_press_event",
                       GTK_SIGNAL_FUNC(cb_key_pressed_shift_tab), todo_desc);

    /* Capture the Enter & Shift-Enter key combinations to move back and
     * forth between the left- and right-hand sides of the display. */
    gtk_signal_connect(GTK_OBJECT(clist), "key_press_event",
                       GTK_SIGNAL_FUNC(cb_key_pressed_left_side), todo_desc);

    gtk_signal_connect(GTK_OBJECT(todo_desc), "key_press_event",
                       GTK_SIGNAL_FUNC(cb_key_pressed_right_side), NULL);

    gtk_signal_connect(GTK_OBJECT(todo_note), "key_press_event",
                       GTK_SIGNAL_FUNC(cb_key_pressed_right_side),
                       GINT_TO_POINTER(1));

    /**********************************************************************/

    gtk_widget_show_all(vbox);
    gtk_widget_show_all(hbox);

    gtk_widget_hide(add_record_button);
    gtk_widget_hide(apply_record_button);
    gtk_widget_hide(undelete_record_button);
    gtk_widget_hide(cancel_record_button);

    todo_refresh();

    return EXIT_SUCCESS;
}


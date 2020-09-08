/*******************************************************************************
 * datebook_gui.c
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
#include <sys/stat.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <pi-dlp.h>

#include "datebook.h"
#include "calendar.h"
#include "i18n.h"
#include "todo.h"
#include "prefs.h"
#include "password.h"
#include "export.h"
#include "print.h"
#include "alarms.h"
#include "stock_buttons.h"

/********************************* Constants **********************************/
//#define EASTER

#define PAGE_NONE  0
#define PAGE_DAY   1
#define PAGE_WEEK  2
#define PAGE_MONTH 3
#define PAGE_YEAR  4
#define BEGIN_DATE_BUTTON  5

/* Maximum length of description (not including the null) */
/* todo check this before every record write */
#define MAX_DESC_LEN 255
#define DATEBOOK_MAX_COLUMN_LEN 80

//todo: these need to be replaced with
// the enum.  treeview can always have all 5 columns
// and just show/hide the 'float' column
// as needed.
#ifdef ENABLE_DATEBK
static int DB_APPT_COLUMN = 4;
#else
static int DB_APPT_COLUMN=3;
#endif

#define NUM_DATEBOOK_CAT_ITEMS 16
#define NUM_EXPORT_TYPES 3
#define NUM_DBOOK_CSV_FIELDS 19
#define NUM_CALENDAR_CSV_FIELDS 20
/* RFCs use CRLF for Internet newline */
#define CRLF "\x0D\x0A"

#define BROWSE_OK     1
#define BROWSE_CANCEL 2

#define CONNECT_SIGNALS 400
#define DISCONNECT_SIGNALS 401

#define CAL_DAY_SELECTED 327

#define UPDATE_DATE_ENTRIES 0x01
#define UPDATE_DATE_MENUS   0x02

#define START_TIME_FLAG 0x00
#define END_TIME_FLAG   0x80
#define HOURS_FLAG      0x40


/* #define DAY_VIEW */

/******************************* Global vars **********************************/
/* Keeps track of whether code is using Datebook, or Calendar database
 * 0 is Datebook, 1 is Calendar */
static long datebook_version = 0;

/* This refers to the main jpilot window.  This should probably
 * be replaced somehow by a GTK call which works out what the
 * top-level window is from the widget.  Right now it relies
 * on the fact that there is only one item titled "window" in
 * the global name space */
extern GtkWidget *window;
extern GtkWidget *glob_date_label;
extern gint glob_date_timer_tag;

static GtkWidget *pane;
static GtkWidget *note_pane;
static GtkWidget *todo_pane;
static GtkWidget *todo_vbox;

static struct CalendarAppInfo dbook_app_info;
static int dbook_category = CATEGORY_ALL;
static struct sorted_cats sort_l[NUM_DATEBOOK_CAT_ITEMS];

static GtkWidget *main_calendar;
static GtkWidget *dow_label;
static GtkTreeView *treeView;
static GtkListStore *listStore;
static GtkWidget *dbook_desc, *dbook_note;
static GObject *dbook_desc_buffer, *dbook_note_buffer;
static GtkWidget *category_menu1;
static GtkWidget *category_menu2;
static GtkWidget *private_checkbox;
static GtkWidget *check_button_alarm;
static GtkWidget *check_button_day_endon;
static GtkWidget *check_button_week_endon;
static GtkWidget *check_button_mon_endon;
static GtkWidget *check_button_year_endon;
static GtkWidget *units_entry;
static GtkWidget *repeat_day_entry;
static GtkWidget *repeat_week_entry;
static GtkWidget *repeat_mon_entry;
static GtkWidget *repeat_year_entry;
static GtkWidget *radio_button_no_time;
static GtkWidget *radio_button_appt_time;
static GtkWidget *radio_button_alarm_min;
static GtkWidget *radio_button_alarm_hour;
static GtkWidget *radio_button_alarm_day;
static GtkWidget *location_entry;
static GtkWidget *glob_endon_day_button;
static struct tm glob_endon_day_tm;
static GtkWidget *glob_endon_week_button;
static struct tm glob_endon_week_tm;
static GtkWidget *glob_endon_mon_button;
static struct tm glob_endon_mon_tm;
static GtkWidget *glob_endon_year_button;
static struct tm glob_endon_year_tm;
static GtkWidget *toggle_button_repeat_days[7];
static GtkWidget *toggle_button_repeat_mon_byday;
static GtkWidget *toggle_button_repeat_mon_bydate;
static GtkWidget *notebook;
static int current_day;   /* range 1-31 */
static int current_month; /* range 0-11 */
static int current_year;  /* years since 1900 */
static int row_selected;
static int record_changed;
#ifdef ENABLE_DATEBK
int datebk_category = 0xFFFF; /* This is a bitmask */
static GtkWidget *datebk_entry;
#endif

static GtkWidget *hbox_alarm1, *hbox_alarm2;

static GtkWidget *scrolled_window;

static struct tm begin_date, end_date;
static GtkWidget *option1, *option2, *option3, *option4;
static GtkWidget *begin_date_button;
static GtkWidget *begin_time_entry, *end_time_entry;

static GtkWidget *new_record_button;
static GtkWidget *apply_record_button;
static GtkWidget *add_record_button;
static GtkWidget *delete_record_button;
static GtkWidget *undelete_record_button;
static GtkWidget *copy_record_button;
static GtkWidget *cancel_record_button;

static GtkAccelGroup *accel_group;

static CalendarEventList *glob_cel = NULL;
/*
 * #define DB_TIME_COLUMN  0
#define DB_NOTE_COLUMN  1
#define DB_ALARM_COLUMN 2
#ifdef ENABLE_DATEBK
#  define DB_FLOAT_COLUMN 3
static int DB_APPT_COLUMN = 4;
#else
static int DB_APPT_COLUMN=3;
#endif
 */
enum {
    DATE_TIME_COLUMN_ENUM = 0,
    DATE_NOTE_COLUMN_ENUM,
    DATE_ALARM_COLUMN_ENUM,
    DATE_FLOAT_COLUMN_ENUM,
    DATE_APPT_COLUMN_ENUM,
    DATE_DATA_COLUMN_ENUM,
    DATE_BACKGROUND_COLOR_ENUM,
    DATE_BACKGROUND_COLOR_ENABLED_ENUM,
    DATE_NUM_COLS
};

/* For todo list */
static GtkTreeView *todo_treeView;
static GtkListStore *todo_listStore;
static GtkTreeSelection *todo_treeSelection;
static GtkWidget *show_todos_button;
static GtkWidget *todo_scrolled_window;
static ToDoList *datebook_todo_list = NULL;

/* For export GUI */
static GtkWidget *export_window;
static GtkWidget *save_as_entry;
static GtkWidget *export_radio_type[NUM_EXPORT_TYPES + 1];
static int glob_export_type;

/****************************** Prototypes ************************************/
static void highlight_days(void);

static int datebook_find(void);

static int datebook_update_listStore(void);

static void update_endon_button(GtkWidget *button, struct tm *t);

static void cb_add_new_record(GtkWidget *widget,
                              gpointer data);

static void set_new_button_to(int new_state);

static void connect_changed_signals(int con_or_dis);

static int datebook_export_gui(GtkWidget *main_window, int x, int y);

void selectFirstTodoRow();

gboolean
findDateRecord(GtkTreeModel *model,
               GtkTreePath *path,
               GtkTreeIter *iter,
               gpointer data);

gboolean
addNewDateRecord(GtkTreeModel *model,
                 GtkTreePath *path,
                 GtkTreeIter *iter,
                 gpointer data);

static gboolean handleDateRowSelection(GtkTreeSelection *selection,
                                       GtkTreeModel *model,
                                       GtkTreePath *path,
                                       gboolean path_currently_selected,
                                       gpointer userdata);

gboolean undeleteDateRecord(GtkTreeModel *model,
                            GtkTreePath *path,
                            GtkTreeIter *iter,
                            gpointer data);

gboolean
selectDateRecordByRow(GtkTreeModel *model,
                      GtkTreePath *path,
                      GtkTreeIter *iter,
                      gpointer data);

gboolean
deleteDateRecord(GtkTreeModel *model,
                 GtkTreePath *path,
                 GtkTreeIter *iter,
                 gpointer data);

void addNewDateRecordToDataStructure(MyCalendarEvent *mcale, gpointer data);

void deleteDateRecordFromDataStructure(MyCalendarEvent *mcale, gpointer data);

void undeleteDate(MyCalendarEvent *mcale, gpointer data);

gboolean
clickedTodoButton(GtkTreeSelection *selection,
                  GtkTreeModel *model,
                  GtkTreePath *path,
                  gboolean path_currently_selected,
                  gpointer userdata);

gint cb_todo_treeview_selection_event(GtkWidget *widget,
                                      GdkEvent *event,
                                      gpointer callback_data);

void buildToDoList(const GtkWidget *vbox);

void
buildTreeView(const GtkWidget *vbox, char *const *titles, long use_db3_tags);

/****************************** Main Code *************************************/
static int datebook_to_text(struct CalendarEvent *cale, char *text, int len) {
    int i;
    const char *short_date;
    const char *pref_time;
    char temp[255];
    char text_time[200];
    char str_begin_date[20];
    char str_begin_time[20];
    char str_end_time[20];
    char text_repeat_type[40];
    char text_repeat_day[200];
    char text_end_date[200];
    char text_repeat_freq[200];
    char text_alarm[40];
    char text_repeat_days[200];
    char text_exceptions[65535];
    char *adv_type[] = {
            N_("Minutes"),
            N_("Hours"),
            N_("Days")
    };
    char *repeat_type[] = {
            N_("Repeat Never"),
            N_("Repeat Daily"),
            N_("Repeat Weekly"),
            N_("Repeat MonthlyByDay"),
            N_("Repeat MonthlyByDate"),
            N_("Repeat YearlyDate"),
            N_("Repeat YearlyDay")
    };
    char *days[] = {
            N_("Su"),
            N_("Mo"),
            N_("Tu"),
            N_("We"),
            N_("Th"),
            N_("Fr"),
            N_("Sa"),
            N_("Su")
    };

    if ((cale->repeatWeekstart < 0) || (cale->repeatWeekstart > 6)) {
        cale->repeatWeekstart = 0;
    }
    get_pref(PREF_SHORTDATE, NULL, &short_date);
    get_pref(PREF_TIME, NULL, &pref_time);

    /* Event date/time */
    strftime(str_begin_date, sizeof(str_begin_date), short_date, &(cale->begin));
    if (cale->event) {
        sprintf(text_time, _("Start Date: %s\nTime: Event"),
                str_begin_date);
    } else {
        strftime(str_begin_time, sizeof(str_begin_time), pref_time, &(cale->begin));
        strftime(str_end_time, sizeof(str_end_time), pref_time, &(cale->end));
        str_begin_date[19] = '\0';
        str_begin_time[19] = '\0';
        str_end_time[19] = '\0';
        sprintf(text_time, _("Start Date: %s\nTime: %s to %s"),
                str_begin_date, str_begin_time, str_end_time);
    }
    /* Alarm */
    if (cale->alarm) {
        sprintf(text_alarm, " %d ", cale->advance);
        i = cale->advanceUnits;
        if ((i > -1) && (i < 3)) {
            strcat(text_alarm, adv_type[i]);
        } else {
            strcat(text_alarm, _("Unknown"));
        }
    } else {
        text_alarm[0] = '\0';
    }
    /* Repeat Type */
    i = cale->repeatType;
    if ((i > -1) && (i < 7)) {
        strcpy(text_repeat_type, _(repeat_type[i]));
    } else {
        strcpy(text_repeat_type, _("Unknown"));
    }
    /* End Date */
    strcpy(text_end_date, _("End Date: "));
    if (cale->repeatForever) {
        strcat(text_end_date, _("Never"));
    } else {
        strftime(temp, sizeof(temp), short_date, &(cale->repeatEnd));
        strcat(text_end_date, temp);
    }
    strcat(text_end_date, "\n");
    sprintf(text_repeat_freq, _("Repeat Frequency: %d\n"), cale->repeatFrequency);
    if (cale->repeatType == calendarRepeatNone) {
        text_end_date[0] = '\0';
        text_repeat_freq[0] = '\0';
    }
    /* Repeat Day (for MonthlyByDay) */
    text_repeat_day[0] = '\0';
    if (cale->repeatType == calendarRepeatMonthlyByDay) {
        sprintf(text_repeat_day, _("Monthly Repeat Day %d\n"), cale->repeatDay);
    }
    /* Repeat Days (for weekly) */
    text_repeat_days[0] = '\0';
    if (cale->repeatType == calendarRepeatWeekly) {
        strcpy(text_repeat_days, _("Repeat on Days:"));
        for (i = 0; i < 7; i++) {
            if (cale->repeatDays[i]) {
                strcat(text_repeat_days, " ");
                strcat(text_repeat_days, _(days[i]));
            }
        }
        strcat(text_repeat_days, "\n");
    }
    text_exceptions[0] = '\0';
    if (cale->exceptions > 0) {
        sprintf(text_exceptions, _("Number of exceptions: %d"), cale->exceptions);
        for (i = 0; i < cale->exceptions; i++) {
            strcat(text_exceptions, "\n");
            strftime(temp, sizeof(temp), short_date, &(cale->exception[i]));
            strcat(text_exceptions, temp);
            if (strlen(text_exceptions) > 65000) {
                strcat(text_exceptions, _("\nmore..."));
                break;
            }
        }
        strcat(text_exceptions, "\n");
    }

    if (datebook_version == 0) {
        /* DateBook app */
        g_snprintf(text, (gulong) len,
                   "%s %s\n"
                   "%s %s\n"
                   "%s\n"
                   "%s %s%s\n"
                   "%s %s\n"
                   "%s"
                   "%s"
                   "%s %s\n"
                   "%s"
                   "%s"
                   "%s",
                   _("Description:"), cale->description,
                   _("Note:"), (cale->note ? cale->note : ""),
                   text_time,
                   _("Alarm:"), cale->alarm ? _("Yes") : _("No"), text_alarm,
                   _("Repeat Type:"), text_repeat_type,
                   text_repeat_freq,
                   text_end_date,
                   _("Start of Week:"), _(days[cale->repeatWeekstart]),
                   text_repeat_day,
                   text_repeat_days,
                   text_exceptions
        );
    } else {
        /* Calendar app */
        g_snprintf(text, (gulong) len,
                   "%s %s\n"
                   "%s %s\n"
                   "%s %s\n"
                   "%s\n"
                   "%s %s%s\n"
                   "%s %s\n"
                   "%s"
                   "%s"
                   "%s %s\n"
                   "%s"
                   "%s"
                   "%s",
                   _("Description:"), cale->description,
                   _("Note:"), (cale->note ? cale->note : ""),
                   _("Location:"), (cale->location ? cale->location : ""),
                   text_time,
                   _("Alarm:"), cale->alarm ? _("Yes") : _("No"), text_alarm,
                   _("Repeat Type:"), text_repeat_type,
                   text_repeat_freq,
                   text_end_date,
                   _("Start of Week:"), _(days[cale->repeatWeekstart]),
                   text_repeat_day,
                   text_repeat_days,
                   text_exceptions
        );


    }
    return EXIT_SUCCESS;
}

/*************** Start Import Code ***************/

static int cb_dbook_import(GtkWidget *parent_window, const char *file_path, int type) {
    FILE *in;
    char text[65536];
    char description[65536];
    char note[65536];
    char location[65536];
    struct CalendarEvent new_cale;
    unsigned char attrib;
    int i, str_i, ret, index;
    int import_all;
    AppointmentList *alist;
    CalendarEventList *celist;
    CalendarEventList *temp_celist;
    struct CategoryAppInfo cai;
    char old_cat_name[32];
    int suggested_cat_num;
    int new_cat_num;
    int priv;
    int year, month, day, hour, minute;

    in = fopen(file_path, "r");
    if (!in) {
        jp_logf(JP_LOG_WARN, _("Unable to open file: %s\n"), file_path);
        return EXIT_FAILURE;
    }
    /* CSV */
    if (type == IMPORT_TYPE_CSV) {
        jp_logf(JP_LOG_DEBUG, "Datebook import CSV [%s]\n", file_path);
        /* Get the first line containing the format and check for reasonableness */
        if (fgets(text, sizeof(text), in) == NULL) {
            jp_logf(JP_LOG_WARN, "fgets failed %s %d\n", __FILE__, __LINE__);
        }
        if (datebook_version == 0) {
            ret = verify_csv_header(text, NUM_DBOOK_CSV_FIELDS, file_path);
        } else {
            ret = verify_csv_header(text, NUM_CALENDAR_CSV_FIELDS, file_path);
        }
        if (EXIT_FAILURE == ret) return EXIT_FAILURE;

        import_all = FALSE;
        while (1) {
            memset(&new_cale, 0, sizeof(new_cale));
            /* Read the category field */
            read_csv_field(in, text, sizeof(text));
            if (feof(in)) break;
#ifdef JPILOT_DEBUG
            printf("category is [%s]\n", text);
#endif
            g_strlcpy(old_cat_name, text, 16);
            /* Figure out what the best category number is */
            suggested_cat_num = 0;
            for (i = 0; i < NUM_DATEBOOK_CAT_ITEMS; i++) {
                if (!dbook_app_info.category.name[i][0]) continue;
                if (!strcmp(dbook_app_info.category.name[i], old_cat_name)) {
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

            /* Description */
            read_csv_field(in, description, sizeof(description));
            if (strlen(description) > 0) {
                new_cale.description = description;
            } else {
                new_cale.description = NULL;
            }

            /* Note */
            read_csv_field(in, note, sizeof(note));
            if (strlen(note) > 0) {
                new_cale.note = note;
            } else {
                new_cale.note = NULL;
            }

            if (datebook_version) {
                /* Location */
                read_csv_field(in, location, sizeof(location));
                if (strlen(location) > 0) {
                    new_cale.location = location;
                } else {
                    new_cale.location = NULL;
                }
            }

            /* Event */
            read_csv_field(in, text, sizeof(text));
            sscanf(text, "%d", &(new_cale.event));

            /* Begin Time */
            memset(&(new_cale.begin), 0, sizeof(new_cale.begin));
            read_csv_field(in, text, sizeof(text));
            sscanf(text, "%d %d %d %d:%d", &year, &month, &day, &hour, &minute);
            new_cale.begin.tm_year = year - 1900;
            new_cale.begin.tm_mon = month - 1;
            new_cale.begin.tm_mday = day;
            new_cale.begin.tm_hour = hour;
            new_cale.begin.tm_min = minute;
            new_cale.begin.tm_isdst = -1;
            mktime(&(new_cale.begin));

            /* End Time */
            memset(&(new_cale.end), 0, sizeof(new_cale.end));
            read_csv_field(in, text, sizeof(text));
            sscanf(text, "%d %d %d %d:%d", &year, &month, &day, &hour, &minute);
            new_cale.end.tm_year = year - 1900;
            new_cale.end.tm_mon = month - 1;
            new_cale.end.tm_mday = day;
            new_cale.end.tm_hour = hour;
            new_cale.end.tm_min = minute;
            new_cale.end.tm_isdst = -1;
            mktime(&(new_cale.end));

            /* Alarm */
            read_csv_field(in, text, sizeof(text));
            sscanf(text, "%d", &(new_cale.alarm));

            /* Alarm Advance */
            read_csv_field(in, text, sizeof(text));
            sscanf(text, "%d", &(new_cale.advance));

            /* Advance Units */
            read_csv_field(in, text, sizeof(text));
            sscanf(text, "%d", &(new_cale.advanceUnits));

            /* Repeat Type */
            read_csv_field(in, text, sizeof(text));
            sscanf(text, "%d", &(i));
            new_cale.repeatType = (enum calendarRepeatType) i;

            /* Repeat Forever */
            read_csv_field(in, text, sizeof(text));
            sscanf(text, "%d", &(new_cale.repeatForever));

            /* Repeat End */
            memset(&(new_cale.repeatEnd), 0, sizeof(new_cale.repeatEnd));
            read_csv_field(in, text, sizeof(text));
            sscanf(text, "%d %d %d", &year, &month, &day);
            new_cale.repeatEnd.tm_year = year - 1900;
            new_cale.repeatEnd.tm_mon = month - 1;
            new_cale.repeatEnd.tm_mday = day;
            new_cale.repeatEnd.tm_isdst = -1;
            mktime(&(new_cale.repeatEnd));

            /* Repeat Frequency */
            read_csv_field(in, text, sizeof(text));
            sscanf(text, "%d", &(new_cale.repeatFrequency));

            /* Repeat Day */
            read_csv_field(in, text, sizeof(text));
            sscanf(text, "%d", &(i));
            new_cale.repeatDay = (enum calendarDayOfMonthType) i;

            /* Repeat Days */
            read_csv_field(in, text, sizeof(text));
            for (i = 0; i < 7; i++) {
                new_cale.repeatDays[i] = (text[i] == '1');
            }

            /* Week Start */
            read_csv_field(in, text, sizeof(text));
            sscanf(text, "%d", &(new_cale.repeatWeekstart));

            /* Number of Exceptions */
            read_csv_field(in, text, sizeof(text));
            sscanf(text, "%d", &(new_cale.exceptions));

            /* Exceptions */
            ret = read_csv_field(in, text, sizeof(text));
            new_cale.exception = calloc((size_t) new_cale.exceptions, sizeof(struct tm));
            for (str_i = 0, i = 0; i < new_cale.exceptions; i++) {
                sscanf(&(text[str_i]), "%d %d %d", &year, &month, &day);
                new_cale.exception[i].tm_year = year - 1900;
                new_cale.exception[i].tm_mon = month - 1;
                new_cale.exception[i].tm_mday = day;
                new_cale.exception[i].tm_isdst = -1;
                mktime(&(new_cale.exception[i]));
                for (; (str_i < sizeof(text)) && (text[str_i]); str_i++) {
                    if (text[str_i] == ',') {
                        str_i++;
                        break;
                    }
                }
            }

            datebook_to_text(&new_cale, text, 65535);
            if (!import_all) {
                ret = import_record_ask(parent_window, pane,
                                        text,
                                        &(dbook_app_info.category),
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

            attrib = (unsigned char) ((new_cat_num & 0x0F) |
                                      (priv ? dlpRecAttrSecret : 0));
            if ((ret == DIALOG_SAID_IMPORT_YES) || (import_all)) {
                if (strlen(new_cale.description) + 1 > MAX_DESC_LEN) {
                    new_cale.description[MAX_DESC_LEN + 1] = '\0';
                    jp_logf(JP_LOG_WARN, _("Appointment description text > %d, truncating to %d\n"), MAX_DESC_LEN,
                            MAX_DESC_LEN);
                }
                pc_calendar_write(&new_cale, NEW_PC_REC, attrib, NULL);
            }
        }
    }

    /* Palm Desktop DAT format */
    if (type == IMPORT_TYPE_DAT) {
        jp_logf(JP_LOG_DEBUG, "Datebook import DAT [%s]\n", file_path);
        if (dat_check_if_dat_file(in) != DAT_DATEBOOK_FILE) {
            dialog_generic_ok(notebook, _("Error"), DIALOG_ERROR,
                              _("File doesn't appear to be datebook.dat format\n"));
            fclose(in);
            return EXIT_FAILURE;
        }
        alist = NULL;
        dat_get_appointments(in, &alist, &cai);

        /* Copy this to a calendar event list */
        copy_appointments_to_calendarEvents(alist, &celist);
        free_AppointmentList(&alist);

        import_all = FALSE;
        for (temp_celist = celist; temp_celist; temp_celist = temp_celist->next) {
            index = temp_celist->mcale.unique_id - 1;
            if (index < 0) {
                g_strlcpy(old_cat_name, _("Unfiled"), 16);
            } else {
                g_strlcpy(old_cat_name, cai.name[index], 16);
            }
            /* Figure out what category it was in the dat file */
            index = temp_celist->mcale.unique_id - 1;
            suggested_cat_num = 0;
            if (index > -1) {
                for (i = 0; i < NUM_DATEBOOK_CAT_ITEMS; i++) {
                    if (!dbook_app_info.category.name[i][0]) continue;
                    if (!strcmp(dbook_app_info.category.name[i], old_cat_name)) {
                        suggested_cat_num = i;
                        break;
                    }
                }
            }

            ret = 0;
            if (!import_all) {
                datebook_to_text(&(temp_celist->mcale.cale), text, 65535);
                ret = import_record_ask(parent_window, pane,
                                        text,
                                        &(dbook_app_info.category),
                                        old_cat_name,
                                        (temp_celist->mcale.attrib & 0x10),
                                        suggested_cat_num,
                                        &new_cat_num);
            } else {
                new_cat_num = suggested_cat_num;
            }
            if (ret == DIALOG_SAID_IMPORT_QUIT) break;
            if (ret == DIALOG_SAID_IMPORT_SKIP) continue;
            if (ret == DIALOG_SAID_IMPORT_ALL) import_all = TRUE;

            attrib = (unsigned char) ((new_cat_num & 0x0F) |
                                      ((temp_celist->mcale.attrib & 0x10) ? dlpRecAttrSecret : 0));
            if ((ret == DIALOG_SAID_IMPORT_YES) || (import_all)) {
                pc_calendar_write(&(temp_celist->mcale.cale), NEW_PC_REC, attrib, NULL);
            }
        }
        free_CalendarEventList(&celist);
    }

    datebook_refresh(FALSE, TRUE);
    fclose(in);
    return EXIT_SUCCESS;
}

int datebook_import(GtkWidget *window) {
    char *type_desc[] = {
            N_("CSV (Comma Separated Values)"),
            N_("DAT/DBA (Palm Archive Formats)"),
            NULL
    };
    int type_int[] = {
            IMPORT_TYPE_CSV,
            IMPORT_TYPE_DAT,
            0
    };

    /* Hide ABA import of CalendarDB until file format has been decoded */
    if (datebook_version == 1) {
        type_desc[1] = NULL;
        type_int[1] = 0;
    }

    import_gui(window, pane, type_desc, type_int, cb_dbook_import);
    return EXIT_SUCCESS;
}
/*** End Import Code ***/

/*************** Start Export Code ***************/

/* TODO rename */
static void appt_export_ok(int type, const char *filename) {
    MyCalendarEvent *mcale;
    CalendarEventList *cel, *temp_list;
    FILE *out;
    struct stat statb;
    int i, r;
    char *button_text[] = {N_("OK")};
    char *button_overwrite_text[] = {N_("No"), N_("Yes")};
    char text[1024];
    char csv_text[65550];
    char *p;
    gchar *end;
    time_t ltime;
    struct tm *now = NULL;
    struct tm ical_time;
    long char_set;
    char username[256];
    char hostname[256];
    const char *svalue;
    long userid = 0;
    const char *short_date;
    char pref_time[40];
    char str1[256], str2[256];
    char date_string[1024];
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
        if (datebook_version == 0) {
            fprintf(out, _("Datebook exported from %s %s on %s\n\n"),
                    PN, VERSION, date_string);
        } else {
            fprintf(out, _("Calendar exported from %s %s on %s\n\n"),
                    PN, VERSION, date_string);
        }
    }

    /* Write a header to the CSV file */
    if (type == EXPORT_TYPE_CSV) {
        if (datebook_version == 0) {
            fprintf(out, "CSV datebook version "VERSION": Category, Private, "
                         "Description, Note, Event, Begin, End, Alarm, Advance, "
                         "Advance Units, Repeat Type, Repeat Forever, Repeat End, "
                         "Repeat Frequency, Repeat Day, Repeat Days, "
                         "Week Start, Number of Exceptions, Exceptions\n");
        } else {
            fprintf(out, "CSV calendar version "VERSION": Category, Private, "
                         "Description, Note, Location, "
                         "Event, Begin, End, Alarm, Advance, "
                         "Advance Units, Repeat Type, Repeat Forever, Repeat End, "
                         "Repeat Frequency, Repeat Day, Repeat Days, "
                         "Week Start, Number of Exceptions, Exceptions\n");
        }
    }

    /* Special setup for ICAL export */
    if (type == EXPORT_TYPE_ICALENDAR) {
        get_pref(PREF_CHAR_SET, &char_set, NULL);
        if (char_set < CHAR_SET_UTF) {
            jp_logf(JP_LOG_WARN, _("Host character encoding is not UTF-8 based.\n"
                                   " Exported ical file may not be standards-compliant\n"));
        }

        /* Convert User Name stored in Palm character set */
        get_pref(PREF_USER, NULL, &svalue);
        g_strlcpy(text, svalue, 128);
        text[127] = '\0';
        charset_p2j(text, 128, (int) char_set);
        str_to_ical_str(username, sizeof(username), text);
        get_pref(PREF_USER_ID, &userid, NULL);
        gethostname(text, sizeof(hostname));
        text[sizeof(hostname) - 1] = '\0';
        str_to_ical_str(hostname, sizeof(hostname), text);
        time(&ltime);
        now = gmtime(&ltime);
    }

    get_pref(PREF_CHAR_SET, &char_set, NULL);

    cel = NULL;
    get_days_calendar_events2(&cel, NULL, 2, 2, 2, CATEGORY_ALL, NULL);

    for (i = 0, temp_list = cel; temp_list; temp_list = temp_list->next, i++) {
        mcale = &(temp_list->mcale);
        switch (type) {
            case EXPORT_TYPE_TEXT:
                csv_text[0] = '\0';
                datebook_to_text(&(mcale->cale), csv_text, sizeof(csv_text));
                fprintf(out, "%s\n", csv_text);
                break;

            case EXPORT_TYPE_CSV:
                if (datebook_version == 0) {
                    fprintf(out, "\"\",");  /* No category for Datebook */
                } else {
                    utf = charset_p2newj(dbook_app_info.category.name[mcale->attrib & 0x0F], 16, (int) char_set);
                    str_to_csv_str(csv_text, utf);
                    fprintf(out, "\"%s\",", csv_text);
                    g_free(utf);
                }

                fprintf(out, "\"%s\",", (mcale->attrib & dlpRecAttrSecret) ? "1" : "0");

                str_to_csv_str(csv_text, mcale->cale.description);
                fprintf(out, "\"%s\",", csv_text);

                str_to_csv_str(csv_text, mcale->cale.note);
                fprintf(out, "\"%s\",", csv_text);

                if (datebook_version) {
                    str_to_csv_str(csv_text, mcale->cale.location);
                    fprintf(out, "\"%s\",", csv_text);
                }

                fprintf(out, "\"%d\",", mcale->cale.event);

                fprintf(out, "\"%d %02d %02d  %02d:%02d\",",
                        mcale->cale.begin.tm_year + 1900,
                        mcale->cale.begin.tm_mon + 1,
                        mcale->cale.begin.tm_mday,
                        mcale->cale.begin.tm_hour,
                        mcale->cale.begin.tm_min);
                fprintf(out, "\"%d %02d %02d  %02d:%02d\",",
                        mcale->cale.end.tm_year + 1900,
                        mcale->cale.end.tm_mon + 1,
                        mcale->cale.end.tm_mday,
                        mcale->cale.end.tm_hour,
                        mcale->cale.end.tm_min);

                fprintf(out, "\"%s\",", (mcale->cale.alarm) ? "1" : "0");
                fprintf(out, "\"%d\",", mcale->cale.advance);
                fprintf(out, "\"%d\",", mcale->cale.advanceUnits);

                fprintf(out, "\"%d\",", mcale->cale.repeatType);

                if (mcale->cale.repeatType == calendarRepeatNone) {
                    /* Single events don't have valid repeat data fields so
             * a standard output data template is used for them */
                    fprintf(out, "\"0\",\"1970 01 01\",\"0\",\"0\",\"0\",\"0\",\"0\",\"");
                } else {
                    fprintf(out, "\"%d\",", mcale->cale.repeatForever);

                    if (mcale->cale.repeatForever) {
                        /* repeatForever events don't have valid end date fields
                * so a standard output date is used for them */
                        fprintf(out, "\"1970 01 01\",");
                    } else {
                        fprintf(out, "\"%d %02d %02d\",",
                                mcale->cale.repeatEnd.tm_year + 1900,
                                mcale->cale.repeatEnd.tm_mon + 1,
                                mcale->cale.repeatEnd.tm_mday);
                    }

                    fprintf(out, "\"%d\",", mcale->cale.repeatFrequency);

                    fprintf(out, "\"%d\",", mcale->cale.repeatDay);

                    fprintf(out, "\"");
                    for (i = 0; i < 7; i++) {
                        fprintf(out, "%d", mcale->cale.repeatDays[i]);
                    }
                    fprintf(out, "\",");

                    fprintf(out, "\"%d\",", mcale->cale.repeatWeekstart);

                    fprintf(out, "\"%d\",", mcale->cale.exceptions);

                    fprintf(out, "\"");
                    if (mcale->cale.exceptions > 0) {
                        for (i = 0; i < mcale->cale.exceptions; i++) {
                            if (i > 0) {
                                fprintf(out, ",");
                            }
                            fprintf(out, "%d %02d %02d",
                                    mcale->cale.exception[i].tm_year + 1900,
                                    mcale->cale.exception[i].tm_mon + 1,
                                    mcale->cale.exception[i].tm_mday);
                        }
                    }   /* if for exceptions */
                }   /* else for repeat event */
                fprintf(out, "\"\n");
                break;

            case EXPORT_TYPE_ICALENDAR:
                /* RFC 2445: Internet Calendaring and Scheduling Core
          *           Object Specification */
                if (i == 0) {
                    fprintf(out, "BEGIN:VCALENDAR"CRLF);
                    fprintf(out, "VERSION:2.0"CRLF);
                    fprintf(out, "PRODID:%s"CRLF, FPI_STRING);
                }
                fprintf(out, "BEGIN:VEVENT"CRLF);
                /* XXX maybe if it's secret export a VFREEBUSY busy instead? */
                if (mcale->attrib & dlpRecAttrSecret) {
                    fprintf(out, "CLASS:PRIVATE"CRLF);
                }
                fprintf(out, "UID:palm-datebook-%08x-%08lx-%s@%s"CRLF,
                        mcale->unique_id, userid, username, hostname);
                fprintf(out, "DTSTAMP:%04d%02d%02dT%02d%02d%02dZ"CRLF,
                        now->tm_year + 1900,
                        now->tm_mon + 1,
                        now->tm_mday,
                        now->tm_hour,
                        now->tm_min,
                        now->tm_sec);
                if (datebook_version) {
                    /* Calendar supports categories and locations */
                    utf = charset_p2newj(dbook_app_info.category.name[mcale->attrib & 0x0F], 16, (int) char_set);
                    str_to_ical_str(text, sizeof(text), utf);
                    fprintf(out, "CATEGORIES:%s"CRLF, text);
                    g_free(utf);
                    if (mcale->cale.location) {
                        str_to_ical_str(text, sizeof(text), mcale->cale.location);
                        fprintf(out, "LOCATION:%s"CRLF, text);
                    }
                }
                /* Create truncated description for use in SUMMARY field */
                if (mcale->cale.description) {
                    g_strlcpy(text, mcale->cale.description, 51);
                    /* truncate the string on a UTF-8 character boundary */
                    if (char_set > CHAR_SET_UTF) {
                        if (!g_utf8_validate(text, -1, (const gchar **) &end))
                            *end = 0;
                    }
                } else {
                    /* Handle pathological case with null description. */
                    text[0] = '\0';
                }
                if ((p = strchr(text, '\n'))) {
                    *p = '\0';
                }
                str_to_ical_str(csv_text, sizeof(csv_text), text);
                fprintf(out, "SUMMARY:%s%s"CRLF, csv_text,
                        strlen(text) > 49 ? "..." : "");
                str_to_ical_str(csv_text, sizeof(csv_text), mcale->cale.description);
                fprintf(out, "DESCRIPTION:%s", csv_text);
                if (mcale->cale.note && mcale->cale.note[0]) {
                    str_to_ical_str(csv_text, sizeof(csv_text), mcale->cale.note);
                    fprintf(out, "\\n"CRLF" %s"CRLF, csv_text);
                } else {
                    fprintf(out, CRLF);
                }
                if (mcale->cale.event) {
                    fprintf(out, "DTSTART;VALUE=DATE:%04d%02d%02d"CRLF,
                            mcale->cale.begin.tm_year + 1900,
                            mcale->cale.begin.tm_mon + 1,
                            mcale->cale.begin.tm_mday);
                    /* XXX unclear: can "event" span multiple days? */
                    /* since DTEND is "noninclusive", should this be the next day? */
                    if (mcale->cale.end.tm_year != mcale->cale.begin.tm_year ||
                        mcale->cale.end.tm_mon != mcale->cale.begin.tm_mon ||
                        mcale->cale.end.tm_mday != mcale->cale.begin.tm_mday) {
                        fprintf(out, "DTEND;VALUE=DATE:%04d%02d%02d"CRLF,
                                mcale->cale.end.tm_year + 1900,
                                mcale->cale.end.tm_mon + 1,
                                mcale->cale.end.tm_mday);
                    }
                } else {
                    /*
             * These are "local" times, so will be treated as being in
             * the other person's timezone when they are imported.  This
             * may or may not be what is desired.  (DateBk calls this
             * "all time zones").
             *
             * DateBk timezones could help us decide what to do here.
             *
             * When using DateBk timezones, we could write them out
             * as iCalendar timezones.
             *
             * Maybe the default should be to write an absolute (UTC) time,
             * and only write a "local" time when using DateBk and it says to.
             * It'd be interesting to see if repeated events get translated
             * properly when doing this, or if they become not eligible for
             * daylight savings.  This probably depends on the importing
             * application.
             */
                    fprintf(out, "DTSTART:%04d%02d%02dT%02d%02d00"CRLF,
                            mcale->cale.begin.tm_year + 1900,
                            mcale->cale.begin.tm_mon + 1,
                            mcale->cale.begin.tm_mday,
                            mcale->cale.begin.tm_hour,
                            mcale->cale.begin.tm_min);
                    fprintf(out, "DTEND:%04d%02d%02dT%02d%02d00"CRLF,
                            mcale->cale.end.tm_year + 1900,
                            mcale->cale.end.tm_mon + 1,
                            mcale->cale.end.tm_mday,
                            mcale->cale.end.tm_hour,
                            mcale->cale.end.tm_min);
                }
                if (mcale->cale.repeatType != calendarRepeatNone) {
                    int wcomma, rptday;
                    char *wday[] = {"SU", "MO", "TU", "WE", "TH", "FR", "SA"};
                    fprintf(out, "RRULE:FREQ=");
                    switch (mcale->cale.repeatType) {
                        case calendarRepeatNone:
                            /* can't happen, just here to silence compiler warning */
                            break;
                        case calendarRepeatDaily:
                            fprintf(out, "DAILY");
                            break;
                        case calendarRepeatWeekly:
                            fprintf(out, "WEEKLY;BYDAY=");
                            wcomma = 0;
                            for (i = 0; i < 7; i++) {
                                if (mcale->cale.repeatDays[i]) {
                                    if (wcomma) {
                                        fprintf(out, ",");
                                    }
                                    wcomma = 1;
                                    fprintf(out, "%s", wday[i]);
                                }
                            }
                            break;
                        case calendarRepeatMonthlyByDay:
                            rptday = (mcale->cale.repeatDay / 7) + 1;
                            fprintf(out, "MONTHLY;BYDAY=%d%s", rptday == 5 ? -1 : rptday,
                                    wday[mcale->cale.repeatDay % 7]);
                            break;
                        case calendarRepeatMonthlyByDate:
                            fprintf(out, "MONTHLY;BYMONTHDAY=%d", mcale->cale.begin.tm_mday);
                            break;
                        case calendarRepeatYearly:
                            fprintf(out, "YEARLY");
                            break;
                    }
                    if (mcale->cale.repeatFrequency != 1) {
                        if (mcale->cale.repeatType == calendarRepeatWeekly &&
                            mcale->cale.repeatWeekstart >= 0 && mcale->cale.repeatWeekstart < 7) {
                            fprintf(out, CRLF" ");  /* Weekly repeats can exceed RFC line length */
                            fprintf(out, ";WKST=%s", wday[mcale->cale.repeatWeekstart]);
                        }
                        fprintf(out, ";INTERVAL=%d", mcale->cale.repeatFrequency);
                    }
                    if (!mcale->cale.repeatForever) {
                        /* RFC 5445, which supercedes RFC 2445, specifies that dates
                * are inclusive of the last event.  This clears up confusion
                * in the earlier specification and means that Jpilot no longer
                * needs to add +1day to the end of repeating events. */
                        memset(&ical_time, 0, sizeof(ical_time));
                        ical_time.tm_year = mcale->cale.repeatEnd.tm_year;
                        ical_time.tm_mon = mcale->cale.repeatEnd.tm_mon;
                        ical_time.tm_mday = mcale->cale.repeatEnd.tm_mday;
                        ical_time.tm_isdst = -1;
                        mktime(&ical_time);
                        fprintf(out, ";UNTIL=%04d%02d%02d",
                                ical_time.tm_year + 1900,
                                ical_time.tm_mon + 1,
                                ical_time.tm_mday);
                    }
                    fprintf(out, CRLF);
                    if (mcale->cale.exceptions > 0) {
                        for (i = 0; i < mcale->cale.exceptions; i++) {
                            fprintf(out, "EXDATE;VALUE=DATE:%04d%02d%02d"CRLF,
                                    mcale->cale.exception[i].tm_year + 1900,
                                    mcale->cale.exception[i].tm_mon + 1,
                                    mcale->cale.exception[i].tm_mday);
                        }
                    }
                }
                if (mcale->cale.alarm) {
                    char *units;
                    fprintf(out, "BEGIN:VALARM"CRLF);
                    fprintf(out, "ACTION:DISPLAY"CRLF);
                    str_to_ical_str(csv_text, sizeof(csv_text), mcale->cale.description);
                    fprintf(out, "DESCRIPTION:%s"CRLF, csv_text);
                    switch (mcale->cale.advanceUnits) {
                        case advMinutes:
                            units = "M";
                            break;
                        case advHours:
                            units = "H";
                            break;
                        case advDays:
                            units = "D";
                            break;
                        default: /* XXX */
                            units = "?";
                            break;
                    }
                    fprintf(out, "TRIGGER:-PT%d%s"CRLF, mcale->cale.advance, units);
                    fprintf(out, "END:VALARM"CRLF);
                }
                fprintf(out, "END:VEVENT"CRLF);
                if (temp_list->next == NULL) {
                    fprintf(out, "END:VCALENDAR"CRLF);
                }
                break;
            default:
                jp_logf(JP_LOG_WARN, _("Unknown export type\n"));
                dialog_generic_ok(notebook, _("Error"), DIALOG_ERROR, _("Unknown export type"));
        }
    }

    free_CalendarEventList(&cel);

    if (out) {
        fclose(out);
    }
}

/*************** Start Export GUI ***************/

int datebook_export(GtkWidget *window) {
    int x, y;

    gdk_window_get_root_origin(gtk_widget_get_window(window), &x, &y);

    x += 40;

    datebook_export_gui(window, x, y);

    return EXIT_SUCCESS;
}

static gboolean cb_export_destroy(GtkWidget *widget) {
    const char *filename;

    filename = gtk_entry_get_text(GTK_ENTRY(save_as_entry));
    set_pref(PREF_DATEBOOK_EXPORT_FILENAME, 0, filename, TRUE);

    gtk_main_quit();

    return FALSE;
}

static void cb_ok(GtkWidget *widget, gpointer data) {
    const char *filename;

    filename = gtk_entry_get_text(GTK_ENTRY(save_as_entry));

    appt_export_ok(glob_export_type, filename);

    gtk_widget_destroy(data);
}

static void cb_export_browse(GtkWidget *widget, gpointer data) {
    int r;
    const char *svalue;

    r = export_browse(GTK_WIDGET(data), PREF_DATEBOOK_EXPORT_FILENAME);
    if (r == BROWSE_OK) {
        get_pref(PREF_DATEBOOK_EXPORT_FILENAME, NULL, &svalue);
        gtk_entry_set_text(GTK_ENTRY(save_as_entry), svalue);
    }
}

static void cb_export_quit(GtkWidget *widget, gpointer data) {
    gtk_widget_destroy(data);
}

static void cb_export_type(GtkWidget *widget, gpointer data) {
    glob_export_type = GPOINTER_TO_INT(data);
}

static int datebook_export_gui(GtkWidget *main_window, int x, int y) {
    GtkWidget *button;
    GtkWidget *vbox;
    GtkWidget *hbox;
    GtkWidget *label;
    GSList *group;
    char *type_text[] = {N_("Text"),
                         N_("CSV"),
                         N_("iCalendar"),
                         NULL};
    int type_int[] = {EXPORT_TYPE_TEXT, EXPORT_TYPE_CSV, EXPORT_TYPE_ICALENDAR};
    int i;
    const char *svalue;

    jp_logf(JP_LOG_DEBUG, "datebook_export_gui()\n");

    glob_export_type = EXPORT_TYPE_TEXT;

    export_window = gtk_widget_new(GTK_TYPE_WINDOW,
                                   "type", GTK_WINDOW_TOPLEVEL,
                                   "title", _("Export"),
                                   NULL);

    gtk_widget_set_uposition(export_window, x, y);

    gtk_window_set_modal(GTK_WINDOW(export_window), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(export_window), GTK_WINDOW(main_window));

    gtk_container_set_border_width(GTK_CONTAINER(export_window), 5);

    gtk_signal_connect(GTK_OBJECT(export_window), "destroy",
                       GTK_SIGNAL_FUNC(cb_export_destroy), export_window);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(export_window), vbox);

    /* Label for instructions */
    label = gtk_label_new(_("Export All Datebook Records"));
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

    /* Export Type Buttons */
    group = NULL;
    for (i = 0; i < NUM_EXPORT_TYPES; i++) {
        if (type_text[i] == NULL) break;
        export_radio_type[i] = gtk_radio_button_new_with_label(group, type_text[i]);
        group = gtk_radio_button_group(GTK_RADIO_BUTTON(export_radio_type[i]));
        gtk_box_pack_start(GTK_BOX(vbox), export_radio_type[i], FALSE, FALSE, 0);
        gtk_signal_connect(GTK_OBJECT(export_radio_type[i]), "pressed",
                           GTK_SIGNAL_FUNC(cb_export_type),
                           GINT_TO_POINTER(type_int[i]));
    }
    export_radio_type[i] = NULL;

    /* Save As entry */
    hbox = gtk_hbox_new(FALSE, 5);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    label = gtk_label_new(_("Save as"));
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    save_as_entry = gtk_entry_new_with_max_length(250);
    svalue = NULL;
    get_pref(PREF_DATEBOOK_EXPORT_FILENAME, NULL, &svalue);

    if (svalue) {
        gtk_entry_set_text(GTK_ENTRY(save_as_entry), svalue);
    }
    gtk_box_pack_start(GTK_BOX(hbox), save_as_entry, TRUE, TRUE, 0);
    button = gtk_button_new_with_label(_("Browse"));
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
    gtk_signal_connect(GTK_OBJECT(button), "clicked",
                       GTK_SIGNAL_FUNC(cb_export_browse), export_window);

    hbox = gtk_hbutton_box_new();
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 12);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(hbox), GTK_BUTTONBOX_END);
    gtk_button_box_set_spacing(GTK_BUTTON_BOX(hbox), 6);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
    gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
    gtk_signal_connect(GTK_OBJECT(button), "clicked",
                       GTK_SIGNAL_FUNC(cb_export_quit), export_window);

    button = gtk_button_new_from_stock(GTK_STOCK_OK);
    gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, TRUE, 0);
    gtk_signal_connect(GTK_OBJECT(button), "clicked",
                       GTK_SIGNAL_FUNC(cb_ok), export_window);

    gtk_widget_show_all(export_window);

    gtk_main();

    return EXIT_SUCCESS;
}
/*** End Export GUI ***/

/*** End Export Code ***/

/*************** Start Datebk3/4 Code ***************/

#ifdef ENABLE_DATEBK
static GtkWidget *window_datebk_cats = NULL;
static GtkWidget *toggle_button[16];

static gboolean cb_destroy_datebk_cats(GtkWidget *widget) {
    window_datebk_cats = NULL;
    return FALSE;
}

static void cb_quit_datebk_cats(GtkWidget *widget, gpointer data) {
    jp_logf(JP_LOG_DEBUG, "cb_quit_datebk_cats\n");
    if (GTK_IS_WIDGET(data)) {
        gtk_widget_destroy(data);
    }
}

static void cb_toggle(GtkWidget *widget, int category) {
    int bit = 1;
    int cat_bit;
    int on;
    static int ignore_count = 0;

    if (category & 0x4000) {
        ignore_count = category & 0xFF;
        return;
    }
    if (ignore_count) {
        ignore_count--;
        return;
    }
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle_button[category]))) {
        on = 1;
    } else {
        on = 0;
    }
    cat_bit = bit << category;
    if (on) {
        datebk_category |= cat_bit;
    } else {
        datebk_category &= ~cat_bit;
    }

    datebook_update_listStore();
}

static void cb_datebk_category(GtkWidget *widget, gpointer data) {
    int i, count;
    int b;
    int flag;

    jp_logf(JP_LOG_DEBUG, "cb_datebk_category\n");

    flag = GPOINTER_TO_INT(data);
    b = dialog_save_changed_record(pane, record_changed);
    if (b == DIALOG_SAID_2) {
        cb_add_new_record(NULL, GINT_TO_POINTER(record_changed));
    }
    count = 0;
    for (i = 0; i < 16; i++) {
        if (GTK_IS_WIDGET(toggle_button[i])) {
            if ((gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle_button[i]))) != (flag)) {
                count++;
            }
        }
    }
    cb_toggle(NULL, count | 0x4000);
    for (i = 0; i < 16; i++) {
        if (GTK_IS_WIDGET(toggle_button[i])) {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle_button[i]), flag);
        }
    }
    if (flag) {
        datebk_category = 0xFFFF;
    } else {
        datebk_category = 0x0000;
    }
    datebook_update_listStore();
}

static void cb_datebk_cats(GtkWidget *widget, gpointer data) {
    struct CalendarAppInfo cai;
    int i;
    int bit;
    char title[200];
    GtkWidget *table;
    GtkWidget *button;
    GtkWidget *vbox, *hbox;
    long char_set;

    jp_logf(JP_LOG_DEBUG, "cb_datebk_cats\n");
    if (GTK_IS_WINDOW(window_datebk_cats)) {
        gdk_window_raise(gtk_widget_get_window(window_datebk_cats));
        jp_logf(JP_LOG_DEBUG, "datebk_cats window is already up\n");
        return;
    }

    get_calendar_or_datebook_app_info(&cai, 0);

    window_datebk_cats = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    gtk_window_set_position(GTK_WINDOW(window_datebk_cats), GTK_WIN_POS_MOUSE);

    gtk_window_set_modal(GTK_WINDOW(window_datebk_cats), TRUE);

    gtk_window_set_transient_for(GTK_WINDOW(window_datebk_cats),
                                 GTK_WINDOW(gtk_widget_get_toplevel(widget)));

    gtk_container_set_border_width(GTK_CONTAINER(window_datebk_cats), 10);
    g_snprintf(title, sizeof(title), "%s %s", PN, _("Datebook Categories"));
    gtk_window_set_title(GTK_WINDOW(window_datebk_cats), title);

    gtk_signal_connect(GTK_OBJECT(window_datebk_cats), "destroy",
                       GTK_SIGNAL_FUNC(cb_destroy_datebk_cats), window_datebk_cats);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(window_datebk_cats), vbox);

    /* Table */
    table = gtk_table_new(8, 2, TRUE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 0);
    gtk_table_set_col_spacings(GTK_TABLE(table), 0);
    gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);

    get_pref(PREF_CHAR_SET, &char_set, NULL);
    for (i = 0, bit = 1; i < 16; i++, bit <<= 1) {
        if (cai.category.name[i][0]) {
            char *l;

            l = charset_p2newj(cai.category.name[i], sizeof(cai.category.name[0]), (int) char_set);
            toggle_button[i] = gtk_toggle_button_new_with_label(l);
            g_free(l);
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle_button[i]),
                                         datebk_category & bit);
            gtk_table_attach_defaults
                    (GTK_TABLE(table), GTK_WIDGET(toggle_button[i]),
                     (i > 7) ? 1 : 0, (i > 7) ? 2 : 1, (guint) ((i > 7) ? i - 8 : i),
                     (guint) ((i > 7) ? i - 7 : i + 1));
            gtk_signal_connect(GTK_OBJECT(toggle_button[i]), "toggled",
                               GTK_SIGNAL_FUNC(cb_toggle), GINT_TO_POINTER(i));
        } else {
            toggle_button[i] = NULL;
        }
    }

    hbox = gtk_hbutton_box_new();
    gtk_button_box_set_spacing(GTK_BUTTON_BOX(hbox), 6);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    /* Close button */
    button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
    gtk_signal_connect(GTK_OBJECT(button), "clicked",
                       GTK_SIGNAL_FUNC(cb_quit_datebk_cats), window_datebk_cats);
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

    /* All button */
    button = gtk_button_new_with_label(_("All"));
    gtk_signal_connect(GTK_OBJECT(button), "clicked",
                       GTK_SIGNAL_FUNC(cb_datebk_category), GINT_TO_POINTER(1));
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

    /* None button */
    button = gtk_button_new_with_label(_("None"));
    gtk_signal_connect(GTK_OBJECT(button), "clicked",
                       GTK_SIGNAL_FUNC(cb_datebk_category), GINT_TO_POINTER(0));
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

    gtk_widget_show_all(window_datebk_cats);
}

#endif
/*** End Datebk3/4 Code ***/

/* Find position of category in sorted category array
 * via its assigned category number */
static int find_sort_cat_pos(int cat) {
    int i;

    for (i = 0; i < NUM_DATEBOOK_CAT_ITEMS; i++) {
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

    if (cat != NUM_DATEBOOK_CAT_ITEMS - 1) {
        return cat;
    } else { /* Unfiled category */
        /* Count how many category entries are filled */
        for (i = 0; i < NUM_DATEBOOK_CAT_ITEMS; i++) {
            if (!sort_l[i].Pcat[0]) {
                return i;
            }
        }
        return 0;
    }
}


int datebook_print(int type) {
    struct tm date;
    long fdow, paper_size;

    date.tm_mon = current_month;
    date.tm_mday = current_day;
    date.tm_year = current_year;
    date.tm_sec = 0;
    date.tm_min = 0;
    date.tm_hour = 11;
    date.tm_isdst = -1;
    mktime(&date);

    switch (type) {
        case DAILY:
            jp_logf(JP_LOG_DEBUG, "datebook_print daily\n");
            print_days_appts(&date);
            break;
        case WEEKLY:
            jp_logf(JP_LOG_DEBUG, "datebook_print weekly\n");
            get_pref(PREF_FDOW, &fdow, NULL);
            /* Get the first day of the week */
            sub_days_from_date(&date, (int) ((7 - fdow + date.tm_wday) % 7));

            get_pref(PREF_PAPER_SIZE, &paper_size, NULL);
            if (paper_size == 1) {
                print_weeks_appts(&date, PAPER_A4);
            } else {
                print_weeks_appts(&date, PAPER_Letter);
            }
            break;
        case MONTHLY:
            jp_logf(JP_LOG_DEBUG, "datebook_print monthly\n");
            get_pref(PREF_PAPER_SIZE, &paper_size, NULL);
            if (paper_size == 1) {
                print_months_appts(&date, PAPER_A4);
            } else {
                print_months_appts(&date, PAPER_Letter);
            }
            break;
        default:
            break;
    }

    return EXIT_SUCCESS;
}

static void cb_monthview(GtkWidget *widget, gpointer data) {
    struct tm date;

    memset(&date, 0, sizeof(date));
    date.tm_mon = current_month;
    date.tm_mday = current_day;
    date.tm_year = current_year;
    monthview_gui(&date);
}

static void cb_cal_dialog(GtkWidget *widget, gpointer data) {
    long fdow;
    int r = 0;
    struct tm *Pt;
    GtkWidget *Pcheck_button;
    GtkWidget *Pbutton;

    switch (GPOINTER_TO_INT(data)) {
        case PAGE_DAY:
            Pcheck_button = check_button_day_endon;
            Pt = &glob_endon_day_tm;
            Pbutton = glob_endon_day_button;
            break;
        case PAGE_WEEK:
            Pcheck_button = check_button_week_endon;
            Pt = &glob_endon_week_tm;
            Pbutton = glob_endon_week_button;
            break;
        case PAGE_MONTH:
            Pcheck_button = check_button_mon_endon;
            Pt = &glob_endon_mon_tm;
            Pbutton = glob_endon_mon_button;
            break;
        case PAGE_YEAR:
            Pcheck_button = check_button_year_endon;
            Pt = &glob_endon_year_tm;
            Pbutton = glob_endon_year_button;
            break;
        case BEGIN_DATE_BUTTON:
            Pcheck_button = NULL;
            Pt = &begin_date;
            Pbutton = begin_date_button;
            break;
        default:;
            jp_logf(JP_LOG_DEBUG, "default hit in cb_cal_dialog()\n");
            return;
    }

    get_pref(PREF_FDOW, &fdow, NULL);

    if (GPOINTER_TO_INT(data) == BEGIN_DATE_BUTTON) {
        r = cal_dialog(GTK_WINDOW(gtk_widget_get_toplevel(widget)),
                       _("Begin On Date"), (int) fdow,
                       &(Pt->tm_mon),
                       &(Pt->tm_mday),
                       &(Pt->tm_year));
    } else {
        r = cal_dialog(GTK_WINDOW(gtk_widget_get_toplevel(widget)),
                       _("End On Date"), (int) fdow,
                       &(Pt->tm_mon),
                       &(Pt->tm_mday),
                       &(Pt->tm_year));
    }
    if (GPOINTER_TO_INT(data) == BEGIN_DATE_BUTTON) {
        end_date.tm_mon = begin_date.tm_mon;
        end_date.tm_mday = begin_date.tm_mday;
        end_date.tm_year = begin_date.tm_year;
    }

    if (r == CAL_DONE) {
        if (Pcheck_button) {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(Pcheck_button), TRUE);
        }
        if (Pbutton) {
            update_endon_button(Pbutton, Pt);
        }
    }
}

static void cb_weekview(GtkWidget *widget, gpointer data) {
    struct tm date;

    memset(&date, 0, sizeof(date));
    date.tm_mon = current_month;
    date.tm_mday = current_day;
    date.tm_year = current_year;
    date.tm_sec = 0;
    date.tm_min = 0;
    date.tm_hour = 11;
    date.tm_isdst = -1;
    mktime(&date);
    weekview_gui(&date);
}

static void init(void) {
    time_t ltime;
    struct tm *now;
    struct tm next_tm;
    int next_found;
    CalendarEventList *ce_list;
    CalendarEventList *temp_cel;
#ifdef ENABLE_DATEBK
    long use_db3_tags;
#endif

#ifdef ENABLE_DATEBK
    get_pref(PREF_USE_DB3, &use_db3_tags, NULL);
    if (use_db3_tags) {
        DB_APPT_COLUMN = 4;
    } else {
        DB_APPT_COLUMN = 3;
    }
#endif

    time(&ltime);
    now = localtime(&ltime);
    current_day = now->tm_mday;
    current_month = now->tm_mon;
    current_year = now->tm_year;

    memcpy(&glob_endon_day_tm, now, sizeof(glob_endon_day_tm));
    memcpy(&glob_endon_week_tm, now, sizeof(glob_endon_week_tm));
    memcpy(&glob_endon_mon_tm, now, sizeof(glob_endon_mon_tm));
    memcpy(&glob_endon_year_tm, now, sizeof(glob_endon_year_tm));

    if (glob_find_id) {
        jp_logf(JP_LOG_DEBUG, "init() glob_find_id = %d\n", glob_find_id);
        /* Search appointments for this id to get its date */
        ce_list = NULL;

        get_days_calendar_events2(&ce_list, NULL, 1, 1, 1, CATEGORY_ALL, NULL);

        for (temp_cel = ce_list; temp_cel; temp_cel = temp_cel->next) {
            if (temp_cel->mcale.unique_id == glob_find_id) {
                jp_logf(JP_LOG_DEBUG, "init() found glob_find_id\n");
                /* Position calendar on the actual event
             * or the next future occurrence
             * depending on which is closest to the current date */
                if (temp_cel->mcale.cale.repeatType == calendarRepeatNone) {
                    next_found = 0;
                } else {
                    next_found = find_next_rpt_event(&(temp_cel->mcale.cale),
                                                     now, &next_tm);
                }

                if (!next_found) {
                    current_month = temp_cel->mcale.cale.begin.tm_mon;
                    current_day = temp_cel->mcale.cale.begin.tm_mday;
                    current_year = temp_cel->mcale.cale.begin.tm_year;
                } else {
                    current_month = next_tm.tm_mon;
                    current_day = next_tm.tm_mday;
                    current_year = next_tm.tm_year;
                }

            }
        }
        free_CalendarEventList(&ce_list);
    }

    row_selected = 0;

    record_changed = CLEAR_FLAG;
}

static int dialog_4_or_last(int dow) {
    char *days[] = {
            N_("Sunday"),
            N_("Monday"),
            N_("Tuesday"),
            N_("Wednesday"),
            N_("Thursday"),
            N_("Friday"),
            N_("Saturday")
    };
    char text[255];
    char *button_text[] = {N_("4th"), N_("Last")};

    sprintf(text,
            _("This appointment can either\n"
              "repeat on the 4th %s\n"
              "of the month, or on the last\n"
              "%s of the month.\n"
              "Which do you want?"),
            _(days[dow]), _(days[dow]));
    return dialog_generic(GTK_WINDOW(gtk_widget_get_toplevel(scrolled_window)),
                          _("Question?"), DIALOG_QUESTION,
                          text, 2, button_text);
}

static int dialog_current_future_all_cancel(void) {
    char text[] =
            N_("This is a repeating event.\n"
               "Do you want to apply these changes to\n"
               "only the CURRENT event,\n"
               "just FUTURE events, or\n"
               "ALL of the occurrences of this event?");
    char *button_text[] = {N_("Current"), N_("Future"), N_("All"), N_("Cancel")
    };

    return dialog_generic(GTK_WINDOW(gtk_widget_get_toplevel(scrolled_window)),
                          _("Question?"), DIALOG_QUESTION,
                          _(text), 4, button_text);
}

#ifdef EASTER
                                                                                                                        static int dialog_easter(int mday)
{
   char text[255];
   char who[50];
   char *button_text[]={"I'll send a present!!!"};

   if (mday==29) {
      strcpy(who, "Judd Montgomery");
   }
   if (mday==20) {
      strcpy(who, "Jacki Montgomery");
   }
   sprintf(text,
           "Today is\n"
           "%s\'s\n"
           "Birthday!\n", who);

   return dialog_generic(GTK_WINDOW(gtk_widget_get_toplevel(scrolled_window)),
                         "Happy Birthday to Me!", DIALOG_INFO,
                         text, 1, button_text);
}
#endif
/* End of Dialog window code */

/* month = 0-11 */
/* dom = day of month 1-31 */
/* year = calendar year - 1900 */
/* dow = day of week 0-6, where 0=Sunday, etc. */
/* */
/* Returns an enum from DayOfMonthType defined in pi-datebook.h */
static long get_dom_type(int month, int dom, int year, int dow) {
    long r;
    int ndim;     /* ndim = number of days in month 28-31 */
    int dow_fdof; /* Day of the week for the first day of the month */
    int result;

    r = (dom - 1) / 7 * 7 + dow;

    /* If its the 5th occurrence of this dow in the month then it is always
    * going to be the last occurrence of that dow in the month.
    * Sometimes this will occur in the 4th week, sometimes in the 5th.
    * If its the 4th occurrence of this dow in the month and there is a 5th
    * then it always the 4th occurrence.
    * If its the 4th occurrence of this dow in the month and there is not a
    * 5th then we need to ask if this appointment repeats on the last dow of
    * the month, or the 4th dow of every month.
    * This should be perfectly clear now, right? */

    /* These are the last 2 lines of the DayOfMonthType enum: */
    /* dom4thSun, dom4thMon, dom4thTue, dom4thWen, dom4thThu, dom4thFri, dom4thSat */
    /* domLastSun, domLastMon, domLastTue, domLastWen, domLastThu, domLastFri, domLastSat */

    if ((r >= dom4thSun) && (r <= dom4thSat)) {
        get_month_info(month, dom, year, &dow_fdof, &ndim);
        if ((ndim - dom < 7)) {
            /* This is the 4th dow, and there is no 5th in this month. */
            result = dialog_4_or_last(dow);
            /* If they want it to be the last dow in the month instead of the */
            /* 4th, then we need to add 7. */
            if (result == DIALOG_SAID_LAST) {
                r += 7;
            }
        }
    }

    return r;
}

/* flag UPDATE_DATE_ENTRY is to set entry fields
 * flag UPDATE_DATE_MENUS is to set menu items */
static void set_begin_end_labels(struct tm *begin, struct tm *end, int flags) {
    char str[255];
    char time1_str[255];
    char time2_str[255];
    char pref_time[40];
    const char *pref_date;

    str[0] = '\0';

    get_pref(PREF_SHORTDATE, NULL, &pref_date);
    strftime(str, sizeof(str), pref_date, begin);
    gtk_label_set_text(GTK_LABEL(gtk_bin_get_child(GTK_BIN(begin_date_button))), str);

    if (flags & UPDATE_DATE_ENTRIES) {
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio_button_no_time))) {
            gtk_entry_set_text(GTK_ENTRY(begin_time_entry), "");
            gtk_entry_set_text(GTK_ENTRY(end_time_entry), "");
        } else {
            get_pref_time_no_secs(pref_time);

            jp_strftime(time1_str, sizeof(time1_str), pref_time, begin);
            jp_strftime(time2_str, sizeof(time2_str), pref_time, end);

            gtk_entry_set_text(GTK_ENTRY(begin_time_entry), time1_str);
            gtk_entry_set_text(GTK_ENTRY(end_time_entry), time2_str);
        }
    }
    if (flags & UPDATE_DATE_MENUS) {
        gtk_option_menu_set_history(GTK_OPTION_MENU(option1), (guint) begin_date.tm_hour);
        gtk_option_menu_set_history(GTK_OPTION_MENU(option2), (guint) (begin_date.tm_min / 5));
        gtk_option_menu_set_history(GTK_OPTION_MENU(option3), (guint) end_date.tm_hour);
        gtk_option_menu_set_history(GTK_OPTION_MENU(option4), (guint) (end_date.tm_min / 5));
    }
}

static void clear_begin_end_labels(void) {
    begin_date.tm_mon = current_month;
    begin_date.tm_mday = current_day;
    begin_date.tm_year = current_year;
    begin_date.tm_hour = 8;
    begin_date.tm_min = 0;
    begin_date.tm_sec = 0;
    begin_date.tm_isdst = -1;

    end_date.tm_mon = current_month;
    end_date.tm_mday = current_day;
    end_date.tm_year = current_year;
    end_date.tm_hour = 9;
    end_date.tm_min = 0;
    end_date.tm_sec = 0;
    end_date.tm_isdst = -1;

    set_begin_end_labels(&begin_date, &end_date, UPDATE_DATE_ENTRIES |
                                                 UPDATE_DATE_MENUS);
}

static void appt_clear_details(void) {
    int i;
    struct tm today;
    int new_cat;
    int sorted_position;
#ifdef ENABLE_DATEBK
    long use_db3_tags;
#endif

    connect_changed_signals(DISCONNECT_SIGNALS);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_button_alarm), FALSE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_button_no_time), TRUE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(private_checkbox), FALSE);

    clear_begin_end_labels();

    gtk_text_buffer_set_text(GTK_TEXT_BUFFER(dbook_desc_buffer), "", -1);
    gtk_text_buffer_set_text(GTK_TEXT_BUFFER(dbook_note_buffer), "", -1);
#ifdef ENABLE_DATEBK
    get_pref(PREF_USE_DB3, &use_db3_tags, NULL);
    if (use_db3_tags) {
        gtk_entry_set_text(GTK_ENTRY(datebk_entry), "");
    }
#endif
    if (datebook_version) {
        /* Calendar has a location field */
        gtk_entry_set_text(GTK_ENTRY(location_entry), "");
    }

    /* Clear the notebook pages */
    gtk_notebook_set_page(GTK_NOTEBOOK(notebook), PAGE_NONE);

    gtk_entry_set_text(GTK_ENTRY(units_entry), "5");
    gtk_entry_set_text(GTK_ENTRY(repeat_day_entry), "1");
    gtk_entry_set_text(GTK_ENTRY(repeat_week_entry), "1");
    gtk_entry_set_text(GTK_ENTRY(repeat_mon_entry), "1");
    gtk_entry_set_text(GTK_ENTRY(repeat_year_entry), "1");

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_button_day_endon), FALSE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_button_week_endon), FALSE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_button_mon_endon), FALSE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_button_year_endon), FALSE);

    for (i = 0; i < 7; i++) {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle_button_repeat_days[i]), FALSE);
    }

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle_button_repeat_mon_bydate), TRUE);

    memset(&today, 0, sizeof(today));
    today.tm_year = current_year;
    today.tm_mon = current_month;
    today.tm_mday = current_day;
    today.tm_hour = 12;
    today.tm_min = 0;
    mktime(&today);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle_button_repeat_days[today.tm_wday]), TRUE);

    memcpy(&glob_endon_day_tm, &today, sizeof(glob_endon_day_tm));
    memcpy(&glob_endon_week_tm, &today, sizeof(glob_endon_week_tm));
    memcpy(&glob_endon_mon_tm, &today, sizeof(glob_endon_mon_tm));
    memcpy(&glob_endon_year_tm, &today, sizeof(glob_endon_year_tm));

    if (datebook_version) {
        /* Calendar supports categories */
        if (dbook_category == CATEGORY_ALL) {
            new_cat = 0;
        } else {
            new_cat = dbook_category;
        }
        sorted_position = find_sort_cat_pos(new_cat);
        if (sorted_position < 0) {
            jp_logf(JP_LOG_WARN, _("Category is not legal\n"));
        } else {
            gtk_combo_box_set_active(GTK_COMBO_BOX(category_menu2), find_menu_cat_pos(sorted_position));
        }
    }

    connect_changed_signals(CONNECT_SIGNALS);
}

/* TODO rename */
static int appt_get_details(struct CalendarEvent *cale, unsigned char *attrib) {
    int i;
    time_t ltime, ltime2;
    char str[30];
    gint page;
    int total_repeat_days;
    char datef[32];
    const char *svalue1, *svalue2;
    const gchar *text1;
    GtkTextIter start_iter;
    GtkTextIter end_iter;
#ifdef ENABLE_DATEBK
    gchar *datebk_note_text = NULL;
    gchar *text2;
    long use_db3_tags;
    char null_str[] = "";
#endif
    const char *period[] = {
            N_("None"),
            N_("day"),
            N_("week"),
            N_("month"),
            N_("year")
    };

#ifdef ENABLE_DATEBK
    get_pref(PREF_USE_DB3, &use_db3_tags, NULL);
#endif

    memset(cale, 0, sizeof(*cale));

    *attrib = 0;
    if (datebook_version) {
        /* Calendar supports categories */
        /* Get the category that is set from the menu */
        if (GTK_IS_WIDGET(category_menu2)) {
            *attrib = get_selected_category_from_combo_box(GTK_COMBO_BOX(category_menu2));
        }
    }

    time(&ltime);
    localtime(&ltime);

    total_repeat_days = 0;

    /* The first day of the week */
    /* I always use 0, Sunday is always 0 in this code */
    cale->repeatWeekstart = 0;

    cale->exceptions = 0;
    cale->exception = NULL;

    /* daylight savings flag */
    cale->end.tm_isdst = cale->begin.tm_isdst = -1;

    /* Begin time */
    cale->begin.tm_mon = begin_date.tm_mon;
    cale->begin.tm_mday = begin_date.tm_mday;
    cale->begin.tm_year = begin_date.tm_year;
    cale->begin.tm_hour = begin_date.tm_hour;
    cale->begin.tm_min = begin_date.tm_min;
    cale->begin.tm_sec = 0;

    /* End time */
    cale->end.tm_mon = end_date.tm_mon;
    cale->end.tm_mday = end_date.tm_mday;
    cale->end.tm_year = end_date.tm_year;
    cale->end.tm_hour = end_date.tm_hour;
    cale->end.tm_min = end_date.tm_min;
    cale->end.tm_sec = 0;

    if ((GTK_TOGGLE_BUTTON(radio_button_no_time))) {
        cale->event = 1;
        /* This event doesn't have a time */
        cale->begin.tm_hour = 0;
        cale->begin.tm_min = 0;
        cale->begin.tm_sec = 0;
        cale->end.tm_hour = 0;
        cale->end.tm_min = 0;
        cale->end.tm_sec = 0;
    } else {
        cale->event = 0;
    }

    ltime = mktime(&cale->begin);

    ltime2 = mktime(&cale->end);

    /* Datebook does not support events spanning midnight where
      the beginning time is greater than the ending time */
    if (datebook_version == 0) {
        if (ltime > ltime2) {
            memcpy(&(cale->end), &(cale->begin), sizeof(struct tm));
        }
    }

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_button_alarm))) {
        cale->alarm = 1;
        text1 = gtk_entry_get_text(GTK_ENTRY(units_entry));
        cale->advance = atoi(text1);
        jp_logf(JP_LOG_DEBUG, "alarm advance %d", cale->advance);
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio_button_alarm_min))) {
            cale->advanceUnits = advMinutes;
            jp_logf(JP_LOG_DEBUG, "min\n");
        }
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio_button_alarm_hour))) {
            cale->advanceUnits = advHours;
            jp_logf(JP_LOG_DEBUG, "hour\n");
        }
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radio_button_alarm_day))) {
            cale->advanceUnits = advDays;
            jp_logf(JP_LOG_DEBUG, "day\n");
        }
    } else {
        cale->alarm = 0;
    }

    page = gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook));

    cale->repeatEnd.tm_hour = 0;
    cale->repeatEnd.tm_min = 0;
    cale->repeatEnd.tm_sec = 0;
    cale->repeatEnd.tm_isdst = -1;

    switch (page) {
        case PAGE_NONE:
            cale->repeatType = calendarRepeatNone;
            jp_logf(JP_LOG_DEBUG, "no repeat\n");
            break;
        case PAGE_DAY:
            cale->repeatType = calendarRepeatDaily;
            text1 = gtk_entry_get_text(GTK_ENTRY(repeat_day_entry));
            cale->repeatFrequency = atoi(text1);
            jp_logf(JP_LOG_DEBUG, "every %d day(s)\n", cale->repeatFrequency);
            if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_button_day_endon))) {
                cale->repeatForever = 0;
                jp_logf(JP_LOG_DEBUG, "end on day\n");
                cale->repeatEnd.tm_mon = glob_endon_day_tm.tm_mon;
                cale->repeatEnd.tm_mday = glob_endon_day_tm.tm_mday;
                cale->repeatEnd.tm_year = glob_endon_day_tm.tm_year;
                cale->repeatEnd.tm_isdst = -1;
                mktime(&cale->repeatEnd);
            } else {
                cale->repeatForever = 1;
            }
            break;
        case PAGE_WEEK:
            cale->repeatType = calendarRepeatWeekly;
            text1 = gtk_entry_get_text(GTK_ENTRY(repeat_week_entry));
            cale->repeatFrequency = atoi(text1);
            jp_logf(JP_LOG_DEBUG, "every %d week(s)\n", cale->repeatFrequency);
            if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_button_week_endon))) {
                cale->repeatForever = 0;
                jp_logf(JP_LOG_DEBUG, "end on week\n");
                cale->repeatEnd.tm_mon = glob_endon_week_tm.tm_mon;
                cale->repeatEnd.tm_mday = glob_endon_week_tm.tm_mday;
                cale->repeatEnd.tm_year = glob_endon_week_tm.tm_year;
                cale->repeatEnd.tm_isdst = -1;
                mktime(&cale->repeatEnd);

                get_pref(PREF_SHORTDATE, NULL, &svalue1);
                get_pref(PREF_TIME, NULL, &svalue2);
                if ((svalue1 == NULL) || (svalue2 == NULL)) {
                    strcpy(datef, "%x %X");
                } else {
                    sprintf(datef, "%s %s", svalue1, svalue2);
                }
                strftime(str, sizeof(str), datef, &cale->repeatEnd);

                jp_logf(JP_LOG_DEBUG, "repeat_end time = %s\n", str);
            } else {
                cale->repeatForever = 1;
            }
            jp_logf(JP_LOG_DEBUG, "Repeat Days:");
            cale->repeatWeekstart = 0;  /* We are going to always use 0 */
            for (i = 0; i < 7; i++) {
                cale->repeatDays[i] = (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle_button_repeat_days[i])));
                total_repeat_days += cale->repeatDays[i];
            }
            jp_logf(JP_LOG_DEBUG, "\n");
            break;
        case PAGE_MONTH:
            text1 = gtk_entry_get_text(GTK_ENTRY(repeat_mon_entry));
            cale->repeatFrequency = atoi(text1);
            jp_logf(JP_LOG_DEBUG, "every %d month(s)\n", cale->repeatFrequency);
            if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_button_mon_endon))) {
                cale->repeatForever = 0;
                jp_logf(JP_LOG_DEBUG, "end on month\n");
                cale->repeatEnd.tm_mon = glob_endon_mon_tm.tm_mon;
                cale->repeatEnd.tm_mday = glob_endon_mon_tm.tm_mday;
                cale->repeatEnd.tm_year = glob_endon_mon_tm.tm_year;
                cale->repeatEnd.tm_isdst = -1;
                mktime(&cale->repeatEnd);

                get_pref(PREF_SHORTDATE, NULL, &svalue1);
                get_pref(PREF_TIME, NULL, &svalue2);
                if ((svalue1 == NULL) || (svalue2 == NULL)) {
                    strcpy(datef, "%x %X");
                } else {
                    sprintf(datef, "%s %s", svalue1, svalue2);
                }
                strftime(str, sizeof(str), datef, &cale->repeatEnd);

                jp_logf(JP_LOG_DEBUG, "repeat_end time = %s\n", str);
            } else {
                cale->repeatForever = 1;
            }
            if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle_button_repeat_mon_byday))) {
                cale->repeatType = calendarRepeatMonthlyByDay;
                cale->repeatDay = (enum calendarDayOfMonthType) get_dom_type(cale->begin.tm_mon, cale->begin.tm_mday,
                                                                             cale->begin.tm_year,
                                                                             cale->begin.tm_wday);
                jp_logf(JP_LOG_DEBUG, "***by day\n");
            }
            if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(toggle_button_repeat_mon_bydate))) {
                cale->repeatType = calendarRepeatMonthlyByDate;
                jp_logf(JP_LOG_DEBUG, "***by date\n");
            }
            break;
        case PAGE_YEAR:
            cale->repeatType = calendarRepeatYearly;
            text1 = gtk_entry_get_text(GTK_ENTRY(repeat_year_entry));
            cale->repeatFrequency = atoi(text1);
            jp_logf(JP_LOG_DEBUG, "every %s year(s)\n", cale->repeatFrequency);
            if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(check_button_year_endon))) {
                cale->repeatForever = 0;
                jp_logf(JP_LOG_DEBUG, "end on year\n");
                cale->repeatEnd.tm_mon = glob_endon_year_tm.tm_mon;
                cale->repeatEnd.tm_mday = glob_endon_year_tm.tm_mday;
                cale->repeatEnd.tm_year = glob_endon_year_tm.tm_year;
                cale->repeatEnd.tm_isdst = -1;
                mktime(&cale->repeatEnd);

                get_pref(PREF_SHORTDATE, NULL, &svalue1);
                get_pref(PREF_TIME, NULL, &svalue2);
                if ((svalue1 == NULL) || (svalue2 == NULL)) {
                    strcpy(datef, "%x %X");
                } else {
                    sprintf(datef, "%s %s", svalue1, svalue2);
                }
                str[0] = '\0';
                strftime(str, sizeof(str), datef, &cale->repeatEnd);

                jp_logf(JP_LOG_DEBUG, "repeat_end time = %s\n", str);
            } else {
                cale->repeatForever = 1;
            }
            break;
        default:
            break;
    }
    gtk_text_buffer_get_bounds(GTK_TEXT_BUFFER(dbook_desc_buffer), &start_iter, &end_iter);
    cale->description = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(dbook_desc_buffer), &start_iter, &end_iter, TRUE);

    /* Empty appointment descriptions crash PalmOS 2.0, but are fine in
    * later versions */
    if (cale->description[0] == '\0') {
        free(cale->description);
        cale->description = strdup(" ");
    }
    if (strlen(cale->description) + 1 > MAX_DESC_LEN) {
        cale->description[MAX_DESC_LEN + 1] = '\0';
        jp_logf(JP_LOG_WARN, _("Appointment description text > %d, truncating to %d\n"), MAX_DESC_LEN, MAX_DESC_LEN);
    }
    if (cale->description) {
        jp_logf(JP_LOG_DEBUG, "description=[%s]\n", cale->description);
    }

#ifdef ENABLE_DATEBK
    if (use_db3_tags) {
        text1 = gtk_entry_get_text(GTK_ENTRY(datebk_entry));
        gtk_text_buffer_get_bounds(GTK_TEXT_BUFFER(dbook_note_buffer), &start_iter, &end_iter);
        text2 = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(dbook_note_buffer), &start_iter, &end_iter, TRUE);
        if (!text1) text1 = null_str;
        if (!text2) text2 = null_str;
        /* 8 extra characters is just being paranoid */
        datebk_note_text = malloc(strlen(text1) + strlen(text2) + 8);
        datebk_note_text[0] = '\0';
        cale->note = datebk_note_text;
        if ((text1) && (text1[0])) {
            strcpy(datebk_note_text, text1);
            strcat(datebk_note_text, "\n");
        }
        strcat(datebk_note_text, text2);
    } else {
        gtk_text_buffer_get_bounds(GTK_TEXT_BUFFER(dbook_note_buffer), &start_iter, &end_iter);
        cale->note = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(dbook_note_buffer), &start_iter, &end_iter, TRUE);
    }
#else /* Datebk #ifdef */
                                                                                                                            gtk_text_buffer_get_bounds(GTK_TEXT_BUFFER(dbook_note_buffer),&start_iter,&end_iter);
   cale->note = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(dbook_note_buffer),&start_iter,&end_iter,TRUE);
#endif /* Datebk #ifdef */
    if (cale->note[0] == '\0') {
        free(cale->note);
        cale->note = NULL;
    }
    if (cale->note) {
        jp_logf(JP_LOG_DEBUG, "text note=[%s]\n", cale->note);
    }

    if (datebook_version) {
        cale->location = strdup(gtk_entry_get_text(GTK_ENTRY(location_entry)));
        if (cale->location[0] == '\0') {
            free(cale->location);
            cale->location = NULL;
        }
        if (cale->location) {
            jp_logf(JP_LOG_DEBUG, "text location=[%s]\n", cale->location);
        }
    } else {
        cale->location = NULL;
    }

    /* We won't allow a repeat frequency of less than 1 */
    if ((page != PAGE_NONE) && (cale->repeatFrequency < 1)) {
        char str[200];
        jp_logf(JP_LOG_WARN,
                _("You cannot have an appointment that repeats every %d %s(s)\n"),
                cale->repeatFrequency, _(period[page]));
        g_snprintf(str, sizeof(str),
                   _("You cannot have an appointment that repeats every %d %s(s)\n"),
                   cale->repeatFrequency, _(period[page]));
        dialog_generic_ok(notebook, _("Error"), DIALOG_ERROR, str);
        cale->repeatFrequency = 1;
        return EXIT_FAILURE;
    }

    /* We won't allow a weekly repeating that doesn't repeat on any day */
    if ((page == PAGE_WEEK) && (total_repeat_days == 0)) {
        dialog_generic_ok(notebook, _("Error"), DIALOG_ERROR,
                          _("You cannot have a weekly repeating appointment that doesn't repeat on any day of the week."));
        return EXIT_FAILURE;
    }

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(private_checkbox))) {
        *attrib |= dlpRecAttrSecret;
    }

    return EXIT_SUCCESS;
}

static void update_endon_button(GtkWidget *button, struct tm *t) {
    const char *short_date;
    char str[255];

    get_pref(PREF_SHORTDATE, NULL, &short_date);
    strftime(str, sizeof(str), short_date, t);

    gtk_label_set_text(GTK_LABEL(gtk_bin_get_child(GTK_BIN(button))), str);
}

/* Do masking like Palm OS 3.5 */
static void clear_myCalendarEvent(MyCalendarEvent *mcale) {
    mcale->unique_id = 0;
    mcale->attrib = (unsigned char) (mcale->attrib & 0xF8);
    mcale->cale.event = 1;
    mcale->cale.alarm = 0;
    mcale->cale.repeatType = calendarRepeatNone;
    memset(&mcale->cale.begin, 0, sizeof(struct tm));
    memset(&mcale->cale.end, 0, sizeof(struct tm));
    if (mcale->cale.location) {
        free(mcale->cale.location);
        mcale->cale.location = strdup("");
    }
    if (mcale->cale.description) {
        free(mcale->cale.description);
        mcale->cale.description = strdup("");
    }
    if (mcale->cale.note) {
        free(mcale->cale.note);
        mcale->cale.note = strdup("");
    }
/* TODO do we need to clear blob and tz? */
    return;
}

/* End Masking */
gboolean
selectDateRecordByRow(GtkTreeModel *model,
                      GtkTreePath *path,
                      GtkTreeIter *iter,
                      gpointer data) {
    int *i = gtk_tree_path_get_indices(path);
    if (i[0] == row_selected) {
        GtkTreeSelection *selection = NULL;
        selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeView));
        gtk_tree_selection_select_path(selection, path);
        gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(treeView), path, (GtkTreeViewColumn *) DATE_APPT_COLUMN_ENUM, FALSE,
                                     1.0, 0.0);
        return TRUE;
    }

    return FALSE;
}

static int datebook_update_listStore(void) {
    GtkTreeIter iter;
    int num_entries, entries_shown, num;
    CalendarEventList *temp_cel;
    char begin_time[32];
    char end_time[32];
    char a_time[sizeof(begin_time) + sizeof(end_time) + 1];
    char datef[20];
    GdkPixbuf *pixmap_note;
    GdkPixbuf *pixmap_alarm;
    int has_note;
#ifdef ENABLE_DATEBK
    int cat_bit;
    int db3_type = 0;
    long use_db3_tags;
    struct db4_struct db4;
    GdkPixbuf *pixmap_float_check;
    GdkPixbuf *pixmap_float_checked;
    GdkPixbuf *noteColumnDisplay;
    GdkPixbuf *alarmColumnDisplay;
    GdkPixbuf *floatColumnDisplay;
#endif
    struct tm new_time;
    int show_priv;
    char str[DATEBOOK_MAX_COLUMN_LEN + 2];
    char str2[DATEBOOK_MAX_COLUMN_LEN];
    long show_tooltips;

    jp_logf(JP_LOG_DEBUG, "datebook_update_listStore()\n");

    free_CalendarEventList(&glob_cel);

    memset(&new_time, 0, sizeof(new_time));
    new_time.tm_hour = 11;
    new_time.tm_mday = current_day;
    new_time.tm_mon = current_month;
    new_time.tm_year = current_year;
    new_time.tm_isdst = -1;
    mktime(&new_time);

    num = get_days_calendar_events2(&glob_cel, &new_time, 2, 2, 1, CATEGORY_ALL, &num_entries);

    jp_logf(JP_LOG_DEBUG, "get_days_appointments==>%d\n", num);
#ifdef ENABLE_DATEBK
    jp_logf(JP_LOG_DEBUG, "datebk_category = 0x%x\n", datebk_category);
#endif

    gtk_list_store_clear(GTK_LIST_STORE(listStore));

#ifdef __APPLE__

#endif

    /* Collect preferences and constant pixmaps for loop */
    show_priv = show_privates(GET_PRIVATES);
    get_pixbufs(PIXMAP_NOTE, &pixmap_note);
    get_pixbufs(PIXMAP_ALARM, &pixmap_alarm);

#ifdef ENABLE_DATEBK
    get_pref(PREF_USE_DB3, &use_db3_tags, NULL);
    get_pixbufs(PIXMAP_FLOAT_CHECK, &pixmap_float_check);
    get_pixbufs(PIXMAP_FLOAT_CHECKED, &pixmap_float_checked);
#endif

    entries_shown = 0;
    for (temp_cel = glob_cel; temp_cel; temp_cel = temp_cel->next) {

        if (datebook_version) {
            /* Filter by category for Calendar application */
            if (((temp_cel->mcale.attrib & 0x0F) != dbook_category) &&
                dbook_category != CATEGORY_ALL) {
                continue;
            }
        }
#ifdef ENABLE_DATEBK
        if (use_db3_tags) {
            db3_parse_tag(temp_cel->mcale.cale.note, &db3_type, &db4);
            jp_logf(JP_LOG_DEBUG, "category = 0x%x\n", db4.category);
            cat_bit = 1 << db4.category;
            if (!(cat_bit & datebk_category)) {
                jp_logf(JP_LOG_DEBUG, "skipping rec not in this category\n");
                continue;
            }
        }
#endif
        /* Do masking like Palm OS 3.5 */
        if ((show_priv == MASK_PRIVATES) &&
            (temp_cel->mcale.attrib & dlpRecAttrSecret)) {
            clear_myCalendarEvent(&temp_cel->mcale);
            gtk_list_store_append(listStore, &iter);
            gtk_list_store_set(listStore, &iter,
                               DATE_TIME_COLUMN_ENUM, "----------",
                               DATE_APPT_COLUMN_ENUM, "---------------",
                               DATE_DATA_COLUMN_ENUM, &(temp_cel->mcale),
                               -1);
            entries_shown++;
            continue;
        }
        /* End Masking */

        /* Hide the private records if need be */
        if ((show_priv != SHOW_PRIVATES) &&
            (temp_cel->mcale.attrib & dlpRecAttrSecret)) {
            continue;
        }



        /* Print the event time */
        if (temp_cel->mcale.cale.event) {
            /* This is a timeless event */
            strcpy(a_time, _("No Time"));
        } else {
            get_pref_time_no_secs_no_ampm(datef);
            strftime(begin_time, sizeof(begin_time), datef, &(temp_cel->mcale.cale.begin));
            get_pref_time_no_secs(datef);
            strftime(end_time, sizeof(end_time), datef, &(temp_cel->mcale.cale.end));
            g_snprintf(a_time, sizeof(a_time), "%s-%s", begin_time, end_time);
        }
        floatColumnDisplay = NULL;
#ifdef ENABLE_DATEBK
        if (use_db3_tags) {
            if (db4.floating_event == DB3_FLOAT) {
                floatColumnDisplay = pixmap_float_check;

            }
            if (db4.floating_event == DB3_FLOAT_COMPLETE) {
                floatColumnDisplay = pixmap_float_checked;

            }
        }
#endif

        has_note = 0;
#ifdef ENABLE_DATEBK
        if (use_db3_tags) {
            if (db3_type != DB3_TAG_TYPE_NONE) {
                if (db4.note && db4.note[0] != '\0') {
                    has_note = 1;
                }
            } else {
                if (temp_cel->mcale.cale.note &&
                    (temp_cel->mcale.cale.note[0] != '\0')) {
                    has_note = 1;
                }
            }
        } else {
            if (temp_cel->mcale.cale.note && (temp_cel->mcale.cale.note[0] != '\0')) {
                has_note = 1;
            }
        }
#else /* Ordinary, non DateBk code */
        if (temp_cel->mcale.cale.note && (temp_cel->mcale.cale.note[0]!='\0')) {
         has_note=1;
      }
#endif
        /* Put a note pixmap up */
        if (has_note) {
            noteColumnDisplay = pixmap_note;
        } else {
            noteColumnDisplay = NULL;
        }

        /* Put an alarm pixmap up */
        if (temp_cel->mcale.cale.alarm) {
            alarmColumnDisplay = pixmap_alarm;
        } else {
            alarmColumnDisplay = NULL;
        }

        /* Print the appointment description */
        lstrncpy_remove_cr_lfs(str2, temp_cel->mcale.cale.description, DATEBOOK_MAX_COLUMN_LEN);

        /* Append number of anniversary years if enabled & appropriate */
        append_anni_years(str2, sizeof(str2), &new_time, NULL, &temp_cel->mcale.cale);



        /* Highlight row background depending on status */
        GdkColor bgColor;
        gboolean showBgColor;
        switch (temp_cel->mcale.rt) {
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
                if (temp_cel->mcale.attrib & dlpRecAttrSecret) {
                    bgColor = get_color(LIST_PRIVATE_RED, LIST_PRIVATE_GREEN, LIST_PRIVATE_BLUE);
                    showBgColor = TRUE;
                } else {
                    showBgColor = FALSE;
                }
        }
        gtk_list_store_append(listStore, &iter);
        gtk_list_store_set(listStore, &iter,
                           DATE_TIME_COLUMN_ENUM, a_time,
                           DATE_NOTE_COLUMN_ENUM, noteColumnDisplay,
                           DATE_ALARM_COLUMN_ENUM, alarmColumnDisplay,
                           DATE_FLOAT_COLUMN_ENUM, floatColumnDisplay,
                           DATE_APPT_COLUMN_ENUM, str2,
                           DATE_DATA_COLUMN_ENUM, &(temp_cel->mcale),
                           DATE_BACKGROUND_COLOR_ENUM, showBgColor ? &bgColor : NULL,
                           DATE_BACKGROUND_COLOR_ENABLED_ENUM, showBgColor,
                           -1);
        entries_shown++;
    }



    /* If there are items in the list, highlight the selected row */
    if (entries_shown > 0) {
        /* First, select any record being searched for */
        if (glob_find_id) {
            datebook_find();
        }
            /* Second, try the currently selected row */
        else if (row_selected < entries_shown) {
            gtk_tree_model_foreach(GTK_TREE_MODEL(listStore), selectDateRecordByRow, NULL);

        }
            /* Third, select row 0 if nothing else is possible */
        else {
            row_selected = 0;
            gtk_tree_model_foreach(GTK_TREE_MODEL(listStore), selectDateRecordByRow, NULL);

        }
    } else {
        set_new_button_to(CLEAR_FLAG);
        appt_clear_details();
    }

    get_pref(PREF_SHOW_TOOLTIPS, &show_tooltips, NULL);
    g_snprintf(str, sizeof(str), _("%d of %d records"), entries_shown, num_entries);
    GtkTreeViewColumn *column = gtk_tree_view_get_column(GTK_TREE_VIEW(treeView), DATE_APPT_COLUMN_ENUM);
    //column->
    //column ->
    set_tooltip((int) show_tooltips, gtk_tree_view_column_get_widget(column), str);

    /* return focus to treeView after any big operation which requires a redraw */
    gtk_widget_grab_focus(GTK_WIDGET(treeView));

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

static gboolean cb_key_pressed_left_side(GtkWidget *widget,
                                         GdkEventKey *event,
                                         gpointer next_widget) {
    GtkTextBuffer *text_buffer;
    GtkTextIter iter;

    if (event->keyval == GDK_KEY_Return) {
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
    if ((event->keyval == GDK_KEY_Return) && (event->state & GDK_SHIFT_MASK)) {
        gtk_signal_emit_stop_by_name(GTK_OBJECT(widget), "key_press_event");
        gtk_widget_grab_focus(GTK_WIDGET(treeView));
        return TRUE;
    }
    /* Call external editor for note text */
    if (data != NULL &&
        (event->keyval == GDK_KEY_e) && (event->state & GDK_CONTROL_MASK)) {
        gtk_signal_emit_stop_by_name(GTK_OBJECT(widget), "key_press_event");

        /* Get current text and place in temporary file */
        GtkTextIter start_iter;
        GtkTextIter end_iter;
        char *text_out;

        gtk_text_buffer_get_bounds(GTK_TEXT_BUFFER(dbook_note_buffer),
                                   &start_iter, &end_iter);
        text_out = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(dbook_note_buffer),
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
                gtk_text_buffer_set_text(GTK_TEXT_BUFFER(dbook_note_buffer),
                                         text_in, -1);
            }
        }

        if (text_out)
            free(text_out);

        return TRUE;
    }   /* End of external editor if */

    return FALSE;
}

static void cb_record_changed(GtkWidget *widget, gpointer data) {
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

gboolean
addNewDateRecord(GtkTreeModel *model,
                 GtkTreePath *path,
                 GtkTreeIter *iter,
                 gpointer data) {

    int *i = gtk_tree_path_get_indices(path);
    if (i[0] == row_selected) {
        MyCalendarEvent *mycale = NULL;
        gtk_tree_model_get(model, iter, DATE_DATA_COLUMN_ENUM, &mycale, -1);
        addNewDateRecordToDataStructure(mycale, data);
        return TRUE;
    }

    return FALSE;


}

gboolean
deleteDateRecord(GtkTreeModel *model,
                 GtkTreePath *path,
                 GtkTreeIter *iter,
                 gpointer data) {

    int *i = gtk_tree_path_get_indices(path);
    if (i[0] == row_selected) {
        MyCalendarEvent *mycale = NULL;
        gtk_tree_model_get(model, iter, DATE_DATA_COLUMN_ENUM, &mycale, -1);
        deleteDateRecordFromDataStructure(mycale, data);
        return TRUE;
    }

    return FALSE;


}

void deleteDateRecordFromDataStructure(MyCalendarEvent *mcale, gpointer data) {
    struct CalendarEvent *cale = NULL;
    int flag;
    int dialog = 0;
    int show_priv;
    long char_set;
    unsigned int *write_unique_id;
    if (mcale < (MyCalendarEvent *) LIST_MIN_DATA) {
        return;
    }

    /* Convert to Palm character set */
    get_pref(PREF_CHAR_SET, &char_set, NULL);
    if (char_set != CHAR_SET_LATIN1) {
        if (mcale->cale.description)
            charset_j2p(mcale->cale.description, (int) (strlen(mcale->cale.description) + 1), char_set);
        if (mcale->cale.note)
            charset_j2p(mcale->cale.note, (int) (strlen(mcale->cale.note) + 1), char_set);
        if (mcale->cale.location)
            charset_j2p(mcale->cale.location, (int) (strlen(mcale->cale.location) + 1), char_set);
    }

    /* Do masking like Palm OS 3.5 */
    show_priv = show_privates(GET_PRIVATES);
    if ((show_priv != SHOW_PRIVATES) &&
        (mcale->attrib & dlpRecAttrSecret)) {
        return;
    }
    /* End Masking */
    flag = GPOINTER_TO_INT(data);
    if ((flag != MODIFY_FLAG) && (flag != DELETE_FLAG)) {
        return;
    }

    /* We need to take care of the 3 options allowed when modifying */
    /* repeating appointments */
    write_unique_id = NULL;

    if (mcale->cale.repeatType != calendarRepeatNone) {
        /* We need more user input. Pop up a dialog */
        dialog = dialog_current_future_all_cancel();
        if (dialog == DIALOG_SAID_RPT_CANCEL) {
            return;
        } else if (dialog == DIALOG_SAID_RPT_CURRENT) {
            /* Create an exception in the appointment */
            copy_calendar_event(&(mcale->cale), &cale);
            datebook_add_exception(cale, current_year, current_month, current_day);
            if ((mcale->rt == PALM_REC) || (mcale->rt == REPLACEMENT_PALM_REC)) {
                write_unique_id = &(mcale->unique_id);
            } else {
                write_unique_id = NULL;
            }
            /* Since this was really a modify, and not a delete */
            flag = MODIFY_FLAG;
        } else if (dialog == DIALOG_SAID_RPT_FUTURE) {
            /* Set an end date on the repeating event to delete future events */
            copy_calendar_event(&(mcale->cale), &cale);
            cale->repeatForever = 0;
            memset(&(cale->repeatEnd), 0, sizeof(struct tm));
            cale->repeatEnd.tm_mon = current_month;
            cale->repeatEnd.tm_mday = current_day - 1;
            cale->repeatEnd.tm_year = current_year;
            cale->repeatEnd.tm_isdst = -1;
            mktime(&(cale->repeatEnd));
            if ((mcale->rt == PALM_REC) || (mcale->rt == REPLACEMENT_PALM_REC)) {
                write_unique_id = &(mcale->unique_id);
            } else {
                write_unique_id = NULL;
            }
        }
    }
    /* Its important to write a delete record first and then a new/modified
    * record in that order. This is so that the sync code can check to see
    * if the remote record is the same as the removed, or changed local
    * or not before it goes and modifies it. */
    if (datebook_version == 0) {
        MyAppointment mappt;
        mappt.rt = mcale->rt;
        mappt.unique_id = mcale->unique_id;
        mappt.attrib = mcale->attrib;
        copy_calendarEvent_to_appointment(&(mcale->cale), &(mappt.appt));
        delete_pc_record(DATEBOOK, &mappt, flag);
        free_Appointment(&(mappt.appt));
    } else {
        delete_pc_record(CALENDAR, mcale, flag);
    }

    if (dialog == DIALOG_SAID_RPT_CURRENT ||
        dialog == DIALOG_SAID_RPT_FUTURE) {
        pc_calendar_write(cale, REPLACEMENT_PALM_REC, mcale->attrib, write_unique_id);
        free_CalendarEvent(cale);
        free(cale);
    }

    if (flag == DELETE_FLAG) {
        /* when we redraw we want to go to the line above the deleted one */
        if (row_selected > 0) {
            row_selected--;
        }
    }

    if ((flag == DELETE_FLAG) || (dialog == DIALOG_SAID_RPT_CURRENT)) {
        datebook_update_listStore();
        highlight_days();
    }
}

void addNewDateRecordToDataStructure(MyCalendarEvent *mcale, gpointer data) {
    struct CalendarEvent *cale;
    struct CalendarEvent new_cale;
    int flag;
    int dialog = 0;
    int r;
    unsigned char attrib;
    int show_priv;
    unsigned int unique_id;
    time_t t_begin, t_end;
    struct tm next_tm;
    int next_found;
    time_t ltime;
    struct tm *now;

    jp_logf(JP_LOG_DEBUG, "cb_add_new_record\n");

    flag = GPOINTER_TO_INT(data);


    unique_id = 0;

    /* Do masking like Palm OS 3.5 */
    if ((flag == COPY_FLAG) || (flag == MODIFY_FLAG)) {
        show_priv = show_privates(GET_PRIVATES);
        if (mcale < (MyCalendarEvent *) LIST_MIN_DATA) {
            return;
        }
        if ((show_priv != SHOW_PRIVATES) &&
            (mcale->attrib & dlpRecAttrSecret)) {
            return;
        }
    }
    /* End Masking */

    if (flag == CLEAR_FLAG) {
        /* Clear button was hit */
        appt_clear_details();
        connect_changed_signals(DISCONNECT_SIGNALS);
        set_new_button_to(NEW_FLAG);
        gtk_widget_grab_focus(GTK_WIDGET(dbook_desc));
        return;
    }
    if ((flag != NEW_FLAG) && (flag != MODIFY_FLAG) && (flag != COPY_FLAG)) {
        return;
    }
    if (flag == MODIFY_FLAG) {
        unique_id = mcale->unique_id;
        if (mcale < (MyCalendarEvent *) LIST_MIN_DATA) {
            return;
        }
        if ((mcale->rt == DELETED_PALM_REC) ||
            (mcale->rt == DELETED_PC_REC) ||
            (mcale->rt == MODIFIED_PALM_REC)) {
            jp_logf(JP_LOG_INFO, _("You can't modify a record that is deleted\n"));
            return;
        }
    }

    r = appt_get_details(&new_cale, &attrib);
    if (r != EXIT_SUCCESS) {
        free_CalendarEvent(&new_cale);
        return;
    }

    /* Validate dates for repeating events */
    if (new_cale.repeatType != calendarRepeatNone) {
        next_found = find_next_rpt_event(&new_cale, &(new_cale.begin), &next_tm);

        if (next_found) {
            jp_logf(JP_LOG_DEBUG, "Repeat event begin day shifted from %d to %d\n",
                    new_cale.begin.tm_mday, next_tm.tm_mday);
            new_cale.begin.tm_year = next_tm.tm_year;
            new_cale.begin.tm_mon = next_tm.tm_mon;
            new_cale.begin.tm_mday = next_tm.tm_mday;
            new_cale.begin.tm_isdst = -1;
            mktime(&(new_cale.begin));
            new_cale.end.tm_year = next_tm.tm_year;
            new_cale.end.tm_mon = next_tm.tm_mon;
            new_cale.end.tm_mday = next_tm.tm_mday;
            new_cale.end.tm_isdst = -1;
            mktime(&(new_cale.end));
        }
    }

    if ((new_cale.repeatType != calendarRepeatNone) && (!(new_cale.repeatForever))) {
        t_begin = mktime_dst_adj(&(new_cale.begin));
        t_end = mktime_dst_adj(&(new_cale.repeatEnd));
        if (t_begin > t_end) {
            dialog_generic_ok(notebook, _("Invalid Appointment"), DIALOG_ERROR,
                              _("The End Date of this appointment\nis before the start date."));
            free_CalendarEvent(&new_cale);
            return;
        }
    }

    if ((flag == MODIFY_FLAG) && (new_cale.repeatType != calendarRepeatNone)) {
        /* We need more user input. Pop up a dialog */
        dialog = dialog_current_future_all_cancel();
        if (dialog == DIALOG_SAID_RPT_CANCEL) {
            return;
        } else if (dialog == DIALOG_SAID_RPT_CURRENT) {
            /* Create an exception in the appointment */
            new_cale.repeatType = calendarRepeatNone;
            new_cale.begin.tm_year = current_year;
            new_cale.begin.tm_mon = current_month;
            new_cale.begin.tm_mday = current_day;
            new_cale.begin.tm_isdst = -1;
            mktime(&new_cale.begin);
            new_cale.repeatType = calendarRepeatNone;
            new_cale.end.tm_year = current_year;
            new_cale.end.tm_mon = current_month;
            new_cale.end.tm_mday = current_day;
            new_cale.end.tm_isdst = -1;
            mktime(&new_cale.end);
        } else if (dialog == DIALOG_SAID_RPT_FUTURE) {
            /* Change rpt. end date on old appt. to end 1 day before new event*/
            mcale->cale.repeatForever = 0;
            memset(&(mcale->cale.repeatEnd), 0, sizeof(struct tm));
            mcale->cale.repeatEnd.tm_mon = current_month;
            mcale->cale.repeatEnd.tm_mday = current_day - 1;
            mcale->cale.repeatEnd.tm_year = current_year;
            mcale->cale.repeatEnd.tm_isdst = -1;
            mktime(&(mcale->cale.repeatEnd));

            /* Create new appt. for future including exceptions from previous event */
            new_cale.begin.tm_year = current_year;
            new_cale.begin.tm_mon = current_month;
            new_cale.begin.tm_mday = current_day;
            new_cale.begin.tm_isdst = -1;
            mktime(&new_cale.begin);

            new_cale.exception = malloc(mcale->cale.exceptions * sizeof(struct tm));
            memcpy(new_cale.exception, mcale->cale.exception, mcale->cale.exceptions * sizeof(struct tm));
            new_cale.exceptions = mcale->cale.exceptions;
        } else if (dialog == DIALOG_SAID_RPT_ALL) {
            /* Keep the list of exceptions from the original record */
            new_cale.exception = malloc(mcale->cale.exceptions * sizeof(struct tm));
            memcpy(new_cale.exception, mcale->cale.exception, mcale->cale.exceptions * sizeof(struct tm));
            new_cale.exceptions = mcale->cale.exceptions;
        }
    }
    /* TODO - take care of blob and tz? */

    set_new_button_to(CLEAR_FLAG);

    /* New record */
    if (flag != MODIFY_FLAG) {
        unique_id = 0; /* Palm will supply unique_id for new record */
        pc_calendar_write(&new_cale, NEW_PC_REC, attrib, &unique_id);
    }

    if (flag == MODIFY_FLAG) {
        long char_set;

        /* Convert to Palm character set */
        get_pref(PREF_CHAR_SET, &char_set, NULL);
        if (char_set != CHAR_SET_LATIN1) {
            if (mcale->cale.description)
                charset_j2p(mcale->cale.description, (int) (strlen(mcale->cale.description) + 1), char_set);
            if (mcale->cale.note)
                charset_j2p(mcale->cale.note, (int) (strlen(mcale->cale.note) + 1), char_set);
            if (mcale->cale.location)
                charset_j2p(mcale->cale.location, (int) (strlen(mcale->cale.location) + 1), char_set);
            /* TODO blob and tz? */
        }

        if (datebook_version == 0) {
            MyAppointment mappt;
            mappt.rt = mcale->rt;
            mappt.unique_id = mcale->unique_id;
            mappt.attrib = mcale->attrib;
            copy_calendarEvent_to_appointment(&(mcale->cale), &(mappt.appt));
            delete_pc_record(DATEBOOK, &mappt, flag);
            free_Appointment(&(mappt.appt));
        } else {
            delete_pc_record(CALENDAR, mcale, flag);
        }

        /* We need to take care of the 3 options allowed when modifying
       * repeating appointments */
        if (dialog == DIALOG_SAID_RPT_CURRENT) {
            copy_calendar_event(&(mcale->cale), &cale);
            /* TODO rename? */
            datebook_add_exception(cale, current_year, current_month, current_day);
            if ((mcale->rt == PALM_REC) || (mcale->rt == REPLACEMENT_PALM_REC)) {
                /* The original record gets the same ID, this exception gets a new one. */
                pc_calendar_write(cale, REPLACEMENT_PALM_REC, attrib, &unique_id);
            } else {
                pc_calendar_write(cale, NEW_PC_REC, attrib, NULL);
            }
            unique_id = 0;
            pc_calendar_write(&new_cale, NEW_PC_REC, attrib, &unique_id);
            free_CalendarEvent(cale);
            free(cale);
        } else if (dialog == DIALOG_SAID_RPT_FUTURE) {
            /* Write old record with rpt. end date */
            if ((mcale->rt == PALM_REC) || (mcale->rt == REPLACEMENT_PALM_REC)) {
                pc_calendar_write(&(mcale->cale), REPLACEMENT_PALM_REC, attrib, &unique_id);
            } else {
                unique_id = 0;
                pc_calendar_write(&(mcale->cale), NEW_PC_REC, attrib, &unique_id);
            }
            /* Write new record with future rpt. events */
            unique_id = 0; /* Palm will supply unique_id for new record */
            pc_calendar_write(&new_cale, NEW_PC_REC, attrib, &unique_id);
        } else {
            if ((mcale->rt == PALM_REC) || (mcale->rt == REPLACEMENT_PALM_REC)) {
                pc_calendar_write(&new_cale, REPLACEMENT_PALM_REC, attrib, &unique_id);
            } else {
                unique_id = 0;
                pc_calendar_write(&new_cale, NEW_PC_REC, attrib, &unique_id);
            }
        }
    }

    /* Position calendar on the actual event or next future occurrence depending
    * on what is closest to the current date */
    if ((flag != COPY_FLAG)) {
        if (new_cale.repeatType == calendarRepeatNone) {
            memcpy(&next_tm, &(new_cale.begin), sizeof(next_tm));
        } else {
            time(&ltime);
            now = localtime(&ltime);
            next_found = find_next_rpt_event(&new_cale, now, &next_tm);
            if (!next_found) {
                memcpy(&next_tm, &(new_cale.begin), sizeof(next_tm));
            }
        }

        gtk_calendar_freeze(GTK_CALENDAR(main_calendar));
        /* Unselect current day before changing to a new month.
       * This prevents a GTK error when the new month does not have the
       * same number of days.  Example: attempting to switch from
       * Jan. 31 to Feb. 31 */
        gtk_calendar_select_day(GTK_CALENDAR(main_calendar), 0);
        gtk_calendar_select_month(GTK_CALENDAR(main_calendar),
                                  (guint) next_tm.tm_mon,
                                  (guint) (next_tm.tm_year + 1900));
        gtk_calendar_select_day(GTK_CALENDAR(main_calendar), (guint) next_tm.tm_mday);
        gtk_calendar_thaw(GTK_CALENDAR(main_calendar));
    }

    free_CalendarEvent(&new_cale);

    highlight_days();
    /* Don't return to modified record if search gui active */
    if (!glob_find_id) {
        glob_find_id = unique_id;
    }
    datebook_update_listStore();


    /* Make sure that the next alarm will go off */
    alarms_find_next(NULL, NULL, TRUE);

    return;
}

static void cb_add_new_record(GtkWidget *widget, gpointer data) {
    if (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(listStore), NULL) != 0) {
        gtk_tree_model_foreach(GTK_TREE_MODEL(listStore), addNewDateRecord, data);
    } else {
        //no records exist in category yet.
        addNewDateRecordToDataStructure(NULL, data);
    }
}


static void cb_delete_appt(GtkWidget *widget, gpointer data) {
    gtk_tree_model_foreach(GTK_TREE_MODEL(listStore), deleteDateRecord, data);
    return;
}

gboolean undeleteDateRecord(GtkTreeModel *model,
                            GtkTreePath *path,
                            GtkTreeIter *iter,
                            gpointer data) {
    int *i = gtk_tree_path_get_indices(path);
    if (i[0] == row_selected) {
        MyCalendarEvent *mcale = NULL;
        gtk_tree_model_get(model, iter, DATE_DATA_COLUMN_ENUM, &mcale, -1);
        undeleteDate(mcale, data);
        return TRUE;
    }

    return FALSE;


}

void undeleteDate(MyCalendarEvent *mcale, gpointer data) {
    int flag;
    int show_priv;

    if (mcale < (MyCalendarEvent *) LIST_MIN_DATA) {
        return;
    }

    /* Do masking like Palm OS 3.5 */
    show_priv = show_privates(GET_PRIVATES);
    if ((show_priv != SHOW_PRIVATES) &&
        (mcale->attrib & dlpRecAttrSecret)) {
        return;
    }
    /* End Masking */

    jp_logf(JP_LOG_DEBUG, "mcale->unique_id = %d\n", mcale->unique_id);
    jp_logf(JP_LOG_DEBUG, "mcale->rt = %d\n", mcale->rt);

    flag = GPOINTER_TO_INT(data);
    if (flag == UNDELETE_FLAG) {
        if (mcale->rt == DELETED_PALM_REC ||
            mcale->rt == DELETED_PC_REC) {
            if (datebook_version == 0) {
                MyAppointment mappt;
                mappt.unique_id = mcale->unique_id;
                undelete_pc_record(DATEBOOK, &mappt, flag);
            } else {
                undelete_pc_record(CALENDAR, mcale, flag);
            }
        }
        /* Possible later addition of undelete for modified records
      else if (mcale->rt == MODIFIED_PALM_REC)
      {
         cb_add_new_record(widget, GINT_TO_POINTER(COPY_FLAG));
      }
      */
    }

    datebook_update_listStore();
    highlight_days();
}

static void cb_undelete_appt(GtkWidget *widget, gpointer data) {
    gtk_tree_model_foreach(GTK_TREE_MODEL(listStore), undeleteDateRecord, data);
    return;
}

static void cb_check_button_alarm(GtkWidget *widget, gpointer data) {
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
        gtk_widget_show(hbox_alarm2);
    } else {
        gtk_widget_hide(hbox_alarm2);
    }
}

static void cb_radio_button_no_time(GtkWidget *widget, gpointer data) {
    /* GTK does not handle nested callbacks well!
    * When a time is selected from the drop-down menus cb_menu_time
    * is called.  cb_menu_time, in turn, de-selects the notime checkbutton
    * which causes a signal to be generated which invokes
    * cb_radio_button_no_time.  Finally, both callback routines call
    * set_begin_end_labels and in that routine is a call which sets the
    * currently selected item in the gtk_option_menu.  This sequence of
    * events screws up the option menu for the first click.  One solution
    * would be to disable signals in cb_menu_time, de-select the checkbutton,
    * and then re-enable signals.  Given the frequency with which the menus
    * are used this solution involves too much of a performance hit.
    * Instead, this routine only updates the entry widgets and the menus
    * are left alone.  Currently(20060111) this produces no difference in
    * jpilot behavior because the menus have been set to correct values in
    * other routines. */
    /*
   set_begin_end_labels(&begin_date, &end_date, UPDATE_DATE_ENTRIES |
                                                UPDATE_DATE_MENUS);
   */
    set_begin_end_labels(&begin_date, &end_date, UPDATE_DATE_ENTRIES);
}

static void cb_check_button_endon(GtkWidget *widget, gpointer data) {
    GtkWidget *Pbutton;
    struct tm *Pt;

    switch (GPOINTER_TO_INT(data)) {
        case PAGE_DAY:
            Pbutton = glob_endon_day_button;
            Pt = &glob_endon_day_tm;
            break;
        case PAGE_WEEK:
            Pbutton = glob_endon_week_button;
            Pt = &glob_endon_week_tm;
            break;
        case PAGE_MONTH:
            Pbutton = glob_endon_mon_button;
            Pt = &glob_endon_mon_tm;
            break;
        case PAGE_YEAR:
            Pbutton = glob_endon_year_button;
            Pt = &glob_endon_year_tm;
            break;
        default:
            return;
    }
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
        update_endon_button(Pbutton, Pt);
    } else {
        gtk_label_set_text(GTK_LABEL(gtk_bin_get_child(GTK_BIN(Pbutton))), _("No Date"));
    }
}

static gboolean handleDateRowSelection(GtkTreeSelection *selection,
                                       GtkTreeModel *model,
                                       GtkTreePath *path,
                                       gboolean path_currently_selected,
                                       gpointer userdata) {
    GtkTreeIter iter;
    struct CalendarEvent *cale;
    MyCalendarEvent *mcale;
    char tempstr[20];
    int i, b;
    int sorted_position;
    unsigned int unique_id = 0;
#ifdef ENABLE_DATEBK
    int type;
    char *note;
    int len;
    long use_db3_tags;
#endif

#ifdef ENABLE_DATEBK
    get_pref(PREF_USE_DB3, &use_db3_tags, NULL);
#endif
    if ((gtk_tree_model_get_iter(model, &iter, path)) && (!path_currently_selected)) {

        int *index = gtk_tree_path_get_indices(path);
        row_selected = index[0];
        gtk_tree_model_get(model, &iter, DATE_DATA_COLUMN_ENUM, &mcale, -1);
        if ((record_changed == MODIFY_FLAG) || (record_changed == NEW_FLAG)) {
            if (mcale != NULL) {
                unique_id = mcale->unique_id;
            }

            b = dialog_save_changed_record_with_cancel(pane, record_changed);
            if (b == DIALOG_SAID_1) { /* Cancel */
                return TRUE;
            }
            if (b == DIALOG_SAID_3) { /* Save */
                cb_add_new_record(NULL, GINT_TO_POINTER(record_changed));
            }

            set_new_button_to(CLEAR_FLAG);

            if (unique_id) {
                glob_find_id = unique_id;
                datebook_find();
            }
            return TRUE;
        }


        if (mcale == NULL) {
            return TRUE;

        }

        if (mcale->rt == DELETED_PALM_REC ||
            (mcale->rt == DELETED_PC_REC))
            /* Possible later addition of undelete code for modified deleted records
             || mcale->rt == MODIFIED_PALM_REC
          */
        {
            set_new_button_to(UNDELETE_FLAG);
        } else {
            set_new_button_to(CLEAR_FLAG);
        }

        connect_changed_signals(DISCONNECT_SIGNALS);

        cale = &(mcale->cale);

        if (datebook_version) {
            /* Calendar supports categories */
            index = (int *) (mcale->attrib & 0x0F);
            sorted_position = find_sort_cat_pos((int) index);
            int pos = findSortedPostion(sorted_position, GTK_COMBO_BOX(category_menu2));
            if (pos != sorted_position && index != 0) {
                /* Illegal category */
                jp_logf(JP_LOG_DEBUG, "Category is not legal\n");
                index = 0;
                sorted_position = find_sort_cat_pos((int) index);
            }
            if (sorted_position < 0) {
                jp_logf(JP_LOG_WARN, _("Category is not legal\n"));
            } else {
                gtk_combo_box_set_active(GTK_COMBO_BOX(category_menu2), find_menu_cat_pos(sorted_position));
            }
        }   /* End check for datebook version */

        if (cale->event) {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_button_no_time), TRUE);
        } else {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_button_appt_time), TRUE);
        }

        gtk_text_buffer_set_text(GTK_TEXT_BUFFER(dbook_desc_buffer), "", -1);
        gtk_text_buffer_set_text(GTK_TEXT_BUFFER(dbook_note_buffer), "", -1);
        if (datebook_version) {
            gtk_entry_set_text(GTK_ENTRY(location_entry), "");
        }
#ifdef ENABLE_DATEBK
        if (use_db3_tags) {
            gtk_entry_set_text(GTK_ENTRY(datebk_entry), "");
        }
#endif

        if (cale->alarm) {
            /* This is to insure that the callback gets called */
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_button_alarm), FALSE);
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_button_alarm), TRUE);
            switch (cale->advanceUnits) {
                case calendar_advMinutes:
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
                                                 (radio_button_alarm_min), TRUE);
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
                                                 (radio_button_alarm_hour), FALSE);
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
                                                 (radio_button_alarm_day), FALSE);
                    break;
                case calendar_advHours:
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
                                                 (radio_button_alarm_min), FALSE);
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
                                                 (radio_button_alarm_hour), TRUE);
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
                                                 (radio_button_alarm_day), FALSE);
                    break;
                case calendar_advDays:
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
                                                 (radio_button_alarm_min), FALSE);
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
                                                 (radio_button_alarm_hour), FALSE);
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
                                                 (radio_button_alarm_day), TRUE);
                    break;
                default:
                    jp_logf(JP_LOG_WARN, _("Error in DateBookDB or Calendar advanceUnits = %d\n"), cale->advanceUnits);
            }
            sprintf(tempstr, "%d", cale->advance);
            gtk_entry_set_text(GTK_ENTRY(units_entry), tempstr);
        } else {
            /* This is to insure that the callback gets called */
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_button_alarm), TRUE);
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_button_alarm), FALSE);
            gtk_entry_set_text(GTK_ENTRY(units_entry), "0");
        }
        if (cale->description) {
            gtk_text_buffer_set_text(GTK_TEXT_BUFFER(dbook_desc_buffer), cale->description, -1);
        }
#ifdef ENABLE_DATEBK
        if (use_db3_tags) {
            if (db3_parse_tag(cale->note, &type, NULL) > 0) {
                /* There is a datebk tag.  Need to separate it from the note */
                note = strdup(cale->note);
                len = (int) strlen(note);
                for (i = 0; i < len; i++) {
                    if (note[i] == '\n') {
                        note[i] = '\0';
                        break;
                    }
                }
                gtk_entry_set_text(GTK_ENTRY(datebk_entry), note);
                gtk_text_buffer_set_text(GTK_TEXT_BUFFER(dbook_note_buffer), &(note[i + 1]), -1);
                free(note);
            } else if (cale->note) {
                gtk_text_buffer_set_text(GTK_TEXT_BUFFER(dbook_note_buffer), cale->note, -1);
            }
        } else if (cale->note) { /* Not using db3 tags */
            gtk_text_buffer_set_text(GTK_TEXT_BUFFER(dbook_note_buffer), cale->note, -1);

        }
#else  /* Ordinary, non-DateBk code */
        if (cale->note) {
          gtk_text_buffer_set_text(GTK_TEXT_BUFFER(dbook_note_buffer), cale->note, -1);
       }
#endif

        begin_date.tm_mon = cale->begin.tm_mon;
        begin_date.tm_mday = cale->begin.tm_mday;
        begin_date.tm_year = cale->begin.tm_year;
        begin_date.tm_hour = cale->begin.tm_hour;
        begin_date.tm_min = cale->begin.tm_min;

        end_date.tm_mon = cale->end.tm_mon;
        end_date.tm_mday = cale->end.tm_mday;
        end_date.tm_year = cale->end.tm_year;
        end_date.tm_hour = cale->end.tm_hour;
        end_date.tm_min = cale->end.tm_min;

        set_begin_end_labels(&begin_date, &end_date, UPDATE_DATE_ENTRIES |
                                                     UPDATE_DATE_MENUS);

        if (datebook_version) {
            /* Calendar has a location field */
            if (cale->location) {
                gtk_entry_set_text(GTK_ENTRY(location_entry), cale->location);
            }
        }

        /* Do the Repeat information */
        switch (cale->repeatType) {
            case calendarRepeatNone:
                gtk_notebook_set_page(GTK_NOTEBOOK(notebook), PAGE_NONE);
                break;
            case calendarRepeatDaily:
                if ((cale->repeatForever)) {
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
                                                 (check_button_day_endon), FALSE);
                } else {
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
                                                 (check_button_day_endon), TRUE);
                    glob_endon_day_tm.tm_mon = cale->repeatEnd.tm_mon;
                    glob_endon_day_tm.tm_mday = cale->repeatEnd.tm_mday;
                    glob_endon_day_tm.tm_year = cale->repeatEnd.tm_year;
                    update_endon_button(glob_endon_day_button, &glob_endon_day_tm);
                }
                sprintf(tempstr, "%d", cale->repeatFrequency);
                gtk_entry_set_text(GTK_ENTRY(repeat_day_entry), tempstr);
                gtk_notebook_set_page(GTK_NOTEBOOK(notebook), PAGE_DAY);
                break;
            case calendarRepeatWeekly:
                if ((cale->repeatForever)) {
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
                                                 (check_button_week_endon), FALSE);
                } else {
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
                                                 (check_button_week_endon), TRUE);
                    glob_endon_week_tm.tm_mon = cale->repeatEnd.tm_mon;
                    glob_endon_week_tm.tm_mday = cale->repeatEnd.tm_mday;
                    glob_endon_week_tm.tm_year = cale->repeatEnd.tm_year;
                    update_endon_button(glob_endon_week_button, &glob_endon_week_tm);
                }
                sprintf(tempstr, "%d", cale->repeatFrequency);
                gtk_entry_set_text(GTK_ENTRY(repeat_week_entry), tempstr);
                for (i = 0; i < 7; i++) {
                    gtk_toggle_button_set_active
                            (GTK_TOGGLE_BUTTON(toggle_button_repeat_days[i]),
                             cale->repeatDays[i]);
                }
                gtk_notebook_set_page(GTK_NOTEBOOK(notebook), PAGE_WEEK);
                break;
            case calendarRepeatMonthlyByDate:
            case calendarRepeatMonthlyByDay:
                jp_logf(JP_LOG_DEBUG, "repeat day=%d\n", cale->repeatDay);
                if ((cale->repeatForever)) {
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
                                                 (check_button_mon_endon), FALSE);
                } else {
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
                                                 (check_button_mon_endon), TRUE);
                    glob_endon_mon_tm.tm_mon = cale->repeatEnd.tm_mon;
                    glob_endon_mon_tm.tm_mday = cale->repeatEnd.tm_mday;
                    glob_endon_mon_tm.tm_year = cale->repeatEnd.tm_year;
                    update_endon_button(glob_endon_mon_button, &glob_endon_mon_tm);
                }
                sprintf(tempstr, "%d", cale->repeatFrequency);
                gtk_entry_set_text(GTK_ENTRY(repeat_mon_entry), tempstr);
                if (cale->repeatType == calendarRepeatMonthlyByDay) {
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
                                                 (toggle_button_repeat_mon_byday), TRUE);
                }
                if (cale->repeatType == calendarRepeatMonthlyByDate) {
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
                                                 (toggle_button_repeat_mon_bydate), TRUE);
                }
                gtk_notebook_set_page(GTK_NOTEBOOK(notebook), PAGE_MONTH);
                break;
            case calendarRepeatYearly:
                if ((cale->repeatForever)) {
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
                                                 (check_button_year_endon), FALSE);
                } else {
                    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON
                                                 (check_button_year_endon), TRUE);

                    glob_endon_year_tm.tm_mon = cale->repeatEnd.tm_mon;
                    glob_endon_year_tm.tm_mday = cale->repeatEnd.tm_mday;
                    glob_endon_year_tm.tm_year = cale->repeatEnd.tm_year;
                    update_endon_button(glob_endon_year_button, &glob_endon_year_tm);
                }
                sprintf(tempstr, "%d", cale->repeatFrequency);
                gtk_entry_set_text(GTK_ENTRY(repeat_year_entry), tempstr);

                gtk_notebook_set_page(GTK_NOTEBOOK(notebook), PAGE_YEAR);
                break;
            default:
                jp_logf(JP_LOG_WARN, _("Unknown repeatType (%d) found in DatebookDB\n"), cale->repeatFrequency);
        }

        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(private_checkbox),
                                     mcale->attrib & dlpRecAttrSecret);

        connect_changed_signals(CONNECT_SIGNALS);
    }

    return TRUE;
}

static void set_date_label(void) {
    struct tm now;
    char str[50];
    char datef[50];
    const char *svalue;

    now.tm_sec = 0;
    now.tm_min = 0;
    now.tm_hour = 11;
    now.tm_isdst = -1;
    now.tm_wday = 0;
    now.tm_yday = 0;
    now.tm_mday = current_day;
    now.tm_mon = current_month;
    now.tm_year = current_year;
    mktime(&now);

    get_pref(PREF_LONGDATE, NULL, &svalue);
    if (svalue == NULL) {
        strcpy(datef, "%x");
    } else {
        sprintf(datef, _("%%a., %s"), svalue);
    }

    /* Determine today for highlighting */
    if (now.tm_mday == get_highlighted_today(&now))
        strcat(datef, _(" (TODAY)"));

    jp_strftime(str, sizeof(str), datef, &now);
    gtk_label_set_text(GTK_LABEL(dow_label), str);
}

static void cb_cancel(GtkWidget *widget, gpointer data) {
    set_new_button_to(CLEAR_FLAG);
    datebook_refresh(FALSE, FALSE);
}

static void cb_edit_cats(GtkWidget *widget, gpointer data) {
    struct CalendarAppInfo cai;
    char db_name[FILENAME_MAX];
    char pdb_name[FILENAME_MAX];
    char full_name[FILENAME_MAX];
    int num;
    size_t size;
    void *buf;
    struct pi_file *pf;
    long datebook_version;
    pi_buffer_t pi_buf;

    jp_logf(JP_LOG_DEBUG, "cb_edit_cats\n");

    get_pref(PREF_DATEBOOK_VERSION, &datebook_version, NULL);

    if (datebook_version) {
        strcpy(pdb_name, "CalendarDB-PDat.pdb");
        strcpy(db_name, "CalendarDB-PDat");
    } else {
        /* Datebook doesn't use categories */
        return;
    }

    get_home_file_name(pdb_name, full_name, sizeof(full_name));

    buf = NULL;
    memset(&cai, 0, sizeof(cai));

    pf = pi_file_open(full_name);
    pi_file_get_app_info(pf, &buf, &size);

    pi_buf.data = buf;
    pi_buf.used = size;
    pi_buf.allocated = size;

    num = unpack_CalendarAppInfo(&cai, &pi_buf);
    if (num <= 0) {
        jp_logf(JP_LOG_WARN, _("Error reading file: %s\n"), pdb_name);
        return;
    }

    pi_file_close(pf);

    edit_cats(widget, db_name, &(cai.category));

    pi_buf.data = NULL;
    pi_buf.used = 0;
    pi_buf.allocated = 0;
    size = (size_t) pack_CalendarAppInfo(&cai, &pi_buf);

    pdb_file_write_app_block(db_name, pi_buf.data, pi_buf.used);

    free(pi_buf.data);

    cb_app_button(NULL, GINT_TO_POINTER(REDRAW));

}

static void cb_category(GtkComboBox *item, int selection) {
    int b;
    if (gtk_combo_box_get_active(GTK_COMBO_BOX(item)) < 0) {
        return;
    }
    int selectedItem = get_selected_category_from_combo_box(item);

    if (dbook_category == selectedItem) { return; }

#ifdef JPILOT_DEBUG
    printf("dbook_category: %d, selection: %d\n", dbook_category, selection);
#endif

    b = dialog_save_changed_record_with_cancel(pane, record_changed);
    if (b == DIALOG_SAID_1) { /* Cancel */
        int index, index2;

        if (dbook_category == CATEGORY_ALL) {
            index = 0;
            index2 = 0;
        } else {
            index = find_sort_cat_pos(dbook_category);
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
        cb_edit_cats(item, NULL);
    } else {
        dbook_category = selectedItem;
    }
    row_selected = 0;
    jp_logf(JP_LOG_DEBUG, "cb_category() cat=%d\n", dbook_category);
    datebook_update_listStore();
    highlight_days();
    jp_logf(JP_LOG_DEBUG, "Leaving cb_category()\n");

}

/* When a calendar day is pressed */
static void cb_cal_changed(GtkWidget *widget,
                           gpointer data) {
    int num;
    unsigned int cal_year, cal_month, cal_day;
    int day_changed, mon_changed, year_changed;
    int b;
#ifdef EASTER
    static int Easter=0;
#endif

    num = GPOINTER_TO_INT(data);

    if (num != CAL_DAY_SELECTED) {
        return;
    }

    /* Get selected date from calendar */
    gtk_calendar_get_date(GTK_CALENDAR(main_calendar),
                          &cal_year, &cal_month, &cal_day);

    /* Handle modified record before switching to new date */
    if ((record_changed == MODIFY_FLAG) || (record_changed == NEW_FLAG)) {
        if (current_day == cal_day) { return; }

        b = dialog_save_changed_record_with_cancel(pane, record_changed);
        if (b == DIALOG_SAID_1) { /* Cancel */
            gtk_signal_disconnect_by_func(GTK_OBJECT(main_calendar),
                                          GTK_SIGNAL_FUNC(cb_cal_changed),
                                          GINT_TO_POINTER(CAL_DAY_SELECTED));
            gtk_calendar_select_month(GTK_CALENDAR(main_calendar), (guint) current_month,
                                      (guint) (1900 + current_year));
            gtk_calendar_select_day(GTK_CALENDAR(main_calendar), (guint) current_day);
            gtk_signal_connect(GTK_OBJECT(main_calendar),
                               "day_selected", GTK_SIGNAL_FUNC(cb_cal_changed),
                               GINT_TO_POINTER(CAL_DAY_SELECTED));
            return;
        }
        if (b == DIALOG_SAID_3) { /* Save */
            /* cb_add_new_record is troublesome because it attempts to
          * change the calendar. Not only must signals be disconnected
          * to avoid re-triggering cb_cal_changed but the original date
          * must be re-selected after the add_new_record has changed it */

            gtk_signal_disconnect_by_func(GTK_OBJECT(main_calendar),
                                          GTK_SIGNAL_FUNC(cb_cal_changed),
                                          GINT_TO_POINTER(CAL_DAY_SELECTED));

            cb_add_new_record(NULL, GINT_TO_POINTER(record_changed));

            gtk_calendar_freeze(GTK_CALENDAR(main_calendar));
            gtk_calendar_select_month(GTK_CALENDAR(main_calendar),
                                      cal_month,
                                      cal_year);
            gtk_calendar_select_day(GTK_CALENDAR(main_calendar), cal_day);
            gtk_calendar_thaw(GTK_CALENDAR(main_calendar));

            gtk_signal_connect(GTK_OBJECT(main_calendar),
                               "day_selected", GTK_SIGNAL_FUNC(cb_cal_changed),
                               GINT_TO_POINTER(CAL_DAY_SELECTED));
        }
    }

    set_new_button_to(CLEAR_FLAG);

    /* Day 0 is used in GTK to unselect the current highlighted day --
    * NOT to change to the zeroeth day */
    if (cal_day == 0) {
        return;
    }

    mon_changed = year_changed = 0;

    if (cal_year < 1903) {
        cal_year = 1903;
        gtk_calendar_select_month(GTK_CALENDAR(main_calendar),
                                  cal_month, 1903);
    }
    if (cal_year > 2037) {
        cal_year = 2037;
        gtk_calendar_select_month(GTK_CALENDAR(main_calendar),
                                  cal_month, 2037);
    }

    if (current_year != cal_year - 1900) {
        current_year = cal_year - 1900;
        year_changed = 1;
        mon_changed = 1;
    }
    if (current_month != cal_month) {
        current_month = cal_month;
        mon_changed = 1;
    }
    day_changed = (current_day != cal_day);
    current_day = cal_day;

    jp_logf(JP_LOG_DEBUG, "cb_cal_changed, %02d/%02d/%02d\n",
            cal_month, cal_day, cal_year);

    /* Easter Egg Code */
#ifdef EASTER
                                                                                                                            if (((current_day==29) && (current_month==7)) ||
       ((current_day==20) && (current_month==11))) {
      Easter++;
      if (Easter>4) {
         Easter=0;
         dialog_easter(current_day);
      }
   } else {
      Easter=0;
   }
#endif

    if (mon_changed) {
        highlight_days();
    }
    if (day_changed || mon_changed || year_changed) {
        set_date_label();
        row_selected = 0;
    }
    datebook_update_listStore();

    /* Keep focus on calendar so that GTK accelerator keys for calendar
    * can continue to be used */
    gtk_widget_grab_focus(GTK_WIDGET(main_calendar));
}

/* Called by week and month views when a user clicks on a date so that we
 * can set the day view to that date */
void datebook_gui_setdate(int year, int month, int day) {
    /* Reset current day pointers to the day the user click on */
    current_year = year;
    current_month = month;
    current_day = day;

    /* Redraw the day view */
    datebook_refresh(FALSE, FALSE);

    /* Force exposure of main window */
    if (window) {
        gtk_window_present(GTK_WINDOW(window));
    }
}

static void highlight_days(void) {
    int bit, mask;
    int dow_int, ndim, i;
    long ivalue;

    get_pref(PREF_DATEBOOK_HIGHLIGHT_DAYS, &ivalue, NULL);
    if (!ivalue) {
        return;
    }

    get_month_info(current_month, 1, current_year, &dow_int, &ndim);

    appointment_on_day_list(current_month, current_year, &mask, dbook_category, (int) datebook_version);

    gtk_calendar_freeze(GTK_CALENDAR(main_calendar));

    for (i = 1, bit = 1; i <= ndim; i++, bit = bit << 1) {
        if (bit & mask) {
            gtk_calendar_mark_day(GTK_CALENDAR(main_calendar), (guint) i);
        } else {
            gtk_calendar_unmark_day(GTK_CALENDAR(main_calendar), (guint) i);
        }
    }
    gtk_calendar_thaw(GTK_CALENDAR(main_calendar));
}

gboolean
findDateRecord(GtkTreeModel *model,
               GtkTreePath *path,
               GtkTreeIter *iter,
               gpointer data) {

    if (glob_find_id) {
        MyCalendarEvent *mycal = NULL;

        gtk_tree_model_get(model, iter, TODO_DATA_COLUMN_ENUM, &mycal, -1);
        if (mycal->unique_id == glob_find_id) {
            GtkTreeSelection *selection = NULL;
            selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeView));
            gtk_tree_selection_select_path(selection, path);
            gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(treeView), path, (GtkTreeViewColumn *) DATE_APPT_COLUMN_ENUM,
                                         FALSE, 1.0, 0.0);
            glob_find_id = 0;
            return TRUE;
        }
    }
    return FALSE;
}

static int datebook_find(void) {
    gtk_tree_model_foreach(GTK_TREE_MODEL(listStore), findDateRecord, NULL);
    return EXIT_SUCCESS;
}

int datebook_refresh(int first, int do_init) {
    int b;
    int index = 0;
    int index2 = 0;
    int copy_current_day;
    int copy_current_month;
    int copy_current_year;

    b = dialog_save_changed_record(pane, record_changed);
    if (b == DIALOG_SAID_2) {
        cb_add_new_record(NULL, GINT_TO_POINTER(record_changed));
    }
    set_new_button_to(CLEAR_FLAG);

    if (do_init)
        init();

    if (datebook_version) {
        /* Contacts supports categories */
        if (glob_find_id) {
            dbook_category = CATEGORY_ALL;
        }
        if (dbook_category == CATEGORY_ALL) {
            index = 0;
            index2 = 0;
        } else {
            index = find_sort_cat_pos(dbook_category);
            index2 = find_menu_cat_pos(index) + 1;
            index += 1;
        }
    }

#ifdef ENABLE_DATEBK
    if (glob_find_id) {
        if (GTK_IS_WINDOW(window_datebk_cats)) {
            cb_datebk_category(NULL, GINT_TO_POINTER(1));
        } else {
            datebk_category = 0xFFFF;
        }
    }
#endif

    /* Need to disconnect signal before using gtk_calendar_select_day
      or callback will be activated inadvertently. */
    gtk_signal_disconnect_by_func(GTK_OBJECT(main_calendar),
                                  GTK_SIGNAL_FUNC(cb_cal_changed),
                                  GINT_TO_POINTER(CAL_DAY_SELECTED));

    if (first) {
        gtk_calendar_select_month(GTK_CALENDAR(main_calendar),
                                  (guint) current_month, (guint) (current_year + 1900));
        gtk_calendar_select_day(GTK_CALENDAR(main_calendar), (guint) current_day);
    } else {
        copy_current_day = current_day;
        copy_current_month = current_month;
        copy_current_year = current_year;
        gtk_calendar_freeze(GTK_CALENDAR(main_calendar));
        /* Unselect current day before changing to a new month */
        gtk_calendar_select_day(GTK_CALENDAR(main_calendar), 0);
        gtk_calendar_select_month(GTK_CALENDAR(main_calendar),
                                  (guint) copy_current_month, (guint) (copy_current_year + 1900));
        gtk_calendar_select_day(GTK_CALENDAR(main_calendar), (guint) copy_current_day);
        gtk_calendar_thaw(GTK_CALENDAR(main_calendar));
    }
    gtk_signal_connect(GTK_OBJECT(main_calendar),
                       "day_selected", GTK_SIGNAL_FUNC(cb_cal_changed),
                       GINT_TO_POINTER(CAL_DAY_SELECTED));

    datebook_update_listStore();
    if (datebook_version) {
        if (index < 0) {
            jp_logf(JP_LOG_WARN, _("Category is not legal\n"));
        } else {
            gtk_combo_box_set_active(GTK_COMBO_BOX(category_menu1), index2);
        }
    }
    highlight_days();
    set_date_label();

    return EXIT_SUCCESS;
}

static void cb_menu_time(GtkWidget *item, gint data) {
    int span;

    if (END_TIME_FLAG & data) {
        if (HOURS_FLAG & data) {
            end_date.tm_hour = data & 0x3F;
        } else {
            end_date.tm_min = data & 0x3F;
        }
    } else {
        /* If start time changed then update end time to keep same appt. length */
        if (HOURS_FLAG & data) {
            span = end_date.tm_hour - begin_date.tm_hour;
            begin_date.tm_hour = data & 0x3F;
            span = (begin_date.tm_hour + span) % 24;
            span < 0 ? span += 24 : span;
            end_date.tm_hour = span;
        } else {
            span = end_date.tm_min - begin_date.tm_min;
            begin_date.tm_min = data & 0x3F;
            span = begin_date.tm_min + span;
            if (span >= 60) {
                span -= 60;
                end_date.tm_hour = (end_date.tm_hour + 1) % 24;
            } else if (span < 0) {
                span += 60;
                end_date.tm_hour = (end_date.tm_hour - 1) % 24;
            }
            end_date.tm_min = span;
        }
    }

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_button_appt_time), TRUE);
    set_begin_end_labels(&begin_date, &end_date, UPDATE_DATE_ENTRIES);
}

static gboolean cb_hide_menu_time(GtkWidget *widget, gpointer data) {
    /* Datebook does not support events spanning midnight where
      the beginning time is greater than the ending time */
    if (datebook_version == 0) {
        if (begin_date.tm_hour > end_date.tm_hour) {
            end_date.tm_hour = begin_date.tm_hour;
        }
    }

    set_begin_end_labels(&begin_date, &end_date, UPDATE_DATE_MENUS |
                                                 UPDATE_DATE_ENTRIES);

    return FALSE;
}

#define PRESSED_P            100
#define PRESSED_A            101
#define PRESSED_TAB          102
#define PRESSED_MINUS        103
#define PRESSED_SHIFT_TAB    104

static void entry_key_pressed(int next_digit, int end_entry) {
    struct tm *Ptm;
    int span_hour, span_min;

    if (end_entry) {
        Ptm = &end_date;
    } else {
        Ptm = &begin_date;
    }

    span_hour = end_date.tm_hour - begin_date.tm_hour;
    span_min = end_date.tm_min - begin_date.tm_min;

    if ((next_digit >= 0) && (next_digit <= 9)) {
        Ptm->tm_hour = ((Ptm->tm_hour) * 10 + (Ptm->tm_min) / 10) % 100;
        Ptm->tm_min = ((Ptm->tm_min) * 10) % 100 + next_digit;
    }
    if ((next_digit == PRESSED_P) && ((Ptm->tm_hour) < 12)) {
        (Ptm->tm_hour) += 12;
    }
    if ((next_digit == PRESSED_A) && ((Ptm->tm_hour) > 11)) {
        (Ptm->tm_hour) -= 12;
    }

    /* Don't let the first digit exceed 2 */
    if (Ptm->tm_hour / 10 > 2) {
        Ptm->tm_hour -= (Ptm->tm_hour / 10 - 2) * 10;
    }
    /* Don't let the hour be > 23 */
    if (Ptm->tm_hour > 23) {
        Ptm->tm_hour = 23;
    }

    /* If start time changed then update end time to keep same appt. length */
    if (!end_entry) {
        span_hour = (begin_date.tm_hour + span_hour) % 24;
        span_hour < 0 ? span_hour += 24 : span_hour;
        end_date.tm_hour = span_hour;

        span_min = begin_date.tm_min + span_min;
        while (span_min >= 60) {
            span_min -= 60;
            span_hour = (span_hour + 1) % 24;
        }
        while (span_min < 0) {
            span_min += 60;
            span_hour = (span_hour - 1) % 24;
        }
        end_date.tm_min = span_min;
        span_hour < 0 ? span_hour += 24 : span_hour;
        end_date.tm_hour = span_hour;
    }

    set_begin_end_labels(&begin_date, &end_date, UPDATE_DATE_ENTRIES |
                                                 UPDATE_DATE_MENUS);
}

static gboolean cb_entry_pressed(GtkWidget *w, gpointer data) {
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_button_appt_time), TRUE);

    set_begin_end_labels(&begin_date, &end_date, UPDATE_DATE_ENTRIES |
                                                 UPDATE_DATE_MENUS);

    /* return FALSE to let GTK know we did not handle the event
    * this allows GTK to finish handling it.*/
    return FALSE;
}

static gboolean cb_entry_key_pressed(GtkWidget *widget,
                                     GdkEventKey *event,
                                     gpointer data) {
    int digit = -1;

    jp_logf(JP_LOG_DEBUG, "cb_entry_key_pressed key = %d\n", event->keyval);

    gtk_signal_emit_stop_by_name(GTK_OBJECT(widget), "key_press_event");

    if ((event->keyval >= GDK_KEY_0) && (event->keyval <= GDK_KEY_9)) {
        digit = (event->keyval) - GDK_KEY_0;
    } else if ((event->keyval >= GDK_KEY_KP_0) && (event->keyval <= GDK_KEY_KP_9)) {
        digit = (event->keyval) - GDK_KEY_KP_0;
    } else if ((event->keyval == GDK_KEY_P) || (event->keyval == GDK_KEY_p)) {
        digit = PRESSED_P;
    } else if ((event->keyval == GDK_KEY_A) || (event->keyval == GDK_KEY_a)) {
        digit = PRESSED_A;
    } else if (event->keyval == GDK_KEY_Tab) {
        digit = PRESSED_TAB;
    } else if (event->keyval == GDK_KEY_ISO_Left_Tab) {
        digit = PRESSED_SHIFT_TAB;
    } else if ((event->keyval == GDK_KEY_KP_Subtract) || (event->keyval == GDK_KEY_minus)) {
        digit = PRESSED_MINUS;
    }

    /* time entry widgets are cycled focus by pressing "-"
    * Tab will go to the next widget
    * Shift-Tab will go to the previous widget */
    if ((digit == PRESSED_TAB) || (digit == PRESSED_MINUS)) {
        if (widget == begin_time_entry) {
            gtk_widget_grab_focus(GTK_WIDGET(end_time_entry));
        }
        if (widget == end_time_entry) {
            if (digit == PRESSED_MINUS) {
                gtk_widget_grab_focus(GTK_WIDGET(begin_time_entry));
            }
            if (digit == PRESSED_TAB) {
                gtk_widget_grab_focus(GTK_WIDGET(dbook_desc));
            }
        }
    } else if (digit == PRESSED_SHIFT_TAB) {
        if (widget == begin_time_entry) {
            gtk_widget_grab_focus(GTK_WIDGET(radio_button_no_time));
        } else if (widget == end_time_entry) {
            gtk_widget_grab_focus(GTK_WIDGET(begin_time_entry));
        }
    }

    if (digit >= 0) {
        if ((digit >= 0) && (digit <= 9)) {
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_button_appt_time), TRUE);
        }
        if (widget == begin_time_entry) {
            entry_key_pressed(digit, 0);
        }
        if (widget == end_time_entry) {
            entry_key_pressed(digit, 1);
        }
        return TRUE;
    }
    return FALSE;
}

static gboolean cb_key_pressed_tab(GtkWidget *widget,
                                   GdkEventKey *event,
                                   gpointer next_widget) {
    if (event->keyval == GDK_KEY_Tab) {
        gtk_signal_emit_stop_by_name(GTK_OBJECT(widget), "key_press_event");
        gtk_widget_grab_focus(GTK_WIDGET(next_widget));
        return TRUE;
    }

    return FALSE;
}

static gboolean cb_key_pressed_tab_entry(GtkWidget *widget,
                                         GdkEventKey *event,
                                         gpointer next_widget) {
    GtkTextIter cursor_pos_iter;
    GtkTextBuffer *text_buffer;

    if (event->keyval == GDK_KEY_Tab) {
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
    if (event->keyval == GDK_KEY_ISO_Left_Tab) {
        gtk_signal_emit_stop_by_name(GTK_OBJECT(widget), "key_press_event");
        gtk_widget_grab_focus(GTK_WIDGET(next_widget));
        return TRUE;
    }
    return FALSE;
}

static gboolean cb_keyboard(GtkWidget *widget, GdkEventKey *event, gpointer *p) {
    struct tm day;
    int up, down;
    int b;

    up = down = 0;
    switch (event->keyval) {
        case GDK_KEY_Page_Up:
        case GDK_KEY_KP_Page_Up:
            up = 1;
            break;
        case GDK_KEY_Page_Down:
        case GDK_KEY_KP_Page_Down:
            down = 1;
            break;
        default:
            break;
    }

    if (up || down) {
        gtk_signal_emit_stop_by_name(GTK_OBJECT(widget), "key_press_event");

        b = dialog_save_changed_record_with_cancel(pane, record_changed);
        if (b == DIALOG_SAID_1) { /* Cancel */
            return TRUE;
        }
        if (b == DIALOG_SAID_3) { /* Save */
            cb_add_new_record(NULL, GINT_TO_POINTER(record_changed));
        }
        set_new_button_to(CLEAR_FLAG);

        memset(&day, 0, sizeof(day));
        day.tm_year = current_year;
        day.tm_mon = current_month;
        day.tm_mday = current_day;
        day.tm_hour = 12;
        day.tm_min = 0;

        if (up) {
            sub_days_from_date(&day, 1);
        }
        if (down) {
            add_days_to_date(&day, 1);
        }

        current_year = day.tm_year;
        current_month = day.tm_mon;
        current_day = day.tm_mday;

        /* This next line prevents a GTK error message from being printed.
       * e.g.  If the day were 31 and the next month has <31 days then the
       * select month call will cause an error message since the 31st isn't
       * valid in that month.  0 is code for unselect the day */
        gtk_calendar_freeze(GTK_CALENDAR(main_calendar));
        gtk_calendar_select_day(GTK_CALENDAR(main_calendar), 0);
        gtk_calendar_select_month(GTK_CALENDAR(main_calendar), (guint) day.tm_mon, (guint) (day.tm_year + 1900));
        gtk_calendar_select_day(GTK_CALENDAR(main_calendar), (guint) day.tm_mday);
        gtk_calendar_thaw(GTK_CALENDAR(main_calendar));

        return TRUE;
    }
    return FALSE;
}

int datebook_gui_cleanup(void) {
    int b;

    b = dialog_save_changed_record(pane, record_changed);
    if (b == DIALOG_SAID_2) {
        cb_add_new_record(NULL, GINT_TO_POINTER(record_changed));
    }

    free_CalendarEventList(&glob_cel);
    free_ToDoList(&datebook_todo_list);

    connect_changed_signals(DISCONNECT_SIGNALS);
    if (datebook_version) {
        set_pref(PREF_LAST_DATE_CATEGORY, dbook_category, NULL, TRUE);
    }
    set_pref(PREF_DATEBOOK_PANE, gtk_paned_get_position(GTK_PANED(pane)), NULL, TRUE);
    set_pref(PREF_DATEBOOK_NOTE_PANE, gtk_paned_get_position(GTK_PANED(note_pane)), NULL, TRUE);
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(show_todos_button))) {
        set_pref(PREF_DATEBOOK_TODO_PANE, gtk_paned_get_position(GTK_PANED(todo_pane)), NULL, TRUE);
    }
    todo_liststore_clear(GTK_LIST_STORE(todo_listStore));

#ifdef ENABLE_DATEBK
    if (GTK_IS_WIDGET(window_datebk_cats)) {
        gtk_widget_destroy(window_datebk_cats);
    }
#endif
    gtk_list_store_clear(GTK_LIST_STORE(listStore));

    /* Remove the accelerators */
    gtk_window_remove_accel_group(GTK_WINDOW(gtk_widget_get_toplevel(main_calendar)), accel_group);

    gtk_signal_disconnect_by_func(GTK_OBJECT(main_calendar),
                                  GTK_SIGNAL_FUNC(cb_keyboard), NULL);
    gtk_signal_disconnect_by_func(GTK_OBJECT(treeView),
                                  GTK_SIGNAL_FUNC(cb_keyboard), NULL);

    return EXIT_SUCCESS;
}

static void connect_changed_signals(int con_or_dis) {
    int i;
    static int connected = 0;
#ifdef ENABLE_DATEBK
    long use_db3_tags;
#endif

#ifdef ENABLE_DATEBK
    get_pref(PREF_USE_DB3, &use_db3_tags, NULL);
#endif

    /* CONNECT */
    if ((con_or_dis == CONNECT_SIGNALS) && (!connected)) {
        connected = 1;

        if (datebook_version) {
            g_signal_connect(G_OBJECT(category_menu2),"changed",G_CALLBACK(cb_record_changed),NULL);
        }

        gtk_signal_connect(GTK_OBJECT(radio_button_alarm_min), "toggled",
                           GTK_SIGNAL_FUNC(cb_record_changed), NULL);
        gtk_signal_connect(GTK_OBJECT(radio_button_alarm_hour), "toggled",
                           GTK_SIGNAL_FUNC(cb_record_changed), NULL);
        gtk_signal_connect(GTK_OBJECT(radio_button_alarm_day), "toggled",
                           GTK_SIGNAL_FUNC(cb_record_changed), NULL);

        gtk_signal_connect(GTK_OBJECT(check_button_alarm), "toggled",
                           GTK_SIGNAL_FUNC(cb_record_changed), NULL);

        gtk_signal_connect(GTK_OBJECT(radio_button_no_time), "toggled",
                           GTK_SIGNAL_FUNC(cb_record_changed), NULL);

        gtk_signal_connect(GTK_OBJECT(radio_button_appt_time), "toggled",
                           GTK_SIGNAL_FUNC(cb_record_changed), NULL);

        gtk_signal_connect(GTK_OBJECT(begin_date_button), "pressed",
                           GTK_SIGNAL_FUNC(cb_record_changed), NULL);

        gtk_signal_connect(GTK_OBJECT(begin_time_entry), "changed",
                           GTK_SIGNAL_FUNC(cb_record_changed), NULL);
        gtk_signal_connect(GTK_OBJECT(end_time_entry), "changed",
                           GTK_SIGNAL_FUNC(cb_record_changed), NULL);

        gtk_signal_connect(GTK_OBJECT(units_entry), "changed",
                           GTK_SIGNAL_FUNC(cb_record_changed), NULL);

        g_signal_connect(dbook_desc_buffer, "changed",
                         GTK_SIGNAL_FUNC(cb_record_changed), NULL);
        g_signal_connect(dbook_note_buffer, "changed",
                         GTK_SIGNAL_FUNC(cb_record_changed), NULL);
        if (datebook_version) {
            g_signal_connect(location_entry, "changed",
                             GTK_SIGNAL_FUNC(cb_record_changed), NULL);
        }
#ifdef ENABLE_DATEBK
        if (use_db3_tags) {
            if (datebk_entry) {
                gtk_signal_connect(GTK_OBJECT(datebk_entry), "changed",
                                   GTK_SIGNAL_FUNC(cb_record_changed), NULL);
            }
        }
#endif

        gtk_signal_connect(GTK_OBJECT(notebook), "switch-page",
                           GTK_SIGNAL_FUNC(cb_record_changed), NULL);

        gtk_signal_connect(GTK_OBJECT(repeat_day_entry), "changed",
                           GTK_SIGNAL_FUNC(cb_record_changed), NULL);
        gtk_signal_connect(GTK_OBJECT(repeat_week_entry), "changed",
                           GTK_SIGNAL_FUNC(cb_record_changed), NULL);
        gtk_signal_connect(GTK_OBJECT(repeat_mon_entry), "changed",
                           GTK_SIGNAL_FUNC(cb_record_changed), NULL);
        gtk_signal_connect(GTK_OBJECT(repeat_year_entry), "changed",
                           GTK_SIGNAL_FUNC(cb_record_changed), NULL);

        gtk_signal_connect(GTK_OBJECT(check_button_day_endon), "toggled",
                           GTK_SIGNAL_FUNC(cb_record_changed), NULL);
        gtk_signal_connect(GTK_OBJECT(check_button_week_endon), "toggled",
                           GTK_SIGNAL_FUNC(cb_record_changed), NULL);
        gtk_signal_connect(GTK_OBJECT(check_button_mon_endon), "toggled",
                           GTK_SIGNAL_FUNC(cb_record_changed), NULL);
        gtk_signal_connect(GTK_OBJECT(check_button_year_endon), "toggled",
                           GTK_SIGNAL_FUNC(cb_record_changed), NULL);

        gtk_signal_connect(GTK_OBJECT(glob_endon_day_button), "pressed",
                           GTK_SIGNAL_FUNC(cb_record_changed), NULL);
        gtk_signal_connect(GTK_OBJECT(glob_endon_week_button), "pressed",
                           GTK_SIGNAL_FUNC(cb_record_changed), NULL);
        gtk_signal_connect(GTK_OBJECT(glob_endon_mon_button), "pressed",
                           GTK_SIGNAL_FUNC(cb_record_changed), NULL);
        gtk_signal_connect(GTK_OBJECT(glob_endon_year_button), "pressed",
                           GTK_SIGNAL_FUNC(cb_record_changed), NULL);

        gtk_signal_connect(GTK_OBJECT(toggle_button_repeat_mon_byday), "toggled",
                           GTK_SIGNAL_FUNC(cb_record_changed), NULL);
        gtk_signal_connect(GTK_OBJECT(toggle_button_repeat_mon_bydate), "toggled",
                           GTK_SIGNAL_FUNC(cb_record_changed), NULL);
        gtk_signal_connect(GTK_OBJECT(private_checkbox), "toggled",
                           GTK_SIGNAL_FUNC(cb_record_changed), NULL);

        for (i = 0; i < 7; i++) {
            gtk_signal_connect(GTK_OBJECT(toggle_button_repeat_days[i]), "toggled",
                               GTK_SIGNAL_FUNC(cb_record_changed), NULL);
        }
    }

    /* DISCONNECT */
    if ((con_or_dis == DISCONNECT_SIGNALS) && (connected)) {
        connected = 0;

        if (datebook_version) {
            g_signal_handlers_disconnect_by_func(G_OBJECT(category_menu2),G_CALLBACK(cb_record_changed),NULL);
        }

        gtk_signal_disconnect_by_func(GTK_OBJECT(radio_button_alarm_min),
                                      GTK_SIGNAL_FUNC(cb_record_changed), NULL);
        gtk_signal_disconnect_by_func(GTK_OBJECT(radio_button_alarm_hour),
                                      GTK_SIGNAL_FUNC(cb_record_changed), NULL);
        gtk_signal_disconnect_by_func(GTK_OBJECT(radio_button_alarm_day),
                                      GTK_SIGNAL_FUNC(cb_record_changed), NULL);

        gtk_signal_disconnect_by_func(GTK_OBJECT(check_button_alarm),
                                      GTK_SIGNAL_FUNC(cb_record_changed), NULL);

        gtk_signal_disconnect_by_func(GTK_OBJECT(radio_button_no_time),
                                      GTK_SIGNAL_FUNC(cb_record_changed), NULL);

        gtk_signal_disconnect_by_func(GTK_OBJECT(radio_button_appt_time),
                                      GTK_SIGNAL_FUNC(cb_record_changed), NULL);

        gtk_signal_disconnect_by_func(GTK_OBJECT(begin_date_button),
                                      GTK_SIGNAL_FUNC(cb_record_changed), NULL);

        gtk_signal_disconnect_by_func(GTK_OBJECT(begin_time_entry),
                                      GTK_SIGNAL_FUNC(cb_record_changed), NULL);
        gtk_signal_disconnect_by_func(GTK_OBJECT(end_time_entry),
                                      GTK_SIGNAL_FUNC(cb_record_changed), NULL);

        gtk_signal_disconnect_by_func(GTK_OBJECT(units_entry),
                                      GTK_SIGNAL_FUNC(cb_record_changed), NULL);

        g_signal_handlers_disconnect_by_func(dbook_desc_buffer,
                                             GTK_SIGNAL_FUNC(cb_record_changed),
                                             NULL);
        g_signal_handlers_disconnect_by_func(dbook_note_buffer,
                                             GTK_SIGNAL_FUNC(cb_record_changed),
                                             NULL);
        if (datebook_version) {
            g_signal_handlers_disconnect_by_func(location_entry,
                                                 GTK_SIGNAL_FUNC(cb_record_changed), NULL);
        }
#ifdef ENABLE_DATEBK
        if (use_db3_tags) {
            if (datebk_entry) {
                gtk_signal_disconnect_by_func(GTK_OBJECT(datebk_entry),
                                              GTK_SIGNAL_FUNC(cb_record_changed),
                                              NULL);
            }
        }
#endif

        gtk_signal_disconnect_by_func(GTK_OBJECT(notebook),
                                      GTK_SIGNAL_FUNC(cb_record_changed), NULL);

        gtk_signal_disconnect_by_func(GTK_OBJECT(repeat_day_entry),
                                      GTK_SIGNAL_FUNC(cb_record_changed), NULL);
        gtk_signal_disconnect_by_func(GTK_OBJECT(repeat_week_entry),
                                      GTK_SIGNAL_FUNC(cb_record_changed), NULL);
        gtk_signal_disconnect_by_func(GTK_OBJECT(repeat_mon_entry),
                                      GTK_SIGNAL_FUNC(cb_record_changed), NULL);
        gtk_signal_disconnect_by_func(GTK_OBJECT(repeat_year_entry),
                                      GTK_SIGNAL_FUNC(cb_record_changed), NULL);

        gtk_signal_disconnect_by_func(GTK_OBJECT(check_button_day_endon),
                                      GTK_SIGNAL_FUNC(cb_record_changed), NULL);
        gtk_signal_disconnect_by_func(GTK_OBJECT(check_button_week_endon),
                                      GTK_SIGNAL_FUNC(cb_record_changed), NULL);
        gtk_signal_disconnect_by_func(GTK_OBJECT(check_button_mon_endon),
                                      GTK_SIGNAL_FUNC(cb_record_changed), NULL);
        gtk_signal_disconnect_by_func(GTK_OBJECT(check_button_year_endon),
                                      GTK_SIGNAL_FUNC(cb_record_changed), NULL);

        gtk_signal_disconnect_by_func(GTK_OBJECT(glob_endon_day_button),
                                      GTK_SIGNAL_FUNC(cb_record_changed), NULL);
        gtk_signal_disconnect_by_func(GTK_OBJECT(glob_endon_week_button),
                                      GTK_SIGNAL_FUNC(cb_record_changed), NULL);
        gtk_signal_disconnect_by_func(GTK_OBJECT(glob_endon_mon_button),
                                      GTK_SIGNAL_FUNC(cb_record_changed), NULL);
        gtk_signal_disconnect_by_func(GTK_OBJECT(glob_endon_year_button),
                                      GTK_SIGNAL_FUNC(cb_record_changed), NULL);

        gtk_signal_disconnect_by_func(GTK_OBJECT(toggle_button_repeat_mon_byday),
                                      GTK_SIGNAL_FUNC(cb_record_changed), NULL);
        gtk_signal_disconnect_by_func(GTK_OBJECT(toggle_button_repeat_mon_bydate),
                                      GTK_SIGNAL_FUNC(cb_record_changed), NULL);
        gtk_signal_disconnect_by_func(GTK_OBJECT(private_checkbox),
                                      GTK_SIGNAL_FUNC(cb_record_changed), NULL);

        for (i = 0; i < 7; i++) {
            gtk_signal_disconnect_by_func(GTK_OBJECT(toggle_button_repeat_days[i]),
                                          GTK_SIGNAL_FUNC(cb_record_changed), NULL);
        }
    }
}

static GtkWidget *create_time_menu(int flags) {
    GtkWidget *option;
    GtkWidget *menu;
    GtkWidget *item;
    char str[64];
    char buf[64];
    int i, i_stop;
    int cb_factor;
    struct tm t;

    option = gtk_option_menu_new();
    menu = gtk_menu_new();

    memset(&t, 0, sizeof(t));

    /* Hours menu */
    if (HOURS_FLAG & flags) {
        i_stop = 24;
        cb_factor = 1;
        get_pref_hour_ampm(str);
    } else {
        i_stop = 12;
        cb_factor = 5;
    }
    for (i = 0; i < i_stop; i++) {
        if (HOURS_FLAG & flags) {
            t.tm_hour = i;
            jp_strftime(buf, sizeof(buf), str, &t);
        } else {
            snprintf(buf, sizeof(buf), "%02d", i * cb_factor);
        }
        item = gtk_menu_item_new_with_label(buf);
        gtk_signal_connect(GTK_OBJECT(item), "select",
                           GTK_SIGNAL_FUNC(cb_menu_time),
                           GINT_TO_POINTER(i * cb_factor | flags));
        gtk_menu_append(GTK_MENU(menu), item);
    }

    gtk_option_menu_set_menu(GTK_OPTION_MENU(option), menu);
    gtk_signal_connect(GTK_OBJECT(menu), "hide",
                       GTK_SIGNAL_FUNC(cb_hide_menu_time), NULL);

    return option;
}

static void cb_todos_show(GtkWidget *widget, gpointer data) {
    long ivalue;

    set_pref(PREF_DATEBOOK_TODO_SHOW, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(show_todos_button)), NULL, TRUE);

    if (!gtk_toggle_button_get_active((GTK_TOGGLE_BUTTON(show_todos_button)))) {
        set_pref(PREF_DATEBOOK_TODO_PANE, gtk_paned_get_position(GTK_PANED(todo_pane)), NULL, TRUE);
    }
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
        get_pref(PREF_DATEBOOK_TODO_PANE, &ivalue, NULL);
        gtk_paned_set_position(GTK_PANED(todo_pane), (gint) ivalue);
        gtk_widget_show_all(GTK_WIDGET(todo_vbox));
    } else {
        gtk_widget_hide_all(GTK_WIDGET(todo_vbox));
    }
}

/*** End ToDo code ***/

#ifdef DAY_VIEW
static void cb_resize(GtkWidget *widget, gpointer data)
{
   printf("resize\n");
}
static gint cb_datebook_idle(gpointer data)
{
   update_daily_view_undo(NULL);
   gtk_signal_connect(GTK_OBJECT(scrolled_window), "configure_event",
                      GTK_SIGNAL_FUNC(cb_resize), NULL);
   return FALSE; /* Cause this function not to be called again */
}
#endif

int datebook_gui(GtkWidget *vbox, GtkWidget *hbox) {
    GtkWidget *button;
    GtkWidget *separator;
    GtkWidget *label;
    GtkWidget *table;
    GtkWidget *vbox1, *vbox2;
    GtkWidget *hbox_temp;
    GtkWidget *vbox_temp;
#ifdef DAY_VIEW
                                                                                                                            GtkWidget *vbox_no_time_appts;
   GtkWidget *scrolled_window2;
#endif
    GtkWidget *vbox_repeat_day;
    GtkWidget *hbox_repeat_day1;
    GtkWidget *hbox_repeat_day2;
    GtkWidget *vbox_repeat_week;
    GtkWidget *hbox_repeat_week1;
    GtkWidget *hbox_repeat_week2;
    GtkWidget *hbox_repeat_week3;
    GtkWidget *vbox_repeat_mon;
    GtkWidget *hbox_repeat_mon1;
    GtkWidget *hbox_repeat_mon2;
    GtkWidget *hbox_repeat_mon3;
    GtkWidget *vbox_repeat_year;
    GtkWidget *hbox_repeat_year1;
    GtkWidget *hbox_repeat_year2;
    GtkWidget *notebook_tab1;
    GtkWidget *notebook_tab2;
    GtkWidget *notebook_tab3;
    GtkWidget *notebook_tab4;
    GtkWidget *notebook_tab5;

    GSList *group;
    char *titles[] = {"", "", "", "", ""};

    long fdow;
    long ivalue;
    long char_set;
    long show_tooltips;
    char *cat_name;
#ifdef ENABLE_DATEBK
    long use_db3_tags;
#endif

    int i, j;
    char days2[12];
    char *days[] = {
            N_("Su"),
            N_("Mo"),
            N_("Tu"),
            N_("We"),
            N_("Th"),
            N_("Fr"),
            N_("Sa"),
            N_("Su")
    };

    get_pref(PREF_DATEBOOK_VERSION, &datebook_version, NULL);

    init();

#ifdef ENABLE_DATEBK
    datebk_entry = NULL;
    get_pref(PREF_USE_DB3, &use_db3_tags, NULL);
#endif

    get_calendar_or_datebook_app_info(&dbook_app_info, datebook_version);

    if (datebook_version) {
        /* Initialize categories */
        get_pref(PREF_CHAR_SET, &char_set, NULL);
        for (i = 1; i < NUM_DATEBOOK_CAT_ITEMS; i++) {
            cat_name = charset_p2newj(dbook_app_info.category.name[i], 31, (int) char_set);
            strcpy(sort_l[i - 1].Pcat, cat_name);
            free(cat_name);
            sort_l[i - 1].cat_num = i;
        }
        /* put reserved 'Unfiled' category at end of list */
        cat_name = charset_p2newj(dbook_app_info.category.name[0], 31, (int) char_set);
        strcpy(sort_l[NUM_DATEBOOK_CAT_ITEMS - 1].Pcat, cat_name);
        free(cat_name);
        sort_l[NUM_DATEBOOK_CAT_ITEMS - 1].cat_num = 0;

        qsort(sort_l, NUM_DATEBOOK_CAT_ITEMS - 1, sizeof(struct sorted_cats), cat_compare);

#ifdef JPILOT_DEBUG
                                                                                                                                for (i=0; i<NUM_DATEBOOK_CAT_ITEMS; i++) {
         printf("cat %d [%s]\n", sort_l[i].cat_num, sort_l[i].Pcat);
      }
#endif

        get_pref(PREF_LAST_DATE_CATEGORY, &ivalue, NULL);
        dbook_category = (int) ivalue;

        if ((dbook_category != CATEGORY_ALL)
            && (dbook_app_info.category.name[dbook_category][0] == '\0')) {
            dbook_category = CATEGORY_ALL;
        }
    }   /* End category code for Calendar app */

    /* Create basic GUI with left and right boxes and sliding pane */
    accel_group = gtk_accel_group_new();
    gtk_window_add_accel_group(GTK_WINDOW(gtk_widget_get_toplevel(vbox)),
                               accel_group);
    get_pref(PREF_SHOW_TOOLTIPS, &show_tooltips, NULL);

    pane = gtk_hpaned_new();
    todo_pane = gtk_vpaned_new();
    get_pref(PREF_DATEBOOK_PANE, &ivalue, NULL);
    gtk_paned_set_position(GTK_PANED(pane), (gint) ivalue);

    get_pref(PREF_DATEBOOK_TODO_PANE, &ivalue, NULL);
    gtk_paned_set_position(GTK_PANED(todo_pane), (gint) ivalue);

    gtk_box_pack_start(GTK_BOX(hbox), pane, TRUE, TRUE, 5);

    vbox1 = gtk_vbox_new(FALSE, 0);
    vbox2 = gtk_vbox_new(FALSE, 0);
    todo_vbox = gtk_vbox_new(FALSE, 0);

    gtk_paned_pack1(GTK_PANED(todo_pane), vbox1, TRUE, FALSE);
    gtk_paned_pack2(GTK_PANED(todo_pane), todo_vbox, TRUE, FALSE);

    gtk_paned_pack1(GTK_PANED(pane), todo_pane, TRUE, FALSE);
    gtk_paned_pack2(GTK_PANED(pane), vbox2, TRUE, FALSE);

    /* Left side of GUI */

    /* Separator */
    separator = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(vbox1), separator, FALSE, FALSE, 5);

    /* Make the 'Today is:' label */
    glob_date_label = gtk_label_new(" ");
    gtk_box_pack_start(GTK_BOX(vbox1), glob_date_label, FALSE, FALSE, 0);
    timeout_date(NULL);
    glob_date_timer_tag = gtk_timeout_add(CLOCK_TICK, timeout_sync_up, NULL);

    /* Separator */
    separator = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(vbox1), separator, FALSE, FALSE, 5);

    if (datebook_version) {
        /* Calendar supports categories */
        /* Left-side Category menu */
        hbox_temp = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox1), hbox_temp, FALSE, FALSE, 0);

        make_category_menu_box(&category_menu1,
                               sort_l, cb_category, TRUE, TRUE);
        gtk_box_pack_start(GTK_BOX(hbox_temp), category_menu1, TRUE, TRUE, 0);
    }

    /* Make the main calendar */
    hbox_temp = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox1), hbox_temp, FALSE, FALSE, 0);

    get_pref(PREF_FDOW, &fdow, NULL);
    if (fdow == 1) {
        fdow = 1;
    } else {
        fdow = 0;
    }
    main_calendar = gtk_calendar_new();
    gtk_calendar_set_display_options(GTK_CALENDAR(main_calendar), GTK_CALENDAR_SHOW_HEADING |
                                                                  GTK_CALENDAR_SHOW_DAY_NAMES |
                                                                  GTK_CALENDAR_SHOW_WEEK_NUMBERS);

    // This way produces a small calendar on the left
    gtk_box_pack_start(GTK_BOX(hbox_temp), main_calendar, FALSE, FALSE, 0);
    // This way produces a centered, small calendar
    // gtk_box_pack_start(GTK_BOX(hbox_temp), main_calendar, TRUE, FALSE, 0);

    gtk_signal_connect(GTK_OBJECT(main_calendar),
                       "day_selected", GTK_SIGNAL_FUNC(cb_cal_changed),
                       GINT_TO_POINTER(CAL_DAY_SELECTED));

    /* Weekview button */
    button = gtk_button_new_with_label(_("Week"));
    gtk_signal_connect(GTK_OBJECT(button), "clicked",
                       GTK_SIGNAL_FUNC(cb_weekview), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 3);
    gtk_widget_show(button);

    /* Accelerator key for starting Weekview GUI */
    gtk_widget_add_accelerator(GTK_WIDGET(button), "clicked", accel_group, GDK_KEY_w,
                               GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    set_tooltip((int) show_tooltips, button, _("View appointments by week   Ctrl+W"));

    /* Monthview button */
    button = gtk_button_new_with_label(_("Month"));
    gtk_signal_connect(GTK_OBJECT(button), "clicked",
                       GTK_SIGNAL_FUNC(cb_monthview), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 3);
    gtk_widget_show(button);

    /* Accelerator key for starting Monthview GUI */
    gtk_widget_add_accelerator(GTK_WIDGET(button), "clicked", accel_group, GDK_KEY_m,
                               GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    set_tooltip((int) show_tooltips, button, _("View appointments by month   Ctrl+M"));

#ifdef ENABLE_DATEBK
    if (use_db3_tags) {
        /* Make Category button */
        button = gtk_button_new_with_label(_("Cats"));
        gtk_signal_connect(GTK_OBJECT(button), "clicked",
                           GTK_SIGNAL_FUNC(cb_datebk_cats), NULL);
        gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 3);
        gtk_widget_show(button);
    }
#endif

    /* DOW label */
    dow_label = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(vbox1), dow_label, FALSE, FALSE, 0);

#ifdef DAY_VIEW
                                                                                                                            /* Appointments list scrolled window. */
   scrolled_window2 = gtk_scrolled_window_new(NULL, NULL);
   gtk_container_set_border_width(GTK_CONTAINER(scrolled_window2), 0);
   gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window2),
                                  GTK_POLICY_NEVER, GTK_POLICY_NEVER);
   /* "No Time" appointment box */
   vbox_no_time_appts = gtk_vbox_new(FALSE, 0);

   gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrolled_window2),
                                         GTK_WIDGET(vbox_no_time_appts));
   gtk_box_pack_start(GTK_BOX(vbox1), scrolled_window2, FALSE, FALSE, 0);
#endif

    /* Appointments list scrolled window. */
    scrolled_window = gtk_scrolled_window_new(NULL, NULL);

    gtk_container_set_border_width(GTK_CONTAINER(scrolled_window), 0);

    /* the policy is one of GTK_POLICY AUTOMATIC, or GTK_POLICY_ALWAYS.
    * GTK_POLICY_AUTOMATIC will automatically decide whether you need
    * scrollbars, whereas GTK_POLICY_ALWAYS will always leave the scrollbars
    * there.  The first one is the horizontal scrollbar, the second,
    * the vertical. */
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    gtk_box_pack_start(GTK_BOX(vbox1), scrolled_window, TRUE, TRUE, 0);
    listStore = gtk_list_store_new(DATE_NUM_COLS, G_TYPE_STRING, GDK_TYPE_PIXBUF, GDK_TYPE_PIXBUF, GDK_TYPE_PIXBUF,
                                   G_TYPE_STRING, G_TYPE_POINTER, GDK_TYPE_COLOR, G_TYPE_BOOLEAN, G_TYPE_STRING,
                                   G_TYPE_BOOLEAN);

    GtkTreeModel *model = GTK_TREE_MODEL(listStore);
    treeView = GTK_TREE_VIEW(gtk_tree_view_new_with_model(model));
    buildTreeView(vbox, titles, use_db3_tags);


#ifdef DAY_VIEW
                                                                                                                            create_daily_view(scrolled_window, vbox_no_time_appts);
   gtk_idle_add(cb_datebook_idle, NULL);
#else
    gtk_container_add(GTK_CONTAINER(scrolled_window), GTK_WIDGET(treeView));

#endif

    /* "Show ToDos" button */
    show_todos_button = gtk_check_button_new_with_label(_("Show ToDos"));
    gtk_box_pack_start(GTK_BOX(vbox1), show_todos_button, FALSE, FALSE, 0);
    gtk_signal_connect(GTK_OBJECT(show_todos_button), "clicked",
                       GTK_SIGNAL_FUNC(cb_todos_show), NULL);

    /* ToDo  */
    buildToDoList(vbox);
    /* End ToDo code */

    /* Right side of GUI */

    hbox_temp = gtk_hbox_new(FALSE, 3);
    gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, FALSE, FALSE, 0);

    /* Add record modification buttons */
    /* Cancel button */
    CREATE_BUTTON(cancel_record_button, _("Cancel"), CANCEL, _("Cancel the modifications"), GDK_KEY_Escape, 0, "ESC")
    gtk_signal_connect(GTK_OBJECT(cancel_record_button), "clicked",
                       GTK_SIGNAL_FUNC(cb_cancel), NULL);

    /* Delete button */
    CREATE_BUTTON(delete_record_button, _("Delete"), DELETE, _("Delete the selected record"), GDK_KEY_d,
                  GDK_CONTROL_MASK,
                  "Ctrl+D")
    gtk_signal_connect(GTK_OBJECT(delete_record_button), "clicked",
                       GTK_SIGNAL_FUNC(cb_delete_appt),
                       GINT_TO_POINTER(DELETE_FLAG));

    /* Undelete button */
    CREATE_BUTTON(undelete_record_button, _("Undelete"), UNDELETE, _("Undelete the selected record"), 0, 0, "")
    gtk_signal_connect(GTK_OBJECT(undelete_record_button), "clicked",
                       GTK_SIGNAL_FUNC(cb_undelete_appt),
                       GINT_TO_POINTER(UNDELETE_FLAG));

    /* Copy button */
    CREATE_BUTTON(copy_record_button, _("Copy"), COPY, _("Copy the selected record"), GDK_KEY_c,
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
    gtk_widget_set_name(GTK_WIDGET(GTK_LABEL(gtk_bin_get_child(GTK_BIN(add_record_button)))),
              "label_high");
#endif

    /* "Apply Changes" button */
    CREATE_BUTTON(apply_record_button, _("Apply Changes"), APPLY, _("Commit the modifications"), GDK_Return,
                  GDK_CONTROL_MASK, "Ctrl+Enter")
    gtk_signal_connect(GTK_OBJECT(apply_record_button), "clicked",
                       GTK_SIGNAL_FUNC(cb_add_new_record),
                       GINT_TO_POINTER(MODIFY_FLAG));
#ifndef ENABLE_STOCK_BUTTONS
    gtk_widget_set_name(GTK_WIDGET(GTK_LABEL(gtk_bin_get_child(GTK_BIN(apply_record_button)))),
      "label_high");
#endif

    /* Separator */
    separator = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(vbox2), separator, FALSE, FALSE, 5);

    if (datebook_version) {
        /* Calendar supports categories */
        /* Right-side Category menu */
        hbox_temp = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, FALSE, FALSE, 0);

        /* Clear GTK option menus before use */
        if (category_menu2 != NULL) {
            GtkTreeModel *clearingmodel = gtk_combo_box_get_model(GTK_COMBO_BOX(category_menu2));
            gtk_list_store_clear(GTK_LIST_STORE(clearingmodel));
        }
        make_category_menu_box(&category_menu2,
                               sort_l, NULL, FALSE, FALSE);
        gtk_box_pack_start(GTK_BOX(hbox_temp), category_menu2, TRUE, TRUE, 0);

        /* Private check box */
        private_checkbox = gtk_check_button_new_with_label(_("Private"));
        gtk_box_pack_end(GTK_BOX(hbox_temp), private_checkbox, FALSE, FALSE, 0);
    }  /* end datebook_version check for category support */

    /* Datebook has alarm checkbox on top */
    if (datebook_version == 0) {
        /* Alarm checkbox */
        hbox_alarm1 = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox2), hbox_alarm1, FALSE, FALSE, 0);

        check_button_alarm = gtk_check_button_new_with_label(_("Alarm"));
        gtk_box_pack_start(GTK_BOX(hbox_alarm1), check_button_alarm, FALSE, FALSE, 2);
        gtk_signal_connect(GTK_OBJECT(check_button_alarm), "clicked",
                           GTK_SIGNAL_FUNC(cb_check_button_alarm), NULL);

        hbox_alarm2 = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox_alarm1), hbox_alarm2, FALSE, FALSE, 0);

        /* Units entry for alarm */
        units_entry = gtk_entry_new_with_max_length(2);
        entry_set_multiline_truncate(GTK_ENTRY(units_entry), TRUE);
        gtk_widget_set_usize(units_entry, 30, 0);
        gtk_box_pack_start(GTK_BOX(hbox_alarm2), units_entry, FALSE, FALSE, 0);

        radio_button_alarm_min = gtk_radio_button_new_with_label(NULL, _("Minutes"));

        group = gtk_radio_button_group(GTK_RADIO_BUTTON(radio_button_alarm_min));
        radio_button_alarm_hour = gtk_radio_button_new_with_label(group, _("Hours"));
        group = gtk_radio_button_group(GTK_RADIO_BUTTON(radio_button_alarm_hour));
        radio_button_alarm_day = gtk_radio_button_new_with_label(group, _("Days"));

        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_button_alarm_min), TRUE);

        gtk_box_pack_start(GTK_BOX(hbox_alarm2),
                           radio_button_alarm_min, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox_alarm2),
                           radio_button_alarm_hour, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox_alarm2),
                           radio_button_alarm_day, FALSE, FALSE, 0);

        /* Private check box */
        private_checkbox = gtk_check_button_new_with_label(_("Private"));
        gtk_box_pack_end(GTK_BOX(hbox_alarm1), private_checkbox, FALSE, FALSE, 0);
    }  /* end of Alarm & Private check boxes for Datebook */

    /* Start date button */
    hbox_temp = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, FALSE, FALSE, 0);

    label = gtk_label_new(_("Date:"));
    gtk_box_pack_start(GTK_BOX(hbox_temp), label, FALSE, FALSE, 4);

    begin_date_button = gtk_button_new_with_label("");
    gtk_box_pack_start(GTK_BOX(hbox_temp), begin_date_button, FALSE, FALSE, 1);
    gtk_signal_connect(GTK_OBJECT(begin_date_button), "clicked",
                       GTK_SIGNAL_FUNC(cb_cal_dialog),
                       GINT_TO_POINTER(BEGIN_DATE_BUTTON));

    /* Appointment time selection */
    vbox_temp = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox2), vbox_temp, FALSE, FALSE, 0);

    hbox_temp = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox_temp), hbox_temp, TRUE, TRUE, 0);

    /* "No Time" radio button */
    radio_button_no_time = gtk_radio_button_new(NULL);
    gtk_button_set_alignment(GTK_BUTTON(radio_button_no_time), 1.0, 0.0);
    gtk_box_pack_start(GTK_BOX(hbox_temp), radio_button_no_time, FALSE, FALSE, 0);
    gtk_signal_connect(GTK_OBJECT(radio_button_no_time), "clicked",
                       GTK_SIGNAL_FUNC(cb_radio_button_no_time), NULL);

    label = gtk_label_new(_("No Time"));
    gtk_box_pack_start(GTK_BOX(hbox_temp), label, FALSE, FALSE, 0);

    /* Appt Time radio button and entry widgets for selecting time */
    hbox_temp = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox_temp), hbox_temp, FALSE, FALSE, 0);

    radio_button_appt_time = gtk_radio_button_new_from_widget(GTK_RADIO_BUTTON(radio_button_no_time));
    gtk_box_pack_start(GTK_BOX(hbox_temp), radio_button_appt_time, FALSE, FALSE, 0);
    /* Currently no need to do anything with appt_time radio button
   gtk_signal_connect(GTK_OBJECT(radio_button_appt_time), "clicked",
                      GTK_SIGNAL_FUNC(cb_radio_button_appt_time), NULL); */

    table = gtk_table_new(2, 4, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 0);
    gtk_table_set_col_spacings(GTK_TABLE(table), 0);
    gtk_box_pack_start(GTK_BOX(hbox_temp), table, FALSE, FALSE, 0);

    /* Start date and time */
    label = gtk_label_new(_("Start"));
    gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(label), 0, 1, 0, 1,
                     GTK_SHRINK, GTK_SHRINK, 0, 0);

    begin_time_entry = gtk_entry_new_with_max_length(7);
    gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(begin_time_entry),
                     1, 2, 0, 1, GTK_SHRINK, GTK_SHRINK, 0, 0);

    option1 = create_time_menu(START_TIME_FLAG | HOURS_FLAG);
    gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(option1),
                     2, 3, 0, 1, GTK_SHRINK, GTK_FILL, 0, 0);

    option2 = create_time_menu(START_TIME_FLAG);
    gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(option2),
                     3, 4, 0, 1, GTK_SHRINK, GTK_FILL, 0, 0);

    /* End date and time */
    label = gtk_label_new(_("End"));
    gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(label),
                     0, 1, 1, 2, GTK_SHRINK, GTK_SHRINK, 0, 0);

    end_time_entry = gtk_entry_new_with_max_length(7);
    gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(end_time_entry),
                     1, 2, 1, 2, GTK_SHRINK, GTK_SHRINK, 0, 0);

    gtk_widget_set_usize(GTK_WIDGET(begin_time_entry), 70, 0);
    gtk_widget_set_usize(GTK_WIDGET(end_time_entry), 70, 0);

    option3 = create_time_menu(END_TIME_FLAG | HOURS_FLAG);
    gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(option3),
                     2, 3, 1, 2, GTK_SHRINK, GTK_FILL, 0, 0);

    option4 = create_time_menu(END_TIME_FLAG);
    gtk_table_attach(GTK_TABLE(table), GTK_WIDGET(option4),
                     3, 4, 1, 2, GTK_SHRINK, GTK_FILL, 0, 0);

    /* Need to connect these signals after the menus are created to avoid errors */
    gtk_signal_connect(GTK_OBJECT(begin_time_entry), "key_press_event",
                       GTK_SIGNAL_FUNC(cb_entry_key_pressed), NULL);
    gtk_signal_connect(GTK_OBJECT(end_time_entry), "key_press_event",
                       GTK_SIGNAL_FUNC(cb_entry_key_pressed), NULL);
    gtk_signal_connect(GTK_OBJECT(begin_time_entry), "button_press_event",
                       GTK_SIGNAL_FUNC(cb_entry_pressed), GINT_TO_POINTER(1));
    gtk_signal_connect(GTK_OBJECT(end_time_entry), "button_press_event",
                       GTK_SIGNAL_FUNC(cb_entry_pressed), GINT_TO_POINTER(2));

    gtk_signal_connect(GTK_OBJECT(begin_date_button), "key_press_event",
                       GTK_SIGNAL_FUNC(cb_key_pressed_tab),
                       radio_button_no_time);
    gtk_signal_connect(GTK_OBJECT(radio_button_no_time), "key_press_event",
                       GTK_SIGNAL_FUNC(cb_key_pressed_tab),
                       begin_time_entry);

    clear_begin_end_labels();

    /* Location entry for Calendar app */
    if (datebook_version) {
        hbox_temp = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, FALSE, FALSE, 0);

        label = gtk_label_new(_("Location:"));
        gtk_box_pack_start(GTK_BOX(hbox_temp), label, FALSE, FALSE, 4);

        location_entry = gtk_entry_new();
        entry_set_multiline_truncate(GTK_ENTRY(location_entry), TRUE);
        gtk_entry_set_width_chars(GTK_ENTRY(location_entry), 21);
        gtk_box_pack_start(GTK_BOX(hbox_temp), location_entry, FALSE, FALSE, 0);
    }

    /* Calendar application has alarm checkbox on bottom */
    if (datebook_version) {
        /* Alarm checkbox */
        hbox_alarm1 = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox2), hbox_alarm1, FALSE, FALSE, 0);

        check_button_alarm = gtk_check_button_new_with_label(_("Alarm"));
        gtk_box_pack_start(GTK_BOX(hbox_alarm1), check_button_alarm, FALSE, FALSE, 2);
        gtk_signal_connect(GTK_OBJECT(check_button_alarm), "clicked",
                           GTK_SIGNAL_FUNC(cb_check_button_alarm), NULL);

        hbox_alarm2 = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox_alarm1), hbox_alarm2, FALSE, FALSE, 0);

        /* Units entry for alarm */
        units_entry = gtk_entry_new_with_max_length(2);
        entry_set_multiline_truncate(GTK_ENTRY(units_entry), TRUE);
        gtk_widget_set_usize(units_entry, 30, 0);
        gtk_box_pack_start(GTK_BOX(hbox_alarm2), units_entry, FALSE, FALSE, 0);

        radio_button_alarm_min = gtk_radio_button_new_with_label(NULL, _("Minutes"));

        group = gtk_radio_button_group(GTK_RADIO_BUTTON(radio_button_alarm_min));
        radio_button_alarm_hour = gtk_radio_button_new_with_label(group, _("Hours"));
        group = gtk_radio_button_group(GTK_RADIO_BUTTON(radio_button_alarm_hour));
        radio_button_alarm_day = gtk_radio_button_new_with_label(group, _("Days"));

        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_button_alarm_min), TRUE);

        gtk_box_pack_start(GTK_BOX(hbox_alarm2),
                           radio_button_alarm_min, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox_alarm2),
                           radio_button_alarm_hour, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(hbox_alarm2),
                           radio_button_alarm_day, FALSE, FALSE, 0);
    }  /* End of Alarm checkbox for Calendar */

    note_pane = gtk_vpaned_new();
    get_pref(PREF_DATEBOOK_NOTE_PANE, &ivalue, NULL);
    gtk_paned_set_position(GTK_PANED(note_pane), (gint) ivalue);
    gtk_box_pack_start(GTK_BOX(vbox2), note_pane, TRUE, TRUE, 5);

    /* Event Description text box */
    hbox_temp = gtk_hbox_new(FALSE, 0);
    gtk_paned_pack1(GTK_PANED(note_pane), hbox_temp, TRUE, FALSE);

    dbook_desc = gtk_text_view_new();
    dbook_desc_buffer = G_OBJECT(gtk_text_view_get_buffer(GTK_TEXT_VIEW(dbook_desc)));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(dbook_desc), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(dbook_desc), GTK_WRAP_WORD);

    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_set_border_width(GTK_CONTAINER(scrolled_window), 1);
    gtk_container_add(GTK_CONTAINER(scrolled_window), dbook_desc);
    gtk_box_pack_start_defaults(GTK_BOX(hbox_temp), scrolled_window);

    /* Note text box */
    hbox_temp = gtk_hbox_new(FALSE, 0);
    gtk_paned_pack2(GTK_PANED(note_pane), hbox_temp, TRUE, FALSE);

    dbook_note = gtk_text_view_new();
    dbook_note_buffer = G_OBJECT(gtk_text_view_get_buffer(GTK_TEXT_VIEW(dbook_note)));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(dbook_note), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(dbook_note), GTK_WRAP_WORD);

    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_set_border_width(GTK_CONTAINER(scrolled_window), 1);
    gtk_container_add(GTK_CONTAINER(scrolled_window), dbook_note);
    gtk_box_pack_start_defaults(GTK_BOX(hbox_temp), scrolled_window);

    /* Datebk tags entry */
#ifdef ENABLE_DATEBK
    if (use_db3_tags) {
        hbox_temp = gtk_hbox_new(FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, FALSE, FALSE, 2);

        label = gtk_label_new(_("DateBk Tags"));
        gtk_box_pack_start(GTK_BOX(hbox_temp), label, FALSE, FALSE, 5);

        datebk_entry = gtk_entry_new_with_max_length(30);
        gtk_box_pack_start(GTK_BOX(hbox_temp), datebk_entry, TRUE, TRUE, 0);
    }
#endif

    /* Notebook for event repeat types */
    hbox_temp = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, FALSE, FALSE, 2);

    notebook = gtk_notebook_new();
    gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_TOP);
    gtk_box_pack_start(GTK_BOX(hbox_temp), notebook, FALSE, FALSE, 5);
    /* Labels for notebook tabs */
    notebook_tab1 = gtk_label_new(_("None"));
    notebook_tab2 = gtk_label_new(_("Day"));
    notebook_tab3 = gtk_label_new(_("Week"));
    notebook_tab4 = gtk_label_new(_("Month"));
    notebook_tab5 = gtk_label_new(_("Year"));

    /* "No Repeat" page for notebook */
    label = gtk_label_new(_("This event will not repeat"));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), label, notebook_tab1);

    /* "Day Repeat" page for notebook */
    vbox_repeat_day = gtk_vbox_new(FALSE, 0);
    hbox_repeat_day1 = gtk_hbox_new(FALSE, 0);
    hbox_repeat_day2 = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox_repeat_day), hbox_repeat_day1, FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(vbox_repeat_day), hbox_repeat_day2, FALSE, FALSE, 2);
    label = gtk_label_new(_("Frequency is Every"));
    gtk_box_pack_start(GTK_BOX(hbox_repeat_day1), label, FALSE, FALSE, 2);
    repeat_day_entry = gtk_entry_new_with_max_length(2);
    entry_set_multiline_truncate(GTK_ENTRY(repeat_day_entry), TRUE);
    gtk_widget_set_usize(repeat_day_entry, 30, 0);
    gtk_box_pack_start(GTK_BOX(hbox_repeat_day1), repeat_day_entry, FALSE, FALSE, 0);
    label = gtk_label_new(_("Day(s)"));
    gtk_box_pack_start(GTK_BOX(hbox_repeat_day1), label, FALSE, FALSE, 2);

    check_button_day_endon = gtk_check_button_new_with_label(_("End on"));
    gtk_signal_connect(GTK_OBJECT(check_button_day_endon), "clicked",
                       GTK_SIGNAL_FUNC(cb_check_button_endon),
                       GINT_TO_POINTER(PAGE_DAY));
    gtk_box_pack_start(GTK_BOX(hbox_repeat_day2), check_button_day_endon, FALSE, FALSE, 0);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
                             vbox_repeat_day, notebook_tab2);

    glob_endon_day_button = gtk_button_new_with_label(_("No Date"));
    gtk_box_pack_start(GTK_BOX(hbox_repeat_day2),
                       glob_endon_day_button, FALSE, FALSE, 0);
    gtk_signal_connect(GTK_OBJECT(glob_endon_day_button), "clicked",
                       GTK_SIGNAL_FUNC(cb_cal_dialog),
                       GINT_TO_POINTER(PAGE_DAY));

    /* "Week Repeat" page */
    vbox_repeat_week = gtk_vbox_new(FALSE, 0);
    hbox_repeat_week1 = gtk_hbox_new(FALSE, 0);
    hbox_repeat_week2 = gtk_hbox_new(FALSE, 0);
    hbox_repeat_week3 = gtk_hbox_new(FALSE, 0);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox_repeat_week, notebook_tab3);
    gtk_box_pack_start(GTK_BOX(vbox_repeat_week), hbox_repeat_week1, FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(vbox_repeat_week), hbox_repeat_week2, FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(vbox_repeat_week), hbox_repeat_week3, FALSE, FALSE, 2);
    label = gtk_label_new(_("Frequency is Every"));
    gtk_box_pack_start(GTK_BOX(hbox_repeat_week1), label, FALSE, FALSE, 2);
    repeat_week_entry = gtk_entry_new_with_max_length(2);
    entry_set_multiline_truncate(GTK_ENTRY(repeat_week_entry), TRUE);
    gtk_widget_set_usize(repeat_week_entry, 30, 0);
    gtk_box_pack_start(GTK_BOX(hbox_repeat_week1), repeat_week_entry, FALSE, FALSE, 0);
    label = gtk_label_new(_("Week(s)"));
    gtk_box_pack_start(GTK_BOX(hbox_repeat_week1), label, FALSE, FALSE, 2);

    check_button_week_endon = gtk_check_button_new_with_label(_("End on"));
    gtk_signal_connect(GTK_OBJECT(check_button_week_endon), "clicked",
                       GTK_SIGNAL_FUNC(cb_check_button_endon),
                       GINT_TO_POINTER(PAGE_WEEK));
    gtk_box_pack_start(GTK_BOX(hbox_repeat_week2), check_button_week_endon, FALSE, FALSE, 0);

    glob_endon_week_button = gtk_button_new_with_label(_("No Date"));
    gtk_box_pack_start(GTK_BOX(hbox_repeat_week2),
                       glob_endon_week_button, FALSE, FALSE, 0);
    gtk_signal_connect(GTK_OBJECT(glob_endon_week_button), "clicked",
                       GTK_SIGNAL_FUNC(cb_cal_dialog),
                       GINT_TO_POINTER(PAGE_WEEK));

    label = gtk_label_new(_("Repeat on Days:"));
    gtk_box_pack_start(GTK_BOX(hbox_repeat_week3), label, FALSE, FALSE, 0);

    get_pref(PREF_FDOW, &fdow, NULL);

    for (i = 0, j = (int) fdow; i < 7; i++, j++) {
        if (j > 6) {
            j = 0;
        }
        g_strlcpy(days2, _(days[j]), sizeof(days2));
        /* If no translation occurred then use the first letter only */
        if (!strcmp(days2, days[j])) {
            days2[0] = days[j][0];
            days2[1] = '\0';
        }

        toggle_button_repeat_days[j] = gtk_toggle_button_new_with_label(days2);
        gtk_box_pack_start(GTK_BOX(hbox_repeat_week3),
                           toggle_button_repeat_days[j], FALSE, FALSE, 0);
    }

    /* "Month Repeat" page */
    vbox_repeat_mon = gtk_vbox_new(FALSE, 0);
    hbox_repeat_mon1 = gtk_hbox_new(FALSE, 0);
    hbox_repeat_mon2 = gtk_hbox_new(FALSE, 0);
    hbox_repeat_mon3 = gtk_hbox_new(FALSE, 0);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox_repeat_mon, notebook_tab4);
    gtk_box_pack_start(GTK_BOX(vbox_repeat_mon), hbox_repeat_mon1, FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(vbox_repeat_mon), hbox_repeat_mon2, FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(vbox_repeat_mon), hbox_repeat_mon3, FALSE, FALSE, 2);
    label = gtk_label_new(_("Frequency is Every"));
    gtk_box_pack_start(GTK_BOX(hbox_repeat_mon1), label, FALSE, FALSE, 2);
    repeat_mon_entry = gtk_entry_new_with_max_length(2);
    entry_set_multiline_truncate(GTK_ENTRY(repeat_mon_entry), TRUE);
    gtk_widget_set_usize(repeat_mon_entry, 30, 0);
    gtk_box_pack_start(GTK_BOX(hbox_repeat_mon1), repeat_mon_entry, FALSE, FALSE, 0);
    label = gtk_label_new(_("Month(s)"));
    gtk_box_pack_start(GTK_BOX(hbox_repeat_mon1), label, FALSE, FALSE, 2);

    check_button_mon_endon = gtk_check_button_new_with_label(_("End on"));
    gtk_signal_connect(GTK_OBJECT(check_button_mon_endon), "clicked",
                       GTK_SIGNAL_FUNC(cb_check_button_endon),
                       GINT_TO_POINTER(PAGE_MONTH));
    gtk_box_pack_start(GTK_BOX(hbox_repeat_mon2), check_button_mon_endon, FALSE, FALSE, 0);

    glob_endon_mon_button = gtk_button_new_with_label(_("No Date"));
    gtk_box_pack_start(GTK_BOX(hbox_repeat_mon2),
                       glob_endon_mon_button, FALSE, FALSE, 0);
    gtk_signal_connect(GTK_OBJECT(glob_endon_mon_button), "clicked",
                       GTK_SIGNAL_FUNC(cb_cal_dialog),
                       GINT_TO_POINTER(PAGE_MONTH));


    label = gtk_label_new(_("Repeat by:"));
    gtk_box_pack_start(GTK_BOX(hbox_repeat_mon3), label, FALSE, FALSE, 0);

    toggle_button_repeat_mon_byday = gtk_radio_button_new_with_label
            (NULL, _("Day of week"));

    gtk_box_pack_start(GTK_BOX(hbox_repeat_mon3),
                       toggle_button_repeat_mon_byday, FALSE, FALSE, 0);

    group = gtk_radio_button_group(GTK_RADIO_BUTTON(toggle_button_repeat_mon_byday));
    toggle_button_repeat_mon_bydate = gtk_radio_button_new_with_label
            (group, _("Date"));
    gtk_box_pack_start(GTK_BOX(hbox_repeat_mon3),
                       toggle_button_repeat_mon_bydate, FALSE, FALSE, 0);

    /* "Year Repeat" page */
    vbox_repeat_year = gtk_vbox_new(FALSE, 0);
    hbox_repeat_year1 = gtk_hbox_new(FALSE, 0);
    hbox_repeat_year2 = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox_repeat_year), hbox_repeat_year1, FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(vbox_repeat_year), hbox_repeat_year2, FALSE, FALSE, 2);
    label = gtk_label_new(_("Frequency is Every"));
    gtk_box_pack_start(GTK_BOX(hbox_repeat_year1), label, FALSE, FALSE, 2);
    repeat_year_entry = gtk_entry_new_with_max_length(2);
    entry_set_multiline_truncate(GTK_ENTRY(repeat_year_entry), TRUE);
    gtk_widget_set_usize(repeat_year_entry, 30, 0);
    gtk_box_pack_start(GTK_BOX(hbox_repeat_year1), repeat_year_entry, FALSE, FALSE, 0);
    label = gtk_label_new(_("Year(s)"));
    gtk_box_pack_start(GTK_BOX(hbox_repeat_year1), label, FALSE, FALSE, 2);

    check_button_year_endon = gtk_check_button_new_with_label(_("End on"));
    gtk_signal_connect(GTK_OBJECT(check_button_year_endon), "clicked",
                       GTK_SIGNAL_FUNC(cb_check_button_endon),
                       GINT_TO_POINTER(PAGE_YEAR));
    gtk_box_pack_start(GTK_BOX(hbox_repeat_year2), check_button_year_endon, FALSE, FALSE, 0);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
                             vbox_repeat_year, notebook_tab5);

    glob_endon_year_button = gtk_button_new_with_label(_("No Date"));
    gtk_box_pack_start(GTK_BOX(hbox_repeat_year2),
                       glob_endon_year_button, FALSE, FALSE, 0);
    gtk_signal_connect(GTK_OBJECT(glob_endon_year_button), "clicked",
                       GTK_SIGNAL_FUNC(cb_cal_dialog),
                       GINT_TO_POINTER(PAGE_YEAR));

    /* Set default values for repeats */
    gtk_entry_set_text(GTK_ENTRY(units_entry), "5");
    gtk_entry_set_text(GTK_ENTRY(repeat_day_entry), "1");
    gtk_entry_set_text(GTK_ENTRY(repeat_week_entry), "1");
    gtk_entry_set_text(GTK_ENTRY(repeat_mon_entry), "1");
    gtk_entry_set_text(GTK_ENTRY(repeat_year_entry), "1");

    gtk_notebook_set_page(GTK_NOTEBOOK(notebook), 0);
    gtk_notebook_popup_enable(GTK_NOTEBOOK(notebook));

    /* end notebook details */

    /* Capture the TAB key in text fields */
    gtk_signal_connect(GTK_OBJECT(dbook_desc), "key_press_event",
                       GTK_SIGNAL_FUNC(cb_key_pressed_tab_entry), dbook_note);
    gtk_signal_connect(GTK_OBJECT(dbook_desc), "key_press_event",
                       GTK_SIGNAL_FUNC(cb_key_pressed_shift_tab), end_time_entry);

    gtk_signal_connect(GTK_OBJECT(dbook_note), "key_press_event",
                       GTK_SIGNAL_FUNC(cb_key_pressed_shift_tab), dbook_desc);

    /* Capture the Enter & Shift-Enter key combinations to move back and
    * forth between the left- and right-hand sides of the display. */
    gtk_signal_connect(GTK_OBJECT(treeView), "key_press_event",
                       GTK_SIGNAL_FUNC(cb_key_pressed_left_side), dbook_desc);

    gtk_signal_connect(GTK_OBJECT(dbook_desc), "key_press_event",
                       GTK_SIGNAL_FUNC(cb_key_pressed_right_side), NULL);

    gtk_signal_connect(GTK_OBJECT(dbook_note), "key_press_event",
                       GTK_SIGNAL_FUNC(cb_key_pressed_right_side),
                       GINT_TO_POINTER(1));

    /* Allow PgUp and PgDown to move selected day in calendar */
    gtk_signal_connect(GTK_OBJECT(main_calendar), "key_press_event",
                       GTK_SIGNAL_FUNC(cb_keyboard), NULL);

    gtk_signal_connect(GTK_OBJECT(treeView), "key_press_event",
                       GTK_SIGNAL_FUNC(cb_keyboard), NULL);

    /**********************************************************************/

    gtk_widget_show_all(vbox);
    gtk_widget_show_all(hbox);

    gtk_widget_hide(add_record_button);
    gtk_widget_hide(apply_record_button);
    gtk_widget_hide(undelete_record_button);
    gtk_widget_hide(cancel_record_button);
    gtk_widget_hide(hbox_alarm2);

    get_pref(PREF_DATEBOOK_TODO_SHOW, &ivalue, NULL);
    if (!ivalue) {
        gtk_widget_hide_all(todo_vbox);
        gtk_paned_set_position(GTK_PANED(todo_pane), 100000);
    } else {
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(show_todos_button), TRUE);
    }

    datebook_refresh(TRUE, TRUE);

    /* The focus doesn't do any good on the application button */
    gtk_widget_grab_focus(GTK_WIDGET(main_calendar));

    return EXIT_SUCCESS;
}

void
buildTreeView(const GtkWidget *vbox, char *const *titles, long use_db3_tags) {
    GtkWidget *pixbufwid;
    GdkPixbuf *pixbuf;
    GtkCellRenderer *timeRenderer = gtk_cell_renderer_text_new();

    GtkTreeViewColumn *timeColumn = gtk_tree_view_column_new_with_attributes("Time",
                                                                             timeRenderer,
                                                                             "text", DATE_TIME_COLUMN_ENUM,
                                                                             "cell-background-gdk",
                                                                             DATE_BACKGROUND_COLOR_ENUM,
                                                                             "cell-background-set",
                                                                             DATE_BACKGROUND_COLOR_ENABLED_ENUM,
                                                                             NULL);
    GtkCellRenderer *appointmentRenderer = gtk_cell_renderer_text_new();

    GtkTreeViewColumn *appointmentColumn = gtk_tree_view_column_new_with_attributes("Appointment",
                                                                                    appointmentRenderer,
                                                                                    "text", DATE_APPT_COLUMN_ENUM,
                                                                                    "cell-background-gdk",
                                                                                    DATE_BACKGROUND_COLOR_ENUM,
                                                                                    "cell-background-set",
                                                                                    DATE_BACKGROUND_COLOR_ENABLED_ENUM,
                                                                                    NULL);
    GtkCellRenderer *noteRenderer = gtk_cell_renderer_pixbuf_new();
    GtkTreeViewColumn *noteColumn = gtk_tree_view_column_new_with_attributes("",
                                                                             noteRenderer,
                                                                             "pixbuf", DATE_NOTE_COLUMN_ENUM,
                                                                             "cell-background-gdk",
                                                                             DATE_BACKGROUND_COLOR_ENUM,
                                                                             "cell-background-set",
                                                                             DATE_BACKGROUND_COLOR_ENABLED_ENUM,
                                                                             NULL);

    GtkCellRenderer *alarmRenderer = gtk_cell_renderer_pixbuf_new();
    GtkTreeViewColumn *alarmColumn = gtk_tree_view_column_new_with_attributes("",
                                                                              alarmRenderer,
                                                                              "pixbuf", DATE_ALARM_COLUMN_ENUM,
                                                                              "cell-background-gdk",
                                                                              DATE_BACKGROUND_COLOR_ENUM,
                                                                              "cell-background-set",
                                                                              DATE_BACKGROUND_COLOR_ENABLED_ENUM,
                                                                              NULL);

    GtkCellRenderer *floatRenderer = gtk_cell_renderer_pixbuf_new();
    GtkTreeViewColumn *floatColumn = gtk_tree_view_column_new_with_attributes("",
                                                                              floatRenderer,
                                                                              "pixbuf", DATE_FLOAT_COLUMN_ENUM,
                                                                              "cell-background-gdk",
                                                                              DATE_BACKGROUND_COLOR_ENUM,
                                                                              "cell-background-set",
                                                                              DATE_BACKGROUND_COLOR_ENABLED_ENUM,
                                                                              NULL);
    gtk_tree_view_insert_column(GTK_TREE_VIEW (treeView), timeColumn, DATE_TIME_COLUMN_ENUM);
    gtk_tree_view_insert_column(GTK_TREE_VIEW (treeView), noteColumn, DATE_NOTE_COLUMN_ENUM);
    gtk_tree_view_insert_column(GTK_TREE_VIEW (treeView), alarmColumn, DATE_ALARM_COLUMN_ENUM);
    gtk_tree_view_insert_column(GTK_TREE_VIEW (treeView), floatColumn, DATE_FLOAT_COLUMN_ENUM);
    gtk_tree_view_insert_column(GTK_TREE_VIEW (treeView), appointmentColumn, DATE_APPT_COLUMN_ENUM);
    gtk_tree_view_column_set_sizing(timeColumn, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    gtk_tree_view_column_set_sizing(noteColumn, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    gtk_tree_view_column_set_sizing(alarmColumn, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    gtk_tree_view_column_set_sizing(floatColumn, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    gtk_tree_view_column_set_sizing(appointmentColumn, GTK_TREE_VIEW_COLUMN_FIXED);
    gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(treeView)),
                                GTK_SELECTION_BROWSE);
    gtk_tree_view_column_set_clickable(timeColumn, gtk_false());
    gtk_tree_view_column_set_clickable(noteColumn, gtk_false());
    gtk_tree_view_column_set_clickable(alarmColumn, gtk_false());
    gtk_tree_view_column_set_clickable(floatColumn, gtk_false());
    gtk_tree_view_column_set_clickable(appointmentColumn, gtk_false());


#ifdef ENABLE_DATEBK
    if (!use_db3_tags) {
        gtk_tree_view_column_set_visible(floatColumn, FALSE);
    } else {
        gtk_tree_view_column_set_visible(floatColumn, TRUE);
    }
#endif

    GtkTreeSelection *treeSelection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeView));
    gtk_tree_selection_set_select_function(treeSelection, handleDateRowSelection, NULL, NULL);
    /* Put pretty pictures in the treeView column headings */
    //get_pixmaps((GtkWidget *) vbox, PIXMAP_NOTE, pixbuf, mask);
    get_pixbufs(PIXMAP_NOTE, &pixbuf);

    pixbufwid = gtk_image_new_from_pixbuf(pixbuf);
    gtk_widget_show(GTK_WIDGET(pixbufwid));
    gtk_tree_view_column_set_widget(noteColumn, pixbufwid);
    gtk_tree_view_column_set_alignment(noteColumn, GTK_JUSTIFY_CENTER);
    get_pixbufs(PIXMAP_ALARM, &pixbuf);

    pixbufwid = gtk_image_new_from_pixbuf(pixbuf);
    gtk_widget_show(GTK_WIDGET(pixbufwid));
    gtk_tree_view_column_set_widget(alarmColumn, pixbufwid);
    gtk_tree_view_column_set_alignment(alarmColumn, GTK_JUSTIFY_CENTER);
#ifdef ENABLE_DATEBK
    if (use_db3_tags) {
        get_pixbufs(PIXMAP_FLOAT_CHECKED, &pixbuf);

        pixbufwid = gtk_image_new_from_pixbuf(pixbuf);
        gtk_widget_show(GTK_WIDGET(pixbufwid));
        gtk_tree_view_column_set_widget(floatColumn, pixbufwid);
        gtk_tree_view_column_set_alignment(floatColumn, GTK_JUSTIFY_CENTER);
    }
#endif

}

void buildToDoList(const GtkWidget *vbox) {
    GtkWidget *pixmapwid;
    GdkPixbuf *pixbuf;
    todo_scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_set_border_width(GTK_CONTAINER(todo_scrolled_window), 0);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(todo_scrolled_window),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(todo_vbox), todo_scrolled_window, TRUE, TRUE, 0);
    todo_listStore = gtk_list_store_new(TODO_NUM_COLS, G_TYPE_BOOLEAN, G_TYPE_STRING, GDK_TYPE_PIXBUF, G_TYPE_STRING,
                                        G_TYPE_STRING, G_TYPE_POINTER, GDK_TYPE_COLOR, G_TYPE_BOOLEAN, G_TYPE_STRING,
                                        G_TYPE_BOOLEAN);
    GtkTreeModel *model = GTK_TREE_MODEL(todo_listStore);
    todo_treeView = GTK_TREE_VIEW(gtk_tree_view_new_with_model(model));
    GtkCellRenderer *taskRenderer = gtk_cell_renderer_text_new();

    GtkTreeViewColumn *taskColumn = gtk_tree_view_column_new_with_attributes("Task",
                                                                             taskRenderer,
                                                                             "text", TODO_TEXT_COLUMN_ENUM,
                                                                             "cell-background-gdk",
                                                                             TODO_BACKGROUND_COLOR_ENUM,
                                                                             "cell-background-set",
                                                                             TODO_BACKGROUND_COLOR_ENABLED_ENUM,
                                                                             NULL);
    gtk_tree_view_column_set_sort_column_id(taskColumn, TODO_TEXT_COLUMN_ENUM);


    GtkCellRenderer *dateRenderer = gtk_cell_renderer_text_new();

    GtkTreeViewColumn *dateColumn = gtk_tree_view_column_new_with_attributes("Due",
                                                                             dateRenderer,
                                                                             "text", TODO_DATE_COLUMN_ENUM,
                                                                             "cell-background-gdk",
                                                                             TODO_BACKGROUND_COLOR_ENUM,
                                                                             "cell-background-set",
                                                                             TODO_BACKGROUND_COLOR_ENABLED_ENUM,
                                                                             "foreground", TODO_FOREGROUND_COLOR_ENUM,
                                                                             "foreground-set",
                                                                             TODO_FORGROUND_COLOR_ENABLED_ENUM,
                                                                             NULL);
    gtk_tree_view_column_set_sort_column_id(dateColumn, TODO_DATE_COLUMN_ENUM);

    GtkCellRenderer *priorityRenderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *priorityColumn = gtk_tree_view_column_new_with_attributes("",
                                                                                 priorityRenderer,
                                                                                 "text", TODO_PRIORITY_COLUMN_ENUM,
                                                                                 "cell-background-gdk",
                                                                                 TODO_BACKGROUND_COLOR_ENUM,
                                                                                 "cell-background-set",
                                                                                 TODO_BACKGROUND_COLOR_ENABLED_ENUM,
                                                                                 NULL);
    gtk_tree_view_column_set_sort_column_id(priorityColumn, TODO_PRIORITY_COLUMN_ENUM);

    GtkCellRenderer *noteRenderer = gtk_cell_renderer_pixbuf_new();
    GtkTreeViewColumn *noteColumn = gtk_tree_view_column_new_with_attributes("",
                                                                             noteRenderer,
                                                                             "pixbuf", TODO_NOTE_COLUMN_ENUM,
                                                                             "cell-background-gdk",
                                                                             TODO_BACKGROUND_COLOR_ENUM,
                                                                             "cell-background-set",
                                                                             TODO_BACKGROUND_COLOR_ENABLED_ENUM,
                                                                             NULL);
    gtk_tree_view_column_set_sort_column_id(noteColumn, TODO_NOTE_COLUMN_ENUM);


    GtkCellRenderer *checkRenderer = gtk_cell_renderer_toggle_new();

    GtkTreeViewColumn *checkColumn = gtk_tree_view_column_new_with_attributes("", checkRenderer, "active",
                                                                              TODO_CHECK_COLUMN_ENUM,
                                                                              "cell-background-gdk",
                                                                              TODO_BACKGROUND_COLOR_ENUM,
                                                                              "cell-background-set",
                                                                              TODO_BACKGROUND_COLOR_ENABLED_ENUM, NULL);
    gtk_tree_view_insert_column(GTK_TREE_VIEW (todo_treeView), checkColumn, TODO_CHECK_COLUMN_ENUM);
    gtk_tree_view_insert_column(GTK_TREE_VIEW (todo_treeView), priorityColumn, TODO_PRIORITY_COLUMN_ENUM);
    gtk_tree_view_insert_column(GTK_TREE_VIEW (todo_treeView), noteColumn, TODO_NOTE_COLUMN_ENUM);
    gtk_tree_view_insert_column(GTK_TREE_VIEW (todo_treeView), dateColumn, TODO_DATE_COLUMN_ENUM);
    gtk_tree_view_insert_column(GTK_TREE_VIEW (todo_treeView), taskColumn, TODO_TEXT_COLUMN_ENUM);
    gtk_tree_view_column_set_sizing(checkColumn, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    gtk_tree_view_column_set_sizing(dateColumn, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    gtk_tree_view_column_set_sizing(priorityColumn, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    gtk_tree_view_column_set_sizing(noteColumn, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    gtk_tree_view_column_set_sizing(taskColumn, GTK_TREE_VIEW_COLUMN_FIXED);
    gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(todo_treeView)),
                                GTK_SELECTION_BROWSE);
    /* Put pretty pictures in the treeView column headings */

    get_pixbufs(PIXMAP_NOTE, &pixbuf);
    pixmapwid = gtk_image_new_from_pixbuf(pixbuf);
    gtk_widget_show(GTK_WIDGET(pixmapwid));
    gtk_tree_view_column_set_widget(noteColumn, pixmapwid);
    gtk_tree_view_column_set_alignment(noteColumn, GTK_JUSTIFY_CENTER);
    get_pixbufs(PIXMAP_BOX_CHECKED, &pixbuf);
    pixmapwid = gtk_image_new_from_pixbuf(pixbuf);
    gtk_widget_show(GTK_WIDGET(pixmapwid));
    gtk_tree_view_column_set_widget(checkColumn, pixmapwid);
    gtk_tree_view_column_set_alignment(checkColumn, GTK_JUSTIFY_CENTER);
    gtk_tree_view_column_set_clickable(checkColumn, gtk_false());
    gtk_tree_view_column_set_clickable(priorityColumn, gtk_false());
    gtk_tree_view_column_set_clickable(noteColumn, gtk_false());
    gtk_tree_view_column_set_clickable(dateColumn, gtk_false());
    gtk_tree_view_column_set_clickable(taskColumn, gtk_false());


    todo_update_liststore(todo_listStore, NULL, &datebook_todo_list, CATEGORY_ALL, FALSE);

    selectFirstTodoRow();
    todo_treeSelection = gtk_tree_view_get_selection(GTK_TREE_VIEW(todo_treeView));

    gtk_tree_selection_set_select_function(todo_treeSelection, clickedTodoButton, NULL, NULL);

    g_signal_connect (todo_treeView, "button_release_event", GTK_SIGNAL_FUNC(cb_todo_treeview_selection_event), NULL);
    gtk_container_add(GTK_CONTAINER(todo_scrolled_window), GTK_WIDGET(todo_treeView));
}

void selectFirstTodoRow() {
    GtkTreeSelection *treeSelection = gtk_tree_view_get_selection(GTK_TREE_VIEW(todo_treeView));
    GtkTreeIter iter;

    gtk_tree_model_get_iter_first(GTK_TREE_MODEL(todo_listStore), &iter);
    GtkTreePath *path = gtk_tree_model_get_path(GTK_TREE_MODEL(todo_listStore), &iter);
    gtk_tree_selection_select_path(treeSelection, path);
    gtk_tree_path_free(path);

}

gboolean
clickedTodoButton(GtkTreeSelection *selection,
                  GtkTreeModel *model,
                  GtkTreePath *path,
                  gboolean path_currently_selected,
                  gpointer userdata) {
    GtkTreeIter iter;
    MyToDo *mtodo;
    if ((gtk_tree_model_get_iter(model, &iter, path)) && (!path_currently_selected)) {

        gtk_tree_model_get(model, &iter, TODO_DATA_COLUMN_ENUM, &mtodo, -1);
        if (mtodo == NULL) {
            return TRUE;
        }
        glob_find_id = mtodo->unique_id;
        return TRUE;
    }


}

gint cb_todo_treeview_selection_event(GtkWidget *widget,
                                      GdkEvent *event,
                                      gpointer callback_data) {

    if (!event) return 1;
    cb_app_button(NULL, GINT_TO_POINTER(TODO));
    return 1;
}
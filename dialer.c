/*******************************************************************************
 * dialer.c
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
#include <gdk/gdk.h>

#include "i18n.h"
#include "utils.h"
#include "log.h"
#include "prefs.h"

/********************************* Constants **********************************/
#define CHOOSE_PHONE 1
#define CHOOSE_EXT   2

/****************************** Prototypes ************************************/
struct dialog_data {
    GtkWidget *entry_pre1;
    GtkWidget *entry_pre2;
    GtkWidget *entry_pre3;
    GtkWidget *check_pre1;
    GtkWidget *check_pre2;
    GtkWidget *check_pre3;
    GtkWidget *entry_phone;
    GtkWidget *entry_ext;
    GtkWidget *entry_command;
    GtkWidget *label_prefix;
};

/****************************** Main Code *************************************/
static void cb_dialog_button(GtkWidget *widget, gpointer data) {
    GtkWidget *w;

    w = gtk_widget_get_toplevel(widget);
     g_object_get_data(G_OBJECT(w), "dialog_data");
    gtk_widget_destroy(GTK_WIDGET(w));
}

static gboolean cb_destroy_dialog(GtkWidget *widget) {
    struct dialog_data *Pdata;
    const gchar *txt;

    Pdata =  g_object_get_data(G_OBJECT(widget), "dialog_data");
    if (!Pdata) {
        return TRUE;
    }
    txt = gtk_entry_get_text(GTK_ENTRY(Pdata->entry_pre1));
    set_pref(PREF_PHONE_PREFIX1, 0, txt, FALSE);
    txt = gtk_entry_get_text(GTK_ENTRY(Pdata->entry_pre2));
    set_pref(PREF_PHONE_PREFIX2, 0, txt, FALSE);
    txt = gtk_entry_get_text(GTK_ENTRY(Pdata->entry_pre3));
    set_pref(PREF_PHONE_PREFIX3, 0, txt, FALSE);
    txt = gtk_entry_get_text(GTK_ENTRY(Pdata->entry_command));
    set_pref(PREF_DIAL_COMMAND, 0, txt, FALSE);

    set_pref(PREF_CHECK_PREFIX1, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(Pdata->check_pre1)),
             NULL, FALSE);
    set_pref(PREF_CHECK_PREFIX2, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(Pdata->check_pre2)),
             NULL, FALSE);
    set_pref(PREF_CHECK_PREFIX3, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(Pdata->check_pre3)),
             NULL, TRUE);

    gtk_main_quit();

    return TRUE;
}

static void set_prefix_label(struct dialog_data *Pdata) {
    char str[70];

    g_snprintf(str, sizeof(str), "%s%s%s",
               gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(Pdata->check_pre1)) ?
               gtk_entry_get_text(GTK_ENTRY(Pdata->entry_pre1)) : "",
               gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(Pdata->check_pre2)) ?
               gtk_entry_get_text(GTK_ENTRY(Pdata->entry_pre2)) : "",
               gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(Pdata->check_pre3)) ?
               gtk_entry_get_text(GTK_ENTRY(Pdata->entry_pre3)) : "");
    gtk_label_set_text(GTK_LABEL(Pdata->label_prefix), str);
}

static void cb_prefix_change(GtkWidget *widget, gpointer data) {
    set_prefix_label(data);
}

static void dialer(gpointer data, int phone_or_ext) {
    struct dialog_data *Pdata;
    char str[70];
    char null_str[] = "";
    const char *Pext;
    char command[1024];
    const char *pref_command;
    char c1, c2;
    int i, len;

    Pdata = data;
    if (phone_or_ext == CHOOSE_PHONE) {
        g_snprintf(str, sizeof(str), "%s%s%s%s",
                   gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(Pdata->check_pre1)) ?
                   gtk_entry_get_text(GTK_ENTRY(Pdata->entry_pre1)) : "",
                   gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(Pdata->check_pre2)) ?
                   gtk_entry_get_text(GTK_ENTRY(Pdata->entry_pre2)) : "",
                   gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(Pdata->check_pre3)) ?
                   gtk_entry_get_text(GTK_ENTRY(Pdata->entry_pre3)) : "",
                   gtk_entry_get_text(GTK_ENTRY(Pdata->entry_phone)));
    }
    if (phone_or_ext == CHOOSE_EXT) {
        Pext = gtk_entry_get_text(GTK_ENTRY(Pdata->entry_ext));
        if (!Pext) Pext = null_str;
        g_strlcpy(str, Pext, sizeof(str));
    }

    pref_command = gtk_entry_get_text(GTK_ENTRY(Pdata->entry_command));
    /* Make a system call command string */
    memset(command, 0, sizeof(command));
    for (i = 0; i < MAX_PREF_LEN - 1; i++) {
        c1 = pref_command[i];
        c2 = pref_command[i + 1];
        len = strlen(command);
        /* expand '%n' */
        if (c1 == '%') {
            if (c2 == 'n') {
                i++;
                strncat(command, str, 1022 - len);
                continue;
            }
        }
        if (len < 1020) {
            command[len++] = c1;
            command[len] = '\0';
        }
        if (c1 == '\0') {
            break;
        }
    }
    command[1022] = '\0';

    jp_logf(JP_LOG_STDOUT | JP_LOG_FILE, _("executing command = [%s]\n"), command);
    if (system(command) == -1) {
        jp_logf(JP_LOG_WARN, "system call failed %s %d\n", __FILE__, __LINE__);
    }
}

static void cb_dial_ext(GtkWidget *widget, gpointer data) {
    dialer(data, CHOOSE_EXT);
}

static void cb_dial_phone(GtkWidget *widget, gpointer data) {
    dialer(data, CHOOSE_PHONE);
}

/*
 * Dialog window for calling external dialing program
 */
int dialog_dial(GtkWindow *main_window, char *string, char *ext) {
    GtkWidget *button, *label;
    GtkWidget *hbox1, *vbox1;
    GtkWidget *dialog;
    GtkWidget *entry;
    GtkWidget *checkbox1, *checkbox2, *checkbox3;
    struct dialog_data *Pdata;
    long use_prefix;
    const char *prefix;
    const char *dial_command;

    dialog = gtk_widget_new(GTK_TYPE_WINDOW,
                            "type", GTK_WINDOW_TOPLEVEL,
                            "title", _("Phone Dialer"),
                            NULL);

    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_MOUSE);

    g_signal_connect(G_OBJECT(dialog), "destroy",
                       G_CALLBACK(cb_destroy_dialog), dialog);

    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);

    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(main_window));

    /* Set up a data structure for the window */
    Pdata = malloc(sizeof(struct dialog_data));

     g_object_set_data(G_OBJECT(dialog), "dialog_data", Pdata);

    vbox1 = gtk_vbox_new(FALSE, 2);

    gtk_container_set_border_width(GTK_CONTAINER(vbox1), 5);

    gtk_container_add(GTK_CONTAINER(dialog), vbox1);

    /* Prefix 1 */
    hbox1 = gtk_hbox_new(FALSE, 2);
    gtk_container_set_border_width(GTK_CONTAINER(hbox1), 5);
    gtk_box_pack_start(GTK_BOX(vbox1), hbox1, FALSE, FALSE, 2);

    get_pref(PREF_PHONE_PREFIX1, NULL, &prefix);
    get_pref(PREF_CHECK_PREFIX1, &use_prefix, NULL);

    checkbox1 = gtk_check_button_new();
    gtk_box_pack_start(GTK_BOX(hbox1), checkbox1, FALSE, FALSE, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox1), use_prefix);

    label = gtk_label_new(_("Prefix 1"));
    gtk_box_pack_start(GTK_BOX(hbox1), label, FALSE, FALSE, 2);

    entry = gtk_entry_new();
    gtk_entry_set_max_length(entry,32);
    gtk_entry_set_text(GTK_ENTRY(entry), prefix);
    gtk_box_pack_start(GTK_BOX(hbox1), entry, TRUE, TRUE, 1);

    Pdata->entry_pre1 = entry;
    Pdata->check_pre1 = checkbox1;

    /* Prefix 2 */
    hbox1 = gtk_hbox_new(FALSE, 2);
    gtk_container_set_border_width(GTK_CONTAINER(hbox1), 5);
    gtk_box_pack_start(GTK_BOX(vbox1), hbox1, FALSE, FALSE, 2);

    get_pref(PREF_PHONE_PREFIX2, NULL, &prefix);
    get_pref(PREF_CHECK_PREFIX2, &use_prefix, NULL);

    checkbox2 = gtk_check_button_new();
    gtk_box_pack_start(GTK_BOX(hbox1), checkbox2, FALSE, FALSE, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox2), use_prefix);

    label = gtk_label_new(_("Prefix 2"));
    gtk_box_pack_start(GTK_BOX(hbox1), label, FALSE, FALSE, 2);

    entry = gtk_entry_new();
    gtk_entry_set_max_length(entry,32);
    gtk_entry_set_text(GTK_ENTRY(entry), prefix);
    gtk_box_pack_start(GTK_BOX(hbox1), entry, TRUE, TRUE, 1);

    Pdata->entry_pre2 = entry;
    Pdata->check_pre2 = checkbox2;

    /* Prefix 3 */
    hbox1 = gtk_hbox_new(FALSE, 2);
    gtk_container_set_border_width(GTK_CONTAINER(hbox1), 5);
    gtk_box_pack_start(GTK_BOX(vbox1), hbox1, FALSE, FALSE, 2);

    get_pref(PREF_PHONE_PREFIX3, NULL, &prefix);
    get_pref(PREF_CHECK_PREFIX3, &use_prefix, NULL);

    checkbox3 = gtk_check_button_new();
    gtk_box_pack_start(GTK_BOX(hbox1), checkbox3, FALSE, FALSE, 0);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox3), use_prefix);

    label = gtk_label_new(_("Prefix 3"));
    gtk_box_pack_start(GTK_BOX(hbox1), label, FALSE, FALSE, 2);

    entry = gtk_entry_new();
    gtk_entry_set_max_length(entry,32);
    gtk_entry_set_text(GTK_ENTRY(entry), prefix);
    gtk_box_pack_start(GTK_BOX(hbox1), entry, TRUE, TRUE, 1);

    Pdata->entry_pre3 = entry;
    Pdata->check_pre3 = checkbox3;

    /* Phone number entry */
    hbox1 = gtk_hbox_new(FALSE, 2);
    gtk_container_set_border_width(GTK_CONTAINER(hbox1), 5);
    gtk_box_pack_start(GTK_BOX(vbox1), hbox1, FALSE, FALSE, 2);

    label = gtk_label_new(_("Phone number:"));
    gtk_box_pack_start(GTK_BOX(hbox1), label, FALSE, FALSE, 2);

    /* Prefix label */
    label = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(hbox1), label, FALSE, FALSE, 2);
    Pdata->label_prefix = label;
    set_prefix_label(Pdata);

    entry = gtk_entry_new();
    gtk_entry_set_max_length(entry,32);
    gtk_entry_set_text(GTK_ENTRY(entry), string);
    g_signal_connect(G_OBJECT(entry), "activate",
                       G_CALLBACK(cb_dial_ext), Pdata);
    gtk_box_pack_start(GTK_BOX(hbox1), entry, TRUE, TRUE, 1);

    Pdata->entry_phone = entry;

    /* Dial Phone Button */
    button = gtk_button_new_with_label(_("Dial"));
    g_signal_connect(G_OBJECT(button), "clicked",
                       G_CALLBACK(cb_dial_phone), Pdata);
    gtk_box_pack_start(GTK_BOX(hbox1), button, TRUE, TRUE, 1);

    gtk_widget_grab_focus(GTK_WIDGET(button));

    /* Extension */
    hbox1 = gtk_hbox_new(FALSE, 2);
    gtk_container_set_border_width(GTK_CONTAINER(hbox1), 5);
    gtk_box_pack_start(GTK_BOX(vbox1), hbox1, FALSE, FALSE, 2);

    label = gtk_label_new(_("Extension"));
    gtk_box_pack_start(GTK_BOX(hbox1), label, FALSE, FALSE, 2);

    entry = gtk_entry_new();
    gtk_entry_set_max_length(entry,32);
    gtk_entry_set_text(GTK_ENTRY(entry), ext);
    g_signal_connect(G_OBJECT(entry), "activate",
                       G_CALLBACK(cb_dial_ext), Pdata);
    gtk_box_pack_start(GTK_BOX(hbox1), entry, TRUE, TRUE, 1);

    Pdata->entry_ext = entry;

    /* Dial Phone Button */
    button = gtk_button_new_with_label(_("Dial"));
    g_signal_connect(G_OBJECT(button), "clicked",
                       G_CALLBACK(cb_dial_ext), Pdata);
    gtk_box_pack_start(GTK_BOX(hbox1), button, TRUE, TRUE, 1);

    /* Command Entry */
    hbox1 = gtk_hbox_new(FALSE, 2);
    gtk_container_set_border_width(GTK_CONTAINER(hbox1), 5);
    gtk_box_pack_start(GTK_BOX(vbox1), hbox1, FALSE, FALSE, 2);

    label = gtk_label_new(_("Dial Command"));
    gtk_box_pack_start(GTK_BOX(hbox1), label, FALSE, FALSE, 2);

    entry = gtk_entry_new();
    gtk_entry_set_max_length(entry,100);
    gtk_entry_set_text(GTK_ENTRY(entry), ext);
    gtk_box_pack_start(GTK_BOX(hbox1), entry, TRUE, TRUE, 1);

    get_pref(PREF_DIAL_COMMAND, NULL, &dial_command);
    if (dial_command) {
        gtk_entry_set_text(GTK_ENTRY(entry), dial_command);
    }

    Pdata->entry_command = entry;

    /* Button Box */
    hbox1 = gtk_hbutton_box_new();
    gtk_container_set_border_width(GTK_CONTAINER(hbox1), 7);
    gtk_button_box_set_layout(GTK_BUTTON_BOX (hbox1), GTK_BUTTONBOX_END);
    gtk_box_pack_start(GTK_BOX(vbox1), hbox1, FALSE, FALSE, 2);

    /* Buttons */
    button = gtk_button_new_with_label("_Close");
    g_signal_connect(G_OBJECT(button), "clicked",
                       G_CALLBACK(cb_dialog_button),
                       GINT_TO_POINTER(DIALOG_SAID_1));
    gtk_box_pack_start(GTK_BOX(hbox1), button, TRUE, TRUE, 1);

    /* We do this down here because the Pdata structure wasn't complete earlier */
    g_signal_connect(G_OBJECT(checkbox1), "clicked",
                       G_CALLBACK(cb_prefix_change), Pdata);
    g_signal_connect(G_OBJECT(checkbox2), "clicked",
                       G_CALLBACK(cb_prefix_change), Pdata);
    g_signal_connect(G_OBJECT(checkbox3), "clicked",
                       G_CALLBACK(cb_prefix_change), Pdata);
    g_signal_connect(G_OBJECT(Pdata->entry_pre1), "changed",
                       G_CALLBACK(cb_prefix_change), Pdata);
    g_signal_connect(G_OBJECT(Pdata->entry_pre2), "changed",
                       G_CALLBACK(cb_prefix_change), Pdata);
    g_signal_connect(G_OBJECT(Pdata->entry_pre3), "changed",
                       G_CALLBACK(cb_prefix_change), Pdata);

    gtk_widget_show_all(dialog);

    gtk_main();

    free(Pdata);

    return EXIT_SUCCESS;
}

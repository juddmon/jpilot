/*******************************************************************************
 * password.c
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
#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include <pi-version.h>
#include <pi-md5.h>

#include "password.h"
#include "i18n.h"
#include "prefs.h"

#ifdef ENABLE_PRIVATE

/********************************* Constants **********************************/
#define DIALOG_SAID_1  454
#define DIALOG_SAID_2  455

/******************************* Global vars **********************************/
static unsigned char short_salt[]=
{
   0x09, 0x02, 0x13, 0x45, 0x07, 0x04, 0x13, 0x44,
   0x0C, 0x08, 0x13, 0x5A, 0x32, 0x15, 0x13, 0x5D,
   0xD2, 0x17, 0xEA, 0xD3, 0xB5, 0xDF, 0x55, 0x63,
   0x22, 0xE9, 0xA1, 0x4A, 0x99, 0x4B, 0x0F, 0x88
};
static unsigned char long_salt[]=
{
   0xB1, 0x56, 0x35, 0x1A, 0x9C, 0x98, 0x80, 0x84,
   0x37, 0xA7, 0x3D, 0x61, 0x7F, 0x2E, 0xE8, 0x76,
   0x2A, 0xF2, 0xA5, 0x84, 0x07, 0xC7, 0xEC, 0x27,
   0x6F, 0x7D, 0x04, 0xCD, 0x52, 0x1E, 0xCD, 0x5B,
   0xB3, 0x29, 0x76, 0x66, 0xD9, 0x5E, 0x4B, 0xCA,
   0x63, 0x72, 0x6F, 0xD2, 0xFD, 0x25, 0xE6, 0x7B,
   0xC5, 0x66, 0xB3, 0xD3, 0x45, 0x9A, 0xAF, 0xDA,
   0x29, 0x86, 0x22, 0x6E, 0xB8, 0x03, 0x62, 0xBC
};

/****************************** Prototypes ************************************/
struct dialog_data {
   GtkWidget *entry;
   int button_hit;
   char text[PASSWD_LEN+2];
};

/****************************** Main Code *************************************/
/* len is the length of the bin str, hex_str must be at least twice as long */
void bin_to_hex_str(unsigned char *bin, char *hex_str, int len)
{
   int i;
   hex_str[0]='\0';
   for (i=0; i<len; i++) {
      sprintf(&(hex_str[i*2]), "%02x", bin[i]);
   }
}

/*
 * encoded is a pre-allocated buffer at least 34 bytes long
 *
 * This is the hashing algorithm used in palm OS < 4.0.
 * It is not very secure and is reversible.
 */
static void palm_encode_hash(unsigned char *ascii, unsigned char *encoded)
{
   unsigned char index, shift;
   unsigned short temp;
   int si, ai, ei; /* salt i, ascii i, encoded i */
   int end;
   int len;
   int m_array[4]={2,16,24,8};
   int mi;
   int m;

   encoded[0]='\0';
   end=0;
   if (strlen((char *)ascii)<5) {
      si = (ascii[0] + strlen((char*)ascii)) % PASSWD_LEN;
      for (ai=ei=0; ei<32; ai++, ei++, si++) {
         if (ascii[ai]=='\0') {
            end=1;
         }
         if (end) {
            encoded[ei]=short_salt[si%PASSWD_LEN];
         } else {
            encoded[ei]=ascii[ai] ^ short_salt[si%PASSWD_LEN];
         }
      }
      return;
   }

   g_strlcpy((char *)encoded, (char *)ascii, PASSWD_LEN);
   len = strlen((char *)encoded);
   for (ai=len; ai < PASSWD_LEN; ai++) {
      encoded[ai] = encoded[ai-len] + len;
   }

   for (mi=0; mi<4; mi++) {
      m=m_array[mi];
      index = (encoded[m] + encoded[m+1]) & 0x3F;
      shift = (encoded[m+2] + encoded[m+3]) & 0x7;
      for (ei=0; ei<PASSWD_LEN; ei++) {
         temp = long_salt[index%64];
         temp <<= 8;
         temp |= long_salt[index%64];
         temp >>= shift;
         encoded[m%32] ^= temp & 0xFF;
         m++;
         index++;
      }
   }
}

/*
 * encoded is a pre-allocated buffer at least 16 bytes long
 *
 * This is the hashing algorithm used in palm OS >= 4.0.
 * Its just a plain ole' MD5.
 */
static void palm_encode_md5(unsigned char *ascii, unsigned char *encoded)
{
   struct MD5Context m;
   unsigned char digest[20];

   MD5Init(&m);
   MD5Update(&m, ascii, strlen((char *)ascii));
   MD5Final(digest, &m);

   memcpy(encoded, digest, 16);
}

#endif

int verify_password(char *password)
{
   int i;
#ifdef ENABLE_PRIVATE
   unsigned char encoded[34];
   unsigned char password_lower[PASSWD_LEN+2];
   const char *pref_password;
   char hex_str[68];
#endif

#ifdef ENABLE_PRIVATE
   if (!password) {
      return FALSE;
   }

   /* The Palm OS lower cases the password first */
   /* Yes, I have found this documented on Palm's website */
   for (i=0; i < PASSWD_LEN; i++) {
      password_lower[i] = tolower(password[i]);
   }

   /* Check to see that the password matches */
   palm_encode_hash(password_lower, encoded);
   bin_to_hex_str(encoded, hex_str, PASSWD_LEN);
   get_pref(PREF_PASSWORD, NULL, &pref_password);
   if (!strcmp(hex_str, pref_password)) {
      return TRUE;
   }

   /* The Password didn't match.
    * It could also be an MD5 password.
    * Try that now.  */
   palm_encode_md5((unsigned char *)password, encoded);
   bin_to_hex_str(encoded, hex_str, PASSWD_LEN);
   hex_str[PASSWD_LEN]='\0';
   get_pref(PREF_PASSWORD, NULL, &pref_password);
   if (!strcmp(hex_str, pref_password)) {
      return TRUE;
   }
   return FALSE;
#else
   /* Return TRUE without checking the password */
   return TRUE;
#endif
}

/*
 * hide passed HIDE_PRIVATES, SHOW_PRIVATES or MASK_PRIVATES will set the flag.
 * hide passed GET_PRIVATES will return the current hide flag.
 * hide flag is always returned.
 */
int show_privates(int hide)
{
#ifdef ENABLE_PRIVATE
   static int hidden=HIDE_PRIVATES;
#else
   static int hidden=SHOW_PRIVATES;
#endif

   if (hide != GET_PRIVATES)
      hidden = hide;

   return hidden;
}

#ifdef ENABLE_PRIVATE
/*
 * PASSWORD GUI
 */
/* Start of Dialog window code */
static void cb_dialog_button(GtkWidget *widget,
                               gpointer   data)
{
   struct dialog_data *Pdata;
   GtkWidget *w;

   w = gtk_widget_get_toplevel(widget);

   Pdata =  g_object_get_data(G_OBJECT(w), "dialog_data");
   if (Pdata) {
      Pdata->button_hit = GPOINTER_TO_INT(data);
   }
   gtk_widget_destroy(GTK_WIDGET(w));
}

static gboolean cb_destroy_dialog(GtkWidget *widget)
{
   struct dialog_data *Pdata;
   const char *entry;

   Pdata =  g_object_get_data(G_OBJECT(widget), "dialog_data");
   if (!Pdata) {
      return TRUE;
   }
   entry = gtk_entry_get_text(GTK_ENTRY(Pdata->entry));

   if (entry) {
      g_strlcpy(Pdata->text, entry, PASSWD_LEN+1);
   }

   gtk_main_quit();

   return TRUE;
}

/*
 * returns 1 if cancel was pressed, 2 if OK was hit
 */
int dialog_password(GtkWindow *main_window, char *ascii_password, int retry)
{
   GtkWidget *button, *label;
   GtkWidget *hbox1, *vbox1;
   GtkWidget *dialog;
   GtkWidget *entry;
   struct dialog_data Pdata;
   int ret;

   if (!ascii_password) {
      return EXIT_FAILURE;
   }
   ascii_password[0]='\0';
   ret = 2;

   dialog = gtk_widget_new(GTK_TYPE_WINDOW,
                           "type", GTK_WINDOW_TOPLEVEL,
                           "title", _("Palm Password"),
                           NULL);

   gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_MOUSE);
   gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
   gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(main_window));

   g_signal_connect(G_OBJECT(dialog), "destroy",
                      G_CALLBACK(cb_destroy_dialog), dialog);

   hbox1 = gtk_hbox_new(FALSE, 2);
   gtk_container_add(GTK_CONTAINER(dialog), hbox1);
   gtk_box_pack_start(GTK_BOX(hbox1), gtk_image_new_from_stock(GTK_STOCK_DIALOG_AUTHENTICATION, GTK_ICON_SIZE_DIALOG), FALSE, FALSE, 2);

   vbox1 = gtk_vbox_new(FALSE, 2);
   gtk_container_set_border_width(GTK_CONTAINER(vbox1), 5);

   gtk_container_add(GTK_CONTAINER(hbox1), vbox1);

   hbox1 = gtk_hbox_new(TRUE, 2);
   gtk_container_set_border_width(GTK_CONTAINER(hbox1), 5);
   gtk_box_pack_start(GTK_BOX(vbox1), hbox1, FALSE, FALSE, 2);

   /* Instruction label */
   if (retry) {
      label = gtk_label_new(_("Incorrect, Reenter PalmOS Password"));
   } else {
      label = gtk_label_new(_("Enter PalmOS Password"));
   }
   gtk_box_pack_start(GTK_BOX(hbox1), label, FALSE, FALSE, 2);

   /* Password entry field */
   entry = gtk_entry_new();
    gtk_entry_set_max_length(entry,PASSWD_LEN);
   gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
   g_signal_connect(G_OBJECT(entry), "activate",
                      G_CALLBACK(cb_dialog_button),
                      GINT_TO_POINTER(DIALOG_SAID_2));
   gtk_box_pack_start(GTK_BOX(hbox1), entry, TRUE, TRUE, 1);

   /* Button Box */
   hbox1 = gtk_hbutton_box_new();
   gtk_button_box_set_layout(GTK_BUTTON_BOX (hbox1), GTK_BUTTONBOX_END);
    gtk_box_set_spacing(GTK_BOX(hbox1), 6);
   gtk_container_set_border_width(GTK_CONTAINER(hbox1), 5);
   gtk_box_pack_start(GTK_BOX(vbox1), hbox1, FALSE, FALSE, 2);

   /* Cancel Button */
   button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
   g_signal_connect(G_OBJECT(button), "clicked",
                      G_CALLBACK(cb_dialog_button),
                      GINT_TO_POINTER(DIALOG_SAID_1));
   gtk_box_pack_start(GTK_BOX(hbox1), button, FALSE, FALSE, 1);

   /* OK Button */
   button = gtk_button_new_from_stock(GTK_STOCK_OK);
   g_signal_connect(G_OBJECT(button), "clicked",
                      G_CALLBACK(cb_dialog_button),
                      GINT_TO_POINTER(DIALOG_SAID_2));
   gtk_box_pack_start(GTK_BOX(hbox1), button, FALSE, FALSE, 1);
   gtk_widget_set_can_default(button,TRUE);
   gtk_widget_grab_default(button);

   /* Set the default button pressed to CANCEL */
   Pdata.button_hit = DIALOG_SAID_1;
   Pdata.entry=entry;
   Pdata.text[0]='\0';
    g_object_set_data(G_OBJECT(dialog), "dialog_data", &Pdata);

   gtk_widget_grab_focus(GTK_WIDGET(entry));

   gtk_widget_show_all(dialog);

   gtk_main();

   if (Pdata.button_hit==DIALOG_SAID_1) {
      ret = 1;
   }
   if (Pdata.button_hit==DIALOG_SAID_2) {
      ret = 2;
   }
   g_strlcpy(ascii_password, Pdata.text, PASSWD_LEN+1);

   return ret;
}
#endif


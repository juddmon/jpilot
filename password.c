/* password.c
 * A module of J-Pilot http://jpilot.org
 *
 * Copyright (C) 1999-2002 by Judd Montgomery
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
 */
#include "config.h"
#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "password.h"
#include "prefs.h"
#include "i18n.h"

#include <pi-version.h>

/* Try to determine if version of pilot-link > 0.9.x */
#ifdef USB_PILOT_LINK
# undef USB_PILOT_LINK
#endif

#if PILOT_LINK_VERSION > 0
# define USB_PILOT_LINK
#else
# if PILOT_LINK_MAJOR > 9
#  define USB_PILOT_LINK
# endif
#endif

#ifdef USB_PILOT_LINK
#include <pi-md5.h>
#endif

#ifdef ENABLE_PRIVATE

struct dialog_data {
   GtkWidget *entry;
   int button_hit;
   char text[PASSWD_LEN+2];
};

#define DIALOG_SAID_1  454
#define DIALOG_SAID_2  455

unsigned char short_salt[]=
{
   0x09, 0x02, 0x13, 0x45, 0x07, 0x04, 0x13, 0x44, 
   0x0C, 0x08, 0x13, 0x5A, 0x32, 0x15, 0x13, 0x5D, 
   0xD2, 0x17, 0xEA, 0xD3, 0xB5, 0xDF, 0x55, 0x63, 
   0x22, 0xE9, 0xA1, 0x4A, 0x99, 0x4B, 0x0F, 0x88
};
unsigned char long_salt[]=
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
void palm_encode_hash(unsigned char *ascii, unsigned char *encoded)
{
   unsigned char index, shift;
   unsigned short temp;
   int si, ai, ei; /* salt i, ascii i, encoded i */
   int end;
   int len;
   int m_array[4]={2,16,24,8
   };
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

   strncpy(encoded, ascii, PASSWD_LEN);
   encoded[PASSWD_LEN-1]='\0';
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

#ifdef USB_PILOT_LINK
/*
 * encoded is a pre-allocated buffer at least 16 bytes long
 * 
 * This is the hashing algorithm used in palm OS >= 4.0.
 * Its just a plain ole' MD5.
 */
void palm_encode_md5(unsigned char *ascii, unsigned char *encoded)
{
   struct MD5Context m;
   unsigned char digest[20];
   
   MD5Init(&m);
   MD5Update(&m, ascii, strlen((char *)ascii));
   MD5Final(digest, &m);
   
   memcpy(encoded, digest, 16);   
}
#endif /* USB_PILOT_LINK */

#endif

int verify_password(char *password)
{
   int i;
#ifdef ENABLE_PRIVATE
   unsigned char encoded[34];
   unsigned char password_lower[PASSWD_LEN+2];
   const char *pref_password;
   char hex_str[68];
   long ivalue;
#endif

#ifdef ENABLE_PRIVATE
   if (!password) {
      return FALSE;
   }

   /* It seems that Palm OS lower cases the password first */
   /* Yes, I have found this documented on Palms site */
   for (i=0; i < PASSWD_LEN; i++) {
      password_lower[i] = tolower(password[i]);
   }

   /* Check to see that the password matches */
   palm_encode_hash(password_lower, encoded);
   bin_to_hex_str(encoded, hex_str, 32);
   get_pref(PREF_PASSWORD, &ivalue, &pref_password);
   if (!strcmp(hex_str, pref_password)) {
      return TRUE;
   }

# ifdef USB_PILOT_LINK
   /* We need a new pilot-link > 0.11 for this */
   /* The Password didn't match.
    * It could also be an MD5 password.
    * Try that now.
    */
   palm_encode_md5((unsigned char *)password, encoded);
   bin_to_hex_str(encoded, hex_str, 32);
   hex_str[32]='\0';
   get_pref(PREF_PASSWORD, &ivalue, &pref_password);
   if (!strcmp(hex_str, pref_password)) {
      return TRUE;
   }
# endif /* USB_PILOT_LINK */
   return FALSE;
#else
   /* Return TRUE without checking the password */
   return TRUE;
#endif
}

/*
 * hide passed HIDE_PRIVATES will set the hide flag.
 * hide passed SHOW_PRIVATES will unset the hide flag and also need a
 *  correct password.
 * hide passed MASK_PRIVATES will unset the hide flag and also need a
 *  correct password if current state is not SHOW_PRIVATES.
 * hide passed GET_PRIVATES will return the current hide flag.
 * hide flag is always returned, it is boolean.
 */
int show_privates(int hide)
{
#ifdef ENABLE_PRIVATE
   static int hidden=HIDE_PRIVATES;
#else
   static int hidden=SHOW_PRIVATES;
#endif
   
   if (hide==GET_PRIVATES) {
      return hidden;
   }
   if (hide==HIDE_PRIVATES) {
      hidden=HIDE_PRIVATES;
      return hidden;
   }
   if ((hide==MASK_PRIVATES) && (hidden==SHOW_PRIVATES)) {
      hidden=MASK_PRIVATES;
      return hidden;
   }
   if ( (hide==SHOW_PRIVATES) ||
       ((hide==MASK_PRIVATES) && (hidden!=SHOW_PRIVATES)) ) {

      hidden=SHOW_PRIVATES;
   }

   return hidden;
}

#ifdef ENABLE_PRIVATE
/*
 * PASSWORD GUI
 */
/*
 * Start of Dialog window code
 */
static void cb_dialog_button(GtkWidget *widget,
			       gpointer   data)
{
   struct dialog_data *Pdata;
   GtkWidget *w;
   int i;

   w = gtk_widget_get_toplevel(widget);

   Pdata = gtk_object_get_data(GTK_OBJECT(w), "dialog_data");
   if (Pdata) {
      Pdata->button_hit = GPOINTER_TO_INT(data);
   }
   gtk_widget_destroy(GTK_WIDGET(w));
}

static gboolean cb_destroy_dialog(GtkWidget *widget)
{
   struct dialog_data *Pdata;
   const char *entry;

   Pdata = gtk_object_get_data(GTK_OBJECT(widget), "dialog_data");
   if (!Pdata) {
      return TRUE;
   }
   entry = gtk_entry_get_text(GTK_ENTRY(Pdata->entry));

   if (entry) {
      strncpy(Pdata->text, entry, PASSWD_LEN);
      Pdata->text[PASSWD_LEN]='\0';
   }

   gtk_main_quit();

   return TRUE;
}

/*
 * returns 1 if OK was pressed, 2 if cancel was hit
 */
int dialog_password(GtkWindow *main_window, char *ascii_password, int retry)
{
   GtkWidget *button, *label;
   GtkWidget *hbox1, *vbox1;
   GtkWidget *dialog;
   GtkWidget *entry;
   struct dialog_data *Pdata;
   int ret;

   if (!ascii_password) {
      return -1;
   }
   ascii_password[0]='\0';
   ret = 2; 

   dialog = gtk_widget_new(GTK_TYPE_WINDOW,
			   "type", GTK_WINDOW_TOPLEVEL,
			   "title", _("Palm Password"),
			   NULL);

   gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_MOUSE);


   gtk_signal_connect(GTK_OBJECT(dialog), "destroy",
                      GTK_SIGNAL_FUNC(cb_destroy_dialog), dialog);

   gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);

   gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(main_window));

   vbox1 = gtk_vbox_new(FALSE, 2);

   gtk_container_set_border_width(GTK_CONTAINER(vbox1), 5);

   gtk_container_add(GTK_CONTAINER(dialog), vbox1);

   hbox1 = gtk_hbox_new(TRUE, 2);
   gtk_container_set_border_width(GTK_CONTAINER(hbox1), 5);
   gtk_box_pack_start(GTK_BOX(vbox1), hbox1, FALSE, FALSE, 2);

   /* Label */
   if (retry) {
      label = gtk_label_new(_("Incorrect, Reenter PalmOS Password"));
   } else {
      label = gtk_label_new(_("Enter PalmOS Password"));
   }
   gtk_box_pack_start(GTK_BOX(hbox1), label, FALSE, FALSE, 2);

   entry = gtk_entry_new_with_max_length(32);
   gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
   gtk_signal_connect(GTK_OBJECT(entry), "activate",
		      GTK_SIGNAL_FUNC(cb_dialog_button),
		      GINT_TO_POINTER(DIALOG_SAID_1));
   gtk_box_pack_start(GTK_BOX(hbox1), entry, TRUE, TRUE, 1);


   /* Button Box */
   hbox1 = gtk_hbox_new(TRUE, 2);
   gtk_container_set_border_width(GTK_CONTAINER(hbox1), 5);
   gtk_box_pack_start(GTK_BOX(vbox1), hbox1, FALSE, FALSE, 2);

   /* Buttons */
   button = gtk_button_new_with_label(_("OK"));
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_dialog_button),
		      GINT_TO_POINTER(DIALOG_SAID_1));
   gtk_box_pack_start(GTK_BOX(hbox1), button, TRUE, TRUE, 1);

   button = gtk_button_new_with_label(_("Cancel"));
   gtk_signal_connect(GTK_OBJECT(button), "clicked",
		      GTK_SIGNAL_FUNC(cb_dialog_button),
		      GINT_TO_POINTER(DIALOG_SAID_2));
   gtk_box_pack_start(GTK_BOX(hbox1), button, TRUE, TRUE, 1);


   Pdata = malloc(sizeof(struct dialog_data));
   if (Pdata) {
      /* Set the default button pressed to CANCEL */
      Pdata->button_hit = DIALOG_SAID_2;
      Pdata->entry=entry;
      Pdata->text[0]='\0';
   }
   gtk_object_set_data(GTK_OBJECT(dialog), "dialog_data", Pdata);                    

   gtk_widget_grab_focus(GTK_WIDGET(entry));

   gtk_widget_show_all(dialog);

   gtk_main();

   if (Pdata->button_hit==DIALOG_SAID_1) {
      ret = 1;
   }
   if (Pdata->button_hit==DIALOG_SAID_2) {
      ret = 2;
   }
   strncpy(ascii_password, Pdata->text, PASSWD_LEN);

   free(Pdata);

   return ret;
}
#endif

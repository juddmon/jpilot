/*******************************************************************************
 * keyring.c
 *
 * This is a plugin for J-Pilot for the KeyRing Palm program.
 * It keeps records and uses DES3 encryption.
 *
 * Copyright (C) 2001-2014 by Judd Montgomery
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <gtk/gtk.h>

#include "config.h"

#ifdef HAVE_LIBGCRYPT

#  include <gcrypt.h>

#else
/* OpenSSL header files */
#  include <openssl/md5.h>
#  include <openssl/des.h>

#endif

/* Pilot-link header files */
#include <pi-appinfo.h>
#include <pi-dlp.h>
#include <pi-file.h>

/* Jpilot header files */
#include "libplugin.h"
#include "utils.h"
#include "i18n.h"
#include "prefs.h"
#include "stock_buttons.h"
#include "export.h"

/********************************* Constants **********************************/
#define KEYRING_CAT1 1
#define KEYRING_CAT2 2
#define NUM_KEYRING_CAT_ITEMS 16

#define PASSWD_ENTER       0
#define PASSWD_ENTER_RETRY 1
#define PASSWD_ENTER_NEW   2

#define PASSWD_LEN 100

#define CONNECT_SIGNALS    400
#define DISCONNECT_SIGNALS 401
#define PASSWD_FLAG 1

#define KEYR_CHGD_COLUMN 0
#define KEYR_NAME_COLUMN 1
#define KEYR_ACCT_COLUMN 2

/* re-ask password PLUGIN_MAX_INACTIVE_TIME seconds after 
 * deselecting the plugin */
#define PLUGIN_MAX_INACTIVE_TIME 10

/* for password hashes */
#define SALT_SIZE 4
#define MESSAGE_BUF_SIZE 64
#define MD5_HASH_SIZE 16

#define MIN_KR_PASS (20)   /* Minimum auto-generated passwd length */
#define MAX_KR_PASS (25)   /* Maximum auto-generated passwd length */

enum {
    KEYRING_CHANGED_COLUMN_ENUM,
    KEYRING_NAME_COLUMN_ENUM,
    KEYRING_ACCOUNT_COLUMN_ENUM,
    KEYRING_DATA_COLUMN_ENUM,
    KEYRING_BACKGROUND_COLOR_ENUM,
    KEYRING_BACKGROUND_COLOR_ENABLED_ENUM,
    KEYRING_FOREGROUND_COLOR_ENUM,
    KEYRINGS_FOREGROUND_COLOR_ENABLED_ENUM,
    KEYRING_NUM_COLS
};
struct KeyRing {
    char *name;     /* Unencrypted */
    char *account;  /* Encrypted */
    char *password; /* Encrypted */
    char *note;     /* Encrypted */
    struct tm last_changed; /* Encrypted */
};
/* My wrapper to the KeyRing structure so that I can put a few more 
 * fields in with it.  */
struct MyKeyRing {
    PCRecType rt;
    unsigned int unique_id;
    unsigned char attrib;
    struct KeyRing kr;
    struct MyKeyRing *next;
};

/******************************* Global vars **********************************/
/* This is the category that is currently being displayed */
static struct CategoryAppInfo keyr_app_info;
static int keyr_category = CATEGORY_ALL;

static GtkTreeView *treeView;
static GtkListStore *listStore;
static GtkWidget *entry_name;
static GtkWidget *entry_account;
static GtkWidget *entry_password;
static GtkWidget *keyr_note;
static GObject *keyr_note_buffer;
static GtkWidget *category_menu1;
static GtkWidget *category_menu2;
static struct sorted_cats sort_l[NUM_KEYRING_CAT_ITEMS];
static GtkWidget *pane = NULL;
static GtkWidget *scrolled_window;
static GtkWidget *new_record_button;
static GtkWidget *apply_record_button;
static GtkWidget *add_record_button;
static GtkWidget *delete_record_button;
static GtkWidget *undelete_record_button;
static GtkWidget *copy_record_button;
static GtkWidget *cancel_record_button;
static GtkWidget *date_button;
static struct tm glob_date;
#ifndef ENABLE_STOCK_BUTTONS
static GtkAccelGroup *accel_group;
#endif
static int record_changed;
static int column_selected;
static int row_selected;

#ifdef HAVE_LIBGCRYPT
static unsigned char key[24];
#else
#ifdef HEADER_NEW_DES_H
static DES_cblock current_key1;
static DES_cblock current_key2;
static DES_key_schedule s1, s2;
#else
static des_cblock current_key1;
static des_cblock current_key2;
static des_key_schedule s1, s2;
#endif
#endif

static time_t plugin_last_time = 0;
static gboolean plugin_active = FALSE;

static struct MyKeyRing *glob_keyring_list = NULL;
static struct MyKeyRing *export_keyring_list = NULL;

/****************************** Prototypes ************************************/

void keyr_update_liststore(GtkListStore *pListStore, struct MyKeyRing **keyring_list,
                           int category, int main);

static void connect_changed_signals(int con_or_dis);

static int keyring_find(int unique_id);

static void update_date_button(GtkWidget *button, struct tm *t);

static gboolean handleKeyringRowSelection(GtkTreeSelection *selection,
                                          GtkTreeModel *model,
                                          GtkTreePath *path,
                                          gboolean path_currently_selected,
                                          gpointer userdata);

void deleteKeyRing(struct MyKeyRing *mkr, gpointer data);

void undeleteKeyRing(struct MyKeyRing *mkr, gpointer data);

void addKeyRing(struct MyKeyRing *mkr, gpointer data);

static GtkWidget *cb_keyr_export_init_treeView();

/****************************** Main Code *************************************/

/* Routine to get category app info from raw buffer. 
 * KeyRing is broken and uses a non-standard length CategoryAppInfo.
 * The KeyRing structure is 276 bytes whereas pilot-link uses 278.
 * Code below is taken from unpack_CategoryAppInfo in pilot-link but modified
 * for the shortened structure. */
static int keyr_plugin_unpack_cai_from_ai(struct CategoryAppInfo *cai,
                                          unsigned char *record,
                                          int len) {
    int i, rec;

    jp_logf(JP_LOG_DEBUG, "unpack_keyring_cai_from_ai\n");

    if (len < 2 + 16 * 16 + 16 + 2)
        return EXIT_FAILURE;
    rec = get_short(record);
    for (i = 0; i < 16; i++) {
        if (rec & (1 << i))
            cai->renamed[i] = 1;
        else
            cai->renamed[i] = 0;
    }
    record += 2;
    for (i = 0; i < 16; i++) {
        memcpy(cai->name[i], record, 16);
        record += 16;
    }
    memcpy(cai->ID, record, 16);
    record += 16;
    cai->lastUniqueID = get_byte(record);

    return EXIT_SUCCESS;
}

int plugin_unpack_cai_from_ai(struct CategoryAppInfo *cai,
                              unsigned char *record,
                              int len) {
    return keyr_plugin_unpack_cai_from_ai(cai, record, len);
}

/* Routine to pack CategoryAppInfo struct into non-standard size buffer */
int plugin_pack_cai_into_ai(struct CategoryAppInfo *cai,
                            unsigned char *record,
                            int len) {
    int i, rec;

    if (!record) {
        return EXIT_SUCCESS;
    }
    if (len < (2 + 16 * 16 + 16 + 2))
        return EXIT_FAILURE;   /* not enough room */
    rec = 0;
    for (i = 0; i < 16; i++) {
        if (cai->renamed[i])
            rec |= (1 << i);
    }
    set_short(record, rec);
    record += 2;
    for (i = 0; i < 16; i++) {
        memcpy(record, cai->name[i], 16);
        record += 16;
    }
    memcpy(record, cai->ID, 16);
    record += 16;
    set_byte(record, cai->lastUniqueID);
    record++;
    set_byte(record, 0);      /* gapfill */

    return EXIT_SUCCESS;
}

static int pack_KeyRing(struct KeyRing *kr,
                        unsigned char *buf,
                        int buf_size,
                        int *wrote_size) {
    int n;
    int i;
    char empty[] = "";
    char last_changed[2];
    unsigned short packed_date;
#ifdef HAVE_LIBGCRYPT
    gcry_error_t err;
    gcry_cipher_hd_t hd;
#endif

    jp_logf(JP_LOG_DEBUG, "KeyRing: pack_KeyRing()\n");

    packed_date = (((kr->last_changed.tm_year - 4) << 9) & 0xFE00) |
                  (((kr->last_changed.tm_mon + 1) << 5) & 0x01E0) |
                  (kr->last_changed.tm_mday & 0x001F);
    set_short(last_changed, packed_date);

    *wrote_size = 0;

    if (!(kr->name)) kr->name = empty;
    if (!(kr->account)) kr->account = empty;
    if (!(kr->password)) kr->password = empty;
    if (!(kr->note)) kr->note = empty;

    /* 2 is for the lastChanged date */
    /* 3 chars accounts for NULL string terminators */
    n = strlen(kr->account) + strlen(kr->password) + strlen(kr->note) + 2 + 3;
    /* The encrypted portion must be a multiple of 8 */
    if ((n % 8)) {
        n = n + (8 - (n % 8));
    }
    /* Now we can add in the unencrypted part */
    n = n + strlen(kr->name) + 1;
    jp_logf(JP_LOG_DEBUG, "pack n=%d\n", n);

    if (n + 2 > buf_size) {
        jp_logf(JP_LOG_WARN, _("KeyRing: pack_KeyRing(): buf_size too small\n"));
        return EXIT_FAILURE;
    }

    memset(buf, 0, n + 1);
    *wrote_size = n;
    strcpy((char *) buf, kr->name);
    i = strlen(kr->name) + 1;
    strcpy((char *) &buf[i], kr->account);
    i += strlen(kr->account) + 1;
    strcpy((char *) &buf[i], kr->password);
    i += strlen(kr->password) + 1;
    strcpy((char *) &buf[i], kr->note);
    i += strlen(kr->note) + 1;
    strncpy((char *) &buf[i], last_changed, 2);
#ifdef HAVE_LIBGCRYPT
    err = gcry_cipher_open(&hd, GCRY_CIPHER_3DES, GCRY_CIPHER_MODE_ECB, 0);
    if (err)
        jp_logf(JP_LOG_DEBUG, "gcry_cipher_open: %s\n", gpg_strerror(err));

    err = gcry_cipher_setkey(hd, key, sizeof(key));
    if (err)
        jp_logf(JP_LOG_DEBUG, "gcry_cipher_setkey: %s\n", gpg_strerror(err));

    for (i = strlen(kr->name) + 1; i < n; i += 8) {
        char tmp[8];
        err = gcry_cipher_encrypt(hd, tmp, 8, &buf[i], 8);
        if (err)
            jp_logf(JP_LOG_DEBUG, "gcry_cipher_encrypt: %s\n", gpg_strerror(err));
        memcpy(&buf[i], tmp, 8);
    }

    gcry_cipher_close(hd);
#else
    for (i=strlen(kr->name)+1; i<n; i=i+8) {
#ifdef HEADER_NEW_DES_H
       DES_ecb3_encrypt((DES_cblock *)&buf[i], (DES_cblock *)&buf[i],
                        &s1, &s2, &s1, DES_ENCRYPT);
#else
       des_ecb3_encrypt((const_des_cblock *)&buf[i], (des_cblock *)(&buf[i]),
                        s1, s2, s1, DES_ENCRYPT);
#endif
    }
#endif

#ifdef JPILOT_DEBUG
    for (i=0;i<n; i++) {
       printf("%02x ", (unsigned char)buf[i]);
    }
    printf("\n");
#endif

    return n;
}

static int unpack_KeyRing(struct KeyRing *kr,
                          unsigned char *buf,
                          int buf_size) {
    int i, j;
    int n;
    int rem;
    unsigned char *clear_text;
    unsigned char *P;
    unsigned char *Pstr[4];
    const char *safety[] = {"", "", "", ""};
    unsigned short packed_date;
#ifdef HAVE_LIBGCRYPT
    gcry_error_t err;
    gcry_cipher_hd_t hd;
#endif

    jp_logf(JP_LOG_DEBUG, "KeyRing: unpack_KeyRing\n");
    if (!memchr(buf, '\0', buf_size)) {
        jp_logf(JP_LOG_DEBUG, "KeyRing: unpack_KeyRing(): No null terminator found in buf\n");
        return 0;
    }
    n = strlen((char *) buf) + 1;

    rem = buf_size - n;
    if (rem > 0xFFFF) {
        /* This can be caused by a bug in libplugin.c from jpilot 0.99.1
         * and before.  It occurs on the last record */
        jp_logf(JP_LOG_DEBUG, "KeyRing: unpack_KeyRing(): buffer too big n=%d, buf_size=%d\n", n, buf_size);
        jp_logf(JP_LOG_DEBUG, "KeyRing: unpack_KeyRing(): truncating\n");
        rem = 0xFFFF - n;
        rem = rem - (rem % 8);
    }
    clear_text = malloc(rem + 8); /* Allow for some safety NULLs */
    memset(clear_text, 0, rem + 8);

    jp_logf(JP_LOG_DEBUG, "KeyRing: unpack_KeyRing(): rem (should be multiple of 8)=%d\n", rem);
    jp_logf(JP_LOG_DEBUG, "KeyRing: unpack_KeyRing(): rem%%8=%d\n", rem % 8);

    P = &buf[n];
#ifdef HAVE_LIBGCRYPT
    err = gcry_cipher_open(&hd, GCRY_CIPHER_3DES, GCRY_CIPHER_MODE_ECB, 0);
    if (err)
        jp_logf(JP_LOG_DEBUG, "gcry_cipher_open: %s\n", gpg_strerror(err));

    err = gcry_cipher_setkey(hd, key, sizeof(key));
    if (err)
        jp_logf(JP_LOG_DEBUG, "gcry_cipher_setkey: %s\n", gpg_strerror(err));

    err = gcry_cipher_decrypt(hd, clear_text, rem, P, rem);
    if (err)
        jp_logf(JP_LOG_DEBUG, "gcry_cipher_decrypt: %s\n", gpg_strerror(err));

    gcry_cipher_close(hd);
#else
    for (i=0; i<rem; i+=8) {
#ifdef HEADER_NEW_DES_H
       DES_ecb3_encrypt((DES_cblock *)&P[i], (DES_cblock *)(clear_text+i),
                        &s1, &s2, &s1, DES_DECRYPT);
#else
       des_ecb3_encrypt((const_des_cblock *)&P[i], (des_cblock *)(clear_text+i),
                        s1, s2, s1, DES_DECRYPT);
#endif
    }
#endif

    Pstr[0] = clear_text;
    Pstr[1] = (unsigned char *) safety[1];
    Pstr[2] = (unsigned char *) safety[2];
    Pstr[3] = (unsigned char *) safety[3];

    for (i = 0, j = 1; (i < rem) && (j < 4); i++) {
        if (!clear_text[i]) {
            Pstr[j] = &clear_text[i + 1];
            j++;
        }
    }
    /*
    kr->name=strdup((char *)buf);
    kr->account=strdup((char *)Pstr[0]);
    kr->password=strdup((char *)Pstr[1]);
    kr->note=strdup((char *)Pstr[2]);
    */

    kr->name = jp_charset_p2newj((char *) buf, -1);
    kr->account = jp_charset_p2newj((char *) Pstr[0], -1);
    kr->password = jp_charset_p2newj((char *) Pstr[1], -1);
    kr->note = jp_charset_p2newj((char *) Pstr[2], -1);

    packed_date = get_short(Pstr[3]);
    kr->last_changed.tm_year = ((packed_date & 0xFE00) >> 9) + 4;
    kr->last_changed.tm_mon = ((packed_date & 0x01E0) >> 5) - 1;
    kr->last_changed.tm_mday = (packed_date & 0x001F);
    kr->last_changed.tm_hour = 0;
    kr->last_changed.tm_min = 0;
    kr->last_changed.tm_sec = 0;
    kr->last_changed.tm_isdst = -1;

    if (0 == packed_date) {
        kr->last_changed.tm_year = 0;
        kr->last_changed.tm_mon = 0;
        kr->last_changed.tm_mday = 0;
    }

#ifdef DEBUG
    printf("name  [%s]\n", buf);
    printf("Pstr0 [%s]\n", Pstr[0]);
    printf("Pstr1 [%s]\n", Pstr[1]);
    printf("Pstr2 [%s]\n", Pstr[2]);
    printf("last_changed %d-%d-%d\n",
            kr->last_changed.tm_year,
            kr->last_changed.tm_mon,
            kr->last_changed.tm_mday);
#endif

    free(clear_text);

    return 1;
}

static int get_keyr_cat_info(struct CategoryAppInfo *cai) {
    unsigned char *buf;
    int buf_size;

    memset(cai, 0, sizeof(struct CategoryAppInfo));
    jp_get_app_info("Keys-Gtkr", &buf, &buf_size);
    keyr_plugin_unpack_cai_from_ai(cai, buf, buf_size);
    free(buf);

    return EXIT_SUCCESS;
}

/*
 * Return EXIT_FAILURE if password isn't good.
 * Return EXIT_SUCCESS if good and global and also sets s1, and s2 set
 */
static int set_password_hash(unsigned char *buf, int buf_size, char *passwd) {
    unsigned char buffer[MESSAGE_BUF_SIZE];
    unsigned char md[MD5_HASH_SIZE];

    if (buf_size < MD5_HASH_SIZE) {
        return EXIT_FAILURE;
    }
    /* Must wipe passwd out of memory after using it */
    memset(buffer, 0, MESSAGE_BUF_SIZE);
    memcpy(buffer, buf, SALT_SIZE);
    strncpy((char *) (buffer + SALT_SIZE), passwd, MESSAGE_BUF_SIZE - SALT_SIZE - 1);
#ifdef HAVE_LIBGCRYPT
    gcry_md_hash_buffer(GCRY_MD_MD5, md, buffer, MESSAGE_BUF_SIZE);
#else
    MD5(buffer, MESSAGE_BUF_SIZE, md);
#endif

    /* wipe out password traces */
    memset(buffer, 0, MESSAGE_BUF_SIZE);

    if (memcmp(md, buf + SALT_SIZE, MD5_HASH_SIZE)) {
        return EXIT_FAILURE;
    }

#ifdef HAVE_LIBGCRYPT
    gcry_md_hash_buffer(GCRY_MD_MD5, md, passwd, strlen(passwd));
    memcpy(key, md, 16);    /* k1 and k2 */
    memcpy(key + 16, md, 8);  /* k1 again */
#else
    MD5((unsigned char *)passwd, strlen(passwd), md);
    memcpy(current_key1, md, 8);
    memcpy(current_key2, md+8, 8);
#ifdef HEADER_NEW_DES_H
    DES_set_key(&current_key1, &s1);
    DES_set_key(&current_key2, &s2);
#else
    des_set_key(&current_key1, s1);
    des_set_key(&current_key2, s2);
#endif
#endif

    return EXIT_SUCCESS;
}

/* Start password change code */

/* 
 * Code for this is written, just need to add another jpilot API for
 * cancelling a sync if the passwords don't match.
 */

/* End password change code   */

/* Utility function to read keyring data file and filter out unwanted records 
 *
 * Returns the number of records read */
static int get_keyring(struct MyKeyRing **mkr_list, int category) {
    GList *records = NULL;
    GList *temp_list;
    buf_rec *br;
    struct MyKeyRing *mkr;
    int rec_count;
    long keep_modified, keep_deleted;

    jp_logf(JP_LOG_DEBUG, "get_keyring()\n");

    *mkr_list = NULL;
    rec_count = 0;

    /* Read raw database of records */
    if (jp_read_DB_files("Keys-Gtkr", &records) == -1)
        return 0;

    /* Get preferences used for filtering */
    get_pref(PREF_SHOW_MODIFIED, &keep_modified, NULL);
    get_pref(PREF_SHOW_DELETED, &keep_deleted, NULL);

    /* Sort through list of records masking out unwanted ones */
    for (temp_list = records; temp_list; temp_list = temp_list->next) {
        if (temp_list->data) {
            br = temp_list->data;
        } else {
            continue;
        }
        if (!br->buf) {
            continue;
        }
        /* record 0 is the hash-key record */
        if (br->attrib & dlpRecAttrSecret) {
            continue;
        }

        /* Filter out deleted or deleted/modified records */
        if (((br->rt == DELETED_PALM_REC) && (!keep_deleted)) ||
            ((br->rt == DELETED_PC_REC) && (!keep_deleted)) ||
            ((br->rt == MODIFIED_PALM_REC) && (!keep_modified))) {
            continue;
        }

        /* Filter by category */
        if (((br->attrib & 0x0F) != category) && category != CATEGORY_ALL) {
            continue;
        }

        mkr = malloc(sizeof(struct MyKeyRing));
        mkr->next = NULL;
        mkr->attrib = br->attrib;
        mkr->unique_id = br->unique_id;
        mkr->rt = br->rt;

        if (unpack_KeyRing(&(mkr->kr), br->buf, br->size) <= 0) {
            free(mkr);
            continue;
        }

        /* prepend to list */
        mkr->next = *mkr_list;
        *mkr_list = mkr;

        rec_count++;
    }

    jp_free_DB_records(&records);

    jp_logf(JP_LOG_DEBUG, "Leaving get_keyring()\n");

    return rec_count;
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

/* Find position of category in sorted category array 
 * via its assigned category number */
static int find_sort_cat_pos(int cat) {
    int i;

    for (i = 0; i < NUM_KEYRING_CAT_ITEMS; i++) {
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

    if (cat != NUM_KEYRING_CAT_ITEMS - 1) {
        return cat;
    } else { /* Unfiled category */
        /* Count how many category entries are filled */
        for (i = 0; i < NUM_KEYRING_CAT_ITEMS; i++) {
            if (!sort_l[i].Pcat[0]) {
                return i;
            }
        }
        return 0;
    }
}


static gint GtkTreeModelKeyrCompareDates(GtkTreeModel *model,
                                         GtkTreeIter *left,
                                         GtkTreeIter *right,
                                         gpointer columnId) {


    struct MyKeyRing *mkr1, *mkr2;

    struct KeyRing *keyr1, *keyr2;
    time_t time1, time2;
    gtk_tree_model_get(GTK_TREE_MODEL(model), left, KEYRING_DATA_COLUMN_ENUM, &mkr1, -1);
    gtk_tree_model_get(GTK_TREE_MODEL(model), right, KEYRING_DATA_COLUMN_ENUM, &mkr2, -1);
    keyr1 = &(mkr1->kr);
    keyr2 = &(mkr2->kr);

    time1 = mktime(&(keyr1->last_changed));
    time2 = mktime(&(keyr2->last_changed));

    return (time1 - time2);
}

/* Function is used to sort list case insensitively
 * not sure if this is really needed as the default sort seems to do the same thing
 */
static gint GtkTreeModelKeyrCompareNocase(GtkTreeModel *model,
                                          GtkTreeIter *left,
                                          GtkTreeIter *right,
                                          gpointer columnId) {


    gchar *str1, *str2;
    gtk_tree_model_get(GTK_TREE_MODEL(model), left, KEYRING_NAME_COLUMN_ENUM, &str1, -1);
    gtk_tree_model_get(GTK_TREE_MODEL(model), right, KEYRING_NAME_COLUMN_ENUM, &str2, -1);
    return g_ascii_strcasecmp(str1, str2);
}

static void cb_record_changed(GtkWidget *widget, gpointer data) {
    int flag;
    struct tm *now;
    time_t ltime;

    jp_logf(JP_LOG_DEBUG, "cb_record_changed\n");

    flag = GPOINTER_TO_INT(data);

    if (record_changed == CLEAR_FLAG) {
        connect_changed_signals(DISCONNECT_SIGNALS);
        if (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(listStore), NULL) > 0) {
            set_new_button_to(MODIFY_FLAG);
            /* Update the lastChanged field when password is modified */
            if (flag == PASSWD_FLAG) {
                time(&ltime);
                now = localtime(&ltime);
                memcpy(&glob_date, now, sizeof(struct tm));
                update_date_button(date_button, &glob_date);
            }
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
        jp_logf(JP_LOG_DEBUG, "KeyRing: connect_changed_signals\n");
        connected = 1;

        if(category_menu2){
            g_signal_connect(G_OBJECT(category_menu2),"changed",G_CALLBACK(cb_record_changed),NULL);
        }

        gtk_signal_connect(GTK_OBJECT(entry_name), "changed",
                           G_CALLBACK(cb_record_changed), NULL);
        gtk_signal_connect(GTK_OBJECT(entry_account), "changed",
                           G_CALLBACK(cb_record_changed), NULL);
        gtk_signal_connect(GTK_OBJECT(entry_password), "changed",
                           G_CALLBACK(cb_record_changed),
                           GINT_TO_POINTER(PASSWD_FLAG));
        gtk_signal_connect(GTK_OBJECT(date_button), "pressed",
                           G_CALLBACK(cb_record_changed), NULL);
        g_signal_connect(keyr_note_buffer, "changed",
                         G_CALLBACK(cb_record_changed), NULL);
    }

    /* DISCONNECT */
    if ((con_or_dis == DISCONNECT_SIGNALS) && (connected)) {
        jp_logf(JP_LOG_DEBUG, "KeyRing: disconnect_changed_signals\n");
        connected = 0;

        if(category_menu2){
            g_signal_connect(G_OBJECT(category_menu2),"changed",G_CALLBACK(cb_record_changed),NULL);
        }

        gtk_signal_disconnect_by_func(GTK_OBJECT(entry_name),
                                      G_CALLBACK(cb_record_changed), NULL);
        gtk_signal_disconnect_by_func(GTK_OBJECT(entry_account),
                                      G_CALLBACK(cb_record_changed), NULL);
        gtk_signal_disconnect_by_func(GTK_OBJECT(entry_password),
                                      G_CALLBACK(cb_record_changed),
                                      GINT_TO_POINTER(PASSWD_FLAG));
        gtk_signal_disconnect_by_func(GTK_OBJECT(date_button),
                                      G_CALLBACK(cb_record_changed), NULL);
        g_signal_handlers_disconnect_by_func(keyr_note_buffer,
                                             G_CALLBACK(cb_record_changed), NULL);
    }
}

static void free_mykeyring_list(struct MyKeyRing **PPmkr) {
    struct MyKeyRing *mkr, *next_mkr;

    jp_logf(JP_LOG_DEBUG, "KeyRing: free_mykeyring_list\n");
    for (mkr = *PPmkr; mkr; mkr = next_mkr) {
        if (mkr->kr.name) free(mkr->kr.name);
        if (mkr->kr.account) free(mkr->kr.account);
        if (mkr->kr.password) free(mkr->kr.password);
        if (mkr->kr.note) free(mkr->kr.note);
        next_mkr = mkr->next;
        free(mkr);
    }
    *PPmkr = NULL;
}

gboolean deleteKeyRingRecord(GtkTreeModel *model,
                             GtkTreePath *path,
                             GtkTreeIter *iter,
                             gpointer data) {
    int *i = gtk_tree_path_get_indices(path);
    if (i[0] == row_selected) {
        struct MyKeyRing *mkr = NULL;
        gtk_tree_model_get(model, iter, KEYRING_DATA_COLUMN_ENUM, &mkr, -1);
        deleteKeyRing(mkr, data);
        return TRUE;
    }

    return FALSE;


}

gboolean undeleteKeyRingRecord(GtkTreeModel *model,
                               GtkTreePath *path,
                               GtkTreeIter *iter,
                               gpointer data) {
    int *i = gtk_tree_path_get_indices(path);
    if (i[0] == row_selected) {
        struct MyKeyRing *mkr = NULL;
        gtk_tree_model_get(model, iter, KEYRING_DATA_COLUMN_ENUM, &mkr, -1);
        undeleteKeyRing(mkr, data);
        return TRUE;
    }

    return FALSE;


}

void undeleteKeyRing(struct MyKeyRing *mkr, gpointer data) {

    buf_rec br;
    char buf[0xFFFF];
    int new_size;
    int flag;
    if (mkr == NULL) {
        return;
    }

    jp_logf(JP_LOG_DEBUG, "mkr->unique_id = %d\n", mkr->unique_id);
    jp_logf(JP_LOG_DEBUG, "mkr->rt = %d\n", mkr->rt);

    pack_KeyRing(&(mkr->kr), (unsigned char *) buf, 0xFFFF, &new_size);

    br.rt = mkr->rt;
    br.unique_id = mkr->unique_id;
    br.attrib = mkr->attrib;
    br.buf = buf;
    br.size = new_size;

    flag = GPOINTER_TO_INT(data);

    if (flag == UNDELETE_FLAG) {
        if (mkr->rt == DELETED_PALM_REC ||
            mkr->rt == DELETED_PC_REC) {
            jp_undelete_record("Keys-Gtkr", &br, flag);
        }
        /* Possible later addition of undelete for modified records
        else if (mmemo->rt == MODIFIED_PALM_REC)
        {
           cb_add_new_record(widget, GINT_TO_POINTER(COPY_FLAG));
        }
        */
    }

    keyr_update_liststore(listStore, &glob_keyring_list, keyr_category, TRUE);
}

void deleteKeyRing(struct MyKeyRing *mkr, gpointer data) {
    struct KeyRing kr;
    int new_size;
    char buf[0xFFFF];
    buf_rec br;
    int flag;

    jp_logf(JP_LOG_DEBUG, "KeyRing: cb_delete_keyring\n");

    if (!mkr) {
        return;
    }

    /* The record that we want to delete should be written to the pc file
     * so that it can be deleted at sync time.  We need the original record
     * so that if it has changed on the pilot we can warn the user that
     * the record has changed on the pilot. */
    kr = mkr->kr;

    kr.name = strdup(kr.name);
    jp_charset_j2p(kr.name, strlen(kr.name) + 1);

    kr.account = strdup(kr.account);
    jp_charset_j2p(kr.account, strlen(kr.account) + 1);

    kr.password = strdup(kr.password);
    jp_charset_j2p(kr.password, strlen(kr.password) + 1);

    kr.note = strdup(kr.note);
    jp_charset_j2p(kr.note, strlen(kr.note) + 1);

    pack_KeyRing(&kr, (unsigned char *) buf, 0xFFFF, &new_size);

    free(kr.name);
    free(kr.account);
    free(kr.password);
    free(kr.note);

    br.rt = mkr->rt;
    br.unique_id = mkr->unique_id;
    br.attrib = mkr->attrib;
    br.buf = buf;
    br.size = new_size;

    flag = GPOINTER_TO_INT(data);
    if ((flag == MODIFY_FLAG) || (flag == DELETE_FLAG)) {
        jp_delete_record("Keys-Gtkr", &br, flag);
        if (flag == DELETE_FLAG) {
            /* when we redraw we want to go to the line above the deleted one */
            if (row_selected > 0) {
                row_selected--;
            }
        }
    }

    if (flag == DELETE_FLAG) {
        keyr_update_liststore(listStore, &glob_keyring_list, keyr_category, TRUE);
    }
}

/* This function gets called when the "delete" button is pressed */
static void cb_delete_keyring(GtkWidget *widget, gpointer data) {
    gtk_tree_model_foreach(GTK_TREE_MODEL(listStore), deleteKeyRingRecord, data);
}

static void cb_undelete_keyring(GtkWidget *widget, gpointer data) {
    gtk_tree_model_foreach(GTK_TREE_MODEL(listStore), undeleteKeyRingRecord, data);

}

static void cb_cancel(GtkWidget *widget, gpointer data) {
    set_new_button_to(CLEAR_FLAG);
    keyr_update_liststore(listStore, &glob_keyring_list, keyr_category, TRUE);
}

static void update_date_button(GtkWidget *button, struct tm *t) {
    const char *short_date;
    char str[255];

    get_pref(PREF_SHORTDATE, NULL, &short_date);
    strftime(str, sizeof(str), short_date, t);

    gtk_label_set_text(GTK_LABEL(gtk_bin_get_child(GTK_BIN(button))), str);
}

/*
 * This is called when the "New" button is pressed.
 * It clears out all the detail fields on the right-hand side.
 */
static int keyr_clear_details(void) {
    struct tm *now;
    time_t ltime;
    int new_cat;
    int sorted_position;

    jp_logf(JP_LOG_DEBUG, "KeyRing: cb_clear\n");

    connect_changed_signals(DISCONNECT_SIGNALS);

    /* Put the current time in the lastChanged part of the record */
    time(&ltime);
    now = localtime(&ltime);
    memcpy(&glob_date, now, sizeof(struct tm));
    update_date_button(date_button, &glob_date);

    gtk_entry_set_text(GTK_ENTRY(entry_name), "");
    gtk_entry_set_text(GTK_ENTRY(entry_account), "");
    gtk_entry_set_text(GTK_ENTRY(entry_password), "");
    gtk_text_buffer_set_text(GTK_TEXT_BUFFER(keyr_note_buffer), "", -1);
    if (keyr_category == CATEGORY_ALL) {
        new_cat = 0;
    } else {
        new_cat = keyr_category;
    }
    sorted_position = find_sort_cat_pos(new_cat);
    if (sorted_position < 0) {
        jp_logf(JP_LOG_WARN, _("Category is not legal\n"));
    } else {
        gtk_combo_box_set_active(GTK_COMBO_BOX(category_menu2),find_menu_cat_pos(sorted_position));
    }

    set_new_button_to(CLEAR_FLAG);
    connect_changed_signals(CONNECT_SIGNALS);

    return EXIT_SUCCESS;
}

gboolean addKeyRingRecord(GtkTreeModel *model,
                          GtkTreePath *path,
                          GtkTreeIter *iter,
                          gpointer data) {
    int *i = gtk_tree_path_get_indices(path);
    if (i[0] == row_selected) {
        struct MyKeyRing *mkr = NULL;
        gtk_tree_model_get(model, iter, KEYRING_DATA_COLUMN_ENUM, &mkr, -1);
        addKeyRing(mkr, data);
        return TRUE;
    }

    return FALSE;


}

void addKeyRing(struct MyKeyRing *mkr, gpointer data) {
    struct KeyRing kr;
    buf_rec br;
    unsigned char buf[0x10000];
    int new_size;
    int flag;

    GtkTextIter start_iter;
    GtkTextIter end_iter;
    int i;
    unsigned int unique_id;


    unique_id = 0;

    jp_logf(JP_LOG_DEBUG, "KeyRing: cb_add_new_record\n");

    flag = GPOINTER_TO_INT(data);

    if (flag == CLEAR_FLAG) {
        keyr_clear_details();
        connect_changed_signals(DISCONNECT_SIGNALS);
        set_new_button_to(NEW_FLAG);
        gtk_widget_grab_focus(GTK_WIDGET(entry_name));
        return;
    }
    if ((flag != NEW_FLAG) && (flag != MODIFY_FLAG) && (flag != COPY_FLAG)) {
        return;
    }

    kr.name = (char *) gtk_entry_get_text(GTK_ENTRY(entry_name));
    kr.account = (char *) gtk_entry_get_text(GTK_ENTRY(entry_account));
    kr.password = (char *) gtk_entry_get_text(GTK_ENTRY(entry_password));

    /* Put the glob_date in the lastChanged part of the record */
    memcpy(&(kr.last_changed), &glob_date, sizeof(struct tm));

    gtk_text_buffer_get_bounds(GTK_TEXT_BUFFER(keyr_note_buffer), &start_iter, &end_iter);
    kr.note = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(keyr_note_buffer), &start_iter, &end_iter, TRUE);

    kr.name = strdup(kr.name);
    jp_charset_j2p(kr.name, strlen(kr.name) + 1);

    kr.account = strdup(kr.account);
    jp_charset_j2p(kr.account, strlen(kr.account) + 1);

    kr.password = strdup(kr.password);
    jp_charset_j2p(kr.password, strlen(kr.password) + 1);

    jp_charset_j2p(kr.note, strlen(kr.note) + 1);

    pack_KeyRing(&kr, buf, 0xFFFF, &new_size);

    /* free allocated memory now that kr structure is packed into buf */
    if (kr.name) free(kr.name);
    if (kr.account) free(kr.account);
    if (kr.password) free(kr.password);
    if (kr.note) free(kr.note);

    /* Any attributes go here.  Usually just the category */
    /* grab category from menu */
    if (GTK_IS_WIDGET(category_menu2)) {
        br.attrib = get_selected_category_from_combo_box(GTK_COMBO_BOX(category_menu2));
    }
    jp_logf(JP_LOG_DEBUG, "category is %d\n", br.attrib);

    br.buf = buf;
    br.size = new_size;

    set_new_button_to(CLEAR_FLAG);

    /* Keep unique ID intact */
    if (flag == MODIFY_FLAG) {
        if (!mkr) {
            return;
        }
        unique_id = mkr->unique_id;

        if ((mkr->rt == DELETED_PALM_REC) ||
            (mkr->rt == DELETED_PC_REC) ||
            (mkr->rt == MODIFIED_PALM_REC)) {
            jp_logf(JP_LOG_INFO, _("You can't modify a record that is deleted\n"));
            return;
        }
    }

    /* Keep unique ID intact */
    if (flag == MODIFY_FLAG) {
        cb_delete_keyring(NULL, data);
        if ((mkr->rt == PALM_REC) || (mkr->rt == REPLACEMENT_PALM_REC)) {
            br.unique_id = unique_id;
            br.rt = REPLACEMENT_PALM_REC;
        } else {
            br.unique_id = 0;
            br.rt = NEW_PC_REC;
        }
    } else {
        br.unique_id = 0;
        br.rt = NEW_PC_REC;
    }

    /* Write out the record.  It goes to the .pc3 file until it gets synced */
    jp_pc_write("Keys-Gtkr", &br);

    keyr_update_liststore(listStore, &glob_keyring_list, keyr_category, TRUE);

    keyring_find(br.unique_id);


}

/*
 * This function is called when the user presses the "Add" button.
 * We collect all of the data from the GUI and pack it into a keyring
 * record and then write it out.
   kr->name=strdup((char *)buf);
   kr->account=strdup((char *)Pstr[0]);
   kr->password=strdup((char *)Pstr[1]);
   kr->note=strdup((char *)Pstr[2]);
 */
static void cb_add_new_record(GtkWidget *widget, gpointer data) {

    if (gtk_tree_model_iter_n_children(GTK_TREE_MODEL(listStore), NULL) != 0) {
        gtk_tree_model_foreach(GTK_TREE_MODEL(listStore), addKeyRingRecord, data);
    } else {
        //no records exist in category yet.
        addKeyRing(NULL, data);
    }


}

static void cb_date_button(GtkWidget *widget, gpointer data) {
    long fdow;
    int ret;
    struct tm temp_glob_date = glob_date;

    get_pref(PREF_FDOW, &fdow, NULL);

    /* date is not set */
    if (glob_date.tm_mon < 0) {
        /* use today date */
        time_t t = time(NULL);
        glob_date = *localtime(&t);
    }

    ret = jp_cal_dialog(GTK_WINDOW(gtk_widget_get_toplevel(widget)), "", fdow,
                        &(glob_date.tm_mon),
                        &(glob_date.tm_mday),
                        &(glob_date.tm_year));
    if (ret == CAL_DONE)
        update_date_button(date_button, &glob_date);
    else
        glob_date = temp_glob_date;
}

/* First pass at password generating code */
static void cb_gen_password(GtkWidget *widget, gpointer data) {
    GtkWidget *entry;
    int i,
            length,
            alpha_size,
            numer_size;
    char alpha[] = "abcdfghjklmnpqrstvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    char numer[] = "1234567890";
    char passwd[MAX_KR_PASS + 1];

    jp_logf(JP_LOG_DEBUG, "KeyRing: cb_gen_password\n");

    entry = data;

    srand(time(NULL) * getpid());
    alpha_size = strlen(alpha);
    numer_size = strlen(numer);

    length = rand() % (MAX_KR_PASS - MIN_KR_PASS) + MIN_KR_PASS;

    for (i = 0; i < length; i++) {
        if ((i % 2) == 0) {
            passwd[i] = alpha[rand() % alpha_size];
        } else {
            passwd[i] = numer[rand() % numer_size];
        }
    }

    passwd[length] = '\0';

    gtk_entry_set_text(GTK_ENTRY(entry), passwd);

    return;
}

/*
 * This function just adds the record to the treeView on the left side of
 * the screen.
 */
static int display_record(struct MyKeyRing *mkr, int row, GtkTreeIter *iter) {
    char temp[8];
    char *nameTxt;
    char *accountTxt;
    const char *svalue;
    char changedTxt[50];

    jp_logf(JP_LOG_DEBUG, "KeyRing: display_record\n");

    /* Highlight row background depending on status */
    GdkColor bgColor;
    gboolean showBgColor;
    switch (mkr->rt) {
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
            showBgColor = FALSE;
    }

    if (mkr->kr.last_changed.tm_year == 0) {
        sprintf(changedTxt, _("No date"));
    } else {
        get_pref(PREF_SHORTDATE, NULL, &svalue);
        strftime(changedTxt, sizeof(changedTxt), svalue, &(mkr->kr.last_changed));
    }

    if ((!(mkr->kr.name)) || (mkr->kr.name[0] == '\0')) {
        sprintf(temp, "#%03d", row);
        nameTxt = temp;
    } else {
        nameTxt = mkr->kr.name;
    }

    if ((!(mkr->kr.account)) || (mkr->kr.account[0] == '\0')) {
        accountTxt = "";
    } else {
        accountTxt = mkr->kr.account;
    }
    gtk_list_store_append(listStore, iter);
    gtk_list_store_set(listStore, iter,
                       KEYRING_CHANGED_COLUMN_ENUM, changedTxt,
                       KEYRING_NAME_COLUMN_ENUM, nameTxt,
                       KEYRING_ACCOUNT_COLUMN_ENUM, accountTxt,
                       KEYRING_DATA_COLUMN_ENUM, mkr,
                       KEYRING_BACKGROUND_COLOR_ENABLED_ENUM, showBgColor,
                       KEYRING_BACKGROUND_COLOR_ENUM, showBgColor ? &bgColor : NULL, -1);


    return EXIT_SUCCESS;
}

static int
display_record_export(GtkListStore *pListStore, struct MyKeyRing *mkr, int row, GtkTreeIter *iter) {
    char temp[8];
    char *nameTxt;

    jp_logf(JP_LOG_DEBUG, "KeyRing: display_record_export\n");

    if ((!(mkr->kr.name)) || (mkr->kr.name[0] == '\0')) {
        sprintf(temp, "#%03d", row);
        nameTxt = temp;
    } else {
        nameTxt = mkr->kr.name;
    }
    //KEYRING_CHANGED_COLUMN_ENUM
    gtk_list_store_append(pListStore, iter);
    gtk_list_store_set(pListStore, iter,
                       KEYRING_CHANGED_COLUMN_ENUM, nameTxt,
                       KEYRING_DATA_COLUMN_ENUM, mkr, -1);
    return EXIT_SUCCESS;
}

void keyr_update_liststore(GtkListStore *pListStore, struct MyKeyRing **keyring_list,
                           int category, int main) {
    GtkTreeIter iter;
    int entries_shown;
    struct MyKeyRing *temp_list;

    jp_logf(JP_LOG_DEBUG, "KeyRing: keyr_update_liststore\n");

    free_mykeyring_list(keyring_list);

    /* This function takes care of reading the database for us */
    get_keyring(keyring_list, category);

    if (main) {
        keyr_clear_details();
    }

    gtk_list_store_clear(pListStore);
    entries_shown = 0;

    for (temp_list = *keyring_list; temp_list; temp_list = temp_list->next) {

        if (main) {
            display_record(temp_list, entries_shown, &iter);
        } else {
            display_record_export(pListStore, temp_list, entries_shown, &iter);
        }
        entries_shown++;
    }
    jp_logf(JP_LOG_DEBUG, "KeyRing: leave keyr_update_liststore\n");
}

static gboolean handleKeyringRowSelection(GtkTreeSelection *selection,
                                          GtkTreeModel *model,
                                          GtkTreePath *path,
                                          gboolean path_currently_selected,
                                          gpointer userdata) {
    GtkTreeIter iter;
    struct MyKeyRing *mkr;
    int index, sorted_position;
    int b;
    unsigned int unique_id = 0;

    jp_logf(JP_LOG_DEBUG, "KeyRing: handleKeyringRowSelection\n");
    if ((gtk_tree_model_get_iter(model, &iter, path)) && (!path_currently_selected)) {
        int *i = gtk_tree_path_get_indices(path);
        row_selected = i[0];
        gtk_tree_model_get(model, &iter, KEYRING_DATA_COLUMN_ENUM, &mkr, -1);
        if ((record_changed == MODIFY_FLAG) || (record_changed == NEW_FLAG)) {


            if (mkr != NULL) {
                unique_id = mkr->unique_id;
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
                keyring_find(unique_id);
            }

            return TRUE;
        }


        if (mkr == NULL) {
            return TRUE;
        }

        if (mkr->rt == DELETED_PALM_REC ||
            (mkr->rt == DELETED_PC_REC))
            /* Possible later addition of undelete code for modified deleted records
            || mkr->rt == MODIFIED_PALM_REC
            */
        {
            set_new_button_to(UNDELETE_FLAG);
        } else {
            set_new_button_to(CLEAR_FLAG);
        }

        connect_changed_signals(DISCONNECT_SIGNALS);

        index = mkr->attrib & 0x0F;
        sorted_position = find_sort_cat_pos(index);
        int pos = findSortedPostion(sorted_position, GTK_COMBO_BOX(category_menu2));
        if (pos != sorted_position && index != 0) {
            /* Illegal category */
            jp_logf(JP_LOG_DEBUG, "Category is not legal\n");
            sorted_position = 0;
        }

        if (sorted_position < 0) {
            jp_logf(JP_LOG_WARN, _("Category is not legal\n"));
        } else {
            gtk_combo_box_set_active(GTK_COMBO_BOX(category_menu2),find_menu_cat_pos(sorted_position));
        }


        if (mkr->kr.name) {
            gtk_entry_set_text(GTK_ENTRY(entry_name), mkr->kr.name);
        } else {
            gtk_entry_set_text(GTK_ENTRY(entry_name), "");
        }

        if (mkr->kr.account) {
            gtk_entry_set_text(GTK_ENTRY(entry_account), mkr->kr.account);
        } else {
            gtk_entry_set_text(GTK_ENTRY(entry_account), "");
        }

        if (mkr->kr.password) {
            gtk_entry_set_text(GTK_ENTRY(entry_password), mkr->kr.password);
        } else {
            gtk_entry_set_text(GTK_ENTRY(entry_password), "");
        }

        memcpy(&glob_date, &(mkr->kr.last_changed), sizeof(struct tm));
        update_date_button(date_button, &(mkr->kr.last_changed));

        gtk_text_buffer_set_text(GTK_TEXT_BUFFER(keyr_note_buffer), "", -1);
        if (mkr->kr.note) {
            gtk_text_buffer_set_text(GTK_TEXT_BUFFER(keyr_note_buffer), mkr->kr.note, -1);
        }

        connect_changed_signals(CONNECT_SIGNALS);
    }
    jp_logf(JP_LOG_DEBUG, "KeyRing: leaving handleKeyringRowSelection\n");
    return TRUE;
}

static void cb_category(GtkComboBox *item, int selection) {
    int b;

    jp_logf(JP_LOG_DEBUG, "KeyRing: cb_category\n");
    if (!item) return;
    if (gtk_combo_box_get_active(GTK_COMBO_BOX(item)) < 0) {
        return;
    }
    int selectedItem = get_selected_category_from_combo_box(item);
    if (selectedItem == -1) {
        return;
    }

    if (keyr_category == selectedItem) { return; }

    b = dialog_save_changed_record_with_cancel(pane, record_changed);
    if (b == DIALOG_SAID_1) { /* Cancel */
        int index, index2;

        if (keyr_category == CATEGORY_ALL) {
            index = 0;
            index2 = 0;
        } else {
            index = find_sort_cat_pos(keyr_category);
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

    keyr_category = selectedItem;
    row_selected = 0;
    keyr_update_liststore(listStore, &glob_keyring_list, keyr_category, TRUE);

}

/***** PASSWORD GUI *****/

/*
 * Start of Dialog window code
 */
struct dialog_data {
    GtkWidget *entry;
    int button_hit;
    char text[PASSWD_LEN + 2];
};

static void cb_dialog_button(GtkWidget *widget, gpointer data) {
    struct dialog_data *Pdata;
    GtkWidget *w;

    /* Find the main window from some widget */
    w = GTK_WIDGET(gtk_widget_get_toplevel(widget));

    if (GTK_IS_WINDOW(w)) {
        Pdata = gtk_object_get_data(GTK_OBJECT(w), "dialog_data");
        if (Pdata) {
            Pdata->button_hit = GPOINTER_TO_INT(data);
        }
        gtk_widget_destroy(GTK_WIDGET(w));
    }
}

static gboolean cb_destroy_dialog(GtkWidget *widget) {
    struct dialog_data *Pdata;
    const char *entry;

    Pdata = gtk_object_get_data(GTK_OBJECT(widget), "dialog_data");
    if (!Pdata) {
        return TRUE;
    }
    entry = gtk_entry_get_text(GTK_ENTRY(Pdata->entry));

    if (entry) {
        strncpy(Pdata->text, entry, PASSWD_LEN);
        Pdata->text[PASSWD_LEN] = '\0';
        /* Clear entry field */
        gtk_entry_set_text(GTK_ENTRY(Pdata->entry), "");
    }

    gtk_main_quit();

    return TRUE;
}

/*
 * returns 2 if OK was pressed, 1 if cancel was hit
 */
static int dialog_password(GtkWindow *main_window,
                           char *ascii_password,
                           int reason) {
    GtkWidget *button, *label;
    GtkWidget *hbox1, *vbox1;
    GtkWidget *dialog;
    GtkWidget *entry;
    struct dialog_data Pdata;
    int ret;

    if (!ascii_password) {
        return EXIT_FAILURE;
    }
    ascii_password[0] = '\0';
    ret = 2;

    dialog = gtk_widget_new(GTK_TYPE_WINDOW,
                            "type", GTK_WINDOW_TOPLEVEL,
                            "title", "KeyRing",
                            NULL);

    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_MOUSE);

    gtk_signal_connect(GTK_OBJECT(dialog), "destroy",
                       G_CALLBACK(cb_destroy_dialog), dialog);

    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);

    if (main_window) {
        if (GTK_IS_WINDOW(main_window)) {
            gtk_window_set_transient_for(GTK_WINDOW(dialog),
                                         GTK_WINDOW(main_window));
        }
    }

    hbox1 = gtk_hbox_new(FALSE, 2);
    gtk_container_add(GTK_CONTAINER(dialog), hbox1);
    gtk_box_pack_start(GTK_BOX(hbox1), gtk_image_new_from_stock(GTK_STOCK_DIALOG_AUTHENTICATION, GTK_ICON_SIZE_DIALOG),
                       FALSE, FALSE, 2);

    vbox1 = gtk_vbox_new(FALSE, 2);

    gtk_container_set_border_width(GTK_CONTAINER(vbox1), 5);

    gtk_container_add(GTK_CONTAINER(hbox1), vbox1);

    hbox1 = gtk_hbox_new(TRUE, 2);
    gtk_container_set_border_width(GTK_CONTAINER(hbox1), 5);
    gtk_box_pack_start(GTK_BOX(vbox1), hbox1, FALSE, FALSE, 2);

    /* Label */
    if (reason == PASSWD_ENTER_RETRY) {
        label = gtk_label_new(_("Incorrect, Reenter KeyRing Password"));
    } else if (reason == PASSWD_ENTER_NEW) {
        label = gtk_label_new(_("Enter a NEW KeyRing Password"));
    } else {
        label = gtk_label_new(_("Enter KeyRing Password"));
    }
    gtk_box_pack_start(GTK_BOX(hbox1), label, FALSE, FALSE, 2);

    entry = gtk_entry_new_with_max_length(32);
    gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
    gtk_signal_connect(GTK_OBJECT(entry), "activate",
                       G_CALLBACK(cb_dialog_button),
                       GINT_TO_POINTER(DIALOG_SAID_2));
    gtk_box_pack_start(GTK_BOX(hbox1), entry, TRUE, TRUE, 1);

    /* Button Box */
    hbox1 = gtk_hbutton_box_new();
    gtk_button_box_set_layout(GTK_BUTTON_BOX (hbox1), GTK_BUTTONBOX_END);
    gtk_button_box_set_spacing(GTK_BUTTON_BOX(hbox1), 6);
    gtk_container_set_border_width(GTK_CONTAINER(hbox1), 5);
    gtk_box_pack_start(GTK_BOX(vbox1), hbox1, FALSE, FALSE, 2);

    /* Buttons */
    button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
    gtk_signal_connect(GTK_OBJECT(button), "clicked",
                       G_CALLBACK(cb_dialog_button),
                       GINT_TO_POINTER(DIALOG_SAID_1));
    gtk_box_pack_start(GTK_BOX(hbox1), button, FALSE, FALSE, 1);

    button = gtk_button_new_from_stock(GTK_STOCK_OK);
    gtk_signal_connect(GTK_OBJECT(button), "clicked",
                       G_CALLBACK(cb_dialog_button),
                       GINT_TO_POINTER(DIALOG_SAID_2));
    gtk_box_pack_start(GTK_BOX(hbox1), button, FALSE, FALSE, 1);

    /* Set the default button pressed to CANCEL */
    Pdata.button_hit = DIALOG_SAID_1;
    Pdata.entry = entry;
    Pdata.text[0] = '\0';

    gtk_object_set_data(GTK_OBJECT(dialog), "dialog_data", &Pdata);
    gtk_widget_grab_focus(GTK_WIDGET(entry));

    gtk_widget_show_all(dialog);

    gtk_main();

    if (Pdata.button_hit == DIALOG_SAID_1) {
        ret = 1;
    }
    if (Pdata.button_hit == DIALOG_SAID_2) {
        ret = 2;
    }
    strncpy(ascii_password, Pdata.text, PASSWD_LEN);
    memset(Pdata.text, 0, PASSWD_LEN);

    return ret;
}

/***** End Password GUI *****/

static int check_for_db(void) {
    char file[] = "Keys-Gtkr.pdb";
    char full_name[1024];
    struct stat buf;

    jp_get_home_file_name(file, full_name, sizeof(full_name));

    if (stat(full_name, &buf)) {
        jp_logf(JP_LOG_FATAL, _("KeyRing: file %s not found.\n"), full_name);
        jp_logf(JP_LOG_FATAL, _("KeyRing: Try Syncing.\n"), full_name);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

/*
 * returns EXIT_SUCCESS on password correct, 
 *         EXIT_FAILURE on password incorrect, 
 *         <0           on error
 */
static int verify_pasword(char *ascii_password) {
    GList *records;
    GList *temp_list;
    buf_rec *br;
    int password_not_correct;

    jp_logf(JP_LOG_DEBUG, "KeyRing: verify_pasword\n");

    if (check_for_db()) {
        return EXIT_FAILURE;
    }

    /* This function takes care of reading the Database for us */
    records = NULL;
    if (jp_read_DB_files("Keys-Gtkr", &records) == -1)
        return EXIT_SUCCESS;

    password_not_correct = 1;
    /* Find special record marked as password */
    for (temp_list = records; temp_list; temp_list = temp_list->next) {
        if (temp_list->data) {
            br = temp_list->data;
        } else {
            continue;
        }
        if (!br->buf) {
            continue;
        }

        if ((br->rt == DELETED_PALM_REC) || (br->rt == MODIFIED_PALM_REC)) {
            continue;
        }

        /* This record should be record 0 and is the hash-key record */
        if (br->attrib & dlpRecAttrSecret) {
            password_not_correct =
                    set_password_hash(br->buf, br->size, ascii_password);
            break;
        }
    }

    jp_free_DB_records(&records);

    if (password_not_correct)
        return EXIT_FAILURE;
    else
        return EXIT_SUCCESS;
}

#define PLUGIN_MAJOR 1
#define PLUGIN_MINOR 1

/* This is a mandatory plugin function. */
void plugin_version(int *major_version, int *minor_version) {
    *major_version = PLUGIN_MAJOR;
    *minor_version = PLUGIN_MINOR;
}

static int static_plugin_get_name(char *name, int len) {
    jp_logf(JP_LOG_DEBUG, "KeyRing: plugin_get_name\n");
    snprintf(name, len, "KeyRing %d.%d", PLUGIN_MAJOR, PLUGIN_MINOR);
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
    strncpy(name, _("KeyRing"), len);
    return EXIT_SUCCESS;
}

/*
 * This is an optional plugin function.
 * This is the name that will show up in the plugins help menu in J-Pilot.
 * If this function is used then plugin_help must be also.
 */
int plugin_get_help_name(char *name, int len) {
    g_snprintf(name, len, _("About %s"), _("KeyRing"));
    return EXIT_SUCCESS;
}

/*
 * This is an optional plugin function.
 * This is the palm database that will automatically be synced.
 */
int plugin_get_db_name(char *name, int len) {
    strncpy(name, "Keys-Gtkr", len);
    return EXIT_SUCCESS;
}

/*
 * This is a plugin callback function which provides information
 * to the user about the plugin.
 */
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
              "KeyRing plugin for J-Pilot was written by\n"
              "Judd Montgomery (c) 2001.\n"
              "judd@jpilot.org, http://jpilot.org\n"
              "\n"
              "KeyRing is a free PalmOS program for storing\n"
              "passwords and other information in encrypted form\n"
              "http://gnukeyring.sourceforge.net"
            ),
            plugin_name
    );
    *height = 0;
    *width = 0;

    return EXIT_SUCCESS;
}

/*
 * This is a plugin callback function that is executed when J-Pilot starts up.
 * base_dir is where J-Pilot is compiled to be installed at (e.g. /usr/local/)
 */
int plugin_startup(jp_startup_info *info) {
    jp_init();

    jp_logf(JP_LOG_DEBUG, "KeyRing: plugin_startup\n");
    if (info) {
        if (info->base_dir) {
            jp_logf(JP_LOG_DEBUG, "KeyRing: base_dir = [%s]\n", info->base_dir);
        }
    }
    return EXIT_SUCCESS;
}

/*
 * This is a plugin callback function that is executed before a sync occurs.
 * Any sync preperation can be done here.
 */
int plugin_pre_sync(void) {
    jp_logf(JP_LOG_DEBUG, "KeyRing: plugin_pre_sync\n");
    return EXIT_SUCCESS;
}

/*
 * This is a plugin callback function that is executed during a sync.
 * Notice that I don't need to sync the KeyRing application.  Since I used
 * the plugin_get_db_name call to tell J-Pilot what to sync for me.  It will
 * be done automatically.
 */
int plugin_sync(int sd) {
    jp_logf(JP_LOG_DEBUG, "KeyRing: plugin_sync\n");
    return EXIT_SUCCESS;
}

/*
 * This is a plugin callback function called after a sync.
 */
int plugin_post_sync(void) {
    jp_logf(JP_LOG_DEBUG, "KeyRing: plugin_post_sync\n");
    return EXIT_SUCCESS;
}

static int add_search_result(const char *line,
                             int unique_id,
                             struct search_result **sr) {
    struct search_result *new_sr;

    jp_logf(JP_LOG_DEBUG, "KeyRing: add_search_result for [%s]\n", line);

    new_sr = malloc(sizeof(struct search_result));
    if (!new_sr) {
        return EXIT_FAILURE;
    }
    new_sr->unique_id = unique_id;
    new_sr->line = strdup(line);
    new_sr->next = *sr;
    *sr = new_sr;

    return EXIT_SUCCESS;
}

/*
 * This function is called when the user does a search.  It should return
 * records which match the search string.
 */
int plugin_search(const char *search_string, int case_sense,
                  struct search_result **sr) {
    struct MyKeyRing *mkr_list;
    struct MyKeyRing *temp_list;
    struct MyKeyRing mkr;
    int num, count;
    char *line;

    jp_logf(JP_LOG_DEBUG, "KeyRing: plugin_search\n");

    *sr = NULL;
    mkr_list = NULL;

    if (!plugin_active) {
        return 0;
    }

    /* This function takes care of reading the Database for us */
    num = get_keyring(&mkr_list, CATEGORY_ALL);
    if (-1 == num)
        return 0;

    count = 0;

    /* Search through returned records */
    for (temp_list = mkr_list; temp_list; temp_list = temp_list->next) {
        mkr = *temp_list;
        line = NULL;

        /* find in record name */
        if (jp_strstr(mkr.kr.name, search_string, case_sense))
            line = mkr.kr.name;

        /* find in record account */
        if (jp_strstr(mkr.kr.account, search_string, case_sense))
            line = mkr.kr.account;

        /* find in record password */
        if (jp_strstr(mkr.kr.password, search_string, case_sense))
            line = mkr.kr.password;

        /* find in record note */
        if (jp_strstr(mkr.kr.note, search_string, case_sense))
            line = mkr.kr.note;

        if (line) {
            /* Add it to our result list */
            jp_logf(JP_LOG_DEBUG, "KeyRing: calling add_search_result\n");
            add_search_result(line, mkr.unique_id, sr);
            jp_logf(JP_LOG_DEBUG, "KeyRing: back from add_search_result\n");
            count++;
        }
    }

    free_mykeyring_list(&mkr_list);

    return count;
}

gboolean
findKeyRingRecord(GtkTreeModel *model,
                  GtkTreePath *path,
                  GtkTreeIter *iter,
                  gpointer data) {
    int uniqueId = GPOINTER_TO_INT(data);
    if (uniqueId) {
        struct MyKeyRing *mkr = NULL;

        gtk_tree_model_get(model, iter, KEYRING_DATA_COLUMN_ENUM, &mkr, -1);
        if (mkr->unique_id == uniqueId) {
            GtkTreeSelection *selection = NULL;
            selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeView));
            gtk_tree_selection_select_path(selection, path);
            gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(treeView), path, KEYRING_CHANGED_COLUMN_ENUM, FALSE, 1.0, 0.0);
            return TRUE;
        }
    }
    return FALSE;
}

static int keyring_find(int unique_id) {
    gtk_tree_model_foreach(GTK_TREE_MODEL(listStore), findKeyRingRecord, GINT_TO_POINTER(unique_id));
    return EXIT_SUCCESS;
}

static void cb_keyr_update_listStore(GtkWidget *treeView, int category) {
    keyr_update_liststore(GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(treeView))), &export_keyring_list,
                          category, FALSE);
}

static void cb_keyr_export_done(GtkWidget *widget, const char *filename) {
    free_mykeyring_list(&export_keyring_list);

    set_pref(PREF_KEYR_EXPORT_FILENAME, 0, filename, TRUE);
}

static void cb_keyr_export_ok(GtkWidget *export_window, GtkWidget *treeView,
                              int type, const char *filename) {
    struct MyKeyRing *mkr;
    GList *list, *temp_list;
    FILE *out;
    struct stat statb;
    int i, r;
    const char *short_date;
    time_t ltime;
    struct tm *now;
    char *button_text[] = {N_("OK")};
    char *button_overwrite_text[] = {N_("No"), N_("Yes")};
    char *button_keepassx_text[] = {N_("Cancel"), N_("Overwrite"), N_("Append")};
    enum {
        NA = 0, cancel = DIALOG_SAID_1, overwrite = DIALOG_SAID_2, append = DIALOG_SAID_3
    } keepassx_answer = NA;
    char text[1024];
    char str1[256], str2[256];
    char date_string[1024];
    char pref_time[40];
    char csv_text[65550];
    long char_set;
    char *utf;
    int cat;

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
        if (type == EXPORT_TYPE_KEEPASSX) {
            g_snprintf(text, sizeof(text), _("KeePassX XML File exists, Do you want to"));
            keepassx_answer = dialog_generic(GTK_WINDOW(export_window),
                                             _("Overwrite File?"),
                                             DIALOG_ERROR, text, 3, button_keepassx_text);
            if (keepassx_answer == cancel) {
                return;
            }
        } else {
            g_snprintf(text, sizeof(text), _("Do you want to overwrite file %s?"), filename);
            r = dialog_generic(GTK_WINDOW(export_window),
                               _("Overwrite File?"),
                               DIALOG_ERROR, text, 2, button_overwrite_text);
            if (r != DIALOG_SAID_2) {
                return;
            }
        }
    }

    if ((keepassx_answer == append)) {
        out = fopen(filename, "r+");
    } else {
        out = fopen(filename, "w");
    }
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
        fprintf(out, _("Keys exported from %s %s on %s\n\n"),
                PN, VERSION, date_string);
    }

    /* Write a header to the CSV file */
    if (type == EXPORT_TYPE_CSV) {
        fprintf(out, "\"Category\",\"Name\",\"Account\",\"Password\",\"Note\"\n");
    }

    /* Write a header to the B-Folders CSV file */
    if (type == EXPORT_TYPE_BFOLDERS) {
        fprintf(out, "Login passwords:\n");
        fprintf(out, "Title,Location,Usename,Password, "
                     "\"Custom Label 1\",\"Custom Value 1\",\"Custom Label 2\",\"Custom Value 2\","
                     "\"Custom Label 3\",\"Custom Value 3\",\"Custom Label 4\",\"Custom Value 4\","
                     "\"Custom Label 5\",\"Custom Value 5\", Note,Folder\n");
    }

    if (type == EXPORT_TYPE_KEEPASSX) {
        if (keepassx_answer != append) {
            /* Write a database header to the KeePassX XML file */
            /* If we append to an XML file we don't need another header */
            fprintf(out, "<!DOCTYPE KEEPASSX_DATABASE>\n");
            fprintf(out, "<database>\n");
        } else {
            /* We'll need to remove the last part of the XML file */
            fseek(out, -12L, SEEK_END);
            fread(text, 11, 1, out);
            text[11] = '\0';
            if (strncmp(text, "</database>", 11)) {
                jp_logf(JP_LOG_WARN, _("This doesn't look like a KeePassX XML file\n"));
                fseek(out, 0L, SEEK_END);
            } else {
                fseek(out, -12L, SEEK_END);
            }
        }
        /* Write a group header to the KeePassX XML file */
        fprintf(out, " <group>\n");
        fprintf(out, "  <title>Keyring</title>\n");
        fprintf(out, "  <icon>0</icon>\n");
    }

    get_pref(PREF_CHAR_SET, &char_set, NULL);
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeView));
    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeView));
    list = gtk_tree_selection_get_selected_rows(selection, &model);

    for (i = 0, temp_list = list; temp_list; temp_list = temp_list->next, i++) {
        GtkTreePath *path = temp_list->data;
        GtkTreeIter iter;
        if (gtk_tree_model_get_iter(model, &iter, path)) {
            gtk_tree_model_get(model, &iter, KEYRING_DATA_COLUMN_ENUM, &mkr, -1);
            if (!mkr) {
                continue;
                jp_logf(JP_LOG_WARN, _("Can't export key %d\n"), (long) temp_list->data + 1);
            }
            switch (type) {
                case EXPORT_TYPE_CSV:
                    utf = charset_p2newj(keyr_app_info.name[mkr->attrib & 0x0F], 16, char_set);
                    fprintf(out, "\"%s\",", utf);
                    g_free(utf);
                    str_to_csv_str(csv_text, mkr->kr.name);
                    fprintf(out, "\"%s\",", csv_text);
                    str_to_csv_str(csv_text, mkr->kr.account);
                    fprintf(out, "\"%s\",", csv_text);
                    str_to_csv_str(csv_text, mkr->kr.password);
                    fprintf(out, "\"%s\",", csv_text);
                    str_to_csv_str(csv_text, mkr->kr.note);
                    fprintf(out, "\"%s\"\n", csv_text);
                    break;

                case EXPORT_TYPE_BFOLDERS:
                    str_to_csv_str(csv_text, mkr->kr.name);
                    fprintf(out, "\"%s\",", csv_text);

                    fprintf(out, "\"\",");

                    str_to_csv_str(csv_text, mkr->kr.account);
                    fprintf(out, "\"%s\",", csv_text);
                    str_to_csv_str(csv_text, mkr->kr.password);
                    fprintf(out, "\"%s\",", csv_text);

                    fprintf(out, "\"\",\"\",\"\",\"\","
                                 "\"\",\"\",\"\",\"\","
                                 "\"\",\"\",");

                    str_to_csv_str(csv_text, mkr->kr.note);
                    fprintf(out, "\"%s\",", csv_text);

                    fprintf(out, "\"KeyRing > ");

                    utf = charset_p2newj(keyr_app_info.name[mkr->attrib & 0x0F], 16, char_set);
                    fprintf(out, "%s\"\n", utf);
                    g_free(utf);

                    break;

                case EXPORT_TYPE_TEXT:
                    fprintf(out, "#%d\n", i + 1);
                    fprintf(out, "Name: %s\n", mkr->kr.name);
                    fprintf(out, "Account: %s\n", mkr->kr.account);
                    fprintf(out, "Password: %s\n", mkr->kr.password);
                    fprintf(out, "Note: %s\n", mkr->kr.note);
                    break;

                case EXPORT_TYPE_KEEPASSX:
                    break;

                default:
                    jp_logf(JP_LOG_WARN, _("Unknown export type\n"));
            }
        }
    }

    /* I'm writing a second loop for the KeePassX XML file because I want to
     * put each category into a folder and we need to write the tag for a folder
     * and then find each record in that category/folder
     */
    if (type == EXPORT_TYPE_KEEPASSX) {
        for (cat = 0; cat < 16; cat++) {
            if (keyr_app_info.name[cat][0] == '\0') {
                continue;
            }
            /* Write a folder XML tag */
            utf = charset_p2newj(keyr_app_info.name[cat], 16, char_set);
            fprintf(out, "  <group>\n");
            fprintf(out, "   <title>%s</title>\n", utf);
            fprintf(out, "   <icon>13</icon>\n");
            g_free(utf);

            for (i = 0, temp_list = list; temp_list; temp_list = temp_list->next, i++) {
                GtkTreePath *path = temp_list->data;
                GtkTreeIter iter;
                if (gtk_tree_model_get_iter(model, &iter, path)) {
                    gtk_tree_model_get(model, &iter, KEYRING_DATA_COLUMN_ENUM, &mkr, -1);
                    if (!mkr) {
                        continue;
                        jp_logf(JP_LOG_WARN, _("Can't export key %d\n"), (long) temp_list->data + 1);
                    }
                    if ((mkr->attrib & 0x0F) != cat) {
                        continue;
                    }
                    fprintf(out, "   <entry>\n");
                    str_to_keepass_str(csv_text, mkr->kr.name);
                    fprintf(out, "    <title>%s</title>\n", csv_text);
                    str_to_keepass_str(csv_text, mkr->kr.account);
                    fprintf(out, "    <username>%s</username>\n", csv_text);
                    str_to_keepass_str(csv_text, mkr->kr.password);
                    fprintf(out, "    <password>%s</password>\n", csv_text);
                    /* No keyring field for url */
                    str_to_keepass_str(csv_text, mkr->kr.note);
                    fprintf(out, "    <comment>%s</comment>\n", csv_text);
                    fprintf(out, "    <icon>0</icon>\n");
                    /* No keyring field for creation */
                    /* No keyring field for lastaccess */
                    /* lastmod */
                    strftime(str1, sizeof(str1), "%Y-%m-%dT%H:%M:%S", &(mkr->kr.last_changed));
                    fprintf(out, "    <lastmod>%s</lastmod>\n", str1);
                    /* No keyring field for expire */
                    fprintf(out, "    <expire>Never</expire>\n");
                    fprintf(out, "   </entry>\n");
                }
                fprintf(out, "  </group>\n");
            }
        }

        /* Write a footer to the KeePassX XML file */
        if (type == EXPORT_TYPE_KEEPASSX) {
            fprintf(out, " </group>\n");
            fprintf(out, "</database>\n");
        }
    }

    if (out) {
        fclose(out);
    }
}

/*
 * This is a plugin callback function to export records.
 */
int plugin_export(GtkWidget *window) {
    int w, h, x, y;
    char *type_text[] = {N_("Text"), N_("CSV"), N_("B-Folders CSV"), N_("KeePassX XML"), NULL};
    int type_int[] = {EXPORT_TYPE_TEXT, EXPORT_TYPE_CSV, EXPORT_TYPE_BFOLDERS, EXPORT_TYPE_KEEPASSX};

    gdk_window_get_size(gtk_widget_get_window(window), &w, &h);
    gdk_window_get_root_origin(gtk_widget_get_window(window), &x, &y);

    w = gtk_paned_get_position(GTK_PANED(pane));
    x += 40;

    export_gui(window,
               w, h, x, y, 1, sort_l,
               PREF_KEYR_EXPORT_FILENAME,
               type_text,
               type_int,
               cb_keyr_export_init_treeView,
               cb_keyr_update_listStore,
               cb_keyr_export_done,
               cb_keyr_export_ok
    );

    return EXIT_SUCCESS;
}

static GtkWidget *cb_keyr_export_init_treeView() {
    GtkListStore *listStore = gtk_list_store_new(KEYRING_NUM_COLS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                                                 G_TYPE_POINTER, GDK_TYPE_COLOR, G_TYPE_BOOLEAN, G_TYPE_STRING,
                                                 G_TYPE_BOOLEAN);
    GtkTreeModel *model = GTK_TREE_MODEL(listStore);
    GtkTreeView *keyr_treeView = gtk_tree_view_new_with_model(model);
    GtkCellRenderer *changedRenderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *changedColumn = gtk_tree_view_column_new_with_attributes("Changed",
                                                                                changedRenderer,
                                                                                "text", KEYRING_CHANGED_COLUMN_ENUM,
                                                                                "cell-background-gdk",
                                                                                KEYRING_BACKGROUND_COLOR_ENUM,
                                                                                "cell-background-set",
                                                                                KEYRING_BACKGROUND_COLOR_ENABLED_ENUM,
                                                                                NULL);
    gtk_tree_view_column_set_sort_column_id(changedColumn, KEYRING_CHANGED_COLUMN_ENUM);
    GtkCellRenderer *nameRenderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *nameColumn = gtk_tree_view_column_new_with_attributes("Name",
                                                                             nameRenderer,
                                                                             "text", KEYRING_NAME_COLUMN_ENUM,
                                                                             "cell-background-gdk",
                                                                             KEYRING_BACKGROUND_COLOR_ENUM,
                                                                             "cell-background-set",
                                                                             KEYRING_BACKGROUND_COLOR_ENABLED_ENUM,
                                                                             NULL);
    gtk_tree_view_column_set_sort_column_id(nameColumn, KEYRING_NAME_COLUMN_ENUM);
    GtkCellRenderer *accountRenderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *accountColumn = gtk_tree_view_column_new_with_attributes("Account",
                                                                                accountRenderer,
                                                                                "text", KEYRING_ACCOUNT_COLUMN_ENUM,
                                                                                "cell-background-gdk",
                                                                                KEYRING_BACKGROUND_COLOR_ENUM,
                                                                                "cell-background-set",
                                                                                KEYRING_BACKGROUND_COLOR_ENABLED_ENUM,
                                                                                NULL);
    gtk_tree_view_column_set_sort_column_id(accountColumn, KEYRING_ACCOUNT_COLUMN_ENUM);
    gtk_tree_view_insert_column(GTK_TREE_VIEW(keyr_treeView), changedColumn, KEYRING_CHANGED_COLUMN_ENUM);
    gtk_tree_view_insert_column(GTK_TREE_VIEW(keyr_treeView), nameColumn, KEYRING_NAME_COLUMN_ENUM);
    gtk_tree_view_insert_column(GTK_TREE_VIEW(keyr_treeView), accountColumn, KEYRING_ACCOUNT_COLUMN_ENUM);
    gtk_tree_view_column_set_sizing(changedColumn, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    gtk_tree_view_column_set_sizing(nameColumn, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    gtk_tree_view_column_set_sizing(accountColumn, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    return GTK_WIDGET(keyr_treeView);
}

/*
 * This is a plugin callback function called during Jpilot program exit.
 */
int plugin_exit_cleanup(void) {
    jp_logf(JP_LOG_DEBUG, "KeyRing: plugin_exit_cleanup\n");
    return EXIT_SUCCESS;
}

/*
 * This is a plugin callback function called when the plugin is terminated
 * such as by switching to another application(ToDo, Memo, etc.)
 */
int plugin_gui_cleanup(void) {
    int b;

    jp_logf(JP_LOG_DEBUG, "KeyRing: plugin_gui_cleanup\n");

    b = dialog_save_changed_record(GTK_WIDGET(treeView), record_changed);
    if (b == DIALOG_SAID_2) {
        cb_add_new_record(NULL, GINT_TO_POINTER(record_changed));
    }

    connect_changed_signals(DISCONNECT_SIGNALS);

    free_mykeyring_list(&glob_keyring_list);

    /* if the password was correct */
    if (plugin_last_time && (TRUE == plugin_active)) {
        plugin_last_time = time(NULL);
    }
    plugin_active = FALSE;

    /* the pane may not exist if the wrong password is entered and
     * the GUI was not built */
    if (pane) {
        /* Remove the accelerators */
#ifndef ENABLE_STOCK_BUTTONS
        gtk_window_remove_accel_group(GTK_WINDOW(gtk_widget_get_toplevel(pane)), accel_group);
#endif

        /* Record the position of the window pane to restore later */
        set_pref(PREF_KEYRING_PANE, gtk_paned_get_position(GTK_PANED(pane)), NULL, TRUE);

        pane = NULL;

        gtk_list_store_clear(listStore);
    }

    return EXIT_SUCCESS;
}

static void column_clicked_cb(GtkTreeViewColumn *column) {
    column_selected = gtk_tree_view_column_get_sort_column_id(column);

}

/*
 * This function is called by J-Pilot when the user selects this plugin
 * from the plugin menu, or from the search window when a search result
 * record is chosen.  In the latter case, unique ID will be set.  This
 * application should go directly to that record in the case.
 */
int plugin_gui(GtkWidget *vbox, GtkWidget *hbox, unsigned int unique_id) {
    GtkWidget *vbox1, *vbox2;
    GtkWidget *hbox_temp;
    GtkWidget *button;
    GtkWidget *label;
    GtkWidget *table;
    GtkWindow *w;
    GtkWidget *separator;
    long ivalue;
    char ascii_password[PASSWD_LEN];
    int r;
    int password_not_correct;
    char *titles[3]; /* { "Changed", "Name", "Account" }; */
    int retry;
    int cycle_category = FALSE;
    long char_set;
    long show_tooltips;
    char *cat_name;
    int new_cat;
    int index, index2;
    int i;
#ifdef HAVE_LIBGCRYPT
    static int gcrypt_init = 0;
#endif

    jp_logf(JP_LOG_DEBUG, "KeyRing: plugin gui started, unique_id=%d\n", unique_id);

    if (check_for_db()) {
        return EXIT_FAILURE;
    }

#ifdef HAVE_LIBGCRYPT
    if (!gcrypt_init) {
        gcrypt_init = 1;

        /* Version check should be the very first call because it
           makes sure that important subsystems are intialized. */
        if (!gcry_check_version(GCRYPT_VERSION)) {
            fputs("libgcrypt version mismatch\n", stderr);
            return EXIT_FAILURE;
        }

        /* We don't want to see any warnings, e.g. because we have not yet
           parsed program options which might be used to suppress such
           warnings. */
        gcry_control(GCRYCTL_SUSPEND_SECMEM_WARN);

        /* ... If required, other initialization goes here.  Note that the
           process might still be running with increased privileges and that
           the secure memory has not been intialized.  */

        /* Allocate a pool of 16k secure memory.  This make the secure memory
           available and also drops privileges where needed.  */
        gcry_control(GCRYCTL_INIT_SECMEM, 16384, 0);

        /* It is now okay to let Libgcrypt complain when there was/is
           a problem with the secure memory. */
        gcry_control(GCRYCTL_RESUME_SECMEM_WARN);

        /* ... If required, other initialization goes here.  */

        /* Tell Libgcrypt that initialization has completed. */
        gcry_control(GCRYCTL_INITIALIZATION_FINISHED, 0);
    }
#endif

    /* Find the main window from some widget */
    w = GTK_WINDOW(gtk_widget_get_toplevel(hbox));

#if 0
    /* Change Password button */
    button = gtk_button_new_with_label(_("Change\nKeyRing\nPassword"));
    gtk_signal_connect(GTK_OBJECT(button), "clicked",
                       G_CALLBACK(cb_change_password), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), button, TRUE, TRUE, 0);
#endif

    if (difftime(time(NULL), plugin_last_time) > PLUGIN_MAX_INACTIVE_TIME) {
        /* reset last time we entered */
        plugin_last_time = 0;

        password_not_correct = TRUE;
        retry = PASSWD_ENTER;
        while (password_not_correct) {
            r = dialog_password(w, ascii_password, retry);
            retry = PASSWD_ENTER_RETRY;
            if (r != 2) {
                memset(ascii_password, 0, PASSWD_LEN - 1);
                return 0;
            }
            password_not_correct = (verify_pasword(ascii_password) > 0);
        }
        memset(ascii_password, 0, PASSWD_LEN - 1);
    } else {
        cycle_category = TRUE;
    }

    /* called to display the result of a search */
    if (unique_id) {
        cycle_category = FALSE;
    }

    /* plugin entered with correct password */
    plugin_last_time = time(NULL);
    plugin_active = TRUE;

    /************************************************************/
    /* Build GUI */
    record_changed = CLEAR_FLAG;
    row_selected = 0;

    /* Do some initialization */
    if (category_menu2 && category_menu2 != NULL) {
        GtkTreeModel *clearingmodel = gtk_combo_box_get_model(GTK_COMBO_BOX(category_menu2));
        gtk_list_store_clear(GTK_LIST_STORE(clearingmodel));
    }

    get_keyr_cat_info(&keyr_app_info);
    get_pref(PREF_CHAR_SET, &char_set, NULL);

    for (i = 1; i < NUM_KEYRING_CAT_ITEMS; i++) {
        cat_name = charset_p2newj(keyr_app_info.name[i], 31, char_set);
        strcpy(sort_l[i - 1].Pcat, cat_name);
        free(cat_name);
        sort_l[i - 1].cat_num = i;
    }
    /* put reserved 'Unfiled' category at end of list */
    cat_name = charset_p2newj(keyr_app_info.name[0], 31, char_set);
    strcpy(sort_l[NUM_KEYRING_CAT_ITEMS - 1].Pcat, cat_name);
    free(cat_name);
    sort_l[NUM_KEYRING_CAT_ITEMS - 1].cat_num = 0;

    qsort(sort_l, NUM_KEYRING_CAT_ITEMS - 1, sizeof(struct sorted_cats), cat_compare);

#ifdef JPILOT_DEBUG
    for (i=0; i<NUM_KEYRING_CAT_ITEMS; i++) {
       printf("cat %d [%s]\n", sort_l[i].cat_num, sort_l[i].Pcat);
    }
#endif

    if (keyr_category > NUM_KEYRING_CAT_ITEMS) {
        keyr_category = CATEGORY_ALL;
    }

    /* Make accelerators for some buttons window */
#ifndef ENABLE_STOCK_BUTTONS
    accel_group = gtk_accel_group_new();
    gtk_window_add_accel_group(GTK_WINDOW(gtk_widget_get_toplevel(vbox)), accel_group);
#endif
    get_pref(PREF_SHOW_TOOLTIPS, &show_tooltips, NULL);

    pane = gtk_hpaned_new();
    get_pref(PREF_KEYRING_PANE, &ivalue, NULL);
    gtk_paned_set_position(GTK_PANED(pane), ivalue);

    gtk_box_pack_start(GTK_BOX(hbox), pane, TRUE, TRUE, 5);

    /* left and right main boxes */
    vbox1 = gtk_vbox_new(FALSE, 0);
    vbox2 = gtk_vbox_new(FALSE, 0);
    gtk_paned_pack1(GTK_PANED(pane), vbox1, TRUE, FALSE);
    gtk_paned_pack2(GTK_PANED(pane), vbox2, TRUE, FALSE);

    gtk_widget_set_usize(GTK_WIDGET(vbox1), 0, 230);
    gtk_widget_set_usize(GTK_WIDGET(vbox2), 0, 230);

    /**********************************************************************/
    /* Left half of screen */
    /**********************************************************************/
    /* Left-side Category menu */
    hbox_temp = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox1), hbox_temp, FALSE, FALSE, 0);

    make_category_menu(&category_menu1,
                       sort_l, cb_category, TRUE, FALSE);
    gtk_box_pack_start(GTK_BOX(hbox_temp), category_menu1, TRUE, TRUE, 0);

    /* Scrolled window */
    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_set_border_width(GTK_CONTAINER(scrolled_window), 0);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(vbox1), scrolled_window, TRUE, TRUE, 0);

    /* listStore */
    listStore = gtk_list_store_new(KEYRING_NUM_COLS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                                   G_TYPE_POINTER, GDK_TYPE_COLOR, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_BOOLEAN);
    titles[0] = _("Changed");
    titles[1] = _("Name");
    titles[2] = _("Account");
    GtkTreeModel *model = GTK_TREE_MODEL(listStore);
    treeView = gtk_tree_view_new_with_model(model);
    GtkCellRenderer *changedRenderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *changedColumn = gtk_tree_view_column_new_with_attributes("Changed",
                                                                                changedRenderer,
                                                                                "text", KEYRING_CHANGED_COLUMN_ENUM,
                                                                                "cell-background-gdk",
                                                                                KEYRING_BACKGROUND_COLOR_ENUM,
                                                                                "cell-background-set",
                                                                                KEYRING_BACKGROUND_COLOR_ENABLED_ENUM,
                                                                                NULL);
    gtk_tree_view_column_set_sort_column_id(changedColumn, KEYRING_CHANGED_COLUMN_ENUM);
    GtkCellRenderer *nameRenderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *nameColumn = gtk_tree_view_column_new_with_attributes("Name",
                                                                             nameRenderer,
                                                                             "text", KEYRING_NAME_COLUMN_ENUM,
                                                                             "cell-background-gdk",
                                                                             KEYRING_BACKGROUND_COLOR_ENUM,
                                                                             "cell-background-set",
                                                                             KEYRING_BACKGROUND_COLOR_ENABLED_ENUM,
                                                                             NULL);
    gtk_tree_view_column_set_sort_column_id(nameColumn, KEYRING_NAME_COLUMN_ENUM);
    GtkCellRenderer *accountRenderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *accountColumn = gtk_tree_view_column_new_with_attributes("Account",
                                                                                accountRenderer,
                                                                                "text", KEYRING_ACCOUNT_COLUMN_ENUM,
                                                                                "cell-background-gdk",
                                                                                KEYRING_BACKGROUND_COLOR_ENUM,
                                                                                "cell-background-set",
                                                                                KEYRING_BACKGROUND_COLOR_ENABLED_ENUM,
                                                                                NULL);
    gtk_tree_view_column_set_sort_column_id(accountColumn, KEYRING_ACCOUNT_COLUMN_ENUM);
    gtk_tree_view_insert_column(GTK_TREE_VIEW(treeView), changedColumn, KEYRING_CHANGED_COLUMN_ENUM);
    gtk_tree_view_insert_column(GTK_TREE_VIEW(treeView), nameColumn, KEYRING_NAME_COLUMN_ENUM);
    gtk_tree_view_insert_column(GTK_TREE_VIEW(treeView), accountColumn, KEYRING_ACCOUNT_COLUMN_ENUM);
    gtk_tree_view_column_set_clickable(changedColumn, gtk_true());
    gtk_tree_view_column_set_clickable(nameColumn, gtk_true());
    gtk_tree_view_column_set_clickable(accountColumn, gtk_true());
    gtk_tree_view_column_set_sizing(changedColumn, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    gtk_tree_view_column_set_fixed_width(nameColumn, 150);
    gtk_tree_view_column_set_sizing(accountColumn, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
    gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(treeView)),
                                GTK_SELECTION_BROWSE);
    GtkTreeSortable *sortable = GTK_TREE_SORTABLE(listStore);
    gtk_tree_sortable_set_sort_func(sortable, KEYRING_CHANGED_COLUMN_ENUM, GtkTreeModelKeyrCompareDates,
                                    GINT_TO_POINTER(KEYRING_CHANGED_COLUMN_ENUM), NULL);
    gtk_tree_sortable_set_sort_func(sortable, KEYRING_NAME_COLUMN_ENUM, GtkTreeModelKeyrCompareNocase,
                                    GINT_TO_POINTER(KEYRING_NAME_COLUMN_ENUM), NULL);
    for (int x = 0; x < KEYRING_NUM_COLS - 5; x++) {
        gtk_tree_view_column_set_sort_indicator(gtk_tree_view_get_column(GTK_TREE_VIEW(treeView), x), gtk_false());
    }
    gtk_tree_view_column_set_sort_indicator(gtk_tree_view_get_column(GTK_TREE_VIEW(treeView), column_selected),
                                            gtk_true());

    g_signal_connect (changedColumn, "clicked", G_CALLBACK(column_clicked_cb), NULL);
    g_signal_connect (nameColumn, "clicked", G_CALLBACK(column_clicked_cb), NULL);
    g_signal_connect (accountColumn, "clicked", G_CALLBACK(column_clicked_cb), NULL);

    GtkTreeSelection *treeSelection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeView));

    gtk_tree_selection_set_select_function(treeSelection, handleKeyringRowSelection, NULL, NULL);

    gtk_container_add(GTK_CONTAINER(scrolled_window), GTK_WIDGET(treeView));

    /**********************************************************************/
    /* Right half of screen */
    /**********************************************************************/
    hbox_temp = gtk_hbox_new(FALSE, 3);
    gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, FALSE, FALSE, 0);

    /* Cancel button */
    CREATE_BUTTON(cancel_record_button, _("Cancel"), CANCEL, _("Cancel the modifications"), GDK_KEY_Escape, 0, "ESC")
    gtk_signal_connect(GTK_OBJECT(cancel_record_button), "clicked",
                       G_CALLBACK(cb_cancel), NULL);

    /* Delete button */
    CREATE_BUTTON(delete_record_button, _("Delete"), DELETE, _("Delete the selected record"), GDK_d, GDK_CONTROL_MASK,
                  "Ctrl+D");
    gtk_signal_connect(GTK_OBJECT(delete_record_button), "clicked",
                       G_CALLBACK(cb_delete_keyring),
                       GINT_TO_POINTER(DELETE_FLAG));

    /* Undelete button */
    CREATE_BUTTON(undelete_record_button, _("Undelete"), UNDELETE, _("Undelete the selected record"), 0, 0, "")
    gtk_signal_connect(GTK_OBJECT(undelete_record_button), "clicked",
                       G_CALLBACK(cb_undelete_keyring),
                       GINT_TO_POINTER(UNDELETE_FLAG));

    /* Copy button */
    CREATE_BUTTON(copy_record_button, _("Copy"), COPY, _("Copy the selected record"), GDK_c,
                  GDK_CONTROL_MASK | GDK_SHIFT_MASK, "Ctrl+Shift+C")
    gtk_signal_connect(GTK_OBJECT(copy_record_button), "clicked",
                       G_CALLBACK(cb_add_new_record),
                       GINT_TO_POINTER(COPY_FLAG));

    /* New Record button */
    CREATE_BUTTON(new_record_button, _("New Record"), NEW, _("Add a new record"), GDK_n, GDK_CONTROL_MASK, "Ctrl+N")
    gtk_signal_connect(GTK_OBJECT(new_record_button), "clicked",
                       G_CALLBACK(cb_add_new_record),
                       GINT_TO_POINTER(CLEAR_FLAG));

    /* Add Record button */
    CREATE_BUTTON(add_record_button, _("Add Record"), ADD, _("Add the new record"), GDK_KEY_Return, GDK_CONTROL_MASK,
                  "Ctrl+Enter")
    gtk_signal_connect(GTK_OBJECT(add_record_button), "clicked",
                       G_CALLBACK(cb_add_new_record),
                       GINT_TO_POINTER(NEW_FLAG));
#ifndef ENABLE_STOCK_BUTTONS
    gtk_widget_set_name(GTK_WIDGET(GTK_LABEL(gtk_bin_get_child(GTK_BIN(add_record_button)))), "label_high");
#endif

    /* Apply Changes button */
    CREATE_BUTTON(apply_record_button, _("Apply Changes"), APPLY, _("Commit the modifications"), GDK_KEY_Return,
                  GDK_CONTROL_MASK, "Ctrl+Enter")
    gtk_signal_connect(GTK_OBJECT(apply_record_button), "clicked",
                       G_CALLBACK(cb_add_new_record),
                       GINT_TO_POINTER(MODIFY_FLAG));
#ifndef ENABLE_STOCK_BUTTONS
    gtk_widget_set_name(GTK_WIDGET(GTK_LABEL(gtk_bin_get_child(GTK_BIN(apply_record_button)))), "label_high");
#endif

    /* Separator */
    separator = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(vbox2), separator, FALSE, FALSE, 5);

    /* Table */
    table = gtk_table_new(5, 10, FALSE);
    gtk_table_set_row_spacings(GTK_TABLE(table), 0);
    gtk_table_set_col_spacings(GTK_TABLE(table), 0);
    gtk_box_pack_start(GTK_BOX(vbox2), table, FALSE, FALSE, 0);

    /* Category menu */
    label = gtk_label_new(_("Category: "));
    gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(label), 0, 1, 0, 1);
    make_category_menu(&category_menu2,
                       sort_l, NULL, FALSE, FALSE);
    gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(category_menu2), 1, 10, 0, 1);
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);

    /* Name entry */
    label = gtk_label_new(_("name: "));
    entry_name = gtk_entry_new();
    entry_set_multiline_truncate(GTK_ENTRY(entry_name), TRUE);
    gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(label), 0, 1, 1, 2);
    gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(entry_name), 1, 10, 1, 2);
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);

    /* Account entry */
    label = gtk_label_new(_("account: "));
    entry_account = gtk_entry_new();
    entry_set_multiline_truncate(GTK_ENTRY(entry_account), TRUE);
    gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(label), 0, 1, 2, 3);
    gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(entry_account), 1, 10, 2, 3);
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);

    /* Password entry */
    label = gtk_label_new(_("password: "));
    entry_password = gtk_entry_new();
    gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(label), 0, 1, 3, 4);
    gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(entry_password), 1, 9, 3, 4);
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);

    /* Last Changed entry */
    label = gtk_label_new(_("last changed: "));
    gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(label), 0, 1, 4, 5);
    gtk_misc_set_alignment(GTK_MISC(label), 1.0, 0.5);

    date_button = gtk_button_new_with_label("");
    gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(date_button), 1, 10, 4, 5);
    gtk_signal_connect(GTK_OBJECT(date_button), "clicked",
                       G_CALLBACK(cb_date_button), date_button);

    /* Generate Password button (creates random password) */
    button = gtk_button_new_with_label(_("Generate Password"));
    gtk_table_attach_defaults(GTK_TABLE(table), GTK_WIDGET(button), 9, 10, 3, 4);
    gtk_signal_connect(GTK_OBJECT(button), "clicked",
                       G_CALLBACK(cb_gen_password), entry_password);

    /* Note textbox */
    label = gtk_label_new(_("Note"));
    gtk_box_pack_start(GTK_BOX(vbox2), label, FALSE, FALSE, 0);

    hbox_temp = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox2), hbox_temp, TRUE, TRUE, 0);

    keyr_note = gtk_text_view_new();
    keyr_note_buffer = G_OBJECT(gtk_text_view_get_buffer(GTK_TEXT_VIEW(keyr_note)));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(keyr_note), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(keyr_note), GTK_WRAP_WORD);

    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
    gtk_container_set_border_width(GTK_CONTAINER(scrolled_window), 1);
    gtk_container_add(GTK_CONTAINER(scrolled_window), keyr_note);
    gtk_box_pack_start_defaults(GTK_BOX(hbox_temp), scrolled_window);

    /**********************************************************************/

    gtk_widget_show_all(hbox);
    gtk_widget_show_all(vbox);

    gtk_widget_hide(add_record_button);
    gtk_widget_hide(apply_record_button);
    gtk_widget_hide(undelete_record_button);
    gtk_widget_hide(cancel_record_button);

    if (cycle_category) {
        /* First cycle keyr_category var */
        if (keyr_category == CATEGORY_ALL) {
            new_cat = -1;
        } else {
            new_cat = find_sort_cat_pos(keyr_category);
        }
        for (i = 0; i < NUM_KEYRING_CAT_ITEMS; i++) {
            new_cat++;
            if (new_cat >= NUM_KEYRING_CAT_ITEMS) {
                keyr_category = CATEGORY_ALL;
                break;
            }
            if ((sort_l[new_cat].Pcat) && (sort_l[new_cat].Pcat[0])) {
                keyr_category = sort_l[new_cat].cat_num;
                break;
            }
        }
        /* Then update menu with new keyr_category */
        if (keyr_category == CATEGORY_ALL) {
            index = 0;
            index2 = 0;
        } else {
            index = find_sort_cat_pos(keyr_category);
            index2 = find_menu_cat_pos(index) + 1;
            index += 1;
        }
        if (index < 0) {
            jp_logf(JP_LOG_WARN, _("Category is not legal\n"));
        } else {
            gtk_combo_box_set_active(GTK_COMBO_BOX(category_menu1), index2);
        }
    } else {
        keyr_category = CATEGORY_ALL;
    }
    keyr_update_liststore(listStore, &glob_keyring_list, keyr_category, TRUE);

    if (unique_id) {
        keyring_find(unique_id);
    }

    return EXIT_SUCCESS;
}


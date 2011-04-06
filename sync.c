/* $Id: sync.c,v 1.120 2011/04/06 12:43:45 rousseau Exp $ */

/*******************************************************************************
 * sync.c
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
 ******************************************************************************/

/********************************* Includes ***********************************/
#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <utime.h>
#include <dirent.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#ifdef USE_FLOCK
#  include <sys/file.h>
#else
#  include <fcntl.h>
#endif

#include <pi-socket.h>
#include <pi-dlp.h>
#include <pi-header.h>
#include <pi-file.h>
#include <pi-version.h>
#include <pi-error.h>
#include <pi-macros.h>

#include "i18n.h"
#include "utils.h"
#include "sync.h"
#include "log.h"
#include "prefs.h"
#include "datebook.h"
#include "plugins.h"
#include "libplugin.h"
#include "password.h"

/********************************* Constants **********************************/
#define FD_ERROR 1001

#define MAX_DBNAME 50

#ifndef min
#  define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

#define USE_LOCKING

/* #define PIPE_DEBUG */
/* #define JPILOT_DEBUG */
/* #define SYNC_CAT_DEBUG */

/******************************* Global vars **********************************/
extern int pipe_to_parent, pipe_from_parent;
extern pid_t glob_child_pid;

/****************************** Prototypes ************************************/
/* From jpilot.c for restoring sync icon after successful sync */
extern void cb_cancel_sync(GtkWidget *widget, unsigned int flags);

static int pi_file_install_VFS(const int fd, const char *basename, const int socket, const char *vfspath, progress_func f);
static int findVFSPath(int sd, const char *path, long *volume, char *rpath, int *rpathlen);

/****************************** Main Code *************************************/

static void sig_handler(int sig)
{
   int status = 0;

   jp_logf(JP_LOG_DEBUG, "caught signal SIGCHLD\n");

   /* wait for any child processes */
   waitpid(-1, &status, WNOHANG);

   /* SIGCHLD status is 0 for innocuous events like suspend/resume. */
   /* We specifically exit with return code 255 to trigger this cleanup */
   if (status > 0) {
      glob_child_pid = 0;
      cb_cancel_sync(NULL, 0);
   }
}

#ifdef USE_LOCKING
static int sync_lock(int *fd)
{
   pid_t pid;
   char lock_file[FILENAME_MAX];
   int r;
   char str[12];
#ifndef USE_FLOCK
   struct flock lock;
#endif

   get_home_file_name("sync_pid", lock_file, sizeof(lock_file));
   *fd = open(lock_file, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
   if (*fd<0) {
      jp_logf(JP_LOG_WARN, _("open lock file failed\n"));
      return EXIT_FAILURE;
   }
#ifndef USE_FLOCK
   lock.l_type = F_WRLCK;
   lock.l_start = 0;
   lock.l_whence = SEEK_SET;
   lock.l_len = 0; /* Lock to the end of file */
   r = fcntl(*fd, F_SETLK, &lock);
#else
   r = flock(*fd, LOCK_EX | LOCK_NB);
#endif
   if (r == -1){
      jp_logf(JP_LOG_WARN, _("lock failed\n"));
      read(*fd, str, 10);
      pid = atoi(str);
      jp_logf(JP_LOG_FATAL, _("sync file is locked by pid %d\n"), pid);
      close(*fd);
      return EXIT_FAILURE;
   } else {
      jp_logf(JP_LOG_DEBUG, "lock succeeded\n");
      pid=getpid();
      sprintf(str, "%d\n", pid);
      write(*fd, str, strlen(str)+1);
      ftruncate(*fd, strlen(str)+1);
   }
   return EXIT_SUCCESS;
}

static int sync_unlock(int fd)
{
   pid_t pid;
   char lock_file[FILENAME_MAX];
   int r;
   char str[12];
#ifndef USE_FLOCK
   struct flock lock;
#endif

   get_home_file_name("sync_pid", lock_file, sizeof(lock_file));

#ifndef USE_FLOCK
   lock.l_type = F_UNLCK;
   lock.l_start = 0;
   lock.l_whence = SEEK_SET;
   lock.l_len = 0;
   r = fcntl(fd, F_SETLK, &lock);
#else
   r = flock(fd, LOCK_UN | LOCK_NB);
#endif
   if (r == -1) {
      jp_logf(JP_LOG_WARN, _("unlock failed\n"));
      read(fd, str, 10);
      pid = atoi(str);
      jp_logf(JP_LOG_WARN, _("sync is locked by pid %d\n"), pid);
      close(fd);
      return EXIT_FAILURE;
   } else {
      jp_logf(JP_LOG_DEBUG, "unlock succeeded\n");
      ftruncate(fd, 0);
      close(fd);
   }
   return EXIT_SUCCESS;
}
#endif

static const char *get_error_str(int error)
{
   static char buf[10];

   switch (error) {
    case SYNC_ERROR_BIND:
      return "SYNC_ERROR_BIND";
    case SYNC_ERROR_LISTEN:
      return "SYNC_ERROR_LISTEN";
    case SYNC_ERROR_OPEN_CONDUIT:
      return "SYNC_ERROR_OPEN_CONDUIT";
    case SYNC_ERROR_PI_ACCEPT:
      return "SYNC_ERROR_PI_ACCEPT";
    case SYNC_ERROR_READSYSINFO:
      return "SYNC_ERROR_PI_CONNECT";
    case SYNC_ERROR_PI_CONNECT:
      return "SYNC_ERROR_READSYSINFO";
    case SYNC_ERROR_NOT_SAME_USER:
      return "SYNC_ERROR_NOT_SAME_USER";
    case SYNC_ERROR_NOT_SAME_USERID:
      return "SYNC_ERROR_NOT_SAME_USERID";
    case SYNC_ERROR_NULL_USERID:
      return "SYNC_ERROR_NULL_USERID";
    default:
      sprintf(buf, "%d", error);
      return NULL;
   }
}

/* Attempt to match records
 *
 * Ideally, one would have comparison routines on a per DB_name basis.
 * This involves a lot of overhead and packing/unpacking of records
 * and the routines are not written.
 *
 * A simpler way to compare records is to use memcmp. Some databases, 
 * however, do not pack tightly into memory and have gaps which are 
 * do-not-cares during comparison. These gaps can assume any value
 * but for comparison they are zeroed out, the same value that 
 * pilot-link uses.
 *
 * For databases that we have no knowledge of only simple comparisons
 * such as record length are possible.  This is almost always good 
 * enough but single character changes will not be caught. */ 
static int match_records(char *DB_name,
                  void *rrec, int rrec_len, int rattr, int rcategory,
                  void *lrec, int lrec_len, int lattr, int lcategory)
{

   if (!rrec || !lrec)         return FALSE;
   if (rrec_len != lrec_len)   return FALSE;
   if (rcategory != lcategory) return FALSE;
   if ((rattr & dlpRecAttrSecret) != (lattr & dlpRecAttrSecret)) {
      return FALSE;
   }

   /* memcmp works for a few specific databases */
   if (!strcmp(DB_name,"DatebookDB") ||
       !strcmp(DB_name,"CalendarDB-PDat")) {
      /* Hack for gapfill byte */
      set_byte(rrec+7,0);
      return !(memcmp(lrec, rrec, lrec_len));
   }

   if (!strcmp(DB_name,"AddressDB"))
      return !(memcmp(lrec, rrec, lrec_len));

   if (!strcmp(DB_name,"ContactsDB-PAdd")) {
      /* Hack for gapfill bytes */
      set_byte(rrec+4,(get_byte(rrec+4)) & 0x0F);
      set_byte(rrec+6,0);
      set_byte(lrec+16,0);
      set_byte(rrec+16,0);
      return !(memcmp(lrec, rrec, lrec_len));
   }

   if (!strcmp(DB_name,"ToDoDB"))
      return !(memcmp(lrec, rrec, lrec_len));

   if (!strcmp(DB_name,"MemoDB")   || 
       !strcmp(DB_name,"Memo32DB") ||
       !strcmp(DB_name,"MemosDB-PMem")) {
      return !(memcmp(lrec, rrec, lrec_len));
   }

   if (!strcmp(DB_name,"ExpenseDB")) {
      /* Hack for gapfill byte */
      set_byte(rrec+5,0);
      return !(memcmp(lrec, rrec, lrec_len));
   }

   if (!strcmp(DB_name,"Keys-Gtkr"))
      return !(memcmp(lrec, rrec, lrec_len));

   /* Lengths match and no other checks possible */
   return TRUE;

}

static void filename_make_legal(char *s)
{
   char *p;

   for (p=s; *p; p++) {
      if (*p=='/') {
         *p='?';
      }
   }
}

static int wait_for_response(int sd)
{
   int i;
   char buf[1024];
   int buf_len, ret;
   fd_set fds;
   struct timeval tv;
   int command;

#ifdef PIPE_DEBUG
   printf("child: wait_for_response()\n");
#endif

   /* For jpilot-sync */
   /* We should never get to this function, but just in case. */
   if (pipe_to_parent==STDOUT_FILENO) {
      return PIPE_SYNC_CANCEL;
   }

   /* Prevent the palm from timing out */
   pi_watchdog(sd, 7);
   /* 120 iterations is 2 minutes */
   for (i=0; i<120; i++) {
#ifdef PIPE_DEBUG
      printf("child wait_for_response() for\n");
      printf("pipe_from_parent = %d\n", pipe_from_parent);
#endif
      /* Linux modifies tv in the select call */
      tv.tv_sec=1;
      tv.tv_usec=0;
      FD_ZERO(&fds);
      FD_SET(pipe_from_parent, &fds);
      ret=select(pipe_from_parent+1, &fds, NULL, NULL, &tv);
      /* if (ret<0) {
         int err=errno;
         jp_logf(JP_LOG_WARN, "sync select %s\n", strerror(err));
      }*/
      if (ret==0) continue;
      /* this happens when waiting, probably a signal in pilot-link */

      if (!FD_ISSET(pipe_from_parent, &fds)) {
#ifdef PIPE_DEBUG
         printf("sync !FD_ISSET\n");
#endif
         continue;
      }
      buf[0]='\0';
      /* Read until newline, null, or end */
      buf_len=0;
      for (i=0; i<1022; i++) {
         ret = read(pipe_from_parent, &(buf[i]), 1);
         /* Error */
         if (ret<1) {
            int err=errno;
            printf("ret<1\n");
            printf("read from parent: %s\n", strerror(err));
            jp_logf(JP_LOG_WARN, "read from parent %s\n", strerror(err));
            break;
         }
#ifdef PIPE_DEBUG
         printf("ret=%d read %d[%d]\n", ret, buf[i], buf[i]);
#endif
         /* EOF */
#ifdef PIPE_DEBUG
         if (ret==0) {
            printf("ret==0\n");
            break;
         }
#endif
         buf_len++;
         if ((buf[i]=='\n')) break;
      }
      if (buf_len >= 1022) {
         buf[1022] = '\0';
      } else {
         if (buf_len > 0) {
            buf[buf_len]='\0';
         }
      }

      /* Look for the command */
      sscanf(buf, "%d:", &command);
#ifdef PIPE_DEBUG
      printf("command from parent=%d\n", command);
      printf("buf=[%s]\n", buf);
#endif
      break;
   }

   /* Back to normal */
   pi_watchdog(sd, 0);

   return command;
}

static int jp_pilot_connect(int *Psd, const char *device)
{
   int sd;
   int ret;
   struct  SysInfo sys_info;

   *Psd=0;

   sd = pi_socket(PI_AF_PILOT, PI_SOCK_STREAM, PI_PF_DLP);
   if (sd < 0) {
      int err = errno;
      perror("pi_socket");
      jp_logf(JP_LOG_WARN, "pi_socket %s\n", strerror(err));
      return EXIT_FAILURE;
   }

   ret = pi_bind(sd, device);
   if (ret < 0) {
      jp_logf(JP_LOG_WARN, "pi_bind error: %s %s\n", device, strerror(errno));
      jp_logf(JP_LOG_WARN, _("Check your sync port and settings\n"));
      pi_close(sd);
      return SYNC_ERROR_BIND;
   }

   ret = pi_listen(sd, 1);
   if (ret < 0) {
      perror("pi_listen");
      jp_logf(JP_LOG_WARN, "pi_listen %s\n", strerror(errno));
      pi_close(sd);
      return SYNC_ERROR_LISTEN;
   }

   sd = pi_accept(sd, 0, 0);
   if(sd < 0) {
      perror("pi_accept");
      jp_logf(JP_LOG_WARN, "pi_accept %s\n", strerror(errno));
      pi_close(sd);
      return SYNC_ERROR_PI_ACCEPT;
   }

   /* We must do this to take care of the password being required to sync
    * on Palm OS 4.x */
   if (dlp_ReadSysInfo(sd, &sys_info) < 0) {
      jp_logf(JP_LOG_WARN, "dlp_ReadSysInfo error\n");
      pi_close(sd);
      return SYNC_ERROR_READSYSINFO;
   }

   *Psd=sd;

   return EXIT_SUCCESS;
}

static void free_file_name_list(GList **Plist)
{
   GList *list, *temp_list;

   if (!Plist) return;
   list = *Plist;

   for (temp_list = list; temp_list; temp_list = temp_list->next) {
      if (temp_list->data) {
         free(temp_list->data);
      }
   }
   g_list_free(list);
   *Plist=NULL;
}

static void move_removed_apps(GList *file_list)
{
   DIR *dir;
   struct dirent *dirent;
   char full_backup_path[FILENAME_MAX];
   char full_remove_path[FILENAME_MAX];
   char full_backup_file[FILENAME_MAX];
   char full_remove_file[FILENAME_MAX];
   char home_dir[FILENAME_MAX];
   GList *list, *temp_list;
   int found;

   list = file_list;

#ifdef JPILOT_DEBUG
   printf("printing file list\n");
   for (temp_list = file_list; temp_list; temp_list = temp_list->next) {
      if (temp_list->data) {
         printf("File list [%s]\n", (char *)temp_list->data);
      }
   }
#endif

   get_home_file_name("", home_dir, sizeof(home_dir));

   /* Make sure the removed directory exists */
   g_snprintf(full_remove_path, sizeof(full_remove_path), "%s/backup_removed", home_dir);
   mkdir(full_remove_path, 0700);


   g_snprintf(full_backup_path, sizeof(full_backup_path), "%s/backup/", home_dir);
   jp_logf(JP_LOG_DEBUG, "opening [%s]\n", full_backup_path);
   dir = opendir(full_backup_path);
   if (dir) {
      while ((dirent = readdir(dir))) {
         jp_logf(JP_LOG_DEBUG, "dirent->d_name = [%s]\n", dirent->d_name);
         found=FALSE;
         if (!strcmp(dirent->d_name, ".")) continue;
         if (!strcmp(dirent->d_name, "..")) continue;
         for (temp_list = list; temp_list; temp_list = temp_list->next) {
            if (temp_list->data) {
               if (!strcmp((char *)temp_list->data, dirent->d_name)) {
                  found=TRUE;
                  break;
               }
            }
         }
         if (!found) {
            g_snprintf(full_backup_file, sizeof(full_backup_file), "%s/backup/%s", home_dir, dirent->d_name);
            g_snprintf(full_remove_file, sizeof(full_remove_file), "%s/backup_removed/%s", home_dir, dirent->d_name);
            jp_logf(JP_LOG_DEBUG, "[%s] not found\n", dirent->d_name);
            jp_logf(JP_LOG_DEBUG, "  moving [%s]\n  to [%s]\n", full_backup_file, full_remove_file);
            rename(full_backup_file, full_remove_file);
         }
      }
      closedir(dir);
   }
}

static int is_backup_dir(char *name)
{
   int i;

   /* backup dirs are of the form backupMMDDHHMM */
   if (strncmp(name, "backup", 6)) {
      return FALSE;
   }
   for (i=6; i<14; i++) {
      if (name[i]=='\0') {
         return FALSE;
      }
      if (!isdigit(name[i])) {
         return FALSE;
      }
   }
   if (name[i]!='\0') {
      return FALSE;
   }
   return TRUE;
}

static int compare_back_dates(char *s1, char *s2)
{
   /* backupMMDDhhmm */
   int i1, i2;

   if ((strlen(s1) < 8) || (strlen(s2) < 8)) {
      return 0;
   }
   i1 = atoi(&s1[6]);
   i2 = atoi(&s2[6]);
   /* Try to guess the year crossover with a 6 month window */
   if (((i1/1000000) <= 3) && ((i2/1000000) >= 10)) {
      return 1;
   }
   if (((i1/1000000) >= 10) && ((i2/1000000) <= 3)) {
      return 2;
   }
   if (i1>i2) {
      return 1;
   }
   if (i1<i2) {
      return 2;
   }
   return 0;
}

static int sync_remove_r(char *full_path)
{
   DIR *dir;
   struct dirent *dirent;
   char full_src[300];
   char last4[8];
   int len;

   dir = opendir(full_path);
   if (dir) {
      while ((dirent = readdir(dir))) {
         sprintf(full_src, "%s/%s", full_path, dirent->d_name);
         /* Just to make sure nothing too wrong is deleted */
         len = strlen(dirent->d_name);
         if (len < 4) {
            continue;
         }
         g_strlcpy(last4, dirent->d_name+len-4, 5);
         if ((strcmp(last4, ".pdb")==0) ||
             (strcmp(last4, ".prc")==0) ||
             (strcmp(last4, ".pqa")==0)) {
            unlink(full_src);
         }
      }
      closedir(dir);
   }
   rmdir(full_path);

   return EXIT_SUCCESS;
}

static int get_oldest_newest_dir(char *oldest, char *newest, int *count)
{
   DIR *dir;
   struct dirent *dirent;
   char home_dir[FILENAME_MAX];
   int r;

   get_home_file_name("", home_dir, sizeof(home_dir));
   jp_logf(JP_LOG_DEBUG, "rotate_backups: opening dir %s\n", home_dir);
   *count = 0;
   oldest[0]='\0';
   newest[0]='\0';
   dir = opendir(home_dir);
   if (!dir) {
      return EXIT_FAILURE;
   }
   *count = 0;
   while((dirent = readdir(dir))) {
      if (is_backup_dir(dirent->d_name)) {
         jp_logf(JP_LOG_DEBUG, "backup dir [%s]\n", dirent->d_name);
         (*count)++;
         if (oldest[0]=='\0') {
            strcpy(oldest, dirent->d_name);
            /* jp_logf(JP_LOG_DEBUG, "oldest is now %s\n", oldest);*/
         }
         if (newest[0]=='\0') {
            strcpy(newest, dirent->d_name);
            /* jp_logf(JP_LOG_DEBUG, "newest is now %s\n", newest);*/
         }
         r = compare_back_dates(oldest, dirent->d_name);
         if (r==1) {
            strcpy(oldest, dirent->d_name);
            /* jp_logf(JP_LOG_DEBUG, "oldest is now %s\n", oldest);*/
         }
         r = compare_back_dates(newest, dirent->d_name);
         if (r==2) {
            strcpy(newest, dirent->d_name);
            /* jp_logf(JP_LOG_DEBUG, "newest is now %s\n", newest);*/
         }
      }
   }
   closedir(dir);
   return EXIT_SUCCESS;
}

static int sync_rotate_backups(const int num_backups)
{
   DIR *dir;
   struct dirent *dirent;
   char home_dir[FILENAME_MAX];
   char full_name[FILENAME_MAX];
   char full_newdir[FILENAME_MAX];
   char full_backup[FILENAME_MAX];
   char full_oldest[FILENAME_MAX];
   char full_src[FILENAME_MAX];
   char full_dest[FILENAME_MAX];
   int r;
   int count, safety;
   char oldest[20];
   char newest[20];
   char newdir[20];
   time_t ltime;
   struct tm *now;

   get_home_file_name("", home_dir, sizeof(home_dir));

   /* We use safety because if removing the directory fails then we
    * will get stuck in an endless loop */
   for (safety=100; safety>0; safety--) {
      r = get_oldest_newest_dir(oldest, newest, &count);
      if (r<0) {
         jp_logf(JP_LOG_WARN, _("Unable to read home dir\n"));
         break;
      }
      if (count > num_backups) {
         sprintf(full_oldest, "%s/%s", home_dir, oldest);
         jp_logf(JP_LOG_DEBUG, "count=%d, num_backups=%d\n", count, num_backups);
         jp_logf(JP_LOG_DEBUG, "removing dir [%s]\n", full_oldest);
         sync_remove_r(full_oldest);
      } else {
         break;
      }
   }

   /* Now we should have the same number of backups (or less) as num_backups */

   time(&ltime);
   now = localtime(&ltime);
   /* Create the new backup directory */
   g_snprintf(newdir, sizeof(newdir), "backup%02d%02d%02d%02d",
              now->tm_mon+1, now->tm_mday, now->tm_hour, now->tm_min);
   if (strncmp(newdir, newest, sizeof(newdir))) {
      g_snprintf(full_newdir, sizeof(full_newdir), "%s/%s", home_dir, newdir);
      if (mkdir(full_newdir, 0700)==0) {
         count++;
      }
   }

   /* Copy from the newest backup, if it exists */
   if (strncmp(newdir, newest, sizeof(newdir))) {
      g_snprintf(full_backup, sizeof(full_backup), "%s/backup", home_dir);
      g_snprintf(full_newdir, sizeof(full_newdir), "%s/%s", home_dir, newdir);
      dir = opendir(full_backup);
      if (dir) {
         while ((dirent = readdir(dir))) {
            g_snprintf(full_src, sizeof(full_src), "%s/%s", full_backup, dirent->d_name);
            g_snprintf(full_dest, sizeof(full_dest), "%s/%s", full_newdir, dirent->d_name);
            jp_copy_file(full_src, full_dest);
         }
         closedir(dir);
      }
   }

   /* Remove the oldest backup if needed */
   if (count > num_backups) {
      if ( (oldest[0]!='\0') && (strncmp(newdir, oldest, sizeof(newdir))) ) {
         g_snprintf(full_oldest, sizeof(full_oldest), "%s/%s", home_dir, oldest);
         jp_logf(JP_LOG_DEBUG, "removing dir [%s]\n", full_oldest);
         sync_remove_r(full_oldest);
      }
   }

   /* Delete the symlink */
   g_snprintf(full_name, sizeof(full_name), "%s/backup", home_dir);
   unlink(full_name);

   /* Create the symlink */
   symlink(newdir, full_name);

   return EXIT_SUCCESS;
}

static int unpack_datebook_cai_from_ai(struct CategoryAppInfo *cai, unsigned char *ai_raw, int len)
{

   struct AppointmentAppInfo ai;
   int r;

   jp_logf(JP_LOG_DEBUG, "unpack_datebook_cai_from_ai\n");

   memset(&ai, 0, sizeof(ai));

   r = unpack_AppointmentAppInfo(&ai, ai_raw, len);
   if ((r <= 0) || (len <= 0)) {
      jp_logf(JP_LOG_DEBUG, "jp_unpack_AppointmentAppInfo failed %s %d\n", __FILE__, __LINE__);
      return EXIT_FAILURE;
   }
   memcpy(cai, &(ai.category), sizeof(struct CategoryAppInfo));

   return EXIT_SUCCESS;
}

static int pack_datebook_cai_into_ai(struct CategoryAppInfo *cai, 
                                     unsigned char *ai_raw, int len)
{
   struct AppointmentAppInfo ai;
   int r;

   jp_logf(JP_LOG_DEBUG, "pack_datebook_cai_into_ai\n");

   r = unpack_AppointmentAppInfo(&ai, ai_raw, len);
   if (r <= 0) {
      jp_logf(JP_LOG_DEBUG, "unpack_AppointmentAppInfo failed %s %d\n", __FILE__, __LINE__);
      return EXIT_FAILURE;
   }
   memcpy(&(ai.category), cai, sizeof(struct CategoryAppInfo));

   r = pack_AppointmentAppInfo(&ai, ai_raw, len);
   if (r <= 0) {
      jp_logf(JP_LOG_DEBUG, "pack_AppointmentAppInfo failed %s %d\n", __FILE__, __LINE__);
      return EXIT_FAILURE;
   }

   return EXIT_SUCCESS;
}

static int unpack_calendar_cai_from_ai(struct CategoryAppInfo *cai, 
                                       unsigned char *ai_raw, int len)
{

   struct CalendarAppInfo ai;
   int r;
   pi_buffer_t pi_buf;

   jp_logf(JP_LOG_DEBUG, "unpack_calendar_cai_from_ai\n");

   memset(&ai, 0, sizeof(ai));
   pi_buf.data = ai_raw;
   pi_buf.used = len;
   pi_buf.allocated = len;

   r = unpack_CalendarAppInfo(&ai, &pi_buf);
   if ((r <= 0) || (len <= 0)) {
      jp_logf(JP_LOG_DEBUG, "unpack_CalendarAppInfo failed %s %d\n", __FILE__, __LINE__);
      return EXIT_FAILURE;
   }
   memcpy(cai, &(ai.category), sizeof(struct CategoryAppInfo));

   return EXIT_SUCCESS;
}

static int pack_calendar_cai_into_ai(struct CategoryAppInfo *cai, 
                                     unsigned char *ai_raw, int len)
{
   struct CalendarAppInfo ai;
   int r;
   pi_buffer_t pi_buf;

   jp_logf(JP_LOG_DEBUG, "pack_calendar_cai_into_ai\n");

   pi_buf.data = ai_raw;
   pi_buf.used = len;
   pi_buf.allocated = len;
   r = unpack_CalendarAppInfo(&ai, &pi_buf);
   if (r <= 0) {
      jp_logf(JP_LOG_DEBUG, "unpack_CalendarAppInfo failed %s %d\n", __FILE__, __LINE__);
      return EXIT_FAILURE;
   }
   memcpy(&(ai.category), cai, sizeof(struct CategoryAppInfo));

   pi_buf.data = NULL;
   pi_buf.used = 0;
   pi_buf.allocated = 0;
   r = pack_CalendarAppInfo(&ai, &pi_buf);
   memcpy(ai_raw, pi_buf.data, len);
   if (r <= 0) {
      jp_logf(JP_LOG_DEBUG, "pack_CalendarAppInfo failed %s %d\n", __FILE__, __LINE__);
      return EXIT_FAILURE;
   }

   return EXIT_SUCCESS;
}

static int unpack_address_cai_from_ai(struct CategoryAppInfo *cai, 
                                      unsigned char *ai_raw, int len)
{
   struct AddressAppInfo ai;
   int r;

   jp_logf(JP_LOG_DEBUG, "unpack_address_cai_from_ai\n");

   memset(&ai, 0, sizeof(ai));
   r = unpack_AddressAppInfo(&ai, ai_raw, len);
   if ((r <= 0) || (len <= 0)) {
      jp_logf(JP_LOG_DEBUG, "unpack_AddressAppInfo failed %s %d\n", __FILE__, __LINE__);
      return EXIT_FAILURE;
   }
   memcpy(cai, &(ai.category), sizeof(struct CategoryAppInfo));

   return EXIT_SUCCESS;
}

static int pack_address_cai_into_ai(struct CategoryAppInfo *cai, 
                                    unsigned char *ai_raw, int len)
{
   struct AddressAppInfo ai;
   int r;

   jp_logf(JP_LOG_DEBUG, "pack_address_cai_into_ai\n");

   r = unpack_AddressAppInfo(&ai, ai_raw, len);
   if (r <= 0) {
      jp_logf(JP_LOG_DEBUG, "unpack_AddressAppInfo failed %s %d\n", __FILE__, __LINE__);
      return EXIT_FAILURE;
   }
   memcpy(&(ai.category), cai, sizeof(struct CategoryAppInfo));

   r = pack_AddressAppInfo(&ai, ai_raw, len);
   if (r <= 0) {
      jp_logf(JP_LOG_DEBUG, "pack_AddressAppInfo failed %s %d\n", __FILE__, __LINE__);
      return EXIT_FAILURE;
   }

   return EXIT_SUCCESS;
}

static int unpack_contact_cai_from_ai(struct CategoryAppInfo *cai, 
                                      unsigned char *ai_raw, int len)
{
   struct ContactAppInfo ai;
   int r;
   pi_buffer_t pi_buf;

   jp_logf(JP_LOG_DEBUG, "unpack_contact_cai_from_ai\n");

   memset(&ai, 0, sizeof(ai));
   pi_buf.data = ai_raw;
   pi_buf.used = len;
   pi_buf.allocated = len;
   r = jp_unpack_ContactAppInfo(&ai, &pi_buf);
   if ((r <= 0) || (len <= 0)) {
      jp_logf(JP_LOG_DEBUG, "jp_unpack_ContactAppInfo failed %s %d\n", __FILE__, __LINE__);
      return EXIT_FAILURE;
   }
   memcpy(cai, &(ai.category), sizeof(struct CategoryAppInfo));

   return EXIT_SUCCESS;
}

static int pack_contact_cai_into_ai(struct CategoryAppInfo *cai, 
                                    unsigned char *ai_raw, int len)
{
   struct ContactAppInfo ai;
   int r;
   pi_buffer_t *pi_buf;

   jp_logf(JP_LOG_DEBUG, "pack_contact_cai_into_ai\n");

   pi_buf = pi_buffer_new(len);
   pi_buffer_append(pi_buf, ai_raw, len);

   r = jp_unpack_ContactAppInfo(&ai, pi_buf);
   if (r <= 0) {
      jp_logf(JP_LOG_DEBUG, "jp_unpack_ContactAppInfo failed %s %d\n", __FILE__, __LINE__);
      pi_buffer_free(pi_buf);
      return EXIT_FAILURE;
   }
   memcpy(&(ai.category), cai, sizeof(struct CategoryAppInfo));

   //r = jp_pack_ContactAppInfo(&ai, ai_raw, len);
   r = jp_pack_ContactAppInfo(&ai, pi_buf);
   //undo check buffer sizes
   memcpy(ai_raw, pi_buf->data, pi_buf->used);
   pi_buffer_free(pi_buf);

   if (r <= 0) {
      jp_logf(JP_LOG_DEBUG, "jp_pack_ContactAppInfo failed %s %d\n", __FILE__, __LINE__);
      return EXIT_FAILURE;
   }

   return EXIT_SUCCESS;
}

static int unpack_todo_cai_from_ai(struct CategoryAppInfo *cai, 
                                   unsigned char *ai_raw, int len)
{
   struct ToDoAppInfo ai;
   int r;

   jp_logf(JP_LOG_DEBUG, "unpack_todo_cai_from_ai\n");

   memset(&ai, 0, sizeof(ai));
   r = unpack_ToDoAppInfo(&ai, ai_raw, len);
   if ((r <= 0) || (len <= 0)) {
      jp_logf(JP_LOG_DEBUG, "unpack_ToDoAppInfo failed %s %d\n", __FILE__, __LINE__);
      return EXIT_FAILURE;
   }
   memcpy(cai, &(ai.category), sizeof(struct CategoryAppInfo));

   return EXIT_SUCCESS;
}

static int pack_todo_cai_into_ai(struct CategoryAppInfo *cai, 
                                 unsigned char *ai_raw, int len)
{
   struct ToDoAppInfo ai;
   int r;

   jp_logf(JP_LOG_DEBUG, "pack_todo_cai_into_ai\n");

   r = unpack_ToDoAppInfo(&ai, ai_raw, len);
   if (r <= 0) {
      jp_logf(JP_LOG_DEBUG, "unpack_ToDoAppInfo failed %s %d\n", __FILE__, __LINE__);
      return EXIT_FAILURE;
   }
   memcpy(&(ai.category), cai, sizeof(struct CategoryAppInfo));

   r = pack_ToDoAppInfo(&ai, ai_raw, len);
   if (r <= 0) {
      jp_logf(JP_LOG_DEBUG, "pack_ToDoAppInfo failed %s %d\n", __FILE__, __LINE__);
      return EXIT_FAILURE;
   }

   return EXIT_SUCCESS;
}

static int unpack_memo_cai_from_ai(struct CategoryAppInfo *cai, 
                                   unsigned char *ai_raw, int len)
{
   struct MemoAppInfo ai;
   int r;

   jp_logf(JP_LOG_DEBUG, "unpack_memo_cai_from_ai\n");

        /* Bug 1922 in pilot-link-0.12.3 and below.
         * unpack_MemoAppInfo does not zero out all bytes of the 
         * appinfo struct and so it must be cleared here with a memset.
         * Can be removed from this and all other unpack routines when
         * Bug 1922 is fixed.
         * RW: 6/1/2008 */
   memset(&ai, 0, sizeof(ai));
   r = unpack_MemoAppInfo(&ai, ai_raw, len);
   if ((r <= 0) || (len <= 0)) {
      jp_logf(JP_LOG_DEBUG, "unpack_MemoAppInfo failed %s %d\n", __FILE__, __LINE__);
      return EXIT_FAILURE;
   }
   memcpy(cai, &(ai.category), sizeof(struct CategoryAppInfo));

   return EXIT_SUCCESS;
}

static int pack_memo_cai_into_ai(struct CategoryAppInfo *cai, 
                                 unsigned char *ai_raw, int len)
{
   struct MemoAppInfo ai;
   int r;

   jp_logf(JP_LOG_DEBUG, "pack_memo_cai_into_ai\n");

   r = unpack_MemoAppInfo(&ai, ai_raw, len);
   if (r <= 0) {
      jp_logf(JP_LOG_DEBUG, "unpack_MemoAppInfo failed %s %d\n", __FILE__, __LINE__);
      return EXIT_FAILURE;
   }
   memcpy(&(ai.category), cai, sizeof(struct CategoryAppInfo));

   r = pack_MemoAppInfo(&ai, ai_raw, len);
   if (r <= 0) {
      jp_logf(JP_LOG_DEBUG, "pack_MemoAppInfo failed %s %d\n", __FILE__, __LINE__);
      return EXIT_FAILURE;
   }

   return EXIT_SUCCESS;
}

/*
 * Fetch the databases from the palm if modified
 */
static void fetch_extra_DBs2(int sd, struct DBInfo info, char *palm_dbname[])
{
   struct pi_file *pi_fp;
   char full_name[FILENAME_MAX];
   struct stat statb;
   struct utimbuf times;
   int i;
   int found;
   char db_copy_name[MAX_DBNAME];
   char creator[5];

   found = 0;
   for (i=0; palm_dbname[i]; i++) {
      if (palm_dbname[i]==NULL) break;
      if (!strcmp(info.name, palm_dbname[i])) {
         jp_logf(JP_LOG_DEBUG, "Found extra DB\n");
         found=1;
         break;
      }
   }

   if (!found) {
      return;
   }

   g_strlcpy(db_copy_name, info.name, MAX_DBNAME-5);
   if (info.flags & dlpDBFlagResource) {
      strcat(db_copy_name,".prc");
   } else if (strncmp(db_copy_name + strlen(db_copy_name) - 4, ".pqa", 4)) {
      strcat(db_copy_name,".pdb");
   }

   filename_make_legal(db_copy_name);

   get_home_file_name(db_copy_name, full_name, sizeof(full_name));

   statb.st_mtime = 0;

   stat(full_name, &statb);

   creator[0] = (info.creator & 0xFF000000) >> 24;
   creator[1] = (info.creator & 0x00FF0000) >> 16;
   creator[2] = (info.creator & 0x0000FF00) >> 8;
   creator[3] = (info.creator & 0x000000FF);
   creator[4] = '\0';

   /* If modification times are the same then we don't need to fetch it */
   if (info.modifyDate == statb.st_mtime) {
      jp_logf(JP_LOG_DEBUG, "%s up to date, modify date (1) %ld\n", info.name, info.modifyDate);
      jp_logf(JP_LOG_GUI, _("%s (Creator ID '%s') is up to date, fetch skipped.\n"), db_copy_name, creator);
      return;
   }

   jp_logf(JP_LOG_GUI, _("Fetching '%s' (Creator ID '%s')... "), info.name, creator);

   info.flags &= 0xff;

   pi_fp = pi_file_create(full_name, &info);

   if (pi_fp==0) {
      jp_logf(JP_LOG_WARN, _("Failed, unable to create file %s\n"), full_name);
      return;
   }
   if (pi_file_retrieve(pi_fp, sd, 0, NULL)<0) {
      jp_logf(JP_LOG_WARN, _("Failed, unable to back up database %s\n"), info.name);
      times.actime = 0;
      times.modtime = 0;
   } else {
      jp_logf(JP_LOG_GUI, _("OK\n"));
      times.actime = info.createDate;
      times.modtime = info.modifyDate;
   }
   pi_file_close(pi_fp);

   /* Set the create and modify times of local file to same as on palm */
   utime(full_name, &times);
}

/*
 * Fetch the databases from the palm if modified
 */
static int fetch_extra_DBs(int sd, char *palm_dbname[])
{
   int cardno, start;
   struct DBInfo info;
   int dbIndex;
   pi_buffer_t *buffer;

   jp_logf(JP_LOG_DEBUG, "fetch_extra_DBs()\n");

   start=cardno=0;

   buffer = pi_buffer_new(32 * sizeof(struct DBInfo));

   /* Pilot-link 0.12 can return multiple db infos if the DLP is 1.2 and above */
   while(dlp_ReadDBList(sd, cardno, dlpDBListRAM | dlpDBListMultiple, start, buffer)>0) {
      for (dbIndex=0; dbIndex < (buffer->used / sizeof(struct DBInfo)); dbIndex++) {
         memcpy(&info, buffer->data + (dbIndex * sizeof(struct DBInfo)), sizeof(struct DBInfo));
         start=info.index+1;
         fetch_extra_DBs2(sd, info, palm_dbname);
      }
   }
   pi_buffer_free(buffer);

   return EXIT_SUCCESS;
}

/*
 * Fetch the databases from the palm if modified
 *
 * Be sure to call free_file_name_list(&file_list); before returning from
 * anywhere in this function.
 */
static int sync_fetch(int sd, unsigned int flags, 
                      const int num_backups, int fast_sync)
{
   struct pi_file *pi_fp;
   char full_name[FILENAME_MAX];
   char full_backup_name[FILENAME_MAX];
   char creator[5];
   struct stat statb;
   struct utimbuf times;
   int i, r;
   int main_app;
   int skip_file;
   int cardno, start;
   struct DBInfo info;
   char db_copy_name[MAX_DBNAME];
   GList *file_list;
   GList *end_of_list;
   int palmos_error;
   int dbIndex;
   pi_buffer_t *buffer;
#ifdef ENABLE_PLUGINS
   GList *temp_list;
   GList *plugin_list;
   struct plugin_s *plugin;
#endif
   char *file_name;
   /* rename_dbnames is used to modify this list to newer databases if needed*/
   char palm_dbname[][32]={
      "DatebookDB",
      "AddressDB",
      "ToDoDB",
      "MemoDB",
#ifdef ENABLE_MANANA
      "MañanaDB",
#endif
      "Saved Preferences",
      ""
   };
   char *extra_dbname[]={
      "Saved Preferences",
      NULL
   };

   typedef struct skip_db_t {
      unsigned int flags;
      unsigned int not_flags;
      const char *creator;
      char *dbname;
   } skip_db_t ;

   skip_db_t skip_db[] = {
      { 0, dlpDBFlagResource, "AvGo", NULL },
      { 0, dlpDBFlagResource, "psys", "Unsaved Preferences" },
      { 0, 0, "a68k", NULL},
      { 0, 0, "appl", NULL},
      { 0, 0, "boot", NULL},
      { 0, 0, "Fntl", NULL},
      { 0, 0, "PMHa", NULL},
      { 0, 0, "PMNe", NULL},
      { 0, 0, "ppp_", NULL},
      { 0, 0, "u8EZ", NULL},
      { 0, 0, NULL, NULL}
   };
   unsigned int full_backup;

   jp_logf(JP_LOG_DEBUG, "sync_fetch flags=0x%x, num_backups=%d, fast=%d\n",
                                             flags, num_backups, fast_sync);

   rename_dbnames(palm_dbname);

   end_of_list=NULL;

   full_backup = flags & SYNC_FULL_BACKUP;

   /* Fast sync still needs to fetch Saved Preferences before exiting */
   if (fast_sync && !full_backup) {
      fetch_extra_DBs(sd, extra_dbname);
      return EXIT_SUCCESS;
   }

   if (full_backup) {
      jp_logf(JP_LOG_DEBUG, "Full Backup\n");
      pi_watchdog(sd,10); /* prevent Palm timing out on long disk copy times */
      sync_rotate_backups(num_backups);
      pi_watchdog(sd,0);  /* back to normal behavior */
   }

   start=cardno=0;
   file_list=NULL;
   end_of_list=NULL;

   buffer = pi_buffer_new(32 * sizeof(struct DBInfo));

   while( (r=dlp_ReadDBList(sd, cardno, dlpDBListRAM | dlpDBListMultiple, start, buffer)) > 0) {
      for (dbIndex=0; dbIndex < (buffer->used / sizeof(struct DBInfo)); dbIndex++) {
         memcpy(&info, buffer->data + (dbIndex * sizeof(struct DBInfo)), sizeof(struct DBInfo));

      start=info.index+1;
      creator[0] = (info.creator & 0xFF000000) >> 24;
      creator[1] = (info.creator & 0x00FF0000) >> 16;
      creator[2] = (info.creator & 0x0000FF00) >> 8;
      creator[3] = (info.creator & 0x000000FF);
      creator[4] = '\0';
#ifdef JPILOT_DEBUG
      jp_logf(JP_LOG_DEBUG, "dbname = %s\n",info.name);
      jp_logf(JP_LOG_DEBUG, "exclude from sync = %d\n",info.miscFlags & dlpDBMiscFlagExcludeFromSync);
      jp_logf(JP_LOG_DEBUG, "flag backup = %d\n",info.flags & dlpDBFlagBackup);
      /* jp_logf(JP_LOG_DEBUG, "type = %x\n",info.type);*/
      jp_logf(JP_LOG_DEBUG, "Creator ID = [%s]\n", creator);
#endif
      if (full_backup) {
         /* Look at the skip list */
         skip_file=0;
         for (i=0; skip_db[i].creator || skip_db[i].dbname; i++) {
            if (skip_db[i].creator &&
                !strcmp(creator, skip_db[i].creator)) {
               if (skip_db[i].dbname && strcmp(info.name,skip_db[i].dbname)) {
                  continue;   /* Only creator matched, not DBname.  */
               }
               else {
                  if (skip_db[i].flags &&
                      (info.flags & skip_db[i].flags) != skip_db[i].flags) {
                     skip_file=1;
                     break;
                  }
                  else if (skip_db[i].not_flags &&
                           !(info.flags & skip_db[i].not_flags)) {
                     skip_file=1;
                     break;
                  }
                  else if (!skip_db[i].flags && !skip_db[i].not_flags) {
                     skip_file=1;
                     break;
                  }
               }
            }
            if (skip_db[i].dbname &&
                !strcmp(info.name,skip_db[i].dbname)) {
               if (skip_db[i].flags &&
                   (info.flags & skip_db[i].flags) != skip_db[i].flags) {
                  skip_file=1;
                  break;
               }
               else if (skip_db[i].not_flags &&
                        (info.flags & skip_db[i].not_flags)) {
                  skip_file=1;
                  break;
               }
               else if (!skip_db[i].flags && !skip_db[i].not_flags) {
                  skip_file=1;
                  break;
               }
            }
         }
         if (skip_file) {
            jp_logf(JP_LOG_GUI, _("Skipping %s (Creator ID '%s')\n"), info.name, creator);
            continue;
         }
      }

      main_app = 0;
      skip_file = 0;
      for (i=0; palm_dbname[i][0]; i++) {
         if (!strcmp(info.name, palm_dbname[i])) {
            jp_logf(JP_LOG_DEBUG, "Found main app\n");
            main_app = 1;
            /* Skip if conduit is not enabled in preferences */
            if (!full_backup) {
               switch (i) {
                case 0:
                  if (!get_pref_int_default(PREF_SYNC_DATEBOOK, 1)) {
                     skip_file = 1; 
                  }
                  break;
                case 1:
                  if (!get_pref_int_default(PREF_SYNC_ADDRESS, 1)) {
                     skip_file = 1; 
                  }
                  break;
                case 2:
                  if (!get_pref_int_default(PREF_SYNC_TODO, 1)) {
                     skip_file = 1; 
                  }
                  break;
                case 3:
                  if (!get_pref_int_default(PREF_SYNC_MEMO, 1)) {
                     skip_file = 1; 
                  }
                  break;
#ifdef ENABLE_MANANA                  
                case 4:
                  if (!get_pref_int_default(PREF_SYNC_MANANA, 1)) {
                     skip_file = 1; 
                  }
                  break;
#endif
               } /* end switch */
            } /* end if checking for excluded conduits */
            break; 
         } /* end if checking for main app */
      } /* for loop over main app names */

      /* skip main app conduit as necessary */
      if (skip_file) continue;

#ifdef ENABLE_PLUGINS
      plugin_list = get_plugin_list();

      if (!main_app) {
         skip_file = 0;
         for (temp_list = plugin_list; temp_list; temp_list = temp_list->next) {
            plugin = (struct plugin_s *)temp_list->data;
            if (!strcmp(info.name, plugin->db_name)) {
               jp_logf(JP_LOG_DEBUG, "Found plugin\n");
               main_app = 1;
               /* Skip if conduit is not enabled */
               if (!full_backup && !plugin->sync_on) {
                 skip_file = 1;
               }
               break;
            }
         }
      }
#endif
      /* skip plugin conduit as necessary */
      if (skip_file) continue;
      
      g_strlcpy(db_copy_name, info.name, MAX_DBNAME-5);
      if (info.flags & dlpDBFlagResource) {
         strcat(db_copy_name,".prc");
      } else if (strncmp(db_copy_name + strlen(db_copy_name) - 4, ".pqa", 4)) {
         strcat(db_copy_name,".pdb");
      }

      filename_make_legal(db_copy_name);

      if (!strcmp(db_copy_name, "Graffiti ShortCuts .prc")) {
         /* Make a special exception for the graffiti shortcuts.
          * We want to save it as this to avoid the confusion of
          * having 2 different versions around */
         strcpy(db_copy_name, "Graffiti ShortCuts.prc");
      }
      get_home_file_name(db_copy_name, full_name, sizeof(full_name));
      get_home_file_name("backup/", full_backup_name, sizeof(full_backup_name));
      strcat(full_backup_name, db_copy_name);

      /* Add this to our file name list if not manually skipped */
      jp_logf(JP_LOG_DEBUG, "appending [%s]\n", db_copy_name);
      file_list = g_list_prepend(file_list, strdup(db_copy_name));

      if ( !fast_sync && !full_backup && !main_app ) {
         continue;
      }
#ifdef JPILOT_DEBUG
      if (main_app) {
         jp_logf(JP_LOG_DEBUG, "main_app is set\n");
      }
#endif

      if (main_app && !fast_sync) {
         file_name = full_name;
      } else {
         file_name = full_backup_name;
      }
      statb.st_mtime = 0;
      stat(file_name, &statb);
#ifdef JPILOT_DEBUG
      jp_logf(JP_LOG_GUI, "palm dbtime= %d, local dbtime = %d\n", info.modifyDate, statb.st_mtime);
      jp_logf(JP_LOG_GUI, "flags=0x%x\n", info.flags);
      jp_logf(JP_LOG_GUI, "backup_flag=%d\n", info.flags & dlpDBFlagBackup);
#endif
      /* If modification times are the same then we don't need to fetch it */
      if (info.modifyDate == statb.st_mtime) {
         jp_logf(JP_LOG_DEBUG, "%s up to date, modify date (2) %ld\n", info.name, info.modifyDate);
         jp_logf(JP_LOG_GUI, _("%s (Creator ID '%s') is up to date, fetch skipped.\n"), db_copy_name, creator);
         continue;
      }

      jp_logf(JP_LOG_GUI, _("Fetching '%s' (Creator ID '%s')... "), info.name, creator);

      info.flags &= 0xff;

      pi_fp = pi_file_create(file_name, &info);
      if (pi_fp==0) {
         jp_logf(JP_LOG_WARN, _("Failed, unable to create file %s\n"),
                main_app ? full_name : full_backup_name);
         continue;
      }
      if (pi_file_retrieve(pi_fp, sd, 0, NULL)<0) {
         jp_logf(JP_LOG_WARN, _("Failed, unable to back up database %s\n"), info.name);
         times.actime = 0;
         times.modtime = 0;
      } else {
         jp_logf(JP_LOG_GUI, _("OK\n"));
         times.actime = info.createDate;
         times.modtime = info.modifyDate;
      }
      pi_file_close(pi_fp);

      /* Set the create and modify times of local file to same as on palm */
      utime(file_name, &times);

      /* This call preserves the file times */
      if (main_app && !fast_sync && full_backup) {
         jp_copy_file(full_name, full_backup_name);
      }
   }
   }
   pi_buffer_free(buffer);
   palmos_error = pi_palmos_error(sd);
   if (palmos_error==dlpErrNotFound) {
      jp_logf(JP_LOG_DEBUG, "Good return code (dlpErrNotFound)\n");
      if (full_backup) {
         jp_logf(JP_LOG_DEBUG, "Removing apps not found on the palm\n");
         move_removed_apps(file_list);
      }
   } else {
      jp_logf(JP_LOG_WARN, "ReadDBList returned = %d\n", r);
      jp_logf(JP_LOG_WARN, "palmos_error = %d\n", palmos_error);
      jp_logf(JP_LOG_WARN, "dlp_strerror is %s\n", dlp_strerror(palmos_error));
   }
        
   free_file_name_list(&file_list);

   return EXIT_SUCCESS;
}

static int sync_install(char *filename, int sd, char *vfspath)
{
   struct pi_file *f;
   struct  DBInfo info;
   char *Pc;
   char log_entry[256];
   int r, try_again;
   long char_set;
   char creator[5];
   int sdcard_install;

   get_pref(PREF_CHAR_SET, &char_set, NULL);

   Pc = strrchr(filename, '/');
   if (!Pc) {
      Pc = filename;
   } else {
      Pc++;
   }

   sdcard_install = (vfspath != NULL);

   jp_logf(JP_LOG_GUI, _("Installing %s "), Pc);
   f = pi_file_open(filename);
   if (f==NULL && !sdcard_install) {
      int fd;

      if ((fd = open(filename, O_RDONLY)) < 0) {
         jp_logf(JP_LOG_WARN, _("\nUnable to open file: '%s': %s!\n"),
                                                    filename, strerror(errno));
      } else {
         close(fd);
         jp_logf(JP_LOG_WARN, _("\nUnable to sync file: '%s': file corrupted?\n"),
                 filename);
      }
      return EXIT_FAILURE;
   }
   if (f != NULL) {
      memset(&info, 0, sizeof(info));
      pi_file_get_info(f, &info);
      creator[0] = (info.creator & 0xFF000000) >> 24;
      creator[1] = (info.creator & 0x00FF0000) >> 16,
      creator[2] = (info.creator & 0x0000FF00) >> 8,
      creator[3] = (info.creator & 0x000000FF);
      creator[4] = '\0';
   }

   if (!sdcard_install) {
      jp_logf(JP_LOG_GUI, _("(Creator ID '%s')... "), creator);
      r = pi_file_install(f, sd, 0, NULL);
   } else {
      if (f != NULL) {
         jp_logf(JP_LOG_GUI, _("(Creator ID '%s') "), creator);
      }
      jp_logf(JP_LOG_GUI, _("(SDcard dir %s)... "), vfspath);
      const char *basename = strrchr(filename, '/');
      if (!basename) {
         basename = filename;
      } else {
         basename++;
      }
      int fd;
      if ((fd = open(filename, O_RDONLY)) < 0)
      {
         jp_logf(JP_LOG_WARN, _("\nUnable to open file: '%s': %s!\n"),
                                                    filename, strerror(errno));
         pi_file_close(f);
         return EXIT_FAILURE;
      }
      r = pi_file_install_VFS(fd, basename, sd, vfspath, NULL);
      close(fd);
   }

   if (r<0 && !sdcard_install) {
      try_again = 0;
      /* TODO: make this generic? Not sure it would work 100% of the time */
      /* Here we make a special exception for graffiti */
      if (!strcmp(info.name, "Graffiti ShortCuts")) {
         strcpy(info.name, "Graffiti ShortCuts ");
         /* This requires a reset */
         info.flags |= dlpDBFlagReset;
         info.flags |= dlpDBFlagNewer;
         pi_file_close(f);
         pdb_file_write_dbinfo(filename, &info);
         f = pi_file_open(filename);
         if (f==0) {
            jp_logf(JP_LOG_WARN, _("\nUnable to open file: %s\n"), filename);
            return EXIT_FAILURE;
         }
         try_again = 1;
      } else if (!strcmp(info.name, "Graffiti ShortCuts ")) {
         strcpy(info.name, "Graffiti ShortCuts");
         /* This requires a reset */
         info.flags |= dlpDBFlagReset;
         info.flags |= dlpDBFlagNewer;
         pi_file_close(f);
         pdb_file_write_dbinfo(filename, &info);
         f = pi_file_open(filename);
         if (f==0) {
            jp_logf(JP_LOG_WARN, _("\nUnable to open file: %s\n"), filename);
            return EXIT_FAILURE;
         }
         try_again = 1;
      }
      /* Here we make a special exception for Net Prefs */
      if (!strcmp(info.name, "Net Prefs")) {
         strcpy(info.name, "Net Prefs ");
         /* This requires a reset */
         info.flags |= dlpDBFlagReset;
         info.flags |= dlpDBFlagNewer;
         pi_file_close(f);
         pdb_file_write_dbinfo(filename, &info);
         f = pi_file_open(filename);
         if (f==0) {
            jp_logf(JP_LOG_WARN, _("\nUnable to open file: %s\n"), filename);
            return EXIT_FAILURE;
         }
         try_again = 1;
      } else if (!strcmp(info.name, "Net Prefs ")) {
         strcpy(info.name, "Net Prefs");
         /* This requires a reset */
         info.flags |= dlpDBFlagReset;
         info.flags |= dlpDBFlagNewer;
         pi_file_close(f);
         pdb_file_write_dbinfo(filename, &info);
         f = pi_file_open(filename);
         if (f==0) {
            jp_logf(JP_LOG_WARN, _("\nUnable to open file: %s\n"), filename);
            return EXIT_FAILURE;
         }
         try_again = 1;
      }
      if (try_again) {
         /* Try again */
         r = pi_file_install(f, sd, 0, NULL);
      }
   }

   if (r<0) {
      g_snprintf(log_entry, sizeof(log_entry), _("Install %s failed"), Pc);
      charset_j2p(log_entry, sizeof(log_entry), char_set);
      dlp_AddSyncLogEntry(sd, log_entry);
      dlp_AddSyncLogEntry(sd, "\n");;
      jp_logf(JP_LOG_GUI, _("Failed.\n"));
      jp_logf(JP_LOG_WARN, "%s\n", log_entry);
      if (f) pi_file_close(f);
      return EXIT_FAILURE;
   }
   else {
      g_snprintf(log_entry, sizeof(log_entry), _("Installed %s"), Pc);
      charset_j2p(log_entry, sizeof(log_entry), char_set);
      dlp_AddSyncLogEntry(sd, log_entry);
      dlp_AddSyncLogEntry(sd, "\n");;
      jp_logf(JP_LOG_GUI, _("OK\n"));
   }
   if (f) pi_file_close(f);

   return EXIT_SUCCESS;
}

/* file must not be open elsewhere when this is called */
/* the first line is 0 */
static int sync_process_install_file(int sd)
{
   FILE *in;
   FILE *out;
   char line[1002];
   char *Pc;
   int r, line_count;
   char vfsdir[] = "/PALM/Launcher/";   // Install location for SDCARD files

   in = jp_open_home_file(EPN".install", "r");
   if (!in) {
      jp_logf(JP_LOG_WARN, _("Unable to open file: %s%s\n"), EPN, ".install");
      return EXIT_FAILURE;
   }

   out = jp_open_home_file(EPN".install.tmp", "w");
   if (!out) {
      jp_logf(JP_LOG_WARN, _("Unable to open file: %s%s\n"), EPN, ".install.tmp");
      fclose(in);
      return EXIT_FAILURE;
   }

   for (line_count=0; (!feof(in)); line_count++) {
      line[0]='\0';
      Pc = fgets(line, 1000, in);
      if (!Pc) {
         break;
      }
      if (line[strlen(line)-1]=='\n') {
         line[strlen(line)-1]='\0';
      }
      if (line[0] == '\001') {
         /* found SDCARD indicator */
         r = sync_install(&(line[1]), sd, vfsdir);
      } else {
         r = sync_install(line, sd, NULL);
      }
      
      if (r==0) {
         continue;
      }
      fprintf(out, "%s\n", line);
   }
   fclose(in);
   fclose(out);

   rename_file(EPN".install.tmp", EPN".install");

   return EXIT_SUCCESS;
}

static int sync_categories(char *DB_name, 
                           int sd,
                           int (*unpack_cai_from_ai)(struct CategoryAppInfo *cai, unsigned char *ai_raw, int len),
                           int (*pack_cai_into_ai)(struct CategoryAppInfo *cai, unsigned char *ai_raw, int len)
)
{
   struct CategoryAppInfo local_cai, remote_cai, orig_remote_cai;
   char full_name[FILENAME_MAX];
   char pdb_name[FILENAME_MAX];
   char log_entry[256];
   struct pi_file *pf;
   pi_buffer_t *buffer;
   size_t local_cai_size;
   int remote_cai_size;
   unsigned char buf[65536];
   int db;
   void *Papp_info;
   char tmp_name[18];
   int tmp_int;
   int i, r, Li, Ri;
   int found_name, found_ID;
   int found_name_at, found_ID_at;
   int found_a_slot;
   int move_from_idx[NUM_CATEGORIES];
   int move_to_idx[NUM_CATEGORIES];
   int move_i = 0;
   int loop;
   long char_set;

   jp_logf(JP_LOG_DEBUG, "sync_categories for %s\n", DB_name);

   get_pref(PREF_CHAR_SET, &char_set, NULL);

   g_snprintf(pdb_name, sizeof(pdb_name), "%s%s", DB_name, ".pdb");
   get_home_file_name(pdb_name, full_name, sizeof(full_name));

   Papp_info=NULL;
   memset(&local_cai, 0, sizeof(local_cai));
   memset(&remote_cai, 0, sizeof(remote_cai));

   pf = pi_file_open(full_name);
   if (!pf) {
      jp_logf(JP_LOG_WARN, _("%s:%d Error reading file: %s\n"), __FILE__, __LINE__, full_name);
      return EXIT_FAILURE;
   }
   pi_file_get_app_info(pf, &Papp_info, &local_cai_size);
   if (local_cai_size <= 0) {
      jp_logf(JP_LOG_WARN, _("%s:%d Error getting app info %s\n"), __FILE__, __LINE__, full_name);
      return EXIT_FAILURE;
   }

   r = unpack_cai_from_ai(&local_cai, Papp_info, local_cai_size);
   if (EXIT_SUCCESS != r) {
      jp_logf(JP_LOG_WARN, _("%s:%d Error unpacking app info %s\n"), __FILE__, __LINE__, full_name);
      return EXIT_FAILURE;
   }

   pi_file_close(pf);

   /* Open the applications database, store access handle in db */
   r = dlp_OpenDB(sd, 0, dlpOpenReadWrite, DB_name, &db);
   if (r < 0) {
      /* File unable to be opened.  
       * Return an error but let the main record sync process 
       * do the logging to the GUI and Palm */
      jp_logf(JP_LOG_DEBUG, "sync_categories: Unable to open file: %s\n", DB_name);
      return EXIT_FAILURE;
   }

   buffer = pi_buffer_new(0xFFFF);
   /* buffer size passed in cannot be any larger than 0xffff */
   remote_cai_size = dlp_ReadAppBlock(sd, db, 0, -1, buffer);
   jp_logf(JP_LOG_DEBUG, "readappblock r=%d\n", remote_cai_size);
   if ((remote_cai_size<=0) || (remote_cai_size > sizeof(buf))) {
      jp_logf(JP_LOG_WARN, _("Error reading appinfo block for %s\n"), DB_name);
      dlp_CloseDB(sd, db);
      pi_buffer_free(buffer);
      return EXIT_FAILURE;
   }
   memcpy(buf, buffer->data, remote_cai_size);
   pi_buffer_free(buffer);
   r = unpack_cai_from_ai(&remote_cai, buf, remote_cai_size);
   if (EXIT_SUCCESS != r) {
      jp_logf(JP_LOG_WARN, _("%s:%d Error unpacking app info %s\n"), __FILE__, __LINE__, full_name);
      return EXIT_FAILURE;
   }
   memcpy(&orig_remote_cai, &remote_cai, sizeof(remote_cai));

   /* Do a memcmp first to see if nothing has changed, the common case */
   if (!memcmp(&(local_cai), &(remote_cai), sizeof(local_cai))) {
      jp_logf(JP_LOG_DEBUG, "Category app info match, nothing to do %s\n", DB_name);
      dlp_CloseDB(sd, db);
      return EXIT_SUCCESS;
   }

#ifdef SYNC_CAT_DEBUG
   printf("--- pre-sync CategoryAppInfo\n");
   printf("--- DB_name [%s]\n", DB_name);
   printf("--- local: size is %d ---\n", local_cai_size);
   for (i=0; i<NUM_CATEGORIES; i++) {
      printf("local: cat %d [%s] ID %d renamed %d\n", i,
             local_cai.name[i],
             local_cai.ID[i], local_cai.renamed[i]);
   }
   printf("--- remote: size is %d ---\n", remote_cai_size);
   for (i=0; i<NUM_CATEGORIES; i++) {
         printf("remote: cat %d [%s] ID %d renamed %d\n", i,
                remote_cai.name[i],
                remote_cai.ID[i], remote_cai.renamed[i]);
   }
#endif

   /* Go through the categories, skipping the reserved 'Unfiled' at index 0,
    * and try to synchronize them.
    * loop variable is to prevent infinite loops */
   for (Li=loop=1; ((Li<NUM_CATEGORIES) && (loop<256)); Li++, loop++) {
      found_name=found_ID=FALSE;
      found_name_at=found_ID_at=0;

      /* Blank entry in table */
      if ((local_cai.name[Li][0]==0) && (local_cai.ID[Li]==0)) {
         continue;
      }
      /* 0: Category deleted locally.  Undocumented by Palm */
      if (local_cai.name[Li][0]==0) {
         if ((!remote_cai.renamed[Li]) && (remote_cai.ID[Li]!=0)) {
#ifdef SYNC_CAT_DEBUG
            printf("cat %d deleted local, del cat on remote\n", Li);
            printf(" remote cat name %s\n", remote_cai.name[Li]);
            printf(" remote rename flag was %d\n", remote_cai.renamed[Li]);
#endif
            remote_cai.name[Li][0]='\0';
            remote_cai.ID[Li]=0;
            remote_cai.renamed[Li]=0;
            /* This category was deleted.  Move records on Palm to Unfiled */
            jp_logf(JP_LOG_DEBUG, "Moving category %d to unfiled...", Li);
            r = dlp_MoveCategory(sd, db, Li, 0);
            jp_logf(JP_LOG_DEBUG, "dlp_MoveCategory returned %d\n", r);
         }
         continue;
      }

      /* Do a search for the local category name and ID on the remote */
      for (Ri = 1; Ri < NUM_CATEGORIES; Ri++) {
         if (! strncmp(local_cai.name[Li], remote_cai.name[Ri], PILOTCAT_NAME_SZ)) {
#ifdef SYNC_CAT_DEBUG
            if (found_name)
               printf("Found name %s twice at %d and %d\n", 
                                  local_cai.name[Li], found_name_at, Ri);
#endif
            found_name=TRUE;
            found_name_at=Ri;
         }
         if (local_cai.ID[Li] == remote_cai.ID[Ri]) {
#ifdef SYNC_CAT_DEBUG
            if (found_ID && (local_cai.ID[Li] != 0))
               printf("Found ID %d twice at %d and %d\n", 
                                local_cai.ID[Li], found_ID_at, Ri);
#endif
            found_ID=TRUE;
            found_ID_at=Ri;
         }
      }

      /* Process the results of the name and ID search */
      if (found_name) {
         if (Li==found_name_at) {
            /* 1: OK. Index and name match so there is nothing to do. */
#ifdef SYNC_CAT_DEBUG
            printf("cat index %d ok\n", Li);
#endif
            continue;
         } else {
            /* 2: Change all local recs to use remote recs ID */
            /* This is where there is a bit of trouble, since we have a pdb
             * file on the local side we don't store the ID and there is no way
             * to do this.
             * Instead, we will swap indexes on the local records.
             */
#ifdef SYNC_CAT_DEBUG
            printf("cat index %d case 2\n", Li);
            printf("Swapping index %d to %d\n", Li, found_name_at);
#endif
            r = pdb_file_swap_indexes(DB_name, Li, found_name_at);
            r = edit_cats_swap_cats_pc3(DB_name, Li, found_name_at);
            /* Swap name, ID, and renamed attributes in local table */
            g_strlcpy(tmp_name, local_cai.name[found_name_at], PILOTCAT_NAME_SZ);
            strncpy(local_cai.name[found_name_at],
                    local_cai.name[Li], PILOTCAT_NAME_SZ);
            strncpy(local_cai.name[Li], tmp_name, PILOTCAT_NAME_SZ);

            tmp_int = local_cai.ID[found_name_at]; 
            local_cai.ID[found_name_at] = local_cai.ID[Li];
            local_cai.ID[Li] = tmp_int;

            tmp_int = local_cai.renamed[found_name_at]; 
            local_cai.renamed[found_name_at] = local_cai.renamed[Li];
            local_cai.renamed[Li] = tmp_int;

            if (found_name_at > Li) {
               /* Need to reprocess this Li index because a new one has
                * has been swapped in. */
               Li--;
            }
            continue;
         }
      }
      if ((!found_name) && (found_ID)) {
         if (local_cai.renamed[Li]) {
            /* 3: Change remote category name to match local at index Li */
#ifdef SYNC_CAT_DEBUG
            printf("cat index %d case 3\n", Li);
#endif
            g_strlcpy(remote_cai.name[found_ID_at],
                      local_cai.name[Li], PILOTCAT_NAME_SZ);
            continue;
         } else {
            if (remote_cai.renamed[found_ID_at]) {
               /* 3a1: Category has been renamed on Palm. Undocumented */
#ifdef SYNC_CAT_DEBUG
               printf("cat index %d case 3a1\n", Li);
#endif
               continue;
            } else {
               /* 3a2: Category has been deleted on Palm. Undocumented */
#ifdef SYNC_CAT_DEBUG
               printf("cat index %d case 3a2\n", Li);
               printf("cat %d deleted remote, del cat on local\n", Li);
               printf(" local cat name %s\n", local_cai.name[Li]);
#endif
               local_cai.renamed[Li]=0;
               local_cai.name[Li][0]='\0';
               local_cai.ID[Li]=0;

               remote_cai.name[found_ID_at][0]='\0';
               remote_cai.ID[found_ID_at]=0;
               remote_cai.renamed[found_ID_at]=0;
               jp_logf(JP_LOG_DEBUG, "Moving local recs category %d to Unfiled\n", Li);
               /* Move only changed records(pc3) to Unfiled.  Records in pdb
                * will be handled by sync record process */
               edit_cats_change_cats_pc3(DB_name, Li, 0);
               continue;
            }
         }
      }
      if ((!found_name) && (!found_ID)) {
         if (remote_cai.name[Li][0]=='\0') {
            /* 4: Add local category to remote */
#ifdef SYNC_CAT_DEBUG
            printf("cat index %d case 4\n", Li);
#endif
            g_strlcpy(remote_cai.name[Li],
                      local_cai.name[Li], PILOTCAT_NAME_SZ);
            remote_cai.ID[Li]=local_cai.ID[Li];
            remote_cai.renamed[Li]=0;
            continue;
         } else {
            if (!remote_cai.renamed[Li]) {
               /* 5a: Category was deleted locally and a new one replaced it.
                *     Undocumented by Palm. */
#ifdef SYNC_CAT_DEBUG
               printf("cat index %d case 5a\n", Li);
#endif
               /* Move the old category's records to Unfiled */
               jp_logf(JP_LOG_DEBUG, "Moving category %d to unfiled...", Li);
               r = dlp_MoveCategory(sd, db, Li, 0);
               jp_logf(JP_LOG_DEBUG, "dlp_MoveCategory returned %d\n", r);

               /* Rename slot to the new category */
               g_strlcpy(remote_cai.name[Li], local_cai.name[Li], PILOTCAT_NAME_SZ);
               remote_cai.ID[Li]=local_cai.ID[Li];
               remote_cai.renamed[Li]=0;
               continue;
            }
            if (!local_cai.renamed[Li]) {
               /* 5b: Category was deleted on Palm and a new one replaced it.
                *     Undocumented by Palm. */
#ifdef SYNC_CAT_DEBUG
               printf("cat index %d case 5b\n", Li);
#endif
               /* Move the old category's records to Unfiled */
               jp_logf(JP_LOG_DEBUG, "Moving local recs category %d to Unfiled\n", Li);
               /* Move only changed records(pc3) to Unfiled.  Records in pdb
                * will be handled by sync record process */
               edit_cats_change_cats_pc3(DB_name, Li, 0);
               remote_cai.renamed[Li]=0;
               continue;
            }

            /* 5: Add local category to remote in the next available slot.
                  local records are changed to use this index. */
#ifdef SYNC_CAT_DEBUG
            printf("cat index %d case 5\n", Li);
#endif
            found_a_slot=FALSE;
            for (i=1; i<NUM_CATEGORIES; i++) {
               if (remote_cai.name[i][0]=='\0') {
                  g_strlcpy(remote_cai.name[i], 
                            local_cai.name[Li], PILOTCAT_NAME_SZ);
                  remote_cai.renamed[i]=1;
                  remote_cai.ID[i]=local_cai.ID[Li];
                  move_from_idx[move_i] = Li;
                  move_to_idx[move_i] = i;
                  if (++move_i >= NUM_CATEGORIES) {
                     move_i = NUM_CATEGORIES-1;
                     jp_logf(JP_LOG_DEBUG, "Exceeded number of categorie for case 5\n");
                  }
                  found_a_slot=TRUE;
                  break;
               }
            }
            if (!found_a_slot) {
               jp_logf(JP_LOG_WARN, _("Could not add category %s to remote.\n"), local_cai.name[Li]);
               jp_logf(JP_LOG_WARN, _("Too many categories on remote.\n"));
               jp_logf(JP_LOG_WARN, _("All records on desktop in %s will be moved to %s.\n"), local_cai.name[Li], local_cai.name[0]);
               /* Fix - need a func for this logging */
               g_snprintf(log_entry, sizeof(log_entry), _("Could not add category %s to remote.\n"), local_cai.name[Li]);
               charset_j2p(log_entry, 255, char_set);
               dlp_AddSyncLogEntry(sd, log_entry);
               g_snprintf(log_entry, sizeof(log_entry), _("Too many categories on remote.\n"));
               charset_j2p(log_entry, sizeof(log_entry), char_set);
               dlp_AddSyncLogEntry(sd, log_entry);
               g_snprintf(log_entry, sizeof(log_entry), _("All records on desktop in %s will be moved to %s.\n"), local_cai.name[Li], local_cai.name[0]);
               charset_j2p(log_entry, sizeof(log_entry), char_set);
               dlp_AddSyncLogEntry(sd, log_entry);

               jp_logf(JP_LOG_DEBUG, "Moving local recs category %d to Unfiled...", Li);
               edit_cats_change_cats_pc3(DB_name, Li, 0);
               edit_cats_change_cats_pdb(DB_name, Li, 0);
            }
            continue;
         }
      }
#ifdef SYNC_CAT_DEBUG
      printf("Error: cat index %d passed through with no processing\n", Li);
#endif
   }
   /* Move records locally into correct index slots */
   /* Note that this happens in reverse order so that A->B, B->C, does not
    * result in records from A and B in category C */
   for (i=move_i-1; i>=0; i--) {
      if (move_from_idx[i]) {
         pdb_file_change_indexes(DB_name, move_from_idx[i], move_to_idx[i]);
         edit_cats_change_cats_pc3(DB_name, move_from_idx[i], move_to_idx[i]);
      }
   }

   /* Clear the rename flags now that sync has occurred */
   for (i=0; i<NUM_CATEGORIES; i++) {
      remote_cai.renamed[i]=0;
   }

   /* Clear any ID fields for blank slots */
   for (i=0; i<NUM_CATEGORIES; i++) {
      if (remote_cai.name[i][0]=='\0') {
         remote_cai.ID[i] = 0;
      }
   }

#ifdef SYNC_CAT_DEBUG
   printf("--- post-sync CategoryAppInfo\n");
   printf("--- DB_name [%s]\n", DB_name);
   for (i=0; i<NUM_CATEGORIES; i++) {
      printf("local: cat %d [%s] ID %d renamed %d\n", i,
             local_cai.name[i],
             local_cai.ID[i], local_cai.renamed[i]);
   }
   printf("-----------------------------\n");
   for (i=0; i<NUM_CATEGORIES; i++) {
      printf("remote: cat %d [%s] ID %d renamed %d\n", i,
             remote_cai.name[i],
             remote_cai.ID[i], remote_cai.renamed[i]);
   }
#endif

   pack_cai_into_ai(&remote_cai, buf, remote_cai_size);

   jp_logf(JP_LOG_DEBUG, "writing out new categories for %s\n", DB_name);
   dlp_WriteAppBlock(sd, db, buf, remote_cai_size);
   pdb_file_write_app_block(DB_name, buf, remote_cai_size);

   dlp_CloseDB(sd, db);

   return EXIT_SUCCESS;
}

static int slow_sync_application(char *DB_name, int sd)
{
   int db;
   int ret;
   int num;
   FILE *pc_in;
   char pc_filename[FILENAME_MAX];
   PC3RecordHeader header;
   unsigned long new_unique_id;
   /* local (.pc3) record */
   void *lrec;
   int lrec_len;
   /* remote (Palm) record */
   pi_buffer_t *rrec;
   int  rindex, rattr, rcategory;
   size_t rrec_len;
   long char_set;
   char log_entry[256];
   char write_log_message[256];
   char error_log_message_w[256];
   char error_log_message_d[256];
   char delete_log_message[256];
   char conflict_log_message[256];
   int  same;

   jp_logf(JP_LOG_DEBUG, "slow_sync_application\n");

   if ((DB_name==NULL) || (strlen(DB_name) == 0) || (strlen(DB_name) > 250)) {
      return EXIT_FAILURE;
   }

   g_snprintf(log_entry, sizeof(log_entry), _("Syncing %s\n"), DB_name);
   jp_logf(JP_LOG_GUI, log_entry);

   get_pref(PREF_CHAR_SET, &char_set, NULL);

   /* This is an attempt to use the proper pronoun most of the time */
   if (strchr("aeiou", tolower(DB_name[0]))) {
      g_snprintf(write_log_message, sizeof(write_log_message),
              _("Wrote an %s record."), DB_name);
      g_snprintf(error_log_message_w, sizeof(error_log_message_w),
              _("Writing an %s record failed."), DB_name);
      g_snprintf(error_log_message_d, sizeof(error_log_message_d),
              _("Deleting an %s record failed."), DB_name);
      g_snprintf(delete_log_message, sizeof(delete_log_message),
              _("Deleted an %s record."), DB_name);
      g_snprintf(conflict_log_message, sizeof(conflict_log_message),
              _("Sync Conflict: duplicated an %s record."), DB_name);
   } else {
      g_snprintf(write_log_message, sizeof(write_log_message),
              _("Wrote a %s record."), DB_name);
      g_snprintf(error_log_message_w, sizeof(error_log_message_w),
              _("Writing a %s record failed."), DB_name);
      g_snprintf(error_log_message_d, sizeof(error_log_message_d),
              _("Deleting a %s record failed."), DB_name);
      g_snprintf(delete_log_message, sizeof(delete_log_message),
              _("Deleted a %s record."), DB_name);
      g_snprintf(conflict_log_message, sizeof(conflict_log_message),
              _("Sync Conflict: duplicated a %s record."), DB_name);
   }

   g_snprintf(pc_filename, sizeof(pc_filename), "%s.pc3", DB_name);
   pc_in = jp_open_home_file(pc_filename, "r+");
   if (pc_in==NULL) {
      jp_logf(JP_LOG_WARN, _("Unable to open file: %s\n"), pc_filename);
      return EXIT_FAILURE;
   }
   /* Open the applications database, store access handle in db */
   ret = dlp_OpenDB(sd, 0, dlpOpenReadWrite, DB_name, &db);
   if (ret < 0) {
      g_snprintf(log_entry, sizeof(log_entry), _("Unable to open file: %s\n"), DB_name);
      charset_j2p(log_entry, sizeof(log_entry), char_set);
      dlp_AddSyncLogEntry(sd, log_entry);
      jp_logf(JP_LOG_WARN, "slow_sync_application: %s", log_entry);
      fclose(pc_in);
      return EXIT_FAILURE;
   }

#ifdef JPILOT_DEBUG
   dlp_ReadOpenDBInfo(sd, db, &num);
   jp_logf(JP_LOG_GUI , "number of records = %d\n", num);
#endif

   /* Loop over records in .pc3 file */
   while (!feof(pc_in)) {
      num = read_header(pc_in, &header);
      if (num!=1) {
         if (ferror(pc_in)) break;
         if (feof(pc_in))   break;
      }

      lrec_len = header.rec_len;
      if (lrec_len > 0x10000) {
         jp_logf(JP_LOG_WARN, _("PC file corrupt?\n"));
         fclose(pc_in);
         dlp_CloseDB(sd, db);
         return EXIT_FAILURE;
      }

      /* Case 5: */
      if ((header.rt==NEW_PC_REC) || (header.rt==REPLACEMENT_PALM_REC)) {
         jp_logf(JP_LOG_DEBUG, "Case 5: new pc record\n");

         lrec = malloc(lrec_len);
         if (!lrec) {
            jp_logf(JP_LOG_WARN, "slow_sync_application(): %s\n", _("Out of memory"));
            break;
         }
         num = fread(lrec, lrec_len, 1, pc_in);
         if (num != 1) {
            if (ferror(pc_in)) {
               free(lrec);
               break;
            }
         }

         if (header.rt==REPLACEMENT_PALM_REC) {
            /* A replacement must be checked against pdb file to make sure 
             * a simultaneous modification on the Palm has not occurred */
            rrec = pi_buffer_new(65536);
            if (!rrec) {
               jp_logf(JP_LOG_WARN, "slow_sync_application(), pi_buffer_new: %s\n",
                                  _("Out of memory"));
               free(lrec);
               break;
            }

            ret = dlp_ReadRecordById(sd, db, header.unique_id, rrec,
                                     &rindex, &rattr, &rcategory);
            rrec_len = rrec->used;
#ifdef JPILOT_DEBUG
            if (ret>=0 ) {
               printf("read record by id %s returned %d\n", DB_name, ret);
               printf("id %ld, index %d, size %d, attr 0x%x, category %d\n",
                      header.unique_id, rindex, rrec_len, rattr, rcategory);
            } else {
               printf("Case 5: read record by id failed\n");
            }
#endif
            if ((ret >=0) && !(dlpRecAttrDeleted & rattr)) {
               /* Modified record was also found on Palm. */
               /* Compare records but ignore category changes */
               /* For simultaneous category changes PC wins */
               same = match_records(DB_name, 
                                    rrec->data, rrec_len, rattr, 0,
                                    lrec, lrec_len, header.attrib&0xF0, 0);
#ifdef JPILOT_DEBUG
               printf("Same is %d\n", same);
#endif
               if (same && (header.unique_id != 0)) {
                  /* Records are the same. Add pc3 record over Palm one */
                  jp_logf(JP_LOG_DEBUG, "Case 5: lrec & rrec match, keeping Jpilot version\n");
               } else {
                  /* Record has been changed on the palm as well as the PC. 
                   *
                   * The changed record has already been transferred to the 
                   * local pdb file from the Palm. It must be copied to the 
                   * Palm and to the local pdb file under a new unique ID 
                   * in order to prevent overwriting by the modified PC record.
                   * The record is placed in the Unfiled category so it can 
                   * be quickly located. */
                  jp_logf(JP_LOG_DEBUG, "Case 5: duplicating record\n");
                  jp_logf(JP_LOG_GUI, _("Sync Conflict: a %s record must be manually merged\n"), DB_name);

                  /* Write record to Palm and get new unique ID */
                  jp_logf(JP_LOG_DEBUG, "Duplicating PC record to palm\n");
                  ret = dlp_WriteRecord(sd, db, rattr & dlpRecAttrSecret,
                                        0, 0,
                                        rrec->data, rrec_len, &new_unique_id);

                  if (ret < 0) {
                     jp_logf(JP_LOG_WARN, "dlp_WriteRecord failed\n");
                     charset_j2p(error_log_message_w,255,char_set);
                     dlp_AddSyncLogEntry(sd, error_log_message_w);
                     dlp_AddSyncLogEntry(sd, "\n");
                  } else {
                     charset_j2p(conflict_log_message,255,char_set);
                     dlp_AddSyncLogEntry(sd, conflict_log_message);
                     dlp_AddSyncLogEntry(sd, "\n");
                  }
               }
            }

            pi_buffer_free(rrec);

         } /* endif REPLACEMENT_PALM_REC */

         jp_logf(JP_LOG_DEBUG, "Writing PC record to palm\n");

         if (header.rt==REPLACEMENT_PALM_REC) {
            ret = dlp_WriteRecord(sd, db, header.attrib & dlpRecAttrSecret,
                                  header.unique_id, header.attrib & 0x0F,
                                  lrec, lrec_len, &header.unique_id);
         } else {
            ret = dlp_WriteRecord(sd, db, header.attrib & dlpRecAttrSecret,
                                  0, header.attrib & 0x0F,
                                  lrec, lrec_len, &header.unique_id);
         }

         if (lrec) {
            free(lrec);
            lrec = NULL;
         }

         if (ret < 0) {
            jp_logf(JP_LOG_WARN, "dlp_WriteRecord failed\n");
            charset_j2p(error_log_message_w,255,char_set);
            dlp_AddSyncLogEntry(sd, error_log_message_w);
            dlp_AddSyncLogEntry(sd, "\n");
         } else {
            charset_j2p(write_log_message,255,char_set);
            dlp_AddSyncLogEntry(sd, write_log_message);
            dlp_AddSyncLogEntry(sd, "\n");
            /* mark the record as deleted in the pc file */
            if (fseek(pc_in, -(header.header_len+lrec_len), SEEK_CUR)) {
               jp_logf(JP_LOG_WARN, _("fseek failed - fatal error\n"));
               fclose(pc_in);
               dlp_CloseDB(sd, db);
               return EXIT_FAILURE;
            }
            header.rt=DELETED_PC_REC;
            write_header(pc_in, &header);
         }
      } /* endif Case 5 */

      /* Case 3 & 4: */
      if ((header.rt==DELETED_PALM_REC) || (header.rt==MODIFIED_PALM_REC)) {
         jp_logf(JP_LOG_DEBUG, "Case 3&4: deleted or modified pc record\n");
         lrec = malloc(lrec_len);
         if (!lrec) {
            jp_logf(JP_LOG_WARN, "slow_sync_application(): %s\n", 
                               _("Out of memory"));
            break;
         }
         num = fread(lrec, lrec_len, 1, pc_in);
         if (num != 1) {
            if (ferror(pc_in)) {
               free(lrec);
               break;
            }
         }

         rrec = pi_buffer_new(65536);
         if (!rrec) {
            jp_logf(JP_LOG_WARN, "slow_sync_application(), pi_buffer_new: %s\n",
                               _("Out of memory"));
            free(lrec);
            break;
         }

         ret = dlp_ReadRecordById(sd, db, header.unique_id, rrec,
                                  &rindex, &rattr, &rcategory);
         rrec_len = rrec->used;
#ifdef JPILOT_DEBUG
         if (ret>=0 ) {
            printf("read record by id %s returned %d\n", DB_name, ret);
            printf("id %ld, index %d, size %d, attr 0x%x, category %d\n",
                   header.unique_id, rindex, rrec_len, rattr, rcategory);
         } else {
            printf("Case 3&4: read record by id failed\n");
         }
#endif

         if ((ret < 0) || (dlpRecAttrDeleted & rattr)) {
            /* lrec can't be found which means it has already 
             * been deleted from the Palm side.
             * Mark the local record as deleted */
            jp_logf(JP_LOG_DEBUG, "Case 3&4: no remote record found, must have been deleted on the Palm\n");
            if (fseek(pc_in, -(header.header_len+lrec_len), SEEK_CUR)) {
               jp_logf(JP_LOG_WARN, _("fseek failed - fatal error\n"));
               fclose(pc_in);
               dlp_CloseDB(sd, db);
               free(lrec);
               pi_buffer_free(rrec);
               return EXIT_FAILURE;
            }
            header.rt=DELETED_DELETED_PALM_REC;
            write_header(pc_in, &header);
         } else {
            /* Record exists on the palm and has been deleted from PC 
             * If the two records are the same, then no changes have
             * occurred on the Palm and the deletion should occur */
            same = match_records(DB_name, 
                                 rrec->data, rrec_len, rattr, rcategory,
                                 lrec, lrec_len, header.attrib&0xF0,
                                                 header.attrib&0x0F);
#ifdef JPILOT_DEBUG
            printf("Same is %d\n", same);
#endif
            if (same && (header.unique_id != 0)) {
               jp_logf(JP_LOG_DEBUG, "Case 3&4: lrec & rrec match, deleting\n");
               ret = dlp_DeleteRecord(sd, db, 0, header.unique_id);
               if (ret < 0) {
                  jp_logf(JP_LOG_WARN, _("dlp_DeleteRecord failed\n"\
                  "This could be because the record was already deleted on the Palm\n"));
                  charset_j2p(error_log_message_d,255,char_set);
                  dlp_AddSyncLogEntry(sd, error_log_message_d);
                  dlp_AddSyncLogEntry(sd, "\n");
               } else {
                  charset_j2p(delete_log_message,255,char_set);
                  dlp_AddSyncLogEntry(sd, delete_log_message);
                  dlp_AddSyncLogEntry(sd, "\n");
               }
               
               /* Now mark the record in pc3 file as deleted */
               if (fseek(pc_in, -(header.header_len+lrec_len), SEEK_CUR)) {
                  jp_logf(JP_LOG_WARN, _("fseek failed - fatal error\n"));
                  fclose(pc_in);
                  dlp_CloseDB(sd, db);
                  free(lrec);
                  pi_buffer_free(rrec);
                  return EXIT_FAILURE;
               }
               header.rt=DELETED_DELETED_PALM_REC;
               write_header(pc_in, &header);

            } else {
               /* Record has been changed on the palm and deletion can't occur
                * Mark the pc3 record as having been dealt with */
               jp_logf(JP_LOG_DEBUG, "Case 3: skipping PC deleted record\n");
               if (fseek(pc_in, -(header.header_len+lrec_len), SEEK_CUR)) {
                  jp_logf(JP_LOG_WARN, _("fseek failed - fatal error\n"));
                  fclose(pc_in);
                  dlp_CloseDB(sd, db);
                  free(lrec);
                  pi_buffer_free(rrec);
                  return EXIT_FAILURE;
               }
               header.rt=DELETED_PC_REC;
               write_header(pc_in, &header);
            } /* end if checking whether old & new records are the same */

            /* free buffers */
            if (lrec) {
               free(lrec);
               lrec = NULL;
            }

            pi_buffer_free(rrec);

         } /* record found on Palm */
      } /* end if Case 3&4 */

      /* move to next record in .pc3 file */
      if (fseek(pc_in, lrec_len, SEEK_CUR)) {
         jp_logf(JP_LOG_WARN, _("fseek failed - fatal error\n"));
         fclose(pc_in);
         dlp_CloseDB(sd, db);
         return EXIT_FAILURE;
      }

   } /* end while on feof(pc_in) */

   fclose(pc_in);
#ifdef JPILOT_DEBUG
   dlp_ReadOpenDBInfo(sd, db, &num);
   jp_logf(JP_LOG_WARN ,"number of records = %d\n", num);
#endif
   dlp_ResetSyncFlags(sd, db);
   dlp_CleanUpDatabase(sd, db);
   dlp_CloseDB(sd, db);

   return EXIT_SUCCESS;
}

static int fast_sync_local_recs(char *DB_name, int sd, int db)
{
   int ret;
   int num;
   FILE *pc_in;
   char pc_filename[FILENAME_MAX];
   PC3RecordHeader header;
   unsigned long orig_unique_id, new_unique_id;
   void *lrec;  /* local (.pc3) record */
   int  lrec_len;
   void *rrec;  /* remote (Palm) record */
   int  rindex, rattr, rcategory;
   size_t rrec_len;
   long char_set;
   char write_log_message[256];
   char error_log_message_w[256];
   char error_log_message_d[256];
   char delete_log_message[256];
   char conflict_log_message[256];
   int same;

   jp_logf(JP_LOG_DEBUG, "fast_sync_local_recs\n");
   get_pref(PREF_CHAR_SET, &char_set, NULL);

   /* This is an attempt to use the proper pronoun most of the time */
   if (strchr("aeiou", tolower(DB_name[0]))) {
      g_snprintf(write_log_message, sizeof(write_log_message),
              _("Wrote an %s record."), DB_name);
      g_snprintf(error_log_message_w, sizeof(error_log_message_w),
              _("Writing an %s record failed."), DB_name);
      g_snprintf(error_log_message_d, sizeof(error_log_message_d),
              _("Deleting an %s record failed."), DB_name);
      g_snprintf(delete_log_message, sizeof(delete_log_message),
              _("Deleted an %s record."), DB_name);
      g_snprintf(conflict_log_message, sizeof(conflict_log_message),
              _("Sync Conflict: duplicated an %s record."), DB_name);
   } else {
      g_snprintf(write_log_message, sizeof(write_log_message),
              _("Wrote a %s record."), DB_name);
      g_snprintf(error_log_message_w, sizeof(error_log_message_w),
              _("Writing a %s record failed."), DB_name);
      g_snprintf(error_log_message_d, sizeof(error_log_message_d),
              _("Deleting a %s record failed."), DB_name);
      g_snprintf(delete_log_message, sizeof(delete_log_message),
              _("Deleted a %s record."), DB_name);
      g_snprintf(conflict_log_message, sizeof(conflict_log_message),
              _("Sync Conflict: duplicated a %s record."), DB_name);
   }
   g_snprintf(pc_filename, sizeof(pc_filename), "%s.pc3", DB_name);
   pc_in = jp_open_home_file(pc_filename, "r+");
   if (pc_in==NULL) {
      jp_logf(JP_LOG_WARN, _("Unable to open file: %s\n"), pc_filename);
      return EXIT_FAILURE;
   }

   /* Loop over records in .pc3 file */
   while (!feof(pc_in)) {
      num = read_header(pc_in, &header);
      if (num!=1) {
         if (ferror(pc_in)) break;
         if (feof(pc_in))   break;
      }

      lrec_len = header.rec_len;
      if (lrec_len > 0x10000) {
         jp_logf(JP_LOG_WARN, _("PC file corrupt?\n"));
         fclose(pc_in);
         return EXIT_FAILURE;
      }

      /* Case 5: */
      if ((header.rt==NEW_PC_REC) || (header.rt==REPLACEMENT_PALM_REC)) {
         jp_logf(JP_LOG_DEBUG, "Case 5: new pc record\n");

         lrec = malloc(lrec_len);
         if (!lrec) {
            jp_logf(JP_LOG_WARN, "fast_sync_local_recs(): %s\n", 
                               _("Out of memory"));
            break;
         }
         num = fread(lrec, lrec_len, 1, pc_in);
         if (num != 1) {
            if (ferror(pc_in)) {
               free(lrec);
               break;
            }
         }

         if (header.rt==REPLACEMENT_PALM_REC) {
            /* A replacement must be checked against pdb file to make sure 
             * a simultaneous modification on the Palm has not occurred */
            ret = pdb_file_read_record_by_id(DB_name,
                                             header.unique_id,
                                             &rrec, &rrec_len, &rindex,
                                             &rattr, &rcategory);
#ifdef JPILOT_DEBUG
            if (ret>=0 ) {
               printf("read record by id %s returned %d\n", DB_name, ret);
               printf("id %ld, index %d, size %d, attr 0x%x, category %d\n",
                      header.unique_id, rindex, rrec_len, rattr, rcategory);
            } else {
               printf("Case 5: read record by id failed\n");
            }
#endif
            if (ret >=0) {
               /* Modified record was also found on Palm. */
               /* Compare records but ignore category changes */
               /* For simultaneous category changes PC wins */
               same = match_records(DB_name, 
                                    rrec, rrec_len, rattr, 0,
                                    lrec, lrec_len, header.attrib&0xF0, 0);
   #ifdef JPILOT_DEBUG
               printf("Same is %d\n", same);
   #endif
               if (same && (header.unique_id != 0)) {
                  /* Records are the same. Add pc3 record over Palm one */
                  jp_logf(JP_LOG_DEBUG, "Case 5: lrec & rrec match, keeping Jpilot version\n");
               } else {
                  /* Record has been changed on the palm as well as the PC. 
                   *
                   * The changed record has already been transferred to the 
                   * local pdb file from the Palm. It must be copied to the 
                   * Palm and to the local pdb file under a new unique ID 
                   * in order to prevent overwriting by the modified PC record.
                   * The record is placed in the Unfiled category so it can 
                   * be quickly located. */
                  jp_logf(JP_LOG_DEBUG, "Case 5: duplicating record\n");
                  jp_logf(JP_LOG_GUI, _("Sync Conflict: a %s record must be manually merged\n"), DB_name);

                  /* Write record to Palm and get new unique ID */
                  jp_logf(JP_LOG_DEBUG, "Duplicating PC record to palm\n");
                  ret = dlp_WriteRecord(sd, db, rattr & dlpRecAttrSecret,
                                        0, 0,
                                        rrec, rrec_len, &new_unique_id);

                  /* Write record to local pdb file */
                  jp_logf(JP_LOG_DEBUG, "Duplicating PC record to local\n");
                  if (ret >=0) {
                     pdb_file_modify_record(DB_name, rrec, rrec_len,
                                            rattr & dlpRecAttrSecret,
                                            0, new_unique_id);
                  }

                  if (ret < 0) {
                     jp_logf(JP_LOG_WARN, "dlp_WriteRecord failed\n");
                     charset_j2p(error_log_message_w,255,char_set);
                     dlp_AddSyncLogEntry(sd, error_log_message_w);
                     dlp_AddSyncLogEntry(sd, "\n");
                  } else {
                     charset_j2p(conflict_log_message,255,char_set);
                     dlp_AddSyncLogEntry(sd, conflict_log_message);
                     dlp_AddSyncLogEntry(sd, "\n");
                  }
               }
            }
         } /* endif REPLACEMENT_PALM_REC */

         jp_logf(JP_LOG_DEBUG, "Writing PC record to palm\n");
       
         orig_unique_id = header.unique_id;

         if (header.rt==REPLACEMENT_PALM_REC) {
            ret = dlp_WriteRecord(sd, db, header.attrib & dlpRecAttrSecret,
                                  header.unique_id, header.attrib & 0x0F,
                                  lrec, lrec_len, &header.unique_id);
         } else {
            ret = dlp_WriteRecord(sd, db, header.attrib & dlpRecAttrSecret,
                                  0, header.attrib & 0x0F,
                                  lrec, lrec_len, &header.unique_id);
         }

         jp_logf(JP_LOG_DEBUG, "Writing PC record to local\n");
         if (ret >=0) {
            if ((header.rt==REPLACEMENT_PALM_REC) &&
                (orig_unique_id != header.unique_id)) {
               /* There is a possibility that the palm handed back a unique ID
                * other than the one we requested */
               pdb_file_delete_record_by_id(DB_name, orig_unique_id);
            }
            pdb_file_modify_record(DB_name, lrec, lrec_len,
                                   header.attrib & dlpRecAttrSecret,
                                   header.attrib & 0x0F, header.unique_id);
         }

         if (lrec) {
            free(lrec);
            lrec = NULL;
         }

         if (ret < 0) {
            jp_logf(JP_LOG_WARN, "dlp_WriteRecord failed\n");
            charset_j2p(error_log_message_w,255,char_set);
            dlp_AddSyncLogEntry(sd, error_log_message_w);
            dlp_AddSyncLogEntry(sd, "\n");
         } else {
            charset_j2p(write_log_message,255,char_set);
            dlp_AddSyncLogEntry(sd, write_log_message);
            dlp_AddSyncLogEntry(sd, "\n");
            /* mark the record as deleted in the pc file */
            if (fseek(pc_in, -(header.header_len+lrec_len), SEEK_CUR)) {
               jp_logf(JP_LOG_WARN, _("fseek failed - fatal error\n"));
               fclose(pc_in);
               return EXIT_FAILURE;
            }
            header.rt=DELETED_PC_REC;
            write_header(pc_in, &header);
         }

      } /* endif Case 5 */

      /* Case 3 & 4: */
      if ((header.rt==DELETED_PALM_REC) || (header.rt==MODIFIED_PALM_REC)) {
         jp_logf(JP_LOG_DEBUG, "Case 3&4: deleted or modified pc record\n");
         lrec = malloc(lrec_len);
         if (!lrec) {
            jp_logf(JP_LOG_WARN, "fast_sync_local_recs(): %s\n", 
                               _("Out of memory"));
            break;
         }
         num = fread(lrec, lrec_len, 1, pc_in);
         if (num != 1) {
            if (ferror(pc_in)) {
               free(lrec);
               break;
            }
         }
         ret = pdb_file_read_record_by_id(DB_name,
                                          header.unique_id,
                                          &rrec, &rrec_len, &rindex,
                                          &rattr, &rcategory);
#ifdef JPILOT_DEBUG
         if (ret>=0 ) {
            printf("read record by id %s returned %d\n", DB_name, ret);
            printf("id %ld, index %d, size %d, attr 0x%x, category %d\n",
                   header.unique_id, rindex, rrec_len, rattr, rcategory);
         } else {
            printf("Case 3&4: read record by id failed\n");
         }
#endif
         if (ret < 0) {
            /* lrec can't be found in pdb file which means it
             * has already been deleted from the Palm side.
             * Mark the local record as deleted */
            jp_logf(JP_LOG_DEBUG, "Case 3&4: no remote record found, must have been deleted on the Palm\n");
            if (fseek(pc_in, -(header.header_len+lrec_len), SEEK_CUR)) {
               jp_logf(JP_LOG_WARN, _("fseek failed - fatal error\n"));
               fclose(pc_in);
               free(lrec);
               free(rrec);
               return EXIT_FAILURE;
            }
            header.rt=DELETED_DELETED_PALM_REC;
            write_header(pc_in, &header);
         } else {
            /* Record exists on the palm and has been deleted from PC 
             * If the two records are the same, then no changes have
             * occurred on the Palm and the deletion should occur */
            same = match_records(DB_name, 
                                 rrec, rrec_len, rattr, rcategory,
                                 lrec, lrec_len, header.attrib&0xF0,
                                                 header.attrib&0x0F);
#ifdef JPILOT_DEBUG
            printf("Same is %d\n", same);
#endif
            if (same && (header.unique_id != 0)) {
               jp_logf(JP_LOG_DEBUG, "Case 3&4: lrec & rrec match, deleting\n");
               ret = dlp_DeleteRecord(sd, db, 0, header.unique_id);
               if (ret < 0) {
                  jp_logf(JP_LOG_WARN, _("dlp_DeleteRecord failed\n"
                                         "This could be because the record was already deleted on the Palm\n"));
                  charset_j2p(error_log_message_d,255,char_set);
                  dlp_AddSyncLogEntry(sd, error_log_message_d);
                  dlp_AddSyncLogEntry(sd, "\n");
               } else {
                  charset_j2p(delete_log_message,255,char_set);
                  dlp_AddSyncLogEntry(sd, delete_log_message);
                  dlp_AddSyncLogEntry(sd, "\n");
                  pdb_file_delete_record_by_id(DB_name, header.unique_id);
               }
               
               /* Now mark the record in pc3 file as deleted */
               if (fseek(pc_in, -(header.header_len+lrec_len), SEEK_CUR)) {
                  jp_logf(JP_LOG_WARN, _("fseek failed - fatal error\n"));
                  fclose(pc_in);
                  free(lrec);
                  free(rrec);
                  return EXIT_FAILURE;
               }
               header.rt=DELETED_DELETED_PALM_REC;
               write_header(pc_in, &header);
            } else {
               /* Record has been changed on the palm and deletion can't occur
                * Mark the pc3 record as having been dealt with */
               jp_logf(JP_LOG_DEBUG, "Case 3: skipping PC deleted record\n");
               if (fseek(pc_in, -(header.header_len+lrec_len), SEEK_CUR)) {
                  jp_logf(JP_LOG_WARN, _("fseek failed - fatal error\n"));
                  fclose(pc_in);
                  free(lrec);
                  free(rrec);
                  return EXIT_FAILURE;
               }
               header.rt=DELETED_PC_REC;
               write_header(pc_in, &header);
            } /* end if checking whether old & new records are the same */

            /* free buffers */
            if (lrec) {
               free(lrec);
               lrec = NULL;
            }

            if (rrec) {
               free(rrec);
               rrec = NULL;
            }
         } /* record found on Palm */
      } /* end if Case 3&4 */

      /* move to next record in .pc3 file */
      if (fseek(pc_in, lrec_len, SEEK_CUR)) {
         jp_logf(JP_LOG_WARN, _("fseek failed - fatal error\n"));
         fclose(pc_in);
         return EXIT_FAILURE;
      }

   } /* end while on feof(pc_in) */

   fclose(pc_in);

   return EXIT_SUCCESS;
}


/*
 * This code does not do archiving.
 *
 * For each remote record (RR):
 *   Case 1:
 *   if RR deleted or archived
 *     remove local record (LR)
 *   Case 2:
 *   if RR changed
 *     change LR, If LR doesn't exist then add it
 *
 * For each local record (LR):
 *   Case 3:
 *   if LR deleted
 *     if RR==OLR (Original LR) remove both RR and LR
 *   Case 4:
 *   if LR changed
 *     We have a new local record (NLR) and a
 *     modified (deleted) local record (MLR)
 *     if NLR==RR then do nothing (either both were changed equally, or
 *                                 local was changed and changed back)
 *     otherwise,
 *       add NLR to remote
 *       if RR==LR remove RR
 *   Case 5:
 *   if new LR
 *     add LR to remote
 */
static int fast_sync_application(char *DB_name, int sd)
{
   int db;
   int ret;
   long char_set;
   char log_entry[256];
   char write_log_message[256];
   char error_log_message_w[256];
   char error_log_message_d[256];
   char delete_log_message[256];
   /* remote (Palm) record */
   pi_buffer_t *rrec;
   recordid_t rid=0;
   int rindex, rrec_len, rattr, rcategory;
   int num_local_recs, num_palm_recs;
   char *extra_dbname[2];

   jp_logf(JP_LOG_DEBUG, "fast_sync_application %s\n", DB_name);

   if ((DB_name==NULL) || (strlen(DB_name) == 0) || (strlen(DB_name) > 250)) {
      return EXIT_FAILURE;
   }

   g_snprintf(log_entry, sizeof(log_entry), _("Syncing %s\n"), DB_name);
   jp_logf(JP_LOG_GUI, log_entry);

   get_pref(PREF_CHAR_SET, &char_set, NULL);

   /* This is an attempt to use the proper pronoun most of the time */
   if (strchr("aeiou", tolower(DB_name[0]))) {
      g_snprintf(write_log_message, sizeof(write_log_message),
              _("Wrote an %s record."),  DB_name);
      g_snprintf(error_log_message_w, sizeof(error_log_message_w),
              _("Writing an %s record failed."), DB_name);
      g_snprintf(error_log_message_d, sizeof(error_log_message_d),
              _("Deleting an %s record failed."), DB_name);
      g_snprintf(delete_log_message, sizeof(delete_log_message),
              _("Deleted an %s record."),  DB_name);
   } else {
      g_snprintf(write_log_message, sizeof(write_log_message),
              _("Wrote a %s record."),  DB_name);
      g_snprintf(error_log_message_w, sizeof(error_log_message_w),
              _("Writing a %s record failed."), DB_name);
      g_snprintf(error_log_message_d, sizeof(error_log_message_d),
              _("Deleting a %s record failed."), DB_name);
      g_snprintf(delete_log_message, sizeof(delete_log_message),
              _("Deleted a %s record."),  DB_name);
   }
   /* Open the applications database, store access handle in db */
   ret = dlp_OpenDB(sd, 0, dlpOpenReadWrite|dlpOpenSecret, DB_name, &db);
   if (ret < 0) {
      g_snprintf(log_entry, sizeof(log_entry), _("Unable to open file: %s\n"), DB_name);
      charset_j2p(log_entry, sizeof(log_entry), char_set);
      dlp_AddSyncLogEntry(sd, log_entry);
      jp_logf(JP_LOG_WARN, "fast_sync_application: %s", log_entry);
      return EXIT_FAILURE;
   }

   /* I can't get the appinfodirty flag to work, so I do this for now */
   /* ret = dlp_ReadAppBlock(sd, db, 0, rrec, 65535);
   jp_logf(JP_LOG_DEBUG, "readappblock ret=%d\n", ret);
   if (ret>0) {
      pdb_file_write_app_block(DB_name, rrec, ret);
   }*/

   /* Loop over all Palm records with dirty bit set */
   while(1) {
      rrec = pi_buffer_new(0);
      ret = dlp_ReadNextModifiedRec(sd, db, rrec,
                                    &rid, &rindex, &rattr, &rcategory);
      rrec_len = rrec->used;
      if (ret>=0 ) {
         jp_logf(JP_LOG_DEBUG, "read next record for %s returned %d\n", DB_name, ret);
         jp_logf(JP_LOG_DEBUG, "id %ld, index %d, size %d, attr 0x%x, category %d\n",rid, rindex, rrec_len, rattr, rcategory);
      } else {
         pi_buffer_free(rrec);
         break;
      }

      /* Case 1: */
      if ((rattr & dlpRecAttrDeleted) || (rattr & dlpRecAttrArchived)) {
         jp_logf(JP_LOG_DEBUG, "Case 1: found a deleted record on palm\n");
         pdb_file_delete_record_by_id(DB_name, rid);
         pi_buffer_free(rrec);
         continue;
      }

      /* Case 2: */
      if (rattr & dlpRecAttrDirty) {
         jp_logf(JP_LOG_DEBUG, "Case 2: found a dirty record on palm\n");
         pdb_file_modify_record(DB_name, rrec->data, rrec->used, rattr, rcategory, rid);
         pi_buffer_free(rrec);
         continue;
      }

      pi_buffer_free(rrec);
   } /* end while over Palm records */

   fast_sync_local_recs(DB_name, sd, db);

   dlp_ResetSyncFlags(sd, db);
   dlp_CleanUpDatabase(sd, db);

   /* Count the number of records, should be equal, may not be */
   dlp_ReadOpenDBInfo(sd, db, &num_palm_recs);
   pdb_file_count_recs(DB_name, &num_local_recs);

   dlp_CloseDB(sd, db);

   if (num_local_recs != num_palm_recs) {
      extra_dbname[0] = DB_name;
      extra_dbname[1] = NULL;
      jp_logf(JP_LOG_DEBUG, "fetch_extra_DBs() [%s]\n", extra_dbname[0]);
      jp_logf(JP_LOG_DEBUG, "palm: number of records = %d\n", num_palm_recs);
      jp_logf(JP_LOG_DEBUG, "disk: number of records = %d\n", num_local_recs);
      fetch_extra_DBs(sd, extra_dbname);
   }

   return EXIT_SUCCESS;
}

static int jp_install_user(const char *device, int sd, 
                           struct my_sync_info *sync_info)
{
   struct PilotUser U;

   U.userID=sync_info->userID;
   U.viewerID=0;
   U.lastSyncPC=0;
   strncpy(U.username, sync_info->username, sizeof(U.username));

   dlp_WriteUserInfo(sd, &U);

   dlp_EndOfSync(sd, 0);
   pi_close(sd);

   jp_logf(JP_LOG_GUI, _("Finished installing user information.\n"));
   return EXIT_SUCCESS;
}

static int jp_sync(struct my_sync_info *sync_info)
{
   int sd;
   int ret;
   struct PilotUser U;
   const char *device;
   char default_device[]="/dev/pilot";
   int found=0, fast_sync=0;
#ifdef ENABLE_PLUGINS
   GList *plugin_list, *temp_list;
   struct plugin_s *plugin;
#endif
#ifdef JPILOT_DEBUG
   int start;
   struct DBInfo info;
#endif
#ifdef ENABLE_PRIVATE
   char hex_password[PASSWD_LEN*2+4];
#endif
   char buf[1024];
   long char_set;
#ifdef JPILOT_DEBUG
   pi_buffer_t *buffer;
#endif
   int i;
   /* rename_dbnames is used to modify this list to newer databases if needed */
   char dbname[][32]={
      "DatebookDB",
      "AddressDB",
      "ToDoDB",
      "MemoDB",
#ifdef ENABLE_MANANA
      "MañanaDB",
#endif
      ""
   };
   int pref_sync_array[]={
      PREF_SYNC_DATEBOOK,
      PREF_SYNC_ADDRESS,
      PREF_SYNC_TODO,
      PREF_SYNC_MEMO,
#ifdef ENABLE_MANANA
      PREF_SYNC_MANANA,
#endif
      0
   };

   /* 
    * This is pretty confusing, but necessary.
    * This is an array of pointers to functions and will need to be changed
    * to point to calendar, contacts, tasks, memos, etc. as preferences
    * dictate.
    */
   int (*unpack_cai_from_buf[])(struct CategoryAppInfo *cai, unsigned char *ai_raw, int len) = {
      NULL,
      unpack_address_cai_from_ai,
      unpack_todo_cai_from_ai,
      unpack_memo_cai_from_ai,
      unpack_memo_cai_from_ai,
#ifdef ENABLE_MANANA
      unpack_todo_cai_from_ai,
#endif
      NULL
   };
   int (*pack_cai_into_buf[])(struct CategoryAppInfo *cai, unsigned char *ai_raw, int len) = {
      NULL,
      pack_address_cai_into_ai,
      pack_todo_cai_into_ai,
      pack_memo_cai_into_ai,
      pack_memo_cai_into_ai,
#ifdef ENABLE_MANANA
      pack_todo_cai_into_ai,
#endif
      NULL
   };

   long datebook_version, address_version, todo_version, memo_version;

   /* Convert to new database names if prefs set */
   rename_dbnames(dbname);

   /* Change unpack/pack function pointers if needed */
   get_pref(PREF_DATEBOOK_VERSION, &datebook_version, NULL);
   get_pref(PREF_ADDRESS_VERSION, &address_version, NULL);
   get_pref(PREF_TODO_VERSION, &todo_version, NULL);
   get_pref(PREF_MEMO_VERSION, &memo_version, NULL);
   if (datebook_version==0) {
      unpack_cai_from_buf[0]=unpack_datebook_cai_from_ai;
      pack_cai_into_buf[0]=pack_datebook_cai_into_ai;
   }
   if (datebook_version==1) {
      unpack_cai_from_buf[0]=unpack_calendar_cai_from_ai;
      pack_cai_into_buf[0]=pack_calendar_cai_into_ai;
   }
   if (address_version==1) {
      unpack_cai_from_buf[1]=unpack_contact_cai_from_ai;
      pack_cai_into_buf[1]=pack_contact_cai_into_ai;
   }
   if (todo_version==1) {
      /* FIXME: Uncomment when support for Task has been added
      unpack_cai_from_buf[2]=unpack_task_cai_from_ai;
      pack_cai_into_buf[2]=pack_task_cai_into_ai;
      */
      ;
   }
   if (memo_version==1) {
      /* No change necessary between Memo and Memos databases */
      ;
   }

   /* Load the plugins for a forked process */
#ifdef ENABLE_PLUGINS
   if (!(SYNC_NO_FORK & sync_info->flags) &&
       !(SYNC_NO_PLUGINS & sync_info->flags)) {
      jp_logf(JP_LOG_DEBUG, "sync:calling load_plugins\n");
      load_plugins();
   }
#endif
#ifdef ENABLE_PLUGINS
   /* Do the plugin_pre_sync_pre_connect calls */
   plugin_list = NULL;
   plugin_list = get_plugin_list();

   for (temp_list = plugin_list; temp_list; temp_list = temp_list->next) {
      plugin = (struct plugin_s *)temp_list->data;
      if (plugin) {
         if (plugin->sync_on) {
            if (plugin->plugin_pre_sync_pre_connect) {
               jp_logf(JP_LOG_DEBUG, "sync:calling plugin_pre_sync_pre_connect for [%s]\n", plugin->name);
               plugin->plugin_pre_sync_pre_connect();
            }
         }
      }
   }
#endif

   device = NULL;
   if (sync_info->port) {
      if (sync_info->port[0]) {
         /* A port was passed in to use */
         device=sync_info->port;
         found = 1;
      }
   }
   if (!found) {
      /* No port was passed in, look in env */
      device = getenv("PILOTPORT");
      if (device == NULL) {
         device = default_device;
      }
   }

   jp_logf(JP_LOG_GUI, "****************************************\n");
   jp_logf(JP_LOG_GUI, _(" Syncing on device %s\n"), device);
   jp_logf(JP_LOG_GUI, _(" Press the HotSync button now\n"));
   jp_logf(JP_LOG_GUI, "****************************************\n");

   ret = jp_pilot_connect(&sd, device);
   if (ret) {
      return ret;
   }

   if (SYNC_INSTALL_USER & sync_info->flags) {
      ret = jp_install_user(device, sd, sync_info);
      write_to_parent(PIPE_FINISHED, "\n");
      return ret;
   }

   /* The connection has been established here */
   /* Plugins should call pi_watchdog(); if they are going to be a while */
#ifdef ENABLE_PLUGINS
   /* Do the pre_sync plugin calls */
   plugin_list=NULL;

   plugin_list = get_plugin_list();

   for (temp_list = plugin_list; temp_list; temp_list = temp_list->next) {
      plugin = (struct plugin_s *)temp_list->data;
      if (plugin) {
         if (plugin->sync_on) {
            if (plugin->plugin_pre_sync) {
               jp_logf(JP_LOG_DEBUG, "sync:calling plugin_pre_sync for [%s]\n", plugin->name);
               plugin->plugin_pre_sync();
            }
         }
      }
   }
#endif

   U.username[0]='\0';
   ret = dlp_ReadUserInfo(sd, &U);

   /* Do some checks to see if this is the same palm that was synced
    * the last time */
   if ( (U.userID == 0) &&
      (!(SYNC_RESTORE & sync_info->flags)) ) {
      jp_logf(JP_LOG_GUI, _("Last Synced Username-->\"%s\"\n"), sync_info->username);
      jp_logf(JP_LOG_GUI, _("Last Synced UserID-->\"%d\"\n"), sync_info->userID);
      jp_logf(JP_LOG_GUI, _(" This Username-->\"%s\"\n"), U.username);
      jp_logf(JP_LOG_GUI, _(" This User ID-->%d\n"), U.userID);

      if (SYNC_NO_FORK & sync_info->flags) {
         return SYNC_ERROR_NULL_USERID;
      } else {
         write_to_parent(PIPE_WAITING_ON_USER, "%d:\n", SYNC_ERROR_NULL_USERID);
         ret = wait_for_response(sd);
      }

      if (ret != PIPE_SYNC_CONTINUE) {
         dlp_EndOfSync(sd, 0);
         pi_close(sd);
         return SYNC_ERROR_NULL_USERID;
      }
   }
   if ((sync_info->userID != U.userID) &&
       (sync_info->userID != 0) &&
       (!(SYNC_OVERRIDE_USER & sync_info->flags)) &&
       (!(SYNC_RESTORE & sync_info->flags))) {
      jp_logf(JP_LOG_GUI, _("Last Synced Username-->\"%s\"\n"), sync_info->username);
      jp_logf(JP_LOG_GUI, _("Last Synced UserID-->\"%d\"\n"), sync_info->userID);
      jp_logf(JP_LOG_GUI, _(" This Username-->\"%s\"\n"), U.username);
      jp_logf(JP_LOG_GUI, _(" This User ID-->%d\n"), U.userID);

      if (SYNC_NO_FORK & sync_info->flags) {
         return SYNC_ERROR_NOT_SAME_USERID;
      } else {
         write_to_parent(PIPE_WAITING_ON_USER, "%d:\n", SYNC_ERROR_NOT_SAME_USERID);
         ret = wait_for_response(sd);
      }

      if (ret != PIPE_SYNC_CONTINUE) {
         dlp_EndOfSync(sd, 0);
         pi_close(sd);
         return SYNC_ERROR_NOT_SAME_USERID;
      }
   } else if ((strcmp(sync_info->username, U.username)) &&
              (sync_info->username[0]!='\0') &&
              (!(SYNC_OVERRIDE_USER & sync_info->flags)) &&
              (!(SYNC_RESTORE & sync_info->flags))) {
      jp_logf(JP_LOG_GUI, _("Last Synced Username-->\"%s\"\n"), sync_info->username);
      jp_logf(JP_LOG_GUI, _("Last Synced UserID-->\"%d\"\n"), sync_info->userID);
      jp_logf(JP_LOG_GUI, _(" This Username-->\"%s\"\n"), U.username);
      jp_logf(JP_LOG_GUI, _(" This User ID-->%d\n"), U.userID);
      write_to_parent(PIPE_WAITING_ON_USER, "%d:\n", SYNC_ERROR_NOT_SAME_USER);
      if (SYNC_NO_FORK & sync_info->flags) {
         return SYNC_ERROR_NOT_SAME_USER;
      } else {
         ret = wait_for_response(sd);
      }

      if (ret != PIPE_SYNC_CONTINUE) {
         dlp_EndOfSync(sd, 0);
         pi_close(sd);
         return SYNC_ERROR_NOT_SAME_USER;
      }
   }

   /* User name and User ID is read by the parent process and stored
    * in the preferences.
    * So, this is more than just displaying it to the user */
   if (!(SYNC_RESTORE & sync_info->flags)) {
      write_to_parent(PIPE_USERNAME, "\"%s\"\n", U.username);
      write_to_parent(PIPE_USERID, "%d", U.userID);
      jp_logf(JP_LOG_GUI, _("Username is \"%s\"\n"), U.username);
      jp_logf(JP_LOG_GUI, _("User ID is %d\n"), U.userID);
   }
   jp_logf(JP_LOG_GUI, _("lastSyncPC = %d\n"), U.lastSyncPC);
   jp_logf(JP_LOG_GUI, _("This PC = %lu\n"), sync_info->PC_ID);
   jp_logf(JP_LOG_GUI, "****************************************\n");

   jp_logf(JP_LOG_DEBUG, "Last Username = [%s]\n", sync_info->username);
   jp_logf(JP_LOG_DEBUG, "Last UserID = %d\n", sync_info->userID);
   jp_logf(JP_LOG_DEBUG, "Username = [%s]\n", U.username);
   jp_logf(JP_LOG_DEBUG, "userID = %d\n", U.userID);
   jp_logf(JP_LOG_DEBUG, "lastSyncPC = %d\n", U.lastSyncPC);

#ifdef ENABLE_PRIVATE
   if (U.passwordLength > 0) {
      bin_to_hex_str((unsigned char *)U.password, hex_password,
                     ((U.passwordLength > 0)&&(U.passwordLength < PASSWD_LEN))
                     ? U.passwordLength : PASSWD_LEN);
   } else {
      strcpy(hex_password, "09021345070413440c08135a3215135dd217ead3b5df556322e9a14a994b0f88");
   }
   jp_logf(JP_LOG_DEBUG, "passwordLength = %d\n", U.passwordLength);
   jp_logf(JP_LOG_DEBUG, "userPassword = [%s]\n", hex_password);
   write_to_parent(PIPE_PASSWORD, "\"%s\"\n", hex_password);
#endif

   if (dlp_OpenConduit(sd)<0) {
      jp_logf(JP_LOG_WARN, "dlp_OpenConduit() failed\n");
      jp_logf(JP_LOG_WARN, _("Sync canceled\n"));
#ifdef ENABLE_PLUGINS
      if (!(SYNC_NO_FORK & sync_info->flags)) 
         free_plugin_list(&plugin_list);
#endif
      dlp_EndOfSync(sd, 0);
      pi_close(sd);
      return SYNC_ERROR_OPEN_CONDUIT;
   }

   sync_process_install_file(sd);

   if ((SYNC_RESTORE & sync_info->flags)) {
      U.userID=sync_info->userID;
      U.viewerID=0;
      U.lastSyncPC=0;
      strncpy(U.username, sync_info->username, sizeof(U.username));

      dlp_WriteUserInfo(sd, &U);

      dlp_EndOfSync(sd, 0);
      pi_close(sd);

      jp_logf(JP_LOG_GUI, _("Finished restoring handheld.\n"));
      jp_logf(JP_LOG_GUI, _("You may need to sync to update J-Pilot.\n"));
      write_to_parent(PIPE_FINISHED, "\n");
      return EXIT_SUCCESS;
   }

#ifdef JPILOT_DEBUG
   start=0;
   buffer = pi_buffer_new(sizeof(struct DBInfo));
   while(dlp_ReadDBList(sd, 0, dlpDBListRAM, start, buffer)>0) {
      memcpy(&info, buffer->data, sizeof(struct DBInfo));
      start=info.index+1;
      if (info.flags & dlpDBFlagAppInfoDirty) {
         printf("appinfo dirty for %s\n", info.name);
      }
   }
   pi_buffer_free(buffer);
#endif

   /* Do a fast, or a slow sync on each application in the arrays */
   if ( (!(SYNC_OVERRIDE_USER & sync_info->flags)) &&
        (U.lastSyncPC == sync_info->PC_ID) ) {
      fast_sync=1;
      jp_logf(JP_LOG_GUI, _("Doing a fast sync.\n"));
      for (i=0; dbname[i][0]; i++) {
         if (get_pref_int_default(pref_sync_array[i], 1)) {
            if (unpack_cai_from_buf[i] && pack_cai_into_buf[i]) {
               sync_categories(dbname[i], sd,
                               unpack_cai_from_buf[i],
                               pack_cai_into_buf[i]);
            }
            fast_sync_application(dbname[i], sd);
         }
      }
   } else {
      fast_sync=0;
      jp_logf(JP_LOG_GUI, _("Doing a slow sync.\n"));
      for (i=0; dbname[i][0]; i++) {
         if (get_pref_int_default(pref_sync_array[i], 1)) {
            if (unpack_cai_from_buf[i] && pack_cai_into_buf[i]) {
               sync_categories(dbname[i], sd,
                               unpack_cai_from_buf[i],
                               pack_cai_into_buf[i]);
            }
            slow_sync_application(dbname[i], sd);
         }
      }
   }


#ifdef ENABLE_PLUGINS
   plugin_list = get_plugin_list();

   for (temp_list = plugin_list; temp_list; temp_list = temp_list->next) {
      plugin = (struct plugin_s *)temp_list->data;
      jp_logf(JP_LOG_DEBUG, "syncing plugin name: [%s]\n", plugin->name);
      if ((plugin->db_name==NULL) || (plugin->db_name[0]=='\0')) {
         jp_logf(JP_LOG_DEBUG, "not syncing plugin DB: [%s]\n", plugin->db_name);
         continue;
      }
      jp_logf(JP_LOG_DEBUG, "syncing plugin DB: [%s]\n", plugin->db_name);
      if (fast_sync) {
         if (plugin->sync_on) {
            if (plugin->plugin_unpack_cai_from_ai &&
                plugin->plugin_pack_cai_into_ai) {
               sync_categories(plugin->db_name, sd,
                               plugin->plugin_unpack_cai_from_ai,
                               plugin->plugin_pack_cai_into_ai);
            }
            fast_sync_application(plugin->db_name, sd);
         }
      } else {
         if (plugin->sync_on) {
            if (plugin->plugin_unpack_cai_from_ai &&
                plugin->plugin_pack_cai_into_ai) {
               sync_categories(plugin->db_name, sd,
                               plugin->plugin_unpack_cai_from_ai,
                               plugin->plugin_pack_cai_into_ai);
            }
            slow_sync_application(plugin->db_name, sd);
         }
      }
   }
#endif

#ifdef ENABLE_PLUGINS
   /* Do the sync plugin calls */
   plugin_list=NULL;

   plugin_list = get_plugin_list();

   for (temp_list = plugin_list; temp_list; temp_list = temp_list->next) {
      plugin = (struct plugin_s *)temp_list->data;
      if (plugin) {
         if (plugin->sync_on) {
            if (plugin->plugin_sync) {
               jp_logf(JP_LOG_DEBUG, "calling plugin_sync for [%s]\n", plugin->name);
               plugin->plugin_sync(sd);
            }
         }
      }
   }
#endif

   sync_fetch(sd, sync_info->flags, sync_info->num_backups, fast_sync);

   /* Tell the user who it is, with this PC id. */
   U.lastSyncPC = sync_info->PC_ID;
   U.successfulSyncDate = time(NULL);
   U.lastSyncDate = U.successfulSyncDate;
   dlp_WriteUserInfo(sd, &U);
   if (strncpy(buf,_("Thank you for using J-Pilot."),1024) == NULL) {
      jp_logf(JP_LOG_DEBUG, "memory allocation internal error\n");
      dlp_EndOfSync(sd, 0);
      pi_close(sd);
#ifdef ENABLE_PLUGINS
      if (!(SYNC_NO_FORK & sync_info->flags)) 
         free_plugin_list(&plugin_list);
#endif
      return 0;
   }
   get_pref(PREF_CHAR_SET, &char_set, NULL);
   charset_j2p(buf,1023,char_set);

   dlp_AddSyncLogEntry(sd, buf);
   dlp_AddSyncLogEntry(sd, "\n");

   dlp_EndOfSync(sd, 0);
   pi_close(sd);

   cleanup_pc_files();

#ifdef ENABLE_PLUGINS
   /* Do the sync plugin calls */
   plugin_list=NULL;

   plugin_list = get_plugin_list();

   for (temp_list = plugin_list; temp_list; temp_list = temp_list->next) {
      plugin = (struct plugin_s *)temp_list->data;
      if (plugin) {
         if (plugin->sync_on) {
            if (plugin->plugin_post_sync) {
               jp_logf(JP_LOG_DEBUG, "calling plugin_post_sync for [%s]\n", plugin->name);
               plugin->plugin_post_sync();
            }
         }
      }
   }

   if (!(SYNC_NO_FORK & sync_info->flags)) {
      jp_logf(JP_LOG_DEBUG, "freeing plugin list\n");
      free_plugin_list(&plugin_list);
   }
#endif

   jp_logf(JP_LOG_GUI, _("Finished.\n"));
   write_to_parent(PIPE_FINISHED, "\n");

   return EXIT_SUCCESS;
}

int sync_once(struct my_sync_info *sync_info)
{
#ifdef USE_LOCKING
   int fd;
#endif
   int r;
   struct my_sync_info sync_info_copy;
   pid_t pid;

#ifdef __APPLE__
   /* bug 1924 */
   sync_info->flags |= SYNC_NO_FORK;
#endif

#ifdef USE_LOCKING
   r = sync_lock(&fd);
   if (r) {
      jp_logf(JP_LOG_DEBUG, "Child cannot lock file\n");
      if (!(SYNC_NO_FORK & sync_info->flags)) {
         _exit(255);
      } else {
         return EXIT_FAILURE;
      }
   }
#endif

   /* This should never be reached with new cancel sync code
    * Although, it can be reached through a remote sync. */
   if (glob_child_pid) {
      jp_logf(JP_LOG_WARN, _("%s: sync process already in progress (process ID = %d)\n"), PN, glob_child_pid);
      jp_logf(JP_LOG_WARN, _("%s: press the HotSync button on the cradle\n"
                             "         or stop the sync by using the cancel sync button\n"
                             "         or stop the sync by typing \"kill %d\" at the command line\n"), PN, glob_child_pid);
      return EXIT_FAILURE;
   }

   /* Make a copy of the sync info for the forked process */
   memcpy(&sync_info_copy, sync_info, sizeof(struct my_sync_info));

   if (!(SYNC_NO_FORK & sync_info->flags)) {
      jp_logf(JP_LOG_DEBUG, "forking sync process\n");
      signal(SIGCHLD, sig_handler);
      glob_child_pid = -1;
      pid = fork();
      switch (pid){
       case -1:
         perror("fork");
         return 0;
       case 0:
         /* child continues sync */
         break;
       default:
         /* parent stores child pid and goes back to GUI */
         if (-1 == glob_child_pid)
            glob_child_pid = pid;
         return EXIT_SUCCESS;
      }
   }

   r = jp_sync(&sync_info_copy);
   if (r) {
      jp_logf(JP_LOG_WARN, _("Exiting with status %s\n"), get_error_str(r));
      jp_logf(JP_LOG_WARN, _("Finished.\n"));
   }
#ifdef USE_LOCKING
   sync_unlock(fd);
#endif
   jp_logf(JP_LOG_DEBUG, "sync child exiting\n");
   if (!(SYNC_NO_FORK & sync_info->flags)) {
      _exit(255);
   } else {
      return r;
   }
}

/***********************************************************************/
/* Imported from pilot-xfer.c, pilot-link-0.12.5, 2010-10-31 */
/***********************************************************************/

/***********************************************************************
 *
 * Function:    pi_file_install_VFS
 *
 * Summary:     Push file(s) to the Palm's VFS (parameters intentionally
 *              similar to pi_file_install).
 *
 * Parameters:  fd       --> open file descriptor for file
 *              basename --> filename or description of file
 *              socket   --> sd, connection to Palm
 *              vfspath  --> target in VFS, may be dir or filename
 *              f        --> progress function, in the style of pi_file_install
 *
 * Returns:     -1 on bad parameters
 *              -2 on cancelled sync
 *              -3 on bad vfs path
 *              -4 on bad local file
 *              -5 on insufficient VFS space for the file
 *              -6 on memory allocation error
 *              >=0 if all went well (size of installed file)
 *
 * Note:        Should probably return an ssize_t and refuse to do files >
 *              2Gb, due to signedness.
 *
 ***********************************************************************/
static int pi_file_install_VFS(const int fd, const char *basename, const int socket, const char *vfspath, progress_func f)
{
	enum { bad_parameters=-1,
	       cancel=-2,
	       bad_vfs_path=-3,
	       bad_local_file=-4,
	       insufficient_space=-5,
	       internal_=-6
	} ;

	char        rpath[vfsMAXFILENAME];
	int         rpathlen = vfsMAXFILENAME;
	FileRef     file;
	unsigned long attributes;
	char        *filebuffer = NULL;
	long        volume = -1;
	long        used,
	            total,
	            freespace;
	int
	            writesize,
	            offset;
	size_t      readsize;
	size_t      written_so_far = 0;
	enum { no_path=0, appended_filename=1, retried=2, done=3 } path_steps;
	struct stat sbuf;
	pi_progress_t progress;

	if (fstat(fd,&sbuf) < 0) {
		fprintf(stderr,"   ERROR: Cannot stat '%s'.\n",basename);
		return bad_local_file;
	}

	if (findVFSPath(socket,vfspath,&volume,rpath,&rpathlen) < 0)
	{
		fprintf(stderr,"\n   VFS path '%s' does not exist.\n\n", vfspath);
		return bad_vfs_path;
	}

	if (dlp_VFSVolumeSize(socket,volume,&used,&total)<0)
	{
		fprintf(stderr,"   Unable to get volume size.\n");
		return bad_vfs_path;
	}

	/* Calculate free space but leave last 64k free on card */
	freespace  = total - used - 65536 ;

	if ((unsigned long)sbuf.st_size > freespace)
	{
		fprintf(stderr, "\n\n");
		fprintf(stderr, "   Insufficient space to install this file on your Palm.\n");
		fprintf(stderr, "   We needed %lu and only had %lu available..\n\n",
				(unsigned long)sbuf.st_size, freespace);
		return insufficient_space;
	}
#define APPEND_BASENAME path_steps-=1; \
			if (rpath[rpathlen-1] != '/') { \
				rpath[rpathlen++]='/'; \
					rpath[rpathlen]=0; \
			} \
			strncat(rpath,basename,vfsMAXFILENAME-rpathlen-1); \
			rpathlen = strlen(rpath);

	path_steps = no_path;

	while (path_steps<retried)
	{
		/* Don't retry by default. APPEND_BASENAME changes
		   the file being tries, so it decrements fd again.
		   Because we add _two_ here, (two steps fwd, one step back)
		   we try at most twice anyway.
		      no_path->retried
		      appended_filename -> done
		   Note that APPEND_BASENAME takes one off, so
		      retried->appended_basename
		*/
		path_steps+=2;

		if (dlp_VFSFileOpen(socket,volume,rpath,dlpVFSOpenRead,&file) < 0)
		{
			/* Target doesn't exist. If it ends with a /, try to
			create the directory and then act as if the existing
			directory was given as target. If it doesn't, carry
			on, it's a regular file to create. */
			if ('/' == rpath[rpathlen-1])
			{
				/* directory, doesn't exist. Don't try to mkdir /. */
				if ((rpathlen > 1)
						&& (dlp_VFSDirCreate(socket,volume,rpath) < 0))
				{
					fprintf(stderr,"   Could not create destination directory.\n");
					return bad_vfs_path;
				}
				APPEND_BASENAME
			}
			if (dlp_VFSFileCreate(socket,volume,rpath) < 0)
			{
				fprintf(stderr,"   Cannot create destination file '%s'.\n",
						rpath);
				return bad_vfs_path;
			}
		}
		else
		{
			/* Exists, and may be a directory, or a filename. If it's
			a filename, that's fine as long as we're installing
			just a single file. */
			if (dlp_VFSFileGetAttributes(socket,file,&attributes) < 0)
			{
				fprintf(stderr,"   Could not get attributes for destination.\n");
				(void) dlp_VFSFileClose(socket,file);
				return bad_vfs_path;
			}

			if (attributes & vfsFileAttrDirectory)
			{
				APPEND_BASENAME
				dlp_VFSFileClose(socket,file);
				/* Now for sure it's a filename in a directory. */
			} else {
				dlp_VFSFileClose(socket,file);
				if ('/' == rpath[rpathlen-1])
				{
					/* was expecting a directory */
					fprintf(stderr,"   Destination is not a directory.\n");
					return bad_vfs_path;
				}
			}
		}
	}
#undef APPEND_BASENAME

	if (dlp_VFSFileOpen(socket,volume,rpath,0x7,&file) < 0)
	{
		fprintf(stderr,"   Cannot open destination file '%s'.\n",rpath);
		return bad_vfs_path;
	}

	/* If the file already exists we want to truncate it so if we write a smaller file
	 * the tail of the previous file won't show */
	if (dlp_VFSFileResize(socket, file, 0) < 0)
	{
		fprintf(stderr,"   Cannot truncate file size to 0 '%s'.\n",rpath);
		/* Non-fatal error, continue */
	}

#define FBUFSIZ 65536
	filebuffer = (char *)malloc(FBUFSIZ);
	if (NULL == filebuffer)
	{
		fprintf(stderr,"   Cannot allocate memory for file copy.\n");
		dlp_VFSFileClose(socket,file);
		close(fd);
		return internal_;
	}

	memset(&progress, 0, sizeof(progress));
	progress.type = PI_PROGRESS_SEND_VFS;
	progress.data.vfs.path = (char *) basename;
	progress.data.vfs.total_bytes = sbuf.st_size;

	writesize = 0;
	written_so_far = 0;
	while (writesize >= 0)
	{
		readsize = read(fd,filebuffer,FBUFSIZ);
		if (readsize <= 0) break;
		offset=0;
		while (readsize > 0)
		{
			writesize = dlp_VFSFileWrite(socket,file,filebuffer+offset,readsize);
			if (writesize < 0)
			{
				fprintf(stderr,"   Error while writing file.\n");
				break;
			}
			readsize -= writesize;
			offset += writesize;
			written_so_far += writesize;
			progress.transferred_bytes += writesize;

			if ((writesize>0) || (readsize > 0)) {
				if (f && (f(socket, &progress) == PI_TRANSFER_STOP)) {
					sbuf.st_size = 0;
					pi_set_error(socket,PI_ERR_FILE_ABORTED);
					goto cleanup;
				}
			}
		}
	}

cleanup:
	free(filebuffer);
	dlp_VFSFileClose(socket,file);
   
	close(fd);
	return sbuf.st_size;
}

/***********************************************************************
 *
 * Function:    findVFSRoot_clumsy
 *
 * Summary:     For internal use only. May contain live weasels.
 *
 * Parameters:  root_component --> root path to search for.
 *              match          <-> volume matching root_component.
 *
 * Returns:     -2 on VFSVolumeEnumerate error,
 *              -1 if no match was found,
 *              0 if a match was found and @p match is set,
 *              1 if no match but only one VFS volume exists and
 *                match is set.
 *
 ***********************************************************************/
static int
findVFSRoot_clumsy(int sd, const char *root_component, long *match)
{
	int				volume_count		= 16;
	int				volumes[16];
	struct VFSInfo	info;
	int				i;
	int				buflen;
	char			buf[vfsMAXFILENAME];
	long			matched_volume		= -1;

	if (dlp_VFSVolumeEnumerate(sd,&volume_count,volumes) < 0)
	{
		return -2;
	}

	/* Here we scan the "root directory" of the Pilot.  We will fake out
	   a bunch of directories pointing to the various "cards" on the
	   device. If we're listing, print everything out, otherwise remain
	   silent and just set matched_volume if there's a match in the
	   first filename component. */
	for (i = 0; i<volume_count; ++i)
	{
		if (dlp_VFSVolumeInfo(sd,volumes[i],&info) < 0)
			continue;

		buflen=vfsMAXFILENAME;
		buf[0]=0;
		(void) dlp_VFSVolumeGetLabel(sd,volumes[i],&buflen,buf);

		/* Not listing, so just check matches and continue. */
		if (0 == strcmp(root_component,buf)) {
			matched_volume = volumes[i];
			break;
		}
		/* volume label no longer important, overwrite */
		sprintf(buf,"card%d",info.slotRefNum);

		if (0 == strcmp(root_component,buf)) {
			matched_volume = volumes[i];
			break;
		}
	}

	if (matched_volume >= 0) {
		*match = matched_volume;
		return 0;
	}

	if ((matched_volume < 0) && (1 == volume_count)) {
		/* Assume that with one card, just go look there. */
		*match = volumes[0];
		return 1;
	}
	return -1;
}

/***********************************************************************
 *
 * Function:    findVFSPath
 *
 * Summary:     Search the VFS volumes for @p path. Sets @p volume
 *              equal to the VFS volume matching @p path (if any) and
 *              fills buffer @p rpath with the path to the file relative
 *              to the volume.
 *
 *              Acceptable root components are /cardX/ for card indicators
 *              or /volumename/ for for identifying VFS volumes by their
 *              volume name. In the special case that there is only one
 *              VFS volume, no root component need be specified, and
 *              "/DCIM/" will map to "/card1/DCIM/".
 *
 * Parameters:  path           --> path to search for.
 *              volume         <-> volume containing path.
 *              rpath          <-> buffer for path relative to volume.
 *              rpathlen       <-> in: length of buffer; out: length of
 *                                 relative path.
 *
 * Returns:     -2 on VFSVolumeEnumerate error,
 *              -1 if no match was found,
 *              0 if a match was found.
 *
 ***********************************************************************/
static int
findVFSPath(int sd, const char *path, long *volume, char *rpath, int *rpathlen)
{
	char	*s;
	int		r;

	if ((NULL == path) || (NULL == rpath) || (NULL == rpathlen))
		return -1;
	if (*rpathlen < strlen(path))
		return -1;

	memset(rpath,0,*rpathlen);
	if ('/'==path[0])
		strncpy(rpath,path+1,*rpathlen-1);
	else
		strncpy(rpath,path,*rpathlen-1);
	s = strchr(rpath,'/');
	if (NULL != s)
		*s=0;


	r = findVFSRoot_clumsy(sd, rpath,volume);
	if (r < 0)
		return r;

	if (0 == r)
	{
		/* Path includes card/volume label. */
		r = strlen(rpath);
		if ('/'==path[0])
			++r; /* adjust for stripped / */
		memset(rpath,0,*rpathlen);
		strncpy(rpath,path+r,*rpathlen-1);
	} else {
		/* Path without card label */
		memset(rpath,0,*rpathlen);
		strncpy(rpath,path,*rpathlen-1);
	}

	if (!rpath[0])
	{
		rpath[0]='/';
		rpath[1]=0;
	}
	*rpathlen = strlen(rpath);
	return 0;
}


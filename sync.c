/* sync.c
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

#define PIPE_DEBUG

# include "config.h"
#include "i18n.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/time.h>
#ifdef USE_FLOCK
#include <sys/file.h>
#else
#include <fcntl.h>
#endif
#include <errno.h>
#include <signal.h>
#include <utime.h>
#include <stdio.h>
#include <ctype.h>
#include "utils.h"
#include "sync.h"
#include "japanese.h"
#include "cp1250.h"
#include "russian.h"
#include "log.h"
#include "prefs.h"
#include "datebook.h"
#include "plugins.h"
#include "libplugin.h"
#include "password.h"

#include <pi-socket.h>
#include <pi-dlp.h>
#include <pi-file.h>
#include <pi-version.h>

#define FD_ERROR 1001


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

/* #define JPILOT_DEBUG */
/* #define SYNC_CAT_DEBUG */

extern int pipe_to_parent, pipe_from_parent;
extern pid_t glob_child_pid;

int slow_sync_application(char *DB_name, int sd);
int fast_sync_application(char *DB_name, int sd);
int sync_fetch(int sd, unsigned int flags, const int num_backups, int fast_sync);
int jp_sync(struct my_sync_info *sync_info);
int sync_lock();
int sync_unlock();
static int sync_process_install_file(int sd);
static int sync_rotate_backups(const int num_backups);

int sync_categories(char *DB_name, int sd,
		    int (*unpack_cai_from_ai)(struct CategoryAppInfo *cai, unsigned char *ai_raw, int len),
		    int (*pack_cai_into_ai)(struct CategoryAppInfo *cai, unsigned char *ai_raw, int len));

int unpack_address_cai_from_ai(struct CategoryAppInfo *cai, unsigned char *ai_raw, int len);
int pack_address_cai_into_ai(struct CategoryAppInfo *cai, unsigned char *ai_raw, int len);
int unpack_todo_cai_from_ai(struct CategoryAppInfo *cai, unsigned char *ai_raw, int len);
int pack_todo_cai_into_ai(struct CategoryAppInfo *cai, unsigned char *ai_raw, int len);
int unpack_memo_cai_from_ai(struct CategoryAppInfo *cai, unsigned char *ai_raw, int len);
int pack_memo_cai_into_ai(struct CategoryAppInfo *cai, unsigned char *ai_raw, int len);

void sig_handler(int sig)
{
   int status;

   jp_logf(JP_LOG_DEBUG, "caught signal SIGCHLD\n");
   glob_child_pid = 0;

   /*wait for any child processes */
   waitpid(-1, &status, WNOHANG);

   /*refresh the screen after a sync */
   /*cb_app_button(NULL, GINT_TO_POINTER(REDRAW));*/

   return;
}

#ifdef USE_LOCKING
int sync_lock(int *fd)
{
   pid_t pid;
   char lock_file[256];
   int r;
   char str[12];
#ifndef USE_FLOCK
   struct flock lock;
#endif

   get_home_file_name("sync_pid", lock_file, 255);
   *fd = open(lock_file, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
   if (*fd<0) {
      jp_logf(JP_LOG_WARN, "open lock file failed\n");
      return -1;
   }
#ifndef USE_FLOCK
   lock.l_type = F_WRLCK;
   lock.l_start = 0;
   lock.l_whence = SEEK_SET;
   lock.l_len = 0; /*Lock to the end of file */
   r = fcntl(*fd, F_SETLK, &lock);
#else
   r = flock(*fd, LOCK_EX | LOCK_NB);
#endif
   if (r == -1){
      jp_logf(JP_LOG_WARN, "lock failed\n");
      read(*fd, str, 10);
      pid = atoi(str);
      jp_logf(JP_LOG_FATAL, "sync file is locked by pid %d\n", pid);
      return -1;
   } else {
      jp_logf(JP_LOG_DEBUG, "lock succeeded\n");
      pid=getpid();
      sprintf(str, "%d\n", pid);
      write(*fd, str, strlen(str)+1);
      ftruncate(*fd, strlen(str)+1);
   }
   return 0;
}

int sync_unlock(int fd)
{
   pid_t pid;
   char lock_file[256];
   int r;
   char str[12];
#ifndef USE_FLOCK
   struct flock lock;
#endif

   get_home_file_name("sync_pid", lock_file, 255);

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
      jp_logf(JP_LOG_WARN, "unlock failed\n");
      read(fd, str, 10);
      pid = atoi(str);
      jp_logf(JP_LOG_WARN, "sync is locked by pid %d\n", pid);
      close(fd);
      return -1;
   } else {
      jp_logf(JP_LOG_DEBUG, "unlock succeeded\n");
      ftruncate(fd, 0);
      close(fd);
   }
   return 0;
}
#endif

static char *get_error_str(int error)
{
   static char buf[10];

   switch(error) {
    case SYNC_ERROR_BIND:
      return "SYNC_ERROR_BIND";
    case SYNC_ERROR_LISTEN:
      return "SYNC_ERROR_LISTEN";
    case SYNC_ERROR_OPEN_CONDUIT:
      return "SYNC_ERROR_OPEN_CONDUIT";
    case SYNC_ERROR_PI_ACCEPT:
      return "SYNC_ERROR_PI_ACCEPT";
    case SYNC_ERROR_READSYSINFO:
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

int sync_once(struct my_sync_info *sync_info)
{
#ifdef USE_LOCKING
   int fd;
#endif
   int r;
   struct my_sync_info *sync_info_copy;

   if (glob_child_pid) {
      jp_logf(JP_LOG_WARN, PN": sync PID = %d\n", glob_child_pid);
      jp_logf(JP_LOG_WARN, PN": press the hotsync button on the cradle "
	   "or \"kill %d\"\n", glob_child_pid);
      return 0;
   }

   /* Make a copy of the sync info for the forked process */
   sync_info_copy = malloc(sizeof(struct my_sync_info));
   if (!sync_info_copy) {
      jp_logf(JP_LOG_WARN, PN":sync_once(): Out of memory\n");
      return 0;
   }
   memcpy(sync_info_copy, sync_info, sizeof(struct my_sync_info));

   if (!(sync_info->flags & SYNC_NO_FORK)) {
      jp_logf(JP_LOG_DEBUG, "forking sync process\n");
      switch ( glob_child_pid = fork() ){
       case -1:
	 perror("fork");
	 return 0;
       case 0:
	 /* close(pipe_from_parent); */
	 break;
       default:
	 if (!(sync_info->flags & SYNC_NO_FORK)) {
	    signal(SIGCHLD, sig_handler);
	 }
	 return 0;
      }
      /* Close all file descriptors in the child except stdout, stderr and
       * the one to talk to/from the parent.
       * This prevents the child from corrupting the X file descriptor.
       */
      /* Cannot close the debug file output descriptor */
      /*
      for (i = 0; i < 255; i++) {
	 if (i != 1 && i != 2 && i != pipe_to_parent && i != pipe_from_parent)
	   close(i);
      }*/
   }
#ifdef USE_LOCKING
   r = sync_lock(&fd);
   if (r) {
      jp_logf(JP_LOG_DEBUG, "Child cannot lock file\n");
      free(sync_info_copy);
      _exit(0);
   }
#endif

   r = jp_sync(sync_info_copy);
   if (r) {
      jp_logf(JP_LOG_WARN, _("Exiting with status %s\n"), get_error_str(r));
      jp_logf(JP_LOG_WARN, _("Finished\n"));
   }
#ifdef USE_LOCKING
   sync_unlock(fd);
#endif
   jp_logf(JP_LOG_DEBUG, "sync child exiting\n");
   free(sync_info_copy);
   if (!(sync_info->flags & SYNC_NO_FORK)) {
      _exit(0);
   } else {
      return r;
   }
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
      /* ret = read(pipe_from_parent, buf, 100); */
#endif
      break;
   }

   /* Back to normal */
   pi_watchdog(sd, 0);
   
   return command;
}

int jp_sync(struct my_sync_info *sync_info)
{
   struct pi_sockaddr addr;
   int sd;
   int ret;
   struct PilotUser U;
   const char *device;
   char default_device[]="/dev/pilot";
   int found=0, fast_sync=0;
   int i;
   int dev_usb;
   char link[256], dev_str[256], dev_dir[256], *Pc;
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
   struct  SysInfo sys_info;
   
   device = NULL;
   if (sync_info->port) {
      if (sync_info->port[0]) {
	 /*A port was passed in to use */
	 device=sync_info->port;
	 found = 1;
      }
   }
   if (!found) {
      /*No port was passed in, look in env */
      device = getenv("PILOTPORT");
      if (device == NULL) {
		 device = default_device;
      }
   }

   jp_logf(JP_LOG_GUI, "****************************************\n");
   jp_logf(JP_LOG_GUI, _(" Syncing on device %s\n"), device);
   jp_logf(JP_LOG_GUI, _(" Press the HotSync button now\n"));
   jp_logf(JP_LOG_GUI, "****************************************\n");

   /* pilot-link older than 0.9.5 didn't have web clipping flag */
#if PILOT_LINK_VERSION <= 0
# if PILOT_LINK_MAJOR <= 9
#  if PILOT_LINK_MINOR <= 5
#   define dlpDBFlagClipping 0x200
#  endif
# endif
#endif

   /* pilot-link > 0.9.5 needed for many USB devices */
#ifdef USB_PILOT_LINK
   /* Newer pilot-link */
   sd = pi_socket(PI_AF_PILOT, PI_SOCK_STREAM, PI_PF_DLP);
   /* We could just do this */
   /* sd = pilot_connect(sync_info->port); */
   addr.pi_family = PI_AF_PILOT;
#else
   /* 0.9.5 or older */
   sd = pi_socket(PI_AF_SLP, PI_SOCK_STREAM, PI_PF_PADP);
   addr.pi_family = PI_AF_SLP;
#endif

   if (sd < 0) {
      int err = errno;
      perror("pi_socket");
      jp_logf(JP_LOG_WARN, "pi_socket %s\n", strerror(err));
      return -1;
   }

   strncpy(addr.pi_device, device, sizeof(addr.pi_device));

   /* This is for USB, whose device doesn't exist until the cradle is pressed
    * We will give them 5 seconds */
   dev_str[0]='\0';
   link[0]='\0';
   strncpy(dev_str, device, 255);
   strncpy(dev_dir, device, 255);
   dev_str[255]='0';
   dev_dir[255]='0';
   dev_usb=0;
   for (Pc=&dev_dir[strlen(dev_dir)-1]; Pc>dev_dir; Pc--) {
      if (*Pc=='/') *Pc='\0';
      else break;
   }
   Pc = strrchr(dev_dir, '/');
   if (Pc) {
      *Pc = '\0';
   }
   for (i=10; i>0; i--) {
      ret = readlink(dev_str, link, 256);
      if (ret>0) {
	 link[ret]='\0';
      } else {
	 if (strstr(dev_str, "usb") || strstr(dev_str, "USB")) {
	    dev_usb=1;
	 }
	 break;
      }
      if (link[0]=='/') {
	 strcpy(dev_str, link);
	 strcpy(dev_dir, link);
	 for (Pc=&dev_dir[strlen(dev_dir)-1]; Pc>dev_dir; Pc--) {
	    if (*Pc=='/') *Pc='\0';
	    else break;
	 }
	 Pc = strrchr(dev_dir, '/');
	 if (Pc) {
	    *Pc = '\0';
	 }
      } else {
	 g_snprintf(dev_str, 255, "%s/%s", dev_dir, link);
	 dev_str[255]='\0';
      }
      if (strstr(link, "usb") || strstr(link, "USB")) {
	 dev_usb=1;
	 break;
      }
   }
   if (dev_usb) {
      for (i=7; i>0; i--) {
	 ret = pi_bind(sd, (struct sockaddr*)&addr, sizeof(addr));
	 if (ret!=-1) break;
	 sleep(1);
      }
   } else {
      ret = pi_bind(sd, (struct sockaddr*)&addr, sizeof(addr));
   }
   if (ret == -1) {
      perror("pi_bind");
      jp_logf(JP_LOG_WARN, "pi_bind %s\n", strerror(errno));
      jp_logf(JP_LOG_WARN, _("Check your serial port and settings\n"));
      return SYNC_ERROR_BIND;
   }

   ret = pi_listen(sd, 1);
   if (ret == -1) {
      perror("pi_listen");
      jp_logf(JP_LOG_WARN, "pi_listen %s\n", strerror(errno));
      return SYNC_ERROR_LISTEN;
   }

   sd = pi_accept(sd, 0, 0);
   if(sd == -1) {
      perror("pi_accept");
      jp_logf(JP_LOG_WARN, "pi_accept %s\n", strerror(errno));
      return SYNC_ERROR_PI_ACCEPT;
   }

   /* We must do this to take care of the password being required to sync
    * on Palm OS 4.x */
   if (dlp_ReadSysInfo(sd, &sys_info) < 0) {
      jp_logf(JP_LOG_WARN, "dlp_ReadSysInfo error\n");
      pi_close(sd);
      return SYNC_ERROR_READSYSINFO;
   }

   /* The connection has been established here */
   /* Plugins should call pi_watchdog(); if they are going to be a while */
#ifdef ENABLE_PLUGINS
   if (!(sync_info->flags & SYNC_NO_PLUGINS)) {
      jp_logf(JP_LOG_DEBUG, "sync:calling load_plugins\n");
      load_plugins();
   }

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
   dlp_ReadUserInfo(sd, &U);

   /* Do some checks to see if this is the same palm that was synced
    * the last time
    */
   if ( (U.userID == 0) &&
      (!(sync_info->flags & SYNC_RESTORE)) ) {
      jp_logf(JP_LOG_GUI, _("Last Synced Username-->\"%s\"\n"), sync_info->username);
      jp_logf(JP_LOG_GUI, _("Last Synced UserID-->\"%d\"\n"), sync_info->userID);
      jp_logf(JP_LOG_GUI, _(" This Username-->\"%s\"\n"), U.username);
      jp_logf(JP_LOG_GUI, _(" This User ID-->%d\n"), U.userID);

      if (sync_info->flags & SYNC_NO_FORK) {
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
       (!(sync_info->flags & SYNC_OVERRIDE_USER)) &&
       (!(sync_info->flags & SYNC_RESTORE))) {
      jp_logf(JP_LOG_GUI, _("Last Synced Username-->\"%s\"\n"), sync_info->username);
      jp_logf(JP_LOG_GUI, _("Last Synced UserID-->\"%d\"\n"), sync_info->userID);
      jp_logf(JP_LOG_GUI, _(" This Username-->\"%s\"\n"), U.username);
      jp_logf(JP_LOG_GUI, _(" This User ID-->%d\n"), U.userID);

      if (sync_info->flags & SYNC_NO_FORK) {
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
   }
   if ((strcmp(sync_info->username, U.username)) &&
       (sync_info->username[0]!='\0') &&
       (!(sync_info->flags & SYNC_OVERRIDE_USER)) &&
       (!(sync_info->flags & SYNC_RESTORE))) {
      jp_logf(JP_LOG_GUI, _("Last Synced Username-->\"%s\"\n"), sync_info->username);
      jp_logf(JP_LOG_GUI, _("Last Synced UserID-->\"%d\"\n"), sync_info->userID);
      jp_logf(JP_LOG_GUI, _(" This Username-->\"%s\"\n"), U.username);
      jp_logf(JP_LOG_GUI, _(" This User ID-->%d\n"), U.userID);
      write_to_parent(PIPE_WAITING_ON_USER, "%d:\n", SYNC_ERROR_NOT_SAME_USER);
      if (sync_info->flags & SYNC_NO_FORK) {
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
    * in the preferences
    * So, this is more than just displaying it to the user */
   if (!(sync_info->flags & SYNC_RESTORE)) {
      write_to_parent(PIPE_USERNAME, "\"%s\"\n", U.username);
      write_to_parent(PIPE_USERID, "%d", U.userID);
      jp_logf(JP_LOG_GUI, _("Username is \"%s\"\n"), U.username);
      jp_logf(JP_LOG_GUI, _("User ID is %d\n"), U.userID);
   }
   jp_logf(JP_LOG_GUI, _("lastSyncPC = %d\n"), U.lastSyncPC);
   jp_logf(JP_LOG_GUI, _("This PC = %lu\n"), sync_info->PC_ID);
   jp_logf(JP_LOG_DEBUG, _("Last Username = [%s]\n"), sync_info->username);
   jp_logf(JP_LOG_DEBUG, _("Last UserID = %d\n"), sync_info->userID);
   jp_logf(JP_LOG_DEBUG, _("Username = [%s]\n"), U.username);
   jp_logf(JP_LOG_DEBUG, _("userID = %d\n"), U.userID);
   jp_logf(JP_LOG_DEBUG, _("lastSyncPC = %d\n"), U.lastSyncPC);

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
      jp_logf(JP_LOG_WARN, _("Sync canceled\n"));
      jp_logf(JP_LOG_WARN, "dlp_OpenConduit() failed\n");
#ifdef ENABLE_PLUGINS
      free_plugin_list(&plugin_list);
#endif
      dlp_EndOfSync(sd, 0);
      pi_close(sd);
      return SYNC_ERROR_OPEN_CONDUIT;
   }

   sync_process_install_file(sd);

   if ((sync_info->flags & SYNC_RESTORE)) {
      U.userID=sync_info->userID;
      U.viewerID=0;
      U.lastSyncPC=0;
      strncpy(U.username, sync_info->username, 128);

      dlp_WriteUserInfo(sd, &U);

      dlp_EndOfSync(sd, 0);
      pi_close(sd);

      /* Do not translate this text */
      jp_logf(JP_LOG_GUI, _("Finished restoring handheld.\n"));
      jp_logf(JP_LOG_GUI, _("You may need to sync to update J-Pilot.\n"));
      write_to_parent(PIPE_FINISHED, "\n");
      return 0;
   }

#ifdef JPILOT_DEBUG
   start=0;
   while(dlp_ReadDBList(sd, 0, dlpOpenRead, start, &info)>0) {
      start=info.index+1;
      if (info.flags & dlpDBFlagAppInfoDirty) {
	 printf("appinfo dirty for %s\n", info.name);
      }
   }
#endif

   if ( (!(sync_info->flags & SYNC_OVERRIDE_USER)) &&
       (U.lastSyncPC == sync_info->PC_ID) ) {
      fast_sync=1;
      jp_logf(JP_LOG_GUI, _("Doing a fast sync.\n"));
      if (get_pref_int_default(PREF_SYNC_DATEBOOK, 1)) {
	 /* sync_categories("DatebookDB", sd); */
	 fast_sync_application("DatebookDB", sd);
      }
      if (get_pref_int_default(PREF_SYNC_ADDRESS, 1)) {
	 sync_categories("AddressDB", sd,
			 unpack_address_cai_from_ai,
			 pack_address_cai_into_ai);
	 fast_sync_application("AddressDB", sd);
      }
      if (get_pref_int_default(PREF_SYNC_TODO, 1)) {
	 sync_categories("ToDoDB", sd,
			 unpack_todo_cai_from_ai,
			 pack_todo_cai_into_ai);
	 fast_sync_application("ToDoDB", sd);
      }
      if (get_pref_int_default(PREF_SYNC_MEMO, 1)) {
	 sync_categories("MemoDB", sd, 
			 unpack_memo_cai_from_ai,
			 pack_memo_cai_into_ai);
	 fast_sync_application("MemoDB", sd);
      }
      if (get_pref_int_default(PREF_SYNC_MEMO32, 1)) {
	 sync_categories("Memo32DB", sd,
			 unpack_memo_cai_from_ai,
			 pack_memo_cai_into_ai);
	 fast_sync_application("Memo32DB", sd);
      }
#ifdef ENABLE_MANANA
      if (get_pref_int_default(PREF_SYNC_MANANA, 1)) {
	 sync_categories("MañanaDB", sd,
			 unpack_todo_cai_from_ai,
			 pack_todo_cai_into_ai);
	 fast_sync_application("MañanaDB", sd);
      }
#endif
   } else {
      fast_sync=0;
      jp_logf(JP_LOG_GUI, _("Doing a slow sync.\n"));
      if (get_pref_int_default(PREF_SYNC_DATEBOOK, 1)) {
	 /* sync_categories("DatebookDB", sd); */
	 slow_sync_application("DatebookDB", sd);
      }
      if (get_pref_int_default(PREF_SYNC_ADDRESS, 1)) {
	 sync_categories("AddressDB", sd,
			 unpack_address_cai_from_ai,
			 pack_address_cai_into_ai);
	 slow_sync_application("AddressDB", sd);
      }
      if (get_pref_int_default(PREF_SYNC_TODO, 1)) {
	 sync_categories("ToDoDB", sd,
			 unpack_todo_cai_from_ai,
			 pack_todo_cai_into_ai);
	 slow_sync_application("ToDoDB", sd);
      }
      if (get_pref_int_default(PREF_SYNC_MEMO, 1)) {
	 sync_categories("Memo32DB", sd,
			 unpack_memo_cai_from_ai,
			 pack_memo_cai_into_ai);
	 slow_sync_application("MemoDB", sd);
      }
      if (get_pref_int_default(PREF_SYNC_MEMO32, 1)) {
	 sync_categories("Memo32DB", sd,
			 unpack_memo_cai_from_ai,
			 pack_memo_cai_into_ai);
	 slow_sync_application("Memo32DB", sd);
      }
#ifdef ENABLE_MANANA
      if (get_pref_int_default(PREF_SYNC_MANANA, 1)) {
	 sync_categories("ToDoDB", sd,
			 unpack_todo_cai_from_ai,
			 pack_todo_cai_into_ai);
	 slow_sync_application("MañanaDB", sd);
      }
#endif
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
      free_plugin_list(&plugin_list);
#endif
      return 0;
   }
   get_pref(PREF_CHAR_SET, &char_set, NULL);
   charset_j2p((unsigned char *)buf,1023,char_set);

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

   jp_logf(JP_LOG_DEBUG, "freeing plugin list\n");
   free_plugin_list(&plugin_list);
#endif

   /* Do not translate this text */
   jp_logf(JP_LOG_GUI, _("Finished.\n"));
   write_to_parent(PIPE_FINISHED, "\n");

   return 0;
}

int slow_sync_application(char *DB_name, int sd)
{
   unsigned long new_id;
   int db;
   int ret;
   int num;
   FILE *pc_in;
   PC3RecordHeader header;
   char *record;
   int rec_len;
   char pc_filename[256];
   char write_log_message[256];
   char error_log_message_w[256];
   char error_log_message_d[256];
   char delete_log_message[256];
   char log_entry[256];
   /* recordid_t id=0; */
   int index, size, attr, category;
   char buffer[65536];
   long char_set;

   if ((DB_name==NULL) || (strlen(DB_name) == 0) || (strlen(DB_name) > 250)) {
      return -1;
   }
   get_pref(PREF_CHAR_SET, &char_set, NULL);

   g_snprintf(log_entry, 255, _("Syncing %s\n"), DB_name);
   log_entry[255]='\0';
   jp_logf(JP_LOG_GUI, log_entry);
   g_snprintf(pc_filename, 255, "%s.pc3", DB_name);
   /* This is an attempt to use the proper pronoun most of the time */
   if (strchr("aeiou", tolower(DB_name[0]))) {
      g_snprintf(write_log_message, 255,
	      _("Wrote an %s record."), DB_name);
      g_snprintf(error_log_message_w, 255,
	      _("Writing an %s record failed."), DB_name);
      g_snprintf(error_log_message_d, 255,
	      _("Deleting an %s record failed."), DB_name);
      g_snprintf(delete_log_message, 256,
	      _("Deleted an %s record."), DB_name);
   } else {
      g_snprintf(write_log_message, 255,
	      _("Wrote a %s record."), DB_name);
      g_snprintf(error_log_message_w, 255,
	      _("Writing a %s record failed."), DB_name);
      g_snprintf(error_log_message_d, 255,
	      _("Deleting a %s record failed."), DB_name);
      g_snprintf(delete_log_message, 256,
	      _("Deleted a %s record."), DB_name);
   }

   pc_in = jp_open_home_file(pc_filename, "r+");
   if (pc_in==NULL) {
      jp_logf(JP_LOG_WARN, _("Unable to open %s\n"), pc_filename);
      return -1;
   }
   /* Open the applications database, store access handle in db */
   ret = dlp_OpenDB(sd, 0, dlpOpenReadWrite, DB_name, &db);
   if (ret < 0) {
      g_snprintf(log_entry, 255, _("Unable to open %s\n"), DB_name);
      log_entry[255]='\0';
      charset_j2p((unsigned char *)log_entry, 255, char_set);
      dlp_AddSyncLogEntry(sd, log_entry);
      return -1;
   }

#ifdef JPILOT_DEBUG
   dlp_ReadOpenDBInfo(sd, db, &num);
   jp_logf(JP_LOG_GUI ,_("number of records = %d\n"), num);
#endif
   while(!feof(pc_in)) {
      num = read_header(pc_in, &header);
      if (num!=1) {
	 if (ferror(pc_in)) {
	    break;
	 }
	 if (feof(pc_in)) {
	    break;
	 }
      }
      rec_len = header.rec_len;
      if (rec_len > 0x10000) {
	 jp_logf(JP_LOG_WARN, _("PC file corrupt?\n"));
	 fclose(pc_in);
	 return -1;
      }
      if ((header.rt==NEW_PC_REC) || (header.rt==REPLACEMENT_PALM_REC)) {
	 record = malloc(rec_len);
	 if (!record) {
	    jp_logf(JP_LOG_WARN, _("slow_sync_application(): Out of memory\n"));
	    break;
	 }
	 num = fread(record, rec_len, 1, pc_in);
	 if (num != 1) {
	    if (ferror(pc_in)) {
	       break;
	    }
	 }

	 if (header.rt==REPLACEMENT_PALM_REC) {
	    ret = dlp_WriteRecord(sd, db, header.attrib & dlpRecAttrSecret,
				  header.unique_id, header.attrib & 0x0F,
				  record, rec_len, &new_id);
	 } else {
	    ret = dlp_WriteRecord(sd, db, header.attrib & dlpRecAttrSecret,
				  0, header.attrib & 0x0F,
				  record, rec_len, &new_id);
	 }

	 if (record) {
	    free(record);
	    record = NULL;
	 }

	 if (ret < 0) {
	    jp_logf(JP_LOG_WARN, _("dlp_WriteRecord failed\n"));
	    charset_j2p((unsigned char *)error_log_message_w,255,char_set);
	    dlp_AddSyncLogEntry(sd, error_log_message_w);
	    dlp_AddSyncLogEntry(sd, "\n");
	 } else {
	    charset_j2p((unsigned char *)write_log_message,255,char_set);
	    dlp_AddSyncLogEntry(sd, write_log_message);
	    dlp_AddSyncLogEntry(sd, "\n");
	    /*Now mark the record as deleted in the pc file */
	    if (fseek(pc_in, -(header.header_len+rec_len), SEEK_CUR)) {
	       jp_logf(JP_LOG_WARN, _("fseek failed - fatal error\n"));
	       fclose(pc_in);
	       return -1;
	    }
	    header.rt=DELETED_PC_REC;
	    write_header(pc_in, &header);
	 }
      }
      if ((header.rt==DELETED_PALM_REC) || (header.rt==MODIFIED_PALM_REC)) {
	 rec_len = header.rec_len;
	 record = malloc(rec_len);
	 num = fread(record, rec_len, 1, pc_in);
	 if (num != 1) {
	    if (ferror(pc_in)) {
	       break;
	    }
	 }
	 if (fseek(pc_in, -rec_len, SEEK_CUR)) {
	    jp_logf(JP_LOG_WARN, _("fseek failed - fatal error\n"));
	    fclose(pc_in);
	    return -1;
	 }
	 ret = dlp_ReadRecordById(sd, db, header.unique_id, buffer,
				  &index, &size, &attr, &category);
	 if (rec_len == size) {
	    jp_logf(JP_LOG_DEBUG, "sizes match!\n");
#ifdef JPILOT_DEBUG
	    if (memcmp(record, buffer, size)==0) {
	       jp_logf(JP_LOG_DEBUG, "Binary is the same!\n");
	    }
	    /* FIXME What should happen here is that if the records are the same
	     * then go ahead and delete, otherwise make a copy of the record
	     * so that there is no data loss.
	     * memcmp doesn't seem to work for datebook, probably because
	     * some of the struct tm fields and things can be different and
	     * not affect the record.
	     */
#endif
	 }
	 /* if (ret>=0 ) {
	    printf("id %ld, index %d, size %d, attr 0x%x, category %d\n",id, index, size, attr, category);
	 }
	 jp_logf(JP_LOG_WARN, "Deleting Palm id=%d,\n",header.unique_id); */

	 ret = dlp_DeleteRecord(sd, db, 0, header.unique_id);

	 if (ret < 0) {
	    jp_logf(JP_LOG_WARN, "dlp_DeleteRecord failed\n"\
            "This could be because the record was already deleted on the Palm\n");
	    charset_j2p((unsigned char *)error_log_message_d,255,char_set);
	    dlp_AddSyncLogEntry(sd, error_log_message_d);
	    dlp_AddSyncLogEntry(sd, "\n");
	 } else {
	    charset_j2p((unsigned char *)delete_log_message,255,char_set);
	    dlp_AddSyncLogEntry(sd, delete_log_message);
	    dlp_AddSyncLogEntry(sd, "\n");
	 }
	 /*Now mark the record as deleted */
	 if (fseek(pc_in, -header.header_len, SEEK_CUR)) {
	    jp_logf(JP_LOG_WARN, _("fseek failed - fatal error\n"));
	    fclose(pc_in);
	    return -1;
	 }
	 header.rt=DELETED_DELETED_PALM_REC;
	 write_header(pc_in, &header);
      }

      /*skip this record now that we are done with it */
      if (fseek(pc_in, rec_len, SEEK_CUR)) {
	 jp_logf(JP_LOG_WARN, _("fseek failed - fatal error\n"));
	 fclose(pc_in);
	 return -1;
      }
   }
   fclose(pc_in);

#ifdef JPILOT_DEBUG
   dlp_ReadOpenDBInfo(sd, db, &num);
   jp_logf(JP_LOG_WARN ,"number of records = %d\n", num);
#endif

   dlp_ResetSyncFlags(sd, db);
   dlp_CleanUpDatabase(sd, db);

   /* Close the database */
   dlp_CloseDB(sd, db);

   return 0;
}

/*
 * Fetch the databases from the palm if modified
 */
int fetch_extra_DBs(int sd, char *palm_dbname[])
{
#define MAX_DBNAME 50
   struct pi_file *pi_fp;
   char full_name[256];
   struct stat statb;
   struct utimbuf times;
   int i;
   int found;
   int cardno, start;
   struct DBInfo info;
   char db_copy_name[MAX_DBNAME];
   char creator[5];

   start=cardno=0;

   while(dlp_ReadDBList(sd, cardno, dlpOpenRead, start, &info)>0) {
      start=info.index+1;
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
	 continue;
      }

      strncpy(db_copy_name, info.name, MAX_DBNAME-5);
      db_copy_name[MAX_DBNAME-5]='\0';
      if (info.flags & dlpDBFlagResource) {
	 strcat(db_copy_name,".prc");
      } else if (strncmp(db_copy_name + strlen(db_copy_name) - 4, ".pqa", 4)) {
	 strcat(db_copy_name,".pdb");
      }

      filename_make_legal(db_copy_name);

      get_home_file_name(db_copy_name, full_name, 255);

      statb.st_mtime = 0;

      stat(full_name, &statb);

      creator[0] = (info.creator & 0xFF000000) >> 24;
      creator[1] = (info.creator & 0x00FF0000) >> 16,
      creator[2] = (info.creator & 0x0000FF00) >> 8,
      creator[3] = (info.creator & 0x000000FF);
      creator[4] = '\0';

      /* If modification times are the same then we don t need to fetch it */
      if (info.modifyDate == statb.st_mtime) {
	 jp_logf(JP_LOG_GUI, _("%s (Creator ID '%s') is up to date, fetch skipped.\n"), db_copy_name, creator);
	 continue;
      }

      jp_logf(JP_LOG_GUI, _("Fetching '%s' (Creator ID '%s')... "), info.name, creator);

      info.flags &= 0xff;

      pi_fp = pi_file_create(full_name, &info);

      if (pi_fp==0) {
	 jp_logf(JP_LOG_WARN, _("Failed, unable to create file %s\n"), full_name);
	 continue;
      }
      if (pi_file_retrieve(pi_fp, sd, 0)<0) {
	 jp_logf(JP_LOG_WARN, _("Failed, unable to back up database\n"));
	 times.actime = 0;
	 times.modtime = 0;
      } else {
	 jp_logf(JP_LOG_GUI, _("OK\n"));
	 times.actime = info.createDate;
	 times.modtime = info.modifyDate;
      }
      pi_file_close(pi_fp);

      /*Set the create and modify times of local file to same as on palm */
      utime(full_name, &times);
   }
   return 0;
}

void free_file_name_list(GList **Plist)
{
   GList *list, *temp_list;

   if (!Plist) return;
   list = *Plist;

   /* Go to first entry in the list */
   for (temp_list = *Plist; temp_list; temp_list = temp_list->prev) {
      list = temp_list;
   }
   for (temp_list = list; temp_list; temp_list = temp_list->next) {
      if (temp_list->data) {
	 free(temp_list->data);
      }
   }
   g_list_free(list);
   *Plist=NULL;
}

void move_removed_apps(GList *file_list)
{
   DIR *dir;
   struct dirent *dirent;
   char full_backup_path[300];
   char full_remove_path[300];
   char full_backup_file[300];
   char full_remove_file[300];
   char home_dir[256];
   GList *list, *temp_list;
   int found;

   list = NULL;
   for (temp_list = file_list; temp_list; temp_list = temp_list->prev) {
      list = temp_list;
   }

#ifdef JPILOT_DEBUG
   printf("printing file list\n");
   for (temp_list = list; temp_list; temp_list = temp_list->next) {
      if (temp_list->data) {
	 printf("File list [%s]\n", (char *)temp_list->data);
      }
   }
#endif

   get_home_file_name("", home_dir, 255);

   /* Make sure the removed directory exists */
   g_snprintf(full_remove_path, 298, "%s/backup_removed", home_dir);
   mkdir(full_remove_path, 0700);


   g_snprintf(full_backup_path, 298, "%s/backup/", home_dir);
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
	    g_snprintf(full_backup_file, 298, "%s/backup/%s", home_dir, dirent->d_name);
	    g_snprintf(full_remove_file, 298, "%s/backup_removed/%s", home_dir, dirent->d_name);
	    jp_logf(JP_LOG_DEBUG, "[%s] not found\n", dirent->d_name);
	    jp_logf(JP_LOG_DEBUG, "  moving [%s]\n  to [%s]\n", full_backup_file, full_remove_file);
	    rename(full_backup_file, full_remove_file);
	 }
      }
      closedir(dir);
   }
}


/*
 * Fetch the databases from the palm if modified
 *
 * Be sure to call free_file_name_list(&file_list); before returning from
 * anywhere in this function.
 */
int sync_fetch(int sd, unsigned int flags, const int num_backups, int fast_sync)
{
#define MAX_DBNAME 50
   struct pi_file *pi_fp;
   char full_name[256];
   char full_backup_name[300];
   char creator[5];
   struct stat statb;
   struct utimbuf times;
   int i, r;
   int main_app;
   int mode;
   int manual_skip;
   int cardno, start;
   struct DBInfo info;
   char db_copy_name[MAX_DBNAME];
   GList *file_list;
   GList *end_of_list;
#ifdef ENABLE_PLUGINS
   GList *temp_list;
   GList *plugin_list;
   struct plugin_s *plugin;
#endif
   char *file_name;
   char *palm_dbname[]={
      "DatebookDB",
      "AddressDB",
      "ToDoDB",
      "MemoDB",
      "Memo32DB",
#ifdef ENABLE_MANANA
      "MañanaDB",
#endif
      "Saved Preferences",
      NULL
   };
   char *extra_dbname[]={
      "Saved Preferences",
      NULL
   };

   typedef struct skip_db_t {
       unsigned int flags;
       unsigned int not_flags;
       char *creator;
       char *dbname;
   } skip_db_t ;

   skip_db_t skip_db[] = {
	{ 0, dlpDBFlagResource, "AvGo", NULL },
	{ 0, dlpDBFlagResource, "psys", "Unsaved Preferences" },
	{ 0, 0, NULL, NULL} 
   };

   /*
    * Here are the possibilities for mode:
    * 0. slow sync                  fetch DBs if main app
    * 1. slow sync && full backup   fetch DBs if main app, copy DB to backup
    * 2. fast sync                  return
    * 3. fast sync && full backup   fetch DBs in backup dir
    */
   jp_logf(JP_LOG_DEBUG, "sync_fetch flags=0x%x, num_backups=%d, fast=%d\n",
	   flags, num_backups, fast_sync);

   end_of_list=NULL;

   mode = ((flags & SYNC_FULL_BACKUP) ? 1:0) + (fast_sync ? 2:0);

   if (mode == 2) {
      fetch_extra_DBs(sd, extra_dbname);
      return 0;
   }

   if ((flags & SYNC_FULL_BACKUP)) {
      jp_logf(JP_LOG_DEBUG, "Full Backup\n");
      pi_watchdog(sd,10); /* prevent from timing out on long copy times */
      sync_rotate_backups(num_backups);
      pi_watchdog(sd,0);  /* back to normal behavior */
   }

   start=cardno=0;
   file_list=NULL;

   while( (r=dlp_ReadDBList(sd, cardno, dlpOpenRead, start, &info)) > 0) {
      start=info.index+1;
      creator[0] = (info.creator & 0xFF000000) >> 24;
      creator[1] = (info.creator & 0x00FF0000) >> 16,
      creator[2] = (info.creator & 0x0000FF00) >> 8,
      creator[3] = (info.creator & 0x000000FF);
      creator[4] = '\0';
#ifdef JPILOT_DEBUG
      jp_logf(JP_LOG_DEBUG, "dbname = %s\n",info.name);
      jp_logf(JP_LOG_DEBUG, "exclude from sync = %d\n",info.miscFlags & dlpDBMiscFlagExcludeFromSync);
      jp_logf(JP_LOG_DEBUG, "flag backup = %d\n",info.flags & dlpDBFlagBackup);
      /*jp_logf(JP_LOG_DEBUG, "type = %x\n",info.type);*/
      jp_logf(JP_LOG_DEBUG, "Creator ID = [%s]\n", creator);
#endif
      if (flags & SYNC_FULL_BACKUP) {
	 /* Look at the skip list */
	 manual_skip=0;
	 for (i=0; skip_db[i].creator || skip_db[i].dbname; i++) {
	    if (skip_db[i].creator && 
		!strcmp(creator, skip_db[i].creator)) {
	       if (skip_db[i].flags && 
		   (info.flags & skip_db[i].flags) != skip_db[i].flags) {
		  manual_skip=1;
		  break;
	       }
	       else if (skip_db[i].not_flags && 
			!(info.flags & skip_db[i].not_flags)) {
		  manual_skip=1;
		  break;
	       }
	       else if (!skip_db[i].flags && !skip_db[i].not_flags) {
		  manual_skip=1;
		  break;
	       }
	    }
	    if (skip_db[i].dbname &&
		!strcmp(info.name,skip_db[i].dbname)) {
	       if (skip_db[i].flags && 
		   (info.flags & skip_db[i].flags) != skip_db[i].flags) {
		  manual_skip=1;
		  break;
	       }
	       else if (skip_db[i].not_flags && 
			(info.flags & skip_db[i].not_flags)) {
		  manual_skip=1;
		  break;
	       }
	       else if (!skip_db[i].flags && !skip_db[i].not_flags) {
		  manual_skip=1;
		  break;
	       }
	    }
	 }
	 if (manual_skip) {
	    jp_logf(JP_LOG_GUI, _("Skipping %s (Creator ID '%s')\n"), info.name, creator);
	    continue;
	 }
      }

      main_app=0;
      for (i=0; palm_dbname[i]; i++) {
	 if (!strcmp(info.name, palm_dbname[i])) {
	    jp_logf(JP_LOG_DEBUG, "Found main app\n");
	    main_app = 1;
	    break;
	 }
      }
#ifdef ENABLE_PLUGINS
      plugin_list = get_plugin_list();

      if (!main_app) {
	 for (temp_list = plugin_list; temp_list; temp_list = temp_list->next) {
	    plugin = (struct plugin_s *)temp_list->data;
	    if (!strcmp(info.name, plugin->db_name)) {
	       jp_logf(JP_LOG_DEBUG, "Found plugin\n");
	       main_app = 1;
	       break;
	    }
	 }
      }
#endif
      strncpy(db_copy_name, info.name, MAX_DBNAME-5);
      db_copy_name[MAX_DBNAME-5]='\0';
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
      get_home_file_name(db_copy_name, full_name, 255);
      get_home_file_name("backup/", full_backup_name, 255);
      strcat(full_backup_name, db_copy_name);

      /* Add this to our file name list if not manually skipped */
      jp_logf(JP_LOG_DEBUG, "appending [%s]\n", db_copy_name);
      if (file_list==NULL) {
	 file_list = g_list_append(file_list, strdup(db_copy_name));
	 end_of_list=file_list;
      } else {
	 file_list = g_list_append(file_list, strdup(db_copy_name));
	 if (end_of_list->next) {
	    end_of_list=end_of_list->next;
	 }
      }

      if ( (mode==0) && (!main_app) ) {
	 continue;
      }
#ifdef JPILOT_DEBUG
      if (main_app) {
	 jp_logf(JP_LOG_DEBUG, "main_app is set\n");
      }
#endif

      statb.st_mtime = 0;

      if (main_app && (mode<2)) {
	 file_name = full_name;
      } else {
	 file_name = full_backup_name;
      }
      stat(file_name, &statb);
#ifdef JPILOT_DEBUG
      jp_logf(JP_LOG_GUI, "palm dbtime= %d, local dbtime = %d\n", info.modifyDate, statb.st_mtime);
      jp_logf(JP_LOG_GUI, "flags=0x%x\n", info.flags);
      jp_logf(JP_LOG_GUI, "backup_flag=%d\n", info.flags & dlpDBFlagBackup);
#endif
      /* If modification times are the same then we don t need to fetch it */
      if (info.modifyDate == statb.st_mtime) {
	 jp_logf(JP_LOG_GUI, _("%s (Creator ID '%s') is up to date, fetch skipped.\n"), db_copy_name, creator);
	 continue;
      }

      jp_logf(JP_LOG_GUI, _("Fetching '%s' (Creator ID '%s')... "), info.name, creator);

      info.flags &= 0xff;

      pi_fp = pi_file_create(file_name, &info);
      if (pi_fp==0) {
	 jp_logf(JP_LOG_WARN, "Failed, unable to create file %s\n",
		main_app ? full_name : full_backup_name);
	 continue;
      }
      if (pi_file_retrieve(pi_fp, sd, 0)<0) {
	 jp_logf(JP_LOG_WARN, "Failed, unable to back up database\n");
	 times.actime = 0;
	 times.modtime = 0;
      } else {
	 jp_logf(JP_LOG_GUI, _("OK\n"));
	 times.actime = info.createDate;
	 times.modtime = info.modifyDate;
      }
      pi_file_close(pi_fp);

      /*Set the create and modify times of local file to same as on palm */
      utime(file_name, &times);

      /* This call preserves the file times */
      if ((main_app) && (mode==1)) {
	 jp_copy_file(full_name, full_backup_name);
      }
   }
   /* I'm not sure why pilot-link-0.11 is returning dlpErrNoneOpen */
   if ( ! ((r==dlpErrNotFound) || (r==dlpErrNoneOpen)) ) {
      jp_logf(JP_LOG_WARN, "ReadDBList returned = %d\n", r);
   } else {
      jp_logf(JP_LOG_DEBUG, "Good return code (dlpErrNotFound)\n");
      if ((mode==1) || (mode==3)) {
	 jp_logf(JP_LOG_DEBUG, "Removing apps not found on the palm\n");
	 move_removed_apps(file_list);
      }
   }

   free_file_name_list(&file_list);

   return 0;
}

static int sync_install(char *filename, int sd)
{
   struct pi_file *f;
   struct  DBInfo info;
   char *Pc;
   char log_entry[256];
   int r, try_again;
   long char_set;
   char creator[5];

   get_pref(PREF_CHAR_SET, &char_set, NULL);

   Pc=strrchr(filename, '/');
   if (!Pc) {
      Pc = filename;
   } else {
      Pc++;
   }

   jp_logf(JP_LOG_GUI, _("Installing %s "), Pc);
   f = pi_file_open(filename);
   if (f==0) {
      jp_logf(JP_LOG_WARN, _("\nUnable to open '%s'!\n"), filename);
      return -1;
   }
   bzero(&info, sizeof(info));
   pi_file_get_info(f, &info);
   creator[0] = (info.creator & 0xFF000000) >> 24;
   creator[1] = (info.creator & 0x00FF0000) >> 16,
   creator[2] = (info.creator & 0x0000FF00) >> 8,
   creator[3] = (info.creator & 0x000000FF);
   creator[4] = '\0';
   jp_logf(JP_LOG_GUI, _("(Creator ID is '%s')..."), creator);

   r = pi_file_install(f, sd, 0);
   if (r<0) {
      try_again = 0;
      /* TODO make this generic? Not sure it would work 100% of the time */
      /* Here we make a special exception for graffiti */
      if (!strcmp(info.name, "Graffiti ShortCuts")) {
	 strcpy(info.name, "Graffiti ShortCuts ");
	 /* This requires a reset */
	 info.flags |= dlpDBFlagReset;
	 info.flags |= dlpDBFlagNewer;
	 try_again = 1;
      } else if (!strcmp(info.name, "Graffiti ShortCuts ")) {
	 strcpy(info.name, "Graffiti ShortCuts");
	 /* This requires a reset */
	 info.flags |= dlpDBFlagReset;
	 info.flags |= dlpDBFlagNewer;
	 try_again = 1;
      }
      /* Here we make a special exception for Net Prefs */
      if (!strcmp(info.name, "Net Prefs")) {
	 strcpy(info.name, "Net Prefs ");
	 /* This requires a reset */
	 info.flags |= dlpDBFlagReset;
	 info.flags |= dlpDBFlagNewer;
	 try_again = 1;
      } else if (!strcmp(info.name, "Net Prefs ")) {
	 strcpy(info.name, "Net Prefs");
	 /* This requires a reset */
	 info.flags |= dlpDBFlagReset;
	 info.flags |= dlpDBFlagNewer;
	 try_again = 1;
      }
      if (try_again) {
	 /* Try again */
	 r = pi_file_install(f, sd, 0);
      }
   }

   if (r<0) {
      g_snprintf(log_entry, 255, _("Install %s failed"), Pc);
      log_entry[255]='\0';
      charset_j2p((unsigned char *)log_entry, 255, char_set);
      dlp_AddSyncLogEntry(sd, log_entry);
      dlp_AddSyncLogEntry(sd, "\n");;
      jp_logf(JP_LOG_GUI, _("Failed.\n"));
      jp_logf(JP_LOG_WARN, log_entry);
      pi_file_close(f);
      return -1;
   }
   else {
      /* the space after the %s is a hack, the last char gets cut off */
      g_snprintf(log_entry, 255, _("Installed %s "), Pc);
      log_entry[255]='\0';
      charset_j2p((unsigned char *)log_entry, 255, char_set);
      dlp_AddSyncLogEntry(sd, log_entry);
      dlp_AddSyncLogEntry(sd, "\n");;
      jp_logf(JP_LOG_GUI, _("OK\n"));
   }
   pi_file_close(f);

   return 0;
}


/*file must not be open elsewhere when this is called */
/*the first line is 0 */
static int sync_process_install_file(int sd)
{
   FILE *in;
   FILE *out;
   char line[1002];
   char *Pc;
   int r, line_count;

   in = jp_open_home_file(EPN"_to_install", "r");
   if (!in) {
      jp_logf(JP_LOG_WARN, _("Cannot open "EPN"_to_install file\n"));
      return -1;
   }

   out = jp_open_home_file(EPN"_to_install.tmp", "w");
   if (!out) {
      jp_logf(JP_LOG_WARN, _("Cannot open "EPN"_to_install.tmp file\n"));
      fclose(in);
      return -1;
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
      r = sync_install(line, sd);
      if (r==0) {
	 continue;
      }
      fprintf(out, "%s\n", line);
   }
   fclose(in);
   fclose(out);

   rename_file(EPN"_to_install.tmp", EPN"_to_install");

   return 0;
}

int is_backup_dir(char *name)
{
   int i;

   /* backup dirs are of the form backupMMDDHHMM */
   if (strncmp(name, "backup", 6)) {
      return 0;
   }
   for (i=6; i<14; i++) {
      if (name[i]=='\0') {
	 return 0;
      }
      if (!isdigit(name[i])) {
	 return 0;
      }
   }
   if (name[i]!='\0') {
      return 0;
   }
   return 1;
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

int sync_remove_r(char *full_path)
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
	 strncpy(last4, dirent->d_name+len-4, 4);
	 last4[4]='\0';
	 if ((strcmp(last4, ".pdb")==0) || 
	     (strcmp(last4, ".prc")==0) ||
	     (strcmp(last4, ".pqa")==0)) {
	    unlink(full_src);
	 }
      }
      closedir(dir);
   }
   rmdir(full_path);

   return 0;
}

static int get_oldest_newest_dir(char *oldest, char *newest, int *count)
{
   DIR *dir;
   struct dirent *dirent;
   char home_dir[256];
   int r;

   get_home_file_name("", home_dir, 255);
   jp_logf(JP_LOG_DEBUG, "rotate_backups: opening dir %s\n", home_dir);
   *count = 0;
   oldest[0]='\0';
   newest[0]='\0';
   dir = opendir(home_dir);
   if (!dir) {
      return -1;
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
	    /*jp_logf(JP_LOG_DEBUG, "newest is now %s\n", newest);*/
	 }
	 r = compare_back_dates(oldest, dirent->d_name);
	 if (r==1) {
	    strcpy(oldest, dirent->d_name);
	    /*jp_logf(JP_LOG_DEBUG, "oldest is now %s\n", oldest);*/
	 }
	 r = compare_back_dates(newest, dirent->d_name);
	 if (r==2) {
	    strcpy(newest, dirent->d_name);
	    /*jp_logf(JP_LOG_DEBUG, "newest is now %s\n", newest);*/
	 }
      }
   }
   closedir(dir);
   return 0;
}

static int sync_rotate_backups(const int num_backups)
{
   DIR *dir;
   struct dirent *dirent;
   char home_dir[256];
   char full_name[300];
   char full_newdir[300];
   char full_backup[300];
   char full_oldest[300];
   char full_src[300];
   char full_dest[300];
   int r;
   int count, safety;
   char oldest[20];
   char newest[20];
   char newdir[20];
   time_t ltime;
   struct tm *now;

   get_home_file_name("", home_dir, 255);

   /* We use safety because if removing the directory fails then we
    * will get stuck in an endless loop */
   for (safety=100; safety>0; safety--) {
      r = get_oldest_newest_dir(oldest, newest, &count);
      if (r<0) {
	 jp_logf(JP_LOG_WARN, "unable to read home dir\n");
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
   sprintf(newdir, "backup%02d%02d%02d%02d",
	   now->tm_mon+1, now->tm_mday, now->tm_hour, now->tm_min);
   if (strcmp(newdir, newest)) {
      sprintf(full_newdir, "%s/%s", home_dir, newdir);
      if (mkdir(full_newdir, 0700)==0) {
	 count++;
      }
   }

   /* Copy from the newest backup, if it exists */
   if (strcmp(newdir, newest)) {
      sprintf(full_backup, "%s/backup", home_dir);
      sprintf(full_newdir, "%s/%s", home_dir, newdir);
      dir = opendir(full_backup);
      if (dir) {
	 while ((dirent = readdir(dir))) {
	    sprintf(full_src, "%s/%s", full_backup, dirent->d_name);
	    sprintf(full_dest, "%s/%s", full_newdir, dirent->d_name);
	    jp_copy_file(full_src, full_dest);
	 }
	 closedir(dir);
      }
   }

   /* Remove the oldest backup if needed */
   if (count > num_backups) {
      if ( (oldest[0]!='\0') && (strcmp(newdir, oldest)) ) {
	 sprintf(full_oldest, "%s/%s", home_dir, oldest);
	 jp_logf(JP_LOG_DEBUG, "removing dir [%s]\n", full_oldest);
	 sync_remove_r(full_oldest);
      }
   }

   /* Delete the symlink */
   sprintf(full_name, "%s/backup", home_dir);
   unlink(full_name);

   /* Create the symlink */
   symlink(newdir, full_name);

   return 0;
}

int fast_sync_local_recs(char *DB_name, int sd, int db)
{
   int ret;
   int num;
   FILE *pc_in;
   PC3RecordHeader header;
   unsigned int old_unique_id;
   char *record;
   void *Pbuf;
   int rec_len;
   char pc_filename[256];
   char write_log_message[256];
   char error_log_message_w[256];
   char error_log_message_d[256];
   char delete_log_message[256];
   int index, size, attr, category;
   long char_set;

   jp_logf(JP_LOG_DEBUG, "fast_sync_local_recs\n");
   get_pref(PREF_CHAR_SET, &char_set, NULL);

   if ((DB_name==NULL) || (strlen(DB_name) > 250)) {
      return -1;
   }
   g_snprintf(pc_filename, 255, "%s.pc3", DB_name);
   /* This is an attempt to use the proper pronoun most of the time */
   if (strchr("aeiou", tolower(DB_name[0]))) {
      g_snprintf(write_log_message, 255,
	      _("Wrote an %s record."), DB_name);
      g_snprintf(error_log_message_w, 255,
	      _("Writing an %s record failed."), DB_name);
      g_snprintf(error_log_message_d, 255,
	      _("Deleting an %s record failed."), DB_name);
      g_snprintf(delete_log_message, 256,
	      _("Deleted an %s record."), DB_name);
   } else {
      g_snprintf(write_log_message, 255,
	      _("Wrote a %s record."), DB_name);
      g_snprintf(error_log_message_w, 255,
	      _("Writing a %s record failed."), DB_name);
      g_snprintf(error_log_message_d, 255,
	      _("Deleting a %s record failed."), DB_name);
      g_snprintf(delete_log_message, 256,
	      _("Deleted a %s record."), DB_name);
   }
   pc_in = jp_open_home_file(pc_filename, "r+");
   if (pc_in==NULL) {
      jp_logf(JP_LOG_WARN, _("Unable to open %s\n"), pc_filename);
      return -1;
   }

   while(!feof(pc_in)) {
      num = read_header(pc_in, &header);
      if (num!=1) {
	 if (ferror(pc_in)) {
	    break;
	 }
	 if (feof(pc_in)) {
	    break;
	 }
      }
      rec_len = header.rec_len;
      if (rec_len > 0x10000) {
	 jp_logf(JP_LOG_WARN, _("PC file corrupt?\n"));
	 fclose(pc_in);
	 return -1;
      }
      /* Case 5: */
      if ((header.rt==NEW_PC_REC) || (header.rt==REPLACEMENT_PALM_REC)) {
	 jp_logf(JP_LOG_DEBUG, "new pc record\n");
	 record = malloc(rec_len);
	 if (!record) {
	    jp_logf(JP_LOG_WARN, _("fast_sync_local_recs(): Out of memory\n"));
	    break;
	 }
	 num = fread(record, rec_len, 1, pc_in);
	 if (num != 1) {
	    if (ferror(pc_in)) {
	       break;
	    }
	 }

	 jp_logf(JP_LOG_DEBUG, "Writing PC record to palm\n");

	 old_unique_id=header.unique_id;
	 if (header.rt==REPLACEMENT_PALM_REC) {
	    ret = dlp_WriteRecord(sd, db, header.attrib & dlpRecAttrSecret,
				  header.unique_id, header.attrib & 0x0F,
				  record, rec_len, &header.unique_id);
	 } else {
	    ret = dlp_WriteRecord(sd, db, header.attrib & dlpRecAttrSecret,
				  0, header.attrib & 0x0F,
				  record, rec_len, &header.unique_id);
	 }

	 jp_logf(JP_LOG_DEBUG, "Writing PC record to local\n");
	 if (ret >=0) {
	    if ((header.rt==REPLACEMENT_PALM_REC) &&
		(old_unique_id != header.unique_id)) {
	       /* There is a possibility that the palm handed back a unique ID
		* other than the one we requested
		*/
	       pdb_file_delete_record_by_id(DB_name, old_unique_id);
	    }
	    pdb_file_modify_record(DB_name, record, rec_len,
				   header.attrib & dlpRecAttrSecret,
				   header.attrib & 0x0F, header.unique_id);
	 }

	 if (record) {
	    free(record);
	    record = NULL;
	 }

	 if (ret < 0) {
	    jp_logf(JP_LOG_WARN, _("dlp_WriteRecord failed\n"));
	    charset_j2p((unsigned char *)error_log_message_w,255,char_set);
	    dlp_AddSyncLogEntry(sd, error_log_message_w);
	    dlp_AddSyncLogEntry(sd, "\n");
	 } else {
	    charset_j2p((unsigned char *)write_log_message,255,char_set);
	    dlp_AddSyncLogEntry(sd, write_log_message);
	    dlp_AddSyncLogEntry(sd, "\n");
	    /* Now mark the record as deleted in the pc file */
	    if (fseek(pc_in, -(header.header_len+rec_len), SEEK_CUR)) {
	       jp_logf(JP_LOG_WARN, _("fseek failed - fatal error\n"));
	       fclose(pc_in);
	       return -1;
	    }
	    header.rt=DELETED_PC_REC;
	    write_header(pc_in, &header);
	 }
      }
      /* Case 3: */
      /* FIXME What should happen here is that if the records are the same
       * then go ahead and delete, otherwise make a copy of the record
       * so that there is no data loss.
       * memcmp doesn't seem to work for datebook, probably because
       * some of the struct tm fields and things can be different and
       * not affect the record.
       */
      if ((header.rt==DELETED_PALM_REC) || (header.rt==MODIFIED_PALM_REC)) {
	 jp_logf(JP_LOG_DEBUG, "deleted or modified pc record\n");
	 rec_len = header.rec_len;
	 record = malloc(rec_len);
	 num = fread(record, rec_len, 1, pc_in);
	 if (num != 1) {
	    if (ferror(pc_in)) {
	       break;
	    }
	 }
	 if (fseek(pc_in, -rec_len, SEEK_CUR)) {
	    jp_logf(JP_LOG_WARN, _("fseek failed - fatal error\n"));
	    fclose(pc_in);
	    return -1;
	 }
	 ret = pdb_file_read_record_by_id(DB_name, 
					  header.unique_id,
					  &Pbuf, &size, &index,
					  &attr, &category);
	 /* ret = dlp_ReadRecordById(sd, db, header.unique_id, buffer,
				  &index, &size, &attr, &category); */
	 if (Pbuf) free (Pbuf);
#ifdef JPILOT_DEBUG
	 if (ret>=0 ) {
	    printf("read record by id %s returned %d\n", DB_name, ret);
	    printf("id %d, index %d, size %d, attr 0x%x, category %d\n",
		   header.unique_id, index, size, attr, category);
	 }
#endif
	 if ((rec_len == size) && (header.unique_id != 0)) {
	    jp_logf(JP_LOG_DEBUG, "sizes match!\n");
	    /* FIXME This code never worked right.  It could be because
	     * Pbuf was freed in pi_file_close before here
	     * I have not tested it since I fixed it */
	    /* if (memcmp(record, Pbuf, size)==0)
	    jp_logf(JP_LOG_DEBUG,"Binary is the same!\n"); */
	    /* jp_logf(JP_LOG_GUI, "Deleting Palm id=%d,\n",header.unique_id);*/
	    ret = dlp_DeleteRecord(sd, db, 0, header.unique_id);
	    if (ret < 0) {
	       jp_logf(JP_LOG_WARN, _("dlp_DeleteRecord failed\n"
		      "This could be because the record was already deleted on the Palm\n"));
	       charset_j2p((unsigned char *)error_log_message_d,255,char_set);
	       dlp_AddSyncLogEntry(sd, error_log_message_d);
	       dlp_AddSyncLogEntry(sd, "\n");
	    } else {
	       charset_j2p((unsigned char *)delete_log_message,255,char_set);
	       dlp_AddSyncLogEntry(sd, delete_log_message);
	       dlp_AddSyncLogEntry(sd, "\n");
	       pdb_file_delete_record_by_id(DB_name, header.unique_id);
	    }
	 }

	 /*Now mark the record as deleted */
	 if (fseek(pc_in, -header.header_len, SEEK_CUR)) {
	    jp_logf(JP_LOG_WARN, _("fseek failed - fatal error\n"));
	    fclose(pc_in);
	    return -1;
	 }
	 header.rt=DELETED_DELETED_PALM_REC;
	 write_header(pc_in, &header);
      }

      /*skip this record now that we are done with it */
      if (fseek(pc_in, rec_len, SEEK_CUR)) {
	 jp_logf(JP_LOG_WARN, _("fseek failed - fatal error\n"));
	 fclose(pc_in);
	 return -1;
      }
   }
   fclose(pc_in);

   return 0;
}

/*
 * This takes 2 category indexes and swaps them in every record.
 * returns the number of record's categories swapped.
 */
int pdb_file_swap_indexes(char *DB_name, int index1, int index2)
{
   char local_pdb_file[256];
   char full_local_pdb_file[256];
   char full_local_pdb_file2[256];
   struct pi_file *pf1, *pf2;
   struct DBInfo infop;
   void *app_info;
   void *sort_info;
   void *record;
   int r;
   int idx;
   int size;
   int attr;
   int cat, new_cat;
   int count;
   pi_uid_t uid;

   jp_logf(JP_LOG_DEBUG, "pi_file_swap_indexes\n");

   g_snprintf(local_pdb_file, 250, "%s.pdb", DB_name);
   get_home_file_name(local_pdb_file, full_local_pdb_file, 250);
   strcpy(full_local_pdb_file2, full_local_pdb_file);
   strcat(full_local_pdb_file2, "2");

   pf1 = pi_file_open(full_local_pdb_file);
   if (!pf1) {
      jp_logf(JP_LOG_WARN, "Couldn't open [%s]\n", full_local_pdb_file);
      return -1;
   }
   pi_file_get_info(pf1, &infop);
   pf2 = pi_file_create(full_local_pdb_file2, &infop);
   if (!pf2) {
      jp_logf(JP_LOG_WARN, "Couldn't open [%s]\n", full_local_pdb_file2);
      return -1;
   }

   pi_file_get_app_info(pf1, &app_info, &size);
   pi_file_set_app_info(pf2, app_info, size);

   pi_file_get_sort_info(pf1, &sort_info, &size);  
   pi_file_set_sort_info(pf2, sort_info, size);

   count = 0;

   for(idx=0;;idx++) {
      r = pi_file_read_record(pf1, idx, &record, &size, &attr, &cat, &uid);
      if (r<0) break;
      new_cat=cat;
      if (cat==index1) {
	 new_cat=index2;
	 count++;
      }
      if (cat==index2) {
	 new_cat=index1;
	 count++;
      }
      pi_file_append_record(pf2, record, size, attr, new_cat, uid);
   }

   pi_file_close(pf1);
   pi_file_close(pf2);

   if (rename(full_local_pdb_file2, full_local_pdb_file) < 0) {
      jp_logf(JP_LOG_WARN, "swap_indexes: rename failed\n");
   }

   return 0;
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
 *     change LR, If it doesn't exist then add it
 * For each LR
 *   Case 3:
 *   if LR deleted or archived
 *     if RR==OLR (Original LR) remove RR, and LR
 *   Case 4:
 *   if LR changed
 *     We have a new local record (NLR) and a 
 *        modified (deleted) local record (MLR)
 *     if NLR==RR then do nothing (either both were changed equally, or
 *				   local was changed and changed back)
 *     add NLR to remote, if RR==LR remove RR
 *   Case 5:
 *   if new LR
 *     add LR to remote
 */
int fast_sync_application(char *DB_name, int sd)
{
   int db;
   int ret;
   char write_log_message[256];
   char error_log_message_w[256];
   char error_log_message_d[256];
   char delete_log_message[256];
   char log_entry[256];
   recordid_t id=0;
   int index, size, attr, category;
   int local_num, palm_num;
   unsigned char buffer[65536];
   char *extra_dbname[2];
   long char_set;

   if ((DB_name==NULL) || (strlen(DB_name) == 0) || (strlen(DB_name) > 250)) {
      return -1;
   }

   jp_logf(JP_LOG_DEBUG, "fast_sync_application %s\n", DB_name);
   get_pref(PREF_CHAR_SET, &char_set, NULL);

   g_snprintf(log_entry, 255, _("Syncing %s\n"), DB_name);
   log_entry[255]='\0';
   jp_logf(JP_LOG_GUI, log_entry);

   /* This is an attempt to use the proper pronoun most of the time */
   if (strchr("aeiou", tolower(DB_name[0]))) {
      g_snprintf(write_log_message, 255,
	      _("Wrote an %s record."),  DB_name);
      g_snprintf(error_log_message_w, 255,
	      _("Writing an %s record failed."), DB_name);
      g_snprintf(error_log_message_d, 255,
	      _("Deleting an %s record failed."), DB_name);
      g_snprintf(delete_log_message, 256,
	      _("Deleted an %s record."),  DB_name);
   } else {
      g_snprintf(write_log_message, 255,
	      _("Wrote a %s record."),  DB_name);
      g_snprintf(error_log_message_w, 255,
	      _("Writing a %s record failed."), DB_name);
      g_snprintf(error_log_message_d, 255,
	      _("Deleting a %s record failed."), DB_name);
      g_snprintf(delete_log_message, 256,
	      _("Deleted a %s record."),  DB_name);
   }
   /* Open the applications database, store access handle in db */
   ret = dlp_OpenDB(sd, 0, dlpOpenReadWrite|dlpOpenSecret, DB_name, &db);
   if (ret < 0) {
      g_snprintf(log_entry, 255, _("Unable to open %s\n"), DB_name);
      log_entry[255]='\0';
      charset_j2p((unsigned char *)log_entry, 255, char_set);
      dlp_AddSyncLogEntry(sd, log_entry);
      return -1;
   }

   /* I can't get the appinfodirty flag to work, so I do this for now */
   /*ret = dlp_ReadAppBlock(sd, db, 0, buffer, 65535);
   jp_logf(JP_LOG_DEBUG, "readappblock ret=%d\n", ret);
   if (ret>0) {
      pdb_file_write_app_block(DB_name, buffer, ret);
   }*/

   while(1) {
      ret = dlp_ReadNextModifiedRec(sd, db, buffer,
				    &id, &index, &size, &attr, &category);
      if (ret>=0 ) {
	 jp_logf(JP_LOG_DEBUG, "read next record for %s returned %d\n", DB_name, ret);
	 jp_logf(JP_LOG_DEBUG, "id %ld, index %d, size %d, attr 0x%x, category %d\n",id, index, size, attr, category);
      } else {
	 break;
      }
      /* Case 1: */
      if ((attr &  dlpRecAttrDeleted) || (attr & dlpRecAttrArchived)) {
	 jp_logf(JP_LOG_DEBUG, "found a deleted record on palm\n");
	 pdb_file_delete_record_by_id(DB_name, id);
	 continue;
      }
      /* Case 2: */
      /* Note that if deleted we don't want to deal with it (taken care of above) */
      if (attr & dlpRecAttrDirty) {
	 jp_logf(JP_LOG_DEBUG, "found a deleted record on palm\n");
	 pdb_file_modify_record(DB_name, buffer, size, attr, category, id);
      }
   }

   fast_sync_local_recs(DB_name, sd, db);

   dlp_ResetSyncFlags(sd, db);
   dlp_CleanUpDatabase(sd, db);

   /* Count the number of records, should be equal, may not be */
   dlp_ReadOpenDBInfo(sd, db, &palm_num);
   pdb_file_count_recs(DB_name, &local_num);

   dlp_CloseDB(sd, db);

   if (local_num != palm_num) {
      extra_dbname[0] = DB_name;
      extra_dbname[1] = NULL;
      jp_logf(JP_LOG_DEBUG, "fetch_extra_DBs() [%s]\n", extra_dbname[0]);
      jp_logf(JP_LOG_DEBUG ,_("palm: number of records = %d\n"), palm_num);
      jp_logf(JP_LOG_DEBUG ,_("disk: number of records = %d\n"), local_num);
      fetch_extra_DBs(sd, extra_dbname);
   }

   return 0;
}


int unpack_address_cai_from_ai(struct CategoryAppInfo *cai, unsigned char *ai_raw, int len)
{
   struct AddressAppInfo ai;
   int r;
   
   jp_logf(JP_LOG_DEBUG, "unpack_address_cai_from_ai\n");

   r = unpack_AddressAppInfo(&ai, ai_raw, len);
   if ((r <= 0) || (len <= 0)) {
      jp_logf(JP_LOG_DEBUG, "unpack_AddressAppInfo failed %s %d\n", __FILE__, __LINE__);
      return -1;
   }
   memcpy(cai, &(ai.category), sizeof(struct CategoryAppInfo));
	  
   return 0;
}

int pack_address_cai_into_ai(struct CategoryAppInfo *cai, unsigned char *ai_raw, int len)
{
   struct AddressAppInfo ai;
   int r;

   jp_logf(JP_LOG_DEBUG, "pack_address_cai_into_ai\n");

   r = unpack_AddressAppInfo(&ai, ai_raw, len);
   if (r <= 0) {
      jp_logf(JP_LOG_DEBUG, "unpack_AddressAppInfo failed %s %d\n", __FILE__, __LINE__);
      return -1;
   }
   memcpy(&(ai.category), cai, sizeof(struct CategoryAppInfo));

   r = pack_AddressAppInfo(&ai, ai_raw, len);
   if (r <= 0) {
      jp_logf(JP_LOG_DEBUG, "pack_AddressAppInfo failed %s %d\n", __FILE__, __LINE__);
      return -1;
   }
   
   return 0;
}

int unpack_todo_cai_from_ai(struct CategoryAppInfo *cai, unsigned char *ai_raw, int len)
{
   struct ToDoAppInfo ai;
   int r;
   
   jp_logf(JP_LOG_DEBUG, "unpack_todo_cai_from_ai\n");

   r = unpack_ToDoAppInfo(&ai, ai_raw, len);
   if ((r <= 0) || (len <= 0)) {
      jp_logf(JP_LOG_DEBUG, "unpack_ToDoAppInfo failed %s %d\n", __FILE__, __LINE__);
      return -1;
   }
   memcpy(cai, &(ai.category), sizeof(struct CategoryAppInfo));
	  
   return 0;
}

int pack_todo_cai_into_ai(struct CategoryAppInfo *cai, unsigned char *ai_raw, int len)
{
   struct ToDoAppInfo ai;
   int r;

   jp_logf(JP_LOG_DEBUG, "pack_todo_cai_into_ai\n");

   r = unpack_ToDoAppInfo(&ai, ai_raw, len);
   if (r <= 0) {
      jp_logf(JP_LOG_DEBUG, "unpack_ToDoAppInfo failed %s %d\n", __FILE__, __LINE__);
      return -1;
   }
   memcpy(&(ai.category), cai, sizeof(struct CategoryAppInfo));

   r = pack_ToDoAppInfo(&ai, ai_raw, len);
   if (r <= 0) {
      jp_logf(JP_LOG_DEBUG, "pack_ToDooAppInfo failed %s %d\n", __FILE__, __LINE__);
      return -1;
   }
   
   return 0;
}

int unpack_memo_cai_from_ai(struct CategoryAppInfo *cai, unsigned char *ai_raw, int len)
{
   struct MemoAppInfo ai;
   int r;
   
   jp_logf(JP_LOG_DEBUG, "unpack_memo_cai_from_ai\n");

   r = unpack_MemoAppInfo(&ai, ai_raw, len);
   if ((r <= 0) || (len <= 0)) {
      jp_logf(JP_LOG_DEBUG, "unpack_MemoAppInfo failed %s %d\n", __FILE__, __LINE__);
      return -1;
   }
   memcpy(cai, &(ai.category), sizeof(struct CategoryAppInfo));
	  
   return 0;
}

int pack_memo_cai_into_ai(struct CategoryAppInfo *cai, unsigned char *ai_raw, int len)
{
   struct MemoAppInfo ai;
   int r;

   jp_logf(JP_LOG_DEBUG, "pack_memo_cai_into_ai\n");

   r = unpack_MemoAppInfo(&ai, ai_raw, len);
   if (r <= 0) {
      jp_logf(JP_LOG_DEBUG, "unpack_MemoAppInfo failed %s %d\n", __FILE__, __LINE__);
      return -1;
   }
   memcpy(&(ai.category), cai, sizeof(struct CategoryAppInfo));

   r = pack_MemoAppInfo(&ai, ai_raw, len);
   if (r <= 0) {
      jp_logf(JP_LOG_DEBUG, "pack_MemoAppInfo failed %s %d\n", __FILE__, __LINE__);
      return -1;
   }
   
   return 0;
}

int sync_categories(char *DB_name, int sd,
		    int (*unpack_cai_from_ai)(struct CategoryAppInfo *cai, unsigned char *ai_raw, int len),
		    int (*pack_cai_into_ai)(struct CategoryAppInfo *cai, unsigned char *ai_raw, int len)
)
{
   struct CategoryAppInfo local_cai, remote_cai;
   char full_name[256];
   char pdb_name[256];
   char log_entry[256];
   unsigned char buf[65536];
   char tmp_name[18];
   int i, r, Li, Ri;
   int size;
   int size_Papp_info;
   void *Papp_info;
   struct pi_file *pf;
   int db;
   int found_name, found_ID;
   int found_name_at, found_ID_at;
   int found_a_hole;
   int loop;
   long char_set;

   get_pref(PREF_CHAR_SET, &char_set, NULL);

   jp_logf(JP_LOG_DEBUG, "sync_categories for %s\n", DB_name);

   sprintf(pdb_name, "%s%s", DB_name, ".pdb");
   get_home_file_name(pdb_name, full_name, 250);

   Papp_info=NULL;
   bzero(&local_cai, sizeof(local_cai));
   bzero(&remote_cai, sizeof(remote_cai));

   pf = pi_file_open(full_name);
   if (!pf) {
      jp_logf(JP_LOG_WARN, _("Error reading at %s : %s %d\n"), full_name, __FILE__, __LINE__);
      return -1;
   }
   r = pi_file_get_app_info(pf, &Papp_info, &size_Papp_info);
   if (size_Papp_info <= 0) {
      jp_logf(JP_LOG_WARN, _("%s:%d Error getting app info %s\n"), __FILE__, __LINE__, full_name);
      return -1;
   }

   r = unpack_cai_from_ai(&local_cai, Papp_info, size_Papp_info);
   if (r < 0) {
      jp_logf(JP_LOG_WARN, _("%s:%d Error unpacking app info %s\n"), __FILE__, __LINE__, full_name);
      return -1;
   }

   pi_file_close(pf);

   /* Open the applications database, store access handle in db */
   r = dlp_OpenDB(sd, 0, dlpOpenReadWrite, DB_name, &db);
   if (r < 0) {
      g_snprintf(log_entry, 255, _("Unable to open %s\n"), DB_name);
      log_entry[255]='\0';
      charset_j2p((unsigned char *)log_entry, 255, char_set);
      dlp_AddSyncLogEntry(sd, log_entry);
      return -1;
   }

   size = dlp_ReadAppBlock(sd, db, 0, buf, 65535);
   jp_logf(JP_LOG_DEBUG, "readappblock r=%d\n", size);
   if (size<=0) {
      jp_logf(JP_LOG_WARN, _("Error reading appinfo block for %s\n"), DB_name);
      dlp_CloseDB(sd, db);
      return -1;
   }

   r = unpack_cai_from_ai(&remote_cai, buf, size);
   if (r < 0) {
      jp_logf(JP_LOG_WARN, _("%s:%d Error unpacking app info %s\n"), __FILE__, __LINE__, full_name);
      return -1;
   }

#if SYNC_CAT_DEBUG
   printf("DB_name [%s]\n", DB_name);
   for (i = 0; i < 16; i++) {
      if (local_cai.name[i][0] != '\0') {
	 printf("local: cat %d [%s] ID %d renamed %d\n", i,
		local_cai.name[i],
		local_cai.ID[i], local_cai.renamed[i]);
      }
   }
   for (i = 0; i < 16; i++) {
      if (remote_cai.name[i][0] != '\0') {
	 printf("remote: cat %d [%s] ID %d renamed %d\n", i,
		remote_cai.name[i],
		remote_cai.ID[i], remote_cai.renamed[i]);
      }
   }
#endif

   /* Do a memcmp first to see if common case, nothing has changed */
   if (!memcmp(&(local_cai), &(remote_cai),
	       sizeof(struct CategoryAppInfo))) {
      jp_logf(JP_LOG_DEBUG, "Category app info match, nothing to do %s\n", DB_name);
      dlp_CloseDB(sd, db);
      return 0;
   }
   
   /* Go through the categories and try to sync them */
   for (Li = loop = 0; ((Li < 16) && (loop<256)); Li++, loop++) {
      found_name=found_ID=FALSE;
      found_name_at=found_ID_at=0;
      /* Did a cat get deleted locally? */
      if ((local_cai.name[Li][0]==0) && (local_cai.ID[Li]!=0)) {
	 for (Ri = 0; Ri < 16; Ri++) {
	    if ((remote_cai.ID[Ri]==local_cai.ID[Li]) && 
		(remote_cai.name[Ri][0])) {
	       remote_cai.renamed[Ri]=0;
	       remote_cai.name[Ri][0]='\0';
	       /* This category was deleted.
		Move the records to unfiled */
	       jp_logf(JP_LOG_DEBUG, "Moving category %d to unfiled...", Ri);
	       r = dlp_MoveCategory(sd, db, Ri, 0);
	       jp_logf(JP_LOG_DEBUG, "dlp_MoveCategory returned %d\n", r);
	    }
	 }
	 continue;
      }
      if (local_cai.name[Li][0]==0) {
	 continue;
      }
      /* Search for the local category name on the remote */
      for (Ri = 0; Ri < 16; Ri++) {
	 if (! strncmp(local_cai.name[Li], remote_cai.name[Ri], 16)) {
	    found_name=TRUE;
	    found_name_at=Ri;
	 }
	 if (local_cai.ID[Li] == remote_cai.ID[Ri]) {
	    found_ID=TRUE;
	    found_ID_at=Ri;
	 }
      }
      if (found_name) {
	 if (Li==found_name_at) {
	    /* 1: OK */
#if SYNC_CAT_DEBUG
	    printf("cat index %d ok\n", Li);
#endif
	 } else {
	    /* 2: change all local recs to use remote recs ID */
	    /* This is where there is a bit of trouble, since we have a pdb
	     * file on the local side we don't store the ID and there is no way
	     * to do this.
	     * So, we will swap indexes on the local records.
	     */
#if SYNC_CAT_DEBUG
	    printf("cat index %d case 2\n", Li);
#endif
	    r = pdb_file_swap_indexes(DB_name, Li, found_name_at);
	    edit_cats_swap_cats_pc3(DB_name, Li, Ri);
	    strncpy(tmp_name, local_cai.name[found_ID_at], 16);
	    tmp_name[15]='\0';
	    strncpy(local_cai.name[found_ID_at],
		    local_cai.name[Li], 16);
	    strncpy(local_cai.name[Li], tmp_name, 16);
	    Li--;
	    continue;
	 }
      }
      if ((!found_name) && (local_cai.renamed[Li])) {
	 if (found_ID) {
	    /* 3: Change remote category name to match local at index Li */
#if SYNC_CAT_DEBUG
	    printf("cat index %d case 3\n", Li);
#endif
	    strncpy(remote_cai.name[found_ID_at],
		    local_cai.name[Li], 16);
	    remote_cai.name[found_ID_at][15]='\0';
	 }
      }
      if ((!found_name) && (!found_ID)) {
	 if (remote_cai.name[Li][0]=='\0') {
	    /* 4: Add local category to remote */
#if SYNC_CAT_DEBUG
	    printf("cat index %d case 4\n", Li);
#endif
	    strncpy(remote_cai.name[Li],
		    local_cai.name[Li], 16);
	    remote_cai.name[Li][15]='\0';
	    remote_cai.renamed[Li]=0;
	    remote_cai.ID[Li]=remote_cai.ID[Li];
	    continue;
	 } else {
	    /* 5: Add local category to remote in the next available slot.
	     local records are changed to use this index. */
#if SYNC_CAT_DEBUG
 	    printf("cat index %d case 5\n", Li);
#endif
	    found_a_hole=FALSE;
	    for (i=1; i<16; i++) {
	       if (remote_cai.name[i][0]=='\0') {
		  strncpy(remote_cai.name[i],
			  local_cai.name[Li], 16);
		  remote_cai.name[i][15]='\0';
		  remote_cai.renamed[i]=0;
		  remote_cai.ID[i]=remote_cai.ID[Li];
		  r = pdb_file_change_indexes(DB_name, Li, i);
		  edit_cats_change_cats_pc3(DB_name, Li, i);
		  found_a_hole=TRUE;
		  break;
	       }
	    }
	    if (!found_a_hole) {
	       jp_logf(JP_LOG_WARN, _("Could not add category %s to remote.\n"), local_cai.name[Li]);
	       jp_logf(JP_LOG_WARN, _("Too many categories on remote.\n"));
	       jp_logf(JP_LOG_WARN, _("All records on desktop in %s will be moved to %s.\n"), local_cai.name[Li], local_cai.name[0]);
	       /* Fix - need a func for this logging */
	       g_snprintf(log_entry, 255, _("Could not add category %s to remote.\n"), local_cai.name[Li]);
	       log_entry[255]='\0';
	       charset_j2p((unsigned char *)log_entry, 255, char_set);
	       dlp_AddSyncLogEntry(sd, log_entry);
	       g_snprintf(log_entry, 255, _("Too many categories on remote.\n"));
	       log_entry[255]='\0';
	       charset_j2p((unsigned char *)log_entry, 255, char_set);
	       dlp_AddSyncLogEntry(sd, log_entry);
	       g_snprintf(log_entry, 255, _("All records on desktop in %s will be moved to %s.\n"), local_cai.name[Li], local_cai.name[0]);
	       log_entry[255]='\0';
	       charset_j2p((unsigned char *)log_entry, 255, char_set);
	       dlp_AddSyncLogEntry(sd, log_entry);
	       jp_logf(JP_LOG_DEBUG, "Moving local recs category %d to unfiled...", Li);
	       edit_cats_change_cats_pc3(DB_name, Li, 0);
	       edit_cats_change_cats_pdb(DB_name, Li, 0);
	    }
	 }
      }
#if SYNC_CAT_DEBUG
      printf("cat index %d case (none)\n", Li);
#endif
   }

   for (i = 0; i < 16; i++) {
      remote_cai.renamed[i]=0;
   }

   pack_cai_into_ai(&remote_cai, buf, size_Papp_info);

   dlp_WriteAppBlock(sd, db, buf, size_Papp_info);

   pdb_file_write_app_block(DB_name, buf, size_Papp_info);

   dlp_CloseDB(sd, db);

   return 0;
}

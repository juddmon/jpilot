/* $Id: sync.c,v 1.78 2007/11/07 00:09:15 rikster5 Exp $ */

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

#include "config.h"
#include "i18n.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/socket.h>
#ifdef USE_FLOCK
# include <sys/file.h>
#else
# include <fcntl.h>
#endif
#include <errno.h>
#include <signal.h>
#include <utime.h>
#include <stdio.h>
#include <ctype.h>
#include "utils.h"
#include "sync.h"
#include "log.h"
#include "prefs.h"
#include "datebook.h"
#include "plugins.h"
#include "libplugin.h"
#include "password.h"

#include <pi-socket.h>
#include <pi-dlp.h>
#include <pi-header.h>
#include <pi-file.h>
#include <pi-version.h>
#ifdef PILOT_LINK_0_12
# include <pi-error.h>
#endif

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

#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

/* #define PIPE_DEBUG */
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
/* New applications */
int unpack_contact_cai_from_ai(struct CategoryAppInfo *cai, unsigned char *ai_raw, int len);
int pack_contact_cai_into_ai(struct CategoryAppInfo *cai, unsigned char *ai_raw, int len);

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
   lock.l_len = 0; /*Lock to the end of file */
   r = fcntl(*fd, F_SETLK, &lock);
#else
   r = flock(*fd, LOCK_EX | LOCK_NB);
#endif
   if (r == -1){
      jp_logf(JP_LOG_WARN, _("lock failed\n"));
      read(*fd, str, 10);
      pid = atoi(str);
      jp_logf(JP_LOG_FATAL, _("sync file is locked by pid %d\n"), pid);
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

int sync_unlock(int fd)
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
 * Unfortunately the palm and Jpilot store some fields differently.
 * This may be caused by endianess issues between the Palm and the host PC.
 * The time structs are also likely to be different since Palm measures from 
 * 1904 and the host PC doesn't.
 * The result is that a direct memcmp usually won't work.  
 * Ideally, one would have comparison routines on a per DB_name basis.
 * Until those are written we fall back on comparing the lengths of records
 * which is imperfect and can obviously hide a lot of simple changes */
int match_records(void *rrec, int rrec_len, 
                  void *lrec, int lrec_len, 
                  char *DB_name)
{

   if (!rrec || !lrec)
      return 0;
   
   if (lrec_len != rrec_len)
      return 0;

   /* memcmp works for a few specific databases */
   if (!strcmp(DB_name,"MemoDB"))
      return !(memcmp(lrec, rrec, lrec_len));

   if (!strcmp(DB_name,"Memo32DB"))
      return !(memcmp(lrec, rrec, lrec_len));

   if (!strcmp(DB_name,"MemosDB-PMem"))
      return !(memcmp(lrec, rrec, lrec_len));

   if (!strcmp(DB_name,"ToDoDB"))
      return !(memcmp(lrec, rrec, lrec_len));

   /* Lengths match and no other checks possible */
   return 1;

}

int sync_once(struct my_sync_info *sync_info)
{
#ifdef USE_LOCKING
   int fd;
#endif
   int r;
   struct my_sync_info *sync_info_copy;
   pid_t pid;

   if (glob_child_pid) {
      jp_logf(JP_LOG_WARN, PN": sync PID = %d\n", glob_child_pid);
      jp_logf(JP_LOG_WARN, _("%s: press the hotsync button on the cradle "
	   "or \"kill %d\"\n"), PN, glob_child_pid);
      return EXIT_SUCCESS;
   }

   /* Make a copy of the sync info for the forked process */
   sync_info_copy = malloc(sizeof(struct my_sync_info));
   if (!sync_info_copy) {
      jp_logf(JP_LOG_WARN, PN":sync_once(): %s\n", _("Out of memory"));
      return 0;
   }
   memcpy(sync_info_copy, sync_info, sizeof(struct my_sync_info));

   if (!(sync_info->flags & SYNC_NO_FORK)) {
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

int jp_install_user(const char *device, int sd, struct my_sync_info *sync_info)
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


#ifdef PILOT_LINK_0_12
int jp_pilot_connect(int *Psd, const char *device)
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
      jp_logf(JP_LOG_WARN, _("Check your serial port and settings\n"));
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

#else /* #ifdef PILOT_LINK_0_12 */
int jp_pilot_connect(int *Psd, const char *device)
{
   struct pi_sockaddr addr;
   int sd;
   int ret;
   int i;
   int dev_usb;
   char link[FILENAME_MAX], dev_str[FILENAME_MAX], dev_dir[FILENAME_MAX], *Pc;
   struct  SysInfo sys_info;
   /* pilot-link > 0.9.5 needed for many USB devices */
#ifdef USB_PILOT_LINK
   /* Newer pilot-link */
   sd = pi_socket(PI_AF_PILOT, PI_SOCK_STREAM, PI_PF_DLP);
#else
   /* 0.9.5 or older */
   sd = pi_socket(PI_AF_SLP, PI_SOCK_STREAM, PI_PF_PADP);
   addr.pi_family = PI_AF_SLP;
#endif

   *Psd=0;
   if (sd < 0) {
      int err = errno;
      perror("pi_socket");
      jp_logf(JP_LOG_WARN, "pi_socket %s\n", strerror(err));
      return EXIT_FAILURE;
   }

   strncpy(addr.pi_device, device, sizeof(addr.pi_device));

   /* This is for USB, whose device doesn't exist until the cradle is pressed
    * We will give them 5 seconds */
   link[0]='\0';
   g_snprintf(dev_str, sizeof(dev_str), "%s", device);
   g_snprintf(dev_dir, sizeof(dev_dir), "%s", device);
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
      ret = readlink(dev_str, link, sizeof(link));
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
	 g_snprintf(dev_str, sizeof(dev_str), "%s/%s", dev_dir, link);
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
      jp_logf(JP_LOG_WARN, "pi_bind error: %s %s\n", device, strerror(errno));
      jp_logf(JP_LOG_WARN, _("Check your serial port and settings\n"));
      pi_close(sd);
      return SYNC_ERROR_BIND;
   }

   ret = pi_listen(sd, 1);
   if (ret == -1) {
      perror("pi_listen");
      jp_logf(JP_LOG_WARN, "pi_listen %s\n", strerror(errno));
      pi_close(sd);
      return SYNC_ERROR_LISTEN;
   }

   sd = pi_accept(sd, 0, 0);
   if(sd == -1) {
      perror("pi_accept");
      jp_logf(JP_LOG_WARN, "pi_accept %s\n", strerror(errno));
      pi_close(sd);
      return SYNC_ERROR_PI_ACCEPT;
   }

   /* We must do this to take care of the password being required to sync
    * on Palm OS 4.x */
   /* Later versions of pilot-link (~=0.10+) did this for us */
   if (dlp_ReadSysInfo(sd, &sys_info) < 0) {
      jp_logf(JP_LOG_WARN, "dlp_ReadSysInfo error\n");
      pi_close(sd);
      return SYNC_ERROR_READSYSINFO;
   }
   *Psd=sd;
   return EXIT_SUCCESS;
}
#endif

int jp_sync(struct my_sync_info *sync_info)
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
# ifdef PILOT_LINK_0_12
   pi_buffer_t *buffer;
# else
   unsigned char buffer[65536];
# endif
#endif
   int i;
   char dbname[][32]={
      "DatebookDB",
	"AddressDB",
	"ToDoDB",
	"MemoDB",
	"Memo32DB",
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
	PREF_SYNC_MEMO32,
#ifdef ENABLE_MANANA
	PREF_SYNC_MANANA,
#endif
	0
   };

   /* 
    * This is pretty confusing, but a little necessary.
    * This is an array of pointers to functions and will need to be changed
    * to point to calendar, contacts, tasks, memos, etc. as preferenced
    * dictate
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
   if (datebook_version==1) {
      /* Not coded yet */
      ;
   }
   if (address_version==1) {
      unpack_cai_from_buf[1]=unpack_contact_cai_from_ai;
      pack_cai_into_buf[1]=pack_contact_cai_into_ai;
   }
   if (todo_version==1) {
      /* Not coded yet */
      ;
   }
   if (memo_version==1) {
      /* Not coded yet */
      ;
   }

   /* Load the plugins for a forked process */
#ifdef ENABLE_PLUGINS
   if (!(sync_info->flags & SYNC_NO_FORK) &&
       !(sync_info->flags & SYNC_NO_PLUGINS)) {
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

   ret = jp_pilot_connect(&sd, device);
   if (ret) {
      return ret;
   }

   if (sync_info->flags & SYNC_INSTALL_USER) {
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
   jp_logf(JP_LOG_GUI, "****************************************\n");

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
      jp_logf(JP_LOG_WARN, "dlp_OpenConduit() failed\n");
      jp_logf(JP_LOG_WARN, _("Sync canceled\n"));
#ifdef ENABLE_PLUGINS
      if (!(sync_info->flags & SYNC_NO_FORK)) 
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
# ifdef PILOT_LINK_0_12
   buffer = pi_buffer_new(sizeof(struct DBInfo));
   while(dlp_ReadDBList(sd, 0, dlpDBListRAM, start, buffer)>0) {
      memcpy(&info, buffer->data, sizeof(struct DBInfo));
      start=info.index+1;
      if (info.flags & dlpDBFlagAppInfoDirty) {
	 printf("appinfo dirty for %s\n", info.name);
      }
   }
   pi_buffer_free(buffer);
# else
   while(dlp_ReadDBList(sd, 0, dlpOpenRead, start, &info)>0) {
      start=info.index+1;
      if (info.flags & dlpDBFlagAppInfoDirty) {
	 printf("appinfo dirty for %s\n", info.name);
      }
   }
# endif
#endif

   /* Do a fast, or a slow sync on each application in the arrays */
   if ( (!(sync_info->flags & SYNC_OVERRIDE_USER)) &&
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
      if (!(sync_info->flags & SYNC_NO_FORK)) 
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

   if (!(sync_info->flags & SYNC_NO_FORK)) {
      jp_logf(JP_LOG_DEBUG, "freeing plugin list\n");
      free_plugin_list(&plugin_list);
   }
#endif

   jp_logf(JP_LOG_GUI, _("Finished.\n"));
   write_to_parent(PIPE_FINISHED, "\n");

   return EXIT_SUCCESS;
}

int slow_sync_application(char *DB_name, int sd)
{
   int db;
   int ret;
   int num;
   FILE *pc_in;
   char pc_filename[FILENAME_MAX];
   PC3RecordHeader header;
   /* local (.pc3) record */
   void *lrec;
   int lrec_len;
   /* remote (Palm) record */
#ifdef PILOT_LINK_0_12
   pi_buffer_t *rrec;
#else
   unsigned char rrec[65536];
#endif
   int  rindex, rattr, rcategory;
   int  rrec_len;
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
   jp_logf(JP_LOG_GUI ,_("number of records = %d\n"), num);
#endif

   /* Loop over records in .pc3 file */
   while (!feof(pc_in)) {
      num = read_header(pc_in, &header);
      if (num!=1) {
	 if (ferror(pc_in)) {
	    break;
	 }
	 if (feof(pc_in)) {
	    break;
	 }
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
#ifdef PILOT_LINK_0_12
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
#else
	 ret = dlp_ReadRecordById(sd, db, header.unique_id, rrec,
				  &rindex, &rrec_len, &rattr, &rcategory);
#endif
#ifdef JPILOT_DEBUG
	 if (ret>=0 ) {
	    printf("read record by id %s returned %d\n", DB_name, ret);
	    printf("id %ld, index %d, size %d, attr 0x%x, category %d\n",
		   header.unique_id, rindex, rrec_len, rattr, rcategory);
	 } else {
	    printf("Case 3&4: read record by id failed\n");
         }
#endif

	 /* Check whether records are the same */
         /* same = rrec && (lrec_len==rrec_len) */
         same = match_records(rrec, rrec_len, lrec, lrec_len, DB_name);
#ifdef JPILOT_DEBUG
         printf("Same is %d\n", same);
#endif

         if (ret < 0) {
            /* lrec can't be found which means it has already 
             * been deleted from the Palm side.
             * Mark the local record as deleted */
            jp_logf(JP_LOG_DEBUG, "Case 3&4: no remote record found, must have been deleted on the Palm\n");
	    if (fseek(pc_in, -(header.header_len+lrec_len), SEEK_CUR)) {
               jp_logf(JP_LOG_WARN, _("fseek failed - fatal error\n"));
               fclose(pc_in);
               dlp_CloseDB(sd, db);
               free(lrec);
#ifdef PILOT_LINK_0_12
               pi_buffer_free(rrec);
#endif
               return EXIT_FAILURE;
            }
            header.rt=DELETED_DELETED_PALM_REC;
            write_header(pc_in, &header);
         } 
         /* Next check for match between lrec and rrec */
	 else if (same && (header.unique_id != 0)) {
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
#ifdef PILOT_LINK_0_12
               pi_buffer_free(rrec);
#endif
               return EXIT_FAILURE;
            }
            header.rt=DELETED_DELETED_PALM_REC;
            write_header(pc_in, &header);

         } else {
            /* Record has been changed on the palm as well as the PC.  
             *
             * It must be copied to the palm under a new unique ID in
             * order to prevent overwriting by the modified PC record.  */
            if ((header.rt==MODIFIED_PALM_REC)) {
               jp_logf(JP_LOG_DEBUG, "Case 4: duplicating record\n");
               jp_logf(JP_LOG_GUI, "Sync Conflict: a %s record must be manually merged\n", DB_name);

               /* Write record to Palm and get new unique ID */
               jp_logf(JP_LOG_DEBUG, "Writing PC record to palm\n");
               ret = dlp_WriteRecord(sd, db, rattr & dlpRecAttrSecret,
                                     0, rattr & 0x0F,
#ifdef PILOT_LINK_0_12
                                     rrec->data,
#else
                                     rrec,
#endif
                                     rrec_len, &header.unique_id);

               if (ret < 0) {
                  jp_logf(JP_LOG_WARN, "dlp_WriteRecord failed\n");
                  charset_j2p(error_log_message_w,255,char_set);
                  dlp_AddSyncLogEntry(sd, error_log_message_w);
                  dlp_AddSyncLogEntry(sd, "\n");
               } else {
                  charset_j2p(conflict_log_message,255,char_set);
                  dlp_AddSyncLogEntry(sd, conflict_log_message);
                  dlp_AddSyncLogEntry(sd, "\n");
                  /* Now mark the record as deleted in the pc file */
                  if (fseek(pc_in, -(header.header_len+lrec_len), SEEK_CUR)) {
                     jp_logf(JP_LOG_WARN, _("fseek failed - fatal error\n"));
                     fclose(pc_in);
                     dlp_CloseDB(sd, db);
                     free(lrec);
#ifdef PILOT_LINK_0_12
                     pi_buffer_free(rrec);
#endif
                     return EXIT_FAILURE;
                  }
                  header.rt=DELETED_PC_REC;
                  write_header(pc_in, &header);
               }
            } else {
               /* Record was a deletion on the PC but modified on the Palm
                * The PC deletion is ignored by skipping the .pc3 file entry */
               jp_logf(JP_LOG_DEBUG, "Case 3: skipping PC deleted record\n");
               if (fseek(pc_in, -(header.header_len+lrec_len), SEEK_CUR)) {
                  jp_logf(JP_LOG_WARN, _("fseek failed - fatal error\n"));
                  fclose(pc_in);
                  dlp_CloseDB(sd, db);
                  free(lrec);
#ifdef PILOT_LINK_0_12
                  pi_buffer_free(rrec);
#endif
                  return EXIT_FAILURE;
               }
               header.rt=DELETED_PC_REC;
               write_header(pc_in, &header);
            }

         } /* end if checking whether old & new records are the same */

         /* free buffers */
	 if (lrec) {
	    free(lrec);
	    lrec = NULL;
	 }

#ifdef PILOT_LINK_0_12
         pi_buffer_free(rrec);
#endif

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

/*
 * Fetch the databases from the palm if modified
 */
void fetch_extra_DBs2(int sd, struct DBInfo info, char *palm_dbname[])
{
#define MAX_DBNAME 50
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

   /* If modification times are the same then we don t need to fetch it */
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
#ifdef PILOT_LINK_0_12
   if (pi_file_retrieve(pi_fp, sd, 0, NULL)<0) {
# ifdef KEEP_EDITOR_HAPPY_WITH_INDENTS
   }
# endif
#else
   if (pi_file_retrieve(pi_fp, sd, 0)<0) {
#endif
      jp_logf(JP_LOG_WARN, _("Failed, unable to back up database %s\n"), info.name);
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

/*
 * Fetch the databases from the palm if modified
 */
int fetch_extra_DBs(int sd, char *palm_dbname[])
{
#define MAX_DBNAME 50
   int cardno, start;
   struct DBInfo info;
#ifdef PILOT_LINK_0_12
   int dbIndex;
   pi_buffer_t *buffer;
#endif

   jp_logf(JP_LOG_DEBUG, "fetch_extra_DBs()\n");

   start=cardno=0;

#ifdef PILOT_LINK_0_12
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
#else

   /* Pilot-link < 0.12 */
   while(dlp_ReadDBList(sd, cardno, dlpOpenRead, start, &info)>0) {
      start=info.index+1;
      fetch_extra_DBs2(sd, info, palm_dbname);
   }
#endif

   return EXIT_SUCCESS;
}

void free_file_name_list(GList **Plist)
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

void move_removed_apps(GList *file_list)
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
#ifdef PILOT_LINK_0_12
   int palmos_error;
   int dbIndex;
   pi_buffer_t *buffer;
#endif
#ifdef ENABLE_PLUGINS
   GList *temp_list;
   GList *plugin_list;
   struct plugin_s *plugin;
#endif
   char *file_name;
   char palm_dbname[][32]={
      "DatebookDB",
      "AddressDB",
      "ToDoDB",
      "MemoDB",
      "Memo32DB",
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
       char *creator;
       char *dbname;
   } skip_db_t ;

   skip_db_t skip_db[] = {
	{ 0, dlpDBFlagResource, "AvGo", NULL },
	{ 0, dlpDBFlagResource, "psys", "Unsaved Preferences" },
	{ 0, 0, "a68k", NULL},
	{ 0, 0, "appl", NULL},
	{ 0, 0, "boot", NULL},
	{ 0, 0, "Fntl", NULL},
	{ 0, 0, "modm", NULL},
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

#ifdef PILOT_LINK_0_12
   buffer = pi_buffer_new(32 * sizeof(struct DBInfo));

   while( (r=dlp_ReadDBList(sd, cardno, dlpDBListRAM | dlpDBListMultiple, start, buffer)) > 0) {
      for (dbIndex=0; dbIndex < (buffer->used / sizeof(struct DBInfo)); dbIndex++) {
	 memcpy(&info, buffer->data + (dbIndex * sizeof(struct DBInfo)), sizeof(struct DBInfo));
# ifdef KEEP_EDITOR_HAPPY_WITH_INDENTS
      }}
# endif
#else
   while( (r=dlp_ReadDBList(sd, cardno, dlpOpenRead, start, &info)) > 0) {
#endif

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
      /*jp_logf(JP_LOG_DEBUG, "type = %x\n",info.type);*/
      jp_logf(JP_LOG_DEBUG, "Creator ID = [%s]\n", creator);
#endif
      if (full_backup) {
	 /* Look at the skip list */
	 skip_file=0;
	 for (i=0; skip_db[i].creator || skip_db[i].dbname; i++) {
	    if (skip_db[i].creator &&
		!strcmp(creator, skip_db[i].creator)) {
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
                case 4:
                  if (!get_pref_int_default(PREF_SYNC_MEMO32, 1)) {
                     skip_file = 1; 
                  }
                  break;
#ifdef ENABLE_MANANA                  
                case 5:
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
      /* If modification times are the same then we don t need to fetch it */
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
#ifdef PILOT_LINK_0_12
      if (pi_file_retrieve(pi_fp, sd, 0, NULL)<0) {
#else
      if (pi_file_retrieve(pi_fp, sd, 0)<0) {
#endif
	 jp_logf(JP_LOG_WARN, _("Failed, unable to back up database %s\n"), info.name);
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
      if (main_app && !fast_sync && full_backup) {
	 jp_copy_file(full_name, full_backup_name);
      }
   }
#ifdef PILOT_LINK_0_12
   }
   pi_buffer_free(buffer);
#endif
#ifdef PILOT_LINK_0_12
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
#else
   /* I'm not sure why pilot-link-0.11 is returning dlpErrNoneOpen */
   if ((r==dlpErrNotFound) || (r==dlpErrNoneOpen)) {
      jp_logf(JP_LOG_DEBUG, "Good return code (dlpErrNotFound)\n");
      if (full_backup) {
	 jp_logf(JP_LOG_DEBUG, "Removing apps not found on the palm\n");
	 move_removed_apps(file_list);
      }
   } else {
      jp_logf(JP_LOG_WARN, "ReadDBList returned = %d\n", r);
   }
#endif
	
   free_file_name_list(&file_list);

   return EXIT_SUCCESS;
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
   if (f==NULL) {
      int fd;

      if ((fd = open(filename, O_RDONLY)) < 0) {
	jp_logf(JP_LOG_WARN, _("\nUnable to open file: '%s': %s!\n"), filename,
		strerror(errno));
      } else {
         close(fd);
         jp_logf(JP_LOG_WARN, _("\nUnable to sync file: '%s': file corrupted?\n"),
		 filename);
      }
      return EXIT_FAILURE;
   }
   memset(&info, 0, sizeof(info));
   pi_file_get_info(f, &info);
   creator[0] = (info.creator & 0xFF000000) >> 24;
   creator[1] = (info.creator & 0x00FF0000) >> 16,
   creator[2] = (info.creator & 0x0000FF00) >> 8,
   creator[3] = (info.creator & 0x000000FF);
   creator[4] = '\0';
   jp_logf(JP_LOG_GUI, _("(Creator ID is '%s')..."), creator);

#ifdef PILOT_LINK_0_12
   r = pi_file_install(f, sd, 0, NULL);
#else
   r = pi_file_install(f, sd, 0);
#endif
   if (r<0) {
      try_again = 0;
      /* TODO make this generic? Not sure it would work 100% of the time */
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
#ifdef PILOT_LINK_0_12
	 r = pi_file_install(f, sd, 0, NULL);
#else
	 r = pi_file_install(f, sd, 0);
#endif
      }
   }

   if (r<0) {
      g_snprintf(log_entry, sizeof(log_entry), _("Install %s failed"), Pc);
      charset_j2p(log_entry, sizeof(log_entry), char_set);
      dlp_AddSyncLogEntry(sd, log_entry);
      dlp_AddSyncLogEntry(sd, "\n");;
      jp_logf(JP_LOG_GUI, _("Failed.\n"));
      jp_logf(JP_LOG_WARN, "%s\n", log_entry);
      pi_file_close(f);
      return EXIT_FAILURE;
   }
   else {
      /* the space after the %s is a hack, the last char gets cut off */
      g_snprintf(log_entry, sizeof(log_entry), _("Installed %s "), Pc);
      charset_j2p(log_entry, sizeof(log_entry), char_set);
      dlp_AddSyncLogEntry(sd, log_entry);
      dlp_AddSyncLogEntry(sd, "\n");;
      jp_logf(JP_LOG_GUI, _("OK\n"));
   }
   pi_file_close(f);

   return EXIT_SUCCESS;
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
      jp_logf(JP_LOG_WARN, _("Unable to open file: %s%s\n"), EPN, "_to_install");
      return EXIT_FAILURE;
   }

   out = jp_open_home_file(EPN"_to_install.tmp", "w");
   if (!out) {
      jp_logf(JP_LOG_WARN, _("Unable to open file: %s%s\n"), EPN, "_to_install.tmp");
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
      r = sync_install(line, sd);
      if (r==0) {
	 continue;
      }
      fprintf(out, "%s\n", line);
   }
   fclose(in);
   fclose(out);

   rename_file(EPN"_to_install.tmp", EPN"_to_install");

   return EXIT_SUCCESS;
}

int is_backup_dir(char *name)
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

int fast_sync_local_recs(char *DB_name, int sd, int db)
{
   int ret;
   int num;
   FILE *pc_in;
   char pc_filename[FILENAME_MAX];
   PC3RecordHeader header;
   unsigned int orig_unique_id;
   void *lrec;  /* local (.pc3) record */
   int  lrec_len;
   void *rrec;  /* remote (Palm) record */
   int  rindex, rattr, rcategory;
#ifdef PILOT_LINK_0_12
   size_t rrec_len;
#else
   int rrec_len;
#endif
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
	 if (ferror(pc_in)) {
	    break;
	 }
	 if (feof(pc_in)) {
	    break;
	 }
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
	 /* Check whether records are the same */
         /* same = rrec && (lrec_len==rrec_len) */
         same = match_records(rrec, rrec_len, lrec, lrec_len, DB_name);
#ifdef JPILOT_DEBUG
         printf("Same is %d\n", same);
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
         } 
         /* Next check for match between lrec and rrec */
	 else if (same && (header.unique_id != 0)) {
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
            /* Record has been changed on the palm as well as the PC.  
             *
             * The changed record has already been transferred to the local pdb 
             * file from the palm. It must be copied to the palm and to the 
             * local pdb file under a new unique ID in order to prevent 
             * overwriting by the modified PC record.  */
            if ((header.rt==MODIFIED_PALM_REC)) {
               jp_logf(JP_LOG_DEBUG, "Case 4: duplicating record\n");
               jp_logf(JP_LOG_GUI, "Sync Conflict: a %s record must be manually merged\n", DB_name);

               /* Write record to Palm and get new unique ID */
               jp_logf(JP_LOG_DEBUG, "Writing PC record to palm\n");
               ret = dlp_WriteRecord(sd, db, rattr & dlpRecAttrSecret,
                                     0, rattr & 0x0F,
                                     rrec, rrec_len, &header.unique_id);

               /* Write record to local pdb file */
               jp_logf(JP_LOG_DEBUG, "Writing PC record to local\n");
               if (ret >=0) {
                  pdb_file_modify_record(DB_name, rrec, rrec_len,
                                         rattr & dlpRecAttrSecret,
                                         rattr & 0x0F, header.unique_id);
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
                  /* Now mark the record as deleted in the pc file */
                  if (fseek(pc_in, -(header.header_len+lrec_len), SEEK_CUR)) {
                     jp_logf(JP_LOG_WARN, _("fseek failed - fatal error\n"));
                     fclose(pc_in);
                     free(lrec);
                     free(rrec);
                     return EXIT_FAILURE;
                  }
                  header.rt=DELETED_PC_REC;
                  write_header(pc_in, &header);
               }
            } else {
               /* Record was a deletion on the PC but modified on the Palm
                * The PC deletion is ignored by skipping the .pc3 file entry */
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
            }

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
 * This takes 2 category indexes and swaps them in every record.
 * returns the number of record's categories swapped.
 */
int pdb_file_swap_indexes(char *DB_name, int index1, int index2)
{
   char local_pdb_file[FILENAME_MAX];
   char full_local_pdb_file[FILENAME_MAX];
   char full_local_pdb_file2[FILENAME_MAX];
   struct pi_file *pf1, *pf2;
   struct DBInfo infop;
   void *app_info;
   void *sort_info;
   void *record;
   int r;
   int idx;
#ifdef PILOT_LINK_0_12
   size_t size;
#else
   int size;
#endif
   int attr;
   int cat, new_cat;
   int count;
   pi_uid_t uid;
   struct stat statb;
   struct utimbuf times;

   jp_logf(JP_LOG_DEBUG, "pi_file_swap_indexes\n");

   g_snprintf(local_pdb_file, sizeof(local_pdb_file), "%s.pdb", DB_name);
   get_home_file_name(local_pdb_file, full_local_pdb_file, sizeof(full_local_pdb_file));
   strcpy(full_local_pdb_file2, full_local_pdb_file);
   strcat(full_local_pdb_file2, "2");

   /* After we are finished, set the create and modify times of new file
      to the same as the old */
   stat(full_local_pdb_file, &statb);
   times.actime = statb.st_atime;
   times.modtime = statb.st_mtime;

   pf1 = pi_file_open(full_local_pdb_file);
   if (!pf1) {
      jp_logf(JP_LOG_WARN, _("Unable to open file: %s\n"), full_local_pdb_file);
      return EXIT_FAILURE;
   }
   pi_file_get_info(pf1, &infop);
   pf2 = pi_file_create(full_local_pdb_file2, &infop);
   if (!pf2) {
      jp_logf(JP_LOG_WARN, _("Unable to open file: %s\n"), full_local_pdb_file2);
      return EXIT_FAILURE;
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
      jp_logf(JP_LOG_WARN, "pdb_file_swap_indexes(): %s\n,", _("rename failed"));
   }

   utime(full_local_pdb_file, &times);

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
 *				   local was changed and changed back)
 *     otherwise,
 *       add NLR to remote
 *       if RR==LR remove RR
 *   Case 5:
 *   if new LR
 *     add LR to remote
 */
int fast_sync_application(char *DB_name, int sd)
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
#ifdef PILOT_LINK_0_12
   pi_buffer_t *rrec;
#else
   unsigned char rrec[65536];
#endif
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
   /*ret = dlp_ReadAppBlock(sd, db, 0, rrec, 65535);
   jp_logf(JP_LOG_DEBUG, "readappblock ret=%d\n", ret);
   if (ret>0) {
      pdb_file_write_app_block(DB_name, rrec, ret);
   }*/

   /* Loop over all Palm records with dirty bit set */
   while(1) {
#ifdef PILOT_LINK_0_12
      rrec = pi_buffer_new(0);
      ret = dlp_ReadNextModifiedRec(sd, db, rrec,
				    &rid, &rindex, &rattr, &rcategory);
      rrec_len = rrec->used;
#else
      ret = dlp_ReadNextModifiedRec(sd, db, rrec,
				    &rid, &rindex, &rrec_len, &rattr, &rcategory);
#endif
      if (ret>=0 ) {
	 jp_logf(JP_LOG_DEBUG, "read next record for %s returned %d\n", DB_name, ret);
	 jp_logf(JP_LOG_DEBUG, "id %ld, index %d, size %d, attr 0x%x, category %d\n",rid, rindex, rrec_len, rattr, rcategory);
      } else {
#ifdef PILOT_LINK_0_12
	 pi_buffer_free(rrec);
#endif
	 break;
      }

      /* Case 1: */
      if ((rattr & dlpRecAttrDeleted) || (rattr & dlpRecAttrArchived)) {
	 jp_logf(JP_LOG_DEBUG, "Case 1: found a deleted record on palm\n");
	 pdb_file_delete_record_by_id(DB_name, rid);
#ifdef PILOT_LINK_0_12
	 pi_buffer_free(rrec);
#endif
	 continue;
      }

      /* Case 2: */
      if (rattr & dlpRecAttrDirty) {
	 jp_logf(JP_LOG_DEBUG, "Case 2: found a dirty record on palm\n");
#ifdef PILOT_LINK_0_12
	 pdb_file_modify_record(DB_name, rrec->data, rrec->used, rattr, rcategory, rid);
#else
	 pdb_file_modify_record(DB_name, rrec, rrec_len, rattr, rcategory, rid);
#endif
      }
#ifdef PILOT_LINK_0_12
      pi_buffer_free(rrec);
#endif
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
      jp_logf(JP_LOG_DEBUG ,_("palm: number of records = %d\n"), num_palm_recs);
      jp_logf(JP_LOG_DEBUG ,_("disk: number of records = %d\n"), num_local_recs);
      fetch_extra_DBs(sd, extra_dbname);
   }

   return EXIT_SUCCESS;
}


int unpack_address_cai_from_ai(struct CategoryAppInfo *cai, unsigned char *ai_raw, int len)
{
   struct AddressAppInfo ai;
   int r;

   jp_logf(JP_LOG_DEBUG, "unpack_address_cai_from_ai\n");

   r = unpack_AddressAppInfo(&ai, ai_raw, len);
   if ((r <= 0) || (len <= 0)) {
      jp_logf(JP_LOG_DEBUG, "unpack_AddressAppInfo failed %s %d\n", __FILE__, __LINE__);
      return EXIT_FAILURE;
   }
   memcpy(cai, &(ai.category), sizeof(struct CategoryAppInfo));

   return EXIT_SUCCESS;
}

int pack_address_cai_into_ai(struct CategoryAppInfo *cai, unsigned char *ai_raw, int len)
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

int unpack_contact_cai_from_ai(struct CategoryAppInfo *cai, unsigned char *ai_raw, int len)
{
   struct ContactAppInfo ai;
   int r;
   pi_buffer_t pi_buf;

   jp_logf(JP_LOG_DEBUG, "unpack_contact_cai_from_ai\n");

   pi_buf.data = ai_raw;
   pi_buf.used = len;
   pi_buf.allocated = len;
   r = unpack_ContactAppInfo(&ai, &pi_buf);
   if ((r <= 0) || (len <= 0)) {
      jp_logf(JP_LOG_DEBUG, "unpack_ContactAppInfo failed %s %d\n", __FILE__, __LINE__);
      return EXIT_FAILURE;
   }
   memcpy(cai, &(ai.category), sizeof(struct CategoryAppInfo));

   return EXIT_SUCCESS;
}

int pack_contact_cai_into_ai(struct CategoryAppInfo *cai, unsigned char *ai_raw, int len)
{
   struct ContactAppInfo ai;
   int r;
   pi_buffer_t *pi_buf;

   jp_logf(JP_LOG_DEBUG, "pack_contact_cai_into_ai\n");

   pi_buffer_new(len);
   pi_buffer_append(pi_buf, ai_raw, len);

   r = unpack_ContactAppInfo(&ai, pi_buf);
   if (r <= 0) {
      jp_logf(JP_LOG_DEBUG, "unpack_ContactAppInfo failed %s %d\n", __FILE__, __LINE__);
      pi_buffer_free(pi_buf);
      return EXIT_FAILURE;
   }
   memcpy(&(ai.category), cai, sizeof(struct CategoryAppInfo));

   //r = pack_ContactAppInfo(&ai, ai_raw, len);
   r = pack_ContactAppInfo(&ai, pi_buf);
   //undo check buffer sizes
   memcpy(ai_raw, pi_buf->data, pi_buf->used);
   pi_buffer_free(pi_buf);

   if (r <= 0) {
      jp_logf(JP_LOG_DEBUG, "pack_ContactAppInfo failed %s %d\n", __FILE__, __LINE__);
      return EXIT_FAILURE;
   }

   return EXIT_SUCCESS;
}

int unpack_todo_cai_from_ai(struct CategoryAppInfo *cai, unsigned char *ai_raw, int len)
{
   struct ToDoAppInfo ai;
   int r;

   jp_logf(JP_LOG_DEBUG, "unpack_todo_cai_from_ai\n");

   r = unpack_ToDoAppInfo(&ai, ai_raw, len);
   if ((r <= 0) || (len <= 0)) {
      jp_logf(JP_LOG_DEBUG, "unpack_ToDoAppInfo failed %s %d\n", __FILE__, __LINE__);
      return EXIT_FAILURE;
   }
   memcpy(cai, &(ai.category), sizeof(struct CategoryAppInfo));

   return EXIT_SUCCESS;
}

int pack_todo_cai_into_ai(struct CategoryAppInfo *cai, unsigned char *ai_raw, int len)
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

int unpack_memo_cai_from_ai(struct CategoryAppInfo *cai, unsigned char *ai_raw, int len)
{
   struct MemoAppInfo ai;
   int r;

   jp_logf(JP_LOG_DEBUG, "unpack_memo_cai_from_ai\n");

   r = unpack_MemoAppInfo(&ai, ai_raw, len);
   if ((r <= 0) || (len <= 0)) {
      jp_logf(JP_LOG_DEBUG, "unpack_MemoAppInfo failed %s %d\n", __FILE__, __LINE__);
      return EXIT_FAILURE;
   }
   memcpy(cai, &(ai.category), sizeof(struct CategoryAppInfo));

   return EXIT_SUCCESS;
}

int pack_memo_cai_into_ai(struct CategoryAppInfo *cai, unsigned char *ai_raw, int len)
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

int sync_categories(char *DB_name, int sd,
		    int (*unpack_cai_from_ai)(struct CategoryAppInfo *cai, unsigned char *ai_raw, int len),
		    int (*pack_cai_into_ai)(struct CategoryAppInfo *cai, unsigned char *ai_raw, int len)
)
{
   struct CategoryAppInfo local_cai, remote_cai, orig_remote_cai;
   char full_name[FILENAME_MAX];
   char pdb_name[FILENAME_MAX];
   char log_entry[256];
#ifdef PILOT_LINK_0_12
   pi_buffer_t *buffer;
   size_t size_Papp_info;
#else
   int size_Papp_info;
#endif
   unsigned char buf[65536];
   char tmp_name[18];
   int i, r, Li, Ri;
   int size;
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
#ifdef PILOT_LINK_0_12
   pi_file_get_app_info(pf, &Papp_info, &size_Papp_info);
#else
   r = pi_file_get_app_info(pf, &Papp_info, &size_Papp_info);
#endif
   if (size_Papp_info <= 0) {
      jp_logf(JP_LOG_WARN, _("%s:%d Error getting app info %s\n"), __FILE__, __LINE__, full_name);
      return EXIT_FAILURE;
   }

   r = unpack_cai_from_ai(&local_cai, Papp_info, size_Papp_info);
   if (r < 0) {
      jp_logf(JP_LOG_WARN, _("%s:%d Error unpacking app info %s\n"), __FILE__, __LINE__, full_name);
      return EXIT_FAILURE;
   }

   pi_file_close(pf);

   /* Open the applications database, store access handle in db */
   r = dlp_OpenDB(sd, 0, dlpOpenReadWrite, DB_name, &db);
   if (r < 0) {
      g_snprintf(log_entry, sizeof(log_entry), _("Unable to open file: %s\n"), DB_name);
      charset_j2p(log_entry, sizeof(log_entry), char_set);
      dlp_AddSyncLogEntry(sd, log_entry);
      jp_logf(JP_LOG_WARN, "sync_categories: %s", log_entry);
      return EXIT_FAILURE;
   }

#ifdef PILOT_LINK_0_12
   buffer = pi_buffer_new(0xFFFF);
   /* buffer size passed in cannot be any larger than 0xffff */
   size = dlp_ReadAppBlock(sd, db, 0, -1, buffer);
   jp_logf(JP_LOG_DEBUG, "readappblock r=%d\n", size);
   if ((size<=0) || (size > sizeof(buf))) {
      jp_logf(JP_LOG_WARN, _("Error reading appinfo block for %s\n"), DB_name);
      dlp_CloseDB(sd, db);
      pi_buffer_free(buffer);
      return EXIT_FAILURE;
   }
   memcpy(buf, buffer->data, size);
   pi_buffer_free(buffer);
#else
   /* buffer size passed in cannot be any larger than 0xffff */
   size = dlp_ReadAppBlock(sd, db, 0, buf, min(sizeof(buf), 0xFFFF));
   jp_logf(JP_LOG_DEBUG, "readappblock r=%d\n", size);
   if (size<=0) {
      jp_logf(JP_LOG_WARN, _("Error reading appinfo block for %s\n"), DB_name);
      dlp_CloseDB(sd, db);
      return EXIT_FAILURE;
   }
#endif
   r = unpack_cai_from_ai(&remote_cai, buf, size);
   memcpy(&orig_remote_cai, &remote_cai, sizeof(remote_cai));
   if (r < 0) {
      jp_logf(JP_LOG_WARN, _("%s:%d Error unpacking app info %s\n"), __FILE__, __LINE__, full_name);
      return EXIT_FAILURE;
   }

#ifdef SYNC_CAT_DEBUG
   printf("DB_name [%s]\n", DB_name);
   for (i = 0; i < CATCOUNT; i++) {
      if (local_cai.name[i][0] != '\0') {
	 printf("local: cat %d [%s] ID %d renamed %d\n", i,
		local_cai.name[i],
		local_cai.ID[i], local_cai.renamed[i]);
      }
   }
   printf("--- remote: size is %d ---\n", size);
   for (i = 0; i < CATCOUNT; i++) {
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
      return EXIT_SUCCESS;
   }

   /* Go through the categories and try to sync them */
   for (Li = loop = 0; ((Li < CATCOUNT) && (loop<256)); Li++, loop++) {
      found_name=found_ID=FALSE;
      found_name_at=found_ID_at=0;
      /* Did a cat get deleted locally? */
      if ((local_cai.name[Li][0]==0) && (local_cai.ID[Li]!=0)) {
	 for (Ri = 0; Ri < CATCOUNT; Ri++) {
	    if ((remote_cai.ID[Ri]==local_cai.ID[Li]) &&
		(remote_cai.name[Ri][0])) {
#ifdef SYNC_CAT_DEBUG
	       printf("cat %d deleted local, del cat on remote\n", Li);
	       printf(" remote cat name %s\n", remote_cai.name[Ri]);
#endif
	       remote_cai.renamed[Ri]=0;
	       remote_cai.name[Ri][0]='\0';
	       /* This category was deleted.
		Move the records to Unfiled */
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
      for (Ri = 0; Ri < CATCOUNT; Ri++) {
	 if (! strncmp(local_cai.name[Li], remote_cai.name[Ri], PILOTCATLTH)) {
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
#ifdef SYNC_CAT_DEBUG
	    printf("cat index %d ok\n", Li);
#endif
	 } else {
	    /* 2: change all local recs to use remote recs ID */
	    /* This is where there is a bit of trouble, since we have a pdb
	     * file on the local side we don't store the ID and there is no way
	     * to do this.
	     * So, we will swap indexes on the local records.
	     */
#ifdef SYNC_CAT_DEBUG
	    printf("cat index %d case 2\n", Li);
#endif
	    r = pdb_file_swap_indexes(DB_name, Li, found_name_at);
	    edit_cats_swap_cats_pc3(DB_name, Li, Ri);
	    g_strlcpy(tmp_name, local_cai.name[found_ID_at], PILOTCATLTH);
	    strncpy(local_cai.name[found_ID_at],
		    local_cai.name[Li], PILOTCATLTH);
	    strncpy(local_cai.name[Li], tmp_name, PILOTCATLTH);
	    Li--;
	    continue;
	 }
      }
      if ((!found_name) && (local_cai.renamed[Li])) {
	 if (found_ID) {
	    /* 3: Change remote category name to match local at index Li */
#ifdef SYNC_CAT_DEBUG
	    printf("cat index %d case 3\n", Li);
#endif
	    g_strlcpy(remote_cai.name[found_ID_at],
		    local_cai.name[Li], PILOTCATLTH);
	 }
      }
      if ((!found_name) && (!found_ID)) {
	 if (remote_cai.name[Li][0]=='\0') {
	    /* 4: Add local category to remote */
#ifdef SYNC_CAT_DEBUG
	    printf("cat index %d case 4\n", Li);
#endif
	    g_strlcpy(remote_cai.name[Li],
		    local_cai.name[Li], PILOTCATLTH);
	    remote_cai.renamed[Li]=0;
 	    remote_cai.ID[Li]=local_cai.ID[Li];
	    continue;
	 } else {
	    /* 5: Add local category to remote in the next available slot.
	     local records are changed to use this index. */
#ifdef SYNC_CAT_DEBUG
 	    printf("cat index %d case 5\n", Li);
#endif
	    found_a_hole=FALSE;
	    for (i=1; i<CATCOUNT; i++) {
	       if (remote_cai.name[i][0]=='\0') {
		  g_strlcpy(remote_cai.name[i],
			  local_cai.name[Li], PILOTCATLTH);
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
	       g_snprintf(log_entry, sizeof(log_entry), _("Could not add category %s to remote.\n"), local_cai.name[Li]);
	       charset_j2p(log_entry, 255, char_set);
	       dlp_AddSyncLogEntry(sd, log_entry);
	       g_snprintf(log_entry, sizeof(log_entry), _("Too many categories on remote.\n"));
	       charset_j2p(log_entry, sizeof(log_entry), char_set);
	       dlp_AddSyncLogEntry(sd, log_entry);
	       g_snprintf(log_entry, sizeof(log_entry), _("All records on desktop in %s will be moved to %s.\n"), local_cai.name[Li], local_cai.name[0]);
	       charset_j2p(log_entry, sizeof(log_entry), char_set);
	       dlp_AddSyncLogEntry(sd, log_entry);
	       jp_logf(JP_LOG_DEBUG, "Moving local recs category %d to unfiled...", Li);
	       edit_cats_change_cats_pc3(DB_name, Li, 0);
	       edit_cats_change_cats_pdb(DB_name, Li, 0);
	    }
	 }
      }
#ifdef SYNC_CAT_DEBUG
      printf("cat index %d case (none)\n", Li);
#endif
   }

   for (i = 0; i < CATCOUNT; i++) {
      remote_cai.renamed[i]=0;
   }

   pack_cai_into_ai(&remote_cai, buf, size_Papp_info);

   /* If the categories changed then write them out */
   if (memcmp(&orig_remote_cai, &remote_cai, sizeof(remote_cai))) {
      jp_logf(JP_LOG_DEBUG, "writing out new categories for %s\n", DB_name);
      dlp_WriteAppBlock(sd, db, buf, size_Papp_info);
      pdb_file_write_app_block(DB_name, buf, size_Papp_info);
   }

   dlp_CloseDB(sd, db);

   return EXIT_SUCCESS;
}

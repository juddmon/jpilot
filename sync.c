/* sync.c
 * 
 * Copyright (C) 1999 by Judd Montgomery
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
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
#include "log.h"
#include "prefs.h"
#include "datebook.h"
#include "plugins.h"

/*#include <pi-source.h> */
#include <pi-socket.h>
#include <pi-dlp.h>
#include <pi-file.h>

#define SYNC_PI_ACCEPT -12

int pipe_in, pipe_out;
extern int pipe_in, pipe_out;
extern pid_t glob_child_pid;

int sync_application(char *DB_name, int sd);
int sync_fetch(int sd, unsigned int flags);
int jpilot_sync(const char *port, unsigned int flags);
int sync_lock();
int sync_unlock();
#ifdef DATEBOOK_HACK
static int hack_delete_record(int sd);
static int hack_write_record(int sd);
#endif
static int sync_process_install_file(int sd);

#ifdef DATEBOOK_HACK
unsigned long hack_record_id;
#endif
int wrote_to_datebook;

void sig_handler(int sig)
{
   jpilot_logf(LOG_DEBUG, "caught signal SIGCHLD\n");
   glob_child_pid = 0;

   /*wait for any child processes */
   waitpid(-1, NULL, WNOHANG);

   /*refresh the screen after a sync */
   cb_app_button(NULL, NULL);
   
   return;
}

static int writef(int fp, char *format, ...)
{
#define WRITE_MAX_BUF	4096
   va_list	       	val;
   char			buf[WRITE_MAX_BUF];

   buf[0] = '\0';

   va_start(val, format);
   g_vsnprintf(buf, WRITE_MAX_BUF ,format, val);
   /*just in case g_vsnprintf reached the max */
   buf[WRITE_MAX_BUF-1] = 0;
   va_end(val);

   write(fp, buf, strlen(buf));

   return TRUE;
}

/*
int sync_loop(const char *port, int full_backup)
{
   int r, done, cons_errors;
   
   done=cons_errors=0;

   r = sync_lock();
   if (r) {
      _exit(0);
   }
   while(!done) {
      r = jpilot_sync(NULL, full_backup);
      if (r) {
	 cons_errors++;
	 if (cons_errors>3) {
	    writef(pipe_out, "sync: too many consecutive errors.  Quiting\n");
	    done=1;
	 }
      } else {
	 cons_errors=0;
      }
   }
   sync_unlock();
   
   return 0;
}
*/

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
      jpilot_logf(LOG_WARN, "open lock file failed\n");
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
      jpilot_logf(LOG_WARN, "lock failed\n");
      read(*fd, str, 10);
      pid = atoi(str);
      jpilot_logf(LOG_FATAL, "sync file is locked by pid %d\n", pid);
      return -1;
   } else {
      jpilot_logf(LOG_DEBUG, "lock succeeded\n");
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
      jpilot_logf(LOG_WARN, "unlock failed\n");
      read(fd, str, 10);
      pid = atoi(str);
      jpilot_logf(LOG_WARN, "sync is locked by pid %d\n", pid);
      close(fd);
      return -1;
   } else {
      jpilot_logf(LOG_DEBUG, "unlock succeeded\n");
      ftruncate(fd, 0);
      close(fd);
   }
   return 0;
}
#endif

int sync_once(const char *port, unsigned int flags)
{
#ifdef USE_LOCKING
   int r;
   int fd;
#endif   

   if (glob_child_pid) {
      jpilot_logf(LOG_WARN, PN": sync PID = %d\n", glob_child_pid);
      jpilot_logf(LOG_WARN, PN": press the hotsync button on the cradle "
	   "or \"kill %d\"\n", glob_child_pid);
      return 0;
   }

   jpilot_logf(LOG_DEBUG, "forking sync process\n");
   switch ( glob_child_pid = fork() ){
    case -1:
      perror("fork");
      return 0;
    case 0:
      /*close(pipe_in); */
      break;
    default:
      signal(SIGCHLD, sig_handler);
      return 0;
   }
#ifdef TEST_SYNC_OUT
   for (r=0; r<10; r++) {
      writef(pipe_out, "Syncing on device\n");
      writef(pipe_out, "testing %d on the write\n");
      sleep(1);
   }
   jpilot_logf(LOG_DEBUG, "sync child exiting\n");
   _exit(0);
#endif
   
#ifdef USE_LOCKING
   r = sync_lock(&fd);
   if (r) {
      jpilot_logf(LOG_DEBUG, "Child cannot lock file\n");
      _exit(0);
   }
#endif

   jpilot_sync(port, flags);
#ifdef USE_LOCKING
   sync_unlock(fd);
#endif
   jpilot_logf(LOG_DEBUG, "sync child exiting\n");
   _exit(0);
}

int jpilot_sync(const char *port, unsigned int flags)
{
   struct pi_sockaddr addr;
   int sd;
   struct PilotUser U;
   int ret;
   const char *device;
   char default_device[]="/dev/pilot";
   int found = 0;
#ifdef ENABLE_PLUGINS
   GList *plugin_list, *temp_list;
   struct plugin_s *plugin;
#endif
   
   device = NULL;
   if (port) {
      if (port[0]) {
	 /*A port was passed in to use */
	 device=port;
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

#ifdef ENABLE_PLUGINS
   if (!(flags & SYNC_NO_PLUGINS)) {
      jpilot_logf(LOG_DEBUG, "sync:calling load_plugins\n");
      load_plugins();
   }
     
   /* Do the pre_sync plugin calls */
   plugin_list=NULL;

   plugin_list = get_plugin_list();

   for (temp_list = plugin_list; temp_list; temp_list = temp_list->next) {
      plugin = (struct plugin_s *)temp_list->data;
      if (plugin) {
	 if (plugin->plugin_pre_sync) {
	    jpilot_logf(LOG_DEBUG, "sync:calling plugin_pre_sync for [%s]\n", plugin->name);
	    plugin->plugin_pre_sync();
	 }
      }
   }
#endif
   
   writef(pipe_out, "****************************************\n");
   writef(pipe_out, "Syncing on device %s\n", device);
   writef(pipe_out, "Press the HotSync button now\n");
   writef(pipe_out, "****************************************\n");
   
   if (!(sd = pi_socket(PI_AF_SLP, PI_SOCK_STREAM, PI_PF_PADP))) {
      perror("pi_socket");
      writef(pipe_out, "pi_socket %s\n", strerror(errno));
#ifdef ENABLE_PLUGINS
      free_plugin_list(&plugin_list);
#endif
      return -1;
   }
    
   addr.pi_family = PI_AF_SLP;
   strcpy(addr.pi_device, device);
  
   ret = pi_bind(sd, (struct sockaddr*)&addr, sizeof(addr));
   if(ret == -1) {
      perror("pi_bind");
      writef(pipe_out, "pi_bind %s\n", strerror(errno));
      writef(pipe_out, "Check your serial port and settings\n");
#ifdef ENABLE_PLUGINS
      free_plugin_list(&plugin_list);
#endif
      return -1;
   }

   ret = pi_listen(sd,1);
   if(ret == -1) {
      perror("pi_listen");
      writef(pipe_out, "pi_listen %s\n", strerror(errno));
#ifdef ENABLE_PLUGINS
      free_plugin_list(&plugin_list);
#endif
      return -1;
   }

   sd = pi_accept(sd, 0, 0);
   if(sd == -1) {
      perror("pi_accept");
      writef(pipe_out, "pi_accept %s\n", strerror(errno));
#ifdef ENABLE_PLUGINS
      free_plugin_list(&plugin_list);
#endif
      return SYNC_PI_ACCEPT;
   }

   dlp_ReadUserInfo(sd, &U);
   
   /*User name is read by the parent process and stored in the preferences */
   /*So, this is more than just displaying it to the user */
   writef(pipe_out, "Username is \"%s\"\n", U.username);
   jpilot_logf(LOG_DEBUG, "Username = [%s]\n", U.username);
   jpilot_logf(LOG_DEBUG, "userID = %d\n", U.userID);
   jpilot_logf(LOG_DEBUG, "lastSyncPC = %d\n", U.lastSyncPC);
/*   set_pref_char(PREF_USER, U.username); */
   /*writef(pipe_out, "passwordlen = [%d]\n", U.passwordLength); */
   /*writef(pipe_out, "password = [%s]\n", U.password); */
  
   if (dlp_OpenConduit(sd)<0) {
      writef(pipe_out, "Sync canceled\n");
#ifdef ENABLE_PLUGINS
      free_plugin_list(&plugin_list);
#endif
      return -1;
   }

   sync_process_install_file(sd);

   sync_application("DatebookDB", sd);
   sync_application("AddressDB", sd);
   sync_application("ToDoDB", sd);
   sync_application("MemoDB", sd);
   
   
#ifdef ENABLE_PLUGINS
   plugin_list = get_plugin_list();

   for (temp_list = plugin_list; temp_list; temp_list = temp_list->next) {
      plugin = (struct plugin_s *)temp_list->data;
      jpilot_logf(LOG_DEBUG, "syncing plugin name: [%s]\n", plugin->name);
      jpilot_logf(LOG_DEBUG, "syncing plugin DB:   [%s]\n", plugin->db_name);
      sync_application(plugin->db_name, sd);
   }
#endif

#ifdef ENABLE_PLUGINS
   /* Do the sync plugin calls */
   plugin_list=NULL;

   plugin_list = get_plugin_list();

   for (temp_list = plugin_list; temp_list; temp_list = temp_list->next) {
      plugin = (struct plugin_s *)temp_list->data;
      if (plugin) {
	 if (plugin->plugin_sync) {
	    jpilot_logf(LOG_DEBUG, "calling plugin_sync for [%s]\n", plugin->name);
	    plugin->plugin_sync(sd);
	 }
      }
   }
#endif
   
#ifdef DATEBOOK_HACK
   hack_write_record(sd);
#endif
   sync_fetch(sd, flags);
   
#ifdef DATEBOOK_HACK
   hack_delete_record(sd);
#endif
   /* Tell the user who it is, with a different PC id. */
   U.lastSyncPC = 0x00010000;
   U.successfulSyncDate = time(NULL);
   U.lastSyncDate = U.successfulSyncDate;
   dlp_WriteUserInfo(sd, &U);


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
	 if (plugin->plugin_post_sync) {
	    jpilot_logf(LOG_DEBUG, "calling plugin_post_sync for [%s]\n", plugin->name);
	    plugin->plugin_post_sync();
	 }
      }
   }

   jpilot_logf(LOG_DEBUG, "freeing plugin list\n");
   free_plugin_list(&plugin_list);
#endif
   
   writef(pipe_out, "Finished.\n");

   return 0;
}

int sync_application(char *DB_name, int sd)
{
   unsigned long new_id;
   int db;
   int ret;
   int num;
   FILE *pc_in;
   PCRecordHeader header;
   char *record;
   char pronoun[10];
   int rec_len;
   char pc_filename[256];
   char write_log_message[256];
   char error_log_message_w[256];
   char error_log_message_d[256];
   char delete_log_message[256];
   char log_entry[256];
/*redo
   recordid_t id;
   int r, index, size, attr, category;
   char buffer[65536];
end redo*/
   if ((DB_name==NULL) || (strlen(DB_name) > 250)) {
      return -1;
   }
   wrote_to_datebook = 0;
   g_snprintf(log_entry, 255, "Syncing %s\n", DB_name);
   log_entry[255]='\0';
   writef(pipe_out, log_entry);
   g_snprintf(pc_filename, 255, "%s.pc", DB_name);
   /* This is an attempt to use the proper pronoun most of the time */
   if (strchr("aeiou", tolower(DB_name[0]))) {
      strcpy(pronoun, "an");
   } else {
      strcpy(pronoun, "a");
   }
   g_snprintf(write_log_message, 255,
	      "Wrote %s %s record to the Pilot.\n\r", pronoun, DB_name);
   g_snprintf(error_log_message_w, 255,
	      "Writing %s %s record to the Pilot failed.\n\r", pronoun, DB_name);
   g_snprintf(error_log_message_d, 255,
	      "Deleting %s %s record from the Pilot failed.\n\r", pronoun, DB_name);
   g_snprintf(delete_log_message, 256,
	      "Deleted %s %s record from the Pilot.\n\r", pronoun, DB_name);

   pc_in = open_file(pc_filename, "r+");
   if (pc_in==NULL) {
      writef(pipe_out, "Unable to open %s\n",pc_filename);
      return -1;
   }
   /* Open the applications database, store access handle in db */
   if (dlp_OpenDB(sd, 0, dlpOpenReadWrite, DB_name, &db) < 0) {
      g_snprintf(log_entry, 255, "Unable to open %s\n\r", DB_name);
      log_entry[255]='\0';
      dlp_AddSyncLogEntry(sd, log_entry);
      return -1;
   }
/*redo
   r = dlp_ReadNextModifiedRec(sd, db, buffer,
			   &id, &index, &size, &attr, &category);
   printf("read next record for %s returned %d\n", DB_name, r);
   if (r>=0 ) {
      printf("id %d, index %d, size %d, attr 0x%x, category %d\n",id, index, size, attr, category);
   }
 end redo
*/
#ifdef JPILOT_DEBUG
   dlp_ReadOpenDBInfo(sd, db, &num);
   writef(pipe_out ,"number of records = %d\n", num);
#endif
   while(!feof(pc_in)) {
      num = fread(&header, sizeof(header), 1, pc_in);
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
	 writef(pipe_out, "PC file corrupt?\n");
	 fclose(pc_in);
	 return -1;
      }
      if (header.rt==NEW_PC_REC) {
	 record = malloc(rec_len);
	 if (!record) {
	    writef(pipe_out, "Out of memory\n");
	    break;
	 }
	 num = fread(record, rec_len, 1, pc_in);
	 if (num != 1) {
	    if (ferror(pc_in)) {
	       break;
	    }
	 }

/*todo move this to before the record is written out.*/
#if defined(WITH_JAPANESE)
	 /* Convert to SJIS Japanese Kanji code (Palm use this code) */
	 /*Write the record to the Palm Pilot */
	 if (!strcmp(DB_name, "DatebookDB")) {
	    struct Appointment a;
	    unpack_Appointment(&a, record, rec_len);
	    if (a.description != NULL)
	      Euc2Sjis(a.description, 65536);
	    if (a.note != NULL)
	      Euc2Sjis(a.note, 65536);
	    rec_len = pack_Appointment(&a, record, 65535);
	    break;
	 }
	 if (!strcmp(DB_name, "AddressDB")) {
	    struct Address a;
	    int i;
	    unpack_Address(&a, record, rec_len);
	    for (i = 0; i < 19; i++)
	      if (a.entry[i] != NULL)
		Euc2Sjis(a.entry[i], 65536);
	    rec_len = pack_Address(&a, record, 65535);
	    break;
	 }
	 if (!strcmp(DB_name, "ToDoDB")) {
	    struct ToDo t;
	    unpack_ToDo(&t, record, rec_len);
	    if (t.description != NULL)
	      Euc2Sjis(t.description, 65536);
            if (t.note != NULL)
	      Euc2Sjis(t.note, 65536);
	    rec_len = pack_ToDo(&t, record, 65535);
	    break;
	 }
	 if (!strcmp(DB_name, "MemoDB")) {
	    struct Memo m;
	    unpack_Memo(&m, record, rec_len);
	    if (m.text != NULL)
	      Euc2Sjis(m.text, 65536);
	    rec_len = pack_Memo(&m, record, 65535);
	    break;
	 }
#endif
	 ret = dlp_WriteRecord(sd, db, 0, 0, header.attrib & 0x0F,
			       record, rec_len, &new_id);
	 
	 if (record) {
	    free(record);
	    record = NULL;
	 }

	 if (ret < 0) {
	    writef(pipe_out, "dlp_WriteRecord failed\n");
	    dlp_AddSyncLogEntry(sd, error_log_message_w);
	 } else {
	    /*Delete the record from the PC database if it succeeded */
	    /*hack2 */
	    wrote_to_datebook = 1;
	    dlp_AddSyncLogEntry(sd, write_log_message);
	    /*Now mark the record as deleted in the pc file */
	    if (fseek(pc_in, -(sizeof(header)+rec_len), SEEK_CUR)) {
	       writef(pipe_out, "fseek failed - fatal error\n");
	       fclose(pc_in);
	       return -1;
	    }
	    header.rt=DELETED_PC_REC;
	    fwrite(&header, sizeof(header), 1, pc_in);
	 }
      }

      if ((header.rt==DELETED_PALM_REC) || (header.rt==MODIFIED_PALM_REC)) {
	 /*writef(pipe_out, "Deleting Palm id=%d,\n",header.unique_id); */
	 ret = dlp_DeleteRecord(sd, db, 0, header.unique_id);
	 
	 if (ret < 0) {
	    writef(pipe_out, "dlp_DeleteRecord failed\n"\
            "This could be because the record was already deleted on the Palm\n");
	    dlp_AddSyncLogEntry(sd, error_log_message_d);
	 } else {
	    dlp_AddSyncLogEntry(sd, delete_log_message);
	 }
	 /*Now mark the record as deleted */
	 if (fseek(pc_in, -sizeof(header), SEEK_CUR)) {
	    writef(pipe_out, "fseek failed - fatal error\n");
	    fclose(pc_in);
	    return -1;
	 }
	 header.rt=DELETED_DELETED_PALM_REC;
	 fwrite(&header, sizeof(header), 1, pc_in);
      }

      /*skip this record now that we are done with it */
      if (fseek(pc_in, rec_len, SEEK_CUR)) {
	 writef(pipe_out, "fseek failed - fatal error\n");
	 fclose(pc_in);
	 return -1;
      }
   }
   fclose(pc_in);

   /*fields[fieldno++] = cPtr; */

#ifdef JPILOT_DEBUG
   dlp_ReadOpenDBInfo(sd, db, &num);
   writef(pipe_out ,"number of records = %d\n", num);
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
int sync_fetch(int sd, unsigned int flags)
{
#define MAX_DBNAME 50
   struct pi_file *pi_fp;
   char full_name[256];
   struct stat statb;
   struct utimbuf times;
   int i, found;
   int cardno, start;
   struct DBInfo info;
   char db_copy_name[MAX_DBNAME];
   char *palm_dbname[]={
      "DatebookDB",
	"AddressDB",
	"ToDoDB",
	"MemoDB",
	NULL
   };
#ifdef ENABLE_PLUGINS
   GList *plugin_list, *temp_list;
   struct plugin_s *plugin;
#endif
   
   
   found=start=cardno=0;
   
   while(dlp_ReadDBList(sd, cardno, dlpOpenRead, start, &info)>0) {
      start=info.index+1;
#ifdef JPILOT_DEBUG
      writef(pipe_out, "dbname = %s\n",info.name);
      writef(pipe_out, "flag backup = %d\n",info.flags & dlpDBFlagBackup);
      writef(pipe_out, "type = %x\n",info.type);
      writef(pipe_out, "creator = %x\n",info.creator);
#endif
      if (!(flags & SYNC_FULL_BACKUP)) {
	 for(i=0, found=0; palm_dbname[i]; i++) {
	    if ((found = !strcmp(info.name, palm_dbname[i])))
	      break;
	 }
      }
#ifdef ENABLE_PLUGINS
      plugin_list = get_plugin_list();

      if ((!found) && (!(flags & SYNC_FULL_BACKUP))) {
	 for (temp_list = plugin_list; temp_list; temp_list = temp_list->next) {
	    plugin = (struct plugin_s *)temp_list->data;
	    if (!strcmp(info.name, plugin->db_name)) {
	       found=1;
	       break;
	    }
	 }
      }
#endif

      if ((flags & SYNC_FULL_BACKUP) || found) {
	 strncpy(db_copy_name, info.name, MAX_DBNAME-5);
	 db_copy_name[MAX_DBNAME-5]='\0';
	 if (info.flags & dlpDBFlagResource) {
	    strcat(db_copy_name,".prc");
	 } else {
	    strcat(db_copy_name,".pdb");
	 }
   
	 get_home_file_name(db_copy_name, full_name, 255);
	 if (stat(full_name, &statb) != 0) {
	    statb.st_mtime = 0;
	 }
#ifdef JPILOT_DEBUG
	 writef(pipe_out, "palm dbtime= %d, local dbtime = %d\n", info.modifyDate, statb.st_mtime);
	 writef(pipe_out, "flags=0%x\n", info.flags);
	 writef(pipe_out, "backup_flag=%d\n", info.flags & dlpDBFlagBackup);
#endif
	 /*If modification times are the same then we don't need to fetch it */
	 if (info.modifyDate == statb.st_mtime) {
	    writef(pipe_out, "%s is up to date, fetch skipped.\n", db_copy_name);
	    continue;
	 }

	 writef(pipe_out, "Fetching '%s'... ", info.name);
   
	 info.flags &= 0xff;
   
	 pi_fp = pi_file_create(full_name, &info);
	 if (pi_fp==0) {
	    writef(pipe_out, "Failed, unable to create file %s\n", full_name);
	    continue;
	 }
	 if(pi_file_retrieve(pi_fp, sd, 0)<0) {
	    writef(pipe_out, "Failed, unable to back up database\n");
	 } else {
	    writef(pipe_out, "OK\n");
	 }
	 pi_file_close(pi_fp);
   
	 /*Set the create and modify times of local file to same as on palm */
	 times.actime = info.createDate;
	 times.modtime = info.modifyDate;
	 utime(full_name, &times);
      }
   }
   return 0;
}

#ifdef DATEBOOK_HACK
static int hack_write_record(int sd)
{
   char record[65536];
   int rec_len;
   int db;
   int ret;
   
   hack_record_id = 0;
   
   jpilot_logf(LOG_DEBUG, "Entering hack_write_record()\n");
   if (wrote_to_datebook) {
      /* Open the applications database, store access handle in db */
      if (dlp_OpenDB(sd, 0, dlpOpenWrite, "DatebookDB", &db) < 0) {
	 dlp_AddSyncLogEntry(sd, "Unable to open DatebookDB\n\r");
	 return -1;
      }
      datebook_create_bogus_record(record, 65536, &rec_len);
      
      /*Write a bogus record to the Palm Pilot, to delete later. */
      jpilot_logf(LOG_DEBUG, "writing bogus record\n");
      ret = dlp_WriteRecord(sd, db, 0, 0, 0,
		      record, rec_len, &hack_record_id);
      if (ret<0) {
	 jpilot_logf(LOG_WARN, "write bogus record failed\n");
      }
      /* Close the database */
      dlp_CloseDB(sd, db);
   }
   return 0;
}
#endif

#ifdef DATEBOOK_HACK
static int hack_delete_record(int sd)
{
   int db;

   if (hack_record_id) {
      /* Open the applications database, store access handle in db */
      if (dlp_OpenDB(sd, 0, dlpOpenWrite, "DatebookDB", &db) < 0) {
	 dlp_AddSyncLogEntry(sd, "Unable to open DatebookDB\r\n");
	 return -1;
      }
      
      jpilot_logf(LOG_DEBUG, "deleting bogus record\n");
      dlp_DeleteRecord(sd, db, 0, hack_record_id);

      /* Close the database */
      dlp_CloseDB(sd, db);
   }
   return 0;
}
#endif

static int sync_install(char *filename, int sd)
{
   struct pi_file *f;
   char *Pc;
   char log_entry[256];
   
   Pc=rindex(filename, '/');
   if (!Pc) {
      Pc = filename;
   } else {
      Pc++;
   }
      
   writef(pipe_out, "Installing %s... ", Pc);
   f = pi_file_open(filename);
   if (f==0) {
      writef(pipe_out, "\nUnable to open '%s'!\n", filename);
      return -1;
   }
   if (pi_file_install(f, sd, 0)<0) {
      g_snprintf(log_entry, 255, "Install %s failed\n\r", Pc);
      log_entry[255]='\0';
      dlp_AddSyncLogEntry(sd, log_entry);
      writef(pipe_out, "Failed.\n");
      pi_file_close(f);
      return -1;
   }
   else {
      g_snprintf(log_entry, 255, "Installed %s\n\r", Pc);
      log_entry[255]='\0';
      dlp_AddSyncLogEntry(sd, log_entry);
      writef(pipe_out, "OK\n");
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

   in = open_file("jpilot_to_install", "r");
   if (!in) {
      writef(pipe_out, "Cannot open jpilot_to_install file\n");
      return -1;
   }

   out = open_file("jpilot_to_install.tmp", "w");
   if (!out) {
      writef(pipe_out, "Cannot open jpilot_to_install.tmp file\n");
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
   
   rename_file("jpilot_to_install.tmp", "jpilot_to_install");
   
   return 0;
}

/*
 * sync.c
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
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <signal.h>
#include <utime.h>
#include "utils.h"
#include "sync.h"
#include "log.h"

//#include <pi-source.h>
#include <pi-socket.h>
#include <pi-dlp.h>
#include <pi-file.h>

#define SYNC_PI_ACCEPT -12

int pipe_in, pipe_out;
pid_t glob_child_pid;extern int pipe_in, pipe_out;
pid_t glob_child_pid;


int sync_application(AppType app_type, int sd);
int sync_fetch(int sd, int full_backup);
int jpilot_sync(char *port, int full_backup);
int sync_lock();
int sync_unlock();

unsigned long hack_record_id;
int wrote_to_datebook;

void sig_handler(int sig)
{
   logf(LOG_DEBUG, "caught signal SIGCHLD\n");
   fflush(stdout);
   glob_child_pid = 0;

   //wait for any child processes
   waitpid(-1, NULL, WNOHANG);
   
   return;
}

static int writef(int fp, char *format, ...)
{
#define WRITE_MAX_BUF	4096
   va_list	       	val;
   char			buf[WRITE_MAX_BUF];

   buf[0] = '\0';

   va_start(val, format);
   vsnprintf(buf, WRITE_MAX_BUF ,format, val);
   //just in case vsnprintf reached the max
   buf[WRITE_MAX_BUF-1] = 0;
   va_end(val);

   write(fp, buf, strlen(buf));

   return TRUE;
}

int sync_loop(char *port, int full_backup)
{
   int r, done, cons_errors;
   
   done=cons_errors=0;

//   sleep(1);
//   while (1) {
//      write(pipe_out, "Syncing on device\n", 18);
//      sleep(1);
//   }
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
}

int sync_lock()
{
   pid_t pid;
   char lock_file[256];
   int fd, r;
   char str[12];

   get_home_file_name("sync_pid", lock_file, 255);
   fd = open(lock_file, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
   if (fd<0) {
      logf(LOG_WARN, "open lock file failed\n");
      return -1;
   }
   r = flock(fd, LOCK_EX | LOCK_NB);
   if (r) {
      logf(LOG_WARN, "lock failed\n");
      read(fd, str, 10);
      pid = atoi(str);
      logf(LOG_FATAL, "sync file is locked by pid %d\n", pid);
      return -1;
   } else {
      logf(LOG_DEBUG, "lock succeeded\n");
      pid=getpid();
      sprintf(str, "%d\n", pid);
      write(fd, str, strlen(str)+1);
      ftruncate(fd, strlen(str)+1);
   }
   return 0;
}

int sync_unlock()
{
   pid_t pid;
   char lock_file[256];
   int fd, r;
   char str[12];

   get_home_file_name("sync_pid", lock_file, 255);
   fd = open(lock_file, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
   if (fd<0) {
      logf(LOG_WARN, "open lock file failed\n");
      return -1;
   }
   r = flock(fd, LOCK_UN | LOCK_NB);
   if (r) {
      logf(LOG_WARN, "unlock failed\n");
      read(fd, str, 10);
      pid = atoi(str);
      logf(LOG_FATAL, "sync is locked by pid %d\n", pid);
      return -1;
   } else {
      logf(LOG_DEBUG, "unlock succeeded\n");
      ftruncate(fd, 0);
   }
   return 0;
}

int sync_once(char *port, int full_backup)
{
   int r;
   
   logf(LOG_DEBUG, "forking sync process\n");
   switch ( glob_child_pid = fork() ){
    case -1:
      perror("fork");
      return;
    case 0:
      //close(pipe_in);
      break;
    default:
      signal(SIGCHLD, sig_handler);
      return;
   }
#ifdef TEST_SYNC_OUT
   for (r=0; r<10; r++) {
      writef(pipe_out, "Syncing on device\n");
      writef(pipe_out, "testing %d on the write\n");
      sleep(1);
   }
   logf(LOG_DEBUG, "sync child exiting\n");
   _exit(0);
#endif
   r = sync_lock();
   if (r) {
      logf(LOG_DEBUG, "sync child exiting\n");
      _exit(0);
   }
   jpilot_sync(port, full_backup);
   sync_unlock();
      logf(LOG_DEBUG, "sync child exiting\n");
   _exit(0);
}

int jpilot_sync(char *port, int full_backup)
{
   struct pi_sockaddr addr;
   int sd;
   struct PilotUser U;
   int ret;
   char *device;
   char default_device[]="/dev/pilot";
   
   if (port) {
      //A port was passed in to use
      device=port;
   } else {
      //No port was passed in, look in env
      device = getenv("PILOTPORT");
      if (device == NULL) {
	 device = default_device;
      }
   }

   writef(pipe_out, "****************************************\n");
   writef(pipe_out, "Syncing on device %s\n", device);
   writef(pipe_out, "Press the HotSync button now\n");
   writef(pipe_out, "****************************************\n");
   
   if (!(sd = pi_socket(PI_AF_SLP, PI_SOCK_STREAM, PI_PF_PADP))) {
      perror("pi_socket");
      return -1;
   }
    
   addr.pi_family = PI_AF_SLP;
   strcpy(addr.pi_device,device);
  
   ret = pi_bind(sd, (struct sockaddr*)&addr, sizeof(addr));
   if(ret == -1) {
      perror("pi_bind");
      return -1;
   }

   ret = pi_listen(sd,1);
   if(ret == -1) {
      perror("pi_listen");
      return -1;
   }

   sd = pi_accept(sd, 0, 0);
   if(sd == -1) {
      perror("pi_accept");
      return SYNC_PI_ACCEPT;
   }

   dlp_ReadUserInfo(sd, &U);
   
   writef(pipe_out, "Username = [%s]\n", U.username);
   //writef(pipe_out, "passwordlen = [%d]\n", U.passwordLength);
   //writef(pipe_out, "password = [%s]\n", U.password);
  
   if (dlp_OpenConduit(sd)<0) {
      writef(pipe_out, "Sync canceled\n");
      //todo - I'm not sure what to do here
      return -1;
   }

   sync_application(DATEBOOK, sd);
   sync_application(ADDRESS, sd);
   sync_application(TODO, sd);
   sync_application(MEMO, sd);

   hack_write_record(sd);
   
   sync_fetch(sd, full_backup);
   
   hack_delete_record(sd);

   // Tell the user who it is, with a different PC id.
   U.lastSyncPC = 0x00010000;
   U.successfulSyncDate = time(NULL);
   U.lastSyncDate = U.successfulSyncDate;
   dlp_WriteUserInfo(sd,&U);


   dlp_EndOfSync(sd,0);
   pi_close(sd);

   cleanup_pc_files();

   writef(pipe_out, "Finished.\n");

   return 0;
}

int sync_application(AppType app_type, int sd)
{
   unsigned long new_id;
   int db;
   int ret;
   int num;
   FILE *pc_in;
   PCRecordHeader header;
   char *record;
   int rec_len;
   char pc_filename[50];
   char palm_dbname[50];
   char write_log_message[50];
   char error_log_message[50];
   char delete_log_message[50];
   
   switch (app_type) {
    case DATEBOOK:
      wrote_to_datebook = 0;
      writef(pipe_out, "Syncing Datebook\n");
      strcpy(pc_filename, "DatebookDB.pc");
      strcpy(palm_dbname, "DatebookDB");
      strcpy(write_log_message, "Wrote an Appointment to the Pilot.\n");
      strcpy(error_log_message, "Writing an Appointment to the Pilot failed.\n");
      strcpy(delete_log_message, "Deleted an Appointment from the Pilot.\n");
      break;
    case ADDRESS:
      writef(pipe_out, "Syncing Addressbook\n");
      strcpy(pc_filename, "AddressDB.pc");
      strcpy(palm_dbname, "AddressDB");
      strcpy(write_log_message, "Wrote an Addresss to the Pilot.\n");
      strcpy(error_log_message, "Writing an Address to the Pilot failed.\n");
      strcpy(delete_log_message, "Deleted an Address from the Pilot.\n");
      break;
    case TODO:
      writef(pipe_out, "Syncing ToDo\n");
      strcpy(pc_filename, "ToDoDB.pc");
      strcpy(palm_dbname, "ToDoDB");
      strcpy(write_log_message, "Wrote a ToDo to the Pilot.\n");
      strcpy(error_log_message, "Writing a ToDo to the Pilot failed.\n");
      strcpy(delete_log_message, "Deleted a ToDo from the Pilot.\n");
      break;
    case MEMO:
      writef(pipe_out, "Syncing Memo\n");
      strcpy(pc_filename, "MemoDB.pc");
      strcpy(palm_dbname, "MemoDB");
      strcpy(write_log_message, "Wrote a Memo to the Pilot.\n");
      strcpy(error_log_message, "Writing a Memo to the Pilot failed.\n");
      strcpy(delete_log_message, "Deleted a Memo from the Pilot.\n");
      break;
    default:
      return;
   }

   pc_in = open_file(pc_filename, "r+");
   if (pc_in==NULL) {
      writef(pipe_out, "Unable to open %s\n",pc_filename);
      return;
   }
   // Open the applications database, store access handle in db
   if (dlp_OpenDB(sd, 0, dlpOpenWrite, palm_dbname, &db) < 0) {
      dlp_AddSyncLogEntry(sd, "Unable to open ");
      dlp_AddSyncLogEntry(sd, palm_dbname);
      dlp_AddSyncLogEntry(sd, "\n.");
      pi_close(sd);
      return -1;
   }
#ifdef JPILOT_DEBUG
   dlp_ReadOpenDBInfo(sd, db, &num);
   writef(pipe_out ,"number of records = %d\n", num);
#endif
   while(!feof(pc_in)) {
      num = fread(&header, sizeof(header), 1, pc_in);
      if (feof(pc_in) || (!num)) {
	 break;
      }
      rec_len = header.rec_len;
      if (rec_len > 0x10000) {
	 writef(pipe_out, "PC file corrupt?\n");
	 fclose(pc_in);
	 return;
      }
      if (header.rt==NEW_PC_REC) {
	 record = malloc(rec_len);
	 num = fread(record, rec_len, 1, pc_in);
	 if (feof(pc_in) || (!num)) {
	    break;
	 }

	 //Write the record to the Palm Pilot
	 ret = dlp_WriteRecord(sd, db, 0, 0, header.attrib & 0x0F,
			       record, rec_len, &new_id);
	 
	 if (record) {
	    free(record);
	    record = NULL;
	 }

	 if (ret < 0) {
	    writef(pipe_out, "dlp_WriteRecord failed\n");
	    dlp_AddSyncLogEntry(sd, error_log_message);	    
	 } else {
	    //hack2
	    wrote_to_datebook = 1;
	    dlp_AddSyncLogEntry(sd, write_log_message);
	    //Now mark the record as deleted in the pc file
	    if (fseek(pc_in, -(sizeof(header)+rec_len), SEEK_CUR)) {
	       writef(pipe_out, "fseek failed - fatal error\n");
	       fclose(pc_in);
	       return;
	    }
	    header.rt=DELETED_PC_REC;
	    fwrite(&header, sizeof(header), 1, pc_in);
	 }
      }

      if ((header.rt==DELETED_PALM_REC) || (header.rt==MODIFIED_PALM_REC)) {
	 //writef(pipe_out, "Deleting Palm id=%d,\n",header.unique_id);
	 ret = dlp_DeleteRecord(sd, db, 0, header.unique_id);
	 
	 if (ret < 0) {
	    writef(pipe_out, "dlp_DeleteRecord failed\n");
	    dlp_AddSyncLogEntry(sd, error_log_message);
	 } else {
	    dlp_AddSyncLogEntry(sd, delete_log_message);
	    //Now mark the record as deleted
	    if (fseek(pc_in, -sizeof(header), SEEK_CUR)) {
	       writef(pipe_out, "fseek failed - fatal error\n");
	       fclose(pc_in);
	       return;
	    }
	    header.rt=DELETED_DELETED_PALM_REC;
	    fwrite(&header, sizeof(header), 1, pc_in);
	 }
      }

      //skip this record now that we are done with it
      if (fseek(pc_in, rec_len, SEEK_CUR)) {
	 writef(pipe_out, "fseek failed - fatal error\n");
	 fclose(pc_in);
	 return;
      }
   }
   fclose(pc_in);

   //fields[fieldno++] = cPtr;

#ifdef JPILOT_DEBUG
   dlp_ReadOpenDBInfo(sd, db, &num);
   writef(pipe_out ,"number of records = %d\n", num);
#endif

   // Close the database
   dlp_CloseDB(sd, db);

}

//
// Fetch the databases from the palm if modified
//
int sync_fetch(int sd, int full_backup)
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
      
   start=cardno=0;
   
   while(dlp_ReadDBList(sd, cardno, dlpOpenRead, start, &info)>0) {
      start=info.index+1;
      //writef(pipe_out, "dbname = %s\n",info.name);
      //writef(pipe_out, "type = %x\n",info.type);
      //writef(pipe_out, "creator = %x\n",info.creator);
      for(i=0, found=0; palm_dbname[i]; i++) {
	 if (found = !strcmp(info.name, palm_dbname[i]))
	   break;
      }
      if (full_backup || found) {
	 strncpy(db_copy_name, info.name, MAX_DBNAME-5);
	 db_copy_name[MAX_DBNAME-5]='\0';
	 if (info.flags & dlpDBFlagResource) {
	    strcat(db_copy_name,".prc");
	 } else {
	    strcat(db_copy_name,".pdb");
	 }
   
	 get_home_file_name(db_copy_name, full_name, 256);
	 if (stat(full_name, &statb) != 0) {
	    statb.st_mtime = 0;
	 }
#ifdef JPILOT_DEBUG
	 writef(pipe_out, "palm dbtime= %d, local dbtime = %d\n", info.modifyDate, statb.st_mtime);
	 writef(pipe_out, "flags=0%x\n", info.flags);
	 writef(pipe_out, "backup_flag=%d\n", info.flags & dlpDBFlagBackup);
#endif
	 //If modification times are the same then we don't need to fetch it
	 if (info.modifyDate == statb.st_mtime) {
	    writef(pipe_out, "%s is up to date, fetch skipped.\n", db_copy_name);
	    continue;
	 }

	 writef(pipe_out, "Fetching '%s'... ", info.name);
	 fflush(stdout);
   
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
   
	 //Set the create and modify times of local file to same as on palm
	 times.actime = info.createDate;
	 times.modtime = info.modifyDate;
	 utime(full_name, &times);
      }
   }
}

int hack_write_record(int sd)
{
   char record[65536];
   int rec_len;
   int db;
   int ret;
   
   hack_record_id = 0;
   
   logf(LOG_DEBUG, "Entering hack_write_record()\n");
   if (wrote_to_datebook) {
      // Open the applications database, store access handle in db
      if (dlp_OpenDB(sd, 0, dlpOpenWrite, "DatebookDB", &db) < 0) {
	 dlp_AddSyncLogEntry(sd, "Unable to open ");
	 dlp_AddSyncLogEntry(sd, "DatebookDB");
	 dlp_AddSyncLogEntry(sd, "\n.");
	 return -1;
      }
      datebook_create_bogus_record(record, 65536, &rec_len);
      
      //Write a bogus record to the Palm Pilot, to delete later.
      logf(LOG_DEBUG, "writing bogus record\n");
      ret = dlp_WriteRecord(sd, db, 0, 0, 0,
		      record, rec_len, &hack_record_id);
      if (ret<0) {
	 logf(LOG_WARN, "write bogus record failed\n");
      }
      // Close the database
      dlp_CloseDB(sd, db);
   }
}

int hack_delete_record(int sd)
{
   int db;

   if (hack_record_id) {
      // Open the applications database, store access handle in db
      if (dlp_OpenDB(sd, 0, dlpOpenWrite, "DatebookDB", &db) < 0) {
	 dlp_AddSyncLogEntry(sd, "Unable to open ");
	 dlp_AddSyncLogEntry(sd, "DatebookDB");
	 dlp_AddSyncLogEntry(sd, "\n.");
	 pi_close(sd);
      }
      
      logf(LOG_DEBUG, "deleting bogus record\n");
      dlp_DeleteRecord(sd, db, 0, hack_record_id);

      // Close the database
      dlp_CloseDB(sd, db);
   }
}

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
#include <sys/stat.h>
#include "utils.h"
#include <utime.h>

//#include <pi-source.h>
#include <pi-socket.h>
#include <pi-dlp.h>
#include <pi-file.h>

int application_sync(AppType app_type, int sd);

int jpilot_sync(char *port)
{
   struct pi_sockaddr addr;
   int sd;
   int i;
   int Appointment_size;
   unsigned char Appointment_buf[0xffff];
   struct Appointment appointment;
   FILE *f;
   struct PilotUser U;
   int ret;
   int filelen;
   char *cPtr;
   char *device;
   char default_device[]="/dev/pilot";
   char *file_text;
   char *fields[4];
   int fieldno;
   
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

   printf("Syncing on device %s\n", device);
   printf("Press the HotSync button\n");
   
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
      return -1;
   }

   dlp_ReadUserInfo(sd, &U);
   
   printf("username = [%s]\n", U.username);
   printf("passwordlen = [%d]\n", U.passwordLength);
   //printf("password = [%s]\n", U.password);
  
   dlp_OpenConduit(sd);
  

   application_sync(DATEBOOK, sd);
   application_sync(ADDRESS, sd);
   application_sync(TODO, sd);
   application_sync(MEMO, sd);
   
   
   // Tell the user who it is, with a different PC id.
   U.lastSyncPC = 0x00010000;
   U.successfulSyncDate = time(NULL);
   U.lastSyncDate = U.successfulSyncDate;
   dlp_WriteUserInfo(sd,&U);

   //dlp_AddSyncLogEntry(sd, "Wrote Appointment to Pilot.\n");
  
   // All of the following code is now unnecessary, but harmless
  
   dlp_EndOfSync(sd,0);
   pi_close(sd);

   cleanup_pc_files();

   printf("Finished.\n");

   return 0;
}

int application_sync(AppType app_type, int sd)
{
   struct pi_file *pi_fp;
   char full_name[256];
   unsigned long new_id;
   int db;
   int ret;
   int num;
   //time_t start, end;
   FILE *pc_in;
   PCRecordHeader header;
   char *record;
   int rec_len;
   struct DBInfo info;
   struct stat statb;
   struct utimbuf times;
   char pc_filename[50];
   char palm_dbname[50];
   char db_copy_name[50];
   char write_log_message[50];
   char error_log_message[50];
   char delete_log_message[50];
   
   switch (app_type) {
    case DATEBOOK:
      printf("Syncing Datebook\n");
      strcpy(pc_filename, "DatebookDB.pc");
      strcpy(palm_dbname, "DatebookDB");
      strcpy(db_copy_name,"DatebookDB.pdb");
      strcpy(write_log_message, "Wrote an Appointment to the Pilot.\n");
      strcpy(error_log_message, "Writing an Appointment to the Pilot failed.\n");
      strcpy(delete_log_message, "Deleted an Appointment from the Pilot.\n");
      break;
    case ADDRESS:
      printf("Syncing Addressbook\n");
      strcpy(pc_filename, "AddressDB.pc");
      strcpy(palm_dbname, "AddressDB");
      strcpy(db_copy_name,"AddressDB.pdb");
      strcpy(write_log_message, "Wrote an Addresss to the Pilot.\n");
      strcpy(error_log_message, "Writing an Address to the Pilot failed.\n");
      strcpy(delete_log_message, "Deleted an Address from the Pilot.\n");
      break;
    case TODO:
      printf("Syncing ToDo\n");
      strcpy(pc_filename, "ToDoDB.pc");
      strcpy(palm_dbname, "ToDoDB");
      strcpy(db_copy_name,"ToDoDB.pdb");
      strcpy(write_log_message, "Wrote a ToDo to the Pilot.\n");
      strcpy(error_log_message, "Writing a ToDo to the Pilot failed.\n");
      strcpy(delete_log_message, "Deleted a ToDo from the Pilot.\n");
      break;
    case MEMO:
      printf("Syncing Memo\n");
      strcpy(pc_filename, "MemoDB.pc");
      strcpy(palm_dbname, "MemoDB");
      strcpy(db_copy_name,"MemoDB.pdb");
      strcpy(write_log_message, "Wrote a Memo to the Pilot.\n");
      strcpy(error_log_message, "Writing a Memo to the Pilot failed.\n");
      strcpy(delete_log_message, "Deleted a Memo from the Pilot.\n");
      break;
    default:
      return;
   }

   pc_in = open_file(pc_filename, "r+");
   if (pc_in==NULL) {
      printf("Unable to open %s\n",pc_filename);
      return;
   }
   // Open the applications database, store access handle in db
   if (dlp_OpenDB(sd, 0, 0x80|0x40, palm_dbname, &db) < 0) {
      dlp_AddSyncLogEntry(sd, "Unable to open ");
      dlp_AddSyncLogEntry(sd, palm_dbname);
      dlp_AddSyncLogEntry(sd, "\n.");
      pi_close(sd);
      return -1;
   }
   //time(&start);
   //time(&end);
   //end+=3600;

   while(!feof(pc_in)) {
      num = fread(&header, sizeof(header), 1, pc_in);
      if (feof(pc_in) || (!num)) {
	 break;
      }
      rec_len = header.rec_len;
      if (rec_len > 0x10000) {
	 printf("PC file corrupt?\n");
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
	 //printf("New ID=%d\n", new_id);	 
	 free(record);
	 
	 if (ret < 0) {
	    printf("dlp_WriteRecord failed\n");
	    dlp_AddSyncLogEntry(sd, error_log_message);	    
	 } else {
	    dlp_AddSyncLogEntry(sd, write_log_message);
	    //Now mark the record as deleted in the pc file
	    if (fseek(pc_in, -(sizeof(header)+rec_len), SEEK_CUR)) {
	       printf("fseek failed - fatal error\n");
	       fclose(pc_in);
	       return;
	    }
	    header.rt=DELETED_PC_REC;
	    fwrite(&header, sizeof(header), 1, pc_in);
	 }
      }

      if (header.rt==DELETED_PALM_REC) {
	 printf("Deleteing Palm id=%d,\n",header.unique_id);
	 ret = dlp_DeleteRecord(sd, db, 0, header.unique_id);
	 
	 if (ret < 0) {
	    printf("dlp_DeleteRecord failed\n");
	    dlp_AddSyncLogEntry(sd, error_log_message);
	 } else {
	    dlp_AddSyncLogEntry(sd, delete_log_message);
	    //Now mark the record as deleted
	    if (fseek(pc_in, -sizeof(header), SEEK_CUR)) {
	       printf("fseek failed - fatal error\n");
	       fclose(pc_in);
	       return;
	    }
	    header.rt=DELETED_DELETED_PALM_REC;
	    fwrite(&header, sizeof(header), 1, pc_in);
	 }
      }

      //skip this record now that we are done with it
      if (fseek(pc_in, rec_len, SEEK_CUR)) {
	 printf("fseek failed - fatal error\n");
	 fclose(pc_in);
	 return;
      }
   }
   fclose(pc_in);

   //fields[fieldno++] = cPtr;

   // Close the database
   dlp_CloseDB(sd, db);

   //
   // Fetch the database from the palm if modified
   //
   
   get_home_file_name(db_copy_name, full_name, 256);
   
   //Get the db info for this database
   if (dlp_FindDBInfo(sd, 0, 0, palm_dbname, 0, 0, &info) != 0) {
      printf("Unable to find %s on the palm, fetch skipped.\n", palm_dbname);
      return;
   }
   
   if (stat(full_name, &statb) != 0) {
      statb.st_mtime = 0;
   }
#ifdef JPILOT_DEBUG
   printf("palm dbtime= %d, local dbtime = %d\n", info.modifyDate, statb.st_mtime);
#endif
   //If modification times are the same then we don't need to fetch it
   if (info.modifyDate == statb.st_mtime) {
      printf("%s is up to date, fetch skipped.\n", palm_dbname);
      return;
   }

   //protect_name(name, dbname);
   //if (info.flags & dlpDBFlagResource)
   //  strcat(name,".prc");
   //else
   //  strcat(name,".pdb");
   
   printf("Fetching '%s'... ", palm_dbname);
   fflush(stdout);
   
   info.flags &= 0xff;
   
   pi_fp = pi_file_create(full_name, &info);
   if (pi_fp==0) {
      printf("Failed, unable to create file %s\n", full_name);
      return;
   }
   if(pi_file_retrieve(pi_fp, sd, 0)<0) {
      printf("Failed, unable to back up database\n");
   } else {
      printf("OK\n");
   }
   pi_file_close(pi_fp);
   
   //Set the create and modify times of local file to same as on palm
   times.actime = info.createDate;
   times.modtime = info.modifyDate;
   utime(full_name, &times);
}

/*
 * datebook.c
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
#include <stdio.h>
#include <pi-source.h>
#include <pi-socket.h>
#include <pi-datebook.h>
#include <pi-dlp.h>
#include <pi-file.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <utime.h>

#include "datebook.h"
#include "utils.h"

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

#define DATEBOOK_EOF 7


static int pc_datebook_read_next_rec(FILE *in, MyAppointment *ma);

//todo
int datebook_cleanup()
{
}

int datebook_sort(AppointmentList **al)
{
   AppointmentList *temp_al, *prev_al, *next;
   int found_one;
   
   //printf("datebook_sort()\n");
   found_one=1;
   while (found_one) {
      found_one=0;
      for (prev_al=NULL, temp_al=*al; temp_al;
	   prev_al=temp_al, temp_al=temp_al->next) {
	 //printf("begin %d\n", temp_al->ma.a.begin.tm_hour);
	 if (temp_al->next) {
	    if (temp_al->ma.a.begin.tm_hour > temp_al->next->ma.a.begin.tm_hour) {
	       found_one=1;
	       next=temp_al->next;
	       if (prev_al) {
		  prev_al->next = next;
	       }
	       temp_al->next=next->next;
	       next->next = temp_al;
	       if (temp_al==*al) {
		  *al=next;
	       }
	       temp_al=next;
	    }
	 }
      }
   }
}

//creates the full path name of a file in the ~/.jpilot dir
int get_home_file_name(char *file, char *full_name, int max_size)
{
   char *home, default_path[]=".";

   home = getenv("HOME");
   if (!home) {//Not home;
      printf("Can't get HOME environment variable\n");
   }
   if (strlen(home)>(max_size-strlen(file)-2)) {
      printf("Your HOME environment variable is too long for me\n");
      home=default_path;
   }
   sprintf(full_name, "%s/.jpilot/%s", home, file);
   return 0;
}


int application_sync(AppType app_type, int sd);

int jpilot_sync()
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
//   struct DBInfo info;
//   struct stat statb;
//   struct utimbuf times;
   //char name[256];
   
   device = getenv("PILOTPORT");
   if (device == NULL) {
      device = default_device;
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

   dlp_ReadUserInfo(sd,&U);
   
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

//todo pass dbinfo in here
int application_sync(AppType app_type, int sd)
{
   struct pi_file *pi_fp;
   char full_name[256];
   unsigned long new_id;
   int db;
   int ret;
   time_t start, end;
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
   char delete_log_message[50];
   
   switch (app_type) {
    case DATEBOOK:
      printf("Syncing Datebook\n");
      strcpy(pc_filename, "DatebookDB.pc");
      strcpy(palm_dbname, "DatebookDB");
      strcpy(db_copy_name,"DatebookDB.pdb");
      strcpy(write_log_message, "Wrote an Appointment to the Pilot.\n");
      strcpy(delete_log_message, "Deleted an Appointment from the Pilot.\n");
      break;
    case ADDRESS:
      printf("Syncing Addressbook\n");
      strcpy(pc_filename, "AddressDB.pc");
      strcpy(palm_dbname, "AddressDB");
      strcpy(db_copy_name,"AddressDB.pdb");
      strcpy(write_log_message, "Wrote an Addresss to the Pilot.\n");
      strcpy(delete_log_message, "Deleted an Address from the Pilot.\n");
      break;
    case TODO:
      printf("Syncing ToDo\n");
      strcpy(pc_filename, "ToDoDB.pc");
      strcpy(palm_dbname, "ToDoDB");
      strcpy(db_copy_name,"ToDoDB.pdb");
      strcpy(write_log_message, "Wrote a ToDo to the Pilot.\n");
      strcpy(delete_log_message, "Deleted a ToDo from the Pilot.\n");
      break;
    case MEMO:
      printf("Syncing Memo\n");
      strcpy(pc_filename, "MemoDB.pc");
      strcpy(palm_dbname, "MemoDB");
      strcpy(db_copy_name,"MemoDB.pdb");
      strcpy(write_log_message, "Wrote a Memo to the Pilot.\n");
      strcpy(delete_log_message, "Deleted a Memo from the Pilot.\n");
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
   time(&start);
   time(&end);
   end+=3600;

   while(!feof(pc_in)) {
      fread(&header, sizeof(header), 1, pc_in);
      if (feof(pc_in)) {
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
	 fread(record, rec_len, 1, pc_in);
	 //Write the record to the Palm Pilot
	 //todo return code?
      	 dlp_WriteRecord(sd, db, 0, 0, header.attrib & 0x0F,
			 record, rec_len, &new_id);
	 free(record);
	 dlp_AddSyncLogEntry(sd, write_log_message);
	 //printf("New ID=%d\n", new_id);
	 //Now mark the record as deleted
	 if (fseek(pc_in, -(sizeof(header)+rec_len), SEEK_CUR)) {
	    printf("fseek failed - fatal error\n");
	    fclose(pc_in);
	    return;
	 }
	 header.rt=DELETED_PC_REC;
	 fwrite(&header, sizeof(header), 1, pc_in);
	 //printf("Record deleted from PC file\n");
      }
      if (header.rt==DELETED_PALM_REC) {
	 printf("Deleteing Palm id=%d,\n",header.unique_id);
	 ret = dlp_DeleteRecord(sd, db, 0, header.unique_id);
	 printf("dlp_DeleteRecord returned %d\n",ret);
	 dlp_AddSyncLogEntry(sd, delete_log_message);
	 //Now mark the record as deleted
	 if (fseek(pc_in, -sizeof(header), SEEK_CUR)) {
	    printf("fseek failed - fatal error\n");
	    fclose(pc_in);
	    return;
	 }
	 header.rt=DELETED_DELETED_PALM_REC;
	 fwrite(&header, sizeof(header), 1, pc_in);
	 //printf("pc record deleted\n");
      }
      //skip this record
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
   if (dlp_FindDBInfo(sd, 0, 0, palm_dbname, 0, 0, &info) < 0) {
      printf("Unable to find %s on the palm, fetch skipped.\n", palm_dbname);
      return;
   }
   
   if (stat(full_name, &statb) != 0) {
      statb.st_mtime = 0;
   }

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


int does_pc_appt_exist(int unique_id, PCRecType rt)
{
   FILE *pc_in;
   PCRecordHeader header;
   
   //printf("looking for unique id = %d\n",unique_id);
   //printf("looking for rt = %d\n",rt);
   pc_in=open_file("DatebookDB.pc", "r");
   if (pc_in==NULL) {
      printf("Couldn't open PC records file\n");
      return 0;
   }
   while(!feof(pc_in)) {
      fread(&header, sizeof(header), 1, pc_in);
      if (feof(pc_in)) break;
      //printf("read unique id = %d\n",header.unique_id);
      //printf("read rt = %d\n",header.rt);
      if ((header.unique_id==unique_id)
	   && (header.rt==rt)) {
	 fclose(pc_in);
	 //printf("TRUE\n");
	 return 1;
      }
      if (fseek(pc_in, header.rec_len, SEEK_CUR)) {
	 printf("fseek failed\n");
      }
   }
   fclose(pc_in);
   return 0;
}


int pc_datebook_write(struct Appointment *a, PCRecType rt)
{
   PCRecordHeader header;
   //PCFileHeader   file_header;
   FILE *out;
   char record[65535];
   int rec_len;
   unsigned int next_unique_id;

   get_next_unique_pc_id(&next_unique_id);
   //printf("next unique id = %d\n",next_unique_id);

   out = open_file("DatebookDB.pc", "a");
   if (!out) {
      printf("Error opening DatebookDB.pc\n");
      return -1;
   }
   //todo check return code - if 0 then buffer was too small
   rec_len = pack_Appointment(a, record, 65535);
   header.rec_len=rec_len;
   header.rt=rt;
   header.unique_id=next_unique_id;
   fwrite(&header, sizeof(header), 1, out);
   fwrite(record, rec_len, 1, out);
   fflush(out);
   fclose(out);
}

/*
 static int pc_datebook_read_file_header(FILE *in)
{
   PCFileHeader   file_header;

   if (ftell(in)!=0) {
      printf("Error: datebook_read_file_header(): Not at BOF\n");
      return -1;
   }
   fread(&file_header, sizeof(file_header), 1, in);
}
 */  
static int pc_datebook_read_next_rec(FILE *in, MyAppointment *ma)
{
   PCRecordHeader header;
   int rec_len;
   char *record;
   //DatebookRecType rt;
   
   if (feof(in)) {
      return DATEBOOK_EOF;
   }
//  if (ftell(in)==0) {
//     printf("Error: File header not read\n");
//      return DATEBOOK_EOF;
//   }
   //todo check return codes
   fread(&header, sizeof(header), 1, in);
   if (feof(in)) {
      return DATEBOOK_EOF;
   }
   rec_len = header.rec_len;
   ma->rt = header.rt;
   ma->unique_id = header.unique_id;
   record = malloc(rec_len);
   fread(record, rec_len, 1, in);
   if (feof(in)) {
      free(record);
      return DATEBOOK_EOF;
   }
   unpack_Appointment(&(ma->a), record, rec_len);
   free(record);
}

int dateToDays(struct tm *tm1)
{
   time_t t1;

   t1 = mktime(tm1);
   return t1/86400;//There are 86400 secs in a day
}

//returns 0 if times equal
//returns 1 if time1 is greater (later)
//returns 2 if time2 is greater (later)
int compareTimesToSec(struct tm *tm1, struct tm *tm2)
{
   time_t t1, t2;

   t1 =  mktime(tm1);
   t2 =  mktime(tm2);
   if (t1 > t2 ) return 1;
   if (t1 < t2 ) return 2;
   return 0;
}

//returns 0 if times equal
//returns 1 if time1 is greater (later)
//returns 2 if time2 is greater (later)
int compareTimesToDay(struct tm *tm1, struct tm *tm2)
{
   unsigned int t1, t2;
   
   t1 = tm1->tm_year*366+tm1->tm_yday;
   t2 = tm2->tm_year*366+tm2->tm_yday;
   if (t1 > t2 ) return 1;
   if (t1 < t2 ) return 2;
   return 0;
}

unsigned int isApptOnDate(struct Appointment *a, struct tm *date)
{
   unsigned int ret;
   unsigned int r;
   int begin_days, days, week1, week2;
   int dow, ndim;
   int i;
   int days_in_month[]={31,28,31,30,31,30,31,31,30,31,30,31
   };
   //time_t ltime;
   //struct tm *now;

   //time( &ltime );
   //now = localtime( &ltime );

   ret = FALSE;

   //See if the appointment starts after date
   r = compareTimesToDay(&(a->begin), date);
   if (r == 1) {
      return FALSE;
   }
   if (r == 0) {
      ret = TRUE;
   }
   //If the appointment has an end date, see that we are not past it
   if (!(a->repeatForever)) {
      r = compareTimesToDay(&(a->repeatEnd), date);
      if (r == 2) {
	 return FALSE;
      }
   }

   switch (a->repeatType) {
    case repeatNone:
      break;
    case repeatDaily:
      //See if this appt repeats on this day
      begin_days = dateToDays(&(a->begin));
      days = dateToDays(date);
      //g_print("days=%d begin_days=%d\n",days, begin_days);
      ret = (((days - begin_days)%(a->repeatFrequency))==0);
      break;
    case repeatWeekly:
      get_month_info(date->tm_mon, date->tm_mday, date->tm_year, &dow, &ndim);
      //See if the appointment repeats on this day
      if (!(a->repeatDays[dow])) {
	 ret = FALSE;
	 break;
      }
      //See if we are in a week that is repeated in
      begin_days = dateToDays(&(a->begin));
      days = dateToDays(date);
      ret = (((days - begin_days)/7)%(a->repeatFrequency)==0);
      break;
    case repeatMonthlyByDay:
      //See if we are in a month that is repeated in
      ret = (((date->tm_year - a->begin.tm_year)*12 +
       (date->tm_mon - a->begin.tm_mon))%(a->repeatFrequency)==0);
      if (!ret) {
	 break;
      }
      //If the days of the month match - good
      //e.g. the 1st Monday in the month, or last Thur
      if (a->begin.tm_mday != date->tm_mday) {
	 ret = FALSE;
	 break;
      }
      week1 = a->begin.tm_mday/7;
      week2 = date->tm_mday/7;
      if (week1 != week2) {
	 ret = FALSE;
	 break;
      }
      //See if they are both the last date/day in the month
      ret = (((date->tm_mday + 7) > days_in_month[date->tm_mon]) &&
	     (date->tm_wday == a->repeatDay % 7));
      break;
    case repeatMonthlyByDate:
      //See if this is the date that the appt repeats on
      if (date->tm_mday != a->begin.tm_mday) {
	 ret = FALSE;
	 break;
      }
      //See if we are in a repeating month
      ret = (((date->tm_year - a->begin.tm_year)*12 +
       (date->tm_mon - a->begin.tm_mon))%(a->repeatFrequency) == 0);
      if (ret) {
	 break;
      }
      //If appt occurs after the last day of the month and this date
      //is the last day of the month then it occurs today
      ret = ((a->begin.tm_mday > days_in_month[date->tm_mon]) &&
	     (date->tm_mday == days_in_month[date->tm_mon]));
      break;
    case repeatYearly:
      if ((date->tm_year - a->begin.tm_year)%(a->repeatFrequency) != 0) {
	 ret = FALSE;
	 break;
      }
      if ((date->tm_mday == a->begin.tm_mday) &&
	  (date->tm_mon == a->begin.tm_mon)) {
	 ret = TRUE;
	 break;
      }
      //Take care of Feb 29th
      if ((a->begin.tm_mon == 1) && (a->begin.tm_mday == 29) &&
	(a->begin.tm_mon == 1) && (a->begin.tm_mday == 28)) {
	 ret = TRUE;
	 break;
      }   
      break;
    default:
	 g_print("unknown repeatType found in DatebookDB\n");
	 ret = FALSE;
   }//switch

   if (ret) {
      //Check for exceptions
      for (i=0; i<a->exceptions; i++) {
	 //printf("exception %d mon %d\n", i, a->exception[i].tm_mon);
	 //printf("exception %d day %d\n", i, a->exception[i].tm_mday);
	 //printf("exception %d year %d\n", i, a->exception[i].tm_year);
	 //printf("exception %d yday %d\n", i, a->exception[i].tm_yday);
	 //printf("today is yday %d\n", date->tm_yday);
	 begin_days = dateToDays(&(a->exception[i]));
	 days = dateToDays(date);
	 //printf("%d == %d\n", begin_days, days);
	 if (begin_days == days) {
	    ret = FALSE;
	 }
      }
   }
   
   return ret;
}

void free_AppointmentList(AppointmentList *al)
{
   AppointmentList *temp_al, *temp_al_next;

   for (temp_al = al; temp_al; temp_al=temp_al_next) {
      free_Appointment(&(temp_al->ma.a));
      temp_al_next = temp_al->next;
      free(temp_al);
   }
   //al = NULL;
}

//todo returns how many
//al_out will be a NULL terminated linked list
int get_days_appointments(AppointmentList **al_out, struct tm *now)
{
   FILE *in, *pc_in;
   char db_name[34];
   char filler[100];
   char buf[65536];
   unsigned char char_num_records[4];
   int num_records, i, num, r;
   unsigned int offset, next_offset, rec_size;
   unsigned char attrib;
   unsigned char c;
   long fpos;  //file position indicator
   record_header rh;
   struct Appointment *temp_a, a;
   MyAppointment ma;
   AppointmentList *temp_al;
   PCRecType rt;
   char *pc_record;
   int pc_rec_len;
   mem_rec_header *mem_rh;
   mem_rec_header *temp_mem_rh;
   unsigned int unique_id;

   mem_rh = NULL;
   *al_out = NULL;
   
   in = open_file("DatebookDB.pdb", "r");
   if (!in) {
      printf("Error opening DatebookDB.pdb\n");
      return -1;
   }
   //Read the database header
   fread(db_name, 32, 1, in);
   fread(filler, 44, 1, in);
   fread(char_num_records, 2, 1, in);
   if (feof(in)) {
      printf("Error opening DatebookDB.pdb\n");
      return -1;
   }
   //g_print("db_name = %s\n", db_name);
   //g_print("char_num_records = %d %d\n", char_num_records[0], char_num_records[1]);
   //Read each record entry header
   num_records = char_num_records[0]*256 + char_num_records[1];
   //g_print("num_records = %d\n", num_records);
   //g_print("sizeof(record_header)=%d\n",sizeof(record_header));
   for (i=1; i<num_records+1; i++) {
      fread(&rh, sizeof(record_header), 1, in);
//      printf("atrib=%d\n",rh.attrib);
//      printf("uniqueID=%d %d %d\n",rh.unique_ID[0],rh.unique_ID[1],rh.unique_ID[2]);
      offset = ((rh.Offset[0]*256+rh.Offset[1])*256+rh.Offset[2])*256+rh.Offset[3];
//      printf("record header %u offset = %u\n",i, offset);
//      printf("       attrib %d\n",rh.attrib);
//      printf("    unique_ID %d %d %d\n",rh.unique_ID[0],rh.unique_ID[1],rh.unique_ID[2]);
      temp_mem_rh = (mem_rec_header *)malloc(sizeof(mem_rec_header));
      temp_mem_rh->next = mem_rh;
      mem_rh = temp_mem_rh;
      mem_rh->rec_num = i;
      mem_rh->offset = offset;
      mem_rh->attrib = rh.attrib;
      mem_rh->unique_id = (rh.unique_ID[0]*256+rh.unique_ID[1])*256+rh.unique_ID[2];
   }
//   fpos = ftell(in);
//   fpos += 223;
   //fsetpos(in, &fpos);
   //Find the first offset and go there
   find_next_offset(mem_rh, 0, &next_offset, &attrib, &unique_id);
   fseek(in, next_offset, SEEK_SET);
   while(!feof(in)) {
      fpos = ftell(in);
      find_next_offset(mem_rh, fpos, &next_offset, &attrib, &unique_id);
//      next_offset += 223;
      rec_size = next_offset - fpos;
//      printf("rec_size = %u\n",rec_size);
//      printf("fpos,next_offset = %u %u\n",fpos,next_offset);
//      printf("----------\n");
      if (feof(in)) break;
      num = fread(buf, 1, rec_size, in);
      unpack_Appointment(&a, buf, rec_size);

      if (isApptOnDate(&a, now)
	  && (!does_pc_appt_exist(unique_id, PALM_REC))) {
	//if ((a.begin.tm_yday==now->tm_yday) && 
	//  (a.begin.tm_year==now->tm_year)) 
	 temp_al = malloc(sizeof(AppointmentList));
	 memcpy(&(temp_al->ma.a), &a, sizeof(struct Appointment));
	 //temp_al->ma.a = temp_a;
	 temp_al->ma.rt = PALM_REC;
	 temp_al->ma.attrib = attrib;
	 temp_al->ma.unique_id = unique_id;
	 temp_al->next = *al_out;
	 *al_out = temp_al;
      } else {
	 //this doesnt really free it, just the string pointers
	 free_Appointment(&a);
      }
   }
   fclose(in);
   free_mem_rec_header(&mem_rh);

   //
   //Get the appointments out of the PC database
   //
   pc_in = open_file("DatebookDB.pc", "r");
   if (pc_in==NULL) {
      return 0;
   }
   //r = pc_datebook_read_file_header(pc_in);
   while(!feof(pc_in)) {
      r = pc_datebook_read_next_rec(pc_in, &ma);
      if (r==DATEBOOK_EOF) break;
      if ((isApptOnDate(&(ma.a), now))
	  &&(ma.rt!=DELETED_PC_REC)
	  &&(ma.rt!=DELETED_PALM_REC)
	  &&(ma.rt!=DELETED_DELETED_PALM_REC)) {
	 temp_al = malloc(sizeof(AppointmentList));
	 memcpy(&(temp_al->ma), &ma, sizeof(MyAppointment));
	 temp_al->next = *al_out;
	 *al_out = temp_al;
      } else {
	 //this doesnt really free it, just the string pointers
	 free_Appointment(&(ma.a));
      }
      if (ma.rt==DELETED_PALM_REC) {
	 for (temp_al = *al_out; temp_al; temp_al=temp_al->next) {
	    if (temp_al->ma.unique_id == ma.unique_id) {
	       temp_al->ma.rt = ma.rt;
	    }
	 }
      }
   }
   
   datebook_sort(al_out);
   
   fclose(pc_in);
}

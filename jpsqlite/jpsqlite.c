/* J-Pilot plugin for sqlite3
   Write from pdb+pc3 records to single SQLite3 database.
   Works for: Address, Datebook, Memo, ToDo, Expense
   Elmar Klausmeier, 16-Apr-2020
   Elmar Klausmeier, 28-Sep-2021: Changed to GTK3

   License: GPL v2 (GNU General Public License)
   https://www.gnu.org/licenses/old-licenses/gpl-2.0.html

   Build with:
   gcc `pkg-config -cflags-only-I gtk+-3.0` -I <J-Pilot src dir> -s -fPIC -shared jpsqlite.c -o libjpsqlite.so

   Lib "-lsqlite3" no longer required, as it is part of GTK3/Tracker.
*/

#include <stdio.h>
#include <string.h>
#include <sqlite3.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include "log.h"
#include "address.h"
#include "calendar.h"
#include "memo.h"
#include "todo.h"
#include <pi-expense.h>
#include "libplugin.h"
#include "stock_buttons.h"
#include "i18n.h"
#include "libsqlite.h"	// for checking global variable glob_sqlite
#include "prefs.h"



// Get bit 5 of x
#define IS_PRIVATE(x)	((((x) & 0xF0) >> 4) & 0x01)
// Run SQLite command/function x and store error text E
#define CHK(x,E)	if ((sqlRet = x) != SQLITE_OK) { sqlErr=E; goto err; }
#define CHKDONE(x,E)	if ((sqlRet = x) != SQLITE_DONE) { sqlErr=E; goto err; }



static sqlite3 *db;	// file global database pointer
static GtkWidget *copy_sqlite_button;
static int connected = 0;	// Gtk signal handlers


void plugin_version(int *major_version, int *minor_version) {
	*major_version = 1;
	*minor_version = 8;
}



int plugin_get_name(char *name, int len) {
	strncpy(name,"SQLite3",len);
	return EXIT_SUCCESS;
}



int plugin_get_help_name(char *name, int len) {
	g_snprintf(name, len, _("About %s"), _("SQLite3"));
	return EXIT_SUCCESS;
}



int plugin_help(char **text, int *width, int *height) {
	*text = strdup(
		"\n"
		"SQLite3\n\n"
		"Elmar Klausmeier, 28-Sep-2021\n"
		"Elmar Klausmeier, 07-Nov-2022\n"
		"https://eklausmeier.goip.de\n"
		"\n");
	*height = 0;
	*width = 0;
	return EXIT_SUCCESS;
}



int plugin_get_menu_name(char *name, int len) {
	strncpy(name,glob_sqlite ? "SQLite3 disabled" : "SQLite3",len);
	return EXIT_SUCCESS;
}



int plugin_exit_cleanup(void) {
	int sqlRet = sqlite3_close(db);	// noop if db==NULL
	if (sqlRet != SQLITE_OK) {
		jp_logf(JP_LOG_FATAL,"Cannot close sqlite3 file, sqlRet=%d\n",sqlRet);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}



sqlite3 *openSQLite(void) {	// return database handle
	char dbName[FILENAME_MAX];	// path + file-name
	char sqlFile[FILENAME_MAX];
	FILE *fp;
	char *p0;	// points to SQL string in jptables.sql
	struct stat sqlFStat;
	sqlite3 *d = NULL;
	int sqlRet = 0;
	char *sqlErr = "";

	jp_get_home_file_name("jptables.db",dbName,FILENAME_MAX);
	jp_logf(JP_LOG_DEBUG,"openSQLite(): dbName=%s\n",dbName);
	if (sqlite3_open_v2(dbName,&d,SQLITE_OPEN_READWRITE,NULL) == SQLITE_OK)
		return d;

	// Now try to create new jptables.db by using SQL script
	// in $HOME/.jpilot/plugins/jptables.sql
	jp_get_home_file_name("plugins/jptables.sql",sqlFile,FILENAME_MAX);
	jp_logf(JP_LOG_DEBUG,"openSQLite(): sqlFile=%s\n",sqlFile);
	if ( stat(sqlFile,&sqlFStat) ) {
		jp_logf(JP_LOG_FATAL,
			"Cannot stat SQL file jptables.sql in plugins directory: %s\n",
			strerror(errno));
		return NULL;
	}
	if (sqlFStat.st_size == 0) {
		jp_logf(JP_LOG_FATAL,"SQL file jptables.sql has no content\n");
		return NULL;
	}
	if ((fp = fopen(sqlFile,"r")) == NULL) {
		jp_logf(JP_LOG_FATAL,
			"Cannot open SQL file jptables.sql in plugins directory: %s\n",
			strerror(errno));
		return NULL;
	}
	jp_logf(JP_LOG_DEBUG,"openSQLite(): jptables.sql size=%ld\n",
		sqlFStat.st_size);
	if ((p0 = malloc(sqlFStat.st_size + 4)) == NULL) {
		jp_logf(JP_LOG_FATAL,
			"Cannot cache jptables.sql: %s\n",
			strerror(errno));
		return NULL;
	}
	if (fread(p0,1,sqlFStat.st_size,fp) != sqlFStat.st_size) {
		jp_logf(JP_LOG_FATAL,
			"Cannot read jptables.sql: %s\n",
			strerror(errno));
		return NULL;
	}
	p0[sqlFStat.st_size] = '\0';
	// Open and create db
	jp_logf(JP_LOG_GUI,"\nCreating new SQLite3 database file %s. This may take approx. 20 seconds.\n\n",dbName);
	CHK(sqlite3_open(dbName,&d),"IOP")
	// Feed entire file content to SQLite3
	CHK(sqlite3_exec(d,p0,NULL,NULL,NULL),"IEXC");

	free(p0);
	if (fclose(fp)) {
		jp_logf(JP_LOG_FATAL,
			"Cannot close jptables.sql: %s\n", strerror(errno));
		return NULL;
	}
	jp_logf(JP_LOG_GUI,"SQLite3 database created successfully.\n");
	return d;

err:
	jp_logf(JP_LOG_FATAL,"openSQLite(): SQLite3 ret=%d, error=%s, rolling back\n%s\n",
		sqlRet, sqlErr, sqlite3_errmsg(d));
	return NULL;
}



int plugin_search(const char *search_string, int case_sense, struct search_result **sr) {
	sqlite3_stmt *dbpstmt = NULL;
	char *srch;	// = % + search_string + %
	struct search_result *prev_sr = NULL;
	size_t len;
	int sqlRet = 0;
	char *sqlErr = "";
	int nrecs;

	// Search only if in debug-mode
	if ( ! (glob_log_file_mask & JP_LOG_DEBUG) ) return 0;
	jp_logf(JP_LOG_DEBUG,"\nIn plugin_search(): case_sense=%d, search_string=|%s|\n",
		case_sense, search_string);
	if (search_string[0] == '\0') return 0;
	len = strlen(search_string);

	if ((srch = malloc(len+4)) == NULL) return 0;
	srch[0] = '%';	// start concating
	strcpy(srch+1,search_string);
	srch[len+1] = '%';
	srch[len+2] = '\0';

	if (db == NULL  &&  (db = openSQLite()) == NULL) {
		free(srch);
		return 0;
	}

	// SQLite3 LIKE is case insensitive by default
	if (case_sense) {
		CHK(sqlite3_exec(db,"PRAGMA case_sensitive_like=true",NULL,NULL,NULL),"SCT")
	} else {
		CHK(sqlite3_exec(db,"PRAGMA case_sensitive_like=false",NULL,NULL,NULL),"SCF")
	}

	CHK(sqlite3_prepare_v2(db,
		"select Id, substr(coalesce(Lastname,Firstname,''),1,80) as Line "
		"from Addr "
		"where Lastname like :srch "
		"or Firstname like :srch "
		"or Phone1 like :srch "
		"or Phone2 like :srch "
		"or Phone3 like :srch "
		"or Phone4 like :srch "
		"or Phone5 like :srch "
		"or Address like :srch "
		"or City like :srch "
		"or State like :srch "
		"or Zip like :srch "
		"or Custom1 like :srch "
		"or Custom2 like :srch "
		"or Custom3 like :srch "
		"or Custom4 like :srch "
		"or Note like :srch "
		"union "
		"select Id, substr(Begin || '  ' || coalesce(Description,''),1,80) as Line "
		"from Datebook "
		"where Description like :srch "
		"or Note like :srch "
		"union "
		"select Id, substr(Text,1,80) as Line "
		"from Memo "
		"where Text like :srch "
		"union "
		"select Id, substr(Description,1,80) as Line "
		"from ToDo "
		"where Description like :srch "
		"or Note like :srch "
		"union "
		"select Id, substr(coalesce(Date,'') || ' ' "
		"    || coalesce(Amount,'') || coalesce(Vendor,'') "
		"    || coalesce(Note,''),1,80) as Line "
		"from Expense "
		"where Amount like :srch "
		"or Vendor like :srch "
		"or City like :srch "
		"or Attendees like :srch "
		"or Note like :srch "
		, -1, &dbpstmt, NULL), "SPRE")
	CHK(sqlite3_bind_text(dbpstmt,1,srch,-1,SQLITE_STATIC),"SB1")
	sqlErr = "SST";
	for (nrecs=0; nrecs < 400; ++nrecs) {
		sqlRet = sqlite3_step(dbpstmt);
		if (sqlRet == SQLITE_DONE) break;
		else if (sqlRet != SQLITE_ROW) goto err;
		if ((*sr = malloc(sizeof(struct search_result))) == NULL)
			goto err;
		(*sr)->unique_id = sqlite3_column_int(dbpstmt,0);
		(*sr)->line = strdup((const char*)sqlite3_column_text(dbpstmt,1));
		(*sr)->next = prev_sr;
		prev_sr = *sr;
		jp_logf(JP_LOG_DEBUG,"\n\t%d: %s\n",
			(*sr)->unique_id, (*sr)->line);
	}

	CHK(sqlite3_finalize(dbpstmt),"SFIN")
	free(srch);
	return nrecs;

err:
	free(srch);
	jp_logf(JP_LOG_FATAL,"plugin_search(): SQLite3 ret=%d, error=%s, cannot search\n%s\n",
		sqlRet, sqlErr, sqlite3_errmsg(db));
	sqlite3_finalize(dbpstmt);
	return 0;
}



//int plugin_pre_sync_pre_connect(void)
static int copy_sqlite(void) {
	struct AddressAppInfo ai;
	AddressList *a;
	CalendarEventList *c;
	char begin[32], end[32], repeatEnd[32], *pRepeatEnd, due[32], date[32], exceptionString[2048];
	struct MemoAppInfo mi;
	MemoList *m;
	struct ToDoAppInfo tdi;
	ToDoList *td;
	struct ExpenseAppInfo ei;
	struct Expense e;
	unsigned char *buf;
	int buf_size;
	GList *explst;
	buf_rec *br;
	int nrecsA=0, nrecsAL=0, nrecsAC=0, nrecsPL=0, nrecsD=0;
	int nrecsT=0, nrecsTC=0, nrecsM=0, nrecsMC=0, nrecsE=0, nrecsEC=0;
	int i, r, total_records, offset, repeatForever;
	sqlite3_stmt *dbpstmt = NULL;
	int sqlRet = 0, errId;
	char *sqlErr = "";


	if (db == NULL  &&  (db = openSQLite()) == NULL)
		return EXIT_FAILURE;
	errId = -1;
	CHK(sqlite3_exec(db,"BEGIN TRANSACTION",NULL,NULL,NULL),"BEGIN")
	CHK(sqlite3_exec(db,
		"delete from Addr; "
		"delete from AddrLabel; "
		"delete from AddrCategory; "
		"delete from PhoneLabel; "
		"delete from Datebook; "
		"delete from ToDo; "
		"delete from ToDoCategory; "
		"delete from Memo; "
		"delete from MemoCategory; "
		"delete from Expense; "
		"delete from ExpenseCategory; "
		"delete from Pref; "
		, NULL, NULL, NULL), "DEALL")


	// Address
	jp_logf(JP_LOG_DEBUG,"get_address_app_info()\n");
	if ((r = get_address_app_info(&ai)) != 0) {
		jp_logf(JP_LOG_DEBUG,"get_address_app_info() returned %d\n",r);
		return EXIT_FAILURE;
	}
	jp_logf(JP_LOG_DEBUG,"\taddress labels\n");
	errId = -1;
	CHK(sqlite3_prepare_v2(db,
		"insert into AddrLabel (Id, Label) "
		"values (:Id, :Label)", -1, &dbpstmt, NULL), "ALPRE")
	for (i=0; i<19; ++i) {
		if (ai.labels[i][0] == '\0') continue;
		jp_logf(JP_LOG_DEBUG,"\t\t%2d %s\n",i,ai.labels[i]);
		++nrecsAL;
		errId = i;
		CHK(sqlite3_bind_int(dbpstmt,1,i),"ALB1")
		CHK(sqlite3_bind_text(dbpstmt,2,ai.labels[i],-1,SQLITE_STATIC),"ALB2")
		CHKDONE(sqlite3_step(dbpstmt),"ALST")
		CHK(sqlite3_clear_bindings(dbpstmt),"ALCL")
		CHK(sqlite3_reset(dbpstmt),"ALRST")
	}
	CHK(sqlite3_finalize(dbpstmt),"ALFIN")

	errId = -1;
	CHK(sqlite3_prepare_v2(db,
		"insert into AddrCategory (Id, Label, InsertDate) "
		"values (:Id, :Label, strftime('%Y-%m-%dT%H:%M:%S', 'now'))",
		-1, &dbpstmt, NULL), "ACPRE")
	jp_logf(JP_LOG_DEBUG,"\tAddress categories\n");
	for (i=0; i<16; ++i) {
		if (ai.category.name[i][0] == '\0') continue;
		jp_logf(JP_LOG_DEBUG,"\t\t%2d %s\n",i,ai.category.name[i]);
		++nrecsAC;
		errId = i;
		CHK(sqlite3_bind_int(dbpstmt,1,i),"ACB1")
		CHK(sqlite3_bind_text(dbpstmt,2,ai.category.name[i],-1,SQLITE_STATIC),"ACB2")
		CHKDONE(sqlite3_step(dbpstmt),"ACST")
		CHK(sqlite3_clear_bindings(dbpstmt),"ACCL")
		CHK(sqlite3_reset(dbpstmt),"ACRST")
	}
	CHK(sqlite3_finalize(dbpstmt),"ACFIN")

	errId = -1;
	CHK(sqlite3_prepare_v2(db,
		"insert into PhoneLabel (Id, Label) "
		"values (:Id, :Label)", -1, &dbpstmt, NULL), "PLPRE")
	for (i=0; i<8; ++i) {
		if (ai.phoneLabels[i][0] == '\0') continue;
		jp_logf(JP_LOG_DEBUG,"\t\t%2d %s\n",i,ai.phoneLabels[i]);
		++nrecsPL;
		errId = i;
		CHK(sqlite3_bind_int(dbpstmt,1,i),"PLB1")
		CHK(sqlite3_bind_text(dbpstmt,2,ai.phoneLabels[i],-1,SQLITE_STATIC),"PLB2")
		CHKDONE(sqlite3_step(dbpstmt),"PLST")
		CHK(sqlite3_clear_bindings(dbpstmt),"PLCL")
		CHK(sqlite3_reset(dbpstmt),"PLRST")
	}
	CHK(sqlite3_finalize(dbpstmt),"PLFIN")

	r = get_addresses(&a, SORT_ASCENDING);
	jp_logf(JP_LOG_DEBUG,"get_addresses() returned %d records\n", r);
	errId = -1;
	CHK(sqlite3_prepare_v2(db,
		"insert into Addr ("
		"    Id, Category, Private, showPhone,"	// 1
		"    Lastname, Firstname, Title, Company,"	// 5
		"    PhoneLabel1, PhoneLabel2, PhoneLabel3,"	// 9
		"    PhoneLabel4, PhoneLabel5,"		// 12
		"    Phone1, Phone2, Phone3, Phone4, Phone5,"	// 14
		"    Address, City, State, Zip, Country,"	// 19
		"    Custom1, Custom2, Custom3, Custom4,"	// 24
		"    Note, InsertDate"					// 28
		") values ("
		"    :Id, :Category, :Private, :showPhone,"
		"    :Lastname, :Firstname, :Title, :Company,"
		"    :PhoneLabel1, :PhoneLabel2, :PhoneLabel3,"
		"    :PhoneLabel4, :PhoneLabel5,"
		"    :Phone1, :Phone2, :Phone3, :Phone4, :Phone5,"
		"    :Address, :City, :State, :Zip, :Country,"
		"    :Custom1, :Custom2, :Custom3, :Custom4,"
		"    :Note, strftime('%Y-%m-%dT%H:%M:%S', 'now')"
		")",
		-1, &dbpstmt, NULL), "APRE")

	for (; a; a=a->next) {
		if (a->maddr.unique_id == 1693099) {
			jp_logf(JP_LOG_DEBUG,"\t%d, %d: %s\n",
				a->maddr.unique_id,
				a->maddr.rt,
				a->maddr.addr.entry[entryLastname]);
		}
		//if (nrecsA < 10)
			jp_logf(JP_LOG_DEBUG,"\t%d, %9u, rt=%d, %s: %s, %s\n",
				a->app_type,
				a->maddr.unique_id,
				a->maddr.rt,
				ai.category.name[a->maddr.attrib & 0x0F],
				a->maddr.addr.entry[entryLastname],
				a->maddr.addr.entry[entryFirstname]);
		if (a->maddr.rt != PALM_REC
		&&  a->maddr.rt != NEW_PC_REC
		&&  a->maddr.rt != REPLACEMENT_PALM_REC)
			continue;
		++nrecsA;
		errId = a->maddr.unique_id;
		CHK(sqlite3_bind_int(dbpstmt,1,a->maddr.unique_id),"AB1")
		CHK(sqlite3_bind_int(dbpstmt,2,a->maddr.attrib & 0x0F),"AB2")
		CHK(sqlite3_bind_int(dbpstmt,3,IS_PRIVATE(a->maddr.attrib)),"AB3")
		CHK(sqlite3_bind_int(dbpstmt,4,a->maddr.addr.showPhone),"AB4")
		CHK(sqlite3_bind_text(dbpstmt,5,a->maddr.addr.entry[entryLastname],-1,SQLITE_STATIC),"AB5")
		CHK(sqlite3_bind_text(dbpstmt,6,a->maddr.addr.entry[entryFirstname],-1,SQLITE_STATIC),"AB6")
		CHK(sqlite3_bind_text(dbpstmt,7,a->maddr.addr.entry[entryTitle],-1,SQLITE_STATIC),"AB7")
		CHK(sqlite3_bind_text(dbpstmt,8,a->maddr.addr.entry[entryCompany],-1,SQLITE_STATIC),"AB8")
		CHK(sqlite3_bind_int(dbpstmt,9,a->maddr.addr.phoneLabel[0]),"AB9")
		CHK(sqlite3_bind_int(dbpstmt,10,a->maddr.addr.phoneLabel[1]),"AB10")
		CHK(sqlite3_bind_int(dbpstmt,11,a->maddr.addr.phoneLabel[2]),"AB11")
		CHK(sqlite3_bind_int(dbpstmt,12,a->maddr.addr.phoneLabel[3]),"AB12")
		CHK(sqlite3_bind_int(dbpstmt,13,a->maddr.addr.phoneLabel[4]),"AB13")
		CHK(sqlite3_bind_text(dbpstmt,14,a->maddr.addr.entry[entryPhone1],-1,SQLITE_STATIC),"AB14")
		CHK(sqlite3_bind_text(dbpstmt,15,a->maddr.addr.entry[entryPhone2],-1,SQLITE_STATIC),"AB15")
		CHK(sqlite3_bind_text(dbpstmt,16,a->maddr.addr.entry[entryPhone3],-1,SQLITE_STATIC),"AB16")
		CHK(sqlite3_bind_text(dbpstmt,17,a->maddr.addr.entry[entryPhone4],-1,SQLITE_STATIC),"AB17")
		CHK(sqlite3_bind_text(dbpstmt,18,a->maddr.addr.entry[entryPhone5],-1,SQLITE_STATIC),"AB18")
		CHK(sqlite3_bind_text(dbpstmt,19,a->maddr.addr.entry[entryAddress],-1,SQLITE_STATIC),"AB19")
		CHK(sqlite3_bind_text(dbpstmt,20,a->maddr.addr.entry[entryCity],-1,SQLITE_STATIC),"AB20")
		CHK(sqlite3_bind_text(dbpstmt,21,a->maddr.addr.entry[entryState],-1,SQLITE_STATIC),"AB21")
		CHK(sqlite3_bind_text(dbpstmt,22,a->maddr.addr.entry[entryZip],-1,SQLITE_STATIC),"AB22")
		CHK(sqlite3_bind_text(dbpstmt,23,a->maddr.addr.entry[entryCountry],-1,SQLITE_STATIC),"AB23")
		CHK(sqlite3_bind_text(dbpstmt,24,a->maddr.addr.entry[entryCustom1],-1,SQLITE_STATIC),"AB24")
		CHK(sqlite3_bind_text(dbpstmt,25,a->maddr.addr.entry[entryCustom2],-1,SQLITE_STATIC),"AB25")
		CHK(sqlite3_bind_text(dbpstmt,26,a->maddr.addr.entry[entryCustom3],-1,SQLITE_STATIC),"AB26")
		CHK(sqlite3_bind_text(dbpstmt,27,a->maddr.addr.entry[entryCustom4],-1,SQLITE_STATIC),"AB27")
		CHK(sqlite3_bind_text(dbpstmt,28,a->maddr.addr.entry[entryNote],-1,SQLITE_STATIC),"AB28")
		CHKDONE(sqlite3_step(dbpstmt),"AST")
		CHK(sqlite3_clear_bindings(dbpstmt),"ACL")
		CHK(sqlite3_reset(dbpstmt),"ARST")
		if (nrecsA > 29500) break;	// runaway?
	}
	CHK(sqlite3_finalize(dbpstmt),"AFIN")
	jp_logf(JP_LOG_DEBUG,"%d address records in list\n", nrecsA);


	// Datebook
	r = get_days_calendar_events(&c, NULL, CATEGORY_ALL, &total_records);
	jp_logf(JP_LOG_DEBUG,"get_days_calendar_events() returned %d\n", r);
	errId = -1;
	CHK(sqlite3_prepare_v2(db,
		"insert into Datebook ("
		"    Id, Private, Timeless, Begin, End,"	// 1
		"    Alarm, Advance, AdvanceUnit, RepeatType,"	// 6
		"    RepeatForever, RepeatEnd, RepeatFreq,"	// 10
		"    RepeatDay,"				// 13
		"    RepeatDaySu, RepeatDayMo, RepeatDayTu,"	// 14
		"    RepeatDayWe, RepeatDayTh, RepeatDayFr,"	// 17
		"    RepeatDaySa, Exceptions, Exception,"		// 20
		"    Description, Note, InsertDate"		// 23
		") values ("
		"    :Id, :Private, :Timeless, :Begin, :End,"
		"    :Alarm, :Advance, :AdvanceUnit, :RepeatType,"
		"    :RepeatForever, :RepeatEnd, :RepeatFreq,"
		"    :RepeatDay,"
		"    :RepeatDaySu, :RepeatDayMo, :RepeatDayTu,"
		"    :RepeatDayWe, :RepeatDayTh, :RepeatDayFr,"
		"    :RepeatDaySa, :Exceptions, :Exception,"
		"    :Description, :Note, strftime('%Y-%m-%dT%H:%M:%S', 'now')"
		")",
		-1, &dbpstmt, NULL), "DPRE")
	for (; c; c=c->next) {
		if (nrecsD < 10)
			jp_logf(JP_LOG_DEBUG,"\t%d rt=%d %s <<%s>>\n",
				c->mcale.unique_id,
				c->mcale.rt,
				c->mcale.cale.description,
				c->mcale.cale.note);
		if (c->mcale.rt != PALM_REC
		&&  c->mcale.rt != NEW_PC_REC
		&&  c->mcale.rt != REPLACEMENT_PALM_REC)
			continue;
		++nrecsD;
		errId = c->mcale.unique_id;
		strftime(begin,32,"%FT%R",&c->mcale.cale.begin);
		strftime(end,32,"%FT%R",&c->mcale.cale.end);
		// Data cleansing
		if (c->mcale.cale.repeatEnd.tm_year < 2
		||  c->mcale.cale.repeatEnd.tm_year > 2050)
			pRepeatEnd = NULL;
		else
			strftime(pRepeatEnd=repeatEnd,32,"%F",&c->mcale.cale.repeatEnd);
		CHK(sqlite3_bind_int(dbpstmt,1,c->mcale.unique_id),"DB1")
		CHK(sqlite3_bind_int(dbpstmt,2,IS_PRIVATE(c->mcale.attrib)),"DB2")
		CHK(sqlite3_bind_int(dbpstmt,3,c->mcale.cale.event),"DB3")
		CHK(sqlite3_bind_text(dbpstmt,4,begin,-1,SQLITE_STATIC),"DB4")
		CHK(sqlite3_bind_text(dbpstmt,5,end,-1,SQLITE_STATIC),"DB5")
		CHK(sqlite3_bind_int(dbpstmt,6,c->mcale.cale.alarm),"DB6")
		CHK(sqlite3_bind_int(dbpstmt,7,c->mcale.cale.advance),"DB7")
		CHK(sqlite3_bind_int(dbpstmt,8,c->mcale.cale.advanceUnits),"DB8")
		CHK(sqlite3_bind_int(dbpstmt,9,c->mcale.cale.repeatType),"DB9")
		// 1 - (c->mcale.cale.repeatForever & 0x01)
		repeatForever = c->mcale.cale.repeatForever & 0x01;
		if (c->mcale.cale.repeatFrequency == 0 && c->mcale.cale.repeatDay == 0 && c->mcale.cale.repeatDays[0] == 0
		&& c->mcale.cale.repeatDays[1] == 0 && c->mcale.cale.repeatDays[2] == 0 && c->mcale.cale.repeatDays[3] == 0
		&& c->mcale.cale.repeatDays[4] == 0 && c->mcale.cale.repeatDays[5] == 0 && c->mcale.cale.repeatDays[6] == 0
		&& pRepeatEnd == NULL)
			repeatForever = 1;
		CHK(sqlite3_bind_int(dbpstmt,10,repeatForever),"DB10")
		CHK(sqlite3_bind_text(dbpstmt,11,pRepeatEnd,-1,SQLITE_STATIC),"DB11")
		CHK(sqlite3_bind_int(dbpstmt,12,c->mcale.cale.repeatFrequency),"DB12")
		CHK(sqlite3_bind_int(dbpstmt,13,c->mcale.cale.repeatDay),"DB13")
		CHK(sqlite3_bind_int(dbpstmt,14,c->mcale.cale.repeatDays[0]),"DB14")
		CHK(sqlite3_bind_int(dbpstmt,15,c->mcale.cale.repeatDays[1]),"DB15")
		CHK(sqlite3_bind_int(dbpstmt,16,c->mcale.cale.repeatDays[2]),"DB16")
		CHK(sqlite3_bind_int(dbpstmt,17,c->mcale.cale.repeatDays[3]),"DB17")
		CHK(sqlite3_bind_int(dbpstmt,18,c->mcale.cale.repeatDays[4]),"DB18")
		CHK(sqlite3_bind_int(dbpstmt,19,c->mcale.cale.repeatDays[5]),"DB19")
		CHK(sqlite3_bind_int(dbpstmt,20,c->mcale.cale.repeatDays[6]),"DB20")
		CHK(sqlite3_bind_int(dbpstmt,21,c->mcale.cale.exceptions),"DB21")
		if (c->mcale.cale.exceptions * 11 > sizeof(exceptionString)) {
			strcpy(exceptionString,"Too many exceptions");
		} else {
			for (i=0,offset=0; i<c->mcale.cale.exceptions; ++i,offset+=11) {
				strftime(exceptionString+offset,11,"%F",c->mcale.cale.exception+i);
				if (i > 0) exceptionString[offset-1] = ' ';	// change previous '\0' to space
			}
		}
		CHK(sqlite3_bind_text(dbpstmt,22,c->mcale.cale.exceptions ? exceptionString : NULL,-1,SQLITE_STATIC),"DB22")
		CHK(sqlite3_bind_text(dbpstmt,23,c->mcale.cale.description,-1,SQLITE_STATIC),"DB23")
		CHK(sqlite3_bind_text(dbpstmt,24,c->mcale.cale.note,-1,SQLITE_STATIC),"DB24")
		CHKDONE(sqlite3_step(dbpstmt),"DST")
		CHK(sqlite3_clear_bindings(dbpstmt),"DCL")
		CHK(sqlite3_reset(dbpstmt),"DRST")
		//if (nrecsD > 350000) break;	// safety check
	}
	CHK(sqlite3_finalize(dbpstmt),"DFIN")
	jp_logf(JP_LOG_DEBUG,"%d calendar records in list\n", nrecsD);


	// Memo
	if ((r = get_memo_app_info(&mi)) != 0) {
		jp_logf(JP_LOG_DEBUG,"get_memo_app_info() returned %d\n",r);
		return EXIT_FAILURE;
	}
	CHK(sqlite3_prepare_v2(db,
		"insert into MemoCategory (Id, Label, InsertDate) "
		"values (:Id, :Label, strftime('%Y-%m-%dT%H:%M:%S', 'now'))",
		-1, &dbpstmt, NULL), "MCPRE")
	for (i=0; i<16; ++i) {
		if (mi.category.name[i][0] == '\0') continue;
		jp_logf(JP_LOG_DEBUG,"\t\t%2d %s\n",i,mi.category.name[i]);
		++nrecsMC;
		errId = i;
		CHK(sqlite3_bind_int(dbpstmt,1,i),"MCB1")
		CHK(sqlite3_bind_text(dbpstmt,2,mi.category.name[i],-1,SQLITE_STATIC),"MCB2")
		CHKDONE(sqlite3_step(dbpstmt),"MCST")
		CHK(sqlite3_clear_bindings(dbpstmt),"MCCL")
		CHK(sqlite3_reset(dbpstmt),"MCRST")
	}
	CHK(sqlite3_finalize(dbpstmt),"MCFIN")

	errId = -1;
	CHK(sqlite3_prepare_v2(db,
		"insert into Memo ("
		"    Id, Category, Private, Text, InsertDate"
		") values ("
		"    :Id, :Category, :Private, :Text, strftime('%Y-%m-%dT%H:%M:%S', 'now')"
		")",
		-1, &dbpstmt, NULL), "MPRE")
	r = get_memos(&m,SORT_ASCENDING);	//,0,0,1,CATEGORY_ALL);
	jp_logf(JP_LOG_DEBUG,"get_memos() returned %d\n", r);
	for (; m; m=m->next) {
		/* * *
		if (nrecsM < 10)
			jp_logf(JP_LOG_DEBUG,"\t%d rt=%d (%s) %.60s\n",
				m->mmemo.unique_id,
				m->mmemo.rt,
				mi.category.name[m->mmemo.attrib & 0x0F],
				m->mmemo.memo.text);
		* * */
		if (m->mmemo.rt != PALM_REC
		&&  m->mmemo.rt != NEW_PC_REC
		&&  m->mmemo.rt != REPLACEMENT_PALM_REC)
			continue;
		++nrecsM;
		errId = m->mmemo.unique_id;
		CHK(sqlite3_bind_int(dbpstmt,1,m->mmemo.unique_id),"MB1")
		CHK(sqlite3_bind_int(dbpstmt,2,m->mmemo.attrib & 0x0F),"MB2")
		CHK(sqlite3_bind_int(dbpstmt,3,IS_PRIVATE(m->mmemo.attrib)),"MB3")
		CHK(sqlite3_bind_text(dbpstmt,4,m->mmemo.memo.text,-1,SQLITE_STATIC),"MB4")
		CHKDONE(sqlite3_step(dbpstmt),"MST")
		CHK(sqlite3_clear_bindings(dbpstmt),"MCL")
		CHK(sqlite3_reset(dbpstmt),"MRST")
		//if (nrecsM > 29500) break;	// runaway?
	}
	CHK(sqlite3_finalize(dbpstmt),"MFIN")
	jp_logf(JP_LOG_DEBUG,"%d memo records in list\n", nrecsM);


	// ToDo
	if ((r = get_todo_app_info(&tdi)) != 0) {
		jp_logf(JP_LOG_DEBUG,"get_todo_app_info() returned %d\n",r);
		return EXIT_FAILURE;
	}
	errId = -1;
	CHK(sqlite3_prepare_v2(db,
		"insert into ToDoCategory (Id, Label, InsertDate) "
		"values (:Id, :Label, strftime('%Y-%m-%dT%H:%M:%S', 'now'))",
		-1, &dbpstmt, NULL),"TCPRE")
	for (i=0; i<16; ++i) {
		if (tdi.category.name[i][0] == '\0') continue;
		jp_logf(JP_LOG_DEBUG,"\t\t%2d %s\n",i,tdi.category.name[i]);
		++nrecsTC;
		errId = i;
		CHK(sqlite3_bind_int(dbpstmt,1,i),"TCB1")
		CHK(sqlite3_bind_text(dbpstmt,2,tdi.category.name[i],-1,SQLITE_STATIC),"TCB2")
		CHKDONE(sqlite3_step(dbpstmt),"TCST")
		CHK(sqlite3_clear_bindings(dbpstmt),"TCCL")
		CHK(sqlite3_reset(dbpstmt),"TCRST")
	}
	CHK(sqlite3_finalize(dbpstmt),"TCFIN")

	errId = -1;
	CHK(sqlite3_prepare_v2(db,
		"insert into ToDo ("
		"    Id, Category, Private, Indefinite,"	// 1
		"    Due, Priority, Complete,"			// 5
		"    Description, Note, InsertDate"		// 8
		") values ("
		"    :Id, :Category, :Private, :Indefinite, "
		"    :Due, :Priority, :Complete, "
		"    :Description, :Note, strftime('%Y-%m-%dT%H:%M:%S', 'now')"
		")",
		-1, &dbpstmt, NULL),"TPRE")
	r = get_todos(&td,SORT_ASCENDING);
	jp_logf(JP_LOG_DEBUG,"get_todos() returned %d\n", r);
	for (; td; td=td->next) {
		if (nrecsT < 10)
			jp_logf(JP_LOG_DEBUG,"\t%d rt=%d (%s) %.60s\n",
				td->mtodo.unique_id,
				td->mtodo.rt,
				tdi.category.name[td->mtodo.attrib & 0x0F],
				td->mtodo.todo.description);
		if (td->mtodo.rt != PALM_REC
		&&  td->mtodo.rt != NEW_PC_REC
		&&  td->mtodo.rt != REPLACEMENT_PALM_REC)
			continue;
		++nrecsT;
		errId = td->mtodo.unique_id;
		strftime(due,32,"%F",&td->mtodo.todo.due);
		CHK(sqlite3_bind_int(dbpstmt,1,td->mtodo.unique_id),"TB1")
		CHK(sqlite3_bind_int(dbpstmt,2,td->mtodo.attrib & 0x0F),"TB2")
		CHK(sqlite3_bind_int(dbpstmt,3,IS_PRIVATE(td->mtodo.attrib)),"TB3")
		CHK(sqlite3_bind_int(dbpstmt,4,td->mtodo.todo.indefinite),"TB4")
		CHK(sqlite3_bind_text(dbpstmt,5,due,-1,SQLITE_STATIC),"TB5")
		CHK(sqlite3_bind_int(dbpstmt,6,td->mtodo.todo.priority),"TB6")
		CHK(sqlite3_bind_int(dbpstmt,7,td->mtodo.todo.complete),"TB7")
		CHK(sqlite3_bind_text(dbpstmt,8,td->mtodo.todo.description,-1,SQLITE_STATIC),"TB8")
		CHK(sqlite3_bind_text(dbpstmt,9,td->mtodo.todo.note,-1,SQLITE_STATIC),"TB9")
		CHKDONE(sqlite3_step(dbpstmt),"TST")
		CHK(sqlite3_clear_bindings(dbpstmt),"TCL")
		CHK(sqlite3_reset(dbpstmt),"TRST")
		//if (nrecsT > 29500) break;	// runaway?
	}
	CHK(sqlite3_finalize(dbpstmt),"TFIN")
	jp_logf(JP_LOG_DEBUG,"%d to-do records in list\n", nrecsT);


	// Expense
	// Below modeled after make_menus() in Expense/expense.c
	jp_get_app_info("ExpenseDB", &buf, &buf_size);
	unpack_ExpenseAppInfo(&ei, buf, buf_size);
	if (buf) free(buf);
	errId = -1;
	CHK(sqlite3_prepare_v2(db,
		"insert into ExpenseCategory (Id, Label,InsertDate) "
		"values (:Id, :Label, strftime('%Y-%m-%dT%H:%M:%S', 'now'))",
		-1, &dbpstmt, NULL), "ECPRE")
	jp_logf(JP_LOG_DEBUG,"\tExpense categories\n");
	for (i=0; i<16; ++i) {
		if (ei.category.name[i][0] == '\0') continue;
		jp_logf(JP_LOG_DEBUG,"\t\t%2d %s\n",i,ei.category.name[i]);
		++nrecsEC;
		errId = i;
		CHK(sqlite3_bind_int(dbpstmt,1,i),"ExpCatB1")
		CHK(sqlite3_bind_text(dbpstmt,2,ei.category.name[i],-1,SQLITE_STATIC),"ExpCatB2")
		CHKDONE(sqlite3_step(dbpstmt),"ExpCatST")
		CHK(sqlite3_clear_bindings(dbpstmt),"ExpCatCL")
		CHK(sqlite3_reset(dbpstmt),"ExpCatRST")
	}
	CHK(sqlite3_finalize(dbpstmt),"ExpCatFIN")

	// Below modeled after display_records() in Expense/expense.c
	r = jp_read_DB_files("ExpenseDB", &explst);
	jp_logf(JP_LOG_DEBUG,"jp_read_DB_file() for ExpenseDB returned %d\n", r);
	errId = -1;
	CHK(sqlite3_prepare_v2(db,
		"insert into Expense ("
		"    Id, Category, Date, Type, Payment,"	// 1
		"    Currency, Amount, Vendor, City,"		// 6
		"    Attendees, Note, InsertDate"			// 10
		") values ("
		"    :Id, :Category, :Date, :Type, :Payment, "
		"    :Currency, :Amount, :Vendor, :City, "
		"    :Attendees, :Note, strftime('%Y-%m-%dT%H:%M:%S', 'now')"
		")",
		-1, &dbpstmt, NULL), "ExpPRE")
	for (; explst; explst=explst->next) {
		if ((br = explst->data) == NULL) continue;
		if (br->rt != PALM_REC
		&&  br->rt != NEW_PC_REC
		&&  br->rt != REPLACEMENT_PALM_REC)
			continue;
		unpack_Expense(&e,br->buf,br->size);
		if (nrecsE < 10)
			jp_logf(JP_LOG_DEBUG,"\t%d rt=%d (%s) %.60s\n",
				br->unique_id,
				br->rt,
				ei.category.name[br->attrib & 0x0F],
				e.vendor);
		++nrecsE;
		errId = br->unique_id;
		strftime(date,32,"%F",&(e.date));
		CHK(sqlite3_bind_int(dbpstmt,1,br->unique_id),"ExpB1")
		CHK(sqlite3_bind_int(dbpstmt,2,br->attrib & 0x0F),"ExpB2")
		CHK(sqlite3_bind_text(dbpstmt,3,date,-1,SQLITE_STATIC),"EB5")
		CHK(sqlite3_bind_int(dbpstmt,4,e.type),"ExpB4")
		CHK(sqlite3_bind_int(dbpstmt,5,e.payment),"ExpB5")
		CHK(sqlite3_bind_int(dbpstmt,6,e.currency),"ExpB6")
		CHK(sqlite3_bind_text(dbpstmt,7,e.amount,-1,SQLITE_STATIC),"ExpB7")
		CHK(sqlite3_bind_text(dbpstmt,8,e.vendor,-1,SQLITE_STATIC),"ExpB8")
		CHK(sqlite3_bind_text(dbpstmt,9,e.city,-1,SQLITE_STATIC),"ExpB9")
		CHK(sqlite3_bind_text(dbpstmt,10,e.attendees,-1,SQLITE_STATIC),"ExpB10")
		CHK(sqlite3_bind_text(dbpstmt,11,e.note,-1,SQLITE_STATIC),"ExpB11")
		CHKDONE(sqlite3_step(dbpstmt),"ExpST")
		CHK(sqlite3_clear_bindings(dbpstmt),"ExpCL")
		CHK(sqlite3_reset(dbpstmt),"ExpRST")
		//if (nrecsE > 29500) break;	// runaway?
	}
	CHK(sqlite3_finalize(dbpstmt),"ExpFIN")
	jp_logf(JP_LOG_DEBUG,"%d expense records in list\n", nrecsE);
	jp_free_DB_records(&explst);

	// Preferences
	CHK(sqlite3_prepare_v2(db,
		"insert into Pref ("
		"    Id, Name, Usertype, Filetype, iValue, sValue, InsertDate"
		") values ("
		"    :Id, :Name, :Usertype, :Filetype, :iValue, :sValue, strftime('%Y-%m-%dT%H:%M:%S', 'now'))",
		-1, &dbpstmt, NULL), "PrefPRE")
	for (i=0; i<NUM_PREFS; i++) {
		CHK(sqlite3_bind_int(dbpstmt,1,i),"PrefB1")
		CHK(sqlite3_bind_text(dbpstmt,2,glob_prefs[i].name,-1,SQLITE_STATIC),"PrefB2")
		CHK(sqlite3_bind_text(dbpstmt,3,glob_prefs[i].usertype == 1 ? "int" : "char",-1,SQLITE_STATIC),"PrefB3")
		CHK(sqlite3_bind_text(dbpstmt,4,glob_prefs[i].filetype == 1 ? "int" : "char",-1,SQLITE_STATIC),"PrefB4")
		CHK(sqlite3_bind_int(dbpstmt,5,glob_prefs[i].ivalue),"PrefB5")
		CHK(sqlite3_bind_text(dbpstmt,6,glob_prefs[i].svalue,-1,SQLITE_STATIC),"PrefB6")
		CHKDONE(sqlite3_step(dbpstmt),"PrefST")
		CHK(sqlite3_clear_bindings(dbpstmt),"PrefCL")
		CHK(sqlite3_reset(dbpstmt),"PrefRST")
	}
	CHK(sqlite3_finalize(dbpstmt),"PrefFIN")
	jp_logf(JP_LOG_DEBUG,"%d Pref records in array\n", i);

	CHK(sqlite3_exec(db,"END TRANSACTION",NULL,NULL,NULL),"END")
	jp_logf(JP_LOG_GUI,"Finished SQLite3 transaction:\n"
		"\t%8d inserted into Addr\n"
		"\t%8d inserted into AddrLabel\n"
		"\t%8d inserted into AddrCategory\n"
		"\t%8d inserted into PhoneLabel\n"
		"\t%8d inserted into Datebook\n"
		"\t%8d inserted into ToDo\n"
		"\t%8d inserted into ToDoCategory\n"
		"\t%8d inserted into Memo\n"
		"\t%8d inserted into MemoCategory\n"
		"\t%8d inserted into Expense\n"
		"\t%8d inserted into ExpenseCategory\n"
		"\t%8d inserted into Pref\n"
		, nrecsA, nrecsAL, nrecsAC, nrecsPL, nrecsD
		, nrecsT, nrecsTC, nrecsM, nrecsMC, nrecsE, nrecsEC, NUM_PREFS
	);

	return EXIT_SUCCESS;

err:
	jp_logf(JP_LOG_FATAL,"SQLite3 ret=%d, error=%s, Id=%d, rolling back\n%s\n",
		sqlRet, sqlErr, errId, sqlite3_errmsg(db));
	sqlite3_finalize(dbpstmt);
	sqlite3_exec(db,"ROLLBACK TRANSACTION",NULL,NULL,NULL);
	return EXIT_FAILURE;
}



static void cb_copy_sqlite(GtkWidget *widget, gpointer data) {
	copy_sqlite();
}



int plugin_gui(GtkWidget *vbox, GtkWidget *hbox, unsigned int unique_id) {
	jp_logf(JP_LOG_DEBUG,"SQLite3 plugin_gui(): unique_id=%d\n",unique_id);

	if (!glob_sqlite) {
		copy_sqlite_button = gtk_button_new_with_label("SQL");
		gtk_box_pack_start(GTK_BOX(vbox),copy_sqlite_button,TRUE,TRUE,0);
		if (connected == 0) {
			g_signal_connect(G_OBJECT(copy_sqlite_button),
				"clicked",
				G_CALLBACK(cb_copy_sqlite), NULL);
			connected = 1;
		}
		gtk_widget_show_all(vbox);
	}

	return EXIT_SUCCESS;
}



int plugin_gui_cleanup(void) {
	if (connected == 1) {
		g_signal_handlers_disconnect_by_func(
			G_OBJECT(copy_sqlite_button),
			G_CALLBACK(cb_copy_sqlite), NULL);
		connected = 0;
	}
	return EXIT_SUCCESS;
}




/* Store J-Pilot data into SQLite3 database directly whenever the data changes in J-Pilot

   Elmar Klausmeier, 20-Sep-2022: Initial revision
*/

#include <stdio.h>
#include <string.h>
#include <time.h>
extern char *strptime (const char *__restrict __s, const char *__restrict __fmt, struct tm *__tp) __THROW;
#include <sys/stat.h>
#include <sqlite3.h>

#include "log.h"
#include "address.h"
#include "datebook.h"
#include "calendar.h"
#include "memo.h"
#include "todo.h"
#include "password.h"
#include "prefs.h"
#include "libsqlite.h"


// Get bit 5 of x
#define IS_PRIVATE(x)	((((x) & 0xF0) >> 4) & 0x01)
// Check if strdup() failed: if pointer returned from SQLite is not NULL, then strdup() it and check for allocation failure
#define ALLOCN(x,y,E)	{ const char *s=y; if (s) { if ((s=strdup(s))!=NULL) x=(char*)s; else { sqlErr=E; goto errAlloc; } } }
// Run SQLite command/function x and store error text E
#define CHK(x,E)	if ((sqlRet = x) != SQLITE_OK) { sqlErr=E; goto err; }
#define CHKDONE(x,E)	if ((sqlRet = x) != SQLITE_DONE) { sqlErr=E; goto err; }
#define CHKROW(x,E)	if ((sqlRet = x) != SQLITE_ROW) { sqlErr=E; goto err; }
// Replace empty strings with NULL
#define RE(x)	((x && x[0]=='\0') ? NULL : x)

static struct {
	sqlite3 *conn;	// file global database pointer
	sqlite3_stmt	// prepared SQL statements
		*stmtAddrLabelSEL, *stmtAddrCategorySEL, *stmtPhoneLabelSEL,
		*stmtAddrSELo1, *stmtAddrSELo2, *stmtAddrSELo3, *stmtAddrINS, *stmtAddrUPD, *stmtAddrDEL,
		*stmtDatebookSEL, *stmtDatebookINS, *stmtDatebookUPD, *stmtDatebookDEL,
		*stmtMemoCategorySEL, *stmtMemoSEL, *stmtMemoINS, *stmtMemoUPD, *stmtMemoDEL,
		*stmtToDoCategorySEL, *stmtToDoSEL, *stmtToDoINS, *stmtToDoUPD, *stmtToDoDEL,
		*stmtExpenseCategorySEL, *stmtExpenseTypeSEL, *stmtExpensePaymentSEL,
		*stmtExpenseCurrencySEL,
		*stmtExpenseSEL, *stmtExpenseINS, *stmtExpenseUPD, *stmtExpenseDEL,
		*stmtPrefSEL, *stmtPrefDEL, *stmtPrefINS;
	int maxIdAddr, maxIdDatebook, maxIdMemo, maxIdToDo, maxIdExpense;
} db;



// Copied from SQLite3 plugin
int jpsqlite_open(void) {	// return database handle
	char dbName[FILENAME_MAX];	// path + file-name
	char sqlFile[FILENAME_MAX];
	FILE *fp;
	char *p0;	// points to SQL string in jptables.sql
	struct stat sqlFStat;
	//sqlite3 *d = NULL;
	int sqlRet = 0;
	const char *sqlErr = "";

	jp_get_home_file_name("jptables.db",dbName,FILENAME_MAX);
	jp_logf(JP_LOG_DEBUG,"jpsqlite_open(): dbName=%s\n",dbName);
	if (sqlite3_open_v2(dbName,&db.conn,SQLITE_OPEN_READWRITE,NULL) == SQLITE_OK)
		return EXIT_SUCCESS;

	// Now try to create new jptables.db by using SQL script
	// in $HOME/.jpilot/plugins/jptables.sql
	jp_get_home_file_name("plugins/jptables.sql",sqlFile,FILENAME_MAX);
	jp_logf(JP_LOG_DEBUG,"jpsqlite_open(): sqlFile=%s\n",sqlFile);
	if ( stat(sqlFile,&sqlFStat) ) {
		jp_logf(JP_LOG_FATAL,
			"jpsqlite_open(): Cannot stat SQL file jptables.sql in plugins directory: %s\n",
			strerror(errno));
		return EXIT_FAILURE;
	}
	if (sqlFStat.st_size == 0) {
		jp_logf(JP_LOG_FATAL,"jpsqlite_open(): SQL file jptables.sql has no content\n");
		return EXIT_FAILURE;
	}
	if ((fp = fopen(sqlFile,"r")) == NULL) {
		jp_logf(JP_LOG_FATAL,
			"jpsqlite_open(): Cannot open SQL file jptables.sql in plugins directory: %s\n",
			strerror(errno));
		return EXIT_FAILURE;
	}
	jp_logf(JP_LOG_DEBUG,"jpsqlite_open(): jptables.sql size=%ld\n",
		sqlFStat.st_size);
	if ((p0 = malloc(sqlFStat.st_size + 4)) == NULL) {
		jp_logf(JP_LOG_FATAL,
			"Cannot cache jptables.sql: %s\n",
			strerror(errno));
		return EXIT_FAILURE;
	}
	if (fread(p0,1,sqlFStat.st_size,fp) != sqlFStat.st_size) {
		jp_logf(JP_LOG_FATAL,
			"jpsqlite_open(): Cannot read jptables.sql: %s\n",
			strerror(errno));
		return EXIT_FAILURE;
	}
	p0[sqlFStat.st_size] = '\0';
	// Open and create db
	jp_logf(JP_LOG_GUI,"\nCreating new SQLite3 database file %s. This may take approx. 20 seconds.\n\n",dbName);
	CHK(sqlite3_open(dbName,&db.conn),"IOP")
	// Feed entire file content to SQLite3
	CHK(sqlite3_exec(db.conn,p0,NULL,NULL,NULL),"IEXC");

	free(p0);
	if (fclose(fp)) {
		jp_logf(JP_LOG_FATAL,
			"jpsqlite_open(): Cannot close jptables.sql: %s\n", strerror(errno));
		return EXIT_FAILURE;
	}
	jp_logf(JP_LOG_GUI,"SQLite3 database created successfully.\n");
	return EXIT_SUCCESS;

err:
	jp_logf(JP_LOG_FATAL,"jpsqlite_open(): SQLite3 ret=%d, error=%s, rolling back\n%s\n",
		sqlRet, sqlErr, sqlite3_errmsg(db.conn));
	return EXIT_FAILURE;
}



int jpsqlite_close(void) {
	int sqlRet = 0;
	const char *sqlErr = "";

	jp_logf(JP_LOG_DEBUG,"jpsqlite_close()\n");

	CHK(sqlite3_finalize(db.stmtAddrLabelSEL),"stmtAddrLabelSEL")
	CHK(sqlite3_finalize(db.stmtAddrCategorySEL),"stmtAddrCategorySEL")
	CHK(sqlite3_finalize(db.stmtPhoneLabelSEL),"stmtPhoneLabelSEL")
	CHK(sqlite3_finalize(db.stmtAddrSELo1),"stmtAddrSELo1")
	CHK(sqlite3_finalize(db.stmtAddrSELo2),"stmtAddrSELo2")
	CHK(sqlite3_finalize(db.stmtAddrSELo3),"stmtAddrSELo3")
	CHK(sqlite3_finalize(db.stmtAddrINS),"stmtAddrINS")
	CHK(sqlite3_finalize(db.stmtAddrUPD),"stmtAddrUPD")
	CHK(sqlite3_finalize(db.stmtAddrDEL),"stmtAddrDEL")
	CHK(sqlite3_finalize(db.stmtDatebookSEL),"stmtDatebookSEL")
	CHK(sqlite3_finalize(db.stmtDatebookINS),"stmtDatebookINS")
	CHK(sqlite3_finalize(db.stmtDatebookUPD),"stmtDatebookUPD")
	CHK(sqlite3_finalize(db.stmtDatebookDEL),"stmtDatebookDEL")
	CHK(sqlite3_finalize(db.stmtMemoCategorySEL),"stmtMemoCategorySEL")
	CHK(sqlite3_finalize(db.stmtMemoSEL),"stmtMemoSEL")
	CHK(sqlite3_finalize(db.stmtMemoINS),"stmtMemoINS")
	CHK(sqlite3_finalize(db.stmtMemoUPD),"stmtMemoUPD")
	CHK(sqlite3_finalize(db.stmtMemoDEL),"stmtMemoDEL")
	CHK(sqlite3_finalize(db.stmtToDoCategorySEL),"stmtToDoCategorySEL")
	CHK(sqlite3_finalize(db.stmtToDoSEL),"stmtToDoSEL")
	CHK(sqlite3_finalize(db.stmtToDoINS),"stmtToDoINS")
	CHK(sqlite3_finalize(db.stmtToDoUPD),"stmtToDoUPD")
	CHK(sqlite3_finalize(db.stmtToDoDEL),"stmtToDoDEL")
	CHK(sqlite3_finalize(db.stmtExpenseCategorySEL),"stmtExpenseCategorySEL")
	CHK(sqlite3_finalize(db.stmtExpenseTypeSEL),"stmtExpenseTypeSEL")
	CHK(sqlite3_finalize(db.stmtExpensePaymentSEL),"stmtExpensePaymentSEL")
	CHK(sqlite3_finalize(db.stmtExpenseCurrencySEL),"stmtExpenseCurrencySEL")
	CHK(sqlite3_finalize(db.stmtExpenseSEL),"stmtExpenseSEL")
	CHK(sqlite3_finalize(db.stmtExpenseINS),"stmtExpenseINS")
	CHK(sqlite3_finalize(db.stmtExpenseUPD),"stmtExpenseUPD")
	CHK(sqlite3_finalize(db.stmtExpenseDEL),"stmtExpenseDEL")
	CHK(sqlite3_finalize(db.stmtPrefSEL),"stmtPrefSEL")
	CHK(sqlite3_finalize(db.stmtPrefDEL),"stmtPrefDEL")
	CHK(sqlite3_finalize(db.stmtPrefINS),"stmtPrefINS")

	sqlRet = sqlite3_close(db.conn);	// noop if db==NULL
	if (sqlRet != SQLITE_OK) {
		jp_logf(JP_LOG_FATAL,"Cannot close sqlite3 file, sqlRet=%d\n",sqlRet);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;

err:
	jp_logf(JP_LOG_FATAL,"jpsqlite_close(): SQLite3 ret=%d, error=%s\n%s\n",
		sqlRet, sqlErr, sqlite3_errmsg(db.conn));
	sqlite3_close(db.conn);	// noop if db==NULL
	return EXIT_FAILURE;
}



int jpsqlite_prepareAllStmt(void) {
	int sqlRet = 0;
	const char *sqlErr = "";

	jp_logf(JP_LOG_DEBUG,"jpsqlite_prepareAllStmt(): Start all prepared statements\n");

	CHK(sqlite3_prepare_v2(db.conn,
		"select Id, Label from AddrLabel order by Id",
		-1, &db.stmtAddrLabelSEL, NULL), "jpAddrLabelSEL")
	CHK(sqlite3_prepare_v2(db.conn,
		"select Id, Label from AddrCategory order by Id",
		-1, &db.stmtAddrCategorySEL, NULL), "jpAddrCategorySEL")
	CHK(sqlite3_prepare_v2(db.conn,
		"select Id, Label from PhoneLabel order by Id",
		-1, &db.stmtPhoneLabelSEL, NULL), "jpPhoneLabelSEL")

	// CATEGORY_ALL == 300
	#define ADDRSELBASE	\
		"select Id, Category, Private, showPhone,"	\
		"    Lastname, Firstname, Title, Company,"	\
		"    PhoneLabel1, PhoneLabel2, PhoneLabel3, PhoneLabel4, PhoneLabel5,"	\
		"    Phone1, Phone2, Phone3, Phone4, Phone5,"	\
		"    Address, City, State, Zip, Country,"	\
		"    Custom1, Custom2, Custom3, Custom4, Note "	\
		"from Addr "	\
		"where (Private=:private0 or Private=:private1) "	\
		"and (Category=:category or 300=:category) "
	CHK(sqlite3_prepare_v2(db.conn,ADDRSELBASE
		"order by Lastname, Firstname, Company",
		-1, &db.stmtAddrSELo1, NULL), "jpAddrSEL1")
	CHK(sqlite3_prepare_v2(db.conn,ADDRSELBASE
		"order by Firstname, Lastname, Company",
		-1, &db.stmtAddrSELo2, NULL), "jpAddrSEL2")
	CHK(sqlite3_prepare_v2(db.conn,ADDRSELBASE
		"order by Company, Lastname, Firstname",
		-1, &db.stmtAddrSELo3, NULL), "jpAddrSEL3")


	CHK(sqlite3_prepare_v2(db.conn,
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
		-1, &db.stmtAddrINS, NULL), "jpAddrINS")

	CHK(sqlite3_prepare_v2(db.conn,
		"update Addr set"
		"    Category=:Category, Private=:Private, showPhone=:showPhone,"	// 1
		"    Lastname=:Lastname, Firstname=:Firstname, Title=:Title, Company=:Company,"	// 4
		"    PhoneLabel1=:PhoneLabel1, PhoneLabel2=:PhoneLabel2, PhoneLabel3=:PhoneLabel3,"	// 8
		"    PhoneLabel4=:PhoneLabel4, PhoneLabel5=:PhoneLabel5,"		// 11
		"    Phone1=:Phone1, Phone2=:Phone2, Phone3=:Phone3, Phone4=:Phone4, Phone5=:Phone5,"	// 13
		"    Address=:Address, City=:City, State=:State, Zip=:Zip, Country=:Country,"	// 18
		"    Custom1=:Custom1, Custom2=:Custom2, Custom3=:Custom3, Custom4=:Custom4,"	// 23
		"    Note=:Note, UpdateDate = strftime('%Y-%m-%dT%H:%M:%S', 'now') "					// 27
		"where Id = :Id",					// 28
		-1, &db.stmtAddrUPD, NULL), "jpAddrUPD")

	CHK(sqlite3_prepare_v2(db.conn,
		"delete from Addr where Id = :Id",
		-1, &db.stmtAddrDEL, NULL), "jpAddrDEL")


	CHK(sqlite3_prepare_v2(db.conn,
		"select Id, Private, Timeless, Begin, End, Alarm,"	// 0
		"    Advance, AdvanceUnit, RepeatType, RepeatForever,"	// 6
		"    RepeatEnd, RepeatFreq, RepeatDay,"	// 10
		"    RepeatDaySu, RepeatDayMo, RepeatDayTu, RepeatDayWe,"	// 13
		"    RepeatDayTh, RepeatDayFr, RepeatDaySa,"	// 17
		"    Exceptions, Exception, Description, Note "	// 20
		"from Datebook "
		"where (Private=0 or Private=:private1) "
		"and ((Begin>=:now||'T00:00' and End<=:now||'T23:59' or RepeatType<>0) or :iNow=0) "
		"order by substr(Begin,11), Begin, Id",	// order by hh:mm
		-1, &db.stmtDatebookSEL, NULL), "jpDatebookSEL")

	CHK(sqlite3_prepare_v2(db.conn,
		"insert into Datebook ("
		"    Id, Private, Timeless, Begin, End,"	// 1
		"    Alarm, Advance, AdvanceUnit, RepeatType,"	// 6
		"    RepeatForever, RepeatEnd, RepeatFreq,"	// 10
		"    RepeatDay,"				// 13
		"    RepeatDaySu, RepeatDayMo, RepeatDayTu,"	// 14
		"    RepeatDayWe, RepeatDayTh, RepeatDayFr,"	// 17
		"    RepeatDaySa, Exceptions, Exception,"		// 20
		"    Description, Note, InsertDate"			// 23
		") values ("
		"    :Id, :Private, :Timeless, :Begin, :End,"
		"    :Alarm, :Advance, :AdvanceUnit, :RepeatType,"
		"    :RepeatForever, :RepeatEnd, :RepeatFreq,"
		"    :RepeatDay,"
		"    :RepeatDaySu, :RepeatDayMo, :RepeatDayTu,"
		"    :RepeatDayWe, :RepeatDayTh, :RepeatDayFr,"
		"    :RepeatDaySa, :Exceptions, :Exception,"
		"    :Description, :Note,"
		"    strftime('%Y-%m-%dT%H:%M:%S', 'now')"
		")",
		-1, &db.stmtDatebookINS, NULL), "jpDatebookINS")

	CHK(sqlite3_prepare_v2(db.conn,
		"update Datebook set"
		"    Private=:Private, Timeless=:Timeless, Begin=:Begin, End=:End, "	// 1
		"    Alarm=:Alarm, Advance=:Advance, AdvanceUnit=:AdvanceUnit, RepeatType=:RepeatType, "	// 5
		"    RepeatForever=:RepeatForever, RepeatEnd=:RepeatEnd, RepeatFreq=:RepeatFreq, "	// 9
		"    RepeatDay=:RepeatDay, "				// 12
		"    RepeatDaySu=:RepeatDaySu, RepeatDayMo=:RepeatDayMo, RepeatDayTu=:RepeatDayTu, "	// 13
		"    RepeatDayWe=:RepeatDayWe, RepeatDayTh=:RepeatDayTh, RepeatDayFr=:RepeatDayFr, "	// 16
		"    RepeatDaySa=:RepeatDaySa, Exceptions=:Exceptions, Exception=:Exception, "	// 19
		"    Description=:Description, Note=:Note, "		// 22
		"    UpdateDate = strftime('%Y-%m-%dT%H:%M:%S', 'now') "
		"where Id = :Id",		// 22
		-1, &db.stmtDatebookUPD, NULL), "jpDatebookUPD")

	CHK(sqlite3_prepare_v2(db.conn,
		"delete from Datebook where Id = :Id",
		-1, &db.stmtDatebookDEL, NULL), "jpDatebookDEL")


	CHK(sqlite3_prepare_v2(db.conn,
		"select Id, Label from MemoCategory order by Id",
		-1, &db.stmtMemoCategorySEL, NULL), "jpMemoCategorySEL")

	// CATEGORY_ALL == 300
	CHK(sqlite3_prepare_v2(db.conn,
		"select Id, Category, Private, Text "
		"from Memo "
		"where (Private=:private0 or Private=:private1) "
		"and (Category=:category or 300=:category) "
		"order by Text",
		-1, &db.stmtMemoSEL, NULL), "jpMemoSEL")

	CHK(sqlite3_prepare_v2(db.conn,
		"insert into Memo ("
		"    Id, Category, Private, Text, "
		"    InsertDate "
		") values ("
		"    :Id, :Category, :Private, :Text, "
		"    strftime('%Y-%m-%dT%H:%M:%S', 'now') "
		")",
		-1, &db.stmtMemoINS, NULL), "jpMemoINS")

	CHK(sqlite3_prepare_v2(db.conn,
		"update Memo set"
		"    Category=:Category, Private=:Private, Text=:Text, "
		"    UpdateDate = strftime('%Y-%m-%dT%H:%M:%S', 'now') "
		"where Id = :Id",
		-1, &db.stmtMemoUPD, NULL), "jpMemoUPD")

	CHK(sqlite3_prepare_v2(db.conn,
		"delete from Memo where Id = :Id",
		-1, &db.stmtMemoDEL, NULL), "jpMemoDEL")


	CHK(sqlite3_prepare_v2(db.conn,
		"select Id, Label from ToDoCategory order by Id",
		-1, &db.stmtToDoCategorySEL, NULL), "jpToDoCategorySEL")


	// CATEGORY_ALL == 300
	CHK(sqlite3_prepare_v2(db.conn,
		"select Id, Category, Private, Indefinite, Due,"
		"    Priority, Complete, Description, Note "
		"from ToDo "
		"where (Private=:private0 or Private=:private1) "
		"and (Category=:category or 300=:category) "
		"order by Priority asc, Due desc, Description asc",
		-1, &db.stmtToDoSEL, NULL), "jpToDoSEL")

	CHK(sqlite3_prepare_v2(db.conn,
		"insert into ToDo ("
		"    Id, Category, Private, Indefinite, "	// 1
		"    Due, Priority, Complete, "			// 5
		"    Description, Note, "			// 8
		"    InsertDate "
		") values ("
		"    :Id, :Category, :Private, :Indefinite, "
		"    :Due, :Priority, :Complete, "
		"    :Description, :Note, "
		"    strftime('%Y-%m-%dT%H:%M:%S', 'now') "
		")",
		-1, &db.stmtToDoINS, NULL),"jpToDoINS")

	CHK(sqlite3_prepare_v2(db.conn,
		"update ToDo set"
		"    Category=:Category, Private=:Private, Indefinite=:Indefinite, "	// 1
		"    Due=:Due, Priority=:Priority, Complete=:Complete, "			// 4
		"    Description=:Description, Note=:Note, "			// 7
		"    UpdateDate = strftime('%Y-%m-%dT%H:%M:%S', 'now') "
		"where Id = :Id",				// 9
		-1, &db.stmtToDoUPD, NULL),"jpToDoUPD")

	CHK(sqlite3_prepare_v2(db.conn,
		"delete from ToDo where Id = :Id",
		-1, &db.stmtToDoDEL, NULL), "jpToDoDEL")


	CHK(sqlite3_prepare_v2(db.conn,
		"select Id, Label from ExpenseCategory order by Id",
		-1, &db.stmtExpenseCategorySEL, NULL), "jpExpenseCategorySEL")

	CHK(sqlite3_prepare_v2(db.conn,
		"select Id, Label from ExpenseType order by Id",
		-1, &db.stmtExpenseTypeSEL, NULL), "jpExpenseTypeSEL")

	CHK(sqlite3_prepare_v2(db.conn,
		"select Id, Label from ExpensePayment order by Id",
		-1, &db.stmtExpensePaymentSEL, NULL), "jpExpensePaymentSEL")

	CHK(sqlite3_prepare_v2(db.conn,
		"select Id, Label from ExpenseCurrency order by Id",
		-1, &db.stmtExpenseCurrencySEL, NULL), "jpExpenseCurrencySEL")

	CHK(sqlite3_prepare_v2(db.conn,
		"select Id, Category, Date, Type, Payment, Currency,"
		"    Amount, Vendor, City, Attendees, Note "
		"from Expense order by Id",
		-1, &db.stmtExpenseSEL, NULL), "jpExpenseSEL")

	CHK(sqlite3_prepare_v2(db.conn,
		"insert into Expense ("
		"    Id, Category, Date, Type, Payment, "	// 1
		"    Currency, Amount, Vendor, City, "		// 6
		"    Attendees, Note, InsertDate "				// 10
		") values ("
		"    :Id, :Category, :Date, :Type, :Payment, "
		"    :Currency, :Amount, :Vendor, :City, "
		"    :Attendees, :Note, "
		"    strftime('%Y-%m-%dT%H:%M:%S', 'now') "
		")",
		-1, &db.stmtExpenseINS, NULL), "jpExpenseINS")

	CHK(sqlite3_prepare_v2(db.conn,
		"update Expense set"
		"    Category=:Category, Date=:Date, Type=:Type, Payment=:Payment, "	// 1
		"    Currency=:Currency, Amount=:Amount, Vendor=:Vendor, City=:City, "		// 5
		"    Attendees=:Attendees, Note=:Note, "				// 9
		"    UpdateDate = strftime('%Y-%m-%dT%H:%M:%S', 'now') "
		"where Id = :Id",		// 11
		-1, &db.stmtExpenseUPD, NULL), "jpExpenseUPD")

	CHK(sqlite3_prepare_v2(db.conn,
		"delete from Expense where Id = :Id",
		-1, &db.stmtExpenseDEL, NULL), "jpExpenseDEL")


	CHK(sqlite3_prepare_v2(db.conn,
		"select Id, Name, Usertype, Filetype, iValue, sValue from Pref "
		"order by Id, Name",
		-1, &db.stmtPrefSEL, NULL), "jpPrefSEL")

	CHK(sqlite3_prepare_v2(db.conn, "delete from Pref",
		-1, &db.stmtPrefDEL, NULL), "jpPrefDEL")

	CHK(sqlite3_prepare_v2(db.conn,
		"insert into Pref ("
		"    Id, Name, Usertype, Filetype, iValue, sValue, InsertDate"
		") values ("
		"    :Id, :Name, :Usertype, :Filetype, :iValue, :sValue, strftime('%Y-%m-%dT%H:%M:%S', 'now'))",
		-1, &db.stmtPrefINS, NULL), "jpPrefINS")


	jp_logf(JP_LOG_DEBUG,"jpsqlite_prepareAllStmt(): Finished all prepared statements\n");


	// Read 1+max(Id) from various tables, which might later get INSERT's
	sqlite3_stmt *stmtMaxIdSEL;
	CHK(sqlite3_prepare_v2(db.conn,
		"select"
		"    1 + (select coalesce(max(Id),0) from Addr),"
		"    1 + (select coalesce(max(Id),0) from Datebook),"
		"    1 + (select coalesce(max(Id),0) from Memo),"
		"    1 + (select coalesce(max(Id),0) from ToDo),"
		"    1 + (select coalesce(max(Id),0) from Expense)",
		-1, &stmtMaxIdSEL, NULL), "jpMaxIdSEL")
	sqlRet = sqlite3_step(stmtMaxIdSEL);
	if (sqlRet != SQLITE_ROW) goto err;
	db.maxIdAddr = sqlite3_column_int(stmtMaxIdSEL,0);
	db.maxIdDatebook = sqlite3_column_int(stmtMaxIdSEL,1);
	db.maxIdMemo = sqlite3_column_int(stmtMaxIdSEL,2);
	db.maxIdToDo = sqlite3_column_int(stmtMaxIdSEL,3);
	db.maxIdExpense = sqlite3_column_int(stmtMaxIdSEL,4);
	sqlite3_finalize(stmtMaxIdSEL);

	jp_logf(JP_LOG_DEBUG,"jpsqlite_prepareAllStmt(): maxIdAddr=%d, maxIdDatebook=%d, maxIdMemo=%d, maxIdToDo=%d, maxIdExpense=%d\n",
		db.maxIdAddr, db.maxIdDatebook, db.maxIdMemo, db.maxIdToDo, db.maxIdExpense);

	return EXIT_SUCCESS;

err:
	jp_logf(JP_LOG_FATAL,"jpsqlite_prepareAllStmt(): ret=%d, error=%s, giving up\n%s\n",
		sqlRet, sqlErr, sqlite3_errmsg(db.conn));
	//sqlite3_finalize(db.stmtXXX);	// We cannot really do much here
	//sqlite3_exec(db.conn,"ROLLBACK TRANSACTION",NULL,NULL,NULL);
	return EXIT_FAILURE;
}



int jpsqlite_AddrUPD(struct Address *addr, PCRecType rt, unsigned char attrib, unsigned int *unique_id) {
	int sqlRet = 0, errId;
	const char *sqlErr = "";

	jp_logf(JP_LOG_DEBUG,"jpsqlite_AddrUPD(): rt=%d, category=%d, unique_id=%u\n",rt,attrib&0x0F,*unique_id);

	errId = *unique_id;
	CHK(sqlite3_bind_int(db.stmtAddrUPD,1,attrib & 0x0F),"AddrUpdB1")
	CHK(sqlite3_bind_int(db.stmtAddrUPD,2,IS_PRIVATE(attrib)),"AddrUpdB2")
	CHK(sqlite3_bind_int(db.stmtAddrUPD,3,addr->showPhone),"AddrUpdB3")
	CHK(sqlite3_bind_text(db.stmtAddrUPD,4,RE(addr->entry[entryLastname]),-1,SQLITE_STATIC),"AddrUpdB4")
	CHK(sqlite3_bind_text(db.stmtAddrUPD,5,RE(addr->entry[entryFirstname]),-1,SQLITE_STATIC),"AddrUpdB5")
	CHK(sqlite3_bind_text(db.stmtAddrUPD,6,RE(addr->entry[entryTitle]),-1,SQLITE_STATIC),"AddrUpdB6")
	CHK(sqlite3_bind_text(db.stmtAddrUPD,7,RE(addr->entry[entryCompany]),-1,SQLITE_STATIC),"AddrUpdB7")
	CHK(sqlite3_bind_int(db.stmtAddrUPD,8,addr->phoneLabel[0]),"AddrUpdB8")
	CHK(sqlite3_bind_int(db.stmtAddrUPD,9,addr->phoneLabel[1]),"AddrUpdB9")
	CHK(sqlite3_bind_int(db.stmtAddrUPD,10,addr->phoneLabel[2]),"AddrUpdB10")
	CHK(sqlite3_bind_int(db.stmtAddrUPD,11,addr->phoneLabel[3]),"AddrUpdB11")
	CHK(sqlite3_bind_int(db.stmtAddrUPD,12,addr->phoneLabel[4]),"AddrUpdB12")
	CHK(sqlite3_bind_text(db.stmtAddrUPD,13,RE(addr->entry[entryPhone1]),-1,SQLITE_STATIC),"AddrUpdB13")
	CHK(sqlite3_bind_text(db.stmtAddrUPD,14,RE(addr->entry[entryPhone2]),-1,SQLITE_STATIC),"AddrUpdB14")
	CHK(sqlite3_bind_text(db.stmtAddrUPD,15,RE(addr->entry[entryPhone3]),-1,SQLITE_STATIC),"AddrUpdB15")
	CHK(sqlite3_bind_text(db.stmtAddrUPD,16,RE(addr->entry[entryPhone4]),-1,SQLITE_STATIC),"AddrUpdB16")
	CHK(sqlite3_bind_text(db.stmtAddrUPD,17,RE(addr->entry[entryPhone5]),-1,SQLITE_STATIC),"AddrUpdB17")
	CHK(sqlite3_bind_text(db.stmtAddrUPD,18,RE(addr->entry[entryAddress]),-1,SQLITE_STATIC),"AddrUpdB18")
	CHK(sqlite3_bind_text(db.stmtAddrUPD,19,RE(addr->entry[entryCity]),-1,SQLITE_STATIC),"AddrUpdB19")
	CHK(sqlite3_bind_text(db.stmtAddrUPD,20,RE(addr->entry[entryState]),-1,SQLITE_STATIC),"AddrUpdB20")
	CHK(sqlite3_bind_text(db.stmtAddrUPD,21,RE(addr->entry[entryZip]),-1,SQLITE_STATIC),"AddrUpdB21")
	CHK(sqlite3_bind_text(db.stmtAddrUPD,22,RE(addr->entry[entryCountry]),-1,SQLITE_STATIC),"AddrUpdB22")
	CHK(sqlite3_bind_text(db.stmtAddrUPD,23,RE(addr->entry[entryCustom1]),-1,SQLITE_STATIC),"AddrUpdB23")
	CHK(sqlite3_bind_text(db.stmtAddrUPD,24,RE(addr->entry[entryCustom2]),-1,SQLITE_STATIC),"AddrUpdB24")
	CHK(sqlite3_bind_text(db.stmtAddrUPD,25,RE(addr->entry[entryCustom3]),-1,SQLITE_STATIC),"AddrUpdB25")
	CHK(sqlite3_bind_text(db.stmtAddrUPD,26,RE(addr->entry[entryCustom4]),-1,SQLITE_STATIC),"AddrUpdB26")
	CHK(sqlite3_bind_text(db.stmtAddrUPD,27,RE(addr->entry[entryNote]),-1,SQLITE_STATIC),"AddrUpdB27")
	CHK(sqlite3_bind_int(db.stmtAddrUPD,28,*unique_id),"AddrUpdB28")
	CHKDONE(sqlite3_step(db.stmtAddrUPD),"AddrUpdST")

	CHK(sqlite3_clear_bindings(db.stmtAddrUPD),"AddrUpdCL")
	CHK(sqlite3_reset(db.stmtAddrUPD),"AddrUpdRST")

	return EXIT_SUCCESS;

err:
	jp_logf(JP_LOG_FATAL,"jpsqlite_AddrINS(): ret=%d, error=%s, Id=%d, rolling back\n%s\n",
		sqlRet, sqlErr, errId, sqlite3_errmsg(db.conn));
	sqlite3_finalize(db.stmtAddrUPD);
	//sqlite3_exec(db.conn,"ROLLBACK TRANSACTION",NULL,NULL,NULL);
	return EXIT_FAILURE;
}




int jpsqlite_AddrINS(struct Address *addr, PCRecType rt, unsigned char attrib, unsigned int *unique_id) {
	int sqlRet = 0, errId;
	const char *sqlErr = "";

	if (rt == REPLACEMENT_PALM_REC)
		return jpsqlite_AddrUPD(addr,rt,attrib,unique_id);
		
	jp_logf(JP_LOG_DEBUG,"jpsqlite_AddrINS(): rt=%d, category=%d, unique_id=%u, maxIdAddr=%d\n",rt,attrib&0x0F,unique_id?*unique_id:(-1),db.maxIdAddr);

	errId = db.maxIdAddr;
	if (unique_id) *unique_id = db.maxIdAddr;
	CHK(sqlite3_bind_int(db.stmtAddrINS,1,db.maxIdAddr),"AddrInsB1")
	CHK(sqlite3_bind_int(db.stmtAddrINS,2,attrib & 0x0F),"AddrInsB2")
	CHK(sqlite3_bind_int(db.stmtAddrINS,3,IS_PRIVATE(attrib)),"AddrInsB3")
	CHK(sqlite3_bind_int(db.stmtAddrINS,4,addr->showPhone),"AddrInsB4")
	CHK(sqlite3_bind_text(db.stmtAddrINS,5,RE(addr->entry[entryLastname]),-1,SQLITE_STATIC),"AddrInsB5")
	CHK(sqlite3_bind_text(db.stmtAddrINS,6,RE(addr->entry[entryFirstname]),-1,SQLITE_STATIC),"AddrInsB6")
	CHK(sqlite3_bind_text(db.stmtAddrINS,7,RE(addr->entry[entryTitle]),-1,SQLITE_STATIC),"AddrInsB7")
	CHK(sqlite3_bind_text(db.stmtAddrINS,8,RE(addr->entry[entryCompany]),-1,SQLITE_STATIC),"AddrInsB8")
	CHK(sqlite3_bind_int(db.stmtAddrINS,9,addr->phoneLabel[0]),"AddrInsB9")
	CHK(sqlite3_bind_int(db.stmtAddrINS,10,addr->phoneLabel[1]),"AddrInsB10")
	CHK(sqlite3_bind_int(db.stmtAddrINS,11,addr->phoneLabel[2]),"AddrInsB11")
	CHK(sqlite3_bind_int(db.stmtAddrINS,12,addr->phoneLabel[3]),"AddrInsB12")
	CHK(sqlite3_bind_int(db.stmtAddrINS,13,addr->phoneLabel[4]),"AddrInsB13")
	CHK(sqlite3_bind_text(db.stmtAddrINS,14,RE(addr->entry[entryPhone1]),-1,SQLITE_STATIC),"AddrInsB14")
	CHK(sqlite3_bind_text(db.stmtAddrINS,15,RE(addr->entry[entryPhone2]),-1,SQLITE_STATIC),"AddrInsB15")
	CHK(sqlite3_bind_text(db.stmtAddrINS,16,RE(addr->entry[entryPhone3]),-1,SQLITE_STATIC),"AddrInsB16")
	CHK(sqlite3_bind_text(db.stmtAddrINS,17,RE(addr->entry[entryPhone4]),-1,SQLITE_STATIC),"AddrInsB17")
	CHK(sqlite3_bind_text(db.stmtAddrINS,18,RE(addr->entry[entryPhone5]),-1,SQLITE_STATIC),"AddrInsB18")
	CHK(sqlite3_bind_text(db.stmtAddrINS,19,RE(addr->entry[entryAddress]),-1,SQLITE_STATIC),"AddrInsB19")
	CHK(sqlite3_bind_text(db.stmtAddrINS,20,RE(addr->entry[entryCity]),-1,SQLITE_STATIC),"AddrInsB20")
	CHK(sqlite3_bind_text(db.stmtAddrINS,21,RE(addr->entry[entryState]),-1,SQLITE_STATIC),"AddrInsB21")
	CHK(sqlite3_bind_text(db.stmtAddrINS,22,RE(addr->entry[entryZip]),-1,SQLITE_STATIC),"AddrInsB22")
	CHK(sqlite3_bind_text(db.stmtAddrINS,23,RE(addr->entry[entryCountry]),-1,SQLITE_STATIC),"AddrInsB23")
	CHK(sqlite3_bind_text(db.stmtAddrINS,24,RE(addr->entry[entryCustom1]),-1,SQLITE_STATIC),"AddrInsB24")
	CHK(sqlite3_bind_text(db.stmtAddrINS,25,RE(addr->entry[entryCustom2]),-1,SQLITE_STATIC),"AddrInsB24")
	CHK(sqlite3_bind_text(db.stmtAddrINS,26,RE(addr->entry[entryCustom3]),-1,SQLITE_STATIC),"AddrInsB26")
	CHK(sqlite3_bind_text(db.stmtAddrINS,27,RE(addr->entry[entryCustom4]),-1,SQLITE_STATIC),"AddrInsB27")
	CHK(sqlite3_bind_text(db.stmtAddrINS,28,RE(addr->entry[entryNote]),-1,SQLITE_STATIC),"AddrInsB28")
	CHKDONE(sqlite3_step(db.stmtAddrINS),"AddrInsST")
	db.maxIdAddr += 1;
	CHK(sqlite3_clear_bindings(db.stmtAddrINS),"AddrInsCL")
	CHK(sqlite3_reset(db.stmtAddrINS),"AddrInsRST")

	return EXIT_SUCCESS;

err:
	jp_logf(JP_LOG_FATAL,"jpsqlite_AddrINS(): ret=%d, error=%s, Id=%d, rolling back\n%s\n",
		sqlRet, sqlErr, errId, sqlite3_errmsg(db.conn));
	sqlite3_finalize(db.stmtAddrINS);
	//sqlite3_exec(db.conn,"ROLLBACK TRANSACTION",NULL,NULL,NULL);
	return EXIT_FAILURE;
}



int jpsqlite_DatebookUPD(struct CalendarEvent *cale, PCRecType rt, unsigned char attrib, unsigned int *unique_id) {
	int sqlRet = 0, errId, i, offset, nRealExcp, repeatForever;
	const char *sqlErr = "";
	char begin[32], end[32], repeatEnd[32], *pRepeatEnd, exceptionString[2048];

	jp_logf(JP_LOG_DEBUG,"jpsqlite_DatebookUPD(): rt=%d, unique_id=%u\n",rt,*unique_id);
	errId = *unique_id;

	strftime(begin,32,"%FT%R",&cale->begin);
	strftime(end,32,"%FT%R",&cale->end);

	// Data cleansing
	if (cale->repeatEnd.tm_year < 2  ||  cale->repeatEnd.tm_year > 2050
	|| cale->repeatEnd.tm_year == 70 && cale->repeatEnd.tm_mon == 0 && cale->repeatEnd.tm_mday == 1)	// 01-Jan-1970
		pRepeatEnd = NULL;
	else
		strftime(pRepeatEnd=repeatEnd,32,"%F",&cale->repeatEnd);

	CHK(sqlite3_bind_int(db.stmtDatebookUPD,1,IS_PRIVATE(attrib)),"DatebookUpdB1")
	CHK(sqlite3_bind_int(db.stmtDatebookUPD,2,cale->event),"DatebookUpdB2")
	CHK(sqlite3_bind_text(db.stmtDatebookUPD,3,begin,-1,SQLITE_STATIC),"DatebookUpdB3")
	CHK(sqlite3_bind_text(db.stmtDatebookUPD,4,end,-1,SQLITE_STATIC),"DatebookUpdB4")
	CHK(sqlite3_bind_int(db.stmtDatebookUPD,5,cale->alarm),"DatebookUpdB5")
	CHK(sqlite3_bind_int(db.stmtDatebookUPD,6,cale->advance),"DatebookUpdB6")
	CHK(sqlite3_bind_int(db.stmtDatebookUPD,7,cale->advanceUnits),"DatebookUpdB7")
	CHK(sqlite3_bind_int(db.stmtDatebookUPD,8,cale->repeatType),"DatebookUpdB8")
	// repeatForever is stored in inverse format as it is used in J-Pilot
	// in SQLite: repeatForever=1: repeating event, repeatForever=0: non-repeating
	// old: 1 - (cale->repeatForever & 0x01)
	// pRepeatEnd && (cale->repeatEnd.tm_year*366 + cale->repeatEnd.tm_yday > cale->begin.tm_year*366 + cale->begin.tm_yday)
	// && pRepeatEnd == NULL
	repeatForever = cale->repeatForever & 0x01;
	if (cale->repeatFrequency == 0 && cale->repeatDay == 0 && cale->repeatDays[0] == 0
	&& cale->repeatDays[1] == 0 && cale->repeatDays[2] == 0 && cale->repeatDays[3] == 0
	&& cale->repeatDays[4] == 0 && cale->repeatDays[5] == 0 && cale->repeatDays[6] == 0
	|| cale->repeatFrequency && pRepeatEnd == NULL)
		repeatForever = 1;
	else if (pRepeatEnd && cale->repeatEnd.tm_year*366 + cale->repeatEnd.tm_yday > cale->end.tm_year*366 + cale->end.tm_yday)
		repeatForever = 0;
	CHK(sqlite3_bind_int(db.stmtDatebookUPD,9,repeatForever),"DatebookUpdB9")
	CHK(sqlite3_bind_text(db.stmtDatebookUPD,10,pRepeatEnd,-1,SQLITE_STATIC),"DatebookUpdB10")
	CHK(sqlite3_bind_int(db.stmtDatebookUPD,11,cale->repeatFrequency),"DatebookUpdB11")
	CHK(sqlite3_bind_int(db.stmtDatebookUPD,12,cale->repeatDay),"DatebookUpdB12")
	CHK(sqlite3_bind_int(db.stmtDatebookUPD,13,cale->repeatDays[0]),"DatebookUpdB13")
	CHK(sqlite3_bind_int(db.stmtDatebookUPD,14,cale->repeatDays[1]),"DatebookUpdB14")
	CHK(sqlite3_bind_int(db.stmtDatebookUPD,15,cale->repeatDays[2]),"DatebookUpdB15")
	CHK(sqlite3_bind_int(db.stmtDatebookUPD,16,cale->repeatDays[3]),"DatebookUpdB16")
	CHK(sqlite3_bind_int(db.stmtDatebookUPD,17,cale->repeatDays[4]),"DatebookUpdB17")
	CHK(sqlite3_bind_int(db.stmtDatebookUPD,18,cale->repeatDays[5]),"DatebookUpdB18")
	CHK(sqlite3_bind_int(db.stmtDatebookUPD,19,cale->repeatDays[6]),"DatebookUpdB19")
	// Data cleansing: remove 1900-00-01 tm's
	for (nRealExcp=0,i=0; i<cale->exceptions; ++i)
		if (cale->exception[i].tm_year != 0) ++nRealExcp;
	CHK(sqlite3_bind_int(db.stmtDatebookUPD,20,nRealExcp),"DatebookUpdB20")
	if (nRealExcp * 11 > sizeof(exceptionString)) {
		jp_logf(JP_LOG_FATAL,"jpsqlite_DatebookUPD(): too many exceptions");
		exceptionString[0] = '\0';
	} else {
		for (i=0,offset=0; i<cale->exceptions; ++i) {
			if (cale->exception[i].tm_year == 0) continue;	// drop 1900-00-01 tm's
			strftime(exceptionString+offset,11,"%F",cale->exception+i);
			if (offset > 0) exceptionString[offset-1] = ' ';	// change previous '\0' to space
			offset += 11;
		}
	}
	CHK(sqlite3_bind_text(db.stmtDatebookUPD,21,nRealExcp ? exceptionString : NULL,-1,SQLITE_STATIC),"DatebookUpdB21")
	CHK(sqlite3_bind_text(db.stmtDatebookUPD,22,cale->description,-1,SQLITE_STATIC),"DatebookUpdB22")
	CHK(sqlite3_bind_text(db.stmtDatebookUPD,23,cale->note,-1,SQLITE_STATIC),"DatebookUpdB23")
	CHK(sqlite3_bind_int(db.stmtDatebookUPD,24,*unique_id),"DatebookUpdB24")
	CHKDONE(sqlite3_step(db.stmtDatebookUPD),"DatebookUpdST")
	db.maxIdDatebook += 1;
	CHK(sqlite3_clear_bindings(db.stmtDatebookUPD),"DatebookUpdCL")
	CHK(sqlite3_reset(db.stmtDatebookUPD),"DatebookUpdRST")

	return EXIT_SUCCESS;

err:
	jp_logf(JP_LOG_FATAL,"jpsqlite_DatebookUPD(): ret=%d, error=%s, Id=%d, rolling back\n%s\n",
		sqlRet, sqlErr, errId, sqlite3_errmsg(db.conn));
	sqlite3_finalize(db.stmtDatebookUPD);
	//sqlite3_exec(db.conn,"ROLLBACK TRANSACTION",NULL,NULL,NULL);
	return EXIT_FAILURE;
}



int jpsqlite_DatebookINS(struct CalendarEvent *cale, PCRecType rt, unsigned char attrib, unsigned int *unique_id) {
	int sqlRet = 0, errId, i, offset, nRealExcp, repeatForever;
	const char *sqlErr = "";
	char begin[32], end[32], repeatEnd[32], *pRepeatEnd, exceptionString[2048];

	jp_logf(JP_LOG_DEBUG,"jpsqlite_DatebookINS(): rt=%d, unique_id=%u, maxIdDatebook=%d\n",rt,unique_id?*unique_id:(-1),db.maxIdDatebook);
	errId = db.maxIdDatebook;	// ignore provided unique_id, instead get it from database
	if (unique_id) *unique_id = db.maxIdDatebook;

	strftime(begin,32,"%FT%R",&cale->begin);
	strftime(end,32,"%FT%R",&cale->end);

	// Data cleansing
	if (cale->repeatEnd.tm_year < 2  ||  cale->repeatEnd.tm_year > 2050
	|| cale->repeatEnd.tm_year == 70 && cale->repeatEnd.tm_mon == 0 && cale->repeatEnd.tm_mday == 1)	// 01-Jan-1970
		pRepeatEnd = NULL;
	else
		strftime(pRepeatEnd=repeatEnd,32,"%F",&cale->repeatEnd);

	CHK(sqlite3_bind_int(db.stmtDatebookINS,1,db.maxIdDatebook),"DatebookInsB1")
	CHK(sqlite3_bind_int(db.stmtDatebookINS,2,IS_PRIVATE(attrib)),"DatebookInsB2")
	CHK(sqlite3_bind_int(db.stmtDatebookINS,3,cale->event),"DatebookInsB3")
	CHK(sqlite3_bind_text(db.stmtDatebookINS,4,begin,-1,SQLITE_STATIC),"DatebookInsB4")
	CHK(sqlite3_bind_text(db.stmtDatebookINS,5,end,-1,SQLITE_STATIC),"DatebookInsB5")
	CHK(sqlite3_bind_int(db.stmtDatebookINS,6,cale->alarm),"DatebookInsB6")
	CHK(sqlite3_bind_int(db.stmtDatebookINS,7,cale->advance),"DatebookInsB7")
	CHK(sqlite3_bind_int(db.stmtDatebookINS,8,cale->advanceUnits),"DatebookInsB8")
	CHK(sqlite3_bind_int(db.stmtDatebookINS,9,cale->repeatType),"DatebookInsB9")
	// old: 1 - (cale->repeatForever & 0x01)
	// pRepeatEnd && (cale->repeatEnd.tm_year*366 + cale->repeatEnd.tm_yday > cale->begin.tm_year*366 + cale->begin.tm_yday)
	// && pRepeatEnd == NULL
	if (cale->repeatFrequency == 0 && cale->repeatDay == 0 && cale->repeatDays[0] == 0
	&& cale->repeatDays[1] == 0 && cale->repeatDays[2] == 0 && cale->repeatDays[3] == 0
	&& cale->repeatDays[4] == 0 && cale->repeatDays[5] == 0 && cale->repeatDays[6] == 0
	|| cale->repeatFrequency && pRepeatEnd == NULL)
		repeatForever = 1;
	else if (pRepeatEnd && cale->repeatEnd.tm_year*366 + cale->repeatEnd.tm_yday > cale->end.tm_year*366 + cale->end.tm_yday)
		repeatForever = 0;
	CHK(sqlite3_bind_int(db.stmtDatebookINS,10,repeatForever),"DatebookInsB10")
	CHK(sqlite3_bind_text(db.stmtDatebookINS,11,pRepeatEnd,-1,SQLITE_STATIC),"DatebookInsB11")
	CHK(sqlite3_bind_int(db.stmtDatebookINS,12,cale->repeatFrequency),"DatebookInsB12")
	CHK(sqlite3_bind_int(db.stmtDatebookINS,13,cale->repeatDay),"DatebookInsB13")
	CHK(sqlite3_bind_int(db.stmtDatebookINS,14,cale->repeatDays[0]),"DatebookInsB14")
	CHK(sqlite3_bind_int(db.stmtDatebookINS,15,cale->repeatDays[1]),"DatebookInsB15")
	CHK(sqlite3_bind_int(db.stmtDatebookINS,16,cale->repeatDays[2]),"DatebookInsB16")
	CHK(sqlite3_bind_int(db.stmtDatebookINS,17,cale->repeatDays[3]),"DatebookInsB17")
	CHK(sqlite3_bind_int(db.stmtDatebookINS,18,cale->repeatDays[4]),"DatebookInsB18")
	CHK(sqlite3_bind_int(db.stmtDatebookINS,19,cale->repeatDays[5]),"DatebookInsB19")
	CHK(sqlite3_bind_int(db.stmtDatebookINS,20,cale->repeatDays[6]),"DatebookInsB20")
	// Data cleansing: remove 1900-00-01 tm's
	for (nRealExcp=0,i=0; i<cale->exceptions; ++i)
		if (cale->exception[i].tm_year != 0) ++nRealExcp;
	CHK(sqlite3_bind_int(db.stmtDatebookINS,21,nRealExcp),"DatebookInsB21")
	if (nRealExcp * 11 > sizeof(exceptionString)) {
		jp_logf(JP_LOG_FATAL,"jpsqlite_DatebookINS(): too many exceptions");
		exceptionString[0] = '\0';
	} else {
		for (i=0,offset=0; i<cale->exceptions; ++i) {
			if (cale->exception[i].tm_year == 0) continue;	// drop 1900-00-01 tm's
			strftime(exceptionString+offset,11,"%F",cale->exception+i);
			if (offset > 0) exceptionString[offset-1] = ' ';	// change previous '\0' to space
			offset += 11;
		}
	}
	CHK(sqlite3_bind_text(db.stmtDatebookINS,22,nRealExcp ? exceptionString : NULL,-1,SQLITE_STATIC),"DatebookInsB22")
	CHK(sqlite3_bind_text(db.stmtDatebookINS,23,cale->description,-1,SQLITE_STATIC),"DatebookInsB23")
	CHK(sqlite3_bind_text(db.stmtDatebookINS,24,cale->note,-1,SQLITE_STATIC),"DatebookInsB24")
	CHKDONE(sqlite3_step(db.stmtDatebookINS),"DatebookInsST")
	db.maxIdDatebook += 1;
	CHK(sqlite3_clear_bindings(db.stmtDatebookINS),"DatebookInsCL")
	CHK(sqlite3_reset(db.stmtDatebookINS),"DatebookInsRST")

	return EXIT_SUCCESS;

err:
	jp_logf(JP_LOG_FATAL,"jpsqlite_DatebookINS(): ret=%d, error=%s, Id=%d, rolling back\n%s\n",
		sqlRet, sqlErr, errId, sqlite3_errmsg(db.conn));
	sqlite3_finalize(db.stmtDatebookINS);
	//sqlite3_exec(db.conn,"ROLLBACK TRANSACTION",NULL,NULL,NULL);
	return EXIT_FAILURE;
}



int jpsqlite_MemoUPD(struct Memo *memo, PCRecType rt, unsigned char attrib, unsigned int *unique_id) {
	int sqlRet = 0, errId;
	const char *sqlErr = "";

	jp_logf(JP_LOG_DEBUG,"jpsqlite_MemoUPD(): rt=%d, category=%d, unique_id=%u\n",rt,attrib&0X0F,*unique_id);

	CHK(sqlite3_bind_int(db.stmtMemoUPD,1,attrib & 0x0F),"MemoUpdB1")
	CHK(sqlite3_bind_int(db.stmtMemoUPD,2,IS_PRIVATE(attrib)),"MemoUpdB2")
	CHK(sqlite3_bind_text(db.stmtMemoUPD,3,memo->text,-1,SQLITE_STATIC),"MemoUpdB3")
	CHK(sqlite3_bind_int(db.stmtMemoUPD,4,*unique_id),"MemoUpdB4")
	CHKDONE(sqlite3_step(db.stmtMemoUPD),"MemoUpdST")

	CHK(sqlite3_clear_bindings(db.stmtMemoUPD),"MemoUpdCL")
	CHK(sqlite3_reset(db.stmtMemoUPD),"MemoUpdRST")

	return EXIT_SUCCESS;

err:
	jp_logf(JP_LOG_FATAL,"jpsqlite_MemoUPD(): ret=%d, error=%s, Id=%d, rolling back\n%s\n",
		sqlRet, sqlErr, errId, sqlite3_errmsg(db.conn));
	sqlite3_finalize(db.stmtMemoUPD);
	//sqlite3_exec(db.conn,"ROLLBACK TRANSACTION",NULL,NULL,NULL);
	return EXIT_FAILURE;
}



int jpsqlite_MemoINS(struct Memo *memo, PCRecType rt, unsigned char attrib, unsigned int *unique_id) {
	int sqlRet = 0, errId;
	const char *sqlErr = "";

	jp_logf(JP_LOG_DEBUG,"jpsqlite_MemoINS(): rt=%d, category=%d, unique_id=%u, maxIdMemo=%d\n",rt,attrib&0X0F,unique_id?*unique_id:(-1),db.maxIdMemo);
	errId = db.maxIdMemo;
	if (unique_id) *unique_id = db.maxIdMemo;

	CHK(sqlite3_bind_int(db.stmtMemoINS,1,db.maxIdMemo),"MemoInsB1")
	CHK(sqlite3_bind_int(db.stmtMemoINS,2,attrib & 0x0F),"MemoInsB2")
	CHK(sqlite3_bind_int(db.stmtMemoINS,3,IS_PRIVATE(attrib)),"MemoInsB3")
	CHK(sqlite3_bind_text(db.stmtMemoINS,4,memo->text,-1,SQLITE_STATIC),"MemoInsB4")
	CHKDONE(sqlite3_step(db.stmtMemoINS),"MemoInsST")
	db.maxIdMemo += 1;
	CHK(sqlite3_clear_bindings(db.stmtMemoINS),"MemoInsCL")
	CHK(sqlite3_reset(db.stmtMemoINS),"MemoInsRST")

	return EXIT_SUCCESS;

err:
	jp_logf(JP_LOG_FATAL,"jpsqlite_MemoINS(): ret=%d, error=%s, Id=%d, rolling back\n%s\n",
		sqlRet, sqlErr, errId, sqlite3_errmsg(db.conn));
	sqlite3_finalize(db.stmtMemoINS);
	//sqlite3_exec(db.conn,"ROLLBACK TRANSACTION",NULL,NULL,NULL);
	return EXIT_FAILURE;
}



int jpsqlite_ToDoUPD(struct ToDo *todo, PCRecType rt, unsigned char attrib, unsigned int *unique_id) {
	int sqlRet = 0, errId;
	const char *sqlErr = "";
	char due[32];

	jp_logf(JP_LOG_DEBUG,"jpsqlite_ToDoUPD(): rt=%d, category=%d, unique_id=%u\n",rt,attrib&0X0F,*unique_id);

	errId = *unique_id;

	//if (rt != PALM_REC && rt != NEW_PC_REC && rt != REPLACEMENT_PALM_REC)
	jp_logf(JP_LOG_DEBUG,"jpsqlite_ToDoUPD(): unique_id=%u rt=%d, category=%d |%.60s|\n",
		*unique_id, rt, attrib & 0x0F, todo->description);

	strftime(due,32,"%F",&todo->due);
	CHK(sqlite3_bind_int(db.stmtToDoUPD,1,attrib & 0x0F),"ToDoUpdB1")
	CHK(sqlite3_bind_int(db.stmtToDoUPD,2,IS_PRIVATE(attrib)),"ToDoUpdB2")
	CHK(sqlite3_bind_int(db.stmtToDoUPD,3,todo->indefinite),"ToDoUpdB3")
	CHK(sqlite3_bind_text(db.stmtToDoUPD,4,due,-1,SQLITE_STATIC),"ToDoUpdB4")
	CHK(sqlite3_bind_int(db.stmtToDoUPD,5,todo->priority),"ToDoUpdB5")
	CHK(sqlite3_bind_int(db.stmtToDoUPD,6,todo->complete),"ToDoUpdB6")
	CHK(sqlite3_bind_text(db.stmtToDoUPD,7,todo->description,-1,SQLITE_STATIC),"ToDoUpdB7")
	CHK(sqlite3_bind_text(db.stmtToDoUPD,8,todo->note,-1,SQLITE_STATIC),"ToDoUpdB8")
	CHK(sqlite3_bind_int(db.stmtToDoUPD,9,*unique_id),"ToDoUpdB9")
	CHKDONE(sqlite3_step(db.stmtToDoUPD),"ToDoUpdST")
	CHK(sqlite3_clear_bindings(db.stmtToDoUPD),"ToDoUpdCL")
	CHK(sqlite3_reset(db.stmtToDoUPD),"ToDoUpdRST")

	return EXIT_SUCCESS;

err:
	jp_logf(JP_LOG_FATAL,"jpsqlite_ToDoUPD(): ret=%d, error=%s, Id=%d, rolling back\n%s\n",
		sqlRet, sqlErr, errId, sqlite3_errmsg(db.conn));
	sqlite3_finalize(db.stmtToDoUPD);
	//sqlite3_exec(db.conn,"ROLLBACK TRANSACTION",NULL,NULL,NULL);
	return EXIT_FAILURE;
}



int jpsqlite_ToDoINS(struct ToDo *todo, PCRecType rt, unsigned char attrib, unsigned int *unique_id) {
	int sqlRet = 0, errId;
	const char *sqlErr = "";
	char due[32];

	jp_logf(JP_LOG_DEBUG,"jpsqlite_ToDoINS(): rt=%d, category=%d, unique_id=%u, maxIdToDo=%d\n",rt,attrib&0X0F,unique_id?*unique_id:(-1),db.maxIdToDo);

	errId = db.maxIdToDo;
	if (unique_id) *unique_id = db.maxIdToDo;

	if (rt != PALM_REC
	&&  rt != NEW_PC_REC
	&&  rt != REPLACEMENT_PALM_REC)
		jp_logf(JP_LOG_DEBUG,"jpsqlite_ToDoINS(): unique_id=%u rt=%d, category=%d |%.60s|\n",
				*unique_id, rt, attrib & 0x0F, todo->description);

	strftime(due,32,"%F",&todo->due);
	CHK(sqlite3_bind_int(db.stmtToDoINS,1,db.maxIdToDo),"ToDoInsB1")
	CHK(sqlite3_bind_int(db.stmtToDoINS,2,attrib & 0x0F),"ToDoInsB2")
	CHK(sqlite3_bind_int(db.stmtToDoINS,3,IS_PRIVATE(attrib)),"ToDoInsB3")
	CHK(sqlite3_bind_int(db.stmtToDoINS,4,todo->indefinite),"ToDoInsB4")
	CHK(sqlite3_bind_text(db.stmtToDoINS,5,due,-1,SQLITE_STATIC),"ToDoInsB5")
	CHK(sqlite3_bind_int(db.stmtToDoINS,6,todo->priority),"ToDoInsB6")
	CHK(sqlite3_bind_int(db.stmtToDoINS,7,todo->complete),"ToDoInsB7")
	CHK(sqlite3_bind_text(db.stmtToDoINS,8,todo->description,-1,SQLITE_STATIC),"ToDoInsB8")
	CHK(sqlite3_bind_text(db.stmtToDoINS,9,todo->note,-1,SQLITE_STATIC),"ToDoInsB9")
	CHKDONE(sqlite3_step(db.stmtToDoINS),"ToDoInsST")
	db.maxIdToDo += 1;
	CHK(sqlite3_clear_bindings(db.stmtToDoINS),"ToDoInsCL")
	CHK(sqlite3_reset(db.stmtToDoINS),"ToDoInsRST")

	return EXIT_SUCCESS;

err:
	jp_logf(JP_LOG_FATAL,"jpsqlite_ToDoINS(): ret=%d, error=%s, Id=%d, rolling back\n%s\n",
		sqlRet, sqlErr, errId, sqlite3_errmsg(db.conn));
	sqlite3_finalize(db.stmtToDoINS);
	//sqlite3_exec(db.conn,"ROLLBACK TRANSACTION",NULL,NULL,NULL);
	return EXIT_FAILURE;
}



int jpsqlite_ExpenseUPD(struct Expense *ex, PCRecType rt, unsigned char attrib, unsigned int *unique_id) {
	int sqlRet = 0, errId;
	const char *sqlErr = "";
	char date[32];

	jp_logf(JP_LOG_DEBUG,"jpsqlite_ExpenseUPD(): rt=%d, category=%d, unique_id=%u\n",rt,attrib&0X0F,*unique_id);

	CHK(sqlite3_bind_int(db.stmtExpenseUPD,1,attrib & 0x0F),"ExpenseUpdB1")
	strftime(date,32,"%F",&(ex->date));
	CHK(sqlite3_bind_text(db.stmtExpenseUPD,2,date,-1,SQLITE_STATIC),"ExpenseUpdB2")
	CHK(sqlite3_bind_int(db.stmtExpenseUPD,3,ex->type),"ExpenseUpdB3")
	CHK(sqlite3_bind_int(db.stmtExpenseUPD,4,ex->payment),"ExpenseUpdB4")
	CHK(sqlite3_bind_int(db.stmtExpenseUPD,5,ex->currency),"ExpenseUpdB5")
	CHK(sqlite3_bind_text(db.stmtExpenseUPD,6,ex->amount,-1,SQLITE_STATIC),"ExpenseUpdB6")
	CHK(sqlite3_bind_text(db.stmtExpenseUPD,7,ex->vendor,-1,SQLITE_STATIC),"ExpenseUpdB7")
	CHK(sqlite3_bind_text(db.stmtExpenseUPD,8,ex->city,-1,SQLITE_STATIC),"ExpenseUpdB8")
	CHK(sqlite3_bind_text(db.stmtExpenseUPD,9,ex->attendees,-1,SQLITE_STATIC),"ExpenseUpdB9")
	CHK(sqlite3_bind_text(db.stmtExpenseUPD,10,ex->note,-1,SQLITE_STATIC),"ExpenseUpdB10")
	CHK(sqlite3_bind_int(db.stmtExpenseUPD,11,*unique_id),"ExpenseUpdB11")
	CHKDONE(sqlite3_step(db.stmtExpenseUPD),"ExpenseUpdST")

	CHK(sqlite3_clear_bindings(db.stmtExpenseUPD),"ExpenseUpdCL")
	CHK(sqlite3_reset(db.stmtExpenseUPD),"ExpenseUpdRST")

	return EXIT_SUCCESS;

err:
	jp_logf(JP_LOG_FATAL,"jpsqlite_ExpenseUPD(): ret=%d, error=%s, Id=%d, rolling back\n%s\n",
		sqlRet, sqlErr, errId, sqlite3_errmsg(db.conn));
	sqlite3_finalize(db.stmtExpenseUPD);
	//sqlite3_exec(db.conn,"ROLLBACK TRANSACTION",NULL,NULL,NULL);
	return EXIT_FAILURE;
}



int jpsqlite_ExpenseINS(struct Expense *ex, unsigned char attrib) {
	int sqlRet = 0, errId;
	const char *sqlErr = "";
	char date[32];

	jp_logf(JP_LOG_DEBUG,"jpsqlite_ExpenseINS(): category=%d, maxIdExpense=%d\n",attrib&0X0F,db.maxIdExpense);
	errId = db.maxIdExpense;

	CHK(sqlite3_bind_int(db.stmtExpenseINS,1,db.maxIdExpense),"ExpenseInsB1")
	CHK(sqlite3_bind_int(db.stmtExpenseINS,2,attrib & 0x0F),"ExpenseInsB2")
	strftime(date,32,"%F",&(ex->date));
	CHK(sqlite3_bind_text(db.stmtExpenseINS,3,date,-1,SQLITE_STATIC),"ExpenseInsB3")
	CHK(sqlite3_bind_int(db.stmtExpenseINS,4,ex->type),"ExpenseInsB4")
	CHK(sqlite3_bind_int(db.stmtExpenseINS,5,ex->payment),"ExpenseInsB5")
	CHK(sqlite3_bind_int(db.stmtExpenseINS,6,ex->currency),"ExpenseInsB6")
	CHK(sqlite3_bind_text(db.stmtExpenseINS,7,ex->amount,-1,SQLITE_STATIC),"ExpenseInsB7")
	CHK(sqlite3_bind_text(db.stmtExpenseINS,8,ex->vendor,-1,SQLITE_STATIC),"ExpenseInsB8")
	CHK(sqlite3_bind_text(db.stmtExpenseINS,9,ex->city,-1,SQLITE_STATIC),"ExpenseInsB9")
	CHK(sqlite3_bind_text(db.stmtExpenseINS,10,ex->attendees,-1,SQLITE_STATIC),"ExpenseInsB10")
	CHK(sqlite3_bind_text(db.stmtExpenseINS,11,ex->note,-1,SQLITE_STATIC),"ExpenseInsB11")
	CHKDONE(sqlite3_step(db.stmtExpenseINS),"ExpenseInsST")
	db.maxIdExpense += 1;
	CHK(sqlite3_clear_bindings(db.stmtExpenseINS),"ExpenseInsCL")
	CHK(sqlite3_reset(db.stmtExpenseINS),"ExpenseInsRST")

	return EXIT_SUCCESS;

err:
	jp_logf(JP_LOG_FATAL,"jpsqlite_ExpenseINS(): ret=%d, error=%s, Id=%d, rolling back\n%s\n",
		sqlRet, sqlErr, errId, sqlite3_errmsg(db.conn));
	sqlite3_finalize(db.stmtExpenseINS);
	//sqlite3_exec(db.conn,"ROLLBACK TRANSACTION",NULL,NULL,NULL);
	return EXIT_FAILURE;
}



int jpsqlite_Delete(AppType app_type, void *VP) {
	int sqlRet = 0, errId;
	const char *sqlErr = "";
	MyAppointment *mappt;
	//MyCalendarEvent *mcale;
	MyAddress *maddr;
	//MyContact *mcont;
	MyToDo *mtodo;
	MyMemo *mmemo;
	struct MyExpense *mex;

	jp_logf(JP_LOG_DEBUG,"jpsqlite_Delete(): app_type=%d\n",app_type);

	switch (app_type) {
		case DATEBOOK:
			mappt = (MyAppointment*) VP;
			CHK(sqlite3_bind_int(db.stmtDatebookDEL,1,errId=mappt->unique_id),"DatebookDelB1")
			CHKDONE(sqlite3_step(db.stmtDatebookDEL),"DatebookDelST")
			CHK(sqlite3_clear_bindings(db.stmtDatebookDEL),"DatebookDelCL")
			CHK(sqlite3_reset(db.stmtDatebookDEL),"DatebookDelRST")
			break;
		case CALENDAR:
			//mcale = (MyCalendarEvent*) VP;
			jp_logf(JP_LOG_FATAL,"SQLite3: Calendar-delete not implemented");
			break;
		case ADDRESS:
			maddr = (MyAddress*) VP;
			CHK(sqlite3_bind_int(db.stmtAddrDEL,1,errId=maddr->unique_id),"AddrDelB1")
			CHKDONE(sqlite3_step(db.stmtAddrDEL),"AddrDelST")
			CHK(sqlite3_clear_bindings(db.stmtAddrDEL),"AddrDelCL")
			CHK(sqlite3_reset(db.stmtAddrDEL),"AddrDelRST")
			break;
		case CONTACTS:
			//mcont = (MyContact*) VP;
			jp_logf(JP_LOG_FATAL,"SQLite3: Contact-delete not implemented");
			break;
		case TODO:
			mtodo = (MyToDo*) VP;
			CHK(sqlite3_bind_int(db.stmtToDoDEL,1,errId=mtodo->unique_id),"ToDoDelB1")
			CHKDONE(sqlite3_step(db.stmtToDoDEL),"ToDoDelST")
			CHK(sqlite3_clear_bindings(db.stmtToDoDEL),"ToDoDelCL")
			CHK(sqlite3_reset(db.stmtToDoDEL),"ToDoDelRST")
			break;
		case MEMO:
			mmemo = (MyMemo*) VP;
			CHK(sqlite3_bind_int(db.stmtMemoDEL,1,errId=mmemo->unique_id),"MemoDelB1")
			CHKDONE(sqlite3_step(db.stmtMemoDEL),"MemoDelST")
			CHK(sqlite3_clear_bindings(db.stmtMemoDEL),"MemoDelCL")
			CHK(sqlite3_reset(db.stmtMemoDEL),"MemoDelRST")
			break;
		case MEMOS+2:	// hacky: should be part of AppType enum
			mex = (struct MyExpense*) VP;
			CHK(sqlite3_bind_int(db.stmtExpenseDEL,1,errId=mex->unique_id),"ExpenseDelB1")
			CHKDONE(sqlite3_step(db.stmtExpenseDEL),"ExpenseDelST")
			CHK(sqlite3_clear_bindings(db.stmtExpenseDEL),"ExpenseDelCL")
			CHK(sqlite3_reset(db.stmtExpenseDEL),"ExpenseDelRST")
			break;
		default:
			return EXIT_SUCCESS;
	}

	return EXIT_SUCCESS;

err:
	jp_logf(JP_LOG_FATAL,"jpsqlite_Delete(): ret=%d, error=%s, Id=%d, rolling back\n%s\n",
		sqlRet, sqlErr, errId, sqlite3_errmsg(db.conn));
	//sqlite3_finalize(db.XXX);
	//sqlite3_exec(db.conn,"ROLLBACK TRANSACTION",NULL,NULL,NULL);
	return EXIT_FAILURE;
}



int jpsqlite_prtAddrAppInfo(struct AddressAppInfo *ai) {
	int i;

	jp_logf(JP_LOG_DEBUG,"jpsqlite_prtAddrAppInfo()\n");
	jp_logf(JP_LOG_DEBUG,"\ttype=%d, country=%d, sortByCompany=%d\n",ai->type,ai->country,ai->sortByCompany);
	for (i=0; i<22; ++i)
		jp_logf(JP_LOG_DEBUG,"\tlabelRenamed[%02d]=%d, labels[%02d]=%s\n",i,ai->labelRenamed[i],i,ai->labels[i]);
	for (i=0; i<8; ++i)
		jp_logf(JP_LOG_DEBUG,"\tphoneLabels[%d]=%s\n",i,ai->phoneLabels[i]);

	return EXIT_SUCCESS;
}



int jpsqlite_AddrLabelSEL(struct AddressAppInfo *ai) {
	int sqlRet = 0, errId, i, id;
	const char *sqlErr = "", *label;

	jp_logf(JP_LOG_DEBUG,"jpsqlite_AddrLabelSEL()\n");

	memset(ai->labels,0,sizeof(ai->labels));	// clear label
	for (i=0; i<19; ++i) {
		errId = i;
		CHKROW(sqlite3_step(db.stmtAddrLabelSEL),"AddrLabelSelST")
		id = sqlite3_column_int(db.stmtAddrLabelSEL,0);
		if (i != id) { i = id; jp_logf(JP_LOG_FATAL,"jpsqlite_AddrLabelSEL(),stmtAddrLabelSEL: i != id"); }
		label = (const char*)sqlite3_column_text(db.stmtAddrLabelSEL,1);	// returns zero-terminated string
		if (label) strncpy(ai->labels[i],label,15);
	}
	CHK(sqlite3_reset(db.stmtAddrLabelSEL),"AddrLabelSelRST")

	return EXIT_SUCCESS;

err:
	jp_logf(JP_LOG_FATAL,"jpsqlite_AddrLabelSEL(): ret=%d, error=%s, Id=%d, rolling back\n%s\n",
		sqlRet, sqlErr, errId, sqlite3_errmsg(db.conn));
	return EXIT_FAILURE;
}




int jpsqlite_PhoneLabelSEL(struct AddressAppInfo *ai) {
	int sqlRet = 0, errId, i, id;
	const char *sqlErr = "", *label;

	jp_logf(JP_LOG_DEBUG,"jpsqlite_PhoneLabelSEL()\n");

	memset(ai->phoneLabels,0,sizeof(ai->phoneLabels));	// clear all labels
	for (i=0; i<8; ++i) {
		errId = i;
		CHKROW(sqlite3_step(db.stmtPhoneLabelSEL),"PhoneLabelLST")
		id = sqlite3_column_int(db.stmtPhoneLabelSEL,0);
		if (i != id) { i = id; jp_logf(JP_LOG_FATAL,"jpsqlite_PhoneLabelSEL(),stmtPhoneLabelSEL: i != id"); }
		label = (const char*)sqlite3_column_text(db.stmtPhoneLabelSEL,1);	// returns zero-terminated string
		if (label) strncpy(ai->phoneLabels[i],label,15);	// 16th bytes must be '\0'
	}
	CHK(sqlite3_reset(db.stmtPhoneLabelSEL),"PhoneLabelSelRST")

	return EXIT_SUCCESS;

err:
	jp_logf(JP_LOG_FATAL,"jpsqlite_PhoneLabelSEL(): ret=%d, error=%s, Id=%d, rolling back\n%s\n",
		sqlRet, sqlErr, errId, sqlite3_errmsg(db.conn));
	return EXIT_FAILURE;
}



int jpsqlite_AddrCatSEL(struct AddressAppInfo *ai) {
	int sqlRet = 0, errId, i;
	const char *sqlErr = "", *label;

	jp_logf(JP_LOG_DEBUG,"jpsqlite_AddrCatSEL()\n");

	memset(ai->category.name,0,sizeof(ai->category.name));	// clear all category names
	memset(ai->category.ID,0,sizeof(ai->category.ID));	// clear all category ID's
	for (i=0; i<16; ++i) {
		errId = i;
		if ((sqlRet = sqlite3_step(db.stmtAddrCategorySEL)) == SQLITE_DONE) break;
		if (sqlRet != SQLITE_ROW) { sqlErr="AddrCatST"; goto err; }
		ai->category.ID[i] = sqlite3_column_int(db.stmtAddrCategorySEL,0);
		//if (i != id) { i = id; jp_logf(JP_LOG_FATAL,"jpsqlite_AddrAppInfo(),stmtAddrCategorySEL: i != id"); }
		label = (const char*)sqlite3_column_text(db.stmtAddrCategorySEL,1);	// returns zero-terminated string
		if (label) strncpy(ai->category.name[i],label,15);	// 16th bytes must be '\0'
	}
	CHK(sqlite3_reset(db.stmtAddrCategorySEL),"AddrCatSelRST")

	return EXIT_SUCCESS;

err:
	jp_logf(JP_LOG_FATAL,"jpsqlite_AddrAppInfo(): ret=%d, error=%s, Id=%d, rolling back\n%s\n",
		sqlRet, sqlErr, errId, sqlite3_errmsg(db.conn));
	return EXIT_FAILURE;
}



// Datebook doesn't use categories, therefore only a simplified version jpsqlite_DatebookCatSEL()
int jpsqlite_DatebookCatSEL(struct CalendarAppInfo *ai) {
	memset(ai->category.name,0,sizeof(ai->category.name));	// clear all category names
	memset(ai->category.ID,0,sizeof(ai->category.ID));	// clear all category ID's

	return EXIT_SUCCESS;
}



int jpsqlite_MemoCatSEL(struct MemoAppInfo *ai) {
	int sqlRet = 0, errId, i;
	const char *sqlErr = "", *label;

	jp_logf(JP_LOG_DEBUG,"jpsqlite_MemoCatSEL()\n");

	memset(ai->category.name,0,sizeof(ai->category.name));	// clear all category names
	memset(ai->category.ID,0,sizeof(ai->category.ID));	// clear all category ID's
	for (i=0; i<16; ++i) {
		errId = i;
		if ((sqlRet = sqlite3_step(db.stmtMemoCategorySEL)) == SQLITE_DONE) break;
		if (sqlRet != SQLITE_ROW) { sqlErr="MemCatST"; goto err; }
		ai->category.ID[i] = sqlite3_column_int(db.stmtMemoCategorySEL,0);
		//if (i != id) { i = id; jp_logf(JP_LOG_FATAL,"jpsqlite_MemoCatSEL(),stmtMemoCategorySEL: i != id\n"); }
		label = (const char*)sqlite3_column_text(db.stmtMemoCategorySEL,1);	// returns zero-terminated string
		if (label) strncpy(ai->category.name[i],label,15);	// 16th bytes must be '\0'
	}
	CHK(sqlite3_reset(db.stmtMemoCategorySEL),"MemCatRST")

	return EXIT_SUCCESS;

err:
	jp_logf(JP_LOG_FATAL,"jpsqlite_MemoCatSEL(): ret=%d, error=%s, Id=%d, rolling back\n%s\n",
		sqlRet, sqlErr, errId, sqlite3_errmsg(db.conn));
	return EXIT_FAILURE;
}



int jpsqlite_ToDoCatSEL(struct ToDoAppInfo *ai) {
	int sqlRet = 0, errId, i;
	const char *sqlErr = "", *label;

	jp_logf(JP_LOG_DEBUG,"jpsqlite_ToDoCatSEL()\n");

	memset(ai->category.name,0,sizeof(ai->category.name));	// clear all category names
	memset(ai->category.ID,0,sizeof(ai->category.ID));	// clear all category ID's
	for (i=0; i<16; ++i) {
		errId = i;
		if ((sqlRet = sqlite3_step(db.stmtToDoCategorySEL)) == SQLITE_DONE) break;
		if (sqlRet != SQLITE_ROW) { sqlErr="ToDoCatST"; goto err; }
		ai->category.ID[i] = sqlite3_column_int(db.stmtToDoCategorySEL,0);
		//if (i != id) { i = id; jp_logf(JP_LOG_FATAL,"jpsqlite_ToDoCatSEL(),stmtToDoCategorySEL: i != id\n"); }
		label = (const char*)sqlite3_column_text(db.stmtToDoCategorySEL,1);	// returns zero-terminated string
		//memset(ai->category.name[i],0,16);	// clear category name
		if (label) strncpy(ai->category.name[i],label,15);	// 16th bytes must be '\0'
	}
	CHK(sqlite3_reset(db.stmtToDoCategorySEL),"ToDoCatRST")

	return EXIT_SUCCESS;

err:
	jp_logf(JP_LOG_FATAL,"jpsqlite_ToDoCatSEL(): ret=%d, error=%s, Id=%d, rolling back\n%s\n",
		sqlRet, sqlErr, errId, sqlite3_errmsg(db.conn));
	return EXIT_FAILURE;
}



int jpsqlite_ExpenseCatSEL(struct ExpenseAppInfo *ai) {
	int sqlRet = 0, errId, i;
	const char *sqlErr = "", *label;

	jp_logf(JP_LOG_DEBUG,"jpsqlite_ExpenseCatSEL()\n");

	memset(ai->category.name,0,sizeof(ai->category.name));	// clear all category names
	memset(ai->category.ID,0,sizeof(ai->category.ID));	// clear all category ID's
	for (i=0; i<16; ++i) {
		errId = i;
		if ((sqlRet = sqlite3_step(db.stmtExpenseCategorySEL)) == SQLITE_DONE) break;
		if (sqlRet != SQLITE_ROW) { sqlErr="ExpenseCatST"; goto err; }
		ai->category.ID[i] = sqlite3_column_int(db.stmtExpenseCategorySEL,0);
		//if (i != id) { i = id; jp_logf(JP_LOG_FATAL,"jpsqlite_ExpenseCatSEL(),stmtExpenseCategorySEL: i != id"); }
		label = (const char*)sqlite3_column_text(db.stmtExpenseCategorySEL,1);	// returns zero-terminated string
		//memset(ai->category.name[i],0,16);	// clear category name
		if (label) strncpy(ai->category.name[i],label,15);	// 16th bytes must be '\0'
	}
	CHK(sqlite3_reset(db.stmtExpenseCategorySEL),"ExpenseCatRST")

	return EXIT_SUCCESS;

err:
	jp_logf(JP_LOG_FATAL,"jpsqlite_ExpenseCatSEL(): ret=%d, error=%s, Id=%d, rolling back\n%s\n",
		sqlRet, sqlErr, errId, sqlite3_errmsg(db.conn));
	return EXIT_FAILURE;
}



int jpsqlite_prtAddrCont(const struct Address *a, struct Contact *c) {
	jp_logf(JP_LOG_DEBUG,"jpsqlite_prtAddrCont(): left Address, right Contact\n"
		"\tLastname=%s %s\n"
		"\tFirstname=%s %s\n"
		"\tCompany=%s %s\n"
		"\tTitle=%s %s\n"
		"\tPhone1=%s %s\n"
		"\tPhone2=%s %s\n"
		"\tPhone3=%s %s\n"
		"\tPhone4=%s %s\n"
		"\tPhone5=%s %s\n"
		"\tCustom1=%s %s\n"
		"\tCustom2=%s %s\n"
		"\tCustom3=%s %s\n"
		"\tCustom4=%s %s\n"
		"\tAddress=%s %s\n"
		"\tCity=%s %s\n"
		"\tState=%s %s\n"
		"\tZip=%s %s\n"
		"\tCountry=%s %s\n"
		"\tNote=|%s| |%s|\n",
		a->entry[entryLastname],c->entry[contLastname],
		a->entry[entryFirstname],c->entry[contFirstname],
		a->entry[entryCompany], c->entry[contCompany],
		a->entry[entryTitle], c->entry[contTitle],
		a->entry[entryPhone1], c->entry[contPhone1],
		a->entry[entryPhone2], c->entry[contPhone2],
		a->entry[entryPhone3], c->entry[contPhone3],
		a->entry[entryPhone4], c->entry[contPhone4],
		a->entry[entryPhone5], c->entry[contPhone5],
		a->entry[entryCustom1], c->entry[contCustom1],
		a->entry[entryCustom2], c->entry[contCustom2],
		a->entry[entryCustom3], c->entry[contCustom3],
		a->entry[entryCustom4], c->entry[contCustom4],
		a->entry[entryAddress], c->entry[contAddress1],
		a->entry[entryCity], c->entry[contCity1],
		a->entry[entryState], c->entry[contState1],
		a->entry[entryZip], c->entry[contZip1],
		a->entry[entryCountry], c->entry[contCountry1],
		a->entry[entryNote], c->entry[contNote]
	);
	return 0;
}



/*
 * sort_order: SORT_ASCENDING | SORT_DESCENDING
 * modified, deleted and private, 0 for no, 1 for yes, 2 for use prefs
 *
 * Elmar Klausmeier, 27-Sep-2022: modified + deleted flag are ignored
 */
int jpsqlite_AddrSEL(AddressList **address_list, int sort_order, int privates, int category) {
	int sqlRet=0, errId=0, private0=0, private1=0, recs_returned=0, cat, priv;
	const char *sqlErr="";
	AddressList *tm, *tmprev=NULL;
	sqlite3_stmt *stmt;

	jp_logf(JP_LOG_DEBUG,"jpsqlite_AddrSEL(): sort_order=%d, privates=%d, category=%d\n",sort_order,privates,category);

	*address_list = NULL;
	switch (sort_order) {
		case SORT_BY_LNAME:
			stmt = db.stmtAddrSELo1;
			break;
		case SORT_BY_FNAME:
			stmt = db.stmtAddrSELo2;
			break;
		case SORT_BY_COMPANY:
			stmt = db.stmtAddrSELo3;
			break;
		default:
			jp_logf(JP_LOG_DEBUG,"jpsqlite_AddrSEL(): sort_order=%d out of range, harmless error\n",sort_order);
			stmt = db.stmtAddrSELo3;
			break;
	}
	if (privates == 2) privates = show_privates(GET_PRIVATES);
	if (privates == 1) private1 = 1;
	CHK(sqlite3_bind_int(stmt,1,private0),"AddrSELB1")	// not really necessary as always zero
	CHK(sqlite3_bind_int(stmt,2,private1),"AddrSELB2")
	CHK(sqlite3_bind_int(stmt,3,category),"AddrSELB3")

	for (errId=1; errId<750000; ++errId) {
		if ((sqlRet = sqlite3_step(stmt)) == SQLITE_DONE) break;
		if (sqlRet != SQLITE_ROW) { sqlErr="ADDRSELST"; goto err; }
		// calloc important here so as to clear out all(!) pointers
		if ((tm = calloc(1,sizeof(AddressList))) == NULL) { sqlErr="tm"; goto errAlloc; }
		tm->maddr.unique_id = sqlite3_column_int(stmt,0);
		cat = sqlite3_column_int(stmt,1);
		priv = sqlite3_column_int(stmt,2);
		tm->maddr.attrib = (cat & 0x0F) | (priv << 4);
		tm->maddr.addr.showPhone = sqlite3_column_int(stmt,3);
		ALLOCN(tm->maddr.addr.entry[entryLastname],sqlite3_column_text(stmt,4),"tm->maddr.addr.entry[entryLastname]")
		ALLOCN(tm->maddr.addr.entry[entryFirstname],sqlite3_column_text(stmt,5),"tm->maddr.addr.entry[entryFirstname]")
		ALLOCN(tm->maddr.addr.entry[entryTitle],sqlite3_column_text(stmt,6),"tm->maddr.addr.entry[entryTitle]")
		ALLOCN(tm->maddr.addr.entry[entryCompany],sqlite3_column_text(stmt,7),"tm->maddr.addr.entry[entryCompany]")
		tm->maddr.addr.phoneLabel[0] = sqlite3_column_int(stmt,8);
		tm->maddr.addr.phoneLabel[1] = sqlite3_column_int(stmt,9);
		tm->maddr.addr.phoneLabel[2] = sqlite3_column_int(stmt,10);
		tm->maddr.addr.phoneLabel[3] = sqlite3_column_int(stmt,11);
		tm->maddr.addr.phoneLabel[4] = sqlite3_column_int(stmt,12);
		ALLOCN(tm->maddr.addr.entry[entryPhone1],sqlite3_column_text(stmt,13),"tm->maddr.addr.entry[entryPhone1]")
		ALLOCN(tm->maddr.addr.entry[entryPhone2],sqlite3_column_text(stmt,14),"tm->maddr.addr.entry[entryPhone2]")
		ALLOCN(tm->maddr.addr.entry[entryPhone3],sqlite3_column_text(stmt,15),"tm->maddr.addr.entry[entryPhone3]")
		ALLOCN(tm->maddr.addr.entry[entryPhone4],sqlite3_column_text(stmt,16),"tm->maddr.addr.entry[entryPhone4]")
		ALLOCN(tm->maddr.addr.entry[entryPhone5],sqlite3_column_text(stmt,17),"tm->maddr.addr.entry[entryPhone5]")
		ALLOCN(tm->maddr.addr.entry[entryAddress],sqlite3_column_text(stmt,18),"tm->maddr.addr.entry[entryAddress]")
		ALLOCN(tm->maddr.addr.entry[entryCity],sqlite3_column_text(stmt,19),"tm->maddr.addr.entry[entryCity]")
		ALLOCN(tm->maddr.addr.entry[entryState],sqlite3_column_text(stmt,20),"tm->maddr.addr.entry[entryState]")
		ALLOCN(tm->maddr.addr.entry[entryZip],sqlite3_column_text(stmt,21),"tm->maddr.addr.entry[entryZip]")
		ALLOCN(tm->maddr.addr.entry[entryCountry],sqlite3_column_text(stmt,22),"tm->maddr.addr.entry[entryCountry]")
		ALLOCN(tm->maddr.addr.entry[entryCustom1],sqlite3_column_text(stmt,23),"tm->maddr.addr.entry[entryCustom1]")
		ALLOCN(tm->maddr.addr.entry[entryCustom2],sqlite3_column_text(stmt,24),"tm->maddr.addr.entry[entryCustom2]")
		ALLOCN(tm->maddr.addr.entry[entryCustom3],sqlite3_column_text(stmt,25),"tm->maddr.addr.entry[entryCustom3]")
		ALLOCN(tm->maddr.addr.entry[entryCustom4],sqlite3_column_text(stmt,26),"tm->maddr.addr.entry[entryCustom4]")
		ALLOCN(tm->maddr.addr.entry[entryNote],sqlite3_column_text(stmt,27),"tm->maddr.addr.entry[entryNote]")
		tm->app_type = ADDRESS;
		//tm->next = NULL;	// already zero due to calloc()
		tm->maddr.rt = PALM_REC;	//NEW_PC_REC;
		if (*address_list == NULL) *address_list = tm;	// start of list
		else tmprev->next = tm;	// previous points to current
		tmprev = tm;	// remember for next round
		recs_returned++;
#ifdef JPILOT_DEBUG
		jp_logf(JP_LOG_DEBUG,"jpsqlite_AddrSEL():\n"
			"\tattrib=%0x\n"
			"\tLastname=%s\n"
			"\tFirstname=%s\n"
			"\tCompany=%s\n"
			"\tTitle=%s\n"
			"\tPhoneLabel1-5=%d %d %d %d %d\n"
			"\tPhone1=%s\n"
			"\tPhone2=%s\n"
			"\tPhone3=%s\n"
			"\tPhone4=%s\n"
			"\tPhone5=%s\n"
			"\tCustom1=%s\n"
			"\tCustom2=%s\n"
			"\tCustom3=%s\n"
			"\tCustom4=%s\n"
			"\tAddress=%s\n"
			"\tCity=%s\n"
			"\tState=%s\n"
			"\tZip=%s\n"
			"\tCountry=%s\n"
			"\tNote=|%s|\n",
			tm->maddr.attrib,
			tm->maddr.addr.entry[entryLastname],
			tm->maddr.addr.entry[entryFirstname],
			tm->maddr.addr.entry[entryCompany],
			tm->maddr.addr.entry[entryTitle],
			tm->maddr.addr.phoneLabel[0], tm->maddr.addr.phoneLabel[1], tm->maddr.addr.phoneLabel[2], tm->maddr.addr.phoneLabel[3], tm->maddr.addr.phoneLabel[4],
			tm->maddr.addr.entry[entryPhone1],
			tm->maddr.addr.entry[entryPhone2],
			tm->maddr.addr.entry[entryPhone3],
			tm->maddr.addr.entry[entryPhone4],
			tm->maddr.addr.entry[entryPhone5],
			tm->maddr.addr.entry[entryCustom1],
			tm->maddr.addr.entry[entryCustom2],
			tm->maddr.addr.entry[entryCustom3],
			tm->maddr.addr.entry[entryCustom4],
			tm->maddr.addr.entry[entryAddress],
			tm->maddr.addr.entry[entryCity],
			tm->maddr.addr.entry[entryState],
			tm->maddr.addr.entry[entryZip],
			tm->maddr.addr.entry[entryCountry],
			tm->maddr.addr.entry[entryNote]
		);
#endif
	}
	CHK(sqlite3_clear_bindings(stmt),"ADDRSELCL")
	CHK(sqlite3_reset(stmt),"ADDRSELRST")
	return recs_returned;

err:
	jp_logf(JP_LOG_FATAL,"jpsqlite_AddrSEL(): ret=%d, error=%s, Id=%d, rolling back\n%s\n",
		sqlRet, sqlErr, errId, sqlite3_errmsg(db.conn));
	sqlite3_clear_bindings(stmt);
	sqlite3_reset(stmt);
	return 0;	// zero elements

errAlloc:
	jp_logf(JP_LOG_FATAL,"jpsqlite_AddrSEL(): memory allocation error for %s\n",sqlErr);
	sqlite3_clear_bindings(stmt);
	sqlite3_reset(stmt);
	return 0;	// zero elements
}



/*
 * If NULL is passed in for date, then all appointments will be returned.
 * Otherwise return all appointments for date=now.
 * modified, deleted and private, 0 for no, 1 for yes, 2 for use prefs
 */
int jpsqlite_DatebookSEL(CalendarEventList **calendar_event_list, struct tm *now, int privates) {
	int i, offset, sqlRet=0, errId=0, private1=0, recs_returned=0, priv, iNow=0, tmUsed=1;
	const char *sqlErr="", *begin, *end, *pRepeatEnd;
	char pNow[16];
	const char *excp;
	CalendarEventList *tm=NULL, *tmprev=NULL;

	*calendar_event_list = NULL;
	pNow[0] = '\0';
	if (now) { iNow = 1; strftime(pNow,16,"%F",now); }
	jp_logf(JP_LOG_DEBUG,"jpsqlite_DatebookSEL(): privates=%d, now=%s\n",privates,pNow);
	if (privates == 2) privates = show_privates(GET_PRIVATES);
	if (privates == 1) private1 = 1;
	//CHK(sqlite3_bind_int(db.stmtDatebookSEL,1,private0),"DatebookSelB1")
	CHK(sqlite3_bind_int(db.stmtDatebookSEL,1,private1),"DatebookSelB1")
	CHK(sqlite3_bind_text(db.stmtDatebookSEL,2,pNow,-1,SQLITE_STATIC),"DatebookSelB2")
	CHK(sqlite3_bind_int(db.stmtDatebookSEL,3,iNow),"DatebookSelB3")

	for (errId=1; errId<750000; ++errId) {
		if ((sqlRet = sqlite3_step(db.stmtDatebookSEL)) == SQLITE_DONE) break;
		if (sqlRet != SQLITE_ROW) { sqlErr="DatebookSelST"; goto err; }
		if (tmUsed) {
			// calloc important here so as to clear out all(!) pointers
			if ((tm = calloc(1,sizeof(CalendarEventList))) == NULL) { sqlErr="tm"; goto errAlloc; }
		} else {
			memset(tm,'\0',sizeof(CalendarEventList));	// clear previous stuff
		}
/***
		// Data cleansing
		if (c->mcale.cale.repeatEnd.tm_year < 2
		||  c->mcale.cale.repeatEnd.tm_year > 2050)
			pRepeatEnd = NULL;
		else
			strftime(pRepeatEnd=repeatEnd,32,"%F",&c->mcale.cale.repeatEnd);
***/
		tm->mcale.unique_id = sqlite3_column_int(db.stmtDatebookSEL,0);
		priv = sqlite3_column_int(db.stmtDatebookSEL,1);
		tm->mcale.attrib = priv << 4;
		tm->mcale.cale.event = sqlite3_column_int(db.stmtDatebookSEL,2);
		begin = (const char*)sqlite3_column_text(db.stmtDatebookSEL,3);
		if (begin && strlen(begin) >= 16) strptime(begin,"%FT%R",&(tm->mcale.cale.begin));
		end = (const char*)sqlite3_column_text(db.stmtDatebookSEL,4);
		if (end && strlen(end) >= 16) strptime(end,"%FT%R",&(tm->mcale.cale.end));
		tm->mcale.cale.alarm = sqlite3_column_int(db.stmtDatebookSEL,5);
		tm->mcale.cale.advance = sqlite3_column_int(db.stmtDatebookSEL,6);
		tm->mcale.cale.advanceUnits = sqlite3_column_int(db.stmtDatebookSEL,7);
		tm->mcale.cale.repeatType = sqlite3_column_int(db.stmtDatebookSEL,8);
		// old: 1-x
		tm->mcale.cale.repeatForever = (sqlite3_column_int(db.stmtDatebookSEL,9) & 0x01);
		pRepeatEnd = (const char*)sqlite3_column_text(db.stmtDatebookSEL,10);
		if (pRepeatEnd && strlen(pRepeatEnd) >= 10) strptime(pRepeatEnd,"%F",&(tm->mcale.cale.repeatEnd));
		tm->mcale.cale.repeatFrequency = sqlite3_column_int(db.stmtDatebookSEL,11);
		tm->mcale.cale.repeatDay = sqlite3_column_int(db.stmtDatebookSEL,12);
		tm->mcale.cale.repeatDays[0] = sqlite3_column_int(db.stmtDatebookSEL,13);
		tm->mcale.cale.repeatDays[1] = sqlite3_column_int(db.stmtDatebookSEL,14);
		tm->mcale.cale.repeatDays[2] = sqlite3_column_int(db.stmtDatebookSEL,15);
		tm->mcale.cale.repeatDays[3] = sqlite3_column_int(db.stmtDatebookSEL,16);
		tm->mcale.cale.repeatDays[4] = sqlite3_column_int(db.stmtDatebookSEL,17);
		tm->mcale.cale.repeatDays[5] = sqlite3_column_int(db.stmtDatebookSEL,18);
		tm->mcale.cale.repeatDays[6] = sqlite3_column_int(db.stmtDatebookSEL,19);
		tm->mcale.cale.exceptions = sqlite3_column_int(db.stmtDatebookSEL,20);
		if (tm->mcale.cale.exceptions == 0) {
			tm->mcale.cale.exception = NULL;
		} else if ((excp = sqlite3_column_text(db.stmtDatebookSEL,21)) != NULL) {
			if ((tm->mcale.cale.exception = calloc(tm->mcale.cale.exceptions,sizeof(struct tm))) == NULL) {
				sqlErr = "tm->mcale.cale.exception";
				goto errAlloc;
			}
			for (i=0,offset=0; i<tm->mcale.cale.exceptions; ++i,offset+=11) {
				strptime(excp+offset,"%F",tm->mcale.cale.exception+i);
				tm->mcale.cale.exception[i].tm_hour = 12;	// probably useful for dateToDays() calculation
			}
		}
		if (now && !calendar_isApptOnDate(&(tm->mcale.cale),now) ) {
			tmUsed = 0;	// we can re-use tm
			if (tm->mcale.cale.exception) free(tm->mcale.cale.exception);
			continue;	// similar to logic in calendar.c:get_days_calendar_events2()
		}
		ALLOCN(tm->mcale.cale.description,sqlite3_column_text(db.stmtDatebookSEL,22),"tm->mcale.cale.description")
		ALLOCN(tm->mcale.cale.note,sqlite3_column_text(db.stmtDatebookSEL,23),"tm->mcale.cale.note")
		tm->app_type = CALENDAR;
		//tm->next = NULL;	// already zero due to calloc()
		tm->mcale.rt = PALM_REC;	//NEW_PC_REC;
		if (*calendar_event_list == NULL) *calendar_event_list = tm;	// start of list
		else tmprev->next = tm;	// previous points to current
		tmprev = tm;	// remember for next round
		tmUsed = 1;	// tm is part of linked list
		recs_returned++;
	}
	if (tmUsed == 0) free(tm);
	CHK(sqlite3_clear_bindings(db.stmtDatebookSEL),"DatebookSelCL")
	CHK(sqlite3_reset(db.stmtDatebookSEL),"DatebookSelRST")
	return recs_returned;

err:
	jp_logf(JP_LOG_FATAL,"jpsqlite_DatebookSEL(): ret=%d, error=%s, Id=%d, rolling back\n%s\n",
		sqlRet, sqlErr, errId, sqlite3_errmsg(db.conn));
	sqlite3_clear_bindings(db.stmtDatebookSEL);
	sqlite3_reset(db.stmtDatebookSEL);
	return 0;	// zero elements

errAlloc:
	jp_logf(JP_LOG_FATAL,"jpsqlite_DatebookSEL(): memory allocation error for %ss\n",sqlErr);
	sqlite3_clear_bindings(db.stmtDatebookSEL);
	sqlite3_reset(db.stmtDatebookSEL);
	return 0;	// zero elements
}



/*
 * sort_order: SORT_ASCENDING | SORT_DESCENDING (used to keep pdb sort order
 *                                               but not yet implemented)
 * modified, deleted, private: 0 for no, 1 for yes, 2 for use prefs
 *
 * Elmar Klausmeier, 26-Sep-2022: sort_order + modified + deleted flag are ignored
 * Memos are always returned sorted in ascending order, see stmtMemoSEL
 */
int jpsqlite_MemoSEL(MemoList **memo_list, int privates, int category) {
	int sqlRet=0, errId=0, private0=0, private1=0, recs_returned=0, cat, priv;
	const char *sqlErr="";
	MemoList *tm, *tmprev=NULL;

	jp_logf(JP_LOG_DEBUG,"jpsqlite_MemoSEL(): privates=%d, category=%d\n",privates,category);

	*memo_list = NULL;
	if (privates == 2) privates = show_privates(GET_PRIVATES);
	if (privates == 1) private1 = 1;
	CHK(sqlite3_bind_int(db.stmtMemoSEL,1,private0),"MemoSELB1")
	CHK(sqlite3_bind_int(db.stmtMemoSEL,2,private1),"MemoSELB2")
	CHK(sqlite3_bind_int(db.stmtMemoSEL,3,category),"MemoSELB3")

	for (errId=1; errId<750000; ++errId) {
		if ((sqlRet = sqlite3_step(db.stmtMemoSEL)) == SQLITE_DONE) break;
		if (sqlRet != SQLITE_ROW) { sqlErr="MemoSELST"; goto err; }
		// calloc important here so as to clear out all(!) pointers
		if ((tm = calloc(1,sizeof(MemoList))) == NULL) { sqlErr="tm"; goto errAlloc; }
		tm->mmemo.unique_id = sqlite3_column_int(db.stmtMemoSEL,0);
		cat = sqlite3_column_int(db.stmtMemoSEL,1);
		priv = sqlite3_column_int(db.stmtMemoSEL,2);
		tm->mmemo.attrib = (cat & 0x0F) | (priv << 4);
		ALLOCN(tm->mmemo.memo.text,sqlite3_column_text(db.stmtMemoSEL,3),"tm->mmemo.memo.text")
		tm->app_type = MEMO;
		//tm->next = NULL;	// already zero due to calloc()
		tm->mmemo.rt = PALM_REC;	//NEW_PC_REC;
		if (*memo_list == NULL) *memo_list = tm;	// start of list
		else tmprev->next = tm;	// previous points to current
		tmprev = tm;	// remember for next round
		recs_returned++;
	}
	CHK(sqlite3_clear_bindings(db.stmtMemoSEL),"MemoSELCL")
	CHK(sqlite3_reset(db.stmtMemoSEL),"MemoSELRST")
	return recs_returned;

err:
	jp_logf(JP_LOG_FATAL,"jpsqlite_MemoSEL(): ret=%d, error=%s, Id=%d, rolling back\n%s\n",
		sqlRet, sqlErr, errId, sqlite3_errmsg(db.conn));
	sqlite3_clear_bindings(db.stmtMemoSEL);
	sqlite3_reset(db.stmtMemoSEL);
	return 0;	// zero elements

errAlloc:
	jp_logf(JP_LOG_FATAL,"jpsqlite_MemoSEL(): memory allocation error for %s\n",sqlErr);
	sqlite3_clear_bindings(db.stmtMemoSEL);
	sqlite3_reset(db.stmtMemoSEL);
	return 0;	// zero elements
}



/*
 * sort_order: SORT_ASCENDING | SORT_DESCENDING
 * modified, deleted, private, completed:
 *  0 for no, 1 for yes, 2 for use prefs
 *
 * Elmar Klausmeier, 27-Sep-2022: sort_order + modified + deleted + completed flag are ignored
 * ToDos are always returned sorted in order by Priority asc, Due desc, Description asc,
 * see stmtToDoSEL
 */
int jpsqlite_ToDoSEL(ToDoList **todo_list, int privates, int category) {
	int sqlRet=0, errId=0, id, private0=0, private1=0, recs_returned=0, cat, priv;
	const char *sqlErr="", *due;
	ToDoList *tm, *tmprev=NULL;

	jp_logf(JP_LOG_DEBUG,"jpsqlite_ToDoSEL(): privates=%d, category=%d\n",privates,category);

	*todo_list = NULL;
	if (privates == 2) privates = show_privates(GET_PRIVATES);
	if (privates == 1) private1 = 1;
	CHK(sqlite3_bind_int(db.stmtToDoSEL,1,private0),"TODOSELB1")
	CHK(sqlite3_bind_int(db.stmtToDoSEL,2,private1),"TODOSELB2")
	CHK(sqlite3_bind_int(db.stmtToDoSEL,3,category),"TODOSELB3")

	for (id=1; id<750000; ++id) {
		errId = id;
		if ((sqlRet = sqlite3_step(db.stmtToDoSEL)) == SQLITE_DONE) break;
		if (sqlRet != SQLITE_ROW) { sqlErr="ToDoSELST"; goto err; }
		// calloc important here so as to clear out all(!) pointers
		if ((tm = calloc(1,sizeof(ToDoList))) == NULL) { sqlErr="tm"; goto errAlloc; }
		tm->mtodo.unique_id = sqlite3_column_int(db.stmtToDoSEL,0);
		cat = sqlite3_column_int(db.stmtToDoSEL,1);
		priv = sqlite3_column_int(db.stmtToDoSEL,2);
		tm->mtodo.attrib = (cat & 0x0F) | (priv << 4);
		tm->mtodo.todo.indefinite = sqlite3_column_int(db.stmtToDoSEL,3);
		due = (const char*)sqlite3_column_text(db.stmtToDoSEL,4);	// returns zero-terminated string
		tm->mtodo.todo.priority = sqlite3_column_int(db.stmtToDoSEL,5);
		tm->mtodo.todo.complete = sqlite3_column_int(db.stmtToDoSEL,6);
		ALLOCN(tm->mtodo.todo.description,sqlite3_column_text(db.stmtToDoSEL,7),"tm->mtodo.todo.description")
		ALLOCN(tm->mtodo.todo.note,sqlite3_column_text(db.stmtToDoSEL,8),"tm->mtodo.todo.note")
		tm->app_type = TODO;
		//tm->next = NULL;	// already zero due to calloc()
		tm->mtodo.rt = PALM_REC;	//NEW_PC_REC;
		if (due && strlen(due) >= 10) strptime(due,"%F",&(tm->mtodo.todo.due));
		if (*todo_list == NULL) *todo_list = tm;	// start of list
		else tmprev->next = tm;	// previous points to current
		tmprev = tm;	// remember for next round
		recs_returned++;
	}
	CHK(sqlite3_clear_bindings(db.stmtToDoSEL),"ToDoSELCL")
	CHK(sqlite3_reset(db.stmtToDoSEL),"ToDoSELRST")
	return recs_returned;

err:
	jp_logf(JP_LOG_FATAL,"jpsqlite_ToDoSEL(): ret=%d, error=%s, Id=%d, rolling back\n%s\n",
		sqlRet, sqlErr, errId, sqlite3_errmsg(db.conn));
	sqlite3_clear_bindings(db.stmtToDoSEL);
	sqlite3_reset(db.stmtToDoSEL);
	return 0;	// zero elements

errAlloc:
	jp_logf(JP_LOG_FATAL,"jpsqlite_ToDoSEL(): memory allocation error for %s\n",sqlErr);
	sqlite3_clear_bindings(db.stmtToDoSEL);
	sqlite3_reset(db.stmtToDoSEL);
	return 0;	// zero elements
}



/* Return expense list
 * Elmar Klausmeier, 14-Oct-2022
 */
int jpsqlite_ExpenseSEL(struct MyExpense **expense_list) {
	int sqlRet=0, errId=0, id, private0=0, private1=0, recs_returned=0, cat, priv;
	const char *sqlErr="", *date;
	struct MyExpense *tm, *tmprev=NULL;

	jp_logf(JP_LOG_DEBUG,"jpsqlite_ExpenseSEL()\n");

	*expense_list = NULL;
	for (id=1; id<750000; ++id) {
		errId = id;
		if ((sqlRet = sqlite3_step(db.stmtExpenseSEL)) == SQLITE_DONE) break;
		if (sqlRet != SQLITE_ROW) { sqlErr="ExpenseSELST"; goto err; }
		// calloc important here so as to clear out all(!) pointers
		if ((tm = calloc(1,sizeof(struct MyExpense))) == NULL) { sqlErr="tm"; goto errAlloc; }
		tm->unique_id = sqlite3_column_int(db.stmtExpenseSEL,0);
		tm->attrib = sqlite3_column_int(db.stmtExpenseSEL,1) & 0x0F;
		date = (const char*)sqlite3_column_text(db.stmtExpenseSEL,2);	// returns zero-terminated string
		if (date && strlen(date) >= 10) strptime(date,"%F",&(tm->ex.date));
		tm->ex.type = sqlite3_column_int(db.stmtExpenseSEL,3);
		tm->ex.payment = sqlite3_column_int(db.stmtExpenseSEL,4);
		tm->ex.currency = sqlite3_column_int(db.stmtExpenseSEL,5);
		ALLOCN(tm->ex.amount,sqlite3_column_text(db.stmtExpenseSEL,6),"tm->ex.amount")
		ALLOCN(tm->ex.vendor,sqlite3_column_text(db.stmtExpenseSEL,7),"tm->ex.vendor")
		ALLOCN(tm->ex.city,sqlite3_column_text(db.stmtExpenseSEL,8),"tm->ex.city")
		ALLOCN(tm->ex.attendees,sqlite3_column_text(db.stmtExpenseSEL,9),"tm->ex.attendees")
		ALLOCN(tm->ex.note,sqlite3_column_text(db.stmtExpenseSEL,10),"tm->ex.note")
		//tm->next = NULL;	// already zero due to calloc()
		if (*expense_list == NULL) *expense_list = tm;	// start of list
		else tmprev->next = tm;	// previous points to current
		tmprev = tm;	// remember for next round
		recs_returned++;
	}
	CHK(sqlite3_clear_bindings(db.stmtExpenseSEL),"ExpenseSELCL")
	CHK(sqlite3_reset(db.stmtExpenseSEL),"ExpenseSELRST")
	return recs_returned;

err:
	jp_logf(JP_LOG_FATAL,"jpsqlite_ExpenseSEL(): ret=%d, error=%s, Id=%d, rolling back\n%s\n",
		sqlRet, sqlErr, errId, sqlite3_errmsg(db.conn));
	sqlite3_clear_bindings(db.stmtExpenseSEL);
	sqlite3_reset(db.stmtExpenseSEL);
	return 0;	// zero elements

errAlloc:
	jp_logf(JP_LOG_FATAL,"jpsqlite_ExpenseSEL(): memory allocation error for %s\n",sqlErr);
	sqlite3_clear_bindings(db.stmtExpenseSEL);
	sqlite3_reset(db.stmtExpenseSEL);
	return 0;	// zero elements
}



/* Write categories: delete + write-all
 * As these operations occur rarely, no prepared statements are used here
 * klm, 09-Oct-2022, 07-Nov-2022
*/
int jpsqlite_CatDELINS(char *db_name, struct CategoryAppInfo *cai) {
	int sqlRet=0, errId=-1, i;
	const char *sqlErr="";
	sqlite3_stmt *dbpstmt;

	jp_logf(JP_LOG_DEBUG,"jpsqlite_CatDELINS()\n");

	CHK(sqlite3_exec(db.conn,"BEGIN TRANSACTION",NULL,NULL,NULL),"jpsqlite_CatDELINS")

	if (strcmp(db_name,"AddressDB") == 0) {
		CHK(sqlite3_exec(db.conn,"delete from AddrCategory",NULL,NULL,NULL),"AddressCatDelEX");
		CHK(sqlite3_prepare_v2(db.conn,
			"insert into AddrCategory (Id, Label, InsertDate) "
			"values (:Id, :Label, strftime('%Y-%m-%dT%H:%M:%S', 'now'))", -1, &dbpstmt, NULL), "jpAddrCatPRE")
		jp_logf(JP_LOG_DEBUG,"\tAddress categories\n");
		for (i=0; i<16; ++i) {
			if (cai->name[i][0] == '\0') continue;
			jp_logf(JP_LOG_DEBUG,"\t\t%2d %s\n",i,cai->name[i]);
			errId = i;
			CHK(sqlite3_bind_int(dbpstmt,1,i),"AddrCatB1")
			CHK(sqlite3_bind_text(dbpstmt,2,cai->name[i],-1,SQLITE_STATIC),"AddrCatB2")
			CHKDONE(sqlite3_step(dbpstmt),"AddrCatST")
			CHK(sqlite3_clear_bindings(dbpstmt),"AddrCatCL")
			CHK(sqlite3_reset(dbpstmt),"AddrCatRST")
		}
		CHK(sqlite3_finalize(dbpstmt),"AddrCatFIN")
	} else if (strcmp(db_name,"MemoDB") == 0 || strcmp(db_name,"MemosDB-PMem") == 0 || strcmp(db_name,"Memo32DB") == 0) {
		CHK(sqlite3_exec(db.conn,"delete from MemoCategory",NULL,NULL,NULL),"MemoCatDelEX");
		CHK(sqlite3_prepare_v2(db.conn,
			"insert into MemoCategory (Id, Label, InsertDate) "
			"values (:Id, :Label, strftime('%Y-%m-%dT%H:%M:%S', 'now'))", -1, &dbpstmt, NULL), "jpMemCatINS")
		for (i=0; i<16; ++i) {
			if (cai->name[i][0] == '\0') continue;
			jp_logf(JP_LOG_DEBUG,"\t\t%2d %s\n",i,cai->name[i]);
			errId = i;
			CHK(sqlite3_bind_int(dbpstmt,1,i),"MemCatB1")
			CHK(sqlite3_bind_text(dbpstmt,2,cai->name[i],-1,SQLITE_STATIC),"MemCatB2")
			CHKDONE(sqlite3_step(dbpstmt),"MemCatST")
			CHK(sqlite3_clear_bindings(dbpstmt),"MemCatCL")
			CHK(sqlite3_reset(dbpstmt),"MemCatRST")
		}
		CHK(sqlite3_finalize(dbpstmt),"MemCatFIN")
	} else if (strcmp(db_name,"ToDoDB") == 0) {
		CHK(sqlite3_exec(db.conn,"delete from ToDoCategory",NULL,NULL,NULL),"ToDoCatDelEX");
		CHK(sqlite3_prepare_v2(db.conn,
			"insert into ToDoCategory (Id, Label, InsertDate) "
			"values (:Id, :Label, strftime('%Y-%m-%dT%H:%M:%S', 'now'))", -1, &dbpstmt, NULL),"jpToDoCatPRE")
		for (i=0; i<16; ++i) {
			if (cai->name[i][0] == '\0') continue;
			jp_logf(JP_LOG_DEBUG,"\t\t%2d %s\n",i,cai->name[i]);
			errId = i;
			CHK(sqlite3_bind_int(dbpstmt,1,i),"ToDoCatB1")
			CHK(sqlite3_bind_text(dbpstmt,2,cai->name[i],-1,SQLITE_STATIC),"ToDoCatB2")
			CHKDONE(sqlite3_step(dbpstmt),"ToDoCatST")
			CHK(sqlite3_clear_bindings(dbpstmt),"ToDoCatCL")
			CHK(sqlite3_reset(dbpstmt),"ToDoCatRST")
		}
		CHK(sqlite3_finalize(dbpstmt),"ToDoCatFIN")
	} else if (strcmp(db_name,"ExpenseDB") == 0) {
		CHK(sqlite3_exec(db.conn,"delete from ExpenseCategory",NULL,NULL,NULL),"ExpenseCatDelEX");
		CHK(sqlite3_prepare_v2(db.conn,
			"insert into ExpenseCategory (Id, Label, InsertDate) "
			"values (:Id, :Label, strftime('%Y-%m-%dT%H:%M:%S', 'now'))", -1, &dbpstmt, NULL),"jpExpenseCatPRE")
		for (i=0; i<16; ++i) {
			if (cai->name[i][0] == '\0') continue;
			jp_logf(JP_LOG_DEBUG,"\t\t%2d %s\n",i,cai->name[i]);
			errId = i;
			CHK(sqlite3_bind_int(dbpstmt,1,i),"ExpenseCatB1")
			CHK(sqlite3_bind_text(dbpstmt,2,cai->name[i],-1,SQLITE_STATIC),"ExpenseCatB2")
			CHKDONE(sqlite3_step(dbpstmt),"ExpenseCatST")
			CHK(sqlite3_clear_bindings(dbpstmt),"ExpenseCatCL")
			CHK(sqlite3_reset(dbpstmt),"ExpenseCatRST")
		}
		CHK(sqlite3_finalize(dbpstmt),"ExpenseCatFIN")
	}

	CHK(sqlite3_exec(db.conn,"END TRANSACTION",NULL,NULL,NULL),"jpsqlite_CatDELINS")
	return EXIT_SUCCESS;

err:
	jp_logf(JP_LOG_FATAL,"jpsqlite_CatDELINS(): ret=%d, error=%s, Id=%d, rolling back\n%s\n",
		sqlRet, sqlErr, errId, sqlite3_errmsg(db.conn));
	sqlite3_finalize(dbpstmt);
	sqlite3_exec(db.conn,"ROLLBACK TRANSACTION",NULL,NULL,NULL);
	return EXIT_FAILURE;
}



int jpsqlite_PrefSEL(prefType prefs[], int count) {
	int sqlRet=0, errId=0, id;
	const char *sqlErr="", *name, *usertype, *filetype, *svalue;

	for (errId=0; errId<count; ++errId) {
		if ((sqlRet = sqlite3_step(db.stmtPrefSEL)) == SQLITE_DONE) break;
		if (sqlRet != SQLITE_ROW) { sqlErr="PrefSELST"; goto err; }
		id = sqlite3_column_int(db.stmtPrefSEL,0);
		if (errId != id) { errId = id; jp_logf(JP_LOG_FATAL,"jpsqlite_PrefSEL(),stmtPrefSEL: errId != id"); }
		//ALLOCN(prefs[errId].name,sqlite3_column_text(db.stmtPrefSEL,1),"pref[errId].name")
		if (strcmp(name=sqlite3_column_text(db.stmtPrefSEL,1),prefs[errId].name) != 0) {
			jp_logf(JP_LOG_FATAL,"jpsqlite_PrefSEL(),stmtPrefSEL: prefs[%d].name != %s\n",errId,name);
		}
		usertype = sqlite3_column_text(db.stmtPrefSEL,2);
		filetype = sqlite3_column_text(db.stmtPrefSEL,3);
		prefs[errId].ivalue = sqlite3_column_int(db.stmtPrefSEL,4);
		svalue = sqlite3_column_text(db.stmtPrefSEL,5);
		prefs[errId].usertype = (strcmp(usertype,"int") == 0) ? 1 : 2;
		prefs[errId].filetype = (strcmp(filetype,"int") == 0) ? 1 : 2;
		if (svalue == NULL) {
			free(prefs[errId].svalue);
			prefs[errId].svalue = NULL;
			prefs[errId].svalue_size = 0;
		} else if (strcmp(svalue,prefs[errId].svalue) != 0) {
			free(prefs[errId].svalue);
			ALLOCN(prefs[errId].svalue,svalue,"pref[errId].svalue")
			prefs[errId].svalue_size = strlen(svalue) + 1;
		}
		/* * *
		if (strcmp(usertype,"int") == 0) {
			prefs[errId].ivalue = atoi(value);
			prefs[errId].svalue = NULL;
			prefs[errId].svalue_size = 0;
		} else if (strcmp(type,"char") == 0) {
			prefs[errId].ivalue = 0;
			ALLOCN(prefs[errId].svalue,value,"pref[errId].svalue")
			prefs[errId].svalue_size = strlen(value) + 1;
		} else {	// this should not happen
			jp_logf(JP_LOG_FATAL,"jpsqlite_PrefSEL(): at errId=%d unknown type %s\n",errId,type);
			prefs[errId].ivalue = 0;
			prefs[errId].svalue = NULL;
			prefs[errId].svalue_size = 0;
		}
		* * */
	}
	CHK(sqlite3_clear_bindings(db.stmtPrefSEL),"PrefSELCL")	// not necessary, as currently there are no bindings
	CHK(sqlite3_reset(db.stmtPrefSEL),"PrefSELRST")
	return errId;

err:
	jp_logf(JP_LOG_FATAL,"jpsqlite_PrefSEL(): ret=%d, error=%s, Id=%d, rolling back\n%s\n",
		sqlRet, sqlErr, errId, sqlite3_errmsg(db.conn));
	sqlite3_clear_bindings(db.stmtPrefSEL);
	sqlite3_reset(db.stmtPrefSEL);
	return 0;	// zero elements

errAlloc:
	jp_logf(JP_LOG_FATAL,"jpsqlite_PrefSEL(): at errId=%d memory allocation error for %s\n",errId,sqlErr);
	sqlite3_clear_bindings(db.stmtPrefSEL);
	sqlite3_reset(db.stmtPrefSEL);
	return 0;	// zero elements
}



int jpsqlite_PrefDELINS(prefType prefs[], int count) {
	int sqlRet=0, errId=0, id;
	const char *sqlErr="", *type, *value;
	char prefLong[32];

	jp_logf(JP_LOG_DEBUG,"jpsqlite_PrefDELINS()\n");

	CHK(sqlite3_exec(db.conn,"BEGIN TRANSACTION",NULL,NULL,NULL),"jpsqlite_PrefDELINS")
	CHKDONE(sqlite3_step(db.stmtPrefDEL),"PrefDELST")
	CHK(sqlite3_reset(db.stmtPrefDEL),"PrefDELRST")

	for (id=0; id<count; ++id) {
		CHK(sqlite3_bind_int(db.stmtPrefINS,1,id),"PrefInsB1")
		CHK(sqlite3_bind_text(db.stmtPrefINS,2,prefs[id].name,-1,SQLITE_STATIC),"PrefInsB2")
		CHK(sqlite3_bind_text(db.stmtPrefINS,3,prefs[id].usertype == 1 ? "int" : "char",-1,SQLITE_STATIC),"PrefInsB3")
		CHK(sqlite3_bind_text(db.stmtPrefINS,4,prefs[id].filetype == 1 ? "int" : "char",-1,SQLITE_STATIC),"PrefInsB4")
		CHK(sqlite3_bind_int(db.stmtPrefINS,5,prefs[id].ivalue),"PrefInsB5")
		CHK(sqlite3_bind_text(db.stmtPrefINS,6,prefs[id].svalue,-1,SQLITE_STATIC),"PrefInsB6")
		/* * *
		if (prefs[errId].usertype == 1) {
			CHK(sqlite3_bind_text(db.stmtPrefINS,3,"int",-1,SQLITE_STATIC),"PrefInsB3")
			sprintf(prefLong,"%ld",prefs[errId].ivalue);
			CHK(sqlite3_bind_text(db.stmtPrefINS,4,prefLong,-1,SQLITE_STATIC),"PrefInsB4")
		} else if (prefs[errId].usertype == 2) {
			CHK(sqlite3_bind_text(db.stmtPrefINS,3,"char",-1,SQLITE_STATIC),"PrefInsB3")
			CHK(sqlite3_bind_text(db.stmtPrefINS,4,prefs[errId].svalue,-1,SQLITE_STATIC),"PrefInsB4")
		} else {
			jp_logf(JP_LOG_FATAL,"jpsqlite_PrefDELINS(): inserting unknown type %d at errId=%d",prefs[errId].usertype,errId);
			CHK(sqlite3_bind_text(db.stmtPrefINS,3,"unknown",-1,SQLITE_STATIC),"PrefInsB3")
			CHK(sqlite3_bind_text(db.stmtPrefINS,4,NULL,-1,SQLITE_STATIC),"PrefInsB4")
		}
		* * */
		CHKDONE(sqlite3_step(db.stmtPrefINS),"PrefInsST")
		CHK(sqlite3_clear_bindings(db.stmtPrefINS),"PrefInsCL")
		CHK(sqlite3_reset(db.stmtPrefINS),"PrefRST")
	}

	CHK(sqlite3_exec(db.conn,"END TRANSACTION",NULL,NULL,NULL),"jpsqlite_PrefDELINS")
	return EXIT_SUCCESS;

err:
	jp_logf(JP_LOG_FATAL,"jpsqlite_PrefSEL(): ret=%d, error=%s, Id=%d, rolling back\n%s\n",
		sqlRet, sqlErr, errId, sqlite3_errmsg(db.conn));
	sqlite3_clear_bindings(db.stmtPrefINS);
	sqlite3_reset(db.stmtPrefINS);
	sqlite3_reset(db.stmtPrefDEL);
	sqlite3_exec(db.conn,"ROLLBACK TRANSACTION",NULL,NULL,NULL);
	return EXIT_SUCCESS;
}



/* SELECTing from Alarms table. Not overly happy with this table, just containing a single column wiht a single row.
 * klm, 20-Nov-2022
*/
int jpsqlite_AlarmsSEL(int *year, int *mon, int *day, int *hour, int *min) {
	int sqlRet=0, errId=1;
	const char *sqlErr="", *upToDate;
	sqlite3_stmt *dbpstmt;

	*year=0, *mon=0, *day=0, *hour=0, *min=0;

	// Only queried once during the lifetime of the program
	CHK(sqlite3_prepare_v2(db.conn, "select UpToDate from Alarms",
		-1, &dbpstmt, NULL), "jpsqlite_AlarmsSEL")
	if ((sqlRet = sqlite3_step(dbpstmt)) == SQLITE_ROW) {
		upToDate = sqlite3_column_text(dbpstmt,0);
		sscanf(upToDate,"%d-%d-%dT%d:%d",year,mon,day,hour,min);
	}
	jp_logf(JP_LOG_DEBUG,"jpsqlite_AlarmsSEL(): year=%04d, month=%02d, day=%02d, hour=%02d, min=%02d\n",*year,*mon,*day,*hour,*min);

	CHK(sqlite3_reset(dbpstmt),"AlarmsSELRST")
	CHK(sqlite3_finalize(dbpstmt),"AlarmsSELFIN")
	return EXIT_SUCCESS;

err:
	jp_logf(JP_LOG_FATAL,"jpsqlite_alarmsSEL(): ret=%d, error=%s, Id=%d, rolling back\n%s\n",
		sqlRet, sqlErr, errId, sqlite3_errmsg(db.conn));
	sqlite3_finalize(dbpstmt);
	return EXIT_FAILURE;
}



// klm, 20-Nov-2022
int jpsqlite_AlarmsINS(struct tm *now) {
	int sqlRet=0, errId=1;
	const char *sqlErr="";
	char upToDate[32];
	sqlite3_stmt *dbpstmt;

	CHK(sqlite3_exec(db.conn,"BEGIN TRANSACTION",NULL,NULL,NULL),"jpsqlite_AlarmsINS")
	CHK(sqlite3_exec(db.conn,"delete from Alarms",NULL,NULL,NULL),"jpsqlite_AlarmsINS")

	// Only inserted once during the lifetime of the program
	CHK(sqlite3_prepare_v2(db.conn, "insert into Alarms(UpToDate) values (:UpToDate)",
		-1, &dbpstmt, NULL), "jpsqlite_AlarmsINS")
	strftime(upToDate,32,"%FT%R",now);
	CHK(sqlite3_bind_text(dbpstmt,1,upToDate,-1,SQLITE_STATIC),"AlarmsInsB1")
	CHKDONE(sqlite3_step(dbpstmt),"AlarmsInsST")
	CHK(sqlite3_clear_bindings(dbpstmt),"AlarmsInsCL")
	CHK(sqlite3_reset(dbpstmt),"AlarmsINSRST")
	CHK(sqlite3_finalize(dbpstmt),"AlarmsINSFIN")

	CHK(sqlite3_exec(db.conn,"END TRANSACTION",NULL,NULL,NULL),"jpsqlite_AlarmsINS")
	return EXIT_SUCCESS;

err:
	jp_logf(JP_LOG_FATAL,"jpsqlite_AlarmsINS(): ret=%d, error=%s, Id=%d, rolling back\n%s\n",
		sqlRet, sqlErr, errId, sqlite3_errmsg(db.conn));
	sqlite3_finalize(dbpstmt);
	sqlite3_exec(db.conn,"ROLLBACK TRANSACTION",NULL,NULL,NULL);
	return EXIT_FAILURE;
}



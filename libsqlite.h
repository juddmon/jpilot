// J-Pilot SQLite3 header file
// Elmar Klausmeier, 10-Oct-2022

#include <pi-expense.h>

// Global variable for SQLite-based data storage instead of Palm binary format
extern int glob_sqlite;

// Copied from expense.c
struct MyExpense {
	PCRecType rt;
	unsigned int unique_id;
	unsigned char attrib;
	struct Expense ex;
	struct MyExpense *next;
};


extern char *jpstrdup(const unsigned char *s);
extern int jpsqlite_open(void);
extern int jpsqlite_close(void);
extern int jpsqlite_prepareAllStmt(void);
extern int jpsqlite_AddrUPD(struct Address *addr, PCRecType rt, unsigned char attrib, unsigned int *unique_id);
extern int jpsqlite_AddrINS(struct Address *addr, PCRecType rt, unsigned char attrib, unsigned int *unique_id);
extern int jpsqlite_DatebookUPD(struct CalendarEvent *cale, PCRecType rt, unsigned char attrib, unsigned int *unique_id);
extern int jpsqlite_DatebookINS(struct CalendarEvent *cale, PCRecType rt, unsigned char attrib, unsigned int *unique_id);
extern int jpsqlite_MemoUPD(struct Memo *memo, PCRecType rt, unsigned char attrib, unsigned int *unique_id);
extern int jpsqlite_MemoINS(struct Memo *memo, PCRecType rt, unsigned char attrib, unsigned int *unique_id);
extern int jpsqlite_ToDoUPD(struct ToDo *todo, PCRecType rt, unsigned char attrib, unsigned int *unique_id);
extern int jpsqlite_ToDoINS(struct ToDo *todo, PCRecType rt, unsigned char attrib, unsigned int *unique_id);
extern int jpsqlite_ExpenseUPD(struct Expense *ex, PCRecType rt, unsigned char attrib, unsigned int *unique_id);
extern int jpsqlite_ExpenseINS(struct Expense *ex, unsigned char attrib);
extern int jpsqlite_Delete(AppType app_type, void *VP);
extern int jpsqlite_prtAddrAppInfo(struct AddressAppInfo *ai);
extern int jpsqlite_AddrLabelSEL(struct AddressAppInfo *ai);
extern int jpsqlite_PhoneLabelSEL(struct AddressAppInfo *ai);
extern int jpsqlite_AddrCatSEL(struct AddressAppInfo *ai);
extern int jpsqlite_DatebookCatSEL(struct CalendarAppInfo *ai);
extern int jpsqlite_MemoCatSEL(struct MemoAppInfo *ai);
extern int jpsqlite_ToDoCatSEL(struct ToDoAppInfo *ai);
extern int jpsqlite_ExpenseCatSEL(struct ExpenseAppInfo *ai);
extern int jpsqlite_AddrSEL(AddressList **address_list, int sort_order, int privates, int category);
extern int jpsqlite_DatebookSEL(CalendarEventList **calendar_event_list, struct tm *now, int privates);
extern int jpsqlite_MemoSEL(MemoList **memo_list, int privates, int category);
extern int jpsqlite_ToDoSEL(ToDoList **todo_list, int privates, int category);
extern int jpsqlite_ExpenseSEL(struct MyExpense **expense_list);
extern int jpsqlite_CatDELINS(char *db_name, struct CategoryAppInfo *cai);
extern int jpsqlite_PrefSEL(prefType prefs[], int count);
extern int jpsqlite_PrefDELINS(prefType prefs[], int count);
extern int jpsqlite_AlarmsSEL(int *year, int *mon, int *day, int *hour, int *min);
extern int jpsqlite_AlarmsINS(struct tm *now);



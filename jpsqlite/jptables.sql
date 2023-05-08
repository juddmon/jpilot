--
-- Create SQLite3 database tables for J-Pilot plugin.
--
-- SQLite3 table names and column names are case insensitive by default.
-- SQLite3 uses type affinity and not rigid types.
--
-- Elmar Klausmeier, 17-Apr-2020
-- Elmar Klausmeier, 31-Oct-2022: exceptions in Datebook
-- Elmar Klausmeier, Nov-2022: added Pref + Alarms tables, InertDate+UpdateDate to various tables


-- By default SQLite3 does not enforce foreign key constraints
PRAGMA foreign_keys = ON;


-- Drop all tables
drop table if exists Addr;
drop table if exists AddrLabel;
drop table if exists AddrCategory;
drop table if exists PhoneLabel;
drop table if exists Datebook;
drop table if exists ToDo;
drop table if exists ToDoCategory;
drop table if exists Memo;
drop table if exists Expense;
drop table if exists MemoCategory;
drop table if exists ExpenseCategory;
drop table if exists ExpenseType;
drop table if exists ExpensePayment;



-- Labels for address columns
create table AddrLabel (
	Id            int primary key,
	Label         text
);
insert into AddrLabel (Id,Label) values (0,'Last name');
insert into AddrLabel (Id,Label) values (1,'First name');
insert into AddrLabel (Id,Label) values (2,'Company');
insert into AddrLabel (Id,Label) values (3,'Work');
insert into AddrLabel (Id,Label) values (4,'Home');
insert into AddrLabel (Id,Label) values (5,'Fax');
insert into AddrLabel (Id,Label) values (6,'Other');
insert into AddrLabel (Id,Label) values (7,'E-mail');
insert into AddrLabel (Id,Label) values (8,'Addr(W)');
insert into AddrLabel (Id,Label) values (9,'City');
insert into AddrLabel (Id,Label) values (10,'State');
insert into AddrLabel (Id,Label) values (11,'Zip Code');
insert into AddrLabel (Id,Label) values (12,'Country');
insert into AddrLabel (Id,Label) values (13,'Title');
insert into AddrLabel (Id,Label) values (14,'User-Id');
insert into AddrLabel (Id,Label) values (15,'Custom 2');
insert into AddrLabel (Id,Label) values (16,'Birthday');
insert into AddrLabel (Id,Label) values (17,'Custom 4');
insert into AddrLabel (Id,Label) values (18,'Note');


-- Labels for address categories, like 'Business', 'Travel', etc.
create table AddrCategory (
	Id            int primary key,
	Label         text,
	InsertDate    text,            -- first insertion date in format YYYY-MM-DDTHH:MM
	UpdateDate    text             -- latest update date in format YYYY-MM-DDTHH:MM
);
insert into AddrCategory (Id,Label) values (0,'Unfiled');


-- Labels for address phone entries, like 'Work', 'Mobile', etc.
create table PhoneLabel (
	Id            int primary key,
	Label         text
);
insert into PhoneLabel (Id,Label) values (0,'Work');
insert into PhoneLabel (Id,Label) values (1,'Home');
insert into PhoneLabel (Id,Label) values (2,'Fax');
insert into PhoneLabel (Id,Label) values (3,'Other');
insert into PhoneLabel (Id,Label) values (4,'E-mail');
insert into PhoneLabel (Id,Label) values (5,'Main');
insert into PhoneLabel (Id,Label) values (6,'Pager');
insert into PhoneLabel (Id,Label) values (7,'Mobile');


-- Actual address information
create table Addr (
	Id            int primary key, -- unique_ID
	Category      int default(0),
	Private       int default(0),  -- boolean, zero or one
	showPhone     int default(1),  -- which of phone1...5 to show as default
	Lastname      text,
	Firstname     text,
	Title         text,
	Company       text,
	PhoneLabel1   int,
	PhoneLabel2   int,
	PhoneLabel3   int,
	PhoneLabel4   int,
	PhoneLabel5   int,
	Phone1        text,            -- either telephone, fax, e-mail, mobile, etc.
	Phone2        text,            -- either telephone, fax, e-mail, mobile, etc.
	Phone3        text,            -- either telephone, fax, e-mail, mobile, etc.
	Phone4        text,            -- either telephone, fax, e-mail, mobile, etc.
	Phone5        text,            -- either telephone, fax, e-mail, mobile, etc.
	Address       text,
	City          text,
	State         text,
	Zip           text,
	Country       text,
	Custom1       text,
	Custom2       text,
	Custom3       text,
	Custom4       text,
	Note          text,
	InsertDate    text,            -- first insertion date in format YYYY-MM-DDTHH:MM
	UpdateDate    text,            -- latest update date in format YYYY-MM-DDTHH:MM
	foreign key (Category) references AddrCategory(Id),
	foreign key (PhoneLabel1) references PhoneLabel(Id),
	foreign key (PhoneLabel2) references PhoneLabel(Id),
	foreign key (PhoneLabel3) references PhoneLabel(Id),
	foreign key (PhoneLabel4) references PhoneLabel(Id),
	foreign key (PhoneLabel5) references PhoneLabel(Id)
);


create table Datebook (
	Id            int primary key,
	Private       int default(0),  -- boolean, zero or one
	Timeless      int default(0),  -- boolean, zero or one
	Begin         text,            -- begin date in format YYYY-MM-DDTHH:MM
	End           text,            -- end date in format YYYY-MM-DDTHH:MM
	Alarm         int,             -- boolean, zero or one
	Advance       int,             -- alarm in advance minutes/hours/days
	AdvanceUnit   int,             -- 0=minutes, 1=hours, 2=days
	RepeatType    int,             -- 0=none, 1=daily, 2=weekly, 3=monthly by day, 4=monthly by date, 5=yearly
	RepeatForever int,             -- boolean, zero or one
	RepeatEnd     text,            -- end date in format YYYY-MM-DD
	RepeatFreq    int,
	RepeatDay     int,
	RepeatDaySu   int,
	RepeatDayMo   int,
	RepeatDayTu   int,
	RepeatDayWe   int,
	RepeatDayTh   int,
	RepeatDayFr   int,
	RepeatDaySa   int,
	Exceptions    int,             -- number of exceptions
	Exception     text,            -- list of dates (format YYYY-MM-DD) separated by space
	Description   text,
	Note          text,
	InsertDate    text,            -- first insertion date in format YYYY-MM-DDTHH:MM
	UpdateDate    text             -- latest update date in format YYYY-MM-DDTHH:MM
);


-- Labels for ToDo categories, like 'Business', 'Personal', etc.
create table ToDoCategory (
	Id            int primary key,
	Label         text,
	InsertDate    text,            -- first insertion date in format YYYY-MM-DDTHH:MM
	UpdateDate    text             -- latest update date in format YYYY-MM-DDTHH:MM
);
insert into ToDoCategory (Id,Label) values (0,'Unfiled');


create table ToDo (
	Id            int primary key,
	Category      int default(0),
	Private       int default(0),  -- boolean, zero or one
	Indefinite    int default(0),  -- boolean, zero or one
	Due           text,            -- due date in format YYYY-MM-DD
	Priority      int default(1),
	Complete      int,             -- boolean, zero or one
	Description   text,
	Note          text,
	InsertDate    text,            -- first insertion date in format YYYY-MM-DDTHH:MM
	UpdateDate    text,            -- latest update date in format YYYY-MM-DDTHH:MM
	foreign key (Category) references ToDoCategory(Id)
);


-- Labels for memo categories, like 'Business', 'Personal', etc.
create table MemoCategory (
	Id            int primary key,
	Label         text,
	InsertDate    text,            -- first insertion date in format YYYY-MM-DDTHH:MM
	UpdateDate    text             -- latest update date in format YYYY-MM-DDTHH:MM
);
insert into MemoCategory (Id,Label) values (0,'Unfiled');


create table Memo (
	Id            int primary key,
	Category      int default(0),
	Private       int default(0),  -- boolean, zero or one
	Text          text,
	InsertDate    text,            -- first insertion date in format YYYY-MM-DDTHH:MM
	UpdateDate    text,            -- latest update date in format YYYY-MM-DDTHH:MM
	foreign key (Category) references MemoCategory(Id)
);


-- Labels for expense categories, like 'Project A', 'Internal', etc.
create table ExpenseCategory (
	Id            int primary key,
	Label         text,
	InsertDate    text,            -- first insertion date in format YYYY-MM-DDTHH:MM
	UpdateDate    text             -- latest update date in format YYYY-MM-DDTHH:MM
);
insert into ExpenseCategory (Id,Label) values (0,'Unfiled');


-- Labels for expense types, like 'airfaire', 'car rental', etc.
create table ExpenseType (
	Id            int primary key,
	Label         text
);
-- Taken from /usr/include/pi-expense.h
insert into ExpenseType (Id,Label) values (0,'Airfare');
insert into ExpenseType (Id,Label) values (1,'Breakfast');
insert into ExpenseType (Id,Label) values (2,'Bus');
insert into ExpenseType (Id,Label) values (3,'Business Meals');
insert into ExpenseType (Id,Label) values (4,'Car Rental');
insert into ExpenseType (Id,Label) values (5,'Dinner');
insert into ExpenseType (Id,Label) values (6,'Entertainment');
insert into ExpenseType (Id,Label) values (7,'Fax');
insert into ExpenseType (Id,Label) values (8,'Gas');
insert into ExpenseType (Id,Label) values (9,'Gifts');
insert into ExpenseType (Id,Label) values (10,'Hotel');
insert into ExpenseType (Id,Label) values (11,'Incidentals');
insert into ExpenseType (Id,Label) values (12,'Laundry');
insert into ExpenseType (Id,Label) values (13,'Limo');
insert into ExpenseType (Id,Label) values (14,'Lodging');
insert into ExpenseType (Id,Label) values (15,'Lunch');
insert into ExpenseType (Id,Label) values (16,'Mileage');
insert into ExpenseType (Id,Label) values (17,'Other');
insert into ExpenseType (Id,Label) values (18,'Parking');
insert into ExpenseType (Id,Label) values (19,'Postage');
insert into ExpenseType (Id,Label) values (20,'Snack');
insert into ExpenseType (Id,Label) values (21,'Subway');
insert into ExpenseType (Id,Label) values (22,'Supplies');
insert into ExpenseType (Id,Label) values (23,'Taxi');
insert into ExpenseType (Id,Label) values (24,'Telephone');
insert into ExpenseType (Id,Label) values (25,'Tips');
insert into ExpenseType (Id,Label) values (26,'Tolls');
insert into ExpenseType (Id,Label) values (27,'Train');


-- Labels for expense payments, like 'cash', 'Visa', etc.
create table ExpensePayment (
	Id            int primary key,
	Label         text
);
-- Taken from /usr/include/pi-expense.h
insert into ExpensePayment (Id,Label) values (1,'AmEx');
insert into ExpensePayment (Id,Label) values (2,'Cash');
insert into ExpensePayment (Id,Label) values (3,'Check');
insert into ExpensePayment (Id,Label) values (4,'Credit Card');
insert into ExpensePayment (Id,Label) values (5,'MasterCard');
insert into ExpensePayment (Id,Label) values (6,'Prepaid');
insert into ExpensePayment (Id,Label) values (7,'Visa');
insert into ExpensePayment (Id,Label) values (8,'Unfiled');


-- Labels for expense currency, like 'US', 'Germany', etc.
create table ExpenseCurrency (
	Id            int primary key,
	Label         text
);
-- Taken from Expense/expense.c
insert into ExpenseCurrency (Id,Label) values (0,'Australia');
insert into ExpenseCurrency (Id,Label) values (1,'Austria');
insert into ExpenseCurrency (Id,Label) values (2,'Belgium');
insert into ExpenseCurrency (Id,Label) values (3,'Brazil');
insert into ExpenseCurrency (Id,Label) values (4,'Canada');
insert into ExpenseCurrency (Id,Label) values (5,'Denmark');
insert into ExpenseCurrency (Id,Label) values (133,'EU (Euro)');
insert into ExpenseCurrency (Id,Label) values (6,'Finland');
insert into ExpenseCurrency (Id,Label) values (7,'France');
insert into ExpenseCurrency (Id,Label) values (8,'Germany');
insert into ExpenseCurrency (Id,Label) values (9,'Hong Kong');
insert into ExpenseCurrency (Id,Label) values (10,'Iceland');
insert into ExpenseCurrency (Id,Label) values (24,'India');
insert into ExpenseCurrency (Id,Label) values (25,'Indonesia');
insert into ExpenseCurrency (Id,Label) values (11,'Ireland');
insert into ExpenseCurrency (Id,Label) values (12,'Italy');
insert into ExpenseCurrency (Id,Label) values (13,'Japan');
insert into ExpenseCurrency (Id,Label) values (26,'Korea');
insert into ExpenseCurrency (Id,Label) values (14,'Luxembourg');
insert into ExpenseCurrency (Id,Label) values (27,'Malaysia');
insert into ExpenseCurrency (Id,Label) values (15,'Mexico');
insert into ExpenseCurrency (Id,Label) values (16,'Netherlands');
insert into ExpenseCurrency (Id,Label) values (17,'New Zealand');
insert into ExpenseCurrency (Id,Label) values (18,'Norway');
insert into ExpenseCurrency (Id,Label) values (28,'P.R.C.');
insert into ExpenseCurrency (Id,Label) values (29,'Philippines');
insert into ExpenseCurrency (Id,Label) values (30,'Singapore');
insert into ExpenseCurrency (Id,Label) values (19,'Spain');
insert into ExpenseCurrency (Id,Label) values (20,'Sweden');
insert into ExpenseCurrency (Id,Label) values (21,'Switzerland');
insert into ExpenseCurrency (Id,Label) values (32,'Taiwan');
insert into ExpenseCurrency (Id,Label) values (31,'Thailand');
insert into ExpenseCurrency (Id,Label) values (22,'United Kingdom');
insert into ExpenseCurrency (Id,Label) values (23,'United States');


create table Expense (
	Id            int primary key,
	Category      int default(0),
	Date          text,            -- date in format YYYY-MM-DD
	Type          int,             -- 0=airfare, 1=breakfast, etc.
	Payment       int,             -- 0=AmEx, 1=Cash, etc.
	Currency      int,
	Amount        text,
	Vendor        text,
	City          text,
	Attendees     text,
	Note          text,
	InsertDate    text,            -- first insertion date in format YYYY-MM-DDTHH:MM
	UpdateDate    text,            -- latest update date in format YYYY-MM-DDTHH:MM
	foreign key (Category) references ExpenseCategory(Id),
	foreign key (Type) references ExpenseType(Id),
	foreign key (Payment) references ExpensePayment(Id),
	foreign key (Currency) references ExpenseCurrency(Id)
);


create table Pref (  -- preferences, previously in jpilot.rc
	Id            int primary key,
	Name          text not null,   -- Id + name can be used as primary key interchangeably
	Usertype      text,            -- either 'int' or 'char'
	Filetype      text,            -- either 'int' or 'char'
	iValue        int,             -- integer value
	sValue        text,            -- string value
	InsertDate    text,            -- first insertion date in format YYYY-MM-DDTHH:MM
	UpdateDate    text             -- latest update date in format YYYY-MM-DDTHH:MM (not used)
);


create table Alarms (	-- just store last time J-Pilot was ran
	UpToDate      text
);




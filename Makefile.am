# $Id: Makefile.am,v 1.14 2003/02/21 00:12:32 judd Exp $

SUBDIRS = intl Expense SyncTime KeyRing docs dialer po

EXTRA_DIST = reconf autogen.sh ChangeLog docs empty icons \
	config.rpath mkinstalldirs \
	BUGS jpilot.spec jpilot-col.spec jpilot.kdelnk jpilot.rh7.spec \
	ChangeLog UPGRADING \
	jpilot.desktop \
	jpilotrc.blue jpilotrc.default jpilotrc.green jpilotrc.purple \
	jpilotrc.steel SlackBuild \
	intltool-extract.in intltool-merge.in intltool-update.in \
	m4/gtk-2.0.m4 \
	dialer/Makefile.unix \
	dialer/README \
	dialer/jpilot-dial.c

bin_PROGRAMS = jpilot jpilot-dump jpilot-sync
jpilot_SOURCES = \
	address.c \
	address.h \
	address_gui.c \
	alarms.c \
	alarms.h \
	category.c \
	cp1250.c \
	cp1250.h \
	dat.c \
	datebook.c \
	datebook.h \
	datebook_gui.c \
	dialer.c \
	export_gui.c \
	export.h \
	i18n.h \
	import_gui.c \
	install_gui.c \
	japanese.c \
	japanese.h \
	jpilot.c \
	libplugin.c \
	libplugin.h \
	log.c \
	log.h \
	memo.c \
	memo_gui.c \
	memo.h \
	monthview_gui.c \
	password.c \
	password.h \
	plugins.c \
	plugins.h \
	prefs.c \
	prefs_gui.c \
	prefs_gui.h \
	prefs.h \
	print.c \
	print_gui.c \
	print.h \
	print_headers.c \
	print_headers.h \
	print_logo.c \
	print_logo.h \
	restore_gui.c \
	restore.h \
	russian.c \
	russian.h \
	search_gui.c \
	sync.c \
	sync.h \
	todo.c \
	todo_gui.c \
	todo.h \
	utils.c \
	utils.h \
	weekview_gui.c \
	address.xpm \
	datebook.xpm \
	memo.xpm \
	todo.xpm

jpilot_dump_SOURCES = \
	address.c \
	category.c \
	cp1250.c \
	datebook.c \
	japanese.c \
	jpilot-dump.c \
	libplugin.c \
	log.c \
	memo.c \
	password.c \
	plugins.c \
	prefs.c \
	russian.c \
	todo.c \
	utils.c

jpilot-dump.o: jpilot-dump.c config.h utils.h log.h prefs.h \
	datebook.h memo.h address.h todo.h password.h

jpilot_sync_SOURCES = \
	cp1250.c \
	category.c \
	jpilot-sync.c \
	japanese.c \
	libplugin.c \
	log.c \
	password.c \
	plugins.c \
	prefs.c \
	russian.c \
	sync.c \
	utils.c

jpilot-sync.o: jpilot-sync.c config.h utils.h log.h prefs.h sync.h \
	libplugin.h password.h

localedir = $(datadir)/locale
gnulocaledir = $(prefix)/share/locale
I18NDEFS = -DLOCALEDIR=\"$(localedir)\" -DGNULOCALEDIR=\"$(gnulocaledir)\"

CFLAGS= -Wall @CFLAGS@ @ccoptions@ @PILOT_FLAGS@ @GTK_CFLAGS@ ${I18NDEFS}

jpilot_LDFLAGS = -export-dynamic
jpilot_LDADD=@LIBS@ @PILOT_LIBS@ @GTK_LIBS@
jpilot_dump_LDADD=@LIBS@ @PILOT_LIBS@ @GTK_LIBS@
jpilot_sync_LDADD=@LIBS@ @PILOT_LIBS@ @GTK_LIBS@

# Automatically update the libtool script if it becomes out-of-date.
LIBTOOL_DEPS = @LIBTOOL_DEPS@
libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck

#ChangeLog:
#	rcs2log > $@
#
#.PHONY: ChangeLog


ACLOCAL_AMFLAGS = -I m4

desktopdir = $(datadir)/gnome/apps/Applications
desktop_DATA = jpilot.desktop

###
# HTML documentation, etc.
###
install-data-local:
	$(INSTALL) -d -m 755 $(prefix)/share/$(PACKAGE)
	$(INSTALL) -m 644 jpilotrc.blue $(prefix)/share/$(PACKAGE)
	$(INSTALL) -m 644 jpilotrc.default $(prefix)/share/$(PACKAGE)/
	$(INSTALL) -m 644 jpilotrc.green $(prefix)/share/$(PACKAGE)/
	$(INSTALL) -m 644 jpilotrc.purple $(prefix)/share/$(PACKAGE)/
	$(INSTALL) -m 644 jpilotrc.steel $(prefix)/share/$(PACKAGE)/
	$(INSTALL) -m 644 empty/DatebookDB.pdb $(prefix)/share/$(PACKAGE)/
	$(INSTALL) -m 644 empty/AddressDB.pdb $(prefix)/share/$(PACKAGE)/
	$(INSTALL) -m 644 empty/ToDoDB.pdb $(prefix)/share/$(PACKAGE)/
	$(INSTALL) -m 644 empty/MemoDB.pdb $(prefix)/share/$(PACKAGE)/
	$(INSTALL) -m 644 empty/Memo32DB.pdb $(prefix)/share/$(PACKAGE)/
	$(INSTALL) -m 644 empty/ExpenseDB.pdb $(prefix)/share/$(PACKAGE)/

uninstall-local:
	rm -f $(prefix)/share/$(PACKAGE)/jpilotrc.blue
	rm -f $(prefix)/share/$(PACKAGE)/jpilotrc.default
	rm -f $(prefix)/share/$(PACKAGE)/jpilotrc.green
	rm -f $(prefix)/share/$(PACKAGE)/jpilotrc.purple
	rm -f $(prefix)/share/$(PACKAGE)/jpilotrc.steel
	rm -f $(prefix)/share/$(PACKAGE)/empty/DatebookDB.pdb
	rm -f $(prefix)/share/$(PACKAGE)/empty/AddressDB.pdb
	rm -f $(prefix)/share/$(PACKAGE)/empty/ToDoDB.pdb
	rm -f $(prefix)/share/$(PACKAGE)/empty/MemoDB.pdb
	rm -f $(prefix)/share/$(PACKAGE)/empty/Memo32DB.pdb
	rm -f $(prefix)/share/$(PACKAGE)/empty/ExpenseDB.pdb

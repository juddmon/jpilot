# $Id: Makefile.am,v 1.24 2004/02/22 20:33:09 judd Exp $

SUBDIRS = Expense SyncTime KeyRing docs dialer po m4 empty icons

# install the manual docs
colordir = $(prefix)/share/$(PACKAGE)
color_DATA = \
	jpilotrc.blue jpilotrc.default jpilotrc.green jpilotrc.purple \
	jpilotrc.steel

EXTRA_DIST = config.rpath  reconf autogen.sh ChangeLog \
	config.rpath mkinstalldirs \
	jpilot.spec jpilot.kdelnk jpilot.rh7.spec \
	jpilot.desktop \
	$(color_DATA) \
	SlackBuild \
	intltool-extract.in intltool-merge.in intltool-update.in \
	m4/gtk-2.0.m4 \
	dialer/Makefile.unix \
	dialer/README \
	dialer/jpilot-dial.c \
	po/POTFILES

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
	cp1253.c \
	cp1253.h \
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
	cp1253.c \
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
	cp1253.c \
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

AM_CFLAGS= -Wall @CFLAGS@ @ccoptions@ @PILOT_FLAGS@ @GTK_CFLAGS@ ${I18NDEFS}

jpilot_LDFLAGS = -export-dynamic
jpilot_LDADD=@LIBS@ @PILOT_LIBS@ @GTK_LIBS@
jpilot_dump_LDADD=@LIBS@ @PILOT_LIBS@ @GTK_LIBS@
jpilot_sync_LDADD=@LIBS@ @PILOT_LIBS@ @GTK_LIBS@

# Automatically update the libtool script if it becomes out-of-date.
LIBTOOL_DEPS = @LIBTOOL_DEPS@
libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck

better-world:
	echo "make better-world: rm -rf -any -all windows"

peace:
	echo "make peace: not war"

#ChangeLog:
#	rcs2log > $@
#
#.PHONY: ChangeLog


ACLOCAL_AMFLAGS = -I m4

desktopdir = $(datadir)/gnome/apps/Applications
desktop_DATA = jpilot.desktop


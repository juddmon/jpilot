# $Id: Makefile.am,v 1.19 2003/06/02 01:24:04 judd Exp $

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
	dialer/jpilot-dial.c \
	po/POTFILES

dist-hook:
	  rm -rf `find $(distdir)/ -name CVS`

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

AM_CFLAGS= -Wall @CFLAGS@ @ccoptions@ @PILOT_FLAGS@ @GTK_CFLAGS@ ${I18NDEFS}

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
	$(INSTALL) -d -m 755 $(DESTDIR)/$(prefix)/doc/$(PACKAGE)-$(VERSION)
	$(INSTALL) -m 644 docs/manual.html $(DESTDIR)/$(prefix)/doc/$(PACKAGE)-$(VERSION)/
	$(INSTALL) -m 644 docs/plugin.html $(DESTDIR)/$(prefix)/doc/$(PACKAGE)-$(VERSION)
	$(INSTALL) -m 644 docs/jpilot-address.png $(DESTDIR)/$(prefix)/doc/$(PACKAGE)-$(VERSION)
	$(INSTALL) -m 644 docs/jpilot-datebook.png $(DESTDIR)/$(prefix)/doc/$(PACKAGE)-$(VERSION)
	$(INSTALL) -m 644 docs/jpilot-expense.png $(DESTDIR)/$(prefix)/doc/$(PACKAGE)-$(VERSION)
	$(INSTALL) -m 644 docs/jpilot-install.png $(DESTDIR)/$(prefix)/doc/$(PACKAGE)-$(VERSION)
	$(INSTALL) -m 644 docs/jpilot-memo.png $(DESTDIR)/$(prefix)/doc/$(PACKAGE)-$(VERSION)
	$(INSTALL) -m 644 docs/jpilot-prefs.png $(DESTDIR)/$(prefix)/doc/$(PACKAGE)-$(VERSION)
	$(INSTALL) -m 644 docs/jpilot-print.png $(DESTDIR)/$(prefix)/doc/$(PACKAGE)-$(VERSION)
	$(INSTALL) -m 644 docs/jpilot-search.png $(DESTDIR)/$(prefix)/doc/$(PACKAGE)-$(VERSION)
	$(INSTALL) -m 644 docs/jpilot-todo.png $(DESTDIR)/$(prefix)/doc/$(PACKAGE)-$(VERSION)
	$(INSTALL) -m 644 docs/jpilot-toplogo.jpg $(DESTDIR)/$(prefix)/doc/$(PACKAGE)-$(VERSION)
	$(INSTALL) -d -m 755 $(DESTDIR)/$(prefix)/doc/$(PACKAGE)-$(VERSION)/icons
	$(INSTALL) -m 644 icons/README $(DESTDIR)/$(prefix)/doc/$(PACKAGE)-$(VERSION)/icons/README
	$(INSTALL) -m 644 icons/jpilot-icon1.xpm $(DESTDIR)/$(prefix)/doc/$(PACKAGE)-$(VERSION)/icons/
	$(INSTALL) -m 644 icons/jpilot-icon2.xpm $(DESTDIR)/$(prefix)/doc/$(PACKAGE)-$(VERSION)/icons/
	$(INSTALL) -m 644 icons/jpilot-icon3.xpm $(DESTDIR)/$(prefix)/doc/$(PACKAGE)-$(VERSION)/icons/
	$(INSTALL) -m 644 icons/jpilot-icon4.xpm $(DESTDIR)/$(prefix)/doc/$(PACKAGE)-$(VERSION)/icons/
	$(INSTALL) -m 644 BUGS $(DESTDIR)/$(prefix)/doc/$(PACKAGE)-$(VERSION)/
	$(INSTALL) -m 644 ChangeLog $(DESTDIR)/$(prefix)/doc/$(PACKAGE)-$(VERSION)/
	$(INSTALL) -m 644 COPYING $(DESTDIR)/$(prefix)/doc/$(PACKAGE)-$(VERSION)/
	$(INSTALL) -m 644 AUTHORS $(DESTDIR)/$(prefix)/doc/$(PACKAGE)-$(VERSION)/
	$(INSTALL) -m 644 INSTALL $(DESTDIR)/$(prefix)/doc/$(PACKAGE)-$(VERSION)/
	$(INSTALL) -m 644 README $(DESTDIR)/$(prefix)/doc/$(PACKAGE)-$(VERSION)/
	$(INSTALL) -m 644 TODO $(DESTDIR)/$(prefix)/doc/$(PACKAGE)-$(VERSION)/
	$(INSTALL) -m 644 UPGRADING $(DESTDIR)/$(prefix)/doc/$(PACKAGE)-$(VERSION)/
	$(INSTALL) -d -m 755 $(DESTDIR)/$(prefix)/share/$(PACKAGE)
	$(INSTALL) -m 644 jpilotrc.blue $(DESTDIR)/$(prefix)/share/$(PACKAGE)/
	$(INSTALL) -m 644 jpilotrc.default $(DESTDIR)/$(prefix)/share/$(PACKAGE)/
	$(INSTALL) -m 644 jpilotrc.green $(DESTDIR)/$(prefix)/share/$(PACKAGE)/
	$(INSTALL) -m 644 jpilotrc.purple $(DESTDIR)/$(prefix)/share/$(PACKAGE)/
	$(INSTALL) -m 644 jpilotrc.steel $(DESTDIR)/$(prefix)/share/$(PACKAGE)/
	$(INSTALL) -m 644 empty/* $(DESTDIR)/$(prefix)/share/$(PACKAGE)/
uninstall-local:
	rm -f $(DESTDIR)/$(prefix)/share/$(PACKAGE)/jpilotrc.*
	rm -f $(DESTDIR)/$(prefix)/share/$(PACKAGE)/empty/*
	rm -f $(DESTDIR)/$(prefix)/doc/$(PACKAGE)-$(VERSION)/

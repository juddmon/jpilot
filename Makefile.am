# $Id: Makefile.am,v 1.51 2006/10/10 01:53:57 rikster5 Exp $

SUBDIRS = Expense SyncTime KeyRing dialer m4 po icons docs empty 

EXTRA_DIST = config.rpath mkinstalldirs reconf autogen.sh \
	intltool-extract.in intltool-merge.in intltool-update.in \
	ChangeLog ChangeLog.cvs \
	po/POTFILES \
	jpilot.spec jpilot.kdelnk \
	SlackBuild description-pak \
	jpilot.desktop \
	$(color_DATA) \
	jpilot.xpm

DISTCLEANFILES = intltool-extract intltool-merge intltool-update

desktopdir = $(datadir)/applications
desktop_DATA = jpilot.desktop

# The skin files for changing jpilot appearance
colordir = $(pkgdatadir)
color_DATA = \
	jpilotrc.blue jpilotrc.default jpilotrc.green jpilotrc.purple \
	jpilotrc.steel

# Instructions for the code to build begins here

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
	install_user.c \
	install_user.h \
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
	otherconv.c \
	otherconv.h \
	password.c \
	password.h \
	pidfile.c \
	pidfile.h \
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
	stock_buttons.h \
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
	otherconv.c \
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
	otherconv.c \
	password.c \
	plugins.c \
	prefs.c \
	russian.c \
	sync.c \
	utils.c

jpilot-sync.o: jpilot-sync.c config.h utils.h log.h prefs.h sync.h \
	libplugin.h password.h

# Include gettext macros that we have placed in the m4 directory
# and include in the distribution.
ACLOCAL_AMFLAGS = -I m4

# Add i18n support
localedir = $(datadir)/locale
I18NDEFS = -DLOCALEDIR=\"$(localedir)\"

AM_CFLAGS= @PILOT_FLAGS@ @GTK_CFLAGS@ ${I18NDEFS}

# Add linkflags
jpilot_LDFLAGS = -export-dynamic
jpilot_LDADD=@LIBS@ @PILOT_LIBS@ @GTK_LIBS@
jpilot_dump_LDADD=@LIBS@ @PILOT_LIBS@ @GTK_LIBS@
jpilot_sync_LDFLAGS = -export-dynamic
jpilot_sync_LDADD=@LIBS@ @PILOT_LIBS@ @GTK_LIBS@

################################################################################
## The rest of the file is copied over to the Makefile with only variable
## substitution.
################################################################################

# Automatically update the libtool script if it becomes out-of-date.
LIBTOOL_DEPS = @LIBTOOL_DEPS@
libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck

better-world:
	echo "make better-world: rm -rf -any -all windows"

peace:
	echo "make peace: not war"

test_compile:
	@-rm -f autogen.log autogen.log2 make.log
	@echo "Configuring GTK1 build"
	autogen.sh --disable-gtk2 > autogen.log 2>&1
	@-grep -v 'warning: underquoted definition' autogen.log > autogen.log2
	!(grep 'warn'   autogen.log2)
	!(grep ' error' autogen.log2)
	make > make.log 2>&1
	!(grep 'warn'  make.log)
	!(grep 'error' make.log)
	@-rm -f autogen.log autogen.log2 make.log
	@echo "Configuring GTK2 build"
	autogen.sh > autogen.log 2>&1
	@-grep -v 'warning: underquoted definition' autogen.log > autogen.log2
	!(grep 'warn'   autogen.log2)
	!(grep ' error' autogen.log2)
	make > make.log 2>&1
	!(grep 'warn'  make.log)
	!(grep 'error' make.log)

ChangeLog.cvs:
	rcs2log | perl -pe 's/rousseau /Ludovic Rousseau /; \
	s/judd /Judd Montgomery /; s/desrod /David A. Desrosiers /; \
	s/rikster5 /Rik Wehbring /' > $@



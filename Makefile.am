SUBDIRS = m4 po Expense SyncTime KeyRing dialer icons docs empty

EXTRA_DIST = config.rpath mkinstalldirs reconf autogen.sh \
	intltool-extract.in intltool-merge.in intltool-update.in gettext.h \
	ChangeLog ChangeLog.git \
	po/POTFILES \
	jpilot.spec \
	SlackBuild description-pak \
	jpilot.desktop \
	$(color_DATA) \
	jpilot.xpm

DISTCLEANFILES = intltool-extract intltool-merge intltool-update ChangeLog.git

desktopdir = $(datadir)/applications
desktop_DATA = jpilot.desktop

# The skin files for changing jpilot appearance
colordir = $(pkgdatadir)
color_DATA = \
	jpilotrc.blue jpilotrc.default jpilotrc.green jpilotrc.purple \
	jpilotrc.steel

# Instructions for the code to build begins here

bin_PROGRAMS = jpilot jpilot-dump jpilot-sync jpilot-merge

jpilot_SOURCES = \
	address.c \
	address.h \
	address_gui.c \
	alarms.c \
	alarms.h \
	category.c \
	calendar.c \
	calendar.h \
	contact.c \
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
	jpilot.h \
	jp-contact.c \
	jp-pi-contact.h \
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
	icons/address.xpm \
	icons/datebook.xpm \
	icons/memo.xpm \
	icons/todo.xpm \
	icons/sync.xpm \
	icons/cancel_sync.xpm \
	icons/backup.xpm \
	icons/list_mini_icons.h \
	icons/appl_menu_icons.h \
	icons/lock_icons.h \
	icons/icons.c

jpilot_dump_SOURCES = \
	address.c \
	calendar.c \
	category.c \
	contact.c \
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
	utils.c \
	jp-contact.c

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
	utils.c \
	jp-contact.c

jpilot_merge_SOURCES = \
	cp1250.c \
	japanese.c \
	libplugin.c \
	log.c \
	jpilot-merge.c \
	otherconv.c \
	plugins.c \
	prefs.c \
	russian.c \
	utils.c


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
jpilot_merge_LDADD=@LIBS@ @PILOT_LIBS@ @GTK_LIBS@

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
	@echo "Configuring Jpilot build"
	autogen.sh > autogen.log 2>&1
	@-grep -v 'warning: underquoted definition' autogen.log > autogen.log2
	!(grep 'warn'   autogen.log2)
	!(grep ' error' autogen.log2)
	make > make.log 2>&1
	!(grep 'warn'  make.log)
	!(grep 'error' make.log)

ChangeLog.cvs:
	rcs2log -h hostname | perl -pe \
	's/<rousseau\@hostname>/Ludovic Rousseau <ludovic.rousseau\@free.fr>/; \
	s/<judd\@hostname>/Judd Montgomery <judd\@jpilot.org>/; \
	s/<desrod\@hostname>/David A. Desrosiers <david.a.desrosiers\@gmail.com>/; \
	s/<rikster5\@hostname>/Rik Wehbring <jpilot\@nomad.inbox5.com>/' > $@
.PHONY: ChangeLog.cvs

ChangeLog.git:
	git log --pretty > $@
.PHONY: ChangeLog.git

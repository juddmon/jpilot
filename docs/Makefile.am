# $Id: Makefile.am,v 1.16 2011/04/06 14:28:59 rousseau Exp $

# Install the man pages
raw_mans = jpilot.man jpilot-dial.man jpilot-sync.man jpilot-dump.man jpilot-merge.1
man_MANS = jpilot.1 jpilot-dial.1 jpilot-sync.1 jpilot-dump.1 jpilot-merge.1

# Install the standard GNU doc files
miscdir = $(datadir)/doc/$(PACKAGE)
misc_DATA = \
	../BUGS \
	../ChangeLog \
	../COPYING \
	../AUTHORS \
	../INSTALL \
	../README \
	../TODO

# Install the manual docs
docdir = $(miscdir)/manual
doc_DATA = \
	manual.html \
	plugin.html \
	jpilot-address.png \
	jpilot-datebook.png \
	jpilot-expense.png \
	jpilot-install.png \
	jpilot-memo.png \
	jpilot-prefs-1.png \
	jpilot-prefs-2.png \
	jpilot-prefs-3.png \
	jpilot-prefs-4.png \
	jpilot-prefs-5.png \
	jpilot-prefs-6.png \
	jpilot-prefs-7.png \
	jpilot-prefs-8.png \
	jpilot-print.png \
	jpilot-search.png \
	jpilot-todo.png \
	jpilot-toplogo.jpg

EXTRA_DIST = $(raw_mans) $(misc_DATA) $(doc_DATA) 
DISTCLEANFILES = $(man_MANS)

# Make rule to build man pages
%.1 : %.man
	sed -e 's|@DOCDIR@|$(miscdir)|g' $< > $@


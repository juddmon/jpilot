# $Id: Makefile.am,v 1.9 2005/10/30 16:49:12 judd Exp $

# install the man pages
man_MANS = jpilot.1 jpilot-dial.1 jpilot-sync.1 jpilot-dump.1

miscdir = $(prefix)/share/doc/$(PACKAGE)-$(VERSION)
misc_DATA = ../BUGS \
	../ChangeLog \
	../COPYING \
	../AUTHORS \
	../INSTALL \
	../README \
	../TODO \
	../UPGRADING

# install the manual docs
docdir = $(miscdir)/manual
doc_DATA = manual.html \
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

EXTRA_DIST = $(doc_DATA) $(misc_DATA) $(man_MANS)


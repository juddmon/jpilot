# $Id: Makefile.am,v 1.8 2004/10/05 20:19:04 rousseau Exp $

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
	    jpilot-prefs.png \
	    jpilot-print.png \
	    jpilot-search.png \
	    jpilot-todo.png \
	    jpilot-toplogo.jpg

EXTRA_DIST = $(doc_DATA) $(misc_DATA) $(man_MANS)


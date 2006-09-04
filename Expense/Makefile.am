# $Id: Makefile.am,v 1.6 2006/09/04 19:45:03 judd Exp $

libdir = $(prefix)/lib/jpilot/plugins

if MAKE_EXPENSE

lib_LTLIBRARIES = libexpense.la
libexpense_la_SOURCES = expense.c
libexpense_la_CFLAGS = @PILOT_FLAGS@ @GTK_CFLAGS@ -I$(top_srcdir)
libexpense_la_LDFLAGS = @GTK_LIBS@

endif

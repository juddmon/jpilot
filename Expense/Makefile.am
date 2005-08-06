# $Id: Makefile.am,v 1.5 2005/08/06 21:46:39 rousseau Exp $

libdir = $(prefix)/lib/jpilot/plugins

if MAKE_EXPENSE

lib_LTLIBRARIES = libexpense.la
libexpense_la_SOURCES = expense.c
libexpense_la_CFLAGS = -Wall @PILOT_FLAGS@ @GTK_CFLAGS@ -I$(top_srcdir)
libexpense_la_LDFLAGS = @GTK_LIBS@

endif

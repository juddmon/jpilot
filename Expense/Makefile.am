# $Id: Makefile.am,v 1.4 2005/02/19 16:03:05 rousseau Exp $

libdir = $(prefix)/lib/jpilot/plugins

if MAKE_EXPENSE

lib_LTLIBRARIES = libexpense.la
libexpense_la_SOURCES = expense.c
libexpense_la_CFLAGS = -Wall @PILOT_FLAGS@ @GTK_CFLAGS@ -I$(top_srcdir)

endif

# $Id: Makefile.am,v 1.2 2003/01/08 06:50:26 judd Exp $

CFLAGS = -Wall @CFLAGS@ @ccoptions@ @PILOT_FLAGS@ @GTK_CFLAGS@ -I$(top_srcdir)
libdir = $(prefix)/lib/jpilot/plugins

if MAKE_EXPENSE

lib_LTLIBRARIES = libexpense.la

libexpense_la_SOURCES = expense.c

endif

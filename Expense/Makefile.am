# $Id: Makefile.am,v 1.3 2003/05/02 04:42:06 judd Exp $

AM_CFLAGS = -Wall @CFLAGS@ @ccoptions@ @PILOT_FLAGS@ @GTK_CFLAGS@ -I$(top_srcdir)
libdir = $(prefix)/lib/jpilot/plugins

if MAKE_EXPENSE

lib_LTLIBRARIES = libexpense.la

libexpense_la_SOURCES = expense.c

endif

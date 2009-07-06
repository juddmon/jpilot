# $Id: Makefile.am,v 1.7 2009/07/06 22:29:47 rikster5 Exp $

libdir = @libdir@/@PACKAGE@/plugins

if MAKE_EXPENSE

lib_LTLIBRARIES = libexpense.la
libexpense_la_SOURCES = expense.c
libexpense_la_CFLAGS = @PILOT_FLAGS@ @GTK_CFLAGS@
libexpense_la_LDFLAGS = -module -avoid-version
libexpense_la_LIBADD = @GTK_LIBS@

endif

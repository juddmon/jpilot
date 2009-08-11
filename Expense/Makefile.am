# $Id: Makefile.am,v 1.8 2009/08/11 20:13:41 rikster5 Exp $

libdir = @libdir@/@PACKAGE@/plugins

if MAKE_EXPENSE

lib_LTLIBRARIES = libexpense.la
libexpense_la_SOURCES = expense.c
libexpense_la_CFLAGS = @PILOT_FLAGS@ @GTK_CFLAGS@ -I$(top_srcdir)
libexpense_la_LDFLAGS = -module -avoid-version
libexpense_la_LIBADD = @GTK_LIBS@

endif

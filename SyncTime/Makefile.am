# $Id: Makefile.am,v 1.7 2009/07/06 22:29:47 rikster5 Exp $

libdir = @libdir@/@PACKAGE@/plugins

if MAKE_SYNCTIME

lib_LTLIBRARIES = libsynctime.la
libsynctime_la_SOURCES = synctime.c
libsynctime_la_CFLAGS = @PILOT_FLAGS@ @GTK_CFLAGS@
libsynctime_la_LDFLAGS = -module -avoid-version
libsynctime_la_LIBADD = @GTK_LIBS@

endif

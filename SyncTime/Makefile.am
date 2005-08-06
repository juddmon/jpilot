# $Id: Makefile.am,v 1.5 2005/08/06 21:46:39 rousseau Exp $

libdir = $(prefix)/lib/jpilot/plugins

if MAKE_SYNCTIME

lib_LTLIBRARIES = libsynctime.la
libsynctime_la_SOURCES = synctime.c
libsynctime_la_CFLAGS = -Wall @PILOT_FLAGS@ @GTK_CFLAGS@ -I$(top_srcdir)
libsynctime_la_LDFLAGS = @GTK_LIBS@

endif

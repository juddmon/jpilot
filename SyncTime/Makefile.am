# $Id: Makefile.am,v 1.6 2006/09/04 19:45:03 judd Exp $

libdir = $(prefix)/lib/jpilot/plugins

if MAKE_SYNCTIME

lib_LTLIBRARIES = libsynctime.la
libsynctime_la_SOURCES = synctime.c
libsynctime_la_CFLAGS = @PILOT_FLAGS@ @GTK_CFLAGS@ -I$(top_srcdir)
libsynctime_la_LDFLAGS = @GTK_LIBS@

endif

# $Id: Makefile.am,v 1.3 2003/05/02 04:42:24 judd Exp $

AM_CFLAGS = -Wall @CFLAGS@ @ccoptions@ @PILOT_FLAGS@ @GTK_CFLAGS@ -I$(top_srcdir)
libdir = $(prefix)/lib/jpilot/plugins

if MAKE_SYNCTIME

lib_LTLIBRARIES = libsynctime.la

libsynctime_la_SOURCES = synctime.c

endif

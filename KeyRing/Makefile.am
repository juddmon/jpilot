# $Id: Makefile.am,v 1.8 2008/06/02 17:24:10 rikster5 Exp $

libdir = $(prefix)/lib/jpilot/plugins

EXTRA_DIST = README

if MAKE_KEYRING

lib_LTLIBRARIES = libkeyring.la

libkeyring_la_SOURCES = keyring.c

libkeyring_la_CFLAGS = @PILOT_FLAGS@ @GTK_CFLAGS@ -I$(top_srcdir)
libkeyring_la_LDFLAGS = -lcrypto @GTK_LIBS@

endif

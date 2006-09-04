# $Id: Makefile.am,v 1.7 2006/09/04 19:45:03 judd Exp $

libdir = $(prefix)/lib/jpilot/plugins

EXTRA_DIST = README.txt

if MAKE_KEYRING

lib_LTLIBRARIES = libkeyring.la

libkeyring_la_SOURCES = keyring.c

libkeyring_la_CFLAGS = @PILOT_FLAGS@ @GTK_CFLAGS@ -I$(top_srcdir)
libkeyring_la_LDFLAGS = -lcrypto @GTK_LIBS@

endif

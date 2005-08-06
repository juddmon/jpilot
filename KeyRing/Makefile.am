# $Id: Makefile.am,v 1.6 2005/08/06 21:46:39 rousseau Exp $

libdir = $(prefix)/lib/jpilot/plugins

EXTRA_DIST = README.txt

if MAKE_KEYRING

lib_LTLIBRARIES = libkeyring.la

libkeyring_la_SOURCES = keyring.c

libkeyring_la_CFLAGS = -Wall @PILOT_FLAGS@ @GTK_CFLAGS@ -I$(top_srcdir)
libkeyring_la_LDFLAGS = -lcrypto @GTK_LIBS@

endif

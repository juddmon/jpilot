# $Id: Makefile.am,v 1.5 2005/02/19 16:03:05 rousseau Exp $

libdir = $(prefix)/lib/jpilot/plugins

EXTRA_DIST = README.txt

if MAKE_KEYRING

lib_LTLIBRARIES = libkeyring.la

libkeyring_la_SOURCES = keyring.c

libkeyring_la_CFLAGS = -Wall @PILOT_FLAGS@ @GTK_CFLAGS@ -I$(top_srcdir)
libkeyring_la_LDFLAGS = -lcrypto

endif

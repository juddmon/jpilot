# $Id: Makefile.am,v 1.3 2003/02/13 22:57:23 judd Exp $

CFLAGS = -Wall @CFLAGS@ @ccoptions@ @PILOT_FLAGS@ @GTK_CFLAGS@ -I$(top_srcdir)
LDFLAGS = @LDFLAGS@ -lcrypto
libdir = $(prefix)/lib/jpilot/plugins

EXTRA_DIST = README.txt

if MAKE_KEYRING

lib_LTLIBRARIES = libkeyring.la

libkeyring_la_SOURCES = keyring.c

endif

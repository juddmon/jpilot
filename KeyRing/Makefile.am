# $Id: Makefile.am,v 1.2 2003/01/08 06:50:26 judd Exp $

CFLAGS = -Wall @CFLAGS@ @ccoptions@ @PILOT_FLAGS@ @GTK_CFLAGS@ -I$(top_srcdir)
LDFLAGS = -lcrypto
libdir = $(prefix)/lib/jpilot/plugins

EXTRA_DIST = README.txt

if MAKE_KEYRING

lib_LTLIBRARIES = libkeyring.la

libkeyring_la_SOURCES = keyring.c

endif

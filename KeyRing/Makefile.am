# $Id: Makefile.am,v 1.4 2003/05/02 04:42:13 judd Exp $

AM_CFLAGS = -Wall @CFLAGS@ @ccoptions@ @PILOT_FLAGS@ @GTK_CFLAGS@ -I$(top_srcdir)
AM_LDFLAGS = @LDFLAGS@ -lcrypto
libdir = $(prefix)/lib/jpilot/plugins

EXTRA_DIST = README.txt

if MAKE_KEYRING

lib_LTLIBRARIES = libkeyring.la

libkeyring_la_SOURCES = keyring.c

endif

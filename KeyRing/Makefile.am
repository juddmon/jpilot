# $Id: Makefile.am,v 1.1 2002/08/28 16:50:47 judd Exp $

lib_LTLIBRARIES = libkeyring.la

EXTRA_DIST = README.txt

libkeyring_la_SOURCES = keyring.c
libdir = $(prefix)/lib/jpilot/plugins

CFLAGS = -Wall @CFLAGS@ @ccoptions@ @PILOT_FLAGS@ @GTK_CFLAGS@ -I$(top_srcdir)
LDFLAGS = -lcrypto


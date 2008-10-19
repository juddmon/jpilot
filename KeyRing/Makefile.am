# $Id: Makefile.am,v 1.9 2008/10/19 19:07:49 rousseau Exp $

libdir = $(prefix)/lib/jpilot/plugins

EXTRA_DIST = README

if MAKE_KEYRING

lib_LTLIBRARIES = libkeyring.la

libkeyring_la_SOURCES = keyring.c

libkeyring_la_CFLAGS = @PILOT_FLAGS@ @GTK_CFLAGS@ @LIBGCRYPT_CFLAGS@ -I$(top_srcdir)
libkeyring_la_LDFLAGS = @OPENSSL_LIBS@ @GTK_LIBS@ @LIBGCRYPT_LIBS@

endif

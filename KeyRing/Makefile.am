# $Id: Makefile.am,v 1.10 2009/07/06 22:29:47 rikster5 Exp $

libdir = @libdir@/@PACKAGE@/plugins

EXTRA_DIST = README

if MAKE_KEYRING

lib_LTLIBRARIES = libkeyring.la
libkeyring_la_SOURCES = keyring.c
libkeyring_la_CFLAGS = @PILOT_FLAGS@ @GTK_CFLAGS@ @LIBGCRYPT_CFLAGS@
libkeyring_la_LDFLAGS = -module -avoid-version
libkeyring_la_LIBADD = @OPENSSL_LIBS@ @GTK_LIBS@ @LIBGCRYPT_LIBS@

endif

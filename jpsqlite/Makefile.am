libdir = @libdir@/@PACKAGE@/plugins
datadir = @libdir@/@PACKAGE@/plugins

data_DATA = jptables.sql

if MAKE_JPSQLITE

lib_LTLIBRARIES = libjpsqlite.la
libjpsqlite_la_SOURCES = jpsqlite.c
libjpsqlite_la_CFLAGS = @PILOT_FLAGS@ @GTK_CFLAGS@ -I$(top_srcdir)
libjpsqlite_la_LDFLAGS = -module -avoid-version
libjpsqlite_la_LIBADD = @GTK_LIBS@ -lsqlite3

endif

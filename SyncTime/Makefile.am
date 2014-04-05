libdir = @libdir@/@PACKAGE@/plugins

if MAKE_SYNCTIME

lib_LTLIBRARIES = libsynctime.la
libsynctime_la_SOURCES = synctime.c
libsynctime_la_CFLAGS = @PILOT_FLAGS@ @GTK_CFLAGS@ -I$(top_srcdir)
libsynctime_la_LDFLAGS = -module -avoid-version
libsynctime_la_LIBADD = @GTK_LIBS@

endif

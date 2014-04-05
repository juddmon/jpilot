libdir = @libdir@/@PACKAGE@/plugins

if MAKE_EXPENSE

lib_LTLIBRARIES = libexpense.la
libexpense_la_SOURCES = expense.c
libexpense_la_CFLAGS = @PILOT_FLAGS@ @GTK_CFLAGS@ -I$(top_srcdir)
libexpense_la_LDFLAGS = -module -avoid-version
libexpense_la_LIBADD = @GTK_LIBS@

endif

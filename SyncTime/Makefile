top_srcdir = ..
top_builddir = ..

# Install variables
prefix=/usr/local

#Pilot flags (include dir), and Libraries
PILOT_FLAGS= -I/usr/extra/pilot/include
PILOT_LIBS=-L/usr/extra/pilot/lib -lpisock

#set CC to your compiler
CC = gcc
INSTALL = /usr/bin/ginstall -c
LIBTOOL = $(SHELL) $(top_builddir)/libtool

# CC options
CCDEFINES = -DHAVE_CONFIG_H
CFLAGS = -g -O2

# CC options
OPTIONS =  $(CFLAGS)

# GTK+ flags
GTKFLAGS = -I/opt/gnome/include/gtk-1.2 -I/opt/gnome/include/glib-1.2 -I/opt/gnome/lib/glib/include -I/usr/X11R6/include
# GTK+ libs
GTKLIBS = -L/opt/gnome/lib -L/usr/X11R6/lib -lgtk -lgdk -rdynamic -lgmodule -lglib -ldl -lXext -lX11 -lm

# Targets
all: libplugin.h libsynctime.la

libsynctime.la: synctime.o
	$(LIBTOOL) --mode=link \
	$(CC) $(OPTIONS) -o libsynctime.la \
	synctime.lo \
	-rpath $(prefix)/lib/jpilot/plugins \
	-version-info 1:1 \
	-avoid-version

synctime.o: synctime.c libplugin.h
	$(LIBTOOL) --mode=compile \
	$(CC) $(OPTIONS) $(GTKFLAGS) $(PILOT_FLAGS) -c synctime.c

libplugin.h: ../libplugin.h
	rm -f libplugin.h; ln -s ../libplugin.h libplugin.h; \

clean: 
	rm -f *.o *~ config.cache .libs/* *.so *.so.? *.so.?.? *.lo *.la
	rm -rf .libs/

distclean: clean
	rm -f config.status config.log *.so Makefile

#
# This is a global install.  You can also install per user in
# ~/.jpilot/plugins
#
install:
	$(INSTALL) -d -m 755 $(prefix)/lib/jpilot; \
	$(INSTALL) -d -m 755 $(prefix)/lib/jpilot/plugins; \
	$(LIBTOOL) --mode=install $(INSTALL) \
	libsynctime.la $(prefix)/lib/jpilot/plugins

local_install:
	$(INSTALL) -d -m 755 $(HOME)/.jpilot; \
	$(INSTALL) -d -m 755 $(HOME)/.jpilot/plugins; \
	$(LIBTOOL) --mode=install $(INSTALL) \
	libsynctime.la $(HOME)/.jpilot/plugins

uninstall:
	rm -f $(prefix)/lib/jpilot/plugins/libsynctime.so*

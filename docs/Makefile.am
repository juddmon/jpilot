# $Id: Makefile.am,v 1.2 2002/10/09 01:55:05 judd Exp $

EXTRA_DIST=manual.html plugin.html \
           jpilot-address.png \
	   jpilot-datebook.png \
	   jpilot-expense.png \
	   jpilot-install.png \
	   jpilot-memo.png \
	   jpilot-prefs.png \
	   jpilot-print.png \
	   jpilot-search.png \
	   jpilot-todo.png \
           jpilot.1 jpilot-sync.1 jpilot-upgrade-99.1

#  install the man pages
man_MANS=jpilot.1 jpilot-sync.1 jpilot-upgrade-99.1

###
# HTML documentation, etc.
###
install-data-local:
	$(INSTALL) -d -m 755 $(prefix)/doc/$(PACKAGE)-$(VERSION)
	$(INSTALL) -m 644 manual.html $(prefix)/doc/$(PACKAGE)-$(VERSION)/
	$(INSTALL) -m 644 plugin.html $(prefix)/doc/$(PACKAGE)-$(VERSION)
	$(INSTALL) -m 644 jpilot-address.png $(prefix)/doc/$(PACKAGE)-$(VERSION)
	$(INSTALL) -m 644 jpilot-datebook.png $(prefix)/doc/$(PACKAGE)-$(VERSION)
	$(INSTALL) -m 644 jpilot-expense.png $(prefix)/doc/$(PACKAGE)-$(VERSION)
	$(INSTALL) -m 644 jpilot-install.png $(prefix)/doc/$(PACKAGE)-$(VERSION)
	$(INSTALL) -m 644 jpilot-memo.png $(prefix)/doc/$(PACKAGE)-$(VERSION)
	$(INSTALL) -m 644 jpilot-prefs.png $(prefix)/doc/$(PACKAGE)-$(VERSION)
	$(INSTALL) -m 644 jpilot-print.png $(prefix)/doc/$(PACKAGE)-$(VERSION)
	$(INSTALL) -m 644 jpilot-search.png $(prefix)/doc/$(PACKAGE)-$(VERSION)
	$(INSTALL) -m 644 jpilot-todo.png $(prefix)/doc/$(PACKAGE)-$(VERSION)
	$(INSTALL) -m 644 jpilot-toplogo.jpg $(prefix)/doc/$(PACKAGE)-$(VERSION)
	$(INSTALL) -d -m 755 $(prefix)/doc/$(PACKAGE)-$(VERSION)/icons
	$(INSTALL) -m 644 ../icons/README $(prefix)/doc/$(PACKAGE)-$(VERSION)/icons/README
	$(INSTALL) -m 644 ../icons/jpilot-icon1.xpm $(prefix)/doc/$(PACKAGE)-$(VERSION)/icons/
	$(INSTALL) -m 644 ../icons/jpilot-icon2.xpm $(prefix)/doc/$(PACKAGE)-$(VERSION)/icons/
	$(INSTALL) -m 644 ../icons/jpilot-icon3.xpm $(prefix)/doc/$(PACKAGE)-$(VERSION)/icons/
	$(INSTALL) -m 644 ../icons/jpilot-icon4.xpm $(prefix)/doc/$(PACKAGE)-$(VERSION)/icons/
	$(INSTALL) -m 644 ../BUGS $(prefix)/doc/$(PACKAGE)-$(VERSION)/
	$(INSTALL) -m 644 ../ChangeLog $(prefix)/doc/$(PACKAGE)-$(VERSION)/
	$(INSTALL) -m 644 ../COPYING $(prefix)/doc/$(PACKAGE)-$(VERSION)/
	$(INSTALL) -m 644 ../AUTHORS $(prefix)/doc/$(PACKAGE)-$(VERSION)/
	$(INSTALL) -m 644 ../INSTALL $(prefix)/doc/$(PACKAGE)-$(VERSION)/
	$(INSTALL) -m 644 ../README $(prefix)/doc/$(PACKAGE)-$(VERSION)/
	$(INSTALL) -m 644 ../TODO $(prefix)/doc/$(PACKAGE)-$(VERSION)/
	$(INSTALL) -m 644 ../UPGRADING $(prefix)/doc/$(PACKAGE)-$(VERSION)/

uninstall-local:
	rm -f $(prefix)/doc/$(PACKAGE)-$(VERSION)/manual.html
	rm -f $(prefix)/doc/$(PACKAGE)-$(VERSION)/plugin.html
	rm -f $(prefix)/doc/$(PACKAGE)-$(VERSION)/jpilot-address.png
	rm -f $(prefix)/doc/$(PACKAGE)-$(VERSION)/jpilot-datebook.png
	rm -f $(prefix)/doc/$(PACKAGE)-$(VERSION)/jpilot-expense.png
	rm -f $(prefix)/doc/$(PACKAGE)-$(VERSION)/jpilot-install.png
	rm -f $(prefix)/doc/$(PACKAGE)-$(VERSION)/jpilot-memo.png
	rm -f $(prefix)/doc/$(PACKAGE)-$(VERSION)/jpilot-prefs.png
	rm -f $(prefix)/doc/$(PACKAGE)-$(VERSION)/jpilot-print.png
	rm -f $(prefix)/doc/$(PACKAGE)-$(VERSION)/jpilot-search.png
	rm -f $(prefix)/doc/$(PACKAGE)-$(VERSION)/jpilot-todo.png
	rm -f $(prefix)/doc/$(PACKAGE)-$(VERSION)/jpilot-toplogo.jpg
	rm -f $(prefix)/doc/$(PACKAGE)-$(VERSION)/icons/README
	rm -f $(prefix)/doc/$(PACKAGE)-$(VERSION)/icons/jpilot-icon1.xpm
	rm -f $(prefix)/doc/$(PACKAGE)-$(VERSION)/icons/jpilot-icon2.xpm
	rm -f $(prefix)/doc/$(PACKAGE)-$(VERSION)/icons/jpilot-icon3.xpm
	rm -f $(prefix)/doc/$(PACKAGE)-$(VERSION)/icons/jpilot-icon4.xpm
	rm -f $(prefix)/doc/$(PACKAGE)-$(VERSION)/BUGS
	rm -f $(prefix)/doc/$(PACKAGE)-$(VERSION)/ChangeLog
	rm -f $(prefix)/doc/$(PACKAGE)-$(VERSION)/COPYING
	rm -f $(prefix)/doc/$(PACKAGE)-$(VERSION)/AUTHORS
	rm -f $(prefix)/doc/$(PACKAGE)-$(VERSION)/INSTALL
	rm -f $(prefix)/doc/$(PACKAGE)-$(VERSION)/README
	rm -f $(prefix)/doc/$(PACKAGE)-$(VERSION)/TODO
	rm -f $(prefix)/doc/$(PACKAGE)-$(VERSION)/UPGRADING
	rmdir $(prefix)/doc/$(PACKAGE)-$(VERSION)/icons
	rmdir $(prefix)/doc/$(PACKAGE)-$(VERSION)

Summary: Jpilot pilot desktop software
Name: jpilot
Version: 0.99.3
Release: 1rh7
Copyright: GPL
Group: Applications/Productivity
URL: http://www.jpilot.org
Packager: Chuck Mead <csm@moongroup.com>
Source: http://www.jpilot.org/jpilot-%{version}.tar.gz
Source1: jpilot.desktop
BuildRoot: /var/tmp/jpilot
Prefix: /usr
Requires: gtk+ >= 1.2.0 , pilot-link

%description
J-Pilot is a desktop organizer application for the palm pilot that runs under
Linux. t is similar in functionality to the one that 3com distributes for a
well known rampant legacy operating system.

%prep
%setup -q

%build
%configure
make all
make jpilot-dump
# Now do the plugin stuff
# make libplugin
# Expense
cd Expense
./configure --prefix=%{_prefix}
make

# SyncTime
cd ../SyncTime
./configure --prefix=%{_prefix}
make

%install
rm -rf ${RPM_BUILD_ROOT}
mkdir -p ${RPM_BUILD_ROOT}%{_prefix}/share/jpilot ${RPM_BUILD_ROOT}%{_prefix}/bin
perl -pi -e 's@/usr/share@\$(PREFIX)/usr/share@g' Makefile
perl -pi -e 's@/usr/bin@\$(PREFIX)/usr/bin@g' Makefile
make prefix=$RPM_BUILD_ROOT%{_prefix} install

cd Expense && make prefix=$RPM_BUILD_ROOT%{_prefix} install
cd ../SyncTime && make prefix=$RPM_BUILD_ROOT%{_prefix} install

mkdir -p $RPM_BUILD_ROOT/etc/X11/applnk/Applications
mkdir -p $RPM_BUILD_ROOT%{_prefix}/share/pixmaps
mkdir -p $RPM_BUILD_ROOT%{_prefix}/share/man/man1
install -m644 %{SOURCE1} $RPM_BUILD_ROOT/etc/X11/applnk/Applications/jpilot.desktop
install -m644 $RPM_BUILD_DIR/jpilot-%{version}/icons/* $RPM_BUILD_ROOT%{_prefix}/share/pixmaps
install -m644 $RPM_BUILD_DIR/jpilot-%{version}/docs/*.1* $RPM_BUILD_ROOT%{_prefix}/share/man/man1

%clean
rm -rf ${RPM_BUILD_ROOT}

%files
%defattr(-,root,root)
%doc BUGS README COPYING TODO CREDITS INSTALL  
%{_prefix}/bin/jpilot
%{_prefix}/bin/jpilot-dump
%{_prefix}/bin/jpilot-sync
%{_prefix}/bin/jpilot-upgrade-99
%{_prefix}/share/jpilot
%{_prefix}/lib/jpilot/plugins/libexpense.so
%{_prefix}/lib/jpilot/plugins/libexpense.so.1
%{_prefix}/lib/jpilot/plugins/libexpense.so.1.0.1
%{_prefix}/lib/jpilot/plugins/libexpense.la
%{_prefix}/lib/jpilot/plugins/libsynctime.so
%{_prefix}/lib/jpilot/plugins/libsynctime.so.1
%{_prefix}/lib/jpilot/plugins/libsynctime.so.1.0.1
%{_prefix}/lib/jpilot/plugins/libsynctime.la
%{_prefix}/share/locale/*/*/*
%{_mandir}/man1/*.1*
%{_prefix}/share/pixmaps/*
%config /etc/X11/applnk/Applications/jpilot.desktop

%changelog
* Wed Mar 28 2001 Chuck Mead <csm@moongroup.com>
- fixed paths for RH7.

* Mon Jul 24 2000 Prospector <prospector@redhat.com>
- rebuilt

* Mon Jul 10 2000 Tim Powers <timp@redhat.com>
- rebuilt

* Fri Jun 30 2000 Prospector <bugzilla@redhat.com>
- automatic rebuild

* Mon May 15 2000 Tim Powers <timp@redhat.com>
- using applnk now instead of a GNOME specific menu entry
- built for 7.0

* Wed Apr 19 2000 Tim Powers <timp@redhat.com>
- added desktop entry for GNOME

* Wed Apr 12 2000 Tim Powers <timp@redhat.com>
- updated to 0.98.1

* Mon Apr 3 2000 Tim Powers <timp@redhat.com>
- updated to 0.98
- bzipped source
- using percent configure instead of ./configure
- quiet setup
- minor spec file cleanups

* Tue Dec 21 1999 Tim Powers <timp@redhat.com>
- changed requires

* Mon Oct 25 1999 Tim Powers <timp@redhat.com>
- changed group to Applications/Productivity
- first build

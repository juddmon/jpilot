%define version 0.99.1r

Summary:   palm pilot desktop for Linux
Name:      jpilot
Version:   %{version}
Release:   1
Copyright: GPL
Group:     Applications/Productivity
Source:    http://jpilot.org/jpilot-%{version}.tar.gz
URL:       http://jpilot.org
Packager:  Judd Montgomery <judd@jpilot.org>
# Patch0:    dst_0.98.patch
# Patch1:    jpilot.makefile.patch
Prefix:    /usr
DocDir:    %{prefix}/share/doc
# BuildRoot: %{_tmppath}/jpilot-root

%description
J-Pilot is a desktop organizer application for the palm pilot that runs
under Linux and Unix using X-Windows and GTK+.  It is similar in
functionality to the one that 3Com distributes and has many features
not found in the 3Com desktop.

%prep

%setup -q
# This seems to have already been applied
# %patch0 -p0 -b .dst_0.98
# %patch1 -p0 -b .makefile

%build
#
./configure --prefix=%{prefix}
#
gzip -9f docs/*.1
#
make
#
make jpilot-dump
# Now do the plugin stuff
# make libplugin
# Expense
#cd Expense
#./configure --prefix=%{prefix}
#make

# SyncTime
#cd ../SyncTime
#./configure --prefix=%{prefix}
#make

%install
rm -rf $RPM_BUILD_ROOT
install -d $RPM_BUILD_ROOT%{_mandir}/man1
install docs/jpilot*.1.gz $RPM_BUILD_ROOT%{_mandir}/man1
strip jpilot
install -d $RPM_BUILD_ROOT%{_bindir}
make prefix=$RPM_BUILD_ROOT%{_prefix} install
make DEST=$RPM_BUILD_ROOT install

%post
#if [ -x `which libtool` ]; then
#    libtool --finish %{prefix}/lib/jpilot/plugins
#fi

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%doc BUGS CHANGELOG COPYING CREDITS INSTALL README TODO UPGRADING
%doc icons docs/plugin.html docs/manual.html
%{_bindir}/jpilot
%{_bindir}/jpilot-dump
%{_bindir}/jpilot-sync
%{_bindir}/jpilot-upgrade-99
%{_datadir}/jpilot/jpilotrc.blue
%{_datadir}/jpilot/jpilotrc.default
%{_datadir}/jpilot/jpilotrc.green
%{_datadir}/jpilot/jpilotrc.purple
%{_datadir}/jpilot/jpilotrc.steel
%{_datadir}/jpilot/DatebookDB.pdb
%{_datadir}/jpilot/AddressDB.pdb
%{_datadir}/jpilot/ToDoDB.pdb
%{_datadir}/jpilot/MemoDB.pdb
%{_datadir}/jpilot/Memo32DB.pdb
%{_datadir}/jpilot/ExpenseDB.pdb
%{_libdir}/jpilot/plugins/libexpense.so
%{_libdir}/jpilot/plugins/libexpense.so.1
%{_libdir}/jpilot/plugins/libexpense.so.1.0.1
%{_libdir}/jpilot/plugins/libexpense.la
%{_libdir}/jpilot/plugins/libsynctime.so
%{_libdir}/jpilot/plugins/libsynctime.so.1
%{_libdir}/jpilot/plugins/libsynctime.so.1.0.1
%{_libdir}/jpilot/plugins/libsynctime.la
%{_datadir}/locale/*/LC_MESSAGES/jpilot.mo
%{_mandir}/man1/jpilot.1.gz
%{_mandir}/man1/jpilot-sync.1.gz
%{_mandir}/man1/jpilot-upgrade-99.1.gz

%changelog
* Tue Jun  5 2001 Christian W. Zuckschwerdt <zany@triq.net>
- moved jpilot.spec to jpilot.spec.in and autoconf'ed it.
- fixed this spec file so we don't need superuser privileges.
- changed the hardcoded path into rpm macros
* Wed Nov 22 2000 Matthew Vanecek <linux4us@home.com>
- deleted the calls to 'install' in the %install section since
  this is already done in the Makefile.
- Deleted the %attr tags from the %files list and made the default
  attribute to -,root,root.
- changed the /usr/ to %{prefix}/
- Added the %post section
- Added the %clean section
- Changed the description

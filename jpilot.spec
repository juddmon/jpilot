%define version 0.99

Summary:   palm pilot desktop for Linux
Name:      jpilot
Version:   %{version}
Release:   1
Copyright: GPL
Group:     Applications/Communications
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
functionality to the one that 3com distributes and has many features
not found in the 3com desktop.

%prep

%setup -q
# This seems to have already been applied
# %patch0 -p0 -b .dst_0.98
# %patch1 -p0 -b .makefile

%build
gzip -9f docs/jpilot*.1
cp docs/jpilot*.1.gz /usr/man/man1/
#
./configure --prefix=%{prefix}
make
#
make jpilot-dump
# Now do the plugin stuff
# make libplugin
# Expense
cd Expense
./configure --prefix=%{prefix}
make

# SyncTime
cd ../SyncTime
./configure --prefix=%{prefix}
make

%install
rm -rf $RPM_BUILD_ROOT
strip jpilot
make DEST=$RPM_BUILD_ROOT install
cd Expense && make prefix=$RPM_BUILD_ROOT%{prefix} install
cd ../SyncTime && make prefix=$RPM_BUILD_ROOT%{prefix} install

%post
if [ -x `which libtool` ]; then
    libtool --finish %{prefix}/lib/jpilot/plugins
fi

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%doc BUGS CHANGELOG COPYING CREDITS INSTALL README TODO UPGRADING
%doc icons docs/plugin.html docs/manual.html
%{prefix}/bin/jpilot
%{prefix}/bin/jpilot-dump
%{prefix}/bin/jpilot-sync
%{prefix}/bin/jpilot-upgrade-99
%{prefix}/share/jpilot/jpilotrc.blue
%{prefix}/share/jpilot/jpilotrc.default
%{prefix}/share/jpilot/jpilotrc.green
%{prefix}/share/jpilot/jpilotrc.purple
%{prefix}/share/jpilot/jpilotrc.steel
%{prefix}/share/jpilot/DatebookDB.pdb
%{prefix}/share/jpilot/AddressDB.pdb
%{prefix}/share/jpilot/ToDoDB.pdb
%{prefix}/share/jpilot/MemoDB.pdb
%{prefix}/lib/jpilot/plugins/libexpense.so
%{prefix}/lib/jpilot/plugins/libexpense.so.1
%{prefix}/lib/jpilot/plugins/libexpense.so.1.0.1
%{prefix}/lib/jpilot/plugins/libexpense.la
%{prefix}/lib/jpilot/plugins/libsynctime.so
%{prefix}/lib/jpilot/plugins/libsynctime.so.1
%{prefix}/lib/jpilot/plugins/libsynctime.so.1.0.1
%{prefix}/lib/jpilot/plugins/libsynctime.la
%{prefix}/share/locale/*/LC_MESSAGES/jpilot.mo
/usr/man/man1/jpilot.1.gz
/usr/man/man1/jpilot-sync.1.gz
/usr/man/man1/jpilot-upgrade-99.1.gz

%changelog
* Wed Nov 22 2000 Matthew Vanecek <linux4us@home.com>
- deleted the calls to 'install' in the %install section since
  this is already done in the Makefile.
- Deleted the %attr tags from the %files list and made the default
  attribute to -,root,root.
- changed the /usr/ to %{prefix}/
- Added the %post section
- Added the %clean section
- Changed the description

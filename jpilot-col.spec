%define version 0.99.5
Name        	: jpilot
Version     	: %{version}
Release     	: 1_COL
Group       	: Office/Organizer

Summary     	: Palm pilot desktop.

Copyright   	: GPL
Packager    	: Bart Whiteley <bart@calderasystems.com>
URL         	: http://jpilot.org/


BuildRoot   	: /tmp/%{Name}-%{Version}

Source0: http://jpilot.org/jpilot-%{version}.tar.gz
Source1: jpilot.kdelnk


%Description
jpilot is a palm pilot desktop for Linux written by:
Judd Montgomery, judd@engineer.com


%Prep

%setup


%Build
./configure --prefix=/usr/
# fix the bad $(PWD) variable in the Makefile
perl -pi -e 's/PWD/shell pwd/g' Makefile

make
#
make jpilot-dump
# Now do the plugin stuff
# make libplugin
cd Expense
./configure --prefix=/usr/
make
#
cd ../SyncTime
./configure --prefix=/usr/
make


%Install
%{mkDESTDIR}
install -d $DESTDIR/usr/share/jpilot/
install -d $DESTDIR/usr/lib/jpilot/
install -d $DESTDIR/usr/lib/jpilot/plugins/
install -d $DESTDIR/usr/bin/
install -d $DESTDIR/opt/kde/share/icons/
install -d $DESTDIR/opt/kde/share/applnk/Utilities/
install -s jpilot $DESTDIR/usr/bin/jpilot
install -s jpilot-dump $DESTDIR/usr/bin/jpilot-dump
install jpilotrc.blue $DESTDIR/usr/share/jpilot/jpilotrc.blue
install jpilotrc.default $DESTDIR/usr/share/jpilot/jpilotrc.default
install jpilotrc.green $DESTDIR/usr/share/jpilot/jpilotrc.green
install jpilotrc.purple $DESTDIR/usr/share/jpilot/jpilotrc.purple
install jpilotrc.steel $DESTDIR/usr/share/jpilot/jpilotrc.steel
install empty/DatebookDB.pdb $DESTDIR/usr/share/jpilot/DatebookDB.pdb
install empty/AddressDB.pdb $DESTDIR/usr/share/jpilot/AddressDB.pdb
install empty/ToDoDB.pdb $DESTDIR/usr/share/jpilot/ToDoDB.pdb
install empty/MemoDB.pdb $DESTDIR/usr/share/jpilot/MemoDB.pdb
install empty/MemoDB.pdb $DESTDIR/usr/share/jpilot/MemoDB.pdb
install Expense/.libs/libexpense.so.1.0.1 $DESTDIR/usr/lib/jpilot/plugins/libexpense.so.1.0.1
install Expense/libexpense.la $DESTDIR/usr/lib/jpilot/plugins/libexpense.la
ln -s libexpense.so.1.0.1 $DESTDIR/usr/lib/jpilot/plugins/libexpense.so
ln -s libexpense.so.1.0.1 $DESTDIR/usr/lib/jpilot/plugins/libexpense.so.1
install SyncTime/.libs/libsynctime.so.1.0.1 $DESTDIR/usr/lib/jpilot/plugins/libsynctime.so.1.0.1
install SyncTime/libsynctime.la $DESTDIR/usr/lib/jpilot/plugins/libsynctime.la
ln -s libsynctime.so.1.0.1 $DESTDIR/usr/lib/jpilot/plugins/libsynctime.so
ln -s libsynctime.so.1.0.1 $DESTDIR/usr/lib/jpilot/plugins/libsynctime.so.1

# install the kde files.
install %{SOURCE1} $DESTDIR/opt/kde/share/applnk/Utilities/jpilot.kdelnk
ln -s /usr/doc/jpilot-0.98.1/icons/jpilot-icon2.xpm $DESTDIR/opt/kde/share/icons/jpilot.xpm

%{fixManPages}


%Clean
%{rmDESTDIR}


%Files
%defattr(0644,root,root)
%attr(-,root,root) %doc BUGS CHANGELOG COPYING CREDITS INSTALL README TODO UPGRADING
%attr(-,root,root) %doc icons 
%doc docs/plugin.html docs/manual.html
%attr(0755,root,root) /usr/bin/jpilot
/usr/share/jpilot/jpilotrc.blue
/usr/share/jpilot/jpilotrc.default
/usr/share/jpilot/jpilotrc.green
/usr/share/jpilot/jpilotrc.purple
/usr/share/jpilot/jpilotrc.steel
/usr/share/jpilot/DatebookDB.pdb
/usr/share/jpilot/AddressDB.pdb
/usr/share/jpilot/ToDoDB.pdb
/usr/share/jpilot/MemoDB.pdb
/usr/lib/jpilot/plugins/libexpense.so
/usr/lib/jpilot/plugins/libexpense.so.1
/usr/lib/jpilot/plugins/libexpense.so.1.0.1
/usr/lib/jpilot/plugins/libexpense.la
/usr/lib/jpilot/plugins/libsynctime.so
/usr/lib/jpilot/plugins/libsynctime.so.1
/usr/lib/jpilot/plugins/libsynctime.so.1.0.1
/usr/lib/jpilot/plugins/libsynctime.la
/opt/kde/share/applnk/Utilities/jpilot.kdelnk
/opt/kde/share/icons/jpilot.xpm


%ChangeLog
* Mon Jan 01 1998 ...
Template Version: 1.31

$Id: jpilot-col.spec,v 1.7 2003/02/22 15:13:47 judd Exp $

%define version 0.93

Summary: palm pilot desktop for Linux
Name: jpilot
Version: %{version}
Release: 1
Copyright: GPL
Group: Applications/Communications
Source: http://jpilot.linuxbox.com/jpilot-%{version}.tar.gz
URL: http://jpilot.linuxbox.com
Packager: Sean Summers <rpm-jpilot@GeneralProtectionFault.com>

%description
jpilot is a palm pilot desktop for Linux written by:
Judd Montgomery, judd@engineer.com

%prep
%setup
%build
./configure --with-prefix=/usr
make
%install
strip jpilot
install -m 555 -s jpilot -o root -g root /usr/bin
install -m 644 -s jpilotrc.blue -o root -g root /usr/share/jpilot
install -m 644 -s jpilotrc.default -o root -g root /usr/share/jpilot
install -m 644 -s jpilotrc.green -o root -g root /usr/share/jpilot

%files
%attr(-, bin, bin) %doc BUGS CHANGELOG COPYING CREDITS INSTALL README TODO
%doc jpilotrc.default jpilotrc.blue jpilotrc.green icons

%attr(0555,root,root) /usr/bin/jpilot
%attr(0644,root,root) /usr/share/jpilotrc.blue
%attr(0644,root,root) /usr/share/jpilotrc.default
%attr(0644,root,root) /usr/share/jpilotrc.green

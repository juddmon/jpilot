# $Id: autogen.sh,v 1.2 2002/11/14 02:52:10 judd Exp $

rm -f configure Makefile.in Makefile config.h.in
aclocal

echo "Running intltoolize"
gettextize --force
intltoolize --force --copy --automake

autoheader
automake --add-missing --foreign
autoconf
./configure --prefix=/usr

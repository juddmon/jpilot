# $Id: autogen.sh,v 1.1 2002/08/28 19:49:54 judd Exp $

rm -f configure Makefile.in Makefile config.h.in
aclocal

echo "Running intltoolize"
gettextize --force
intltoolize --force --copy --automake

autoheader
automake --add-missing --foreign
autoconf
./configure --prefix=/usr/local/ --with-db3


# $Id: autogen.sh,v 1.11 2004/11/14 19:15:41 rousseau Exp $

set -x

#Use these when updating libtool
#libtoolize --force --copy
#aclocal
#autoheader
#automake
#autoconf
#
#gettextize -f --intl

rm -f configure Makefile.in Makefile config.h.in

echo "Running intltoolize"
#gettextize --force --intl
aclocal -I m4
intltoolize --force --copy --automake

autoheader
automake --add-missing --foreign
autoconf

if test x$NOCONFIGURE = x; then
  echo Running configure $conf_flags "$@" ...
  ./configure $conf_flags "$@" \
  || exit 1
else
  echo Skipping configure process.
fi

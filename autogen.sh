# $Id: autogen.sh,v 1.10 2004/10/02 07:45:52 rousseau Exp $

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
aclocal -I m4

echo "Running intltoolize"
#gettextize --force --intl
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

# $Id: autogen.sh,v 1.6 2003/02/11 07:29:50 judd Exp $

set -x

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

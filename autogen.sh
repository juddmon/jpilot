# $Id: autogen.sh,v 1.5 2002/12/25 06:47:55 judd Exp $

rm -f configure Makefile.in Makefile config.h.in
aclocal

echo "Running intltoolize"
gettextize --force
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

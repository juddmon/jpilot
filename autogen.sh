# $Id: autogen.sh,v 1.9 2004/09/29 18:31:08 rousseau Exp $

set -x

if test -f Makefile
then
	make distclean
fi
rm -rf *.cache *.m4 config.guess config.log \
config.status config.sub depcomp ltmain.sh
(cat m4/*.m4 > acinclude.m4 2> /dev/null)
autoreconf --verbose --install

exit

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

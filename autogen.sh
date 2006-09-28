# $Id: autogen.sh,v 1.13 2006/09/28 04:12:21 rikster5 Exp $

# Echo commands before they execute to help show 
set -x

# Clear the files that will be rebuilt by autoconf, automake, & autoheader
rm -f configure Makefile Makefile.in config.h.in

# Create aclocal.m4, macros that are used in all subsequent processing
aclocal -I m4

# Copy over scripts for libtool
libtoolize --force --copy --automake

# Copy over scripts to allow localization and internationalization
intltoolize --force --copy --automake

# Create config.h.in from configure.in
autoheader
# Create Makefile.in from Makefile.am & configure.in
automake --add-missing --foreign
# Create config.h, Makefile, & configure script
autoconf


# By default, run configure after generating build environment
if test x$NOCONFIGURE = x; then
  echo Running configure $conf_flags "$@" ...
  ./configure $conf_flags "$@" \
  || exit 1
else
  echo Skipping configure process.
fi


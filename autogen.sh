# Exit if a command fails
set -e

# Echo commands before they execute to help show progress
set -x

# Clear files that will be rebuilt by autoconf, automake, & autoheader
rm -f configure Makefile Makefile.in config.h.in

# autopoint is a lightweight gettextize which copies required files
# but does not change Makefile.am
autopoint --force
# Copy over scripts to allow internationalization
intltoolize --force --copy --automake
# Copy over scripts for libtool
libtoolize --force --copy --automake

# Update aclocal after other scripts have run
aclocal -I m4

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


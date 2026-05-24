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
# Copy over scripts for libtool (glibtoolize on macOS, libtoolize elsewhere)
if command -v libtoolize >/dev/null 2>&1; then
   LIBTOOLIZE=libtoolize
elif command -v glibtoolize >/dev/null 2>&1; then
   LIBTOOLIZE=glibtoolize
else
   echo "Error: neither libtoolize nor glibtoolize found in PATH" >&2
   exit 1
fi
$LIBTOOLIZE --force --copy --automake

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


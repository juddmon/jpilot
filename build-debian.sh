#!/bin/bash
# Build Debian package(s) for jpilot.
# Run from the project root. Version is taken from configure.ac (AC_INIT).
# Requires: build-essential, devscripts, and Build-Depends from debian/control.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Extract version from configure.ac (e.g. AC_INIT([jpilot],[2.0.2]) -> 2.0.2)
VERSION=$(sed -n 's/^AC_INIT(\[jpilot\],\[\([^]]*\)\].*/\1/p' configure.ac)
if [[ -z "$VERSION" ]]; then
  echo "Could not determine version from configure.ac (expected AC_INIT([jpilot],[X.Y.Z]))." >&2
  exit 1
fi

ORIG_TAR="jpilot_${VERSION}.orig.tar.gz"
DIST_TAR="jpilot-${VERSION}.tar.gz"
BUILD_DIR="jpilot-${VERSION}"

echo "Building Debian package for jpilot ${VERSION}"

# Create distribution tarball (requires automake, configured tree)
make dist

if [[ ! -f "$DIST_TAR" ]]; then
  echo "make dist did not create ${DIST_TAR}" >&2
  exit 1
fi

# Debian orig tarball (no debian/ in orig)
cp "$DIST_TAR" "$ORIG_TAR"

# Extract and copy in our debian/ directory
rm -rf "$BUILD_DIR"
tar xf "$DIST_TAR"
rsync -a debian/ "$BUILD_DIR/debian/"

# Build packages (-us -uc = do not sign; use -sa to include .orig in .dsc)
cd "$BUILD_DIR"
dpkg-buildpackage -us -uc

cd ..
echo "Built. Outputs:"
ls -la jpilot_${VERSION}-*.deb jpilot-plugins_${VERSION}-*.deb 2>/dev/null || true
ls -la jpilot_${VERSION}-*.dsc jpilot_${VERSION}.orig.tar.gz 2>/dev/null || true

# Optional: sign with debsign and upload
# debsign jpilot_${VERSION}-1_amd64.changes
# package_cloud push judd/jpilot/ubuntu/jammy ./jpilot_${VERSION}-1_amd64.deb
# package_cloud push judd/jpilot/ubuntu/jammy ./jpilot-plugins_${VERSION}-1_amd64.deb

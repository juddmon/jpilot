#!/bin/bash
# Build Debian package(s) for jpilot.
# Run from the project root. Version is taken from configure.ac (AC_INIT).
# Requires: build-essential, devscripts, and Build-Depends from debian/control.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ARTIFACT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
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

# Debian orig tarball (same bytes as make dist output; dpkg expects *_<ver>.orig.tar.gz)
cp "$DIST_TAR" "$ORIG_TAR"

# Extract from .orig; remove Automake-named tarball from the repo (duplicate of .orig)
rm -rf "$BUILD_DIR"
tar xf "$ORIG_TAR"
rm -f "$DIST_TAR"
rsync -a debian/ "$BUILD_DIR/debian/"

# Build packages (-us -uc = do not sign; use -sa to include .orig in .dsc).
# dpkg-buildpackage writes into SCRIPT_DIR (parent of the build tree).
cd "$BUILD_DIR"
dpkg-buildpackage -us -uc

cd "$SCRIPT_DIR"

# Move release artifacts to the parent of the repository directory.
shopt -s nullglob
artifacts=(
  "$SCRIPT_DIR/jpilot_${VERSION}.orig.tar.gz"
  "$SCRIPT_DIR/jpilot_${VERSION}"-*.dsc
  "$SCRIPT_DIR/jpilot_${VERSION}"-*.debian.tar.xz
  "$SCRIPT_DIR/jpilot_${VERSION}"-*.changes
  "$SCRIPT_DIR/jpilot_${VERSION}"-*.buildinfo
  "$SCRIPT_DIR/jpilot_${VERSION}"-*.deb
  "$SCRIPT_DIR/jpilot-plugins_${VERSION}"-*.deb
  "$SCRIPT_DIR/jpilot-dbgsym_${VERSION}"-*.ddeb
  "$SCRIPT_DIR/jpilot-plugins-dbgsym_${VERSION}"-*.ddeb
)
for f in "${artifacts[@]}"; do
  if [[ -f "$f" ]]; then
    mv -v "$f" "$ARTIFACT_DIR/"
  fi
done
shopt -u nullglob

# Upstream-style name next to .orig (same inode; no extra disk)
if [[ -f "$ARTIFACT_DIR/$ORIG_TAR" ]]; then
  ln -f "$ARTIFACT_DIR/$ORIG_TAR" "$ARTIFACT_DIR/$DIST_TAR" || true
fi

echo "Built. Artifacts in ${ARTIFACT_DIR}:"
ls -la "$ARTIFACT_DIR"/jpilot[-_]${VERSION}* "$ARTIFACT_DIR"/jpilot-plugins_${VERSION}* \
  "$ARTIFACT_DIR"/jpilot-dbgsym_${VERSION}* "$ARTIFACT_DIR"/jpilot-plugins-dbgsym_${VERSION}* \
  2>/dev/null || true

# Optional: sign with debsign and upload (paths are under ARTIFACT_DIR)
# debsign "${ARTIFACT_DIR}/jpilot_${VERSION}-1_amd64.changes"
# package_cloud push judd/jpilot/ubuntu/jammy "${ARTIFACT_DIR}/jpilot_${VERSION}-1_amd64.deb"
# package_cloud push judd/jpilot/ubuntu/jammy "${ARTIFACT_DIR}/jpilot-plugins_${VERSION}-1_amd64.deb"

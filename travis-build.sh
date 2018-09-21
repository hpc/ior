#!/usr/bin/env bash
#
# Build the IOR source package.  Returns the path to the built artifact.
#

BASE_DIR="$(cd "${0%/*}" && pwd)"
if [ -z "$BASE_DIR" -o ! -d "$BASE_DIR" ]; then
    echo "Cannot determine BASE_DIR (${BASE_DIR})" >&2
    exit 2
fi
BUILD_DIR="${BASE_DIR}/build"

PACKAGE="$(awk '/^Package/ {print $2}' $BASE_DIR/META)"
VERSION="$(awk '/^Version/ {print $2}' $BASE_DIR/META)"
DIST_TGZ="${PACKAGE}-${VERSION}.tar.gz"

# Build the distribution
set -e
./bootstrap
test -d "$BUILD_DIR" && rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
$BASE_DIR/configure
set +e

make dist && mv -v "${BUILD_DIR}/${DIST_TGZ}" "${BASE_DIR}/${DIST_TGZ}"
